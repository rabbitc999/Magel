#include "cmd_file_transfer.h"
#include "cmd_runtime.h"
#include "ff.h"
#include <string.h>
extern "C" uint32_t HAL_GetTick(void);
extern "C" uint64_t Timer_GetTimestampMs(void);

namespace
{
constexpr uint16_t ChunkSize = 512;
constexpr uint32_t TimeoutMs = 30000;
enum Status : uint8_t
{
    Ok,
    Format,
    Busy,
    Session,
    Offset,
    Crc,
    Size,
    FileSystem
};
struct Transfer
{
    FIL file;
    char path[96], temporary[104];
    uint32_t id, nextId, size, offset, crc, expectedCrc, lastTick;
    uint16_t port;
    uint8_t mode; /* 0 idle, 1 upload, 2 download. */
    bool cached;
    uint16_t cachedPort, requestLength, responseLength;
    uint8_t function, request[ChunkSize + 16], response[ChunkSize + 18];
};
Transfer transfer;
/**
 * @brief 从网络字节序缓冲读取一个 32 位无符号数。
 * @param p 至少包含四字节网络字节序整数的输入。
 * @return 读取的整数。
 */
uint32_t read32(const uint8_t *p)
{
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
/**
 * @brief 将一个 32 位无符号数写入网络字节序缓冲。
 * @param p 至少可容纳四字节整数的输出。
 * @param value 需要写入的 32 位整数。
 */
void write32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++) { p[i] = uint8_t(value >> (24 - 8 * i)); }
}
/**
 * @brief 将一段数据并入未取反的 CRC32 累计状态。
 * @param crc 尚未执行最终取反的 CRC32 累计值。
 * @param p 待并入 CRC 的输入字节。
 * @param bytes 本段输入数据的字节数。
 * @return 更新后的 CRC 状态，尚未执行最终取反。
 */
uint32_t crcUpdate(uint32_t crc, const uint8_t *p, size_t bytes)
{
    while (bytes--)
    {
        crc ^= *p++;
        for (unsigned i = 0; i < 8; i++) { crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320U : 0); }
    }
    return crc;
}
/**
 * @brief 验证并复制限定于 0:/ 的 ASCII 路径，拒绝越级与非法分段。
 * @param path 接收校验通过路径的输出缓冲，至少为 96 字节。
 * @param data 不含结束符的路径字节。
 * @param bytes 本段输入数据的字节数。
 * @return 路径有效且完成复制时为 true，否则为 false。
 */
bool pathRead(char *path, const uint8_t *data, size_t bytes)
{
    if (bytes < 4 || bytes >= 96 || memcmp(data, "0:/", 3)) { return false; }
    for (size_t i = 3; i < bytes; i++)
    {
        if (data[i] < 32 || data[i] > 126 || data[i] == '\\' || data[i] == ':' || data[i] == '*' || data[i] == '?' || data[i] == '"' || data[i] == '<' || data[i] == '>' || data[i] == '|') { return false; }
    }
    memcpy(path, data, bytes);
    path[bytes] = 0;
    const char *part = path + 3;
    while (*part)
    {
        const char *end = strchr(part, '/');
        size_t length = end ? size_t(end - part) : strlen(part);
        if (length == 0 || (length == 1 && part[0] == '.') || (length == 2 && part[0] == '.' && part[1] == '.') || part[length - 1] == ' ' || part[length - 1] == '.') { return false; }
        if (!end) { return true; }
        part = end + 1;
    }
    return false;
}
/**
 * @brief 给文件传输响应设置响应位并通过命令运行时发送。
 * @param port 命令通信通道编号。
 * @param function 文件传输功能编号；响应时自动设置最高位。
 * @param data 待处理的数据缓冲。
 * @param length 数据缓冲中有效内容的字节数。
 */
void send(uint16_t port, uint8_t function, uint8_t *data, uint16_t length)
{
    CmdHexDataPacket_T response = { 0x12, uint8_t(function | 0x80), data, length, 0 };
    CMD_RuntimeSendHex(port, &response, Timer_GetTimestampMs());
}
/**
 * @brief 关闭活动的文件传输句柄并清除传输模式。
 */
void release(void)
{
    if (transfer.mode) { f_close(&transfer.file); }
    transfer.mode = 0;
}
}  // namespace

/**
 * @brief 关闭超时传输并清除响应缓存，保留未完成的临时文件。
 */
extern "C" void CMD_FileTransferPoll(void)
{
    if ((transfer.mode || transfer.cached) && uint32_t(HAL_GetTick() - transfer.lastTick) >= TimeoutMs)
    {
        release(); /* Retain an incomplete .part file; never publish it as complete. */
        transfer.cached = false;
    }
}

/**
 * @brief 处理文件查询、分块上传下载、提交和中止，并缓存响应以支持重试。
 * @param port 命令通信通道编号。
 * @param packet 调用期间借用的命令包与载荷缓冲。
 */
extern "C" void CMD_FileTransfer(uint16_t port, CmdHexDataPacket_T *packet)
{
    if (!packet || !packet->data || packet->dataLength < 4 || packet->dataLength > sizeof(transfer.request)) { return; }
    CMD_FileTransferPoll();
    uint8_t *p = packet->data;
    uint16_t length = packet->dataLength;
    uint8_t function = packet->funcId;
    uint8_t reply[ChunkSize + 18] = {};
    memcpy(reply, p, 4);
    uint16_t replyLength = 6;
    if (transfer.mode && port != transfer.port)
    {
        reply[4] = Busy;
        send(port, function, reply, replyLength);
        return;
    }
    if (transfer.cached && transfer.cachedPort == port && transfer.function == function && transfer.requestLength == length && !memcmp(transfer.request, p, length))
    {
        transfer.lastTick = HAL_GetTick();
        send(port, function, transfer.response, transfer.responseLength);
        return;
    }
    Status status = Ok;
    FRESULT result = FR_OK;
    char path[96];
    FILINFO info;
    if (function == 0) /* stat */
    {
        if (!pathRead(path, p + 4, length - 4)) { status = Format; }
        else
        {
            result = f_stat(path, &info);
            if (result == FR_OK)
            {
                write32(reply + 6, uint32_t(info.fsize));
                reply[10] = info.fattrib;
                replyLength = 11;
            }
        }
    }
    else if (function == 1 || function == 2) /* begin upload/download */
    {
        uint16_t prefix = function == 1 ? 12 : 4;
        if (length <= prefix || !pathRead(path, p + prefix, length - prefix)) { status = Format; }
        else if (transfer.mode) { status = Busy; }
        else
        {
            if (function == 1)
            {
                result = f_stat(path, &info);
                if (result == FR_OK) { result = FR_EXIST; }
                else if (result == FR_NO_FILE)
                {
                    strcpy(transfer.temporary, path);
                    strcat(transfer.temporary, ".part");
                    result = f_open(&transfer.file, transfer.temporary, FA_WRITE | FA_CREATE_NEW);
                }
            }
            else
            {
                result = f_open(&transfer.file, path, FA_READ);
            }
            if (result == FR_OK)
            {
                transfer.mode = function;
                transfer.port = port;
                transfer.id = ++transfer.nextId;
                if (!transfer.id) { transfer.id = ++transfer.nextId; }
                strcpy(transfer.path, path);
                transfer.size = function == 1 ? read32(p + 4) : uint32_t(f_size(&transfer.file));
                transfer.expectedCrc = function == 1 ? read32(p + 8) : 0;
                transfer.offset = 0;
                transfer.crc = 0xFFFFFFFFU;
                write32(reply + 6, transfer.id);
                write32(reply + 10, transfer.size);
                reply[14] = uint8_t(ChunkSize >> 8);
                reply[15] = uint8_t(ChunkSize);
                replyLength = 16;
            }
        }
    }
    else if (function >= 3 && function <= 6)
    {
        if (length < 8) { status = Format; }
        else if (!transfer.mode || read32(p + 4) != transfer.id) { status = Session; }
        else if (function == 3) /* upload chunk: id, offset, data CRC32, data */
        {
            if (transfer.mode != 1 || length <= 16 || length > 16 + ChunkSize) { status = Format; }
            else if (read32(p + 8) != transfer.offset) { status = Offset; }
            else if (length - 16U > transfer.size - transfer.offset) { status = Size; }
            else if ((crcUpdate(0xFFFFFFFFU, p + 16, length - 16) ^ 0xFFFFFFFFU) != read32(p + 12)) { status = Crc; }
            else
            {
                UINT written = 0;
                result = f_write(&transfer.file, p + 16, length - 16, &written);
                if (result == FR_OK && written != length - 16U) { result = FR_DISK_ERR; }
                if (result == FR_OK)
                {
                    transfer.crc = crcUpdate(transfer.crc, p + 16, written);
                    transfer.offset += written;
                    write32(reply + 6, transfer.offset);
                    replyLength = 10;
                }
                else
                {
                    release();
                }
            }
        }
        else if (function == 4) /* download chunk: id, offset, requested count */
        {
            if (transfer.mode != 2 || length != 14) { status = Format; }
            else
            {
                uint32_t offset = read32(p + 8);
                uint16_t count = uint16_t((p[12] << 8) | p[13]);
                if (!count || count > ChunkSize || offset > transfer.size) { status = Offset; }
                else
                {
                    if (count > transfer.size - offset) { count = uint16_t(transfer.size - offset); }
                    result = f_lseek(&transfer.file, offset);
                    UINT got = 0;
                    if (result == FR_OK) { result = f_read(&transfer.file, reply + 18, count, &got); }
                    if (result == FR_OK && got != count) { result = FR_DISK_ERR; }
                    if (result == FR_OK)
                    {
                        write32(reply + 6, transfer.id);
                        write32(reply + 10, offset);
                        write32(reply + 14, crcUpdate(0xFFFFFFFFU, reply + 18, got) ^ 0xFFFFFFFFU);
                        replyLength = uint16_t(18 + got);
                    }
                }
            }
        }
        else if (length != 8) { status = Format; }
        else if (function == 5) /* commit upload or close download */
        {
            if (transfer.mode == 1 && transfer.offset != transfer.size) { status = Size; }
            else if (transfer.mode == 1 && (transfer.crc ^ 0xFFFFFFFFU) != transfer.expectedCrc) { status = Crc; }
            else
            {
                bool upload = transfer.mode == 1;
                if (upload) { result = f_sync(&transfer.file); }
                FRESULT closeResult = f_close(&transfer.file);
                transfer.mode = 0;
                if (result == FR_OK) { result = closeResult; }
                if (result == FR_OK && upload) { result = f_rename(transfer.temporary, transfer.path); }
                if (result == FR_OK)
                {
                    write32(reply + 6, transfer.size);
                    replyLength = 10;
                }
            }
        }
        else /* abort */
        {
            bool upload = transfer.mode == 1;
            result = f_close(&transfer.file);
            transfer.mode = 0;
            if (result == FR_OK && upload) { result = f_unlink(transfer.temporary); }
        }
    }
    else
    {
        status = Format;
    }
    if (result != FR_OK) { status = FileSystem; }
    reply[4] = status;
    reply[5] = uint8_t(result);
    if (status != Ok) { replyLength = 6; }
    transfer.lastTick = HAL_GetTick();
    transfer.cached = true;
    transfer.cachedPort = port;
    transfer.function = function;
    transfer.requestLength = length;
    memcpy(transfer.request, p, length);
    transfer.responseLength = replyLength;
    memcpy(transfer.response, reply, replyLength);
    send(port, function, transfer.response, replyLength);
}
