/**
 * @file magel_macro_slots_test.cpp
 * @brief 宏临时变量编号、展开隔离与文本保护单元测试。
 */
#include "../source/magel_macro.hpp"
#include <cassert>

/**
 * @brief 验证零号临时变量与既有编号共存、多次和嵌套展开独立。
 * @return 成功返回 0。
 */
int main()
{
    Memory::String error;
    assert(magel_macro::gensymName("M", 0, "00010") == magel_macro::gensymName("M", 0, "+10"));
    assert(magel_macro::gensymName("M", 0, "-0") == magel_macro::gensymName("M", 0, "0"));
    assert(magel_macro::gensymName("M", 0, "-10") != magel_macro::gensymName("M", 0, "10"));
    preprocess::Result source;
    source.text = R"($macro INNER(x)
int64 $0=x; int64 $1=$0; int64 $9=$1; int64 $10=$0009; int64 $9999999999999999999999999999999999999999=$10; int64 $-123=$+10;
$end_macro
$macro OUTER(x)
int64 $0=x;
$INNER(x)
$0=$0+1;
$end_macro
$replace ONE(x) { int64 $0=x; }
$OUTER(7)
$OUTER(8)
$ONE(9)
"$0"; // $0
)";
    error.clear();
    assert(magel_macro::run(source, error));
    assert(source.text.find("__magel_macro_OUTER_0_0") != Memory::String::npos);
    assert(source.text.find("__magel_macro_INNER_1_0") != Memory::String::npos);
    assert(source.text.find("__magel_macro_OUTER_2_0") != Memory::String::npos);
    assert(source.text.find("__magel_macro_INNER_3_9") != Memory::String::npos);
    assert(source.text.find("__magel_macro_ONE_4_0") != Memory::String::npos);
    assert(source.text.find("\"$0\"; // $0") != Memory::String::npos);
    assert(source.text.find("__magel_macro_INNER_1_9999999999999999999999999999999999999999") != Memory::String::npos);
    assert(source.text.find("__magel_macro_INNER_1_negative_123") != Memory::String::npos);
    source.text = "int64 $9999999999999999999999999999=1;\n";
    error.clear();
    assert(!magel_macro::run(source, error));
    assert(error.find("只能出现在") != Memory::String::npos);
    return 0;
}
