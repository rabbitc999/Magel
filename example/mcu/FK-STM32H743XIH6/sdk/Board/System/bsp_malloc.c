#include "bsp_malloc.h"
#include "bsp_memory.h"

/* 内存池定义 */
static uint8_t s_Mem1Base[MEM1_MAX_SIZE] __attribute__((aligned(64))) __attribute__((section(".bss.ARM.__at_0xC0500000"))); /* XSPI2 HyperRAM内存池 */
#if SRAM_BANK >= 2
static uint8_t s_Mem2Base[MEM2_MAX_SIZE] __attribute__((aligned(64))); /* H7内部AHB-SRAM1~2内存池 */
#endif

/* 内存管理表 */
static MT_TYPE s_Mem1MapBase[MEM1_ALLOC_TABLE_SIZE] __attribute__((section(".bss.ARM.__at_0xC1E00000"))); /* XSPI2 HyperRAM内存池MAP */
#if SRAM_BANK >= 2
static MT_TYPE s_Mem2MapBase[MEM2_ALLOC_TABLE_SIZE]; /* H7内部AHB-SRAM1~2内存池MAP */
#endif

/* 内存管理参数 */
#if SRAM_BANK >= 2
static const uint32_t s_MemTblSize[SRAM_BANK] = { MEM1_ALLOC_TABLE_SIZE, MEM2_ALLOC_TABLE_SIZE }; /* 内存表大小 */
static const uint32_t s_MemBlkSize[SRAM_BANK] = { MEM1_BLOCK_SIZE, MEM2_BLOCK_SIZE };             /* 内存分块大小 */
static const uint32_t s_MemSize[SRAM_BANK] = { MEM1_MAX_SIZE, MEM2_MAX_SIZE };                    /* 内存总大小 */
#else
static const uint32_t s_MemTblSize[SRAM_BANK] = { MEM1_ALLOC_TABLE_SIZE }; /* 内存表大小 */
static const uint32_t s_MemBlkSize[SRAM_BANK] = { MEM1_BLOCK_SIZE };       /* 内存分块大小 */
static const uint32_t s_MemSize[SRAM_BANK] = { MEM1_MAX_SIZE };            /* 内存总大小 */
#endif

/* 内存管理控制器 */
static MemoryManager_T s_MemoryManager = {
    .memoryInit = MemoryManager_Init,       /* 内存初始化 */
    .memoryPerused = MemoryManager_Perused, /* 内存使用率 */
#if SRAM_BANK >= 2
    .memBase = { s_Mem1Base, s_Mem2Base },
    .memMap = { s_Mem1MapBase, s_Mem2MapBase },
    .memRdy = { 0, 0 }, /* 内存管理未就绪 */
#else
    .memBase = { s_Mem1Base },
    .memMap = { s_Mem1MapBase },
    .memRdy = { 0 }, /* 内存管理未就绪 */
#endif
};

/**
 * @brief  内存管理初始化
 * @param  memx 所属内存块
 */
void MemoryManager_Init(uint8_t memx)
{
    uint8_t mttSize = sizeof(MT_TYPE);                                         /* 获取memmap数组的类型长度(uint16_t /uint32_t)*/
    Memory_Set(s_MemoryManager.memMap[memx], 0, s_MemTblSize[memx] * mttSize); /* 内存状态表数据清零 */
    s_MemoryManager.memRdy[memx] = 1;                                          /* 内存管理初始化OK */
}

/**
 * @brief  获取内存使用率
 * @param  memx 所属内存块
 * @retval 使用率(扩大了10倍,0~1000,代表0.0%~100.0%)
 */
uint16_t MemoryManager_Perused(uint8_t memx)
{
    uint32_t used = 0;
    uint32_t i;

    for (i = 0; i < s_MemTblSize[memx]; i++)
    {
        if (s_MemoryManager.memMap[memx][i]) used++;
    }

    return (used * 1000) / (s_MemTblSize[memx]);
}

/**
 * @brief  内存分配(内部调用)
 * @param  memx 所属内存块
 * @param  size 要分配的内存大小(字节)
 * @retval 内存偏移地址
 *   @arg  0 ~ 0XFFFFFFFE : 有效的内存偏移地址
 *   @arg  0XFFFFFFFF     : 无效的内存偏移地址
 */
static uint32_t _MemoryManager_Malloc(uint8_t memx, uint32_t size)
{
    signed long offset = 0;
    uint32_t nmemb;     /* 需要的内存块数 */
    uint32_t cmemb = 0; /* 连续空内存块数 */
    uint32_t i;

    if (!s_MemoryManager.memRdy[memx])
    {
        s_MemoryManager.memoryInit(memx); /* 未初始化,先执行初始化 */
    }

    if (size == 0) return 0XFFFFFFFF; /* 不需要分配 */

    nmemb = size / s_MemBlkSize[memx]; /* 获取需要分配的连续内存块数 */

    if (size % s_MemBlkSize[memx]) nmemb++;

    for (offset = s_MemTblSize[memx] - 1; offset >= 0; offset--) /* 搜索整个内存控制区 */
    {
        if (!s_MemoryManager.memMap[memx][offset])
        {
            cmemb++; /* 连续空内存块数增加 */
        }
        else
        {
            cmemb = 0; /* 连续内存块清零 */
        }

        if (cmemb == nmemb) /* 找到了连续nmemb个空内存块 */
        {
            for (i = 0; i < nmemb; i++) /* 标注内存块非空 */
            {
                s_MemoryManager.memMap[memx][offset + i] = nmemb;
            }

            return (offset * s_MemBlkSize[memx]); /* 返回偏移地址 */
        }
    }

    return 0XFFFFFFFF; /* 未找到符合分配条件的内存块 */
}

/**
 * @brief  释放内存(内部调用) - 增强版
 * @param  memx 所属内存块
 * @param  offset 内存地址偏移
 * @retval 释放结果
 *   @arg  0, 释放成功;
 *   @arg  1, 释放失败;
 *   @arg  2, 超区域了(失败);
 *   @arg  3, 地址未对齐(失败);
 *   @arg  4, 重复释放(失败);
 *   @arg  5, 释放未分配的内存(失败);
 */
static uint8_t _MemoryManager_Free(uint8_t memx, uint32_t offset)
{
    int i;

    if (!s_MemoryManager.memRdy[memx]) /* 未初始化,先执行初始化 */
    {
        s_MemoryManager.memoryInit(memx);
        return 1; /* 未初始化 */
    }

    /* 检查1: 偏移地址是否在内存池范围内 */
    if (offset >= s_MemSize[memx])
    {
        return 2; /* 偏移超区了 */
    }

    /* 检查2: 偏移地址是否对齐到内存块边界 */
    if (offset % s_MemBlkSize[memx] != 0)
    {
        return 3; /* 地址未对齐 */
    }

    int index = offset / s_MemBlkSize[memx];         /* 偏移所在内存块号码 */
    int nmemb = s_MemoryManager.memMap[memx][index]; /* 内存块数量 */

    /* 检查3: 是否释放未分配的内存 */
    if (nmemb == 0)
    {
        return 5; /* 释放未分配的内存 */
    }

    /* 检查4: 是否重复释放（检查整个内存块范围） */
    for (i = 0; i < nmemb; i++)
    {
        if (s_MemoryManager.memMap[memx][index + i] == 0)
        {
            return 4; /* 重复释放 */
        }
    }

    /* 所有检查通过，执行释放 */
    for (i = 0; i < nmemb; i++) /* 内存块清零 */
    {
        s_MemoryManager.memMap[memx][index + i] = 0;
    }

    return 0; /* 释放成功 */
}

/**
 * @brief  释放内存(外部调用) - 增强版
 * @param  memx 所属内存块
 * @param  ptr 内存首地址
 * @retval 释放结果
 *   @arg  0, 释放成功;
 *   @arg  1, 释放失败(各种原因);
 */
uint8_t MemoryManager_Free(uint8_t memx, void *ptr)
{
    uint32_t offset;
    uint8_t result;

    /* 检查1: 指针是否为NULL */
    if (ptr == NULL)
    {
        return 1; /* 地址为0 */
    }

    /* 检查2: 指针是否在有效的内存池范围内 */
    uint32_t memStart = (uint32_t)s_MemoryManager.memBase[memx];
    uint32_t memEnd = memStart + s_MemSize[memx];
    uint32_t ptrAddr = (uint32_t)ptr;

    if (ptrAddr < memStart || ptrAddr >= memEnd)
    {
        return 1; /* 指针超出内存池范围 */
    }

    /* 计算偏移地址 */
    offset = ptrAddr - memStart;

    /* 调用内部释放函数 */
    result = _MemoryManager_Free(memx, offset);

    return result;
}

/**
 * @brief  分配内存(外部调用)
 * @param  memx 所属内存块
 * @param  size 要分配的内存大小(字节)
 * @retval 分配到的内存首地址，分配失败返回NULL
 */
void *MemoryManager_Malloc(uint8_t memx, uint32_t size)
{
    uint32_t offset;
    void *newPtr = NULL;

    offset = _MemoryManager_Malloc(memx, size);

    if (offset == 0XFFFFFFFF) /* 申请出错 */
    {
        newPtr = NULL; /* 返回空(0) */
    }
    else /* 申请没问题, 返回首地址 */
    {
        newPtr = (void *)((uint32_t)s_MemoryManager.memBase[memx] + offset);
    }

    /* 返回新的内存地址 */
    return newPtr;
}
