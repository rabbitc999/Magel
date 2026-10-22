#include "chali_parser.hpp"
#include "macro/source.hpp"
#include <iostream>

/**
 * @brief 使用权威文法生成的解析器直接验证 AST 宏，不经过旧文本展开器。
 * @param text 待解析源码。
 * @param result 接收根列表编号。
 * @return 语法分析成功时为 true。
 */
static bool parse(Memory::String text, int &result)
{
    chali_macro::markHeaderBoundaries(text);
    chali::Engine parser;
    if (parser.runText(text, &result) == chali::Status::Ok) return true;
    if (const auto *error = parser.lastError()) std::cerr << error->message() << '\n';
    return false;
}

/**
 * @brief 核对无参数组、空参数组及表达式模板在语法分析后保留准确结构。
 * @return 全部解析及结构检查通过时返回零。
 */
int main()
{
    int root = -1;
    if (!parse("$macro A\n42\n$end_macro\nint32 main() { return $A; }\n", root)) return 1;
    const auto first = M_ParsedNode(root).kids;
    if (first.size() != 2 || M_ParsedNode(first[0]).kind != MNode::MacroDefinition) return 2;
    if (M_ParsedNode(first[0]).kids[0] != -1) return 3;
    const auto &body = M_ParsedNode(M_ParsedNode(first[0]).kids[1]);
    if (body.kind != MNode::MacroTemplate || M_ParsedNode(body.kids[1]).i != 42) return 4;
    if (!parse("$macro B() // empty group\n42\n$end_macro\nint32 main() { return $B(); }\n", root)) return 5;
    const auto second = M_ParsedNode(root).kids;
    const int parameters = M_ParsedNode(second[0]).kids[0];
    if (parameters < 0 || M_ParsedNode(parameters).kind != MNode::MacroParameters || !M_ParsedNode(parameters).kids.empty()) return 6;
    if (!parse("$macro SQUARE(x)\nx * x\n$end_macro\nint32 main() { return $SQUARE(a + b); }\n", root)) return 7;
    const auto third = M_ParsedNode(root).kids;
    const auto &definition = M_ParsedNode(third[0]);
    const auto &product = M_ParsedNode(M_ParsedNode(definition.kids[1]).kids[1]);
    if (product.kind != MNode::Bin || product.s != "*") return 8;
    for (const auto child : product.kids)
        if (M_ParsedNode(child).kind != MNode::Var || M_ParsedNode(child).s != "x") return 9;
    chali::Engine invalid;
    if (invalid.runText("int32 main() { return $0; }", &root) == chali::Status::Ok) return 10;
    if (!parse("$macro ADD<T:a>(a, b)\na + b\n$end_macro\nint32 main() { return $ADD<int32>(1, 2); }\n", root)) return 11;
    const auto genericDefinition = M_ParsedNode(M_ParsedNode(root).kids[0]);
    const auto generics = M_ParsedNode(genericDefinition.kids[2]).kids;
    if (generics.size() != 1 || M_ParsedNode(generics[0]).kind != MNode::MacroGeneric || M_ParsedNode(generics[0]).s != "T" || M_ParsedNode(generics[0]).s2 != "a") return 12;
    for (const char *category : { "expr", "ident", "type", "block", "stmt" })
    {
        const auto source = Memory::String("$macro SELECT(A)\n$match A is ") + category + "\n1\n$else is ident\n2\n$else\n3\n$end_match\n$end_macro\nint32 main() { return 0; }\n";
        if (!parse(source, root)) return 13;
        const auto &definitionNode = M_ParsedNode(M_ParsedNode(root).kids[0]);
        const auto &templateNode = M_ParsedNode(definitionNode.kids[1]);
        const auto &match = M_ParsedNode(M_ParsedNode(templateNode.kids[0]).kids[0]);
        if (match.kind != MNode::MacroMatch || match.s != "A") return 14;
        const auto branches = M_ParsedNode(match.kids[0]).kids;
        if (branches.size() != 3 || M_ParsedNode(branches[0]).s != category || M_ParsedNode(branches[1]).s != "ident" || !M_ParsedNode(branches[2]).s.empty()) return 15;
    }
    if (!parse("$macro DIGITS(A)\n$regex A is \"[0-9]+\"\n1\n$else\n0\n$end_regex\n$end_macro\nint32 main() { return 0; }\n", root)) return 16;
    const auto &regexDefinition = M_ParsedNode(M_ParsedNode(root).kids[0]);
    const auto &regexTemplate = M_ParsedNode(regexDefinition.kids[1]);
    const auto &regex = M_ParsedNode(M_ParsedNode(regexTemplate.kids[0]).kids[0]);
    if (regex.kind != MNode::MacroRegex || regex.s != "A" || regex.s2 != "[0-9]+" || regex.kids[1] < 0) return 17;
    Memory::String unknown = "$macro BAD(A)\n$match A is token\n1\n$end_match\n$end_macro\n";
    chali_macro::markHeaderBoundaries(unknown);
    chali::Engine unknownCategory;
    if (unknownCategory.runText(unknown, &root) == chali::Status::Ok) return 18;
    if (!parse("$macro RAW(A)\n$regex A is \"[0-9]+\"\n\"digits\"\n$else\nis + 1\n$end_regex\n$end_macro\nint32 main() { return 0; }\n", root)) return 19;
    std::cout << "generated AST macro parser passed\n";
    return 0;
}
