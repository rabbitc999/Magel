#include "../allocator/magel_allocator.hpp"
#ifndef PARSE_ENGINE_STD_REGEX_HPP
#define PARSE_ENGINE_STD_REGEX_HPP

#include "../magel_glue.hpp"

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

// The registry owns compiled patterns. Regex values only carry an integer ID.
struct Cache
{
    std::mutex mutex;
    Memory::Map<Memory::String, std::int64_t> byPattern;
    Memory::Vector<std::regex> patterns;
    /* 限制已尽量放开：不设模式字节数上限，也不设编译条数上限。
     * `Vector` 与 `byPattern` 按需增长，程序用多少就存多少（内存由宿主注册表按句柄持有，
     * MAGEL 侧仍只存整数槽，复制/忽略/离开作用域都不需要手动释放）。 */
};

inline std::optional<std::regex> get(magel_glue::Context &ctx,
                                     const std::shared_ptr<Cache> &cache,
                                     std::int64_t handle)
{
    std::lock_guard<std::mutex> lock(cache->mutex);
    if (handle <= 0 || static_cast<std::size_t>(handle) > cache->patterns.size())
    {
        ctx.fail("正则表达式句柄无效；请先调用 std::lang::Regex::compile");
        return std::nullopt;
    }
    return cache->patterns[static_cast<std::size_t>(handle) - 1u];
}

inline bool bind(magel_glue::Registry &registry, Memory::String *error = nullptr)
{
    using magel_glue::Context;
    const auto cache = Memory::makeShared<Cache>();
    return registry.bindTypedWithContext<std::int64_t, Memory::String>(
               "std::regex::__regexCompileHandle", [cache](Context &, Memory::String pattern) {
                   std::lock_guard<std::mutex> lock(cache->mutex);
                   const auto existing = cache->byPattern.find(pattern);
                   if (existing != cache->byPattern.end()) return existing->second;
                   try
                   {
                       std::regex compiled(pattern, std::regex::ECMAScript);
                       const std::int64_t id = static_cast<std::int64_t>(cache->patterns.size() + 1u);
                       cache->patterns.push_back(std::move(compiled));
                       Memory::String stored(pattern.data(), pattern.size(),
                                             Memory::ContainerAllocator<char>(cache->byPattern.get_allocator()));
                       cache->byPattern.emplace(std::move(stored), id);
                       return id;
                   } catch (const std::regex_error &)
                   {
                       return std::int64_t{ -1 };
                   }
               },
               error)
           && registry.bindTypedWithContext<bool, std::int64_t, Memory::String>("std::regex::__regexFullMatch", [cache](Context &ctx, std::int64_t handle, Memory::String text) {
                   auto regex = get(ctx, cache, handle);
                   return regex && std::regex_match(text, *regex); }, error) && registry.bindTypedWithContext<bool, std::int64_t, Memory::String>("std::regex::__regexContains", [cache](Context &ctx, std::int64_t handle, Memory::String text) {
                   auto regex = get(ctx, cache, handle);
                   return regex && std::regex_search(text, *regex); }, error) && registry.bindTypedWithContext<std::int64_t, std::int64_t, Memory::String>("std::regex::__regexFindByteIndex", [cache](Context &ctx, std::int64_t handle, Memory::String text) {
                   auto regex = get(ctx, cache, handle);
                   std::match_results<Memory::String::const_iterator> match;
                   return regex && std::regex_search(text, match, *regex)
                              ? static_cast<std::int64_t>(match.position()) : std::int64_t{-1}; }, error) && registry.bindTypedWithContext<Memory::String, std::int64_t, Memory::String, int>("std::regex::__regexCapture", [cache](Context &ctx, std::int64_t handle, Memory::String text, int group) {
                   auto regex = get(ctx, cache, handle);
                   std::match_results<Memory::String::const_iterator> match;
                   if (!regex || !std::regex_search(text, match, *regex) ||
                       group < 0 || static_cast<std::size_t>(group) >= match.size())
                       return Memory::String{};
                   return Memory::String(match[static_cast<std::size_t>(group)].str()); }, error) && registry.bindTypedWithContext<Memory::String, std::int64_t, Memory::String, Memory::String>("std::regex::__regexReplaceFirst", [cache](Context &ctx, std::int64_t handle, Memory::String text, Memory::String replacement) {
                   auto regex = get(ctx, cache, handle);
                   return regex ? std::regex_replace(text, *regex, replacement,
                       std::regex_constants::format_first_only) : Memory::String{}; }, error) && registry.bindTypedWithContext<Memory::String, std::int64_t, Memory::String, Memory::String>("std::regex::__regexReplaceAll", [cache](Context &ctx, std::int64_t handle, Memory::String text, Memory::String replacement) {
                   auto regex = get(ctx, cache, handle);
                   return regex ? std::regex_replace(text, *regex, replacement) : Memory::String{}; }, error);
}

}  // namespace std_regex

#endif
