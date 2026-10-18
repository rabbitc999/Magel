#include "lidar_localization_runtime.h"
#include "run_tasks.h"
#include "gui.h"
#include "storage.h"
#include "logger.h"
#include "cmd_file_transfer.h"
#include "simple_task.h"

/** ****************************************************************************************************/
/* 全局变量 */
LidarDataList_T g_LidarDataList = { 0 };
uint8_t g_LidarDataReadyFlag = 0, g_ConditionBeepFlag = 0, g_ConditionCancelFlag = 0, g_DataOutputEnabled = 0;
uint8_t g_LidarStart = 0, g_LidarKdTreeUpdateFlag = 1;
uint8_t g_DataOutputFlag = 0;
MedianFilt_T g_FrontFilt, g_BackFilt;
volatile float g_FrontUltraSonicDistance = 0, g_BackUltraSonicDistance = 0;
float g_BatteryVoltage = 0;
uint8_t g_SleepFlag = 0;

/* 任务句柄 */
Task_T *g_LidarTask = NULL, *g_GUI_Task = NULL, *g_DataOutputTask = NULL, *g_UartRxTask = NULL, *g_UartTxTask = NULL;

/******************************************************************************************************/
/* 函数定义 */
void UART_ReceiveFinishCallback(UartPort_T comX, uint8_t *buffer, uint32_t length);
void UART_TransmitStartCallback(UartPort_T comX);

/******************************************************************************************************/
/* FreeRTOS任务函数实现 */

/**
 * @brief  通用任务
 * @param  pvParameters FreeRTOS任务参数（未使用）
 */
TaskRet_T Main_Task(TaskParam_T *param)
{
    Log_Poll();
    CMD_FileTransferPoll();
    Storage_Poll();
    CMD_RuntimePoll();
    CMD_RuntimeGetStats(&g_CMD_RuntimeStats);

    /* 电池电压获取 = 读取电压值 * 修正系数 */
    g_BatteryVoltage = Battery_GetVoltage() * g_ParamData.system_VoltageCoff / 100.0f;

    /* 低压报警判断 */
    /* Low-voltage audible alarm removed as requested. */
    if (g_GUI_User.rtcSetFlag == 0) { RTC_ReadTime(); }

    /* 蜂鸣器状态判断 */
    Beep_Disable(); /* Keep the board silent during UART/IMU debugging. */

    /* 待机时间设置 */
    if (g_ParamData.pwr_standbyTime < 30) { g_ParamData.pwr_standbyTime = 30; }
    PWR_SetPowerDownTime(g_ParamData.pwr_standbyTime);

    /* 数据输出状态处理 */
    uint8_t hasDataOutputPort = FALSE;
    for (UartPort_T port = COM1; port < COM_COUNT; port++)
    {
        if ((g_CMD.comBoardCast & (0x01 << port)) != 0 || g_CMD.dataOutputShowFlag[port] != 0)
        {
            Task_Enable(g_DataOutputTask);
            break;
        }
    }

    /* 延时50ms */
    return Task_Delay(50);
}

/**
 * @brief  激光雷达解算循环任务
 * @param  pvParameters FreeRTOS任务参数（未使用）
 */
TaskRet_T Lidar_Task(TaskParam_T *param)
{
    Lidar_ProcessBuffer();
    return Task_Delay(100);
}

/**
 * @brief  采集完成缓冲区回调函数
 */
void Lidar_FinishOneBufferCallback(void)
{
    /* 激光雷达数据读取 */
    LidarDataList_T *temp = Lidar_GetPoints();
    if (temp != NULL) { Memory_Copy(&g_LidarDataList, temp, sizeof(LidarDataList_T)); }
    if (temp != NULL) { LidarLoc_RuntimeSubmit(temp, Timer_GetTicks()); }
    g_LidarKdTreeUpdateFlag = 1;
    g_LidarDataReadyFlag = 1;
}

/** ****************************************************************************************************/
/* 初始化函数 */
/**
 * @brief  程序任务初始化，创建所有FreeRTOS任务、信号量和互斥量
 */
volatile uint32_t g_LidarLocalizationTaskRuns = 0;

static TaskRet_T LidarLocalization_Task(TaskParam_T *param)
{
    (void)param;
    ChassisEKF_RuntimePoll();
    LidarLoc_RuntimePoll(Timer_GetTicks());
    g_LidarLocalizationTaskRuns++;
    return Task_Delay(10);
}

void ProgramTask_Init(void)
{
    /* 声明任务 */
    TaskRet_T Condition_Task(TaskParam_T * param);
    TaskRet_T BluetoothInit_Task(TaskParam_T * param);
    TaskRet_T ESP8266Init_Task(TaskParam_T * param);
    TaskRet_T GUI_Task(TaskParam_T * param);
    TaskRet_T IMU_Task(TaskParam_T * param);
    TaskRet_T CS200A_Task(TaskParam_T * param);
    TaskRet_T NRF24L01_Task(TaskParam_T * param);
    TaskRet_T DataOutput_Task(TaskParam_T * param);
    TaskRet_T UartRx_Task(TaskParam_T * param);
    TaskRet_T UartTx_Task(TaskParam_T * param);

    /* 创建任务 */
    Task_Add(Main_Task, "Main");
    Task_Add(Condition_Task, "Condition");
    Task_Add(BluetoothInit_Task, "BluetoothInit");
    Task_Add(ESP8266Init_Task, "ESP8266Init");
    Task_Add(IMU_Task, "IMU");
    Task_Add(LidarLocalization_Task, "LidarLocalization");
    Task_Add(CS200A_Task, "CS200A");
    Task_Add(NRF24L01_Task, "NRF24L01");

    g_GUI_Task = Task_Add(GUI_Task, "GUI");
    g_LidarTask = Task_Add(Lidar_Task, "Lidar");
    g_DataOutputTask = Task_Add(DataOutput_Task, "DataOutput");
    g_UartRxTask = Task_Add(UartRx_Task, "UartRx");
#if UART_TX_USE_BUFFER
    g_UartTxTask = Task_Add(UartTx_Task, "UartTx");
#endif

    /* 添加串口处理任务 */
    for (UartPort_T port = COM1; port < COM_COUNT; port++)
    {
        UartBlock_T *block = UART_FindBlockByPort(port);
        UART_SetReceiveFinishCallback(port, UART_ReceiveFinishCallback);
#if UART_TX_USE_BUFFER
        UART_SetTransmitStartCallback(port, UART_TransmitStartCallback);
#endif
    }

    /* 初始化滤波器 */
    MedianFilt_Init(&g_FrontFilt, 5);
    MedianFilt_Init(&g_BackFilt, 5);

    /* 关闭灯光 */
    Light_TurnOff();

    /* 设置待机时间 */
    PWR_RecordTime();
}

/**
 * @brief  空闲任务钩子函数
 */
void vApplicationIdleHook(void)
{
    /* 判断是否需要进入低功耗模式 */
    if (!g_SleepFlag && g_GUI_User.rtcSetFlag == 0 && PWR_IsAlarm())
    {
        LED1_OFF();
        LED2_OFF();
        LCD_OFF();
        Task_Disable(g_GUI_Task);
        g_SleepFlag = 1;
    }

    /* 如果按下DOWN键，则退出低功耗模式 */
    if (g_SleepFlag && Key_IsAction(KEY_DOWN))
    {
        LCD_ON();
        Task_Enable(g_GUI_Task);
        Beep_Play(100, 200, 2);
        PWR_RecordTime();
        Key_ClearAll();
        g_SleepFlag = 0;
    }

    __WFI();
}
