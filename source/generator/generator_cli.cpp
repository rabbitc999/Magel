#include "core_assets.hpp"
#include "../../library/caspp/core_assets.hpp"
#include "../magel_runtime_allocator.hpp"
#include "../../library/template/engine.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

const char *const kManifestName = ".magel-generator-manifest";
const char *const kManifestHeader = "# Magel generator files v1";

/**
 * @brief 以二进制方式完整读取编译器工程源文件。
 * @param path 待读取的磁盘文件路径。
 * @param content 成功时接收完整文件内容。
 * @return 文件读取完整且无 I/O 错误时返回 true。
 */
bool readFile(const fs::path &path, Memory::String *content)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    Memory::OutputStringStream output;
    output << input.rdbuf();
    if (!input.eof() && input.fail()) return false;
    *content = output.str();
    return true;
}

/**
 * @brief 递归展开权威文法的包含文件，并重定位生成期代码片段的文件路径。
 * @param path 当前文法文件路径。
 * @param content 接收展开后的文法内容。
 * @param parents 当前递归链，用于拒绝循环包含。
 * @param error 接收文件读取或包含失败的诊断。
 * @return 全部包含成功展开时返回 true。
 * @note 仅处理文法文件组合；语言解析仍由 ParseEngine 生成的解析器负责。
 */
bool expandGrammarIncludes(const fs::path &path, Memory::String *content,
                           Memory::Set<fs::path> *parents, Memory::String *error)
{
    std::error_code pathError;
    const auto canonical = fs::weakly_canonical(path, pathError);
    if (pathError || !parents->insert(canonical).second)
    {
        *error = "无法展开文法路径或存在循环包含：" + path.u8string();
        return false;
    }
    Memory::String source;
    if (!readFile(canonical, &source))
    {
        *error = "无法读取文法：" + canonical.u8string();
        parents->erase(canonical);
        return false;
    }
    static const std::regex includeRule(R"gram(^\s*%include\s+"([^"]+)"\s*$)gram");
    static const std::regex codeRule(R"gram(^(\s*%(?:code|epilogue|main)\s+include\s+)"([^"]+)"(\s*)$)gram");
    std::istringstream lines(source);
    std::string line;
    content->clear();
    while (std::getline(lines, line))
    {
        std::smatch match;
        if (std::regex_match(line, match, includeRule))
        {
            Memory::String included;
            if (!expandGrammarIncludes(canonical.parent_path() / fs::u8path(match[1].str()),
                                       &included, parents, error))
            {
                parents->erase(canonical);
                return false;
            }
            *content += included;
        }
        else if (std::regex_match(line, match, codeRule))
        {
            const auto code = (canonical.parent_path() / fs::u8path(match[2].str())).lexically_normal();
            *content += match[1].str() + "\"" + code.generic_u8string() + "\"" + match[3].str() + "\n";
        }
        else
        {
            *content += line + "\n";
        }
    }
    parents->erase(canonical);
    return true;
}

/**
 * @brief 校验生成清单中的相对文件路径并规范化。
 * @param name 生成清单保存的逻辑路径。
 * @param relative 成功时接收规范化的相对路径。
 * @return 路径不越出工程且不覆盖清单本身时返回 true。
 */
bool safeRelative(const Memory::String &name, fs::path *relative)
{
    *relative = fs::path(name).lexically_normal();
    if (relative->empty() || *relative == "." || relative->is_absolute() ||
        relative->has_root_path() || relative->generic_string() == kManifestName)
        return false;
    return std::none_of(relative->begin(), relative->end(),
                        [](const fs::path &part) { return part == ".."; });
}

/**
 * @brief 拒绝生成目标的父路径经由符号链接重定向。
 * @param root 生成工程的绝对根目录。
 * @param relative 已通过相对路径检查的目标路径。
 * @param error 拒绝访问或查询失败时接收诊断。
 * @return 目标的所有现有父路径均可安全使用时返回 true。
 */
bool rejectSymlinkParents(const fs::path &root, const fs::path &relative,
                          Memory::String *error)
{
    fs::path current = root;
    for (const fs::path &part : relative.parent_path())
    {
        current /= part;
        std::error_code statusError;
        const fs::file_status status = fs::symlink_status(current, statusError);
        if (statusError && statusError != std::errc::no_such_file_or_directory)
        {
            *error = "无法检查编译器产物目录：" + current.string();
            return false;
        }
        if (!statusError && fs::is_symlink(status))
        {
            *error = "编译器产物目录不能经过符号链接：" + current.string();
            return false;
        }
    }
    return true;
}

/**
 * @brief 读取生成器清单并校验每个受管理文件名。
 * @param root 生成工程的根目录。
 * @param files 接收清单记录的规范化相对路径集合。
 * @param error 清单格式或路径无效时接收诊断。
 * @return 清单可完整读取且全部路径合法时返回 true。
 */
bool readManifest(const fs::path &root, Memory::Set<Memory::String> *files,
                  Memory::String *error)
{
    files->clear();
    const fs::path manifest = root / kManifestName;
    std::error_code statusError;
    if (!fs::exists(manifest, statusError)) return !statusError;
    std::ifstream input(manifest, std::ios::binary);
    Memory::String line;
    if (!input || !std::getline(input, line))
    {
        *error = "无法读取编译器产物清单：" + manifest.string();
        return false;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != kManifestHeader)
    {
        *error = "编译器产物清单版本无法识别：" + manifest.string();
        return false;
    }
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        fs::path relative;
        if (!safeRelative(line, &relative))
        {
            *error = "编译器产物清单包含不安全路径：" + line;
            return false;
        }
        files->insert(relative.generic_string());
    }
    return true;
}

/**
 * @brief 仅移除旧清单拥有且新工程不再保留的文件。
 * @param root 受管理工程的绝对根目录。
 * @param files 旧清单拥有的相对文件路径。
 * @param kept 需要继续保留的生成文件集合。
 * @param removed 接收实际删除的文件数量。
 * @param error 路径检查或文件删除失败时接收诊断。
 * @return 所有过期生成文件均已安全处理时返回 true。
 */
bool removeManaged(const fs::path &root, const Memory::Set<Memory::String> &files,
                   const Memory::Set<Memory::String> &kept, std::size_t *removed,
                   Memory::String *error)
{
    Memory::Vector<fs::path> parents;
    for (const Memory::String &name : files)
    {
        if (kept.count(name) != 0u) continue;
        fs::path relative;
        if (!safeRelative(name, &relative))
        {
            *error = "编译器产物清单包含不安全路径：" + name;
            return false;
        }
        if (!rejectSymlinkParents(root, relative, error)) return false;
        const fs::path target = root / relative;
        std::error_code statusError;
        const bool exists = fs::exists(target, statusError) || fs::is_symlink(target, statusError);
        if (statusError)
        {
            *error = "无法检查旧产物：" + target.string();
            return false;
        }
        if (exists)
        {
            std::error_code removeError;
            if (!fs::remove(target, removeError) || removeError)
            {
                *error = "无法删除旧产物：" + target.string();
                return false;
            }
            ++*removed;
        }
        for (fs::path parent = target.parent_path(); parent != root && !parent.empty();
             parent = parent.parent_path())
            parents.push_back(parent);
    }
    std::sort(parents.begin(), parents.end(), [](const fs::path &left, const fs::path &right) {
        return left.native().size() == right.native().size()
                   ? left.native() < right.native()
                   : left.native().size() > right.native().size();
    });
    parents.erase(std::unique(parents.begin(), parents.end()), parents.end());
    for (const fs::path &parent : parents)
    {
        std::error_code errorCode;
        if (fs::is_empty(parent, errorCode)) fs::remove(parent, errorCode);
    }
    return true;
}

/**
 * @brief 按清单写入独立编译器工程并保留用户文件。
 * @param root 目标工程的绝对根目录。
 * @param project 生成文件的相对路径及完整内容。
 * @param error 路径、写入或清理失败时接收诊断。
 * @return 全部文件及管理清单成功更新时返回 true。
 */
bool writeProject(const fs::path &root,
                  const Memory::Map<Memory::String, Memory::String> &project,
                  Memory::String *error)
{
    std::error_code directoryError;
    fs::create_directories(root, directoryError);
    if (directoryError)
    {
        *error = MAGEL_TEXT("无法创建输出目录：") + directoryError.message();
        return false;
    }
    Memory::Set<Memory::String> current;
    for (const auto &[name, content] : project)
    {
        (void)content;
        fs::path relative;
        if (!safeRelative(name, &relative))
        {
            *error = "不安全的编译器产物路径：" + name;
            return false;
        }
        if (!rejectSymlinkParents(root, relative, error)) return false;
        current.insert(relative.generic_string());
    }
    Memory::Set<Memory::String> old;
    if (!readManifest(root, &old, error)) return false;
    for (const auto &[name, content] : project)
    {
        const fs::path target = root / fs::path(name);
        fs::create_directories(target.parent_path(), directoryError);
        std::ofstream output(target, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            *error = "无法写入编译器产物：" + target.string();
            return false;
        }
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
    }
    std::size_t removed = 0u;
    if (!removeManaged(root, old, current, &removed, error)) return false;
    std::ofstream manifest(root / kManifestName, std::ios::binary | std::ios::trunc);
    if (!manifest)
    {
        *error = "无法写入编译器产物清单";
        return false;
    }
    manifest << kManifestHeader << '\n';
    for (const Memory::String &name : current) manifest << name << '\n';
    return static_cast<bool>(manifest);
}

/**
 * @brief 将单个工具参数编码为当前平台的 shell 字面量。
 * @param text 未经编码的单个参数。
 * @return 保持参数边界的转义及引用文本。
 */
Memory::String shellQuote(const Memory::String &text)
{
#if defined(_WIN32)
    Memory::String out = "\"";
    for (const char ch : text) out += ch == '"' ? "\\\"" : Memory::String(1u, ch);
    return out + "\"";
#else
    Memory::String out = "'";
    for (const char ch : text) out += ch == '\'' ? "'\\''" : Memory::String(1u, ch);
    return out + "'";
#endif
}

/* 把「文件在导出工程里的位置」换算成相对它的 `#include` 写法。
 * 例：文件在 `runtime/`、目标在 `runtime/magel_allocator.hpp` -> `magel_allocator.hpp`；
 *     文件在 `glue/`、目标在 `json/json.hpp`            -> `../json/json.hpp`。 */
/**
 * @brief 按导出文件位置计算其目标头文件的相对引用。
 * @param target 当前导出文件在工程中的逻辑路径。
 * @param destination 目标头文件在导出工程中的逻辑路径。
 * @return 从当前文件目录指向目标头的相对路径。
 */
Memory::String relativeInclude(const Memory::String &target, const char *destination)
{
    const std::string to(destination);
    std::string dir;
    const std::size_t slash = target.rfind('/');
    if (slash != Memory::String::npos) dir.assign(target.c_str(), slash);

    std::vector<std::string> from;
    std::vector<std::string> parts;
    if (!dir.empty())
        for (std::size_t at = 0;;)
        {
            const std::size_t next = dir.find('/', at);
            from.push_back(dir.substr(at, next == std::string::npos ? std::string::npos : next - at));
            if (next == std::string::npos) break;
            at = next + 1u;
        }
    for (std::size_t at = 0;;)
    {
        const std::size_t next = to.find('/', at);
        parts.push_back(to.substr(at, next == std::string::npos ? std::string::npos : next - at));
        if (next == std::string::npos) break;
        at = next + 1u;
    }
    std::size_t common = 0;
    while (common < from.size() && common + 1u < parts.size() && from[common] == parts[common]) ++common;
    std::string out;
    for (std::size_t i = common; i < from.size(); ++i) out += "../";
    for (std::size_t i = common; i < parts.size(); ++i)
    {
        out += parts[i];
        if (i + 1u < parts.size()) out += '/';
    }
    return Memory::String(out.c_str());
}

/**
 * @brief 使用当前平台 shell 启动一次外部生成工具。
 * @param arguments 程序路径及按顺序排列的独立参数。
 * @return 系统调用返回的进程执行状态。
 */
int run(const Memory::Vector<Memory::String> &arguments)
{
    Memory::String command;
#if defined(_WIN32)
    // cmd /c strips one outer quote pair; keep it separate from per-argument quoting.
    command = "\"";
#endif
    for (const Memory::String &argument : arguments)
    {
        if (!command.empty()) command += ' ';
        command += shellQuote(argument);
    }
#if defined(_WIN32)
    command += "\"";
#endif
    return std::system(command.c_str());
}

/**
 * @brief 打印独立编译器工程生成及清理命令的用法。
 * @param program 显示在用法示例中的程序名称。
 */
void usage(const Memory::String &program)
{
    std::cout << MAGEL_TEXT("用法：\n")
              << "  " << program << " compiler --out-dir DIR\n"
              << "  " << program << " clear --out-dir DIR\n";
}

} // namespace

namespace magel_generator
{

/**
 * @brief 解析生成器参数并执行工程导出或清单清理。
 * @param argc 命令行参数个数。
 * @param argv 程序名及生成器命令参数。
 * @return 成功返回零；生成失败或参数错误返回非零状态。
 */
int runCli(int argc, char **argv)
{
    const fs::path self = fs::absolute(argc > 0 ? argv[0] : "magel-generator").lexically_normal();
    fs::path root = self.parent_path().parent_path().parent_path();
    int commandIndex = 1;
    /* 统一入口在进程内调用时会显式给仓库根：`runCli({"magel-generator","--root",ROOT,"compiler",...})`。
     * 独立二进制的 argv[0] 在 build/magel-generator/ 下，往上三级正好是仓库根，不用传。 */
    if (argc > 2 && Memory::String(argv[1]) == "--root")
    {
        std::error_code rootError;
        root = fs::absolute(fs::path(argv[2]), rootError).lexically_normal();
        commandIndex = 3;
    }
    // 独立仓库的 build/magel-generator 位于 Magel 自己的根下。
    if (fs::exists(root / "source/magel.gram")) root = root.parent_path();
    const fs::path mc = root / "magel";
    const fs::path templateDir = mc / "source/generator";
    const fs::path parseEngine = root / "build/bin/ParseEngine.exe";
    const fs::path parseEnginePortable = root / "build/bin/ParseEngine";
    if (argc <= commandIndex)
    {
        usage(self.string());
        return 2;
    }
    const Memory::String command = argv[commandIndex];
    Memory::String outDir;
    Memory::Vector<Memory::String> forwarded;
    for (int index = commandIndex + 1; index < argc; ++index)
    {
        const Memory::String argument = argv[index];
        if (argument == "--out-dir" && index + 1 < argc)
        {
            outDir = argv[++index];
        }
        else
        {
            forwarded.push_back(argument);
        }
    }
    if (outDir.empty())
    {
        std::cerr << "必须指定 --out-dir DIR\n";
        return 2;
    }
    const fs::path output = fs::absolute(outDir).lexically_normal();
    if (command == "clear")
    {
        if (!forwarded.empty())
        {
            std::cerr << "clear 只接受 --out-dir\n";
            return 2;
        }
        Memory::String error;
        Memory::Set<Memory::String> files;
        if (!readManifest(output, &files, &error))
        {
            std::cerr << error << '\n';
            return 1;
        }
        std::size_t removed = 0u;
        if (!removeManaged(output, files, {}, &removed, &error))
        {
            std::cerr << error << '\n';
            return 1;
        }
        std::error_code removeError;
        fs::remove(output / kManifestName, removeError);
        std::cout << MAGEL_TEXT("清理完成，共删除 ") << removed << MAGEL_TEXT(" 个编译器文件\n");
        return 0;
    }
    if (command != "compiler")
    {
        usage(self.string());
        return 2;
    }
    if (!forwarded.empty()) { usage(self.string()); return 2; }

    fs::path parser = parseEnginePortable;
    const char *configuredEngine = std::getenv("ENGINE");
    if (configuredEngine != nullptr && *configuredEngine != '\0')
    {
        parser = fs::u8path(configuredEngine);
    }
    else
    {
        for (const auto &candidate : {mc / "ParseEngine.exe", mc / "ParseEngine",
                root / "ParseEngine/build/bin/ParseEngine.exe", root / "ParseEngine/build/bin/ParseEngine",
                parseEngine, parseEnginePortable}) {
            if (fs::exists(candidate)) { parser = candidate; break; }
        }
    }
    if (!fs::exists(parser))
    {
        std::cerr << "找不到 ParseEngine：" << parser.string() << '\n';
        return 2;
    }
    const fs::path stage = root / "tmp" /
        ("magel-generator-" + Memory::toString(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code stageError;
    fs::create_directories(stage, stageError);
    Memory::String expandedGrammar, grammarError;
    Memory::Set<fs::path> grammarParents;
    if (!expandGrammarIncludes(mc / "source/magel.gram", &expandedGrammar, &grammarParents, &grammarError))
    {
        std::cerr << grammarError << '\n';
        fs::remove_all(stage, stageError);
        return 1;
    }
    const auto grammarPath = stage / "magel.gram";
    {
        std::ofstream grammar(grammarPath, std::ios::binary);
        grammar << expandedGrammar;
        if (!grammar)
        {
            std::cerr << "无法写入展开文法：" << grammarPath.u8string() << '\n';
            fs::remove_all(stage, stageError);
            return 1;
        }
    }
    const int generated = run({parser.string(), grammarPath.string(), "--mode", "ielr",
                               "--cpp-lexer", "std", "--no-build-files", "--out-dir", stage.string()});
    if (generated != 0)
    {
        fs::remove_all(stage, stageError);
        return 1;
    }

    Memory::Map<Memory::String, Memory::String> project;
    /* 复制到导出工程会把源文件摊平：源文件里指向仓库原布局的相对 `#include` 必须
     * 改写成新布局下的相对路径。表给出「原写法 -> 该文件在导出工程里的位置」，
     * 匹配一律带引号，且长串排在前面。 */
    const auto rewriteIncludes = [](Memory::String *content, const Memory::String &target) {
        struct Fix { const char *from; const char *to; };
        static const Fix fixes[] = {
            {"../../library/ipage/source/ipage/sql.hpp", "ipage/sql.hpp"},
            {"../../library/ipage/source/ipage/dialect.hpp", "ipage/dialect.hpp"},
            {"../../library/ipage/source/ipage/model.hpp", "ipage/model.hpp"},
            {"../../library/caspp/core_assets.hpp", "caspp/core_assets.hpp"},
            {"../../library/allocator/magel_allocator.hpp", "runtime/magel_allocator.hpp"},
            {"../../source/allocator/magel_allocator.hpp", "runtime/magel_allocator.hpp"},
            {"../library/allocator/magel_allocator.hpp", "runtime/magel_allocator.hpp"},
            {"../allocator/magel_allocator.hpp", "runtime/magel_allocator.hpp"},
            {"allocator/magel_allocator.hpp", "runtime/magel_allocator.hpp"},
            {"../../source/magel_i18n.hpp", "magel_i18n.hpp"},
            {"../magel_i18n.hpp", "magel_i18n.hpp"},
            {"../library/stackvm/value_text.hpp", "runtime/value_text.hpp"},
            {"../../library/stackvm/stackvm.hpp", "runtime/stackvm.hpp"},
            {"../library/stackvm/stackvm.hpp", "runtime/stackvm.hpp"},
            {"../../library/sqlite/sqlite3.h", "sqlite/sqlite3.h"},
            {"../../library/lztext/lztext.hpp", "lztext/lztext.hpp"},
            {"../../library/template/engine.hpp", "template/engine.hpp"},
            {"../library/template/engine.hpp", "template/engine.hpp"},
            {"../../library/miniz/miniz.h", "miniz/miniz.h"},
            {"../library/miniz/miniz.h", "miniz/miniz.h"},
            {"../../library/json/json.hpp", "json/json.hpp"},
            {"../json/json.hpp", "json/json.hpp"},
            {"../../library/json/json.cpp", "json/json.cpp"},
            {"../json/json.cpp", "json/json.cpp"},
            {"../../library/toml/toml.hpp", "toml/toml.hpp"},
            {"../toml/toml.hpp", "toml/toml.hpp"},
            {"../../library/toml/toml.cpp", "toml/toml.cpp"},
            {"../toml/toml.cpp", "toml/toml.cpp"},
            {"../../library/toml/toml_source.cpp", "toml/toml_source.cpp"},
            {"../toml/toml_source.cpp", "toml/toml_source.cpp"},
            {"../../library/toml/toml_source.hpp", "toml/toml_source.hpp"},
            {"../toml/toml_source.hpp", "toml/toml_source.hpp"},
        };
        for (const Fix &fix : fixes) {
            /* 只在 `#include "..."` 的引号里匹配：否则裸的 `allocator/magel_allocator.hpp`
             * 会连带命中 `"../../other/allocator/magel_allocator.hpp"` 的尾巴。 */
            const Memory::String needle = Memory::String("\"") + fix.from + "\"";
            const Memory::String value = Memory::String("\"") + relativeInclude(target, fix.to) + "\"";
            if (needle == value) continue;
            std::size_t at = 0;
            while ((at = content->find(needle, at)) != Memory::String::npos) {
                content->replace(at, needle.size(), value);
                at += value.size();
            }
        }
    };
    const auto copy = [&](const fs::path &source, const Memory::String &target) -> bool {
        Memory::String content;
        if (!readFile(source, &content)) return false;
        rewriteIncludes(&content, target);
        project[target] = content;
        return true;
    };
    bool ok = true;
    for (const char *name : {"magel_parser.cpp", "magel_parser.hpp",
                             "magel_parser_user.h", "magel_main.cpp", "RESULT.md"})
        ok = copy(stage / name, name) && ok;
    for (const char *name : {"magel_driver.cpp", "magel_i18n.hpp", "magel_diagnostics.hpp", "magel_borrow_checker.hpp", "magel_borrow_flow.hpp", "magel_resource_cleanup.hpp", "magel_resource_cleanup.cpp", "magel_module.hpp",
                             "magel_incremental.hpp", "magel_package.hpp", "magel_project.hpp", "magel_cli.hpp",
                             "magel_lambda.hpp", "magel_generic.hpp", "magel_generic_name.hpp", "magel_glue.hpp",
                             "magel_stdlib_embedded.cpp", "magel_stdlib.hpp", "magel_generic.cpp", "magel_lambda.cpp", "magel_generic_closure.cpp", "magel_package.cpp", "magel_report.cpp", "magel_runtime_allocator.hpp",
                             "magel_ast.hpp", "magel_semantics.cpp"})
        ok = copy(mc / "source" / name, name) && ok;
    /* 控制台 UTF-8 头来自 `library/console/`，导出工程里仍放 `console/` 的目录名：副本站 `console/utf8.hpp`，
     * `magel_i18n.hpp` 里也是 `#include "console/utf8.hpp"`，导出工程保持同一相对布局。 */
    ok = copy(mc / "library/console/utf8.hpp", "console/utf8.hpp") && ok;
    /* `magel_semantics.cpp` 按职责拆成 `semantics/*.cpp`，它用相对 `#include`
     * 拼回；生成工程必须把分片一起带过去（编译 `magel_parser.cpp` 时按相对路径找到）。 */
    for (const char *name : {"symbol_table.cpp", "type_ids.cpp", "type_ids_shared.cpp",
                             "lifetime.cpp", "aggregate_ownership.cpp", "resource_lifetime.cpp", "scope.cpp",
                             "return_expression_ast.cpp", "named_function_ast.cpp", "derive_ast.cpp", "derive.cpp",
                             "types.cpp", "interfaces.cpp", "formatted_output.cpp", "builtins.cpp", "generic_inference.cpp", "constants.cpp", "annotation_ast.cpp", "annotations.cpp", "reflection_metadata.cpp", "reflection_queries.cpp", "reflection_calls.cpp", "reflection_serialization.cpp",
                             "lowering.cpp", "borrow_facts.cpp", "borrow_flow.cpp", "borrow_return_sources.cpp", "expression_effects.cpp", "interface_dispatch.cpp", "boxing.cpp",
                             "symbol_registration.cpp", "program_lowering.cpp", "artifacts.cpp"})
        ok = copy(mc / "source/semantics" / name, Memory::String("semantics/") + name) && ok;
    const Memory::Map<Memory::String, fs::path> runtime{
        {"magel_allocator.hpp", root / "magel/source/allocator/magel_allocator.hpp"},
        {"magel_pool.h", root / "magel/source/allocator/magel_pool.h"},
        {"magel_pool.c", root / "magel/source/allocator/magel_pool.c"},
        {"simple_ast.cpp", root / "magel/library/simple_ast/simple_ast.cpp"},
        {"simple_ast.hpp", root / "magel/library/simple_ast/simple_ast.hpp"},
        {"preprocess.cpp", root / "magel/library/preprocess/preprocess.cpp"},
        {"preprocess.hpp", root / "magel/library/preprocess/preprocess.hpp"},
        {"bigint.cpp", root / "magel/library/bignum/bigint.cpp"},
        {"bigint.hpp", root / "magel/library/bignum/bigint.hpp"},
        {"bigfloat.cpp", root / "magel/library/bignum/bigfloat.cpp"},
        {"bigfloat.hpp", root / "magel/library/bignum/bigfloat.hpp"},
        {"bigfloat_math.hpp", root / "magel/library/bignum/bigfloat_math.hpp"},
        {"bigfloat_math_detail.hpp", root / "magel/library/bignum/bigfloat_math_detail.hpp"},
        {"bigfloat_transcendental.cpp", root / "magel/library/bignum/bigfloat_transcendental.cpp"},
        {"bigfloat_trigonometry.cpp", root / "magel/library/bignum/bigfloat_trigonometry.cpp"},
    };
    for (const char *name : {"ast_type.cpp", "ast_node.cpp", "ast_data.cpp", "ast_storage_detail.hpp"})
        ok = copy(root / "magel/library/simple_ast" / name, Memory::String("runtime/") + name) && ok;
    for (const char *name : {"bigint_multiplication.cpp", "bigfloat_roots.cpp", "bigint_division.cpp", "bigint_number_theory.cpp", "bigint_bits.cpp", "bigint_modular.cpp", "bigint_conversion.cpp"})
        ok = copy(root / "magel/library/bignum" / name, Memory::String("runtime/") + name) && ok;
    for (const auto &[name, source] : runtime) ok = copy(source, "runtime/" + name) && ok;
    for (const char *name : stackvm::assets::kCoreAssets)
        ok = copy(root / "magel/library/stackvm" / name, Memory::String("runtime/") + name) && ok;
    for (const char *name : Cas::Assets::kCoreAssets)
        ok = copy(root / "magel/library/caspp" / name, Memory::String("caspp/") + name) && ok;
    for (const char *name : {"sql.hpp", "sql.cpp", "dialect.hpp", "dialect.cpp", "model.hpp", "field.hpp"})
        ok = copy(root / "magel/library/ipage/source/ipage" / name, Memory::String("ipage/") + name) && ok;
    ok = copy(root / "magel/library/ipage/LICENSE", "ipage/LICENSE") && ok;
    for (const char *name : {"toml_source.hpp", "toml_source.cpp"})
        ok = copy(root / "magel/library/toml" / name, Memory::String("toml/") + name) && ok;
    for (const char *format : {"json", "toml"}) {
        for (const char *extension : {"hpp", "cpp"}) {
            const Memory::String relative = Memory::String(format) + "/" + format + "." + extension;
            ok = copy(root / "magel/library" / relative, relative) && ok;
        }
    }
    for (const char *name : {"std_math.hpp", "std_random.hpp", "std_regex.hpp",
                             "std_native_record.hpp", "std_big_number.hpp", "std_file.hpp", "std_file_api.hpp", "std_desktop_file.hpp",
                             "std_signal.hpp",
                             "std_structured_data.hpp", "std_json.hpp", "std_toml.hpp", "std_numeric.hpp", "std_binary.hpp", "std_linalg.hpp", "std_parse.hpp",
                             "std_process.hpp", "std_sql.hpp", "std_net.hpp", "std_filesystem.hpp", "std_registry.hpp"})
        ok = copy(root / "magel/source/glue" / name, Memory::String("glue/") + name) && ok;
    // 前端头和标准胶水随实现扩展；按受控目录收集，避免固定清单遗漏新依赖。
    for (const auto &entry : fs::directory_iterator(mc / "source"))
    {
        const auto name = entry.path().filename().u8string();
        const auto extension = entry.path().extension().string();
        if (entry.is_regular_file() && name.rfind("magel_parser", 0) != 0 &&
            (extension == ".hpp" || extension == ".h" || extension == ".inc"))
            ok = copy(entry.path(), name) && ok;
    }
    for (const auto &entry : fs::directory_iterator(mc / "source/glue"))
    {
        const auto extension = entry.path().extension().string();
        if (entry.is_regular_file() && (extension == ".hpp" || extension == ".h" || extension == ".cpp"))
            ok = copy(entry.path(), "glue/" + entry.path().filename().u8string()) && ok;
    }
    for (const auto &entry : fs::directory_iterator(mc / "source/std"))
        if (entry.is_regular_file() && entry.path().extension() == ".mgl")
            ok = copy(entry.path(), "std/" + entry.path().filename().u8string()) && ok;
    for (const auto &entry : fs::directory_iterator(mc / "source/semantics"))
    {
        const auto extension = entry.path().extension().string();
        if (entry.is_regular_file() && (extension == ".hpp" || extension == ".h" ||
                                      extension == ".cpp" || extension == ".inc"))
            ok = copy(entry.path(), "semantics/" + entry.path().filename().u8string()) && ok;
    }
    for (const char *directory : {"macro", "literals", "syntax", "editor"})
        for (const auto &entry : fs::directory_iterator(mc / "source" / directory))
        {
            const auto extension = entry.path().extension().string();
            if (entry.is_regular_file() && (extension == ".hpp" || extension == ".cpp"))
                ok = copy(entry.path(), Memory::String(directory) + "/" + entry.path().filename().u8string()) && ok;
        }
    for (const char *name : {"dap_server.hpp", "dap_server.cpp"})
        ok = copy(mc / "source/dap" / name, Memory::String("dap/") + name) && ok;
    for (const char *directory : {"lztext", "miniz", "sqlite", "template"})
    {
        for (const auto &entry : fs::directory_iterator(mc / "library" / directory))
        {
            const auto extension = entry.path().extension().string();
            if (entry.is_regular_file() && (extension == ".hpp" || extension == ".h" || extension == ".c" || extension == ".cpp"))
                ok = copy(entry.path(), Memory::String(directory) + "/" + entry.path().filename().u8string()) && ok;
        }
    }
    Memory::String langSource;
    Memory::String langTemplate;
    Memory::String cmakeTemplate;
    Memory::String readmeTemplate;
    if (!readFile(mc / "source/std/lang.mgl", &langSource) ||
        !readFile(templateDir / "compiler_lang.mgl.tmpl", &langTemplate) ||
        !readFile(templateDir / "compiler_cmake_lists.txt.tmpl", &cmakeTemplate) ||
        !readFile(templateDir / "compiler_readme.md.tmpl", &readmeTemplate))
        ok = false;
    try
    {
        tmpl::SnippetTable snippets;
        snippets.emplace("magel_lang", langSource);
        project["std/lang.mgl"] = tmpl::renderValues(
            langTemplate, {}, snippets);
        project["CMakeLists.txt"] = tmpl::renderValues(
            cmakeTemplate);
        project["README.md"] = tmpl::renderValues(
            readmeTemplate);
    }
    catch (const std::exception &exception)
    {
        std::cerr << MAGEL_TEXT("编译器模板渲染失败：") << exception.what() << '\n';
        ok = false;
    }
    fs::remove_all(stage, stageError);
    if (!ok)
    {
        std::cerr << MAGEL_TEXT("读取编译器源文件失败\n");
        return 1;
    }
    Memory::String error;
    if (!writeProject(output, project, &error))
    {
        std::cerr << MAGEL_TEXT("编译器工程导出失败：") << error << '\n';
        return 1;
    }
    std::cout << MAGEL_TEXT("已生成 Magel 编译器工程：") << output.string() << "（"
              << project.size() << MAGEL_TEXT(" 个文件，lang.mgl 来自引用模板）\n");
    return 0;
}

} // namespace magel_generator

/* 独立 `magel-generator` 二进制自带 main；统一入口在进程内调用 `magel_generator::runCli`，
 * 构建时用 `-DMAGEL_GENERATOR_NO_MAIN` 关掉这里。 */
#ifndef MAGEL_GENERATOR_NO_MAIN
/**
 * @brief 配置显示语言并进入独立生成器的命令分派。
 * @param argc 宿主传入的命令行参数个数。
 * @param argv 宿主传入的程序名与参数数组。
 * @return 语言配置或生成器命令的执行状态。
 */
int main(int argc, char **argv) {
    auto arguments=magel_i18n::processArguments(argc,argv);
    std::string error;
    if(!magel_i18n::configure(arguments,error)){std::fprintf(stderr,"magel: %s\n",error.c_str());return 2;}
    std::vector<char*> values;
    for(auto &argument:arguments)values.push_back(argument.data());
    values.push_back(nullptr);
    return magel_generator::runCli(static_cast<int>(arguments.size()),values.data());
}
#endif
