/** @file vofa.h
 * @brief USART2 FireWater 图片包与 USART1 日志发送。接口仅供主循环调用。
 */
#ifndef APPLICATION_VOFA_H
#define APPLICATION_VOFA_H
#include <stdint.h>

typedef enum { VOFA_TX_ERROR = 0, VOFA_TX_MORE, VOFA_TX_DONE } Vofa_TxResult;

/* 每次发送最多一个数据分块；在 DONE 或 ERROR 前数据缓冲必须保持有效。 */
Vofa_TxResult Vofa_SendJpegChunk(const uint8_t *data, uint32_t size,
                                 uint32_t *offset, uint16_t width, uint16_t height);
uint8_t Vofa_IsFaulted(void);
/* 主循环调用；通过 USART1 发送一条排队日志，不在中断中调用。 */
void Vofa_Process(void);
#endif
