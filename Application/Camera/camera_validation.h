/** @file camera_validation.h
 * @brief USART1 单帧导出和推理对照接口；当前双模式主流程没有调用入口。
 */
#ifndef CAMERA_VALIDATION_H
#define CAMERA_VALIDATION_H

/* 仅在单帧 DMA 已停止、帧缓冲保持有效时，由主循环调用。
 * USART1 命令 P/I/T 的传输及推理为阻塞操作。若重新接入，须协调 USART1
 * 日志输出，避免日志混入二进制验证报文。 */
void CameraValidation_Process(void);

#endif
