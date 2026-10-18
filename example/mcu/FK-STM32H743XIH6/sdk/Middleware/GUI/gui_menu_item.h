#ifndef __GUI_MENU_ITEM_H
#define __GUI_MENU_ITEM_H

/* 头文件包含 */
#include "gui_port.h"
#include "gui_menu.h"

#ifdef __cplusplus
extern "C" {
#endif

/* GUI菜单项类型枚举 */
typedef enum __GUI_MenuItemType_T
{
    GUI_MENU_ITEM_TYPE_TEXT = 0,  /* 文本类型：显示静态文本，不可交互 */
    GUI_MENU_ITEM_TYPE_JUMP = 1,  /* 跳转类型：点击后跳转到子菜单 */
    GUI_MENU_ITEM_TYPE_FUNC = 2,  /* 函数类型：点击后执行指定函数 */
    GUI_MENU_ITEM_TYPE_VALUE = 3, /* 数值类型：可编辑的数值项，支持增减操作 */
    GUI_MENU_ITEM_TYPE_RADIO = 4, /* 单选类型：从多个选项中选择一个 */
    GUI_MENU_ITEM_TYPE_CHECK = 5, /* 复选类型：勾选/取消勾选操作 */
    GUI_MENU_ITEM_TYPE_MODE = 6   /* 模式类型：可以通过单击切换不同模式 */
} GUI_MenuItemType_T;

/* GUI菜单值更新类型枚举 */
typedef enum __GUI_MenuValueUpdateType_T
{
    GUI_MENU_VALUE_UPDATE_CHANGE = 0,    /* 正常更新模式：数值可以自由修改 */
    GUI_MENU_VALUE_UPDATE_SET_VALUE = 1, /* 数值更新模式：数值只能设置为指定值或恢复默认值 */
    GUI_MENU_VALUE_UPDATE_READONLY = 2   /* 只读模式：数值只能查看，不能修改 */
} GUI_MenuValueUpdateType_T;

/* GUI菜单值类型枚举 */
typedef enum __GUI_MenuValueType_T
{
    GUI_MENU_VALUE_TYPE_FLOAT = 0,  /* 单精度浮点数类型 */
    GUI_MENU_VALUE_TYPE_DOUBLE = 1, /* 双精度浮点数类型 */
    GUI_MENU_VALUE_TYPE_INT32 = 2,  /* 32位有符号整数类型 */
    GUI_MENU_VALUE_TYPE_UINT32 = 3, /* 32位无符号整数类型 */
    GUI_MENU_VALUE_TYPE_INT16 = 4,  /* 16位有符号整数类型 */
    GUI_MENU_VALUE_TYPE_UINT16 = 5, /* 16位无符号整数类型 */
    GUI_MENU_VALUE_TYPE_INT8 = 6,   /* 8位有符号整数类型 */
    GUI_MENU_VALUE_TYPE_UINT8 = 7   /* 8位无符号整数类型 */
} GUI_MenuValueType_T;

/* GUI菜单函数类型枚举 */
typedef enum __GUI_MenuFunctionType_T
{
    GUI_MENU_FUNCTION_PAGE = 0, /* 持续执行模式：函数在菜单显示期间持续执行 */
    GUI_MENU_FUNCTION_ONCE = 1  /* 单次执行模式：函数只在点击时执行一次 */
} GUI_MenuFunctionType_T;

/* 菜单项基类结构体 */
typedef struct __GUI_BaseItem_T
{
    /* 基本属性 */
    const char *title;    /* 菜单项标题字符串 */
    GUI_Menu_T *baseMenu; /* 所属的父菜单指针 */

    /* 事件回调函数 */
    GUI_MenuChangeEventCallback_T change;       /* 数值修改回调函数 */
    GUI_MenuDrawEventCallback_T drawDetail;     /* 详情绘制回调函数 */
    GUI_MenuDefaultEventCallback_T enter;       /* 进入事件回调函数 */
    GUI_MenuDefaultEventCallback_T exit;        /* 退出事件回调函数 */
    GUI_MenuDefaultEventCallback_T customEnter; /* 自定义进入回调函数 */
    GUI_MenuDefaultEventCallback_T customExit;  /* 自定义退出回调函数 */

    /* 链表结构 */
    GUI_BaseItem_T *prev; /* 前一个菜单项指针 */
    GUI_BaseItem_T *next; /* 后一个菜单项指针 */

    /* 标志位 */
    GUI_MenuItemType_T itemType; /* 菜单项类型（GUI_MENU_ITEM_TYPE_*） */
    uint8_t selectable;          /* 是否可被选中 */
} GUI_BaseItem_T;

/* 文本菜单项结构体 */
typedef struct __GUI_TextItem_T
{
    GUI_BaseItem_T parent; /* 基类成员 */
} GUI_TextItem_T;

/* 跳转菜单项结构体 */
typedef struct __GUI_JumpItem_T
{
    GUI_BaseItem_T parent; /* 基类成员 */
    GUI_Menu_T *jump;      /* 跳转目标菜单指针 */
} GUI_JumpItem_T;

/* 功能菜单项结构体 */
typedef struct __GUI_FuncItem_T
{
    GUI_BaseItem_T parent; /* 基类成员 */
    union
    {
        GUI_MenuPageCallback_T page; /* 循环执行回调函数 */
        GUI_MenuOnceCallback_T once; /* 单次执行回调函数 */
    } callback;
    GUI_MenuFunctionType_T funcType; /* 函数类型（GUI_MENU_FUNCTION_*） */
} GUI_FuncItem_T;

/* 页面菜单项结构体 */
typedef GUI_FuncItem_T GUI_PageItem_T;

/* 模式菜单项的模式映射元素 */
typedef struct __GUI_ModeItemElement_T
{
    const char *Name; /* 模式名称数组 */
    uint32_t value;   /* 模式数值 */
} GUI_ModeItemElement_T;

/* 模式菜单项结构体 */
typedef struct __GUI_ModeItem_T
{
    GUI_BaseItem_T parent;                 /* 基类成员 */
    const GUI_ModeItemElement_T *elements; /* 模式元素数组 */
    uint8_t modeIndex;                     /* 模式索引 */
    uint8_t modeCount;                     /* 模式总数 */
    uint32_t *valuePtr;                    /* 当前数值指针 */
    uint8_t readOnly;                      /* 是否只读 */
    uint8_t isShortValue;                  /* 是否为短数值类型 */
    GUI_MenuValueType_T valueType;         /* 数值类型（GUI_MENU_VALUE_TYPE_*） */
    GUI_MenuDefaultEventCallback_T custom; /* 自定义操作函数 */
} GUI_ModeItem_T;

/* 存储类型联合体 */
typedef union __GUI_StoreValue_T
{
    float floatValue;
    double doubleValue;
    int8_t int8Value;
    uint8_t uint8Value;
    int16_t int16Value;
    uint16_t uint16Value;
    int32_t int32Value;
    uint32_t uint32Value;
} GUI_StoreValue_T;

/* 多选菜单项结构体 */
typedef struct __GUI_CheckItem_T
{
    GUI_BaseItem_T parent; /* 基类成员 */

    /* 数值变量指针 */
    uint8_t *pValue;

    /* 存储配置 */
    uint8_t equivalent; /* 等效位 */

    /* 自定义显示字符串 */
    const char *selectShowString;   /* 选择时显示的字符串 */
    const char *unSelectShowString; /* 未选择时显示的字符串 */
} GUI_CheckItem_T;

/* 数值菜单项结构体 */
typedef struct __GUI_ValueItem_T
{
    GUI_BaseItem_T parent; /* 基类成员 */

    /* 数值变量指针（联合体实现多类型支持） */
    GUI_StoreValue_T *pValue;

    /* 数值范围限制 */
    GUI_StoreValue_T maxValue;
    GUI_StoreValue_T minValue;

    /* 配置标志 */
    uint8_t disableShowNumberFlag;        /* 是否不显示数字 */
    GUI_MenuValueType_T valueType;        /* 数值类型（GUI_MENU_VALUE_TYPE_*） */
    GUI_MenuValueUpdateType_T updateType; /* 更新类型（GUI_MENU_VALUE_UPDATE_*） */

    /* 其他配置 */
    GUI_StoreValue_T setDefaultValue; /* 默认设置值 */
} GUI_ValueItem_T;

GUI_BaseItem_T *GUI_CreateBaseItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, char *title, uint32_t size);

GUI_TextItem_T *GUI_CreateTextItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *titleStr);
GUI_FuncItem_T *GUI_CreateFuncItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, GUI_MenuOnceCallback_T pFunc);
GUI_PageItem_T *GUI_CreatePageItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, GUI_MenuPageCallback_T pFunc);

GUI_JumpItem_T *GUI_CreateJumpItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, GUI_Menu_T *pJumpMenu);
GUI_JumpItem_T *GUI_CreateJumpItemF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, GUI_Menu_T *pJumpMenu, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit);

GUI_ValueItem_T *GUI_CreateFloatItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, float *pValue, float Min, float Max);
GUI_ValueItem_T *GUI_CreateFloatItemF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, float *pValue, float Min, float Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit);
GUI_ValueItem_T *GUI_CreateFloatItemR(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, float *pValue);
GUI_ValueItem_T *GUI_CreateFloatItemZ(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, float *pValue, float setDefaultValue);
GUI_ValueItem_T *GUI_CreateFloatItemZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, float *pValue, float setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc);

GUI_ValueItem_T *GUI_CreateDoubleItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, double *pValue, double Min, double Max);
GUI_ValueItem_T *GUI_CreateDoubleItemF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, double *pValue, double Min, double Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit);
GUI_ValueItem_T *GUI_CreateDoubleItemR(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, double *pValue);
GUI_ValueItem_T *GUI_CreateDoubleItemZ(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, double *pValue, double setDefaultValue);
GUI_ValueItem_T *GUI_CreateDoubleItemZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, double *pValue, double setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc);

GUI_ValueItem_T *GUI_CreateUintItem32(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint32_t *pValue, uint32_t Min, uint32_t Max);
GUI_ValueItem_T *GUI_CreateUintItem32F(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint32_t *pValue, uint32_t Min, uint32_t Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit);
GUI_ValueItem_T *GUI_CreateUintItem32R(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint32_t *pValue);
GUI_ValueItem_T *GUI_CreateUintItem32Z(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint32_t *pValue, uint32_t setDefaultValue);
GUI_ValueItem_T *GUI_CreateUintItem32ZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint32_t *pValue, uint32_t setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc);

GUI_ValueItem_T *GUI_CreateUintItem16(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint16_t *pValue, uint16_t Min, uint16_t Max);
GUI_ValueItem_T *GUI_CreateUintItem16F(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint16_t *pValue, uint16_t Min, uint16_t Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit);
GUI_ValueItem_T *GUI_CreateUintItem16R(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint16_t *pValue);
GUI_ValueItem_T *GUI_CreateUintItem16Z(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint16_t *pValue, uint16_t setDefaultValue);
GUI_ValueItem_T *GUI_CreateUintItem16ZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint16_t *pValue, uint16_t setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc);

GUI_ValueItem_T *GUI_CreateUintItem8(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint8_t *pValue, uint8_t Min, uint8_t Max);
GUI_ValueItem_T *GUI_CreateUintItem8F(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint8_t *pValue, uint8_t Min, uint8_t Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit);
GUI_ValueItem_T *GUI_CreateUintItem8R(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint8_t *pValue);
GUI_ValueItem_T *GUI_CreateUintItem8Z(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint8_t *pValue, uint8_t setDefaultValue);
GUI_ValueItem_T *GUI_CreateUintItem8ZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint8_t *pValue, uint8_t setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc);

GUI_ValueItem_T *GUI_CreateIntItem32(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int32_t *pValue, int32_t Min, int32_t Max);
GUI_ValueItem_T *GUI_CreateIntItem32F(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int32_t *pValue, int32_t Min, int32_t Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit);
GUI_ValueItem_T *GUI_CreateIntItem32R(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int32_t *pValue);
GUI_ValueItem_T *GUI_CreateIntItem32Z(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int32_t *pValue, int32_t setDefaultValue);
GUI_ValueItem_T *GUI_CreateIntItem32ZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int32_t *pValue, int32_t setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc);

GUI_ValueItem_T *GUI_CreateIntItem16(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int16_t *pValue, int16_t Min, int16_t Max);
GUI_ValueItem_T *GUI_CreateIntItem16F(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int16_t *pValue, int16_t Min, int16_t Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit);
GUI_ValueItem_T *GUI_CreateIntItem16R(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int16_t *pValue);
GUI_ValueItem_T *GUI_CreateIntItem16Z(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int16_t *pValue, int16_t setDefaultValue);
GUI_ValueItem_T *GUI_CreateIntItem16ZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int16_t *pValue, int16_t setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc);

GUI_ValueItem_T *GUI_CreateIntItem8(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int8_t *pValue, int8_t Min, int8_t Max);
GUI_ValueItem_T *GUI_CreateIntItem8F(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int8_t *pValue, int8_t Min, int8_t Max, GUI_MenuDefaultEventCallback_T enter, GUI_MenuDefaultEventCallback_T exit);
GUI_ValueItem_T *GUI_CreateIntItem8R(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int8_t *pValue);
GUI_ValueItem_T *GUI_CreateIntItem8Z(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int8_t *pValue, int8_t setDefaultValue);
GUI_ValueItem_T *GUI_CreateIntItem8ZF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, int8_t *pValue, int8_t setDefaultValue, GUI_MenuDefaultEventCallback_T pFunc);

GUI_CheckItem_T *GUI_CreateCheckItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint8_t *pVal, uint8_t eq);
GUI_CheckItem_T *GUI_CreateCheckItemF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, uint8_t *pVal, uint8_t eq, GUI_MenuDefaultEventCallback_T pFunc);

GUI_ModeItem_T *GUI_CreateModeItem8(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, const GUI_ModeItemElement_T *elements, uint8_t *currentValue);
GUI_ModeItem_T *GUI_CreateModeItem8F(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, const GUI_ModeItemElement_T *elements, uint8_t *currentValue, GUI_MenuDefaultEventCallback_T pFunc);
GUI_ModeItem_T *GUI_CreateModeItem8R(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, const GUI_ModeItemElement_T *elements, uint8_t *currentValue);

GUI_ModeItem_T *GUI_CreateModeItem(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, const GUI_ModeItemElement_T *elements, uint32_t *currentValue);
GUI_ModeItem_T *GUI_CreateModeItemF(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, const GUI_ModeItemElement_T *elements, uint32_t *currentValue, GUI_MenuDefaultEventCallback_T pFunc);
GUI_ModeItem_T *GUI_CreateModeItemR(GUI_Device_T *baseUI, GUI_Menu_T *pMenu, const char *title, const GUI_ModeItemElement_T *elements, uint32_t *currentValue);

#ifdef __cplusplus
}
#endif

#endif
