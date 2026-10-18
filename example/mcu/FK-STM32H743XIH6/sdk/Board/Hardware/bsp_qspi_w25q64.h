#ifndef __BSP_W25QXX_H
#define __BSP_W25QXX_H

#include "bsp_system.h"

/*----------------------------------------------- 命名参数宏 -------------------------------------------*/

#define W25QXX_OK                          0  /* W25Q64通信正常 */
#define W25QXX_ERROR_INIT                  -1 /* 初始化错误 */
#define W25QXX_ERROR_WRITE_ENABLE          -2 /* 写使能错误 */
#define W25QXX_ERROR_AUTOPOLLING           -3 /* 轮询等待错误，无响应 */
#define W25QXX_ERROR_ERASE                 -4 /* 擦除错误 */
#define W25QXX_ERROR_TRANSMIT              -5 /* 传输错误 */
#define W25QXX_ERROR_MEMORT_MAPPED         -6 /* 内存映射模式错误 */

#define W25QXX_CMD_ENABLE_RESET            0x66 /* 使能复位 */
#define W25QXX_CMD_RESET_DEVICE            0x99 /* 复位器件 */
#define W25QXX_CMD_JEDEC_ID                0x9F /* JEDEC ID */
#define W25QXX_CMD_WRITE_ENABLE            0X06 /* 写使能 */

#define W25QXX_CMD_SECTOR_ERASE            0x20 /* 扇区擦除，4K字节， 参考擦除时间 45ms */
#define W25QXX_CMD_BLOCK_ERASE_32K         0x52 /* 块擦除，  32K字节，参考擦除时间 120ms */
#define W25QXX_CMD_BLOCK_ERASE_64K         0xD8 /* 块擦除，  64K字节，参考擦除时间 150ms */
#define W25QXX_CMD_CHIP_ERASE              0xC7 /* 整片擦除，参考擦除时间 20S */

#define W25QXX_CMD_QUAD_INPUT_PAGE_PROGRAM 0x32 /* 1-1-4模式下(1线指令1线地址4线数据)，页编程指令，参考写入时间 0.4ms */
#define W25QXX_CMD_FAST_READ_QUAD_IO       0xEB /* 1-4-4模式下(1线指令4线地址4线数据)，快速读取指令 */

#define W25QXX_CMD_READ_STATUS_REG1        0X05 /* 读状态寄存器1 */
#define W25QXX_STATUS_REG1_BUSY            0x01 /* 读状态寄存器1的第0位(只读)，Busy标志位，当正在擦除/写入数据/写命令时会被置1 */
#define W25QXX_STATUS_REG1_WEL             0x02 /* 读状态寄存器1的第1位(只读)，WEL写使能标志位，该标志位为1时，代表可以进行写操作 */

/*---------------------------------------------- 内存地址分配 -----------------------------------------*/
#define W25QXX_PAGE_SIZE                   256        /* 页大小，256字节 */
#define W25QXX_FLASH_SIZE                  0x00800000 /* W25Q64大小，8M字节 */
#define W25QXX_FLASH_ID                    0x00EF4017 /* W25Q64 JEDEC ID */
#define W25QXX_CHIP_ERASE_TIMEOUT_MAX      100000U    /* 超时等待时间，W25Q64整片擦除所需最大时间是100S */
#define W25QXX_MEM_ADDR                    0x90000000 /* 内存映射模式的地址 */
#define QSPI_SECTOR_SIZE                   (1 << 12)  /* 扇区大小 */
#define QSPI_DISK_START_ADDRESS            0x00247000 /* FATFS文件系统开始内存地址 */
#define QSPI_DISK_SIZE                     (W25QXX_FLASH_SIZE - QSPI_DISK_START_ADDRESS)
#define QSPI_DISK_SECTOR_COUNT             ((QSPI_DISK_SIZE) / (QSPI_SECTOR_SIZE))
#define QSPI_ADDR_PARAM                    0x00246000
#define QSPI_ADDR_CHINESE_FONT             0x001A0000

/*----------------------------------------------- 函数声明 ---------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif
int8_t W25QXX_Init(void);             /* W25Qxx初始化 */
int8_t W25QXX_Reset(void);            /* 复位器件 */
uint32_t W25QXX_ReadId(void);         /* 读取器件ID */
int8_t W25QXX_MemoryMappedMode(void); /* 进入内存映射模式 */

int8_t W25QXX_SectorErase(uint32_t sectorAddress);   /* 扇区擦除，4K字节， 参考擦除时间 45ms */
int8_t W25QXX_BlockErase32k(uint32_t sectorAddress); /* 块擦除，  32K字节，参考擦除时间 120ms */
int8_t W25QXX_BlockErase64k(uint32_t sectorAddress); /* 块擦除，  64K字节，参考擦除时间 150ms，实际建议使用64K擦除，擦除的时间最快 */
int8_t W25QXX_ChipErase(void);                       /* 整片擦除，参考擦除时间 20S */

int8_t W25QXX_WritePage(uint8_t *pBuffer, uint32_t writeAddr, uint16_t numByteToWrite); /* 按页写入，最大256字节 */
int8_t W25QXX_ReadBuffer(uint8_t *pBuffer, uint32_t readAddr, uint32_t numByteToRead);  /* 读取数据，最大不能超过flash芯片的大小 */
int8_t W25QXX_EraseBySectorRange(uint32_t startSector, uint32_t sectorCount);           /* 扇区擦除函数 */
int8_t W25QXX_WriteBuffer(uint8_t *pData, uint32_t writeAddr, uint32_t size);           /* 自动写入缓冲区 */

#ifdef __cplusplus
}
#endif

#endif /* W25QXX_H */
