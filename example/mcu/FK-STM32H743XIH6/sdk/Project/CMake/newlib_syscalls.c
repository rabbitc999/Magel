/* Newlib I/O uses the same FatFs volume as the existing Keil runtime. */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "ff.h"
#include "bsp_uart.h"
#include "bsp_timer.h"

static FIL files[8];
static uint8_t used[8];
extern char __heap_start, __heap_end;
void *_sbrk(int increment)
{
    static char *cursor;
    if (!cursor) cursor = &__heap_start;
    if ((increment > 0 && increment > &__heap_end - cursor) || (increment < 0 && increment < &__heap_start - cursor))
    {
        errno = ENOMEM;
        return (void *)-1;
    }
    char *previous = cursor;
    cursor += increment;
    return previous;
}
int _open(const char *path, int flags, ...)
{
    BYTE mode = (flags & O_RDWR) ? FA_READ | FA_WRITE : ((flags & O_WRONLY) ? FA_WRITE : FA_READ);
    if (flags & O_APPEND)
        mode |= FA_OPEN_APPEND;
    else if (flags & O_TRUNC)
        mode |= FA_CREATE_ALWAYS;
    else if (flags & O_CREAT)
        mode |= (flags & O_EXCL) ? FA_CREATE_NEW : FA_OPEN_ALWAYS;
    for (unsigned i = 0; i < 8; i++)
        if (!used[i])
        {
            if (f_open(&files[i], path, mode) != FR_OK)
            {
                errno = EIO;
                return -1;
            }
            used[i] = 1;
            return i + 3;
        }
    errno = EMFILE;
    return -1;
}
static FIL *file(int fd) { return fd >= 3 && fd < 11 && used[fd - 3] ? &files[fd - 3] : NULL; }
int _close(int fd)
{
    FIL *f = file(fd);
    if (!f)
    {
        errno = EBADF;
        return -1;
    }
    FRESULT result = f_close(f);
    if (result != FR_OK)
    {
        errno = EIO;
        return -1;
    }
    used[fd - 3] = 0;
    return 0;
}
int _write(int fd, const void *buffer, size_t count)
{
    if (fd == 1 || fd == 2)
    {
#if defined(FK_VM_DEBUG_SYNC_IO) && FK_VM_DEBUG_SYNC_IO
        UART_SendArrayImm(COM1, (uint8_t *)buffer, count);
#else
        UART_SendArray(COM1, (uint8_t *)buffer, count);
#endif
        return count;
    }
    FIL *f = file(fd);
    UINT written;
    if (!f)
    {
        errno = EBADF;
        return -1;
    }
    if (f_write(f, buffer, count, &written) != FR_OK)
    {
        errno = EIO;
        return -1;
    }
    return written;
}
int _read(int fd, void *buffer, size_t count)
{
    if (fd == 0) return 0;
    FIL *f = file(fd);
    UINT received;
    if (!f)
    {
        errno = EBADF;
        return -1;
    }
    if (f_read(f, buffer, count, &received) != FR_OK)
    {
        errno = EIO;
        return -1;
    }
    return received;
}
long _lseek(int fd, long offset, int origin)
{
    FIL *f = file(fd);
    if (!f)
    {
        errno = EBADF;
        return -1;
    }
    int64_t base = origin == 0 ? 0 : origin == 1 ? f_tell(f) :
                                 origin == 2     ? f_size(f) :
                                                   -1;
    int64_t position = base + offset;
    if (base < 0 || position < 0 || position > UINT32_MAX)
    {
        errno = EINVAL;
        return -1;
    }
    if (f_lseek(f, position) != FR_OK)
    {
        errno = EIO;
        return -1;
    }
    return f_tell(f);
}
int _fstat(int fd, struct stat *s)
{
    memset(s, 0, sizeof(*s));
    if (fd >= 0 && fd <= 2)
    {
        s->st_mode = S_IFCHR;
        return 0;
    }
    FIL *f = file(fd);
    if (!f)
    {
        errno = EBADF;
        return -1;
    }
    s->st_mode = S_IFREG;
    s->st_size = f_size(f);
    return 0;
}
int _stat(const char *path, struct stat *s)
{
    FILINFO info;
    if (f_stat(path, &info) != FR_OK)
    {
        errno = ENOENT;
        return -1;
    }
    memset(s, 0, sizeof(*s));
    s->st_size = info.fsize;
    s->st_mode = (info.fattrib & AM_DIR) ? S_IFDIR : S_IFREG;
    return 0;
}
int _isatty(int fd) { return fd >= 0 && fd <= 2; }
int _unlink(const char *path)
{
    if (f_unlink(path) == FR_OK) return 0;
    errno = EIO;
    return -1;
}
int _rename(const char *a, const char *b)
{
    if (f_rename(a, b) == FR_OK) return 0;
    errno = EIO;
    return -1;
}
int _gettimeofday(struct timeval *tv, void *zone)
{
    (void)zone;
    uint64_t ms = Timer_GetTimestampMs();
    if (tv)
    {
        tv->tv_sec = ms / 1000;
        tv->tv_usec = (ms % 1000) * 1000;
    }
    return 0;
}
int _getpid(void) { return 1; }
int _kill(int pid, int signal)
{
    (void)pid;
    (void)signal;
    errno = EINVAL;
    return -1;
}
void _exit(int status)
{
    (void)status;
    __disable_irq();
    for (;;) { __WFI(); }
}
void _init(void) {}
void _fini(void) {}
