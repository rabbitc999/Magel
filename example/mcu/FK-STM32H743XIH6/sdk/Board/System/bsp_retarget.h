#ifndef __RETARGET_H
#define __RETARGET_H

#include "stm32h7xx_hal.h"

#define USE_MICRO_LIB 0

#if !(defined(USE_MICRO_LIB) && USE_MICRO_LIB)
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int _isatty(int fd);
int _write(int fd, char *ptr, int len);
int _close(int fd);
int _lseek(int fd, int ptr, int dir);
int _read(int fd, char *ptr, int len);

#ifdef __cplusplus
}
#endif /* #ifdef __cplusplus */
#endif /* #ifndef _RETARGET_H */

#endif /* #ifndef __RETARGET_H */
