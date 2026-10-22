#include "std_linkedlist.hpp"
#include "std_native_record.hpp"
#include "../chali_glue.hpp"
#include <algorithm>
#include <list>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace std_linkedlist_glue
{
namespace
{
using namespace native_record_glue;
/**
 * @brief 保存稳定游标和节点值的双向链表条目。
 */
struct Entry
{ std::int64_t id, value; };
/**
 * @brief 独立拥有双向节点与游标索引的链表；游标不重复使用。
 */
struct List
{
    std::list<Entry> entries;
    std::unordered_map<std::int64_t, std::list<Entry>::iterator> cursors;
    std::int64_t nextId = 0;
    /**
     * @brief 在指定位置插入并分配从未使用过的游标。
     * @param position 后继位置，end 表示尾插。
     * @param value 要插入的节点值。
     * @return 新节点游标。
     */
    std::int64_t insert(std::list<Entry>::iterator position, std::int64_t value)
    {
        if (nextId == INT64_MAX) throw std::length_error("链表游标已耗尽");
        auto id = nextId++;
        auto node = entries.insert(position, Entry{ id, value });
        try
        {
            cursors.emplace(id, node);
        } catch (...)
        {
            entries.erase(node);
            throw;
        }
        return id;
    }
};
/**
 * @brief 深复制节点并在副本中重建游标索引。
 * @param source 原链表。
 * @param error 本操作不产生可恢复诊断。
 * @return 拥有独立节点的副本。
 */
void *cloneList(const void *source, Memory::String &error)
{
    (void)error;
    const auto &original = *static_cast<const List *>(source);
    auto copy = std::make_unique<List>();
    copy->entries = original.entries;
    copy->nextId = original.nextId;
    for (auto it = copy->entries.begin(); it != copy->entries.end(); ++it) copy->cursors.emplace(it->id, it);
    return copy.release();
}
/**
 * @brief 释放链表节点及索引。
 * @param value 待释放的链表。
 */
void destroyList(void *value) { delete static_cast<List *>(value); }
const stackvm::NativeRecordType listType{ "std::linkedlist::LinkedList", cloneList, destroyList, nullptr };
/**
 * @brief 创建 VM 拥有的空链表。
 * @param state 当前调用状态。
 * @param arguments 无实参。
 * @return 原生记录句柄。
 */
Value create(State &state, const Memory::Vector<Value> &arguments)
{
    (void)arguments;
    auto value = std::make_unique<List>();
    auto handle = state.host.makeNativeRecord(listType, value.get());
    if (handle) value.release();
    return Value::fromInt(handle);
}
/**
 * @brief 原地修改链表或查询标量，不复制或导出节点。
 * @param state 当前调用状态。
 * @param arguments 链表句柄、操作编号、游标及值。
 * @return 操作对应的游标、值、数量或标志。
 */
Value operate(State &state, const Memory::Vector<Value> &arguments)
{
    auto *list = record<List>(state, arguments[0], listType, "链表");
    if (!list) return Value::fromInt(-1);
    auto cursor = arguments[2].asInt(), value = arguments[3].asInt();
    auto found = list->cursors.find(cursor);
    auto position = found == list->cursors.end() ? list->entries.end() : found->second;
    switch (arguments[1].asInt())
    {
    case 0: return Value::fromInt(list->entries.size());
    case 1: return Value::fromInt(list->insert(list->entries.begin(), value));
    case 2: return Value::fromInt(list->insert(list->entries.end(), value));
    case 3: return Value::fromInt(position == list->entries.end() ? -1 : list->insert(position, value));
    case 4: return Value::fromInt(position == list->entries.end() ? -1 : list->insert(std::next(position), value));
    case 5: return Value::fromInt(position != list->entries.end());
    case 6:
    case 7:
    {
        if (position == list->entries.end()) throw std::out_of_range("链表游标已失效");
        auto result = position->value;
        if (arguments[1].asInt() == 7)
        {
            list->entries.erase(position);
            list->cursors.erase(found);
        }
        return Value::fromInt(result);
    }
    case 8: return Value::fromInt(list->entries.empty() ? -1 : list->entries.front().id);
    case 9: return Value::fromInt(list->entries.empty() ? -1 : list->entries.back().id);
    case 10: list->entries.reverse(); return Value::fromInt(0);
    case 11:
        for (const auto &entry : list->entries)
            if (entry.value == value) return Value::fromInt(entry.id);
        return Value::fromInt(-1);
    case 12:
    {
        if (cursor < 0 || static_cast<std::uint64_t>(cursor) >= list->entries.size()) return Value::fromInt(-1);
        auto it = list->entries.begin();
        std::advance(it, cursor);
        return Value::fromInt(it->id);
    }
    case 13:
        list->entries.clear();
        list->cursors.clear();
        return Value::fromInt(0);
    default: throw std::invalid_argument("链表操作编号无效");
    }
}
/**
 * @brief 按链表顺序导出独立值快照。
 * @param state 当前调用状态。
 * @param arguments 链表句柄。
 * @return 调用方拥有的值列表。
 */
Value snapshot(State &state, const Memory::Vector<Value> &arguments)
{
    const auto *list = record<List>(state, arguments[0], listType, "链表快照");
    if (!list) return Value::fromInt(0);
    Memory::Vector<std::int64_t> values;
    for (const auto &entry : list->entries) values.push_back(entry.value);
    return chali_glue::Codec<Memory::Vector<std::int64_t>>::encode(state.host, values);
}
}  // namespace
/**
 * @brief 注册链表生命周期、原地操作和快照入口。
 * @param registry 当前宿主注册表。
 * @param error 接收绑定失败的诊断。
 * @return 所有入口注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    auto &native = registry.native();
    return native_record_glue::bind(native, "std::linkedlist::__listMake", 0, create, error)
           && native_record_glue::bind(native, "std::linkedlist::__listNew", 0, create, error)
           && native_record_glue::bind(native, "std::linkedlist::__listCopy", 1, copy, error)
           && native_record_glue::bind(native, "std::linkedlist::__listDrop", 1, drop, error)
           && native_record_glue::bind(native, "std::linkedlist::__listOp", 4, operate, error)
           && native_record_glue::bind(native, "std::linkedlist::__listSnapshot", 1, snapshot, error);
}
}  // namespace std_linkedlist_glue
