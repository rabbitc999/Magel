#ifndef __CMD_HEX_H
#define __CMD_HEX_H

#include "cmd_config.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* HEX数据包声明 */
typedef struct __CmdHexDataPacket_T
{
    uint8_t commandId;
    uint8_t funcId;
    uint8_t *data;
    uint16_t dataLength;
    uint64_t timestampMs;
} CmdHexDataPacket_T;

/* HEX回调函数声明 */
typedef void (*CmdHexCallback_T)(uint16_t comX, CmdHexDataPacket_T *packet);

typedef struct __CmdHexNode_T
{
    uint8_t commandId;
    CmdHexCallback_T callback;
} CmdHexNode_T;

typedef struct __CmdHexTable_T
{
    const CmdHexNode_T *table;
    CmdHexCallback_T notFoundCallback;
    uint16_t count;
} CmdHexTable_T;

extern CmdHexTable_T g_CMDHexTable;

void CMD_SendHexData(uint16_t comX, CmdHexDataPacket_T *packet);
void CMD_HexProcess(uint16_t comX, CmdHexDataPacket_T *packet);
void CMD_HexTransmitOk(uint16_t comX, uint8_t commandId);
void CMD_HexTransmitError(uint16_t comX, uint8_t commandId, uint8_t errorCode);
void CMD_HexTransmitData(uint16_t comX, CmdHexDataPacket_T *packet);
uint32_t CMD_CalcCRC32(const uint8_t *data, size_t len);

void CMD_HexSetNotFoundCallback(CmdHexCallback_T callback);

/* 从数据包中读取特定偏移的数据 */
double CMD_HexReadDouble(const CmdHexDataPacket_T *packet, uint16_t offset);
float CMD_HexReadFloat(const CmdHexDataPacket_T *packet, uint16_t offset);
int32_t CMD_HexReadInt32(const CmdHexDataPacket_T *packet, uint16_t offset);
uint32_t CMD_HexReadUint32(const CmdHexDataPacket_T *packet, uint16_t offset);
int16_t CMD_HexReadInt16(const CmdHexDataPacket_T *packet, uint16_t offset);
uint16_t CMD_HexReadUint16(const CmdHexDataPacket_T *packet, uint16_t offset);
int8_t CMD_HexReadInt8(const CmdHexDataPacket_T *packet, uint16_t offset);
uint8_t CMD_HexReadUint8(const CmdHexDataPacket_T *packet, uint16_t offset);

/* 向数据包写入特定偏移的数据 */
void CMD_HexWriteDouble(CmdHexDataPacket_T *packet, uint16_t offset, double value);
void CMD_HexWriteFloat(CmdHexDataPacket_T *packet, uint16_t offset, float value);
void CMD_HexWriteInt32(CmdHexDataPacket_T *packet, uint16_t offset, int32_t value);
void CMD_HexWriteUint32(CmdHexDataPacket_T *packet, uint16_t offset, uint32_t value);
void CMD_HexWriteInt16(CmdHexDataPacket_T *packet, uint16_t offset, int16_t value);
void CMD_HexWriteUint16(CmdHexDataPacket_T *packet, uint16_t offset, uint16_t value);
void CMD_HexWriteInt8(CmdHexDataPacket_T *packet, uint16_t offset, int8_t value);
void CMD_HexWriteUint8(CmdHexDataPacket_T *packet, uint16_t offset, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif
