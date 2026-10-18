#ifndef ARM_CONTROL_H
#define ARM_CONTROL_H
#include "arm_kinematics.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct
{
    float zero_deg, direction, min_deg, max_deg;
    float pulse_min_us, pulse_max_us;
} ArmServoCalibration;
/* Call after board initialization. Does not move joints or change PCA frequency.
 * calibration[0..6]: joints; calibration[7]: camera pitch. */
ArmStatus ArmControl_Enable(const ArmServoCalibration calibration[8]);
void ArmControl_Disable(void);
int ArmControl_IsEnabled(void);
ArmStatus ArmControl_WriteJoints(const float q[7]);
ArmStatus ArmControl_SetCameraPitch(float radians);
#ifdef __cplusplus
}
#endif
#endif
