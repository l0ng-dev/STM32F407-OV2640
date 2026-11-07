/**
 ******************************************************************************
 * @file    camera_app.h
 * @brief   摄像头模式与采集调度接口
 *
 * @details
 * 主要功能：
 * 提供初始化、轮询处理和 DCMI 回调转发接口。
 *
 * 模块关系：
 * Core 通过本接口驱动 Camera Application，具体硬件访问由 BSP 完成。
 *
 * 主要接口：
 * CameraApp_Init()、CameraApp_Process()。
 *
 * @note
 * 具体模式状态由应用模块维护。
 ******************************************************************************
 */

#ifndef APPLICATION_CAMERA_APP_H
#define APPLICATION_CAMERA_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dcmi.h"

void CameraApp_Init(void);
void CameraApp_Process(void);
void CameraApp_NotifyFrameEvent(DCMI_HandleTypeDef *dcmi_handle);
void CameraApp_NotifyError(DCMI_HandleTypeDef *dcmi_handle);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_CAMERA_APP_H */
