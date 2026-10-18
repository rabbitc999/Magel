#include "gui.h"
#include "car.h"

#include "gui_user_encoder.h"

void StartRemoteDebugOperator(GUI_BaseItem_T *sender);
void StopRemoteDebugOperator(GUI_BaseItem_T *sender);

/**
 * @brief 编码器读取函数(状态机实现)
 * @param sender 菜单项指针
 * @param object 保留参数
 */
uint16_t EncoderView_Function(GUI_FuncItem_T *sender, GUI_MenuParam_T *param)
{
    static int32_t Encoder[ENCODER_COUNT] = { 0 };
    GUI_Device_T *baseUI = param->baseUI;

    /* 首次调用时启动编码器读取 */
    if (param->InitFlag)
    {
        GUI_LCD_SetShowFont(GUI_FONT_TYPE_GB2312);
        StopRemoteDebugOperator((GUI_BaseItem_T *)sender);

        for (uint8_t i = CAR_INDEX_M1; i < CAR_MAX_INDEX; i++)
        {
            Encoder[i] = 0;
        }

        /* ===== 标题行 ===== */
        GUI_LCD_ShowString(COL(0), ROW(0) + 3, "编码器脉冲读取:", GUI_WHITE, GUI_BLACK);

        /* ===== 四个编码器标签 ===== */
        GUI_LCD_ShowString(COL(0), ROW(1) + 3, "编码器A:", GUI_WHITE, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(2) + 3, "编码器B:", GUI_WHITE, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(3) + 3, "编码器C:", GUI_WHITE, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(4) + 3, "编码器D:", GUI_WHITE, GUI_BLACK);

        /* ===== 底部操作提示 ===== */
        GUI_LCD_ShowString(COL(0), ROW(14) + 3, "确定: 编码器脉冲清零", GUI_WHITE, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(15) + 3, "模式: 退出预览", GUI_WHITE, GUI_BLACK);
        return 0;
    }

    /* 检查退出按键 */
    if (Key_IsAction(KEY_MODE))
    {
        StartRemoteDebugOperator((GUI_BaseItem_T *)sender);
        GUI_ExitPage(baseUI);
        return 0;
    }

    /* 检查进入按键 */
    if (Key_IsAction(KEY_ENTER))
    {
        for (uint8_t i = CAR_INDEX_M1; i < CAR_MAX_INDEX; i++)
        {
            Encoder[i] = 0;
        }
        return 0;
    }

    /* ===== 累加编码器脉冲 ===== */
    for (uint8_t i = CAR_INDEX_M1; i < CAR_MAX_INDEX; i++)
    {
        Encoder[i] += CarControl_EncoderGetCount(&g_Car.remote, i);
    }

    /* ===== 显示四个编码器数值（右对齐，第12列开始） ===== */
    GUI_LCD_ShowNumber(COL(12), ROW(1) + 3, Encoder[ENCODER_A], 5, GUI_YELLOW, GUI_BLACK, TRUE);
    GUI_LCD_ShowNumber(COL(12), ROW(2) + 3, Encoder[ENCODER_B], 5, GUI_YELLOW, GUI_BLACK, TRUE);
    GUI_LCD_ShowNumber(COL(12), ROW(3) + 3, Encoder[ENCODER_C], 5, GUI_YELLOW, GUI_BLACK, TRUE);
    GUI_LCD_ShowNumber(COL(12), ROW(4) + 3, Encoder[ENCODER_D], 5, GUI_YELLOW, GUI_BLACK, TRUE);

    return 0;
}