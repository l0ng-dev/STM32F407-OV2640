#ifndef BSP_CAMERA_H
#define BSP_CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dcmi.h"

#ifndef DEBUG_VERBOSE
#define DEBUG_VERBOSE  0
#endif

typedef enum
{
    CAMERA_CAPTURE_IDLE = 0U,
    CAMERA_CAPTURE_BUSY,
    CAMERA_CAPTURE_DONE,
    CAMERA_CAPTURE_ERROR
} Camera_CaptureState_t;

typedef enum
{
    CAMERA_ERROR_NONE = 0U,
    CAMERA_ERROR_INVALID_PARAM,
    CAMERA_ERROR_DCMI_CONFIG,
    CAMERA_ERROR_DMA_CONFIG,
    CAMERA_ERROR_START,
    CAMERA_ERROR_BUFFER_OVERFLOW,
    CAMERA_ERROR_DCMI,
    CAMERA_ERROR_DMA,
    CAMERA_ERROR_STOP
} Camera_Error_t;

void Camera_Init(void);
HAL_StatusTypeDef Camera_Stop(void);
HAL_StatusTypeDef Camera_SetDCMIJPEGMode(uint32_t jpeg_mode);
HAL_StatusTypeDef Camera_ConfigureRGB565LcdDma(void);
HAL_StatusTypeDef Camera_ConfigureJpegDma(void);
HAL_StatusTypeDef Camera_StartRGB565Snapshot(uint32_t destination,
                                             uint32_t word_count);
Camera_Error_t Camera_StartJPEGCapture(uint8_t *buffer, uint32_t buffer_size);
void Camera_Process(void);
void Camera_NotifyFrameEvent(DCMI_HandleTypeDef *dcmi_handle);
void Camera_NotifyError(DCMI_HandleTypeDef *dcmi_handle);
Camera_CaptureState_t Camera_GetState(void);
Camera_Error_t Camera_GetError(void);
uint32_t Camera_GetCapturedSize(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_CAMERA_H */
