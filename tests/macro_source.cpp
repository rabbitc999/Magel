#include "macro/source.hpp"
#include <algorithm>
#include <iostream>

/**
 * @brief 验证宏头词法边界标记保留宏体、注释、字符串及源码行数。
 * @return 所有边界保持规则通过时返回零。
 */
int main()
{
    Memory::String source = "$macro A() // declaration\nvalue + 1\n$end_macro\n"
                            "// $macro fake()\n/* $macro fake()\n */\n"
                            "String s = \"$macro fake()\";\n"
                            "$macro B\n{ int32 local = 1; }\n$end_macro\n";
    const auto original = source;
    chali_macro::markHeaderBoundaries(source);
    if (source.find("$macro A()  @!macrobody // declaration\n") == Memory::String::npos) return 1;
    if (source.find("$macro B @!macrobody \n") == Memory::String::npos) return 2;
    if (source.find("value + 1\n$end_macro") == Memory::String::npos) return 3;
    if (source.find("// $macro fake()\n/* $macro fake()\n */\nString s = \"$macro fake()\";\n") == Memory::String::npos) return 4;
    if (std::count(source.begin(), source.end(), '\n') != std::count(original.begin(), original.end(), '\n')) return 5;
    Memory::String branches = "$regex A is \"[0-9]+\"\n\"yes\"\n$else\nis + 1\n$end_regex\n";
    chali_macro::markHeaderBoundaries(branches);
    if (branches.find("$regex A is \"[0-9]+\" @!macrobody \n\"yes\"") == Memory::String::npos) return 6;
    if (branches.find("$else @!macrobody \nis + 1") == Memory::String::npos) return 7;
    chali_macro::ProtectedDefinitions definitions;
    Memory::String protectedSource = original;
    std::string error;
    if (!definitions.hide(protectedSource, error) || !error.empty()) return 8;
    if (protectedSource.find("value + 1") != Memory::String::npos || protectedSource.find("$macro A") != Memory::String::npos) return 9;
    if (std::count(protectedSource.begin(), protectedSource.end(), '\n') != std::count(original.begin(), original.end(), '\n')) return 10;
    protectedSource = "// relocated\n" + protectedSource + "\nint64 generated() { return 1; }\n";
    if (!definitions.restore(protectedSource, error) || protectedSource != "// relocated\n" + original + "\nint64 generated() { return 1; }\n") return 11;
    Memory::String replacement = "$replace ADD<T:a>(a, b) a + b // comment\n$replace URL() \"https://example\"\n";
    if (!chali_macro::markReplacementBoundaries(replacement, error)) return 12;
    if (replacement.find("$replace ADD<T:a>(a, b)  @!macrobody a + b  $end_macro // comment") == Memory::String::npos) return 13;
    if (replacement.find("\"https://example\" $end_macro") == Memory::String::npos) return 14;
    std::cout << "macro declaration boundaries passed\n";
    return 0;
}
