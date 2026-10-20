#ifndef PARSE_ENGINE_STRUCTURED_DATA_HPP
#define PARSE_ENGINE_STRUCTURED_DATA_HPP
#include "../magel_glue.hpp"
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string_view>

/**
 * @brief JSON/TOML 的只读文档、节点句柄和公共胶水。
 */
namespace structured_data_glue
{
using Context = magel_glue::Context;
using String = Memory::String;
using Bytes = Memory::Vector<std::uint8_t>;

/**
 * @brief 注册表拥有文档；节点只保存稳定索引，不暴露宿主指针。
 */
template <class Traits>
struct Cache
{
    using Value = typename Traits::Value;
    /**
     * @brief 一次解析的根值、节点索引和诊断。
     */
    struct Document
    {
        std::optional<Value> root;
        Memory::Vector<const Value *> nodes;
        Memory::Map<const Value *, std::int64_t> indices;
        String error;
        /**
         * @brief 迭代建立节点索引，避免胶水层递归遍历。
         */
        void index()
        {
            if (!root) return;
            nodes.push_back(&*root);
            for (std::size_t i = 0; i < nodes.size(); ++i)
            {
                const Value *value = nodes[i];
                indices.emplace(value, static_cast<std::int64_t>(i));
                if (const auto *array = value->asArray())
                    for (const auto &child : *array) nodes.push_back(&child);
                if (const auto *object = value->asObject())
                    for (const auto &member : *object) nodes.push_back(&member.value);
            }
        }
    };
    std::mutex mutex;
    Memory::RuntimeAllocator *owner = Memory::currentRuntimeAllocator();
    std::int64_t next = 1;
    Memory::Map<std::int64_t, std::shared_ptr<Document>> documents;
};

/**
 * @brief 持锁访问节点，关闭后的句柄报告运行错误。
 * @param cache 文档表。
 * @param ctx 上下文。
 * @param handle 文档句柄。
 * @param node 节点索引。
 * @param function 访问操作。
 * @return 操作结果。
 */
template <class Result, class Traits, class Function>
Result access(const std::shared_ptr<Cache<Traits>> &cache, Context &ctx,
              std::int64_t handle, std::int64_t node, Function function)
{
    std::lock_guard<std::mutex> lock(cache->mutex);
    const auto found = cache->documents.find(handle);
    if (found == cache->documents.end() || node < 0 || static_cast<std::uint64_t>(node) >= found->second->nodes.size())
    {
        ctx.fail("结构化数据：文档已关闭或节点无效");
        return Result{};
    }
    auto &doc = *found->second;
    return function(doc, *doc.nodes[static_cast<std::size_t>(node)]);
}

/**
 * @brief 查找保序对象成员名。
 * @param value 对象节点。
 * @param index 成员序号。
 * @return 键名指针，不存在返回空。
 */
template <class Value>
const std::string *keyAt(const Value &value, std::int64_t index)
{
    const auto *object = value.asObject();
    if (!object || index < 0 || static_cast<std::uint64_t>(index) >= object->size()) return nullptr;
    return &(object->begin() + index)->key;
}

/**
 * @brief 注册一种结构化数据格式。
 * @param registry 胶水注册表。
 * @param prefix 模块限定名。
 * @param error 注册错误。
 * @return 是否成功。
 */
template <class Traits>
bool bind(magel_glue::Registry &registry, const String &prefix, String *error)
{
    using Data = Cache<Traits>;
    using Value = typename Traits::Value;
    using Document = typename Data::Document;
    const auto cache = Memory::makeShared<Data>();
    bool ok = registry.bindTyped<std::int64_t, String>(prefix + "__parse", [cache](String text) {
        // 文档可以跨 VM 调用存活，必须归属注册表的分配区域。
        Memory::AllocationScope ownership(cache->owner);
        auto parsed = Traits::parse(std::string_view(text.data(), text.size()));
        auto doc = Memory::makeShared<Document>();
        doc->root = std::move(parsed.value);
        if (!doc->root) {
            const auto message = parsed.diagnostics.empty() ? std::string("解析失败") : parsed.diagnostics.front().toString();
            doc->error.assign(message.data(), message.size());
        }
        doc->index();
        std::lock_guard<std::mutex> lock(cache->mutex);
        if (cache->next == std::numeric_limits<std::int64_t>::max()) throw std::overflow_error("文档句柄耗尽");
        const auto handle = cache->next++;
        cache->documents.emplace(handle, std::move(doc));
        return handle; }, error);
    ok = registry.bindTyped<bool, std::int64_t>(prefix + "__valid", [cache](std::int64_t handle) {
        std::lock_guard<std::mutex> lock(cache->mutex);
        const auto found = cache->documents.find(handle);
        return found != cache->documents.end() && found->second->root.has_value(); }, error) && ok;
    ok = registry.bindTyped<String, std::int64_t>(prefix + "__takeError", [cache](std::int64_t handle) {
        std::lock_guard<std::mutex> lock(cache->mutex);
        const auto found = cache->documents.find(handle);
        if (found == cache->documents.end()) return String("无效文档句柄");
        String message = found->second->error;
        if (!found->second->root) cache->documents.erase(found);
        return message; }, error) && ok;
    ok = registry.bindTyped<bool, std::int64_t>(prefix + "__close", [cache](std::int64_t handle) {
        std::lock_guard<std::mutex> lock(cache->mutex);
        return cache->documents.erase(handle) != 0; }, error) && ok;
    // 只读标量访问统一走 access，所有节点与关闭操作共用同一把锁。
    const auto bindRead = [&](const char *name, auto resultTag, auto function) {
        using Result = decltype(resultTag);
        return registry.bindTypedWithContext<Result, std::int64_t, std::int64_t>(prefix + name, [cache, function](Context &ctx, std::int64_t handle, std::int64_t node) { return access<Result>(cache, ctx, handle, node,
                                                                                                                                                                                                [&](Document &, const Value &value) { return function(value); }); }, error);
    };
    ok = bindRead("__kind", std::int64_t{}, [](const Value &value) { return Traits::kind(value); }) && ok;
    ok = bindRead("__size", std::int64_t{}, [](const Value &value) {
             if (auto *a = value.asArray()) return static_cast<std::int64_t>(a->size());
             if (auto *o = value.asObject()) return static_cast<std::int64_t>(o->size());
             return std::int64_t{ 0 };
         })
         && ok;
    ok = bindRead("__int", std::int64_t{}, [](const Value &v) { return v.isInt() ? v.asInt().value() : 0; }) && ok;
    ok = bindRead("__float", double{}, [](const Value &v) { return v.asDouble().value_or(0.0); }) && ok;
    ok = bindRead("__bool", false, [](const Value &v) { return v.asBool().value_or(false); }) && ok;
    ok = bindRead("__textAvailable", false, [](const Value &v) {
             auto text = v.asString();
             return text && text->find('\0') == std::string_view::npos;
         })
         && ok;
    ok = bindRead("__string", String{}, [](const Value &v) {
             auto text = v.asString();
             return text ? String(text->data(), text->size()) : String();
         })
         && ok;
    ok = bindRead("__bytes", Bytes{}, [](const Value &v) {
             auto text = v.asString();
             return text ? Bytes(text->begin(), text->end()) : Bytes();
         })
         && ok;
    ok = bindRead("__dateTime", String{}, [](const Value &v) {
             const auto text = Traits::dateTime(v);
             return String(text.data(), text.size());
         })
         && ok;
    ok = bindRead("__dump", String{}, [](const Value &v) {
             const auto text = Traits::dump(v);
             return String(text.data(), text.size());
         })
         && ok;
    const auto bindFind = [&](const char *name, auto keyTag) {
        using Key = decltype(keyTag);
        return registry.bindTypedWithContext<std::int64_t, std::int64_t, std::int64_t, Key>(prefix + name, [cache](Context &ctx, std::int64_t handle, std::int64_t node, Key key) { return access<std::int64_t>(cache, ctx, handle, node, [&](Document &doc, const Value &v) {
                                                                                                                                                                                        const auto *child = v.find(std::string_view(reinterpret_cast<const char *>(key.data()), key.size()));
                                                                                                                                                                                        return child ? doc.indices.at(child) : std::int64_t{ -1 };
                                                                                                                                                                                    }); }, error);
    };
    ok = bindFind("__find", String{}) && ok;
    ok = bindFind("__findBytes", Bytes{}) && ok;
    ok = registry.bindTypedWithContext<std::int64_t, std::int64_t, std::int64_t, std::int64_t>(prefix + "__at", [cache](Context &ctx, std::int64_t handle, std::int64_t node, std::int64_t index) { return access<std::int64_t>(cache, ctx, handle, node, [&](Document &doc, const Value &v) {
                                                                                                                                                                                                        const Value *child = nullptr;
                                                                                                                                                                                                        if (index >= 0)
                                                                                                                                                                                                        {
                                                                                                                                                                                                            if (auto *a = v.asArray(); a && static_cast<std::uint64_t>(index) < a->size()) child = &(*a)[static_cast<std::size_t>(index)];
                                                                                                                                                                                                            if (auto *o = v.asObject(); o && static_cast<std::uint64_t>(index) < o->size()) child = &(o->begin() + index)->value;
                                                                                                                                                                                                        }
                                                                                                                                                                                                        return child ? doc.indices.at(child) : std::int64_t{ -1 };
                                                                                                                                                                                                    }); }, error) && ok;
    ok = registry.bindTypedWithContext<bool, std::int64_t, std::int64_t, std::int64_t>(prefix + "__keyAvailable", [cache](Context &ctx, std::int64_t handle, std::int64_t node, std::int64_t index) { return access<bool>(cache, ctx, handle, node, [&](Document &, const Value &v) {
                                                                                                                                                                                                          auto key = keyAt(v, index);
                                                                                                                                                                                                          return key && key->find('\0') == std::string::npos;
                                                                                                                                                                                                      }); }, error) && ok;
    ok = registry.bindTypedWithContext<String, std::int64_t, std::int64_t, std::int64_t>(prefix + "__key", [cache](Context &ctx, std::int64_t handle, std::int64_t node, std::int64_t index) { return access<String>(cache, ctx, handle, node, [&](Document &, const Value &v) {
                                                                                                                                                                                                   auto key = keyAt(v, index);
                                                                                                                                                                                                   return key ? String(key->data(), key->size()) : String();
                                                                                                                                                                                               }); }, error) && ok;
    ok = registry.bindTypedWithContext<Bytes, std::int64_t, std::int64_t, std::int64_t>(prefix + "__keyBytes", [cache](Context &ctx, std::int64_t handle, std::int64_t node, std::int64_t index) { return access<Bytes>(cache, ctx, handle, node, [&](Document &, const Value &v) {
                                                                                                                                                                                                       auto key = keyAt(v, index);
                                                                                                                                                                                                       return key ? Bytes(key->begin(), key->end()) : Bytes();
                                                                                                                                                                                                   }); }, error) && ok;
    return ok;
}
}  // namespace structured_data_glue
#endif
