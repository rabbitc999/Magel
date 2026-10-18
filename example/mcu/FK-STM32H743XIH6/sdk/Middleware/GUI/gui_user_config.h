#ifndef __GUI_USER_CONFIG_H
#define __GUI_USER_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "gui_menu.h"
#include "store.h"
#include "gui_user_file_list.h"

/* 配置文件模式状态 */
typedef enum __ConfigModeState_T
{
    CONFIG_MODE_IDLE,      /* 空闲状态 */
    CONFIG_MODE_SELECTING, /* 选择文件中 */
    CONFIG_MODE_SAVING,    /* 保存文件中 */
    CONFIG_MODE_LOADING,   /* 加载文件中 */
    CONFIG_MODE_ERROR      /* 错误状态 */
} ConfigModeState_T;

/* 配置文件模式状态 */
typedef enum __ConfigShowState_T
{
    CONFIG_SHOW_SELECT_INIT, /* 选择文件（初始化） */
    CONFIG_SHOW_SELECT,      /* 选择文件 */
    CONFIG_SHOW_OPERATOR,    /* 操作文件 */
    CONFIG_SHOW_FINISH_INIT, /* 结束操作（初始化） */
    CONFIG_SHOW_FINISH,      /* 结束操作 */
} ConfigShowState_T;

/* 配置文件模式状态 */
typedef enum __ConfigOperatorState_T
{
    CONFIG_OP_NEW_CONFIG,   /* 新建配置 */
    CONFIG_OP_LOAD_CONFIG,  /* 加载配置 */
    CONFIG_OP_DELETE_CONFIG /* 删除配置 */
} ConfigOperatorState_T;

/* 配置文件管理器函数 */
void ConfigManager_Init(void);
void ConfigManager_Deinit(void);
void ConfigManager_SaveConfig(GUI_Device_T *baseUi);
void ConfigManager_LoadConfig(GUI_Device_T *baseUi);
void ConfigManager_DeleteConfig(void);
void ConfigManager_DrawMainInterface(void);
uint16_t ConfigManager_Function(GUI_FuncItem_T *sender, GUI_MenuParam_T *param);
void ConfigManager_DrawOperationMenu(uint8_t selectedOption, uint8_t maxOptions);

/* 配置文件选择回调 */
void ConfigManager_FileSelectionCallback(FileSelector_T *selector, const char *filePath);

#ifdef __cplusplus
}
#endif

#endif
