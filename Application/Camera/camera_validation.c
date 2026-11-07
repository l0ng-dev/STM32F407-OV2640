/** @file camera_validation.c
 * @brief USART1 单帧导出、输入回放及板端/电脑端推理对照。
 * 当前主流程未接入；调用前须确保帧缓冲不再被采集或显示改写。
 */
#include "camera_validation.h"
#include "gesture_inference.h"
#include "rgb565_preprocess.h"
#include "sram_layout.h"
#include "usart.h"
#include <stdio.h>

#if RGB565_PREPROCESS_INPUT_BYTES != SRAM_LAYOUT_RGB_SIZE || RGB565_PREPROCESS_OUTPUT_BYTES != SRAM_LAYOUT_INPUT_SIZE
#error "Validation buffers do not match the SRAM layout"
#endif

static uint8_t CameraValidation_SendBlock(const uint8_t *data, uint32_t length,
                                      uint32_t *checksum)
{
    uint32_t i;
    while (length != 0U)
    {
        uint16_t chunk = (length > 1024U) ? 1024U : (uint16_t)length;
        for (i = 0U; i < chunk; i++)
        {
            *checksum = (*checksum ^ data[i]) * 16777619UL;
        }
        if (HAL_UART_Transmit(&huart1, data, chunk, 500U) != HAL_OK)
        {
            return 0U;
        }
        data += chunk;
        length -= chunk;
    }
    return 1U;
}

static void CameraValidation_ExportRgb565(void)
{
    const uint8_t header[4] = {'R', '5', 'C', '1'};
    uint32_t checksum = 2166136261UL;
    uint8_t trailer[4];
    int8_t *tensor = (int8_t *)SRAM_LAYOUT_INPUT_BASE;

    if (RGB565_Preprocess((const volatile uint16_t *)SRAM_LAYOUT_RGB_BASE, tensor) != 0)
    {
        printf("[CAM] RGB preprocess failed\r\n");
        return;
    }
    if ((HAL_UART_Transmit(&huart1, (uint8_t *)header, sizeof(header), 500U) != HAL_OK) ||
        (CameraValidation_SendBlock((const uint8_t *)SRAM_LAYOUT_RGB_BASE,
                                RGB565_PREPROCESS_INPUT_BYTES, &checksum) == 0U) ||
        (CameraValidation_SendBlock((const uint8_t *)tensor,
                                RGB565_PREPROCESS_OUTPUT_BYTES, &checksum) == 0U))
    {
        printf("[CAM] RGB export failed\r\n");
        return;
    }
    trailer[0] = (uint8_t)checksum;
    trailer[1] = (uint8_t)(checksum >> 8);
    trailer[2] = (uint8_t)(checksum >> 16);
    trailer[3] = (uint8_t)(checksum >> 24);
    if (HAL_UART_Transmit(&huart1, trailer, sizeof(trailer), 500U) != HAL_OK)
    {
        printf("[CAM] RGB export failed\r\n");
        return;
    }
    printf("[CAM] RGB export complete\r\n");
}

static void CameraValidation_ExportInference(uint8_t use_host_input)
{
    uint8_t header[4] = {'I', '5', 'C', '1'};
    uint8_t ready[4] = {'R', 'D', 'Y', '1'};
    int8_t output[3];
    int8_t *input = (int8_t *)SRAM_LAYOUT_INPUT_BASE;
    uint8_t payload[7];
    uint8_t trailer[4];
    uint32_t elapsed, error, checksum = 2166136261UL;

    /* T 命令先握手再接收并校验主机张量；I 命令使用当前帧预处理。 */
    if (use_host_input != 0U)
    {
        uint32_t offset = 0U, host_checksum = 2166136261UL, i;
        uint8_t received_checksum[4];
        if (HAL_UART_Transmit(&huart1, ready, sizeof(ready), 500U) != HAL_OK) return;
        while (offset < RGB565_PREPROCESS_OUTPUT_BYTES)
        {
            uint32_t left = RGB565_PREPROCESS_OUTPUT_BYTES - offset;
            uint16_t chunk = (left > 1024U) ? 1024U : (uint16_t)left;
            if (HAL_UART_Receive(&huart1, (uint8_t *)input + offset, chunk, 2000U) != HAL_OK)
            {
                printf("[AI] input receive failed\r\n");
                return;
            }
            for (i = 0U; i < chunk; i++)
                host_checksum = (host_checksum ^ ((const uint8_t *)input)[offset + i]) * 16777619UL;
            offset += chunk;
        }
        if (HAL_UART_Receive(&huart1, received_checksum, sizeof(received_checksum), 2000U) != HAL_OK ||
            received_checksum[0] != (uint8_t)host_checksum ||
            received_checksum[1] != (uint8_t)(host_checksum >> 8) ||
            received_checksum[2] != (uint8_t)(host_checksum >> 16) ||
            received_checksum[3] != (uint8_t)(host_checksum >> 24))
        {
            printf("[AI] input checksum failed\r\n");
            return;
        }
    }
    else if (RGB565_Preprocess((const volatile uint16_t *)SRAM_LAYOUT_RGB_BASE, input) != 0)
    {
        printf("[AI] preprocess failed\r\n");
        return;
    }
    if (GestureInference_Run(input, output, &elapsed, &error) != 0)
    {
        printf("[AI] inference failed 0x%08lX\r\n", (unsigned long)error);
        return;
    }
    /* 返回三个 INT8 原始输出和小端毫秒数；FNV 覆盖张量及此负载。 */
    payload[0] = (uint8_t)output[0];
    payload[1] = (uint8_t)output[1];
    payload[2] = (uint8_t)output[2];
    payload[3] = (uint8_t)elapsed;
    payload[4] = (uint8_t)(elapsed >> 8);
    payload[5] = (uint8_t)(elapsed >> 16);
    payload[6] = (uint8_t)(elapsed >> 24);
    if ((HAL_UART_Transmit(&huart1, header, sizeof(header), 500U) != HAL_OK) ||
        (CameraValidation_SendBlock((const uint8_t *)input, RGB565_PREPROCESS_OUTPUT_BYTES, &checksum) == 0U) ||
        (CameraValidation_SendBlock(payload, sizeof(payload), &checksum) == 0U))
    {
        printf("[AI] export failed\r\n");
        return;
    }
    trailer[0] = (uint8_t)checksum;
    trailer[1] = (uint8_t)(checksum >> 8);
    trailer[2] = (uint8_t)(checksum >> 16);
    trailer[3] = (uint8_t)(checksum >> 24);
    if (HAL_UART_Transmit(&huart1, trailer, sizeof(trailer), 500U) != HAL_OK)
    {
        printf("[AI] export failed\r\n");
        return;
    }
    printf("[AI] logits=%d,%d,%d time=%lu ms\r\n", (int)output[0],
           (int)output[1], (int)output[2], (unsigned long)elapsed);
}

void CameraValidation_Process(void)
{
    uint8_t command;
    if (HAL_UART_Receive(&huart1, &command, 1U, 0U) != HAL_OK) return;
    if (command == (uint8_t)'P') CameraValidation_ExportRgb565();
    else if (command == (uint8_t)'I') CameraValidation_ExportInference(0U);
    else if (command == (uint8_t)'T') CameraValidation_ExportInference(1U);
}
