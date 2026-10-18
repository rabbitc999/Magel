#ifndef __IMU_H
#define __IMU_H

#include "bsp_system.h"
#include "algorithm.h"

/* 数据更新 */
#define IMU_ACC_UPDATE   0x01
#define IMU_GYRO_UPDATE  0x02
#define IMU_ANGLE_UPDATE 0x04
#define IMU_MAG_UPDATE   0x08
#define IMU_READ_UPDATE  0x80
#define IMU_QUAT_UPDATE  0x10
#define IMU_PRESS_UPDATE 0x20

#ifdef __cplusplus
extern "C" {
#endif

/* 陀螺仪运行状态结构体 */
typedef enum __ImuRunState_T
{
    IMU_FIX_STATE_READY = 1,      /* 准备校准状态 */
    IMU_FIX_STATE_WAIT = 2,       /* 等待校准 */
    IMU_FIX_STATE_START = 3,      /* 校准状态 */
    IMU_FIX_STATE_DONE = 4,       /* 校准完成状态 */
    IMU_INIT_FLAG = 6,            /* 初始化状态 */
    IMU_FIX_STATE_MAG_START = 7,  /* 磁力计开始校准状态 */
    IMU_FIX_STATE_MAG_UPDATE = 8, /* 磁力计校准状态 */
    IMU_FIX_STATE_MAG_END = 9,    /* 磁力计结束校准状态 */
    IMU_PROCESS_STATE = 0         /* 正常运行状态 */
} ImuRunState_T;

/* 陀螺仪数据处理结构体 */
typedef struct __ImuControl_T
{
    /* 数据处理信息 */
    float GyroX, GyroY, GyroZ;
    float AccelX, AccelY, AccelZ;
    float MagX, MagY, MagZ;
    float Pitch, Roll, Yaw;
    float Q0, Q1, Q2, Q3;

    /* 数据采集 */
    ImuRunState_T runState; /* 运行状态 */

    /* 运动学信息 */
    float ImuSpeedX, ImuSpeedY;    /* IMU速度信息 (世界坐标系) */
    float EncSpeedX, EncSpeedY;    /* 编码器速度信息 (相对于初始位置) */
    float OdomSpeedX, OdomSpeedY;  /* 里程计速度信息 (相对于初始位置) */
    float LocationX, LocationY;    /* 位置信息 (相对于初始位置) */
    float OdomAngleZ, StartAngleZ; /* 偏航角 (相对于初始位置) */

    /* 其他 */
    uint64_t lastTick; /* 上次更新时间戳 */
    float lastYaw;     /* 上次的偏航角，用于计算偏航角变化 */
} ImuControl_T;

extern ImuControl_T g_IMU;

/* 原有函数声明 */
void IMU_ResetOrientation(void);
void IMU_Loop(void);

#ifdef __cplusplus
}
#endif

#endif /* __IMU_H */