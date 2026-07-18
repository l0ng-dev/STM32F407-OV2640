/**
 ******************************************************************************
 * @file    sccb.c
 * @brief   基于 CubeMX GPIO 配置的 SCCB GPIO 模拟通信
 *
 * @details
 * 主要功能：
 * 1. 生成 SCCB 起始、停止、字节收发和 ACK/NACK 时序。
 * 2. 提供带超时保护的单字节寄存器读写。
 *
 * 模块关系：
 * OV2640 驱动调用本模块，本模块使用 main.h 中 CubeMX 生成的 GPIO 定义。
 *
 * 主要接口：
 * SCCB_Init()、SCCB_WriteReg()、SCCB_ReadReg()。
 *
 * @note
 * 只控制现有 GPIO 电平，不重新配置 SCCB 引脚模式。
 ******************************************************************************
 */

#include "sccb.h"

#define SCCB_BIT_DELAY_US       50U
#define SCCB_LINE_TIMEOUT_US  1000U

static void SCCB_DelayUs(uint32_t delay_us);
static SCCB_Status_t SCCB_WaitPinState(GPIO_TypeDef *gpio_port,
                                       uint16_t gpio_pin,
                                       GPIO_PinState expected_state);
static SCCB_Status_t SCCB_SetSclHigh(void);
static SCCB_Status_t SCCB_SetSdaHigh(void);
static void SCCB_SetSclLow(void);
static void SCCB_SetSdaLow(void);
static void SCCB_ReleaseSda(void);
static SCCB_Status_t SCCB_Start(void);
static SCCB_Status_t SCCB_Stop(void);
static SCCB_Status_t SCCB_SendByte(uint8_t data);
static SCCB_Status_t SCCB_ReadByte(uint8_t *data);
static SCCB_Status_t SCCB_SendNack(void);
static SCCB_Status_t SCCB_Abort(SCCB_Status_t status);

/**
 * @brief  释放 SCCB 总线到空闲状态
 * @note   PD6/PD7 已由 CubeMX 初始化，本函数只设置输出电平。
 */
SCCB_Status_t SCCB_Init(void)
{
    SCCB_Status_t status;

    /*
     * SCCB 空闲态要求 SCL/SDA 均释放为高电平；先确认总线可释放，
     * 可在摄像头未连接或线路被拉低时尽早返回超时，而不是继续写寄存器。
     */
    status = SCCB_SetSdaHigh();
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    status = SCCB_SetSclHigh();
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    SCCB_DelayUs(SCCB_BIT_DELAY_US);
    return SCCB_STATUS_OK;
}

/**
 * @brief 向 SCCB 设备的单字节寄存器写入一个字节
 */
SCCB_Status_t SCCB_WriteReg(uint8_t device_write_address,
                            uint8_t register_address,
                            uint8_t value)
{
    SCCB_Status_t status;

    if ((device_write_address & 0x01U) != 0U)
    {
        return SCCB_STATUS_INVALID_PARAM;
    }

    status = SCCB_Start();
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    status = SCCB_SendByte(device_write_address);
    if (status != SCCB_STATUS_OK)
    {
        return SCCB_Abort(status);
    }

    status = SCCB_SendByte(register_address);
    if (status != SCCB_STATUS_OK)
    {
        return SCCB_Abort(status);
    }

    status = SCCB_SendByte(value);
    if (status != SCCB_STATUS_OK)
    {
        return SCCB_Abort(status);
    }

    return SCCB_Stop();
}

/**
 * @brief 从 SCCB 设备的单字节寄存器读取一个字节
 */
SCCB_Status_t SCCB_ReadReg(uint8_t device_write_address,
                           uint8_t register_address,
                           uint8_t *value)
{
    SCCB_Status_t status;

    if ((value == NULL) || ((device_write_address & 0x01U) != 0U))
    {
        return SCCB_STATUS_INVALID_PARAM;
    }

    status = SCCB_Start();
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    status = SCCB_SendByte(device_write_address);
    if (status != SCCB_STATUS_OK)
    {
        return SCCB_Abort(status);
    }

    status = SCCB_SendByte(register_address);
    if (status != SCCB_STATUS_OK)
    {
        return SCCB_Abort(status);
    }

    status = SCCB_Stop();
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    status = SCCB_Start();
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    status = SCCB_SendByte((uint8_t)(device_write_address | 0x01U));
    if (status != SCCB_STATUS_OK)
    {
        return SCCB_Abort(status);
    }

    status = SCCB_ReadByte(value);
    if (status != SCCB_STATUS_OK)
    {
        return SCCB_Abort(status);
    }

    status = SCCB_SendNack();
    if (status != SCCB_STATUS_OK)
    {
        return SCCB_Abort(status);
    }

    return SCCB_Stop();
}

/**
 * @brief 使用有界软件循环提供保守的 SCCB 位间延时
 */
static void SCCB_DelayUs(uint32_t delay_us)
{
    volatile uint32_t loop_count;
    uint32_t loops_per_us;

    loops_per_us = SystemCoreClock / 3000000U;
    if (loops_per_us == 0U)
    {
        loops_per_us = 1U;
    }

    loop_count = loops_per_us * delay_us;
    while (loop_count > 0U)
    {
        loop_count--;
        __NOP();
    }
}

/**
 * @brief 等待指定引脚到达目标电平，防止总线卡死导致无限等待
 */
static SCCB_Status_t SCCB_WaitPinState(GPIO_TypeDef *gpio_port,
                                       uint16_t gpio_pin,
                                       GPIO_PinState expected_state)
{
    uint32_t timeout_us = SCCB_LINE_TIMEOUT_US;

    while (HAL_GPIO_ReadPin(gpio_port, gpio_pin) != expected_state)
    {
        if (timeout_us == 0U)
        {
            return SCCB_STATUS_TIMEOUT;
        }

        SCCB_DelayUs(1U);
        timeout_us--;
    }

    return SCCB_STATUS_OK;
}

static SCCB_Status_t SCCB_SetSclHigh(void)
{
    HAL_GPIO_WritePin(SCCB_SCL_GPIO_Port, SCCB_SCL_Pin, GPIO_PIN_SET);
    return SCCB_WaitPinState(SCCB_SCL_GPIO_Port, SCCB_SCL_Pin, GPIO_PIN_SET);
}

static SCCB_Status_t SCCB_SetSdaHigh(void)
{
    SCCB_ReleaseSda();
    return SCCB_WaitPinState(SCCB_SDA_GPIO_Port, SCCB_SDA_Pin, GPIO_PIN_SET);
}

static void SCCB_SetSclLow(void)
{
    HAL_GPIO_WritePin(SCCB_SCL_GPIO_Port, SCCB_SCL_Pin, GPIO_PIN_RESET);
}

static void SCCB_SetSdaLow(void)
{
    HAL_GPIO_WritePin(SCCB_SDA_GPIO_Port, SCCB_SDA_Pin, GPIO_PIN_RESET);
}

static void SCCB_ReleaseSda(void)
{
    HAL_GPIO_WritePin(SCCB_SDA_GPIO_Port, SCCB_SDA_Pin, GPIO_PIN_SET);
}

static SCCB_Status_t SCCB_Start(void)
{
    SCCB_Status_t status;

    status = SCCB_SetSdaHigh();
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    status = SCCB_SetSclHigh();
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    SCCB_DelayUs(SCCB_BIT_DELAY_US);
    SCCB_SetSdaLow();
    SCCB_DelayUs(SCCB_BIT_DELAY_US);
    SCCB_SetSclLow();
    SCCB_DelayUs(SCCB_BIT_DELAY_US);

    return SCCB_STATUS_OK;
}

static SCCB_Status_t SCCB_Stop(void)
{
    SCCB_Status_t status;

    SCCB_SetSdaLow();
    SCCB_DelayUs(SCCB_BIT_DELAY_US);

    status = SCCB_SetSclHigh();
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    SCCB_DelayUs(SCCB_BIT_DELAY_US);
    status = SCCB_SetSdaHigh();
    SCCB_DelayUs(SCCB_BIT_DELAY_US);

    return status;
}

static SCCB_Status_t SCCB_SendByte(uint8_t data)
{
    SCCB_Status_t status;
    uint8_t bit_index;

    for (bit_index = 0U; bit_index < 8U; bit_index++)
    {
        if ((data & 0x80U) != 0U)
        {
            status = SCCB_SetSdaHigh();
            if (status != SCCB_STATUS_OK)
            {
                return status;
            }
        }
        else
        {
            SCCB_SetSdaLow();
        }

        SCCB_DelayUs(SCCB_BIT_DELAY_US);
        status = SCCB_SetSclHigh();
        if (status != SCCB_STATUS_OK)
        {
            return status;
        }

        SCCB_DelayUs(SCCB_BIT_DELAY_US);
        SCCB_SetSclLow();
        SCCB_DelayUs(SCCB_BIT_DELAY_US);
        data <<= 1;
    }

    SCCB_ReleaseSda();
    SCCB_DelayUs(SCCB_BIT_DELAY_US);

    status = SCCB_SetSclHigh();
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    SCCB_DelayUs(SCCB_BIT_DELAY_US);
    status = (HAL_GPIO_ReadPin(SCCB_SDA_GPIO_Port, SCCB_SDA_Pin) == GPIO_PIN_RESET)
                 ? SCCB_STATUS_OK
                 : SCCB_STATUS_NACK;

    SCCB_SetSclLow();
    SCCB_DelayUs(SCCB_BIT_DELAY_US);

    return status;
}

static SCCB_Status_t SCCB_ReadByte(uint8_t *data)
{
    SCCB_Status_t status;
    uint8_t bit_index;
    uint8_t received = 0U;

    SCCB_ReleaseSda();
    SCCB_DelayUs(SCCB_BIT_DELAY_US);

    for (bit_index = 0U; bit_index < 8U; bit_index++)
    {
        received <<= 1;

        status = SCCB_SetSclHigh();
        if (status != SCCB_STATUS_OK)
        {
            return status;
        }

        SCCB_DelayUs(SCCB_BIT_DELAY_US);
        if (HAL_GPIO_ReadPin(SCCB_SDA_GPIO_Port, SCCB_SDA_Pin) == GPIO_PIN_SET)
        {
            received |= 0x01U;
        }

        SCCB_SetSclLow();
        SCCB_DelayUs(SCCB_BIT_DELAY_US);
    }

    *data = received;
    return SCCB_STATUS_OK;
}

static SCCB_Status_t SCCB_SendNack(void)
{
    SCCB_Status_t status;

    status = SCCB_SetSdaHigh();
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    SCCB_DelayUs(SCCB_BIT_DELAY_US);
    status = SCCB_SetSclHigh();
    if (status != SCCB_STATUS_OK)
    {
        return status;
    }

    SCCB_DelayUs(SCCB_BIT_DELAY_US);
    SCCB_SetSclLow();
    SCCB_DelayUs(SCCB_BIT_DELAY_US);

    return SCCB_STATUS_OK;
}

/**
 * @brief 尝试发送停止条件，同时保留原始错误状态
 */
static SCCB_Status_t SCCB_Abort(SCCB_Status_t status)
{
    (void)SCCB_Stop();
    return status;
}
