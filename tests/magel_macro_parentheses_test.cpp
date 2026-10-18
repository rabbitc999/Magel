/**
 * @file magel_macro_parentheses_test.cpp
 * @brief 表达式宏默认标注、普通宏显式标注及非法声明回归。
 */
#include "../source/magel_macro.hpp"
#include <cassert>

/**
 * @brief 解析宏头并验证成功。
 * @param text 定义头。
 * @return 解析后的宏。
 */
magel_macro::Macro parse(const char *text)
{
    Memory::String name, error;
    magel_macro::Macro macro;
    assert(magel_macro::header(text, name, macro, error));
    assert(name == "ADD");
    return macro;
}

/**
 * @brief 验证括号规则和展开后的优先级保护。
 * @return 成功返回零。
 */
int main()
{
    const auto plain = parse("$macro ADD(a, (b), ...(args))");
    assert(!plain.parenthesizeResult && !plain.parenthesized.count("a"));
    assert(plain.parenthesized.count("b") && plain.parenthesizeVariadic);
    const auto implicit = parse("$macro expr ADD(a, b, ...args)");
    const auto explicit_ = parse("$macro expr ADD((a), (b), ...(args))");
    assert(implicit.parenthesizeResult && implicit.parenthesizeVariadic);
    assert(implicit.params == explicit_.params);
    assert(implicit.parenthesized == explicit_.parenthesized);
    assert(implicit.parenthesizeVariadic == explicit_.parenthesizeVariadic);
    for (const char *text : { "$macro !ADD(a)", "$macro ADD(`a)",
                              "$macro ADD(...`args)", "$macro ADD(((a)))", "$macro ADD((a, b))",
                              "$macro ADD((a)", "$macro ADD((a), a)", "$macro ADD(...(args), a)",
                              "$macro ADD((...args))", "$macro ADD(...())" })
    {
        Memory::String name, error;
        magel_macro::Macro macro;
        assert(!magel_macro::header(text, name, macro, error));
        assert(!error.empty());
    }
    preprocess::Result source;
    source.text = "$replace expr ADD(a, (b)) a + b\n"
                  "3 * $ADD(x + 1, y * 2)\n"
                  "$macro PRINT(...(args))\n"
                  "$for v in args\n"
                  "v * v;\n"
                  "$end_for\n$end_macro\n$PRINT(x + 1, f(2, 3))\n";
    Memory::String error;
    assert(magel_macro::run(source, error));
    const auto expanded = magel_macro::cleanPasteMarkers(source.text, true);
    Memory::String compact;
    for (char ch : expanded)
        if (!std::isspace(static_cast<unsigned char>(ch))) compact += ch;
    assert(compact.find("3*((x+1)+(y*2))") != Memory::String::npos);
    assert(compact.find("(x+1)*(x+1)") != Memory::String::npos);
    assert(compact.find("(f(2,3))*(f(2,3))") != Memory::String::npos);
    source.text = "$replace expr ADD(a, b) a + b\n$!ADD(1, 2)\n";
    error.clear();
    assert(!magel_macro::run(source, error));
    assert(error.find("`$!` 已移除") != Memory::String::npos);
    return 0;
}
