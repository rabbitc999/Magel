#ifndef __ALGORITHM_H
#define __ALGORITHM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MATRIX_SVD_MAX_ITERATIONS 100
#define MATRIX_SVD_EPSILON        1e-4

/* 矩阵结构体 */
typedef struct __Matrix2x2_T
{
    float m11;
    float m12;
    float m21;
    float m22;
} Matrix2x2_T;

/* SVD结果结构体 */
typedef struct __SVDResult_T
{
    Matrix2x2_T uMatrix;     /* 左奇异向量矩阵 */
    Matrix2x2_T vMatrix;     /* 右奇异向量矩阵 */
    float singularValue1;    /* 奇异值1 */
    float singularValue2;    /* 奇异值2 */
    uint8_t convergenceFlag; /* 收敛标志 */
    uint16_t iterationCount; /* 迭代次数 */
} SVDResult_T;

#define MID_FILT_ALLOC_SIZE 256 /* 预分配的缓冲区大小 */

/* 平均值滤波器结构体 */
typedef struct __AverageFilter
{
    float *data;      /* 数据缓冲区 */
    uint16_t size;    /* 滤波器大小 */
    uint16_t index;   /* 当前索引 */
    float sum;        /* 数据和 */
    uint8_t InitFlag; /* 初始化标志 */
} AverageFilt_T;

/* 中值滤波器结构体 */
typedef struct __MedianFilt_T
{
    float *buffer;          /* 数据缓冲区 */
    float *sortedBuffer;    /* 排序后的缓冲区 */
    uint16_t bufferSize;    /* 缓冲区大小 */
    uint16_t currentIndex;  /* 当前写入索引 */
    uint16_t dataCount;     /* 当前有效数据个数 */
    uint8_t isBufferFilled; /* 缓冲区是否已填满 */
    uint8_t InitFlag;       /* 初始化标志 */
} MedianFilt_T;

/* 卡尔曼滤波器结构体 */
typedef struct __KalmanFilter_T
{
    float X_Est;  /* 估计值 */
    float P_Est;  /* 估计误差协方差 */
    float Q;      /* 过程噪声协方差 */
    float R;      /* 测量噪声协方差 */
    float X_Pred; /* 预测值 */
    float P_Pred; /* 预测误差协方差 */
    float K;      /* 卡尔曼增益 */
    float initX;  /* 记录初始化时的初始状态估计值 */
    float initP;  /* 记录初始化时的初始协方差 */
} KalmanFilter_T;

/* PID参数结构体 */
typedef struct __PidParam_T
{
    float Kp;
    float Ki;
    float Kd;
    float OutputLimit;
    float IntegralLimit;
} PidParam_T;

/* PID控制器的结构体 */
typedef struct __PID_T
{
    float Output;             /* PID控制器输出值 */
    float Target;             /* 目标值 */
    float Actual, calcActual; /* 实际值与计算使用的实际值 */
    float Error;              /* 当前偏差 (Target - calcActual) */
    float LastError;          /* 前一时刻偏差 */
    float LastOutput;         /* 上一时刻的输出值 */
    float Kp, Ki, Kd;         /* PID系数 */
    float OutputLimit;        /* 输出限幅 */
    float IntegralLimit;      /* 积分限幅 */
    float ErrorInt;           /* 积分项 */
    float ZeroLimit;          /* 零点限制 */
} PID_T;

void PID_Clear(PID_T *pid);
float PID_Control(PID_T *pid, float deltaTime);
void PID_Copy(PID_T *dest, PID_T *src);
void PID_SetParam(PID_T *dest, PidParam_T *src);

void AverageFilt_Init(AverageFilt_T *filter, uint16_t size);
float AverageFilt_Filter(AverageFilt_T *filter, float newData);
void AverageFilt_Clear(AverageFilt_T *filter);

void MedianFilt_Init(MedianFilt_T *filter, uint16_t size);
float MedianFilt_Update(MedianFilt_T *filter, float newValue);
void MedianFilt_Clear(MedianFilt_T *filter);
uint16_t MedianFilt_GetDataCount(const MedianFilt_T *filter);
uint8_t MedianFilt_IsBufferFilled(const MedianFilt_T *filter);

void Kalman_Init(KalmanFilter_T *kalmanFilter, float initX, float initP, float Q, float R);
float Kalman_Update(KalmanFilter_T *kalmanFilter, float measurement);
void Kalman_Clear(KalmanFilter_T *kalmanFilter);

void HeapSort(void *base, size_t nmemb, size_t size, int (*compare)(const void *, const void *));
float QuickSelect_SelectKthByExtractor(void *base, size_t count, size_t size, float (*extract)(const void *), size_t k);
float QuickSelect_GetMedianByExtractor(void *base, size_t count, size_t size, float (*extract)(const void *));
void *QuickSelect_GetKthElementByExtractor(void *base, size_t count, size_t size, float (*extract)(const void *), size_t k);

uint16_t GCD(uint16_t a, uint16_t b);
uint16_t GCD_Array(uint16_t arr[], uint16_t arrLength);

int8_t Matrix_SVD_Decompose(const Matrix2x2_T *inputMatrix, SVDResult_T *result);
float Matrix_SVD_CalculateConditionNumber(const SVDResult_T *result);
int8_t Matrix_SVD_ReconstructMatrix(const SVDResult_T *result, Matrix2x2_T *outputMatrix);

#ifdef __cplusplus
}
#endif

#endif /* __ALGORITHM_H */