#ifndef GESTURE_INFERENCE_H
#define GESTURE_INFERENCE_H

#include <stdint.h>

/* 输入保留在调用方缓冲区；output 为三个原始 INT8 logits。 */
int GestureInference_Run(const int8_t *input, int8_t output[3],
                         uint32_t *elapsed_ms, uint32_t *error_code);

#endif
