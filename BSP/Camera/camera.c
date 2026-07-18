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
}

HAL_StatusTypeDef Camera_Stop(void)
{
    HAL_StatusTypeDef status;

    __HAL_DCMI_DISABLE_IT(&hdcmi, DCMI_IT_FRAME);
    status = HAL_DCMI_Stop(&hdcmi);
    Camera_Init();

    return status;
}

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
    return Camera_ConfigureDma(DMA_MINC_DISABLE,
                               DMA_MDATAALIGN_HALFWORD);
}

HAL_StatusTypeDef Camera_ConfigureJpegDma(void)
{
    return Camera_ConfigureDma(DMA_MINC_ENABLE,
                               DMA_MDATAALIGN_WORD);
}

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
    camera_state = CAMERA_CAPTURE_BUSY;

    __HAL_DCMI_CLEAR_FLAG(&hdcmi,
                          DCMI_FLAG_FRAMERI | DCMI_FLAG_OVRRI |
                          DCMI_FLAG_ERRRI);

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

    /* 可变长JPEG不会填满整个DMA缓存，因此需要立即使能帧结束中断。 */
    __HAL_DCMI_ENABLE_IT(&hdcmi, DCMI_IT_FRAME);

    return CAMERA_ERROR_NONE;
}

void Camera_Process(void)
{
    HAL_StatusTypeDef stop_status;

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

    if ((dcmi_handle == &hdcmi) &&
        (camera_state == CAMERA_CAPTURE_BUSY))
    {
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
        hdcmi.Instance->CR &= ~DCMI_CR_CAPTURE;
        __HAL_DMA_DISABLE(&hdma_dcmi);

        if (HAL_DMA_GetError(&hdma_dcmi) != HAL_DMA_ERROR_NONE)
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

static HAL_StatusTypeDef Camera_ConfigureDma(uint32_t memory_increment,
                                             uint32_t memory_alignment)
{
    if ((hdcmi.State == HAL_DCMI_STATE_BUSY) ||
        (hdma_dcmi.State == HAL_DMA_STATE_BUSY))
    {
        return HAL_BUSY;
    }

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
