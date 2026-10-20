#include "std_collection_storage.hpp"

namespace collection_glue
{

/**
 * @brief 将通用值快照转换为胶水独占的键，不改变输入值的所有权。
 * @param value 数值、文本、聚合或对象身份的快照。
 * @param keepOwnership 是否在键中保留拥有型句柄标记。
 * @return 用于 Equal 查找及哈希桶索引的键快照。
 */
ObjectKey valueKey(const stackvm::NativeStoredValue &value, bool keepOwnership)
{
    ObjectKey out;
    out.isStruct = value.isStruct;
    out.isObject = value.objectIdentity != 0;
    out.isText = value.isText;
    out.isFloat = value.isFloat;
    out.ownsObject = keepOwnership && value.ownsObject;
    out.integer = value.objectIdentity != 0 ? value.objectIdentity : value.integer;
    out.ownedInteger = value.integer;
    out.real = value.real;
    out.text = value.text;
    for (const auto &field : value.fields) out.fields.push_back(valueKey(field, keepOwnership));
    return out;
}

/**
 * @brief 为内建键生成与数值等价关系兼容的哈希，碰撞仍需精确比较。
 * @param key 不含用户回调的键快照。
 * @return 哈希位模式。
 */
std::uint64_t intrinsicHash(const ObjectKey &key)
{
    std::uint64_t hash = 1469598103934665603ULL;
    if (key.isStruct)
    {
        for (const auto &field : key.fields) hash = (hash ^ intrinsicHash(field)) * 1099511628211ULL;
        return hash;
    }
    if (key.isText)
    {
        for (unsigned char byte : key.text) hash = (hash ^ byte) * 1099511628211ULL;
        return hash;
    }
    return static_cast<std::uint64_t>(std::hash<double>{}(key.isFloat ? key.real : static_cast<double>(key.integer)));
}

}  // namespace collection_glue

namespace collection_glue
{
/**
 * @brief 判断左值是否小于右值。
 *
 * 类型不同时按"数值 < 字符串"排（保证严格弱序）；同类型按值比。
 * @param other 参与当前运算或比较的另一对象。
 * @retval 左值小于右值时为 true，否则为 false。
 */
bool ObjectKey::operator<(const ObjectKey &other) const
{
    if (hashed != other.hashed) return !hashed;
    if (hashed && hash != other.hash) return hash < other.hash;
    if (isStruct != other.isStruct) return !isStruct;
    if (isStruct) return fields < other.fields;
    if (isText != other.isText) return !isText;
    if (isText) return text < other.text;
    if (isFloat != other.isFloat)
    {
        const double left = isFloat ? real : static_cast<double>(integer);
        const double right = other.isFloat ? other.real : static_cast<double>(other.integer);
        return left < right;
    }
    return isFloat ? (real < other.real) : (integer < other.integer);
}
}  // namespace collection_glue

namespace collection_glue
{
/**
 * @brief 取得指定哈希值的候选位置。
 * @param hash 已计算的用户或内建哈希。
 * @return 候选位置数组；没有该桶时返回空数组。
 */
const Memory::Vector<std::size_t> &Entries::bucket(std::int64_t hash) const
{
    const auto found = buckets_.find(hash);
    static const Memory::Vector<std::size_t> empty;
    return found == buckets_.end() ? empty : found->second;
}

/**
 * @brief 使用精确内建比较查找键，哈希模式只检查对应桶。
 * @param key 已携带存储策略及哈希的查询键。
 * @return 匹配项，未找到返回 end。
 */
Entries::iterator Entries::find(const Key &key)
{
    if (key.hashed)
    {
        for (auto index : bucket(key.hash))
            if (!(key < values_[index].first) && !(values_[index].first < key)) return begin() + index;
        return end();
    }
    return std::find_if(begin(), end(), [&](const Pair &entry) { return !(key < entry.first) && !(entry.first < key); });
}

/**
 * @brief 追加已完成去重和所有权处理的键值。
 * @param key 独立拥有的键。
 * @param value 独立拥有的值。
 * @return 新项迭代器与成功标志。
 */
std::pair<Entries::iterator, bool> Entries::emplace(Key key, Value value)
{
    const auto index = values_.size();
    if (key.hashed) buckets_[key.hash].push_back(index);
    values_.emplace_back(std::move(key), std::move(value));
    return { begin() + index, true };
}

/**
 * @brief 为宿主标量接口查找或新建内建哈希键。
 * @param key 原生标量键快照。
 * @return 对应的可写值槽。
 */
Entries::Value &Entries::operator[](Key key)
{
    key.hashed = true;
    key.hash = static_cast<std::int64_t>(intrinsicHash(key));
    auto found = find(key);
    if (found == end()) found = emplace(std::move(key), {}).first;
    return found->second;
}

/**
 * @brief 删除项并用末项补洞，同步维护两个受影响的哈希桶。
 * @param position 有效项迭代器，其资源已由调用者清理。
 */
void Entries::erase(iterator position)
{
    const auto index = static_cast<std::size_t>(position - begin());
    const auto last = values_.size() - 1;
    const bool hashed = position->first.hashed;
    const auto hash = position->first.hash;
    if (hashed)
    {
        auto &indices = buckets_[hash];
        indices.erase(std::find(indices.begin(), indices.end(), index));
        if (indices.empty()) buckets_.erase(hash);
    }
    if (index != last)
    {
        values_[index] = std::move(values_[last]);
        if (values_[index].first.hashed)
        {
            auto &indices = buckets_[values_[index].first.hash];
            *std::find(indices.begin(), indices.end(), last) = index;
        }
    }
    values_.pop_back();
}
}  // namespace collection_glue
