#ifndef _FF_FILE_H
#define _FF_FILE_H

#include <stdint.h>
#include "bsp_system.h"

#define FILE_LIST_MAX_COUNT 256
#define FATFS_PATH_LEN_MAX  256

#ifdef __cplusplus
extern "C" {
#endif

typedef enum __FatFsStatus_T
{
    FATFS_RET_OK = 0,
    FATFS_RET_ERROR_MALLOC = 1,
    FATFS_RET_ERROR_FATFS = 2,
    FATFS_RET_ERROR_PARAMETER = 3
} FatFsStatus_T;

typedef enum __FatFileType_T
{
    FATFS_TYPE_FILE = 0,
    FATFS_TYPE_DIRECTORY = 1,
} FatFileType_T;

typedef enum
{
    DEVICE_W25Q64 = 0, /* 对应 "0:" */
    DEVICE_SD = 1,     /* 对应 "1:" */
    DEVICE_MAX
} FatFsDevice_T;

typedef struct __FatFsFileInfo_T
{
    char *Name;         /* 文件名 */
    char *fullPath;     /* 完整路径 */
    char *extension;    /* 后缀名 */
    FatFileType_T type; /* 文件类型 */
    uint32_t size;      /* 文件大小 */
} FatFsFileInfo_T;

typedef struct __FatFsList_T
{
    FatFsFileInfo_T *files; /* 文件数组 */
    uint16_t count;         /* 文件数量 */
} FatFsFileList_T;

typedef struct __FatFsCopyProgressInfo_T
{
    uint32_t totalSize;                       /* 总字节数 */
    uint32_t copiedSize;                      /* 已复制的字节数 */
    uint32_t fileCount;                       /* 总文件数 */
    uint32_t filesCopied;                     /* 已复制的文件数 */
    uint32_t fileCurrentTotalSize;            /* 当前文件的总字节数 */
    uint32_t fileCurrentCopiedSize;           /* 当前文件已复制的字节数 */
    char currentPath[FATFS_PATH_LEN_MAX + 1]; /* 当前正在复制的文件路径 */
    char *currentFile;                        /* 当前正在复制的文件名称（由currentPath通过指针偏移获取） */
    void (*callback)(struct __FatFsCopyProgressInfo_T *info);
} FatFsProgressInfo_T;

typedef void (*FatFsProgressCallback_T)(FatFsProgressInfo_T *info);

extern const char *g_FrTable[];
extern uint8_t g_FileMountFlag;

FatFsStatus_T FatFS_Init(void);
uint8_t FatFS_WildcardMatch(const char *pattern, const char *string);
FatFsStatus_T FatFS_FreeFileList(FatFsFileList_T *list);
FatFsStatus_T FatFS_GetFileInfo(const char *path, FatFsFileInfo_T *fileInfo);
FatFsStatus_T FatFS_GetFileList(const char *path, const char *pattern, FatFsFileList_T *list, uint8_t needClear);
FatFsStatus_T FatFS_GetListByExtension(const char *path, const char *extension, FatFsFileList_T *list, uint8_t needClear);
FatFsStatus_T FatFS_CreateDirectory(const char *dirPath);
FatFsStatus_T FatFS_DeleteDirectory(const char *dirPath);
FatFsStatus_T FatFS_DeleteFile(const char *path);
uint32_t FatFS_GetFileSize(const char *path);
FatFsStatus_T FatFS_GetFileName(const char *path, const char **Name);
FatFsStatus_T FatFS_GetExtension(const char *path, const char **extension);
void FatFS_ShowInfo(void);

uint32_t FatFS_ReadFile(const char *fileName, uint8_t *buffer, uint32_t size);
uint32_t FatFS_WriteFile(const char *fileName, uint8_t *buffer, uint32_t size);
uint32_t FatFS_AppendFile(const char *filePath, uint8_t *buffer, uint32_t size);
uint32_t FatFS_ReadFileAll(MemorySource_T source, const char *fileName, uint8_t **buffer);
uint32_t FatFS_ReadFileAtOffset(const char *fileName, uint8_t *buffer, uint32_t size, uint32_t offset);
uint32_t FatFS_WriteFileAtOffset(const char *fileName, uint8_t *buffer, uint32_t size, uint32_t offset);

typedef unsigned short WCHAR_T;

int UTF16_To_GB2312(const WCHAR_T *utf16_str, char *local_buf, int buf_size);
int GB2312_To_UTF16(const char *local_str, WCHAR_T *utf16_buf, int buf_size);
int UTF8_To_UTF16(const char *utf8, WCHAR_T *utf16, int max_len);
int UTF16_To_UTF8(const WCHAR_T *utf16, char *utf8, int max_len);
int UTF8_To_GB2312(const char *utf8_str, char *local_buf, int buf_size);
int GB2312_To_UTF8(const char *local_str, char *utf8_buf, int buf_size);

#ifdef __cplusplus
}
#endif

#endif
