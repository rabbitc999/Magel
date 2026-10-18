#include "allocator/magel_allocator.hpp"
#include "toml/toml.hpp"  // 仓库 TOML 库的 Magel 内置副本（magel.toml 的语法交给它）
#ifndef MAGEL_PACKAGE_HPP
#define MAGEL_PACKAGE_HPP

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace magel_package
{
struct Project
{
    Memory::String file, contents, name, version, description, license;
    /// 编译器最低版本、依赖清单与锁文件签名。
    Memory::String compiler, lockSignature;
    Memory::Map<Memory::String, Memory::String> dependencies;
    Memory::Vector<Memory::String> authors, includes, imports, searchPaths, includePaths, importPaths;
    /**
     * `[magel] debug`：写 `.svmb` 时是否带调试节，缺省 `false`（不带）。
     *  为 `true` 时等价于命令行加 `--debug`：保留"指令 → 文件/行"位置表，
     *  名字与类型表本来就在。
     */
    bool debug = false;
    /**
     * @brief [magel] compress_symbols：无名变长符号产物，默认关闭。
     */
    bool compressSymbols = false;
    /**
     * @brief [magel] symbol_map：PC 映射文件，空值使用 <前缀>.symbol_map.json。
     */
    Memory::String symbolMap;
    /**
     * `[magel] run`：项目默认运行方式，缺省 `"interpret"`。取值
     *  `interpret` / `svmb` / `aot`（也接受 `smvb` 拼写）；命令行的子命令或
     *  `--mode` 覆盖它。统一入口 `magel` 据此决定解释 / 导出。
     */
    Memory::String run = "interpret";
    /**
     * `[magel] cpp_output`：导出可执行文件时是否保留生成的 C++ 源码，缺省
     *  `false`（导出程序不写/不留代码）。等价命令行 `--cpp-output`。
     */
    bool cppOutput = false;
    /**
     * `[magel] cpp_output_dir`：生成的 C++ 源码目录（缺省与可执行文件同目录）。
     */
    Memory::String cppOutputDir;
    /**
     * `[magel] execute_output`：可执行文件 / 二进制的输出路径（等价 `--execute-output`）。
     */
    Memory::String executeOutput;
    /**
     * `[magel] opt_level`：AOT / VM 导出时传给 C/C++ 编译器的优化等级
     *  （`0/1/2/3/s/fast` 或裸 `-O2`）。空串 = 用各模式缺省。
     */
    Memory::String optLevel;
    /**
     * `[magel] cxx`（别名 `g++` / `gxx`）/ `cc`（别名 `gcc`）：导出程序时用的
     *  C++ / C 编译器（空 = 环境变量 `CXX` / `CC`，再退回 `g++` / `gcc`）。
     *  命令行 `--cxx` / `--g++` / `--cc` / `--gcc` 优先。
     */
    Memory::String cxx, cc;
    Memory::String gccToolchain;  ///< GCC 目录或可执行文件路径；裸命令保持 PATH 查找。
    /**
     * `[magel] make`：构建生成工程时用的 make 路径（空 = 环境变量 `MAKE`，再退回
     *  `make`）。命令行 `--make` 优先。
     */
    Memory::String makeTool;
    /**
     * `[embedded] enabled`：是否开启嵌入式构建模式，缺省 `false`。开启后编译器按
     *  嵌入式运行时标准做能力检查，用了被限制模块 / 不存在的能力会编译报错。
     */
    bool embeddedEnabled = false;
    /**
     * `[embedded] restrict_modules`：嵌入式模式下额外禁用的模块名列表（如
     *  `"std::net"`）。内置硬禁模块（net / process / sql / io / thread / coroutine /
     *  concurrency）与语言能力（异常、线程、大数）始终禁用，这里只做追加。
     */
    Memory::Vector<Memory::String> embeddedRestrict;
    /**
     * `[embedded] max_memory`：嵌入式模式的最大运行内存（字节）。嵌入式**用正常解释器**
     *  执行，只是把内存上限压到这个值；`0` 表示不额外限制（沿用运行时缺省容量）。
     */
    std::int64_t maxMemory = 0;
    /**
     * `[magel] main`：工程主文件（入口）。指定后命令在不给源文件时默认用它，
     *  程序从该文件的 `main` 开始执行。
     */
    Memory::String main;
    /**
     * @brief 清单内容 + 锁文件内容的签名（锁文件把依赖的签名也拼进来）。
     * @return 清单文件为空（没有项目）时是空串。
     */
    Memory::String signature() const
    {
        if (file.empty()) return Memory::String();
        return file + "\n" + contents + "\n" + lockSignature;
    }
};
/**
 * @brief 去掉首尾空白（空格 / 制表 / 换行）。
 * @param s 输入文本。
 * @return 去掉首尾空白后的副本；整串都是空白时返回空串。
 */
inline Memory::String trim(const Memory::String &s)
{
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == Memory::String::npos) return Memory::String();
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}

/**
 * @brief 归一化运行方式：把 `smvb` 拼写折到 `svmb`。
 * @param mode 配置里写的运行方式。
 * @return 归一化后的名字；不认识时返回空串（调用方据此报错）。
 */
inline Memory::String normalizeRunMode(const Memory::String &mode)
{
    if (mode == "smvb") return "svmb";
    if (mode == "interpret" || mode == "svmb" || mode == "aot") return mode;
    return Memory::String();
}

/**
 * @brief 模块名是否合法：`a::b::c` 形式的点分标识符（`::` 分段，每段以字母或下划线开头）。
 * @param s 待判定的名字。
 * @return 合法返回 true。
 */
inline bool moduleName(const Memory::String &s)
{
    bool first = true; /* 当前段是不是刚开头（开头不许是数字） */
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (!first && c == ':' && i + 1 < s.size() && s[i + 1] == ':')
        {
            ++i;
            first = true; /* `::`：下一段重新开始 */
            continue;
        }
        if (!(std::isalpha(c) || c == '_' || (!first && std::isdigit(c)))) return false;
        first = false;
    }
    return !s.empty() && !first;
}

/**
 * @brief 嵌入式模式禁用的模块集合 = 内置硬禁模块 + 用户在 `[embedded] restrict_modules`
 *        里追加的模块。net / process / sql / io / thread / coroutine / concurrency 在
 *        嵌入式运行时（mcu_vm）里没有后端，异常 / 线程 / 大数等语言能力也始终禁用。
 * @param project 已解析的项目（提供 `embeddedRestrict`）。
 * @return 去重后的模块名集合，便于 `count` 判定某个 import 是否被禁。
 */
inline Memory::Set<Memory::String> embeddedDeniedModules(const Project &project)
{
    static const Memory::Vector<Memory::String> builtin = {
        "std::net", "std::process", "std::sql", "std::io",
        "std::thread", "std::coroutine", "std::concurrency"
    };
    Memory::Set<Memory::String> denied;
    for (const auto &module : builtin) denied.insert(module);
    for (const auto &module : project.embeddedRestrict) denied.insert(module);
    return denied;
}

/**
 * @brief 归一化 `%include` 的路径：反斜杠转正斜杠，并拒绝空串 / 跨行 / 含 NUL。
 * @param s 配置里写的路径。
 * @return 归一化后的路径。
 * @throws std::runtime_error 路径为空、跨行或含 NUL 时。
 */
inline Memory::String includeName(const Memory::String &s)
{
    Memory::String normalized = s;
    for (char &c : normalized)
    {
        if (c == '\\') c = '/';
    }
    if (normalized.empty() || normalized.find_first_of("\r\n") != Memory::String::npos || normalized.find('\0') != Memory::String::npos)
    {
        throw std::runtime_error("path must be a nonempty single-line string");
    }
    return normalized;
}

/**
 * @brief 把配置里的相对路径按包清单所在目录展开成绝对路径。
 * @param s 配置里的路径。
 * @param base 包清单所在目录。
 * @return 规范化的绝对路径（正斜杠）。
 */
inline Memory::String path(const Memory::String &s, const std::filesystem::path &base)
{
    return std::filesystem::absolute(base / Memory::u8path(includeName(s)))
        .lexically_normal()
        .generic_u8string();
}

/**
 * @brief 把文本写成 TOML 双引号字符串（转义 `\` 与 `"`）；`magel new` 生成清单时用。
 * @param s 文本。
 * @return 带引号的字面量。
 */
inline Memory::String quote(const Memory::String &s)
{
    Memory::String out = "\"";
    for (char c : s)
    {
        if (c == '\\' || c == '"') out += '\\';
        out += c;
    }
    return out + '"';
}

/*
 * `magel.toml` 的**格式校验**（与 TOML 语法本身分开）：
 *
 *   * 语法（字符串转义、数组、表头、注释、行尾 …）交给仓库自己的 TOML 库
 *     `magel/library/toml`（`library/toml` 的 Magel 内置副本，与 `std::toml` 同一份实现）；
 *   * 这里只回答"**这个格式允许哪些表和键、每个值怎么解释**"：未知表、未知键、
 *     类型不对、该非空却空了一律明确报错，不静默忽略。
 *
 * 报错位置统一写成 `表.键`（例如 `magel.symbol_map`），好让用户直接找到那行配置。
 */

/**
 * @brief 取一个字符串值。
 * @param value 待取的值。
 * @param where 位置（`表.键`），只用于报错。
 * @return 文本。
 * @throws std::runtime_error 值不是字符串时。
 */
inline Memory::String tomlText(const toml::Value &value, const Memory::String &where)
{
    const std::optional<std::string_view> text = value.asString();
    if (!text.has_value()) throw std::runtime_error("expects a quoted string: " + where);
    return Memory::String(*text);
}

/**
 * @brief 取一个布尔值。
 * @param value 待取的值。
 * @param where 位置（`表.键`），只用于报错。
 * @return 布尔。
 * @throws std::runtime_error 值不是布尔时。
 */
inline bool tomlBool(const toml::Value &value, const Memory::String &where)
{
    const std::optional<bool> flag = value.asBool();
    if (!flag.has_value()) throw std::runtime_error("expected a boolean (true/false): " + where);
    return *flag;
}

/**
 * @brief 取一个整数值。
 * @param value 待取的值。
 * @param where 位置（`表.键`），只用于报错。
 * @return 整数。
 * @throws std::runtime_error 值不是整数时。
 */
inline std::int64_t tomlInt(const toml::Value &value, const Memory::String &where)
{
    const std::optional<std::int64_t> number = value.asInt();
    if (!number.has_value()) throw std::runtime_error("expects an integer: " + where);
    return *number;
}

/**
 * @brief 取一个字符串数组（每一项都必须是字符串）。
 * @param value 待取的值。
 * @param where 位置（`表.键`），只用于报错。
 * @return 各项文本，保持文件里的顺序。
 * @throws std::runtime_error 值不是数组、或某一项不是字符串时。
 */
inline Memory::Vector<Memory::String> tomlArray(const toml::Value &value,
                                                const Memory::String &where)
{
    const toml::Array *items = value.asArray();
    if (items == nullptr) throw std::runtime_error("expects a string array: " + where);
    Memory::Vector<Memory::String> values;
    for (const toml::Value &item : *items) values.push_back(tomlText(item, where));
    return values;
}

/**
 * @brief 当前工具链版本，参与项目最低版本验证。
 */
inline constexpr const char *compilerVersion = "1.0.0";

/**
 * @brief 解析 `1.2.3` 形式的三段版本号。
 * @param text 版本串。
 * @return 三段整数（主 / 次 / 补丁）。
 * @throws std::runtime_error 段数不对、含非数字或结尾有多余内容时（**拒绝模糊范围**）。
 */
inline std::array<unsigned, 3> versionParts(const Memory::String &text)
{
    std::array<unsigned, 3> parts{};
    Memory::InputStringStream in(text);
    char dot = 0;
    for (int i = 0; i < 3; ++i)
    {
        if (!(in >> parts[i])) throw std::runtime_error("invalid compiler version");
        if (i < 2 && (!(in >> dot) || dot != '.'))
            throw std::runtime_error("invalid compiler version");
    }
    if (in.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("invalid compiler version");
    return parts;
}

/**
 * @brief 计算依赖源码与配置的**确定性内容指纹**（FNV-1a 64 位，16 位十六进制）。
 *
 * 口径（改这里等于让所有 `magel.lock` 失效，动之前先想清楚）：
 *   * 只收源码类后缀（`{mgl,inc,h,hpp,c,cpp,toml,json}`）与 `magel.lock`；
 *   * 跳过 `.git` / `.magel` / `build` / `tmp` / `node_modules` / `__pycache__`；
 *   * 文件名按字典序排序，`CRLF` 统一成 `LF`，名字与内容之间用一个 `\0` 分隔；
 *   * 这是**内容指纹**（防手改），不是安全签名。
 *
 * @param root 依赖根目录。
 * @return 十六进制指纹。
 * @throws std::runtime_error 路径里有符号链接、或源码读不出来时。
 */
inline Memory::String dependencyFingerprint(const std::filesystem::path &root)
{
    namespace fs = std::filesystem;
    Memory::Vector<Memory::String> names;
    const Memory::Set<Memory::String> ignored = { ".git", ".magel", "build",
                                                  "tmp", "node_modules", "__pycache__" };
    const Memory::Set<Memory::String> extensions = { ".mgl", ".inc", ".h", ".hpp",
                                                     ".c", ".cpp", ".toml", ".json" };
    for (fs::recursive_directory_iterator it(root), end; it != end; ++it)
    {
        if (it->is_symlink()) throw std::runtime_error("dependency symlinks are unsupported");
        if (it->is_directory() && ignored.count(it->path().filename().generic_u8string()))
        {
            it.disable_recursion_pending(); /* 生成目录整棵不看 */
            continue;
        }
        if (it->is_regular_file() && (extensions.count(it->path().extension().generic_u8string()) || it->path().filename() == "magel.lock"))
        {
            names.push_back(it->path().lexically_relative(root).generic_u8string());
        }
    }
    std::sort(names.begin(), names.end());
    std::uint64_t hash = 14695981039346656037ull; /* FNV-1a 64 位偏移基数 */
    auto add = [&](unsigned char c) {
        hash ^= c;
        hash *= 1099511628211ull; /* FNV-1a 质数 */
    };
    for (const auto &name : names)
    {
        for (unsigned char c : name) add(c);
        add(0); /* 名字与内容的分隔符 */
        std::ifstream input(root / Memory::u8path(name), std::ios::binary);
        if (!input) throw std::runtime_error("cannot hash dependency source");
        char c = 0;
        while (input.get(c))
        {
            if (c == '\r' && input.peek() == '\n') continue; /* CRLF 统一成 LF */
            add(static_cast<unsigned char>(c));
        }
        add(0);
    }
    Memory::OutputStringStream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}

/**
 * @brief `validateDependencies` 的前置声明（`read()` 里用得上）。
 */
inline bool validateDependencies(Project &project, Memory::String &error);
inline bool read(const std::filesystem::path &file, Project &project, Memory::String &error)
{
    project = Project{};
    std::size_t manifestLine = 1;
    try
    {
        project.file = std::filesystem::absolute(file).lexically_normal().generic_u8string();
        std::ifstream input(file, std::ios::binary);
        if (!input) throw std::runtime_error("cannot read package manifest");
        Memory::OutputStringStream buffer;
        buffer << input.rdbuf();
        project.contents = buffer.str();
        /* 去掉可能的 UTF-8 BOM（TOML 库不认），再交给 toml 库解析语法。 */
        Memory::String text = project.contents;
        if (text.compare(0, 3, "\xef\xbb\xbf") == 0) text.erase(0, 3);
        const toml::ParseResult parsed = toml::parse(std::string_view(text.data(), text.size()));
        if (!parsed.ok())
        {
            const std::string why = parsed.diagnostics.empty() ? std::string("语法错误") : parsed.diagnostics.front().toString();
            if (!parsed.diagnostics.empty()) manifestLine = parsed.diagnostics.front().line;
            throw std::runtime_error("invalid TOML: " + why);
        }
        const toml::Value &root = *parsed.value;
        const auto base = file.parent_path();
        if (!root.isTable()) throw std::runtime_error("the manifest root must be a table");
        for (const toml::Member &table : root.asTable()->members())
        {
            const Memory::String name = table.key;
            if (!table.value.isTable()) throw std::runtime_error("not a table: " + name);
            if (name == "dependencies")
            {
                for (const toml::Member &entry : table.value.asTable()->members())
                    project.dependencies[entry.key] =
                        tomlText(entry.value, "dependencies." + entry.key);
                continue;
            }
            if (name != "package" && name != "magel" && name != "embedded")
                throw std::runtime_error("unknown or repeated table: " + name);
            for (const toml::Member &entry : table.value.asTable()->members())
            {
                const Memory::String key = entry.key;
                const Memory::String where = name + "." + key;
                if (name == "embedded")
                {
                    if (entry.key == "enabled")
                    {
                        project.embeddedEnabled = tomlBool(entry.value, where);
                    }
                    else if (entry.key == "restrict_modules")
                    {
                        for (const auto &value : tomlArray(entry.value, where))
                        {
                            if (!moduleName(value))
                                throw std::runtime_error("invalid module name: " + value);
                            project.embeddedRestrict.push_back(value);
                        }
                    }
                    else if (entry.key == "max_memory")
                    {
                        const std::int64_t bytes = tomlInt(entry.value, where);
                        if (bytes <= 0)
                            throw std::runtime_error("max_memory must be a positive integer: " + where);
                        project.maxMemory = bytes;
                    }
                    else
                        throw std::runtime_error("unknown embedded key: " + where);
                    continue;
                }
                if (name == "package")
                {
                    if (key == "authors")
                    {
                        const auto values = tomlArray(entry.value, where);
                        project.authors.insert(project.authors.end(), values.begin(), values.end());
                        continue;
                    }
                    if (key != "name" && key != "version" && key != "author" && key != "description" && key != "license" && key != "compiler")
                        throw std::runtime_error("unknown package key: " + key);
                    const Memory::String value = tomlText(entry.value, where);
                    if (value.empty()) throw std::runtime_error("package value cannot be empty: " + key);
                    if (key == "name")
                        project.name = value;
                    else if (key == "version")
                        project.version = value;
                    else if (key == "author")
                        project.authors.push_back(value);
                    else if (key == "description")
                        project.description = value;
                    else if (key == "license")
                        project.license = value;
                    else
                        project.compiler = value;
                    continue;
                }
                if (key == "run")
                {
                    const auto mode = normalizeRunMode(tomlText(entry.value, where));
                    if (mode.empty()) throw std::runtime_error("invalid run mode (expect interpret|svmb|aot)");
                    project.run = mode;
                }
                else if (key == "debug")
                {
                    project.debug = tomlBool(entry.value, where);
                }
                else if (key == "compress_symbols")
                {
                    project.compressSymbols = tomlBool(entry.value, where);
                }
                else if (key == "symbol_map")
                {
                    project.symbolMap = path(tomlText(entry.value, where), base);
                }
                else if (key == "cpp_output")
                {
                    project.cppOutput = tomlBool(entry.value, where);
                }
                else if (key == "cpp_output_dir")
                {
                    project.cppOutputDir = path(tomlText(entry.value, where), base);
                }
                else if (key == "execute_output")
                {
                    project.executeOutput = path(tomlText(entry.value, where), base);
                }
                else if (key == "opt_level")
                {
                    project.optLevel = tomlText(entry.value, where);
                }
                else if (key == "gcc_toolchain")
                {
                    const auto value = tomlText(entry.value, where);
                    project.gccToolchain = value.find('/') != Memory::String::npos || value.find(static_cast<char>(92)) != Memory::String::npos ? path(value, base) : value;
                }
                else if (key == "cxx" || key == "g++" || key == "gxx" || key == "cc" || key == "gcc" || key == "make")
                {
                    const Memory::String value = tomlText(entry.value, where);
                    if (key == "cxx" || key == "g++" || key == "gxx")
                        project.cxx = value;
                    else if (key == "cc" || key == "gcc")
                        project.cc = value;
                    else
                        project.makeTool = value;
                }
                else if (key == "main")
                {
                    project.main = path(tomlText(entry.value, where), base);
                }
                else if (key == "include" || key == "search_paths" || key == "include_paths" || key == "import_paths")
                {
                    const auto values = tomlArray(entry.value, where);
                    auto &target = key == "include" ? project.includes : key == "include_paths" ? project.includePaths :
                                                                     key == "import_paths"      ? project.importPaths :
                                                                                                  project.searchPaths;
                    for (const auto &value : values)
                        target.push_back(key == "include" ? includeName(value) : path(value, base));
                }
                else if (key == "import")
                {
                    for (const auto &value : tomlArray(entry.value, where))
                    {
                        if (!moduleName(value)) throw std::runtime_error("invalid import: " + value);
                        project.imports.push_back(value);
                    }
                }
                else if (key == "commands")
                {
                    for (const auto &value : tomlArray(entry.value, where))
                    {
                        const auto command = trim(value);
                        if (command.size() > 7 && command.compare(0, 6, "import") == 0 && std::isspace(static_cast<unsigned char>(command[6])) && command.back() == ';')
                        {
                            const auto imported = trim(command.substr(7, command.size() - 8));
                            if (!moduleName(imported)) throw std::runtime_error("invalid import command");
                            project.imports.push_back(imported);
                        }
                        else if (command.size() > 9 && command.compare(0, 8, "%include") == 0 && std::isspace(static_cast<unsigned char>(command[8])))
                        {
                            const auto argument = trim(command.substr(9));
                            if (argument.size() < 2 || argument.front() != '"' || argument.back() != '"')
                                throw std::runtime_error("include command needs a quoted path");
                            project.includes.push_back(includeName(argument.substr(1, argument.size() - 2)));
                        }
                        else
                            throw std::runtime_error("commands only supports %include and import");
                    }
                }
                else
                    throw std::runtime_error("unknown key: " + where);
            }
        }

        /* 作者名不能是空串（`author = ""` 或 `authors` 里的空项）。 */
        for (const auto &author : project.authors)
        {
            if (author.empty()) throw std::runtime_error("author cannot be empty");
        }
        /* 搜索路径总以清单所在目录和它的 `modules/` 打头，配置里的路径排在后面。 */
        const auto absoluteBase = std::filesystem::absolute(base);
        project.searchPaths.push_back(absoluteBase.generic_u8string());
        project.searchPaths.push_back((absoluteBase / "modules").generic_u8string());
        /* `compiler` 只认 `>=x.y.z` 一种写法：要求和当前工具链都拆成三段整数再比，
         * 版本不够（或写法不对）直接报错。 */
        if (!project.compiler.empty())
        {
            const bool supported =
                project.compiler.rfind(">=", 0) == 0 && versionParts(compilerVersion) >= versionParts(project.compiler.substr(2));
            if (!supported)
                throw std::runtime_error("compiler requirement not satisfied: " + project.compiler);
        }
        /* 清单本身没问题了，最后核对 `magel.lock`（有依赖时才要求它）。 */
        return validateDependencies(project, error);
    } catch (const std::exception &exception)
    {
        /* 报错带清单文件与行号：TOML 语法错误用诊断里的行号，其余按第 1 行算。 */
        error = project.file + ":" + Memory::toString(manifestLine) + ": " + exception.what();
        return false;
    }
}
/**
 * @brief 校验锁定来源、包版本与源码指纹；编译过程绝不联网。
 *
 * 校验链（任何一步不符都直接报错，提示重跑 `magel deps lock`）：
 *   1. `magel.lock` 存在（有依赖才要求）且是本文认识的格式；
 *   2. `[lock] version` 必须是 `1`、`compiler` 非空；
 *   3. 依赖条数与 `magel.toml` 一致，每条的四元数组首项（来源）相同；
 *   4. 本地目录与锁定来源推导出的目录一致，且真在磁盘上；
 *   5. 递归校验依赖自己的 `magel.toml`（顺带检测依赖环）。
 *
 * @param project 项目与依赖搜索路径。
 * @param error 错误文本。
 * @return 全部校验通过时为 true。
 */
inline bool validateDependencies(Project &project, Memory::String &error)
{
    namespace fs = std::filesystem;
    /* 依赖环检测：同一份 magel.toml 在递归里再进来一次就是环。用 thread_local 集合
     * 记住"正在处理链上"的文件，配一个 RAII Guard，正常返回或抛异常都会把自己摘掉。 */
    static thread_local Memory::Set<Memory::String> active;
    if (!active.insert(project.file).second)
    {
        error = "dependency cycle: " + project.file;
        return false;
    }
    struct Guard
    {
        Memory::Set<Memory::String> &set;
        Memory::String name;
        ~Guard() { set.erase(name); }
    } guard{ active, project.file };
    try
    {
        const auto base = Memory::u8path(project.file).parent_path();
        const auto lockFile = base / "magel.lock";
        /* 没有依赖、也没有锁文件：没什么可校验的。 */
        if (project.dependencies.empty() && !fs::exists(lockFile)) return true;
        std::ifstream input(lockFile, std::ios::binary);
        if (!input) throw std::runtime_error("missing magel.lock; run magel deps lock");
        Memory::OutputStringStream buffer;
        buffer << input.rdbuf();
        project.lockSignature = buffer.str();
        /* 锁文件与 magel.toml 是同一套 TOML 语法，同样交给 `magel/library/toml` 解析；
         * "允许哪些表和键"仍在这里校验：
         *   [lock]         version = "1"        —— 锁文件格式版本
         *                  compiler = ">=x.y.z" —— 生成它的编译器要求
         *   [dependencies] 名字 = [来源, 目录, 提交, 指纹] —— 四项字符串数组 */
        Memory::String lockText = project.lockSignature;
        if (lockText.compare(0u, 3u, "\xef\xbb\xbf") == 0) lockText.erase(0u, 3u);
        const toml::ParseResult lockParsed =
            toml::parse(std::string_view(lockText.data(), lockText.size()));
        if (!lockParsed.ok() || !(*lockParsed.value).isTable())
            throw std::runtime_error("invalid TOML in magel.lock; run magel deps lock");
        Memory::Map<Memory::String, Memory::Vector<Memory::String>> locked;
        Memory::String format;
        Memory::String compiler;
        for (const toml::Member &table : (*lockParsed.value).asTable()->members())
        {
            if (!table.value.isTable()) throw std::runtime_error("unknown lock table");
            if (table.key == "lock")
            {
                for (const toml::Member &entry : table.value.asTable()->members())
                {
                    if (entry.key == "version")
                        format = tomlText(entry.value, "lock.version");
                    else if (entry.key == "compiler")
                        compiler = tomlText(entry.value, "lock.compiler");
                    else
                        throw std::runtime_error("unknown lock key");
                }
            }
            else if (table.key == "dependencies")
            {
                for (const toml::Member &entry : table.value.asTable()->members())
                    locked[entry.key] = tomlArray(entry.value, "dependencies." + entry.key);
            }
            else
            {
                throw std::runtime_error("unknown lock table");
            }
        }
        if (format != "1" || compiler.empty() || locked.size() != project.dependencies.size())
            throw std::runtime_error("stale or unsupported magel.lock; run magel deps lock");

        /* 逐条核对锁文件里的依赖记录：四项 = [来源, 目录, 版本, 指纹]。 */
        for (const auto &dependency : project.dependencies)
        {
            const auto record = locked.find(dependency.first);
            if (record == locked.end() || record->second.size() != 4 || record->second[0] != dependency.second)
            {
                throw std::runtime_error("dependency source mismatch: " + dependency.first);
            }
            const auto &fields = record->second;
            const auto root = fs::weakly_canonical(base / Memory::u8path(fields[1]));
            const auto source = dependency.second;
            /* 在线缓存位于生成器 libraries；兼容旧 Git 缓存，路径依赖仍按清单展开。 */
            fs::path expected;
            if (source.rfind("git+", 0) == 0)
            {
                const auto hash = source.rfind('#');
                if (hash == Memory::String::npos || source.size() - hash - 1 != 40)
                    throw std::runtime_error("git dependency requires full commit");
                const auto leaf = Memory::u8path(dependency.first + "-" + source.substr(hash + 1));
                expected = base / ".magel" / "deps" / leaf;
                if (root.parent_path().filename() == "libraries" && root.filename() == leaf)
                    expected = root;
            }
            else if (source.rfind("http://", 0) == 0 || source.rfind("https://", 0) == 0)
            {
                const auto hash = source.rfind("#sha256=");
                if (hash == Memory::String::npos || source.size() - hash - 8 != 64)
                    throw std::runtime_error("HTTP dependency requires full SHA-256");
                auto digest = source.substr(hash + 8);
                for (char &c : digest)
                {
                    if (!std::isxdigit(static_cast<unsigned char>(c)))
                        throw std::runtime_error("invalid HTTP dependency SHA-256");
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                const auto leaf = Memory::u8path(dependency.first + "-" + digest);
                if (root.parent_path().filename() != "libraries" || root.filename() != leaf)
                    throw std::runtime_error("HTTP dependency cache must be in libraries");
                expected = root;
            }
            else
            {
                expected = base / Memory::u8path(source.rfind("path:", 0) == 0 ? source.substr(5) : source);
            }
            if (root != fs::weakly_canonical(expected) || !fs::is_directory(root))
                throw std::runtime_error("missing or mismatched dependency: " + dependency.first);
            /* 依赖自己的清单也要读得动，且版本与指纹和锁定值一致（编译期绝不联网）。 */
            Project child;
            Memory::String childError;
            if (!read(root / "magel.toml", child, childError)) throw std::runtime_error(childError);
            if (child.version != fields[2] || dependencyFingerprint(root) != fields[3])
                throw std::runtime_error("dependency changed: " + dependency.first + "; run magel deps lock");
            project.searchPaths.insert(project.searchPaths.end(), child.searchPaths.begin(),
                                       child.searchPaths.end());
            project.searchPaths.insert(project.searchPaths.end(), child.importPaths.begin(),
                                       child.importPaths.end());
            project.includePaths.insert(project.includePaths.end(), child.includePaths.begin(),
                                        child.includePaths.end());
            project.lockSignature += "\n" + child.signature();
        }
        return true;
    } catch (const std::exception &e)
    {
        error = project.file + ": " + e.what();
        return false;
    }
}

/**
 * @brief 从入口文件所在目录向上找 `magel.toml`；找不到就用默认项目设置。
 * @param entry 入口 `.mgl` 路径（空 = 当前目录）。
 * @param project 输出：解析出的项目。
 * @param error 错误文本。
 * @return 成功（含"没找到清单"这一正常情况）返回 true。
 */
inline bool discover(const Memory::String &entry, Project &project, Memory::String &error)
{
    namespace fs = std::filesystem;
    project = Project{};
    auto dir = entry.empty() ? fs::current_path() : fs::absolute(Memory::u8path(entry)).parent_path();
    for (;;)
    {
        const auto candidate = dir / "magel.toml";
        std::error_code ec;
        if (fs::exists(candidate, ec)) return read(candidate, project, error);
        const auto parent = dir.parent_path();
        if (parent == dir || parent.empty()) return true; /* 到根了：用默认设置 */
        dir = parent;
    }
}
}  // namespace magel_package
#endif
