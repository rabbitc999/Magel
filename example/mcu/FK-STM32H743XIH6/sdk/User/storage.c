#include "storage.h"
#include "bsp_system.h"
#include "ff.h"
#include "diskio.h"
#include <string.h>

#define STORAGE_BASE_SIZE    12345U
#define STORAGE_APPEND_SIZE  521U
#define STORAGE_PATCH_OFFSET 4093U
#define STORAGE_PATCH_SIZE   513U
#define STORAGE_FINAL_SIZE   (STORAGE_BASE_SIZE + STORAGE_APPEND_SIZE)

/* Foreground cooperative test. Never format or overwrite an existing file. */
static FIL s_StorageFile;
static uint8_t s_StorageBuffer[4097];
static char s_StoragePath[96];
static uint8_t s_StoragePort, s_StorageOpen, s_StorageVerifyOnly;
volatile uint32_t g_StoragePhase, g_StorageOffset, g_StorageResult;
volatile uint32_t g_StorageMaxStepMs, g_StorageGuardCRC;
static uint32_t s_StorageStart;

static uint8_t Storage_Pattern(uint32_t offset, uint8_t modified)
{
    uint8_t value = (uint8_t)((offset * 37U) ^ (offset >> 8) ^ 0xA5U);
    if (modified && offset >= STORAGE_PATCH_OFFSET && offset < STORAGE_PATCH_OFFSET + STORAGE_PATCH_SIZE)
    {
        value ^= 0x5A;
    }
    if (offset >= STORAGE_BASE_SIZE) { value ^= 0x3C; }
    return value;
}

static void Storage_Finish(uint32_t result)
{
    if (s_StorageOpen)
    {
        FRESULT closeResult = f_close(&s_StorageFile);
        s_StorageOpen = 0;
        if (result == FR_OK) { result = closeResult; }
    }
    g_StorageResult = result;
    UART_Printf(s_StoragePort, "storage %s result=%lu phase=%lu offset=%lu max_step_ms=%lu elapsed_ms=%lu path=%s\r\n",
                result == FR_OK ? "PASS" : "FAIL", result, g_StoragePhase, g_StorageOffset,
                g_StorageMaxStepMs, HAL_GetTick() - s_StorageStart, s_StoragePath);
    g_StoragePhase = 0;
}

uint8_t Storage_Command(uint8_t port, const char *operation, const char *path)
{
    if (g_StoragePhase != 0)
    {
        UART_Printf(port, "storage busy phase=%lu offset=%lu\r\n", g_StoragePhase, g_StorageOffset);
        return 0;
    }
    if (strcmp(operation, "info") == 0)
    {
        FATFS *fs = NULL;
        DWORD freeClusters = 0, eraseSectors = 0;
        WORD sectorSize = 0;
        LBA_t sectors = 0;
        uint32_t id = W25QXX_ReadId();
        FRESULT result = f_getfree("0:", &freeClusters, &fs);
        DRESULT r1 = disk_ioctl(0, GET_SECTOR_COUNT, &sectors);
        DRESULT r2 = disk_ioctl(0, GET_SECTOR_SIZE, &sectorSize);
        DRESULT r3 = disk_ioctl(0, GET_BLOCK_SIZE, &eraseSectors);
        UART_Printf(port, "storage JEDEC=%06lX flash_bytes=%lu disk_start=%08lX sectors=%lu sector_bytes=%u erase_sectors=%lu qspi_hz=%lu\r\n",
                    id, (uint32_t)W25QXX_FLASH_SIZE, (uint32_t)QSPI_DISK_START_ADDRESS, (uint32_t)sectors,
                    sectorSize, eraseSectors, HAL_RCC_GetHCLKFreq() / ((QUADSPI->CR >> 24) + 1U));
        UART_Printf(port, "storage mount_result=%u geometry_result=%u/%u/%u fat_type=%u free_bytes=%lu\r\n",
                    result, r1, r2, r3, fs ? fs->fs_type : 0,
                    fs ? (uint32_t)(freeClusters * fs->csize * sectorSize) : 0);
        return result == FR_OK && id == W25QXX_FLASH_ID && r1 == RES_OK && r2 == RES_OK && r3 == RES_OK;
    }
    if (strcmp(operation, "bounds") == 0)
    {
        DRESULT a = disk_read(0, s_StorageBuffer, QSPI_DISK_SECTOR_COUNT, 1);
        DRESULT b = disk_write(0, s_StorageBuffer, QSPI_DISK_SECTOR_COUNT - 1, 2);
        DRESULT c = disk_write(0, s_StorageBuffer, 0, 0);
        DRESULT d = disk_read(0, NULL, 0, 1);
        UART_Printf(port, "storage bounds=%u/%u/%u/%u expected=%u\r\n", a, b, c, d, RES_PARERR);
        return a == RES_PARERR && b == RES_PARERR && c == RES_PARERR && d == RES_PARERR;
    }
    s_StoragePort = port;
    if (strcmp(operation, "guard") == 0)
    {
        strcpy(s_StoragePath, "reserved-flash-read-only");
        g_StorageGuardCRC = 0xFFFFFFFFU;
        g_StoragePhase = 20;
    }
    else
    {
        if (path == NULL || strncmp(path, "0:/", 3) != 0 || strlen(path) >= sizeof(s_StoragePath)) { return 0; }
        if (strcmp(operation, "test") != 0 && strcmp(operation, "verify") != 0) { return 0; }
        strcpy(s_StoragePath, path);
        s_StorageVerifyOnly = strcmp(operation, "verify") == 0;
        g_StoragePhase = s_StorageVerifyOnly ? 7 : 1;
    }
    g_StorageOffset = 0;
    g_StorageResult = 0xFFFFFFFFU;
    g_StorageMaxStepMs = 0;
    s_StorageStart = HAL_GetTick();
    UART_Printf(port, "storage START operation=%s path=%s\r\n", operation, s_StoragePath);
    return 1;
}

void Storage_Poll(void)
{
    if (g_StoragePhase == 0) { return; }
    uint32_t start = HAL_GetTick();
    FRESULT result = FR_OK;
    UINT count = 0, length;
    uint8_t *buffer = s_StorageBuffer + 1; /* Exercise unaligned caller buffers. */
    switch (g_StoragePhase)
    {
    case 1:
        result = f_open(&s_StorageFile, s_StoragePath, FA_CREATE_NEW | FA_WRITE);
        if (result == FR_OK)
        {
            s_StorageOpen = 1;
            g_StoragePhase = 2;
        }
        break;
    case 2:
        length = g_StorageOffset == 0 ? 1023U : 4096U;
        if (length > STORAGE_BASE_SIZE - g_StorageOffset) { length = STORAGE_BASE_SIZE - g_StorageOffset; }
        for (UINT i = 0; i < length; i++) { buffer[i] = Storage_Pattern(g_StorageOffset + i, 0); }
        result = f_write(&s_StorageFile, buffer, length, &count);
        if (result == FR_OK && count != length) { result = FR_DISK_ERR; }
        g_StorageOffset += count;
        if (g_StorageOffset == STORAGE_BASE_SIZE) { g_StoragePhase = 3; }
        break;
    case 3:
        result = f_sync(&s_StorageFile);
        if (result == FR_OK)
        {
            result = f_close(&s_StorageFile);
            s_StorageOpen = 0;
        }
        if (result == FR_OK) { g_StoragePhase = 4; }
        break;
    case 4:
        result = f_open(&s_StorageFile, s_StoragePath, FA_READ);
        if (result == FR_OK)
        {
            s_StorageOpen = 1;
            g_StorageOffset = 0;
            g_StoragePhase = 5;
        }
        break;
    case 5:
    case 8:
    {
        uint8_t modified = g_StoragePhase == 8;
        uint32_t size = modified ? STORAGE_FINAL_SIZE : STORAGE_BASE_SIZE;
        if (f_size(&s_StorageFile) != size)
        {
            result = FR_INT_ERR;
            break;
        }
        length = size - g_StorageOffset;
        if (length > 4093U) { length = 4093U; }
        result = f_read(&s_StorageFile, buffer, length, &count);
        if (result == FR_OK && count != length) { result = FR_DISK_ERR; }
        for (UINT i = 0; result == FR_OK && i < count; i++)
        {
            if (buffer[i] != Storage_Pattern(g_StorageOffset + i, modified))
            {
                g_StorageOffset += i;
                result = FR_INT_ERR;
                break;
            }
        }
        if (result != FR_OK) { break; }
        g_StorageOffset += count;
        if (g_StorageOffset == size)
        {
            result = f_close(&s_StorageFile);
            s_StorageOpen = 0;
            if (result == FR_OK)
            {
                if (modified) { Storage_Finish(FR_OK); }
                else
                {
                    g_StoragePhase = 6;
                }
            }
        }
        break;
    }
    case 6:
        result = f_open(&s_StorageFile, s_StoragePath, FA_READ | FA_WRITE);
        if (result == FR_OK)
        {
            s_StorageOpen = 1;
            g_StoragePhase = 9;
        }
        break;
    case 9:
        result = f_lseek(&s_StorageFile, STORAGE_PATCH_OFFSET);
        for (UINT i = 0; i < STORAGE_PATCH_SIZE; i++) { buffer[i] = Storage_Pattern(STORAGE_PATCH_OFFSET + i, 1); }
        if (result == FR_OK) { result = f_write(&s_StorageFile, buffer, STORAGE_PATCH_SIZE, &count); }
        if (result == FR_OK && count != STORAGE_PATCH_SIZE) { result = FR_DISK_ERR; }
        if (result == FR_OK) { g_StoragePhase = 10; }
        break;
    case 10:
        result = f_lseek(&s_StorageFile, STORAGE_BASE_SIZE);
        for (UINT i = 0; i < STORAGE_APPEND_SIZE; i++) { buffer[i] = Storage_Pattern(STORAGE_BASE_SIZE + i, 1); }
        if (result == FR_OK) { result = f_write(&s_StorageFile, buffer, STORAGE_APPEND_SIZE, &count); }
        if (result == FR_OK && count != STORAGE_APPEND_SIZE) { result = FR_DISK_ERR; }
        if (result == FR_OK) { g_StoragePhase = 11; }
        break;
    case 11:
        result = f_sync(&s_StorageFile);
        if (result == FR_OK)
        {
            result = f_close(&s_StorageFile);
            s_StorageOpen = 0;
        }
        if (result == FR_OK) { g_StoragePhase = 7; }
        break;
    case 7:
        result = f_open(&s_StorageFile, s_StoragePath, FA_READ);
        if (result == FR_OK)
        {
            s_StorageOpen = 1;
            g_StorageOffset = 0;
            g_StoragePhase = 8;
        }
        break;
    case 20:
        length = QSPI_DISK_START_ADDRESS - g_StorageOffset;
        if (length > 4096U) { length = 4096U; }
        if (W25QXX_ReadBuffer(buffer, g_StorageOffset, length) != W25QXX_OK)
        {
            result = FR_DISK_ERR;
            break;
        }
        for (UINT i = 0; i < length; i++)
        {
            uint32_t crc = g_StorageGuardCRC ^ buffer[i];
            for (uint8_t bit = 0; bit < 8; bit++) { crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0); }
            g_StorageGuardCRC = crc;
        }
        g_StorageOffset += length;
        if (g_StorageOffset == QSPI_DISK_START_ADDRESS)
        {
            g_StorageGuardCRC ^= 0xFFFFFFFFU;
            UART_Printf(s_StoragePort, "storage reserved_crc=%08lX bytes=%lu\r\n", g_StorageGuardCRC, g_StorageOffset);
            Storage_Finish(FR_OK);
        }
        break;
    default: result = FR_INT_ERR; break;
    }
    uint32_t elapsed = HAL_GetTick() - start;
    if (elapsed > g_StorageMaxStepMs) { g_StorageMaxStepMs = elapsed; }
    if (result != FR_OK) { Storage_Finish(result); }
}
