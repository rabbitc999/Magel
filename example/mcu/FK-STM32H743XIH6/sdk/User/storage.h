#ifndef __STORAGE_H
#define __STORAGE_H
#include <stdint.h>
uint8_t Storage_Command(uint8_t port, const char *operation, const char *path);
void Storage_Poll(void);
#endif
