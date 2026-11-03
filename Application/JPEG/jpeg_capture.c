/**
 ******************************************************************************
 * @file    jpeg_capture.c
 * @brief   JPEG 帧缓存与有效数据解析模块
 *
 * @details
 * 主要功能：
 * 1. 提供 DMA 可访问的 64 KB JPEG 缓存。
 * 2. 协调 Camera BSP 完成单帧采集并解析 SOI/EOI 边界。
 * 3. 在 DEBUG_VERBOSE 模式下统计 JPEG Marker 和输出缓存头。
 *
 * 模块关系：
 * Camera Application 调用本模块，本模块调用 Camera BSP 完成 DCMI/DMA 采集。
 *
 * 主要接口：
 * JPEG_Capture_Init()、JPEG_Capture_Start()、JPEG_Capture_Process()。
 *
 * @note
 * 本模块只处理缓存和 JPEG 边界，不修改 DCMI/DMA 外设配置。
 ******************************************************************************
 */

#include "jpeg_capture.h"

#include "camera.h"
#include <stdio.h>

#define JPEG_SOI_FIRST_BYTE   0xFFU
#define JPEG_SOI_SECOND_BYTE  0xD8U
#define JPEG_EOI_FIRST_BYTE   0xFFU
#define JPEG_EOI_SECOND_BYTE  0xD9U
#define JPEG_SOS_SECOND_BYTE  0xDAU
#define JPEG_DQT_SECOND_BYTE  0xDBU
#define JPEG_SOF_SECOND_BYTE  0xC0U
#define JPEG_DHT_SECOND_BYTE  0xC4U
#define JPEG_BUFFER_WORDS     (JPEG_BUFFER_SIZE_BYTES / sizeof(uint32_t))
/* 当前阶段验证 DCMI DMA 写外部 SRAM；设为 0 可恢复片内缓存。 */
#define JPEG_BUFFER_EXTERNAL_SRAM 1
#define JPEG_EXTERNAL_SRAM_BASE  0x68000000UL
#if (DEBUG_VERBOSE == 1)
#define JPEG_DEBUG_DUMP_BYTES 32U
#endif

/*
 * 外部 SRAM 在 MX_FSMC_Init() 后可用；采集、解析和发送依次独占缓存。
 * 起始地址按 4 字节对齐，容量 64 KiB，不使用 CCM。
 */
#if JPEG_BUFFER_EXTERNAL_SRAM
#define jpeg_buffer ((uint32_t *)JPEG_EXTERNAL_SRAM_BASE)
#else
static uint32_t jpeg_buffer[JPEG_BUFFER_WORDS] __attribute__((aligned(4)));
#endif
static JPEG_CaptureState_t jpeg_capture_state = JPEG_CAPTURE_IDLE;
static JPEG_Error_t jpeg_capture_error = JPEG_ERROR_NONE;
static uint32_t jpeg_frame_offset;
static uint32_t jpeg_frame_size;

#if (DEBUG_VERBOSE == 1)
/* Marker 统计只用于判断 JPEG 结构是否完整，默认关闭以避免串口刷屏。 */
static void JPEG_PrintMarkerStatistic(uint32_t captured_size)
{
    const uint8_t *data = (const uint8_t *)(const void *)jpeg_buffer;
    uint32_t soi_count = 0U;
    uint32_t eoi_count = 0U;
    uint32_t sos_count = 0U;
    uint32_t dqt_count = 0U;
    uint32_t sof_count = 0U;
    uint32_t dht_count = 0U;
    uint32_t index;

    for (index = 0U; (index + 1U) < captured_size; ++index)
    {
        if (data[index] != JPEG_SOI_FIRST_BYTE)
        {
            continue;
        }

        switch (data[index + 1U])
        {
            case JPEG_SOI_SECOND_BYTE:
                ++soi_count;
                break;

            case JPEG_EOI_SECOND_BYTE:
                ++eoi_count;
                break;

            case JPEG_SOS_SECOND_BYTE:
                ++sos_count;
                break;

            case JPEG_DQT_SECOND_BYTE:
                ++dqt_count;
                break;

            case JPEG_SOF_SECOND_BYTE:
                ++sof_count;
                break;

            case JPEG_DHT_SECOND_BYTE:
                ++dht_count;
                break;

            default:
                break;
        }
    }

    printf("[JPEG] Marker:\r\n");
    printf("SOI=%lu EOI=%lu SOS=%lu DQT=%lu SOF=%lu DHT=%lu\r\n",
           (unsigned long)soi_count,
           (unsigned long)eoi_count,
           (unsigned long)sos_count,
           (unsigned long)dqt_count,
           (unsigned long)sof_count,
           (unsigned long)dht_count);
}

static void JPEG_PrintBufferHead(void)
{
    const uint8_t *data = (const uint8_t *)(const void *)jpeg_buffer;
    uint32_t index;

    printf("[JPEG] Buffer Head: ");
    for (index = 0U; index < JPEG_DEBUG_DUMP_BYTES; ++index)
    {
        printf("%02X", (unsigned int)data[index]);
        printf("%s", (index + 1U < JPEG_DEBUG_DUMP_BYTES) ? " " : "\r\n");
    }
}
#endif

static JPEG_Error_t JPEG_ParseFrame(uint32_t captured_size)
{
    const uint8_t *data = (const uint8_t *)(const void *)jpeg_buffer;
    uint32_t index;
    uint32_t soi_offset = captured_size;

    if (captured_size > JPEG_BUFFER_SIZE_BYTES)
    {
        captured_size = JPEG_BUFFER_SIZE_BYTES;
    }

#if (DEBUG_VERBOSE == 1)
    JPEG_PrintMarkerStatistic(captured_size);
#endif

    /* 先定位首个 SOI，跳过 DCMI/DMA 起始处可能存在的无效填充字节。 */
    for (index = 0U; (index + 1U) < captured_size; ++index)
    {
        if ((data[index] == JPEG_SOI_FIRST_BYTE) &&
            (data[index + 1U] == JPEG_SOI_SECOND_BYTE))
        {
            soi_offset = index;
            break;
        }
    }

    if (soi_offset == captured_size)
    {
        return JPEG_ERROR_SOI_NOT_FOUND;
    }

    /* 从 SOI 之后查找首个 EOI，仅向上层暴露完整 JPEG 的有效范围。 */
    for (index = soi_offset + 2U; (index + 1U) < captured_size; ++index)
    {
        if ((data[index] == JPEG_EOI_FIRST_BYTE) &&
            (data[index + 1U] == JPEG_EOI_SECOND_BYTE))
        {
            jpeg_frame_offset = soi_offset;
            jpeg_frame_size = index - soi_offset + 2U;
            return JPEG_ERROR_NONE;
        }
    }

    return JPEG_ERROR_EOI_NOT_FOUND;
}

void JPEG_Buffer_Init(void)
{
    JPEG_Buffer_Reset();
}

void JPEG_Buffer_Reset(void)
{
    jpeg_frame_offset = 0U;
    jpeg_frame_size = 0U;
}

uint8_t *JPEG_Buffer_GetData(void)
{
    return (uint8_t *)(void *)jpeg_buffer;
}

uint32_t JPEG_Buffer_GetCapacity(void)
{
    return JPEG_BUFFER_SIZE_BYTES;
}

uint32_t JPEG_Buffer_GetSize(void)
{
    return jpeg_frame_size;
}

void JPEG_Capture_Init(void)
{
    Camera_Init();
    JPEG_Buffer_Init();
    jpeg_capture_error = JPEG_ERROR_NONE;
    jpeg_capture_state = JPEG_CAPTURE_IDLE;
}

JPEG_Error_t JPEG_Capture_Start(void)
{
    Camera_Error_t camera_status;

    /* 每帧复用同一缓存；发送完成后才由 CameraApp 请求下一帧。 */
    JPEG_Buffer_Reset();
    jpeg_capture_error = JPEG_ERROR_NONE;
    jpeg_capture_state = JPEG_CAPTURE_BUSY;

    camera_status = Camera_StartJPEGCapture(JPEG_Buffer_GetData(),
                                            JPEG_Buffer_GetCapacity());
    if (camera_status != CAMERA_ERROR_NONE)
    {
        jpeg_capture_error =
            (JPEG_Error_t)(JPEG_ERROR_CAMERA_BASE | (uint32_t)camera_status);
        jpeg_capture_state = JPEG_CAPTURE_ERROR;
    }

    return jpeg_capture_error;
}

void JPEG_Capture_Process(void)
{
    Camera_CaptureState_t camera_state;

    if (jpeg_capture_state != JPEG_CAPTURE_BUSY)
    {
        return;
    }

    Camera_Process();
    camera_state = Camera_GetState();

    if (camera_state == CAMERA_CAPTURE_ERROR)
    {
        jpeg_capture_error =
            (JPEG_Error_t)(JPEG_ERROR_CAMERA_BASE |
                           (uint32_t)Camera_GetError());
        jpeg_capture_state = JPEG_CAPTURE_ERROR;
    }
    else if (camera_state == CAMERA_CAPTURE_DONE)
    {
        jpeg_capture_error = JPEG_ParseFrame(Camera_GetCapturedSize());
        if (jpeg_capture_error == JPEG_ERROR_NONE)
        {
            jpeg_capture_state = JPEG_CAPTURE_DONE;
        }
        else
        {
#if (DEBUG_VERBOSE == 1)
            JPEG_PrintBufferHead();
#endif
            jpeg_capture_state = JPEG_CAPTURE_ERROR;
        }
    }
}

JPEG_CaptureState_t JPEG_Capture_GetState(void)
{
    return jpeg_capture_state;
}

JPEG_Error_t JPEG_Capture_GetError(void)
{
    return jpeg_capture_error;
}

const uint8_t *JPEG_Capture_GetData(void)
{
    return ((const uint8_t *)(const void *)jpeg_buffer) + jpeg_frame_offset;
}

uint32_t JPEG_Capture_GetSize(void)
{
    return jpeg_frame_size;
}
