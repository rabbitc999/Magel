#include "chassis_ekf.h"
#include "car.h"
#include "bsp_system.h"
#include "fast_math.h"
#include "imu.h"
#include "store.h"

#define CAR_LIMIT(x, min, max) (((x) > (max)) ? (max) : (((x) < (min)) ? (min) : (x)))
#define CAR_OUTPUT_OFFSET(v)                                                \
    do                                                                      \
    {                                                                       \
        if (v > 0) { v += pencent; }                                        \
        if (v < 0) { v -= pencent; }                                        \
        if (v < -remote->remoteMaxSpeedX) { v = -remote->remoteMaxSpeedX; } \
        if (v > remote->remoteMaxSpeedX) { v = remote->remoteMaxSpeedX; }   \
    } while (0)
#define CAR_SGN(x) ((x) > CAR_CTRL_MIN_EPS ? (1) : ((x) < CAR_CTRL_MIN_EPS ? (-1) : (0)))

/* 小车运动控制相关参数 */
Car_T g_Car = { 0 };

extern uint8_t g_LidarStart;
extern LidarDataList_T g_LidarDataList;
extern volatile float g_FrontUltraSonicDistance;  // 假设前方超声波距离全局变量
extern uint8_t g_SleepFlag;

/**
 * @brief  使能小车电机
 */
void Car_MotorEnable(void)
{
    Motor_Enable(MOTOR_A);
    Motor_Enable(MOTOR_B);
    Motor_Enable(MOTOR_C);
    Motor_Enable(MOTOR_D);
}

/**
 * @brief  禁止小车电机
 */
void Car_MotorDisable(void)
{
    Motor_Disable(MOTOR_A);
    Motor_Disable(MOTOR_B);
    Motor_Disable(MOTOR_C);
    Motor_Disable(MOTOR_D);
}

/**
 * @brief  初始化小车运动控制相关的硬件和状态
 */
void Car_Init(void)
{
    /* 设置停止状态 */
    g_Car.state = CAR_MODE_STOP;

    /* 初始化小车参数 */
    g_Car.readMotorEncoderFlag = 0;
    g_Car.remote.useIntervalControl = 1;
}

/**
 * @brief  停止小车运动
 */
void Car_Stop(void)
{
    /* 停止状态直接切换 */
    g_Car.state = CAR_MODE_STOP;

    float deltaTime = Timer_GetDiffUs(g_Car.remote.lastUpdateTick) / 1e6f;
    g_Car.remote.lastUpdateTick = Timer_GetTicks();

    /* 限制更新频率 */
    if (deltaTime > 1.0f || deltaTime < CAR_CTRL_DELTA_TICK / 2000.0f)
    {
        return;
    }

    /* 清除PID控制器 */
    int32_t ekfCounts[CAR_MAX_INDEX];
    for (uint8_t i = 0; i < CAR_MAX_INDEX; i++)
    {
        ekfCounts[i] = CarControl_EncoderGetCount(&g_Car.remote, i);
        if (g_Car.readMotorEncoderFlag)
        {
            Motor_SetMode(g_ParamData.motor_controlMode);
            Car_MotorEnable();
            g_Car.stopMotorEncoder[i] = ekfCounts[i];
            CarControl_MotorSetPWM(&g_Car.remote, i, g_Car.stopMotorSpeed[i]);
        }
        else
        {
            /* 设置为无速度状态 */
            for (uint8_t i = 0; i < CAR_MAX_INDEX; i++)
            {
                Motor_SetPWM(i, 0);
            }
            Car_MotorDisable();
        }
    }
    /* Continue measured odometry while stopped, including coasting/pushing. */
    {
        float diameter = g_Car.remote.wheelDiameter / 100.0f;
        float track = g_Car.remote.wheelBase / 100.0f;
        float speed = NAN, rate = NAN;
        int supported = g_ParamData.car_chassisType == CHASSIS_TYPE_DIFFERENTIAL_2 || g_ParamData.car_chassisType == CHASSIS_TYPE_DIFFERENTIAL_4;
        if (supported && diameter > 0 && track > 0 && g_Car.remote.encoderPerRev > 0)
        {
            float scale = M_PI * diameter / (g_Car.remote.encoderPerRev * deltaTime);
            float left = ekfCounts[0] * scale, right = ekfCounts[1] * scale;
            if (g_ParamData.car_chassisType == CHASSIS_TYPE_DIFFERENTIAL_4)
            {
                left = (left + ekfCounts[2] * scale) * 0.5f;
                right = (right + ekfCounts[3] * scale) * 0.5f;
            }
            speed = (left + right) * 0.5f;
            rate = RAD2DEG((right - left) / track);
        }
        ChassisEKF_RuntimeEncoder(speed, rate, g_Car.remote.lastUpdateTick, supported);
    }
}

/**
 * @brief  设置小车的运行状态
 * @param  newState 新的运行状态
 * @note   状态切换请求会被标记，实际切换将在主循环中处理
 */
void Car_SetMode(CarMode_T newState)
{
    g_Car.remote.targetSpeedX = 0;
    g_Car.remote.targetSpeedT = 0;

    /* 模式相同，则忽略切换模式 */
    if (newState == g_Car.state) { return; }

    /* 取消内联模式 */
    g_Car.remote.useIntervalControl = 1;
    g_LidarStart = 1;

    /* 停止模式切换需要初始化遥控信息 */
    if (g_Car.state == CAR_MODE_STOP)
    {
        IMU_ResetOrientation();
        CarControl_Init(&g_Car.remote);
        g_Car.startTick = HAL_GetTick();
    }

    /* 以下为请求响应的动作 */
    if (newState == CAR_MODE_REMOTE)
    {
        g_Car.state = CAR_MODE_REMOTE;
        g_Car.remote.remoteMaxSpeedX = g_ParamData.car_MaxSpeedX;
        g_Car.remote.remoteMaxSpeedT = g_ParamData.car_MaxSpeedT;
    }
    else if (newState == CAR_MODE_AVOIDANCE)
    {
        g_Car.avoidance.baseRemote = &g_Car.remote;
        g_Car.state = CAR_MODE_AVOIDANCE;
        g_Car.remote.remoteMaxSpeedX = g_ParamData.car_MaxSpeedX;
        g_Car.remote.remoteMaxSpeedT = g_ParamData.car_MaxSpeedT;
    }
    else
    {
        /* 清除速度设置 */
        for (uint8_t i = 0; i < CAR_MAX_INDEX; i++)
        {
            g_Car.stopMotorSpeed[i] = 0;
        }

        /* 熄灭灯光 */
        WS2812_SetAll(0x000000);
        WS2812_Update();

        /* 设置为停止模式 */
        g_Car.state = CAR_MODE_STOP;
        g_Car.startTick = HAL_GetTick();

        /* 重置姿态 */
        IMU_ResetOrientation();
    }
}

/**
 * @brief  设置小车速度
 * @param  ctrlSpeedX X方向速度
 * @param  ctrlSpeedT 转向方向速度
 * @note   该函数用于设置避障模式中的目标引力方向
 */
void Car_SetSpeed(float ctrlSpeedX, float ctrlSpeedT)
{
    /* 设置避障模块的目标方向 */
    if (g_Car.state == CAR_MODE_REMOTE)
    {
        CarControl_SetSpeed(&g_Car.remote, ctrlSpeedX, ctrlSpeedT);
    }
    else
    {
        g_Car.avoidance.ctrlSpeedX = ctrlSpeedX;
        g_Car.avoidance.ctrlSpeedT = ctrlSpeedT;
    }
}

/**
 * @brief  设置小车转向增量
 * @param  deltaYaw 偏航角增量
 * @note   该函数用于设置避障模式中的目标引力方向
 */
void Car_Turn(float deltaYaw)
{
    /* 设置避障模块的目标方向 */
    CarControl_SetTurn(&g_Car.remote, deltaYaw);
}

/**
 * @brief  设置小车目标方向
 * @param  directionYaw 目标方向偏航角
 * @note   该函数用于设置避障模式中的目标引力方向
 */
void Car_SetDirection(float directionYaw)
{
    /* 设置避障模块的目标方向 */
    CarControl_SetDirection(&g_Car.remote, directionYaw);
}

/**
 * @brief  停止避障模式
 * @note   将避障模式切换回遥控模式
 */
void Car_StopAvoidance(void)
{
    /* 小车状态切换回遥控模式 */
    g_Car.state = CAR_MODE_REMOTE;
}

/**
 * @brief  获取小车当前状态
 * @retval 当前状态
 */
CarMode_T Car_GetMode(void)
{
    return g_Car.state;
}

/**
 * @brief  获取小车运行时间
 * @retval 运行时间（秒）
 */
float Car_GetRunTime(void)
{
    return (float)Timer_GetDiffMs(g_Car.startTick) / 1000.0f;
}

enum
{
    LIGHT_STATE_OFF,
    LIGHT_STATE_ON
};
enum
{
    LIGHT_TYPE_OFF,
    LIGHT_TYPE_TURN,
    LIGHT_TYPE_BACK,
    LIGHT_TYPE_TURN_BACK = LIGHT_TYPE_TURN | LIGHT_TYPE_BACK
};

/**
 * @brief  灯光状态机
 */
void Car_LightStateMachine(void)
{
    static uint8_t state = LIGHT_STATE_OFF, timer = 0;
    const uint8_t updateTime = 20;  // 20ms * 20 = 400ms
    uint8_t type = LIGHT_TYPE_OFF;

    timer++;

    /* 转向灯判断 */
    if (fabsf(g_Car.remote.targetSpeedT) > 25)
    {
        type |= LIGHT_TYPE_TURN;
    }
    if (g_Car.remote.targetSpeedX <= -0.09)
    {
        type |= LIGHT_TYPE_BACK;
    }

    /* 转向灯状态机 */
    switch (state)
    {
    case LIGHT_STATE_OFF:
    {
        WS2812_SetAll(0x000000);
        WS2812_Update();
        if (timer >= updateTime && type != LIGHT_TYPE_OFF)
        {
            timer = 0;
            state = LIGHT_STATE_ON;
        }
        break;
    }
    case LIGHT_STATE_ON:
    {
        uint8_t lightId = g_Car.remote.targetSpeedT > 0 ? LIGHT_BACK_LEFT_ID : LIGHT_BACK_RIGHT_ID;
        if (type == LIGHT_TYPE_TURN)
        {
            WS2812_Set(1 - lightId, 0x000000);
            WS2812_Set(lightId, 0xFFFF00);
            WS2812_Update();
        }
        if (type == LIGHT_TYPE_BACK)
        {
            WS2812_SetAll(0xFF0000);
            WS2812_Update();
        }
        if (type == LIGHT_TYPE_TURN_BACK)
        {
            WS2812_Set(1 - lightId, 0xFF0000);
            WS2812_Set(lightId, 0xFFFF00);
            WS2812_Update();
        }
        if (timer >= updateTime)
        {
            timer = 0;
            state = LIGHT_STATE_OFF;
        }
        break;
    }
    }
}

/**
 * @brief  小车运动控制函数
 */
void Car_Tick(void)
{
    static uint8_t ledCount = 0;
    float measurement[6] = { 0 };

    /* 闪灯提示 */
    if (ledCount++ >= 100 && !g_SleepFlag)
    {
        LED2_Turn();
        ledCount = 0;
    }

    /* 更新位置和偏航角信息 */
    CarControl_UpdateLocation(&g_Car.remote);

    /* 根据不同模式进行更新 */
    switch (g_Car.state)
    {
    case CAR_MODE_REMOTE:
        CarControl_Update(&g_Car.remote);
        break;

    case CAR_MODE_AVOIDANCE:
        Avoidance_Tick(&g_Car.avoidance);
        CarControl_Update(&g_Car.remote);
        break;

    default:
        /* 未知状态/停止状态，切换到停止模式 */
        g_Car.state = CAR_MODE_STOP;
        Car_Stop();
        return;
    }

    /* 转向灯状态机 */
    Car_LightStateMachine();
}

/**
 * @brief  角度规范化到[-180, 180]范围
 * @param  angle 输入角度（度）
 * @retval 规范化后的角度
 */
static inline float NormalizeAngle(float angle)
{
    angle = fmodf(angle, 360.0f);
    return angle > 180.0f ? angle - 360.0f : (angle < -180.0f ? angle + 360.0f : angle);
}

/**
 * @brief  设置电机PWM输出
 * @param  remote 遥控控制结构体指针
 * @param  motorIndex 电机索引
 * @param  pwmValue PWM值
 */
void CarControl_MotorSetPWM(CarControl_T *remote, CarIndex_T motorIndex, float pwmValue)
{
    if (motorIndex >= CAR_MAX_INDEX) { return; }
    if (g_ParamData.car_motorMapper[motorIndex] == CAR_MAPPER_NONE) { return; }

    int8_t sign = (g_ParamData.car_motorMapper[motorIndex] % 2 == 1) ? -1 : 1;
    int8_t mapper = (g_ParamData.car_motorMapper[motorIndex] - 1) / 2;

    Motor_SetPWM(mapper, sign * pwmValue);
}

/**
 * @brief  获取编码器计数值
 * @param  remote 遥控控制结构体指针
 * @param  encoderIndex 编码器索引
 * @retval 编码器计数值
 */
int32_t CarControl_EncoderGetCount(CarControl_T *remote, CarIndex_T encoderIndex)
{
    if (encoderIndex >= CAR_MAX_INDEX) { return 0; }
    if (g_ParamData.car_encoderMapper[encoderIndex] == CAR_MAPPER_NONE) { return 0; }

    int8_t sign = (g_ParamData.car_encoderMapper[encoderIndex] % 2 == 1) ? -1 : 1;
    int8_t mapper = (g_ParamData.car_encoderMapper[encoderIndex] - 1) / 2;

    return sign * Encoder_GetCounter(mapper);
}

/**
 * @brief  设置舵机角度
 * @param  remote 遥控控制结构体指针
 * @param  servoIndex 舵机索引
 * @param  angle 角度值(度)
 */
void CarControl_ServoSetAngle(CarControl_T *remote, ServoSelect_T servoIndex, float angle)
{
    float actualAngle = angle;
    Servo_SetAngle(servoIndex, actualAngle);
}

/**
 * @brief  设置底盘类型
 * @param  remote 遥控控制结构体指针
 * @param  chassisType 底盘类型
 */
void CarControl_SetChassisType(CarControl_T *remote, ChassisType_T chassisType)
{
    g_ParamData.car_chassisType = chassisType;
}

/**
 * @brief  获取底盘类型
 * @param  remote 遥控控制结构体指针
 * @retval 底盘类型
 */
ChassisType_T CarControl_GetChassisType(CarControl_T *remote)
{
    return g_ParamData.car_chassisType;
}

/**
 * @brief  设置遥控模式速度
 * @param  remote 遥控控制结构体指针
 * @param  speedX X方向速度
 * @param  speedT 转向方向速度
 */
void CarControl_SetSpeed(CarControl_T *remote, float speedX, float speedT)
{
    remote->targetSpeedX = speedX;
    remote->targetSpeedT = speedT;
}

/**
 * @brief  设置遥控模式目标方向
 * @param  remote 遥控控制结构体指针
 * @param  directionYaw 目标方向偏航角
 */
void CarControl_SetDirection(CarControl_T *remote, float directionYaw)
{
    remote->pidYaw.Target = directionYaw;
    remote->pidYaw.Target = NormalizeAngle(remote->pidYaw.Target);
    remote->currentState = CAR_STATE_DIRECTION;
}

/**
 * @brief  设置遥控模式转向增量
 * @param  remote 遥控控制结构体指针
 * @param  deltaYaw 偏航角增量
 */
void CarControl_SetTurn(CarControl_T *remote, float deltaYaw)
{
    remote->pidYaw.Target += deltaYaw;
    remote->pidYaw.Target = NormalizeAngle(remote->pidYaw.Target);
    remote->currentState = CAR_STATE_DIRECTION;
}

/**
 * @brief  判断是否处于转向状态
 * @param  remote 遥控控制结构体指针
 * @retval 1-处于转向状态，0-非转向状态
 */
uint8_t CarControl_IsTurnState(CarControl_T *remote)
{
    return remote->currentState == CAR_STATE_DIRECTION;
}

/**
 * @brief  停止遥控控制
 * @param  remote 遥控控制结构体指针
 */
void CarControl_Stop(CarControl_T *remote)
{
    remote->targetSpeedX = 0;
    remote->targetSpeedT = 0;
}

/**
 * @brief  打滑度检测函数
 * @param  x 第一个输入值
 * @param  y 第二个输入值
 * @retval 打滑度值（0~1）
 */
static float SlipFunction(float x, float y)
{
#define MIN(a, b) ((a) < (b) ? (a) : (b))
    if (fabsf(x) < CAR_CTRL_MIN_EPS || fabsf(y) < CAR_CTRL_MIN_EPS) { return 0; }
    float delta2 = (x - y) * (x - y);
    float term1 = delta2 * x * x;
    float term2 = delta2 * y * y;
    float min_term = MIN(term1, term2);
    if (isnan(min_term)) { min_term = 1; }
    return MIN(min_term, 1);
#undef MIN
}

/**
 * @brief  打滑度计算函数
 * @param  remote 遥控控制结构体指针
 * @param  sT 时间系数
 * @param  vA1 第一轮实际速度
 * @param  vA2 第二轮实际速度
 * @param  vA3 第三轮实际速度
 * @param  vA4 第四轮实际速度
 * @param  vO1 第一轮输出速度
 * @param  vO2 第二轮输出速度
 * @param  vO3 第三轮输出速度
 * @param  vO4 第四轮输出速度
 */
static void SlipCalc(CarControl_T *remote, float sT, float vA1, float vA2, float vA3, float vA4, float vO1, float vO2, float vO3, float vO4)
{
#define MAX(a, b) ((a) > (b) ? (a) : (b))
    /* 静态遍历，存储上一次的数值 */
    static float last_vA1 = 0, last_vA2 = 0, last_vA3 = 0, last_vA4 = 0, last_vO1 = 0, last_vO2 = 0, last_vO3 = 0, last_vO4 = 0;

    /* 中间变量计算 */
    float alpha1 = SlipFunction(vA1 / vO1 * 100, last_vA1 / last_vO1 * 100);
    float alpha2 = SlipFunction(vA2 / vO1 * 100, last_vA2 / last_vO2 * 100);
    float alpha3 = SlipFunction(vA3 / vO1 * 100, last_vA3 / last_vO3 * 100);
    float alpha4 = SlipFunction(vA4 / vO1 * 100, last_vA4 / last_vO4 * 100);
    float alpha12 = MAX(alpha1, alpha2);
    float alpha34 = MAX(alpha3, alpha4);
    float beta13 = SlipFunction(vA1, vA3);
    float beta24 = SlipFunction(vA2, vA4);

    /* 计算打滑度 */
    remote->slipAlpha = MAX(alpha12, alpha34);
    remote->slipBeta = MAX(beta13, beta24);
    remote->slipGamma = remote->slipK1 * remote->slipAlpha + remote->slipK2 * remote->slipBeta + remote->slipTheta;
    if (remote->slipGamma >= 1) { remote->slipGamma = 1.0f; }

    /* 更新上一次的变量 */
    last_vA1 = vA1;
    last_vA2 = vA2;
    last_vA3 = vA3;
    last_vA4 = vA4;
    last_vO1 = vO1;
    last_vO2 = vO2;
    last_vO3 = vO3;
    last_vO4 = vO4;
#undef MAX
}

/**
 * @brief  遥控控制更新函数
 * @param  remote 遥控控制结构体指针
 */
void CarControl_Update(CarControl_T *remote)
{
    /* 基本数据计算 */
    float deltaTime = Timer_GetDiffUs(remote->lastUpdateTick) / 1e6f;
    float scaleFactor = (float)remote->encoderPerRev / (M_PI * remote->wheelDiameter / 100.0f);
    float sT = scaleFactor * deltaTime;
    remote->lastUpdateTick = Timer_GetTicks();

    /* 限制更新频率 */
    if (deltaTime > 1.0f || deltaTime < 0.01f)
    {
        return;
    }

    /* 获取编码器实际值并计算各轮线速度 */
    remote->pidMotor[CAR_INDEX_M1].Actual = CarControl_EncoderGetCount(remote, CAR_INDEX_M1);
    remote->pidMotor[CAR_INDEX_M2].Actual = CarControl_EncoderGetCount(remote, CAR_INDEX_M2);
    remote->pidMotor[CAR_INDEX_M3].Actual = CarControl_EncoderGetCount(remote, CAR_INDEX_M3);
    remote->pidMotor[CAR_INDEX_M4].Actual = CarControl_EncoderGetCount(remote, CAR_INDEX_M4);

    float vA1 = remote->pidMotor[CAR_INDEX_M1].Actual / sT;
    float vA2 = remote->pidMotor[CAR_INDEX_M2].Actual / sT;
    float vA3 = remote->pidMotor[CAR_INDEX_M3].Actual / sT;
    float vA4 = remote->pidMotor[CAR_INDEX_M4].Actual / sT;

    /* 正运动学解算：轮速 -> 底盘速度 */
    float W = remote->wheelBase / 200.0f; /* 左右半轮距(m) */
    float actualX = 0, actualT = 0;

    if (g_ParamData.car_chassisType == CHASSIS_TYPE_DIFFERENTIAL_2)
    {
        /* 差速底盘正运动学 */
        actualX = (vA1 + vA2) / 2.0f;
        actualT = RAD2DEG((vA2 - vA1) / (2.0f * W));
    }
    if (g_ParamData.car_chassisType == CHASSIS_TYPE_DIFFERENTIAL_4)
    {
        /* 差速底盘正运动学 */
        actualX = (vA1 + vA2 + vA3 + vA4) / 4.0f;
        actualT = RAD2DEG((-vA1 + vA2 - vA3 + vA4) / (4.0f * W));
    }
    if (g_ParamData.car_chassisType == CHASSIS_TYPE_MECANUM)
    {
        /* 麦轮底盘正运动学 */
        actualX = (vA1 + vA2 + vA3 + vA4) / 4.0f;
        actualT = RAD2DEG((-vA1 + vA2 - vA3 + vA4) / (4.0f * W));
    }

    // TODO: 此处代码待修正
    if (g_ParamData.car_chassisType == CHASSIS_TYPE_OMNI_4)
    {
        /* 四轮全向轮底盘正运动学 */
        actualX = (vA1 + vA2 + vA3 + vA4) / 4.0f;
        actualT = RAD2DEG((-vA1 + vA2 - vA3 + vA4) / (4.0f * W));
    }
    if (g_ParamData.car_chassisType == CHASSIS_TYPE_OMNI_3)
    {
        /* 三轮全向轮底盘正运动学 */
        actualX = (vA1 + vA2 + vA3) / 3.0f;
        actualT = RAD2DEG((-vA1 + vA2 - vA3) / (3.0f * W));
    }
    if (g_ParamData.car_chassisType == CHASSIS_TYPE_ACKERMANN)
    {
        /* 阿克曼底盘正运动学 */
        actualX = (vA1 + vA2) / 2.0f;
        actualT = RAD2DEG((vA2 - vA1) / (remote->wheelBase / 100.0f));
    }

    /* 正运动学计算结果 */
    remote->actualSpeedX = actualX;
    remote->actualSpeedT = actualT;
    if (remote == &g_Car.remote)
    {
        ChassisEKF_RuntimeEncoder(actualX, actualT, remote->lastUpdateTick,
                                  g_ParamData.car_chassisType == CHASSIS_TYPE_DIFFERENTIAL_2 || g_ParamData.car_chassisType == CHASSIS_TYPE_DIFFERENTIAL_4);
    }

    /* 若使用角度外环PID，则使用角度环PID进行修正 */
    if (fabsf(remote->pidYaw.Kp) > CAR_CTRL_MIN_EPS)
    {
        /* 速度的累加操作 */
        remote->pidYaw.Actual = remote->carYaw;
        remote->pidYaw.Target += remote->targetSpeedT * deltaTime;
        remote->pidYaw.Target = NormalizeAngle(remote->pidYaw.Target);

        /* 运行外环PID计算 */
        float beta = 0.75f, lastOutputT = remote->pidYaw.Output;
        PID_Control(&remote->pidYaw, deltaTime);
        remote->outerSpeedT = beta * remote->pidYaw.Output + (1 - beta) * lastOutputT;

        /* 判断Yaw位置环是否完成控制 */
        if (fabsf(remote->pidYaw.Error) <= CAR_CTRL_END_YAW_ERROR) { remote->currentState = CAR_STATE_SPEED; }
        remote->outerSpeedX = remote->currentState == CAR_STATE_SPEED ? remote->targetSpeedX : 0;
    }
    else
    {
        remote->outerSpeedX = remote->targetSpeedX;
        remote->outerSpeedT = remote->targetSpeedT;
    }

    /* 差速底盘逆运动学 - 使用外环输出速度 */
    float vT1 = 0, vT2 = 0, vT3 = 0, vT4 = 0;
    vT1 = vT3 = remote->outerSpeedX - DEG2RAD(remote->outerSpeedT) * W;
    vT2 = vT4 = remote->outerSpeedX + DEG2RAD(remote->outerSpeedT) * W;

    /* 输入死区和输出偏移 */
    float pencent = remote->inputDeadZone / 10000.0f * remote->remoteMaxSpeedX;
    if (fabsf(vT1) <= pencent && fabsf(vT2) <= pencent && fabsf(vT3) <= pencent && fabsf(vT4) <= pencent
        && fabsf(remote->targetSpeedX) <= CAR_CTRL_MIN_EPS && fabsf(remote->targetSpeedT) <= CAR_CTRL_MIN_EPS
        && remote->currentState == CAR_STATE_SPEED)
    {
        vT1 = vT2 = vT3 = vT4 = 0;
    }
    else
    {
        CAR_OUTPUT_OFFSET(vT1);
        CAR_OUTPUT_OFFSET(vT2);
        CAR_OUTPUT_OFFSET(vT3);
        CAR_OUTPUT_OFFSET(vT4);
    }

    /* PID电机环 */
    remote->pidMotor[CAR_INDEX_M1].Target = vT1 * sT;
    remote->pidMotor[CAR_INDEX_M2].Target = vT2 * sT;
    remote->pidMotor[CAR_INDEX_M3].Target = vT3 * sT;
    remote->pidMotor[CAR_INDEX_M4].Target = vT4 * sT;
    PID_SetParam(&remote->pidMotor[CAR_INDEX_M1], &remote->pidMotorParam);
    PID_SetParam(&remote->pidMotor[CAR_INDEX_M2], &remote->pidMotorParam);
    PID_SetParam(&remote->pidMotor[CAR_INDEX_M3], &remote->pidMotorParam);
    PID_SetParam(&remote->pidMotor[CAR_INDEX_M4], &remote->pidMotorParam);
    float vO1 = PID_Control(&remote->pidMotor[CAR_INDEX_M1], deltaTime);
    float vO2 = PID_Control(&remote->pidMotor[CAR_INDEX_M2], deltaTime);
    float vO3 = PID_Control(&remote->pidMotor[CAR_INDEX_M3], deltaTime);
    float vO4 = PID_Control(&remote->pidMotor[CAR_INDEX_M4], deltaTime);

    /* 打滑检测计算 */
    SlipCalc(remote, sT, vA1, vA2, vA3, vA4, vO1, vO2, vO3, vO4);

    /* 使用PID控制器进行输出 */
    CarControl_MotorSetPWM(remote, CAR_INDEX_M1, vO1);
    CarControl_MotorSetPWM(remote, CAR_INDEX_M2, vO2);
    CarControl_MotorSetPWM(remote, CAR_INDEX_M3, vO3);
    CarControl_MotorSetPWM(remote, CAR_INDEX_M4, vO4);
}

/**
 * @brief  更新位置和偏航角信息
 * @param  remote 遥控控制结构体指针
 */
void CarControl_UpdateLocation(CarControl_T *remote)
{
    if (remote->useIntervalControl)
    {
        remote->carPitch = g_IMU.Pitch;
        remote->carRoll = g_IMU.Roll;
        remote->carYaw = g_IMU.Yaw - g_IMU.StartAngleZ;
        remote->carLocationX = g_IMU.LocationX;
        remote->carLocationY = g_IMU.LocationY;
    }
}

/**
 * @brief  初始化遥控控制结构体
 * @param  remote 遥控控制结构体指针
 */
void CarControl_Init(CarControl_T *remote)
{
    /* 初始化时间戳 */
    remote->lastUpdateTick = Timer_GetTicks();

    /* 初始化目标速度 */
    remote->actualSpeedX = remote->targetSpeedX = 0.0f;
    remote->actualSpeedT = remote->targetSpeedT = 0.0f;

    /* 清除PID控制器 */
    PID_Clear(&remote->pidYaw);
    PID_Clear(&remote->pidMotor[CAR_INDEX_M1]);
    PID_Clear(&remote->pidMotor[CAR_INDEX_M2]);
    PID_Clear(&remote->pidMotor[CAR_INDEX_M3]);
    PID_Clear(&remote->pidMotor[CAR_INDEX_M4]);

    /* 设置为速度模式 */
    remote->pidYaw.ZeroLimit = 180;
    remote->currentState = CAR_STATE_SPEED;
    Encoder_Clear();

    /* 设置初始偏航角 */
    g_IMU.StartAngleZ = g_IMU.Yaw;

    /* 设置小车电机控制模式 */
    Motor_SetMode(g_ParamData.motor_controlMode);

    /* 使能控制 */
    Motor_Enable(MOTOR_A);
    Motor_Enable(MOTOR_B);
    Motor_Enable(MOTOR_C);
    Motor_Enable(MOTOR_D);
}

/**
 * @brief  判断角度是否在前方避障范围内
 * @param  angle 角度（0-360度）
 * @param  frontAngleRange 前方角度范围（总宽度）
 * @retval 1表示在前方范围内，0表示不在
 */
static uint8_t IsAngleInFrontRange(float angle, float frontAngleRange)
{
    float halfRange = frontAngleRange;

    // 将角度规范化到[-180, 180]范围，0度为正前方
    float normalizedAngle = angle;
    if (normalizedAngle > 180.0f)
    {
        normalizedAngle -= 360.0f;  // 转换为负角度（左侧角度）
    }

    // 检查是否在[-halfRange, halfRange]范围内
    return (normalizedAngle >= -halfRange && normalizedAngle <= halfRange);
}

/**
 * @brief  避障模块初始化
 * @param  avoidance 避障模块结构体指针
 * @param  remote 遥控模块指针
 */
void Avoidance_Init(Avoidance_T *avoidance, CarControl_T *remote)
{
    avoidance->baseRemote = remote;
}

/**
 * @brief  处理后退时的偏航角修正
 * @param  avoidance 避障模块结构体指针
 */
static void HandleReverseYawCorrection(Avoidance_T *avoidance)
{
    // 检测速度方向变化
    if (avoidance->ctrlSpeedX < 0 && !avoidance->isReversed)
    {
        // 从前进切换到后退：偏航角+180度
        CarControl_SetTurn(avoidance->baseRemote, 180.0f);
        avoidance->isReversed = 1;
    }
    else if (avoidance->ctrlSpeedX > 0 && avoidance->isReversed)
    {
        // 从后退切换到前进：恢复原始偏航角
        CarControl_SetTurn(avoidance->baseRemote, 180.0f);
        avoidance->isReversed = 0;
    }
}

/**
 * @brief  超声波障碍物检测
 * @param  avoidance 避障模块结构体指针
 * @note   在避障中调用此函数，传入超声波测量值
 */
static void UltraSonicCheck(Avoidance_T *avoidance)
{
    // 仅在运行且距离小于阈值时触发避障
    if (fabsf(avoidance->ctrlSpeedX) > CAR_CTRL_MIN_EPS && g_FrontUltraSonicDistance < avoidance->ultraSonicThreshold)
    {
        // 超声波检测到低矮障碍物，强制触发避障程序
        avoidance->isUltraSonicTrigger = 1;
    }
    else
    {
        avoidance->isUltraSonicTrigger = 0;
    }
}

/**
 * @brief  避障模块主循环处理
 * @param  avoidance 避障模块结构体指针
 */
void Avoidance_Tick(Avoidance_T *avoidance)
{
    /* 初始化障碍物检测计数器 */
    uint16_t leftObstacleCount = 0;   // 左侧障碍物数量
    uint16_t rightObstacleCount = 0;  // 右侧障碍物数量
    uint8_t obstacleDetected = 0;     // 是否检测到障碍物

    /* 判断数据合法性 */
    if (avoidance->baseRemote == NULL) { return; }

    /* 如果小车正在转弯，则忽略处理 */
    if (CarControl_IsTurnState(avoidance->baseRemote)) { return; }

    /* 处理后退时的偏航角修正 */
    HandleReverseYawCorrection(avoidance);
    UltraSonicCheck(avoidance);

    /* 如果超声波被触发，则添加默认的障碍物，默认向左转 */
    if (avoidance->isUltraSonicTrigger)
    {
        obstacleDetected = 1;
    }

    /* 如果激光雷达数据无效，跳过处理 */
    else if (g_LidarDataList.count == 0 || g_LidarDataList.count > PACKET_LIDAR_MAX_SAMPLES)
    {
        avoidance->baseRemote->targetSpeedX = fabsf(avoidance->ctrlSpeedX);
        avoidance->baseRemote->targetSpeedT = avoidance->ctrlSpeedT;
        return;
    }

    /* 遍历所有激光雷达点 */
    for (uint16_t i = 0; i < g_LidarDataList.count; i++)
    {
        float angle = g_LidarDataList.buffer[i].angle;
        float distance = g_LidarDataList.buffer[i].distance;

        // 只处理前方指定角度范围内的点，并过滤无效距离（小于150mm的点认为是噪声）
        if (!IsAngleInFrontRange(angle, avoidance->frontAngleRange) || distance < 150.0f)
        {
            continue;  // 非前方范围的数据直接舍弃
        }

        // 检测障碍物：距离小于阈值
        if (distance < avoidance->obstacleDistanceThreshold)
        {
            obstacleDetected = 1;

            // 判断障碍物在左还是右
            // 根据LIDAR坐标系：角度在(0, 180)为右侧，(180, 360)为左侧
            if (angle > 0.0f && angle < 180.0f)
            {
                rightObstacleCount++;  // 右侧障碍物
            }
            else
            {
                leftObstacleCount++;  // 左侧障碍物
            }
        }
    }

    /* 根据检测结果控制小车运动 */
    if (obstacleDetected)
    {
        // 默认情况：左右都有障碍或只有中央障碍，优先向左转
        float turnDirection = 1.0f;

        // 如果有障碍，向障碍物少的一侧转
        if (leftObstacleCount > 0 || rightObstacleCount > 0)
        {
            turnDirection = (leftObstacleCount > rightObstacleCount) ? 1.0f : -1.0f;
        }

        // 设置转向速度
        const float beta = 0.1f; /* 变化率 */
        float nextSpeedT = turnDirection * avoidance->ctrlSpeedTGain * avoidance->baseRemote->remoteMaxSpeedT;
        avoidance->baseRemote->targetSpeedT = beta * nextSpeedT + (1 - beta) * avoidance->baseRemote->targetSpeedT;

        // 避障时不运动
        avoidance->baseRemote->targetSpeedX = 0;
    }
    else
    {
        // ---- 前方没有障碍物，正常前进/后退 ----
        avoidance->ctrlSpeedX = CAR_LIMIT(avoidance->ctrlSpeedX, -avoidance->baseRemote->remoteMaxSpeedX, avoidance->baseRemote->remoteMaxSpeedX);
        avoidance->ctrlSpeedT = CAR_LIMIT(avoidance->ctrlSpeedT, -avoidance->baseRemote->remoteMaxSpeedT, avoidance->baseRemote->remoteMaxSpeedT);
        avoidance->baseRemote->targetSpeedX = fabsf(avoidance->ctrlSpeedX) - avoidance->minSpeedX > CAR_CTRL_MIN_EPS ? fabsf(avoidance->ctrlSpeedX) : avoidance->minSpeedX;
        avoidance->baseRemote->targetSpeedT = fabsf(avoidance->ctrlSpeedT) - avoidance->minSpeedT > CAR_CTRL_MIN_EPS ? avoidance->ctrlSpeedT : CAR_SGN(avoidance->ctrlSpeedT) * avoidance->minSpeedT;
    }
}
