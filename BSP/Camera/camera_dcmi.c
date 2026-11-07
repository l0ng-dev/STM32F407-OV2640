/**
 ******************************************************************************
 * @file    camera_dcmi.c
 * @brief   DCMI/DMA 摄像头采集适配模块
 *
 * @details
 * 主要功能：
 * 1. 为 RGB565 和 JPEG 配置 DCMI，使用同一套 SRAM 采集 DMA。
 * 2. 启动 Snapshot 采集并记录帧结束、DMA 计数和硬件错误。
 * 3. 向上层提供非阻塞采集状态和有效字节数。
 *
 * 模块关系：
 * Camera Application/JPEG 调用本模块；本模块复用 CubeMX 的 hdcmi 和 hdma_dcmi。
 *
 * 主要接口：
 * Camera_ConfigureSramDma()、Camera_StartRGB565SramSnapshot()、
 * Camera_StartJPEGCapture()。
 *
 * @note
 * 本模块不修改 DCMI GPIO、DMA Stream/Channel 或 CubeMX 静态配置来源。
 ******************************************************************************
 */

#include "camera_dcmi.h"

#include "dma.h"
#include <stdio.h>

#define CAMERA_JPEG_FRAME_TIMEOUT_MS   3000U
#define CAMERA_DMA_STOP_IRQ_POLLS      1024U

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
}

static HAL_StatusTypeDef Camera_StopCapture(void)
{
    HAL_StatusTypeDef status;

    __HAL_DCMI_DISABLE_IT(&hdcmi, DCMI_IT_FRAME | DCMI_IT_ERR | DCMI_IT_OVR |
                         DCMI_IT_LINE | DCMI_IT_VSYNC);
    status = HAL_DCMI_Stop(&hdcmi);
    /* HAL_DCMI_Stop 不向上传递其内部 HAL_DMA_Abort 的结果。 */
    if ((status == HAL_OK) &&
        (((hdma_dcmi.Instance->CR & DMA_SxCR_EN) != 0U) ||
         (HAL_DMA_GetError(&hdma_dcmi) == HAL_DMA_ERROR_TIMEOUT)))
    {
        return HAL_TIMEOUT;
    }
    return status;
}

HAL_StatusTypeDef Camera_Stop(void)
{
    HAL_StatusTypeDef status = Camera_StopCapture();
    if (status == HAL_OK) Camera_Init();
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

HAL_StatusTypeDef Camera_ConfigureSramDma(void)
{
    if ((hdcmi.State == HAL_DCMI_STATE_BUSY) ||
        (hdma_dcmi.State == HAL_DMA_STATE_BUSY) ||
        ((hdma_dcmi.Instance->CR & DMA_SxCR_EN) != 0U))
    {
        return HAL_BUSY;
    }

    /* 清理上一模式的 DMA 状态和回调，沿用 CubeMX 初始化的参数。 */
    if (HAL_DMA_DeInit(&hdma_dcmi) != HAL_OK) return HAL_ERROR;
    return HAL_DMA_Init(&hdma_dcmi);
}

HAL_StatusTypeDef Camera_StartRGB565SramSnapshot(uint32_t destination,
                                                 uint32_t word_count)
{
    if ((destination == 0U) || ((destination & 3U) != 0U) ||
        (word_count == 0U) || (word_count > 0xFFFFU) ||
        (hdcmi.Init.JPEGMode != DCMI_JPEG_DISABLE) ||
        (hdma_dcmi.Instance != DMA2_Stream1) ||
        (hdma_dcmi.Init.Channel != DMA_CHANNEL_1) ||
        (hdma_dcmi.Init.Mode != DMA_NORMAL) ||
        (hdma_dcmi.Init.PeriphDataAlignment != DMA_PDATAALIGN_WORD) ||
        (hdma_dcmi.Init.MemDataAlignment != DMA_MDATAALIGN_WORD) ||
        (hdma_dcmi.Init.MemInc != DMA_MINC_ENABLE))
    {
        return HAL_ERROR;
    }

    __HAL_DCMI_CLEAR_FLAG(&hdcmi, DCMI_FLAG_FRAMERI | DCMI_FLAG_OVRRI |
                          DCMI_FLAG_ERRRI);
    /* HAL 在 Snapshot 帧中断中关闭错误中断；连续重启时需重新启用。 */
    __HAL_DCMI_ENABLE_IT(&hdcmi, DCMI_IT_ERR | DCMI_IT_OVR);
    return HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT,
                              destination, word_count);
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
    camera_state = CAMERA_CAPTURE_BUSY;

    __HAL_DCMI_CLEAR_FLAG(&hdcmi,
                          DCMI_FLAG_FRAMERI | DCMI_FLAG_OVRRI |
                          DCMI_FLAG_ERRRI);

    camera_capture_start_tick = HAL_GetTick();
    /* Snapshot 帧结束后 HAL 会关闭 ERR/OVR，每帧开始前恢复。 */
    __HAL_DCMI_ENABLE_IT(&hdcmi, DCMI_IT_ERR | DCMI_IT_OVR);
    hal_status = HAL_DCMI_Start_DMA(&hdcmi,
                                    DCMI_MODE_SNAPSHOT,
                                    (uint32_t)buffer,
                                    camera_buffer_word_count);
    if (hal_status != HAL_OK)
    {
        camera_error = (Camera_StopCapture() == HAL_OK) ? CAMERA_ERROR_START : CAMERA_ERROR_STOP;
        camera_state = CAMERA_CAPTURE_ERROR;
        return camera_error;
    }

    /* 可变长 JPEG 通常不会填满整个 DMA 缓存，因此需要立即使能帧结束中断。 */
    __HAL_DCMI_ENABLE_IT(&hdcmi, DCMI_IT_FRAME);

    return CAMERA_ERROR_NONE;
}

void Camera_Process(void)
{
    /*
     * JPEG 长度由帧结束时的 NDTR 推导；若 NDTR 先变为 0，说明缓存已写满，
     * 应报告溢出而不是把不完整数据交给上层解析。
     */
    if ((camera_state == CAMERA_CAPTURE_BUSY) && (camera_frame_pending == 0U) &&
        (camera_error_pending == 0U))
    {
        Camera_Error_t error = CAMERA_ERROR_NONE;
        if (__HAL_DMA_GET_COUNTER(&hdma_dcmi) == 0U)
            error = CAMERA_ERROR_BUFFER_OVERFLOW;
        else if ((HAL_GetTick() - camera_capture_start_tick) >= CAMERA_JPEG_FRAME_TIMEOUT_MS)
            error = CAMERA_ERROR_FRAME_TIMEOUT;

        if (error != CAMERA_ERROR_NONE)
        {
            uint32_t primask = __get_PRIMASK();
            __disable_irq();
            /* 如果判断期间已收到帧或错误事件，保留中断记录。 */
            if ((camera_frame_pending == 0U) && (camera_error_pending == 0U))
            {
                camera_frame_dma_ndtr = __HAL_DMA_GET_COUNTER(&hdma_dcmi);
                camera_frame_dcmi_error = HAL_DCMI_GetError(&hdcmi);
                camera_frame_dma_error = HAL_DMA_GetError(&hdma_dcmi);
                camera_error = error;
                camera_error_pending = 1U;
            }
            __set_PRIMASK(primask);
        }
    }

    if (camera_error_pending != 0U)
    {
        camera_state = CAMERA_CAPTURE_ERROR;
        camera_error_pending = 0U;
        camera_frame_pending = 0U;
        if (Camera_StopCapture() != HAL_OK) camera_error = CAMERA_ERROR_STOP;
        return;
    }

    if (camera_frame_pending != 0U)
    {
        HAL_StatusTypeDef stop_status;
        camera_frame_pending = 0U;
        stop_status = Camera_StopCapture();
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
        else if (camera_error_pending != 0U)
        {
            camera_error_pending = 0U;
            camera_state = CAMERA_CAPTURE_ERROR;
        }
        else if ((camera_frame_dcmi_error != HAL_DCMI_ERROR_NONE) ||
                 (camera_frame_dma_error != HAL_DMA_ERROR_NONE))
        {
            camera_error = (camera_frame_dma_error != HAL_DMA_ERROR_NONE) ?
                           CAMERA_ERROR_DMA : CAMERA_ERROR_DCMI;
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
    /*
     * 帧回调立即停用 DMA 并保存 NDTR 快照，防止可变长 JPEG 帧结束后继续写入。
     * HAL_DCMI_Stop() 和错误打印仍由 Camera_Process() 在主循环完成。
     */
    if ((dcmi_handle == &hdcmi) &&
        (camera_state == CAMERA_CAPTURE_BUSY) && (camera_error_pending == 0U))
    {
        uint32_t polls = CAMERA_DMA_STOP_IRQ_POLLS;
        uint32_t remaining_words;

        __HAL_DMA_DISABLE(&hdma_dcmi);
        /* 中断优先级高于 SysTick，使用有限次数轮询。 */
        while (((hdma_dcmi.Instance->CR & DMA_SxCR_EN) != 0U) && (polls != 0U))
        {
            polls--;
        }
        remaining_words = __HAL_DMA_GET_COUNTER(&hdma_dcmi);
        camera_frame_dma_ndtr = remaining_words;
        camera_frame_dcmi_error = HAL_DCMI_GetError(dcmi_handle);
        camera_frame_dma_error = HAL_DMA_GetError(&hdma_dcmi);
        if ((hdma_dcmi.Instance->CR & DMA_SxCR_EN) != 0U)
        {
            camera_error = CAMERA_ERROR_DMA_STOP_TIMEOUT;
            camera_error_pending = 1U;
            return;
        }
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
