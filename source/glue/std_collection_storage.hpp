#ifndef CHALI_COLLECTION_STORAGE_HPP
#define CHALI_COLLECTION_STORAGE_HPP
#include "stackvm.hpp"
#include <algorithm>
#include <functional>

namespace collection_glue
{
/**
 * @brief 集合胶水独占的键比较快照：对象按跨副本身份比较，其余按值比较。
 */
struct ObjectKey
{
    bool isStruct = false;
    bool isObject = false;
    bool isText = false;
    bool isFloat = false;
    bool ownsObject = false;
    std::int64_t integer = 0;
    std::int64_t ownedInteger = 0;
    double real = 0.0;
    Memory::String text;
    Memory::Vector<ObjectKey> fields;
    bool hashed = false;    ///< 自定义 Map 键是否已缓存哈希。
    std::int64_t hash = 0;  ///< Hash<T> 返回值按位保存，用于同哈希范围查找。

    /**
     * @brief 判断左值是否小于右值。
     *
     * 类型不同时按"数值 < 字符串"排（保证严格弱序）；同类型按值比。
     * @param other 参与当前运算或比较的另一对象。
     * @retval 左值小于右值时为 true，否则为 false。
     */
    bool operator<(const ObjectKey &other) const;
};

/**
 * @brief 将通用值快照转换为胶水独占的键，不改变输入值的所有权。
 * @param value 数值、文本、聚合或对象身份的快照。
 * @param keepOwnership 是否在键中保留拥有型句柄标记。
 * @return 用于 Equal 查找及哈希桶索引的键快照。
 */
ObjectKey valueKey(const stackvm::NativeStoredValue &value, bool keepOwnership = false);

/**
 * @brief 为内建键生成与数值等价关系兼容的哈希，碰撞仍需精确比较。
 * @param key 不含用户回调的键快照。
 * @return 哈希位模式。
 */
std::uint64_t intrinsicHash(const ObjectKey &key);

/**
 * @brief 密集数组保存键值；启用 Hash 时另建哈希桶，否则按数组线性查找。
 * @note 删除用末项补洞；任何模式均不承诺遍历顺序。
 */
class Entries
{
public:
    using Key = ObjectKey;
    using Value = stackvm::NativeStoredValue;
    using Pair = std::pair<Key, Value>;
    using iterator = Memory::Vector<Pair>::iterator;
    using const_iterator = Memory::Vector<Pair>::const_iterator;

    /**
     * @brief 返回可写的数组起点。
     * @return 首项迭代器。
     */
    iterator begin() { return values_.begin(); }
    /**
     * @brief 返回只读的数组起点。
     * @return 首项只读迭代器。
     */
    const_iterator begin() const { return values_.begin(); }
    /**
     * @brief 返回可写的数组终点。
     * @return 尾后迭代器。
     */
    iterator end() { return values_.end(); }
    /**
     * @brief 返回只读的数组终点。
     * @return 尾后只读迭代器。
     */
    const_iterator end() const { return values_.end(); }
    /**
     * @brief 查询当前键值数量。
     * @return 数组中的有效项数。
     */
    std::size_t size() const { return values_.size(); }
    /**
     * @brief 查询是否没有键值项。
     * @return 空数组返回 true。
     */
    bool empty() const { return values_.empty(); }
    /**
     * @brief 取得指定哈希值的候选位置。
     * @param hash 已计算的用户或内建哈希。
     * @return 候选位置数组；没有该桶时返回空数组。
     */
    const Memory::Vector<std::size_t> &bucket(std::int64_t hash) const;
    /**
     * @brief 使用精确内建比较查找键，哈希模式只检查对应桶。
     * @param key 已携带存储策略及哈希的查询键。
     * @return 匹配项，未找到返回 end。
     */
    iterator find(const Key &key);
    /**
     * @brief 追加已完成去重和所有权处理的键值。
     * @param key 独立拥有的键。
     * @param value 独立拥有的值。
     * @return 新项迭代器与成功标志。
     */
    std::pair<iterator, bool> emplace(Key key, Value value);
    /**
     * @brief 为宿主标量接口查找或新建内建哈希键。
     * @param key 原生标量键快照。
     * @return 对应的可写值槽。
     */
    Value &operator[](Key key);
    /**
     * @brief 删除项并用末项补洞，同步维护两个受影响的哈希桶。
     * @param position 有效项迭代器，其资源已由调用者清理。
     */
    void erase(iterator position);
    /**
     * @brief 清空已由调用者释放资源的存储与哈希索引。
     */
    void clear()
    {
        values_.clear();
        buckets_.clear();
    }

private:
    Memory::Vector<Pair> values_;
    Memory::UnorderedMap<std::int64_t, Memory::Vector<std::size_t>> buckets_;
};
}  // namespace collection_glue
#endif
