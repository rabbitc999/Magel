#ifndef __GUI_H
#define __GUI_H

#include <string.h>
#include <stdlib.h>

#include "gui_port.h"
#include "gui_menu.h"

#include "gui_coordinate_graph.h"
#include "gui_polar_graph.h"
#include "gui_font.h"

#include "gui_memory_painter.h"
#include "gui_painter_base.h"
#include "gui_painter_lcd.h"

#include "gui_user_menu.h"
#include "gui_user_main.h"
#include "gui_user_image.h"

#include "gui_font.h"

#include "file.h"

#ifdef __cplusplus
extern "C" {
#endif

float GUI_FloatRound(float input);

#ifdef __cplusplus
}
#endif

#endif
