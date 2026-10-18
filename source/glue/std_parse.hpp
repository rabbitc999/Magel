#ifndef PARSE_ENGINE_STD_PARSE_HPP
#define PARSE_ENGINE_STD_PARSE_HPP

#include "../magel_glue.hpp"

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace std_parse_glue
{

/* `std::lang::Parse` 的整数 / 浮点 / 布尔解析。从 VM 内部原生表（`kSlotNativeEntries`）
 * 搬到 Registry 胶水，状态码与 MAGEL 侧 `ParseError` 一一对应：
 *   0 = Ok、1 = Empty、2 = Overflow、3 = Invalid。
 * `…Value` 只在 `…Error` 返回 0 之后调用，失败路径是防御性的（照原实现报运行期错误）。 */
enum class ParseStatus : std::int64_t
{
    Ok = 0,
    Empty = 1,
    Overflow = 2,
    Invalid = 3
};

template <typename Integer>
ParseStatus parseIntegerText(const Memory::String &text, Integer *value)
{
    if (text.empty()) return ParseStatus::Empty;
    Integer parsed{};
    const char *const begin = text.data();
    const char *const end = begin + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, parsed, 10);
    if (result.ec == std::errc::result_out_of_range) return ParseStatus::Overflow;
    if (result.ec != std::errc() || result.ptr != end) return ParseStatus::Invalid;
    if (value != nullptr) *value = parsed;
    return ParseStatus::Ok;
}

inline ParseStatus parseFloatText(const Memory::String &text, double *value)
{
    if (text.empty()) return ParseStatus::Empty;
    errno = 0;
    char *end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    if (errno == ERANGE || !std::isfinite(parsed)) return ParseStatus::Overflow;
    if (end != text.c_str() + text.size()) return ParseStatus::Invalid;
    if (value != nullptr) *value = parsed;
    return ParseStatus::Ok;
}

inline ParseStatus parseBoolText(const Memory::String &text, bool *value)
{
    if (text.empty()) return ParseStatus::Empty;
    if (text == "true" || text == "false")
    {
        if (value != nullptr) *value = text == "true";
        return ParseStatus::Ok;
    }
    return ParseStatus::Invalid;
}

inline bool bind(magel_glue::Registry &registry, Memory::String *error = nullptr)
{
    using magel_glue::Context;
    using Memory::String;
    const auto asInt = [](ParseStatus status) { return static_cast<std::int64_t>(status); };
    return registry.bindTyped<std::int64_t, String>(
               "std::parse::__parseInt64Error",
               [asInt](String text) {
                   return asInt(parseIntegerText<std::int64_t>(text, nullptr));
               },
               error)
           && registry.bindTypedWithContext<std::int64_t, String>(
               "std::parse::__parseInt64Value",
               [](Context &context, String text) {
                   std::int64_t value = 0;
                   const ParseStatus status = parseIntegerText<std::int64_t>(text, &value);
                   if (status != ParseStatus::Ok) context.fail("std::lang::Parse.parseInt64 输入无效");
                   return value;
               },
               error)
           && registry.bindTyped<std::int64_t, String>(
               "std::parse::__parseUInt64Error",
               [asInt](String text) {
                   return asInt(parseIntegerText<std::uint64_t>(text, nullptr));
               },
               error)
           && registry.bindTypedWithContext<std::int64_t, String>(
               "std::parse::__parseUInt64Value",
               [](Context &context, String text) {
                   std::uint64_t value = 0;
                   const ParseStatus status = parseIntegerText<std::uint64_t>(text, &value);
                   if (status != ParseStatus::Ok) context.fail("std::lang::Parse.parseUInt64 输入无效");
                   return static_cast<std::int64_t>(value);
               },
               error)
           && registry.bindTyped<std::int64_t, String>(
               "std::parse::__parseFloat64Error",
               [asInt](String text) { return asInt(parseFloatText(text, nullptr)); }, error)
           && registry.bindTypedWithContext<double, String>(
               "std::parse::__parseFloat64Value",
               [](Context &context, String text) {
                   double value = 0.0;
                   const ParseStatus status = parseFloatText(text, &value);
                   if (status != ParseStatus::Ok) context.fail("std::lang::Parse.parseFloat64 输入无效");
                   return value;
               },
               error)
           && registry.bindTyped<std::int64_t, String>(
               "std::parse::__parseBoolError",
               [asInt](String text) { return asInt(parseBoolText(text, nullptr)); }, error)
           && registry.bindTypedWithContext<bool, String>(
               "std::parse::__parseBoolValue",
               [](Context &context, String text) {
                   bool value = false;
                   const ParseStatus status = parseBoolText(text, &value);
                   if (status != ParseStatus::Ok) context.fail("std::lang::Parse.parseBool 输入无效");
                   return value;
               },
               error);
}

}  // namespace std_parse_glue

#endif
