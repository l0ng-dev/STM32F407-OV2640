#ifndef CAMERA_VALIDATION_H
#define CAMERA_VALIDATION_H

/* 仅在单帧 DMA 已停止、帧缓冲保持有效时，由主循环调用。
 * USART1 命令 P/I/T 的传输及推理为阻塞操作。 */
void CameraValidation_Process(void);

#endif
