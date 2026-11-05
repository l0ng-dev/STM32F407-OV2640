/**
 ******************************************************************************
 * @file    camera.h
 * @brief   DCMI/DMA 摄像头采集接口
 *
 * @details
 * 主要功能：
 * 1. 定义底层采集状态和错误码。
 * 2. 提供 RGB565 LCD 与 JPEG Buffer 的 DMA 配置和 Snapshot 接口。
 * 3. 接收统一 DCMI 回调转发并提供采集结果查询。
 *
 * 模块关系：
 * Camera Application 与 JPEG 模块调用本接口，底层使用 CubeMX HAL 句柄。
 *
 * 主要接口：
 * Camera_Stop()、Camera_SetDCMIJPEGMode()、Camera_Process()。
 *
 * @note
 * DEBUG_VERBOSE 仅控制诊断输出，不改变采集流程。
 ******************************************************************************
 */

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
HAL_StatusTypeDef Camera_ConfigureRGB565SramDma(void);
HAL_StatusTypeDef Camera_StartRGB565SramSnapshot(uint32_t destination,
                                                 uint32_t word_count);
HAL_StatusTypeDef Camera_StartRGB565Snapshot(uint32_t destination,
                                             uint32_t word_count);
Camera_Error_t Camera_StartJPEGCapture(uint8_t *buffer, uint32_t buffer_size);
void Camera_Process(void);
void Camera_NotifyFrameEvent(DCMI_HandleTypeDef *dcmi_handle);
void Camera_NotifyError(DCMI_HandleTypeDef *dcmi_handle);
Camera_CaptureState_t Camera_GetState(void);
Camera_Error_t Camera_GetError(void);
uint32_t Camera_GetCapturedSize(void);
uint32_t Camera_GetDmaErrorSnapshot(void);
uint32_t Camera_GetDcmiErrorSnapshot(void);
uint32_t Camera_GetDmaRemainingSnapshot(void);
int32_t Camera_GetFirstDmaProgressMs(void);
int32_t Camera_GetDmaProgressToFrameMs(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_CAMERA_H */
