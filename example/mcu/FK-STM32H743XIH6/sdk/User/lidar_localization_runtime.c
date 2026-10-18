#include "lidar_localization_runtime.h"
#include <math.h>
#include <string.h>
static LidarLocalizer localizer;
static LidarLocPoint pending[LIDAR_LOC_MAX_POINTS];
static LidarLocMount mounting;
static unsigned pending_count;
static uint64_t pending_us;
static uint32_t pending_epoch;
static uint8_t have_pending;
LidarLocResult g_LidarLocResult = { .status = LIDAR_LOC_DISABLED };
LidarLocRuntimeStats g_LidarLocStats;
int LidarLoc_RuntimeEnable(const LidarLocMount *m)
{
    LidarLoc_RuntimeDisable();
    if (!m || !isfinite(m->forward_m) || !isfinite(m->left_m) || !isfinite(m->yaw_rad) || m->angle_sign != -1 || !isfinite(m->min_range_m) || !isfinite(m->max_range_m) || m->min_range_m < 0.05f || m->max_range_m <= m->min_range_m || m->max_range_m > 30 || !isfinite(m->max_linear_speed) || m->max_linear_speed <= 0 || m->max_linear_speed > 0.3f || !isfinite(m->max_angular_speed) || m->max_angular_speed <= 0 || m->max_angular_speed > 0.3f || !isfinite(m->reference_max_speed) || m->reference_max_speed < 0 || m->reference_max_speed > m->max_linear_speed || !isfinite(m->reference_max_rate) || m->reference_max_rate < 0 || m->reference_max_rate > m->max_angular_speed) return 0;
    mounting = *m;
    LidarLoc_Init(&localizer, 0);
    have_pending = 0;
    memset(&g_LidarLocStats, 0, sizeof(g_LidarLocStats));
    g_LidarLocStats.enabled = 1;
    g_LidarLocStats.reference_epoch = g_ChassisEKF_Epoch;
    g_LidarLocStats.front_min_m = INFINITY;
    memset(&g_LidarLocResult, 0, sizeof(g_LidarLocResult));
    g_LidarLocResult.status = LIDAR_LOC_WARMUP;
    return 1;
}
void LidarLoc_RuntimeDisable(void)
{
    g_LidarLocStats.enabled = 0;
    have_pending = 0;
    LidarLoc_Init(&localizer, 0);
    g_LidarLocResult.status = LIDAR_LOC_DISABLED;
}
void LidarLoc_RuntimeSubmit(const LidarDataList_T *scan, uint64_t receipt_us)
{
    unsigned i, valid = 0, stride;
    uint32_t coverage = 0;
    float front = INFINITY;
    if (!g_LidarLocStats.enabled) return;
    if (!scan || scan->count > PACKET_LIDAR_MAX_SAMPLES)
    {
        g_LidarLocResult.status = LIDAR_LOC_INVALID;
        return;
    }
    if (have_pending) g_LidarLocStats.overwritten++;
    have_pending = 0;
    pending_count = 0;
    /* Deterministic even downsampling; retain raw scan unchanged for existing avoidance. */
    stride = (scan->count + LIDAR_LOC_MAX_POINTS - 1) / LIDAR_LOC_MAX_POINTS;
    if (!stride) stride = 1;
    for (i = 0; i < scan->count; i++)
    {
        float range = scan->buffer[i].distance * 0.001f;
        float degrees = scan->buffer[i].angle, theta, x, y;
        if (!isfinite(range) || !isfinite(degrees) || range < mounting.min_range_m || range > mounting.max_range_m) continue;
        degrees = fmodf(degrees, 360);
        if (degrees < 0) degrees += 360;
        theta = mounting.angle_sign * degrees * 0.01745329252f + mounting.yaw_rad;
        x = mounting.forward_m + range * cosf(theta);
        y = mounting.left_m + range * sinf(theta);
        if (x > 0 && fabsf(y) < 0.57735027f * x) front = fminf(front, hypotf(x, y));
        coverage |= 1UL << ((unsigned)(degrees / 15) % 24);
        valid++;
        if (i % stride == 0 && pending_count < LIDAR_LOC_MAX_POINTS)
        {
            pending[pending_count].x = x;
            pending[pending_count++].y = y;
        }
    }
    g_LidarLocStats.front_min_m = front;
    g_LidarLocStats.near_obstacle = front < 0.7f;
    {
        unsigned bins = 0;
        for (i = 0; i < 24; i++)
            if (coverage & (1UL << i)) bins++;
        if (valid < 40 || pending_count < 40 || bins < 18)
        {
            g_LidarLocResult.status = LIDAR_LOC_TOO_FEW;
            g_LidarLocStats.rejected++;
            return;
        }
    }
    pending_us = receipt_us;
    pending_epoch = g_ChassisEKF_Epoch;
    have_pending = 1;
    g_LidarLocStats.submitted++;
    g_LidarLocStats.last_scan_us = receipt_us;
}
void LidarLoc_RuntimePoll(uint64_t now_us)
{
    LidarLocStatus status;
    if (!g_LidarLocStats.enabled) return;
    if (g_LidarLocStats.reference_epoch != g_ChassisEKF_Epoch)
    {
        LidarLoc_Init(&localizer, 0);
        have_pending = 0;
        g_LidarLocStats.reference_epoch = g_ChassisEKF_Epoch;
        g_LidarLocResult.status = LIDAR_LOC_WARMUP;
    }
    if (!have_pending) return;
    if (pending_epoch != g_ChassisEKF_Epoch || now_us < pending_us || now_us - pending_us > 100000ULL)
    {
        have_pending = 0;
        g_LidarLocStats.rejected++;
        g_LidarLocResult.status = LIDAR_LOC_STALE;
        return;
    }
    /* Wait until a wheel sample reaches scan receipt time; never extrapolate silently. */
    if (!g_ChassisEKF_Ready || g_ChassisEKF_TimestampUs < pending_us) return;
    have_pending = 0;
    if (now_us < g_ChassisEKF_TimestampUs || now_us - g_ChassisEKF_TimestampUs > 100000ULL)
    {
        g_LidarLocStats.rejected++;
        g_LidarLocResult.status = LIDAR_LOC_STALE;
        return;
    }
    if (fabsf(g_ChassisEKF.state[EKF_V]) > mounting.max_linear_speed || fabsf(g_ChassisEKF.state[EKF_W]) > mounting.max_angular_speed || (!localizer.reference_count && (fabsf(g_ChassisEKF.state[EKF_V]) > mounting.reference_max_speed || fabsf(g_ChassisEKF.state[EKF_W]) > mounting.reference_max_rate)))
    {
        g_LidarLocStats.rejected++;
        g_LidarLocResult.status = LIDAR_LOC_MOTION;
        return;
    }
    status = LidarLoc_Correct(&localizer, pending, pending_count, pending_us, g_ChassisEKF_TimestampUs,
                              &g_ChassisEKF, &g_LidarLocResult);
    if (status == LIDAR_LOC_OK)
    {
        g_LidarLocStats.accepted++;
        g_LidarLocStats.last_correction_us = g_ChassisEKF_TimestampUs;
    }
    else if (status != LIDAR_LOC_REFERENCE && status != LIDAR_LOC_WARMUP)
        g_LidarLocStats.rejected++;
}
