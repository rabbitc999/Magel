/**
 * @file magel_macro_structured_test.cpp
 * @brief 宏结构化拼接 `$concat` 与字符串化 `$stringify`单元测试。
 *
 * 覆盖：结构化拼接的标识符检查、形参加括号绕过、内部来源标记清理、`$for` 循环变量字符串化、
 * 字符串化的原文捕获及表达式参数标注的共存。
 */
#include "../source/magel_macro.hpp"
#include <cassert>

namespace
{

/**
 * @brief 统计子串出现次数。
 * @param text 被查找文本。
 * @param needle 子串。
 * @return 出现次数。
 */
std::size_t count(const Memory::String &text, const Memory::String &needle)
{
    std::size_t total = 0;
    for (std::size_t at = text.find(needle); at != Memory::String::npos;
         at = text.find(needle, at + needle.size()))
        ++total;
    return total;
}

/**
 * @brief 跑一遍预处理并断言失败，返回报错文案。
 * @param code 源码。
 * @return 错误信息。
 */
Memory::String mustFail(const Memory::String &code)
{
    preprocess::Result source;
    source.text = code;
    Memory::String error;
    assert(!magel_macro::run(source, error));
    assert(!error.empty());
    return error;
}

/**
 * @brief 跑一遍预处理并断言成功，返回结果文本。
 * @param code 源码。
 * @return 预处理结果。
 */
Memory::String mustPass(const Memory::String &code)
{
    preprocess::Result source;
    source.text = code;
    Memory::String error;
    assert(magel_macro::run(source, error));
    assert(error.empty());
    return source.text;
}

}  // namespace

/**
 * @brief 结构化拼接与字符串化的全部断言。
 * @return 成功返回 0。
 */
int main()
{
    /* `$concat` 拼出标识符：两个实参都是标识符。 */
    {
        const Memory::String out = mustPass(
            "$macro DECLARE(prefix, suffix, value)\n"
            "int64 $concat(prefix, suffix) = value;\n"
            "$end_macro\n"
            "$DECLARE(local, Count, 11)\n");
        assert(out.find("int64 localCount") != Memory::String::npos);
        assert(out.find("11") != Memory::String::npos);
        assert(out.find("$concat") == Memory::String::npos);
    }

    /* 一个实参是字面标识符，另一个是形参。 */
    {
        const Memory::String out = mustPass(
            "$macro M(suffix)\n"
            "int64 $concat(saved_, suffix) = 1;\n"
            "$end_macro\n"
            "$M(Tail)\n");
        assert(out.find("int64 saved_Tail = 1;") != Memory::String::npos);
    }

    /* 形参的括号标注会被 `$concat` 绕开：要的正是拼接后的标识符。 */
    {
        const Memory::String out = mustPass(
            "$macro M((x))\n"
            "int64 $concat(x, Tail) = 1;\n"
            "$end_macro\n"
            "$M(a)\n");
        assert(out.find("int64 aTail = 1;") != Memory::String::npos);
    }

    /* 实参里嵌套宏展开时，展开结果带的内部来源标记不能混进拼接结果。 */
    {
        const Memory::String out = mustPass(
            "$macro INNER()\n"
            "local\n"
            "$end_macro\n"
            "$macro M(a)\n"
            "int64 $concat(a, Tail) = 1;\n"
            "$end_macro\n"
            "$M($INNER)\n");
        assert(out.find("int64 localTail = 1;") != Memory::String::npos);
    }

    /* 普通代码里也能用：没有形参，实参按字面标识符解析。 */
    {
        const Memory::String out = mustPass("int64 $concat(foo, bar) = 0;\n");
        assert(out.find("int64 foobar = 0;") != Memory::String::npos);
    }

    /* 实参类型在展开阶段检查：不是标识符就报错。 */
    {
        const Memory::String error = mustFail(
            "$macro M(x)\n"
            "int64 $concat(x, 1) = 1;\n"
            "$end_macro\n"
            "$M(a)\n");
        assert(error.find("$concat 的实参必须是标识符") != Memory::String::npos);
    }
    {
        const Memory::String error = mustFail(
            "$macro M(a)\n"
            "int64 $concat(a + b, Tail) = 1;\n"
            "$end_macro\n"
            "$M(x)\n");
        assert(error.find("得到 `a + b`") != Memory::String::npos);
    }

    /* 实参个数固定两个。 */
    {
        const Memory::String error = mustFail(
            "$macro M(a)\n"
            "int64 $concat(a) = 1;\n"
            "$end_macro\n"
            "$M(b)\n");
        assert(error.find("$concat 只接受两个标识符") != Memory::String::npos);
        assert(error.find("宏 M 的") != Memory::String::npos);
    }

    /* `$stringify(形参)` 捕获调用点的原始实参文本，不受参数标注影响。 */
    {
        const Memory::String out = mustPass(
            "$macro A((expr))\n"
            "$stringify(expr)\n"
            "$end_macro\n"
            "$macro B(expr)\n"
            "$stringify(expr)\n"
            "$end_macro\n"
            "$A(a + b)\n"
            "$B(a + b)\n");
        assert(count(out, "\"a + b\"") == 2u);
        assert(out.find("#expr") == Memory::String::npos);
    }

    /* 捕获的是原文：实参里的宏调用不被展开。 */
    {
        const Memory::String out = mustPass(
            "$macro SOURCE(expr)\n"
            "$stringify(expr)\n"
            "$end_macro\n"
            "$macro SQ(x)\n"
            "((x) * (x))\n"
            "$end_macro\n"
            "std::lang::println($SOURCE($SQ(2)));\n");
        assert(out.find("\"$SQ(2)\"") != Memory::String::npos);
    }

    /* 实参不是形参名时取写下的原文。 */
    {
        const Memory::String out = mustPass(
            "$macro M()\n"
            "std::lang::println($stringify(a + b));\n"
            "$end_macro\n"
            "$M\n");
        assert(out.find("std::lang::println(\"a + b\");") != Memory::String::npos);
    }

    /* 普通代码里也能用，和 `$line` / `$file` / `$module` 同属 `$` 前缀元数据。 */
    {
        const Memory::String out = mustPass("std::lang::println($stringify(1 + 2));\n");
        assert(out.find("std::lang::println(\"1 + 2\");") != Memory::String::npos);
    }

    /* 跨行写实参时，内部来源标记不能落进字符串字面量。 */
    {
        const Memory::String out = mustPass(
            "$macro M()\n"
            "$stringify(a +\n"
            "2)\n"
            "$end_macro\n"
            "$M\n");
        assert(out.find("\"a +\\n2\"") != Memory::String::npos);
        assert(out.find("__magel_source") == Memory::String::npos);
    }

    /* `$for` 的循环变量配 `$stringify` 能取到该元素的文本；`#` 仍然被拒绝。 */
    {
        const Memory::String out = mustPass(
            "$macro MANY(...values)\n"
            "$for item in values\n"
            "std::lang::println($stringify(item));\n"
            "$end_for\n"
            "$end_macro\n"
            "$MANY(1+1, 2)\n");
        assert(out.find("std::lang::println(\"1+1\");") != Memory::String::npos);
        assert(out.find("std::lang::println(\"2\");") != Memory::String::npos);
    }
    /* 只接受一个实参，否则展开期报错。 */
    {
        const Memory::String error = mustFail(
            "$macro M()\n"
            "std::lang::println($stringify(a, b));\n"
            "$end_macro\n"
            "$M\n");
        assert(error.find("$stringify 只接受一个表达式") != Memory::String::npos);
    }
    {
        const Memory::String error = mustFail(
            "$macro M()\n"
            "$stringify(a\n"
            "$end_macro\n"
            "$M\n");
        assert(error.find("$stringify 的实参表缺少 `)`") != Memory::String::npos);
    }

    /* `concat` / `stringify` 是内部 DSL 关键字，不能拿来当宏名。 */
    assert(magel_macro::macroDslKeyword("concat"));
    assert(magel_macro::macroDslKeyword("stringify"));
    {
        const Memory::String error = mustFail(
            "$macro concat(x)\n"
            "x\n"
            "$end_macro\n"
            "int64 main() { return 0; }\n");
        assert(error.find("内部 DSL 关键字") != Memory::String::npos);
    }

    /* 拼接与字符串化可在同一段源码中共存。 */
    {
        const Memory::String out = mustPass(
            "$macro DECLARE(prefix, suffix, value)\n"
            "int64 $concat(prefix, suffix) = value;\n"
            "$end_macro\n"
            "$macro STRINGIFY(value)\n"
            "$stringify(value)\n"
            "$end_macro\n"
            "$DECLARE(local, Count, 11)\n"
            "std::lang::println($STRINGIFY($SQUARE(3 + 1)));\n");
        assert(out.find("int64 localCount") != Memory::String::npos);
        assert(out.find("\"$SQUARE(3 + 1)\"") != Memory::String::npos);
    }

    return 0;
}
