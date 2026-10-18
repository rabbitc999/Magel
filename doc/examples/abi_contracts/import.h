#pragma once
#include <stdint.h>
typedef struct Packet
{
    int32_t value;
    double real;
} Packet;
enum Values
{
    FIRST = 7,
    SECOND
};
int32_t identity(int32_t value);
Packet roundtrip(Packet value);
typedef int32_t (*Visit)(int32_t);
int32_t apply(Visit callback, int32_t value);
