#ifndef __CAR_CTRL_H
#define __CAR_CTRL_H

#include "imu.h"
#include <stdint.h>

/* 遥控模式设置 */
#define CAR_CTRL_DELTA_TICK    20    /* 遥控控制间隔时间 (ms) */
#define CAR_CTRL_MIN_EPS       1e-3f /* 最小计算精度 */
#define CAR_CTRL_END_YAW_ERROR 5.0f  /* 偏航角控制结束角度 */

#ifdef __cplusplus
extern "C" {
#endif

/* 电机编号定义 */
typedef enum __CarIndex_T
{
    CAR_INDEX_M1 = 0, /* 电机1 */
    CAR_INDEX_M2,     /* 电机2 */
    CAR_INDEX_M3,     /* 电机3 */
    CAR_INDEX_M4,     /* 电机4 */
    CAR_MAX_INDEX
} CarIndex_T;

/* 电机编号定义 */
typedef enum __CarMapper_T
{
    CAR_MAPPER_NONE = 0, /* 保留位，不驱动任何电机 */
    CAR_MAPPER_MA_POS,   /* 电机/编码器1(正向) */
    CAR_MAPPER_MA_NEG,   /* 电机/编码器1(反向) */
    CAR_MAPPER_MB_POS,   /* 电机/编码器2(正向) */
    CAR_MAPPER_MB_NEG,   /* 电机/编码器2(反向) */
    CAR_MAPPER_MC_POS,   /* 电机/编码器3(正向) */
    CAR_MAPPER_MC_NEG,   /* 电机/编码器3(反向) */
    CAR_MAPPER_MD_POS,   /* 电机/编码器4(正向) */
    CAR_MAPPER_MD_NEG,   /* 电机/编码器4(反向) */
    CAR_MAX_MAPPER_INDEX
} CarMapper_T;

/* 底盘类型枚举 */
typedef enum __ChassisType_T
{
    CHASSIS_TYPE_DIFFERENTIAL_2, /* 两轮差速底盘 */
    CHASSIS_TYPE_DIFFERENTIAL_4, /* 四轮差速底盘 */
    CHASSIS_TYPE_MECANUM,        /* 麦轮底盘 */
    CHASSIS_TYPE_OMNI_4,         /* 四轮全向轮底盘 */
    CHASSIS_TYPE_OMNI_3,         /* 三轮全向轮底盘 */
    CHASSIS_TYPE_ACKERMANN,      /* 阿克曼底盘 */
    CHASSIS_TYPE_COUNT
} ChassisType_T;

/* 底盘类型枚举 */
typedef enum __CarState_T
{
    CAR_STATE_SPEED,     /* 速度控制状态 (可以控制小车的角速度和线速度) */
    CAR_STATE_DIRECTION, /* 偏航角状态 (只能自动控制小车方向，直到小车偏航角方向正确才能控制速度) */
} CarState_T;

/* 小车运动的状态 */
typedef enum __CarMode_T
{
    CAR_MODE_STOP = 0,      /* 停止模式 */
    CAR_MODE_REMOTE = 1,    /* 遥控模式 */
    CAR_MODE_AVOIDANCE = 2, /* 避障模式 */
    CAR_MODE_COUNT = 3,     /* 模式总数 */
} CarMode_T;

/* 小车控制器的结构体 */
typedef struct __CarControl_T
{
    /* PID控制器 */
    PID_T pidLocationX;            /* X方向位置控制器 */
    PID_T pidLocationY;            /* Y方向位置控制器 */
    PID_T pidMotor[CAR_MAX_INDEX]; /* 电机速度环 */
    PID_T pidYaw;                  /* 旋转方向的角度环PID */
    PidParam_T pidMotorParam;      /* 电机速度环参数 */
    uint8_t useIntervalControl;    /* 使用内部位置信息进行航向控制标志 */
    float carPitch;                /* 小车控制使用的偏航角 */
    float carRoll;                 /* 小车控制使用的偏航角 */
    float carYaw;                  /* 小车控制使用的偏航角 */
    float carLocationX;            /* 小车控制使用的偏航角 */
    float carLocationY;            /* 小车控制使用的偏航角 */

    /* 运动控制器 */
    float targetSpeedX; /* 前进速度(m/s) */
    float targetSpeedT; /* 旋转速度(deg/s) */

    /* 反馈速度 */
    float actualSpeedX; /* 实际前进速度(m/s) */
    float actualSpeedY; /* 实际前进速度(m/s) */
    float actualSpeedT; /* 实际旋转速度(deg/s) */

    /* 外环输出速度 */
    float outerSpeedX; /* 外环计算后的X速度(m/s) */
    float outerSpeedY; /* 外环计算后的X速度(m/s) */
    float outerSpeedT; /* 外环计算后的T转速(deg/s) */

    /* 位置环最大速度 */
    float remoteMaxSpeedX; /* 实际前进速度(m/s) */
    float remoteMaxSpeedT; /* 实际旋转速度(deg/s) */

    /* 里程计位置 */
    float slipAlpha;      /* 计算出的系数1 */
    float slipBeta;       /* 计算出的系数2 */
    float slipGamma;      /* 计算出的系数3 */
    float slipTheta;      /* 固定系数4 */
    float slipK1, slipK2; /* 输入的系数K1,K2 */

    /* 小车数据 */
    float wheelBase;        /* 左右轮距(cm) */
    float wheelDiameter;    /* 轮胎直径(cm) */
    float inputDeadZone;    /* 输入死区 */
    uint32_t encoderPerRev; /* 单圈编码器转数 */

    /* 标志位 */
    uint64_t lastUpdateTick; /* 上次更新时间 */
    CarState_T currentState; /* 控制模式 */
} CarControl_T;

/* 避障模块控制结构体 */
typedef struct __Avoidance_T
{
    struct __CarControl_T *baseRemote; /* 基础遥控指针 */
    float ctrlSpeedX;                  /* 前进/后退速度（正值前进，负值后退） */
    float ctrlSpeedT;                  /* 转向速度 */
    float minSpeedX;                   /* 最小前进速度 */
    float minSpeedT;                   /* 最小转动角度 */
    float frontAngleRange;             /* 前方避障角度范围（总宽度，默认100度） */
    float obstacleDistanceThreshold;   /* 避障距离阈值（默认220mm） */
    float ultraSonicThreshold;         /* 超声波避障阈值（低矮障碍物） */
    float ctrlSpeedTGain;              /* 转向速度增益 */
    uint8_t isReversed;                /* 是否处于后退状态标志 */
    uint8_t isUltraSonicTrigger;       /* 是否触发超声波避障 */
} Avoidance_T;

/* 小车运动的数据 */
typedef struct __Car_T
{
    int16_t stopMotorSpeed[CAR_MAX_INDEX];
    int16_t stopMotorEncoder[CAR_MAX_INDEX];
    CarControl_T remote;
    struct __Avoidance_T avoidance;
    int8_t readMotorEncoderFlag;
    volatile CarMode_T state; /* 当前状态 */
    uint32_t startTick;       /* 开始时刻 */
} Car_T;

extern Car_T g_Car;

void CarControl_Init(CarControl_T *remote);
void CarControl_Update(CarControl_T *remote);
void CarControl_SetChassisType(CarControl_T *remote, ChassisType_T chassisType);
void CarControl_Stop(CarControl_T *remote);
void CarControl_SetDirection(CarControl_T *remote, float directionYaw);
void CarControl_SetTurn(CarControl_T *remote, float deltaYaw);
void CarControl_UpdateLocation(CarControl_T *remote);
ChassisType_T CarControl_GetChassisType(CarControl_T *remote);

void CarControl_MotorSetPWM(CarControl_T *remote, CarIndex_T motorIndex, float pwmValue);
void CarControl_ServoSetAngle(CarControl_T *remote, ServoSelect_T servoIndex, float angle);
int32_t CarControl_EncoderGetCount(CarControl_T *remote, CarIndex_T encoderIndex);
void CarControl_SetSpeed(CarControl_T *remote, float speedX, float speedT);

uint8_t CarControl_IsTurnState(CarControl_T *remote);

void Avoidance_Init(Avoidance_T *avoidance, struct __CarControl_T *remote);
void Avoidance_Tick(Avoidance_T *avoidance);
void Avoidance_UltraSonicCheck(Avoidance_T *avoidance, float frontDistance);

void Car_Init(void);
void Car_Stop(void);
void Car_Tick(void);
CarMode_T Car_GetMode(void);
float Car_GetRunTime(void);
void Car_SetMode(CarMode_T newState);
void Car_SetSpeed(float ctrlSpeedX, float ctrlSpeedT);
void Car_Turn(float deltaYaw);
void Car_SetDirection(float directionYaw);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_CONTROL_H */
