#ifndef ARM_KINEMATICS_H
#define ARM_KINEMATICS_H
#ifdef __cplusplus
extern "C" {
#endif
#define ARM_DOF 7
/* Standard DH: Rz(q+offset) Tz(d) Tx(a) Rx(alpha).
 * Lengths: mm; all angles: radians. Rotation columns are tool axes in base. */
typedef struct
{
    float a, d, alpha, offset, min, max;
} ArmJoint;
typedef struct
{
    ArmJoint joint[ARM_DOF];
    float damping, orientation_scale, position_tolerance, rotation_tolerance;
    float max_step;
    unsigned iterations;
} ArmModel;
typedef struct
{
    float p[3], r[3][3];
} ArmPose;
typedef enum
{
    ARM_OK,
    ARM_INVALID,
    ARM_NO_CONVERGENCE
} ArmStatus;
ArmStatus Arm_Forward(const ArmModel *model, const float q[ARM_DOF], ArmPose *pose);
/* Seed and result may alias. Result is written only on success. No hardware writes. */
ArmStatus Arm_Inverse(const ArmModel *model, const ArmPose *target,
                      const float seed[ARM_DOF], float result[ARM_DOF]);
#ifdef __cplusplus
}
#endif
#endif
