#include "chassis_ekf.h"
#include "imu.h"
#include "bsp_system.h"
#include "fast_math.h"
#include "store.h"
#include "car.h"
#include "simple_task.h"

ImuControl_T g_IMU = { 0 };
static volatile char s_cDataUpdate = 0, s_cCmd = 0xff;
static volatile char s_ReadFlag = 0;

static void IMU_DataUpdate(uint32_t uiReg, uint32_t uiRegNum);
static void IMU_UartSend(uint8_t *p_data, uint32_t uiSize);
static void IMU_Delay_ms(uint16_t ucMs);

/**
 * @brief  初始化陀螺仪
 * @note   初始化IMU硬件和相关组件，设置数据更新回调函数和串口发送函数
 */
void IMU_Init(void)
{
    /* 初始化ATK-MS901M */
    WIT_Init(WIT_PROTOCOL_NORMAL, 0x50);
    WIT_SerialWriteRegister(IMU_UartSend);
    WIT_RegisterCallBack(IMU_DataUpdate);
    WIT_DelayMsRegister(IMU_Delay_ms);
}

/**
 * @brief  更新IMU数据
 * @param  uiReg 寄存器地址
 * @param  uiRegNum 寄存器数量
 * @note   该函数在接收到新的IMU数据时被调用，更新全局IMU控制结构体中的数据，
 * 并进行必要的处理，如滤波等
 */
static void IMU_DataUpdate(uint32_t uiReg, uint32_t uiRegNum)
{
    int i;
    for (i = 0; i < uiRegNum; i++)
    {
        switch (uiReg)
        {
        case WIT_AZ:
            s_cDataUpdate |= IMU_ACC_UPDATE;
            break;
        case WIT_GZ:
            s_cDataUpdate |= IMU_GYRO_UPDATE;
            break;
        case WIT_HZ:
            s_cDataUpdate |= IMU_MAG_UPDATE;
            break;
        case WIT_Yaw:
            s_cDataUpdate |= IMU_ANGLE_UPDATE;
            break;
        case WIT_Q3:
            s_cDataUpdate |= IMU_QUAT_UPDATE;
            break;
        default:
            s_cDataUpdate |= IMU_READ_UPDATE;
            break;
        }
        uiReg++;
    }
}

/**
 * @brief  串口发送函数
 * @param  p_data 待发送数据指针
 * @param  uiSize 待发送数据长度
 * @note   该函数用于通过串口发送数据，实际发送操作由注册的串口发送函数实现
 */
static void IMU_UartSend(uint8_t *p_data, uint32_t uiSize)
{
    UART_SendArray(COM5, p_data, uiSize);
}

/**
 * @brief  延时函数，支持FreeRTOS调度器和裸机环境
 * @param  ucMs 延时时间，单位为毫秒
 * @note   在FreeRTOS环境下使用vTaskDelay，在裸机环境下使用DWT_DelayMs实现延时
 */
static void IMU_Delay_ms(uint16_t ucMs)
{
    Task_DelayUntil(ucMs, NULL);
}

/**
 * @brief  重置运动学信息
 * @note   重置速度、位置等运动相关参数
 */
void IMU_ResetOrientation(void)
{
    ChassisEKF_RuntimeReset();
    g_IMU.StartAngleZ = g_IMU.Yaw;
    g_IMU.LocationX = 0;
    g_IMU.LocationY = 0;
    g_IMU.ImuSpeedX = 0;
    g_IMU.ImuSpeedY = 0;
    g_IMU.EncSpeedX = 0;
    g_IMU.EncSpeedY = 0;
    g_IMU.OdomSpeedX = 0;
    g_IMU.OdomSpeedY = 0;
    g_IMU.OdomAngleZ = 0;
    g_IMU.lastTick = Timer_GetTicks();
    g_IMU.lastYaw = g_IMU.Yaw;
}

/**
 * @brief  IMU主循环函数，处理数据采集和校准状态机
 * @note   处理不同运行状态下的IMU操作
 */
void IMU_Loop(void)
{
    /* 判断是否需要更新里程计 */
    if (s_ReadFlag != (IMU_ACC_UPDATE | IMU_ANGLE_UPDATE | IMU_QUAT_UPDATE)) { return; }

    /* 时间计算 */
    uint64_t currentTick = Timer_GetTicks();
    float dt = (currentTick - g_IMU.lastTick) / 1e6f;  // 转换为秒
    g_IMU.lastTick = currentTick;

    /* 偏航角计算 */
    float delta_theta = g_IMU.Yaw - g_IMU.lastYaw;
    g_IMU.lastYaw = g_IMU.Yaw;

    /* 去除重力加速度后加速度计计算 */
    float q0 = g_IMU.Q0, q1 = g_IMU.Q1, q2 = g_IMU.Q2, q3 = g_IMU.Q3;
    float gravity_x = 2.0f * (q1 * q3 - q0 * q2);
    float gravity_y = 2.0f * (q0 * q1 + q2 * q3);
    float gravity_z = 1.0f - (q1 * q1 + q2 * q2);
    float gravity = 9.80665f;  // 重力加速度
    float accel_x = gravity * (g_IMU.AccelX - gravity_x);
    float accel_y = gravity * (g_IMU.AccelY - gravity_y);
    float accel_z = gravity * (g_IMU.AccelZ - gravity_z);

    /* 互补滤波法计算速度 */
    float gamma = g_Car.remote.slipGamma;
    g_IMU.EncSpeedX = g_Car.remote.actualSpeedX;
    g_IMU.EncSpeedY = 0;
    g_IMU.ImuSpeedX = gamma * (g_IMU.ImuSpeedX + accel_x * dt) + (1.0f - gamma) * g_IMU.EncSpeedX;
    g_IMU.ImuSpeedY = gamma * (g_IMU.ImuSpeedY + accel_y * dt) + (1.0f - gamma) * g_IMU.EncSpeedY;
    g_IMU.OdomSpeedX = gamma * g_IMU.ImuSpeedX + (1.0f - gamma) * g_IMU.EncSpeedX;
    g_IMU.OdomSpeedY = gamma * g_IMU.ImuSpeedY + (1.0f - gamma) * g_IMU.EncSpeedY;
    g_IMU.OdomAngleZ = g_IMU.Yaw - g_IMU.StartAngleZ;

    float combined_speed = g_IMU.OdomSpeedX;  // 这里可以根据需要选择使用哪个速度信息
    float combined_delta = delta_theta / dt;  // 这里可以根据需要选择使用哪个角度
    float delta_x, delta_y;                   // 计算位置增量

    /* 位置增量计算 */
    if (fabsf(delta_theta) < 0.0001f)
    {
        // 当角度变化很小时，使用近似公式
        // 在数学坐标系中，x方向对应cos，y方向对应sin
        delta_x = combined_speed * dt * cosf(g_IMU.OdomAngleZ);
        delta_y = combined_speed * dt * sinf(g_IMU.OdomAngleZ);
    }
    else
    {
        // 精确公式，考虑圆弧运动
        // 注意：当速度接近0时，需要处理除零问题
        if (fabsf(combined_speed) < 0.001f)
        {
            delta_x = 0;
            delta_y = 0;
        }
        else
        {
            float radius = combined_speed / combined_delta;  // 转弯半径
            delta_x = radius * (sinf(g_IMU.OdomAngleZ + delta_theta) - sinf(g_IMU.OdomAngleZ));
            delta_y = -radius * (cosf(g_IMU.OdomAngleZ + delta_theta) - cosf(g_IMU.OdomAngleZ));
        }
    }

    /* 位置累加 */
    g_IMU.LocationX += delta_x;
    g_IMU.LocationY += delta_y;

    /* 清空标志位 */
    s_ReadFlag = 0;
}

/**
 * @brief  更新FIFO数据，从串口接收缓冲区读取数据并传递给IMU数据处理函数
 * @note   该函数用于更新IMU FIFO缓冲区中的数据，确保数据的连续性和实时性
 */
void IMU_FifoUpdate(void)
{
    uint8_t buffer[512];
    UartBlock_T *block = UART_FindBlockByPort(COM5);
    /* Drain the backlog after a long cooperative GUI/SD task. */
    uint32_t budget = block->RxFIFO.size;
    while (budget != 0)
    {
        uint32_t request = budget < sizeof(buffer) ? budget : sizeof(buffer);
        uint32_t primask = __get_PRIMASK();
        __disable_irq();
        uint32_t length = FIFO_ReadBuffer(&block->RxFIFO, buffer, request);
        __set_PRIMASK(primask);
        if (length == 0) { break; }
        budget -= length;
        for (uint32_t i = 0; i < length; i++) { WIT_SerialDataIn(buffer[i]); }
    }
}

/**
 * @brief  获取IMU数据并更新全局IMU控制结构体
 * @note   该函数从全局寄存器数组中读取最新的IMU数据，并根据数据更新全局IMU控制
 * 结构体中的加速度、陀螺仪、磁力计和姿态角等信息
 */
void IMU_GetData(void)
{
    if (!s_cDataUpdate) { return; }

    if (s_cDataUpdate & IMU_ACC_UPDATE)
    {
        g_IMU.AccelX = WIT_Reg[WIT_AX] / 32768.0f * 16.0f;
        g_IMU.AccelY = WIT_Reg[WIT_AY] / 32768.0f * 16.0f;
        g_IMU.AccelZ = WIT_Reg[WIT_AZ] / 32768.0f * 16.0f;
        s_cDataUpdate &= ~IMU_ACC_UPDATE;
        s_ReadFlag |= IMU_ACC_UPDATE;
    }
    if (s_cDataUpdate & IMU_GYRO_UPDATE)
    {
        g_IMU.GyroX = WIT_Reg[WIT_GX] / 32768.0f * 2000.0f;
        g_IMU.GyroY = WIT_Reg[WIT_GY] / 32768.0f * 2000.0f;
        g_IMU.GyroZ = WIT_Reg[WIT_GZ] / 32768.0f * 2000.0f;
        ChassisEKF_RuntimeGyro(g_IMU.GyroZ, Timer_GetTicks());
        s_cDataUpdate &= ~IMU_GYRO_UPDATE;
    }
    if (s_cDataUpdate & IMU_ANGLE_UPDATE)
    {
        g_IMU.Roll = WIT_Reg[WIT_Roll] / 32768.0f * 180.0f;
        g_IMU.Pitch = WIT_Reg[WIT_Pitch] / 32768.0f * 180.0f;
        g_IMU.Yaw = WIT_Reg[WIT_Yaw] / 32768.0f * 180.0f;
        s_cDataUpdate &= ~IMU_ANGLE_UPDATE;
        s_ReadFlag |= IMU_ANGLE_UPDATE;
    }
    if (s_cDataUpdate & IMU_MAG_UPDATE)
    {
        g_IMU.MagX = WIT_Reg[WIT_HX];
        g_IMU.MagY = WIT_Reg[WIT_HY];
        g_IMU.MagZ = WIT_Reg[WIT_HZ];
        s_cDataUpdate &= ~IMU_MAG_UPDATE;
    }
    if (s_cDataUpdate & IMU_QUAT_UPDATE)
    {
        g_IMU.Q0 = WIT_Reg[WIT_Q0] / 32768.0f;
        g_IMU.Q1 = WIT_Reg[WIT_Q1] / 32768.0f;
        g_IMU.Q2 = WIT_Reg[WIT_Q2] / 32768.0f;
        g_IMU.Q3 = WIT_Reg[WIT_Q3] / 32768.0f;
        s_cDataUpdate &= ~IMU_QUAT_UPDATE;
        s_ReadFlag |= IMU_QUAT_UPDATE;
    }
}

/**
 * @brief  IMU循环任务
 * @param  pvParameters FreeRTOS任务参数（未使用）
 */
TaskRet_T IMU_Task(TaskParam_T *param)
{
    /* 初始化，延时100ms */
    switch (T_STA(param))
    {
    case 0:
    {
        IMU_Init();
        return Task_Return(1, 100);
    }
    case 1:
    {
        WIT_SetContent(WIT_RSW_ACC | WIT_RSW_GYRO | WIT_RSW_ANGLE | WIT_RSW_MAG | WIT_RSW_Q);
        return Task_Return(2, 100);
    }
    default:
    {
        IMU_FifoUpdate();  // 更新FIFO数据
        IMU_GetData();
        ChassisEKF_RuntimePoll();
        IMU_Loop();
        return Task_Delay(10);  // 周期性延时10ms
    }
    }
}
