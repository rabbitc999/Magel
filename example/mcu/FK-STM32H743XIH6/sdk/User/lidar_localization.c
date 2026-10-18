#include "lidar_localization.h"
#include <math.h>
#include <string.h>
static float angle(float x) { return remainderf(x, 6.28318530718f); }
static int finite_pose(const float *p) { return p && isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2]); }
static int positive(float x) { return isfinite(x) && x > 0; }
static int config_valid(const LidarLocConfig *c)
{
    return c && positive(c->correspondence_distance) && positive(c->residual_limit) && positive(c->huber_scale) && positive(c->min_inlier_ratio) && c->min_inlier_ratio <= 1 && positive(c->max_rmse) && positive(c->normal_ratio) && c->normal_ratio < 1 && positive(c->min_geometry_ratio) && c->min_geometry_ratio < 1 && positive(c->max_seed_translation) && positive(c->max_seed_yaw) && positive(c->temporal_translation_slack) && positive(c->temporal_yaw_slack) && positive(c->max_update_translation) && positive(c->max_update_yaw) && positive(c->max_nis) && positive(c->position_variance_floor) && positive(c->yaw_variance_floor) && c->min_points >= 6 && c->min_points <= LIDAR_LOC_MAX_POINTS && c->iterations > 0 && c->iterations <= LIDAR_LOC_MAX_ITERATIONS && c->confirmations > 0 && c->confirmations <= 20;
}
void LidarLoc_DefaultConfig(LidarLocConfig *c)
{
    if (!c) return;
    c->correspondence_distance = 0.6f;
    c->residual_limit = 0.25f;
    c->huber_scale = 0.035f;
    c->min_inlier_ratio = 0.6f;
    c->max_rmse = 0.045f;
    c->normal_ratio = 0.15f;
    c->min_geometry_ratio = 0.03f;
    c->max_seed_translation = 0.4f;
    c->max_seed_yaw = 0.25f;
    c->temporal_translation_slack = 0.08f;
    c->temporal_yaw_slack = 0.05f;
    c->max_update_translation = 0.06f;
    c->max_update_yaw = 0.035f;
    c->max_nis = 11.345f;
    c->position_variance_floor = 0.0004f;
    c->yaw_variance_floor = 0.000076154f;
    c->min_points = 40;
    c->iterations = LIDAR_LOC_MAX_ITERATIONS;
    c->confirmations = 3;
}
void LidarLoc_Init(LidarLocalizer *l, const LidarLocConfig *c)
{
    LidarLocConfig copy;
    if (!l) return;
    if (c)
        copy = *c;
    else
        LidarLoc_DefaultConfig(&copy);
    memset(l, 0, sizeof(*l));
    l->config = copy;
}
static float coord(const LidarLocalizer *l, unsigned index, unsigned axis)
{
    return axis ? l->reference[index].y : l->reference[index].x;
}
/* Balanced implicit KD tree. Fixed memory; insertion-sort construction runs only on reference setup. */
static void build(LidarLocalizer *l, int start, int end, unsigned depth)
{
    int i, j, mid;
    unsigned axis = depth % 2;
    if (start >= end) return;
    for (i = start + 1; i <= end; i++)
    {
        uint16_t value = l->order[i];
        j = i - 1;
        while (j >= start && coord(l, l->order[j], axis) > coord(l, value, axis))
        {
            l->order[j + 1] = l->order[j];
            j--;
        }
        l->order[j + 1] = value;
    }
    mid = (start + end) / 2;
    build(l, start, mid - 1, depth + 1);
    build(l, mid + 1, end, depth + 1);
}
typedef struct
{
    float distance[5];
    unsigned index[5], count, capacity;
} Neighbors;
static void search(const LidarLocalizer *l, int start, int end, unsigned depth, float x, float y, Neighbors *n)
{
    int mid;
    unsigned id, i;
    float dx, dy, d, split, limit;
    if (start > end) return;
    mid = (start + end) / 2;
    id = l->order[mid];
    dx = x - l->reference[id].x;
    dy = y - l->reference[id].y;
    d = dx * dx + dy * dy;
    if (n->count < n->capacity || d < n->distance[n->count - 1])
    {
        i = n->count < n->capacity ? n->count++ : n->count - 1;
        while (i > 0 && n->distance[i - 1] > d)
        {
            n->distance[i] = n->distance[i - 1];
            n->index[i] = n->index[i - 1];
            i--;
        }
        n->distance[i] = d;
        n->index[i] = id;
    }
    split = depth % 2 ? dy : dx;
    if (split < 0)
        search(l, start, mid - 1, depth + 1, x, y, n);
    else
        search(l, mid + 1, end, depth + 1, x, y, n);
    limit = n->count < n->capacity ? INFINITY : n->distance[n->count - 1];
    if (split * split <= limit)
    {
        if (split < 0)
            search(l, mid + 1, end, depth + 1, x, y, n);
        else
            search(l, start, mid - 1, depth + 1, x, y, n);
    }
}
static void normal(LidarLocalizer *l, unsigned id)
{
    Neighbors n = { { 0 }, { 0 }, 0, 5 };
    unsigned i;
    float mx = 0, my = 0, xx = 0, xy = 0, yy = 0, trace, disc, theta;
    search(l, 0, (int)l->reference_count - 1, 0, l->reference[id].x, l->reference[id].y, &n);
    if (n.count < 5 || n.distance[4] > 0.36f) return;
    for (i = 0; i < n.count; i++)
    {
        mx += l->reference[n.index[i]].x;
        my += l->reference[n.index[i]].y;
    }
    mx /= n.count;
    my /= n.count;
    for (i = 0; i < n.count; i++)
    {
        float x = l->reference[n.index[i]].x - mx, y = l->reference[n.index[i]].y - my;
        xx += x * x;
        xy += x * y;
        yy += y * y;
    }
    trace = xx + yy;
    disc = sqrtf((xx - yy) * (xx - yy) + 4 * xy * xy);
    if (trace < 1e-6f || (trace - disc) / (trace + disc) > l->config.normal_ratio) return;
    theta = 0.5f * atan2f(2 * xy, xx - yy);
    l->normals[id].x = -sinf(theta);
    l->normals[id].y = cosf(theta);
}
LidarLocStatus LidarLoc_SetReference(LidarLocalizer *l, const LidarLocPoint *p, unsigned count, const float pose[3])
{
    unsigned i, usable = 0;
    float c, s;
    if (!l || !p || !finite_pose(pose) || !config_valid(&l->config) || count > LIDAR_LOC_MAX_POINTS) return LIDAR_LOC_INVALID;
    /* Clear stale reference before any failure. */
    l->reference_count = 0;
    l->consecutive = 0;
    l->last_scan_us = l->last_good_us = 0;
    if (count < l->config.min_points) return LIDAR_LOC_TOO_FEW;
    c = cosf(pose[2]);
    s = sinf(pose[2]);
    for (i = 0; i < count; i++)
        if (!isfinite(p[i].x) || !isfinite(p[i].y) || hypotf(p[i].x, p[i].y) > 50) return LIDAR_LOC_INVALID;
    for (i = 0; i < count; i++)
    {
        l->reference[i].x = pose[0] + c * p[i].x - s * p[i].y;
        l->reference[i].y = pose[1] + s * p[i].x + c * p[i].y;
        l->normals[i].x = l->normals[i].y = 0;
        l->order[i] = (uint16_t)i;
    }
    l->reference_count = count;
    build(l, 0, (int)count - 1, 0);
    for (i = 0; i < count; i++)
    {
        normal(l, i);
        if (hypotf(l->normals[i].x, l->normals[i].y) > 0.5f) usable++;
    }
    if (usable < l->config.min_points)
    {
        l->reference_count = 0;
        return LIDAR_LOC_DEGENERATE;
    }
    return LIDAR_LOC_REFERENCE;
}
static int inverse3(const float h[3][3], float inv[3][3])
{
    float a[3][6];
    unsigned i, j, k;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 6; j++) a[i][j] = j < 3 ? h[i][j] : (j - 3 == i ? 1.0f : 0);
    for (i = 0; i < 3; i++)
    {
        unsigned pivot = i;
        float scale;
        for (j = i + 1; j < 3; j++)
            if (fabsf(a[j][i]) > fabsf(a[pivot][i])) pivot = j;
        if (!isfinite(a[pivot][i]) || fabsf(a[pivot][i]) < 1e-6f) return 0;
        for (k = 0; k < 6; k++)
        {
            float tmp = a[i][k];
            a[i][k] = a[pivot][k];
            a[pivot][k] = tmp;
        }
        scale = a[i][i];
        for (k = 0; k < 6; k++) a[i][k] /= scale;
        for (j = 0; j < 3; j++)
            if (j != i)
            {
                scale = a[j][i];
                for (k = 0; k < 6; k++) a[j][k] -= scale * a[i][k];
            }
    }
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) inv[i][j] = a[i][j + 3];
    return 1;
}
static float geometry(const float h[3][3])
{
    float trace = h[0][0] + h[1][1], disc = sqrtf((h[0][0] - h[1][1]) * (h[0][0] - h[1][1]) + 4 * h[0][1] * h[0][1]);
    float det = h[0][0] * h[1][1] - h[0][1] * h[1][0], schur;
    if (trace <= 0 || det <= 1e-6f || h[2][2] <= 1e-6f) return 0;
    schur = h[2][2] - (h[0][2] * h[0][2] * h[1][1] - 2 * h[0][2] * h[1][2] * h[0][1] + h[1][2] * h[1][2] * h[0][0]) / det;
    return fminf((trace - disc) / (trace + disc), schur / h[2][2]);
}
static unsigned accumulate(const LidarLocalizer *l, const LidarLocPoint *p, unsigned count,
                           const float pose[3], float h[3][3], float b[3], float *squared, int final)
{
    unsigned i, j, k, n = 0;
    float c = cosf(pose[2]), s = sinf(pose[2]);
    memset(h, 0, 9 * sizeof(float));
    memset(b, 0, 3 * sizeof(float));
    *squared = 0;
    for (i = 0; i < count; i++)
    {
        Neighbors near = { { 0 }, { 0 }, 0, 1 };
        LidarLocPoint normal, ref;
        float rx, ry, x, y, r, w, J[3];
        rx = c * p[i].x - s * p[i].y;
        ry = s * p[i].x + c * p[i].y;
        x = pose[0] + rx;
        y = pose[1] + ry;
        search(l, 0, (int)l->reference_count - 1, 0, x, y, &near);
        if (!near.count || near.distance[0] > l->config.correspondence_distance * l->config.correspondence_distance) continue;
        normal = l->normals[near.index[0]];
        ref = l->reference[near.index[0]];
        if (normal.x * normal.x + normal.y * normal.y < 0.5f) continue;
        r = normal.x * (x - ref.x) + normal.y * (y - ref.y);
        if (fabsf(r) > (final ? l->config.max_rmse * 2 : l->config.residual_limit)) continue;
        w = fabsf(r) <= l->config.huber_scale ? 1 : l->config.huber_scale / fabsf(r);
        J[0] = normal.x;
        J[1] = normal.y;
        J[2] = -normal.x * ry + normal.y * rx;
        for (j = 0; j < 3; j++)
        {
            b[j] += w * J[j] * r;
            for (k = 0; k < 3; k++) h[j][k] += w * J[j] * J[k];
        }
        *squared += r * r;
        n++;
    }
    return n;
}
LidarLocStatus LidarLoc_Match(const LidarLocalizer *l, const LidarLocPoint *p, unsigned count, const float seed[3], LidarLocResult *out)
{
    float pose[3], h[3][3], b[3], inverse[3][3], squared = 0;
    unsigned iteration, i, j, n = 0;
    int converged = 0;
    if (!out) return LIDAR_LOC_INVALID;
    memset(out, 0, sizeof(*out));
    out->status = LIDAR_LOC_INVALID;
    if (!l || !p || !finite_pose(seed) || !config_valid(&l->config) || count > LIDAR_LOC_MAX_POINTS || l->reference_count > LIDAR_LOC_MAX_POINTS) return out->status;
    if (count < l->config.min_points || l->reference_count < l->config.min_points) return out->status = LIDAR_LOC_TOO_FEW;
    for (i = 0; i < count; i++)
        if (!isfinite(p[i].x) || !isfinite(p[i].y) || hypotf(p[i].x, p[i].y) > 50) return out->status;
    memcpy(pose, seed, sizeof(pose));
    for (iteration = 0; iteration < l->config.iterations; iteration++)
    {
        float step[3] = { 0 }, scale = 1, translation;
        n = accumulate(l, p, count, pose, h, b, &squared, 0);
        if (n < l->config.min_points) return out->status = LIDAR_LOC_LOW_OVERLAP;
        out->geometry_ratio = geometry(h);
        if (out->geometry_ratio < l->config.min_geometry_ratio || !inverse3(h, inverse)) return out->status = LIDAR_LOC_DEGENERATE;
        for (i = 0; i < 3; i++)
            for (j = 0; j < 3; j++) step[i] -= inverse[i][j] * b[j];
        translation = hypotf(step[0], step[1]);
        if (translation > 0.12f) scale = fminf(scale, 0.12f / translation);
        if (fabsf(step[2]) > 0.05f) scale = fminf(scale, 0.05f / fabsf(step[2]));
        for (i = 0; i < 3; i++) pose[i] += step[i] * scale;
        pose[2] = angle(pose[2]);
        if (translation < 0.0005f && fabsf(step[2]) < 0.0003f)
        {
            converged = 1;
            break;
        }
    }
    memcpy(out->pose, pose, sizeof(pose));
    n = accumulate(l, p, count, pose, h, b, &squared, 1);
    out->inliers = n;
    out->unmatched = count - n;
    out->inlier_ratio = (float)n / count;
    out->rmse = n ? sqrtf(squared / n) : INFINITY;
    out->geometry_ratio = geometry(h);
    if (n < l->config.min_points || out->inlier_ratio < l->config.min_inlier_ratio) return out->status = LIDAR_LOC_LOW_OVERLAP;
    if (out->rmse > l->config.max_rmse) return out->status = LIDAR_LOC_NOISY;
    if (out->geometry_ratio < l->config.min_geometry_ratio || !inverse3(h, inverse)) return out->status = LIDAR_LOC_DEGENERATE;
    if (!converged) return out->status = LIDAR_LOC_NOT_CONVERGED;
    if (hypotf(pose[0] - seed[0], pose[1] - seed[1]) > l->config.max_seed_translation || fabsf(angle(pose[2] - seed[2])) > l->config.max_seed_yaw) return out->status = LIDAR_LOC_JUMP;
    {
        /* Conservative covariance: account for correlated rays and outliers. */
        float variance = fmaxf(squared / fmaxf((float)n - 3, 1), 0.0001f) * 50 / (out->inlier_ratio * out->inlier_ratio);
        for (i = 0; i < 3; i++)
            for (j = 0; j < 3; j++) out->covariance[i][j] = 0.5f * (inverse[i][j] + inverse[j][i]) * variance;
        out->covariance[0][0] += l->config.position_variance_floor;
        out->covariance[1][1] += l->config.position_variance_floor;
        out->covariance[2][2] += l->config.yaw_variance_floor;
    }
    return out->status = LIDAR_LOC_OK;
}
LidarLocStatus LidarLoc_Correct(LidarLocalizer *l, const LidarLocPoint *p, unsigned count, uint64_t scan_us,
                                uint64_t state_us, ChassisEkf *f, LidarLocResult *out)
{
    LidarLocStatus status;
    unsigned attempt;
    if (!l || !f || !out) return LIDAR_LOC_INVALID;
    memset(out, 0, sizeof(*out));
    out->status = LIDAR_LOC_INVALID;
    if (!config_valid(&l->config)) return out->status;
    for (attempt = 0; attempt < EKF_N; attempt++)
        if (!isfinite(f->state[attempt])) return out->status;
    if (scan_us <= l->last_scan_us || scan_us > state_us || state_us - scan_us > 100000ULL) return out->status = LIDAR_LOC_STALE;
    l->last_scan_us = scan_us;
    if (!l->reference_count)
    {
        status = LidarLoc_SetReference(l, p, count, f->state);
        l->last_scan_us = scan_us;
        return out->status = status;
    }
    status = LidarLoc_Match(l, p, count, f->state, out);
    if (status != LIDAR_LOC_OK)
    {
        l->consecutive = 0;
        return status;
    }
    if (l->last_good_us && scan_us - l->last_good_us <= 500000ULL)
    {
        float dt = (scan_us - l->last_good_us) * 1e-6f;
        float yaw = l->last_good_pose[2] + 0.5f * f->state[EKF_W] * dt;
        float px = l->last_good_pose[0] + f->state[EKF_V] * cosf(yaw) * dt;
        float py = l->last_good_pose[1] + f->state[EKF_V] * sinf(yaw) * dt;
        float heading = angle(l->last_good_pose[2] + f->state[EKF_W] * dt);
        if (hypotf(out->pose[0] - px, out->pose[1] - py) > l->config.temporal_translation_slack || fabsf(angle(out->pose[2] - heading)) > l->config.temporal_yaw_slack)
        {
            l->consecutive = 0;
            return out->status = LIDAR_LOC_JUMP;
        }
    }
    else
        l->consecutive = 0;
    memcpy(l->last_good_pose, out->pose, sizeof(out->pose));
    l->last_good_us = scan_us;
    if (l->consecutive < l->config.confirmations) l->consecutive++;
    if (l->consecutive < l->config.confirmations) return out->status = LIDAR_LOC_WARMUP;
    /* Keep NIS test based on original measurement covariance, before softening gain. */
    {
        ChassisEkf candidate = *f;
        float R[3][3];
        EkfStatus ekf;
        memcpy(R, out->covariance, sizeof(R));
        ekf = ChassisEKF_UpdatePose(&candidate, out->pose, R, l->config.max_nis);
        if (ekf != EKF_OK) return out->status = LIDAR_LOC_EKF_REJECTED;
        for (attempt = 0; attempt < 7; attempt++)
        {
            unsigned i, j;
            if (hypotf(candidate.state[0] - f->state[0], candidate.state[1] - f->state[1]) <= l->config.max_update_translation && fabsf(angle(candidate.state[2] - f->state[2])) <= l->config.max_update_yaw)
            {
                *f = candidate;
                return out->status = LIDAR_LOC_OK;
            }
            for (i = 0; i < 3; i++)
                for (j = 0; j < 3; j++) R[i][j] *= 4;
            candidate = *f;
            if (ChassisEKF_UpdatePose(&candidate, out->pose, R, l->config.max_nis) != EKF_OK) break;
        }
    }
    return out->status = LIDAR_LOC_EKF_REJECTED;
}
