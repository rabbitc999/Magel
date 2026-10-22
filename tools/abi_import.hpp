#pragma once
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <climits>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace chali_abi_import
{
/**
 * @brief 去掉声明两端的空白。
 * @param text 声明文本。
 * @return 去除两端空白的结果。
 */
inline std::string trim(const std::string &text)
{
    const auto first = text.find_first_not_of(" \r\n\t");
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \r\n\t") - first + 1);
}
/**
 * @brief 转义生成的 ChaLi 字符串字面量。
 * @param text 库路径或符号。
 * @return 含引号的字面量。
 */
inline std::string quoted(const std::string &text)
{
    std::string out = "\"";
    for (char c : text)
    {
        if (c == '\\' || c == '"') out += '\\';
        out += c;
    }
    return out + '"';
}
/**
 * @brief 映射明确支持的 C 类型；未知布局和指针不会被猜测为整数。
 * @param value C 类型文本。
 * @param records 已登记的结构体和枚举类型。
 * @return ChaLi 类型；不支持时返回空串。
 */
inline std::string type(std::string value, const std::set<std::string> &records, const std::map<std::string, std::string> &aliases)
{
    value = trim(std::regex_replace(value, std::regex("[ \\t\\r\\n]+"), " "));
    static const std::map<std::string, std::string> scalar = {
        { "void", "void" }, { "bool", "bool" }, { "_Bool", "bool" }, { "int8_t", "int8" }, { "uint8_t", "uint8" }, { "int16_t", "int16" }, { "uint16_t", "uint16" }, { "int32_t", "int32" }, { "uint32_t", "uint32" }, { "int64_t", "int64" }, { "uint64_t", "uint64" }, { "float", "float32" }, { "double", "float64" }
    };
    if (value.rfind("const ", 0) == 0) value = trim(value.substr(6));
    const auto alias = aliases.find(value);
    if (alias != aliases.end()) return alias->second;
    const auto found = scalar.find(value);
    if (found != scalar.end()) return found->second;
    if (value.rfind("struct ", 0) == 0) value = trim(value.substr(7));
    if (value.rfind("enum ", 0) == 0) value = trim(value.substr(5));
    return records.count(value) ? "C_" + value : std::string();
}
/**
 * @brief 将固定宽度标量、简单值结构体、整数枚举和函数原型转换为 ABI 声明。
 * @param argc 命令行参数数量。
 * @param argv 完整命令行，前三项为程序、gen 和 abi。
 * @return 成功为 0，遇到不支持的 C 结构时为非零且不输出半成品。
 */
inline int run(int argc, char **argv)
{
    if (argc < 5)
    {
        std::cerr << "用法：chali gen abi HEADER --library LIBRARY --out FILE.chl\n";
        return 2;
    }
    std::string library, output;
    for (int i = 4; i < argc; ++i)
    {
        const std::string option = argv[i];
        if (i + 1 >= argc || (option != "--library" && option != "--out"))
        {
            std::cerr << "未知选项或缺少值：" << option << '\n';
            return 2;
        }
        (option == "--library" ? library : output) = argv[++i];
    }
    if (library.empty() || output.empty())
    {
        std::cerr << "必须指定 --library 和 --out\n";
        return 2;
    }
    std::ifstream input(std::filesystem::u8path(argv[3]), std::ios::binary);
    if (!input)
    {
        std::cerr << "无法读取 C 头文件\n";
        return 1;
    }
    std::string text((std::istreambuf_iterator<char>(input)), {});
    text = std::regex_replace(text, std::regex("/\\*[\\s\\S]*?\\*/|//[^\\n]*"), " ");
    std::istringstream lines(text);
    std::string line, declarations;
    std::set<std::string> guards;
    bool guardDefined = false, guardClosed = false;
    while (std::getline(lines, line))
    {
        const auto clean = trim(line);
        if (clean.empty()) continue;
        if (guardClosed)
        {
            std::cerr << "头文件保护结束后存在声明\n";
            return 1;
        }
        if (clean[0] == '#')
        {
            std::smatch match;
            if (std::regex_match(clean, std::regex("#\\s*include\\s*<(stdint|stdbool|stddef)\\.h>")) || std::regex_match(clean, std::regex("#\\s*pragma\\s+once"))) continue;
            if (std::regex_match(clean, match, std::regex("#\\s*ifndef\\s+([A-Za-z_][A-Za-z0-9_]*)")))
            {
                if (!guards.empty() || !declarations.empty())
                {
                    std::cerr << "不支持条件 C 声明\n";
                    return 1;
                }
                guards.insert(match[1]);
                continue;
            }
            if (std::regex_match(clean, match, std::regex("#\\s*define\\s+([A-Za-z_][A-Za-z0-9_]*)")) && guards.count(match[1]))
            {
                guardDefined = true;
                continue;
            }
            if (std::regex_match(clean, std::regex("#\\s*endif")) && guardDefined)
            {
                guardClosed = true;
                continue;
            }
            std::cerr << "C 头文件导入暂不支持此预处理指令：" << clean << '\n';
            return 1;
        }
        declarations += line + '\n';
    }
    if (!guards.empty() && (!guardDefined || !guardClosed))
    {
        std::cerr << "头文件保护不完整\n";
        return 1;
    }
    std::vector<std::string> units;
    std::size_t start = 0;
    int braces = 0;
    for (std::size_t i = 0; i < declarations.size(); ++i)
    {
        if (declarations[i] == '{') ++braces;
        if (declarations[i] == '}') --braces;
        if (braces < 0)
        {
            std::cerr << "C 头文件花括号不匹配\n";
            return 1;
        }
        if (!braces && declarations[i] == ';')
        {
            units.push_back(trim(declarations.substr(start, i - start)));
            start = i + 1;
        }
    }
    if (braces || !trim(declarations.substr(start)).empty())
    {
        std::cerr << "C 声明不完整或缺少分号\n";
        return 1;
    }
    std::string recordsText, functions;
    std::set<std::string> records;
    std::map<std::string, std::string> aliases;
    for (const auto &unit : units)
    {
        std::smatch record;
        if (unit.size() > 16384)
        {
            std::cerr << "C 声明过大，请拆分包装头文件\n";
            return 1;
        }
        if (std::regex_match(unit, record, std::regex("typedef\\s+(.+?)\\s*\\(\\s*\\*\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*\\)\\s*\\(([^()]*)\\)")))
        {
            const auto result = type(record[1], records, aliases);
            if (result.empty() || result.find(" fn(") != std::string::npos || result.rfind("C_", 0) == 0)
            {
                std::cerr << "回调 typedef 只能返回标量或 void\n";
                return 1;
            }
            std::string signature = result + " fn(";
            std::istringstream args(record[3]);
            bool first = true;
            while (std::getline(args, line, ','))
            {
                line = trim(line);
                if (line.empty() || line == "void") continue;
                auto mapped = type(line, records, aliases);
                std::smatch parameter;
                if (mapped.empty() && std::regex_match(line, parameter, std::regex("(.+?)\\s+([A-Za-z_][A-Za-z0-9_]*)"))) mapped = type(parameter[1], records, aliases);
                if (mapped.empty() || mapped == "void" || mapped.find(" fn(") != std::string::npos || mapped.rfind("C_", 0) == 0)
                {
                    std::cerr << "回调 typedef 只能接受固定标量参数\n";
                    return 1;
                }
                if (!first) signature += ", ";
                first = false;
                signature += mapped;
            }
            aliases[record[2]] = signature + ")";
            continue;
        }
        if (std::regex_match(unit, record, std::regex("typedef\\s+(.+?)\\s+([A-Za-z_][A-Za-z0-9_]*)")))
        {
            const auto mapped = type(record[1], records, aliases);
            if (!mapped.empty())
            {
                aliases[record[2]] = mapped;
                continue;
            }
        }
        if (std::regex_match(unit, record, std::regex("(?:typedef\\s+)?struct\\s*([A-Za-z_][A-Za-z0-9_]*)?\\s*\\{([\\s\\S]*)\\}\\s*([A-Za-z_][A-Za-z0-9_]*)?")))
        {
            const std::string name = record[3].matched ? record[3].str() : record[1].str();
            if (name.empty() || (record[1].matched && record[3].matched && record[1] != record[3]))
            {
                std::cerr << "结构体需要一致的标签与别名\n";
                return 1;
            }
            std::string fields;
            std::istringstream body(record[2]);
            while (std::getline(body, line, ';'))
            {
                line = trim(line);
                if (line.empty()) continue;
                std::smatch field;
                if (!std::regex_match(line, field, std::regex("(.+?)\\s+([A-Za-z_][A-Za-z0-9_]*)")) || type(field[1], records, aliases).empty() || type(field[1], records, aliases) == "void" || type(field[1], records, aliases).find(" fn(") != std::string::npos)
                {
                    std::cerr << "不支持的 C 结构体字段：" << line << '\n';
                    return 1;
                }
                fields += "    " + type(field[1], records, aliases) + " " + field[2].str() + ";\n";
            }
            if (fields.empty())
            {
                std::cerr << "不支持空 C 结构体\n";
                return 1;
            }
            records.insert(name);
            recordsText += "struct C_" + name + " {\n" + fields + "};\n";
            continue;
        }
        if (std::regex_match(unit, record, std::regex("(?:typedef\\s+)?enum\\s*([A-Za-z_][A-Za-z0-9_]*)?\\s*\\{([\\s\\S]*)\\}\\s*([A-Za-z_][A-Za-z0-9_]*)?")))
        {
            const auto name = record[3].matched ? record[3].str() : record[1].str();
            if (name.empty())
            {
                std::cerr << "枚举必须有名称\n";
                return 1;
            }
            std::istringstream items(record[2]);
            std::int64_t next = 0;
            while (std::getline(items, line, ','))
            {
                line = trim(line);
                if (line.empty()) continue;
                std::smatch item;
                if (!std::regex_match(line, item, std::regex("([A-Za-z_][A-Za-z0-9_]*)(?:\\s*=\\s*([+-]?(?:0[xX][0-9a-fA-F]+|[0-9]+)))?")))
                {
                    std::cerr << "枚举值必须是整数字面量：" << line << '\n';
                    return 1;
                }
                if (item[2].matched)
                {
                    try
                    {
                        next = std::stoll(item[2], nullptr, 0);
                    } catch (...)
                    {
                        std::cerr << "枚举值越界\n";
                        return 1;
                    }
                }
                if (next < INT32_MIN || next > INT32_MAX)
                {
                    std::cerr << "枚举值超出 int32\n";
                    return 1;
                }
                recordsText += "const int32 C_" + item[1].str() + " = " + std::to_string(next++) + ";\n";
            }
            // 枚举参数使用固定宽度 C 包装接口，避免猜测宿主枚举宽度。
            continue;
        }
        std::smatch function;
        if (!std::regex_match(unit, function, std::regex("(.+?)\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*\\(([^()]*)\\)")))
        {
            std::cerr << "不支持的 C 声明：" << unit << '\n';
            return 1;
        }
        const auto result = type(function[1], records, aliases);
        if (result.empty() || result.find(" fn(") != std::string::npos)
        {
            std::cerr << "不支持的 C 返回类型：" << function[1] << '\n';
            return 1;
        }
        std::string parameters;
        std::istringstream args(function[3]);
        std::size_t index = 0;
        while (std::getline(args, line, ','))
        {
            line = trim(line);
            if (line.empty() || line == "void") continue;
            std::smatch parameter;
            std::string mapped = type(line, records, aliases), name = "arg" + std::to_string(index++);
            if (mapped.empty() && std::regex_match(line, parameter, std::regex("(.+?)\\s+([A-Za-z_][A-Za-z0-9_]*)")))
            {
                mapped = type(parameter[1], records, aliases);
                name = parameter[2];
            }
            if (mapped.empty() || mapped == "void")
            {
                std::cerr << "不支持的 C 参数：" << line << '\n';
                return 1;
            }
            if (!parameters.empty()) parameters += ", ";
            parameters += mapped + " " + name;
        }
        functions += "    " + result + " c_" + function[2].str() + "(" + parameters + ") = " + quoted(function[2]) + ";\n";
    }
    std::ofstream file(std::filesystem::u8path(output), std::ios::binary);
    file << "/* 由 C 头文件生成；指针、宏、位域、联合体和可变参数须手写包装。 */\n"
         << recordsText << "abi " << quoted(library) << " {\n"
         << functions << "}\n";
    if (!file)
    {
        std::cerr << "无法写入 ABI 声明\n";
        return 1;
    }
    return 0;
}
}  // namespace chali_abi_import
