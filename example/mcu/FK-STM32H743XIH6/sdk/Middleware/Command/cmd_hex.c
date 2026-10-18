#include "cmd.h"

/**
 * @brief 判断设备的端序
 * @retval 1: 大端序, 0: 小端序
 * @note 该函数用于判断设备的字节序，如果为大端序则不需要转换
 */
static inline uint8_t CMD_IsBigEndian(void)
{
    uint16_t test = 0x0001;
    uint8_t *p = (uint8_t *)&test;
    return p[0] == 0x00; /* 大端序: 最高字节在前 */
}

/**
 * @brief 命令协议数据发送 (已修改为2字节数据长度和CRC32校验)
 * @param comX 串口号
 * @param packet 数据包指针
 */
void CMD_SendHexData(uint16_t comX, CmdHexDataPacket_T *packet)
{
    if (packet != NULL)
    {
        CMD_RuntimeSendHex(comX, packet, Timer_GetTimestampMs());
    }
}

/**
 * @brief 串口命令协议发送正确回复
 * @param comX 串口号
 * @param commandId 命令ID
 */
void CMD_HexTransmitOk(uint16_t comX, uint8_t commandId)
{
    CmdHexDataPacket_T transmitPacket;

    transmitPacket.commandId = commandId;
    transmitPacket.funcId = CMD_HEX_CALLBACK_OK;
    transmitPacket.data = NULL;
    transmitPacket.dataLength = 0;

    CMD_SendHexData(comX, &transmitPacket);
}

/**
 * @brief 串口命令协议发送错误回复
 * @param comX 串口号
 * @param commandId 命令ID
 * @param errorCode 错误编码
 */
void CMD_HexTransmitError(uint16_t comX, uint8_t commandId, uint8_t errorCode)
{
    CmdHexDataPacket_T transmitPacket;
    uint8_t hexSendData = errorCode;

    transmitPacket.commandId = commandId;
    transmitPacket.funcId = CMD_HEX_CALLBACK_ERROR;
    transmitPacket.data = &hexSendData;
    transmitPacket.dataLength = 1;

    CMD_SendHexData(comX, &transmitPacket);
}

/**
 * @brief 串口命令协议发送数据回复
 * @param comX 串口号
 * @param packet 数据包指针
 */
void CMD_HexTransmitData(uint16_t comX, CmdHexDataPacket_T *packet)
{
    CmdHexDataPacket_T transmitPacket;

    transmitPacket.commandId = packet->commandId;
    transmitPacket.funcId = CMD_HEX_CALLBACK_DATA;
    transmitPacket.data = packet->data;
    transmitPacket.dataLength = packet->dataLength;

    CMD_SendHexData(comX, &transmitPacket);
}

/**
 * @brief 命令循环处理
 * @param comX 串口号
 * @param packet 数据包指针
 */
/**
 * @brief 命令循环处理（线性查找版本）
 * @param comX 串口号
 * @param packet 数据包指针
 */
void CMD_HexProcess(uint16_t comX, CmdHexDataPacket_T *packet)
{
    uint8_t targetId = packet->commandId;

    /* 线性查找匹配的命令ID */
    for (int i = 0; i < g_CMDHexTable.count; i++)
    {
        if (g_CMDHexTable.table[i].commandId == targetId)
        {
            /* 找到匹配的命令，执行回调 */
            if (g_CMDHexTable.table[i].callback != NULL)
            {
                g_CMDHexTable.table[i].callback(comX, packet);
            }
            /* 找到后立即返回 */
            return;
        }
    }

    /* 如果遍历结束仍未找到，说明命令ID不存在 */
    if (g_CMDHexTable.notFoundCallback != NULL)
    {
        /* 调用未找到命令的回调函数 */
        g_CMDHexTable.notFoundCallback(comX, packet);
    }
    else
    {
        /* 使用默认处理 */
        CMD_HexTransmitError(comX, packet->commandId, CMD_HEX_ERROR_UNKNOWN_CMD);
    }
}

/**
 * @brief  设置HEX命令未找到回调函数
 * @param  callback: 回调函数指针
 * @retval 无
 * @note   此函数用于设置当HEX命令不存在时的回调处理
 */
void CMD_HexSetNotFoundCallback(CmdHexCallback_T callback)
{
    g_CMDHexTable.notFoundCallback = callback;
}

/**
 * @brief 从大端序数据中读取float值
 * @param data 指向大端序数据的指针
 * @return 主机字节序的float值
 */
static float __CMD_HexReadFloat(const uint8_t *data)
{
    union
    {
        float f;
        uint8_t bytes[4];
    } converter;

    if (CMD_IsBigEndian())
    {
        // 大端序，输入数据已经是大端序，不需要转换
        for (int i = 0; i < 4; i++)
        {
            converter.bytes[i] = data[i];
        }
    }
    else
    {
        // 小端序，输入数据是小端序，需要转换为大端序
        for (int i = 0; i < 4; i++)
        {
            converter.bytes[3 - i] = data[i];
        }
    }

    return converter.f;
}

/**
 * @brief 从大端序数据中读取double值
 * @param data 指向大端序数据的指针
 * @return 主机字节序的double值
 */
static double __CMD_HexReadDouble(const uint8_t *data)
{
    union
    {
        double d;
        uint8_t bytes[8];
    } converter;

    if (CMD_IsBigEndian())
    {
        // 大端序，输入数据已经是大端序，不需要转换
        for (int i = 0; i < 8; i++)
        {
            converter.bytes[i] = data[i];
        }
    }
    else
    {
        // 小端序，输入数据是小端序，需要转换为大端序
        for (int i = 0; i < 8; i++)
        {
            converter.bytes[7 - i] = data[i]; /* 大端序转小端序 */
        }
    }

    return converter.d;
}

/**
 * @brief 从大端序数据中读取16位有符号整数
 * @param data 指向大端序数据的指针
 * @return 主机字节序的int16_t值
 */
static int16_t __CMD_HexReadInt16(const uint8_t *data)
{
    if (CMD_IsBigEndian())
    {
        return *(int16_t *)data;
    }
    else
    {
        return (int16_t)((data[0] << 8) | data[1]);
    }
}

/**
 * @brief 从大端序数据中读取16位无符号整数
 * @param data 指向大端序数据的指针
 * @return 主机字节序的uint16_t值
 */
static uint16_t __CMD_HexReadUint16(const uint8_t *data)
{
    if (CMD_IsBigEndian())
    {
        return *(uint16_t *)data;
    }
    else
    {
        return (uint16_t)((data[0] << 8) | data[1]);
    }
}

/**
 * @brief 从大端序数据中读取32位有符号整数
 * @param data 指向大端序数据的指针
 * @return 主机字节序的int32_t值
 */
static int32_t __CMD_HexReadInt32(const uint8_t *data)
{
    if (CMD_IsBigEndian())
    {
        return *(int32_t *)data;
    }
    else
    {
        return (int32_t)((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
    }
}

/**
 * @brief 从大端序数据中读取32位无符号整数
 * @param data 指向大端序数据的指针
 * @return 主机字节序的uint32_t值
 */
static uint32_t __CMD_HexReadUint32(const uint8_t *data)
{
    if (CMD_IsBigEndian())
    {
        return *(uint32_t *)data;
    }
    else
    {
        return (uint32_t)((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
    }
}

/**
 * @brief 将float值写入大端序缓冲区
 * @param buffer 目标缓冲区
 * @param value 要写入的float值
 */
static void __CMD_HexWriteFloat(uint8_t *buffer, float value)
{
    union
    {
        float f;
        uint8_t bytes[4];
    } converter;

    converter.f = value;

    if (CMD_IsBigEndian())
    {
        for (int i = 0; i < 4; i++)
        {
            buffer[i] = converter.bytes[i];
        }
    }
    else
    {
        for (int i = 0; i < 4; i++)
        {
            buffer[i] = converter.bytes[3 - i]; /* 小端序转大端序 */
        }
    }
}

/**
 * @brief 将double值写入大端序缓冲区
 * @param buffer 目标缓冲区
 * @param value 要写入的double值
 */
static void __CMD_HexWriteDouble(uint8_t *buffer, double value)
{
    union
    {
        double d;
        uint8_t bytes[8];
    } converter;

    converter.d = value;

    if (CMD_IsBigEndian())
    {
        for (int i = 0; i < 8; i++)
        {
            buffer[i] = converter.bytes[i];
        }
    }
    else
    {
        for (int i = 0; i < 8; i++)
        {
            buffer[i] = converter.bytes[7 - i]; /* 小端序转大端序 */
        }
    }
}

/**
 * @brief 将16位有符号整数写入大端序缓冲区
 * @param buffer 目标缓冲区
 * @param value 要写入的int16_t值
 */
static void __CMD_HexWriteInt16(uint8_t *buffer, int16_t value)
{
    if (CMD_IsBigEndian())
    {
        buffer[0] = value & 0xFF;
        buffer[1] = (value >> 8) & 0xFF;
    }
    else
    {
        buffer[0] = (value >> 8) & 0xFF;
        buffer[1] = value & 0xFF;
    }
}

/**
 * @brief 将16位无符号整数写入大端序缓冲区
 * @param buffer 目标缓冲区
 * @param value 要写入的uint16_t值
 */
static void __CMD_HexWriteUint16(uint8_t *buffer, uint16_t value)
{
    if (CMD_IsBigEndian())
    {
        buffer[0] = value & 0xFF;
        buffer[1] = (value >> 8) & 0xFF;
    }
    else
    {
        buffer[0] = (value >> 8) & 0xFF;
        buffer[1] = value & 0xFF;
    }
}

/**
 * @brief 将32位有符号整数写入大端序缓冲区
 * @param buffer 目标缓冲区
 * @param value 要写入的int32_t值
 */
static void __CMD_HexWriteInt32(uint8_t *buffer, int32_t value)
{
    if (CMD_IsBigEndian())
    {
        buffer[0] = value & 0xFF;
        buffer[1] = (value >> 8) & 0xFF;
        buffer[2] = (value >> 16) & 0xFF;
        buffer[3] = (value >> 24) & 0xFF;
    }
    else
    {
        buffer[0] = (value >> 24) & 0xFF;
        buffer[1] = (value >> 16) & 0xFF;
        buffer[2] = (value >> 8) & 0xFF;
        buffer[3] = value & 0xFF;
    }
}

/**
 * @brief 将32位无符号整数写入大端序缓冲区
 * @param buffer 目标缓冲区
 * @param value 要写入的uint32_t值
 */
static void __CMD_HexWriteUint32(uint8_t *buffer, uint32_t value)
{
    if (CMD_IsBigEndian())
    {
        buffer[0] = value & 0xFF;
        buffer[1] = (value >> 8) & 0xFF;
        buffer[2] = (value >> 16) & 0xFF;
        buffer[3] = (value >> 24) & 0xFF;
    }
    else
    {
        buffer[0] = (value >> 24) & 0xFF;
        buffer[1] = (value >> 16) & 0xFF;
        buffer[2] = (value >> 8) & 0xFF;
        buffer[3] = value & 0xFF;
    }
}

/**
 * @brief 从数据包特定偏移读取float值
 * @param packet 数据包指针
 * @param offset 偏移量（字节）
 * @return float值
 */
double CMD_HexReadDouble(const CmdHexDataPacket_T *packet, uint16_t offset)
{
    if (packet->dataLength < offset + 8 || packet->data == NULL)
    {
        return 0.0f;
    }
    return __CMD_HexReadDouble(packet->data + offset);
}

/**
 * @brief 从数据包特定偏移读取float值
 * @param packet 数据包指针
 * @param offset 偏移量（字节）
 * @return float值
 */
float CMD_HexReadFloat(const CmdHexDataPacket_T *packet, uint16_t offset)
{
    if (packet->dataLength < offset + 4 || packet->data == NULL)
    {
        return 0.0f;
    }
    return __CMD_HexReadFloat(packet->data + offset);
}

/**
 * @brief 从数据包特定偏移读取32位有符号整数
 * @param packet 数据包指针
 * @param offset 偏移量（字节）
 * @return int32_t值
 */
int32_t CMD_HexReadInt32(const CmdHexDataPacket_T *packet, uint16_t offset)
{
    if (packet->dataLength < offset + 4 || packet->data == NULL)
    {
        return 0;
    }
    return __CMD_HexReadInt32(packet->data + offset);
}

/**
 * @brief 从数据包特定偏移读取32位无符号整数
 * @param packet 数据包指针
 * @param offset 偏移量（字节）
 * @return uint32_t值
 */
uint32_t CMD_HexReadUint32(const CmdHexDataPacket_T *packet, uint16_t offset)
{
    if (packet->dataLength < offset + 4 || packet->data == NULL)
    {
        return 0;
    }
    return __CMD_HexReadUint32(packet->data + offset);
}

/**
 * @brief 从数据包特定偏移读取16位有符号整数
 * @param packet 数据包指针
 * @param offset 偏移量（字节）
 * @return int16_t值
 */
int16_t CMD_HexReadInt16(const CmdHexDataPacket_T *packet, uint16_t offset)
{
    if (packet->dataLength < offset + 2 || packet->data == NULL)
    {
        return 0;
    }
    return __CMD_HexReadInt16(packet->data + offset);
}

/**
 * @brief 从数据包特定偏移读取16位无符号整数
 * @param packet 数据包指针
 * @param offset 偏移量（字节）
 * @return uint16_t值
 */
uint16_t CMD_HexReadUint16(const CmdHexDataPacket_T *packet, uint16_t offset)
{
    if (packet->dataLength < offset + 2 || packet->data == NULL)
    {
        return 0;
    }
    return __CMD_HexReadUint16(packet->data + offset);
}

/**
 * @brief 从数据包特定偏移读取16位有符号整数
 * @param packet 数据包指针
 * @param offset 偏移量（字节）
 * @return int16_t值
 */
int8_t CMD_HexReadInt8(const CmdHexDataPacket_T *packet, uint16_t offset)
{
    if (packet->dataLength < offset + 1 || packet->data == NULL)
    {
        return 0;
    }
    return packet->data[offset];
}

/**
 * @brief 从数据包特定偏移读取16位无符号整数
 * @param packet 数据包指针
 * @param offset 偏移量（字节）
 * @return uint16_t值
 */
uint8_t CMD_HexReadUint8(const CmdHexDataPacket_T *packet, uint16_t offset)
{
    if (packet->dataLength < offset + 1 || packet->data == NULL)
    {
        return 0;
    }
    return packet->data[offset];
}

/**
 * @brief 将float值写入数据包的特定偏移
 * @param data 数据指针
 * @param offset 偏移量（字节）
 * @param value 要写入的float值
 */
void CMD_HexWriteDouble(CmdHexDataPacket_T *packet, uint16_t offset, double value)
{
    if (packet->dataLength < offset + 8 || packet->data == NULL)
    {
        return;
    }
    __CMD_HexWriteDouble(packet->data + offset, value);
}

/**
 * @brief 将float值写入数据包的特定偏移
 * @param data 数据指针
 * @param offset 偏移量（字节）
 * @param value 要写入的float值
 */
void CMD_HexWriteFloat(CmdHexDataPacket_T *packet, uint16_t offset, float value)
{
    if (packet->dataLength < offset + 4 || packet->data == NULL)
    {
        return;
    }
    __CMD_HexWriteFloat(packet->data + offset, value);
}

/**
 * @brief 将32位有符号整数写入数据包的特定偏移
 * @param data 数据指针
 * @param offset 偏移量（字节）
 * @param value 要写入的int32_t值
 */
void CMD_HexWriteInt32(CmdHexDataPacket_T *packet, uint16_t offset, int32_t value)
{
    if (packet->dataLength < offset + 4 || packet->data == NULL)
    {
        return;
    }
    __CMD_HexWriteInt32(packet->data + offset, value);
}

/**
 * @brief 将32位无符号整数写入数据包的特定偏移
 * @param data 数据指针
 * @param offset 偏移量（字节）
 * @param value 要写入的uint32_t值
 */
void CMD_HexWriteUint32(CmdHexDataPacket_T *packet, uint16_t offset, uint32_t value)
{
    if (packet->dataLength < offset + 4 || packet->data == NULL)
    {
        return;
    }
    __CMD_HexWriteUint32(packet->data + offset, value);
}

/**
 * @brief 将16位有符号整数写入数据包的特定偏移
 * @param data 数据指针
 * @param offset 偏移量（字节）
 * @param value 要写入的int16_t值
 */
void CMD_HexWriteInt16(CmdHexDataPacket_T *packet, uint16_t offset, int16_t value)
{
    if (packet->dataLength < offset + 2 || packet->data == NULL)
    {
        return;
    }
    __CMD_HexWriteInt16(packet->data + offset, value);
}

/**
 * @brief 将16位无符号整数写入数据包的特定偏移
 * @param data 数据指针
 * @param offset 偏移量（字节）
 * @param value 要写入的uint16_t值
 */
void CMD_HexWriteUint16(CmdHexDataPacket_T *packet, uint16_t offset, uint16_t value)
{
    if (packet->dataLength < offset + 2 || packet->data == NULL)
    {
        return;
    }
    __CMD_HexWriteUint16(packet->data + offset, value);
}

/**
 * @brief 将16位有符号整数写入数据包的特定偏移
 * @param data 数据指针
 * @param offset 偏移量（字节）
 * @param value 要写入的int16_t值
 */
void CMD_HexWriteInt8(CmdHexDataPacket_T *packet, uint16_t offset, int8_t value)
{
    if (packet->dataLength < offset + 1 || packet->data == NULL)
    {
        return;
    }
    packet->data[offset] = value;
}

/**
 * @brief 将16位无符号整数写入数据包的特定偏移
 * @param data 数据指针
 * @param offset 偏移量（字节）
 * @param value 要写入的uint16_t值
 */
void CMD_HexWriteUint8(CmdHexDataPacket_T *packet, uint16_t offset, uint8_t value)
{
    if (packet->dataLength < offset + 1 || packet->data == NULL)
    {
        return;
    }
    packet->data[offset] = value;
}

/* CRC32校验表 */
static uint32_t CRC32_Table[256] = {
    0x00000000L, 0x77073096L, 0xEE0E612CL, 0x990951BAL, 0x076DC419L,
    0x706AF48FL, 0xE963A535L, 0x9E6495A3L, 0x0EDB8832L, 0x79DCB8A4L,
    0xE0D5E91EL, 0x97D2D988L, 0x09B64C2BL, 0x7EB17CBDL, 0xE7B82D07L,
    0x90BF1D91L, 0x1DB71064L, 0x6AB020F2L, 0xF3B97148L, 0x84BE41DEL,
    0x1ADAD47DL, 0x6DDDE4EBL, 0xF4D4B551L, 0x83D385C7L, 0x136C9856L,
    0x646BA8C0L, 0xFD62F97AL, 0x8A65C9ECL, 0x14015C4FL, 0x63066CD9L,
    0xFA0F3D63L, 0x8D080DF5L, 0x3B6E20C8L, 0x4C69105EL, 0xD56041E4L,
    0xA2677172L, 0x3C03E4D1L, 0x4B04D447L, 0xD20D85FDL, 0xA50AB56BL,
    0x35B5A8FAL, 0x42B2986CL, 0xDBBBC9D6L, 0xACBCF940L, 0x32D86CE3L,
    0x45DF5C75L, 0xDCD60DCFL, 0xABD13D59L, 0x26D930ACL, 0x51DE003AL,
    0xC8D75180L, 0xBFD06116L, 0x21B4F4B5L, 0x56B3C423L, 0xCFBA9599L,
    0xB8BDA50FL, 0x2802B89EL, 0x5F058808L, 0xC60CD9B2L, 0xB10BE924L,
    0x2F6F7C87L, 0x58684C11L, 0xC1611DABL, 0xB6662D3DL, 0x76DC4190L,
    0x01DB7106L, 0x98D220BCL, 0xEFD5102AL, 0x71B18589L, 0x06B6B51FL,
    0x9FBFE4A5L, 0xE8B8D433L, 0x7807C9A2L, 0x0F00F934L, 0x9609A88EL,
    0xE10E9818L, 0x7F6A0DBBL, 0x086D3D2DL, 0x91646C97L, 0xE6635C01L,
    0x6B6B51F4L, 0x1C6C6162L, 0x856530D8L, 0xF262004EL, 0x6C0695EDL,
    0x1B01A57BL, 0x8208F4C1L, 0xF50FC457L, 0x65B0D9C6L, 0x12B7E950L,
    0x8BBEB8EAL, 0xFCB9887CL, 0x62DD1DDFL, 0x15DA2D49L, 0x8CD37CF3L,
    0xFBD44C65L, 0x4DB26158L, 0x3AB551CEL, 0xA3BC0074L, 0xD4BB30E2L,
    0x4ADFA541L, 0x3DD895D7L, 0xA4D1C46DL, 0xD3D6F4FBL, 0x4369E96AL,
    0x346ED9FCL, 0xAD678846L, 0xDA60B8D0L, 0x44042D73L, 0x33031DE5L,
    0xAA0A4C5FL, 0xDD0D7CC9L, 0x5005713CL, 0x270241AAL, 0xBE0B1010L,
    0xC90C2086L, 0x5768B525L, 0x206F85B3L, 0xB966D409L, 0xCE61E49FL,
    0x5EDEF90EL, 0x29D9C998L, 0xB0D09822L, 0xC7D7A8B4L, 0x59B33D17L,
    0x2EB40D81L, 0xB7BD5C3BL, 0xC0BA6CADL, 0xEDB88320L, 0x9ABFB3B6L,
    0x03B6E20CL, 0x74B1D29AL, 0xEAD54739L, 0x9DD277AFL, 0x04DB2615L,
    0x73DC1683L, 0xE3630B12L, 0x94643B84L, 0x0D6D6A3EL, 0x7A6A5AA8L,
    0xE40ECF0BL, 0x9309FF9DL, 0x0A00AE27L, 0x7D079EB1L, 0xF00F9344L,
    0x8708A3D2L, 0x1E01F268L, 0x6906C2FEL, 0xF762575DL, 0x806567CBL,
    0x196C3671L, 0x6E6B06E7L, 0xFED41B76L, 0x89D32BE0L, 0x10DA7A5AL,
    0x67DD4ACCL, 0xF9B9DF6FL, 0x8EBEEFF9L, 0x17B7BE43L, 0x60B08ED5L,
    0xD6D6A3E8L, 0xA1D1937EL, 0x38D8C2C4L, 0x4FDFF252L, 0xD1BB67F1L,
    0xA6BC5767L, 0x3FB506DDL, 0x48B2364BL, 0xD80D2BDAL, 0xAF0A1B4CL,
    0x36034AF6L, 0x41047A60L, 0xDF60EFC3L, 0xA867DF55L, 0x316E8EEFL,
    0x4669BE79L, 0xCB61B38CL, 0xBC66831AL, 0x256FD2A0L, 0x5268E236L,
    0xCC0C7795L, 0xBB0B4703L, 0x220216B9L, 0x5505262FL, 0xC5BA3BBEL,
    0xB2BD0B28L, 0x2BB45A92L, 0x5CB36A04L, 0xC2D7FFA7L, 0xB5D0CF31L,
    0x2CD99E8BL, 0x5BDEAE1DL, 0x9B64C2B0L, 0xEC63F226L, 0x756AA39CL,
    0x026D930AL, 0x9C0906A9L, 0xEB0E363FL, 0x72076785L, 0x05005713L,
    0x95BF4A82L, 0xE2B87A14L, 0x7BB12BAEL, 0x0CB61B38L, 0x92D28E9BL,
    0xE5D5BE0DL, 0x7CDCEFB7L, 0x0BDBDF21L, 0x86D3D2D4L, 0xF1D4E242L,
    0x68DDB3F8L, 0x1FDA836EL, 0x81BE16CDL, 0xF6B9265BL, 0x6FB077E1L,
    0x18B74777L, 0x88085AE6L, 0xFF0F6A70L, 0x66063BCAL, 0x11010B5CL,
    0x8F659EFFL, 0xF862AE69L, 0x616BFFD3L, 0x166CCF45L, 0xA00AE278L,
    0xD70DD2EEL, 0x4E048354L, 0x3903B3C2L, 0xA7672661L, 0xD06016F7L,
    0x4969474DL, 0x3E6E77DBL, 0xAED16A4AL, 0xD9D65ADCL, 0x40DF0B66L,
    0x37D83BF0L, 0xA9BCAE53L, 0xDEBB9EC5L, 0x47B2CF7FL, 0x30B5FFE9L,
    0xBDBDF21CL, 0xCABAC28AL, 0x53B39330L, 0x24B4A3A6L, 0xBAD03605L,
    0xCDD70693L, 0x54DE5729L, 0x23D967BFL, 0xB3667A2EL, 0xC4614AB8L,
    0x5D681B02L, 0x2A6F2B94L, 0xB40BBE37L, 0xC30C8EA1L, 0x5A05DF1BL,
    0x2D02EF8DL
};

/**
 * @brief 计算32位CRC校验值
 * @param data 指向待计算数据的指针，数据应为uint8_t类型
 * @param len 待计算数据的字节长度
 * @return uint32_t 返回32位CRC32校验值
 */
uint32_t CMD_CalcCRC32(const uint8_t *data, size_t len)
{
    uint32_t ctx = 0xFFFFFFFFL;
    for (size_t i = 0; i < len; i++)
    {
        ctx = (ctx >> 8) ^ CRC32_Table[(ctx & 0xFF) ^ *data++];
    }
    ctx ^= 0xFFFFFFFFUL;
    return ctx;
}
