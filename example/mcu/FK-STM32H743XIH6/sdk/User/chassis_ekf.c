#include "chassis_ekf.h"
#include <math.h>
#include <string.h>
#define PI_F 3.14159265358979323846f
static float wrap(float a) { return remainderf(a, 2 * PI_F); }
static int positive(float x) { return isfinite(x) && x > 0; }
static int config_valid(const ChassisEkfConfig *c)
{
    return c && positive(c->acceleration_noise) && positive(c->angular_acceleration_noise) && positive(c->bias_walk_noise) && positive(c->encoder_speed_variance) && positive(c->encoder_rate_variance) && positive(c->gyro_variance) && positive(c->gate_sigma) && positive(c->max_dt) && c->max_dt <= 1;
}
static int valid(const ChassisEkf *f)
{
    unsigned i, j;
    if (!f || !config_valid(&f->config)) return 0;
    for (i = 0; i < EKF_N; i++)
    {
        if (!isfinite(f->state[i]) || f->covariance[i][i] < 0) return 0;
        for (j = 0; j < EKF_N; j++)
            if (!isfinite(f->covariance[i][j])) return 0;
    }
    return 1;
}
void ChassisEKF_DefaultConfig(ChassisEkfConfig *c)
{
    if (!c) return;
    c->acceleration_noise = 1;
    c->angular_acceleration_noise = 1;
    c->bias_walk_noise = 0.000001f;
    c->encoder_speed_variance = 0.0025f;
    c->encoder_rate_variance = 0.01f;
    c->gyro_variance = 0.0004f;
    c->gate_sigma = 5;
    c->max_dt = 0.2f;
}
EkfStatus ChassisEKF_Init(ChassisEkf *f, const ChassisEkfConfig *c, float x, float y, float yaw)
{
    const float diagonal[6] = { 1, 1, 0.25f, 1, 1, 0.01f };
    unsigned i;
    if (!f || !config_valid(c) || !isfinite(x) || !isfinite(y) || !isfinite(yaw)) return EKF_INVALID;
    {
        ChassisEkfConfig copy = *c;
        memset(f, 0, sizeof(*f));
        f->config = copy;
    }
    f->state[0] = x;
    f->state[1] = y;
    f->state[2] = wrap(yaw);
    for (i = 0; i < EKF_N; i++) f->covariance[i][i] = diagonal[i];
    return EKF_OK;
}
EkfStatus ChassisEKF_Predict(ChassisEkf *f, float dt)
{
    ChassisEkf n;
    float F[6][6] = { { 0 } }, tmp[6][6] = { { 0 } };
    float theta, c, s, v;
    unsigned i, j, k;
    if (!valid(f) || !positive(dt) || dt > f->config.max_dt) return EKF_INVALID;
    n = *f;
    v = f->state[EKF_V];
    theta = f->state[EKF_YAW] + 0.5f * f->state[EKF_W] * dt;
    c = cosf(theta);
    s = sinf(theta);
    n.state[0] += v * c * dt;
    n.state[1] += v * s * dt;
    n.state[2] = wrap(n.state[2] + n.state[EKF_W] * dt);
    for (i = 0; i < 6; i++) F[i][i] = 1;
    F[0][2] = -v * s * dt;
    F[1][2] = v * c * dt;
    F[0][3] = c * dt;
    F[1][3] = s * dt;
    F[0][4] = -0.5f * v * s * dt * dt;
    F[1][4] = 0.5f * v * c * dt * dt;
    F[2][4] = dt;
    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++)
            for (k = 0; k < 6; k++) tmp[i][j] += F[i][k] * f->covariance[k][j];
    memset(n.covariance, 0, sizeof(n.covariance));
    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++)
            for (k = 0; k < 6; k++) n.covariance[i][j] += tmp[i][k] * F[j][k];
    /* Continuous white acceleration: exact leading position/velocity blocks.
     * Noise fields are spectral densities, not per-sample variances. */
    {
        float q = f->config.acceleration_noise, a[6] = { c, s, 0, 0, 0, 0 };
        float r = f->config.angular_acceleration_noise;
        for (i = 0; i < 2; i++)
            for (j = 0; j < 2; j++) n.covariance[i][j] += q * a[i] * a[j] * dt * dt * dt / 3;
        for (i = 0; i < 2; i++)
        {
            n.covariance[i][3] += q * a[i] * dt * dt / 2;
            n.covariance[3][i] += q * a[i] * dt * dt / 2;
        }
        n.covariance[3][3] += q * dt;
        n.covariance[2][2] += r * dt * dt * dt / 3;
        n.covariance[2][4] += r * dt * dt / 2;
        n.covariance[4][2] += r * dt * dt / 2;
        n.covariance[4][4] += r * dt;
        n.covariance[5][5] += f->config.bias_walk_noise * dt;
    }
    if (!valid(&n)) return EKF_INVALID;
    *f = n;
    return EKF_OK;
}
static EkfStatus observe(ChassisEkf *f, const float h[6], float z, float r, int angular)
{
    ChassisEkf n;
    float ph[6] = { 0 }, gain[6], A[6][6], tmp[6][6] = { { 0 } }, innovation = z, S = r;
    unsigned i, j, k;
    if (!valid(f) || !isfinite(z) || !positive(r)) return EKF_INVALID;
    for (i = 0; i < 6; i++)
    {
        innovation -= h[i] * f->state[i];
        for (j = 0; j < 6; j++) ph[i] += f->covariance[i][j] * h[j];
    }
    if (angular) innovation = wrap(innovation);
    for (i = 0; i < 6; i++) S += h[i] * ph[i];
    if (!positive(S)) return EKF_INVALID;
    if (fabsf(innovation) / sqrtf(S) > f->config.gate_sigma) return EKF_REJECTED;
    n = *f;
    for (i = 0; i < 6; i++)
    {
        gain[i] = ph[i] / S;
        n.state[i] += gain[i] * innovation;
        for (j = 0; j < 6; j++) A[i][j] = (i == j ? 1.0f : 0) - gain[i] * h[j];
    }
    n.state[2] = wrap(n.state[2]);
    /* Joseph form preserves positive semidefiniteness better in float. */
    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++)
            for (k = 0; k < 6; k++) tmp[i][j] += A[i][k] * f->covariance[k][j];
    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++)
        {
            n.covariance[i][j] = gain[i] * r * gain[j];
            for (k = 0; k < 6; k++) n.covariance[i][j] += tmp[i][k] * A[j][k];
        }
    for (i = 0; i < 6; i++)
        for (j = 0; j < i; j++)
        {
            float mean = (n.covariance[i][j] + n.covariance[j][i]) * 0.5f;
            n.covariance[i][j] = n.covariance[j][i] = mean;
        }
    if (!valid(&n)) return EKF_INVALID;
    *f = n;
    return EKF_OK;
}
EkfStatus ChassisEKF_UpdateEncoder(ChassisEkf *f, float speed, float rate)
{
    const float hv[6] = { 0, 0, 0, 1, 0, 0 }, hw[6] = { 0, 0, 0, 0, 1, 0 };
    ChassisEkf n;
    EkfStatus status;
    if (!valid(f) || !isfinite(speed) || !isfinite(rate)) return EKF_INVALID;
    n = *f;
    status = observe(&n, hv, speed, n.config.encoder_speed_variance, 0);
    if (status == EKF_OK) status = observe(&n, hw, rate, n.config.encoder_rate_variance, 0);
    if (status == EKF_OK) *f = n;
    return status;
}
EkfStatus ChassisEKF_UpdateGyro(ChassisEkf *f, float rate)
{
    const float h[6] = { 0, 0, 0, 0, 1, 1 };
    if (!f) return EKF_INVALID;
    return observe(f, h, rate, f->config.gyro_variance, 0);
}
EkfStatus ChassisEKF_UpdatePosition(ChassisEkf *f, float x, float y, float rx, float ry)
{
    const float hx[6] = { 1, 0, 0, 0, 0, 0 }, hy[6] = { 0, 1, 0, 0, 0, 0 };
    ChassisEkf n;
    EkfStatus status;
    if (!valid(f) || !isfinite(x) || !isfinite(y) || !positive(rx) || !positive(ry)) return EKF_INVALID;
    n = *f;
    status = observe(&n, hx, x, rx, 0);
    if (status == EKF_OK) status = observe(&n, hy, y, ry, 0);
    if (status == EKF_OK) *f = n;
    return status;
}
EkfStatus ChassisEKF_UpdateHeading(ChassisEkf *f, float yaw, float variance)
{
    const float h[6] = { 0, 0, 1, 0, 0, 0 };
    return observe(f, h, yaw, variance, 1);
}

/* Invert a symmetric positive definite 3x3 matrix by Cholesky solves. */
static int inverse_spd3(const float a[3][3], float inv[3][3])
{
    float L[3][3] = { { 0 } };
    unsigned i, j, k, col;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            if (!isfinite(a[i][j]) || fabsf(a[i][j] - a[j][i]) > 1e-5f) return 0;
    for (i = 0; i < 3; i++)
        for (j = 0; j <= i; j++)
        {
            float sum = a[i][j];
            for (k = 0; k < j; k++) sum -= L[i][k] * L[j][k];
            if (i == j)
            {
                if (!positive(sum)) return 0;
                L[i][j] = sqrtf(sum);
            }
            else
                L[i][j] = sum / L[j][j];
        }
    for (col = 0; col < 3; col++)
    {
        float y[3], x[3];
        int row;
        for (i = 0; i < 3; i++)
        {
            float sum = i == col ? 1 : 0;
            for (k = 0; k < i; k++) sum -= L[i][k] * y[k];
            y[i] = sum / L[i][i];
        }
        for (row = 2; row >= 0; row--)
        {
            float sum = y[row];
            for (k = (unsigned)row + 1; k < 3; k++) sum -= L[k][row] * x[k];
            x[row] = sum / L[row][row];
        }
        for (i = 0; i < 3; i++) inv[i][col] = x[i];
    }
    return 1;
}
EkfStatus ChassisEKF_UpdatePose(ChassisEkf *f, const float pose[3], const float R[3][3], float max_nis)
{
    ChassisEkf n;
    float S[3][3], inverse[3][3], unused[3][3], e[3], nis = 0;
    float K[6][3] = { { 0 } }, A[6][6], tmp[6][6] = { { 0 } };
    unsigned i, j, k, l;
    if (!valid(f) || !pose || !R || !positive(max_nis) || !inverse_spd3(R, unused)) return EKF_INVALID;
    for (i = 0; i < 3; i++)
    {
        if (!isfinite(pose[i])) return EKF_INVALID;
        e[i] = pose[i] - f->state[i];
        for (j = 0; j < 3; j++) S[i][j] = f->covariance[i][j] + R[i][j];
    }
    e[2] = wrap(e[2]);
    if (!inverse_spd3(S, inverse)) return EKF_INVALID;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) nis += e[i] * inverse[i][j] * e[j];
    if (!isfinite(nis)) return EKF_INVALID;
    if (nis > max_nis) return EKF_REJECTED;
    n = *f;
    for (i = 0; i < 6; i++)
    {
        for (j = 0; j < 3; j++)
            for (k = 0; k < 3; k++) K[i][j] += f->covariance[i][k] * inverse[k][j];
        for (j = 0; j < 3; j++) n.state[i] += K[i][j] * e[j];
        for (j = 0; j < 6; j++) A[i][j] = (i == j ? 1.0f : 0) - (j < 3 ? K[i][j] : 0);
    }
    n.state[2] = wrap(n.state[2]);
    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++)
            for (k = 0; k < 6; k++) tmp[i][j] += A[i][k] * f->covariance[k][j];
    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++)
        {
            n.covariance[i][j] = 0;
            for (k = 0; k < 6; k++) n.covariance[i][j] += tmp[i][k] * A[j][k];
            for (k = 0; k < 3; k++)
                for (l = 0; l < 3; l++) n.covariance[i][j] += K[i][k] * R[k][l] * K[j][l];
        }
    for (i = 0; i < 6; i++)
        for (j = 0; j < i; j++)
        {
            float mean = (n.covariance[i][j] + n.covariance[j][i]) * 0.5f;
            n.covariance[i][j] = n.covariance[j][i] = mean;
        }
    if (!valid(&n)) return EKF_INVALID;
    *f = n;
    return EKF_OK;
}
