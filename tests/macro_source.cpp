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
    magel_macro::markHeaderBoundaries(source);
    if (source.find("$macro A()  @!macrobody // declaration\n") == Memory::String::npos) return 1;
    if (source.find("$macro B @!macrobody \n") == Memory::String::npos) return 2;
    if (source.find("value + 1\n$end_macro") == Memory::String::npos) return 3;
    if (source.find("// $macro fake()\n/* $macro fake()\n */\nString s = \"$macro fake()\";\n") == Memory::String::npos) return 4;
    if (std::count(source.begin(), source.end(), '\n') != std::count(original.begin(), original.end(), '\n')) return 5;
    Memory::String branches = "$regex A is \"[0-9]+\"\n\"yes\"\n$else\nis + 1\n$end_regex\n";
    magel_macro::markHeaderBoundaries(branches);
    if (branches.find("$regex A is \"[0-9]+\" @!macrobody \n\"yes\"") == Memory::String::npos) return 6;
    if (branches.find("$else @!macrobody \nis + 1") == Memory::String::npos) return 7;
    std::cout << "macro declaration boundaries passed\n";
    return 0;
}
