/**
 * @file native_memory_access.cpp
 * @brief 受检查的堆读写、字符串分配与地址范围验证。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
/**
 * @brief 验证访问完全位于同一个活跃分配中，避免宽访问跨过块尾。
 * @param heap 分配器。
 * @param address 地址。
 * @param count 字节数。
 * @return 零长度或有效分配范围返回 true。
 */
bool allocatedHeapRange(const HeapAllocator &heap, std::size_t address, std::size_t count)
{
#if STACKVM_WITH_ALLOCATION_GUARDS
    std::size_t capacity = 0;
    return count == 0 || (heap.capacityAt(address, capacity) && count <= capacity);
#else
    (void)heap;
    (void)address;
    (void)count;
    return true;
#endif
}

/**
 * @brief 从堆或数据区里读一段字节。
 * @param ctx 环境。
 * @param address 地址。
 * @param out 输出缓冲。
 * @param count 字节数。
 * @return 读得到返回 true（已释放 / 越界返回 false）。
 */
bool nativeRead(const NativeContext &ctx, std::size_t address, char *out, std::size_t count)
{
    if (count == 0) return true;
#if STACKVM_WITH_ALLOCATION_GUARDS
    const bool heap = ctx.heap != nullptr && ctx.heap->contains(address);
    const bool valid = heap ? allocatedHeapRange(*ctx.heap, address, count) :
                              ctx.data != nullptr && address <= ctx.data->size() && count <= ctx.data->size() - address;
    if (!valid)
    {
        if (ctx.memoryFault.empty()) ctx.memoryFault = CHALI_TEXT("panic：访问未分配内存（读）：地址 ") + Memory::toString(address) + CHALI_TEXT("，字节数 ") + Memory::toString(count);
        return false;
    }
#endif
    if (ctx.heap != nullptr && ctx.heap->contains(address))
    {
        return ctx.heap->readBytes(address, out, count);
    }
    return (ctx.data != nullptr) && ctx.data->readBytes(address, out, count);
}

/**
 * @brief 往堆或数据区按字节写一段（改字符串里的某个字节用它）。
 * @param ctx 环境。
 * @param address 地址。
 * @param in 输入缓冲。
 * @param count 字节数。
 * @return 成功返回 true。
 */
bool nativeWrite(NativeContext &ctx, std::size_t address, const char *in, std::size_t count)
{
    if (count == 0) return true;
#if STACKVM_WITH_ALLOCATION_GUARDS
    const bool heap = ctx.heap != nullptr && ctx.heap->contains(address);
    const bool valid = heap ? allocatedHeapRange(*ctx.heap, address, count) :
                              ctx.data != nullptr && address <= ctx.data->size() && count <= ctx.data->size() - address;
    if (!valid)
    {
        if (ctx.memoryFault.empty()) ctx.memoryFault = CHALI_TEXT("panic：访问未分配内存（写）：地址 ") + Memory::toString(address) + CHALI_TEXT("，字节数 ") + Memory::toString(count);
        return false;
    }
#endif
    if (ctx.heap != nullptr && ctx.heap->contains(address))
    {
        return ctx.heap->writeBytes(address, in, count);
    }
    return (ctx.data != nullptr) && const_cast<DataArea *>(ctx.data)->writeBytes(address, in, count);
}

/**
 * @brief 读取显式长度的堆字符串，或读取生成期带哨兵的数据区字面量。
 * @param ctx 环境。
 * @param address 地址（0 = 空指针）。
 * @param out 结果。
 * @return 成功返回 true；长度非法、地址不可读或数据区哨兵缺失返回 false。
 */
bool nativeReadCString(const NativeContext &ctx, std::size_t address, Memory::String *out)
{
    out->clear();
    if (address == 0u) return true;  ///< 空指针当空串
    if (ctx.heap != nullptr && ctx.heap->contains(address))
    {
        std::int64_t length = 0;
        std::size_t capacity = 0u;
        if (!nativeRead(ctx, address, reinterpret_cast<char *>(&length), sizeof(length)) || length < 0 || !ctx.heap->capacityAt(address, capacity) || static_cast<std::uint64_t>(length) > capacity - std::min<std::size_t>(capacity, 8u))
            return false;
        out->resize(static_cast<std::size_t>(length));
        return length == 0 || nativeRead(ctx, address + 8u, out->data(), out->size());
    }
    for (std::size_t index = 0; index < kStringScanLimit; ++index)
    {
        char ch = 0;
        if (!nativeRead(ctx, address + index, &ch, 1u)) return false;
        if (ch == '\0') return true;
        out->push_back(ch);
    }
    return false;
}

/**
 * @brief 分配新存储并清除旧地址的销毁标记。
 * @param ctx 运行环境。
 * @param bytes 字节数。
 * @return 新地址，失败返回零。
 */
std::size_t nativeAllocate(NativeContext &ctx, std::size_t bytes);

/**
 * @brief 造一个显式长度的堆字符串，返回长度字段的地址。
 * @param ctx 环境。
 * @param text UTF-8 字节内容。
 * @return 地址；0 = 失败（`ctx.error` 里是原因）。
 */
std::size_t nativeAllocString(NativeContext &ctx, const Memory::String &text)
{
    if (ctx.heap == nullptr) return 0u;
    const std::size_t bytes = text.size() + 8u;
    const std::size_t address = nativeAllocate(ctx, bytes);
    if (address == 0u)
    {
        ctx.error = "堆内存不足：拼接 / 格式化需要 " + Memory::toString(bytes) + " 字节";
        return 0u;
    }
    if (!ctx.heap->writeInt(address, static_cast<std::int64_t>(text.size())) || (!text.empty() && !ctx.heap->writeBytes(address + 8u, text.data(), text.size())))
    {
        ctx.error = "往堆上写字符串失败（地址 " + Memory::toString(address) + "）";
        return 0u;
    }
    return address;
}
}  // namespace runtimeDetail
}  // namespace stackvm
