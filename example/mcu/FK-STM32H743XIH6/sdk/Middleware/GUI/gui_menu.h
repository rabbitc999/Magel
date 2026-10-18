#ifndef __GUI_MENU_H
#define __GUI_MENU_H

/* 头文件包含 */
#include "gui_port.h"

#include <stdint.h>

/* 菜单UI信息 */
#define GUI_MENU_DEPTH      4   /* 菜单调用深度 */
#define GUI_MAX_VALUE_COUNT 256 /* 最大变量个数 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __GUI_Menu_T GUI_Menu_T;
typedef struct __GUI_BaseItem_T GUI_BaseItem_T;
typedef struct __GUI_Device_T GUI_Device_T;
typedef struct __GUI_FuncItem_T GUI_FuncItem_T;

/* GUI菜单按键枚举 */
typedef enum __GUI_MenuKey_T
{
    GUI_MENU_KEY_NONE = 0x00,      /* 无按键按下 */
    GUI_MENU_KEY_ENTER = 0x20,     /* 确认/进入键，用于选择菜单项或确认修改 */
    GUI_MENU_KEY_EXIT = 0x40,      /* 退出/返回键，用于返回上级菜单或取消操作 */
    GUI_MENU_KEY_ITEM_UP = 0x60,   /* 菜单项向上键，用于选择上一个菜单项 */
    GUI_MENU_KEY_ITEM_DOWN = 0x80, /* 菜单项向下键，用于选择下一个菜单项 */
    GUI_MENU_KEY_VALUE_UP = 0xA0,  /* 数值增加键，用于增加当前选中项的数值 */
    GUI_MENU_KEY_VALUE_DOWN = 0xC0 /* 数值减少键，用于减少当前选中项的数值 */
} GUI_MenuKey_T;

/* GUI菜单单位枚举 */
typedef enum __GUI_MenuUnit_T
{
    GUI_MENU_UNITA = 0, /* 单位类型A，具体含义由应用定义 */
    GUI_MENU_UNITB = 1, /* 单位类型B，具体含义由应用定义 */
    GUI_MENU_UNITC = 2, /* 单位类型C，具体含义由应用定义 */
    GUI_MENU_UNITD = 3  /* 单位类型D，具体含义由应用定义 */
} GUI_MenuUnit_T;

/* GUI菜单事件枚举 */
typedef enum __GUI_MenuEvent_T
{
    GUI_MENU_EVENT_ENTER = 0, /* 进入事件：当用户进入某个菜单项时触发 */
    GUI_MENU_EVENT_EXIT = 1,  /* 退出事件：当用户离开某个菜单项时触发 */
    GUI_MENU_EVENT_VALUE = 2, /* 值改变事件：当菜单项的值发生变化时触发 */
    GUI_MENU_EVENT_ITEM = 3   /* 菜单项事件：当菜单项被选中时触发 */
} GUI_MenuEvent_T;

/* GUI菜单返回值枚举 */
typedef enum __GUI_MenuReturn_T
{
    GUI_MENU_EXIT_RET = 0xFFFF /* 菜单退出返回值，表示用户请求退出当前菜单 */
} GUI_MenuReturn_T;

/* 菜单绘制器配置结构体 */
typedef struct GUI_MenuParam_T
{
    GUI_Device_T *baseUI;
    GUI_Menu_T *baseMenu;
    GUI_MenuKey_T key;
    uint8_t pressCount;
    uint16_t userState;
    uint16_t InitFlag;
} GUI_MenuParam_T;

/**
 * @brief 菜单默认事件回调函数类型
 * @param sender 触发事件的菜单项指针
 */
typedef void (*GUI_MenuDefaultEventCallback_T)(GUI_BaseItem_T *sender);

/**
 * @brief 菜单绘制事件回调函数类型
 * @param sender 需要绘制的菜单项指针
 * @param line 绘制的行号（从0开始）
 */
typedef void (*GUI_MenuDrawEventCallback_T)(GUI_BaseItem_T *sender, uint16_t line);

/**
 * @brief 菜单数值变更回调函数类型
 * @param sender 数值变更的菜单项指针
 * @param change 变更的数值量（正负表示增减方向）
 */
typedef void (*GUI_MenuChangeEventCallback_T)(GUI_BaseItem_T *sender, int16_t change);

/**
 * @brief 菜单循环回调函数类型
 * @param sender 发送事件的菜单项指针（可为NULL）
 * @param key 按键输入值（使用GUI_MENU_KEY_*宏定义）
 * @param state 用户状态数据（可自定义用途）
 * @param InitFlag 是否为首次进入标志（TRUE表示首次进入）
 * @return uint16_t 更新后的用户状态数据
 *
 * @details 该回调函数用于实现复杂的菜单交互逻辑，支持状态保持和多次调用。
 *          返回GUI_MENU_EXIT_RET可立即退出菜单循环。
 */
typedef uint16_t (*GUI_MenuPageCallback_T)(GUI_FuncItem_T *sender, GUI_MenuParam_T *param);

/**
 * @brief 菜单单次事件回调函数类型
 * @param sender 触发事件的菜单项指针
 */
typedef void (*GUI_MenuOnceCallback_T)(GUI_BaseItem_T *sender);

/* 菜单绘制器配置结构体 */
typedef struct GUI_MenuPainter_T
{
    /* 几何尺寸配置 */
    int32_t lineInterval; /* 行间距（像素） */
    int32_t charWidth;    /* ASCII字符宽度（像素） */
    int32_t charHeight;   /* ASCII字符高度（像素） */
    int32_t Width;        /* 菜单显示区域宽度（像素） */
    int32_t Height;       /* 菜单显示区域高度（像素） */
    int32_t StartX;       /* 菜单起始X坐标（屏幕相对位置） */
    int32_t StartY;       /* 菜单起始Y坐标（屏幕相对位置） */

    /* 颜色配置系统 */
    uint16_t textItemFontColor;           /* 文本菜单项字体颜色 */
    uint16_t textItemBlackColor;          /* 文本菜单项背景颜色 */
    uint16_t valueItemEditFontColor;      /* 数值项编辑字体颜色 */
    uint16_t valueItemEditBlackColor;     /* 数值项编辑背景颜色 */
    uint16_t valueItemReadOnlyFontColor;  /* 数值项只读模式字体颜色 */
    uint16_t valueItemReadOnlyBlackColor; /* 数值项只读模式背景颜色 */
    uint16_t headerFontColor;             /* 标题栏字体颜色 */
    uint16_t headerBlackColor;            /* 标题栏背景颜色 */
    uint16_t selectFontColor;             /* 选中项字体颜色 */
    uint16_t selectBlackColor;            /* 选中项背景颜色 */
    uint16_t notSelectFontColor;          /* 未选中项字体颜色 */
    uint16_t notSelectBlackColor;         /* 未选中项背景颜色 */

    /* 单位调节系统 */
    float unitValue[4];        /* 单位调节数值数组 */
    const char *unitString[4]; /* 单位显示字符串数组 */

    /* 模式标识字符串 */
    const char *editModeString; /* 编辑模式标识字符串 */
    const char *itemModeString; /* 项目模式标识字符串 */

    /* 菜单项类型标识字符串 */
    const char *textItemString;           /* 文本项标识字符串 */
    const char *jumpItemString;           /* 跳转项标识字符串 */
    const char *funcItemString;           /* 功能项标识字符串 */
    const char *pageItemString;           /* 页面项标识字符串 */
    const char *checkItemSelectString;    /* 多选项选中标识 */
    const char *checkItemNotSelectString; /* 多选项未选中标识 */

    /* 数值显示配置 */
    uint16_t floatValueShowLength; /* 浮点数显示长度（字符数） */
    uint16_t intValueShowLength;   /* 整数显示长度（字符数） */

    /* 菜单移动进度条配置 */
    uint16_t scrollBarWidth;           /* 滚动条宽度 */
    uint16_t scrollBarBackgroundColor; /* 滚动条背景颜色 */
    uint16_t scrollBarForegroundColor; /* 滚动条前景颜色 */
    uint8_t showScrollBar;             /* 是否显示滚动条 */
} GUI_MenuPainter_T;

/* 菜单结构体 */
typedef struct __GUI_Menu_T
{
    const char *title;    /* 菜单标题字符串 */
    GUI_Device_T *baseUI; /* 所属的菜单设备指针 */

    /* 链表管理 */
    GUI_BaseItem_T *first;  /* 链表首项指针 */
    GUI_BaseItem_T *last;   /* 链表末项指针 */
    GUI_BaseItem_T *start;  /* 当前显示起始项 */
    GUI_BaseItem_T *select; /* 当前选中项 */

    /* 状态标志 */
    uint8_t firstUpdate; /* 首次更新标志 */
} GUI_Menu_T;

/* 页面结构体 */
typedef struct __GUI_Page_T
{
    GUI_MenuPageCallback_T page; /* 循环执行回调函数 */
    uint16_t userState;          /* 用户状态数据 */
    uint8_t InitFlag;            /* 初始化标志位 */
} GUI_Page_T;

/* 菜单设备结构体 */
typedef struct __GUI_Device_T
{
    /* 函数指针 */
    GUI_MenuPageCallback_T mainPage; /* 主界面函数 */

    /* 栈管理 */
    GUI_Menu_T *menuStack[GUI_MENU_DEPTH]; /* 菜单栈数组 */

    /* 页面管理（页面跳转模型） */
    GUI_Page_T currentPage; /* 当前页面 */
    GUI_Page_T returnPage;  /* 返回页面（保存上一页面状态） */

    /* 显示配置 */
    GUI_MenuPainter_T menuPainter; /* 菜单绘制器配置 */

    /* 运行状态 */
    uint8_t updateAllFlag;   /* 全局更新标志 */
    uint8_t valueFlag;       /* 数值修改模式标志 */
    uint8_t pageRunFlag;     /* 页面运行标志 */
    uint8_t pageChangeFlag;  /* 页面修改标志 */
    uint8_t pageLastRunFlag; /* 上次页面运行标志 */
    uint8_t changeUnit;      /* 当前调节单位 */
    uint16_t menuStackTop;   /* 菜单栈顶位置 */
} GUI_Device_T;

/* 函数定义 */
GUI_Menu_T *GUI_GetCurrentMenu(GUI_Device_T *baseUI);
GUI_BaseItem_T *GUI_GetSelectItem(GUI_Device_T *baseUI);

/* 页面管理 */
void GUI_EnterMenu(GUI_Device_T *baseUI, GUI_Menu_T *rootMenu);
uint8_t GUI_ExitMenu(GUI_Device_T *baseUI);
uint8_t GUI_ExitPage(GUI_Device_T *baseUI);
uint8_t GUI_EnterPage(GUI_Device_T *baseUI, GUI_MenuPageCallback_T runPage);
void GUI_ChangeRunPage(GUI_Device_T *baseUI, GUI_MenuPageCallback_T runPage);

/* 基本API操作 */
void GUI_MenuLoop(GUI_Device_T *baseUI);
void GUI_MenuEnterEvent(GUI_Device_T *baseUI);
void GUI_MenuExitEvent(GUI_Device_T *baseUI);
void GUI_MenuValueChangeEvent(GUI_Device_T *baseUI, int32_t deltaValue);
void GUI_MenuItemChangeEvent(GUI_Device_T *baseUI, int32_t deltaValue);
void GUI_MenuDeviceInit(GUI_Device_T *baseUI, GUI_MenuPageCallback_T mainPage);
void GUI_DeleteItem(GUI_Device_T *baseUI, GUI_BaseItem_T *pItem);
void GUI_DeleteMenu(GUI_Device_T *baseUI, GUI_Menu_T *pMenu);
void GUI_MenuSetScrollBarVisible(GUI_Device_T *baseUI, uint8_t show);
void GUI_SetRootMenu(GUI_Device_T *baseUI, GUI_Menu_T *rootMenu);
uint8_t GUI_HasRootMenu(GUI_Device_T *baseUI);

GUI_Menu_T *GUI_CreateMenu(GUI_Device_T *baseUI, const char *title);

#ifdef __cplusplus
}
#endif

/* 菜单项目操作函数 */
#include "gui_menu_item.h"

#endif
