#ifndef CHALI_MACRO_REGEX_MATCHER_HPP
#define CHALI_MACRO_REGEX_MATCHER_HPP

#include <memory>
#include <string>
#include <string_view>

namespace chali_macro
{
/**
 * @brief 编译期正则检查结果，区分普通不匹配和无效模式。
 */
enum class RegexResult
{
    Matched,
    Unmatched,
    Invalid
};

/**
 * @brief 缓存 ECMAScript 模式并完整匹配宏实参原文，不导出捕获组。
 * @note 实现独立编译，解析器及宏接口无需包含标准库 regex 实现头。
 */
class RegexMatcher
{
public:
    /**
     * @brief 创建本次编译独享的模式缓存。
     */
    RegexMatcher();
    /**
     * @brief 释放本次编译缓存的模式及无效模式诊断。
     */
    ~RegexMatcher();
    /**
     * @brief 禁止复制模式缓存，保持每次编译的状态独立。
     * @param other 不允许复制的另一个匹配器。
     * @note 此操作已删除。
     */
    RegexMatcher(const RegexMatcher &other) = delete;
    /**
     * @brief 禁止以另一个编译会话的匹配器覆盖当前缓存。
     * @param other 不允许赋值的另一个匹配器。
     * @note 此操作已删除，不会执行赋值。
     */
    RegexMatcher &operator=(const RegexMatcher &other) = delete;
    /**
     * @brief 检查模式是否合法并缓存编译结果，不执行匹配。
     * @param pattern 已按语言字符串规则解码的正则模式。
     * @param error 无效模式诊断，成功时清空。
     * @return 模式有效返回 true，无效返回 false。
     */
    bool validate(std::string_view pattern, std::string &error);
    /**
     * @brief 去掉片段两端空白后完整匹配，保留内部引号、注释及换行。
     * @param text 宏参数原始片段，不是 AST 重新打印的文本。
     * @param pattern 已解码的 ECMAScript 模式。
     * @param error 无效模式或匹配引擎错误，普通匹配及不匹配时清空。
     * @return 成功匹配、普通不匹配或错误三种状态。
     */
    RegexResult match(std::string_view text, std::string_view pattern, std::string &error);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace chali_macro
#endif
