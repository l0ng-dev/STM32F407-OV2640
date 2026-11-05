#include "rgb565_preprocess.h"

#include <stddef.h>

#define SRC_WIDTH 320U
#define OUT_WIDTH 160U
#define OUT_HEIGHT 160U
#define RESIZED_HEIGHT 120U
#define TOP_PADDING 20U
#define PLANE_SIZE (OUT_WIDTH * OUT_HEIGHT)

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
static uint32_t horizontal(const volatile uint16_t *frame, uint32_t row,
                           uint32_t x, uint32_t channel)
{
    const volatile uint16_t *p = frame + row * SRC_WIDTH;
    uint32_t s;
    if (x == 0U)
    {
        s = 3U * channel_value(p[0], channel) +
            3U * channel_value(p[1], channel) + channel_value(p[2], channel);
        return (s + 3U) / 7U;
    }
    if (x == OUT_WIDTH - 1U)
    {
        s = 3U * channel_value(p[319], channel) +
            3U * channel_value(p[318], channel) + channel_value(p[317], channel);
        return (s + 3U) / 7U;
    }
    x *= 2U;
    s = channel_value(p[x - 1U], channel) + 3U * channel_value(p[x], channel) +
        3U * channel_value(p[x + 1U], channel) + channel_value(p[x + 2U], channel);
    return (s + 4U) / 8U;
}

static uint32_t resized_pixel(const volatile uint16_t *frame, uint32_t y,
                              uint32_t x, uint32_t channel)
{
    uint32_t s;
    if (y == 0U)
    {
        s = 3U * horizontal(frame, 0U, x, channel) +
            3U * horizontal(frame, 1U, x, channel) + horizontal(frame, 2U, x, channel);
        return (s + 3U) / 7U;
    }
    if (y == RESIZED_HEIGHT - 1U)
    {
        s = 3U * horizontal(frame, 239U, x, channel) +
            3U * horizontal(frame, 238U, x, channel) + horizontal(frame, 237U, x, channel);
        return (s + 3U) / 7U;
    }
    y *= 2U;
    s = horizontal(frame, y - 1U, x, channel) + 3U * horizontal(frame, y, x, channel) +
        3U * horizontal(frame, y + 1U, x, channel) + horizontal(frame, y + 2U, x, channel);
    return (s + 4U) / 8U;
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
    for (c = 0U; c < 3U; c++)
    {
        for (y = 0U; y < OUT_HEIGHT; y++)
        {
            for (x = 0U; x < OUT_WIDTH; x++)
            {
                uint32_t value = 0U;
                if ((y >= TOP_PADDING) && (y < TOP_PADDING + RESIZED_HEIGHT))
                    value = resized_pixel(frame, y - TOP_PADDING, x, c);
                output[c * PLANE_SIZE + y * OUT_WIDTH + x] = quantize(value, c);
            }
        }
    }
    return 0;
}
