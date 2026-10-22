#include "regex_matcher.hpp"
#include <map>
#include <regex>

namespace chali_macro
{
/**
 * @brief 保存编译成功的模式及失败诊断，避免重复构造正则。
 */
struct RegexMatcher::Impl
{
    /**
     * @brief 一个已编译模式或其稳定错误诊断。
     */
    struct Pattern
    {
        std::regex expression;
        std::string error;
    };
    std::map<std::string, Pattern, std::less<>> patterns;

    /**
     * @brief 查找模式，首次访问时按固定 ECMAScript 语法编译并记录错误。
     * @param pattern 已解码的模式。
     * @return 缓存项，error 为空时 expression 有效。
     */
    const Pattern &find(std::string_view pattern)
    {
        if (pattern.empty()) pattern = "";
        const auto existing = patterns.find(pattern);
        if (existing != patterns.end()) return existing->second;
        Pattern value;
        try
        {
            value.expression.assign(pattern.begin(), pattern.end(), std::regex_constants::ECMAScript);
        } catch (const std::regex_error &error)
        {
            value.error = "无效的宏正则表达式（错误代码 " + std::to_string(static_cast<int>(error.code())) + "）";
        }
        return patterns.emplace(std::string(pattern), std::move(value)).first->second;
    }
};

/**
 * @brief 创建空的编译期正则缓存。
 */
RegexMatcher::RegexMatcher() : impl_(std::make_unique<Impl>()) {}

/**
 * @brief 释放正则缓存及其实现。
 */
RegexMatcher::~RegexMatcher() = default;

/**
 * @brief 验证并缓存模式，非法模式不被当作普通不匹配。
 * @param pattern 已解码模式。
 * @param error 接收错误，合法时为空。
 * @return 模式合法时为 true。
 */
bool RegexMatcher::validate(std::string_view pattern, std::string &error)
{
    error = impl_->find(pattern).error;
    return error.empty();
}

/**
 * @brief 按固定模式完整匹配原始片段，保留原文内部结构。
 * @param text 未改写的参数原文。
 * @param pattern 已解码模式。
 * @param error 接收引擎错误，正常结果时为空。
 * @return 匹配、不匹配或引擎错误。
 */
RegexResult RegexMatcher::match(std::string_view text, std::string_view pattern, std::string &error)
{
    const auto &compiled = impl_->find(pattern);
    error = compiled.error;
    if (!error.empty()) return RegexResult::Invalid;
    constexpr std::string_view spaces = " \t\r\n\v\f";
    const auto first = text.find_first_not_of(spaces);
    if (first == std::string_view::npos)
        text = "";
    else
        text = text.substr(first, text.find_last_not_of(spaces) - first + 1);
    try
    {
        return std::regex_match(text.begin(), text.end(), compiled.expression) ? RegexResult::Matched : RegexResult::Unmatched;
    } catch (const std::regex_error &failure)
    {
        error = "宏正则匹配失败（错误代码 " + std::to_string(static_cast<int>(failure.code())) + "）";
        return RegexResult::Invalid;
    }
}
}  // namespace chali_macro
