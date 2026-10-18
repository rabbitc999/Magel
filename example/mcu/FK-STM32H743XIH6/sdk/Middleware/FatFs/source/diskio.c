/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2025        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"     /* Basic definitions of FatFs */
#include "diskio.h" /* Declarations FatFs MAI */

/* Example: Declarations of the platform and disk functions in the project */
#include "bsp_system.h"

/* Example: Mapping of physical drive number for each drive */
#define DEV_FLASH                     0 /* Map W25Q64 to physical drive 0 */

#define W25Q64_SPI_DEFAULT_BLOCK_SIZE 1

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/
DSTATUS disk_status(
    BYTE pdrv /* Physical drive nmuber to identify the drive */
)
{
    switch (pdrv)
    {
    case DEV_FLASH:
    {
        if (W25QXX_ReadId() == W25QXX_FLASH_ID) { return 0; }
        return STA_NOINIT;
    }
    }
    return STA_NOINIT;
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/
DSTATUS disk_initialize(
    BYTE pdrv /* Physical drive nmuber to identify the drive */
)
{
    switch (pdrv)
    {
    case DEV_FLASH:
    {
        if (W25QXX_ReadId() == W25QXX_FLASH_ID) { return 0; }
        return STA_NOINIT;
    }
    }
    return STA_NOINIT;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
DRESULT disk_read(
    BYTE pdrv,    /* Physical drive nmuber to identify the drive */
    BYTE *buff,   /* Data buffer to store read data */
    LBA_t sector, /* Start sector in LBA */
    UINT count    /* Number of sectors to read */
)
{
    switch (pdrv)
    {
    case DEV_FLASH:
    {
        if (buff == NULL || count == 0 || sector >= QSPI_DISK_SECTOR_COUNT || count > QSPI_DISK_SECTOR_COUNT - sector)
        {
            return RES_PARERR;
        }
        uint32_t read_addr = QSPI_DISK_START_ADDRESS + (sector << 12);
        if (W25QXX_ReadBuffer(buff, read_addr, count << 12) == W25QXX_OK) /* 以4K字节为单位 */
        {
            return RES_OK;
        }
        return RES_ERROR;
    }
    }

    return RES_PARERR;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
#if FF_FS_READONLY == 0

DRESULT disk_write(
    BYTE pdrv,        /* Physical drive nmuber to identify the drive */
    const BYTE *buff, /* Data to be written */
    LBA_t sector,     /* Start sector in LBA */
    UINT count        /* Number of sectors to write */
)
{
    switch (pdrv)
    {
    case DEV_FLASH:
    {
        if (buff == NULL || count == 0 || sector >= QSPI_DISK_SECTOR_COUNT || count > QSPI_DISK_SECTOR_COUNT - sector)
        {
            return RES_PARERR;
        }
        uint32_t total_bytes = count * 4096;                            /* 总字节数(假设扇区大小为4096字节) */
        uint32_t write_addr = QSPI_DISK_START_ADDRESS + (sector << 12); /* 计算实际写入地址 */

        /* 执行实际写入操作 */
        if (W25QXX_WriteBuffer((uint8_t *)buff, write_addr, total_bytes) == W25QXX_OK)
        {
            return RES_OK;
        }
        return RES_ERROR;
    }
    }

    return RES_PARERR;
}

#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/
DRESULT disk_ioctl(
    BYTE pdrv, /* Physical drive nmuber (0..) */
    BYTE cmd,  /* Control code */
    void *buff /* Buffer to send/receive control data */
)
{
    if (cmd != CTRL_SYNC && buff == NULL) { return RES_PARERR; }
    switch (pdrv)
    {
    case DEV_FLASH:
    {
        switch (cmd)
        {
        case CTRL_SYNC:
            return RES_OK;

        case GET_SECTOR_COUNT:
            *(DWORD *)buff = QSPI_DISK_SECTOR_COUNT; /* 总的扇区数 */
            return RES_OK;

        case GET_SECTOR_SIZE:
            *(WORD *)buff = QSPI_SECTOR_SIZE; /* 定义一个扇区大小为4K */
            return RES_OK;

        case GET_BLOCK_SIZE:
            *(DWORD *)buff = W25Q64_SPI_DEFAULT_BLOCK_SIZE;
            return RES_OK;

        default:
            return RES_ERROR;
        }
    }
    }

    return RES_ERROR;
}
