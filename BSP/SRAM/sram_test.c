/** @file sram_test.c
 * @brief 外部 SRAM 上电读写自检；执行时覆盖整个器件。
 */
#include "sram_test.h"
#include <stdio.h>

static uint8_t sram_test_passed;
static uint32_t sram_failure_address, sram_failure_expected, sram_failure_actual;
static uint32_t sram_failure_stage;

/* 上电后覆盖整个外部 SRAM，仅用于当前阶段的读写验证。 */
void SRAM_Test(void)
{
  volatile uint32_t * const words = (volatile uint32_t *)0x68000000UL;
  volatile uint8_t * const bytes = (volatile uint8_t *)0x68000000UL;
  const uint32_t word_count = (1024UL * 1024UL) / sizeof(uint32_t);
  uint32_t i;
  uint32_t expected;
  uint32_t actual;
  uint32_t pass;

  printf("[SRAM] Test start (destructive, 1 MiB)\r\n");
  for (pass = 0; pass < 2U; pass++)
  {
    for (i = 0; i < word_count; i++)
    {
      expected = i ^ 0xA5A55A5AUL;
      words[i] = (pass == 0U) ? expected : ~expected;
    }
    for (i = 0; i < word_count; i++)
    {
      expected = i ^ 0xA5A55A5AUL;
      expected = (pass == 0U) ? expected : ~expected;
      actual = words[i];
      if (actual != expected)
      {
        sram_failure_stage = pass + 1U;
        sram_failure_address = 0x68000000UL + i * 4UL;
        sram_failure_expected = expected;
        sram_failure_actual = actual;
        printf("[SRAM] FAIL pass=%lu addr=0x%08lX expected=0x%08lX actual=0x%08lX\r\n",
               (unsigned long)pass, (unsigned long)(0x68000000UL + i * 4UL),
               (unsigned long)expected, (unsigned long)actual);
        sram_test_passed = 0U;
        return;
      }
    }
  }

  words[0] = 0xA55A3CC3UL;
  bytes[1] = 0x12U;
  bytes[2] = 0x34U;
  actual = words[0];
  if (actual != 0xA53412C3UL)
  {
    sram_failure_stage = 3U;
    sram_failure_address = 0x68000000UL;
    sram_failure_expected = 0xA53412C3UL;
    sram_failure_actual = actual;
    printf("[SRAM] FAIL byte lanes actual=0x%08lX\r\n", (unsigned long)actual);
    sram_test_passed = 0U;
    return;
  }
  printf("[SRAM] PASS: 1 MiB word patterns and byte lanes\r\n");
  sram_test_passed = 1U;
}

uint8_t SRAM_TestPassed(void)
{
  return sram_test_passed;
}

void SRAM_ReportResult(void)
{
  if (sram_test_passed != 0U)
  {
    printf("[SRAM] PASS\n");
  }
  else
  {
    printf("[SRAM] FAIL stage=%lu addr=0x%08lX expected=0x%08lX actual=0x%08lX\n",
           (unsigned long)sram_failure_stage, (unsigned long)sram_failure_address,
           (unsigned long)sram_failure_expected, (unsigned long)sram_failure_actual);
  }
}

