#ifndef LIDAR_LOCALIZATION_H
#define LIDAR_LOCALIZATION_H
#include "chassis_ekf.h"
#ifdef __cplusplus
extern "C" {
#endif
#define LIDAR_LOC_MAX_POINTS     256
#define LIDAR_LOC_MAX_ITERATIONS 12
typedef struct
{
    float x, y;
} LidarLocPoint;
typedef enum
{
    LIDAR_LOC_OK,
    LIDAR_LOC_REFERENCE,
    LIDAR_LOC_INVALID,
    LIDAR_LOC_TOO_FEW,
    LIDAR_LOC_LOW_OVERLAP,
    LIDAR_LOC_NOISY,
    LIDAR_LOC_DEGENERATE,
    LIDAR_LOC_NOT_CONVERGED,
    LIDAR_LOC_JUMP,
    LIDAR_LOC_EKF_REJECTED,
    LIDAR_LOC_STALE,
    LIDAR_LOC_WARMUP,
    LIDAR_LOC_MOTION,
    LIDAR_LOC_DISABLED
} LidarLocStatus;
typedef struct
{
    float correspondence_distance, residual_limit, huber_scale;
    float min_inlier_ratio, max_rmse, normal_ratio, min_geometry_ratio;
    float max_seed_translation, max_seed_yaw;
    float temporal_translation_slack, temporal_yaw_slack;
    float max_update_translation, max_update_yaw, max_nis;
    float position_variance_floor, yaw_variance_floor;
    unsigned min_points, iterations, confirmations;
} LidarLocConfig;
typedef struct
{
    LidarLocStatus status;
    float pose[3], covariance[3][3];
    float rmse, inlier_ratio, geometry_ratio;
    unsigned inliers, unmatched;
} LidarLocResult;
typedef struct
{
    LidarLocConfig config;
    LidarLocPoint reference[LIDAR_LOC_MAX_POINTS];
    LidarLocPoint normals[LIDAR_LOC_MAX_POINTS];
    uint16_t order[LIDAR_LOC_MAX_POINTS];
    unsigned reference_count, consecutive;
    uint64_t last_scan_us, last_good_us;
    float last_good_pose[3];
} LidarLocalizer;
void LidarLoc_DefaultConfig(LidarLocConfig *config);
void LidarLoc_Init(LidarLocalizer *localizer, const LidarLocConfig *config);
/* Input points: metres in body frame, already corrected for lidar mounting.
 * Reference is frozen, anchored in EKF local world; not a global SLAM map. */
LidarLocStatus LidarLoc_SetReference(LidarLocalizer *localizer,
                                     const LidarLocPoint *points, unsigned count, const float world_pose[3]);
LidarLocStatus LidarLoc_Match(const LidarLocalizer *localizer,
                              const LidarLocPoint *points, unsigned count, const float seed[3], LidarLocResult *result);
/* Task context only. Reject stale scans; quality + temporal gates + joint EKF NIS.
 * last_good tracks geometrically valid lidar poses independently of EKF acceptance. */
LidarLocStatus LidarLoc_Correct(LidarLocalizer *localizer,
                                const LidarLocPoint *points, unsigned count, uint64_t scan_us, uint64_t state_us,
                                ChassisEkf *filter, LidarLocResult *result);
#ifdef __cplusplus
}
#endif
#endif
