/**
 ******************************************************************************
 * @file    ov2640.h
 * @brief   OV2640 SCCB 驱动与完整初始化接口
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
 * @brief OV2640 初始化状态
 */
typedef enum
{
    OV2640_STATUS_OK = 0U,
    OV2640_STATUS_SCCB_INVALID_PARAM,
    OV2640_STATUS_SCCB_NACK,
    OV2640_STATUS_SCCB_TIMEOUT,
    OV2640_STATUS_ID_MISMATCH
} OV2640_Status_t;

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
 * @brief  读取 Sensor Bank 中的 PID、VER
 * @param  id 芯片标识输出地址
 * @retval SCCB 操作状态
 */
SCCB_Status_t OV2640_Read_ID(OV2640_ID_t *id);

/**
 * @brief  使用 CubeMX 已配置的控制引脚完成硬件复位时序
 */
void OV2640_HardwareReset(void);

/**
 * @brief  完成软件复位、芯片识别和 RGB565 初始化
 * @retval OV2640 初始化状态
 */
OV2640_Status_t OV2640_Init(void);

/**
 * @brief  将已初始化的 OV2640 切换为 JPEG 输出模式
 * @note   仅写入 YUV422 和 JPEG 配置，不重新初始化 GPIO 或 SCCB
 * @retval SCCB 操作状态
 */
SCCB_Status_t OV2640_SetJPEGMode(void);

/**
 * @brief  设置 DSP 输出分辨率
 * @param  width  输出宽度，必须为 4 的倍数
 * @param  height 输出高度，必须为 4 的倍数
 * @retval SCCB 操作状态
 */
SCCB_Status_t OV2640_SetOutputSize(uint16_t width, uint16_t height);

/**
 * @brief  回读并输出 OV2640 JPEG 模式相关寄存器
 * @note   依次读取 DSP Bank、Sensor Bank，结束时恢复到 DSP Bank
 * @retval SCCB 操作状态
 */
SCCB_Status_t OV2640_Debug_ReadMode(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_OV2640_H */
