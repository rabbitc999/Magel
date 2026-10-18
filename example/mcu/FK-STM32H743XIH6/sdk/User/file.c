#include <string.h>
#include "file.h"
#include "bsp_system.h"
#include "ff.h"

#define FATFS_COPY_BUFFER_SIZE 4096 /* 文件复制缓冲区大小 */

/* FatFs API的返回值 */
const char *g_FrTable[] = {
    "FR_OK:成功",                                           /* (0) Succeeded */
    "FR_DISK_ERR:底层硬件错误",                             /* (1) A hard error occurred in the low level disk I/O layer */
    "FR_INT_ERR:断言失败",                                  /* (2) Assertion failed */
    "FR_NOT_READY:物理驱动没有工作",                        /* (3) The physical drive cannot work */
    "FR_NO_FILE:文件不存在",                                /* (4) Could not find the file */
    "FR_NO_PATH:路径不存在",                                /* (5) Could not find the path */
    "FR_INVALID_NAME:无效文件名",                           /* (6) The path Name format is invalid */
    "FR_DENIED:由于禁止访问或者目录已满访问被拒绝",         /* (7) Access denied due to prohibited access or directory full */
    "FR_EXIST:文件已经存在",                                /* (8) Access denied due to prohibited access */
    "FR_INVALID_OBJECT:文件或者目录对象无效",               /* (9) The file/directory object is invalid */
    "FR_WRITE_PROTECTED:物理驱动被写保护",                  /* (10) The physical drive is write protected */
    "FR_INVALID_DRIVE:逻辑驱动号无效",                      /* (11) The logical drive number is invalid */
    "FR_NOT_ENABLED:卷中无工作区",                          /* (12) The volume has no work area */
    "FR_NO_FILESYSTEM:没有有效的FAT卷",                     /* (13) There is no valid FAT volume */
    "FR_MKFS_ABORTED:由于参数错误f_mkfs()被终止",           /* (14) The f_mkfs() aborted due to any parameter error */
    "FR_TIMEOUT:在规定的时间内无法获得访问卷的许可",        /* (15) Could not get a grant to access the volume within defined period */
    "FR_LOCKED:由于文件共享策略操作被拒绝",                 /* (16) The operation is rejected according to the file sharing policy */
    "FR_NOT_ENOUGH_CORE:无法分配长文件名工作区",            /* (17) LFN working buffer could not be allocated */
    "FR_TOO_MANY_OPEN_FILES:当前打开的文件数大于_FS_SHARE", /* (18) Number of open files > _FS_SHARE */
    "FR_INVALID_PARAMETER:参数无效"                         /* (19) Given parameter is invalid */
};

static FATFS s_W25q64FileSystem = { 0 }; /* 定义文件系统对象 */
// static SemaphoreHandle_t xFatFsMutex;
uint8_t g_FileMountFlag = 0;

/**
 * @brief  初始化文件系统
 * @retval 状态码：FATFS_RET_OK 成功，其他值失败
 */
FatFsStatus_T FatFS_Init(void)
{
    FILINFO fileInfo; /* 用于存储文件信息的结构体 */

    /* 挂载QSPI文件系统 */
    if (f_mount(&s_W25q64FileSystem, "0:", 1) != FR_OK)
    {
        RTC_WriteBKP(0x11111234);
        HAL_Delay(500);
        NVIC_SystemReset();
    }
    else
    {
        g_FileMountFlag = 1;
    }

    //    /* 创建互斥锁 */
    //    xFatFsMutex = xSemaphoreCreateMutex();

    return g_FileMountFlag == 1 ? FATFS_RET_OK : FATFS_RET_ERROR_FATFS;
}

/**
 * @brief  FATFS加锁
 */
void FatFS_Lock(void)
{
    //    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) { return; }
    //    if (xSemaphoreTake(xFatFsMutex, portMAX_DELAY) == pdTRUE) { return; }
    //    vTaskDelete(NULL);
}

/**
 * @brief  FATFS解锁
 */
void FatFS_UnLock(void)
{
    //    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) { return; }
    //    xSemaphoreGive(xFatFsMutex);
}

/**
 * @brief  大小写不敏感的通配符匹配函数
 * @param  pattern 匹配模式，支持 '*' (匹配任意多个字符) 和 '?' (匹配单个字符)
 * @param  string 待匹配的字符串
 * @retval 1-匹配成功，0-匹配失败
 * @note   此函数实现了标准的通配符匹配算法，并且不区分大小写。
 *        算法核心是使用两个回溯指针(mp, cp)来处理'*'的贪婪匹配。
 */
uint8_t FatFS_WildcardMatchSingle(const char *pattern, const char *string)
{
    const char *cp = NULL; /* 字符串回溯点：当'*'匹配失败时，从这里重新尝试 */
    const char *mp = NULL; /* 模式回溯点：始终指向'*'之后的下一个字符 */

    /*
     * 第一阶段：处理模式中第一个'*'之前的部分
     * 这部分必须严格匹配（除了'?'可以匹配任意单字符）
     */
    while (*string != '\0' && *pattern != '*')
    {
        /* 如果当前字符不匹配（忽略大小写）且模式字符不是'?'，则匹配失败 */
        if (tolower(*pattern) != tolower(*string) && *pattern != '?')
        {
            return 0;
        }
        /* 字符匹配，则同时推进两个指针 */
        pattern++;
        string++;
    }

    /*
     * 第二阶段：处理包含'*'的模式匹配
     * 这是最复杂的部分，通过回溯机制实现
     */
    while (*string != '\0')
    {
        /* 如果模式当前字符是'*' */
        if (*pattern == '*')
        {
            /* 跳过'*'，检查是否是模式的结尾 */
            if (*++pattern == '\0')
            {
                /* 如果'*'是模式的最后一个字符，它可以匹配字符串剩余的所有部分 */
                return 1;
            }
            /* 记录回溯点：mp指向'*'后面的字符，cp指向字符串当前字符的下一个 */
            mp = pattern;
            cp = string + 1;
        }
        /* 如果模式当前字符与字符串当前字符匹配（忽略大小写）或是'?' */
        else if (tolower(*pattern) == tolower(*string) || *pattern == '?')
        {
            /* 匹配成功，同时推进两个指针 */
            pattern++;
            string++;
        }
        /* 如果字符不匹配 */
        else
        {
            /* 匹配失败，回溯：让'*'多匹配一个字符 */
            /* 将模式指针重置到'*'之后的位置 */
            pattern = mp;
            /* 将字符串指针推进到下一个尝试点 */
            string = cp++;
        }
    }

    /*
     * 第三阶段：收尾检查
     * 当字符串已经遍历完毕，检查模式是否也匹配完毕
     * 跳过模式末尾可能存在的所有'*'（它们可以匹配空字符串）
     */
    while (*pattern == '*')
    {
        pattern++;
    }

    /* 如果模式也到达了结尾，说明完全匹配 */
    return *pattern == '\0';
}

/**
 * @brief  通配符匹配函数（支持分号分隔的多模式）
 * @param  pattern 包含通配符的模式字符串（支持*和?，支持分号分隔多模式，如"*.bmp;*.jpg;*.gif"）
 * @param  string 要匹配的字符串
 * @retval 1-匹配成功，0-匹配失败
 */
uint8_t FatFS_WildcardMatch(const char *pattern, const char *string)
{
    const char *patternStart = pattern;
    const char *patternEnd = NULL;
    char tempPattern[16]; /* 临时存储单个模式 */
    uint8_t patternLen;

    /* 如果模式中没有分号，使用原有逻辑 */
    if (strchr(pattern, ';') == NULL)
    {
        return FatFS_WildcardMatchSingle(pattern, string);
    }

    /* 处理分号分隔的多模式 */
    while (*patternStart != '\0')
    {
        /* 找到下一个分号或字符串结束 */
        patternEnd = strchr(patternStart, ';');
        if (patternEnd == NULL)
        {
            /* 最后一个模式 */
            patternEnd = patternStart + String_GetLength(patternStart, FF_MAX_LFN);
        }

        /* 计算模式长度并复制到临时缓冲区 */
        patternLen = patternEnd - patternStart;
        if (patternLen > 0 && patternLen < 16)
        {
            /* 跳过前导空格 */
            while (*patternStart == ' ' && patternLen > 0)
            {
                patternStart++;
                patternLen--;
            }

            /* 跳过尾随空格 */
            while (patternLen > 0 && *(patternEnd - 1) == ' ')
            {
                patternLen--;
            }

            if (patternLen > 0)
            {
                /* 复制模式到临时缓冲区 */
                Memory_Copy(tempPattern, patternStart, patternLen);
                tempPattern[patternLen] = '\0';

                /* 检查是否匹配 */
                if (FatFS_WildcardMatchSingle(tempPattern, string))
                {
                    return 1; /* 匹配成功，立即返回 */
                }
            }
        }

        /* 移动到下一个模式 */
        if (*patternEnd == ';')
        {
            patternStart = patternEnd + 1;
            /* 跳过分号后的空格 */
            while (*patternStart == ' ')
            {
                patternStart++;
            }
        }
        else
        {
            break; /* 字符串结束 */
        }
    }

    return 0; /* 所有模式都不匹配 */
}

/**
 * @brief  获取指定路径的文件信息
 * @param  path 文件或目录路径
 * @param  fileInfo 输出参数，存储文件信息
 * @retval 状态码：FATFS_RET_OK 成功，其他值失败
 */
FatFsStatus_T FatFS_GetFileInfo(const char *path, FatFsFileInfo_T *fileInfo)
{
    FRESULT res;
    FILINFO fno;

    /* 参数检查 */
    if (path == NULL || fileInfo == NULL) { return FATFS_RET_ERROR_PARAMETER; }

    /* 获取文件信息 */
    res = f_stat(path, &fno);
    if (res != FR_OK) { return FATFS_RET_ERROR_FATFS; }

    /* 直接使用path作为fullPath指针 - 无需分配内存 */
    fileInfo->fullPath = (char *)path;

    /* 设置Name指针 - 指向path中的文件名部分 */
    fileInfo->Name = strrchr(fileInfo->fullPath, '/');
    if (fileInfo->Name == NULL)
    {
        /* 如果没有找到'/'，可能是根目录下的文件，尝试查找'\' */
        fileInfo->Name = strrchr(fileInfo->fullPath, '\\');
        if (fileInfo->Name == NULL)
        {
            /* 如果都没有找到，说明path本身就是文件名 */
            fileInfo->Name = fileInfo->fullPath;
        }
        else
        {
            /* 跳过'\'字符 */
            fileInfo->Name++;
        }
    }
    else
    {
        /* 跳过'/'字符 */
        fileInfo->Name++;
    }

    /* 设置extension指针 - 指向Name中的后缀名部分 */
    fileInfo->extension = strrchr(fileInfo->Name, '.');
    if (fileInfo->extension != NULL && *(fileInfo->extension + 1) != '\0')
    {
        /* 跳过'.'字符 */
        fileInfo->extension++;
    }
    else
    {
        /* 没有后缀名 */
        fileInfo->extension = NULL;
    }

    /* 设置文件类型和大小 */
    fileInfo->type = (fno.fattrib & AM_DIR) ? FATFS_TYPE_DIRECTORY : FATFS_TYPE_FILE;
    fileInfo->size = fno.fsize;

    return FATFS_RET_OK;
}

/**
 * @brief  获取文件大小
 * @param  path 文件路径
 * @retval 文件大小（字节），0表示失败
 */
uint32_t FatFS_GetFileSize(const char *path)
{
    FatFsFileInfo_T info = { 0 };
    FatFS_GetFileInfo(path, &info);
    return info.size;
}

/**
 * @brief  获取文件名
 * @param  path 文件路径
 * @param  Name 输出参数，返回文件名指针
 * @retval 状态码：FATFS_RET_OK 成功，其他值失败
 */
FatFsStatus_T FatFS_GetFileName(const char *path, const char **Name)
{
    FatFsFileInfo_T info = { 0 };
    FatFsStatus_T res = FatFS_GetFileInfo(path, &info);
    *Name = info.Name;
    return res;
}

/**
 * @brief  获取文件后缀名
 * @param  path 文件路径
 * @param  extension 输出参数，返回后缀名指针
 * @retval 状态码：FATFS_RET_OK 成功，其他值失败
 */
FatFsStatus_T FatFS_GetExtension(const char *path, const char **extension)
{
    FatFsFileInfo_T info = { 0 };
    FatFsStatus_T res = FatFS_GetFileInfo(path, &info);
    *extension = info.extension;
    return res;
}

/**
 * @brief  读取指定文件夹的指定后缀文件列表
 * @param  path 目录路径
 * @param  pattern 匹配模式（支持*和?通配符）
 * @param  list 输出参数，存储文件列表
 * @param  needClear 是否清空前一次缓存
 * @retval 状态码：FATFS_RET_OK 成功，其他值失败
 */
FatFsStatus_T FatFS_GetFileList(const char *path, const char *pattern, FatFsFileList_T *list, uint8_t needClear)
{
    FRESULT res;
    FILINFO fno;
    DIR dir;
    char *tempFullPath = NULL;
    uint32_t pathLen;
    uint8_t needSlash = 0;

    /* 参数检查 */
    if (path == NULL || pattern == NULL || list == NULL)
    {
        return FATFS_RET_ERROR_PARAMETER;
    }

    /* 清空前一次缓存 */
    if (needClear) { FatFS_FreeFileList(list); }

    /* 上锁 */
    FatFS_Lock();

    /* 如果不存在，则创建目录 */
    f_mkdir(path);

    /* 打开目录 */
    if ((res = f_opendir(&dir, path)) != FR_OK)
    {
        FatFS_UnLock();
        DEBUG_PRINTF("打开目录失败：\r\n");
        DEBUG_PRINTF("%s\r\n", g_FrTable[res]);
        return FATFS_RET_ERROR_FATFS;
    }

    /* 分配文件列表数组内存 */
    list->files = (FatFsFileInfo_T *)Memory_Malloc(MP_RAM_IN, FILE_LIST_MAX_COUNT * sizeof(FatFsFileInfo_T));
    if (list->files == NULL)
    {
        f_closedir(&dir);
        FatFS_UnLock();
        return FATFS_RET_ERROR_MALLOC;
    }

    /* 初始化指针数组为NULL */
    for (uint16_t i = 0; i < FILE_LIST_MAX_COUNT; i++)
    {
        list->files[i].Name = NULL;
        list->files[i].fullPath = NULL;
        list->files[i].extension = NULL;
    }

    /* 预先分配临时路径缓冲区 */
    pathLen = String_GetLength(path, FF_MAX_LFN);
    needSlash = (pathLen > 0 && (path[pathLen - 1] != '/' && path[pathLen - 1] != '\\')) ? 1 : 0;
    tempFullPath = (char *)Memory_Malloc(MP_RAM_IN, pathLen + needSlash + FF_MAX_LFN + 1);

    if (tempFullPath == NULL)
    {
        f_closedir(&dir);
        FatFS_UnLock();
        Memory_Free(MP_RAM_IN, list->files);
        list->files = NULL;
        return FATFS_RET_ERROR_MALLOC;
    }

    /* 重新打开目录并读取文件名 */
    f_rewinddir(&dir);
    uint16_t index = 0;

    while (1)
    {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0 || index >= FILE_LIST_MAX_COUNT) { break; }

        /* 使用通配符匹配 */
        if (!FatFS_WildcardMatch(pattern, fno.fname)) { continue; }

        /* 构建完整路径到临时缓冲区 */
        Memory_Set(tempFullPath, 0, pathLen + needSlash + FF_MAX_LFN + 1);
        strcpy(tempFullPath, path);
        if (needSlash) { strcat(tempFullPath, "/"); }
        strcat(tempFullPath, fno.fname);

        /* 分配完整路径内存 */
        list->files[index].fullPath = (char *)Memory_Malloc(MP_RAM_IN, String_GetLength(tempFullPath, FATFS_PATH_LEN_MAX) + 1);
        if (list->files[index].fullPath == NULL) { break; } /* 内存分配失败，退出循环 */

        /* 复制完整路径 */
        strcpy(list->files[index].fullPath, tempFullPath);
        FatFS_GetFileInfo(list->files[index].fullPath, &list->files[index]);
        index++;
    }

    f_closedir(&dir);
    FatFS_UnLock();

    list->count = index;

    /* 释放临时路径缓冲区 */
    if (tempFullPath != NULL) { Memory_Free(MP_RAM_IN, tempFullPath); }

    return FATFS_RET_OK;
}

/**
 * @brief  释放文件列表内存
 * @param  list 文件列表结构体指针
 * @retval 状态码：FATFS_RET_OK 成功，其他值失败
 */
FatFsStatus_T FatFS_FreeFileList(FatFsFileList_T *list)
{
    /* 参数检查 */
    if (list == NULL) { return FATFS_RET_ERROR_PARAMETER; }

    /* 是否文件列表 */
    if (list->files != NULL)
    {
        for (uint16_t i = 0; i < FILE_LIST_MAX_COUNT; i++)
        {
            if (list->files[i].fullPath != NULL)
            {
                Memory_Free(MP_RAM_IN, list->files[i].fullPath);
            }
            /* Name和extension是fullPath的偏移量，不需要单独释放 */
            list->files[i].Name = NULL;
            list->files[i].fullPath = NULL;
            list->files[i].extension = NULL;
        }
        Memory_Free(MP_RAM_IN, list->files);
    }

    list->files = NULL;
    list->count = 0;

    return FATFS_RET_OK;
}

/**
 * @brief  读取指定文件夹的指定后缀文件列表（仅匹配后缀，保持向后兼容）
 * @param  path 目录路径
 * @param  extension 文件后缀（如".txt"）
 * @param  list 输出参数，存储文件列表
 * @param  needClear 是否清空前一次缓存
 * @retval 状态码：FATFS_RET_OK 成功，其他值失败
 */
FatFsStatus_T FatFS_GetListByExtension(const char *path, const char *extension, FatFsFileList_T *list, uint8_t needClear)
{
    char pattern[FF_MAX_LFN + 2] = "*"; /* +2 for '*' and null terminator */

    /* 参数检查 */
    if (path == NULL || extension == NULL || list == NULL) { return FATFS_RET_ERROR_PARAMETER; }

    /* 构建通配符模式，如 "*.txt" */
    strcat(pattern, extension);

    /* 调用通配符版本的函数 */
    return FatFS_GetFileList(path, pattern, list, needClear);
}

/**
 * @brief  从文件读取数据
 * @param  fileName 文件名路径
 * @param  buffer 存储读取数据的缓冲区
 * @param  size 要读取的最大字节数
 * @retval 实际读取的字节数（0表示失败）
 */
uint32_t FatFS_ReadFile(const char *fileName, uint8_t *buffer, uint32_t size)
{
    FIL *file = NULL;
    FRESULT res;
    UINT bytesRead = 0;

    /* 参数检查 */
    if (fileName == NULL || buffer == NULL || size == 0) { return 0; }

    /* 上锁 */
    FatFS_Lock();

    /* 动态分配文件对象 */
    if ((file = (FIL *)Memory_Malloc(MP_RAM_IN, sizeof(FIL))) == NULL)
    {
        DEBUG_PRINTF("文件对象分配失败\r\n");
        file = NULL;
        goto finish;
    }

    /* 打开文件 */
    if ((res = f_open(file, fileName, FA_READ)) != FR_OK)
    {
        DEBUG_PRINTF("文件打开失败: %s\r\n", g_FrTable[res]);
        goto finish;
    }

    /* 读取文件内容 */
    if ((res = f_read(file, buffer, size, &bytesRead)) != FR_OK)
    {
        DEBUG_PRINTF("文件读取失败: %s\r\n", g_FrTable[res]);
        goto finish;
    }

finish:
    /* 关闭文件并释放内存 */
    if (file != NULL) { f_close(file); }
    if (file != NULL) { Memory_Free(MP_RAM_IN, file); }
    FatFS_UnLock();

    return bytesRead;
}

/**
 * @brief  向文件写入数据
 * @param  fileName 文件名路径
 * @param  buffer 包含要写入数据的缓冲区
 * @param  size 要写入的字节数
 * @retval 实际写入的字节数（0表示失败）
 */
uint32_t FatFS_WriteFile(const char *fileName, uint8_t *buffer, uint32_t size)
{
    FIL *file = NULL;
    FRESULT res;
    UINT bytesWritten = 0;

    /* 参数检查 */
    if (fileName == NULL || buffer == NULL || size == 0) { return 0; }

    /* 上锁 */
    FatFS_Lock();

    /* 动态分配文件对象 */
    if ((file = (FIL *)Memory_Malloc(MP_RAM_IN, sizeof(FIL))) == NULL)
    {
        DEBUG_PRINTF("文件对象分配失败\r\n");
        file = NULL;
        goto finish;
    }

    /* 打开文件（创建/写入模式） */
    if ((res = f_open(file, fileName, FA_CREATE_ALWAYS | FA_WRITE)) != FR_OK)
    {
        DEBUG_PRINTF("文件创建失败: %s\r\n", g_FrTable[res]);
        goto finish;
    }

    /* 写入数据到文件 */
    if ((res = f_write(file, buffer, size, &bytesWritten)) != FR_OK)
    {
        DEBUG_PRINTF("文件写入失败: %s\r\n", g_FrTable[res]);
        goto finish;
    }

finish:
    /* 关闭文件并释放内存 */
    FatFS_UnLock();
    if (file != NULL) { f_close(file); }
    if (file != NULL) { Memory_Free(MP_RAM_IN, file); }

    return bytesWritten;
}

/**
 * @brief  追加数据到文件末尾
 * @param  fileName 文件路径
 * @param  buffer 要追加的数据
 * @param  size 数据大小
 * @retval 实际写入的字节数（0表示失败）
 */
uint32_t FatFS_AppendFile(const char *fileName, uint8_t *buffer, uint32_t size)
{
    FIL *file = NULL;
    FRESULT res;
    UINT bytesWritten = 0;

    /* 参数检查 */
    if (fileName == NULL || buffer == NULL || size == 0) { return 0; }

    /* 上锁 */
    FatFS_Lock();

    /* 动态分配文件对象 */
    if ((file = (FIL *)Memory_Malloc(MP_RAM_IN, sizeof(FIL))) == NULL)
    {
        DEBUG_PRINTF("文件对象分配失败\r\n");
        file = NULL;
        goto finish;
    }

    /* 打开文件（追加模式） */
    if ((res = f_open(file, fileName, FA_OPEN_APPEND | FA_WRITE)) != FR_OK)
    {
        DEBUG_PRINTF("打开文件失败: %s\r\n", g_FrTable[res]);
        goto finish;
    }

    /* 写入数据 */
    if ((res = f_write(file, buffer, size, &bytesWritten)) != FR_OK)
    {
        DEBUG_PRINTF("写入文件失败: %s\r\n", g_FrTable[res]);
        goto finish;
    }

finish:
    /* 关闭文件并释放内存 */
    FatFS_UnLock();
    if (file != NULL) { f_close(file); }
    if (file != NULL) { Memory_Free(MP_RAM_IN, file); }

    return bytesWritten;
}

/**
 * @brief  创建文件夹（支持多级目录，如果目录已存在则直接返回成功）
 * @param  dirPath 目录路径，支持 "0:/dir" 和 "0:dir" 等格式
 * @retval 状态码：FATFS_RET_OK 成功，其他值失败
 */
FatFsStatus_T FatFS_CreateDirectory(const char *dirPath)
{
    FRESULT res;
    char *pathCopy = NULL;
    char *p = NULL;
    char *pathStart = NULL; /* 指向真正需要开始处理的路径部分 */
    DIR dir;

    /* 参数检查 */
    if (dirPath == NULL) { return FATFS_RET_ERROR_PARAMETER; }

    /* 上锁 */
    FatFS_Lock();

    /* 先检查目录是否已存在，存在则直接返回 */
    res = f_opendir(&dir, dirPath);
    if (res == FR_OK)
    {
        f_closedir(&dir);
        FatFS_UnLock();
        return FATFS_RET_OK;
    }

    /* 复制路径字符串 */
    if ((pathCopy = String_MallocCopy(MP_RAM_IN, dirPath, FF_MAX_LFN + 1)) == NULL)
    {
        FatFS_UnLock();
        return FATFS_RET_ERROR_MALLOC;
    }

    strcpy(pathCopy, dirPath);

    /* 定位到第一个目录名的起始位置 */
    pathStart = pathCopy;

    /* 1. 跳过驱动器部分，例如 "0:" */
    char *colon = strchr(pathStart, ':');
    if (colon != NULL)
    {
        pathStart = colon + 1; /* pathStart 现在指向 ':' 后面的字符 */
    }

    /* 2. 跳过驱动器后的第一个路径分隔符（如果有的话） */
    /*    这样可以统一处理 "0:/dir" 和 "0:dir" */
    if (*pathStart == '/' || *pathStart == '\\')
    {
        pathStart++;
    }

    /* 现在，pathStart 指向了第一个目录名的首字母（例如 "setting" 中的 's'） */
    /* 我们从这里开始寻找路径分隔符并进行创建 */
    p = pathStart;
    while (*p != '\0')
    {
        if (*p == '/' || *p == '\\')
        {
            *p = '\0'; /* 临时截断路径 */

            /* 尝试创建当前级别的目录（例如 "0:setting"） */
            res = f_mkdir(pathCopy);
            if (res != FR_OK && res != FR_EXIST)
            {
                FatFS_UnLock();
                DEBUG_PRINTF("创建目录失败: %s (错误: %s)\r\n", pathCopy, g_FrTable[res]);
                return FATFS_RET_ERROR_FATFS;
            }
            *p = '/'; /* 恢复路径分隔符 */
        }
        p++;
    }

    /* 创建最后一级目录 */
    res = f_mkdir(pathCopy);

    /* 关闭文件并释放内存 */
    FatFS_UnLock();
    if (pathCopy != NULL) { Memory_Free(MP_RAM_IN, pathCopy); }

    return res != FR_OK ? FATFS_RET_ERROR_FATFS : FATFS_RET_OK;
}

/**
 * @brief  递归删除目录及其内容
 * @param  dirPath 目录路径
 * @retval 状态码：FATFS_RET_OK 成功，其他值失败
 */
static FatFsStatus_T FatFS_DeleteDirectoryRecursive(const char *dirPath)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    FatFsStatus_T ret;
    char *fullPath;

    /* 上锁 */
    FatFS_Lock();

    /* 打开目录 */
    if ((res = f_opendir(&dir, dirPath)) != FR_OK)
    {
        FatFS_UnLock();
        return FATFS_RET_ERROR_FATFS;
    }

    /* 分配路径缓冲区 */
    if ((fullPath = (char *)String_MallocCopy(MP_RAM_IN, dirPath, FF_MAX_LFN + 1)) == NULL)
    {
        f_closedir(&dir);
        FatFS_UnLock();
        return FATFS_RET_ERROR_MALLOC;
    }

    /* 遍历目录 */
    while (1)
    {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break;

        /* 跳过 "." 和 ".." */
        if (fno.fname[0] == '.' && (fno.fname[1] == '\0' || (fno.fname[1] == '.' && fno.fname[2] == '\0')))
        {
            continue;
        }

        /* 智能拼接路径，避免产生双斜杠 */
        size_t dirPathLen = String_GetLength(dirPath, FATFS_PATH_LEN_MAX);
        if (dirPathLen > 0 && (dirPath[dirPathLen - 1] == '/' || dirPath[dirPathLen - 1] == '\\'))
        {
            /* 如果路径以 '/' 或 '\' 结尾，直接拼接 */
            sprintf(fullPath, "%s%s", dirPath, fno.fname);
        }
        else
        {
            /* 否则，先添加 '/' 再拼接 */
            sprintf(fullPath, "%s/%s", dirPath, fno.fname);
        }

        if (fno.fattrib & AM_DIR)
        {
            /* 递归删除子目录 */
            if ((ret = FatFS_DeleteDirectoryRecursive(fullPath)) != FATFS_RET_OK)
            {
                Memory_Free(MP_RAM_IN, fullPath);
                f_closedir(&dir);
                FatFS_UnLock();
                return ret;
            }
        }
        else
        {
            /* 删除文件 */
            res = f_unlink(fullPath);
            if (res != FR_OK)
            {
                DEBUG_PRINTF("删除文件失败: %s (错误: %s)\r\n", fullPath, g_FrTable[res]);
                Memory_Free(MP_RAM_IN, fullPath);
                f_closedir(&dir);
                FatFS_UnLock();
                return FATFS_RET_ERROR_FATFS;
            }
        }
    }

    Memory_Free(MP_RAM_IN, fullPath);
    f_closedir(&dir);

    /* 删除空目录 */
    res = f_unlink(dirPath);
    FatFS_UnLock();

    return (res == FR_OK) ? FATFS_RET_OK : FATFS_RET_ERROR_FATFS;
}

/**
 * @brief  删除多级目录
 * @param  dirPath 目录路径
 * @retval 状态码：FATFS_RET_OK 成功，其他值失败
 */
FatFsStatus_T FatFS_DeleteDirectory(const char *dirPath)
{
    /* 参数检查 */
    if (dirPath == NULL) { return FATFS_RET_ERROR_PARAMETER; }

    FatFsStatus_T res = FatFS_DeleteDirectoryRecursive(dirPath);
    return res;
}

/**
 * @brief  删除指定文件
 * @param  path 文件路径
 * @retval 状态码：FATFS_RET_OK 成功，其他值失败
 */
FatFsStatus_T FatFS_DeleteFile(const char *path)
{
    FatFS_Lock();
    FatFsStatus_T res = f_unlink(path) == FR_OK ? FATFS_RET_OK : FATFS_RET_ERROR_FATFS;
    FatFS_UnLock();
    return res;
}

/**
 * @brief  读取整个文件内容并自动分配缓冲区
 * @param  source 内存来源类型
 * @param  fileName 文件路径
 * @param  buffer 输出参数，返回分配的缓冲区指针
 * @retval 实际读取的字节数（0表示失败）
 * @note   调用者需使用Memory_Free释放内存
 */
uint32_t FatFS_ReadFileAll(MemorySource_T source, const char *fileName, uint8_t **buffer)
{
    uint32_t fileSize;
    uint32_t bytesRead;
    uint8_t *localBuffer = NULL;

    /* 参数检查 */
    if (fileName == NULL || buffer == NULL)
    {
        return 0;
    }

    /* 初始化输出参数 */
    *buffer = NULL;

    /* 获取文件大小 */
    fileSize = FatFS_GetFileSize(fileName);
    if (fileSize == 0)
    {
        DEBUG_PRINTF("文件不存在或为空\r\n");
        return 0;
    }

    /* 分配缓冲区（+1用于字符串终止符） */
    localBuffer = (uint8_t *)Memory_Malloc(source, fileSize + 1);
    if (localBuffer == NULL)
    {
        DEBUG_PRINTF("内存分配失败\r\n");
        return 0;
    }

again:
    /* 读取文件内容 */
    bytesRead = FatFS_ReadFile(fileName, localBuffer, fileSize);

    /* 添加字符串终止符 */
    localBuffer[bytesRead] = '\0';

    /* 判断读取的字节数 */
    if (bytesRead != fileSize)
    {
        DEBUG_PRINTF("文件读取不完整，期望%u字节，实际读取%u字节\r\n", fileSize, bytesRead);
        goto again;
    }

    /* 设置输出参数 */
    *buffer = localBuffer;

    return bytesRead;
}

/**
 * @brief  显示文件系统信息
 */
void FatFS_ShowInfo(void)
{
    if (!g_FileMountFlag) { return; }

    FATFS *fptr = NULL;                           /* 定义文件系统定义结构体指针 */
    DWORD freClust, freSect, totSect;             /* 空闲簇，空闲扇区数，总扇区数 */
    f_getfree("0:", &freClust, &fptr);            /* 获取剩余簇 */
    totSect = (fptr->n_fatent - 2) * fptr->csize; /* 总扇区数量 = 总的簇 * 每个簇包含的扇区数 */
    freSect = freClust * fptr->csize;             /* 计算剩余的可用扇区数 */

    DEBUG_PRINTF("文件系统挂载成功：\r\n");
    DEBUG_PRINTF("文件系统容量：%.2f MB\r\n", totSect / 256.0f);
    DEBUG_PRINTF("文件系统剩余：%.2f MB\r\n", freSect / 256.0f);
}

/* --- 新增函数：带偏移量的读写操作 --- */

/**
 * @brief  从文件指定偏移位置读取数据
 * @param  fileName 文件路径
 * @param  buffer 读取缓冲区
 * @param  size 要读取的字节数
 * @param  offset 从文件开头的偏移量（字节）
 * @retval 实际读取的字节数（0表示失败）
 */
uint32_t FatFS_ReadFileAtOffset(const char *fileName, uint8_t *buffer, uint32_t size, uint32_t offset)
{
    FIL *file = NULL;
    FRESULT res;
    UINT bytesRead = 0;

    /* 参数检查 */
    if (fileName == NULL || buffer == NULL || size == 0)
    {
        return 0;
    }

    /* 上锁 */
    FatFS_Lock();

    /* 动态分配文件对象 */
    if ((file = (FIL *)Memory_Malloc(MP_RAM_IN, sizeof(FIL))) == NULL)
    {
        DEBUG_PRINTF("文件对象分配失败\r\n");
        file = NULL;
        goto finish;
    }

    /* 打开文件 */
    if ((res = f_open(file, fileName, FA_READ)) != FR_OK)
    {
        DEBUG_PRINTF("文件打开失败: %s\r\n", g_FrTable[res]);
        goto finish;
    }

    /* 移动文件指针到指定偏移量 */
    if ((res = f_lseek(file, offset)) != FR_OK)
    {
        DEBUG_PRINTF("文件指针定位失败: %s\r\n", g_FrTable[res]);
        goto finish;
    }

    /* 读取文件内容 */
    if ((res = f_read(file, buffer, size, &bytesRead)) != FR_OK)
    {
        DEBUG_PRINTF("文件读取失败: %s\r\n", g_FrTable[res]);
        goto finish;
    }

finish:
    /* 关闭文件并释放内存 */
    if (file != NULL)
    {
        f_close(file);
    }
    if (file != NULL)
    {
        Memory_Free(MP_RAM_IN, file);
    }
    FatFS_UnLock();
    return bytesRead;
}

/**
 * @brief  向文件指定偏移位置写入数据
 * @param  fileName 文件路径
 * @param  buffer 写入数据缓冲区
 * @param  size 要写入的字节数
 * @param  offset 从文件开头的偏移量（字节）
 * @retval 实际写入的字节数（0表示失败）
 * @note   此函数为覆盖写。如果需要扩展文件，请确保 offset + size 不超过文件系统限制。
 *        如果 offset 大于当前文件大小，中间部分可能产生空洞（填充0）。
 */
uint32_t FatFS_WriteFileAtOffset(const char *fileName, uint8_t *buffer, uint32_t size, uint32_t offset)
{
    FIL *file = NULL;
    FRESULT res;
    UINT bytesWritten = 0;

    /* 参数检查 */
    if (fileName == NULL || buffer == NULL || size == 0)
    {
        return 0;
    }

    /* 上锁 */
    FatFS_Lock();

    /* 动态分配文件对象 */
    if ((file = (FIL *)Memory_Malloc(MP_RAM_IN, sizeof(FIL))) == NULL)
    {
        DEBUG_PRINTF("文件对象分配失败\r\n");
        file = NULL;
        goto finish;
    }

    /* 打开文件（读写模式） */
    if ((res = f_open(file, fileName, FA_READ | FA_WRITE)) != FR_OK)
    {
        /* 如果文件不存在，尝试创建它 */
        if (res == FR_NO_FILE)
        {
            if ((res = f_open(file, fileName, FA_CREATE_ALWAYS | FA_READ | FA_WRITE)) != FR_OK)
            {
                DEBUG_PRINTF("文件创建失败: %s\r\n", g_FrTable[res]);
                goto finish;
            }
        }
        else
        {
            DEBUG_PRINTF("文件打开失败: %s\r\n", g_FrTable[res]);
            goto finish;
        }
    }

    /* 移动文件指针到指定偏移量 */
    if ((res = f_lseek(file, offset)) != FR_OK)
    {
        DEBUG_PRINTF("文件指针定位失败: %s\r\n", g_FrTable[res]);
        goto finish;
    }

    /* 写入数据到文件 */
    if ((res = f_write(file, buffer, size, &bytesWritten)) != FR_OK)
    {
        DEBUG_PRINTF("文件写入失败: %s\r\n", g_FrTable[res]);
        goto finish;
    }

    /* 如果写入位置在文件末尾之后，需要更新文件大小 */
    if (f_size(file) < offset + bytesWritten)
    {
        f_truncate(file);  // 截断或扩展文件到当前指针位置
    }

finish:
    /* 关闭文件并释放内存 */
    FatFS_UnLock();
    if (file != NULL)
    {
        f_close(file);
    }
    if (file != NULL)
    {
        Memory_Free(MP_RAM_IN, file);
    }
    return bytesWritten;
}

/**
 * @brief  UTF-16字符串转本地编码（GB2312）字符串
 * @param  utf16_str UTF-16字符串
 * @param  local_buf 本地编码缓冲区
 * @param  buf_size 缓冲区大小
 * @retval 转换后的字节数
 */
int UTF16_To_GB2312(const WCHAR_T *utf16_str, char *local_buf, int buf_size)
{
    int i = 0, j = 0;
    WORD cp = FF_CODE_PAGE;

    while (utf16_str[i] != 0 && j < buf_size - 1)
    {
        DWORD unicode = utf16_str[i++];
        WCHAR local = ff_uni2oem(unicode, cp);

        if (local == 0 && unicode != 0)
        {
            // 转换失败，用问号替代
            local_buf[j++] = '?';
        }
        else if (local < 0x100)
        {
            // 单字节字符
            local_buf[j++] = (char)local;
        }
        else
        {
            // 双字节字符（GBK等）
            if (j + 1 >= buf_size - 1) break;
            local_buf[j++] = (char)(local >> 8);
            local_buf[j++] = (char)(local & 0xFF);
        }
    }
    local_buf[j] = '\0';
    return j;
}

/**
 * @brief  本地编码（GB2312）字符串转UTF-16字符串
 * @param  local_str 本地编码字符串
 * @param  utf16_buf UTF-16缓冲区
 * @param  buf_size 缓冲区大小
 * @retval 转换后的字符数
 */
int GB2312_To_UTF16(const char *local_str, WCHAR_T *utf16_buf, int buf_size)
{
    int i = 0, j = 0;
    WORD cp = FF_CODE_PAGE;

    while (local_str[i] != 0 && j < buf_size - 1)
    {
        WCHAR oem_char;

        // 检查是否双字节字符（GBK、BIG5等）
        if (local_str[i] & 0x80)
        {
            // 双字节字符
            if (local_str[i + 1] == 0) break;
            oem_char = ((WCHAR)local_str[i] << 8) | (WCHAR)local_str[i + 1];
            i += 2;
        }
        else
        {
            // 单字节字符
            oem_char = (WCHAR)local_str[i];
            i++;
        }

        // 转换为 Unicode
        utf16_buf[j] = ff_oem2uni(oem_char, cp);
        j++;
    }
    utf16_buf[j] = 0;
    return j;
}

/**
 * @brief  UTF-8字符串转UTF-16字符串
 * @param  utf8 UTF-8字符串
 * @param  utf16 UTF-16缓冲区
 * @param  max_len 最大字符数
 * @retval 转换后的字符数
 */
int UTF8_To_UTF16(const char *utf8, WCHAR_T *utf16, int max_len)
{
    int i = 0, j = 0;

    while (utf8[i] != 0 && j < max_len - 1)
    {
        DWORD ch = 0;

        if ((utf8[i] & 0x80) == 0)
        {
            // 1字节: 0xxxxxxx
            ch = utf8[i++];
        }
        else if ((utf8[i] & 0xE0) == 0xC0)
        {
            // 2字节: 110xxxxx 10xxxxxx
            if ((utf8[i + 1] & 0xC0) != 0x80) break;
            ch = ((utf8[i++] & 0x1F) << 6);
            ch |= (utf8[i++] & 0x3F);
        }
        else if ((utf8[i] & 0xF0) == 0xE0)
        {
            // 3字节: 1110xxxx 10xxxxxx 10xxxxxx
            if ((utf8[i + 1] & 0xC0) != 0x80 || (utf8[i + 2] & 0xC0) != 0x80) break;
            ch = ((utf8[i++] & 0x0F) << 12);
            ch |= ((utf8[i++] & 0x3F) << 6);
            ch |= (utf8[i++] & 0x3F);
        }
        else
        {
            // 不支持4字节及以上
            break;
        }

        utf16[j++] = (WCHAR)ch;
    }
    utf16[j] = 0;
    return j;
}

/**
 * @brief  UTF-16字符串转UTF-8字符串
 * @param  utf16 UTF-16字符串
 * @param  utf8 UTF-8缓冲区
 * @param  max_len 最大字节数
 * @retval 转换后的字节数
 */
int UTF16_To_UTF8(const WCHAR_T *utf16, char *utf8, int max_len)
{
    int i = 0, j = 0;

    while (utf16[i] != 0 && j < max_len - 4)
    {
        DWORD ch = utf16[i++];

        if (ch < 0x80)
        {
            // 1字节
            utf8[j++] = (char)ch;
        }
        else if (ch < 0x800)
        {
            // 2字节
            utf8[j++] = 0xC0 | (ch >> 6);
            utf8[j++] = 0x80 | (ch & 0x3F);
        }
        else
        {
            // 3字节
            utf8[j++] = 0xE0 | (ch >> 12);
            utf8[j++] = 0x80 | ((ch >> 6) & 0x3F);
            utf8[j++] = 0x80 | (ch & 0x3F);
        }
    }
    utf8[j] = '\0';
    return j;
}

/**
 * @brief  UTF-8字符串转本地编码（GB2312）字符串
 * @param  utf8_str UTF-8字符串
 * @param  local_buf 本地编码缓冲区
 * @param  buf_size 缓冲区大小
 * @retval 转换后的字节数
 */
int UTF8_To_GB2312(const char *utf8_str, char *local_buf, int buf_size)
{
    WCHAR_T *utf16_buf = (WCHAR_T *)Memory_Malloc(MP_RAM_EX, buf_size * sizeof(WCHAR_T));

    // UTF-8 转 UTF-16
    int len = UTF8_To_UTF16(utf8_str, utf16_buf, buf_size * sizeof(WCHAR_T));

    // UTF-16 转本地编码
    len = UTF16_To_GB2312(utf16_buf, local_buf, buf_size);
    Memory_Free(MP_RAM_EX, utf16_buf);
    return len;
}

/**
 * @brief  本地编码（GB2312）字符串转UTF-8字符串
 * @param  local_str 本地编码字符串
 * @param  utf8_buf UTF-8缓冲区
 * @param  buf_size 缓冲区大小
 * @retval 转换后的字节数
 */
int GB2312_To_UTF8(const char *local_str, char *utf8_buf, int buf_size)
{
    WCHAR_T *utf16_buf = (WCHAR_T *)Memory_Malloc(MP_RAM_EX, buf_size * sizeof(WCHAR_T));

    // 本地编码转 UTF-16
    int len = GB2312_To_UTF16(local_str, utf16_buf, buf_size * sizeof(WCHAR_T));

    // UTF-16 转 UTF-8
    len = UTF16_To_UTF8(utf16_buf, utf8_buf, buf_size);
    Memory_Free(MP_RAM_EX, utf16_buf);
    return len;
}
