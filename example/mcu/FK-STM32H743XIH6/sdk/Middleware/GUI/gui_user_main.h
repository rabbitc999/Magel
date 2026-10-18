#ifndef __GUI_USER_MAIN_H
#define __GUI_USER_MAIN_H

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "gui.h"
#include "algorithm.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct __GUI_User_T
{
    GUI_Device_T device;
    GUI_Menu_T *rootMenu;
    uint8_t rtcSetFlag;
    uint8_t showPlotIndex;
    uint8_t showPlotMotorPidIndex;
    uint8_t dragonClicked;
} g_GUI_User;

extern GUI_CoordinateGraph_T g_MotorMapperGraph;
extern GUI_CoordinateGraph_T g_MotorGraph;
extern GUI_CoordinateGraph_T g_ImuGraph;
extern GUI_CoordinateGraph_T g_DetailGraph;
extern GUI_CoordinateGraph_T g_CarGraph;
extern GUI_CoordinateGraph_T g_SlipGraph;

extern GUI_PolarGraph_T s_polarGraph;

#ifdef __cplusplus
}
#endif

#endif
