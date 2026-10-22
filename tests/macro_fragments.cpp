#include "macro/fragments.hpp"
#include <iostream>

/**
 * @brief 验证词法片段捕获保留原文、括号种类、空组和精确行列，不展开代码。
 * @return 全部结构及回滚检查通过时返回零。
 */
int main()
{
    chali_macro::SourceFragments fragments;
    Memory::String source = "$macro A(x)\nx\n$end_macro\nvoid main() { $A(a + /* raw */ b); }\n";
    const auto original = source;
    std::string error;
    if (!fragments.capture(source, error) || !error.empty()) return 1;
    if (source.size() != original.size()) return 2;
    const auto index = fragments.find(4, 15);
    if (index == static_cast<std::size_t>(-1)) return 3;
    const auto &call = fragments.at(index);
    if (call.name != "A" || call.groups.size() != 1 || call.groups[0].opening != '(' || call.groups[0].arguments.size() != 1) return 4;
    if (call.groups[0].arguments[0].text != "a + /* raw */ b" || call.groups[0].arguments[0].line != 4 || call.groups[0].arguments[0].column != 18) return 5;
    Memory::String multiple = "$F<int64>[name](\"a,b\", $OTHER(1, 2)){ { a(); } }";
    if (!fragments.capture(multiple, error)) return 6;
    const auto &grouped = fragments.at(fragments.find(1, 1));
    if (!grouped.explicitTypes || grouped.types.size() != 1 || grouped.types[0].text != "int64" || grouped.groups.size() != 3) return 7;
    if (grouped.groups[1].arguments.size() != 2 || grouped.groups[1].arguments[0].text != "\"a,b\"" || grouped.groups[1].arguments[1].text != " $OTHER(1, 2)") return 8;
    Memory::String empty = "$EMPTY( /* comment */ )";
    if (!fragments.capture(empty, error) || !fragments.at(fragments.find(1, 1)).groups[0].arguments.empty()) return 9;
    Memory::String literal = "\"$A(1)\" /* $B(2) */ // $C(3)\n";
    const auto unchanged = literal;
    if (!fragments.capture(literal, error) || literal != unchanged || fragments.find(1, 2) != static_cast<std::size_t>(-1)) return 10;
    Memory::String multiline = "$M(\n value,\n other\n); next();";
    const auto before = multiline;
    if (!fragments.capture(multiline, error, 20, 3) || multiline.size() != before.size()) return 11;
    const auto &lines = fragments.at(fragments.find(20, 3));
    if (lines.groups[0].arguments.size() != 2 || lines.groups[0].arguments[1].text != "\n other\n") return 12;
    Memory::String invalid = "$M(1]";
    const auto bad = invalid;
    if (fragments.capture(invalid, error) || error.empty() || invalid != bad) return 13;
    fragments.clear();
    Memory::String anchored = "$A( a + b ); $A(2)";
    if (!fragments.capture(anchored, error, 1, 1, true)) return 14;
    if (fragments.resolveMarker("$A@!0") != 0 || fragments.resolveMarker("$A@!1") != 1 || fragments.resolveMarker("$B@!0") != static_cast<std::size_t>(-1)) return 15;
    const auto markerText = anchored;
    if (!fragments.capture(anchored, error, 1, 1, true) || anchored != markerText) return 16;
    fragments.rebase(100);
    if (fragments.at(0).line != 101 || fragments.at(0).groups[0].arguments[0].line != 101) return 17;
    if (fragments.resolveMarker("$A@!9999999999999999999999999999999") != static_cast<std::size_t>(-1)) return 18;
    std::cout << "macro original fragments passed\n";
    return 0;
}
