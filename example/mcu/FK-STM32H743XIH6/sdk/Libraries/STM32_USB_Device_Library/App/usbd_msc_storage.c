/**
 * *****************************************************************************
 * @file    usbd_msc_storage_template.c
 * @author  MCD Application Team
 * @brief   Memory management layer
 * *****************************************************************************
 * @attention
 *
 * Copyright (c) 2015 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 * *****************************************************************************
 */

/* BSPDependencies
- "stm32xxxxx_{eval}{discovery}{nucleo_144}.c"
- "stm32xxxxx_{eval}{discovery}_io.c"
- "stm32xxxxx_{eval}{discovery}{adafruit}_sd.c"
EndBSPDependencies */

/* Includes ------------------------------------------------------------------*/
#include "usbd_msc_storage.h"
#include "bsp_qspi_w25q64.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define DEV_FLASH       0 /* Map FTL to physical drive 0 */
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
/* Extern function prototypes ------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
#define STORAGE_LUN_NBR 2U
#define STORAGE_BLK_NBR 0x10000U
#define STORAGE_BLK_SIZ 0x200U

int8_t STORAGE_Init(uint8_t lun);

int8_t STORAGE_GetCapacity(uint8_t lun, uint32_t *block_num,
                           uint16_t *block_size);

int8_t STORAGE_IsReady(uint8_t lun);

int8_t STORAGE_IsWriteProtected(uint8_t lun);

int8_t STORAGE_Read(uint8_t lun, uint8_t *buf, uint32_t blk_addr,
                    uint16_t blk_len);

int8_t STORAGE_Write(uint8_t lun, uint8_t *buf, uint32_t blk_addr,
                     uint16_t blk_len);

int8_t STORAGE_GetMaxLun(void);

/* USB Mass storage Standard Inquiry Data */
int8_t STORAGE_Inquirydata[] = /* 36 */
    {
        /* LUN 0 */
        0x00,
        0x80,
        0x02,
        0x02,
        (STANDARD_INQUIRY_DATA_LEN - 5),
        0x00,
        0x00,
        0x00,
        'R',
        'A',
        'B',
        'B',
        'I',
        'T',
        'C',
        ' ', /* Manufacturer : 8 bytes */
        'W',
        '2',
        '5',
        'Q',
        '6',
        '4',
        ' ',
        'S', /* Product      : 16 Bytes */
        'P',
        'I',
        ' ',
        'F',
        'L',
        'A',
        'S',
        'H',
        '0',
        '.',
        '0',
        '1', /* Version      : 4 Bytes */

        /* LUN 1 */
        0x00,
        0x80,
        0x02,
        0x02,
        (STANDARD_INQUIRY_DATA_LEN - 5),
        0x00,
        0x00,
        0x00,
        'R',
        'A',
        'B',
        'B',
        'I',
        'T',
        'C',
        ' ', /* Manufacturer : 8 bytes */
        'S',
        'D',
        'C',
        'A',
        'R',
        'D',
        ' ',
        ' ', /* Product      : 16 Bytes */
        ' ',
        ' ',
        ' ',
        ' ',
        ' ',
        ' ',
        ' ',
        ' ',
        '0',
        '.',
        '0',
        '1', /* Version      : 4 Bytes */
    };

USBD_StorageTypeDef USBD_MSC_Interface_fops_FS = {
    STORAGE_Init,
    STORAGE_GetCapacity,
    STORAGE_IsReady,
    STORAGE_IsWriteProtected,
    STORAGE_Read,
    STORAGE_Write,
    STORAGE_GetMaxLun,
    STORAGE_Inquirydata,
};

/**
 * @brief  Initializes the storage unit (medium)
 * @param  lun: Logical unit number
 * @retval Status (0 : OK / -1 : Error)
 */
int8_t STORAGE_Init(uint8_t lun)
{
    switch (lun)
    {
    case DEV_FLASH:
    {
        if (W25QXX_ReadId() == W25QXX_FLASH_ID) { return 0; }
        return -1;
    }
    }
    return -1;
}

/**
 * @brief  Returns the medium capacity.
 * @param  lun: Logical unit number
 * @param  block_num: Number of total block number
 * @param  block_size: Block size
 * @retval Status (0: OK / -1: Error)
 */
int8_t STORAGE_GetCapacity(uint8_t lun, uint32_t *block_num, uint16_t *block_size)
{
    switch (lun)
    {
    case DEV_FLASH:
    {
        *block_num = QSPI_DISK_SECTOR_COUNT;
        *block_size = QSPI_SECTOR_SIZE;
        return 0;
    }
    }
    return -1;
}

/**
 * @brief  Checks whether the medium is ready.
 * @param  lun: Logical unit number
 * @retval Status (0: OK / -1: Error)
 */
int8_t STORAGE_IsReady(uint8_t lun)
{
    return 0;
}

/**
 * @brief  Checks whether the medium is write protected.
 * @param  lun: Logical unit number
 * @retval Status (0: write enabled / -1: otherwise)
 */
int8_t STORAGE_IsWriteProtected(uint8_t lun)
{
    UNUSED(lun);

    return 0;
}

/**
 * @brief  Reads data from the medium.
 * @param  lun: Logical unit number
 * @param  buf: data buffer
 * @param  blk_addr: Logical block address
 * @param  blk_len: Blocks number
 * @retval Status (0: OK / -1: Error)
 */
int8_t STORAGE_Read(uint8_t lun, uint8_t *buf,
                    uint32_t blk_addr, uint16_t blk_len)
{
    switch (lun)
    {
    case DEV_FLASH:
    {
        uint32_t read_addr = QSPI_DISK_START_ADDRESS + (blk_addr << 12);   /* 以4K字节为单位 */
        if (W25QXX_ReadBuffer(buf, read_addr, blk_len << 12) == W25QXX_OK) /* 以4K字节为单位 */
        {
            return 0;
        }
        return -1;
    }
    }

    return -1;
}

/**
 * @brief  Writes data into the medium.
 * @param  lun: Logical unit number
 * @param  buf: data buffer
 * @param  blk_addr: Logical block address
 * @param  blk_len: Blocks number
 * @retval Status (0 : OK / -1 : Error)
 */
int8_t STORAGE_Write(uint8_t lun, uint8_t *buf,
                     uint32_t blk_addr, uint16_t blk_len)
{
    switch (lun)
    {
    case DEV_FLASH:
    {
        uint32_t total_bytes = blk_len * 4096;                            /* 总字节数(假设扇区大小为4096字节) */
        uint32_t write_addr = QSPI_DISK_START_ADDRESS + (blk_addr << 12); /* 计算实际写入地址 */

        /* 执行实际写入操作 */
        if (W25QXX_WriteBuffer((uint8_t *)buf, write_addr, total_bytes) == W25QXX_OK)
        {
            return 0;
        }
        return -1;
    }
    }

    return -1;
}

/**
 * @brief  Returns the Max Supported LUNs.
 * @param  None
 * @retval Lun(s) number
 */
int8_t STORAGE_GetMaxLun(void)
{
    return 0;
}
