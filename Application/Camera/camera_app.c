/**
 ******************************************************************************
 * @file    camera_app.c
 * @brief   Camera 双模式应用管理模块
 *
 * @details
 * 主要功能：
 * 1. 管理 IDLE、RGB565 LCD 和 JPEG VOFA+ 三种运行状态。
 * 2. 处理 WKUP/KEY0 按键事件及模式安全切换。
 * 3. 协调 OV2640、DCMI/DMA、LCD、JPEG 解析和 USART2 发送。
 *
 * 模块关系：
 * main.c 调用本模块；本模块调用 Camera BSP、OV2640、JPEG 和 LCD 模块。
 * USART2 使用 VOFA+ FireWater 格式发送图像和状态。
 *
 * 主要接口：
 * CameraApp_Init()、CameraApp_Process()、CameraApp_NotifyFrameEvent()。
 *
 * @note
 * 模式切换前先停止 DCMI/DMA，再切换传感器格式并重配 DMA；
 * 不修改 CubeMX 底层外设和已验证的 LCD/FSMC 配置。
 ******************************************************************************
 */

#include "camera_app.h"

#include "camera.h"
#include "jpeg_capture.h"
#include "lcd.h"
#include "main.h"
#include "ov2640.h"
#include "sccb.h"
#include "sram_test.h"
#include "vofa.h"

#include <stdio.h>

#define CAMERA_RGB_WIDTH              320U
#define CAMERA_RGB_HEIGHT             240U
#define CAMERA_RGB_BYTES_PER_PIXEL    2U
#define CAMERA_RGB_WORD_COUNT         ((CAMERA_RGB_WIDTH * CAMERA_RGB_HEIGHT * \
                                        CAMERA_RGB_BYTES_PER_PIXEL) / sizeof(uint32_t))
#define CAMERA_JPEG_WIDTH             320U
#define CAMERA_JPEG_HEIGHT            240U
#define CAMERA_KEY_DEBOUNCE_MS        30U
#define CAMERA_JPEG_SETTLE_MS         50U
/* 外部 SRAM 采集试验：JPEG 模式将传感器输入时钟二分频。 */
#define CAMERA_JPEG_SENSOR_CLKRC      0x01U

typedef struct
{
    uint8_t stable_pressed;
    uint8_t candidate_pressed;
    uint32_t candidate_tick;
} CameraApp_KeyState_t;

/*
 * camera_mode 表示已经生效的硬件状态，camera_requested_mode 表示按键请求。
 * 将“请求”与“当前状态”分开，可确保所有模式变化都经过统一的停止与重配流程，
 * 避免在 DCMI/DMA 正忙时直接切换目标地址或传感器输出格式。
 */
static volatile Camera_Mode_t camera_mode = CAMERA_MODE_IDLE;
static Camera_Mode_t camera_requested_mode = CAMERA_MODE_IDLE;
static volatile uint8_t rgb_frame_pending;
static volatile uint8_t dcmi_error_pending;
static volatile uint32_t dcmi_error_code;
static uint8_t camera_sensor_ready;
static CameraApp_KeyState_t wkup_key;
static CameraApp_KeyState_t key0_key;
static uint8_t jpeg_tx_active;
static uint32_t jpeg_tx_offset;
static uint32_t jpeg_tx_size;
static uint8_t status_sent;
static uint32_t last_status_tick;
static uint32_t jpeg_capture_start_tick;
static uint32_t jpeg_capture_ms;
static uint32_t jpeg_send_start_tick;

static uint8_t CameraApp_InitSensor(void);
static void CameraApp_InitLcd(void);
static void CameraApp_ShowLcdId(uint16_t id);
static void CameraApp_ScanKeys(void);
static uint8_t CameraApp_UpdateKey(CameraApp_KeyState_t *key,
                                   uint8_t pressed,
                                   uint32_t now);
static void CameraApp_StopCurrentMode(void);
static uint8_t CameraApp_StartRgb565Mode(void);
static uint8_t CameraApp_StartJpegMode(void);
static HAL_StatusTypeDef CameraApp_StartRgb565Snapshot(void);
static void CameraApp_ProcessRgb565(void);
static void CameraApp_ProcessJpeg(void);
static uint8_t CameraApp_TransmitJpegChunk(void);
static void CameraApp_EnterIdleWithError(const char *operation,
                                         uint32_t error_code);
static void CameraApp_Diagnostic(const char *operation, uint32_t value);

void CameraApp_Init(void)
{
    uint32_t now = HAL_GetTick();

    printf("[CAM] Start\r\n");

    camera_mode = CAMERA_MODE_IDLE;
    camera_requested_mode = CAMERA_MODE_IDLE;
    rgb_frame_pending = 0U;
    dcmi_error_pending = 0U;
    dcmi_error_code = HAL_DCMI_ERROR_NONE;
    jpeg_tx_active = 0U;
    jpeg_tx_offset = 0U;
    jpeg_tx_size = 0U;

    wkup_key.stable_pressed =
        (HAL_GPIO_ReadPin(WKUP_GPIO_Port, WKUP_Pin) == GPIO_PIN_SET) ? 1U : 0U;
    wkup_key.candidate_pressed = wkup_key.stable_pressed;
    wkup_key.candidate_tick = now;

    key0_key.stable_pressed =
        (HAL_GPIO_ReadPin(KEY0_GPIO_Port, KEY0_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
    key0_key.candidate_pressed = key0_key.stable_pressed;
    key0_key.candidate_tick = now;

    Camera_Init();
    JPEG_Capture_Init();
    /* 先完成硬件复位，再释放 SCCB 总线并写入 RGB565 基础配置。 */
    camera_sensor_ready = CameraApp_InitSensor();
    CameraApp_InitLcd();

    printf("[CAM] LCD ready, mode IDLE\r\n");
}

void CameraApp_Process(void)
{
    if ((Vofa_IsFaulted() != 0U) && (camera_mode == CAMERA_MODE_JPEG_VOFA))
    {
        CameraApp_StopCurrentMode();
        camera_requested_mode = CAMERA_MODE_IDLE;
    }
    CameraApp_ScanKeys();

    /*
     * 只有请求模式与当前模式不一致时才执行切换。
     * JPEG 正在发送时先完成整帧，再执行模式切换。
     */
    if (camera_requested_mode != camera_mode)
    {
        if ((camera_mode == CAMERA_MODE_JPEG_VOFA) &&
            (jpeg_tx_active != 0U))
        {
            if (CameraApp_TransmitJpegChunk() == 0U)
            {
                return;
            }
        }

        if (camera_requested_mode == CAMERA_MODE_RGB565_LCD)
        {
            (void)CameraApp_StartRgb565Mode();
        }
        else if (camera_requested_mode == CAMERA_MODE_JPEG_VOFA)
        {
            (void)CameraApp_StartJpegMode();
        }
        else
        {
            CameraApp_StopCurrentMode();
        }

        return;
    }

    if (camera_mode == CAMERA_MODE_RGB565_LCD)
    {
        CameraApp_ProcessRgb565();
    }
    else if (camera_mode == CAMERA_MODE_JPEG_VOFA)
    {
        CameraApp_ProcessJpeg();
    }
}

void CameraApp_NotifyFrameEvent(DCMI_HandleTypeDef *dcmi_handle)
{
    if ((dcmi_handle == NULL) || (dcmi_handle->Instance != DCMI))
    {
        return;
    }

    /* 中断上下文只设置标志或转发事件，耗时处理统一留在主循环。 */
    if (camera_mode == CAMERA_MODE_RGB565_LCD)
    {
        rgb_frame_pending = 1U;
    }
    else if (camera_mode == CAMERA_MODE_JPEG_VOFA)
    {
        Camera_NotifyFrameEvent(dcmi_handle);
    }
}

void CameraApp_NotifyError(DCMI_HandleTypeDef *dcmi_handle)
{
    if ((dcmi_handle == NULL) || (dcmi_handle->Instance != DCMI))
    {
        return;
    }

    if (camera_mode == CAMERA_MODE_JPEG_VOFA)
    {
        Camera_NotifyError(dcmi_handle);
    }
    else if (camera_mode == CAMERA_MODE_RGB565_LCD)
    {
        dcmi_error_code = HAL_DCMI_GetError(dcmi_handle);
        dcmi_error_pending = 1U;
    }
}

Camera_Mode_t CameraApp_GetMode(void)
{
    return camera_mode;
}

static uint8_t CameraApp_InitSensor(void)
{
    SCCB_Status_t sccb_status;
    OV2640_Status_t init_status;

    OV2640_HardwareReset();
    sccb_status = SCCB_Init();
    if (sccb_status != SCCB_STATUS_OK)
    {
        printf("[CAM] Error: OV2640 SCCB Init Failed (%u)\r\n",
               (unsigned int)sccb_status);
        return 0U;
    }

    init_status = OV2640_Init();
    if (init_status != OV2640_STATUS_OK)
    {
        printf("[CAM] Error: OV2640 Init Failed (%u)\r\n",
               (unsigned int)init_status);
        return 0U;
    }

    printf("[CAM] OV2640 Init OK\r\n");
    return 1U;
}

static void CameraApp_InitLcd(void)
{
    lcd_init();
    lcd_display_dir(1U);
    lcd_clear(WHITE);
    lcd_show_string(20, 20, 200, 24, 24, "LCD TEST", BLACK);
    lcd_show_string(20, 50, 200, 24, 24, "FSMC OK", BLUE);
    CameraApp_ShowLcdId(lcddev.id);
    lcd_fill(20, 120, 70, 180, RED);
    lcd_fill(90, 120, 140, 180, GREEN);
    lcd_fill(160, 120, 210, 180, BLUE);
}

static void CameraApp_ShowLcdId(uint16_t id)
{
    static const char hex_digits[] = "0123456789ABCDEF";
    char id_text[] = "LCD ID: 0x0000";

    id_text[10] = hex_digits[(id >> 12) & 0x0FU];
    id_text[11] = hex_digits[(id >> 8) & 0x0FU];
    id_text[12] = hex_digits[(id >> 4) & 0x0FU];
    id_text[13] = hex_digits[id & 0x0FU];
    lcd_show_string(20, 80, 200, 24, 24, id_text, BLACK);
}

static void CameraApp_ScanKeys(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t wkup_pressed;
    uint8_t key0_pressed;

    wkup_pressed =
        (HAL_GPIO_ReadPin(WKUP_GPIO_Port, WKUP_Pin) == GPIO_PIN_SET) ? 1U : 0U;
    key0_pressed =
        (HAL_GPIO_ReadPin(KEY0_GPIO_Port, KEY0_Pin) == GPIO_PIN_RESET) ? 1U : 0U;

    /* 消抖完成后的按下沿只产生模式请求，不在按键处理处直接操作 DCMI。 */
    if (CameraApp_UpdateKey(&wkup_key, wkup_pressed, now) != 0U)
    {
        camera_requested_mode = CAMERA_MODE_RGB565_LCD;
    }

    if (CameraApp_UpdateKey(&key0_key, key0_pressed, now) != 0U)
    {
        camera_requested_mode = CAMERA_MODE_JPEG_VOFA;
    }
}

static uint8_t CameraApp_UpdateKey(CameraApp_KeyState_t *key,
                                   uint8_t pressed,
                                   uint32_t now)
{
    if (pressed != key->candidate_pressed)
    {
        key->candidate_pressed = pressed;
        key->candidate_tick = now;
        return 0U;
    }

    if ((pressed != key->stable_pressed) &&
        ((now - key->candidate_tick) >= CAMERA_KEY_DEBOUNCE_MS))
    {
        key->stable_pressed = pressed;
        return pressed;
    }

    return 0U;
}

static void CameraApp_StopCurrentMode(void)
{
    /*
     * 切换前清除上一个模式的帧标志和发送进度，再停止 DCMI/DMA。
     * 这样新模式不会误用旧模式遗留的帧完成事件或 JPEG 长度。
     */
    camera_mode = CAMERA_MODE_IDLE;
    rgb_frame_pending = 0U;
    dcmi_error_pending = 0U;
    jpeg_tx_active = 0U;
    jpeg_tx_offset = 0U;
    jpeg_tx_size = 0U;
    (void)Camera_Stop();
}

static uint8_t CameraApp_StartRgb565Mode(void)
{
    SCCB_Status_t sccb_status;
    OV2640_Status_t init_status;

    /*
     * RGB565 模式必须恢复 OV2640 RGB565 表、关闭 DCMI JPEG 位，
     * 并把 DMA 目标配置为不递增的 LCD GRAM 半字写入。
     */
    CameraApp_StopCurrentMode();
    printf("[CAM] Switch RGB565 LCD\r\n");

    if (camera_sensor_ready == 0U)
    {
        camera_sensor_ready = CameraApp_InitSensor();
        if (camera_sensor_ready == 0U)
        {
            CameraApp_EnterIdleWithError("RGB sensor init", 0U);
            return 0U;
        }
    }
    else
    {
        init_status = OV2640_Init();
        if (init_status != OV2640_STATUS_OK)
        {
            camera_sensor_ready = 0U;
            CameraApp_EnterIdleWithError("RGB mode", (uint32_t)init_status);
            return 0U;
        }
    }

    sccb_status = OV2640_SetOutputSize(CAMERA_RGB_WIDTH,
                                      CAMERA_RGB_HEIGHT);
    if (sccb_status != SCCB_STATUS_OK)
    {
        CameraApp_EnterIdleWithError("RGB size", (uint32_t)sccb_status);
        return 0U;
    }

    if (Camera_SetDCMIJPEGMode(DCMI_JPEG_DISABLE) != HAL_OK)
    {
        CameraApp_EnterIdleWithError("RGB DCMI", HAL_DCMI_GetError(&hdcmi));
        return 0U;
    }

    if (Camera_ConfigureRGB565LcdDma() != HAL_OK)
    {
        CameraApp_EnterIdleWithError("RGB DMA", HAL_DMA_GetError(hdcmi.DMA_Handle));
        return 0U;
    }

    camera_mode = CAMERA_MODE_RGB565_LCD;
    if (CameraApp_StartRgb565Snapshot() != HAL_OK)
    {
        CameraApp_EnterIdleWithError("RGB start", HAL_DCMI_GetError(&hdcmi));
        return 0U;
    }

    printf("[LCD] Preview Start\r\n");
    return 1U;
}

static uint8_t CameraApp_StartJpegMode(void)
{
    SCCB_Status_t sccb_status;
    JPEG_Error_t jpeg_status;

    if (Vofa_IsFaulted() != 0U)
    {
        camera_requested_mode = CAMERA_MODE_IDLE;
        return 0U;
    }

    /*
     * JPEG 模式需要可变长字节流连续写入 SRAM，因此开启 DCMI JPEG 位，
     * DMA 使用 Word 对齐且目标地址递增；与 LCD 直写参数不能混用。
     */
    CameraApp_StopCurrentMode();
    printf("[CAM] Switch JPEG VOFA+\r\n");
    SRAM_ReportResult();

    if (camera_sensor_ready == 0U)
    {
        camera_sensor_ready = CameraApp_InitSensor();
        if (camera_sensor_ready == 0U)
        {
            CameraApp_EnterIdleWithError("JPEG sensor init", 0U);
            return 0U;
        }
    }

    sccb_status = OV2640_SetJPEGMode();
    if (sccb_status != SCCB_STATUS_OK)
    {
        CameraApp_EnterIdleWithError("JPEG sensor mode", (uint32_t)sccb_status);
        return 0U;
    }

    sccb_status = OV2640_SetOutputSize(CAMERA_JPEG_WIDTH,
                                      CAMERA_JPEG_HEIGHT);
    if (sccb_status != SCCB_STATUS_OK)
    {
        CameraApp_EnterIdleWithError("JPEG size", (uint32_t)sccb_status);
        return 0U;
    }

    /* 仅 JPEG 模式降低传感器时钟；RGB 模式重新初始化时恢复原配置。 */
    sccb_status = OV2640_WriteReg(0xFFU, 0x01U);
    if (sccb_status == SCCB_STATUS_OK)
    {
        sccb_status = OV2640_WriteReg(0x11U, CAMERA_JPEG_SENSOR_CLKRC);
    }
    if (sccb_status == SCCB_STATUS_OK)
    {
        sccb_status = OV2640_WriteReg(0xFFU, 0x00U);
    }
    if (sccb_status != SCCB_STATUS_OK)
    {
        CameraApp_EnterIdleWithError("JPEG clock", (uint32_t)sccb_status);
        return 0U;
    }

    HAL_Delay(CAMERA_JPEG_SETTLE_MS);

    if (Camera_SetDCMIJPEGMode(DCMI_JPEG_ENABLE) != HAL_OK)
    {
        CameraApp_EnterIdleWithError("JPEG DCMI", HAL_DCMI_GetError(&hdcmi));
        return 0U;
    }

    if (Camera_ConfigureJpegDma() != HAL_OK)
    {
        CameraApp_EnterIdleWithError("JPEG DMA", HAL_DMA_GetError(hdcmi.DMA_Handle));
        return 0U;
    }

    JPEG_Capture_Init();
    status_sent = 0U;
    camera_mode = CAMERA_MODE_JPEG_VOFA;
    jpeg_capture_start_tick = HAL_GetTick();
    jpeg_status = JPEG_Capture_Start();
    if (jpeg_status != JPEG_ERROR_NONE)
    {
        CameraApp_EnterIdleWithError("JPEG start", (uint32_t)jpeg_status);
        return 0U;
    }

    printf("[JPEG] VOFA+ Start\r\n");
    return 1U;
}

static HAL_StatusTypeDef CameraApp_StartRgb565Snapshot(void)
{
    lcd_set_window(0U, 0U, CAMERA_RGB_WIDTH, CAMERA_RGB_HEIGHT);
    lcd_write_ram_prepare();

    return Camera_StartRGB565Snapshot((uint32_t)&LCD->LCD_RAM,
                                      CAMERA_RGB_WORD_COUNT);
}

static void CameraApp_ProcessRgb565(void)
{
    if (dcmi_error_pending != 0U)
    {
        uint32_t error_code = dcmi_error_code;

        dcmi_error_pending = 0U;
        CameraApp_EnterIdleWithError("RGB DCMI runtime", error_code);
        return;
    }

    if (rgb_frame_pending != 0U)
    {
        rgb_frame_pending = 0U;
#if (DEBUG_VERBOSE == 1)
        printf("[DCMI] RGB Frame OK\r\n");
        printf("[LCD] Frame Drawn\r\n");
#endif

        if ((camera_requested_mode == CAMERA_MODE_RGB565_LCD) &&
            (CameraApp_StartRgb565Snapshot() != HAL_OK))
        {
            CameraApp_EnterIdleWithError("RGB restart",
                                         HAL_DCMI_GetError(&hdcmi));
        }
    }
}

static void CameraApp_ProcessJpeg(void)
{
    JPEG_CaptureState_t capture_state;
    JPEG_Error_t jpeg_status;

    /* 主循环完成状态推进、SOI/EOI 解析和发送，回调中不做阻塞操作。 */
    JPEG_Capture_Process();
    capture_state = JPEG_Capture_GetState();

    if (capture_state == JPEG_CAPTURE_ERROR)
    {
        CameraApp_Diagnostic("DMA error flags", Camera_GetDmaErrorSnapshot());
        CameraApp_Diagnostic("DCMI error flags", Camera_GetDcmiErrorSnapshot());
        CameraApp_Diagnostic("DMA remaining words", Camera_GetDmaRemainingSnapshot());
        CameraApp_EnterIdleWithError("JPEG capture",
                                     (uint32_t)JPEG_Capture_GetError());
        return;
    }

    if ((capture_state == JPEG_CAPTURE_DONE) &&
        (jpeg_tx_active == 0U))
    {
        jpeg_tx_size = JPEG_Capture_GetSize();
        jpeg_tx_offset = 0U;
        if (jpeg_tx_size == 0U)
        {
            CameraApp_EnterIdleWithError("JPEG size", 0U);
            return;
        }

        jpeg_tx_active = 1U;
        jpeg_send_start_tick = HAL_GetTick();
        jpeg_capture_ms = jpeg_send_start_tick - jpeg_capture_start_tick;
#if (DEBUG_VERBOSE == 1)
        printf("[JPEG] Capture OK\r\n");
        printf("[JPEG] Size: %lu Bytes\r\n",
               (unsigned long)jpeg_tx_size);
#endif
    }

    if (jpeg_tx_active != 0U)
    {
        if (CameraApp_TransmitJpegChunk() == 0U)
        {
            return;
        }

#if (DEBUG_VERBOSE == 1)
        printf("[UART2] JPEG Send OK\r\n");
#endif
        if (camera_requested_mode == CAMERA_MODE_JPEG_VOFA)
        {
            jpeg_capture_start_tick = HAL_GetTick();
            jpeg_status = JPEG_Capture_Start();
            if (jpeg_status != JPEG_ERROR_NONE)
            {
                CameraApp_EnterIdleWithError("JPEG restart",
                                             (uint32_t)jpeg_status);
            }
        }
    }
}

static uint8_t CameraApp_TransmitJpegChunk(void)
{
    const uint8_t *jpeg_data = JPEG_Capture_GetData();
    Vofa_TxResult result;

    if (jpeg_tx_active == 0U)
    {
        return 1U;
    }

    result = Vofa_SendJpegChunk(jpeg_data, jpeg_tx_size, &jpeg_tx_offset,
                                CAMERA_JPEG_WIDTH, CAMERA_JPEG_HEIGHT);
    if (result == VOFA_TX_ERROR)
    {
        CameraApp_EnterIdleWithError("VOFA JPEG", 0U);
        return 0U;
    }
    if (result == VOFA_TX_MORE)
    {
        return 0U;
    }

    /* 每秒最多发送一次数值，图片字节全部发送后才输出。 */
    if ((status_sent == 0U) || ((HAL_GetTick() - last_status_tick) >= 1000U))
    {
        char status_frame[112];
        uint32_t send_ms = HAL_GetTick() - jpeg_send_start_tick;
        int status_length = snprintf(status_frame, sizeof(status_frame),
                                     "sram:%u,%lu,%lu,%lu,%ld,%ld\n",
                                     (unsigned int)SRAM_TestPassed(),
                                     (unsigned long)jpeg_tx_size,
                                     (unsigned long)jpeg_capture_ms,
                                     (unsigned long)send_ms,
                                     (long)Camera_GetFirstDmaProgressMs(),
                                     (long)Camera_GetDmaProgressToFrameMs());
        if ((status_length < 0) || ((size_t)status_length >= sizeof(status_frame)))
        {
            CameraApp_EnterIdleWithError("status format", 0U);
            return 0U;
        }
        if (Vofa_SendLine(status_frame, (uint16_t)status_length) == 0U)
        {
            CameraApp_EnterIdleWithError("VOFA status", 0U);
            return 0U;
        }
        last_status_tick = HAL_GetTick();
        status_sent = 1U;
    }
    jpeg_tx_active = 0U;
    jpeg_tx_offset = 0U;
    jpeg_tx_size = 0U;
    return 1U;
}

static void CameraApp_EnterIdleWithError(const char *operation,
                                         uint32_t error_code)
{
    printf("[CAM] Error: %s (0x%08lX)\r\n",
           operation,
           (unsigned long)error_code);
    CameraApp_StopCurrentMode();
    camera_requested_mode = CAMERA_MODE_IDLE;
    printf("[CAM] Mode: IDLE\r\n");
}

static void CameraApp_Diagnostic(const char *operation, uint32_t value)
{
    printf("diag %s 0x%08lX\n", operation, (unsigned long)value);
}
