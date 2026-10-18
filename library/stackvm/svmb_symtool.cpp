/**
 * @file svmb_symtool.cpp
 * @brief PC 端符号表查看、反查、转换及引用宽度统计。
 */
#include "stackvm.hpp"
#include "../../library/json/json.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
namespace
{
/**
 * @brief 读取受长度限制的 PC 映射。
 * @param path 文件。
 * @return 读取结果，失败为空。
 */
std::string readMap(const std::string &path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream || stream.tellg() < 0 || stream.tellg() > 512 * 1024 * 1024) return {};
    stream.seekg(0);
    return { std::istreambuf_iterator<char>(stream), {} };
}
/**
 * @brief 根据物理编号或原名查询。
 * @param query 查询。
 * @param path 映射。
 * @return 命中返回 0。
 */
int lookup(const std::string &query, const std::string &path)
{
    auto parsed = json::parse(readMap(path));
    if (!parsed.ok())
    {
        std::cerr << "映射 JSON 解析失败\n";
        return 1;
    }
    const auto *records = parsed.value->find("symbols");
    if (!records || !records->isArray())
    {
        std::cerr << "映射缺少 symbols 数组\n";
        return 1;
    }
    std::uint64_t index = 0;
    const auto converted = std::from_chars(query.data(), query.data() + query.size(), index);
    const bool byIndex = converted.ec == std::errc() && converted.ptr == query.data() + query.size();
    for (const auto &s : *records->asArray())
    {
        const auto *original = s.find("original"), *physical = s.find("physical_idx");
        const auto *logical = s.find("logical_id");
        if (!original || !original->isString() || !physical || !physical->isInt() || !logical || !logical->isString() || *physical->asInt() < 0)
        {
            std::cerr << "映射条目非法\n";
            return 1;
        }
        if (byIndex ? std::uint64_t(*physical->asInt()) == index : *original->asString() == query)
        {
            std::cout << s.dump() << '\n';
            return 0;
        }
    }
    std::cerr << "未找到符号\n";
    return 1;
}
}  // namespace
/**
 * @brief 符号工具入口。
 * @param argc 参数个数。
 * @param argv 参数。
 * @return 0 成功，1 失败，2 用法错误。
 */
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "svmb_symtool dump|stats FILE\n"
                     "svmb_symtool lookup INDEX|NAME [--map symbol_map.json]\n"
                     "svmb_symtool compress INPUT --output FILE --map FILE [--module NAME --version N]\n";
        return 2;
    }
    std::string command = argv[1], map = "symbol_map.json", output, module, version;
    for (int i = 3; i < argc; ++i)
    {
        const std::string option = argv[i];
        if (i + 1 >= argc)
        {
            std::cerr << "选项缺少取值\n";
            return 2;
        }
        const std::string value = argv[++i];
        if (option == "--map")
            map = value;
        else if (option == "--output")
            output = value;
        else if (option == "--module")
            module = value;
        else if (option == "--version")
            version = value;
        else
        {
            std::cerr << "未知选项：" << option << '\n';
            return 2;
        }
    }
    if (command == "lookup") return lookup(argv[2], map);
    stackvm::Program program;
    Memory::Vector<Memory::String> errors;
    auto diagnostics = [&] { for (const auto &e : errors) std::cerr << e << '\n'; };
    if (!stackvm::loadProgramBinary(argv[2], program, &errors))
    {
        diagnostics();
        return 1;
    }
    if (command == "compress")
    {
        if (output.empty())
        {
            std::cerr << "--output 必填\n";
            return 2;
        }
        const auto inputPath = std::filesystem::absolute(argv[2]).lexically_normal();
        const auto outputPath = std::filesystem::absolute(output).lexically_normal();
        const auto mapPath = std::filesystem::absolute(map).lexically_normal();
        if (inputPath == outputPath || inputPath == mapPath || outputPath == mapPath)
        {
            std::cerr << "输入、输出与映射路径必须不同\n";
            return 2;
        }
        if (!stackvm::saveSymbolMap(program, map, module, version, &errors) || !stackvm::saveProgramBinary(program, output, &errors, true))
        {
            diagnostics();
            return 1;
        }
        return 0;
    }
    if (!program.compactSymbols)
    {
        std::cerr << "需要压缩格式输入\n";
        return 1;
    }
    if (command == "dump")
    {
        for (const auto &s : program.symbols)
            std::cout << s.physicalIndex << " logical_id=" << s.logicalId << " kind=" << unsigned(s.kind)
                      << " flags=" << unsigned(s.flags) << " offset=" << s.offset << " size=" << s.size << '\n';
        return 0;
    }
    if (command == "stats")
    {
        std::uint64_t widths[11] = {}, refs = 0, bytes = 0;
        for (const auto &s : program.symbols)
        {
            std::uint64_t value = s.physicalIndex;
            unsigned width = 1;
            while (value >= 128)
            {
                value >>= 7;
                ++width;
            }
            widths[width] += s.refCount;
            refs += s.refCount;
            bytes += width * s.refCount;
        }
        std::cout << "file_bytes=" << std::filesystem::file_size(argv[2])
                  << " symbols=" << program.symbols.size() << " references=" << refs
                  << " reference_bytes=" << bytes << '\n';
        for (unsigned i = 1; i <= 10; ++i)
            if (i <= 3 || widths[i])
                std::cout << i << "-byte: " << widths[i] << " ("
                          << (refs ? 100.0 * widths[i] / refs : 0.0) << "%)\n";
        return 0;
    }
    std::cerr << "未知命令\n";
    return 2;
}
