/**
 * @file test_symbols.cpp
 * @brief 变长符号格式、运行等价性与规模回归。
 */
#include "stackvm.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <cstdlib>
namespace
{
/**
 * @brief 断言失败立即报告。
 * @param ok 条件。
 * @param line 行。
 */
void check(bool ok, int line)
{
    if (!ok)
    {
        std::cerr << "断言失败：" << line << '\n';
        std::exit(1);
    }
}
#define CHECK(x) check((x), __LINE__)
/**
 * @brief 读取文件。
 * @param path 路径。
 * @return 字节。
 */
std::vector<std::uint8_t> bytes(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    return { std::istreambuf_iterator<char>(in), {} };
}
/**
 * @brief 写异常测试文件并重算 CRC，验证深层格式检查。
 * @param data 内容。
 * @param path 路径。
 */
void corrupt(std::vector<std::uint8_t> data, const std::string &path)
{
    auto crc = stackvm::symbols::crc32(data.data() + 44, data.size() - 44);
    for (unsigned i = 0; i < 4; ++i) data[40 + i] = static_cast<std::uint8_t>(crc >> (8 * i));
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(data.data()), data.size());
}
/**
 * @brief 验证真实组装程序压缩前后的输出与宿主调用一致。
 * @param directory 临时目录。
 */
void execution(const std::string &directory)
{
    const auto quads = stackvm::parseQuads(
        "(decl, i32, global, calculate_result)\n"
        "(:=, 41, _, calculate_result)\n"
        "(param, calculate_result, _, value)\n"
        "(call, host::increment, 1, calculate_result)\n"
        "(write, calculate_result, _, _)\n(halt, _, _, _)\n",
        "test.quad");
    stackvm::Program p, loaded;
    CHECK(stackvm::assemble(quads, p));
    stackvm::NativeRegistry registry;
    CHECK(registry.add("host::increment", 1, [](stackvm::NativeCallContext &, const Memory::Vector<stackvm::NativeValue> &args) {
        return stackvm::NativeValue::fromInt(args[0].integer + 1);
    }));
    stackvm::Machine before, after;
    before.setNativeRegistry(&registry);
    after.setNativeRegistry(&registry);
    const auto result = before.run(p);
    const auto path = directory + "/execution.svmb";
    CHECK(stackvm::saveProgramBinary(p, path, nullptr, true));
    CHECK(stackvm::loadProgramBinary(path, loaded));
    CHECK(loaded.compactSymbols);
    const auto converted = after.run(loaded);
    CHECK(result.ok);
    CHECK(converted.ok);
    CHECK(result.text == converted.text);
    CHECK(result.output == converted.output);
    auto binary = bytes(path);
    CHECK(std::string(binary.begin(), binary.end()).find("calculate_result") == std::string::npos);
    CHECK(loaded.debug.empty());
    auto bad = binary;
    bad[32] = 0;
    corrupt(bad, directory + "/bad.svmb");
    CHECK(!stackvm::loadProgramBinary(directory + "/bad.svmb", loaded));
    bad = binary;
    bad.pop_back();
    corrupt(bad, directory + "/bad.svmb");
    CHECK(!stackvm::loadProgramBinary(directory + "/bad.svmb", loaded));
}

/**
 * @brief 验证数据区原名绑定、函数指针及内建函数编号调用。
 * @param directory 临时目录。
 */
void indirect(const std::string &directory)
{
    stackvm::Program p, loaded;
    p.name = "indirect";
    p.code.resize(10);
    p.code[0].op = stackvm::Op::AddrOf;
    p.code[0].name = "callback_address";
    p.code[1].op = stackvm::Op::CallIndirect;
    p.code[1].name2 = "calculate_result";
    p.code[2].op = stackvm::Op::Load;
    p.code[2].name = "calculate_result";
    p.code[3].op = stackvm::Op::Print;
    p.code[4].op = stackvm::Op::PushText;
    p.code[4].textValue = "preserved string";
    p.code[5].op = stackvm::Op::PrintText;
    p.code[6].op = stackvm::Op::Call;
    p.code[6].name = "test_native";
    p.code[6].name2 = "length";
    p.code[7].op = stackvm::Op::Halt;
    p.code[8].op = stackvm::Op::PushInt;
    p.code[8].intValue = 42;
    p.code[9].op = stackvm::Op::Ret;
    p.labels["calculate_callback"] = 8;
    p.parameters["calculate_callback"] = {};
    for (std::size_t i = 0; i < p.code.size(); ++i) p.code[i].quadIndex = i;
    stackvm::DataArea data;
    const Memory::String name = "calculate_callback";
    data.binary.push_back(0);
    data.binary.insert(data.binary.end(), name.begin(), name.end());
    data.binary.push_back(0);
    data.symbols["callback_address"] = 1;
    stackvm::NativeRegistry registry;
    registry.add("test_native", 0, [](stackvm::NativeCallContext &, const Memory::Vector<stackvm::NativeValue> &) {
        return stackvm::NativeValue::fromInt(0);
    });
    stackvm::Machine before, after;
    before.setNativeRegistry(&registry);
    after.setNativeRegistry(&registry);
    before.setData(&data);
    after.setData(&data);
    const auto expected = before.run(p);
    CHECK(stackvm::saveProgramBinary(p, directory + "/indirect.svmb", nullptr, true));
    CHECK(stackvm::loadProgramBinary(directory + "/indirect.svmb", loaded));
    const auto actual = after.run(loaded);
    if (!expected.ok) std::cerr << expected.error << '\n';
    if (!actual.ok) std::cerr << actual.error << '\n';
    CHECK(expected.ok && actual.ok);
    CHECK(expected.output == "42");
    CHECK(expected.output == actual.output);
    CHECK(expected.text == actual.text);
}

/**
 * @brief 生成规模样本并校验逻辑 ID 与物理索引。
 * @param n 符号数。
 * @param directory 输出目录。
 */
void fixture(std::size_t n, const std::string &directory)
{
    stackvm::Program p;
    p.name = "benchmark";
    auto append = [&](std::size_t symbol) {
        stackvm::Instruction i;
        i.op = stackvm::Op::Load;
        i.name = "calculate_symbol_" + Memory::toString(symbol);
        i.quadIndex = p.code.size();
        p.code.push_back(std::move(i));
    };
    for (std::size_t i = 0; i < 4 * n; ++i) append(0);
    for (std::size_t i = 0; i < n; ++i) append(i);
    const auto prefix = directory + "/" + std::to_string(n);
    CHECK(stackvm::saveProgramBinary(p, prefix + ".raw.svmb"));
    CHECK(stackvm::saveSymbolMap(p, prefix + ".map.json", p.name, "1"));
    CHECK(stackvm::saveProgramBinary(p, prefix + ".svmb", nullptr, true));
    CHECK(stackvm::saveProgramBinary(p, prefix + ".again.svmb", nullptr, true));
    CHECK(bytes(prefix + ".svmb") == bytes(prefix + ".again.svmb"));
    stackvm::Program loaded;
    CHECK(stackvm::loadProgramBinary(prefix + ".svmb", loaded));
    CHECK(loaded.symbols.size() == n);
    CHECK(loaded.symbols[0].physicalIndex == 0);
    CHECK(loaded.symbols[0].refCount == 4 * n + 1);
    for (const auto &s : loaded.symbols) CHECK(s.physicalIndex < n);
    const auto old = loaded.symbols[0].logicalId;
    append(n);
    CHECK(stackvm::saveProgramBinary(p, prefix + ".incremental.svmb", nullptr, true));
    CHECK(stackvm::loadProgramBinary(prefix + ".incremental.svmb", loaded));
    CHECK(loaded.symbols[0].logicalId == old);
    if (n == 10) CHECK(bytes(prefix + ".svmb").size() * 10 < bytes(prefix + ".raw.svmb").size() * 7);
}
}  // namespace
/**
 * @brief 执行全部压缩回归。
 * @param argc 参数数。
 * @param argv 输出目录。
 * @return 结果。
 */
int main(int argc, char **argv)
{
    const std::string directory = argc > 1 ? argv[1] : "symbol-test";
    std::filesystem::create_directories(directory);
    for (std::uint64_t value : { 0ull, 127ull, 128ull, 16383ull, 16384ull, 4294967295ull,
                                 9223372036854775807ull, 18446744073709551615ull })
    {
        std::vector<std::uint8_t> data;
        stackvm::symbols::writeVarUInt(data, value);
        std::size_t pos = 0;
        std::uint64_t decoded = 42;
        CHECK(stackvm::symbols::readVarUInt(data.data(), data.size(), pos, decoded) == stackvm::symbols::Error::Ok);
        CHECK(decoded == value);
        CHECK(pos == data.size());
    }
    for (const auto &data : std::vector<std::vector<std::uint8_t>>{
             { 128 }, { 128, 0 }, { 255, 255, 255, 255, 255, 255, 255, 255, 255, 2 }, { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 0 } })
    {
        std::size_t pos = 0;
        std::uint64_t value = 9;
        CHECK(stackvm::symbols::readVarUInt(data.data(), data.size(), pos, value) != stackvm::symbols::Error::Ok);
        CHECK(pos == 0 && value == 9);
    }
    execution(directory);
    indirect(directory);
    for (std::size_t n : { 10u, 1000u, 100000u }) fixture(n, directory);
    std::cout << "符号压缩回归全部通过\n";
}
