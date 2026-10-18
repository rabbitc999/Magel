#include "gui_capture.h"
#include "gui_runtime.h"
#include "bsp_lcd.h"
#include "bsp_timer.h"

volatile uint8_t g_GUICaptureState = 0;
uint16_t g_GUICaptureWidth = 0, g_GUICaptureHeight = 0;
uint32_t g_GUICaptureBytes = 0, g_GUICaptureId = 0;
uint64_t g_GUICaptureTimestampMs = 0;

void GUI_CaptureRequest(void)
{
    if (g_GUICaptureState == 1)
    {
        return;
    }
    g_GUICaptureState = g_LCD.TftFlag ? 1 : 3;
}

void GUI_CaptureService(void)
{
    if (g_GUICaptureState != 1)
    {
        return;
    }
    /* Copy a completed display frame; never mix bands from different frames. */
    if (!GUI_RuntimeCopyFrame(GUI_CAPTURE_BUFFER, GUI_CAPTURE_CAPACITY, &g_GUICaptureWidth, &g_GUICaptureHeight))
    {
        return;
    }
    g_GUICaptureBytes = (uint32_t)g_GUICaptureWidth * g_GUICaptureHeight * 2;
    g_GUICaptureTimestampMs = Timer_GetTimestampMs();
    g_GUICaptureId++;
    __DSB();
    g_GUICaptureState = 2;
}
