#ifndef __TRAJECTORY_TRACKER_H
#define __TRAJECTORY_TRACKER_H

#include "bsp_system.h"  // 仍需包含基础类型定义

#include "car_map.h"
#include "path_planing.h"

// ================= 轨迹追踪配置参数 =================
// 轨迹追踪器路径的最大长度
// 注意：这里比 MAX_SOLVE_NODES 多了 1，是为起点预留的
#define TRACK_LEN_MAX 129

// 函数声明
void Tracker_Init(TrajectoryTracker *tracker, PointType *track, uint16_t len, float ld, float speed);
void Tracker_Update(TrajectoryTracker *tracker, VectorType current_pose);

#endif