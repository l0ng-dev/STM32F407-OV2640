/**
 ******************************************************************************
 * @file    camera.c
 * @brief   DCMI/DMA 摄像头采集适配模块
 *
 * @details
 * 主要功能：
 * 1. 在 RGB565 LCD 与 JPEG SRAM 两种目标之间安全重配 DCMI/DMA。
 * 2. 启动 Snapshot 采集并记录帧结束、DMA 计数和硬件错误。
 * 3. 向上层提供非阻塞采集状态和有效字节数。
 *
 * 模块关系：
 * Camera Application/JPEG 调用本模块；本模块复用 CubeMX 的 hdcmi 和 hdma_dcmi。
 *
 * 主要接口：
 * Camera_ConfigureRGB565LcdDma()、Camera_ConfigureJpegDma()、
 * Camera_StartRGB565Snapshot()、Camera_StartJPEGCapture()。
 *
 * @note
 * 本模块不修改 DCMI GPIO、DMA Stream/Channel 或 CubeMX 静态配置来源。
 ******************************************************************************
 */

#include "camera.h"

#include "dma.h"
#include <stdio.h>

extern DMA_HandleTypeDef hdma_dcmi;

static volatile Camera_CaptureState_t camera_state = CAMERA_CAPTURE_IDLE;
static volatile Camera_Error_t camera_error = CAMERA_ERROR_NONE;
static volatile uint8_t camera_frame_pending;
static volatile uint8_t camera_error_pending;
static volatile uint32_t camera_captured_size;
static volatile uint32_t camera_frame_dma_ndtr;
static volatile uint32_t camera_frame_dcmi_error;
static volatile uint32_t camera_frame_dma_error;
static uint32_t camera_buffer_word_count;
static uint32_t camera_capture_start_tick;
static uint32_t camera_first_progress_tick;
static volatile uint32_t camera_frame_tick;
static uint8_t camera_first_progress_seen;

static HAL_StatusTypeDef Camera_ConfigureDma(uint32_t memory_increment,
                                             uint32_t memory_alignment);

void Camera_Init(void)
{
    camera_state = CAMERA_CAPTURE_IDLE;
    camera_error = CAMERA_ERROR_NONE;
    camera_frame_pending = 0U;
    camera_error_pending = 0U;
    camera_captured_size = 0U;
    camera_frame_dma_ndtr = 0U;
    camera_frame_dcmi_error = HAL_DCMI_ERROR_NONE;
    camera_frame_dma_error = HAL_DMA_ERROR_NONE;
    camera_buffer_word_count = 0U;
    camera_first_progress_seen = 0U;
    camera_frame_tick = 0U;
}

HAL_StatusTypeDef Camera_Stop(void)
{
    HAL_StatusTypeDef status;

    __HAL_DCMI_DISABLE_IT(&hdcmi, DCMI_IT_FRAME);
    status = HAL_DCMI_Stop(&hdcmi);
    Camera_Init();

    return status;
}

/**
 * @brief  在停止状态下切换 DCMI JPEGMode
 * @note   HAL_DCMI_Init() 会根据新的 JPEGMode 重建 DCMI 状态，因此必须先确认
 *         DCMI 和 DMA 都不处于 BUSY，避免切换期间破坏正在进行的帧。
 */
HAL_StatusTypeDef Camera_SetDCMIJPEGMode(uint32_t jpeg_mode)
{
    if ((jpeg_mode != DCMI_JPEG_DISABLE) &&
        (jpeg_mode != DCMI_JPEG_ENABLE))
    {
        return HAL_ERROR;
    }

    if ((hdcmi.State == HAL_DCMI_STATE_BUSY) ||
        (hdma_dcmi.State == HAL_DMA_STATE_BUSY))
    {
        return HAL_BUSY;
    }

    hdcmi.Init.JPEGMode = jpeg_mode;
    return HAL_DCMI_Init(&hdcmi);
}

HAL_StatusTypeDef Camera_ConfigureRGB565LcdDma(void)
{
    /* LCD GRAM 是固定写端口：目标地址不递增，按 RGB565 半字宽度写入。 */
    return Camera_ConfigureDma(DMA_MINC_DISABLE,
                               DMA_MDATAALIGN_HALFWORD);
}

HAL_StatusTypeDef Camera_ConfigureJpegDma(void)
{
    /* JPEG 是连续字节流：目标地址必须递增，并以 Word 对齐写入 SRAM。 */
    return Camera_ConfigureDma(DMA_MINC_ENABLE,
                               DMA_MDATAALIGN_WORD);
}

/**
 * @brief  启动一帧 RGB565 到 LCD GRAM 的 Snapshot
 * @note   Snapshot 在一帧后自动结束；上层收到帧事件后重新设置 LCD 窗口，
 *         再启动下一帧，从而保持 LCD 内部 GRAM 写指针与画面同步。
 */
HAL_StatusTypeDef Camera_StartRGB565Snapshot(uint32_t destination,
                                             uint32_t word_count)
{
    if ((destination == 0U) || (word_count == 0U))
    {
        return HAL_ERROR;
    }

    if ((hdcmi.Init.JPEGMode != DCMI_JPEG_DISABLE) ||
        (hdma_dcmi.Init.Mode != DMA_NORMAL) ||
        (hdma_dcmi.Init.PeriphDataAlignment != DMA_PDATAALIGN_WORD) ||
        (hdma_dcmi.Init.MemDataAlignment != DMA_MDATAALIGN_HALFWORD) ||
        (hdma_dcmi.Init.MemInc != DMA_MINC_DISABLE))
    {
        return HAL_ERROR;
    }

    __HAL_DCMI_CLEAR_FLAG(&hdcmi,
                          DCMI_FLAG_FRAMERI | DCMI_FLAG_OVRRI |
                          DCMI_FLAG_ERRRI);

    return HAL_DCMI_Start_DMA(&hdcmi,
                              DCMI_MODE_SNAPSHOT,
                              destination,
                              word_count);
}

Camera_Error_t Camera_StartJPEGCapture(uint8_t *buffer, uint32_t buffer_size)
{
    HAL_StatusTypeDef hal_status;

    if ((buffer == NULL) ||
        ((((uint32_t)buffer) & 0x3U) != 0U) ||
        (buffer_size == 0U) ||
        ((buffer_size & 0x3U) != 0U))
    {
        camera_error = CAMERA_ERROR_INVALID_PARAM;
        camera_state = CAMERA_CAPTURE_ERROR;
        return camera_error;
    }

    if (hdcmi.Init.JPEGMode != DCMI_JPEG_ENABLE)
    {
        camera_error = CAMERA_ERROR_DCMI_CONFIG;
        camera_state = CAMERA_CAPTURE_ERROR;
        return camera_error;
    }

    if ((hdma_dcmi.Instance != DMA2_Stream1) ||
        (hdma_dcmi.Init.Channel != DMA_CHANNEL_1) ||
        (hdma_dcmi.Init.Mode != DMA_NORMAL) ||
        (hdma_dcmi.Init.PeriphDataAlignment != DMA_PDATAALIGN_WORD) ||
        (hdma_dcmi.Init.MemDataAlignment != DMA_MDATAALIGN_WORD) ||
        (hdma_dcmi.Init.MemInc != DMA_MINC_ENABLE))
    {
        camera_error = CAMERA_ERROR_DMA_CONFIG;
        camera_state = CAMERA_CAPTURE_ERROR;
        return camera_error;
    }

    camera_error = CAMERA_ERROR_NONE;
    camera_frame_pending = 0U;
    camera_error_pending = 0U;
    camera_captured_size = 0U;
    camera_buffer_word_count = buffer_size / sizeof(uint32_t);
    camera_frame_dma_ndtr = camera_buffer_word_count;
    camera_frame_dcmi_error = HAL_DCMI_ERROR_NONE;
    camera_frame_dma_error = HAL_DMA_ERROR_NONE;
    camera_first_progress_seen = 0U;
    camera_frame_tick = 0U;
    camera_state = CAMERA_CAPTURE_BUSY;

    __HAL_DCMI_CLEAR_FLAG(&hdcmi,
                          DCMI_FLAG_FRAMERI | DCMI_FLAG_OVRRI |
                          DCMI_FLAG_ERRRI);

    camera_capture_start_tick = HAL_GetTick();
    hal_status = HAL_DCMI_Start_DMA(&hdcmi,
                                    DCMI_MODE_SNAPSHOT,
                                    (uint32_t)buffer,
                                    camera_buffer_word_count);
    if (hal_status != HAL_OK)
    {
        camera_error = CAMERA_ERROR_START;
        camera_state = CAMERA_CAPTURE_ERROR;
        return camera_error;
    }

    /* 可变长 JPEG 通常不会填满整个 DMA 缓存，因此需要立即使能帧结束中断。 */
    __HAL_DCMI_ENABLE_IT(&hdcmi, DCMI_IT_FRAME);

    return CAMERA_ERROR_NONE;
}

void Camera_Process(void)
{
    HAL_StatusTypeDef stop_status;

    /* 主循环首次观察到 DMA 计数变化；这是受轮询延迟影响的近似时刻。 */
    if ((camera_state == CAMERA_CAPTURE_BUSY) &&
        (camera_first_progress_seen == 0U) && (camera_frame_pending == 0U) &&
        (camera_error_pending == 0U) &&
        (__HAL_DMA_GET_COUNTER(&hdma_dcmi) < camera_buffer_word_count))
    {
        camera_first_progress_tick = HAL_GetTick();
        camera_first_progress_seen = 1U;
    }

    /*
     * JPEG 长度由帧结束时的 NDTR 推导；若 NDTR 先变为 0，说明缓存已写满，
     * 应报告溢出而不是把不完整数据交给上层解析。
     */
    if ((camera_state == CAMERA_CAPTURE_BUSY) &&
        (camera_frame_pending == 0U) &&
        (__HAL_DMA_GET_COUNTER(&hdma_dcmi) == 0U))
    {
        camera_error = CAMERA_ERROR_BUFFER_OVERFLOW;
        camera_error_pending = 1U;
    }

    if (camera_error_pending != 0U)
    {
        camera_error_pending = 0U;
        (void)HAL_DCMI_Stop(&hdcmi);
        camera_state = CAMERA_CAPTURE_ERROR;
        return;
    }

    if (camera_frame_pending != 0U)
    {
        camera_frame_pending = 0U;
        stop_status = HAL_DCMI_Stop(&hdcmi);
#if (DEBUG_VERBOSE == 1)
        printf("[DCMI] Frame Event\r\n");
        printf("[DMA] NDTR: %lu\r\n",
               (unsigned long)camera_frame_dma_ndtr);
        printf("[DMA] Captured: %lu Bytes\r\n",
               (unsigned long)camera_captured_size);
        printf("[DCMI] Error: 0x%08lX\r\n",
               (unsigned long)camera_frame_dcmi_error);
        printf("[DMA] Error: 0x%08lX\r\n",
               (unsigned long)camera_frame_dma_error);
#endif
        if (stop_status != HAL_OK)
        {
            camera_error = CAMERA_ERROR_STOP;
            camera_state = CAMERA_CAPTURE_ERROR;
        }
        else if (camera_captured_size >=
                 (camera_buffer_word_count * sizeof(uint32_t)))
        {
            camera_error = CAMERA_ERROR_BUFFER_OVERFLOW;
            camera_state = CAMERA_CAPTURE_ERROR;
        }
        else
        {
            camera_state = CAMERA_CAPTURE_DONE;
        }
    }
}

void Camera_NotifyFrameEvent(DCMI_HandleTypeDef *dcmi_handle)
{
    uint32_t remaining_words;

    /*
     * 帧回调立即停用 DMA 并保存 NDTR 快照，防止可变长 JPEG 帧结束后继续写入。
     * HAL_DCMI_Stop() 和错误打印仍由 Camera_Process() 在主循环完成。
     */
    if ((dcmi_handle == &hdcmi) &&
        (camera_state == CAMERA_CAPTURE_BUSY))
    {
        camera_frame_tick = HAL_GetTick();
        __HAL_DMA_DISABLE(&hdma_dcmi);
        while ((hdma_dcmi.Instance->CR & DMA_SxCR_EN) != 0U)
        {
        }
        remaining_words = __HAL_DMA_GET_COUNTER(&hdma_dcmi);
        camera_frame_dma_ndtr = remaining_words;
        camera_frame_dcmi_error = HAL_DCMI_GetError(dcmi_handle);
        camera_frame_dma_error = HAL_DMA_GetError(&hdma_dcmi);
        camera_captured_size =
            (camera_buffer_word_count - remaining_words) * sizeof(uint32_t);
        camera_frame_pending = 1U;
    }
}

void Camera_NotifyError(DCMI_HandleTypeDef *dcmi_handle)
{
    if ((dcmi_handle == &hdcmi) &&
        (camera_state == CAMERA_CAPTURE_BUSY))
    {
        camera_frame_dma_error = HAL_DMA_GetError(&hdma_dcmi);
        camera_frame_dcmi_error = HAL_DCMI_GetError(dcmi_handle);
        camera_frame_dma_ndtr = __HAL_DMA_GET_COUNTER(&hdma_dcmi);
        hdcmi.Instance->CR &= ~DCMI_CR_CAPTURE;
        __HAL_DMA_DISABLE(&hdma_dcmi);

        if (camera_frame_dma_error != HAL_DMA_ERROR_NONE)
        {
            camera_error = CAMERA_ERROR_DMA;
        }
        else
        {
            camera_error = CAMERA_ERROR_DCMI;
        }

        camera_error_pending = 1U;
    }
}

Camera_CaptureState_t Camera_GetState(void)
{
    return camera_state;
}

Camera_Error_t Camera_GetError(void)
{
    return camera_error;
}

uint32_t Camera_GetCapturedSize(void)
{
    return camera_captured_size;
}

uint32_t Camera_GetDmaErrorSnapshot(void)
{
    return camera_frame_dma_error;
}

uint32_t Camera_GetDcmiErrorSnapshot(void)
{
    return camera_frame_dcmi_error;
}

uint32_t Camera_GetDmaRemainingSnapshot(void)
{
    return camera_frame_dma_ndtr;
}

int32_t Camera_GetFirstDmaProgressMs(void)
{
    return (camera_first_progress_seen != 0U) ?
           (int32_t)(camera_first_progress_tick - camera_capture_start_tick) : -1;
}

int32_t Camera_GetDmaProgressToFrameMs(void)
{
    int32_t elapsed = (int32_t)(camera_frame_tick - camera_first_progress_tick);
    return ((camera_first_progress_seen != 0U) && (elapsed >= 0)) ? elapsed : -1;
}

static HAL_StatusTypeDef Camera_ConfigureDma(uint32_t memory_increment,
                                             uint32_t memory_alignment)
{
    if ((hdcmi.State == HAL_DCMI_STATE_BUSY) ||
        (hdma_dcmi.State == HAL_DMA_STATE_BUSY))
    {
        return HAL_BUSY;
    }

    /*
     * RGB565 与 JPEG 的目标递增和数据宽度不同，必须完整 DeInit/Init；
     * 只改目标地址会遗留上一模式的 DMA 参数并造成花屏或缓存覆盖。
     */
    if (HAL_DMA_DeInit(&hdma_dcmi) != HAL_OK)
    {
        return HAL_ERROR;
    }

    hdma_dcmi.Instance = DMA2_Stream1;
    hdma_dcmi.Init.Channel = DMA_CHANNEL_1;
    hdma_dcmi.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_dcmi.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_dcmi.Init.MemInc = memory_increment;
    hdma_dcmi.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_dcmi.Init.MemDataAlignment = memory_alignment;
    hdma_dcmi.Init.Mode = DMA_NORMAL;
    hdma_dcmi.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    hdma_dcmi.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    hdma_dcmi.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL;
    hdma_dcmi.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_dcmi.Init.PeriphBurst = DMA_PBURST_SINGLE;

    return HAL_DMA_Init(&hdma_dcmi);
}
