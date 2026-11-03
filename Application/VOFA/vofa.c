/** @file vofa.c
 * @brief USART2 FireWater 图片和有界日志队列；不在中断中发送。
 */
#include "vofa.h"
#include "usart.h"
#include <stdio.h>

#define VOFA_CHUNK_SIZE 1024U
#define VOFA_TIMEOUT_MS 100U
#define VOFA_JPEG_FORMAT 27U
#define LOG_LINE_CAPACITY 128U
#define LOG_QUEUE_CAPACITY 12U

static char log_queue[LOG_QUEUE_CAPACITY][LOG_LINE_CAPACITY];
static uint16_t log_lengths[LOG_QUEUE_CAPACITY];
static uint8_t log_head, log_tail, log_count;
static char log_line[LOG_LINE_CAPACITY];
static uint16_t log_line_length;
static uint8_t log_line_truncated;
static uint32_t log_dropped;
static uint8_t image_active;
static uint8_t stream_fault;

/* 串口输出只允许在主循环中执行；失败后锁定流，避免破坏接收端帧边界。 */
static uint8_t Vofa_Write(const uint8_t *data, uint16_t length)
{
    if (stream_fault != 0U) return 0U;
    if (HAL_UART_Transmit(&huart2, data, length, VOFA_TIMEOUT_MS) != HAL_OK)
    {
        stream_fault = 1U;
        return 0U;
    }
    return 1U;
}

Vofa_TxResult Vofa_SendJpegChunk(const uint8_t *data, uint32_t size,
                                 uint32_t *offset, uint16_t width, uint16_t height)
{
    uint32_t remaining;
    uint16_t chunk;
    if ((stream_fault != 0U) || (data == NULL) || (offset == NULL) ||
        (size == 0U) || (*offset >= size))
    {
        if (image_active != 0U) stream_fault = 1U;
        return VOFA_TX_ERROR;
    }

    if (*offset == 0U)
    {
        char header[48];
        int n;
        if (image_active != 0U)
        {
            stream_fault = 1U;
            return VOFA_TX_ERROR;
        }
        n = snprintf(header, sizeof(header), "image:0,%lu,%u,%u,%u\n",
                     (unsigned long)size, (unsigned int)width, (unsigned int)height,
                     (unsigned int)VOFA_JPEG_FORMAT);
        if ((n <= 0) || ((size_t)n >= sizeof(header))) return VOFA_TX_ERROR;
        image_active = 1U;
        if (Vofa_Write((const uint8_t *)header, (uint16_t)n) == 0U) return VOFA_TX_ERROR;
    }
    else if (image_active == 0U) return VOFA_TX_ERROR;

    remaining = size - *offset;
    chunk = (remaining > VOFA_CHUNK_SIZE) ? VOFA_CHUNK_SIZE : (uint16_t)remaining;
    if (Vofa_Write(&data[*offset], chunk) == 0U) return VOFA_TX_ERROR;
    *offset += chunk;
    if (*offset < size) return VOFA_TX_MORE;
    image_active = 0U;
    return VOFA_TX_DONE;
}

uint8_t Vofa_SendLine(const char *line, uint16_t length)
{
    if ((image_active != 0U) || (line == NULL) || (length == 0U) ||
        (line[length - 1U] != '\n')) return 0U;
    return Vofa_Write((const uint8_t *)line, length);
}

uint8_t Vofa_IsFaulted(void)
{
    return stream_fault;
}

/* printf 按完整行入队；不适用于中断或多个并发生产者。 */
int fputc(int ch, FILE *stream)
{
    (void)stream;
    if (ch == '\r') return ch;
    if (ch != '\n')
    {
        if (log_line_length < LOG_LINE_CAPACITY - 2U) log_line[log_line_length++] = (char)ch;
        else log_line_truncated = 1U;
        return ch;
    }
    if (log_line_truncated != 0U) log_line[log_line_length++] = '~';
    log_line[log_line_length++] = '\n';
    if (log_count < LOG_QUEUE_CAPACITY)
    {
        uint16_t i;
        for (i = 0U; i < log_line_length; i++) log_queue[log_head][i] = log_line[i];
        log_lengths[log_head] = log_line_length;
        log_head = (uint8_t)((log_head + 1U) % LOG_QUEUE_CAPACITY);
        log_count++;
    }
    else log_dropped++;
    log_line_length = 0U;
    log_line_truncated = 0U;
    return ch;
}

void Vofa_Process(void)
{
    if ((image_active != 0U) || (stream_fault != 0U)) return;
    if (log_count != 0U)
    {
        if (Vofa_Write((const uint8_t *)log_queue[log_tail], log_lengths[log_tail]) == 0U) return;
        log_tail = (uint8_t)((log_tail + 1U) % LOG_QUEUE_CAPACITY);
        log_count--;
    }
    else if (log_dropped != 0U)
    {
        char message[48];
        int n = snprintf(message, sizeof(message), "diag logs dropped %lu\n", (unsigned long)log_dropped);
        if ((n > 0) && ((size_t)n < sizeof(message)) &&
            (Vofa_Write((const uint8_t *)message, (uint16_t)n) != 0U)) log_dropped = 0U;
    }
}
