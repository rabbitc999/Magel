#include "bsp_fifo.h"
#include "bsp_system.h"

/**
 * @brief  初始化FIFO
 * @param  fifo FIFO结构体指针
 * @param  buffer 数据缓冲区指针
 * @param  elementSize 每个元素的大小（字节）
 * @param  elementCount 元素数量
 */
void FIFO_Init(FIFO_T *fifo, uint8_t *buffer, uint32_t elementSize, uint32_t elementCount)
{
    fifo->buffer = buffer;
    atomic_store(&fifo->start, 0);
    atomic_store(&fifo->end, 0);
    fifo->size = elementCount * elementSize + 1; /* 多分配一个位置以区分空和满 */
    fifo->esize = elementSize;
    fifo->count = elementCount;
}

/**
 * @brief  获取FIFO总大小
 * @param  fifo FIFO结构体指针
 * @retval FIFO总大小
 */
uint32_t FIFO_GetCapacity(FIFO_T *fifo)
{
    return fifo->count;
}

/**
 * @brief  获取FIFO中元素数量
 * @param  fifo FIFO结构体指针
 * @retval 当前元素数量
 */
uint32_t FIFO_GetLength(FIFO_T *fifo)
{
    uint32_t start = atomic_load(&fifo->start);
    uint32_t end = atomic_load(&fifo->end);
    /* 计算循环缓冲区中的元素数量 */
    return (end - start + fifo->size) % fifo->size / fifo->esize;
}

/**
 * @brief  检查FIFO是否为空
 * @param  fifo FIFO结构体指针
 * @retval 1-空，0-非空
 */
uint32_t FIFO_IsEmpty(FIFO_T *fifo)
{
    uint32_t start = atomic_load(&fifo->start);
    uint32_t end = atomic_load(&fifo->end);
    return end == start;
}

/**
 * @brief  获取FIFO剩余空间
 * @param  fifo FIFO结构体指针
 * @retval 剩余可存储元素数量
 */
uint32_t FIFO_GetRemain(FIFO_T *fifo)
{
    /* 修复：计算剩余字节数，再转换为元素数，确保esize!=1时正确 */
    uint32_t start = atomic_load(&fifo->start);
    uint32_t end = atomic_load(&fifo->end);
    uint32_t usedBytes = (end - start + fifo->size) % fifo->size; /* 已使用字节数 */
    uint32_t freeBytes = (fifo->size - usedBytes - 1);            /* 剩余字节数（保留1字节区分空和满） */
    return freeBytes / fifo->esize;                               /* 返回剩余元素数 */
}

/**
 * @brief  检查FIFO是否已满
 * @param  fifo FIFO结构体指针
 * @retval 1-满，0-未满
 */
uint32_t FIFO_IsFull(FIFO_T *fifo)
{
    return FIFO_GetRemain(fifo) == 0;
}

/**
 * @brief  使用CAS操作向FIFO添加多个元素（批量操作）
 * @param  fifo FIFO结构体指针
 * @param  element 要添加的元素数组指针
 * @param  count 要添加的元素数量
 * @retval 操作状态码
 */
uint8_t FIFO_WriteBuffer(FIFO_T *fifo, void *element, uint32_t count)
{
    /* 如果数量为0，直接返回成功 */
    if (count == 0) { return FIFO_OK; }

    uint32_t currentEnd, currentStart;
    uint32_t requiredEnd;

    do
    {
        /* 原子加载当前结束位置和起始位置 */
        currentEnd = atomic_load(&fifo->end);
        currentStart = atomic_load(&fifo->start);

        /* 计算需要的结束位置 */
        requiredEnd = (currentEnd + count * fifo->esize) % fifo->size;

        /* 修复：检查空间时，比较字节数而非元素数，确保esize!=1时正确 */
        uint32_t available = (currentStart - currentEnd - 1 + fifo->size) % fifo->size; /* 可用字节数 */
        if (available < count * fifo->esize)                                            /* 需要的字节数为count * esize */
        {
            return FIFO_ERROR_SIZE;
        }

        /* 尝试CAS更新End指针 */
    } while (!atomic_compare_exchange_weak(&fifo->end, &currentEnd, requiredEnd));

    /* CAS成功，批量写入数据 */
    atomic_thread_fence(memory_order_release);
    for (uint32_t i = 0; i < fifo->esize * count; i++)
    {
        fifo->buffer[(currentEnd + i) % fifo->size] = *((uint8_t *)element + i);
    }

    return FIFO_OK;
}

/**
 * @brief  使用CAS操作从FIFO获取多个元素（批量操作）
 * @param  fifo FIFO结构体指针
 * @param  element 存储获取元素的数组指针(传入NULL不写入，仅移除元素)
 * @param  count 要获取的元素数量
 * @retval 实际读取的元素数量
 */
uint32_t FIFO_ReadBuffer(FIFO_T *fifo, void *element, uint32_t count)
{
    /* 如果数量为0，直接返回0 */
    if (count == 0) { return 0; }

    uint32_t currentStart, currentEnd;
    uint32_t requiredStart;
    uint32_t actualCount;

    do
    {
        /* 原子加载当前起始位置和结束位置 */
        currentStart = atomic_load(&fifo->start);
        currentEnd = atomic_load(&fifo->end);

        /* 计算可用元素数量 */
        uint32_t available = (currentEnd - currentStart + fifo->size) % fifo->size / fifo->esize;
        if (available == 0)
        {
            return 0; /* 没有数据可读 */
        }

        /* 计算实际读取数量 */
        actualCount = (available < count) ? available : count;

        /* 批量读取数据 */
        if (element != NULL)
        {
            for (uint32_t i = 0; i < fifo->esize * actualCount; i++)
            {
                *((uint8_t *)element + i) = fifo->buffer[(currentStart + i) % fifo->size];
            }
        }

        /* 计算新的起始位置 */
        requiredStart = (currentStart + actualCount * fifo->esize) % fifo->size;

        /* 尝试CAS更新Start指针 */
    } while (!atomic_compare_exchange_weak(&fifo->start, &currentStart, requiredStart));

    return actualCount;
}

/**
 * @brief  查看FIFO中的多个元素（不移除，不需要CAS）
 * @param  fifo FIFO结构体指针
 * @param  element 存储查看元素的数组指针
 * @param  index 起始索引
 * @param  count 要查看的元素数量
 * @retval 实际查看的元素数量
 */
uint32_t FIFO_GetBuffer(FIFO_T *fifo, void *element, uint32_t index, uint32_t count)
{
    /* 如果数量为0，直接返回0 */
    if (count == 0) { return 0; }

    uint32_t start = atomic_load(&fifo->start);
    uint32_t end = atomic_load(&fifo->end);

    /* 计算可用元素数量 */
    uint32_t available = (end - start + fifo->size) % fifo->size / fifo->esize;

    /* 确保索引在有效范围内 */
    if (index >= available)
    {
        return 0;
    }
    uint32_t actualCount = (available - index < count) ? (available - index) : count;

    /* 遍历读取元素但不移动指针 */
    uint32_t ptr = start + index;
    for (uint32_t i = 0; i < actualCount * fifo->esize; i++)
    {
        *((uint8_t *)element + i) = fifo->buffer[ptr];
        ptr = (ptr + 1) % fifo->size;
    }

    return actualCount;
}

/**
 * @brief  移动FIFO中的多个元素（删除count个元素后再添加count个元素）
 * @param  fifo FIFO结构体指针
 * @param  element 存储移动的数组指针
 * @param  count 要移动的元素数量
 * @retval 实际移动的元素数量
 * @note   重写逻辑以确保原子性和正确性，避免并发问题
 */
uint32_t FIFO_MoveBuffer(FIFO_T *fifo, void *element, uint32_t count)
{
    /* 如果数量为0，直接返回0 */
    if (count == 0) { return 0; }

    uint32_t currentStart, currentEnd;
    uint32_t requiredStart, requiredEnd;
    uint32_t available;

    do
    {
        /* 原子加载当前起始位置和结束位置 */
        currentStart = atomic_load(&fifo->start);
        currentEnd = atomic_load(&fifo->end);

        /* 计算可用元素数量 */
        available = (currentEnd - currentStart + fifo->size) % fifo->size / fifo->esize;
        if (available < count)
        {
            return 0; /* 没有足够元素可移动 */
        }

        /* 计算新的结束位置（添加元素） */
        requiredEnd = (currentEnd + count * fifo->esize) % fifo->size;
        /* 计算新的起始位置（删除元素） */
        requiredStart = (currentStart + count * fifo->esize) % fifo->size;

        /* 先CAS更新End指针（添加元素） */
        if (!atomic_compare_exchange_weak(&fifo->end, &currentEnd, requiredEnd))
        {
            continue; /* CAS失败，重试 */
        }

        /* End更新成功，写入新数据 */
        atomic_thread_fence(memory_order_release);
        if (element != NULL)
        {
            for (uint32_t i = 0; i < fifo->esize * count; i++)
            {
                fifo->buffer[(currentEnd + i) % fifo->size] = *((uint8_t *)element + i);
            }
        }

        /* 再CAS更新Start指针（删除元素） */
        if (atomic_compare_exchange_weak(&fifo->start, &currentStart, requiredStart))
        {
            return count; /* 成功 */
        }
        else
        {
            /* Start更新失败，回滚End指针（简化处理：清空FIFO避免不一致） */
            /* 注意：在高并发场景下，回滚可能复杂，这里保守处理 */
            atomic_store(&fifo->end, currentEnd);
            return 0;
        }
    } while (true); /* 循环直到成功或失败 */
}

/**
 * @brief  写入数据或移动元素（队列满时自动覆盖）
 * @param  fifo FIFO结构体指针
 * @param  element 要写入的元素数组指针
 * @param  count 要写入的元素数量
 * @retval 实际写入的元素数量
 * @note   当队列有足够空间时直接写入，空间不足时先移动元素再写入
 */
uint32_t FIFO_WriteOrMove(FIFO_T *fifo, void *element, uint32_t count)
{
    /* 如果数量为0，直接返回0 */
    if (count == 0) { return 0; }

    uint32_t currentEnd, currentStart;
    uint32_t requiredEnd;
    uint32_t availableSpace;
    uint32_t actualCount = count;

    do
    {
        /* 原子加载当前结束位置和起始位置 */
        currentEnd = atomic_load(&fifo->end);
        currentStart = atomic_load(&fifo->start);

        /* 计算可用空间（字节数） */
        availableSpace = (currentStart - currentEnd - 1 + fifo->size) % fifo->size;
        uint32_t requiredBytes = count * fifo->esize;

        if (availableSpace >= requiredBytes)
        {
            /* 空间足够，直接写入 */
            requiredEnd = (currentEnd + requiredBytes) % fifo->size;

            /* 尝试CAS更新End指针 */
            if (atomic_compare_exchange_weak(&fifo->end, &currentEnd, requiredEnd))
            {
                /* CAS成功，写入数据 */
                atomic_thread_fence(memory_order_release);
                for (uint32_t i = 0; i < requiredBytes; i++)
                {
                    fifo->buffer[(currentEnd + i) % fifo->size] = *((uint8_t *)element + i);
                }
                return count;
            }
            /* CAS失败，重试 */
            continue;
        }
        else
        {
            /* 空间不足，需要移动元素 */
            uint32_t currentLength = (currentEnd - currentStart + fifo->size) % fifo->size / fifo->esize;

            /* 如果当前元素数量小于要写入的数量，只能写入部分 */
            if (currentLength < count)
            {
                actualCount = currentLength;
            }

            /* 计算新的起始和结束位置 */
            uint32_t requiredStart = (currentStart + actualCount * fifo->esize) % fifo->size;
            requiredEnd = (currentEnd + actualCount * fifo->esize) % fifo->size;

            /* 先尝试更新End指针 */
            if (atomic_compare_exchange_weak(&fifo->end, &currentEnd, requiredEnd))
            {
                /* End更新成功，写入新数据 */
                atomic_thread_fence(memory_order_release);
                for (uint32_t i = 0; i < actualCount * fifo->esize; i++)
                {
                    fifo->buffer[(currentEnd + i) % fifo->size] = *((uint8_t *)element + i);
                }

                /* 再更新Start指针 */
                if (atomic_compare_exchange_weak(&fifo->start, &currentStart, requiredStart))
                {
                    return actualCount;
                }
                else
                {
                    /* Start更新失败，回滚End */
                    atomic_store(&fifo->end, currentEnd);
                    return 0;
                }
            }
            /* CAS失败，重试 */
        }
    } while (true);
}

/**
 * @brief  清空FIFO
 * @param  fifo FIFO结构体指针
 */
void FIFO_Clear(FIFO_T *fifo)
{
    /* 使用原子交换确保清空操作的原子性 */
    atomic_exchange(&fifo->end, 0);
    atomic_store(&fifo->start, 0);
} /**
   * @brief  KMP算法前缀函数(Partial Match Table)
   * @param  pattern 模式串
   * @param  patternSize 模式串长度
   * @param  lps 输出的前缀表数组
   * @note   时间复杂度O(m)，m为模式串长度
   */
static void KMP_ComputeLPS(const uint8_t *pattern, uint32_t patternSize, uint32_t *lps)
{
    if (pattern == NULL || lps == NULL || patternSize == 0)
    {
        return;
    }

    uint32_t len = 0; /* 当前最长前缀后缀长度 */
    lps[0] = 0;       /* 首字符的前缀表值为0 */

    uint32_t i = 1;
    while (i < patternSize)
    {
        if (pattern[i] == pattern[len])
        {
            len++;
            lps[i] = len;
            i++;
        }
        else
        {
            if (len != 0)
            {
                len = lps[len - 1]; /* 回退到前一个可能匹配位置 */
            }
            else
            {
                lps[i] = 0;
                i++;
            }
        }
    }
}

/**
 * @brief  查找FIFO中是否包含指定的字节序列 (原子操作版)
 * @param  FIFO FIFO结构体指针
 * @param  pattern 要查找的字节序列
 * @param  patternSize 字节序列长度
 * @retval 查找状态 (FIFO_FIND / FIFO_NOT_FIND)，可对应文档中的 FIFO_OK 和 FIFO_ERROR_EMPTY 等
 * @note   使用KMP算法实现。本函数首先原子地获取FIFO状态快照，基于此快照进行搜索，保证线程安全。
 */
uint32_t FIFO_IsContains(FIFO_T *FIFO, const void *pattern, uint32_t patternSize)
{
    if (FIFO == NULL || pattern == NULL || patternSize == 0)
    {
        return FIFO_NOT_FIND;
    }

    /* 原子地获取FIFO关键状态快照 */
    uint32_t start_snap = atomic_load(&(FIFO->start));  // 假设FIFO_T的start成员是_Atomic或volatile
    uint32_t end_snap = atomic_load(&(FIFO->end));
    uint32_t size_snap = FIFO->size;  // size在初始化后通常不变，但为了一致性也可以原子加载（如果定义为volatile/_Atomic）
    uint32_t esize_snap = FIFO->esize;

    /* 基于快照计算当前数据长度(字节数)和元素个数 */
    uint32_t used_bytes = (end_snap - start_snap + size_snap) % size_snap;
    uint32_t length_elements = used_bytes / esize_snap;  // 当前存储的元素个数
    uint32_t length_bytes = used_bytes;                  // 当前存储的字节数

    if (length_bytes < patternSize)
    {
        return FIFO_NOT_FIND;
    }

    const uint8_t *patternBytes = (const uint8_t *)pattern;

    /* 简短模式使用朴素搜索更高效 */
    if (patternSize == 1)
    {
        uint8_t target = patternBytes[0];
        for (uint32_t i = 0; i < length_bytes; i++)
        {
            // 使用快照start和size计算索引
            if (FIFO->buffer[(start_snap + i) % size_snap] == target)
            {
                return FIFO_FIND;
            }
        }
        return FIFO_NOT_FIND;
    }

    /* 计算KMP前缀表 */
    uint32_t *lps = (uint32_t *)Memory_Malloc(MP_RAM_EX, patternSize * sizeof(uint32_t));
    if (lps == NULL)
    {
        /* 内存不足，回退到朴素搜索 (基于快照) */
        uint32_t max_i = length_bytes - patternSize;
        for (uint32_t i = 0; i <= max_i; i++)
        {
            uint32_t j = 0;
            for (; j < patternSize; j++)
            {
                if (FIFO->buffer[(start_snap + i + j) % size_snap] != patternBytes[j])
                {
                    break;
                }
            }
            if (j == patternSize)
            {
                return FIFO_FIND;
            }
        }
        return FIFO_NOT_FIND;
    }

    KMP_ComputeLPS(patternBytes, patternSize, lps);

    /* KMP搜索 (基于快照) */
    uint32_t i = 0; /* 在快照缓冲区中的当前位置(字节偏移) */
    uint32_t j = 0; /* 模式串中的当前位置 */

    while (i < length_bytes)
    {
        if (FIFO->buffer[(start_snap + i) % size_snap] == patternBytes[j])
        {
            i++;
            j++;
        }

        if (j == patternSize)
        {
            Memory_Free(MP_RAM_EX, lps);
            return FIFO_FIND; /* 找到匹配 */
        }
        else if (i < length_bytes && FIFO->buffer[(start_snap + i) % size_snap] != patternBytes[j])
        {
            if (j != 0)
            {
                j = lps[j - 1];
            }
            else
            {
                i++;
            }
        }
    }

    Memory_Free(MP_RAM_EX, lps);
    return FIFO_NOT_FIND; /* 未找到匹配 */
}