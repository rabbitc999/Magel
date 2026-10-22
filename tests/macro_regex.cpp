#include "macro/regex_matcher.hpp"
#include <iostream>

/**
 * @brief 验证宏正则完整匹配、原文保存、捕获能力、缓存与非法模式诊断。
 * @return 全部行为满足编译期宏约定时返回零。
 */
int main()
{
    using chali_macro::RegexResult;
    chali_macro::RegexMatcher matcher;
    std::string error;
    if (matcher.match(" 7\n", "[0-9]", error) != RegexResult::Matched || !error.empty()) return 1;
    if (matcher.match("123", "[0-9]", error) != RegexResult::Unmatched || !error.empty()) return 2;
    if (matcher.match("123", "[0-9]+", error) != RegexResult::Matched) return 3;
    if (matcher.match("value7", "[0-9]+", error) != RegexResult::Unmatched) return 4;
    if (matcher.match("\"123\"", "[0-9]+", error) != RegexResult::Unmatched) return 5;
    if (matcher.match("1 /* comment */ 2", "[0-9]+", error) != RegexResult::Unmatched) return 6;
    if (matcher.match("foo42", "(foo|bar)[0-9]+", error) != RegexResult::Matched) return 7;
    if (matcher.match("abab", R"((ab)\1)", error) != RegexResult::Matched) return 8;
    if (matcher.match("value42", "value(?=[0-9])[0-9]+", error) != RegexResult::Matched) return 9;
    if (matcher.match("a\nb", R"(a\nb)", error) != RegexResult::Matched) return 10;
    if (matcher.match(" \t ", "", error) != RegexResult::Matched) return 11;
    if (matcher.validate("[", error) || error.empty()) return 12;
    const auto diagnostic = error;
    if (matcher.match("7", "[", error) != RegexResult::Invalid || error != diagnostic) return 13;
    if (matcher.match("9", "[0-9]", error) != RegexResult::Matched || !error.empty()) return 14;
    if (matcher.match({}, {}, error) != RegexResult::Matched || !error.empty()) return 15;
    std::cout << "macro regex matching passed\n";
    return 0;
}
