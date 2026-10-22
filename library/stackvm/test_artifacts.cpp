/**
 * @file test_artifacts.cpp
 * @brief 二进制中间产物的等价性、损坏输入与解码耗时对照。
 */
#include "stackvm.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstdlib>
/**
 * @brief 执行断言。
 * @param ok 条件。
 * @param line 源行。
 */
void require(bool ok, int line)
{
    if (!ok)
    {
        std::cerr << "断言失败：" << line << '\n';
        std::exit(1);
    }
}
#define REQUIRE(x) require((x), __LINE__)
/**
 * @brief 验证结构与诊断完整保留，并输出可复现的体积和解码耗时。
 * @param argc 参数数目。
 * @param argv 可选文本四元式、布局路径。
 * @return 状态码。
 */
int main(int argc, char **argv)
{
    Memory::String text = "# 名称: artifact\n# 输入: x\n# chali-source: 12 \"中文.chl\"\n0: (write, 42, _, _)\n1: (halt, _, _, _)\n";
    Memory::String layout = "message offset=0 size=8 type=i64 stride=8\n";
    if (argc > 1)
    {
        std::ifstream f(argv[1], std::ios::binary);
        REQUIRE(bool(f));
        text.assign(std::istreambuf_iterator<char>(f), {});
    }
    if (argc > 2)
    {
        std::ifstream f(argv[2], std::ios::binary);
        REQUIRE(bool(f));
        layout.assign(std::istreambuf_iterator<char>(f), {});
    }
    const auto original = stackvm::parseQuads(text, "source.quad");
    REQUIRE(original.ok());
    const auto binary = stackvm::encodeQuadBinary(original);
    REQUIRE(!binary.empty());
    const auto decoded = stackvm::parseQuads(binary, "binary.quad");
    REQUIRE(decoded.ok());
    REQUIRE(stackvm::encodeQuadBinary(decoded) == binary);
    REQUIRE(decoded.name == original.name && decoded.input == original.input && decoded.expected == original.expected && decoded.expectedText == original.expectedText);
    REQUIRE(decoded.quads.size() == original.quads.size());
    for (std::size_t i = 0; i < original.quads.size(); ++i)
    {
        const auto &a = original.quads[i], &b = decoded.quads[i];
        REQUIRE(a.line == b.line && a.sourceLine == b.sourceLine && a.sourceFile == b.sourceFile && a.op == b.op && a.args == b.args);
    }
    stackvm::Program a, b;
    REQUIRE(stackvm::assemble(original, a));
    REQUIRE(stackvm::assemble(decoded, b));
    REQUIRE(a.code.size() == b.code.size());
    for (std::size_t i = 0; i < a.code.size(); ++i) REQUIRE(a.code[i].toString() == b.code[i].toString());
    for (std::size_t n : { std::size_t(4), std::size_t(5), binary.size() / 2, binary.size() - 1 }) REQUIRE(!stackvm::parseQuads(std::string_view(binary.data(), n), "truncated").ok());
    auto corrupt = binary;
    corrupt[4] = 99;
    REQUIRE(!stackvm::parseQuads(corrupt, "version").ok());
    REQUIRE(!stackvm::parseQuads(binary + "x", "trailing").ok());
    corrupt = binary;
    corrupt.back() = char(255);
    REQUIRE(!stackvm::parseQuads(corrupt, "reference").ok());
    Memory::Vector<stackvm::LayoutRow> rows, restored;
    REQUIRE(stackvm::decodeDataLayout(layout, rows));
    const auto packed = stackvm::encodeDataLayout(rows);
    REQUIRE(stackvm::decodeDataLayout(packed, restored));
    REQUIRE(stackvm::encodeDataLayout(restored) == packed);
    REQUIRE(!stackvm::decodeDataLayout(packed.substr(0, packed.size() - 1), restored));
    REQUIRE(!stackvm::decodeDataLayout(packed + "x", restored));
    corrupt = packed;
    corrupt[4] = 99;
    REQUIRE(!stackvm::decodeDataLayout(corrupt, restored));
    using Clock = std::chrono::steady_clock;
    const auto bench = [&](const Memory::String &input, bool isLayout) {
        const auto start = Clock::now();
        std::size_t iterations = 0;
        do
        {
            if (isLayout)
            {
                Memory::Vector<stackvm::LayoutRow> r;
                REQUIRE(stackvm::decodeDataLayout(input, r));
            }
            else
                REQUIRE(stackvm::parseQuads(input, "benchmark").ok());
            ++iterations;
        } while (Clock::now() - start < std::chrono::milliseconds(250));
        return std::chrono::duration<double, std::micro>(Clock::now() - start).count() / iterations;
    };
    std::cout << "quad_text_bytes=" << text.size() << " quad_binary_bytes=" << binary.size()
              << " layout_text_bytes=" << layout.size() << " layout_binary_bytes=" << packed.size()
              << "\nquad_text_us=" << bench(text, false) << " quad_binary_us=" << bench(binary, false)
              << " layout_text_us=" << bench(layout, true) << " layout_binary_us=" << bench(packed, true) << '\n';
}
