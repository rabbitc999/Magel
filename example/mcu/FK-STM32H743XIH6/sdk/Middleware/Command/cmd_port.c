#include "cmd.h"
#include "cmd_port.h"
#include "bsp_system.h"
#include "imu.h"
#include "car.h"

void CMD_HexShow(uint16_t comX, CmdHexDataPacket_T *packet);

CMD_T g_CMD = { 0 };

/**
 * @brief 格式化输出
 * @param comX 串口号
 * @param array 数据数组
 * @param length 数据长度
 */
void CMD_SendArray(uint16_t comX, uint8_t *array, uint16_t length)
{
    UART_SendArray(comX, array, length);
}

/**
 * @brief 格式化输出
 * @param comX 串口号
 * @param format 格式化字符串
 */
void CMD_Printf(uint16_t comX, char *format, ...)
{
    va_list arg;                          /* 可变参数列表对象 */
    va_start(arg, format);                /* 初始化可变参数 */
    UART_VaListPrintf(comX, format, arg); /* 发送结果字符串 */
    va_end(arg);                          /* 清理可变参数 */
}

/**
 * @brief 串口原始数据处理函数
 * @param comX 串口号
 * @param array 数据数组
 * @param size 数据大小
 */
void UART_RxCpltCallback(UartPort_T comX, uint8_t *array, uint32_t size)
{
    /* 如果size<1，则忽略该数据帧 */
    if (size < 1 || array == NULL) { return; }

    /* 将后续的数据清为0 */
    Memory_Set(array + size, 0, UART_BUFFER_SIZE - size + 1);

    /* 复制到串口缓冲区中 */
    if (size < CMD_RECEIVE_DATA_BUFFER_SIZE)
    {
        Memory_Copy(g_CMD.receiveData[comX], (char *)array, size);
        g_CMD.receiveData[comX][size] = 0;
    }

    /* 判断是否为命令接收模式 */
    if ((g_CMD.receiveMode & (0x01 << comX)) == 0) { return; }

    /* 不同命令系统的选择（HEX命令优先，其次是字符串命令） */
    CMD_ProcessDFA(comX, array, size, g_CMD.hexStrictMode & (0x01 << comX));
}

/**
 * @brief 输出数据
 * @param comX: 串口号
 */
void CMD_OutputData(uint8_t comX)
{
    uint8_t showLineFlag = 0;

    /* 广播数据包 */
    CmdHexDataPacket_T transmitPacket = {
        .commandId = 0x04,
        .funcId = 0x00,
        .dataLength = 0
    };

    /* 状态数据广播 */
    if (((g_CMD.comBoardCast & (0x01 << comX)) != 0))
    {
        CMD_HexShow(comX, &transmitPacket);
    }
}

/**
 * @brief HEX空闲读取函数
 * @param comX: 串口号
 * @param ch: 当前字符输入
 */
__weak void CMD_IDLE_Read(UartPort_T comX, char ch)
{
}
