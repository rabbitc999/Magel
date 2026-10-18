#ifndef __KD_TREE_H
#define __KD_TREE_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include "bsp_system.h"

#define LIDAR_POSITION_MAX_NODES 600  // 最大节点数

typedef struct tagLidarPoint2D_T  // 定义二维点的结构体.
{
    float x, y;
} LidarPoint2D_T;

typedef struct tagLidarNodeKD_T  // 定义节点的结构体.
{
    LidarPoint2D_T point;
    struct tagLidarNodeKD_T *left;
    struct tagLidarNodeKD_T *right;
} LidarNodeKD_T;

typedef struct tagLidarPosition_T
{
    LidarNodeKD_T *RootNode;                                  // 上一时刻点云数据建立的树的根节点
    LidarPoint2D_T NodePoints[LIDAR_POSITION_MAX_NODES];      // 当前时刻的点云数据
    LidarPoint2D_T NodePointsCopy[LIDAR_POSITION_MAX_NODES];  // 中间变量
    LidarPoint2D_T TempPoints[LIDAR_POSITION_MAX_NODES];      // 中间变量
    LidarNodeKD_T NodeKDNodes[LIDAR_POSITION_MAX_NODES];      // 静态分配节点数组
    LidarNodeKD_T LastNodeKDNodes[LIDAR_POSITION_MAX_NODES];  // 静态分配节点数组
    int32_t availableNodeIndex;                               // 跟踪下一个可用节点的索引
} LidarPosition_T;

void LidarPosition_Positioning(LidarDataList_T *PointList, float currentYaw, int32_t *result, uint8_t startFlag);
void LidarPosition_PreorderKDTree(char *outputBuffer, uint32_t length);

#endif
