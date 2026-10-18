#include "gui.h"
#include "gui_user_config.h"

#include <string.h>
#include <stdlib.h>

#include "bsp_system.h"
#include "file.h"

/* 全局变量 */
ConfigModeState_T g_ConfigState; /* 配置文件管理器状态 */

/* 配置文件管理器局部变量 */
static FileSelector_T s_FileSelector; /* 文件选择器 */
static char s_StatusMsg[128];         /* 状态信息 */
static uint8_t s_InitFlag = 0;

extern const char *FR_Table[];

/**
 * @brief 生成基于时间的文件名
 * @param filename 输出文件名缓冲区
 * @param size 缓冲区大小
 */
static void ConfigManager_GenerateFilename(char *filename, uint32_t size)
{
    RTC_ReadTime();
    /* 生成文件名 */
    snprintf(filename, size, "0:/config/%04d年%02d月%02d日 %02d时%02d分%02d秒.bin",
             g_RTC_Data.year, g_RTC_Data.month, g_RTC_Data.day,
             g_RTC_Data.hour, g_RTC_Data.minute, g_RTC_Data.second);
}

/**
 * @brief 初始化配置文件管理器
 */
void ConfigManager_Init(void)
{
    /* 先清除缓存 */
    ConfigManager_Deinit();

    /* 配置文件选择器 */
    FileSelectorConfig_T config = {
        .directoryPath = "0:/config",
        .filePattern = "*.bin",
        .maxDisplayItems = 10,
        .itemHeight = 30,
        .StartY = 60,
        .titleHeight = 30,
        .hintHeight = 90,
        .showTitle = "配置程序文件选择器"
    };

    /* 初始化文件选择器 */
    FileSelector_Init(&s_FileSelector, &config);

    if (FileSelector_GetState(&s_FileSelector) == FILE_SELECTOR_SELECTING)
    {
        g_ConfigState = CONFIG_MODE_SELECTING;
        s_InitFlag = 1;
    }
    else
    {
        g_ConfigState = CONFIG_MODE_ERROR;
        snprintf(s_StatusMsg, sizeof(s_StatusMsg), "初始化文件选择器失败");
    }
}

/**
 * @brief 释放配置文件管理器资源
 */
void ConfigManager_Deinit(void)
{
    FileSelector_Deinit(&s_FileSelector);
    s_InitFlag = 0;
}

/**
 * @brief 保存配置文件
 */
void ConfigManager_SaveConfig(GUI_Device_T *baseUi)
{
    char filename[128];

    /* 生成基于时间的文件名 */
    ConfigManager_GenerateFilename(filename, sizeof(filename));

    /* 确保目录存在（使用专用函数） */
    FatFS_CreateDirectory("0:/config");

    /* 设置魔数标识有效数据 */
    g_ParamData.MagicNumber = PARAM_MAGIC_NUMBER;

    /* 自动处理FIL对象的分配和释放 */
    if (FatFS_WriteFile(filename, (uint8_t *)&g_ParamData, sizeof(ParamData_T)) != sizeof(ParamData_T))
    {
        snprintf(s_StatusMsg, sizeof(s_StatusMsg), "保存失败: %s", filename);
        g_ConfigState = CONFIG_MODE_ERROR;
    }
    else
    {
        snprintf(s_StatusMsg, sizeof(s_StatusMsg), "保存成功: %s", filename);
        g_ConfigState = CONFIG_MODE_IDLE;
    }

    /* 刷新文件列表 */
    FileSelector_LoadFileList(&s_FileSelector);
}

/**
 * @brief 加载配置文件
 */
void ConfigManager_LoadConfig(GUI_Device_T *baseUi)
{
    const char *filePath = FileSelector_GetCurrentFilePath(&s_FileSelector);
    uint32_t fileSize;

    if (filePath == NULL)
    {
        snprintf(s_StatusMsg, sizeof(s_StatusMsg), "未选择文件!");
        g_ConfigState = CONFIG_MODE_ERROR;
        return;
    }

    /* 获取文件大小 */
    fileSize = FatFS_GetFileSize(filePath);
    if (fileSize == 0)
    {
        snprintf(s_StatusMsg, sizeof(s_StatusMsg), "文件为空: %s", filePath);
        g_ConfigState = CONFIG_MODE_ERROR;
        return;
    }

    /* 为新数据分配内存 */
    ParamData_T *newData = Memory_Malloc(MP_RAM_IN, sizeof(ParamData_T));
    if (newData == NULL)
    {
        snprintf(s_StatusMsg, sizeof(s_StatusMsg), "内存分配失败!");
        g_ConfigState = CONFIG_MODE_ERROR;
        return;
    }

    /* 读取文件内容 */
    if (FatFS_ReadFile(filePath, (uint8_t *)newData, sizeof(ParamData_T)) <= 0)
    {
        snprintf(s_StatusMsg, sizeof(s_StatusMsg), "文件读取失败!");
        g_ConfigState = CONFIG_MODE_ERROR;
        Memory_Free(MP_RAM_IN, newData);
        return;
    }

    /* 文件大小与结构体大小相同，检查magic number */
    if (newData->MagicNumber == PARAM_MAGIC_NUMBER)
    {
        snprintf(s_StatusMsg, sizeof(s_StatusMsg), "加载成功: %s", filePath);
        Memory_Copy(&g_ParamData, newData, sizeof(ParamData_T));
        Param_LoadFromData(newData);
        g_ConfigState = CONFIG_MODE_IDLE;
    }
    else
    {
        /* Magic Number不匹配 */
        snprintf(s_StatusMsg, sizeof(s_StatusMsg), "魔数不匹配 (当前: 0x%08X, 期望: 0x%08X)", newData->MagicNumber, PARAM_MAGIC_NUMBER);
        g_ConfigState = CONFIG_MODE_ERROR;
    }

    /* 释放临时分配的内存 */
    Memory_Free(MP_RAM_IN, newData);
}

/**
 * @brief 删除选中的配置文件
 */
void ConfigManager_DeleteConfig(void)
{
    const char *filePath = FileSelector_GetCurrentFilePath(&s_FileSelector);

    if (filePath == NULL)
    {
        snprintf(s_StatusMsg, sizeof(s_StatusMsg), "未选择文件!");
        g_ConfigState = CONFIG_MODE_ERROR;
        return;
    }

    if (FatFS_DeleteFile(filePath) == FATFS_RET_OK)
    {
        snprintf(s_StatusMsg, sizeof(s_StatusMsg), "删除成功: %s", filePath);
        /* 重新加载文件列表 */
        FileSelector_LoadFileList(&s_FileSelector);
        g_ConfigState = CONFIG_MODE_IDLE;
    }
    else
    {
        snprintf(s_StatusMsg, sizeof(s_StatusMsg), "删除失败: %s", filePath);
        g_ConfigState = CONFIG_MODE_ERROR;
    }
}

/**
 * @brief 绘制主界面
 */
void ConfigManager_DrawMainInterface(void)
{
    /* ===== 清屏 + 标题栏 ===== */
    GUI_LCD_FillColor(COL(0), ROW(0), MENU_AREA_WIDTH, GUI_SCREEN_HEIGHT, GUI_BLACK);
    GUI_LCD_FillColor(COL(0), ROW(0), MENU_AREA_WIDTH, TITLE_HEIGHT, GUI_BLUE);
    GUI_LCD_ShowString(COL(0), ROW(0) + 3, "配置文件管理器", GUI_WHITE, GUI_BLUE);

    /* ===== 状态标签 ===== */
    GUI_LCD_ShowString(COL(0), ROW(2) + 3, "状态:", GUI_WHITE, GUI_BLACK);

    switch (g_ConfigState)
    {
    case CONFIG_MODE_ERROR:
        GUI_LCD_ShowString(COL(7), ROW(2) + 3, "错误  ", GUI_MAGENTA, GUI_BLACK);
        GUI_LCD_ShowStringBlock(COL(0), ROW(3) + 10, MENU_AREA_WIDTH, 120, s_StatusMsg, GUI_YELLOW, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(9) + 3, "任意键:退出      ", GUI_WHITE, GUI_BLACK);
        break;

    case CONFIG_MODE_IDLE:
        GUI_LCD_ShowString(COL(7), ROW(2) + 3, "完成  ", GUI_GREEN, GUI_BLACK);
        GUI_LCD_ShowStringBlock(COL(0), ROW(3) + 10, MENU_AREA_WIDTH, 120, s_StatusMsg, GUI_YELLOW, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(9) + 3, "任意键:退出", GUI_WHITE, GUI_BLACK);
        break;

    case CONFIG_MODE_SAVING:
        GUI_LCD_ShowString(COL(7), ROW(2) + 3, "保存中", GUI_CYAN, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(3) + 3, "正在保存配置文件...", GUI_WHITE, GUI_BLACK);
        break;

    case CONFIG_MODE_LOADING:
        GUI_LCD_ShowString(COL(7), ROW(2) + 3, "加载中", GUI_CYAN, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(3) + 3, "正在加载配置文件...", GUI_WHITE, GUI_BLACK);
        break;

    default:
        GUI_LCD_ShowString(COL(7), ROW(2) + 3, "就绪  ", GUI_YELLOW, GUI_BLACK);
        break;
    }
}

/**
 * @brief 执行选中的配置操作
 * @param selectedOption 选中的操作选项
 * @param baseUi 基础UI设备
 */
static void ConfigManager_ExecuteOperation(uint8_t selectedOption, GUI_Device_T *baseUi)
{
    FileSelectorState_T state = FileSelector_GetState(&s_FileSelector);

    if (state != FILE_SELECTOR_SELECTING || s_FileSelector.fileList.count <= 0)
    {
        /* 无文件时直接新建保存 */
        g_ConfigState = CONFIG_MODE_SAVING;
        ConfigManager_SaveConfig(baseUi);
        return;
    }

    switch (selectedOption)
    {
    case CONFIG_OP_LOAD_CONFIG: /* 加载配置 */
        g_ConfigState = CONFIG_MODE_LOADING;
        ConfigManager_LoadConfig(baseUi);
        break;

    case CONFIG_OP_NEW_CONFIG: /* 新建配置 */
        g_ConfigState = CONFIG_MODE_SAVING;
        ConfigManager_SaveConfig(baseUi);
        break;

    case CONFIG_OP_DELETE_CONFIG: /* 删除配置 */
        ConfigManager_DeleteConfig();
        break;
    }
}

/**
 * @brief 文件选择回调函数
 * @param selector 文件选择器实例
 * @param filePath 选择的文件路径
 */
void ConfigManager_FileSelectionCallback(FileSelector_T *selector, const char *filePath)
{
    /* 进入操作模式 */
    ConfigManager_DrawOperationMenu(CONFIG_OP_NEW_CONFIG, 3);
}

/**
 * @brief 配置文件管理器主函数
 * @param sender 事件发送者
 */
uint16_t ConfigManager_Function(GUI_FuncItem_T *sender, GUI_MenuParam_T *param)
{
    static uint8_t selectedOption = CONFIG_OP_NEW_CONFIG;
    uint8_t maxOptions = (s_FileSelector.fileList.count > 0) ? 3 : 1; /* 有文件时3个选项 */

    /* 首次进入时初始化 */
    if (param->InitFlag != 0)
    {
        GUI_LCD_FillColor(COL(0), ROW(0), MENU_AREA_WIDTH, GUI_SCREEN_HEIGHT, GUI_BLACK);
        /* 龙脉图像已删除 */
        GUI_LCD_FillColor(COL(0), ROW(0), MENU_AREA_WIDTH, TITLE_HEIGHT, GUI_BLUE);
        return CONFIG_SHOW_SELECT_INIT;
    }

    /* 根据不同状态选择不同操作 */
    switch (param->userState)
    {
    case CONFIG_SHOW_SELECT_INIT: /* 选择文件中 */
    {
        Key_ClearAll();
        ConfigManager_Init();
        FileSelector_DrawFileList(&s_FileSelector);
        selectedOption = CONFIG_OP_NEW_CONFIG;
        return CONFIG_SHOW_SELECT;
    }
    case CONFIG_SHOW_SELECT: /* 选择文件中 */
    {
        /* 使用文件选择器处理按键输入 */
        if (Key_IsAction(KEY_MODE))
        {
            /* 选择模式下按MODE退出 */
            ConfigManager_Deinit();
            GUI_ExitPage(param->baseUI);
        }
        else if (Key_IsAction(KEY_ENTER))
        {
            /* ENTER键进入操作模式 */
            ConfigManager_FileSelectionCallback(&s_FileSelector, FileSelector_GetCurrentFilePath(&s_FileSelector));
            return CONFIG_SHOW_OPERATOR;
        }
        else
        {
            /* 其他按键交给文件选择器处理 */
            FileSelector_HandleKeyInput(&s_FileSelector, NULL);
            return CONFIG_SHOW_SELECT;
        }
    }
    case CONFIG_SHOW_OPERATOR: /* 保存文件中 */
    {
        /* 上下键选择操作 */
        if (Key_IsAction(KEY_UP))
        {
            if (s_FileSelector.fileList.count > 0)
            {
                selectedOption = (selectedOption + 1) % maxOptions;
                ConfigManager_DrawOperationMenu(selectedOption, maxOptions);
            }
        }
        else if (Key_IsAction(KEY_DOWN))
        {
            if (s_FileSelector.fileList.count > 0)
            {
                selectedOption = (selectedOption + maxOptions - 1) % maxOptions;
                ConfigManager_DrawOperationMenu(selectedOption, maxOptions);
            }
        }
        else if (Key_IsAction(KEY_ENTER))
        {
            ConfigManager_ExecuteOperation(selectedOption, param->baseUI);

            /* 删除操作后直接返回文件列表 */
            if (selectedOption == CONFIG_OP_DELETE_CONFIG)
            {
                FileSelector_DrawFileList(&s_FileSelector);
                return CONFIG_SHOW_SELECT_INIT;
            }
            else if (selectedOption == CONFIG_OP_NEW_CONFIG)
            {
                return CONFIG_SHOW_FINISH_INIT;
            }
            else
            {
                ConfigManager_DrawMainInterface();
                return CONFIG_SHOW_FINISH_INIT;
            }
        }
        else if (Key_IsAction(KEY_MODE))
        {
            /* 操作模式下按MODE返回选择模式 */
            FileSelector_DrawFileList(&s_FileSelector);
            return CONFIG_SHOW_SELECT_INIT;
        }
        return CONFIG_SHOW_OPERATOR;
    }
    case CONFIG_SHOW_FINISH_INIT:
    {
        ConfigManager_DrawMainInterface();
        return CONFIG_SHOW_FINISH;
    }
    case CONFIG_SHOW_FINISH: /* 保存文件中 */
    {
        /* 检查任意按键是否按下 */
        if (Key_IsAnyKeyPressed())
        {
            g_ConfigState = CONFIG_MODE_IDLE;
            return CONFIG_SHOW_SELECT_INIT;
        }
        return CONFIG_SHOW_FINISH;
    }
    default:
        return CONFIG_SHOW_SELECT_INIT;
    }
}

/**
 * @brief 绘制操作菜单（高亮选中的选项）
 */
void ConfigManager_DrawOperationMenu(uint8_t selectedOption, uint8_t maxOptions)
{
    /* ===== 清屏 + 标题栏 ===== */
    GUI_LCD_FillColor(COL(0), ROW(0), MENU_AREA_WIDTH, GUI_SCREEN_HEIGHT, GUI_BLACK);
    /* 龙脉图像已删除 */
    GUI_LCD_FillColor(COL(0), ROW(0), MENU_AREA_WIDTH, TITLE_HEIGHT, GUI_BLUE);
    GUI_LCD_ShowString(COL(0), ROW(0) + 3, "配置文件操作", GUI_WHITE, GUI_BLUE);

    if (s_FileSelector.fileList.count > 0)
    {
        const char *fileName = s_FileSelector.fileList.files[s_FileSelector.currentIndex].Name;

        GUI_LCD_ShowString(COL(0), ROW(2) + 3, "选中文件:", GUI_WHITE, GUI_BLACK);
        GUI_LCD_ShowStringBlock(COL(0), ROW(3) + 10, MENU_AREA_WIDTH, 24, fileName, GUI_YELLOW, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(5) + 3, "请选择操作:", GUI_CYAN, GUI_BLACK);

        /* ===== 加载配置选项 ===== */
        if (selectedOption == CONFIG_OP_LOAD_CONFIG)
        {
            GUI_LCD_FillColor(COL(0), ROW(6) + 10, MENU_AREA_WIDTH, 30, GUI_GRAY);
            GUI_LCD_ShowString(COL(0), ROW(6) + 13, "加载配置", GUI_WHITE, GUI_GRAY);
        }
        else
        {
            GUI_LCD_ShowString(COL(0), ROW(6) + 13, "加载配置", GUI_WHITE, GUI_BLACK);
        }

        /* ===== 新建保存选项 ===== */
        if (selectedOption == CONFIG_OP_NEW_CONFIG)
        {
            GUI_LCD_FillColor(COL(0), ROW(7) + 10, MENU_AREA_WIDTH, 30, GUI_GRAY);
            GUI_LCD_ShowString(COL(0), ROW(7) + 13, "新建配置", GUI_WHITE, GUI_GRAY);
        }
        else
        {
            GUI_LCD_ShowString(COL(0), ROW(7) + 13, "新建配置", GUI_WHITE, GUI_BLACK);
        }

        /* ===== 删除配置选项 ===== */
        if (selectedOption == CONFIG_OP_DELETE_CONFIG)
        {
            GUI_LCD_FillColor(COL(0), ROW(8) + 10, MENU_AREA_WIDTH, 30, GUI_GRAY);
            GUI_LCD_ShowString(COL(0), ROW(8) + 13, "删除配置", GUI_WHITE, GUI_GRAY);
        }
        else
        {
            GUI_LCD_ShowString(COL(0), ROW(8) + 13, "删除配置", GUI_WHITE, GUI_BLACK);
        }

        GUI_LCD_ShowString(COL(0), ROW(10) + 3, "模式:返回列表", GUI_YELLOW, GUI_BLACK);
    }
    else
    {
        GUI_LCD_ShowString(COL(0), ROW(2) + 3, "无配置文件", GUI_RED, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(5) + 3, "请选择操作:", GUI_CYAN, GUI_BLACK);
        GUI_LCD_ShowString(COL(0), ROW(6) + 13, "新建保存", GUI_WHITE, GUI_GRAY);
        GUI_LCD_ShowString(COL(0), ROW(10) + 3, "模式:返回", GUI_YELLOW, GUI_BLACK);
    }
}