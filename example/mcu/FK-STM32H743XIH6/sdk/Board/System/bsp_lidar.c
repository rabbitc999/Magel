#include "bsp_system.h"

#include <stdint.h>
#include <string.h>
#include <math.h>

#include "bsp_lidar.h"

/* 注意的点：雷达左边为0-180度，右边为180-360度，但是需要注意右手定则，也就是把左边变为180-360度，右边变为0-180度 */

LidarState_T g_LidarState = {
    .currentLidarType = LIDAR_TYPE_UNKNOWN,
    .outputBufferReady = 0,
    .processingBuffer = { .count = 0, .type = LIDAR_TYPE_UNKNOWN },
    .outputBuffer = { .count = 0, .type = LIDAR_TYPE_UNKNOWN }
};

/* YDLidar X2 角度修正表 */
static const float s_AngleCorrectionTable[] = {
    79.382998f, 57.367742f, 39.475163f, 28.014059f, 20.599788f, 15.566605f, 11.977174f, 9.307485f, 7.252401f, 5.625466f, 4.307475f, 3.219134f, 2.305847f, 1.528893f, 0.860098f, 0.278495f, -0.231822f, -0.683136f, -1.085073f, -1.445281f,
    -1.769915f, -2.063977f, -2.331579f, -2.576128f, -2.800472f, -3.007010f, -3.197776f, -3.374506f, -3.538692f, -3.691620f, -3.834408f, -3.968031f, -4.093343f, -4.211094f, -4.321948f, -4.426492f, -4.525250f, -4.618689f, -4.707227f, -4.791238f,
    -4.871062f, -4.947004f, -5.019339f, -5.088318f, -5.154170f, -5.217102f, -5.277305f, -5.334951f, -5.390200f, -5.443199f, -5.494081f, -5.542973f, -5.589987f, -5.635230f, -5.678800f, -5.720788f, -5.761279f, -5.800351f, -5.838079f, -5.874528f,
    -5.909765f, -5.943848f, -5.976832f, -6.008771f, -6.039713f, -6.069704f, -6.098787f, -6.127003f, -6.154390f, -6.180984f, -6.206819f, -6.231926f, -6.256337f, -6.280080f, -6.303182f, -6.325668f, -6.347563f, -6.368889f, -6.389669f, -6.409924f,
    -6.429673f, -6.448934f, -6.467726f, -6.486065f, -6.503968f, -6.521450f, -6.538526f, -6.555209f, -6.571514f, -6.587452f, -6.603036f, -6.618279f, -6.633189f, -6.647780f, -6.662060f, -6.676040f, -6.689728f, -6.703135f, -6.716268f, -6.729135f,
    -6.741746f, -6.754107f, -6.766226f, -6.778109f, -6.789764f, -6.801198f, -6.812415f, -6.823423f, -6.834227f, -6.844833f, -6.855247f, -6.865472f, -6.875516f, -6.885381f, -6.895074f, -6.904598f, -6.913957f, -6.923157f, -6.932201f, -6.941093f,
    -6.949837f, -6.958437f, -6.966895f, -6.975216f, -6.983403f, -6.991459f, -6.999387f, -7.007190f, -7.014872f, -7.022434f, -7.029880f, -7.037213f, -7.044434f, -7.051547f, -7.058553f, -7.065456f, -7.072257f, -7.078959f, -7.085564f, -7.092074f,
    -7.098491f, -7.104817f, -7.111054f, -7.117204f, -7.123268f, -7.129249f, -7.135148f, -7.140966f, -7.146706f, -7.152369f, -7.157956f, -7.163470f, -7.168911f, -7.174280f, -7.179580f, -7.184812f, -7.189977f, -7.195075f, -7.200110f, -7.205081f,
    -7.209990f, -7.214837f, -7.219626f, -7.224355f, -7.229026f, -7.233642f, -7.238201f, -7.242706f, -7.247157f, -7.251556f, -7.255902f, -7.260198f, -7.264444f, -7.268641f, -7.272790f, -7.276891f, -7.280946f, -7.284955f, -7.288919f, -7.292839f,
    -7.296715f, -7.300548f, -7.304339f, -7.308089f, -7.311798f, -7.315466f, -7.319096f, -7.322686f, -7.326239f, -7.329754f, -7.333231f, -7.336673f, -7.340078f, -7.343449f, -7.346784f, -7.350086f, -7.353353f, -7.356588f, -7.359789f, -7.362959f,
    -7.366097f, -7.369204f, -7.372280f, -7.375325f, -7.378341f, -7.381328f, -7.384285f, -7.387214f, -7.390114f, -7.392987f, -7.395833f, -7.398651f, -7.401443f, -7.404209f, -7.406949f, -7.409664f, -7.412353f, -7.415017f, -7.417658f, -7.420274f,
    -7.422866f, -7.425435f, -7.427980f, -7.430503f, -7.433003f, -7.435481f, -7.437937f, -7.440372f, -7.442785f, -7.445177f, -7.447548f, -7.449899f, -7.452229f, -7.454540f, -7.456831f, -7.459102f, -7.461354f, -7.463587f, -7.465801f, -7.467997f,
    -7.470174f, -7.472333f, -7.474475f, -7.476599f, -7.478705f, -7.480794f, -7.482866f, -7.484922f, -7.486961f, -7.488983f, -7.490990f, -7.492980f, -7.494955f, -7.496914f, -7.498857f, -7.500785f, -7.502699f, -7.504597f, -7.506481f, -7.508350f,
    -7.510204f, -7.512045f, -7.513871f, -7.515684f, -7.517483f, -7.519268f, -7.521040f, -7.522798f, -7.524544f, -7.526276f, -7.527996f, -7.529703f, -7.531397f, -7.533079f, -7.534749f, -7.536407f, -7.538052f, -7.539686f, -7.541308f, -7.542918f,
    -7.544517f, -7.546104f, -7.547680f, -7.549245f, -7.550799f, -7.552342f, -7.553875f, -7.555396f, -7.556907f, -7.558408f, -7.559898f, -7.561378f, -7.562848f, -7.564308f, -7.565758f, -7.567198f, -7.568628f, -7.570049f, -7.571460f, -7.572862f,
    -7.574254f, -7.575637f, -7.577011f, -7.578376f, -7.579732f, -7.581079f, -7.582418f, -7.583747f, -7.585068f, -7.586380f, -7.587684f, -7.588980f, -7.590267f, -7.591546f, -7.592817f, -7.594079f, -7.595334f, -7.596581f, -7.597820f, -7.599051f,
    -7.600275f, -7.601491f, -7.602699f, -7.603900f, -7.605093f, -7.606280f, -7.607458f, -7.608630f, -7.609795f, -7.610952f, -7.612102f, -7.613246f, -7.614382f, -7.615512f, -7.616635f, -7.617751f, -7.618861f, -7.619964f, -7.621061f, -7.622151f,
    -7.623234f, -7.624312f, -7.625383f, -7.626447f, -7.627506f, -7.628558f, -7.629605f, -7.630645f, -7.631679f, -7.632708f, -7.633730f, -7.634747f, -7.635758f, -7.636763f, -7.637762f, -7.638756f, -7.639744f, -7.640727f, -7.641704f, -7.642676f,
    -7.643643f, -7.644604f, -7.645560f, -7.646510f, -7.647455f, -7.648395f, -7.649330f, -7.650260f, -7.651185f, -7.652105f, -7.653020f, -7.653930f, -7.654835f, -7.655735f, -7.656630f, -7.657521f, -7.658406f, -7.659287f, -7.660164f, -7.661036f,
    -7.661903f, -7.662766f, -7.663624f, -7.664478f, -7.665327f, -7.666172f, -7.667012f, -7.667848f, -7.668680f, -7.669508f, -7.670331f, -7.671150f, -7.671965f, -7.672776f, -7.673582f, -7.674385f, -7.675183f, -7.675978f, -7.676768f, -7.677555f,
    -7.678337f, -7.679116f, -7.679891f, -7.680662f, -7.681429f, -7.682192f, -7.682952f, -7.683708f, -7.684460f, -7.685208f, -7.685953f, -7.686694f, -7.687432f, -7.688166f, -7.688897f, -7.689624f, -7.690347f, -7.691067f, -7.691784f, -7.692497f,
    -7.693207f, -7.693913f, -7.694616f, -7.695316f, -7.696012f, -7.696705f, -7.697395f, -7.698082f, -7.698765f, -7.699445f, -7.700122f, -7.700796f, -7.701467f, -7.702135f, -7.702800f, -7.703461f, -7.704120f, -7.704775f, -7.705428f, -7.706077f,
    -7.706724f, -7.707368f, -7.708009f, -7.708646f, -7.709281f, -7.709914f, -7.710543f, -7.711169f, -7.711793f, -7.712414f, -7.713032f, -7.990596f /* >4500mm (fixed value) */
};

/**
 * @brief  根据角度判断激光雷达采样点的方位标签
 * @param  angle 采样点角度（0-360度）
 * @retval 采样点的方位标签（前/后/左/右的组合）
 */
static LidarPointTag_T Lidar_GetPointType(float angle)
{
    LidarPointTag_T tag = LIDAR_POINT_TAG_UNKNOW;

    if (angle <= g_LidarState.frontBackAngleCheckThreshold || angle >= 360 - g_LidarState.frontBackAngleCheckThreshold)
    {
        tag |= LIDAR_POINT_TAG_FORWARD;
    }
    if (angle >= 180 - g_LidarState.frontBackAngleCheckThreshold && angle <= 180 + g_LidarState.frontBackAngleCheckThreshold)
    {
        tag |= LIDAR_POINT_TAG_BACK;
    }
    if (angle >= g_LidarState.sideAngleCheckThreshold && angle <= 180 - g_LidarState.sideAngleCheckThreshold)
    {
        tag |= LIDAR_POINT_TAG_LEFT;
    }
    if (angle >= 180 - g_LidarState.sideAngleCheckThreshold && angle <= 180 + g_LidarState.sideAngleCheckThreshold)
    {
        tag |= LIDAR_POINT_TAG_RIGHT;
    }

    return tag;
}

/**
 * @brief  角度修正计算
 * @param  distance_mm 距离（单位：毫米）
 * @retval 角度修正值（单位：度）
 */
static float Ydlidar_GetAngleCorrection(float distance_mm)
{
    if (distance_mm <= 1.0f) { return s_AngleCorrectionTable[0]; }

    /* 大于4500mm，使用固定值（因为数值已经收敛） */
    if (distance_mm > 4500.0f) { return s_AngleCorrectionTable[450]; }

    /* 计算插值位置 */
    uint16_t index = (uint16_t)(distance_mm / 10.0f);
    float factor = (distance_mm - index * 10.0f) / 10.0f;

    /* 线性插值 */
    return s_AngleCorrectionTable[index] * (1.0f - factor) + s_AngleCorrectionTable[index + 1] * factor;
}

/**
 * @brief  采集完成缓冲区回调函数
 */
__weak void Lidar_FinishOneBufferCallback(void)
{
}

/**
 * @brief  切换缓冲区
 */
static void SwapBuffers(void)
{
    /* 复制数据 */
    Memory_Copy(&g_LidarState.outputBuffer, &g_LidarState.processingBuffer, sizeof(LidarDataList_T));
    g_LidarState.outputBufferReady = 1;
    Lidar_FinishOneBufferCallback();

    /* 清空解算缓冲区 */
    g_LidarState.processingBuffer.count = 0;
}

/**
 * @brief  解析当前YDLidar数据包并填充距离数据表
 * @param  PointIndex 当前采样点索引
 */
static void Ydlidar_ProcessData(uint16_t PointIndex)
{
    /* 计算角度差(处理360°跨越情况) */
    float angleDiff = g_LidarState.ydlidarEndAngle - g_LidarState.ydlidarStartAngle;
    if (angleDiff < 0) { angleDiff += 360.0f; }

    /* 解析每个采样点 */
    /* 计算距离(单位：mm) */
    float distance = g_LidarState.ydlidarCurrentPacket.Samples[PointIndex] / 4.0f;
    float angle = 0;

    /* 计算角度(线性插值) */
    if (g_LidarState.ydlidarPointCount > 1)
    {
        angle = g_LidarState.ydlidarStartAngle + (angleDiff * PointIndex) / (g_LidarState.ydlidarPointCount - 1);
    }
    else
    {
        angle = g_LidarState.ydlidarStartAngle;
    }

    /* 归一化角度到0-360范围 */
    angle = fmodf(angle, 360.0f);
    angle = 360.0f - angle;

    /* 应用角度修正 */
    if (distance > 1.00f) /* 最小精度为1.0f，此时才写入数据 */
    {
        /* 计算角度修正 */
        angle += Ydlidar_GetAngleCorrection(distance);

        /* 再次归一化角度 */
        angle = fmodf(angle, 360.0f);

        /* 写入解算缓冲区 */
        if (g_LidarState.processingBuffer.count < YDLIDAR_LIDAR_MAX_SAMPLES)
        {
            g_LidarState.processingBuffer.buffer[g_LidarState.processingBuffer.count].pointType = Lidar_GetPointType(angle);
            g_LidarState.processingBuffer.buffer[g_LidarState.processingBuffer.count].distance = distance;
            g_LidarState.processingBuffer.buffer[g_LidarState.processingBuffer.count].angle = angle;
            g_LidarState.processingBuffer.count += 1;
            g_LidarState.collectPointCount += 1;
        }
    }
}

/**
 * @brief  处理YDLidar X2单个字节
 * @param  byte 输入字节
 */
void Ydlidar_ProcessByte(uint8_t byte)
{
    static float lastEndAngle = 0;

    switch (g_LidarState.ydlidarCurrentState)
    {
    case YDLIDAR_STATE_SYNC1:
        /* 等待第一个同步字节 (0xAA) */
        if (byte == 0xAA)
        {
            g_LidarState.ydlidarCurrentState = YDLIDAR_STATE_SYNC2;
        }
        break;

    case YDLIDAR_STATE_SYNC2:
        /* 等待第二个同步字节 (0x55) */
        if (byte == 0x55)
        {
            g_LidarState.ydlidarCurrentState = YDLIDAR_STATE_CT;
        }
        else
        {
            /* 同步失败，回到初始状态 */
            g_LidarState.ydlidarCurrentState = YDLIDAR_STATE_SYNC1;
        }
        break;

    case YDLIDAR_STATE_CT:
        /* 接收包类型字节 */
        g_LidarState.ydlidarCurrentPacket.PacketType = byte;
        g_LidarState.ydlidarCurrentState = YDLIDAR_STATE_LSN;
        break;

    case YDLIDAR_STATE_LSN:
        /* 接收采样数量字节 */
        g_LidarState.ydlidarPointCount = byte;
        g_LidarState.ydlidarCurrentState = YDLIDAR_STATE_FSA_L;
        break;

    case YDLIDAR_STATE_FSA_L:
        /* 接收起始角低字节 */
        g_LidarState.ydlidarCurrentPacket.StartAngle = byte;
        g_LidarState.ydlidarCurrentState = YDLIDAR_STATE_FSA_H;
        break;

    case YDLIDAR_STATE_FSA_H:
        /* 接收起始角高字节并组合 */
        g_LidarState.ydlidarCurrentPacket.StartAngle |= (byte << 8);
        g_LidarState.ydlidarStartAngle = (g_LidarState.ydlidarCurrentPacket.StartAngle >> 1) / 64.0f;
        g_LidarState.ydlidarCurrentState = YDLIDAR_STATE_LSA_L;
        break;

    case YDLIDAR_STATE_LSA_L:
        /* 接收结束角低字节 */
        g_LidarState.ydlidarCurrentPacket.EndAngle = byte;
        g_LidarState.ydlidarCurrentState = YDLIDAR_STATE_LSA_H;
        break;

    case YDLIDAR_STATE_LSA_H:
        /* 接收结束角高字节并组合 */
        g_LidarState.ydlidarCurrentPacket.EndAngle |= (byte << 8);
        g_LidarState.ydlidarEndAngle = (g_LidarState.ydlidarCurrentPacket.EndAngle >> 1) / 64.0f;
        g_LidarState.ydlidarCurrentState = YDLIDAR_STATE_CS_L;
        break;

    case YDLIDAR_STATE_CS_L:
        /* 接收校验码低字节 */
        g_LidarState.ydlidarCurrentPacket.CheckCode = byte;
        g_LidarState.ydlidarCurrentState = YDLIDAR_STATE_CS_H;
        break;

    case YDLIDAR_STATE_CS_H:
        /* 接收校验码高字节并组合 */
        g_LidarState.ydlidarCurrentPacket.CheckCode |= (byte << 8);
        g_LidarState.ydlidarCurrentState = YDLIDAR_STATE_SAMPLE_DATA_L;
        g_LidarState.ydlidarCurrentPacket.SampleIndex = 0;
        break;

    case YDLIDAR_STATE_SAMPLE_DATA_L:
        /* 低字节 */
        g_LidarState.ydlidarCurrentPacket.Samples[g_LidarState.ydlidarCurrentPacket.SampleIndex] = byte;
        g_LidarState.ydlidarCurrentState = YDLIDAR_STATE_SAMPLE_DATA_H;
        break;

    case YDLIDAR_STATE_SAMPLE_DATA_H:
        /* 高字节 */
        g_LidarState.ydlidarCurrentPacket.Samples[g_LidarState.ydlidarCurrentPacket.SampleIndex] |= (byte << 8);
        g_LidarState.ydlidarCurrentPacket.SampleIndex++;

        /* 检查是否完成所有采样点接收 */
        if (g_LidarState.ydlidarCurrentPacket.SampleIndex >= g_LidarState.ydlidarPointCount)
        {
            /* 点云数据包：处理所有点 */
            for (uint16_t i = 0; i < g_LidarState.ydlidarPointCount; i++)
            {
                Ydlidar_ProcessData(i);
            }

            /* 检查是否完成一圈（角度差接近360度）*/
            float angleDiff = g_LidarState.ydlidarEndAngle - lastEndAngle;

            /* 如果角度差接近360度，则认为一圈完成 */
            if (angleDiff < 0.0f || g_LidarState.collectPointCount >= YDLIDAR_LIDAR_MAX_SAMPLES)
            {
                /* 一圈完成，交换缓冲区 */
                SwapBuffers();

                /* 重置收集状态 */
                g_LidarState.collectPointCount = 0;
            }

            /* 更新上次角度变化 */
            lastEndAngle = g_LidarState.ydlidarEndAngle;

            /* 重置状态机，准备接收下一个数据包 */
            g_LidarState.ydlidarCurrentState = YDLIDAR_STATE_SYNC1;
        }
        else
        {
            g_LidarState.ydlidarCurrentState = YDLIDAR_STATE_SAMPLE_DATA_L;
        }
        break;
    }
}

/**
 * @brief  处理YDLidar X2缓冲区中的多个字节
 * @param  buffer 输入缓冲区
 * @param  length 缓冲区长度
 */
void Ydlidar_ProcessBuffer(uint8_t *buffer, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++)
    {
        Ydlidar_ProcessByte(buffer[i]);
    }
}

/**
 * @brief  处理N10/N10P单个字节
 * @param  byte 输入字节
 */
void N10_ProcessByte(uint8_t byte)
{
    switch (g_LidarState.n10CurrentState)
    {
    case N10_STATE_HEADER_0:
        if (byte == 0xA5)
        {
            g_LidarState.n10CurrentPacket.Header0 = byte;
            g_LidarState.n10CurrentState = N10_STATE_HEADER_1;
            g_LidarState.n10CrcSum = byte; /* 开始计算CRC */
        }
        break;

    case N10_STATE_HEADER_1:
        if (byte == 0x5A)
        {
            g_LidarState.n10CurrentPacket.Header1 = byte;
            g_LidarState.n10CurrentState = N10_STATE_LENGTH;
            g_LidarState.n10CrcSum += byte;
        }
        else
        {
            /* 同步失败，回到初始状态 */
            g_LidarState.n10CurrentState = N10_STATE_HEADER_0;
            g_LidarState.n10CrcSum = 0;
        }
        break;

    case N10_STATE_LENGTH:
        /* 接收数据包长度字节，应为0x3A (58字节) */
        g_LidarState.n10CurrentPacket.Length = byte;
        g_LidarState.n10CurrentState = N10_STATE_SPEED_H;
        g_LidarState.n10CrcSum += byte;
        break;

    case N10_STATE_SPEED_H:
        g_LidarState.n10CurrentPacket.SpeedH = byte;
        g_LidarState.n10CurrentState = N10_STATE_SPEED_L;
        g_LidarState.n10CrcSum += byte;
        break;

    case N10_STATE_SPEED_L:
        g_LidarState.n10CurrentPacket.SpeedL = byte;
        g_LidarState.n10CurrentState = N10_STATE_START_ANGLE_H;
        g_LidarState.n10CrcSum += byte;
        break;

    case N10_STATE_START_ANGLE_H:
        g_LidarState.n10CurrentPacket.StartAngleH = byte;
        g_LidarState.n10CurrentState = N10_STATE_START_ANGLE_L;
        g_LidarState.n10CrcSum += byte;
        break;

    case N10_STATE_START_ANGLE_L:
        g_LidarState.n10CurrentPacket.StartAngleL = byte;
        g_LidarState.n10CurrentState = N10_STATE_POINTS;
        g_LidarState.n10CrcSum += byte;
        g_LidarState.n10PointIndex = 0; /* 重置点索引 */
        break;

    case N10_STATE_POINTS:
        if (g_LidarState.n10PointIndex < N10_POINT_COUNT * 3) /* 16个点，每个点3字节，共48字节 */
        {
            uint8_t pointIndex = g_LidarState.n10PointIndex / 3;
            uint8_t byteIndex = g_LidarState.n10PointIndex % 3;

            switch (byteIndex)
            {
            case 0: /* 距离高字节 */
                g_LidarState.n10CurrentPacket.Points[pointIndex].DistanceH = byte;
                break;
            case 1: /* 距离低字节 */
                g_LidarState.n10CurrentPacket.Points[pointIndex].DistanceL = byte;
                break;
            case 2: /* 强度 */
                g_LidarState.n10CurrentPacket.Points[pointIndex].Strength = byte;
                break;
            }

            g_LidarState.n10CrcSum += byte;
            g_LidarState.n10PointIndex++;

            /* 如果处理完16个点（48字节），进入结束角度处理 */
            if (g_LidarState.n10PointIndex == N10_POINT_COUNT * 3)
            {
                g_LidarState.n10CurrentState = N10_STATE_END_ANGLE_H;
            }
        }
        break;

    case N10_STATE_END_ANGLE_H:
        g_LidarState.n10CurrentPacket.EndAngleH = byte;
        g_LidarState.n10CurrentState = N10_STATE_END_ANGLE_L;
        g_LidarState.n10CrcSum += byte;
        break;

    case N10_STATE_END_ANGLE_L:
        g_LidarState.n10CurrentPacket.EndAngleL = byte;
        g_LidarState.n10CurrentState = N10_STATE_CRC;
        g_LidarState.n10CrcSum += byte;
        break;

    case N10_STATE_CRC:
        g_LidarState.n10CurrentPacket.CheckCode = byte;

        /* CRC校验：从Header0到EndAngleL的累加和应与CheckCode相等 */
        if ((g_LidarState.n10CrcSum & 0xFF) == g_LidarState.n10CurrentPacket.CheckCode)
        {
            /* 解析数据并存储到解算缓冲区 */
            float startAngle = (((uint16_t)g_LidarState.n10CurrentPacket.StartAngleH << 8) + g_LidarState.n10CurrentPacket.StartAngleL) / 100.0f;
            float endAngle = (((uint16_t)g_LidarState.n10CurrentPacket.EndAngleH << 8) + g_LidarState.n10CurrentPacket.EndAngleL) / 100.0f;

            /* 处理角度跨越0度的情况 */
            if (startAngle > endAngle)
            {
                endAngle += 360.0f;
            }

            /* 计算角度步长 */
            float angleStep = 0;
            if (N10_POINT_COUNT > 1)
            {
                angleStep = (endAngle - startAngle) / (N10_POINT_COUNT - 1);
            }

            /* 解析每个点 */
            for (int i = 0; i < N10_POINT_COUNT; i++)
            {
                /* 计算距离（mm） */
                float distance = ((uint16_t)(g_LidarState.n10CurrentPacket.Points[i].DistanceH & 0x7F) << 8) + g_LidarState.n10CurrentPacket.Points[i].DistanceL;

                /* 计算角度（线性插值） */
                float angle = 0;
                if (N10_POINT_COUNT > 1)
                {
                    angle = startAngle + angleStep * i;
                }
                else
                {
                    angle = startAngle;
                }

                /* 归一化角度到0-360范围 */
                angle = fmodf(angle, 360.0f);
                angle = 360.0f - angle;

                /* 存储到解算缓冲区 */
                if (g_LidarState.processingBuffer.count < N10_LIDAR_MAX_SAMPLES)
                {
                    g_LidarState.processingBuffer.buffer[g_LidarState.processingBuffer.count].pointType = Lidar_GetPointType(angle);
                    g_LidarState.processingBuffer.buffer[g_LidarState.processingBuffer.count].distance = distance;
                    g_LidarState.processingBuffer.buffer[g_LidarState.processingBuffer.count].angle = angle;
                    g_LidarState.processingBuffer.count++;

                    /* 设置数据类型 */
                    g_LidarState.processingBuffer.type = LIDAR_TYPE_N10;
                }
                else
                {
                    /* 缓冲区满，交换缓冲区 */
                    SwapBuffers();

                    /* 开始新的数据收集 */
                    g_LidarState.processingBuffer.count = 0;
                    g_LidarState.processingBuffer.type = LIDAR_TYPE_N10;

                    /* 存储当前点 */
                    g_LidarState.processingBuffer.buffer[0].distance = distance;
                    g_LidarState.processingBuffer.buffer[0].angle = angle;
                    g_LidarState.processingBuffer.count = 1;
                }
            }
        }
        else
        {
            /* CRC校验失败，增加失败计数 */
            g_LidarState.n10FailedCount++;
        }

        /* 重置状态机，准备接收下一个数据包 */
        g_LidarState.n10CurrentState = N10_STATE_HEADER_0;
        g_LidarState.n10CrcSum = 0;
        break;
    }
}

/**
 * @brief  处理N10/N10P缓冲区中的多个字节
 * @param  buffer 输入缓冲区
 * @param  length 缓冲区长度
 */
void N10_ProcessBuffer(uint8_t *buffer, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++)
    {
        N10_ProcessByte(buffer[i]);
    }
}

/**
 * @brief  解析 N10P 数据包
 */
static void N10P_ParsePacket(void)
{
    /* 计算起始角度（单位：度） */
    float startAngle = (((uint16_t)g_LidarState.n10pCurrentPacket.StartAngleH << 8) + g_LidarState.n10pCurrentPacket.StartAngleL) / 100.0f;

    /* 计算结束角度（单位：度） */
    float endAngle = (((uint16_t)g_LidarState.n10pCurrentPacket.EndAngleH << 8) + g_LidarState.n10pCurrentPacket.EndAngleL) / 100.0f;

    /* 处理角度跨越0度的情况 */
    if (startAngle > endAngle)
    {
        endAngle += 360.0f;
    }

    /* 计算每个点之间的角度间隔 */
    float angleStep = 0;
    if (N10P_POINT_COUNT > 1)
    {
        angleStep = (endAngle - startAngle) / (N10P_POINT_COUNT - 1);
    }

    /* 解析每个点 */
    for (int i = 0; i < N10P_POINT_COUNT; i++)
    {
        /* 计算距离（单位：毫米） */
        float distance = ((uint16_t)(g_LidarState.n10pCurrentPacket.Points[i].DistanceH & 0x7F) << 8) + g_LidarState.n10pCurrentPacket.Points[i].DistanceL;

        /* 检查是否为有效点（距离不为0） */
        if (distance > 0.0f)
        {
            /* 计算角度（线性插值） */
            float angle = 0;
            if (N10P_POINT_COUNT > 1)
            {
                angle = startAngle + angleStep * i;
            }
            else
            {
                angle = startAngle;
            }

            /* 归一化角度到0-360范围 */
            angle = fmodf(angle, 360.0f);
            angle = 360.0f - angle;

            /* 存储到解算缓冲区 */
            if (g_LidarState.n10pReadCount >= N10P_LIDAR_MAX_SAMPLES)
            {
                /* 缓冲区满，交换缓冲区 */
                SwapBuffers();

                /* 开始新的数据收集 */
                g_LidarState.processingBuffer.count = 0;
                g_LidarState.processingBuffer.type = LIDAR_TYPE_N10P;

                /* 存储当前点 */
                g_LidarState.processingBuffer.buffer[0].distance = distance;
                g_LidarState.processingBuffer.buffer[0].angle = angle;
                g_LidarState.processingBuffer.count = 1;
                g_LidarState.n10pReadCount = 1;
            }
            /* 降采样到450个点 */
            else if (g_LidarState.n10pReadCount++ % 2 == 0)
            {
                g_LidarState.processingBuffer.buffer[g_LidarState.processingBuffer.count].pointType = Lidar_GetPointType(angle);
                g_LidarState.processingBuffer.buffer[g_LidarState.processingBuffer.count].distance = distance;
                g_LidarState.processingBuffer.buffer[g_LidarState.processingBuffer.count].angle = angle;
                g_LidarState.processingBuffer.count++;
                g_LidarState.processingBuffer.type = LIDAR_TYPE_N10P;
            }
        }
    }
}

/**
 * @brief  计算 N10P 数据包的 CRC 校验和
 * @retval 计算得到的 CRC 校验和
 */
static uint8_t N10P_CalculateCRC(void)
{
    uint8_t crcSum = 0;

    /* 从 Header0 到 EndAngleL 累加 */
    crcSum += g_LidarState.n10pCurrentPacket.Header0;
    crcSum += g_LidarState.n10pCurrentPacket.Header1;
    crcSum += g_LidarState.n10pCurrentPacket.Length;
    crcSum += g_LidarState.n10pCurrentPacket.SpeedH;
    crcSum += g_LidarState.n10pCurrentPacket.SpeedL;
    crcSum += g_LidarState.n10pCurrentPacket.StartAngleH;
    crcSum += g_LidarState.n10pCurrentPacket.StartAngleL;

    /* 累加点数据 */
    for (int i = 0; i < N10P_POINT_COUNT; i++)
    {
        crcSum += g_LidarState.n10pCurrentPacket.Points[i].DistanceH;
        crcSum += g_LidarState.n10pCurrentPacket.Points[i].DistanceL;
        crcSum += g_LidarState.n10pCurrentPacket.Points[i].Strength;
    }

    /* 累加未知字节 */
    crcSum += g_LidarState.n10pCurrentPacket.Unknown1;
    crcSum += g_LidarState.n10pCurrentPacket.Unknown2;

    /* 累加结束角度 */
    crcSum += g_LidarState.n10pCurrentPacket.EndAngleH;
    crcSum += g_LidarState.n10pCurrentPacket.EndAngleL;

    return crcSum;
}

/**
 * @brief  校验并解析 N10P 数据包
 */
static void N10P_CheckAndParsePacket(void)
{
    /* 计算 CRC 校验和 */
    uint8_t calculatedCRC = N10P_CalculateCRC();

    /* 校验 */
    if (calculatedCRC == g_LidarState.n10pCurrentPacket.CheckCode)
    {
        /* 校验成功，重置失败计数 */
        g_LidarState.n10pFailedCount = 0;

        /* 解析数据并存储到解算缓冲区 */
        N10P_ParsePacket();
    }
}

/**
 * @brief  处理 N10P 单个字节
 * @param  byte 输入字节
 */
static void N10P_ProcessByte(uint8_t byte)
{
    switch (g_LidarState.n10pCurrentState)
    {
    case N10P_STATE_HEADER_0:
        if (byte == 0xA5)
        {
            g_LidarState.n10pCurrentPacket.Header0 = byte;
            g_LidarState.n10pCurrentState = N10P_STATE_HEADER_1;
            g_LidarState.n10pCurrentPacket.PointIndex = 0;
        }
        break;

    case N10P_STATE_HEADER_1:
        if (byte == 0x5A)
        {
            g_LidarState.n10pCurrentPacket.Header1 = byte;
            g_LidarState.n10pCurrentState = N10P_STATE_LENGTH;
        }
        else
        {
            g_LidarState.n10pCurrentState = N10P_STATE_HEADER_0;
        }
        break;

    case N10P_STATE_LENGTH:
        /* 接收数据包长度字节，应为 0x6C (108字节) */
        g_LidarState.n10pCurrentPacket.Length = byte;
        g_LidarState.n10pCurrentState = N10P_STATE_SPEED_H;
        break;

    case N10P_STATE_SPEED_H:
        g_LidarState.n10pCurrentPacket.SpeedH = byte;
        g_LidarState.n10pCurrentState = N10P_STATE_SPEED_L;
        break;

    case N10P_STATE_SPEED_L:
        g_LidarState.n10pCurrentPacket.SpeedL = byte;
        g_LidarState.n10pCurrentState = N10P_STATE_START_ANGLE_H;
        break;

    case N10P_STATE_START_ANGLE_H:
        g_LidarState.n10pCurrentPacket.StartAngleH = byte;
        g_LidarState.n10pCurrentState = N10P_STATE_START_ANGLE_L;
        break;

    case N10P_STATE_START_ANGLE_L:
        g_LidarState.n10pCurrentPacket.StartAngleL = byte;
        g_LidarState.n10pCurrentState = N10P_STATE_POINT_DATA;
        g_LidarState.n10pCurrentPacket.PointIndex = 0;
        break;

    case N10P_STATE_POINT_DATA:
        /* 处理点数据，共32个点，每个点3字节 */
        if (g_LidarState.n10pCurrentPacket.PointIndex < N10P_POINT_COUNT * 3)
        {
            uint8_t pointIndex = g_LidarState.n10pCurrentPacket.PointIndex / 3;

            /* 根据状态计算当前是点的第几个字节 */
            uint8_t byteInPoint = g_LidarState.n10pCurrentPacket.PointIndex % 3;

            switch (byteInPoint)
            {
            case 0: /* 距离高字节 */
                g_LidarState.n10pCurrentPacket.Points[pointIndex].DistanceH = byte & 0x7F;
                break;
            case 1: /* 距离低字节 */
                g_LidarState.n10pCurrentPacket.Points[pointIndex].DistanceL = byte;
                break;
            case 2: /* 强度 */
                g_LidarState.n10pCurrentPacket.Points[pointIndex].Strength = byte;
                break;
            }

            g_LidarState.n10pCurrentPacket.PointIndex++;

            /* 如果处理完32个点，进入未知字节处理 */
            if (g_LidarState.n10pCurrentPacket.PointIndex == N10P_POINT_COUNT * 3)
            {
                g_LidarState.n10pCurrentState = N10P_STATE_UNKNOWN_1;
            }
        }
        break;

    case N10P_STATE_UNKNOWN_1:
        g_LidarState.n10pCurrentPacket.Unknown1 = byte;
        g_LidarState.n10pCurrentState = N10P_STATE_UNKNOWN_2;
        break;

    case N10P_STATE_UNKNOWN_2:
        g_LidarState.n10pCurrentPacket.Unknown2 = byte;
        g_LidarState.n10pCurrentState = N10P_STATE_END_ANGLE_H;
        break;

    case N10P_STATE_END_ANGLE_H:
        g_LidarState.n10pCurrentPacket.EndAngleH = byte;
        g_LidarState.n10pCurrentState = N10P_STATE_END_ANGLE_L;
        break;

    case N10P_STATE_END_ANGLE_L:
        g_LidarState.n10pCurrentPacket.EndAngleL = byte;
        g_LidarState.n10pCurrentState = N10P_STATE_CRC;
        break;

    case N10P_STATE_CRC:
        g_LidarState.n10pCurrentPacket.CheckCode = byte;

        /* 完整数据包接收完成，开始校验和解析 */
        N10P_CheckAndParsePacket();

        /* 重置状态机，准备接收下一个数据包 */
        g_LidarState.n10pCurrentState = N10P_STATE_HEADER_0;
        break;
    }
}

/**
 * @brief  处理 N10P 缓冲区中的多个字节
 * @param  buffer 输入缓冲区
 * @param  length 缓冲区长度
 */
void N10P_ProcessBuffer(uint8_t *buffer, uint16_t length)
{
    for (uint16_t i = 0; i < length; i++)
    {
        N10P_ProcessByte(buffer[i]);
    }
}

/* ATK-MS901M的通信句柄 */
#define LIDAR_UART_BLOCK s_Uart3Block
extern UartBlock_T LIDAR_UART_BLOCK;

/**
 * @brief  处理激光雷达数据缓冲区
 */
void Lidar_ProcessBuffer(void)
{
    static uint8_t buffer[1024];
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint16_t length = FIFO_ReadBuffer(&LIDAR_UART_BLOCK.RxFIFO, buffer, 1024);
    __set_PRIMASK(primask);

    while (length != 0)
    {
        LidarType_T currentType = Lidar_GetType();

#ifdef LIDAR_FIXED_TYPE
        // 固定类型：无需检测，直接使用已设定的类型
        if (currentType == LIDAR_TYPE_UNKNOWN)
        {
            // 如果因某些原因仍未设置，则强制设置为固定类型（保险）
            Lidar_AutoConnect();
            g_LidarState.currentLidarType = LIDAR_FIXED_TYPE;
            currentType = LIDAR_FIXED_TYPE;
        }
#else
        /* ----- 原有的自动检测代码（仅在未定义固定类型时生效） ----- */
        if (currentType == LIDAR_TYPE_UNKNOWN)
        {
            /* 检测不同类型的激光雷达 */
            for (uint16_t i = 1; i < length; i++)
            {
                if (g_LidarState.outputBufferReady == LIDAR_TYPE_YDLIDAR_X2 && buffer[i - 1] == 0xAA && buffer[i] == 0x55)
                {
                    currentType = LIDAR_TYPE_YDLIDAR_X2;
                    g_LidarState.collectPointCount = 0;
                    g_LidarState.outputBufferReady = 0;
                    break;
                }
                if (g_LidarState.outputBufferReady == LIDAR_TYPE_N10 && buffer[i - 1] == 0xA5 && buffer[i] == 0x5A)
                {
                    currentType = LIDAR_TYPE_N10;
                    g_LidarState.collectPointCount = 0;
                    g_LidarState.outputBufferReady = 0;
                    break;
                }
                if (g_LidarState.outputBufferReady == LIDAR_TYPE_N10P && buffer[i - 1] == 0xA5 && buffer[i] == 0x5A)
                {
                    currentType = LIDAR_TYPE_N10P;
                    g_LidarState.collectPointCount = 0;
                    g_LidarState.outputBufferReady = 0;
                    break;
                }
            }

            g_LidarState.currentLidarType = currentType;

            /* 如果没有检测到，则直接返回 */
            if (currentType == LIDAR_TYPE_UNKNOWN) { return; }
        }
#endif /* LIDAR_FIXED_TYPE */

        /* 根据当前雷达类型处理数据（固定类型或检测到的类型） */
        switch (currentType)
        {
        case LIDAR_TYPE_YDLIDAR_X2:
            Ydlidar_ProcessBuffer(buffer, length);
            break;

        case LIDAR_TYPE_N10:
            N10_ProcessBuffer(buffer, length);
            break;

        case LIDAR_TYPE_N10P:
            N10P_ProcessBuffer(buffer, length);
            break;

        default:
            break;
        }

        primask = __get_PRIMASK();
        __disable_irq();
        length = FIFO_ReadBuffer(&LIDAR_UART_BLOCK.RxFIFO, buffer, 1024);
        __set_PRIMASK(primask);
    }
}

/**
 * @brief  雷达自动连接状态机
 */
void Lidar_AutoConnect(void)
{
#ifdef LIDAR_FIXED_TYPE
    if (g_LidarState.collectPointCount > 0) return;
    // 固定类型：直接设置对应的波特率和当前类型
    switch (LIDAR_FIXED_TYPE)
    {
    case LIDAR_TYPE_YDLIDAR_X2:
        UART_SetBaudRate(COM3, 115200);
        break;
    case LIDAR_TYPE_N10:
        UART_SetBaudRate(COM3, 230400);
        break;
    case LIDAR_TYPE_N10P:
        UART_SetBaudRate(COM3, 460800);
        break;
    default:
        return;
    }
    g_LidarState.currentLidarType = LIDAR_FIXED_TYPE;
    g_LidarState.outputBufferReady = 1;  // 表示缓冲区可用（按原逻辑）
    g_LidarState.collectPointCount++;
#else
    // 原有的自动切换逻辑
    switch (g_LidarState.outputBufferReady)
    {
    case LIDAR_TYPE_YDLIDAR_X2:
        UART_SetBaudRate(COM3, 115200);
        g_LidarState.outputBufferReady = LIDAR_TYPE_YDLIDAR_X2;
        break;
    case LIDAR_TYPE_N10:
        UART_SetBaudRate(COM3, 230400);
        g_LidarState.outputBufferReady = LIDAR_TYPE_N10;
        break;
    case LIDAR_TYPE_N10P:
        UART_SetBaudRate(COM3, 460800);
        g_LidarState.outputBufferReady = LIDAR_TYPE_N10P;
        break;
    default:
        g_LidarState.outputBufferReady = LIDAR_TYPE_YDLIDAR_X2;
        break;
    }

    if (g_LidarState.collectPointCount++ >= 100)
    {
        g_LidarState.collectPointCount = 0;
        g_LidarState.outputBufferReady++;
    }
#endif
}

/**
 * @brief  获取解析后的点云数据
 * @retval 点云数据指针，若无新数据则返回NULL
 */
LidarDataList_T *Lidar_GetPoints(void)
{
    if (g_LidarState.outputBufferReady)
    {
        g_LidarState.outputBufferReady = 0;
        return &g_LidarState.outputBuffer;
    }
    return NULL;
}

/**
 * @brief  获取当前雷达类型
 * @retval 雷达类型
 */
LidarType_T Lidar_GetType(void)
{
    return g_LidarState.currentLidarType;
}

/**
 * @brief  重置雷达状态
 */
void Lidar_Reset(void)
{
    g_LidarState.outputBufferReady = 0;
    g_LidarState.n10FailedCount = 0;

    /* 重置各状态机 */
    g_LidarState.ydlidarCurrentState = YDLIDAR_STATE_SYNC1;
    g_LidarState.n10CurrentState = N10_STATE_HEADER_0;
    g_LidarState.n10pCurrentState = N10P_STATE_HEADER_0;

    /* 清空缓冲区 */
    g_LidarState.processingBuffer.count = 0;
    g_LidarState.outputBuffer.count = 0;

    /* 重置雷达类型为未知 */
    g_LidarState.currentLidarType = LIDAR_TYPE_UNKNOWN;
}