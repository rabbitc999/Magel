#ifndef __CAR_MAP_H
#define __CAR_MAP_H

#include "bsp_system.h"  // 仍需包含基础类型定义

// ================= 地图配置参数 =================
// 地图物理尺寸 (像素数)
#define CAR_MAP_SIZE    200
// 每个点占用的比特数 (2bit模式: 0=未知, 1=空闲, 2=占据, 3=静态障碍)
#define BIT_PER_POINT   2
// 地图中两个点格子之间的距离 (单位: 米)
#define POINT_GAP       0.01

// 计算每行占用的字节数 (用于位操作)
#define MAP_ROW_BYTES   ((CAR_MAP_SIZE * BIT_PER_POINT) / (sizeof(uint8_t) * 8))
// 每个字节存储的点数
#define POINTS_PER_BYTE (8 / BIT_PER_POINT)
// 位掩码
#define BIT_MASK        ((1 << BIT_PER_POINT) - 1)

// 轮廓线距离 (用于边缘检测)
#define CONTOUR_DIST    3

// 射线追踪建图任务
void Update_Map(void);

// 图像处理
void erode_map(void);
void dilate_map(void);
void Map_Deep_Boundary_Clear(void);
void Map_Generate_Boundary_Contour(void);
void Map_Global_Opening_InPlace(void);

// 图像压缩和测试图像的生成
void Map_Compress_Process(int meter_pixel);
void Map_Generate_Test_C_Shape_Advanced(void);

// ================= 地图读写操作 =================
// 像素级读取
void SetMapValue_ByPixel(int x, int y, uint8_t state);
uint8_t GetMapValue_ByPixel(int x, int y);

// 真实物理坐标读写
void SetMapValue(float x, float y, uint8_t state);
uint8_t GetMapValue(float x, float y);
void UpdateMapValue(float x, float y, int8_t delta);

#endif