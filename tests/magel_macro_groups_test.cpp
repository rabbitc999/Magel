/**
 * @file magel_macro_groups_test.cpp
 * @brief 验证多括号宏的文本捕获、隔离及多行来源映射。
 */
#include "../source/magel_macro.hpp"
#include <cassert>

/**
 * @brief 运行宏预处理并核对是否成功。
 * @param text 输入源码。
 * @param success 预期成功状态。
 * @retval 成功时返回映射结果，失败时把错误保存在 text 字段中。
 */
preprocess::Result process(const Memory::String &text, bool success = true)
{
    preprocess::Result result;
    result.text = text;
    for (std::size_t row = 0; row <= static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n')); ++row)
        result.lineMap.push_back({ "groups.mgl", row + 1 });
    Memory::String error;
    assert(magel_macro::run(result, error) == success);
    if (!success) result.text = error;
    return result;
}

/**
 * @brief 验证展开中指定文本的文件、行与列。
 * @param result 展开结果。
 * @param needle 应唯一出现的文本。
 * @param line 原始行号。
 * @param column 原始列号，从一计数。
 */
void position(const preprocess::Result &result, const Memory::String &needle, std::size_t line, std::size_t column)
{
    const auto at = result.text.find(needle);
    assert(at != Memory::String::npos);
    const auto row = static_cast<std::size_t>(std::count(result.text.begin(), result.text.begin() + at, '\n'));
    const auto previous = result.text.rfind('\n', at);
    assert(at - (previous == Memory::String::npos ? 0 : previous + 1) + 1 == column);
    assert(result.lineMap[row].file == "groups.mgl");
    assert(result.lineMap[row].line == line);
}

/**
 * @brief 执行有序多括号宏与原始字符分组回归。
 * @retval 所有断言通过返回零。
 */
int main()
{
    Memory::Vector<Memory::String> partial;
    std::size_t partialEnd = 0;
    bool closed = true;
    assert(!magel_macro::groupArguments("(1, fn(a, b) { a + b }, /* editing", 0, partial, partialEnd));
    partial.clear();
    assert(magel_macro::groupArguments("(1, fn(a, b) { a + b }, /* editing", 0, partial, partialEnd, true, &closed));
    assert(!closed && partial.size() == 3);
    partial.clear();
    assert(!magel_macro::groupArguments("(1, [2)}", 0, partial, partialEnd, true));
    const auto result = process(
        "$macro M<..Types>[..Attrs](..Args){..Body}\n"
        "$stringify(Types); $stringify(Attrs); $stringify(Args); Body\n"
        "$end_macro\n"
        "$M<List<List<int64>>, String>[readonly, shared](x, fn(a, b)) {\n"
        "    first();\n"
        "    if (ok) { second(); } // } ] >\n"
        "    third(\"} > ]\");\n"
        "} after();\n"
        "tail();\n");
    assert(result.text.find("\"List<List<int64>>, String\"") != Memory::String::npos);
    assert(result.text.find("\"readonly, shared\"") != Memory::String::npos);
    assert(result.text.find("\"x, fn(a, b)\"") != Memory::String::npos);
    position(result, "first();", 5, 5);
    position(result, "second();", 6, 15);
    position(result, "third(", 7, 5);
    position(result, "after();", 8, 3);
    position(result, "tail();", 9, 1);
    assert(result.text.find("__magel_source") == Memory::String::npos);

    assert(process("$macro E()()\nempty;\n$end_macro\n$E()()\n").text.find("empty;") != Memory::String::npos);
    assert(process("$macro R[](x)()\nx\n$end_macro\n$R[](raw)()\n").text.find("raw") != Memory::String::npos);
    assert(process("$macro R[a][b]\n$stringify(a); $stringify(b);\n$end_macro\n$R[left][right]\n").text.find("\"right\"") != Memory::String::npos);
    assert(process("$macro E<..a>[..b](..c){..d}\n$stringify(a); $stringify(b); $stringify(c); $stringify(d);\n$end_macro\n$E<>[](){}\n").text.find("\"\"") != Memory::String::npos);
    assert(process("$macro R[x]\nx\n$end_macro\n$macro R<x>\nx\n$end_macro\n$R[left]; $R<right>;\n").text.find("right") != Memory::String::npos);
    for (const auto *call : { "$R(x)[y]", "$R[x](y)", "$R[x]{y}", "$R[x][y", "$R[(x]][y]" })
        assert(process(Memory::String("$macro R[a][b]\na b\n$end_macro\n") + call + "\n", false).text.find("宏Token分组不匹配") != Memory::String::npos);
    // 未使用的内容保持任意文本，不触发类型、表达式或 AST 检查。
    assert(process("$macro IGNORE<..a>{..b}\nokay;\n$end_macro\n$IGNORE<Unknown<Broken>>{ nonsense @@@; }\n").text.find("okay;") != Memory::String::npos);
    assert(process("$macro B{..body}\nbody\n$end_macro\n$B{ if (x > y && a < b) { result = x >> 1; } }\n").text.find("x >> 1") != Memory::String::npos);
    const auto loops = process("$macro F<..types>[..attrs]\n$for t in types\n$for a in attrs\n$stringify(t); $stringify(a);\n$end_for\n$end_for\n$end_macro\n$F<int64, String>[readonly, shared]\n");
    assert(loops.text.find("\"int64\"") != Memory::String::npos);
    assert(loops.text.find("\"shared\"") != Memory::String::npos);
    const auto nested = process("$macro INNER{..body}\nbody\n$end_macro\n$macro OUTER{..body}\n$INNER{body}\n$end_macro\n$OUTER{\n    nested();\n}\n");
    position(nested, "nested();", 8, 5);
    const auto literal = process("$macro M{..body}\nbody missing;\n$end_macro\n$M{value;}\n");
    position(literal, "missing;", 2, 6);
    const auto comment = process("$macro M{..body}\nbody\n$end_macro\n$M{\n  /* first\n     second */ afterComment();\n}\n");
    position(comment, "afterComment();", 6, 16);
    const auto string = process("$macro M{..body}\nbody\n$end_macro\n$M{\"first\nsecond\"; afterString();}\n");
    assert(string.text.find("\"first\nsecond\"") != Memory::String::npos);
    position(string, "afterString();", 5, 10);
    assert(process("$macro C(..args)\n$argc\n$end_macro\n$C(x, y, z)\n").text.find("3") != Memory::String::npos);
    assert(process("$macro C<..types>[..attrs]\n$if $argc == 3\ncounted;\n$end_if\n$end_macro\n$C<int64, String>[readonly]\n").text.find("counted;") != Memory::String::npos);
    return 0;
}
