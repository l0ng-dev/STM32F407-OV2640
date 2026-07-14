/**
 ******************************************************************************
 * @file    sccb.h
 * @brief   SCCB GPIO 模拟通信接口
 ******************************************************************************
 */

#ifndef BSP_SCCB_H
#define BSP_SCCB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
 * @brief SCCB 操作状态
 */
typedef enum
{
    SCCB_STATUS_OK = 0U,
    SCCB_STATUS_INVALID_PARAM,
    SCCB_STATUS_NACK,
    SCCB_STATUS_TIMEOUT
} SCCB_Status_t;

/**
 * @brief  释放 SCCB 总线到空闲状态
 * @note   GPIO 已由 CubeMX 初始化，本函数不会重新配置 GPIO。
 * @retval SCCB 操作状态
 */
SCCB_Status_t SCCB_Init(void);

/**
 * @brief  向 SCCB 设备的单字节寄存器写入一个字节
 * @param  device_write_address 8 位写地址，最低位必须为 0
 * @param  register_address     寄存器地址
 * @param  value                待写入数据
 * @retval SCCB 操作状态
 */
SCCB_Status_t SCCB_WriteReg(uint8_t device_write_address,
                            uint8_t register_address,
                            uint8_t value);

/**
 * @brief  从 SCCB 设备的单字节寄存器读取一个字节
 * @param  device_write_address 8 位写地址，最低位必须为 0
 * @param  register_address     寄存器地址
 * @param  value                读取结果输出地址
 * @retval SCCB 操作状态
 */
SCCB_Status_t SCCB_ReadReg(uint8_t device_write_address,
                           uint8_t register_address,
                           uint8_t *value);

#ifdef __cplusplus
}
#endif

#endif /* BSP_SCCB_H */
