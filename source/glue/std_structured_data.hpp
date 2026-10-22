#ifndef PARSE_ENGINE_STRUCTURED_DATA_HPP
#define PARSE_ENGINE_STRUCTURED_DATA_HPP
#include "../chali_glue.hpp"
#include "std_native_record.hpp"
#include <memory>
#include <optional>
#include <string>
#include <string_view>

/**
 * @brief JSON/TOML 共用的拥有型值记录，不保存全局文档表或失效节点指针。
 */
namespace structured_data_glue
{
using Context = chali_glue::Context;
using String = Memory::String;
using Bytes = Memory::Vector<std::uint8_t>;
/**
 * @brief 独占一棵结构化子树和该值的解析诊断。
 * @tparam Traits JSON 或 TOML 的值与序列化策略。
 */
template <class Traits>
struct OwnedValue
{
    std::optional<typename Traits::Value> value;
    std::string error;
};
/**
 * @brief 深复制子树，关闭或释放任一副本不影响其他值。
 * @tparam Traits 当前格式策略。
 * @param source 待复制的拥有型值。
 * @param error 本操作不产生可恢复诊断。
 * @return 具有独立值存储的新记录。
 */
template <class Traits>
void *cloneValue(const void *source, String &error)
{
    (void)error;
    return new OwnedValue<Traits>(*static_cast<const OwnedValue<Traits> *>(source));
}
/**
 * @brief 销毁拥有的子树与解析诊断。
 * @tparam Traits 当前格式策略。
 * @param value 待释放的值记录。
 */
template <class Traits>
void destroyValue(void *value)
{ delete static_cast<OwnedValue<Traits> *>(value); }
/**
 * @brief 取得格式专属的原生记录生命周期描述符。
 * @tparam Traits 当前格式策略。
 * @return 生命周期覆盖整个进程的描述符。
 */
template <class Traits>
const stackvm::NativeRecordType &valueType()
{
    static const stackvm::NativeRecordType type{ Traits::recordName, cloneValue<Traits>, destroyValue<Traits>, nullptr };
    return type;
}
/**
 * @brief 将一棵独立子树或失败诊断移交给 VM 的 opcode 记录。
 * @tparam Traits 当前格式策略。
 * @param context 当前调用上下文。
 * @param value 待接管的子树，空值表示未初始化或解析失败。
 * @param error 与空值关联的原始诊断。
 * @return 由当前 VM 拥有的记录句柄。
 */
template <class Traits>
std::int64_t makeValue(Context &context, std::optional<typename Traits::Value> value = {}, std::string error = {})
{
    auto owned = std::make_unique<OwnedValue<Traits>>();
    owned->value = std::move(value);
    owned->error = std::move(error);
    const auto handle = context.makeNativeRecord(valueType<Traits>(), owned.get());
    if (handle) owned.release();
    return handle;
}
/**
 * @brief 校验值句柄，不借用另一个文档的生命周期。
 * @tparam Traits 当前格式策略。
 * @param context 当前调用上下文。
 * @param handle 拥有型值记录句柄。
 * @return 存活记录；无效句柄报告诊断并返回空。
 */
template <class Traits>
OwnedValue<Traits> *ownedValue(Context &context, std::int64_t handle)
{
    auto *value = static_cast<OwnedValue<Traits> *>(context.nativeRecord(handle, valueType<Traits>()));
    if (!value) context.fail("结构化数据：值已经释放或类型无效");
    return value;
}
/**
 * @brief 在有效值上执行只读查询；关闭的值报告错误。
 * @tparam Result 查询返回类型。
 * @tparam Traits 当前格式策略。
 * @tparam Function 查询操作的类型。
 * @param context 当前调用上下文。
 * @param handle 拥有型值记录句柄。
 * @param function 接收不可变子树的查询操作。
 * @return 查询结果；失败时报告诊断并返回默认值。
 */
template <class Result, class Traits, class Function>
Result access(Context &context, std::int64_t handle, Function function)
{
    auto *owned = ownedValue<Traits>(context, handle);
    if (!owned) return Result{};
    if (!owned->value)
    {
        context.fail("结构化数据：值已经关闭或未初始化");
        return Result{};
    }
    return function(*owned->value);
}
/**
 * @brief 按插入顺序读取对象键名。
 * @tparam Value 当前格式的值类型。
 * @param value 待查询对象。
 * @param index 从零开始的成员位置。
 * @return 存在的键名指针，否则为空。
 */
template <class Value>
const std::string *keyAt(const Value &value, std::int64_t index)
{
    const auto *object = value.asObject();
    return object && index >= 0 && static_cast<std::uint64_t>(index) < object->size() ? &(object->begin() + index)->key : nullptr;
}
/**
 * @brief 注册两种格式共享的值复制、关闭、子树提取与只读访问。
 * @tparam Traits 当前格式的解析和序列化策略。
 * @param registry 当前宿主函数注册表。
 * @param prefix 当前格式的模块限定前缀。
 * @param error 接收注册失败诊断。
 * @return 全部入口注册成功时返回 true。
 */
template <class Traits>
bool bind(chali_glue::Registry &registry, const String &prefix, String *error)
{
    using Value = typename Traits::Value;
    bool ok = registry.bindTypedWithContext<std::int64_t>(prefix + "__valueNew", [](Context &context) { return makeValue<Traits>(context); }, error);
    ok = native_record_glue::bind(registry.native(), (prefix + "__valueCopy").c_str(), 1, native_record_glue::copy, error) && ok;
    ok = native_record_glue::bind(registry.native(), (prefix + "__valueDrop").c_str(), 1, native_record_glue::drop, error) && ok;
    ok = registry.bindTypedWithContext<std::int64_t, String>(prefix + "__parse", [](Context &context, String text) {
        auto parsed = Traits::parse(std::string_view(text.data(), text.size()));
        std::string message;
        if (!parsed.value) message = parsed.diagnostics.empty() ? "解析失败" : parsed.diagnostics.front().toString();
        return makeValue<Traits>(context, std::move(parsed.value), std::move(message)); }, error) && ok;
    ok = registry.bindTypedWithContext<bool, std::int64_t>(prefix + "__valid", [](Context &context, std::int64_t handle) {
        if (!handle) return false; auto *value = ownedValue<Traits>(context, handle); return value && value->value.has_value(); }, error) && ok;
    ok = registry.bindTypedWithContext<String, std::int64_t>(prefix + "__takeError", [](Context &context, std::int64_t handle) {
        auto *value = ownedValue<Traits>(context, handle); return value ? String(value->error.c_str()) : String("无效值句柄"); }, error) && ok;
    ok = registry.bindTypedWithContext<bool, std::int64_t>(prefix + "__close", [](Context &context, std::int64_t handle) {
        if (!handle) return false; auto *value = ownedValue<Traits>(context, handle); if (!value || !value->value) return false; value->value.reset(); return true; }, error) && ok;
    const auto bindRead = [&](const char *name, auto resultTag, auto function) {
        using Result = decltype(resultTag);
        return registry.bindTypedWithContext<Result, std::int64_t, std::int64_t>(prefix + name, [function](Context &context, std::int64_t handle, std::int64_t) { return access<Result, Traits>(context, handle, function); }, error);
    };
    ok = bindRead("__kind", std::int64_t{}, [](const Value &value) { return Traits::kind(value); }) && ok;
    ok = bindRead("__size", std::int64_t{}, [](const Value &value) { if (auto *a = value.asArray()) return static_cast<std::int64_t>(a->size()); if (auto *o = value.asObject()) return static_cast<std::int64_t>(o->size()); return std::int64_t(0); }) && ok;
    ok = bindRead("__int", std::int64_t{}, [](const Value &value) { return value.isInt() ? value.asInt().value() : std::int64_t(0); }) && ok;
    ok = bindRead("__float", double{}, [](const Value &value) { return value.asDouble().value_or(0.0); }) && ok;
    ok = bindRead("__bool", false, [](const Value &value) { return value.asBool().value_or(false); }) && ok;
    ok = bindRead("__textAvailable", false, [](const Value &value) { auto text = value.asString(); return text && text->find('\0') == std::string_view::npos; }) && ok;
    ok = bindRead("__string", String{}, [](const Value &value) { auto text = value.asString(); return text ? String(text->data(), text->size()) : String(); }) && ok;
    ok = bindRead("__bytes", Bytes{}, [](const Value &value) { auto text = value.asString(); return text ? Bytes(text->begin(), text->end()) : Bytes(); }) && ok;
    ok = bindRead("__dateTime", String{}, [](const Value &value) { const auto text = Traits::dateTime(value); return String(text.data(), text.size()); }) && ok;
    ok = bindRead("__dump", String{}, [](const Value &value) { const auto text = Traits::dump(value); return String(text.data(), text.size()); }) && ok;
    const auto bindFind = [&](const char *name, auto keyTag) {
        using Key = decltype(keyTag);
        return registry.bindTypedWithContext<std::int64_t, std::int64_t, std::int64_t, Key>(prefix + name, [](Context &context, std::int64_t handle, std::int64_t, Key key) { return access<std::int64_t, Traits>(context, handle, [&](const Value &value) {
                                                                                                                                                                                  const auto *child = value.find(std::string_view(reinterpret_cast<const char *>(key.data()), key.size()));
                                                                                                                                                                                  return makeValue<Traits>(context, child ? std::optional<Value>(*child) : std::nullopt);
                                                                                                                                                                              }); }, error);
    };
    ok = bindFind("__find", String{}) && ok;
    ok = bindFind("__findBytes", Bytes{}) && ok;
    ok = registry.bindTypedWithContext<std::int64_t, std::int64_t, std::int64_t, std::int64_t>(prefix + "__at", [](Context &context, std::int64_t handle, std::int64_t, std::int64_t index) { return access<std::int64_t, Traits>(context, handle, [&](const Value &value) {
                                                                                                                                                                                                  const Value *child = nullptr;
                                                                                                                                                                                                  if (index >= 0)
                                                                                                                                                                                                  {
                                                                                                                                                                                                      if (auto *a = value.asArray(); a && static_cast<std::uint64_t>(index) < a->size()) child = &(*a)[static_cast<std::size_t>(index)];
                                                                                                                                                                                                      if (auto *o = value.asObject(); o && static_cast<std::uint64_t>(index) < o->size()) child = &(o->begin() + index)->value;
                                                                                                                                                                                                  }
                                                                                                                                                                                                  return makeValue<Traits>(context, child ? std::optional<Value>(*child) : std::nullopt);
                                                                                                                                                                                              }); }, error) && ok;
    ok = registry.bindTypedWithContext<bool, std::int64_t, std::int64_t, std::int64_t>(prefix + "__keyAvailable", [](Context &context, std::int64_t handle, std::int64_t, std::int64_t index) { return access<bool, Traits>(context, handle, [&](const Value &value) { const auto *key = keyAt(value,index); return key && key->find('\0') == std::string::npos; }); }, error) && ok;
    ok = registry.bindTypedWithContext<String, std::int64_t, std::int64_t, std::int64_t>(prefix + "__key", [](Context &context, std::int64_t handle, std::int64_t, std::int64_t index) { return access<String, Traits>(context, handle, [&](const Value &value) { const auto *key = keyAt(value,index); return key ? String(key->data(),key->size()) : String(); }); }, error) && ok;
    ok = registry.bindTypedWithContext<Bytes, std::int64_t, std::int64_t, std::int64_t>(prefix + "__keyBytes", [](Context &context, std::int64_t handle, std::int64_t, std::int64_t index) { return access<Bytes, Traits>(context, handle, [&](const Value &value) { const auto *key = keyAt(value,index); return key ? Bytes(key->begin(),key->end()) : Bytes(); }); }, error) && ok;
    return ok;
}
}  // namespace structured_data_glue
#endif
