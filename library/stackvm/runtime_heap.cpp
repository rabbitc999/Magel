/**
 * @file runtime_heap.cpp
 * @brief 默认字节堆的分配、释放及存活边界。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
#if STACKVM_WITH_DEFAULT_HEAP

/**
 * @brief 重置整个运行的槽与活跃分配。
 * @param base 数据区之后的堆基址。
 */
void Heap::reset(std::size_t base)
{
    base_ = base < kRuntimeCellBytes ? kRuntimeCellBytes : base;
    bytes_.clear();
    blocks_.clear();
    peak_ = 0u;
    index_.reset(base_);
}

/**
 * @brief 取得底层内存区域的起始地址。
 *
 * 堆区起始地址。
 * @retval 对应运行时内存区域的地址。
 */
[[nodiscard]] std::size_t Heap::base() const
{ return base_; }

/**
 * @brief 分配一块字节寻址的内存。
 * @param bytes 字节数。
 * @return 地址；参数为 0 或没内存了返回 0。
 */
std::size_t Heap::allocate(std::size_t bytes)
{
    if (bytes == 0u) return 0u;
    std::lock_guard<std::mutex> guard(mutex_);
    if (!index_.available(bytes)) return 0u;

    /* 先在**已释放**的块里首次适配：够大就切下一段复用，剩下的留成新的空闲块。
     * 这就是"链表式"分配器：空闲块在块表里串着，重复申请 / 释放不会让高水位一直涨。 */
    for (std::size_t index = 0; index < blocks_.size(); ++index)
    {
        if (!blocks_[index].free || blocks_[index].size < bytes) continue;
        std::size_t offset = 0;
        if (!index_.placement(blocks_[index].offset, blocks_[index].size, bytes, offset)) continue;
        if (offset != blocks_[index].offset)
        {
            const Block original = blocks_[index];
            Block tail;
            tail.offset = offset;
            tail.size = original.size - (offset - original.offset);
            blocks_[index].size = offset - original.offset;
            try
            {
                blocks_.insert(blocks_.begin() + static_cast<std::ptrdiff_t>(index + 1), tail);
            } catch (const std::bad_alloc &)
            {
                blocks_[index] = original;
                return 0u;
            } catch (const std::length_error &)
            {
                blocks_[index] = original;
                return 0u;
            }
            ++index;
        }
        Block &block = blocks_[index];
        const std::size_t address = index_.add(offset, bytes);
        if (!address) return 0u;
        block.address = address;
        const std::size_t leftover = block.size - bytes;
        block.size = bytes;
        block.free = false;
        if (leftover > 0u)
        {
            Block rest;
            rest.offset = offset + bytes;
            rest.size = leftover;
            rest.free = true;
            try
            {
                blocks_.insert(
                    blocks_.begin() + static_cast<std::ptrdiff_t>(index + 1u), rest);
            } catch (const std::bad_alloc &)
            {
                index_.release(address);
                blocks_[index].size = bytes + leftover;
                blocks_[index].free = true;
                return 0u;
            } catch (const std::length_error &)
            {
                index_.release(address);
                blocks_[index].size = bytes + leftover;
                blocks_[index].free = true;
                return 0u;
            }
        }
        std::memset(bytes_.data() + offset, 0, bytes);
        refreshPeak();
        return address;
    }

    /* 没有可复用的空闲块：往高地址再长一块。 */
    if (bytes_.size() > std::numeric_limits<std::size_t>::max() - base_ || bytes > std::numeric_limits<std::size_t>::max() - base_ - bytes_.size() || bytes > bytes_.max_size() - bytes_.size()) return 0u;
    Block block;
    block.offset = bytes_.size();
    block.address = 0;
    block.size = bytes;
    block.free = false;
    try
    {
        bytes_.resize(bytes_.size() + bytes, 0u);
    } catch (const std::bad_alloc &) { return 0u; } catch (const std::length_error &)
    {
        return 0u;
    }
    try
    {
        blocks_.push_back(block);
    } catch (const std::bad_alloc &)
    {
        bytes_.resize(bytes_.size() - bytes);
        return 0u;
    } catch (const std::length_error &)
    {
        bytes_.resize(bytes_.size() - bytes);
        return 0u;
    }
    blocks_.back().address = block.address = index_.add(block.offset, bytes);
    if (!block.address)
    {
        blocks_.pop_back();
        bytes_.resize(block.offset);
        return 0u;
    }
    refreshPeak();
    return block.address;
}

/**
 * @brief 释放一块内存（地址 0 当空指针放过；相邻空闲块合并）。
 * @param address 地址。
 * @param reason 失败原因（可选）。
 * @return 成功（或空指针）返回 true。
 */
bool Heap::release(std::size_t address, Memory::String *reason)
{
    if (address == 0u) return true;  // free(NULL) 是空操作
    std::lock_guard<std::mutex> guard(mutex_);
    for (std::size_t index = 0; index < blocks_.size(); ++index)
    {
        Block &block = blocks_[index];
        if (block.free || block.address != address) continue;
        index_.release(address);
        block.free = true;
        /* 和左右相邻的空闲块合并：块表不会因为"反复申请 / 释放"越来越多。 */
        if (index + 1u < blocks_.size() && blocks_[index + 1u].free)
        {
            blocks_[index].size += blocks_[index + 1u].size;
            blocks_.erase(blocks_.begin() + static_cast<std::ptrdiff_t>(index + 1u));
        }
        if (index > 0u && blocks_[index - 1u].free)
        {
            blocks_[index - 1u].size += blocks_[index].size;
            blocks_.erase(blocks_.begin() + static_cast<std::ptrdiff_t>(index));
        }
        return true;
    }
    if (reason != nullptr)
    {
        *reason = "[MGL1205] 无效或已经释放的身份：地址 " + Memory::toString(address) + " 不在堆里（堆区从 " + Memory::toString(base_) + " 起）";
    }
    return false;
}

/**
 * @brief 判断是否属于编码堆空间，包括已清扫身份。
 * @param address 地址。
 * @return 属于堆空间为真。
 */
[[nodiscard]] bool Heap::contains(std::size_t address) const
{
    return index_.contains(address);
}

/**
 * @brief 地址所在的**已分配块**从这里起还能用多少字节。
 * @param address 地址。
 * @param capacity 输出：从 address 到块尾的字节数。
 * @return 地址不在任何已分配块里（没分配 / 已经 free）返回 false。
 */
bool Heap::capacityAt(std::size_t address, std::size_t &capacity) const
{
    const auto *block = index_.find(address);
    if (!block) return false;
    capacity = block->size - (address - block->address);
    return true;
}

/**
 * @brief 查询活跃字节数。
 * @return 字节数。
 */
[[nodiscard]] std::size_t Heap::liveBytes() const
{
    return index_.liveBytes();
}

/**
 * @brief 查询活跃块数。
 * @return 块数。
 */
[[nodiscard]] std::size_t Heap::liveBlocks() const
{ return index_.liveBlocks(); }

/**
 * @brief 查询活跃字节峰值。
 * @return 字节数。
 */
[[nodiscard]] std::size_t Heap::peakBytes() const
{ return peak_; }

/**
 * @brief 读 8 字节（只允许读**已分配**的块）。
 * @param address 地址。
 * @param out 结果。
 * @return 越界 / 已释放返回 false。
 */
bool Heap::readInt(std::size_t address, std::int64_t &out) const

{
    if (!usable(address)) return false;
    std::memcpy(&out, bytes_.data() + physicalOffset(address), sizeof(out));
    return true;
}

/**
 * @brief 写 8 字节。
 * @param address 地址。
 * @param value 值。
 * @return 越界 / 已释放返回 false。
 */
bool Heap::writeInt(std::size_t address, std::int64_t value)

{
    if (!usable(address)) return false;
    std::memcpy(bytes_.data() + physicalOffset(address), &value, sizeof(value));
    return true;
}

/**
 * @brief 读 8 字节，按 IEEE-754 双精度解释。
 * @param address 地址。
 * @param out 结果。
 * @return 越界 / 已释放返回 false。
 */
bool Heap::readDouble(std::size_t address, double &out) const

{
    if (!usable(address)) return false;
    std::memcpy(&out, bytes_.data() + physicalOffset(address), sizeof(out));
    return true;
}

/**
 * @brief 把双精度写成 8 字节。
 * @param address 地址。
 * @param value 值。
 * @return 越界 / 已释放返回 false。
 */
bool Heap::writeDouble(std::size_t address, double value)

{
    if (!usable(address)) return false;
    std::memcpy(bytes_.data() + physicalOffset(address), &value, sizeof(value));
    return true;
}

/**
 * @brief 按字节读一段（字符串跨 8 字节槽，不能按槽读）。
 * @param address 地址。
 * @param out 输出缓冲（至少 count 字节）。
 * @param count 字节数。
 * @return 这 count 个字节全在**同一块已分配内存**里返回 true。
 */
bool Heap::readBytes(std::size_t address, char *out, std::size_t count) const
{
    if (count == 0u) return true;
    if (!usableRange(address, count)) return false;
    std::memcpy(out, bytes_.data() + physicalOffset(address), count);
    return true;
}

/**
 * @brief 按字节写一段。
 * @param address 地址。
 * @param in 输入缓冲（至少 count 字节）。
 * @param count 字节数。
 * @return 这 count 个字节全在**同一块已分配内存**里返回 true。
 */
bool Heap::writeBytes(std::size_t address, const char *in, std::size_t count)
{
    if (count == 0u) return true;
    if (!usableRange(address, count)) return false;
    std::memcpy(bytes_.data() + physicalOffset(address), in, count);
    return true;
}

/**
 * @brief 将有效逻辑地址转换为物理偏移。
 * @param address 已验证的逻辑地址。
 * @return 字节区偏移。
 */
std::size_t Heap::physicalOffset(std::size_t address) const

{
    const auto *block = index_.find(address);
    return block ? block->offset + address - block->address : bytes_.size();
}

/**
 * @brief 整段范围必须落在同一活跃分配内。
 * @param address 地址。
 * @param count 长度。
 * @return 可访问时为 true。
 */
[[nodiscard]] bool Heap::usableRange(std::size_t address, std::size_t count) const

{
    const auto *block = index_.find(address);
    return block && count <= block->size - (address - block->address);
}

/**
 * @brief 地址是否可读写：落在某个**已分配**块里，且 8 个字节都在块内。
 * @param address 运行时内存地址。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
[[nodiscard]] bool Heap::usable(std::size_t address) const
{ return usableRange(address, 8u); }

/**
 * @brief 更新峰值。
 */
void Heap::refreshPeak()

{
    const std::size_t live = liveBytes();
    if (live > peak_) peak_ = live;
}
#else
/**
 * @brief 将内存分配器恢复到初始状态。
 */
void NoHeapAllocator::reset(std::size_t)
{
}

/**
 * @brief 取得底层内存区域的起始地址。
 * @retval 对应运行时内存区域的地址。
 */
[[nodiscard]] std::size_t NoHeapAllocator::base() const
{ return kRuntimeCellBytes; }

/**
 * @brief 分配指定字节数的运行时内存。
 * @retval 对应运行时内存区域的地址。
 */
[[nodiscard]] std::size_t NoHeapAllocator::allocate(std::size_t)
{ return 0u; }

/**
 * @brief 释放指定地址对应的运行时资源。
 * @param reason 接收释放失败原因或停止原因的对象。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NoHeapAllocator::release(std::size_t, Memory::String *reason)
{
    if (reason != nullptr) *reason = "这个构建没有默认字节堆（必须由宿主注入字节池）";
    return false;
}

/**
 * @brief 判断地址是否属于当前内存区域。
 * @retval 地址属于当前内存区域时为 true，否则为 false。
 */
[[nodiscard]] bool NoHeapAllocator::contains(std::size_t) const
{ return false; }

/**
 * @brief 查询指定地址所在分配块的容量。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
[[nodiscard]] bool NoHeapAllocator::capacityAt(std::size_t, std::size_t &) const
{ return false; }

/**
 * @brief 查询活跃字节数。
 * @return 字节数。
 */
[[nodiscard]] std::size_t NoHeapAllocator::liveBytes() const
{ return 0u; }

/**
 * @brief 查询活跃块数。
 * @return 块数。
 */
[[nodiscard]] std::size_t NoHeapAllocator::liveBlocks() const
{ return 0u; }

/**
 * @brief 查询活跃字节峰值。
 * @return 字节数。
 */
[[nodiscard]] std::size_t NoHeapAllocator::peakBytes() const
{ return 0u; }

/**
 * @brief 从指定地址读取一段字节数据。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
[[nodiscard]] bool NoHeapAllocator::readBytes(std::size_t, char *, std::size_t) const
{ return false; }

/**
 * @brief 将字节数据写入指定地址。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool NoHeapAllocator::writeBytes(std::size_t, const char *, std::size_t)
{ return false; }
#endif
}  // namespace runtimeDetail
}  // namespace stackvm
