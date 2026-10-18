#ifndef LIDAR_LOCALIZATION_RUNTIME_H
#define LIDAR_LOCALIZATION_RUNTIME_H
#include "lidar_localization.h"
#include "bsp_lidar.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct
{
    float forward_m, left_m, yaw_rad;
    /* Confirmed convention: zero forward, clockwise positive. Use -1. */
    float angle_sign;
    float min_range_m, max_range_m;
    /* Low-speed gates because parser has no per-point acquisition timestamps. */
    float max_linear_speed, max_angular_speed;
    float reference_max_speed, reference_max_rate;
} LidarLocMount;
typedef struct
{
    uint32_t submitted, overwritten, accepted, rejected, reference_epoch;
    uint64_t last_scan_us, last_correction_us;
    float front_min_m;
    uint8_t enabled, near_obstacle;
} LidarLocRuntimeStats;
extern LidarLocResult g_LidarLocResult;
extern LidarLocRuntimeStats g_LidarLocStats;
/* Default disabled. Supply measured mounting values before enabling.
 * All functions run in cooperative task context, never ISR. */
int LidarLoc_RuntimeEnable(const LidarLocMount *mount);
void LidarLoc_RuntimeDisable(void);
void LidarLoc_RuntimeSubmit(const LidarDataList_T *scan, uint64_t receipt_us);
void LidarLoc_RuntimePoll(uint64_t now_us);
#ifdef __cplusplus
}
#endif
#endif
