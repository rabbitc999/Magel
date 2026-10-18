#ifndef __GUI_CAPTURE_H
#define __GUI_CAPTURE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define GUI_CAPTURE_BUFFER    ((uint8_t *)0xC0300000)
#define GUI_CAPTURE_CAPACITY  (1024 * 1024)
#define GUI_CAPTURE_CHUNK_MAX 192
/* State: 0 idle, 1 pending, 2 ready, 3 unsupported display. */
extern volatile uint8_t g_GUICaptureState;
extern uint16_t g_GUICaptureWidth, g_GUICaptureHeight;
extern uint32_t g_GUICaptureBytes, g_GUICaptureId;
extern uint64_t g_GUICaptureTimestampMs;
void GUI_CaptureRequest(void);
void GUI_CaptureService(void);
#ifdef __cplusplus
}
#endif
#endif
