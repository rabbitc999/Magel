/**
 * @file native_memory_access.hpp
 * @brief VM 内部 native memory access 模块的类型与接口。
 */
#pragma once
#include "runtime_objects.hpp"
#include "value_transfer.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief 验证访问完全位于同一个活跃分配中，避免宽访问跨过块尾。
 * @param heap 分配器。
 * @param address 地址。
 * @param count 字节数。
 * @return 零长度或有效分配范围返回 true。
 */
bool allocatedHeapRange(const HeapAllocator &heap, std::size_t address, std::size_t count);

/**
 * @brief 从堆或数据区里读一段字节。
 * @param ctx 环境。
 * @param address 地址。
 * @param out 输出缓冲。
 * @param count 字节数。
 * @return 读得到返回 true（已释放 / 越界返回 false）。
 */
bool nativeRead(const NativeContext &ctx, std::size_t address, char *out, std::size_t count);

/**
 * @brief 往堆或数据区按字节写一段（改字符串里的某个字节用它）。
 * @param ctx 环境。
 * @param address 地址。
 * @param in 输入缓冲。
 * @param count 字节数。
 * @return 成功返回 true。
 */
bool nativeWrite(NativeContext &ctx, std::size_t address, const char *in, std::size_t count);

/**
 * @brief 读取显式长度的堆字符串，或读取生成期带哨兵的数据区字面量。
 * @param ctx 环境。
 * @param address 地址（0 = 空指针）。
 * @param out 结果。
 * @return 成功返回 true；长度非法、地址不可读或数据区哨兵缺失返回 false。
 */
bool nativeReadCString(const NativeContext &ctx, std::size_t address, Memory::String *out);

/**
 * @brief 造一个显式长度的堆字符串，返回长度字段的地址。
 * @param ctx 环境。
 * @param text UTF-8 字节内容。
 * @return 地址；0 = 失败（`ctx.error` 里是原因）。
 */
std::size_t nativeAllocString(NativeContext &ctx, const Memory::String &text);
}  // namespace stackvm::runtimeDetail
