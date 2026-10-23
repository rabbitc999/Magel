/**
 * @file compiler_native_safety.cpp
 * @brief 验证编译器数据导出、大数边界和结构化文档模板的安全回归。
 */
#include "../source/bignum/bigfloat.hpp"
#include "../source/simple_ast/simple_ast.hpp"
#include "../source/json/json.hpp"
#include "../source/toml/toml.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

/**
 * @brief 从导出的标量数据区还原八字节无符号位模式。
 * @param literal 待导出的十进制整数文本。
 * @return 数据区的第一个小端八字节整数。
 */
std::uint64_t exportedInteger(const char *literal)
{
    simple_ast::CodegenResult result;
    result.entries.push_back({ "number", simple_ast::Type::u64(), literal });
    const auto bytes = result.exportDataBinary();
    assert(bytes.size() == 24);
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8; ++index)
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[index + 8])) << (index * 8);
    return value;
}

/**
 * @brief 检查一项大数边界操作是否以明确溢出异常结束。
 * @param operation 待执行的边界运算。
 */
template <typename Operation>
void expectOverflow(Operation operation)
{
    bool caught = false;
    try
    {
        operation();
    } catch (const std::overflow_error &)
    {
        caught = true;
    }
    assert(caught);
}

/**
 * @brief 检查整数精确导出、浮点范围、大数参数和 JSON/TOML 模板实例化。
 * @return 所有断言通过时返回零。
 */
int main()
{
    assert(exportedInteger("9007199254740993") == 9007199254740993ull);
    assert(exportedInteger("9223372036854775807") == 9223372036854775807ull);
    assert(exportedInteger("-9223372036854775808") == 0x8000000000000000ull);
    assert(exportedInteger("18446744073709551615") == std::numeric_limits<std::uint64_t>::max());
    assert(exportedInteger("-1") == std::numeric_limits<std::uint64_t>::max());
    simple_ast::CodegenResult data;
    data.entries.push_back({ "large", simple_ast::Type::f64(), "1e300" });
    const auto bytes = data.exportDataBinary();
    double value = 0;
    std::memcpy(&value, bytes.data() + 8, sizeof(value));
    assert(value == 1e300);
    using bigfloat::BigFloat;
    expectOverflow([] { (void)BigFloat("1e1000000").pow(9223372036854775807ull); });
    expectOverflow([] { (void)BigFloat("1e-1000000").pow(std::numeric_limits<std::uint64_t>::max()); });
    expectOverflow([] { (void)BigFloat("1").root(3, std::numeric_limits<int>::max()); });
    assert(BigFloat("1").pow(std::numeric_limits<std::uint64_t>::max()).toString() == "1");
    assert(BigFloat("1").root(std::numeric_limits<std::uint64_t>::max(), 16).toString() == "1");
    assert(BigFloat("1e2").pow(3).toString() == "1000000");
    assert(BigFloat("1e-2").pow(3).toString() == "1e-6");
    assert(BigFloat(static_cast<long>(42)).toString() == "42");
    assert(BigFloat(std::numeric_limits<std::int64_t>::min()).toPlainString(0) == "-9223372036854775808");
    json::Object jsonObject;
    jsonObject.emplace("value", std::int64_t(42));
    assert(jsonObject.tryGet("value")->asInt().value() == 42);
    toml::Object tomlObject;
    tomlObject.emplace("value", std::int64_t(42));
    assert(tomlObject.tryGet("value")->asInt().value() == 42);
    std::cout << "native adversarial boundaries passed\n";
    return 0;
}
