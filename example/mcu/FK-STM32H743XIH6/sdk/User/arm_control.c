#include "arm_control.h"
#include "bsp_servo.h"
#include "bsp_pca9685.h"
#include <math.h>
#include <string.h>
static ArmServoCalibration calibration[8];
static int enabled;
static float pca_tick_us;
ArmStatus ArmControl_Enable(const ArmServoCalibration c[8])
{
    unsigned i;
    unsigned prescale;
    if (!c) return ARM_INVALID;
    for (i = 0; i < 8; i++)
    {
        if (!isfinite(c[i].zero_deg) || !isfinite(c[i].direction) || fabsf(c[i].direction) != 1 || !isfinite(c[i].min_deg) || !isfinite(c[i].max_deg) || c[i].min_deg < 0 || c[i].max_deg > 180 || c[i].min_deg > c[i].max_deg || !isfinite(c[i].pulse_min_us) || !isfinite(c[i].pulse_max_us) || c[i].pulse_min_us < 500 || c[i].pulse_max_us > 2500 || c[i].pulse_min_us >= c[i].pulse_max_us) return ARM_INVALID;
        /* Existing STM32 driver uses fixed 500..2500 us calibration. */
        if (i >= 6 && (c[i].pulse_min_us != 500 || c[i].pulse_max_us != 2500)) return ARM_INVALID;
    }
    prescale = PCA9685_Read(0xFE);
    if (prescale < 3) return ARM_INVALID;
    pca_tick_us = (prescale + 1) / 25.0f; /* Nominal 25 MHz oscillator. */
    memcpy(calibration, c, sizeof(calibration));
    enabled = 1;
    return ARM_OK;
}
void ArmControl_Disable(void) { enabled = 0; }
int ArmControl_IsEnabled(void) { return enabled; }
static int convert(unsigned channel, float radians, float *degrees, unsigned *ticks)
{
    const ArmServoCalibration *c = &calibration[channel];
    float us;
    if (!isfinite(radians)) return 0;
    *degrees = c->zero_deg + c->direction * radians * 57.295779513f;
    if (*degrees < c->min_deg || *degrees > c->max_deg) return 0;
    us = c->pulse_min_us + (*degrees / 180) * (c->pulse_max_us - c->pulse_min_us);
    *ticks = (unsigned)(us / pca_tick_us + 0.5f);
    return *ticks > 0 && *ticks < 4096;
}
ArmStatus ArmControl_WriteJoints(const float q[7])
{
    float degrees[7];
    unsigned ticks[7], i;
    if (!enabled || !q) return ARM_INVALID;
    /* Validate the complete command before writing any output. */
    for (i = 0; i < 7; i++)
        if (!convert(i, q[i], &degrees[i], &ticks[i])) return ARM_INVALID;
    for (i = 0; i < 6; i++) PCA9685_SetPWM((uint8_t)i, 0, ticks[i]);
    Servo_SetAngle(SERVO_1, degrees[6]); /* H33 PB14 TIM1_CH2N */
    return ARM_OK;
}
ArmStatus ArmControl_SetCameraPitch(float radians)
{
    float degrees;
    unsigned ticks;
    if (!enabled || !convert(7, radians, &degrees, &ticks)) return ARM_INVALID;
    Servo_SetAngle(SERVO_2, degrees); /* H35 PB15 TIM1_CH3N */
    return ARM_OK;
}
