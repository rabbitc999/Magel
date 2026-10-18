#ifndef __BSP_FIFO_H
#define __BSP_FIFO_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/* FIFO操作状态码 */
#define FIFO_OK             0 /* 操作成功 */
#define FIFO_ERROR_FULL     1 /* FIFO已满错误 */
#define FIFO_ERROR_EMPTY    2 /* FIFO为空错误 */
#define FIFO_ERROR_SIZE     3 /* 大小错误（批量操作时空间不足） */
#define FIFO_ERROR_OTHER    3 /* 其他错误 */

/* FIFO查找状态码 */
#define FIFO_FIND           1
#define FIFO_NOT_FIND       0

/* 是否支持原子操作头文件 */
#define FIFO_ATOMIC_SUPPORT 1

/* 如果不支持原子操作，提供基本实现 */
#if defined(FIFO_ATOMIC_SUPPORT) && FIFO_ATOMIC_SUPPORT
#include <stdatomic.h>
#else
/* 简单的原子操作模拟（非线程安全，仅用于单线程环境） */
#define _Atomic                volatile
#define atomic_store(ptr, val) (*(ptr) = (val))
#define atomic_load(ptr)       (*(ptr))
#define atomic_compare_exchange_weak(ptr, expected, desired) \
    (*(ptr) == *(expected) ? (*(ptr) = (desired), 1) : (*(expected) = *(ptr), 0))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FIFO结构体定义
 *
 * 使用循环缓冲区实现的线程安全FIFO，支持多生产者和多消费者
 */
typedef struct __FIFO_T
{
#if defined(FIFO_ATOMIC_SUPPORT) && FIFO_ATOMIC_SUPPORT
    _Atomic uint32_t start; /* 起始位置指针（原子操作） */
    _Atomic uint32_t end;   /* 结束位置指针（原子操作） */
#else
    volatile uint32_t start; /* 起始位置指针（原子操作） */
    volatile uint32_t end;   /* 结束位置指针（原子操作） */
#endif
    volatile uint32_t size;   /* 缓冲区总大小 */
    volatile uint32_t count;  /* 元素数量 */
    volatile uint32_t esize;  /* 每个元素的大小（字节） */
    volatile uint8_t *buffer; /* 数据缓冲区指针 */
} FIFO_T;

void FIFO_Init(FIFO_T *fifo, uint8_t *buffer, uint32_t elementSize, uint32_t elementCount);
uint32_t FIFO_GetCapacity(FIFO_T *fifo);
uint32_t FIFO_GetLength(FIFO_T *fifo);
uint32_t FIFO_IsEmpty(FIFO_T *fifo);
uint32_t FIFO_GetRemain(FIFO_T *fifo);
uint32_t FIFO_IsFull(FIFO_T *fifo);
uint8_t FIFO_WriteBuffer(FIFO_T *fifo, void *element, uint32_t count);
uint32_t FIFO_ReadBuffer(FIFO_T *fifo, void *element, uint32_t count);
uint32_t FIFO_GetBuffer(FIFO_T *fifo, void *element, uint32_t index, uint32_t count);
uint32_t FIFO_MoveBuffer(FIFO_T *fifo, void *element, uint32_t count);
uint32_t FIFO_WriteOrMove(FIFO_T *fifo, void *element, uint32_t count);
uint32_t FIFO_IsContains(FIFO_T *FIFO, const void *pattern, uint32_t patternSize);
void FIFO_Clear(FIFO_T *fifo);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_FIFO_H */
