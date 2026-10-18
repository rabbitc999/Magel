#ifndef __MALLOC_H
#define __MALLOC_H

#include <stdint.h>
#include <stdlib.h>
#include "bsp_system.h"

/**
 * 定义所使用HyperRAM的规格
 *
 * 0: HyperRAM容量为8MB（默认）
 * 1: HyperRAM容量为32MB
 */
#define CONFIG_HYPERRAM_HIGH_SPEC 0

/* 内存池编号定义 */
#define SRAM_EX                   0 /* XSPI2 HyperRAM内存池，XSPI2 HyperRAM共32MB，LTDC使用了3B */
#define SRAM_IN                   1 /* H7内部的AHB-SRAM1~2 */
#define SRAM_BANK                 2 /* 定义支持的SRAM块数. */

/* 内存管理表类型定义,需保证(((1 << (sizeof(MT_TYPE) * 8)) - 1) * MEMx_BLOCK_SIZE) >= MEMx_MAX_SIZE */
#define MT_TYPE                   uint32_t

/* LTDC 显存预留大小 (5MB) - 根据你的实际屏幕分辨率调整，这里给足余量 */
#define LTDC_RESERVED_SIZE        (5 * 1024 * 1024)
#define LTDC_END_ADDR             (SDRAM_BASE_ADDR + LTDC_RESERVED_SIZE)

/* LVGL 内存池 (mem1) 配置 */
#define MEM1_BLOCK_SIZE           (64)                              /* 内存块大小 64 字节 (Cache Line 对齐) */
#define MEM1_START_ADDR           (LTDC_END_ADDR)                   /* 起始地址: 0xC0500000 */
#define MEM1_MAX_SIZE             (24 * 1024 * 1024)                /* 分配 24MB 给 LVGL */
#define MEM1_ALLOC_TABLE_SIZE     (MEM1_MAX_SIZE / MEM1_BLOCK_SIZE) /* 内存表大小: 393216 */

/* 内存管理表 (Map) 配置 */
/* 放在 SDRAM 的尾部，预留 2MB 空间 (0xC1E00000 - 0xC2000000) */
#define MEM1_MAP_START_ADDR       0xC1E00000

/* mem2内存参数设定.mem2是H7内部的AHB-SRAM1~2 */
#define MEM2_BLOCK_SIZE           (64)                                                                    /* 内存块大小为64字节 */
#define MEM2_MAX_SIZE             (((64 * 1024) / (MEM2_BLOCK_SIZE + sizeof(MT_TYPE))) * MEM2_BLOCK_SIZE) /* AHB-SRAM1~2空闲64K */
#define MEM2_ALLOC_TABLE_SIZE     (MEM2_MAX_SIZE / MEM2_BLOCK_SIZE)                                       /* 内存表大小 */

#if STSTEM_USE_RTOS
/* 互斥量声明 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#endif

/* 内存管理控制器 */
typedef struct __MemoryManager_T
{
    void (*memoryInit)(uint8_t);        /* 初始化 */
    uint16_t (*memoryPerused)(uint8_t); /* 内存使用率 */
    uint8_t *memBase[SRAM_BANK];        /* 内存池 管理SRAM_BANK个区域的内存 */
    MT_TYPE *memMap[SRAM_BANK];         /* 内存管理状态表 */
    uint8_t memRdy[SRAM_BANK];          /* 内存管理是否就绪 */
} MemoryManager_T;

extern MemoryManager_T g_MemoryManager; /* 在malloc.c里面定义 */

/* 用户调用函数 */
void MemoryManager_Init(uint8_t memx);        /* 内存管理初始化函数(外/内部调用) */
uint16_t MemoryManager_Perused(uint8_t memx); /* 获得内存使用率(外/内部调用) */

uint8_t MemoryManager_Free(uint8_t memx, void *ptr);     /* 内存释放(外部调用) */
void *MemoryManager_Malloc(uint8_t memx, uint32_t size); /* 内存分配(外部调用) */

#endif
