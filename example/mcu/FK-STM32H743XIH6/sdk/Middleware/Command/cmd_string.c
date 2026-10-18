#include "cmd_string.h"
#include "cmd_config.h"
#include "cmd_port.h"
#include "cmd_string.h"

/* 命令注册结构 */
typedef struct __CmdStringReg_T
{
    char Name[CMD_STR_MAX_COMMAND_LENGTH + 1]; /* 命令名 */
    CmdStringFunc_T func;                      /* 处理函数 */
    uint8_t parameterCount;                    /* 参数个数 */
    const char *helpString;                    /* 帮助信息 */
    uint8_t inUse;                             /* 使用标志 */
} CmdStringReg_T;

/* 全局命令表 */
static struct __CmdStringDevice_T
{
    CmdStringReg_T commands[CMD_STR_MAX_REG_COUNT]; /* 命令表存储 */
    CmdStringNotFoundCallback_T notFoundCallback;
    uint16_t count;
} s_CmdStringDev = { .count = 0 };

/* 静态缓冲区 */
static CmdStringVar_T s_CmdVariableBuffer[CMD_STR_MAX_VARIABLE_COUNT];

/**
 * @brief  跳过字符串中的空白字符
 * @param  p: 指向当前字符位置的指针
 * @param  end: 指向字符串结束位置的指针
 * @retval 返回指向第一个非空白字符的指针，如果已到字符串末尾则返回end
 * @note   此函数用于在命令解析时跳过命令名或参数之间的空格
 */
static char *CMD_StrPassSpace(char *p, char *end)
{
    if (p == NULL || p >= end) return p;

    while (p < end && *p && isspace((unsigned char)*p))
    {
        p++;
    }
    return p;
}

/**
 * @brief  跳过字符串中的一个单词（非空白字符序列）
 * @param  p: 指向当前字符位置的指针
 * @param  end: 指向字符串结束位置的指针
 * @retval 返回指向单词后第一个空白字符或字符串结尾的指针
 * @note   此函数用于在命令解析时提取命令名或参数
 */
static char *CMD_StrPassWord(char *p, char *end)
{
    if (p == NULL || p >= end) return p;

    while (p < end && *p != 0 && !isspace((unsigned char)*p))
    {
        p++;
    }
    return p;
}

/**
 * @brief  设置命令未找到回调函数
 * @param  callback: 回调函数指针
 * @retval 无
 * @note   此函数用于设置当命令不存在时的回调处理，为Lua等脚本语言预留接口
 */
void CMD_StrSetNotFoundCallback(CmdStringNotFoundCallback_T callback)
{
    s_CmdStringDev.notFoundCallback = callback;
}

/**
 * @brief  在线性表中查找命令
 * @param  Name: 要查找的命令名
 * @param  paramCount: 命令的参数个数
 * @retval 返回找到的命令结构体指针，如果未找到则返回NULL
 * @note   此函数使用线性查找，支持精确参数匹配和可变参数匹配
 */
static CmdStringReg_T *CMD_StrFindCommand(const char *Name, uint16_t paramCount)
{
    for (uint16_t i = 0; i < CMD_STR_MAX_REG_COUNT; i++)
    {
        CmdStringReg_T *cmd = &s_CmdStringDev.commands[i];

        if (cmd->inUse && String_CompareWithoutCase(cmd->Name, Name, CMD_STR_MAX_COMMAND_LENGTH) == 0 && (cmd->parameterCount == paramCount || cmd->parameterCount == CMD_STR_VARIABLE_PARAMS))
        {
            return cmd;
        }
    }

    return NULL;
}

/**
 * @brief  查找最佳匹配命令（优先精确参数匹配）
 * @param  Name: 要查找的命令名
 * @param  paramCount: 命令的参数个数
 * @retval 返回找到的最佳匹配命令结构体指针，如果未找到则返回NULL
 * @note   此函数优先返回参数数量精确匹配的命令，如果没有则返回支持可变参数的命令
 */
static CmdStringReg_T *CMD_StrFindBest(const char *Name, uint16_t paramCount)
{
    CmdStringReg_T *exactMatch = NULL;
    CmdStringReg_T *variableMatch = NULL;

    for (uint16_t i = 0; i < CMD_STR_MAX_REG_COUNT; i++)
    {
        CmdStringReg_T *cmd = &s_CmdStringDev.commands[i];

        if (cmd->inUse && String_CompareWithoutCase(cmd->Name, Name, CMD_STR_MAX_COMMAND_LENGTH) == 0)
        {
            if (cmd->parameterCount == paramCount)
            {
                exactMatch = cmd;
                break; /* 找到精确匹配，立即返回 */
            }
            else if (cmd->parameterCount == CMD_STR_VARIABLE_PARAMS)
            {
                variableMatch = cmd;
            }
        }
    }

    return exactMatch ? exactMatch : variableMatch;
}

/**
 * @brief  参数解析函数
 * @param  argStr 指向参数字符串的指针
 * @param  length 指向参数字符串的指针长度
 * @param  var 指向用于存储解析结果的变量结构体指针
 * @retval 返回解析结果状态码
 * @note   此函数按十六进制(0x...)、二进制(0b...)、浮点数、字符串的顺序尝试解析
 */
static int8_t CMD_StrParseArgument(char *argStr, uint16_t length, CmdStringVar_T *var)
{
    var->string = argStr;

    /* 检查十六进制 */
    if (strnlen(argStr, length) > 2 && argStr[0] == '0' && (argStr[1] == 'x' || argStr[1] == 'X'))
    {
        char *endPtr;
        long num = strtol(argStr + 2, &endPtr, 16);
        if (*endPtr == '\0')
        {
            var->type = CMD_STR_VAR_NUMBER;
            var->floatNumber = (float)num;
            var->intNumber = (int32_t)num;
            var->unsignedNumber = (num >= 0) ? (uint32_t)num : 0;
            return CMD_NO_ERROR;
        }
    }

    /* 检查二进制 */
    if (strnlen(argStr, length) > 2 && argStr[0] == '0' && (argStr[1] == 'b' || argStr[1] == 'B'))
    {
        char *endPtr;
        long num = strtol(argStr + 2, &endPtr, 2);
        if (*endPtr == '\0')
        {
            var->type = CMD_STR_VAR_NUMBER;
            var->floatNumber = (float)num;
            var->intNumber = (int32_t)num;
            var->unsignedNumber = (num >= 0) ? (uint32_t)num : 0;
            return CMD_NO_ERROR;
        }
    }

    /* 尝试解析为浮点数 */
    char *endPtr;
    float num = strtof(argStr, &endPtr);
    if (*endPtr == '\0')
    {
        var->type = CMD_STR_VAR_NUMBER;
        var->floatNumber = num;
        var->intNumber = (int32_t)num;
        var->unsignedNumber = strtol(argStr, &endPtr, 10);
        return CMD_NO_ERROR;
    }

    /* 默认为字符串 */
    var->type = CMD_STR_VAR_STRING;
    var->floatNumber = 0.0f;
    var->intNumber = 0;
    var->unsignedNumber = 0;
    return CMD_NO_ERROR;
}

/**
 * @brief  命令执行函数
 * @param  comX: 串口号
 * @param  cmd: 指向命令字符串的指针
 * @param  cmdLen: 命令字符串的长度
 * @retval 返回执行结果状态码
 * @note   此函数负责解析命令名和参数，并调用相应的处理函数
 */
static int8_t CMD_StrExecuteCommand(uint8_t comX, char *cmd, uint16_t cmdLen)
{
    char *cmdPtr = cmd;
    char *cmdEnd = cmd + cmdLen;
    cmdPtr = CMD_StrPassSpace(cmdPtr, cmdEnd);

    if (*cmdPtr == '\0')
        return CMD_ERROR_COMMAND;

    /* 提取命令名 */
    char *cmdStart = cmdPtr;
    cmdPtr = CMD_StrPassWord(cmdStart, cmdEnd);
    char cmdName[CMD_STR_MAX_COMMAND_LENGTH + 1];
    int len = cmdPtr - cmdStart;
    if (len > CMD_STR_MAX_COMMAND_LENGTH)
        len = CMD_STR_MAX_COMMAND_LENGTH;
    strncpy(cmdName, cmdStart, len);
    cmdName[len] = '\0';

    if (*cmdPtr != '\0')
        *cmdPtr++ = '\0';

    cmdPtr = CMD_StrPassSpace(cmdPtr, cmdEnd);

    /* 解析参数 */
    uint16_t varCount = 0;
    while (*cmdPtr != '\0' && varCount < CMD_STR_MAX_VARIABLE_COUNT)
    {
        if (*cmdPtr == '"')
        {
            /* 处理带引号的字符串 */
            cmdPtr++;
            char *stringStart = cmdPtr;

            while (*cmdPtr != '\0' && *cmdPtr != '"')
                cmdPtr++;

            if (*cmdPtr != '"')
                return CMD_ERROR_PARAMENER_FORMAT;

            *cmdPtr++ = '\0';
            CMD_StrParseArgument(stringStart, cmdPtr - stringStart, &s_CmdVariableBuffer[varCount++]);
        }
        else
        {
            /* 处理普通参数 */
            char *argStart = cmdPtr;
            while (*cmdPtr && !isspace((unsigned char)*cmdPtr))
            {
                if (*cmdPtr == '"')
                    return CMD_ERROR_PARAMENER_FORMAT;
                cmdPtr++;
            }

            if (*cmdPtr != '\0')
                *cmdPtr++ = '\0';

            CMD_StrParseArgument(argStart, cmdPtr - argStart, &s_CmdVariableBuffer[varCount++]);
        }

        cmdPtr = CMD_StrPassSpace(cmdPtr, cmdEnd);
    }

    /* 查找并执行命令 */
    CmdStringReg_T *found = CMD_StrFindBest(cmdName, varCount);
    if (found == NULL)
    {
        /* 如果命令未找到，检查是否有回调函数 */
        if (s_CmdStringDev.notFoundCallback != NULL)
        {
            CmdStringDataPacket_T packet = {
                .varList = s_CmdVariableBuffer,
                .count = varCount
            };
            /* 调用未找到命令的回调函数 */
            return s_CmdStringDev.notFoundCallback(comX, cmdName, &packet);
        }
        else
        {
            return CMD_ERROR_COMMAND;
        }
    }

    if (found->parameterCount != CMD_STR_VARIABLE_PARAMS && found->parameterCount != varCount)
    {
        return CMD_ERROR_PARAMENER_COUNT;
    }

    CmdStringDataPacket_T packet = {
        .varList = s_CmdVariableBuffer,
        .count = varCount
    };

    if (found->func != NULL)
    {
        return found->func(comX, &packet);
    }
    else
    {
        return CMD_ERROR_COMMAND;
    }
}

/**
 * @brief  在命令表中插入命令
 * @param  command: 命令名
 * @param  func: 命令处理函数
 * @param  parameterCount: 参数个数
 * @param  helpString: 帮助信息
 * @retval 返回插入结果状态码
 * @note   如果命令已存在，则更新现有命令；如果命令表已满，返回错误
 */
static int8_t CMD_StrInsertToTable(const char *command, CmdStringFunc_T func, uint8_t parameterCount, const char *helpString)
{
    /* 首先检查是否已存在相同命令 */
    for (uint16_t i = 0; i < CMD_STR_MAX_REG_COUNT; i++)
    {
        CmdStringReg_T *cmd = &s_CmdStringDev.commands[i];

        if (cmd->inUse && String_CompareWithoutCase(cmd->Name, command, CMD_STR_MAX_COMMAND_LENGTH) == 0 && cmd->parameterCount == parameterCount)
        {
            /* 更新现有命令 */
            cmd->func = func;
            cmd->helpString = helpString;
            return CMD_NO_ERROR;
        }
    }

    /* 找到第一个空位插入 */
    for (uint16_t i = 0; i < CMD_STR_MAX_REG_COUNT; i++)
    {
        CmdStringReg_T *cmd = &s_CmdStringDev.commands[i];

        if (!cmd->inUse)
        {
            strncpy(cmd->Name, command, CMD_STR_MAX_COMMAND_LENGTH);
            cmd->Name[CMD_STR_MAX_COMMAND_LENGTH] = '\0';
            cmd->func = func;
            cmd->parameterCount = parameterCount;
            cmd->helpString = helpString;
            cmd->inUse = TRUE;

            s_CmdStringDev.count++;
            return CMD_NO_ERROR;
        }
    }

    /* 命令表已满 */
    return CMD_ERROR_BUFFER_OVERFLOW;
}

/**
 * @brief  添加命令到系统中
 * @param  command: 命令名
 * @param  func: 命令处理函数
 * @param  parameterCount: 参数个数，CMD_STR_VARIABLE_PARAMS表示可变参数
 * @param  helpString: 帮助信息字符串
 * @retval 返回添加结果状态码
 * @note   如果命令已存在，则更新现有命令；如果系统已满，返回错误
 */
int8_t CMD_StrAdd(const char *command, CmdStringFunc_T func, uint8_t parameterCount, const char *helpString)
{
    if (s_CmdStringDev.count >= CMD_STR_MAX_REG_COUNT)
        return CMD_ERROR_BUFFER_OVERFLOW;

    return CMD_StrInsertToTable(command, func, parameterCount, helpString);
}

/**
 * @brief  清理命令设备，移除所有已注册的命令
 * @param  无
 * @retval 无
 * @note   此函数将重置命令表，清除所有命令注册信息
 */
void CMD_StrClearUp(void)
{
    for (int i = 0; i < CMD_STR_MAX_REG_COUNT; i++)
    {
        s_CmdStringDev.commands[i].inUse = FALSE;
        s_CmdStringDev.commands[i].Name[0] = '\0';
    }
    s_CmdStringDev.count = 0;
}

/**
 * @brief  内置的帮助命令处理函数
 * @param  comX: 串口号
 * @param  packet: 指向命令数据包的指针，包含参数列表
 * @retval 返回执行结果状态码
 * @note   此函数显示一个或多个指定命令的帮助信息
 */
static uint16_t CMD_StrHelp(uint8_t comX, CmdStringDataPacket_T *packet)
{
    /* 实现帮助命令 */
    for (uint16_t i = 0; i < packet->count; i++)
    {
        const char *cmdName = packet->varList[i].string;
        uint8_t found = FALSE;

        for (int j = 0; j < CMD_STR_MAX_REG_COUNT; j++)
        {
            CmdStringReg_T *cmd = &s_CmdStringDev.commands[j];
            if (cmd->inUse && String_CompareWithoutCase(cmd->Name, cmdName, CMD_STR_MAX_COMMAND_LENGTH) == 0)
            {
                CMD_Printf(comX, "命令 %s 的帮助信息\r\n", cmdName);
                if (cmd->parameterCount == CMD_STR_VARIABLE_PARAMS)
                    CMD_Printf(comX, "用法 (可变参数): \r\n%s\r\n", cmd->helpString);
                else
                    CMD_Printf(comX, "用法 (%d 参数): \r\n%s\r\n", cmd->parameterCount, cmd->helpString);
                found = TRUE;
            }
        }

        if (!found)
            CMD_Printf(comX, "未找到命令: %s\r\n", cmdName);

        if (i < packet->count - 1)
            CMD_Printf(comX, "--------------------------------\r\n");
    }
    return CMD_NO_ERROR;
}

/**
 * @brief  内置的列表命令处理函数
 * @param  comX: 串口号
 * @param  packet: 指向命令数据包的指针（此命令不使用参数）
 * @retval 返回执行结果状态码
 * @note   此函数列出所有已注册的命令，并显示每个命令的用法数量
 */
static uint16_t CMD_StrList(uint8_t comX, CmdStringDataPacket_T *packet)
{
    CMD_Printf(comX, "可用命令:\r\n");
    uint8_t lineCount = 0;

    /* 用于统计命令名和用法数量 */
    typedef struct
    {
        char Name[CMD_STR_MAX_COMMAND_LENGTH + 1];
        uint8_t usageCount;
    } CmdUsageInfo_T;

    CmdUsageInfo_T cmdUsages[CMD_STR_MAX_REG_COUNT];
    uint8_t uniqueCmdCount = 0;

    /* 第一遍：统计每个命令名的用法数量 */
    for (int i = 0; i < CMD_STR_MAX_REG_COUNT; i++)
    {
        CmdStringReg_T *cmd = &s_CmdStringDev.commands[i];
        if (cmd->inUse)
        {
            /* 查找是否已存在该命令名 */
            int foundIndex = -1;
            for (int j = 0; j < uniqueCmdCount; j++)
            {
                if (String_CompareWithoutCase(cmd->Name, cmdUsages[j].Name, CMD_STR_MAX_REG_COUNT) == 0)
                {
                    foundIndex = j;
                    break;
                }
            }

            if (foundIndex >= 0)
            {
                /* 已存在，增加用法计数 */
                cmdUsages[foundIndex].usageCount++;
            }
            else
            {
                /* 新命令，添加到统计数组 */
                strncpy(cmdUsages[uniqueCmdCount].Name, cmd->Name, CMD_STR_MAX_COMMAND_LENGTH);
                cmdUsages[uniqueCmdCount].Name[CMD_STR_MAX_COMMAND_LENGTH] = '\0';
                cmdUsages[uniqueCmdCount].usageCount = 1;
                uniqueCmdCount++;
            }
        }
    }

    /* 第二遍：显示命令和用法数 */
    for (int i = 0; i < uniqueCmdCount; i++)
    {
        CMD_Printf(comX, "%s(%d)", cmdUsages[i].Name, cmdUsages[i].usageCount);

        if (++lineCount >= 6)
        {
            CMD_Printf(comX, "\r\n");
            lineCount = 0;
        }
        else
        {
            CMD_Printf(comX, ", ");
        }
    }

    if (lineCount != 0)
        CMD_Printf(comX, "\r\n");
    CMD_Printf(comX, "总计 %d 个命令\r\n", uniqueCmdCount);
    return CMD_NO_ERROR;
}

/**
 * @brief  内置的测试命令处理函数
 * @param  comX: 串口号
 * @param  packet: 指向命令数据包的指针，包含参数列表
 * @retval 返回执行结果状态码
 * @note   此函数用于测试系统，打印接收到的参数个数和内容
 */
static uint16_t CMD_StrTest(uint8_t comX, CmdStringDataPacket_T *packet)
{
    CMD_Printf(comX, "测试指令, 参数个数=%d.\r\n", packet->count);
    for (int i = 0; i < packet->count; i++)
    {
        if (packet->varList[i].type == CMD_STR_VAR_NUMBER)
        {
            CMD_Printf(comX, "-- 参数(%d) - 数字类型: %.2f (%s)\r\n", i, packet->varList[i].floatNumber, packet->varList[i].string);
        }
        else
        {
            CMD_Printf(comX, "-- 参数(%d) - 字符类型: %s\r\n", i, packet->varList[i].string);
        }
    }
    return CMD_NO_ERROR;
}

/**
 * @brief  处理命令字符串
 * @param  comX: 串口号
 * @param  str: 指向命令字符串的指针
 * @param  len: 命令字符串的长度
 * @retval 无
 * @note   此函数调用命令执行函数，并根据返回的错误码打印相应的错误信息
 */
void CMD_StrProcess(uint8_t comX, char *str, uint16_t len)
{
    int error = CMD_StrExecuteCommand(comX, str, len);

    /* 错误处理保持不变... */
    switch (error)
    {
    case CMD_NO_ERROR:
        break;
    case CMD_ERROR_COMMAND:
        CMD_Printf(comX, "错误: 未知命令 '%s'\r\n", str);
        CMD_Printf(comX, "  - 使用 'list' 列出可用命令\r\n");
        break;
    case CMD_ERROR_EXCUTE:
        CMD_Printf(comX, "错误: 命令执行错误 '%s'\r\n", str);
        break;
    case CMD_ERROR_PARAMENER_FORMAT:
        CMD_Printf(comX, "错误: 参数格式错误 '%s'\r\n", str);
        CMD_Printf(comX, "  - 使用 'help' 查看命令帮助\r\n");
        break;
    case CMD_ERROR_PARAMENER_COUNT:
        CMD_Printf(comX, "错误: 参数数量不正确 '%s'\r\n", str);
        CMD_Printf(comX, "  - 使用 'help' 查看命令帮助\r\n");
        break;
    case CMD_ERROR_BUFFER_OVERFLOW:
        CMD_Printf(comX, "错误: 缓冲区溢出 '%s'\r\n", str);
        break;
    default:
        CMD_Printf(comX, "错误: 未知错误 '%s' (代码: %d)\r\n", str, error);
        break;
    }
}

/**
 * @brief  初始化命令设备，注册内置命令
 * @param  无
 * @retval 无
 * @note   此函数会清空现有命令，并注册list、help、test三个内置命令
 */
void CMD_StrBaseInit(void)
{
    CMD_StrClearUp();

    CMD_StrAdd("list", CMD_StrList, 0, "list - 列出所有指令");
    CMD_StrAdd("help", CMD_StrHelp, CMD_STR_VARIABLE_PARAMS, "help [cmd1] [cmd2] ... - 展示这些指令的信息");
    CMD_StrAdd("test", CMD_StrTest, CMD_STR_VARIABLE_PARAMS, "test [...] - 测试指令");
}

/**
 * @brief  从系统中删除一个命令
 * @param  command: 要删除的命令名
 * @param  parameterCount: 命令的参数个数
 * @retval 返回删除结果状态码
 * @note   此函数会查找并移除指定的命令，如果未找到则返回错误
 */
int8_t CMD_StrRemove(const char *command, uint8_t parameterCount)
{
    for (uint16_t i = 0; i < CMD_STR_MAX_REG_COUNT; i++)
    {
        CmdStringReg_T *cmd = &s_CmdStringDev.commands[i];

        if (cmd->inUse && String_CompareWithoutCase(cmd->Name, command, CMD_STR_MAX_COMMAND_LENGTH) == 0 && cmd->parameterCount == parameterCount)
        {
            cmd->inUse = FALSE;
            cmd->Name[0] = '\0';
            s_CmdStringDev.count--;
            return CMD_NO_ERROR;
        }
    }

    return CMD_ERROR_COMMAND;
}

/**
 * @brief  查找命令（对外接口）
 * @param  command: 要查找的命令名
 * @param  parameterCount: 命令的参数个数
 * @retval 返回找到的命令结构体指针，如果未找到则返回NULL
 * @note   此函数是静态查找函数的对外封装接口
 */
CmdStringReg_T *CMD_StrFind(const char *command, uint8_t parameterCount)
{
    return CMD_StrFindCommand(command, parameterCount);
}
