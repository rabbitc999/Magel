#ifndef PARSE_ENGINE_MCU_VM_METADATA_HEAP_HPP
#define PARSE_ENGINE_MCU_VM_METADATA_HEAP_HPP
/**
 * @file mcu_metadata_heap.hpp
 * @brief 元数据 arena 的观测接口（footprint 报告 / 板端调预算用）。
 */

#include <cstdint>

/**
 * @brief 元数据 arena 是否已经用上（上过就不能换 arena）。
 */
bool mcuMetadataInUse();
/**
 * @brief 指定元数据 arena 的内存（外部 SDRAM / PSRAM / 板级 arena）。
 *
 * 必须在第一次 `new` 之前调用；已经在用返回 false。见 `mcu_arenas.hpp`。
 * @param memory 起始地址（按 `std::max_align_t` 对齐）。
 * @param bytes 字节数。
 * @return 成功返回 true。
 */
bool mcuMetadataSetArena(void *memory, std::uint32_t bytes);

/**
 * @brief 元数据 arena 的总容量（字节）。
 */
std::uint32_t mcuMetadataCapacityBytes();
/**
 * @brief 元数据 arena 当前还在用的字节数。
 */
std::uint32_t mcuMetadataLiveBytes();
/**
 * @brief 元数据 arena 的历史峰值（回调后不复位）——**这个数决定板端要留多少 RAM**。
 */
std::uint32_t mcuMetadataPeakBytes();
/**
 * @brief 元数据 arena 的块表当前条目数（看碎片化）。
 */
std::uint32_t mcuMetadataBlockCount();

/**
 * @brief 软重启用：整块元数据 arena（含应急预留）重来。
 *
 * 只允许在"可恢复终止"之后调用——上一轮 `new` 出来的对象已经不可信。调用方必须保证
 * 不再持有上一轮 `new` 出来的任何对象（板端宿主的跨轮状态应当是静态 / POD）。
 */
void mcuMetadataResetForRestart();
/**
 * @brief 元数据应急预留还剩多少字节（footprint / 排查用）。
 */
std::uint32_t mcuMetadataRescueRemainingBytes();

#endif
