/**
 ******************************************************************************
 * @file    ov2640.c
 * @brief   OV2640 SCCB 驱动与完整初始化实现
 ******************************************************************************
 */

#include "ov2640.h"

#define OV2640_REG_BANK_SELECT       0xFFU
#define OV2640_BANK_DSP              0x00U
#define OV2640_BANK_SENSOR           0x01U
#define OV2640_REG_PID               0x0AU
#define OV2640_REG_VER               0x0BU
#define OV2640_SENSOR_COM7           0x12U
#define OV2640_SENSOR_SOFT_RESET     0x80U
#define OV2640_DSP_RESET             0xE0U
#define OV2640_DSP_DVP_RESET         0x04U
#define OV2640_DSP_OUT_WIDTH         0x5AU
#define OV2640_DSP_OUT_HEIGHT        0x5BU
#define OV2640_DSP_OUT_SIZE_HIGH     0x5CU
#define OV2640_POWER_DELAY_MS        10U
#define OV2640_RESET_DELAY_MS        20U
#define OV2640_SOFT_RESET_DELAY_MS   50U

typedef struct
{
    uint8_t register_address;
    uint8_t value;
} OV2640_RegisterConfig_t;

/*
 * 正点原子实验24的 ATK-MC2640 UXGA 基础初始化表。
 * 表内同时包含 Sensor Bank 与 DSP Bank 配置，基础传感器模式为 UXGA。
 */
static const OV2640_RegisterConfig_t ov2640_uxga_init_table[] =
{
    {0xFFU, 0x00U}, {0x2CU, 0xFFU}, {0x2EU, 0xDFU}, {0xFFU, 0x01U},
    {0x3CU, 0x32U}, {0x11U, 0x00U}, {0x09U, 0x02U}, {0x04U, 0xD8U},
    {0x13U, 0xE5U}, {0x14U, 0x48U}, {0x2CU, 0x0CU}, {0x33U, 0x78U},
    {0x3AU, 0x33U}, {0x3BU, 0xFBU}, {0x3EU, 0x00U}, {0x43U, 0x11U},
    {0x16U, 0x10U}, {0x39U, 0x92U}, {0x35U, 0xDAU}, {0x22U, 0x1AU},
    {0x37U, 0xC3U}, {0x23U, 0x00U}, {0x34U, 0xC0U}, {0x36U, 0x1AU},
    {0x06U, 0x88U}, {0x07U, 0xC0U}, {0x0DU, 0x87U}, {0x0EU, 0x41U},
    {0x4CU, 0x00U}, {0x48U, 0x00U}, {0x5BU, 0x00U}, {0x42U, 0x03U},
    {0x4AU, 0x81U}, {0x21U, 0x99U}, {0x24U, 0x40U}, {0x25U, 0x38U},
    {0x26U, 0x82U}, {0x5CU, 0x00U}, {0x63U, 0x00U}, {0x46U, 0x00U},
    {0x0CU, 0x3CU}, {0x61U, 0x70U}, {0x62U, 0x80U}, {0x7CU, 0x05U},
    {0x20U, 0x80U}, {0x28U, 0x30U}, {0x6CU, 0x00U}, {0x6DU, 0x80U},
    {0x6EU, 0x00U}, {0x70U, 0x02U}, {0x71U, 0x94U}, {0x73U, 0xC1U},
    {0x3DU, 0x34U}, {0x5AU, 0x57U}, {0x12U, 0x00U}, {0x17U, 0x11U},
    {0x18U, 0x75U}, {0x19U, 0x01U}, {0x1AU, 0x97U}, {0x32U, 0x36U},
    {0x03U, 0x0FU}, {0x37U, 0x40U}, {0x4FU, 0xCAU}, {0x50U, 0xA8U},
    {0x5AU, 0x23U}, {0x6DU, 0x00U}, {0x6DU, 0x38U}, {0xFFU, 0x00U},
    {0xE5U, 0x7FU}, {0xF9U, 0xC0U}, {0x41U, 0x24U}, {0xE0U, 0x14U},
    {0x76U, 0xFFU}, {0x33U, 0xA0U}, {0x42U, 0x20U}, {0x43U, 0x18U},
    {0x4CU, 0x00U}, {0x87U, 0xD5U}, {0x88U, 0x3FU}, {0xD7U, 0x03U},
    {0xD9U, 0x10U}, {0xD3U, 0x82U}, {0xC8U, 0x08U}, {0xC9U, 0x80U},
    {0x7CU, 0x00U}, {0x7DU, 0x00U}, {0x7CU, 0x03U}, {0x7DU, 0x48U},
    {0x7DU, 0x48U}, {0x7CU, 0x08U}, {0x7DU, 0x20U}, {0x7DU, 0x10U},
    {0x7DU, 0x0EU}, {0x90U, 0x00U}, {0x91U, 0x0EU}, {0x91U, 0x1AU},
    {0x91U, 0x31U}, {0x91U, 0x5AU}, {0x91U, 0x69U}, {0x91U, 0x75U},
    {0x91U, 0x7EU}, {0x91U, 0x88U}, {0x91U, 0x8FU}, {0x91U, 0x96U},
    {0x91U, 0xA3U}, {0x91U, 0xAFU}, {0x91U, 0xC4U}, {0x91U, 0xD7U},
    {0x91U, 0xE8U}, {0x91U, 0x20U}, {0x92U, 0x00U}, {0x93U, 0x06U},
    {0x93U, 0xE3U}, {0x93U, 0x05U}, {0x93U, 0x05U}, {0x93U, 0x00U},
    {0x93U, 0x04U}, {0x93U, 0x00U}, {0x93U, 0x00U}, {0x93U, 0x00U},
    {0x93U, 0x00U}, {0x93U, 0x00U}, {0x93U, 0x00U}, {0x93U, 0x00U},
    {0x96U, 0x00U}, {0x97U, 0x08U}, {0x97U, 0x19U}, {0x97U, 0x02U},
    {0x97U, 0x0CU}, {0x97U, 0x24U}, {0x97U, 0x30U}, {0x97U, 0x28U},
    {0x97U, 0x26U}, {0x97U, 0x02U}, {0x97U, 0x98U}, {0x97U, 0x80U},
    {0x97U, 0x00U}, {0x97U, 0x00U}, {0xC3U, 0xEFU}, {0xA4U, 0x00U},
    {0xA8U, 0x00U}, {0xC5U, 0x11U}, {0xC6U, 0x51U}, {0xBFU, 0x80U},
    {0xC7U, 0x10U}, {0xB6U, 0x66U}, {0xB8U, 0xA5U}, {0xB7U, 0x64U},
    {0xB9U, 0x7CU}, {0xB3U, 0xAFU}, {0xB4U, 0x97U}, {0xB5U, 0xFFU},
    {0xB0U, 0xC5U}, {0xB1U, 0x94U}, {0xB2U, 0x0FU}, {0xC4U, 0x5CU},
    {0xC0U, 0xC8U}, {0xC1U, 0x96U}, {0x8CU, 0x00U}, {0x86U, 0x3DU},
    {0x50U, 0x00U}, {0x51U, 0x90U}, {0x52U, 0x2CU}, {0x53U, 0x00U},
    {0x54U, 0x00U}, {0x55U, 0x88U}, {0x5AU, 0x90U}, {0x5BU, 0x2CU},
    {0x5CU, 0x05U}, {0xD3U, 0x02U}, {0xC3U, 0xEDU}, {0x7FU, 0x00U},
    {0xDAU, 0x09U}, {0xE5U, 0x1FU}, {0xE1U, 0x67U}, {0xE0U, 0x00U},
    {0xDDU, 0x7FU}, {0x05U, 0x00U}
};

/* 正点原子实验24的 RGB565 输出格式配置表。 */
static const OV2640_RegisterConfig_t ov2640_rgb565_table[] =
{
    {0xFFU, 0x00U}, {0xDAU, 0x09U}, {0xD7U, 0x03U}, {0xDFU, 0x02U},
    {0x33U, 0xA0U}, {0x3CU, 0x00U}, {0xE1U, 0x67U}, {0xFFU, 0x01U},
    {0xE0U, 0x00U}, {0xE1U, 0x00U}, {0xE5U, 0x00U}, {0xD7U, 0x00U},
    {0xDAU, 0x00U}, {0xE0U, 0x00U}
};

static SCCB_Status_t OV2640_WriteTable(const OV2640_RegisterConfig_t *table,
                                       uint16_t table_length);
static OV2640_Status_t OV2640_MapSccbStatus(SCCB_Status_t status);

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

OV2640_Status_t OV2640_Init(void)
{
    OV2640_ID_t id;
    SCCB_Status_t sccb_status;

    sccb_status = OV2640_WriteReg(OV2640_REG_BANK_SELECT, OV2640_BANK_SENSOR);
    if (sccb_status != SCCB_STATUS_OK)
    {
        return OV2640_MapSccbStatus(sccb_status);
    }

    sccb_status = OV2640_WriteReg(OV2640_SENSOR_COM7, OV2640_SENSOR_SOFT_RESET);
    if (sccb_status != SCCB_STATUS_OK)
    {
        return OV2640_MapSccbStatus(sccb_status);
    }

    HAL_Delay(OV2640_SOFT_RESET_DELAY_MS);

    sccb_status = OV2640_Read_ID(&id);
    if (sccb_status != SCCB_STATUS_OK)
    {
        return OV2640_MapSccbStatus(sccb_status);
    }

    if ((id.pid != OV2640_PID_EXPECTED) || (id.ver != OV2640_VER_EXPECTED))
    {
        return OV2640_STATUS_ID_MISMATCH;
    }

    sccb_status = OV2640_WriteTable(
        ov2640_uxga_init_table,
        (uint16_t)(sizeof(ov2640_uxga_init_table) / sizeof(ov2640_uxga_init_table[0])));
    if (sccb_status != SCCB_STATUS_OK)
    {
        return OV2640_MapSccbStatus(sccb_status);
    }

    sccb_status = OV2640_WriteTable(
        ov2640_rgb565_table,
        (uint16_t)(sizeof(ov2640_rgb565_table) / sizeof(ov2640_rgb565_table[0])));

    return OV2640_MapSccbStatus(sccb_status);
}

SCCB_Status_t OV2640_SetOutputSize(uint16_t width, uint16_t height)
{
    SCCB_Status_t status;
    SCCB_Status_t restore_status;
    uint16_t output_width;
    uint16_t output_height;
    uint8_t output_size_high;

    if ((width == 0U) || (height == 0U) ||
        ((width & 0x03U) != 0U) || ((height & 0x03U) != 0U))
    {
        return SCCB_STATUS_INVALID_PARAM;
    }

    output_width = width >> 2U;
    output_height = height >> 2U;
    output_size_high = (uint8_t)((output_width >> 8U) & 0x03U);
    output_size_high |= (uint8_t)((output_height >> 6U) & 0x04U);

    status = OV2640_WriteReg(OV2640_REG_BANK_SELECT, OV2640_BANK_DSP);
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    status = OV2640_WriteReg(OV2640_DSP_RESET, OV2640_DSP_DVP_RESET);
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    status = OV2640_WriteReg(OV2640_DSP_OUT_WIDTH, (uint8_t)output_width);
    if (status == SCCB_STATUS_OK)
    {
        status = OV2640_WriteReg(OV2640_DSP_OUT_HEIGHT, (uint8_t)output_height);
    }
    if (status == SCCB_STATUS_OK)
    {
        status = OV2640_WriteReg(OV2640_DSP_OUT_SIZE_HIGH, output_size_high);
    }

    restore_status = OV2640_WriteReg(OV2640_DSP_RESET, 0x00U);
    return (status != SCCB_STATUS_OK) ? status : restore_status;
}

/**
 * @brief 使用 CubeMX 已配置的控制引脚完成上电和硬件复位时序
 */
void OV2640_HardwareReset(void)
{
    HAL_GPIO_WritePin(OV_PWDN_GPIO_Port, OV_PWDN_Pin, GPIO_PIN_RESET);
    HAL_Delay(OV2640_POWER_DELAY_MS);

    HAL_GPIO_WritePin(OV_RESET_GPIO_Port, OV_RESET_Pin, GPIO_PIN_RESET);
    HAL_Delay(OV2640_RESET_DELAY_MS);
    HAL_GPIO_WritePin(OV_RESET_GPIO_Port, OV_RESET_Pin, GPIO_PIN_SET);
    HAL_Delay(OV2640_RESET_DELAY_MS);
}

/**
 * @brief  顺序写入寄存器配置表，遇到首个通信错误时停止
 */
static SCCB_Status_t OV2640_WriteTable(const OV2640_RegisterConfig_t *table,
                                       uint16_t table_length)
{
    SCCB_Status_t status;
    uint16_t index;

    if (table == NULL)
    {
        return SCCB_STATUS_INVALID_PARAM;
    }

    for (index = 0U; index < table_length; index++)
    {
        status = OV2640_WriteReg(table[index].register_address, table[index].value);
        if (status != SCCB_STATUS_OK)
        {
            return status;
        }
    }

    return SCCB_STATUS_OK;
}

/**
 * @brief 将 SCCB 状态映射为 OV2640 初始化状态
 */
static OV2640_Status_t OV2640_MapSccbStatus(SCCB_Status_t status)
{
    switch (status)
    {
        case SCCB_STATUS_OK:
            return OV2640_STATUS_OK;

        case SCCB_STATUS_NACK:
            return OV2640_STATUS_SCCB_NACK;

        case SCCB_STATUS_TIMEOUT:
            return OV2640_STATUS_SCCB_TIMEOUT;

        case SCCB_STATUS_INVALID_PARAM:
        default:
            return OV2640_STATUS_SCCB_INVALID_PARAM;
    }
}
