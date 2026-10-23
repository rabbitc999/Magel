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
#include "chali_package.hpp"
#include "toml/toml.hpp"

namespace chali_package
{

/**
 * @brief 去掉首尾空白（空格 / 制表 / 换行）。
 * @param s 输入文本。
 * @return 去掉首尾空白后的副本；整串都是空白时返回空串。
 */
Memory::String trim(const Memory::String &s)
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
Memory::String normalizeRunMode(const Memory::String &mode)
{
    if (mode == "smvb") return "svmb";
    if (mode == "interpret" || mode == "svmb") return mode;
    return Memory::String();
}

/**
 * @brief 模块名是否合法：`a::b::c` 形式的点分标识符（`::` 分段，每段以字母或下划线开头）。
 * @param s 待判定的名字。
 * @return 合法返回 true。
 */
bool moduleName(const Memory::String &s)
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
 * @brief 归一化工程配置路径：反斜杠转正斜杠，并拒绝空串 / 跨行 / 含 NUL。
 * @param s 配置里写的路径。
 * @return 归一化后的路径。
 * @throws std::runtime_error 路径为空、跨行或含 NUL 时。
 */
Memory::String normalizedPathText(const Memory::String &s)
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
Memory::String path(const Memory::String &s, const std::filesystem::path &base)
{
    return std::filesystem::absolute(base / Memory::u8path(normalizedPathText(s)))
        .lexically_normal()
        .generic_u8string();
}

/**
 * @brief 把文本写成 TOML 双引号字符串（转义 `\` 与 `"`）；`chali new` 生成清单时用。
 * @param s 文本。
 * @return 带引号的字面量。
 */
Memory::String quote(const Memory::String &s)
{
    Memory::String out = "\"";
    for (char c : s)
    {
        if (c == '\\' || c == '"') out += '\\';
        out += c;
    }
    return out + '"';
}

/**
 * @brief 取一个字符串值。
 * @param value 待取的值。
 * @param where 位置（`表.键`），只用于报错。
 * @return 文本。
 * @throws std::runtime_error 值不是字符串时。
 */
Memory::String tomlText(const toml::Value &value, const Memory::String &where)
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
bool tomlBool(const toml::Value &value, const Memory::String &where)
{
    const std::optional<bool> flag = value.asBool();
    if (!flag.has_value()) throw std::runtime_error("expected a boolean (true/false): " + where);
    return *flag;
}

/**
 * @brief 取一个字符串数组（每一项都必须是字符串）。
 * @param value 待取的值。
 * @param where 位置（`表.键`），只用于报错。
 * @return 各项文本，保持文件里的顺序。
 * @throws std::runtime_error 值不是数组、或某一项不是字符串时。
 */
Memory::Vector<Memory::String> tomlArray(const toml::Value &value,
                                         const Memory::String &where)
{
    const toml::Array *items = value.asArray();
    if (items == nullptr) throw std::runtime_error("expects a string array: " + where);
    Memory::Vector<Memory::String> values;
    for (const toml::Value &item : *items) values.push_back(tomlText(item, where));
    return values;
}

/**
 * @brief 解析 `1.2.3` 形式的三段版本号。
 * @param text 版本串。
 * @return 三段整数（主 / 次 / 补丁）。
 * @throws std::runtime_error 段数不对、含非数字或结尾有多余内容时（**拒绝模糊范围**）。
 */
std::array<unsigned, 3> versionParts(const Memory::String &text)
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
 * 口径（改这里等于让所有 `chali.lock` 失效，动之前先想清楚）：
 *   * 只收源码类后缀（`{chl,inc,h,hpp,c,cpp,toml,json}`）与 `chali.lock`；
 *   * 跳过 `.git` / `.chali` / `build` / `tmp` / `node_modules` / `__pycache__`；
 *   * 文件名按字典序排序，`CRLF` 统一成 `LF`，名字与内容之间用一个 `\0` 分隔；
 *   * 这是**内容指纹**（防手改），不是安全签名。
 *
 * @param root 依赖根目录。
 * @return 十六进制指纹。
 * @throws std::runtime_error 路径里有符号链接、或源码读不出来时。
 */
Memory::String dependencyFingerprint(const std::filesystem::path &root)
{
    namespace fs = std::filesystem;
    Memory::Vector<Memory::String> names;
    const Memory::Set<Memory::String> ignored = { ".git", ".chali", "build",
                                                  "tmp", "node_modules", "__pycache__" };
    const Memory::Set<Memory::String> extensions = { ".chl", ".inc", ".h", ".hpp",
                                                     ".c", ".cpp", ".toml", ".json" };
    for (fs::recursive_directory_iterator it(root), end; it != end; ++it)
    {
        if (it->is_symlink()) throw std::runtime_error("dependency symlinks are unsupported");
        if (it->is_directory() && ignored.count(it->path().filename().generic_u8string()))
        {
            it.disable_recursion_pending(); /* 生成目录整棵不看 */
            continue;
        }
        if (it->is_regular_file() && (extensions.count(it->path().extension().generic_u8string()) || it->path().filename() == "chali.lock"))
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
 * @brief 读取并校验工程清单、目标配置及依赖声明。
 * @param file 工程清单文件的磁盘路径。
 * @param project 成功时接收规范化的工程设置。
 * @param error 失败时接收带清单位置的诊断。
 * @return 清单语法、目标配置和依赖均有效时返回 true。
 */
bool read(const std::filesystem::path &file, Project &project, Memory::String &error)
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
            if (name != "package" && name != "chali")
                throw std::runtime_error("unknown or repeated table: " + name);
            for (const toml::Member &entry : table.value.asTable()->members())
            {
                const Memory::String key = entry.key;
                const Memory::String where = name + "." + key;
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
                    if (mode.empty()) throw std::runtime_error("invalid run mode (expect interpret|svmb)");
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
                else if (key == "main")
                {
                    project.main = path(tomlText(entry.value, where), base);
                }
                else if (key == "search_paths" || key == "include_paths" || key == "import_paths")
                {
                    const auto values = tomlArray(entry.value, where);
                    auto &target = key == "include_paths" ? project.includePaths :
                                   key == "import_paths"  ? project.importPaths :
                                                            project.searchPaths;
                    for (const auto &value : values)
                        target.push_back(path(value, base));
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
                        else
                            throw std::runtime_error("commands only supports import");
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
        /* 清单本身没问题了，最后核对 `chali.lock`（有依赖时才要求它）。 */
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
 * 校验链（任何一步不符都直接报错，提示重跑 `chali deps lock`）：
 *   1. `chali.lock` 存在（有依赖才要求）且是本文认识的格式；
 *   2. `[lock] version` 必须是 `1`、`compiler` 非空；
 *   3. 依赖条数与 `chali.toml` 一致，每条的四元数组首项（来源）相同；
 *   4. 本地目录与锁定来源推导出的目录一致，且真在磁盘上；
 *   5. 递归校验依赖自己的 `chali.toml`（顺带检测依赖环）。
 *
 * @param project 项目与依赖搜索路径。
 * @param error 错误文本。
 * @return 全部校验通过时为 true。
 */
bool validateDependencies(Project &project, Memory::String &error)
{
    namespace fs = std::filesystem;
    /* 依赖环检测：同一份 chali.toml 在递归里再进来一次就是环。用 thread_local 集合
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
        const auto lockFile = base / "chali.lock";
        /* 没有依赖、也没有锁文件：没什么可校验的。 */
        if (project.dependencies.empty() && !fs::exists(lockFile)) return true;
        std::ifstream input(lockFile, std::ios::binary);
        if (!input) throw std::runtime_error("missing chali.lock; run chali deps lock");
        Memory::OutputStringStream buffer;
        buffer << input.rdbuf();
        project.lockSignature = buffer.str();
        /* 锁文件与 chali.toml 是同一套 TOML 语法，同样交给 `chali/source/toml` 解析；
         * "允许哪些表和键"仍在这里校验：
         *   [lock]         version = "1"        —— 锁文件格式版本
         *                  compiler = ">=x.y.z" —— 生成它的编译器要求
         *   [dependencies] 名字 = [来源, 目录, 提交, 指纹] —— 四项字符串数组 */
        Memory::String lockText = project.lockSignature;
        if (lockText.compare(0u, 3u, "\xef\xbb\xbf") == 0) lockText.erase(0u, 3u);
        const toml::ParseResult lockParsed =
            toml::parse(std::string_view(lockText.data(), lockText.size()));
        if (!lockParsed.ok() || !(*lockParsed.value).isTable())
            throw std::runtime_error("invalid TOML in chali.lock; run chali deps lock");
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
            throw std::runtime_error("stale or unsupported chali.lock; run chali deps lock");

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
                expected = base / ".chali" / "deps" / leaf;
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
            if (!read(root / "chali.toml", child, childError)) throw std::runtime_error(childError);
            if (child.version != fields[2] || dependencyFingerprint(root) != fields[3])
                throw std::runtime_error("dependency changed: " + dependency.first + "; run chali deps lock");
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
 * @brief 从入口文件所在目录向上找 `chali.toml`；找不到就用默认项目设置。
 * @param entry 入口 `.chl` 路径（空 = 当前目录）。
 * @param project 输出：解析出的项目。
 * @param error 错误文本。
 * @return 成功（含"没找到清单"这一正常情况）返回 true。
 */
bool discover(const Memory::String &entry, Project &project, Memory::String &error)
{
    namespace fs = std::filesystem;
    project = Project{};
    auto dir = entry.empty() ? fs::current_path() : fs::absolute(Memory::u8path(entry)).parent_path();
    for (;;)
    {
        const auto candidate = dir / "chali.toml";
        std::error_code ec;
        if (fs::exists(candidate, ec)) return read(candidate, project, error);
        const auto parent = dir.parent_path();
        if (parent == dir || parent.empty()) return true; /* 到根了：用默认设置 */
        dir = parent;
    }
}

}  // namespace chali_package
