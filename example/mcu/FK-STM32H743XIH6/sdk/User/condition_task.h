#ifndef __CONDITION_TASK_H
#define __CONDITION_TASK_H

#include "bsp_system.h"
#include "expression.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 条件域数量定义 */
#define CONDITION_DOMAIN_COUNT    12
#define CONDITION_DOMAIN_INFINITE 0xFFFFFFFF

/* 条件域状态枚举 */
typedef enum __ConditionState_T
{
    CONDITION_DOMAIN_STATE_DISABLED = 0, /* 禁用状态 */
    CONDITION_DOMAIN_STATE_ENABLED,      /* 启用但未触发 */
    CONDITION_DOMAIN_STATE_TRIGGERED,    /* 已触发 */
    CONDITION_DOMAIN_STATE_EXPIRED       /* 已过期 */
} ConditionState_T;

/* 条件域结构体 */
typedef struct __Condition_T
{
    /* 基本配置 */
    uint8_t domainId;        /* 域ID (0-31) */
    uint8_t expressionValid; /* 表达式是否有效 */
    ConditionState_T state;  /* 当前状态 */
    uint32_t durationMs;     /* 持续时间(ms)，0xFFFFFFFF表示永久 */

    /* 表达式相关 */
    char *condExpr;          /* 条件表达式字符串 */
    char *triggerExpr;       /* 触发表达式字符串 */
    uint32_t condExprLen;    /* 条件表达式长度 */
    uint32_t triggerExprLen; /* 触发表达式长度 */

    /* 触发时刻信息 */
    float triggerX, triggerY;      /* 触发时的位置坐标 */
    float triggerYaw, triggerTick; /* 触发时的姿态角 */

    /* 变化量信息 (相对于触发时刻) */
    float deltaX, deltaY;      /* 位置变化量 */
    float deltaYaw, deltaTime; /* 姿态变化量 */

    /* 统计信息 */
    uint32_t triggerCount;   /* 触发次数统计 */
    uint32_t lastUpdateTick; /* 最后更新时间 */
} Condition_T;

/* 条件域管理器 */
typedef struct __ConditionManager_T
{
    Condition_T domains[CONDITION_DOMAIN_COUNT]; /* 条件域数组 */
    uint8_t initialized;                         /* 初始化标志 */
    uint32_t systemStartTick;                    /* 系统启动时刻 */

} ConditionManager_T;

/* 全局变量声明 */
extern ConditionManager_T g_ConditionManager;

/* 函数声明 */
void Condition_Init(void);
uint8_t Condition_Configure(uint8_t domainId, const char *condExpr, const char *triggerExpr, uint32_t durationMs);
uint8_t Condition_Enable(uint8_t domainId);
uint8_t Condition_Disable(uint8_t domainId);
uint8_t Condition_Reset(uint8_t domainId);
uint8_t Condition_UpdateVariables(uint8_t domainId);
uint8_t Condition_Evaluate(uint8_t domainId, uint8_t *result);
uint8_t Condition_GetState(uint8_t domainId, ConditionState_T *state);
uint8_t Condition_GetTriggerInfo(uint8_t domainId, float *deltaX, float *deltaY, float *deltaZ, float *deltaYaw, float *deltaTime);
uint8_t Condition_IsTriggered(uint8_t domainId);
uint8_t Condition_GetExpression(uint8_t domainId, char *buffer, uint32_t bufferSize);
void Condition_UpdateAllDomains(void);
void Condition_PrintStatus(char *buffer, uint32_t bufferSize);
uint8_t Condition_CalcTriggerExpr(uint8_t domainId);

#ifdef __cplusplus
}
#endif

#endif /* __CONDITION_DOMAIN_H */