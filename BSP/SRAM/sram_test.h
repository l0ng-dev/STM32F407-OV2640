/** @file sram_test.h
 * @brief 外部 SRAM 自检接口；必须在 FSMC 初始化之后调用。
 */
#ifndef BSP_SRAM_TEST_H
#define BSP_SRAM_TEST_H
#include <stdint.h>

/* 覆盖 0x68000000 起 1 MiB；不能与其他 SRAM 用户并发执行。 */
void SRAM_Test(void);
uint8_t SRAM_TestPassed(void);
void SRAM_ReportResult(void);
#endif
