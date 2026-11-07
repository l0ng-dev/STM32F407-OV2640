/** @file rgb565_preprocess.c
 * @brief RGB565 帧缩放、黑边填充及模型输入量化。
 * 输出为 160×160、RGB 通道优先的 INT8 张量，与当前模型输入契约对应。
 */
#include "rgb565_preprocess.h"

#include <stddef.h>

#define SRC_WIDTH 320U
#define OUT_WIDTH 160U
#define OUT_HEIGHT 160U
#define RESIZED_HEIGHT 120U
#define TOP_PADDING 20U
#define PLANE_SIZE (OUT_WIDTH * OUT_HEIGHT)

static int8_t quantized_values[3][256];
static uint8_t quantized_values_ready;

/* RGB565 通道展开规则；最终字节序和色彩须通过同一实帧对照确认。 */
static uint32_t channel_value(uint16_t pixel, uint32_t channel)
{
    uint32_t value;
    if (channel == 1U)
    {
        value = (pixel >> 5) & 63U;
        return (value << 2) | (value >> 4);
    }
    value = (channel == 0U) ? ((pixel >> 11) & 31U) : (pixel & 31U);
    return (value << 3) | (value >> 2);
}

/* 与 Pillow 的 2:1 双线性缩小一致：横向先舍入，边缘重新归一化。 */
static void horizontal(const volatile uint16_t *frame, uint32_t row,
                       uint32_t x, uint32_t result[3])
{
    const volatile uint16_t *p = frame + row * SRC_WIDTH;
    uint16_t pixels[4];
    uint32_t weights[4], count, divisor, rounding, i, c;
    uint32_t sums[3] = {0U, 0U, 0U};
    if (x == 0U)
    {
        pixels[0] = p[0]; pixels[1] = p[1]; pixels[2] = p[2];
        weights[0] = 3U; weights[1] = 3U; weights[2] = 1U;
        count = 3U; divisor = 7U; rounding = 3U;
    }
    else if (x == OUT_WIDTH - 1U)
    {
        pixels[0] = p[319]; pixels[1] = p[318]; pixels[2] = p[317];
        weights[0] = 3U; weights[1] = 3U; weights[2] = 1U;
        count = 3U; divisor = 7U; rounding = 3U;
    }
    else
    {
        x *= 2U;
        pixels[0] = p[x - 1U]; pixels[1] = p[x];
        pixels[2] = p[x + 1U]; pixels[3] = p[x + 2U];
        weights[0] = 1U; weights[1] = 3U; weights[2] = 3U; weights[3] = 1U;
        count = 4U; divisor = 8U; rounding = 4U;
    }
    for (i = 0U; i < count; i++)
        for (c = 0U; c < 3U; c++)
            sums[c] += weights[i] * channel_value(pixels[i], c);
    for (c = 0U; c < 3U; c++) result[c] = (sums[c] + rounding) / divisor;
}

static void resized_pixel(const volatile uint16_t *frame, uint32_t y,
                          uint32_t x, uint32_t result[3])
{
    uint32_t rows[4], weights[4], count, divisor, rounding, i, c;
    uint32_t sums[3] = {0U, 0U, 0U}, values[3];
    if (y == 0U)
    {
        rows[0] = 0U; rows[1] = 1U; rows[2] = 2U;
        weights[0] = 3U; weights[1] = 3U; weights[2] = 1U;
        count = 3U; divisor = 7U; rounding = 3U;
    }
    else if (y == RESIZED_HEIGHT - 1U)
    {
        rows[0] = 239U; rows[1] = 238U; rows[2] = 237U;
        weights[0] = 3U; weights[1] = 3U; weights[2] = 1U;
        count = 3U; divisor = 7U; rounding = 3U;
    }
    else
    {
        y *= 2U;
        rows[0] = y - 1U; rows[1] = y; rows[2] = y + 1U; rows[3] = y + 2U;
        weights[0] = 1U; weights[1] = 3U; weights[2] = 3U; weights[3] = 1U;
        count = 4U; divisor = 8U; rounding = 4U;
    }
    for (i = 0U; i < count; i++)
    {
        horizontal(frame, rows[i], x, values);
        for (c = 0U; c < 3U; c++) sums[c] += weights[i] * values[c];
    }
    for (c = 0U; c < 3U; c++) result[c] = (sums[c] + rounding) / divisor;
}

static int8_t quantize(uint32_t value, uint32_t channel)
{
    static const float mean[3] = {0.485f, 0.456f, 0.406f};
    static const float stddev[3] = {0.229f, 0.224f, 0.225f};
    float q = (((float)value / 255.0f - mean[channel]) / stddev[channel]) /
              0.01865844801068306f;
    int32_t base = (int32_t)q;
    float fraction;
    if (q < (float)base) base--;
    fraction = q - (float)base;
    /* 恰好半整数时舍入到偶数，再叠加零点并饱和。 */
    if ((fraction > 0.5f) || ((fraction == 0.5f) && ((base & 1) != 0))) base++;
    base -= 14;
    if (base < -128) base = -128;
    if (base > 127) base = 127;
    return (int8_t)base;
}

int RGB565_Preprocess(const volatile uint16_t *frame, int8_t *output)
{
    uint32_t c, y, x;
    if ((frame == NULL) || (output == NULL)) return -1;
    /* 每个通道仅有 256 个输入值，首次调用时生成查表，后续帧复用。 */
    if (quantized_values_ready == 0U)
    {
        for (c = 0U; c < 3U; c++)
            for (x = 0U; x < 256U; x++)
                quantized_values[c][x] = quantize(x, c);
        quantized_values_ready = 1U;
    }
    for (y = 0U; y < OUT_HEIGHT; y++)
    {
        for (x = 0U; x < OUT_WIDTH; x++)
        {
            uint32_t values[3] = {0U, 0U, 0U};
            /* 320×240 缩至 160×120，上下各 20 行黑边，再逐通道量化。 */
            if ((y >= TOP_PADDING) && (y < TOP_PADDING + RESIZED_HEIGHT))
                resized_pixel(frame, y - TOP_PADDING, x, values);
            for (c = 0U; c < 3U; c++)
                output[c * PLANE_SIZE + y * OUT_WIDTH + x] = quantized_values[c][values[c]];
        }
    }
    return 0;
}
