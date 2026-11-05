/**
 ******************************************************************************
 * @file    camera_app.h
 * @brief   摄像头模式与采集调度接口
 *
 * @details
 * 主要功能：
 * 1. 定义 Camera Application 运行模式。
 * 2. 提供初始化、轮询处理和 DCMI 回调转发接口。
 *
 * 模块关系：
 * Core 通过本接口驱动 Camera Application，具体硬件访问由 BSP 完成。
 *
 * 主要接口：
 * CameraApp_Init()、CameraApp_Process()、CameraApp_GetMode()。
 *
 * @note
 * 本接口只描述应用状态，不修改底层硬件配置。
 ******************************************************************************
 */

#ifndef APPLICATION_CAMERA_APP_H
#define APPLICATION_CAMERA_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dcmi.h"

/**
 * @brief Camera Application 运行模式
 * @note  IDLE 表示外设已初始化但未采集；其余三种状态对应各自的
 *        OV2640、DCMI 和 DMA 运行参数，不能直接复用 DMA 目标地址。
 */
typedef enum
{
    CAMERA_MODE_IDLE = 0U,
    CAMERA_MODE_RGB565_LCD,
    CAMERA_MODE_JPEG_VOFA,
    CAMERA_MODE_RGB565_SRAM
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
