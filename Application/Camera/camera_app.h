#ifndef APPLICATION_CAMERA_APP_H
#define APPLICATION_CAMERA_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dcmi.h"

typedef enum
{
    CAMERA_MODE_IDLE = 0U,
    CAMERA_MODE_RGB565_LCD,
    CAMERA_MODE_JPEG_XCAM
} Camera_Mode_t;

void CameraApp_Init(void);
void CameraApp_Process(void);
void CameraApp_NotifyFrameEvent(DCMI_HandleTypeDef *dcmi_handle);
void CameraApp_NotifyError(DCMI_HandleTypeDef *dcmi_handle);
Camera_Mode_t CameraApp_GetMode(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_CAMERA_APP_H */
