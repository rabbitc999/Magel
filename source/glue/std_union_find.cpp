#include "std_union_find.hpp"
#include "std_native_record.hpp"
#include "../chali_glue.hpp"
#include <numeric>
#include <memory>
#include <stdexcept>
#include <vector>

namespace std_union_find_glue
{
namespace
{
using namespace native_record_glue;
/**
 * @brief 拥有父指针、根集合大小和分量数的独立并查集状态。
 */
struct UnionState
{
    std::vector<std::int64_t> parents;
    std::vector<std::int64_t> sizes;
    std::int64_t components = 0;
};
/**
 * @brief 复制完整并查集状态，避免副本共享后续压缩或合并结果。
 * @param source 原并查集状态。
 * @param error 本操作不产生可恢复诊断。
 * @return 具有独立数组的新状态。
 */
void *cloneState(const void *source, Memory::String &error)
{
    (void)error;
    return new UnionState(*static_cast<const UnionState *>(source));
}
/**
 * @brief 释放并查集及其拥有的数组。
 * @param value 待释放的并查集状态。
 */
void destroyState(void *value)
{
    delete static_cast<UnionState *>(value);
}
const stackvm::NativeRecordType unionType{ "std::graph::UnionFind", cloneState, destroyState, nullptr };
/**
 * @brief 创建独立并查集，并把释放责任交给当前 VM。
 * @param state 当前胶水调用状态。
 * @param arguments 元素数，省略时建立空并查集。
 * @return 拥有新状态的原生记录句柄。
 */
Value create(State &state, const Memory::Vector<Value> &arguments)
{
    const auto count = arguments.empty() ? 0 : arguments[0].asInt();
    if (count < 0 || count > 16777216) throw std::out_of_range("并查集容量必须在 0..16777216 之间");
    auto value = std::make_unique<UnionState>();
    value->parents.resize(static_cast<std::size_t>(count));
    value->sizes.assign(static_cast<std::size_t>(count), 1);
    std::iota(value->parents.begin(), value->parents.end(), 0);
    value->components = count;
    const auto handle = state.host.makeNativeRecord(unionType, value.get());
    if (handle) value.release();
    return Value::fromInt(handle);
}
/**
 * @brief 查找根并原地压缩沿途父指针，不扫描无关元素。
 * @param value 当前并查集状态。
 * @param index 已校验范围的元素下标。
 * @return 元素所属集合的根下标。
 */
std::int64_t findRoot(UnionState &value, std::int64_t index)
{
    auto root = index;
    while (value.parents[static_cast<std::size_t>(root)] != root)
        root = value.parents[static_cast<std::size_t>(root)];
    while (index != root)
    {
        auto &parent = value.parents[static_cast<std::size_t>(index)];
        const auto next = parent;
        parent = root;
        index = next;
    }
    return root;
}
/**
 * @brief 执行原地查询或按集合大小合并，查询不复制父指针数组。
 * @param state 当前胶水调用状态。
 * @param arguments 并查集句柄、操作编号和两个元素下标。
 * @return 根、布尔结果、集合大小、分量数或总容量。
 */
Value operate(State &state, const Memory::Vector<Value> &arguments)
{
    auto *value = record<UnionState>(state, arguments[0], unionType, "并查集");
    if (!value) return Value::fromInt(0);
    const auto operation = arguments[1].asInt();
    if (operation == 4) return Value::fromInt(value->components);
    if (operation == 5) return Value::fromInt(static_cast<std::int64_t>(value->parents.size()));
    const auto left = arguments[2].asInt(), right = arguments[3].asInt();
    if (left < 0 || static_cast<std::uint64_t>(left) >= value->parents.size()
        || ((operation == 1 || operation == 2) && (right < 0 || static_cast<std::uint64_t>(right) >= value->parents.size())))
        throw std::out_of_range("并查集元素下标越界");
    auto root = findRoot(*value, left);
    if (operation == 0) return Value::fromInt(root);
    if (operation == 3) return Value::fromInt(value->sizes[static_cast<std::size_t>(root)]);
    if (operation != 1 && operation != 2) throw std::invalid_argument("并查集操作编号无效");
    auto other = findRoot(*value, right);
    if (operation == 2) return Value::fromInt(root == other);
    if (root == other) return Value::fromInt(0);
    if (value->sizes[static_cast<std::size_t>(root)] < value->sizes[static_cast<std::size_t>(other)]) std::swap(root, other);
    value->parents[static_cast<std::size_t>(other)] = root;
    value->sizes[static_cast<std::size_t>(root)] += value->sizes[static_cast<std::size_t>(other)];
    --value->components;
    return Value::fromInt(1);
}
/**
 * @brief 显式导出父指针或集合大小的独立列表快照。
 * @param state 当前胶水调用状态。
 * @param arguments 并查集句柄和是否导出大小数组的标志。
 * @return 由调用方拥有、修改不影响并查集的列表。
 */
Value snapshot(State &state, const Memory::Vector<Value> &arguments)
{
    const auto *value = record<UnionState>(state, arguments[0], unionType, "并查集快照");
    if (!value) return Value::fromInt(0);
    const auto &source = arguments[1].asInt() ? value->sizes : value->parents;
    Memory::Vector<std::int64_t> result(source.begin(), source.end());
    return chali_glue::Codec<Memory::Vector<std::int64_t>>::encode(state.host, result);
}
}  // namespace
/**
 * @brief 注册并查集的默认构造、深复制、释放和原地操作。
 * @param registry 当前宿主的原生函数注册表。
 * @param error 接收注册失败的诊断。
 * @return 所有入口注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    auto &native = registry.native();
    return native_record_glue::bind(native, "std::graph::__std_unionNew", 0, create, error)
           && native_record_glue::bind(native, "std::graph::__std_unionCreate", 1, create, error)
           && native_record_glue::bind(native, "std::graph::__std_unionCopy", 1, copy, error)
           && native_record_glue::bind(native, "std::graph::__std_unionDrop", 1, drop, error)
           && native_record_glue::bind(native, "std::graph::__std_unionOp", 4, operate, error)
           && native_record_glue::bind(native, "std::graph::__std_unionSnapshot", 2, snapshot, error);
}
}  // namespace std_union_find_glue
