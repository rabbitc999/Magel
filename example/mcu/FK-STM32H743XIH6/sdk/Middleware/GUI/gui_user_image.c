#include "gui_user_image.h"

#include "bsp_system.h"
#include "gui.h"
#include "file.h"

/**
 * @brief  显示龙脉图像
 */
void GUI_ShowDragonImage(void)
{
    GUI_LCD_FillColor(360, 0, 440, 480, GUI_GRAY);

    // 共有11个点，首尾坐标相同，构成10条线段
    int points[][2] = {
        { 220, 90 },   // 顶点 0
        { 255, 191 },  // 顶点 1
        { 362, 193 },  // 顶点 2
        { 277, 258 },  // 顶点 3
        { 308, 361 },  // 顶点 4
        { 220, 300 },  // 顶点 5
        { 131, 361 },  // 顶点 6
        { 162, 258 },  // 顶点 7
        { 77, 193 },   // 顶点 8
        { 184, 191 },  // 顶点 9
        { 220, 90 }    // 顶点 10 (与顶点0相同，用于闭合)
    };

    // 循环绘制线段
    for (int i = 0; i < 10; i++)
    {
        LCD_DrawThickLine(360 + points[i][0], points[i][1], 360 + points[i + 1][0], points[i + 1][1], 5, GUI_YELLOW);
    }
}
