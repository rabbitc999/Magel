#pragma once
#include <limits>

/**
 * @file allocation_index.hpp
 * @brief 稳定物理槽的分配身份与活跃区间索引。
 */
namespace stackvm
{
namespace execution_detail
{
/**
 * @brief 活跃分配的逻辑区间与物理偏移。
 */
struct Allocation
{ std::size_t address = 0, offset = 0, size = 0; };
/**
 * @brief 每个曾使用的物理块首一个槽，逻辑地址编码稳定槽号、分配代次与块内偏移。
 * 槽号采用自描述位宽，偏移位宽也包含在引用中；合并后的块可正常复用。
 */
class AllocationIndex
{
    static constexpr unsigned addressBits = sizeof(std::size_t) >= 8 ? 62 : 32;
    static constexpr unsigned headerBits = sizeof(std::size_t) >= 8 ? 6 : 5;
    static constexpr unsigned classBits = sizeof(std::size_t) >= 8 ? 5 : 4;
    static constexpr unsigned minimumDisplacementBits = sizeof(std::size_t) >= 8 ? 8 : 3;
    static constexpr unsigned prefixBits = addressBits - headerBits;
    /**
     * @brief 物理槽的最新代次、清扫边界和当前偏移位宽。
     */
    struct Slot
    {
        std::size_t offset = 0, generation = 0, swept = 0;
        unsigned displacementBits = minimumDisplacementBits;
    };
    /**
     * @brief 计算槽号的编码位宽。
     * @param value 非零槽号。
     * @return 位数。
     */
    static unsigned width(std::size_t value)
    {
        unsigned result = 0;
        for (; value; value >>= 1) ++result;
        return result;
    }
    /**
     * @brief 选择足以容纳请求的偏移位宽。
     * @param bytes 请求长度。
     * @return 位数。
     */
    static unsigned displacement(std::size_t bytes)
    {
        unsigned result = minimumDisplacementBits;
        while (bytes >= (std::size_t(1) << result)) ++result;
        return result;
    }
    /**
     * @brief 查询当前槽号和块大小可表示的代次上限。
     * @param id 槽下标。
     * @param bits 偏移位宽。
     * @return 上限，空间不足返回零。
     */
    std::size_t limit(std::size_t id, unsigned bits) const
    {
        const auto slotBits = width(id + 1);
        if (slotBits + classBits + bits >= prefixBits) return 0;
        const auto encoded = (std::size_t(1) << (prefixBits - slotBits - classBits - bits)) - 1;
        return encoded < generationLimit_ ? encoded : generationLimit_;
    }
    /**
     * @brief 编码槽中的身份。
     * @param id 槽下标。
     * @return 逻辑块首地址。
     */
    std::size_t encode(std::size_t id) const
    {
        const auto &slot = slots_[id];
        const auto slotBits = width(id + 1);
        return (std::size_t(slotBits) << prefixBits) | ((id + 1) << (prefixBits - slotBits)) | (std::size_t(slot.displacementBits - minimumDisplacementBits) << (prefixBits - slotBits - classBits)) | (slot.generation << slot.displacementBits);
    }
    /**
     * @brief 从引用自身解码身份，不读取槽中的最新位宽。
     * @param address 地址。
     * @param id 输出槽下标。
     * @param generation 输出代次。
     * @param bits 输出偏移位宽。
     * @return 编码有效为真。
     */
    bool decode(std::size_t address, std::size_t &id, std::size_t &generation, unsigned &bits) const
    {
        const auto slotBits = static_cast<unsigned>(address >> prefixBits);
        if (!slotBits || slotBits + classBits + minimumDisplacementBits >= prefixBits) return false;
        const auto number = (address >> (prefixBits - slotBits)) & ((std::size_t(1) << slotBits) - 1);
        if (!number || number > slots_.size() || width(number) != slotBits) return false;
        id = number - 1;
        const auto payloadBits = prefixBits - slotBits - classBits;
        bits = minimumDisplacementBits + static_cast<unsigned>((address >> payloadBits) & ((std::size_t(1) << classBits) - 1));
        if (bits >= payloadBits) return false;
        generation = (address >> bits) & ((std::size_t(1) << (payloadBits - bits)) - 1);
        return generation && generation <= generationLimit_;
    }

public:
    /**
     * @brief 创建身份索引。
     * @param generationLimit 代次上限，默认用完整编码空间；较小值用于边界验证。
     */
    explicit AllocationIndex(std::size_t generationLimit = std::numeric_limits<std::size_t>::max()) : generationLimit_(generationLimit) {}
    /**
     * @brief 重置整个运行的槽与活跃分配。
     * @param base 数据区之后的堆基址。
     * @return 无。
     */
    void reset(std::size_t base)
    {
        base_ = base;
        active_.clear();
        slots_.clear();
        byOffset_.clear();
        cached_ = {};
        live_ = peak_ = 0;
    }
    /**
     * @brief 检查请求是否可编码。
     * @param bytes 请求长度。
     * @return 可编码时为真。
     */
    bool available(std::size_t bytes) const { return bytes && bytes < (std::size_t(1) << (minimumDisplacementBits + (std::size_t(1) << classBits) - 1)) && base_ < (std::size_t(1) << prefixBits); }
    /**
     * @brief 检查物理区间和代次是否可用。
     * @param offset 物理块首。
     * @param bytes 请求长度。
     * @return 可以分配时为真。
     */
    bool canAdd(std::size_t offset, std::size_t bytes) const
    {
        if (!available(bytes) || bytes > std::numeric_limits<std::size_t>::max() - offset) return false;
        const auto found = byOffset_.find(offset);
        if (found == byOffset_.end()) return limit(slots_.size(), displacement(bytes)) != 0;
        const auto &slot = slots_[found->second];
        return slot.generation < limit(found->second, displacement(bytes)) && !root(encode(found->second));
    }
    /**
     * @brief 在空闲物理区间中寻找可编码块首，优先复用已有槽。
     * @param begin 区间起点。
     * @param capacity 区间长度。
     * @param bytes 请求长度。
     * @param offset 输出块首。
     * @return 找到位置时为真。
     */
    bool placement(std::size_t begin, std::size_t capacity, std::size_t bytes, std::size_t &offset) const
    {
        if (!available(bytes) || capacity < bytes) return false;
        if (canAdd(begin, bytes))
        {
            offset = begin;
            return true;
        }
        const auto last = begin + (capacity - bytes);
        for (auto it = byOffset_.lower_bound(begin); it != byOffset_.end() && it->first <= last; ++it)
            if (canAdd(it->first, bytes))
            {
                offset = it->first;
                return true;
            }
        auto candidate = begin;
        for (auto it = byOffset_.lower_bound(begin); it != byOffset_.end() && it->first == candidate; ++it)
        {
            if (candidate == last) return false;
            ++candidate;
        }
        if (candidate <= last && canAdd(candidate, bytes))
        {
            offset = candidate;
            return true;
        }
        return false;
    }
    /**
     * @brief 为物理块建立新身份，不回绕代次。
     * @param offset 物理块首。
     * @param bytes 请求长度。
     * @return 地址，编码空间耗尽返回零。
     */
    std::size_t add(std::size_t offset, std::size_t bytes)
    {
        if (!canAdd(offset, bytes)) return 0;
        auto found = byOffset_.find(offset);
        std::size_t id;
        if (found == byOffset_.end())
        {
            id = slots_.size();
            slots_.push_back(Slot{ offset, 0, 0, displacement(bytes) });
            byOffset_.emplace(offset, id);
        }
        else
            id = found->second;
        auto &slot = slots_[id];
        slot.displacementBits = displacement(bytes);
        ++slot.generation;
        const auto address = encode(id);
        active_.emplace(address, Allocation{ address, offset, bytes });
        live_ += bytes;
        if (live_ > peak_) peak_ = live_;
        cached_ = {};
        return address;
    }
    /**
     * @brief 查找包含地址的活跃区间。
     * @param address 地址。
     * @return 活跃区间或空指针。
     */
    const Allocation *find(std::size_t address) const
    {
        if (cached_.size && address >= cached_.address && address - cached_.address < cached_.size) return &cached_;
        std::size_t id = 0, generation = 0;
        unsigned bits = 0;
        if (!decode(address, id, generation, bits)) return nullptr;
        const auto rootAddress = address & ~((std::size_t(1) << bits) - 1);
        auto found = active_.find(rootAddress);
        if (found == active_.end() || address - rootAddress >= found->second.size) return nullptr;
        cached_ = found->second;
        return &cached_;
    }
    /**
     * @brief 查询精确块首。
     * @param address 地址。
     * @return 活跃块或空指针。
     */
    const Allocation *root(std::size_t address) const
    {
        auto found = active_.find(address);
        return found == active_.end() ? nullptr : &found->second;
    }
    /**
     * @brief 幂等释放精确身份。
     * @param address 块首地址。
     * @return 活跃或未清扫的已释放身份为真。
     */
    bool release(std::size_t address)
    {
        if (!address || retired(address)) return true;
        auto found = active_.find(address);
        if (found == active_.end()) return false;
        live_ -= found->second.size;
        active_.erase(found);
        cached_ = {};
        return true;
    }
    /**
     * @brief 判断身份是否仍享有幂等释放保证。
     * @param address 精确块首。
     * @return 未清扫的已释放身份为真。
     */
    bool retired(std::size_t address) const
    {
        std::size_t id = 0, generation = 0;
        unsigned bits = 0;
        if (!decode(address, id, generation, bits) || (address & ((std::size_t(1) << bits) - 1))) return false;
        const auto &slot = slots_[id];
        return generation > slot.swept && generation <= slot.generation && !root(address);
    }
    /**
     * @brief 撤销所有已释放身份的幂等保证，保留活跃身份。
     * @return 本次撤销的身份数量。
     */
    std::size_t sweep()
    {
        std::size_t count = 0;
        for (std::size_t id = 0; id < slots_.size(); ++id)
        {
            auto &slot = slots_[id];
            const auto boundary = slot.generation - (root(encode(id)) ? 1 : 0);
            count += boundary - slot.swept;
            slot.swept = boundary;
        }
        return count;
    }
    /**
     * @brief 判断是否属于编码堆空间，包括已清扫身份。
     * @param address 地址。
     * @return 属于堆空间为真。
     */
    bool contains(std::size_t address) const
    {
        std::size_t id = 0, generation = 0;
        unsigned bits = 0;
        return address >= base_ && decode(address, id, generation, bits);
    }
    /**
     * @brief 查询占用的物理身份槽数。
     * @return 槽数。
     */
    std::size_t slotCount() const { return slots_.size(); }
    /**
     * @brief 查询仍享有幂等保证的已释放逻辑身份数，不等同于存储槽数。
     * @return 身份数。
     */
    std::size_t retiredCount() const
    {
        std::size_t count = 0;
        for (const auto &slot : slots_) count += slot.generation - slot.swept;
        return count - active_.size();
    }
    /**
     * @brief 查询活跃字节数。
     * @return 字节数。
     */
    std::size_t liveBytes() const { return live_; }
    /**
     * @brief 查询活跃块数。
     * @return 块数。
     */
    std::size_t liveBlocks() const { return active_.size(); }
    /**
     * @brief 查询活跃字节峰值。
     * @return 字节数。
     */
    std::size_t peakBytes() const { return peak_; }

private:
    Memory::Map<std::size_t, Allocation> active_;
    Memory::Vector<Slot> slots_;
    Memory::Map<std::size_t, std::size_t> byOffset_;
    mutable Allocation cached_{};
    std::size_t base_ = 8, live_ = 0, peak_ = 0, generationLimit_ = std::numeric_limits<std::size_t>::max();
};
}  // namespace execution_detail
}  // namespace stackvm
