#ifndef __CMD_STRING_H
#define __CMD_STRING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 变量类型 */
typedef enum __CmdStringVarType_T
{
    CMD_STR_VAR_NUMBER,
    CMD_STR_VAR_STRING
} CmdStringVarType_T;

/* 变量结构 */
typedef struct __CmdStringVar_T
{
    char *string;
    volatile float floatNumber;
    volatile int32_t intNumber;
    volatile uint32_t unsignedNumber;
    CmdStringVarType_T type;
} CmdStringVar_T;

/* 变量数据包 */
typedef struct __CmdStringDataPacket_T
{
    CmdStringVar_T *varList;
    uint16_t count;
} CmdStringDataPacket_T;

/* 命令函数类型 */
typedef uint16_t (*CmdStringFunc_T)(uint8_t comX, CmdStringDataPacket_T *packet);

/* 命令未找到回调函数指针类型 */
typedef uint16_t (*CmdStringNotFoundCallback_T)(uint8_t comX, const char *commandName, CmdStringDataPacket_T *packet);

/* 命令注册函数 */
int8_t CMD_StrAdd(const char *command, CmdStringFunc_T func, uint8_t parameterCount, const char *helpString);
void CMD_StrClearUp(void);
void CMD_StrProcess(uint8_t comX, char *str, uint16_t len);
void CMD_StrSetNotFoundCallback(CmdStringNotFoundCallback_T callback);

/* 初始化函数 */
void CMD_StrInit(void);
void CMD_StrBaseInit(void);

/* 工具函数 */
void CMD_StrReadDfa(uint8_t comX, uint8_t *rxData, uint16_t rxDataLength);

#ifdef __cplusplus
}
#endif

#endif
