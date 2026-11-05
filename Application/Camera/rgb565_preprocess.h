#ifndef RGB565_PREPROCESS_H
#define RGB565_PREPROCESS_H

#include <stdint.h>

#define RGB565_PREPROCESS_INPUT_BYTES  (320U * 240U * 2U)
#define RGB565_PREPROCESS_OUTPUT_BYTES (160U * 160U * 3U)

/* 输入为 320x240 行优先 RGB565 半字，输出为 160x160 INT8 NCHW。
 * 采集必须已结束；输入与输出内存不能重叠。调用方保证缓冲区足够大。 */
int RGB565_Preprocess(const volatile uint16_t *frame, int8_t *output);

#endif
