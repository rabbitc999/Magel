#include "arm_kinematics.h"
#include <math.h>
#include <string.h>
static int valid(const ArmModel *m, const float *q)
{
    unsigned i;
    if (!m || !q) return 0;
    for (i = 0; i < ARM_DOF; i++)
    {
        const ArmJoint *j = &m->joint[i];
        if (!isfinite(j->a) || !isfinite(j->d) || !isfinite(j->alpha) || !isfinite(j->offset) || !isfinite(j->min) || !isfinite(j->max) || j->min > j->max || !isfinite(q[i]) || q[i] < j->min || q[i] > j->max) return 0;
    }
    return 1;
}
static void forward(const ArmModel *m, const float *q, ArmPose *p, float jac[6][7])
{
    float t[4][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
    float origins[7][3], axes[7][3];
    unsigned i, x, y, k;
    for (i = 0; i < 7; i++)
    {
        const ArmJoint *j = &m->joint[i];
        float c = cosf(q[i] + j->offset), s = sinf(q[i] + j->offset);
        float ca = cosf(j->alpha), sa = sinf(j->alpha);
        float a[4][4] = { { c, -s * ca, s * sa, j->a * c }, { s, c * ca, -c * sa, j->a * s }, { 0, sa, ca, j->d }, { 0, 0, 0, 1 } }, n[4][4] = { { 0 } };
        for (x = 0; x < 3; x++)
        {
            origins[i][x] = t[x][3];
            axes[i][x] = t[x][2];
        }
        for (x = 0; x < 4; x++)
            for (y = 0; y < 4; y++)
                for (k = 0; k < 4; k++) n[x][y] += t[x][k] * a[k][y];
        memcpy(t, n, sizeof(t));
    }
    for (x = 0; x < 3; x++)
    {
        p->p[x] = t[x][3];
        for (y = 0; y < 3; y++) p->r[x][y] = t[x][y];
    }
    if (jac)
        for (i = 0; i < 7; i++)
            for (x = 0; x < 3; x++)
            {
                unsigned u = (x + 1) % 3, v = (x + 2) % 3;
                jac[x][i] = axes[i][u] * (p->p[v] - origins[i][v]) - axes[i][v] * (p->p[u] - origins[i][u]);
                jac[x + 3][i] = axes[i][x];
            }
}
ArmStatus Arm_Forward(const ArmModel *m, const float q[7], ArmPose *p)
{
    if (!p || !valid(m, q)) return ARM_INVALID;
    forward(m, q, p, 0);
    return ARM_OK;
}
static int rotation_valid(const ArmPose *p)
{
    unsigned i, j, k;
    float det;
    for (i = 0; i < 3; i++)
    {
        if (!isfinite(p->p[i])) return 0;
        for (j = 0; j < 3; j++)
            if (!isfinite(p->r[i][j])) return 0;
    }
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
        {
            float dot = 0;
            for (k = 0; k < 3; k++) dot += p->r[k][i] * p->r[k][j];
            if (fabsf(dot - (i == j ? 1.0f : 0.0f)) > 0.001f) return 0;
        }
    det = p->r[0][0] * (p->r[1][1] * p->r[2][2] - p->r[1][2] * p->r[2][1])
          - p->r[0][1] * (p->r[1][0] * p->r[2][2] - p->r[1][2] * p->r[2][0])
          + p->r[0][2] * (p->r[1][0] * p->r[2][1] - p->r[1][1] * p->r[2][0]);
    return fabsf(det - 1) < 0.001f;
}
/* SO(3) logarithm, including rotations close to pi. */
static void rotation_error(const ArmPose *a, const ArmPose *b, float *e)
{
    float r[3][3] = { { 0 } }, c, angle, s;
    unsigned i, j, k;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            for (k = 0; k < 3; k++) r[i][j] += a->r[i][k] * b->r[j][k];
    c = (r[0][0] + r[1][1] + r[2][2] - 1) * 0.5f;
    c = fmaxf(-1, fminf(1, c));
    angle = acosf(c);
    if (angle > 3.13f)
    {
        unsigned h = 0;
        float axis[3];
        if (r[1][1] > r[h][h]) h = 1;
        if (r[2][2] > r[h][h]) h = 2;
        axis[h] = sqrtf(fmaxf(0, (r[h][h] + 1) * 0.5f));
        for (i = 0; i < 3; i++)
            if (i != h) axis[i] = (r[i][h] + r[h][i]) / (4 * axis[h]);
        s = axis[0] * (r[2][1] - r[1][2]) + axis[1] * (r[0][2] - r[2][0]) + axis[2] * (r[1][0] - r[0][1]);
        for (i = 0; i < 3; i++) e[i] = angle * axis[i] * (s < 0 ? -1 : 1);
    }
    else
    {
        s = angle < 0.0001f ? 0.5f : angle / (2 * sinf(angle));
        e[0] = s * (r[2][1] - r[1][2]);
        e[1] = s * (r[0][2] - r[2][0]);
        e[2] = s * (r[1][0] - r[0][1]);
    }
}
ArmStatus Arm_Inverse(const ArmModel *m, const ArmPose *target, const float seed[7], float result[7])
{
    float q[7];
    unsigned iteration, i, j, k;
    if (!target || !result || !valid(m, seed) || !rotation_valid(target)) return ARM_INVALID;
    if (!isfinite(m->damping) || m->damping <= 0 || !isfinite(m->orientation_scale) || m->orientation_scale <= 0 || !isfinite(m->position_tolerance) || m->position_tolerance <= 0 || !isfinite(m->rotation_tolerance) || m->rotation_tolerance <= 0 || !isfinite(m->max_step) || m->max_step <= 0 || !m->iterations) return ARM_INVALID;
    memcpy(q, seed, sizeof(q));
    for (iteration = 0; iteration <= m->iterations; iteration++)
    {
        ArmPose current;
        float jac[6][7], e[6], a[6][7], pe = 0, re = 0, dq[7], largest = 0;
        forward(m, q, &current, jac);
        rotation_error(target, &current, e + 3);
        for (i = 0; i < 3; i++)
        {
            e[i] = target->p[i] - current.p[i];
            pe += e[i] * e[i];
            re += e[i + 3] * e[i + 3];
        }
        if (sqrtf(pe) <= m->position_tolerance && sqrtf(re) <= m->rotation_tolerance)
        {
            memcpy(result, q, sizeof(q));
            return ARM_OK;
        }
        if (iteration == m->iterations) break;
        for (i = 3; i < 6; i++)
        {
            e[i] *= m->orientation_scale;
            for (j = 0; j < 7; j++) jac[i][j] *= m->orientation_scale;
        }
        for (i = 0; i < 6; i++)
        {
            for (j = 0; j < 6; j++)
            {
                a[i][j] = i == j ? m->damping * m->damping : 0;
                for (k = 0; k < 7; k++) a[i][j] += jac[i][k] * jac[j][k];
            }
            a[i][6] = e[i];
        }
        for (i = 0; i < 6; i++)
        {
            unsigned pivot = i;
            float v;
            for (j = i + 1; j < 6; j++)
                if (fabsf(a[j][i]) > fabsf(a[pivot][i])) pivot = j;
            if (!isfinite(a[pivot][i]) || fabsf(a[pivot][i]) < 1e-12f) return ARM_NO_CONVERGENCE;
            for (k = i; k < 7; k++)
            {
                v = a[i][k];
                a[i][k] = a[pivot][k];
                a[pivot][k] = v;
            }
            v = a[i][i];
            for (k = i; k < 7; k++) a[i][k] /= v;
            for (j = 0; j < 6; j++)
                if (j != i)
                {
                    v = a[j][i];
                    for (k = i; k < 7; k++) a[j][k] -= v * a[i][k];
                }
        }
        for (i = 0; i < 7; i++)
        {
            dq[i] = 0;
            for (j = 0; j < 6; j++) dq[i] += jac[j][i] * a[j][6];
            if (!isfinite(dq[i])) return ARM_NO_CONVERGENCE;
            largest = fmaxf(largest, fabsf(dq[i]));
        }
        for (i = 0; i < 7; i++) q[i] = fmaxf(m->joint[i].min, fminf(m->joint[i].max, q[i] + dq[i] * (largest > m->max_step ? m->max_step / largest : 1)));
    }
    return ARM_NO_CONVERGENCE;
}
