/**
 * @file chali_lambda_test.cpp
 * @brief lambda 提升器的返回体分类与回调模板单元测试。
 */
#include "chali_lambda.hpp"
#include <cstdio>
#include <cstdlib>

/**
 * @brief 检查测试条件。
 * @param value 条件。
 * @param message 失败说明。
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
 * @brief 转换一个独立源片段。
 * @param source 源码。
 * @return 提升后的源码。
 */
Memory::String lower(const char *source)
{
    preprocess::Result result;
    result.text = source;
    Memory::String error;
    require(chali_lambda::run(result, error), error.c_str());
    return result.text;
}

/**
 * @brief 验证语义差异，而非只镜像实现分支。
 * @return 全部通过返回零。
 */
int main()
{
    auto text = lower("apply(fn(value) {value+1}, 2);");
    require(text.find("<McLambdaParameter0>") != Memory::String::npos, "内联回调未保留待推导参数");
    text = lower("module helpers; apply(fn(value){helper(value)},2); module __root;");
    require(text.find("module helpers;\nlet __chali_lambda_") != Memory::String::npos, "提升后的回调丢失声明模块");
    text = lower("action(void fn() {});");
    require(text == "action(void fn() {});", "显式签名应交给权威文法解析");
    preprocess::Result legacy;
    legacy.text = "action(fn(x) -> let {x});";
    Memory::String migration;
    require(!chali_lambda::run(legacy, migration) && migration.find("0.12.0") != Memory::String::npos,
            "省略形参类型的旧箭头 lambda 绕过了迁移检查");
    text = lower("apply(fn(x) {\"a;b\"}, 2);");
    require(text.find("return \"a;b\";") != Memory::String::npos, "字符串分号影响了函数体分类");
    text = lower("apply(fn(x) { /* ; return */ 7 }, 2);");
    require(text.find("return /* ; return */ 7;") != Memory::String::npos, "注释影响了表达式体分类");
    text = lower("apply(fn(x) {\n return x;\n}, 2);\nnext();");
    require(text.find("__chali_lambda_const_1\n\n, 2);\nnext();") != Memory::String::npos, "多行提升破坏原行数");
    preprocess::Result invalid;
    invalid.text = "let f=fn(int64 x){int64 y=x;};";
    Memory::String error;
    require(chali_lambda::run(invalid, error), "显式形参应交给文法及返回类型检查");
    require(invalid.text == "let f=fn(int64 x){int64 y=x;};", "预处理不应改写显式签名");
    std::puts("lambda unit tests OK");
    return 0;
}
