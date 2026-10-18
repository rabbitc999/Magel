#include <ctype.h>
#include <rt_sys.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "bsp_retarget.h"
#include "bsp_uart.h"
#include "bsp_rtc.h"

#include "ff.h"

#if !(defined(USE_MICRO_LIB) && USE_MICRO_LIB)
/* 不使用半主机模式 */
#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
/* Arm Compiler 6 */
__asm(".global __use_no_semihosting_swi");
#else
/* Arm Compiler 5 */
#pragma import(__use_no_semihosting_swi)
#endif

/* 定义标准输入输出设备标识 */
enum
{
    STDIN,
    STDOUT,
    STDERR
} STDIO;
#define IS_STD(fh) ((fh) >= STDIN && (fh) <= STDERR)

/*
 * These names are used during library initialization as the
 * file names opened for stdin, stdout, and stderr.
 * As we define _sys_open() to always return the same file handle,
 * these can be left as their default values.
 */
const char __stdin_name[] = ":kkl";
const char __stdout_name[] = "kkl";
const char __stderr_name[] = "kkl";

/**
 * @brief  打开文件（非半主机重定向）：标准输入输出返回专用句柄，其他文件经FatFs打开
 * @param  Name 文件名
 * @param  openmode 打开模式
 * @retval 成功返回文件句柄，标准输入输出返回STDIN/STDOUT/STDERR，失败返回-1
 */
FILEHANDLE _sys_open(const char *Name, int openmode)
{
#ifdef FF_DEFINED
    BYTE mode;
    FIL *fp;
    FRESULT fr;
#endif
    if (Name == __stdin_name)
    {
        return STDIN;
    }
    else if (Name == __stdout_name)
    {
        return STDOUT;
    }
    else if (Name == __stderr_name)
    {
        return STDERR;
    }
#ifdef FF_DEFINED
    if (sizeof(FILEHANDLE) < sizeof(void *))
    {
        UART_SendString(COM1, "sizeof(FILEHANDLE) should be no less than sizeof(void *)!\n");
        return -1;
    }
    fp = ff_memalloc(sizeof(FIL)); /* 使用自己的malloc函数替换 */
    if (fp == NULL)
        return -1;

    /* http://elm-chan.org/fsw/ff/doc/open.html */
    if (openmode & OPEN_W)
    {
        mode = FA_CREATE_ALWAYS | FA_WRITE;
        if (openmode & OPEN_PLUS)
            mode |= FA_READ;
    }
    else if (openmode & OPEN_A)
    {
        mode = FA_OPEN_APPEND | FA_WRITE;
        if (openmode & OPEN_PLUS)
            mode |= FA_READ;
    }
    else
    {
        mode = FA_READ;
        if (openmode & OPEN_PLUS)
            mode |= FA_WRITE;
    }

    fr = f_open(fp, Name, mode);
    if (fr == FR_OK)
    {
        return (uintptr_t)fp;
    }

    ff_memfree(fp); /* 使用自己的free函数替换 */
#endif

    return -1;
}

/**
 * @brief  关闭文件（非半主机重定向）：标准输出直接成功，其他文件经FatFs关闭
 * @param  fh 文件句柄
 * @retval 0-成功，-1-失败
 */
int _sys_close(FILEHANDLE fh)
{
#ifdef FF_DEFINED
    FRESULT fr;
#endif

    if (IS_STD(fh))
    {
        if (fh == STDOUT)
            return 0;
        return -1;
    }

#ifdef FF_DEFINED
    fr = f_close((FIL *)fh);
    if (fr == FR_OK)
    {
        ff_memfree((void *)fh); /* 使用自己的free函数替换 */
        return 0;
    }
#endif

    return -1;
}

/**
 * @brief  写数据到文件（非半主机重定向）：标准输出/错误重定向到串口1发送
 * @param  fh 文件句柄
 * @param  buf 待写入的数据缓冲区
 * @param  len 要写入的数据长度（字节）
 * @param  mode 模式参数（未使用）
 * @retval 未写入的字节数，失败返回-1
 */
int _sys_write(FILEHANDLE fh, const unsigned char *buf, unsigned len, int mode)
{
#ifdef FF_DEFINED
    FRESULT fr;
    UINT bw;
#endif

    if (fh == STDIN)
    {
        return -1;
    }

    if (fh == STDOUT || fh == STDERR)
    {
        UART_SendString(COM1, (char *)buf);
        return 0;
    }

#ifdef FF_DEFINED
    fr = f_write((FIL *)fh, buf, len, &bw);
    if (fr == FR_OK)
    {
        return len - bw;
    }
#endif

    return -1;
}

/**
 * @brief  从文件读取数据（非半主机重定向）：经FatFs读取
 * @param  fh 文件句柄
 * @param  buf 读取数据存放缓冲区
 * @param  len 要读取的数据长度（字节）
 * @param  mode 模式参数（未使用）
 * @retval 未读取的字节数，失败返回-1
 */
int _sys_read(FILEHANDLE fh, unsigned char *buf, unsigned len, int mode)
{
#ifdef FF_DEFINED
    FRESULT fr;
    UINT br;
#endif

    if (fh == STDIN)
    {
        return -1;
    }
    else if (fh == STDOUT || fh == STDERR)
    {
        return -1;
    }

#ifdef FF_DEFINED
    fr = f_read((FIL *)fh, buf, len, &br);
    if (fr == FR_OK)
    {
        return len - br;
    }
#endif

    return -1;
}

/**
 * @brief  检查句柄是否为终端
 * @param  fh 文件句柄
 * @retval 1-是标准输入输出终端，0-非终端
 */
int _sys_istty(FILEHANDLE fh)
{
    return IS_STD(fh);
}

/**
 * @brief  移动文件读写指针（非半主机重定向）：经FatFs的f_lseek实现
 * @param  fh 文件句柄
 * @param  pos 目标偏移位置（字节）
 * @retval 0-成功，-1-失败
 */
int _sys_seek(FILEHANDLE fh, long pos)
{
#ifdef FF_DEFINED
    FRESULT fr;

    if (!IS_STD(fh))
    {
        fr = f_lseek((FIL *)fh, pos);
        if (fr == FR_OK)
        {
            return 0;
        }
    }
#endif
    return -1;
}

/**
 * @brief  刷新句柄关联的缓冲区
 * @param  fh 文件句柄
 * @retval 0-成功
 */
int _sys_ensure(FILEHANDLE fh)
{
    return 0;
}

/**
 * @brief  返回文件当前长度（非半主机重定向）：经FatFs的f_size实现
 * @param  fh 文件句柄
 * @retval 文件长度（字节），标准输入输出或失败返回-1
 */
long _sys_flen(FILEHANDLE fh)
{
#ifdef FF_DEFINED
    if (!IS_STD(fh))
    {
        return f_size((FIL *)fh);
    }
#endif
    return -1;
}

/**
 * @brief  退出程序（非半主机重定向，空实现）
 * @param  status 退出状态码
 */
void _sys_exit(int status)
{
}

/**
 * @brief  生成临时文件名（非半主机重定向，空实现）
 * @param  Name 存放文件名的缓冲区
 * @param  fileno 文件编号
 * @param  maxlength 文件名最大长度
 */
void _sys_tmpnam(char *Name, int fileno, unsigned maxlength)
{
}

/**
 * @brief  将一个字符写入控制台（重定向到串口1发送）
 * @param  ch 要输出的字符
 */
void _ttywrch(int ch)
{
    UART_SendArray(COM1, (uint8_t *)&ch, 1);
}

/**
 * @brief  删除文件（空实现，直接返回成功）
 * @param  filename 要删除的文件名
 * @retval 0
 */
int remove(const char *filename)
{
    return 0;
}

/**
 * @brief  重命名文件（空实现，直接返回成功）
 * @param  oldname 旧文件名
 * @param  newname 新文件名
 * @retval 0
 */
int rename(const char *oldname, const char *newname)
{
    return 0;
}

/* 定义main函数argv的内容 */
/* 可以把命令行内容放入大小为len的cmd缓存区然后返回 */
/* 也可以直接返回一个字符串 */
/* return "./foo -f bar"; */
/**
 * @brief  定义main函数argv的内容（非半主机重定向，未使用命令行）
 * @param  cmd 命令行内容缓存区
 * @param  len 缓存区长度
 * @retval 命令行字符串指针，未使用返回NULL
 */
char *_sys_command_string(char *cmd, int len)
{
    (void)cmd;
    (void)len;
    return NULL;
}

/**
 * @brief  生成临时文件名2（非半主机重定向，空实现）
 * @param  Name 存放文件名的缓冲区
 * @param  sig 文件签名编号
 * @param  maxlen 文件名最大长度
 * @retval 0
 */
int _sys_tmpnam2(char *Name, int sig, unsigned maxlen)
{
    (void)Name;
    (void)sig;
    (void)maxlen;
    return 0;
}
#else
/**
 * @brief  重定向 fputc 函数，目的是使用 printf 函数
 * @param  ch 要输出的字符
 * @param  f 文件指针(这里用不到)
 * @retval 输出的字符ch
 */
int fputc(int ch, FILE *f)
{
    UNUSED(f);
    Serial_SendArray(COM1, (uint8_t *)&ch, 1); /* 发送单字节数据 */
    return (ch);
}

#endif

/**
 * @brief  获取程序运行时间（重定向标准库clock函数）
 * @retval 程序启动至今的毫秒数
 */
clock_t clock(void)
{
    return HAL_GetTick();
}

/**
 * @brief  获取当前时间，对应于time.h（通过RTC获取UTC时间戳）
 * @param  time 指向存储时间结果缓冲区的指针
 * @retval 当前UTC时间戳（秒）
 */
time_t time(time_t *time)
{
    *time = RTC_GetTimestamp();
    return *time;
}

/**
 * @brief  发送系统指令，对应于stdlib.h（空实现，仅判断指令是否为空）
 * @param  cmd 系统指令字符串
 * @retval 0-指令为NULL，-1-有指令（不支持执行）
 */
int system(const char *cmd)
{
    return (cmd) == NULL ? 0 : -1;
}
