#ifndef __CMD_CONFIG_H
#define __CMD_CONFIG_H

/* 错误码 */
#define CMD_NO_ERROR               0
#define CMD_ERROR_OTHER            -1
#define CMD_ERROR_TYPE             -3
#define CMD_ERROR_EXCUTE           -4
#define CMD_ERROR_COMMAND          -5
#define CMD_ERROR_PARAMENER_FORMAT -6
#define CMD_ERROR_PARAMENER_COUNT  -7
#define CMD_ERROR_BUFFER_OVERFLOW  -8

/* 串口相关 */
#define CMD_COM_COUNT              7

/* 错误码定义 */
#define CMD_HEX_ERROR_DATA_LENGTH  0x01 /* 数据长度错误 */
#define CMD_HEX_ERROR_DATA_FORMAT  0x02 /* 时间戳无效 */
#define CMD_HEX_ERROR_UNKNOWN_CMD  0x03 /* 未知命令 */

/* 内存分配大小 */
#define CMD_STR_BUFFER_SIZE        256
#define CMD_STR_MAX_REG_COUNT      40 /* 最大命令节点数 */
#define CMD_STR_MAX_VARIABLE_COUNT 5
#define CMD_STR_MAX_COMMAND_LENGTH 10

/* HEX命令模式 */
#define CMD_HEX_CALLBACK_OK        0 /* 回应成功 */
#define CMD_HEX_CALLBACK_ERROR     1 /* 回应错误 */
#define CMD_HEX_CALLBACK_DATA      2 /* 回应数据 */
#define CMD_HEX_CALLBACK_CHECK     3 /* 校验错误 */

/* 命令相关 */
#define CMD_STR_VARIABLE_PARAMS    0x7F /* 命令可变标志 */
#define CMD_HEX_HEADER_1           0x55
#define CMD_HEX_HEADER_2           0xAA
#define CMD_HEX_FOOTER_1           0xFF
#define CMD_HEX_FOOTER_2           0xFE
#define CMD_HEX_CHECK_ERROR_CMD    0xFF /* 校验失败命令回调 */

#define CMD_HEX_MAX_PAYLOAD        4084
#define CMD_RUNTIME_ARENA_SIZE     (64 * 1024)
#define CMD_RUNTIME_TIMEOUT_MS     1000

#endif
