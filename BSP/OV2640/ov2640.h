/**
 ******************************************************************************
 * @file    ov2640.h
 * @brief   OV2640 最小 SCCB 通信验证接口
 ******************************************************************************
 */

#ifndef BSP_OV2640_H
#define BSP_OV2640_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sccb.h"

#define OV2640_SCCB_WRITE_ADDRESS  0x60U
#define OV2640_PID_EXPECTED        0x26U
#define OV2640_VER_EXPECTED        0x42U

/**
 * @brief OV2640 芯片标识
 */
typedef struct
{
    uint8_t pid;
    uint8_t ver;
} OV2640_ID_t;

/**
 * @brief  写入一个 OV2640 寄存器
 * @param  register_address 寄存器地址
 * @param  value            待写入数据
 * @retval SCCB 操作状态
 */
SCCB_Status_t OV2640_WriteReg(uint8_t register_address, uint8_t value);

/**
 * @brief  读取一个 OV2640 寄存器
 * @param  register_address 寄存器地址
 * @param  value            读取结果输出地址
 * @retval SCCB 操作状态
 */
SCCB_Status_t OV2640_ReadReg(uint8_t register_address, uint8_t *value);

/**
 * @brief  执行最小硬件复位并读取 PID、VER
 * @param  id 芯片标识输出地址
 * @retval SCCB 操作状态
 */
SCCB_Status_t OV2640_Read_ID(OV2640_ID_t *id);

#ifdef __cplusplus
}
#endif

#endif /* BSP_OV2640_H */
