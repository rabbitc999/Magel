/**
 * @file magel_function_types_test.cpp
 * @brief 匿名函数类型在泛型与 lambda 扫描中的组合边界。
 */
#include "magel_generic.hpp"
#include "magel_lambda.hpp"
#include <cstdio>
#include <cstdlib>

/**
 * @brief 验证条件并输出失败原因。
 * @param value 条件。
 * @param message 原因。
 * @return 无。
 */
void require(bool value, const char *message)
{
    if (!value)
    {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

/**
 * @brief 验证嵌套类型和返回函数值的模板收集。
 * @return 全部通过返回零。
 */
int main()
{
    using namespace magel_generic;
    const Memory::String text = "<String const& fn(List<String>,bool fn(int64)),int64>tail";
    std::size_t close = 0;
    require(balanced(text, 0, '<', '>', close) && text.substr(close + 1u) == "tail", "嵌套函数类型截断泛型实参");
    const auto parts = splitTop(text.substr(1, close - 1));
    require(parts.size() == 2u && parts[1] == "int64", "嵌套函数类型的逗号分隔失败");
    coreAliasShadows.clear();
    require(canonicalType("int64 fn(String, bool fn(int64))") == "int64 fn(std::lang::String,bool fn(int64))",
            "嵌套函数类型没有生成可重新解析的前置签名");
    require(canonicalType("String & fn(List<String>, String fn(String &))") == "std::lang::String const& fn(std::lang::List<std::lang::String>,std::lang::String fn(std::lang::String const&))",
            "函数参数或返回值未递归归一字符串与引用身份");
    require(magel_lambda::split("int64 fn(int64) callback, int64 value").size() == 2u,
            "lambda 函数类型形参未正确分隔");
    require(canonicalType("(String fn(String)) &") == "(std::lang::String fn(std::lang::String)) const&", "函数值引用未保留括号或归一叶子类型");
    Memory::String source = "T fn(T) make<T>(T value){return fn(T x){x+value};}";
    normalizeGenericSyntax(source);
    Memory::Map<Memory::String, Generic> generics;
    Memory::String error;
    require(collect(source, {}, generics, error), error.c_str());
    require(generics.count("make") == 1u && generics.at("make").declaration.find("T fn(T) make") == 0u,
            "泛型函数返回函数值时错误定位声明名称");
    std::puts("function type unit tests OK");
    return 0;
}
