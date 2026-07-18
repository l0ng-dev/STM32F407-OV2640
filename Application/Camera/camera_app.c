#include "camera_app.h"

#include "camera.h"
#include "jpeg.h"
#include "lcd.h"
#include "main.h"
#include "ov2640.h"
#include "sccb.h"
#include "usart.h"

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
#define CAMERA_USART2_CHUNK_SIZE      1024U
#define CAMERA_USART2_TIMEOUT_MS      100U

typedef struct
{
    uint8_t stable_pressed;
    uint8_t candidate_pressed;
    uint32_t candidate_tick;
} CameraApp_KeyState_t;

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

void CameraApp_Init(void)
{
    uint32_t now = HAL_GetTick();

    printf("========================\r\n");
    printf(" Camera System Start\r\n");
    printf(" MCU: STM32F407ZG\r\n");
    printf(" Sensor: OV2640\r\n");
    printf("========================\r\n");

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
    camera_sensor_ready = CameraApp_InitSensor();
    CameraApp_InitLcd();

    printf("[LCD] Init OK\r\n");
    printf("[UART1] Debug Ready\r\n");
    printf("[UART2] JPEG TX Ready\r\n");
    printf("[CAM] Mode: IDLE\r\n");
}

void CameraApp_Process(void)
{
    CameraApp_ScanKeys();

    if (camera_requested_mode != camera_mode)
    {
        if ((camera_mode == CAMERA_MODE_JPEG_XCAM) &&
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
        else if (camera_requested_mode == CAMERA_MODE_JPEG_XCAM)
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
    else if (camera_mode == CAMERA_MODE_JPEG_XCAM)
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

    if (camera_mode == CAMERA_MODE_RGB565_LCD)
    {
        rgb_frame_pending = 1U;
    }
    else if (camera_mode == CAMERA_MODE_JPEG_XCAM)
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

    if (camera_mode == CAMERA_MODE_JPEG_XCAM)
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

    if (CameraApp_UpdateKey(&wkup_key, wkup_pressed, now) != 0U)
    {
        printf("[KEY] WKUP Press\r\n");
        camera_requested_mode = CAMERA_MODE_RGB565_LCD;
    }

    if (CameraApp_UpdateKey(&key0_key, key0_pressed, now) != 0U)
    {
        printf("[KEY] KEY0 Press\r\n");
        camera_requested_mode = CAMERA_MODE_JPEG_XCAM;
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

    CameraApp_StopCurrentMode();
    printf("[CAM] Switch JPEG XCAM\r\n");

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
    camera_mode = CAMERA_MODE_JPEG_XCAM;
    jpeg_status = JPEG_Capture_Start();
    if (jpeg_status != JPEG_ERROR_NONE)
    {
        CameraApp_EnterIdleWithError("JPEG start", (uint32_t)jpeg_status);
        return 0U;
    }

    printf("[JPEG] XCAM Start\r\n");
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

    JPEG_Capture_Process();
    capture_state = JPEG_Capture_GetState();

    if (capture_state == JPEG_CAPTURE_ERROR)
    {
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
        if (camera_requested_mode == CAMERA_MODE_JPEG_XCAM)
        {
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
    uint32_t remaining;
    uint16_t chunk_size;

    if (jpeg_tx_active == 0U)
    {
        return 1U;
    }

    remaining = jpeg_tx_size - jpeg_tx_offset;
    chunk_size = (remaining > CAMERA_USART2_CHUNK_SIZE) ?
                 (uint16_t)CAMERA_USART2_CHUNK_SIZE :
                 (uint16_t)remaining;

    if (HAL_UART_Transmit(&huart2,
                          &jpeg_data[jpeg_tx_offset],
                          chunk_size,
                          CAMERA_USART2_TIMEOUT_MS) != HAL_OK)
    {
        CameraApp_EnterIdleWithError("USART2 JPEG", HAL_UART_GetError(&huart2));
        return 0U;
    }

    jpeg_tx_offset += chunk_size;
    if (jpeg_tx_offset < jpeg_tx_size)
    {
        return 0U;
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
