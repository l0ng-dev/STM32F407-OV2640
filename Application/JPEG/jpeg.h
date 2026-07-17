#ifndef APPLICATION_JPEG_H
#define APPLICATION_JPEG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define JPEG_BUFFER_SIZE_BYTES  (64U * 1024U)

typedef enum
{
    JPEG_CAPTURE_IDLE = 0U,
    JPEG_CAPTURE_BUSY,
    JPEG_CAPTURE_DONE,
    JPEG_CAPTURE_ERROR
} JPEG_CaptureState_t;

typedef enum
{
    JPEG_ERROR_NONE = 0U,
    JPEG_ERROR_CAMERA_BASE = 0x0100U,
    JPEG_ERROR_SOI_NOT_FOUND = 0x0201U,
    JPEG_ERROR_EOI_NOT_FOUND = 0x0202U
} JPEG_Error_t;

void JPEG_Buffer_Init(void);
void JPEG_Buffer_Reset(void);
uint8_t *JPEG_Buffer_GetData(void);
uint32_t JPEG_Buffer_GetCapacity(void);
uint32_t JPEG_Buffer_GetSize(void);

void JPEG_Capture_Init(void);
JPEG_Error_t JPEG_Capture_Start(void);
void JPEG_Capture_Process(void);
JPEG_CaptureState_t JPEG_Capture_GetState(void);
JPEG_Error_t JPEG_Capture_GetError(void);
const uint8_t *JPEG_Capture_GetData(void);
uint32_t JPEG_Capture_GetSize(void);

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_JPEG_H */
