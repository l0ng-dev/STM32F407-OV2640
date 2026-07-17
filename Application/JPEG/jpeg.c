#include "jpeg.h"

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
#define JPEG_DEBUG_DUMP_BYTES 32U

/* 静态缓存位于普通SRAM，由链接MAP确认地址；不放入0x10000000 CCM区域。 */
static uint32_t jpeg_buffer[JPEG_BUFFER_WORDS] __attribute__((aligned(4)));
static JPEG_CaptureState_t jpeg_capture_state = JPEG_CAPTURE_IDLE;
static JPEG_Error_t jpeg_capture_error = JPEG_ERROR_NONE;
static uint32_t jpeg_frame_offset;
static uint32_t jpeg_frame_size;

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

    printf("JPEG Marker Statistic:\r\n");
    printf("SOI: %lu\r\n", (unsigned long)soi_count);
    printf("EOI: %lu\r\n", (unsigned long)eoi_count);
    printf("SOS: %lu\r\n", (unsigned long)sos_count);
    printf("DQT: %lu\r\n", (unsigned long)dqt_count);
    printf("SOF: %lu\r\n", (unsigned long)sof_count);
    printf("DHT: %lu\r\n", (unsigned long)dht_count);
}

static void JPEG_PrintBufferHead(void)
{
    const uint8_t *data = (const uint8_t *)(const void *)jpeg_buffer;
    uint32_t index;

    printf("JPEG Buffer:\r\n");
    for (index = 0U; index < JPEG_DEBUG_DUMP_BYTES; ++index)
    {
        printf("%02X", (unsigned int)data[index]);
        printf("%s", (index + 1U < JPEG_DEBUG_DUMP_BYTES) ? " " : "\r\n");
    }
}

static JPEG_Error_t JPEG_ParseFrame(uint32_t captured_size)
{
    const uint8_t *data = (const uint8_t *)(const void *)jpeg_buffer;
    uint32_t index;
    uint32_t soi_offset = captured_size;

    if (captured_size > JPEG_BUFFER_SIZE_BYTES)
    {
        captured_size = JPEG_BUFFER_SIZE_BYTES;
    }

    JPEG_PrintMarkerStatistic(captured_size);

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
            JPEG_PrintBufferHead();
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
