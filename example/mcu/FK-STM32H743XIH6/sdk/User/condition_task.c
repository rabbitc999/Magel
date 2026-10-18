#include "condition_task.h"
#include "imu.h"
#include "car.h"
#include "bsp_system.h"
#include "fast_math.h"
#include "simple_task.h"
#include "store.h"
#include <string.h>

/* 全局变量定义 */
ConditionManager_T g_ConditionManager = { 0 };
extern uint8_t g_ConditionBeepFlag, g_ConditionCancelFlag;

/**
 * @brief  初始化条件域管理器
 * @note   清空所有条件域状态，设置初始化标志
 */
void Condition_Init(void)
{
    /* 清空管理器 */
    Memory_Set(&g_ConditionManager, 0, sizeof(ConditionManager_T));

    /* 初始化每个条件域 */
    for (uint8_t i = 0; i < CONDITION_DOMAIN_COUNT; i++)
    {
        g_ConditionManager.domains[i].domainId = i;
        g_ConditionManager.domains[i].state = CONDITION_DOMAIN_STATE_DISABLED;
        g_ConditionManager.domains[i].durationMs = 0;
        g_ConditionManager.domains[i].condExpr = NULL;
        g_ConditionManager.domains[i].triggerExpr = NULL;
        g_ConditionManager.domains[i].condExprLen = 0;
        g_ConditionManager.domains[i].triggerExprLen = 0;
        g_ConditionManager.domains[i].expressionValid = FALSE;
        g_ConditionManager.domains[i].triggerCount = 0;
    }

    /* 记录系统启动时间 */
    g_ConditionManager.systemStartTick = HAL_GetTick();
    g_ConditionManager.initialized = TRUE;
}

/**
 * @brief  释放指定条件域的表达式内存
 * @param  domainId 域ID (0-31)
 */
void Condition_FreeExpression(uint8_t domainId)
{
    if (domainId >= CONDITION_DOMAIN_COUNT)
    {
        return;
    }

    Condition_T *domain = &g_ConditionManager.domains[domainId];

    if (domain->condExpr != NULL)
    {
        Memory_Free(MP_RAM_EX, domain->condExpr);
        domain->condExpr = NULL;
        domain->condExprLen = 0;
    }

    if (domain->triggerExpr != NULL)
    {
        Memory_Free(MP_RAM_EX, domain->triggerExpr);
        domain->triggerExpr = NULL;
        domain->triggerExprLen = 0;
    }

    domain->expressionValid = FALSE;
}

/**
 * @brief  设置条件域的表达式（动态分配内存）
 * @param  domainId    域ID (0-31)
 * @param  condExpr    条件表达式字符串
 * @param  triggerExpr 触发表达式字符串
 * @retval TRUE: 成功, FALSE: 失败
 */
uint8_t Condition_SetExpression(uint8_t domainId, const char *condExpr, const char *triggerExpr)
{
    if (domainId >= CONDITION_DOMAIN_COUNT || condExpr == NULL || triggerExpr == NULL)
    {
        return FALSE;
    }

    Condition_T *domain = &g_ConditionManager.domains[domainId];

    /* 释放旧的表达式内存 */
    Condition_FreeExpression(domainId);

    /* 计算字符串长度 */
    uint32_t condLen = String_GetLength(condExpr, EXPR_MAX_EXPRESSION_LENGTH);
    uint32_t triggerLen = String_GetLength(triggerExpr, EXPR_MAX_EXPRESSION_LENGTH);

    if (condLen >= EXPR_MAX_EXPRESSION_LENGTH || triggerLen >= EXPR_MAX_EXPRESSION_LENGTH)
    {
        return FALSE;
    }

    /* 动态分配内存 */
    domain->condExpr = (char *)Memory_Malloc(MP_RAM_EX, (condLen + 1) * sizeof(char));  // SDRAM
    domain->triggerExpr = (char *)Memory_Malloc(MP_RAM_EX, (triggerLen + 1) * sizeof(char));

    if (domain->condExpr == NULL || domain->triggerExpr == NULL)
    {
        /* 分配失败，清理资源 */
        Condition_FreeExpression(domainId);
        return FALSE;
    }

    /* 复制字符串 */
    Memory_Copy(domain->condExpr, condExpr, condLen);
    domain->condExpr[condLen] = '\0';
    domain->condExprLen = condLen;

    Memory_Copy(domain->triggerExpr, triggerExpr, triggerLen);
    domain->triggerExpr[triggerLen] = '\0';
    domain->triggerExprLen = triggerLen;

    /* 验证表达式语法 */
    Value_T testResult;
    char errorMsg[EXPR_MAX_ERROR_MSG_LENGTH];
    uint8_t hasError = Expr_Calculate(condExpr, &testResult, errorMsg, sizeof(errorMsg));

    domain->expressionValid = !hasError;
    return domain->expressionValid;
}

/**
 * @brief  配置条件域
 * @param  domainId    域ID (0-31)
 * @param  condExpr    条件表达式字符串
 * @param  triggerExpr 触发表达式字符串
 * @param  durationMs  持续时间(ms)，0xFFFFFFFF表示永久
 * @retval TRUE: 成功, FALSE: 失败
 */
uint8_t Condition_Configure(uint8_t domainId, const char *condExpr, const char *triggerExpr, uint32_t durationMs)
{
    if (!Condition_SetExpression(domainId, condExpr, triggerExpr))
    {
        return FALSE;
    }

    Condition_T *domain = &g_ConditionManager.domains[domainId];
    domain->durationMs = durationMs;

    /* 设置为命令开始时的位置坐标 */
    domain->triggerTick = HAL_GetTick();
    domain->triggerX = g_IMU.LocationX;
    domain->triggerY = g_IMU.LocationY;
    domain->triggerYaw = g_IMU.OdomAngleZ;

    Condition_Enable(domainId);

    return domain->expressionValid;
}

/**
 * @brief  启用条件域
 * @param  domainId 域ID (0-31)
 * @retval TRUE: 成功, FALSE: 失败
 */
uint8_t Condition_Enable(uint8_t domainId)
{
    if (domainId >= CONDITION_DOMAIN_COUNT)
    {
        return FALSE;
    }

    Condition_T *domain = &g_ConditionManager.domains[domainId];

    /* 只有表达式有效才能启用 */
    if (!domain->expressionValid)
    {
        return FALSE;
    }

    domain->state = CONDITION_DOMAIN_STATE_ENABLED;
    domain->triggerCount = 0;

    return TRUE;
}

/**
 * @brief  禁用条件域
 * @param  domainId 域ID (0-31)
 * @retval TRUE: 成功, FALSE: 失败
 */
uint8_t Condition_Disable(uint8_t domainId)
{
    if (domainId >= CONDITION_DOMAIN_COUNT)
    {
        return FALSE;
    }

    g_ConditionManager.domains[domainId].state = CONDITION_DOMAIN_STATE_DISABLED;
    Condition_FreeExpression(domainId);

    return TRUE;
}

/**
 * @brief  重置条件域状态
 * @param  domainId 域ID (0-31)
 * @retval TRUE: 成功, FALSE: 失败
 */
uint8_t Condition_Reset(uint8_t domainId)
{
    if (domainId >= CONDITION_DOMAIN_COUNT)
    {
        return FALSE;
    }

    /* 获取条件域 */
    Condition_T *domain = &g_ConditionManager.domains[domainId];

    /* 如果原来是启用状态，保持启用但重置为未触发 */
    if (domain->state == CONDITION_DOMAIN_STATE_TRIGGERED || domain->state == CONDITION_DOMAIN_STATE_EXPIRED)
    {
        domain->state = CONDITION_DOMAIN_STATE_ENABLED;
    }

    return TRUE;
}

/**
 * @brief  更新条件域的内部变量值
 * @param  domainId 域ID (0-31)
 * @retval TRUE: 成功, FALSE: 失败
 */
uint8_t Condition_UpdateVariables(uint8_t domainId)
{
    if (domainId >= CONDITION_DOMAIN_COUNT)
    {
        return FALSE;
    }

    Condition_T *domain = &g_ConditionManager.domains[domainId];

    /* 计算时间变化量 */
    uint32_t currentTick = HAL_GetTick();
    if (domain->state == CONDITION_DOMAIN_STATE_TRIGGERED)
    {
        domain->deltaTime = (float)Timer_GetDiffMs(domain->triggerTick) / 1000.0f; /* 转换为秒 */
    }
    else
    {
        domain->deltaTime = 0.0f;
    }

    /* 计算世界坐标系下的位移变化量 */
    float worldDeltaX = g_IMU.LocationX - domain->triggerX;
    float worldDeltaY = g_IMU.LocationY - domain->triggerY;

    /* 计算姿态变化量 */
    float worldDeltaYaw = g_IMU.OdomAngleZ - domain->triggerYaw;

    /* 归一化角度变化量到[-180, 180]范围 */
    worldDeltaYaw = fmodf(worldDeltaYaw, 360);
    if (worldDeltaYaw > 180) { worldDeltaYaw -= 360; }
    if (worldDeltaYaw < -180) { worldDeltaYaw += 360; }

    /* ========== 世界坐标系到局部坐标系的转换 ========== */

    /* 计算旋转矩阵（世界坐标系到局部坐标系的逆变换） */
    float cosYaw = Math_Cos(domain->triggerYaw);
    float sinYaw = Math_Sin(domain->triggerYaw);

    /* 应用二维刚性变换的逆变换：局部坐标 = R^T * (世界坐标 - 触发位置) */
    /* 其中 R = [cosθ  -sinθ]  是旋转矩阵，R^T = [cosθ   sinθ]
                [sinθ   cosθ]                   [-sinθ  cosθ] */

    domain->deltaX = worldDeltaX * cosYaw + worldDeltaY * sinYaw;  /* 局部坐标系X方向位移 */
    domain->deltaY = -worldDeltaX * sinYaw + worldDeltaY * cosYaw; /* 局部坐标系Y方向位移 */

    /* 偏航角变化在局部坐标系和世界坐标系下相同（绕Z轴旋转） */
    domain->deltaYaw = worldDeltaYaw;

    /* ========== 坐标系转换结束 ========== */

    /* 设置表达式变量值 */
    Expr_SetVariable("dx", domain->deltaX);
    Expr_SetVariable("dy", domain->deltaY);
    Expr_SetVariable("dw", domain->deltaYaw);
    Expr_SetVariable("dt", domain->deltaTime);

    domain->lastUpdateTick = currentTick;
    return TRUE;
}

/**
 * @brief  评估条件域表达式
 * @param  domainId 域ID (0-31)
 * @param  result   评估结果输出
 * @retval TRUE: 评估成功, FALSE: 失败
 */
uint8_t Condition_Evaluate(uint8_t domainId, uint8_t *result)
{
    if (domainId >= CONDITION_DOMAIN_COUNT || result == NULL)
    {
        return FALSE;
    }

    Condition_T *domain = &g_ConditionManager.domains[domainId];

    /* 只有启用的域才进行评估 */
    if (domain->state != CONDITION_DOMAIN_STATE_ENABLED && domain->state != CONDITION_DOMAIN_STATE_TRIGGERED)
    {
        return FALSE;
    }

    /* 更新变量值 */
    if (!Condition_UpdateVariables(domainId))
    {
        return FALSE;
    }

    /* 检查表达式指针是否有效 */
    if (domain->condExpr == NULL || !domain->expressionValid)
    {
        return FALSE;
    }

    /* 评估表达式 */
    Value_T evalResult;
    char errorMsg[EXPR_MAX_ERROR_MSG_LENGTH];
    uint8_t hasError = Expr_Calculate(domain->condExpr, &evalResult, errorMsg, sizeof(errorMsg));

    /* 表达式评估错误，禁用该域 */
    if (hasError)
    {
        domain->state = CONDITION_DOMAIN_STATE_DISABLED;
        return FALSE;
    }

    /* 转换为布尔值 */
    if (evalResult.type == EXPR_VALUE_TYPE_BOOL)
    {
        *result = evalResult.boolValue;
    }
    else if (evalResult.type == EXPR_VALUE_TYPE_NUMBER)
    {
        *result = fabsf(evalResult.numberValue) > 1e-6f;
    }
    else
    {
        *result = FALSE;
    }

    return TRUE;
}

/**
 * @brief  获取条件域状态
 * @param  domainId 域ID (0-31)
 * @param  state    状态输出
 * @retval TRUE: 成功, FALSE: 失败
 */
uint8_t Condition_GetState(uint8_t domainId, ConditionState_T *state)
{
    if (domainId >= CONDITION_DOMAIN_COUNT || state == NULL)
    {
        return FALSE;
    }

    *state = g_ConditionManager.domains[domainId].state;
    return TRUE;
}

/**
 * @brief  获取条件域触发信息
 * @param  domainId  域ID (0-31)
 * @param  deltaX    X变化量输出
 * @param  deltaY    Y变化量输出
 * @param  deltaZ    Z变化量输出
 * @param  deltaYaw  偏航角变化量输出
 * @param  deltaTime 时间变化量输出
 * @retval TRUE: 成功, FALSE: 失败
 */
uint8_t Condition_GetTriggerInfo(uint8_t domainId, float *deltaX, float *deltaY, float *deltaZ, float *deltaYaw, float *deltaTime)
{
    if (domainId >= CONDITION_DOMAIN_COUNT)
    {
        return FALSE;
    }

    Condition_T *domain = &g_ConditionManager.domains[domainId];

    if (deltaX) *deltaX = domain->deltaX;
    if (deltaY) *deltaY = domain->deltaY;
    if (deltaYaw) *deltaYaw = domain->deltaYaw;
    if (deltaTime) *deltaTime = domain->deltaTime;

    return TRUE;
}

/**
 * @brief  检查条件域是否已触发
 * @param  domainId 域ID (0-31)
 * @retval TRUE: 已触发, FALSE: 未触发或无效
 */
uint8_t Condition_IsTriggered(uint8_t domainId)
{
    if (domainId >= CONDITION_DOMAIN_COUNT)
    {
        return FALSE;
    }

    Condition_T *domain = &g_ConditionManager.domains[domainId];
    return (domain->state == CONDITION_DOMAIN_STATE_TRIGGERED);
}

/**
 * @brief  获取条件域表达式
 * @param  domainId   域ID (0-31)
 * @param  buffer     输出缓冲区
 * @param  bufferSize 缓冲区大小
 * @retval TRUE: 成功, FALSE: 失败
 */
uint8_t Condition_GetExpression(uint8_t domainId, char *buffer, uint32_t bufferSize)
{
    if (domainId >= CONDITION_DOMAIN_COUNT || buffer == NULL || bufferSize == 0)
    {
        return FALSE;
    }

    Condition_T *domain = &g_ConditionManager.domains[domainId];
    uint32_t exprLen = String_GetLength(domain->condExpr, EXPR_MAX_EXPRESSION_LENGTH);

    if (exprLen >= bufferSize)
    {
        return FALSE;
    }

    Memory_Copy(buffer, domain->condExpr, exprLen);
    buffer[exprLen] = '\0';
    return TRUE;
}

/**
 * @brief  更新所有条件域状态
 * @note   在主循环中调用，用于定期评估所有条件域
 */
void Condition_UpdateAllDomains(void)
{
    uint32_t currentTick = HAL_GetTick();

    Expr_SetVariable("wx", g_IMU.LocationX);
    Expr_SetVariable("wy", g_IMU.LocationY);
    Expr_SetVariable("sx", g_IMU.OdomSpeedX);
    Expr_SetVariable("sy", g_IMU.OdomSpeedY);
    Expr_SetVariable("sw", g_Car.remote.actualSpeedT);
    Expr_SetVariable("pitch", g_IMU.Pitch);
    Expr_SetVariable("roll", g_IMU.Roll);
    Expr_SetVariable("yaw", g_IMU.Yaw);
    Expr_SetVariable("dt", Car_GetRunTime());

    for (uint8_t i = 0; i < CONDITION_DOMAIN_COUNT; i++)
    {
        Condition_T *domain = &g_ConditionManager.domains[i];

        /* 跳过禁用状态和过期状态的域 */
        if (domain->state == CONDITION_DOMAIN_STATE_DISABLED)
        {
            continue;
        }

        /* 检查已触发域是否过期 */
        if (domain->state == CONDITION_DOMAIN_STATE_TRIGGERED)
        {
            if (domain->durationMs != CONDITION_DOMAIN_INFINITE)
            {
                if (Timer_GetDiffMs(domain->triggerTick) >= domain->durationMs)
                {
                    domain->state = CONDITION_DOMAIN_STATE_EXPIRED;
                    continue;
                }
            }
        }

        /* 评估条件表达式 */
        uint8_t evalResult;
        if (Condition_Evaluate(i, &evalResult))
        {
            if (evalResult)
            {
                /* 条件为真 */
                if (domain->state == CONDITION_DOMAIN_STATE_ENABLED)
                {
                    /* 首次触发，记录触发时刻信息 */
                    domain->triggerCount++;
                    domain->state = CONDITION_DOMAIN_STATE_TRIGGERED;
                }
                /* 如果已经是触发状态，保持状态不变 */
            }
            else
            {
                /* 条件为假 */
                if (domain->state == CONDITION_DOMAIN_STATE_TRIGGERED)
                {
                    /* 从触发状态回到启用状态 */
                    domain->state = CONDITION_DOMAIN_STATE_ENABLED;
                }
            }
        }
    }
}

/**
 * @brief  计算并执行条件域的触发表达式
 * @param  domainId 域ID (0-31)
 * @retval TRUE: 成功, FALSE: 失败
 */
uint8_t Condition_CalcTriggerExpr(uint8_t domainId)
{
    /* 判断ID数值的合法性 */
    if (domainId >= CONDITION_DOMAIN_COUNT)
    {
        return FALSE;
    }

    /* 更新变量值 */
    if (!Condition_UpdateVariables(domainId)) { return FALSE; }
    Expr_SetVariable("max_x", 0);
    Expr_SetVariable("max_w", 0);
    Expr_SetVariable("min_x", 0);
    Expr_SetVariable("min_w", 0);
    Expr_SetVariable("t_dyaw", 0);
    Expr_SetVariable("stop_flag", 0);
    Expr_SetVariable("beep_flag", 0);

    /* 计算表达式 */
    Condition_T *domain = &g_ConditionManager.domains[domainId];
    Value_T evalResult;
    char errorMsg[EXPR_MAX_ERROR_MSG_LENGTH];
    uint8_t hasError = Expr_Calculate(domain->triggerExpr, &evalResult, errorMsg, sizeof(errorMsg));

    /* 表达式评估错误，禁用该域 */
    if (hasError)
    {
        domain->state = CONDITION_DOMAIN_STATE_DISABLED;
        return FALSE;
    }

    float tempValue = 0;
    if (Expr_GetVariable("max_x", &tempValue) && fabsf(tempValue) > EXPR_FLOAT_EPSILON)
    {
        if (tempValue < g_Car.remote.remoteMaxSpeedX) { g_Car.remote.remoteMaxSpeedX = tempValue; }
    }
    if (Expr_GetVariable("max_w", &tempValue) && fabsf(tempValue) > EXPR_FLOAT_EPSILON)
    {
        if (tempValue < g_Car.remote.remoteMaxSpeedT) { g_Car.remote.remoteMaxSpeedT = tempValue; }
    }
    if (Expr_GetVariable("min_x", &tempValue) && fabsf(tempValue) > EXPR_FLOAT_EPSILON)
    {
        if (tempValue > g_Car.avoidance.minSpeedX) { g_Car.avoidance.minSpeedX = tempValue; }
    }
    if (Expr_GetVariable("min_w", &tempValue) && fabsf(tempValue) > EXPR_FLOAT_EPSILON)
    {
        if (tempValue > g_Car.avoidance.minSpeedT) { g_Car.avoidance.minSpeedT = tempValue; }
    }
    if (Expr_GetVariable("t_dyaw", &tempValue) && fabsf(tempValue) > EXPR_FLOAT_EPSILON) { Car_Turn(tempValue); }
    if (Expr_GetVariable("stop_flag", &tempValue) && fabsf(tempValue) > EXPR_FLOAT_EPSILON) { g_Car.remote.remoteMaxSpeedX = g_Car.remote.remoteMaxSpeedT = 0; }
    if (Expr_GetVariable("beep_flag", &tempValue) && fabsf(tempValue) > EXPR_FLOAT_EPSILON) { Beep_Play(100, 300, 5); }

    return TRUE;
}

/**
 * @brief  打印条件域状态信息到缓冲区
 * @param  buffer     输出缓冲区
 * @param  bufferSize 缓冲区大小
 */
void Condition_PrintStatus(char *buffer, uint32_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0)
    {
        return;
    }

    uint32_t offset = 0;

    offset += snprintf(buffer + offset, bufferSize - offset,
                       "=== 条件域状态 ===\n");

    for (uint8_t i = 0; i < CONDITION_DOMAIN_COUNT && offset < bufferSize - 1; i++)
    {
        Condition_T *domain = &g_ConditionManager.domains[i];

        if (domain->state != CONDITION_DOMAIN_STATE_DISABLED)
        {
            const char *stateStr = "未知";
            switch (domain->state)
            {
            case CONDITION_DOMAIN_STATE_DISABLED: stateStr = "禁用"; break;
            case CONDITION_DOMAIN_STATE_ENABLED: stateStr = "启用"; break;
            case CONDITION_DOMAIN_STATE_TRIGGERED: stateStr = "已触发"; break;
            case CONDITION_DOMAIN_STATE_EXPIRED: stateStr = "已过期"; break;
            default: stateStr = "未知"; break;
            }

            /* 安全处理可能为NULL的表达式字符串 */
            const char *condExprStr = (domain->condExpr != NULL) ? domain->condExpr : "(null)";
            const char *triggerExprStr = (domain->triggerExpr != NULL) ? domain->triggerExpr : "(null)";

            offset += snprintf(buffer + offset, bufferSize - offset,
                               "域%2d: %s 条件: %s 触发: %s\n",
                               i, stateStr, condExprStr, triggerExprStr);

            if (domain->state == CONDITION_DOMAIN_STATE_TRIGGERED)
            {
                offset += snprintf(buffer + offset, bufferSize - offset,
                                   "      触发次数: %u, dx=%.3f, dy=%.3f, dt=%.1fs\n",
                                   domain->triggerCount, domain->deltaX,
                                   domain->deltaY, domain->deltaTime);
            }

            /* 添加空行分隔 */
            if (offset < bufferSize - 2)
            {
                offset += snprintf(buffer + offset, bufferSize - offset, "\n");
            }
        }
    }

    /* 添加总结信息 */
    if (offset < bufferSize - 50)
    {
        uint32_t enabledCount = 0, triggeredCount = 0;
        for (uint8_t i = 0; i < CONDITION_DOMAIN_COUNT; i++)
        {
            if (g_ConditionManager.domains[i].state == CONDITION_DOMAIN_STATE_ENABLED) enabledCount++;
            if (g_ConditionManager.domains[i].state == CONDITION_DOMAIN_STATE_TRIGGERED) triggeredCount++;
        }
        offset += snprintf(buffer + offset, bufferSize - offset,
                           "总计: %d启用, %d触发, %d禁用\n",
                           enabledCount, triggeredCount,
                           CONDITION_DOMAIN_COUNT - enabledCount - triggeredCount);
    }

    /* 确保字符串以null结尾 */
    if (offset >= bufferSize)
    {
        buffer[bufferSize - 1] = '\0';
    }
    else
    {
        buffer[offset] = '\0';
    }
}

/**
 * @brief  条件域任务，根据触发的条件域执行最大/最小速度限制等操作
 * @param  pvParameters 任务参数（未使用）
 */
TaskRet_T Condition_Task(TaskParam_T *param)
{
    static uint32_t triggerTick = 0;
    static uint8_t speedX_SetMaxFlag = FALSE, speedX_SetMinFlag = FALSE, speedT_SetMaxFlag = FALSE, speedT_SetMinFlag = FALSE;

    speedX_SetMaxFlag = FALSE;
    speedX_SetMinFlag = FALSE;
    speedT_SetMaxFlag = FALSE;
    speedT_SetMinFlag = FALSE;
    Condition_UpdateAllDomains();

    /* 查找已经触发的条件域信息 */
    for (uint8_t i = 0; i < CONDITION_DOMAIN_COUNT; i++)
    {
        Condition_T *domain = &g_ConditionManager.domains[i];

        /* 评估条件表达式 */
        if (domain->state == CONDITION_DOMAIN_STATE_TRIGGERED)
        {
            if (g_Car.state == CAR_MODE_REMOTE && !g_ConditionBeepFlag && !g_ConditionCancelFlag)
            {
                g_ConditionBeepFlag = TRUE;
                triggerTick = HAL_GetTick();
                Beep_Play(100, 200, 5);
            }
            if (g_Car.state == CAR_MODE_AVOIDANCE)
            {
                Condition_CalcTriggerExpr(i);

                float tempValue = 0;
                if (Expr_GetVariable("max_x", &tempValue) && fabsf(tempValue) > EXPR_FLOAT_EPSILON)
                {
                    if (tempValue < g_Car.remote.remoteMaxSpeedX)
                    {
                        g_Car.remote.remoteMaxSpeedX = tempValue;
                    }
                    speedX_SetMaxFlag = TRUE;
                }
                if (Expr_GetVariable("max_w", &tempValue) && fabsf(tempValue) > EXPR_FLOAT_EPSILON)
                {
                    if (tempValue < g_Car.remote.remoteMaxSpeedT)
                    {
                        g_Car.remote.remoteMaxSpeedT = tempValue;
                    }
                    speedT_SetMaxFlag = TRUE;
                }
                if (Expr_GetVariable("min_x", &tempValue) && fabsf(tempValue) > EXPR_FLOAT_EPSILON)
                {
                    if (tempValue > g_Car.avoidance.minSpeedX)
                    {
                        g_Car.avoidance.minSpeedX = tempValue;
                    }
                    speedX_SetMinFlag = TRUE;
                }
                if (Expr_GetVariable("min_w", &tempValue) && fabsf(tempValue) > EXPR_FLOAT_EPSILON)
                {
                    if (tempValue > g_Car.avoidance.minSpeedT)
                    {
                        g_Car.avoidance.minSpeedT = tempValue;
                    }
                    speedT_SetMinFlag = TRUE;
                }
                if (Expr_GetVariable("stop_flag", &tempValue) && fabsf(tempValue) > EXPR_FLOAT_EPSILON)
                {
                    if (tempValue > g_Car.avoidance.minSpeedT)
                    {
                        g_Car.remote.remoteMaxSpeedX = 0;
                        g_Car.remote.remoteMaxSpeedT = 0;
                        g_Car.avoidance.minSpeedX = 0;
                        g_Car.avoidance.minSpeedT = 0;
                    }
                    speedX_SetMaxFlag = TRUE;
                    speedT_SetMaxFlag = TRUE;
                }
            }
        }
    }

    /* 如果小车没有被设置了最大/最小速度，则使用默认速度 */
    if (!speedX_SetMaxFlag) { g_Car.remote.remoteMaxSpeedX = g_ParamData.car_MaxSpeedX; }
    if (!speedT_SetMaxFlag) { g_Car.remote.remoteMaxSpeedT = g_ParamData.car_MaxSpeedT; }
    if (!speedX_SetMinFlag) { g_Car.avoidance.minSpeedX = 0; }
    if (!speedT_SetMinFlag) { g_Car.avoidance.minSpeedT = 0; }

    /* 取消条件域时，取消蜂鸣器蜂鸣 */
    if (g_Car.state == CAR_MODE_REMOTE && g_ConditionCancelFlag)
    {
        g_ConditionBeepFlag = FALSE;
        Beep_Stop();
    }

    /* 定时取消蜂鸣数值 */
    if (g_ConditionBeepFlag && Timer_GetDiffMs(triggerTick) > 1500)
    {
        g_ConditionBeepFlag = FALSE;
        Beep_Stop();
    }

    return Task_Delay(100);
}
