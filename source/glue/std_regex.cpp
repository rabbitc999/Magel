#include "std_regex.hpp"
#include "std_native_record.hpp"
#include "../allocator/chali_allocator.hpp"

#include "../chali_glue.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace std_regex
{

/**
 * @brief 拥有不可变编译模式的引用；复制共享模式而不共享可变匹配状态。
 */
struct Pattern
{
    std::shared_ptr<const std::regex> compiled;
};
/**
 * @brief 复制模式引用，共享不可变的编译结果。
 * @param source 原模式引用。
 * @param error 本操作不产生可恢复诊断。
 * @return 新分配的模式引用。
 */
void *clonePattern(const void *source, Memory::String &error)
{
    (void)error;
    return new Pattern(*static_cast<const Pattern *>(source));
}
/**
 * @brief 释放模式引用，最后一个引用释放编译结果。
 * @param value 要释放的模式引用。
 */
void destroyPattern(void *value) { delete static_cast<Pattern *>(value); }
const stackvm::NativeRecordType patternType{ "std::regex::Regex", clonePattern, destroyPattern, nullptr };
/**
 * @brief 创建一个拥有独立生命周期的模式引用。
 * @param ctx 当前调用上下文。
 * @param compiled 编译结果，空值表示未初始化或编译失败。
 * @return 原生记录句柄。
 */
std::int64_t makePattern(chali_glue::Context &ctx, std::shared_ptr<const std::regex> compiled = {})
{
    auto value = std::make_unique<Pattern>();
    value->compiled = std::move(compiled);
    auto handle = ctx.makeNativeRecord(patternType, value.get());
    if (handle) value.release();
    return handle;
}
/**
 * @brief 取得不可变模式的共享引用，调用结束即可释放引用。
 * @param ctx 当前调用上下文。
 * @param handle 拥有模式引用的原生记录。
 * @return 编译模式；记录无效时报告诊断并返回空。
 */
std::shared_ptr<const std::regex> get(chali_glue::Context &ctx, std::int64_t handle)
{
    auto *value = static_cast<Pattern *>(ctx.nativeRecord(handle, patternType));
    if (!value || !value->compiled)
    {
        ctx.fail("正则表达式无效；请先调用 std::regex::Regex::compile");
        return {};
    }
    return value->compiled;
}

/**
 * @brief 为宿主注册 std_regex 模块的标准库接口。
 * @param registry 接收类型化函数绑定的宿主注册表。
 * @param error 绑定失败时接收原始诊断，可为空。
 * @return 全部入口绑定成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    using chali_glue::Context;
    auto &native = registry.native();
    bool hooks = native_record_glue::bind(native, "std::regex::__regexCopy", 1, native_record_glue::copy, error)
                 && native_record_glue::bind(native, "std::regex::__regexDrop", 1, native_record_glue::drop, error);
    return hooks && registry.bindTypedWithContext<std::int64_t>("std::regex::__regexNew", [](Context &ctx) { return makePattern(ctx); }, error) && registry.bindTypedWithContext<bool, std::int64_t>("std::regex::__regexValid", [](Context &ctx, std::int64_t handle) {
            auto *value = static_cast<Pattern *>(ctx.nativeRecord(handle, patternType));
            return value && static_cast<bool>(value->compiled); }, error) && registry.bindTypedWithContext<std::int64_t, Memory::String>("std::regex::__regexCompileHandle", [](Context &ctx, Memory::String pattern) {
                   try { return makePattern(ctx, std::make_shared<const std::regex>(pattern, std::regex::ECMAScript)); }
                   catch (const std::regex_error &) { return makePattern(ctx); } }, error) && registry.bindTypedWithContext<bool, std::int64_t, Memory::String>("std::regex::__regexFullMatch", [](Context &ctx, std::int64_t handle, Memory::String text) {
                   auto regex = get(ctx, handle);
                   return regex && std::regex_match(text, *regex); }, error) && registry.bindTypedWithContext<bool, std::int64_t, Memory::String>("std::regex::__regexContains", [](Context &ctx, std::int64_t handle, Memory::String text) {
                   auto regex = get(ctx, handle);
                   return regex && std::regex_search(text, *regex); }, error) && registry.bindTypedWithContext<std::int64_t, std::int64_t, Memory::String>("std::regex::__regexFindByteIndex", [](Context &ctx, std::int64_t handle, Memory::String text) {
                   auto regex = get(ctx, handle);
                   std::match_results<Memory::String::const_iterator> match;
                   return regex && std::regex_search(text, match, *regex)
                              ? static_cast<std::int64_t>(match.position()) : std::int64_t{-1}; }, error) && registry.bindTypedWithContext<Memory::String, std::int64_t, Memory::String, int>("std::regex::__regexCapture", [](Context &ctx, std::int64_t handle, Memory::String text, int group) {
                   auto regex = get(ctx, handle);
                   std::match_results<Memory::String::const_iterator> match;
                   if (!regex || !std::regex_search(text, match, *regex) ||
                       group < 0 || static_cast<std::size_t>(group) >= match.size())
                       return Memory::String{};
                   return Memory::String(match[static_cast<std::size_t>(group)].str()); }, error) && registry.bindTypedWithContext<Memory::String, std::int64_t, Memory::String, Memory::String>("std::regex::__regexReplaceFirst", [](Context &ctx, std::int64_t handle, Memory::String text, Memory::String replacement) {
                   auto regex = get(ctx, handle);
                   return regex ? std::regex_replace(text, *regex, replacement,
                       std::regex_constants::format_first_only) : Memory::String{}; }, error) && registry.bindTypedWithContext<Memory::String, std::int64_t, Memory::String, Memory::String>("std::regex::__regexReplaceAll", [](Context &ctx, std::int64_t handle, Memory::String text, Memory::String replacement) {
                   auto regex = get(ctx, handle);
                   return regex ? std::regex_replace(text, *regex, replacement) : Memory::String{}; }, error);
}

}  // namespace std_regex
