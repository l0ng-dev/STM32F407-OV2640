/** @file gesture_inference.h
 * @brief 手势模型同步推理接口；调用方负责提供有效输入与输出缓冲。
 */
#ifndef GESTURE_INFERENCE_H
#define GESTURE_INFERENCE_H

#include <stdint.h>

/* 推理节点之间调用，不得递归进入推理；传 NULL 可关闭。 */
void GestureInference_SetService(void (*service)(void));

/* 输入保留在调用方缓冲区；output 为三个原始 INT8 logits。
 * 成功返回 0；失败返回 -1，有效参数下 error_code 接收 STAI 错误码。
 * elapsed_ms 包含推理期间执行的服务回调。 */
int GestureInference_Run(const int8_t *input, int8_t output[3],
                         uint32_t *elapsed_ms, uint32_t *error_code);

#endif
