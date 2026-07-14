/**
 ******************************************************************************
 * @file    ov2640.c
 * @brief   OV2640 最小 SCCB 通信验证实现
 ******************************************************************************
 */

#include "ov2640.h"

#define OV2640_REG_BANK_SELECT   0xFFU
#define OV2640_BANK_SENSOR       0x01U
#define OV2640_REG_PID           0x0AU
#define OV2640_REG_VER           0x0BU
#define OV2640_POWER_DELAY_MS       10U
#define OV2640_RESET_DELAY_MS       20U

static void OV2640_HardwareReset(void);

SCCB_Status_t OV2640_WriteReg(uint8_t register_address, uint8_t value)
{
    return SCCB_WriteReg(OV2640_SCCB_WRITE_ADDRESS, register_address, value);
}

SCCB_Status_t OV2640_ReadReg(uint8_t register_address, uint8_t *value)
{
    return SCCB_ReadReg(OV2640_SCCB_WRITE_ADDRESS, register_address, value);
}

SCCB_Status_t OV2640_Read_ID(OV2640_ID_t *id)
{
    SCCB_Status_t status;

    if (id == NULL)
    {
        return SCCB_STATUS_INVALID_PARAM;
    }

    OV2640_HardwareReset();

    status = OV2640_WriteReg(OV2640_REG_BANK_SELECT, OV2640_BANK_SENSOR);
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    status = OV2640_ReadReg(OV2640_REG_PID, &id->pid);
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    return OV2640_ReadReg(OV2640_REG_VER, &id->ver);
}

/**
 * @brief 使用 CubeMX 已配置的控制引脚完成上电和硬件复位时序
 */
static void OV2640_HardwareReset(void)
{
    HAL_GPIO_WritePin(OV_PWDN_GPIO_Port, OV_PWDN_Pin, GPIO_PIN_RESET);
    HAL_Delay(OV2640_POWER_DELAY_MS);

    HAL_GPIO_WritePin(OV_RESET_GPIO_Port, OV_RESET_Pin, GPIO_PIN_RESET);
    HAL_Delay(OV2640_RESET_DELAY_MS);
    HAL_GPIO_WritePin(OV_RESET_GPIO_Port, OV_RESET_Pin, GPIO_PIN_SET);
    HAL_Delay(OV2640_RESET_DELAY_MS);
}
