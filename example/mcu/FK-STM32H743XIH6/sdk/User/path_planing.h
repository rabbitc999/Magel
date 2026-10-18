#ifndef __PATH_PLANING_H
#define __PATH_PLANING_H

#include "bsp_system.h"

// 基础的点类型
typedef struct
{
    float x;
    float y;
    float z;
} PointType;

// 基础的向量类型
typedef struct
{
    float x;        // X坐标，单位：m
    float y;        // Y坐标，单位：m
    float theta;    // 朝向角，单位：rad，x轴正方向为0，逆时针为正
    float v;        // 线速度 m/s
    float angular;  // 角速度 rad/s
} VectorType;

// 路径规划求解器使用的障碍物类型
typedef enum CrackType
{
    FIND_SAFE = 0U,  // 没有障碍物
    FIND_CRACK       // 有障碍物
} CrackType;

// 路径规划求解器使用的路径点解类型
typedef enum SolveType
{
    NO_FIND = 0U,        // 无解
    FIND_BACKED_SOLVE,   // 解在历史路径上
    FIND_EDGE_SOLVE,     // 解在障碍物边缘
    FIND_SPACIOUS_SOLVE  // 解在空旷位置
} SolveType;

// 路径规划的状态信息
typedef enum
{
    SOLVE_OK,        // 求解成功
    STACK_OVERFLOW,  // 求解堆栈溢出
    NO_SPACE_POINT,  // 求解某个路径点时发现没有解
    IDLE             // 求解器未运行
} SolveErrorType;

// 路径规划求解器得到的路径解
typedef struct
{
    PointType point;       // 当前解的数值
    SolveType solvestate;  // 当前解的状态
} TrackPointType;

// 轨迹追踪器信息------------------------------------------------------------
// 轨迹追踪器结构
typedef struct
{
    PointType *track;           // 路径点数组指针
    uint16_t track_length;      // 路径点总数
    uint16_t near_index;        // 当前追踪的路径点索引（最近点）
    uint16_t lookahead_index;   // 预瞄点索引
    PointType lookahead_point;  // 预瞄点
    PointType lookahead_bias;   // 预瞄点和当前位置的偏差
    float lookahead_distance;   // 前瞻距离 Ld (米)
    float target_speed;         // 目标线速度 v (米/秒)
    float k_pure_pursuit;       // 纯追踪增益系数
    FunctionalState state;      // 追踪器状态
    float slowdown_start_dist;  // 開始減速的距離 (米)
    float stop_dist;            // 完全停止的距離 (米)
    // 输出
    float cmd_linear;   // 计算出的线速度指令
    float cmd_angular;  // 计算出的角速度指令 (弧度/秒)
} TrajectoryTracker;

// 函数声明
void PlanPath_Iterative(VectorType vstart, PointType goal, PointType *path, uint16_t *path_len);

#endif