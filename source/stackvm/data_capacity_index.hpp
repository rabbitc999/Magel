#pragma once
#include <algorithm>

namespace stackvm
{
namespace execution_detail
{
/**
 * @brief 按地址索引静态数据区；重叠布局仍按原符号顺序查询。
 */
class DataCapacityIndex
{
    /**
     * @brief 非空静态符号的起始地址和字节数。
     */
    struct Range
    {
        std::size_t begin, bytes;
    };
    const DataArea *data_;
    Memory::Vector<Range> ranges_;
    bool overlaps_ = false;

public:
    /**
     * @brief 为本次运行建立静态数据区间索引，不保存可变堆地址。
     * @param data 数据区，可为空。
     */
    explicit DataCapacityIndex(const DataArea *data) : data_(data)
    {
        if (!data_) return;
        ranges_.reserve(data_->symbols.size());
        for (const auto &item : data_->symbols)
        {
            const auto size = data_->sizes.find(item.first);
            if (size != data_->sizes.end() && size->second) ranges_.push_back({ item.second, size->second });
        }
        std::sort(ranges_.begin(), ranges_.end(), [](const Range &a, const Range &b) { return a.begin < b.begin; });
        for (std::size_t i = 1; i < ranges_.size(); ++i)
            if (ranges_[i].begin - ranges_[i - 1].begin < ranges_[i - 1].bytes) overlaps_ = true;
    }
    /**
     * @brief 查询地址至所属符号末尾的容量，以减法避免地址加法溢出。
     * @param address 逻辑地址。
     * @param capacity 成功时写入剩余字节数。
     * @return 地址属于非空静态符号时返回真。
     */
    bool capacityAt(std::size_t address, std::size_t &capacity) const
    {
        if (overlaps_)
        {
            for (const auto &item : data_->symbols)
            {
                const auto size = data_->sizes.find(item.first);
                if (size != data_->sizes.end() && address >= item.second && address - item.second < size->second)
                {
                    capacity = size->second - (address - item.second);
                    return true;
                }
            }
            return false;
        }
        auto upper = std::upper_bound(ranges_.begin(), ranges_.end(), address, [](std::size_t value, const Range &range) { return value < range.begin; });
        if (upper == ranges_.begin()) return false;
        --upper;
        if (address - upper->begin >= upper->bytes) return false;
        capacity = upper->bytes - (address - upper->begin);
        return true;
    }
};
}  // namespace execution_detail
}  // namespace stackvm
