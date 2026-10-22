#include "syntax/source_text.hpp"
#include "syntax/constant_expression.hpp"
#include <iostream>

/**
 * @brief 验证从旧宏引擎剥离的普通源码分组及 const 泛型求值保持独立可用。
 * @return 所有常量、短路、溢出及分组检查通过时返回零。
 */
int main()
{
    Memory::Map<Memory::String, Memory::String> values{ { "N", "40" }, { "M", "N + 2" } };
    Memory::String error;
    if (chali_syntax::evaluateConstant("M", values, error).number != 42 || !error.empty()) return 1;
    if (chali_syntax::evaluateConstant("false && (1 / 0)", values, error).truth() || !error.empty()) return 2;
    if (!chali_syntax::evaluateConstant("true || missing", values, error).truth() || !error.empty()) return 3;
    chali_syntax::evaluateConstant("9223372036854775808", values, error);
    if (error.empty()) return 4;
    error.clear();
    values["N"] = "N";
    chali_syntax::evaluateConstant("N", values, error);
    if (error.empty()) return 5;
    Memory::Vector<Memory::String> arguments;
    std::size_t end = 0;
    if (!chali_syntax::groupArguments("(\"a,b\", nested(1,2), /*,*/ value)", 0, arguments, end) || arguments.size() != 3) return 6;
    if (arguments[0] != "\"a,b\"" || arguments[1] != " nested(1,2)" || arguments[2] != " /*,*/ value") return 7;
    std::cout << "source grouping and const expressions passed\n";
    return 0;
}
