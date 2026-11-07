/**
 ******************************************************************************
 * @file    camera_app.c
 * @brief   摄像头模式、按键与采集调度
 *
 * @details
 * 主要功能：
 * 1. 管理空闲、LCD 预览识别和 JPEG 状态。
 * 2. 处理 WKUP/KEY0 按键事件及模式安全切换。
 * 3. 协调 OV2640、DCMI/DMA、LCD、AI 推理、JPEG 解析和 USART2 发送。
 *
 * 模块关系：
 * main.c 调用本模块；本模块调用 Camera BSP、OV2640、JPEG 和 LCD 模块。
 * USART2 使用 VOFA+ FireWater 格式发送图像；USART1 输出日志。
 *
 * 主要接口：
 * CameraApp_Init()、CameraApp_Process()、CameraApp_NotifyFrameEvent()。
 *
 * @note
 * 模式切换前先停止 DCMI/DMA，再切换传感器格式并重配 DMA；
 * 故障退出后仅在 DMA 已停止时绘制提示；USART2 流故障保持锁定。
 * 不修改 CubeMX 底层外设和已验证的 LCD/FSMC 配置。
 ******************************************************************************
 */

#include "camera_app.h"

#include "camera_dcmi.h"
#include "dma.h"
#include "gesture_glyphs.h"
#include "gesture_inference.h"
#include "jpeg_capture.h"
#include "lcd.h"
#include "main.h"
#include "ov2640.h"
#include "sccb.h"
#include "sram_test.h"
#include "sram_layout.h"
#include "rgb565_preprocess.h"
#include "network.h"
#include "vofa.h"

#include <math.h>
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
/* LCD + AI 并发时发生 DCMI overrun，先降低一级传感器时钟作对照。 */
#define CAMERA_RGB_SENSOR_CLKRC       0x02U
#define CAMERA_RGB_SRAM_TIMEOUT_MS     3000U
#define CAMERA_LCD_DMA_TIMEOUT_MS      3000U
/* 切入 RGB 模式后给传感器曝光稳定时间，效果以实测为准。 */
#define CAMERA_RGB_SRAM_SETTLE_MS      1000U

typedef struct
{
    uint8_t stable_pressed;
    uint8_t candidate_pressed;
    uint32_t candidate_tick;
} CameraApp_KeyState_t;

typedef enum
{
    CAMERA_MODE_IDLE = 0U,
    CAMERA_MODE_RGB565_LCD,
    CAMERA_MODE_JPEG_VOFA
} Camera_Mode_t;

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
static uint8_t lcd_ready;
static CameraApp_KeyState_t wkup_key;
static CameraApp_KeyState_t key0_key;
static uint8_t jpeg_tx_active;
static const uint8_t *jpeg_tx_data;
static uint32_t jpeg_tx_offset;
static uint32_t jpeg_tx_size;
static uint8_t key0_armed;
#define PREVIEW_NONE                 (-1)
#define PREVIEW_DMA_HALF_PIXELS      38400U
#if (SRAM_LAYOUT_RGB_SIZE != 153600UL) || (SRAM_LAYOUT_RGB2_SIZE != SRAM_LAYOUT_RGB_SIZE) || \
    (PREVIEW_DMA_HALF_PIXELS * 2UL * 2UL != SRAM_LAYOUT_RGB_SIZE)
#error "Preview frame buffers and DMA segments do not match"
#endif
static const uint32_t preview_buffers[2] = { SRAM_LAYOUT_RGB_BASE, SRAM_LAYOUT_RGB2_BASE };
/* 主线程管理采集/就绪缓冲；显示 DMA 中断只读取 display 索引并更新阶段标志。 */
static int8_t preview_capture_index;
static int8_t preview_ready_index;
static volatile int8_t preview_display_index;
static volatile uint8_t preview_dma_phase;
static volatile uint8_t preview_dma_done;
static volatile uint8_t preview_dma_error;
static uint8_t preview_need_sample;
static uint8_t preview_failed;
static uint32_t preview_capture_tick;
static uint32_t preview_dma_tick;
static uint32_t preview_error_code;
static uint32_t preview_capture_started;
static uint32_t preview_capture_completed;
static volatile uint8_t preview_inference_active;
static volatile uint8_t preview_error_display_phase;
static volatile uint8_t preview_error_inference_active;
static volatile uint32_t preview_error_capture_ndtr;
static volatile uint32_t preview_error_display_ndtr;
static uint8_t preview_result_valid;
static uint8_t preview_result_class;
static uint32_t preview_result_confidence;
static uint32_t preview_result_elapsed;

static uint8_t CameraApp_InitSensor(void);
static SCCB_Status_t CameraApp_SetSensorClock(uint8_t clock_divider);
static uint8_t CameraApp_InitLcd(void);
static void CameraApp_ScanKeys(void);
static uint8_t CameraApp_UpdateKey(CameraApp_KeyState_t *key,
                                   uint8_t pressed,
                                   uint32_t now);
static uint8_t CameraApp_StopCurrentMode(void);
static uint8_t CameraApp_PrepareCapture(void);
static uint8_t CameraApp_StartRgb565Mode(void);
static uint8_t CameraApp_StartJpegMode(void);
static void CameraApp_PreviewService(void);
static void CameraApp_Overlay(uint16_t *frame);
static void CameraApp_DisplayDone(DMA_HandleTypeDef *dma);
static void CameraApp_DisplayError(DMA_HandleTypeDef *dma);
static void CameraApp_PreviewDiagnostic(void);
static void CameraApp_ProcessRgb565(void);
static void CameraApp_ProcessJpeg(void);
static uint8_t CameraApp_TransmitJpegChunk(void);
static void CameraApp_EnterIdleWithError(const char *operation,
                                         uint32_t error_code);
static void CameraApp_ShowError(uint8_t vofa_fault);
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
    jpeg_tx_data = NULL;
    jpeg_tx_offset = 0U;
    jpeg_tx_size = 0U;

    wkup_key.stable_pressed =
        (HAL_GPIO_ReadPin(WKUP_GPIO_Port, WKUP_Pin) == GPIO_PIN_SET) ? 1U : 0U;
    wkup_key.candidate_pressed = wkup_key.stable_pressed;
    wkup_key.candidate_tick = now;

    key0_key.stable_pressed =
        (HAL_GPIO_ReadPin(KEY0_GPIO_Port, KEY0_Pin) == GPIO_PIN_SET) ? 1U : 0U;
    key0_key.candidate_pressed = key0_key.stable_pressed;
    key0_key.candidate_tick = now;
    key0_armed = (key0_key.stable_pressed == 0U) ? 1U : 0U;

    Camera_Init();
    /* 先完成硬件复位，再释放 SCCB 总线并写入 RGB565 基础配置。 */
    camera_sensor_ready = CameraApp_InitSensor();
    lcd_ready = CameraApp_InitLcd();

    printf("[CAM] Mode: IDLE\r\n");
}

void CameraApp_Process(void)
{
    if ((Vofa_IsFaulted() != 0U) && (camera_mode == CAMERA_MODE_JPEG_VOFA))
    {
        CameraApp_EnterIdleWithError("VOFA stream", 0U);
        return;
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
            (void)CameraApp_StopCurrentMode();
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
        preview_error_display_phase = preview_dma_phase;
        preview_error_inference_active = preview_inference_active;
        preview_error_capture_ndtr = __HAL_DMA_GET_COUNTER(hdcmi.DMA_Handle);
        preview_error_display_ndtr = __HAL_DMA_GET_COUNTER(&hdma_memtomem_dma2_stream0);
        dcmi_error_pending = 1U;
    }
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

static SCCB_Status_t CameraApp_SetSensorClock(uint8_t clock_divider)
{
    SCCB_Status_t status = OV2640_WriteReg(0xFFU, 0x01U);
    if (status == SCCB_STATUS_OK)
        status = OV2640_WriteReg(0x11U, clock_divider);
    if (status == SCCB_STATUS_OK)
        status = OV2640_WriteReg(0xFFU, 0x00U);
    return status;
}

static uint8_t CameraApp_InitLcd(void)
{
    if (lcd_init() == 0U)
    {
        printf("[CAM] Error: unrecognized LCD ID 0x%04X\r\n", (unsigned int)lcddev.id);
        return 0U;
    }
    lcd_display_dir(1U);
    lcd_clear(WHITE);
    lcd_show_string(16U, 18U, 288U, 32U, 32U, "F4Camera", DARKBLUE);
    lcd_show_string(16U, 58U, 288U, 16U, 16U, "STM32F407 / OV2640", GRAYBLUE);
    lcd_fill(16U, 82U, 303U, 83U, DARKBLUE);

    if (SRAM_TestPassed() == 0U)
    {
        lcd_show_utf8_16(16U, 102U, "存储自检失败", RED);
        lcd_show_utf8_16(16U, 136U, "查看串口日志", BLACK);
    }
    else
    {
        if (camera_sensor_ready != 0U)
            lcd_show_utf8_16(16U, 102U, "就绪", DARKBLUE);
        else
            lcd_show_utf8_16(16U, 102U, "摄像头未就绪", RED);

        lcd_show_string(16U, 136U, 48U, 24U, 24U, "WKUP", DARKBLUE);
        lcd_show_utf8_16(88U, 140U, "预览与识别", BLACK);
        lcd_show_string(16U, 174U, 48U, 24U, 24U, "KEY0", DARKBLUE);
        lcd_show_utf8_16(88U, 174U, "JPEG 图像传输 / VOFA+", BLACK);
        lcd_show_utf8_16(88U, 194U, "按下后松开", GRAYBLUE);
    }

    LCD_BL(1U);
    printf("[CAM] LCD ready, ID=0x%04X\r\n", (unsigned int)lcddev.id);
    return 1U;
}

static void CameraApp_ScanKeys(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t wkup_pressed;
    uint8_t key0_pressed;
    uint8_t key0_was_pressed;

    wkup_pressed =
        (HAL_GPIO_ReadPin(WKUP_GPIO_Port, WKUP_Pin) == GPIO_PIN_SET) ? 1U : 0U;
    key0_pressed =
        (HAL_GPIO_ReadPin(KEY0_GPIO_Port, KEY0_Pin) == GPIO_PIN_SET) ? 1U : 0U;

    /* 消抖完成后的按下沿只产生模式请求，不在按键处理处直接操作 DCMI。 */
    if (CameraApp_UpdateKey(&wkup_key, wkup_pressed, now) != 0U)
    {
        camera_requested_mode = CAMERA_MODE_RGB565_LCD;
    }

    key0_was_pressed = key0_key.stable_pressed;
    (void)CameraApp_UpdateKey(&key0_key, key0_pressed, now);
    if ((key0_was_pressed != 0U) && (key0_key.stable_pressed == 0U))
    {
        if (key0_armed != 0U)
        {
            camera_requested_mode = CAMERA_MODE_JPEG_VOFA;
        }
        key0_armed = 1U;
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

static uint8_t CameraApp_StopCurrentMode(void)
{
    HAL_StatusTypeDef lcd_status = HAL_OK;
    HAL_StatusTypeDef camera_status;

    /*
     * 切换前清除上一个模式的帧标志和发送进度，再停止 DCMI/DMA。
     * 这样新模式不会误用旧模式遗留的帧完成事件或 JPEG 长度。
     */
    camera_mode = CAMERA_MODE_IDLE;
    GestureInference_SetService(NULL);
    if (HAL_DMA_GetState(&hdma_memtomem_dma2_stream0) == HAL_DMA_STATE_BUSY)
    {
        lcd_status = HAL_DMA_Abort(&hdma_memtomem_dma2_stream0);
        /* DMA 可能在查询 BUSY 与执行 Abort 之间正常结束。 */
        if ((lcd_status == HAL_ERROR) &&
            (HAL_DMA_GetError(&hdma_memtomem_dma2_stream0) == HAL_DMA_ERROR_NO_XFER))
        {
            lcd_status = HAL_OK;
        }
    }
    if ((hdma_memtomem_dma2_stream0.Instance->CR & DMA_SxCR_EN) != 0U)
    {
        lcd_status = HAL_TIMEOUT;
    }
    rgb_frame_pending = 0U;
    dcmi_error_pending = 0U;
    jpeg_tx_active = 0U;
    jpeg_tx_data = NULL;
    jpeg_tx_offset = 0U;
    jpeg_tx_size = 0U;
    camera_status = Camera_Stop();
    if ((lcd_status != HAL_OK) || (camera_status != HAL_OK))
    {
        printf("[CAM] Stop failed: LCD DMA=%u DCMI/DMA=%u\r\n",
               (unsigned int)lcd_status, (unsigned int)camera_status);
        camera_requested_mode = CAMERA_MODE_IDLE;
        return 0U;
    }
    return 1U;
}

static uint8_t CameraApp_PrepareCapture(void)
{
    if (CameraApp_StopCurrentMode() == 0U) return 0U;
    if (SRAM_TestPassed() == 0U)
    {
        SRAM_ReportResult();
        printf("[CAM] Error: external SRAM self-test failed\r\n");
        camera_requested_mode = CAMERA_MODE_IDLE;
        return 0U;
    }
    return 1U;
}

static uint8_t CameraApp_StartRgb565Mode(void)
{
    SCCB_Status_t sccb_status;
    OV2640_Status_t init_status;

    if (lcd_ready == 0U)
    {
        printf("[CAM] Error: LCD unavailable\r\n");
        camera_requested_mode = camera_mode;
        return 0U;
    }
    if (CameraApp_PrepareCapture() == 0U) return 0U;
    printf("[CAM] Switch RGB565 LCD + AI\r\n");

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

    sccb_status = CameraApp_SetSensorClock(CAMERA_RGB_SENSOR_CLKRC);
    if (sccb_status != SCCB_STATUS_OK)
    {
        CameraApp_EnterIdleWithError("RGB clock", (uint32_t)sccb_status);
        return 0U;
    }
    HAL_Delay(CAMERA_RGB_SRAM_SETTLE_MS);

    if (Camera_SetDCMIJPEGMode(DCMI_JPEG_DISABLE) != HAL_OK)
    {
        CameraApp_EnterIdleWithError("RGB DCMI", HAL_DCMI_GetError(&hdcmi));
        return 0U;
    }

    if (Camera_ConfigureSramDma() != HAL_OK)
    {
        CameraApp_EnterIdleWithError("RGB DMA", HAL_DMA_GetError(hdcmi.DMA_Handle));
        return 0U;
    }

    preview_capture_index = PREVIEW_NONE;
    preview_ready_index = PREVIEW_NONE;
    preview_display_index = PREVIEW_NONE;
    preview_dma_phase = 0U;
    preview_dma_done = 0U;
    preview_dma_error = 0U;
    preview_need_sample = 1U;
    preview_failed = 0U;
    preview_error_code = 0U;
    preview_capture_started = 0U;
    preview_capture_completed = 0U;
    preview_inference_active = 0U;
    preview_error_display_phase = 0U;
    preview_error_inference_active = 0U;
    preview_error_capture_ndtr = 0U;
    preview_error_display_ndtr = 0U;
    preview_result_valid = 0U;
    hdma_memtomem_dma2_stream0.XferCpltCallback = CameraApp_DisplayDone;
    hdma_memtomem_dma2_stream0.XferHalfCpltCallback = NULL;
    hdma_memtomem_dma2_stream0.XferErrorCallback = CameraApp_DisplayError;
    camera_mode = CAMERA_MODE_RGB565_LCD;
    GestureInference_SetService(CameraApp_PreviewService);
    CameraApp_PreviewService();
    if (preview_failed != 0U)
    {
        CameraApp_EnterIdleWithError("RGB start", HAL_DCMI_GetError(&hdcmi));
        return 0U;
    }

    printf("[LCD] Preview Start\r\n");
    return 1U;
}

static void CameraApp_DisplayDone(DMA_HandleTypeDef *dma)
{
    uint32_t source;

    if ((camera_mode != CAMERA_MODE_RGB565_LCD) ||
        (HAL_DMA_GetError(dma) != HAL_DMA_ERROR_NONE)) return;

    if ((preview_dma_phase == 1U) && (preview_display_index != PREVIEW_NONE))
    {
        /* 单次 DMA 计数不足整帧，在完成中断中无缝启动后半帧。 */
        source = preview_buffers[(uint8_t)preview_display_index] +
                 PREVIEW_DMA_HALF_PIXELS * sizeof(uint16_t);
        preview_dma_phase = 2U;
        if (HAL_DMA_Start_IT(dma, source, (uint32_t)&LCD->LCD_RAM,
                             PREVIEW_DMA_HALF_PIXELS) != HAL_OK)
            preview_dma_error = 1U;
    }
    else if (preview_dma_phase == 2U)
        preview_dma_done = 1U;
    else
        preview_dma_error = 1U;
}

static void CameraApp_DisplayError(DMA_HandleTypeDef *dma)
{
    (void)dma;
    preview_dma_error = 1U;
}

static void CameraApp_PreviewDiagnostic(void)
{
    printf("[diag] capture start=%lu done=%lu cap=%d ready=%d display=%d phase=%u\r\n",
           (unsigned long)preview_capture_started, (unsigned long)preview_capture_completed,
           (int)preview_capture_index, (int)preview_ready_index,
           (int)preview_display_index, (unsigned int)preview_dma_phase);
    printf("[diag] dcmi ndtr=%lu risr=%08lX ier=%08lX sr=%08lX cr=%08lX\r\n",
           (unsigned long)__HAL_DMA_GET_COUNTER(hdcmi.DMA_Handle),
           (unsigned long)hdcmi.Instance->RISR, (unsigned long)hdcmi.Instance->IER,
           (unsigned long)hdcmi.Instance->SR, (unsigned long)hdcmi.Instance->CR);
    printf("[diag] display_ndtr=%lu dcmi_error=%08lX capture_dma_error=%08lX display_dma_error=%08lX dma_lisr=%08lX\r\n",
           (unsigned long)__HAL_DMA_GET_COUNTER(&hdma_memtomem_dma2_stream0),
           (unsigned long)HAL_DCMI_GetError(&hdcmi),
           (unsigned long)HAL_DMA_GetError(hdcmi.DMA_Handle),
           (unsigned long)HAL_DMA_GetError(&hdma_memtomem_dma2_stream0),
           (unsigned long)DMA2->LISR);
    if (dcmi_error_pending != 0U)
    {
        printf("[diag] error_irq capture_ndtr=%lu display_phase=%u display_ndtr=%lu inference=%u\r\n",
               (unsigned long)preview_error_capture_ndtr,
               (unsigned int)preview_error_display_phase,
               (unsigned long)preview_error_display_ndtr,
               (unsigned int)preview_error_inference_active);
    }
}

static void CameraApp_OverlayGlyph(volatile uint16_t *frame, uint32_t x, uint8_t glyph)
{
    uint32_t row, col;
    for (row = 0U; row < 16U; row++)
    {
        for (col = 0U; col < 16U; col++)
        {
            if ((gesture_glyphs[glyph][row] & (0x8000U >> col)) != 0U)
                frame[(row + 1U) * CAMERA_RGB_WIDTH + x + col] = WHITE;
        }
    }
}

static uint32_t CameraApp_OverlayText(volatile uint16_t *frame, uint32_t x, const char *text)
{
    uint32_t row, col;
    while ((*text != '\0') && (x + 8U <= CAMERA_RGB_WIDTH))
    {
        uint8_t glyph;
        if ((*text >= '0') && (*text <= '9')) glyph = (uint8_t)(12U + (*text - '0'));
        else if (*text == '%') glyph = 22U;
        else if (*text == ':') glyph = 23U;
        else if (*text == 'm') glyph = 24U;
        else if (*text == 's') glyph = 25U;
        else { x += 8U; text++; continue; }
        for (row = 0U; row < 16U; row++)
        {
            for (col = 0U; col < 8U; col++)
            {
                if ((gesture_glyphs[glyph][row] & (0x0800U >> col)) != 0U)
                    frame[(row + 1U) * CAMERA_RGB_WIDTH + x + col] = WHITE;
            }
        }
        x += 8U;
        text++;
    }
    return x;
}

static void CameraApp_Overlay(uint16_t *pixels)
{
    volatile uint16_t *frame = pixels;
    uint32_t x, y;
    char text[20];
    if (preview_result_valid == 0U) return;

    for (y = 0U; y < 18U; y++)
        for (x = 0U; x < 232U; x++) frame[y * CAMERA_RGB_WIDTH + x] = BLACK;

    x = 4U;
    CameraApp_OverlayGlyph(frame, x, (preview_result_class == 0U) ? 0U :
                                    (preview_result_class == 1U) ? 2U : 3U);
    x += 16U;
    CameraApp_OverlayGlyph(frame, x, (preview_result_class == 2U) ? 4U : 1U);
    x += 24U;
    CameraApp_OverlayGlyph(frame, x, 5U); x += 16U;
    CameraApp_OverlayGlyph(frame, x, 6U); x += 16U;
    CameraApp_OverlayGlyph(frame, x, 7U); x += 16U;
    (void)snprintf(text, sizeof(text), ":%lu%%", (unsigned long)preview_result_confidence);
    x = CameraApp_OverlayText(frame, x, text) + 8U;
    CameraApp_OverlayGlyph(frame, x, 8U); x += 16U;
    CameraApp_OverlayGlyph(frame, x, 9U); x += 16U;
    (void)snprintf(text, sizeof(text), ":%lums",
                   (unsigned long)((preview_result_elapsed > 9999U) ? 9999U : preview_result_elapsed));
    (void)CameraApp_OverlayText(frame, x, text);
}

/* 推理节点回调与主循环推进预览；LCD DMA 中断衔接第二段并发布结果。 */
static void CameraApp_PreviewService(void)
{
    uint32_t source;
    if ((camera_mode != CAMERA_MODE_RGB565_LCD) || (preview_failed != 0U)) return;
    CameraApp_ScanKeys();
    if (camera_requested_mode != CAMERA_MODE_RGB565_LCD) return;

    if ((dcmi_error_pending != 0U) || (preview_dma_error != 0U))
    {
        preview_error_code = (dcmi_error_pending != 0U) ? dcmi_error_code :
                             HAL_DMA_GetError(&hdma_memtomem_dma2_stream0);
        CameraApp_PreviewDiagnostic();
        preview_failed = 1U;
        return;
    }
    if (rgb_frame_pending != 0U)
    {
        rgb_frame_pending = 0U;
        if ((preview_capture_index == PREVIEW_NONE) ||
            (__HAL_DMA_GET_COUNTER(hdcmi.DMA_Handle) != 0U) ||
            (HAL_DMA_GetError(hdcmi.DMA_Handle) != HAL_DMA_ERROR_NONE) ||
            (HAL_DCMI_GetError(&hdcmi) != HAL_DCMI_ERROR_NONE))
        {
            preview_error_code = HAL_DCMI_GetError(&hdcmi);
            CameraApp_PreviewDiagnostic();
            preview_failed = 1U;
            return;
        }
        /* HAL 不返回内部 DMA Abort 的结果；确认停稳后才交出帧缓冲。 */
        if (Camera_Stop() != HAL_OK)
        {
            preview_error_code = 0x10000003UL;
            CameraApp_PreviewDiagnostic();
            preview_failed = 1U;
            return;
        }
        preview_ready_index = preview_capture_index;
        preview_capture_index = PREVIEW_NONE;
        preview_capture_completed++;
    }
    else if ((preview_capture_index != PREVIEW_NONE) &&
             ((HAL_GetTick() - preview_capture_tick) >= CAMERA_RGB_SRAM_TIMEOUT_MS))
    {
        CameraApp_PreviewDiagnostic();
        preview_error_code = 0x10000001UL;
        preview_failed = 1U;
        return;
    }

    if ((preview_dma_phase != 0U) && (preview_dma_done == 0U) &&
        ((HAL_GetTick() - preview_dma_tick) >= CAMERA_LCD_DMA_TIMEOUT_MS))
    {
        CameraApp_PreviewDiagnostic();
        preview_error_code = 0x10000002UL;
        preview_failed = 1U;
        return;
    }

    if (preview_dma_done != 0U)
    {
        preview_dma_done = 0U;
        if (preview_dma_phase == 2U)
        {
            preview_dma_phase = 0U;
            preview_display_index = PREVIEW_NONE;
        }
        else preview_failed = 1U;
    }
    if (preview_failed != 0U) return;

    /* 采样帧先生成模型输入，再允许覆盖叠字并交给 LCD DMA。 */
    if ((preview_ready_index != PREVIEW_NONE) && (preview_display_index == PREVIEW_NONE) &&
        (preview_need_sample == 0U))
    {
        preview_display_index = preview_ready_index;
        preview_ready_index = PREVIEW_NONE;
        source = preview_buffers[(uint8_t)preview_display_index];
        CameraApp_Overlay((uint16_t *)source);
        lcd_set_window(0U, 0U, CAMERA_RGB_WIDTH, CAMERA_RGB_HEIGHT);
        lcd_write_ram_prepare();
        preview_dma_phase = 1U;
        preview_dma_tick = HAL_GetTick();
        if (HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0, source,
                             (uint32_t)&LCD->LCD_RAM, PREVIEW_DMA_HALF_PIXELS) != HAL_OK)
        {
            preview_error_code = HAL_DMA_GetError(&hdma_memtomem_dma2_stream0);
            preview_failed = 1U;
            return;
        }
    }
    if ((preview_capture_index == PREVIEW_NONE) && (preview_ready_index == PREVIEW_NONE))
    {
        /* 新采集必须避开 LCD DMA 正在读取的另一块帧缓冲。 */
        preview_capture_index = (preview_display_index == 0) ? 1 : 0;
        rgb_frame_pending = 0U;
        preview_capture_tick = HAL_GetTick();
        preview_capture_started++;
        if (Camera_StartRGB565SramSnapshot(preview_buffers[(uint8_t)preview_capture_index],
                                           CAMERA_RGB_WORD_COUNT) != HAL_OK)
        {
            preview_error_code = HAL_DCMI_GetError(&hdcmi);
            preview_failed = 1U;
        }
    }
}

static uint8_t CameraApp_StartJpegMode(void)
{
    SCCB_Status_t sccb_status;
    JPEG_Error_t jpeg_status;

    if (Vofa_IsFaulted() != 0U)
    {
        CameraApp_EnterIdleWithError("VOFA stream", 0U);
        return 0U;
    }

    /*
     * JPEG 模式需要可变长字节流连续写入 SRAM，因此开启 DCMI JPEG 位，
     * 采集 DMA 与 RGB565 共用 Word 对齐、目标地址递增的 SRAM 配置。
     */
    if (CameraApp_PrepareCapture() == 0U) return 0U;
    printf("[CAM] Switch JPEG VOFA+\r\n");

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

    sccb_status = CameraApp_SetSensorClock(CAMERA_JPEG_SENSOR_CLKRC);
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

    if (Camera_ConfigureSramDma() != HAL_OK)
    {
        CameraApp_EnterIdleWithError("JPEG DMA", HAL_DMA_GetError(hdcmi.DMA_Handle));
        return 0U;
    }

    JPEG_Capture_Init();
    camera_mode = CAMERA_MODE_JPEG_VOFA;
    jpeg_status = JPEG_Capture_Start();
    if (jpeg_status != JPEG_ERROR_NONE)
    {
        CameraApp_EnterIdleWithError("JPEG start", (uint32_t)jpeg_status);
        return 0U;
    }

    printf("[JPEG] VOFA+ Start\r\n");
    return 1U;
}

static void CameraApp_ProcessRgb565(void)
{
    int8_t output[3];
    int8_t *input = (int8_t *)SRAM_LAYOUT_INPUT_BASE;
    uint32_t elapsed, error;
    uint8_t best = 0U, i;
    float sum = 0.0f;

    CameraApp_PreviewService();
    if (preview_failed != 0U)
    {
        CameraApp_EnterIdleWithError("preview DMA/DCMI", preview_error_code);
        return;
    }
    if ((camera_requested_mode != CAMERA_MODE_RGB565_LCD) ||
        (preview_need_sample == 0U) || (preview_ready_index == PREVIEW_NONE)) return;

    if (RGB565_Preprocess((const volatile uint16_t *)preview_buffers[(uint8_t)preview_ready_index], input) != 0)
    {
        CameraApp_EnterIdleWithError("AI preprocess", 0U);
        return;
    }
    preview_need_sample = 0U;
    CameraApp_PreviewService();
    if ((preview_failed != 0U) || (camera_requested_mode != CAMERA_MODE_RGB565_LCD)) return;
    preview_inference_active = 1U;
    i = (GestureInference_Run(input, output, &elapsed, &error) == 0) ? 0U : 1U;
    preview_inference_active = 0U;
    if (i != 0U)
    {
        CameraApp_EnterIdleWithError("AI inference", error);
        return;
    }
    if (preview_failed != 0U)
    {
        CameraApp_EnterIdleWithError("preview DMA/DCMI", preview_error_code);
        return;
    }
    if (camera_requested_mode != CAMERA_MODE_RGB565_LCD) return;
    /* 三类共用量化 scale；减去最大值后计算 softmax，避免指数溢出。 */
    for (i = 1U; i < 3U; i++) if (output[i] > output[best]) best = i;
    for (i = 0U; i < 3U; i++)
        sum += expf(((float)output[i] - (float)output[best]) * STAI_NETWORK_OUT_1_SCALE);
    preview_result_class = best;
    preview_result_confidence = (uint32_t)(100.0f / sum + 0.5f);
    preview_result_elapsed = elapsed;
    preview_result_valid = 1U;
    preview_need_sample = 1U;
}

static void CameraApp_ProcessJpeg(void)
{
    JPEG_CaptureState_t capture_state;

    /* 主循环完成状态推进、SOI/EOI 解析和发送，回调中不做阻塞操作。 */
    JPEG_Capture_Process();
    capture_state = JPEG_Capture_GetState();

    /* 已开始的 FireWater 帧必须先发送完，再报告并停止下一帧采集错误。 */
    if ((capture_state == JPEG_CAPTURE_ERROR) && (jpeg_tx_active == 0U))
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
        jpeg_tx_data = JPEG_Capture_GetData();
        jpeg_tx_offset = 0U;
        if ((jpeg_tx_size == 0U) || (jpeg_tx_data == NULL))
        {
            CameraApp_EnterIdleWithError("JPEG size", 0U);
            return;
        }

        jpeg_tx_active = 1U;
        /* 发送当前缓存时，DCMI 在另一块缓存中采集下一帧。 */
        (void)JPEG_Capture_Start();
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
    }
}

static uint8_t CameraApp_TransmitJpegChunk(void)
{
    Vofa_TxResult result;

    if (jpeg_tx_active == 0U)
    {
        return 1U;
    }

    result = Vofa_SendJpegChunk(jpeg_tx_data, jpeg_tx_size, &jpeg_tx_offset,
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

    jpeg_tx_active = 0U;
    jpeg_tx_data = NULL;
    jpeg_tx_offset = 0U;
    jpeg_tx_size = 0U;
    return 1U;
}

static void CameraApp_EnterIdleWithError(const char *operation,
                                         uint32_t error_code)
{
    uint8_t stopped;

    printf("[CAM] Error: %s (0x%08lX)\r\n",
           operation,
           (unsigned long)error_code);
    stopped = CameraApp_StopCurrentMode();
    camera_requested_mode = CAMERA_MODE_IDLE;
    printf("[CAM] Mode: IDLE\r\n");
    /* 只有确认 DMA 已停稳才可由 CPU 接管 LCD 总线。 */
    if (stopped != 0U) CameraApp_ShowError(Vofa_IsFaulted());
}

/* 故障页仅在主线程绘制；图片串口锁定后仍可通过 USART1 输出诊断。 */
static void CameraApp_ShowError(uint8_t vofa_fault)
{
    if (lcd_ready == 0U) return;

    lcd_clear(WHITE);
    if (vofa_fault != 0U)
    {
        lcd_show_utf8_16(16U, 80U, "VOFA+ 传输失败", RED);
        lcd_show_string(16U, 116U, 288U, 16U, 16U, "REOPEN VOFA+ THEN RESET MCU", BLACK);
    }
    else
    {
        lcd_show_utf8_16(16U, 80U, "预览 / 传输失败", RED);
        lcd_show_utf8_16(16U, 116U, "查看串口日志", BLACK);
    }
}

static void CameraApp_Diagnostic(const char *operation, uint32_t value)
{
    printf("diag %s 0x%08lX\n", operation, (unsigned long)value);
}
