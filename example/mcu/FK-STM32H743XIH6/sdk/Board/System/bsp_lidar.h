#ifndef __BSP_LIDAR_H
#define __BSP_LIDAR_H

#include <stdint.h>

/* 固定雷达类型（取消注释其中一项以取消自动检测） */
#define LIDAR_FIXED_TYPE          LIDAR_TYPE_YDLIDAR_X2
// #define LIDAR_FIXED_TYPE   LIDAR_TYPE_N10
// #define LIDAR_FIXED_TYPE   LIDAR_TYPE_N10P

/* 雷达参数 */
#define N10P_LIDAR_MAX_SAMPLES    900
#define N10_LIDAR_MAX_SAMPLES     450
#define YDLIDAR_LIDAR_MAX_SAMPLES 600
#define N10_POINT_COUNT           16
#define N10P_FAILURE_THRESHOLD    10 /* N10P 连续失败10次后重置检测 */
#define N10_FAILURE_THRESHOLD     10 /* N10  连续失败10次后重置检测 */

/* N10P 激光雷达相关定义 */
#define N10P_POINT_COUNT          32  /* 32个点 */
#define N10P_PACKET_LENGTH        108 /* 数据包总长度 */

/* N10P 包雷达最大接收点 */
#define PACKET_LIDAR_MAX_SAMPLES  900

#ifdef __cplusplus
extern "C" {
#endif

/* YDLidar X2 状态枚举 */
typedef enum __YdlidarState_T
{
    YDLIDAR_STATE_SYNC1,         /* 等待第一个同步字节 (0xAA) */
    YDLIDAR_STATE_SYNC2,         /* 等待第二个同步字节 (0x55) */
    YDLIDAR_STATE_CT,            /* 接收包类型字节 */
    YDLIDAR_STATE_LSN,           /* 接收采样数量字节 */
    YDLIDAR_STATE_FSA_L,         /* 接收起始角低字节 */
    YDLIDAR_STATE_FSA_H,         /* 接收起始角高字节 */
    YDLIDAR_STATE_LSA_L,         /* 接收结束角低字节 */
    YDLIDAR_STATE_LSA_H,         /* 接收结束角高字节 */
    YDLIDAR_STATE_CS_L,          /* 接收校验码低字节 */
    YDLIDAR_STATE_CS_H,          /* 接收校验码高字节 */
    YDLIDAR_STATE_SAMPLE_DATA_L, /* 接收低字节采样数据 */
    YDLIDAR_STATE_SAMPLE_DATA_H  /* 接收高字节采样数据 */
} YdlidarState_T;

/* N10 状态枚举 */
typedef enum __N10State_T
{
    N10_STATE_HEADER_0,      /* 帧头字节0 (0xA5) */
    N10_STATE_HEADER_1,      /* 帧头字节1 (0x5A) */
    N10_STATE_LENGTH,        /* 数据包长度字节 (Byte_2) */
    N10_STATE_SPEED_H,       /* 转速高字节 (Byte_3) */
    N10_STATE_SPEED_L,       /* 转速低字节 (Byte_4) */
    N10_STATE_START_ANGLE_H, /* 起始角度高字节 (Byte_5) */
    N10_STATE_START_ANGLE_L, /* 起始角度低字节 (Byte_6) */
    N10_STATE_POINTS,        /* 点云数据 (Byte_7 - Byte_54) */
    N10_STATE_END_ANGLE_H,   /* 结束角度高字节 (Byte_55) */
    N10_STATE_END_ANGLE_L,   /* 结束角度低字节 (Byte_56) */
    N10_STATE_CRC            /* 校验字节 (Byte_57) */
} N10State_T;

/* N10P 状态机 */
typedef enum __N10PState_T
{
    N10P_STATE_HEADER_0,      /* 帧头字节0 (0xA5) */
    N10P_STATE_HEADER_1,      /* 帧头字节1 (0x5A) */
    N10P_STATE_LENGTH,        /* 数据包长度 */
    N10P_STATE_SPEED_H,       /* 转速高字节 */
    N10P_STATE_SPEED_L,       /* 转速低字节 */
    N10P_STATE_START_ANGLE_H, /* 起始角度高字节 */
    N10P_STATE_START_ANGLE_L, /* 起始角度低字节 */
    N10P_STATE_POINT_DATA,    /* 点数据 (96字节，32个点×3字节) */
    N10P_STATE_UNKNOWN_1,     /* 未知字节1 (字节103) */
    N10P_STATE_UNKNOWN_2,     /* 未知字节2 (字节104) */
    N10P_STATE_END_ANGLE_H,   /* 结束角度高字节 */
    N10P_STATE_END_ANGLE_L,   /* 结束角度低字节 */
    N10P_STATE_CRC            /* 校验字节 */
} N10PState_T;

/* 雷达类型枚举 */
typedef enum __LidarType_T
{
    LIDAR_TYPE_UNKNOWN,
    LIDAR_TYPE_YDLIDAR_X2,
    LIDAR_TYPE_N10,
    LIDAR_TYPE_N10P
} LidarType_T;

/* 雷达类型枚举 */
typedef enum __LidarPointTag_T
{
    LIDAR_POINT_TAG_UNKNOW = 0x00,
    LIDAR_POINT_TAG_FORWARD = 0x01,
    LIDAR_POINT_TAG_BACK = 0x02,
    LIDAR_POINT_TAG_LEFT = 0x04,
    LIDAR_POINT_TAG_RIGHT = 0x08,
} LidarPointTag_T;

/* YDLidar 数据包结构 */
typedef struct __YdlidarPacket_T
{
    uint8_t PacketType;                         /* 包类型 */
    uint8_t PointCount;                         /* 采样点数量 */
    uint16_t StartAngle;                        /* 起始角度 */
    uint16_t EndAngle;                          /* 结束角度 */
    uint16_t Samples[PACKET_LIDAR_MAX_SAMPLES]; /* 采样数据 */
    uint16_t SampleIndex;                       /* 当前采样点索引 */
    uint16_t CheckCode;                         /* 校验码 */
} YdlidarPacket_T;

/* N10/N10P 数据包结构体 */
typedef struct __N10Packet_T
{
    uint8_t Header0;     /* 帧头字节0 (0xA5) */
    uint8_t Header1;     /* 帧头字节1 (0x5A) */
    uint8_t Length;      /* 数据包长度 (0x3A = 58字节) */
    uint8_t SpeedH;      /* 转速高字节 */
    uint8_t SpeedL;      /* 转速低字节 */
    uint8_t StartAngleH; /* 起始角度高字节 */
    uint8_t StartAngleL; /* 起始角度低字节 */
    struct
    {
        uint8_t DistanceH;     /* 距离高字节 */
        uint8_t DistanceL;     /* 距离低字节 */
        uint8_t Strength;      /* 强度 */
    } Points[N10_POINT_COUNT]; /* 16个点数据 */
    uint8_t EndAngleH;         /* 结束角度高字节 */
    uint8_t EndAngleL;         /* 结束角度低字节 */
    uint8_t CheckCode;         /* 校验码 */
} N10Packet_T;

/* N10P 数据包结构体 */
typedef struct __N10PPacket_T
{
    uint8_t Header0;     /* 帧头字节0 (0xA5) */
    uint8_t Header1;     /* 帧头字节1 (0x5A) */
    uint8_t Length;      /* 数据包长度 */
    uint8_t SpeedH;      /* 转速高字节 */
    uint8_t SpeedL;      /* 转速低字节 */
    uint8_t StartAngleH; /* 起始角度高字节 */
    uint8_t StartAngleL; /* 起始角度低字节 */
    struct
    {
        uint8_t DistanceH;      /* 距离高字节 */
        uint8_t DistanceL;      /* 距离低字节 */
        uint8_t Strength;       /* 强度 */
    } Points[N10P_POINT_COUNT]; /* 32个点数据 */
    uint8_t Unknown1;           /* 未知字节1 */
    uint8_t Unknown2;           /* 未知字节2 */
    uint8_t EndAngleH;          /* 结束角度高字节 */
    uint8_t EndAngleL;          /* 结束角度低字节 */
    uint8_t CheckCode;          /* 校验码 */
    uint8_t PointIndex;         /* 当前点索引 */
} N10PPacket_T;

/* 雷达解析后的数据点结构 */
typedef struct __LidarData_T
{
    LidarPointTag_T pointType; /* 点类型 */
    float distance;            /* 距离 (mm) */
    float angle;               /* 角度 (度) */
} LidarData_T;

/* 雷达解析后的数据点列表结构 */
typedef struct __LidarDataList_T
{
    LidarData_T buffer[PACKET_LIDAR_MAX_SAMPLES];
    uint32_t count;
    LidarType_T type; /* 雷达类型 */
} LidarDataList_T;

/* 雷达全局状态结构 */
typedef struct __LidarState_T
{
    /* 分界判断阈值 */
    float frontBackAngleCheckThreshold;
    float sideAngleCheckThreshold;

    /* 雷达状态缓存 */
    union
    {
        /* YDLidar X2 当前状态和缓存 */
        struct
        {
            YdlidarState_T ydlidarCurrentState;
            YdlidarPacket_T ydlidarCurrentPacket;
            uint16_t ydlidarPointCount;
            float ydlidarStartAngle;
            float ydlidarEndAngle;
            uint8_t ydlidarFailedCount;
        };
        /* N10 当前状态和缓存 */
        struct
        {
            N10State_T n10CurrentState;
            N10Packet_T n10CurrentPacket;
            uint8_t n10PointIndex;
            uint8_t n10CrcSum;
            uint8_t n10FailedCount;
        };
        /* N10P 当前状态和数据 */
        struct
        {
            N10PState_T n10pCurrentState;
            N10PPacket_T n10pCurrentPacket;
            uint16_t n10pDataIndex; /* 距离数据索引 */
            uint16_t n10pGpsIndex;  /* GPS 数据索引 */
            uint16_t n10pReadCount;
            uint8_t n10pFailedCount;
        };
    };

    /* 当前雷达类型 */
    LidarType_T currentLidarType;

    /* 数据收集状态 */
    uint8_t outputBufferReady; /* 输出缓冲区是否就绪/激光雷达类型检测 */

    /* 双缓冲区 */
    LidarDataList_T processingBuffer; /* 解算缓冲区 */
    LidarDataList_T outputBuffer;     /* 输出缓冲区 */

    /* 雷达收集信息 */
    uint16_t collectPointCount; /* 或者是检测重试时间 */
} LidarState_T;

void Lidar_AutoConnect(void);
void Lidar_ProcessBuffer(void);
LidarDataList_T *Lidar_GetPoints(void);
LidarType_T Lidar_GetType(void);
void Lidar_Reset(void);

extern LidarState_T g_LidarState;

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LIDAR_H */