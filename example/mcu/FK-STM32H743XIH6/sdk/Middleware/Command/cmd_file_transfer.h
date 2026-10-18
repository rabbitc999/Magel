#ifndef __CMD_FILE_TRANSFER_H
#define __CMD_FILE_TRANSFER_H
#include "cmd_hex.h"
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief 处理文件查询、分块上传下载、提交和中止，并缓存响应以支持重试。
 * @param port 命令通信通道编号。
 * @param packet 调用期间借用的命令包与载荷缓冲。
 */
void CMD_FileTransfer(uint16_t port, CmdHexDataPacket_T *packet);
/**
 * @brief 关闭超时传输并清除响应缓存，保留未完成的临时文件。
 */
void CMD_FileTransferPoll(void);
#ifdef __cplusplus
}
#endif
#endif
