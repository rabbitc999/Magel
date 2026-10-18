#pragma once
#include "embed_pool.h"
#include "stackvm.hpp"
#include "allocation_index.hpp"
#include <cstring>
#include <cstdint>
namespace embed_stackvm
{
/**
 * @brief 使用调用方静态字节池的无异常堆；逻辑身份和区间索引与工具链 VM 共用。
 * 索引元数据使用板端 Memory 分配器，字节载荷保存在传入的固定容量池中。
 */
class EmbedPoolAllocatorView : public stackvm::HeapAllocator
{
public:
    /**
     * @brief 绑定字节池。
     * @param buffer 外部缓冲区。
     * @param bytes 容量。
     * @return 绑定是否成功。
     */
    bool bind(void *buffer, std::uint32_t bytes)
    {
        if (!buffer || !bytes || EmbedPool_Create(&pool_, buffer, bytes) != EMBED_POOL_ERR_OK) return false;
        buffer_ = static_cast<std::uint8_t *>(buffer);
        capacity_ = bytes;
        reset(1);
        return true;
    }
    /**
     * @brief 查询绑定状态。
     * @return 已绑定时为真。
     */
    bool bound() const { return buffer_ != nullptr; }
    /**
     * @brief 清空池和分配身份。
     * @param base 逻辑基址。
     * @return 无。
     */
    void reset(std::size_t base) override
    {
        base_ = base ? base : 1;
        EmbedPool_Reset(&pool_);
        index_.reset(base_);
    }
    /**
     * @brief 查询逻辑基址。
     * @return 基址。
     */
    std::size_t base() const override { return base_; }
    /**
     * @brief 分配清零字节并生成不会复用的逻辑身份。
     * @param bytes 字节数。
     * @return 地址，失败为零。
     */
    std::size_t allocate(std::size_t bytes) override
    {
        if (!bound() || bytes > 0xffffffffu || !index_.available(bytes)) return 0;
        auto *block = static_cast<std::uint8_t *>(EmbedPool_Alloc(&pool_, static_cast<std::uint32_t>(bytes)));
        if (!block) return 0;
        std::memset(block, 0, bytes);
        const auto address = index_.add(static_cast<std::size_t>(block - buffer_), bytes);
        if (!address) EmbedPool_Free(&pool_, block);
        return address;
    }
    /**
     * @brief 幂等释放精确块首，旧身份不会释放新对象。
     * @param address 地址。
     * @param reason 错误输出。
     * @return 是否成功。
     */
    bool release(std::size_t address, Memory::String *reason = nullptr) override
    {
        if (!address || index_.retired(address)) return true;
        const auto *allocation = index_.root(address);
        if (!allocation) return fail(reason, "释放的不是有效的堆块首");
        if (EmbedPool_Free(&pool_, buffer_ + allocation->offset) != EMBED_POOL_ERR_OK) return fail(reason, "静态字节池释放失败");
        return index_.release(address);
    }
    /**
     * @brief 清扫已释放身份。
     * @return 清扫数量。
     */
    std::size_t sweep() override { return index_.sweep(); }
    /**
     * @brief 查询未清扫墓碑。
     * @param address 块首。
     * @return 是否可幂等释放。
     */
    bool retired(std::size_t address) const override { return index_.retired(address); }
    /**
     * @brief 判断地址所属逻辑空间。
     * @param address 地址。
     * @return 属于本次堆运行时为真。
     */
    bool contains(std::size_t address) const override { return index_.contains(address); }
    /**
     * @brief 查询活跃区间剩余长度。
     * @param address 地址。
     * @param capacity 剩余长度。
     * @return 地址是否有效。
     */
    bool capacityAt(std::size_t address, std::size_t &capacity) const override
    {
        const auto *allocation = index_.find(address);
        if (!allocation) return false;
        capacity = allocation->size - (address - allocation->address);
        return true;
    }
    /**
     * @brief 查询物理池占用字节。
     * @return 字节数。
     */
    std::size_t liveBytes() const override { return EmbedPool_LiveBytes(&pool_); }
    /**
     * @brief 查询活跃块数。
     * @return 块数。
     */
    std::size_t liveBlocks() const override { return EmbedPool_LiveBlocks(&pool_); }
    /**
     * @brief 查询物理池占用峰值。
     * @return 字节数。
     */
    std::size_t peakBytes() const override { return EmbedPool_PeakBytes(&pool_); }
    /**
     * @brief 读取一个活跃分配内的字节。
     * @param address 地址。
     * @param out 输出。
     * @param count 长度。
     * @return 是否成功。
     */
    bool readBytes(std::size_t address, char *out, std::size_t count) const override
    {
        if (!count) return true;
        const auto *allocation = index_.find(address);
        if (!out || !allocation || count > allocation->size - (address - allocation->address)) return false;
        std::memcpy(out, buffer_ + allocation->offset + (address - allocation->address), count);
        return true;
    }
    /**
     * @brief 写入一个活跃分配内的字节。
     * @param address 地址。
     * @param data 输入。
     * @param count 长度。
     * @return 是否成功。
     */
    bool writeBytes(std::size_t address, const char *data, std::size_t count) override
    {
        if (!count) return true;
        const auto *allocation = index_.find(address);
        if (!data || !allocation || count > allocation->size - (address - allocation->address)) return false;
        std::memcpy(buffer_ + allocation->offset + (address - allocation->address), data, count);
        return true;
    }
    /**
     * @brief 查询固定容量。
     * @return 字节数。
     */
    std::size_t capacity() const { return capacity_; }
    /**
     * @brief 查询物理块表项数。
     * @return 表项数。
     */
    std::uint32_t blockCount() const { return EmbedPool_BlockCount(&pool_); }
    /**
     * @brief 检查池结构。
     * @param message 错误输出。
     * @return C 池错误码。
     */
    int8_t validate(const char **message = nullptr) const { return EmbedPool_Validate(&pool_, message); }

private:
    static bool fail(Memory::String *reason, const char *message)
    {
        if (reason) *reason = message;
        return false;
    }
    EmbedPool_T pool_{};
    std::uint8_t *buffer_ = nullptr;
    std::size_t capacity_ = 0, base_ = 1;
    stackvm::execution_detail::AllocationIndex index_;
};
/**
 * @brief 自带静态字节缓冲区的固定容量堆。
 * @tparam Capacity 字节容量。
 */
template <std::size_t Capacity>
class EmbedPoolAllocator final : public EmbedPoolAllocatorView
{
    static_assert(Capacity > 0 && Capacity <= 0xffffffffu, "静态池容量无效");

public:
    /**
     * @brief 创建并绑定自有存储。
     */
    EmbedPoolAllocator() { bind(storage_, static_cast<std::uint32_t>(Capacity)); }

private:
    std::uint8_t storage_[Capacity]{};
};
}  // namespace embed_stackvm
