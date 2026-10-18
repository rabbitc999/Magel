#include "cmd.h"

static uint64_t s_CmdArena[CMD_RUNTIME_ARENA_SIZE / sizeof(uint64_t)];
CmdRuntimeStats_T g_CMD_RuntimeStats;

static void CMD_RuntimeHexPacket(void *context, uint8_t port, CmdHexDataPacket_T *packet)
{
    (void)context;
    CMD_HexProcess(port, packet);
}

static void CMD_RuntimeStringLine(void *context, uint8_t port, char *line, uint16_t length)
{
    (void)context;
    CMD_StrProcess(port, line, length);
}

static void CMD_RuntimeIdleByte(void *context, uint8_t port, uint8_t byte)
{
    (void)context;
    CMD_IDLE_Read((UartPort_T)port, byte);
}

static void CMD_RuntimeCrcError(void *context, uint8_t port, uint8_t command, uint32_t calculated, uint32_t received)
{
    (void)context;
    uint8_t data[9] = { 0 };
    CmdHexDataPacket_T packet = { 0 };
    packet.commandId = command;
    packet.funcId = CMD_HEX_CALLBACK_ERROR;
    packet.data = data;
    packet.dataLength = sizeof(data);
    CMD_HexWriteInt8(&packet, 0, CMD_HEX_CHECK_ERROR_CMD);
    CMD_HexWriteInt32(&packet, 1, calculated);
    CMD_HexWriteInt32(&packet, 5, received);
    CMD_SendHexData(port, &packet);
}

static uint8_t CMD_RuntimeSend(void *context, uint16_t port, const uint8_t *data, uint16_t length)
{
    (void)context;
    UartBlock_T *block = UART_FindBlockByPort((UartPort_T)port);
    if (block == NULL || !block->TxEnable || FIFO_GetRemain(&block->TxFIFO) < length)
    {
        return 0;
    }
    CMD_SendArray(port, (uint8_t *)data, length);
    return 1;
}

static uint32_t CMD_RuntimeMilliseconds(void *context)
{
    (void)context;
    return HAL_GetTick();
}

void CMD_Init(void)
{
    const CmdRuntimeHooks_T hooks = {
        .hexPacket = CMD_RuntimeHexPacket,
        .stringLine = CMD_RuntimeStringLine,
        .idleByte = CMD_RuntimeIdleByte,
        .crcError = CMD_RuntimeCrcError,
        .send = CMD_RuntimeSend,
        .milliseconds = CMD_RuntimeMilliseconds,
    };
    _Static_assert(CMD_COM_COUNT == COM_COUNT, "Command/UART port count mismatch");
    if (!CMD_RuntimeInit(s_CmdArena, sizeof(s_CmdArena), &hooks))
    {
        Assert_Failed(__FILE__, __LINE__);
        return;
    }
    CMD_HexProcInit();
    CMD_StringProcInit();
    g_CMD.receiveMode |= 0x01;
    CMD_RuntimeGetStats(&g_CMD_RuntimeStats);
}

/* Compatibility entry point; protocol ownership is handled by the C++ runtime. */
uint8_t CMD_ProcessDFA(uint8_t comX, uint8_t *rxData, uint16_t rxDataLength, uint8_t strictMode)
{
    uint8_t result = CMD_RuntimeFeed(comX, rxData, rxDataLength, strictMode);
    CMD_RuntimeGetStats(&g_CMD_RuntimeStats);
    return result;
}
