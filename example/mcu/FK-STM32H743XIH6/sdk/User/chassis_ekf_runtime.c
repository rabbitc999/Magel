#include "chassis_ekf.h"
#include <math.h>
#if defined(__arm__) || defined(__thumb__)
#include "bsp_system.h"
static uint32_t lock_queue(void)
{
    uint32_t p = __get_PRIMASK();
    __disable_irq();
    return p;
}
static void unlock_queue(uint32_t p) { __set_PRIMASK(p); }
#else
static uint32_t lock_queue(void) { return 0; }
static void unlock_queue(uint32_t p) { (void)p; }
#endif
#define SAMPLE_CAPACITY 8
typedef struct
{
    float speed, rate;
    uint64_t timestamp;
    int supported;
} EncoderSample;
static volatile EncoderSample samples[SAMPLE_CAPACITY];
static volatile unsigned queue_read, queue_write, queue_count;
static volatile uint8_t queue_overflow;
#define DEG_TO_RAD      0.01745329251994329577f
#define GYRO_MAX_AGE_US 100000ULL
ChassisEkf g_ChassisEKF;
uint32_t g_ChassisEKF_Epoch;
uint64_t g_ChassisEKF_TimestampUs;
EkfStatus g_ChassisEKF_Status = EKF_INVALID;
uint8_t g_ChassisEKF_Ready;
static uint64_t last_encoder_us, gyro_us;
static float gyro_rate;
static uint8_t initialized, have_encoder, have_gyro, discontinuity;
void ChassisEKF_RuntimeReset(void)
{
    uint32_t key = lock_queue();
    ChassisEkfConfig c;
    ChassisEKF_DefaultConfig(&c);
    g_ChassisEKF_Status = ChassisEKF_Init(&g_ChassisEKF, &c, 0, 0, 0);
    g_ChassisEKF_Epoch++;
    initialized = 1;
    have_encoder = have_gyro = discontinuity = 0;
    g_ChassisEKF_Ready = 0;
    last_encoder_us = gyro_us = g_ChassisEKF_TimestampUs = 0;
    queue_read = queue_write = queue_count = queue_overflow = 0;
    unlock_queue(key);
}
void ChassisEKF_RuntimeGyro(float rate, uint64_t timestamp)
{
    if (!initialized)
    {
        ChassisEkfConfig c;
        ChassisEKF_DefaultConfig(&c);
        ChassisEKF_Init(&g_ChassisEKF, &c, 0, 0, 0);
        initialized = 1;
    }
    if (!isfinite(rate))
    {
        have_gyro = 0;
        return;
    }
    /* Only the latest packet is retained, and is consumed at most once. */
    gyro_rate = rate * DEG_TO_RAD;
    gyro_us = timestamp;
    have_gyro = 1;
}
static void process_encoder(float speed, float rate, uint64_t now, int supported)
{
    EkfStatus status;
    if (!initialized) ChassisEKF_RuntimeReset();
    if (discontinuity || !supported || !isfinite(speed) || !isfinite(rate))
    {
        g_ChassisEKF_Ready = 0;
        g_ChassisEKF_Status = EKF_INVALID;
        discontinuity = 1;
        return;
    }
    if (have_encoder)
    {
        float dt;
        if (now <= last_encoder_us)
        {
            g_ChassisEKF_Ready = 0;
            g_ChassisEKF_Status = EKF_INVALID;
            discontinuity = 1;
            return;
        }
        dt = (now - last_encoder_us) * 1e-6f;
        last_encoder_us = now;
        status = ChassisEKF_Predict(&g_ChassisEKF, dt);
        if (status != EKF_OK)
        {
            g_ChassisEKF_Status = status;
            g_ChassisEKF_Ready = 0;
            discontinuity = 1;
            return;
        }
    }
    else
    {
        last_encoder_us = now;
        have_encoder = 1;
    }
    g_ChassisEKF_TimestampUs = now;
    g_ChassisEKF_Status = ChassisEKF_UpdateEncoder(&g_ChassisEKF, speed, rate * DEG_TO_RAD);
    if (have_gyro && gyro_us <= now)
    {
        if (now - gyro_us <= GYRO_MAX_AGE_US)
        {
            status = ChassisEKF_UpdateGyro(&g_ChassisEKF, gyro_rate);
            if (g_ChassisEKF_Status == EKF_OK) g_ChassisEKF_Status = status;
        }
        have_gyro = 0;
    }
    g_ChassisEKF_Ready = (g_ChassisEKF_Status == EKF_OK);
}

void ChassisEKF_RuntimeEncoder(float speed, float rate, uint64_t now, int supported)
{
    uint32_t key = lock_queue();
    if (queue_count == SAMPLE_CAPACITY)
        queue_overflow = 1;
    else
    {
        samples[queue_write].speed = speed;
        samples[queue_write].rate = rate;
        samples[queue_write].timestamp = now;
        samples[queue_write].supported = supported;
        queue_write = (queue_write + 1) % SAMPLE_CAPACITY;
        queue_count++;
    }
    unlock_queue(key);
}
void ChassisEKF_RuntimePoll(void)
{
    unsigned processed;
    if (!initialized)
    {
        ChassisEkfConfig c;
        ChassisEKF_DefaultConfig(&c);
        ChassisEKF_Init(&g_ChassisEKF, &c, 0, 0, 0);
        initialized = 1;
    }
    /* Bounded work per poll; only this task mutates filter state. */
    for (processed = 0; processed < SAMPLE_CAPACITY; processed++)
    {
        EncoderSample sample;
        uint32_t key = lock_queue();
        if (queue_overflow)
        {
            queue_overflow = 0;
            queue_count = 0;
            queue_read = queue_write;
            unlock_queue(key);
            discontinuity = 1;
            g_ChassisEKF_Status = EKF_INVALID;
            g_ChassisEKF_Ready = 0;
            return;
        }
        if (!queue_count)
        {
            unlock_queue(key);
            return;
        }
        sample = samples[queue_read];
        queue_read = (queue_read + 1) % SAMPLE_CAPACITY;
        queue_count--;
        unlock_queue(key);
        process_encoder(sample.speed, sample.rate, sample.timestamp, sample.supported);
    }
}
