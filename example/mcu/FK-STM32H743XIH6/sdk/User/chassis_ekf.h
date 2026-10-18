#ifndef CHASSIS_EKF_H
#define CHASSIS_EKF_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* SI units. Body x forward, world yaw counterclockwise. */
enum
{
    EKF_X,
    EKF_Y,
    EKF_YAW,
    EKF_V,
    EKF_W,
    EKF_BIAS,
    EKF_N
};
typedef enum
{
    EKF_OK,
    EKF_INVALID,
    EKF_REJECTED
} EkfStatus;
typedef struct
{
    float acceleration_noise, angular_acceleration_noise, bias_walk_noise;
    float encoder_speed_variance, encoder_rate_variance, gyro_variance;
    float gate_sigma, max_dt;
} ChassisEkfConfig;
typedef struct
{
    float state[EKF_N], covariance[EKF_N][EKF_N];
    ChassisEkfConfig config;
} ChassisEkf;
void ChassisEKF_DefaultConfig(ChassisEkfConfig *config);
EkfStatus ChassisEKF_Init(ChassisEkf *filter, const ChassisEkfConfig *config,
                          float x, float y, float yaw);
EkfStatus ChassisEKF_Predict(ChassisEkf *filter, float dt);
/* Independent wheel speed and wheel yaw-rate observations, diagonal R.
 * Pair update is transactional: either both observations pass or neither changes state. */
EkfStatus ChassisEKF_UpdateEncoder(ChassisEkf *filter, float speed, float rate);
EkfStatus ChassisEKF_UpdateGyro(ChassisEkf *filter, float rate);
EkfStatus ChassisEKF_UpdatePosition(ChassisEkf *filter, float x, float y,
                                    float x_variance, float y_variance);
EkfStatus ChassisEKF_UpdateHeading(ChassisEkf *filter, float yaw, float variance);
/* Joint correlated pose observation, transactional NIS gate (3 DoF).
 * R must be symmetric positive definite. No delayed-state replay. */
EkfStatus ChassisEKF_UpdatePose(ChassisEkf *filter, const float pose[3],
                                const float covariance[3][3], float max_nis);
extern uint32_t g_ChassisEKF_Epoch;
/* Board adapter. Reset, Gyro and Poll: same cooperative task context.
 * Encoder only enqueues a bounded sample and may run in the control ISR.
 * Timestamps are host receipt times in microseconds, not sensor acquisition times. */
extern ChassisEkf g_ChassisEKF;
extern uint64_t g_ChassisEKF_TimestampUs;
extern EkfStatus g_ChassisEKF_Status;
extern uint8_t g_ChassisEKF_Ready;
void ChassisEKF_RuntimeReset(void);
void ChassisEKF_RuntimePoll(void);
void ChassisEKF_RuntimeGyro(float degrees_per_second, uint64_t timestamp_us);
void ChassisEKF_RuntimeEncoder(float speed, float degrees_per_second,
                               uint64_t timestamp_us, int supported_chassis);
#ifdef __cplusplus
}
#endif
#endif
