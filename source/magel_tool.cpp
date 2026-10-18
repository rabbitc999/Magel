/**
 * @file magel_tool.cpp
 * @brief Magel 统一工具链入口：把前端（解释 / 出四元式 / 出 `.svmb`）、
 *        `magel-emit` 的 AOT 导出以及 `magel-generator` 的工程 / 清理合并到**一个**
 *        `magel` 二进制里。
 *
 * 运行方式（magel.toml 的 `[magel] run` 或缺省）：
 *   interpret  内部解释运行（缺省，行为与老 `magel` 完全一致）
 *   svmb       只导出 `<前缀>.svmb`，不执行
 *   aot        导出 AOT 可执行文件（`magel-emit --target cpp-aot`）
 *
 * 选择顺序：子命令 / `--mode`  >  magel.toml 的 `run`  >  interpret。
 * 子命令：run|interpret、emit、svmb|smvb、aot、new、gen、clear。
 *
 * 这个文件是**手写**的（不经过 ParseEngine 生成）；`magel_driver.cpp` 里的
 * `magel_main_entry` 才是前端主体，本文件只负责派发。统一构建在 `magel_main.o`
 * 上定义 `MAGEL_FRONT_END_NO_MAIN`，所以真正的 `main` 只有这里一份。
 */
#include "runtime_allocator.hpp"

#include "lsp/server.hpp"
#include "tools/native_development.hpp"
#include "tools/abi_import.hpp"

#include "magel_package.hpp"
#include "magel_project.hpp"
#include "magel_cli.hpp"
#include "magel-emit/emit_cli.hpp"
#include "magel-generator/generator_cli.hpp"

#include <chrono>
#include <cerrno>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

/* 由生成的 `magel_main.cpp`（`%main` 正文）提供；见 magel_driver.cpp。 */
int magel_main_entry(int argc, char **argv);
const char *magel_development_tool(const char *name);

namespace
{

namespace fs = std::filesystem;

const char *const kModeNames[] = { "interpret", "svmb", "aot" };

/**
 * @brief 判断命令行文本是否为支持的运行模式。
 * @param mode 文件访问或工具运行模式。
 * @retval 命令行文本为支持的运行模式时为 true，否则为 false。
 */
bool isKnownMode(const std::string &mode)
{
    for (const char *name : kModeNames)
        if (mode == name)
            return true;
    return false;
}

/**
 * @brief 判断前端选项是否需要后续参数值。
 * @param a 待检查的命令行选项文本。
 * @retval 前端选项需要后续参数值时为 true，否则为 false。
 */
bool frontEndTakesValue(const std::string &a)
{
    return a == "--abi-cache" || a == "--abi-bridge" || a == "--abi-write-bridge" || a == "--out" || a == "--input" || a == "--memory-size" || a == "--step-limit" || a == "--include-dir" || a == "--include-path" || a == "--import-dir" || a == "--define" || a == "--symbol-map" || a == "-I" || a == "-D" || a == "--max-restarts" || a == "--watchdog-watermark" || a == "--cache-dir" || a == "--source-overlays" || a == "--analysis-deps";
}

/**
 * @brief 判断导出器选项是否需要后续参数值。
 * @param a 待检查的命令行选项文本。
 * @retval 导出器选项需要后续参数值时为 true，否则为 false。
 */
bool emitTakesValue(const std::string &a)
{
    return a == "--module-cache" || a == "--opt-level" || a == "--runtime-arena" || a == "--vm-dir" || a == "--magel-dir" || a == "--bigint-dir" || a == "--stack-slots" || a == "--name" || a == "--target" || a == "--quad" || a == "--data" || a == "--layout" || a == "--out-dir" || a == "--glue";
}

/**
 * @brief 判断选项是否属于编译前端。
 * @param a 待检查的命令行选项文本。
 * @retval 选项属于编译前端时为 true，否则为 false。
 */
bool frontEndFlag(const std::string &a)
{
    return a == "--abi-no-cache" || a == "--abi-prepare" || a == "--source-stdin" || a == "--hard-realtime" || a == "--reflection" || a == "--no-reflection" || a == "--dap" || a == "--check" || a == "--diagnostics=json" || a == "--raw" || a == "--deps" || a == "--incremental" || a == "--stats" || a == "--watchdog" || a == "--no-watchdog" || a == "--emit" || a == "--svmb" || a == "--compress-symbols" || a == "--no-compress-symbols" || a == "-h" || a == "--help";
}

/**
 * @brief 判断选项是否仅由代码导出器处理。
 * @param a 待检查的命令行选项文本。
 * @retval 选项仅由代码导出器处理时为 true，否则为 false。
 */
bool emitOnlyFlag(const std::string &a)
{
    return a == "--shared" || a == "--object" || a == "--checked" || a == "--unchecked" || a == "--separate-modules" || a == "--keep-intermediate" || a == "--clean" || a == "--release" || a == "--no-opt";
}

/**
 * @brief 判断选项是否由前端与导出器共同使用。
 * @param a 待检查的命令行选项文本。
 * @retval 选项由前端与导出器共同使用时为 true，否则为 false。
 */
bool sharedFlag(const std::string &a)
{
    return a == "--threads" || a == "--debug" || a == "--no-debug";
}

/**
 * @brief 提取文件路径中的父目录。
 * @param path 目标文件或目录的路径。
 * @retval 生成或读取的结果文本。
 */
std::string dirOf(const std::string &path)
{
    const fs::path p = fs::u8path(path);
    return p.has_parent_path() ? p.parent_path().u8string() : std::string(".");
}

/**
 * @brief 从可执行文件目录与当前目录向上找仓库根（含 `magel/source/magel-emit/aot.cpp`）。
 * @param argv0 命令行中的程序路径。
 * @retval 对应对象的指针。
 */
fs::path findRepoRoot(const char *argv0)
{
    std::error_code ec;
    const fs::path self = fs::absolute(fs::u8path(argv0 ? argv0 : "magel"), ec);
    const std::vector<fs::path> starts{ self.parent_path(), fs::current_path(ec) };
    fs::path fallback;
    for (fs::path current : starts)
    {
        while (!current.empty())
        {
            if (fs::exists(current / "magel" / "source" / "magel-emit" / "aot.cpp", ec))
            {
                if (fallback.empty()) fallback = current;
                // build/work 含资源副本；优先实际仓库，生成器才能找到 build/bin。
                if (fs::exists(current / ".git", ec)) return current.lexically_normal();
            }
            const fs::path parent = current.parent_path();
            if (parent == current) break;
            current = parent;
        }
    }
    return fallback.lexically_normal();
}

struct Scan
{
    std::vector<std::string> frontEnd;  ///< 给前端的参数（含 `--out` / 位置参数）
    std::vector<std::string> emit;      ///< 给 magel-emit 的参数（导出目标相关）
    std::vector<std::string> rest;      ///< 去掉子命令后的原始参数（gen/clear 转发用）
    std::string outPath;
    std::string executeOutput;                    ///< `--execute-output FILE`：可执行文件位置
    std::string cppOutputDir;                     ///< `--cpp-output-dir DIR`：生成的 C++ 写到哪
    std::string cxx, cc, makeTool, gccToolchain;  ///< `--cxx` / `--cc` / `--make`：工具链位置
    std::string source;
    std::string optLevel;
    std::string cliMode;
    bool keep = false;
    bool threads = false;
    bool wantDebug = false;
    bool release = false;
    bool cppOutput = false;     ///< `--cpp-output`：导出可执行文件时是否保留 C++
    bool outPathGiven = false;  ///< 命令行是否显式给了 `--out`
    bool help = false;
};

/**
 * @brief 解析统一工具链参数并划分各阶段选项。
 * @param args 按接口约定顺序传入的实参列表。
 * @param scan 接收命令行解析结果的对象。
 * @param error 接收失败原因的错误信息。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool scanArguments(const std::vector<std::string> &args, Scan *scan, std::string *error)
{
    const std::size_t count = args.size();
    for (std::size_t index = 0; index < count;)
    {
        const std::string &argument = args[index];
        if (argument == "--")
        {
            scan->frontEnd.push_back(argument);
            for (++index; index < count; ++index)
                scan->frontEnd.push_back(args[index]);
            break;
        }
        if (argument == "--mode" || argument == "--run-mode")
        {
            if (index + 1 >= count)
            {
                *error = argument + MAGEL_TEXT(" 缺少取值");
                return false;
            }
            scan->cliMode = args[++index];
            ++index;
            continue;
        }
        if (argument.rfind("--mode=", 0) == 0)
        {
            scan->cliMode = argument.substr(7);
            ++index;
            continue;
        }
        if (argument.rfind("--run-mode=", 0) == 0)
        {
            scan->cliMode = argument.substr(11);
            ++index;
            continue;
        }
        if (argument.rfind("--opt-level=", 0) == 0)
        {
            scan->emit.push_back(argument);
            scan->optLevel = argument.substr(12);
            ++index;
            continue;
        }
        if (argument.size() > 2 && argument[0] == '-' && argument[1] == 'O')
        {
            scan->emit.push_back(argument);
            scan->optLevel = argument;
            ++index;
            continue;
        }
        /* `--cpp-output` / `--cpp-output-dir` / `--execute-output`：只影响 aot
         * 导出，不往前端透传；具体拼给 magel-emit 或自己编译时用。 */
        if (argument == "--cpp-output" || argument == "--cpp-output=true")
        {
            scan->cppOutput = true;
            ++index;
            continue;
        }
        if (argument == "--no-cpp-output" || argument == "--cpp-output=false")
        {
            scan->cppOutput = false;
            ++index;
            continue;
        }
        if (argument == "--cpp-output-dir" || argument == "--execute-output")
        {
            if (index + 1 >= count)
            {
                *error = argument + MAGEL_TEXT(" 缺少取值");
                return false;
            }
            const std::string value = args[index + 1];
            if (argument == "--cpp-output-dir")
            {
                scan->cppOutputDir = value;
                scan->cppOutput = true;
            }
            else
                scan->executeOutput = value;
            index += 2;
            continue;
        }
        if (argument.rfind("--cpp-output-dir=", 0) == 0)
        {
            scan->cppOutputDir = argument.substr(17);
            scan->cppOutput = true;
            ++index;
            continue;
        }
        if (argument.rfind("--execute-output=", 0) == 0)
        {
            scan->executeOutput = argument.substr(17);
            ++index;
            continue;
        }
        if (argument == "--gcc-toolchain")
        {
            if (index + 1 >= count)
            {
                *error = "--gcc-toolchain 缺少目录、路径或命令";
                return false;
            }
            scan->gccToolchain = args[index + 1];
            index += 2;
            continue;
        }
        if (argument.rfind("--gcc-toolchain=", 0) == 0)
        {
            scan->gccToolchain = argument.substr(16);
            ++index;
            continue;
        }
        /* `--cc` / `--cxx` / `--make`（以及 `--gcc` / `--g++` / `--gxx` 别名）：工具链
         * 位置，只在 aot 导出与构建脚本里用，不往前端透传。 */
        if (argument == "--cc" || argument == "--cxx" || argument == "--make" || argument == "--gcc" || argument == "--g++" || argument == "--gxx")
        {
            if (index + 1 >= count)
            {
                *error = argument + MAGEL_TEXT(" 缺少取值");
                return false;
            }
            const std::string value = args[index + 1];
            if (argument == "--cc" || argument == "--gcc")
                scan->cc = value;
            else if (argument == "--cxx" || argument == "--g++" || argument == "--gxx")
                scan->cxx = value;
            else
                scan->makeTool = value;
            index += 2;
            continue;
        }
        if (argument.rfind("--cc=", 0) == 0)
        {
            scan->cc = argument.substr(5);
            ++index;
            continue;
        }
        if (argument.rfind("--gcc=", 0) == 0)
        {
            scan->cc = argument.substr(6);
            ++index;
            continue;
        }
        if (argument.rfind("--cxx=", 0) == 0)
        {
            scan->cxx = argument.substr(6);
            ++index;
            continue;
        }
        if (argument.rfind("--g++=", 0) == 0)
        {
            scan->cxx = argument.substr(6);
            ++index;
            continue;
        }
        if (argument.rfind("--gxx=", 0) == 0)
        {
            scan->cxx = argument.substr(6);
            ++index;
            continue;
        }
        if (argument.rfind("--make=", 0) == 0)
        {
            scan->makeTool = argument.substr(7);
            ++index;
            continue;
        }
        if (argument.size() > 2 && argument[0] == '-' && (argument[1] == 'I' || argument[1] == 'D'))
        {
            scan->frontEnd.push_back(argument);
            ++index;
            continue;
        }
        const bool feValue = frontEndTakesValue(argument);
        const bool emValue = emitTakesValue(argument);
        if (feValue || emValue)
        {
            if (index + 1 >= count)
            {
                *error = argument + MAGEL_TEXT(" 缺少取值");
                return false;
            }
            const std::string value = args[index + 1];
            if (argument == "--out")
            {
                scan->outPath = value;
                scan->outPathGiven = true;
            }
            if (argument == "--opt-level") scan->optLevel = value;
            if (feValue)
            {
                scan->frontEnd.push_back(argument);
                scan->frontEnd.push_back(value);
            }
            if (emValue)
            {
                scan->emit.push_back(argument);
                scan->emit.push_back(value);
            }
            index += 2;
            continue;
        }
        if (frontEndFlag(argument))
        {
            scan->frontEnd.push_back(argument);
            scan->help = scan->help || argument == "-h" || argument == "--help";
            ++index;
            continue;
        }
        if (emitOnlyFlag(argument))
        {
            scan->emit.push_back(argument);
            if (argument == "--keep-intermediate") scan->keep = true;
            if (argument == "--release") scan->release = true;
            ++index;
            continue;
        }
        if (sharedFlag(argument))
        {
            scan->frontEnd.push_back(argument);
            scan->emit.push_back(argument);
            if (argument == "--threads") scan->threads = true;
            if (argument == "--debug") scan->wantDebug = true;
            ++index;
            continue;
        }
        if (!argument.empty() && argument[0] == '-')
        {
            *error = MAGEL_TEXT("不认识的选项：") + argument;
            return false;
        }
        if (scan->source.empty())
            scan->source = argument;
        else
        {
            *error = MAGEL_TEXT("一次只能编译一个源文件");
            return false;
        }
        scan->frontEnd.push_back(argument);
        ++index;
    }
    return true;
}

/**
 * @brief 将字符串参数集合转换为 C 风格参数指针数组。
 * @param program 待生成或执行的程序。
 * @param arguments 按调用顺序保存的实参列表。
 * @retval 按处理顺序保存的结果元素集合。
 */
std::vector<char *> toArgv(const std::string &program, const std::vector<std::string> &arguments)
{
    std::vector<char *> argv;
    argv.reserve(arguments.size() + 2);
    argv.push_back(const_cast<char *>(program.c_str()));
    for (const std::string &argument : arguments)
        argv.push_back(const_cast<char *>(argument.c_str()));
    argv.push_back(nullptr);
    return argv;
}

/**
 * @brief 调用统一工具链的编译前端。
 * @param arguments 按调用顺序保存的实参列表。
 * @retval 进程退出状态，0 表示成功，非零表示失败。
 */
int runFrontEnd(const std::vector<std::string> &arguments)
{
    std::vector<char *> argv = toArgv("magel", arguments);
    return magel_main_entry(static_cast<int>(argv.size() - 1), argv.data());
}

/**
 * @brief 调用统一工具链的代码导出器。
 * @param arguments 按调用顺序保存的实参列表。
 * @retval 进程退出状态，0 表示成功，非零表示失败。
 */
int runEmitter(const std::vector<std::string> &arguments)
{
    std::vector<char *> argv = toArgv("magel-emit", arguments);
    return magel_emit::runCli(static_cast<int>(argv.size() - 1), argv.data());
}

/**
 * @brief 调用统一工具链的工程生成器。
 * @param arguments 按调用顺序保存的实参列表。
 * @retval 进程退出状态，0 表示成功，非零表示失败。
 */
int runGenerator(const std::vector<std::string> &arguments)
{
    std::vector<char *> argv = toArgv("magel-generator", arguments);
    return magel_generator::runCli(static_cast<int>(argv.size() - 1), argv.data());
}

const char *const kToolManifestName = ".magel-tool-manifest";
const char *const kToolManifestHeader = "# Magel toolchain files v1";

/**
 * @brief 清单里只允许单层文件名：拒绝目录分隔、`..` 与保留名，避免路径逃逸。
 * @param name 名称文本。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool simpleManifestName(const std::string &name)
{
    if (name.empty() || name == "." || name == ".." || name == kToolManifestName)
        return false;
    return name.find('/') == std::string::npos && name.find('\\') == std::string::npos && name.find('\0') == std::string::npos;
}

/**
 * @brief 写入本次工具链生成文件的清单。
 * @param root 工作区根目录或根语法对象。
 * @param names 生成文件的名称列表。
 * @param error 接收失败原因的错误信息。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool writeToolManifest(const fs::path &root, const std::vector<std::string> &names,
                       std::string *error)
{
    std::ofstream out(root / kToolManifestName, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        *error = "写不了统一工具清单：" + (root / kToolManifestName).u8string();
        return false;
    }
    out << kToolManifestHeader << "\n";
    for (const std::string &name : names)
        if (simpleManifestName(name)) out << name << "\n";
    return static_cast<bool>(out);
}

/**
 * @brief 读取工具链生成文件清单。
 * @param root 工作区根目录或根语法对象。
 * @param names 生成文件的名称列表。
 * @param error 接收失败原因的错误信息。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool readToolManifest(const fs::path &root, std::vector<std::string> *names, std::string *error)
{
    std::ifstream input(root / kToolManifestName, std::ios::binary);
    if (!input) return true; /* 没有清单不是错误。 */
    std::string line;
    if (!std::getline(input, line))
    {
        *error = "读不了统一工具清单";
        return false;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != kToolManifestHeader)
    {
        *error = "统一工具清单版本无法识别";
        return false;
    }
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (!simpleManifestName(line))
        {
            *error = "统一工具清单包含不安全路径：" + line;
            return false;
        }
        names->push_back(line);
    }
    return true;
}

/**
 * @brief 按清单删除工具链生成的中间产物。
 * @param root 工作区根目录或根语法对象。
 * @param names 生成文件的名称列表。
 * @param removed 接收实际删除的文件数量。
 * @param error 接收失败原因的错误信息。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool deleteToolFiles(const fs::path &root, const std::vector<std::string> &names,
                     std::size_t *removed, std::string *error)
{
    std::error_code ec;
    if (fs::is_symlink(fs::symlink_status(root, ec)))
    {
        *error = MAGEL_TEXT("输出目录不能是符号链接：") + root.u8string();
        return false;
    }
    for (const std::string &name : names)
    {
        if (!simpleManifestName(name))
        {
            *error = MAGEL_TEXT("不安全路径：") + name;
            return false;
        }
        const fs::path target = root / name;
        const fs::file_status status = fs::symlink_status(target, ec);
        if (ec && ec != std::errc::no_such_file_or_directory)
        {
            *error = MAGEL_TEXT("无法检查产物：") + target.u8string();
            return false;
        }
        if (fs::exists(status) || fs::is_symlink(status))
        {
            std::error_code removeError;
            fs::remove(target, removeError);
            if (removeError)
            {
                *error = MAGEL_TEXT("无法删除产物：") + target.u8string();
                return false;
            }
            ++*removed;
        }
    }
    fs::remove(root / kToolManifestName, ec);
    return true;
}

}  // namespace

/**
 * @brief 将内部前端选项插到程序参数分隔符之前。
 * @param args 完整参数。
 * @param options 新选项。
 * @return 无。
 */
void AppendCompilerOptions(std::vector<std::string> &args, std::initializer_list<std::string> options)
{
    args.insert(std::find(args.begin(), args.end(), "--"), options.begin(), options.end());
}

/**
 * @brief 解析从 PATH 启动的工具位置。
 * @param name argv[0]。
 * @return 可执行文件绝对路径。
 */
fs::path DevelopmentCompilerPath(const char *name)
{
    fs::path candidate = fs::u8path(name);
    if (fs::exists(candidate)) return fs::absolute(candidate);
    const char *environment = std::getenv("PATH");
    std::string paths = environment ? environment : "";
#ifdef _WIN32
    const char separator = ';';
#else
    const char separator = ':';
#endif
    std::size_t start = 0;
    while (start <= paths.size())
    {
        auto end = paths.find(separator, start);
        auto parent = paths.substr(start, end == std::string::npos ? end : end - start);
        auto file = fs::u8path(parent) / candidate;
        if (fs::exists(file)) return fs::absolute(file);
#ifdef _WIN32
        file += ".exe";
        if (fs::exists(file)) return fs::absolute(file);
#endif
        if (end == std::string::npos) break;
        start = end + 1;
    }
    throw std::runtime_error(MAGEL_TEXT("无法定位 magel 可执行文件"));
}

/**
 * @brief 解析统一工具链命令行并派发编译、运行及开发工具任务。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数指针数组。
 * @retval 进程退出状态，0 表示成功，非零表示失败。
 */
int main(int argc, char **argv)
{
    auto originalArguments = magel_i18n::processArguments(argc, argv);
    std::vector<char *> originalValues;
    for (auto &argument : originalArguments) originalValues.push_back(argument.data());
    auto normalized = magel_cli::normalize(static_cast<int>(originalArguments.size()), originalValues.data());
    std::string localeError;
    if (!magel_i18n::configure(normalized, localeError))
    {
        std::fprintf(stderr, "magel: %s\n", localeError.c_str());
        return 2;
    }
    std::vector<char *> normalizedArgv;
    for (auto &argument : normalized) normalizedArgv.push_back(argument.data());
    normalizedArgv.push_back(nullptr);
    argc = static_cast<int>(normalized.size());
    argv = normalizedArgv.data();
    if (argc > 2 && std::strcmp(argv[1], "gen") == 0 && std::strcmp(argv[2], "abi") == 0) return magel_abi_import::run(argc, argv);
    if (argc > 1 && std::strcmp(argv[1], "new") == 0) return magel_project::runCli(argc, argv);
    /* `magel lsp --macros`：在**本进程内**跑 Magel 宏/块的 LSP（magel/source/lsp 编进了编译器，
     * 不需要 Python，也没有第二个可执行文件）；普通 `magel lsp` 同样使用原生 C++ 实现。 */
    if (argc > 2 && std::strcmp(argv[1], "lsp") == 0 && std::strcmp(argv[2], "--macros") == 0)
    {
        lsp::Server server;
        return lsp::runStdioServer(server, std::cin, std::cout, std::cerr);
    }
    if (argc > 1 && (std::strcmp(argv[1], "man") == 0 || std::strcmp(argv[1], "test") == 0 || std::strcmp(argv[1], "fmt") == 0 || std::strcmp(argv[1], "lsp") == 0 || std::strcmp(argv[1], "deps") == 0)) return magel_development::run(argc, argv, DevelopmentCompilerPath(argv[0]));
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
    for (int index = 1; index < argc; ++index)
        arguments.emplace_back(argv[index]);

    const auto optionsEnd = std::find(arguments.begin(), arguments.end(), "--");
    const bool checking = std::find(arguments.begin(), optionsEnd, "--check") != optionsEnd || std::find(arguments.begin(), optionsEnd, "--diagnostics=json") != optionsEnd;
    if (checking)
    {
        if (!arguments.empty() && arguments.front() == "aot") arguments.erase(arguments.begin());
        std::vector<char *> checkArgs{ argv[0] };
        for (auto &a : arguments) checkArgs.push_back(a.data());
        return magel_main_entry(static_cast<int>(checkArgs.size()), checkArgs.data());
    }

    /* 1. 前置子命令（只在第一个参数且属于固定集合时识别）。 */
    std::string subcommand;
    if (!arguments.empty())
    {
        const std::string &first = arguments.front();
        if (first == "run" || first == "interpret" || first == "emit" || first == "svmb" || first == "smvb" || first == "aot" || first == "gen" || first == "clear")
        {
            subcommand = first;
            arguments.erase(arguments.begin());
        }
    }
    if (!subcommand.empty() && subcommand != "gen" && subcommand != "clear" && arguments.empty())
    {
        /* 允许从标准输入读源码，继续。 */
    }

    Scan scan;
    scan.rest = arguments;
    std::string error;
    if (!scanArguments(arguments, &scan, &error))
    {
        std::fprintf(stderr, "magel：%s\n", error.c_str());
        return 2;
    }

    /* 没有任何参数（或只要帮助）时，默认显示与 `--help` 相同的页面，
     * 而不是挂起等标准输入——与 ParseEngine 的 `argc <= 1 → printUsage` 一致。
     * 管道用法仍然可用：给一个选项（如 `--lang python`）即可，同 ParseEngine。 */
    if (argc <= 1 || scan.help)
        return runFrontEnd({ "--help" });

    const bool cleanFlag =
        std::find(arguments.begin(), arguments.end(), "--clean") != arguments.end();

    std::string mode;
    if (subcommand == "run" || subcommand == "interpret")
        mode = "interpret";
    else if (subcommand == "emit")
        mode = "emit";
    else if (subcommand == "svmb" || subcommand == "smvb")
        mode = "svmb";
    else if (subcommand == "aot")
        mode = "aot";
    else if (subcommand == "gen")
        mode = "gen";
    else if (subcommand == "clear")
        mode = "clear";

    if (!scan.cliMode.empty())
    {
        const std::string normalized = magel_package::normalizeRunMode(Memory::String(scan.cliMode));
        if (normalized.empty())
        {
            std::fprintf(stderr, MAGEL_TEXT("magel：不认识的运行方式：%s（用 interpret|svmb|aot）\n"),
                         scan.cliMode.c_str());
            return 2;
        }
        if (!mode.empty() && mode != "gen" && mode != "clear" && mode != "emit" && mode != normalized)
        {
            std::fprintf(stderr, MAGEL_TEXT("magel：子命令与 --mode=%s 冲突\n"), normalized.c_str());
            return 2;
        }
        if (mode.empty() || (mode != "gen" && mode != "clear")) mode = normalized;
    }

    /* 读一次 magel.toml：既提供缺省运行方式，也提供导出相关选项的缺省值
     * （`cpp_output` / `cpp_output_dir` / `execute_output` / `opt_level`）。 */
    magel_package::Project project;
    Memory::String packageError;
    const bool haveProject =
        magel_package::discover(Memory::String(scan.source), project, packageError);
    if (mode.empty())
    {
        if (haveProject && !project.run.empty())
            mode = std::string(project.run);
        else
            mode = "interpret";
        if (!isKnownMode(mode)) mode = "interpret";
    }
    if (haveProject)
    {
        /* 命令行优先：只在没显式给时用 magel 的值。 */
        if (scan.optLevel.empty() && !project.optLevel.empty())
            scan.optLevel = std::string(project.optLevel);
        if (!scan.cppOutput && project.cppOutput) scan.cppOutput = true;
        if (scan.cppOutputDir.empty() && !project.cppOutputDir.empty())
            scan.cppOutputDir = std::string(project.cppOutputDir);
        if (scan.executeOutput.empty() && !scan.outPathGiven && !project.executeOutput.empty())
            scan.executeOutput = std::string(project.executeOutput);
        const bool cliToolchain = !scan.gccToolchain.empty();
        if (scan.gccToolchain.empty()) scan.gccToolchain = std::string(project.gccToolchain);
        if (!cliToolchain && scan.cxx.empty() && !project.cxx.empty()) scan.cxx = std::string(project.cxx);
        if (!cliToolchain && scan.cc.empty() && !project.cc.empty()) scan.cc = std::string(project.cc);
        if (scan.makeTool.empty() && !project.makeTool.empty())
            scan.makeTool = std::string(project.makeTool);
    }

    /* `--clean`（没有源文件、没有显式子命令 / --mode）等价于 `clear`：只按清单清理。 */
    if (cleanFlag && subcommand.empty() && scan.cliMode.empty() && scan.source.empty())
        mode = "clear";

    if (mode != "aot" && (std::find(scan.emit.begin(), scan.emit.end(), "--separate-modules") != scan.emit.end() || std::find(scan.emit.begin(), scan.emit.end(), "--module-cache") != scan.emit.end()))
    {
        std::fprintf(stderr, "magel：模块独立编译只支持 aot 模式\n");
        return 2;
    }
    if (mode != "aot" && (std::find(scan.emit.begin(), scan.emit.end(), "--unchecked") != scan.emit.end() || std::find(scan.emit.begin(), scan.emit.end(), "--checked") != scan.emit.end()))
    {
        std::fprintf(stderr, MAGEL_TEXT("magel：checked / unchecked 只支持 aot 模式\n"));
        return 2;
    }
    if (mode != "aot" && (std::find(scan.emit.begin(), scan.emit.end(), "--shared") != scan.emit.end() || std::find(scan.emit.begin(), scan.emit.end(), "--object") != scan.emit.end()))
    {
        std::fprintf(stderr, "magel：库输出只支持 aot 模式\n");
        return 2;
    }
    if (mode == "interpret" || mode == "emit" || mode == "svmb")
    {
        if (mode == "emit") AppendCompilerOptions(scan.frontEnd, { "--emit" });
        if (mode == "svmb") AppendCompilerOptions(scan.frontEnd, { "--emit", "--svmb" });
        return runFrontEnd(scan.frontEnd);
    }

    const fs::path root = findRepoRoot(argc > 0 ? argv[0] : "magel");
    /* aot 不再要求仓库目录：运行时源码已经由
     * `tools/embed_stdlib.py` 内嵌进本二进制（`magel_stdlib_embedded.cpp`）。
     * 脱离仓库时改用**仓库相对路径**当键，让 `magel-emit::readWholeFile` 读磁盘失败后
     * 回退到内嵌资产；`gen` / `clear` 仍需要真实仓库根（要跑 magel-generator 与模板）。 */
    const fs::path assetRoot = root.empty() ? fs::path() : root;

    if (mode == "gen" || mode == "clear")
    {
        if (root.empty())
        {
            std::fprintf(stderr, "magel：找不到仓库根（缺 magel/source/magel-emit/aot.cpp），无法转发 %s\n",
                         mode.c_str());
            return 1;
        }
        if (mode == "clear")
        {
            /* 清理登记过的中间产物：magel-generator 的工程清单 + magel-emit 的 AOT 清单
             * （`.magel-aot-manifest`）。只删清单内文件，
             * 最终可执行文件与用户文件不动。 */
            std::string outDir;
            for (std::size_t index = 0; index + 1 < scan.emit.size(); ++index)
            {
                if (scan.emit[index] == "--out-dir")
                    outDir = scan.emit[index + 1];
                else if (scan.emit[index] == "--out" && outDir.empty())
                    outDir = dirOf(scan.emit[index + 1]);
            }
            if (outDir.empty() && !scan.outPath.empty()) outDir = dirOf(scan.outPath);
            if (outDir.empty())
            {
                std::fprintf(stderr, MAGEL_TEXT("magel：clear 需要 --out-dir DIR\n"));
                return 2;
            }
            int status = runGenerator({ "--root", root.u8string(), "clear", "--out-dir", outDir });
            status |= runEmitter({ "--target", "cpp-aot", "--clean", "--out-dir", outDir });
            std::vector<std::string> toolFiles;
            std::string toolError;
            if (!readToolManifest(outDir, &toolFiles, &toolError))
            {
                std::fprintf(stderr, "magel：%s\n", toolError.c_str());
                return 1;
            }
            std::size_t removed = 0u;
            if (!deleteToolFiles(outDir, toolFiles, &removed, &toolError))
            {
                std::fprintf(stderr, "magel：%s\n", toolError.c_str());
                return 1;
            }
            if (removed != 0u) std::printf("清理完成，共删除 %zu 个工具中间文件\n", removed);
            return status == 0 ? 0 : 1;
        }
        /* `gen`：用户写 `${program} gen compiler ...`，或只给选项时补 `compiler`。 */
        std::vector<std::string> generatorArgs{ "--root", root.u8string() };
        const bool hasCommand = !scan.rest.empty() && (scan.rest[0] == "compiler" || scan.rest[0] == "clear");
        if (!hasCommand) generatorArgs.push_back("compiler");
        generatorArgs.insert(generatorArgs.end(), scan.rest.begin(), scan.rest.end());
        return runGenerator(generatorArgs);
    }

    /* aot：先出四元式与数据区，再让 magel-emit 生成 C/C++ 并编译。
     * `assetRoot` 为空时用 `magel/...` 这样的仓库相对键走内嵌资产。 */
    /* `--execute-output FILE` 指定可执行文件位置；缺省用 `--out`。两者都等价于
     * 前端的产物前缀（`.quad` / `.data.bin` / `.layout` 与它同目录同前缀）。 */
    std::string exePath = !scan.executeOutput.empty() ? scan.executeOutput : scan.outPath;
    if (exePath.empty())
    {
        if (!scan.source.empty())
        {
            fs::path sourcePath = fs::u8path(scan.source);
            sourcePath.replace_extension();
            exePath = sourcePath.filename().u8string();
        }
        else
        {
            exePath = "build/magel_out";
        }
    }
    {
        bool frontHasOut = false;
        for (std::size_t index = 0; index + 1 < scan.frontEnd.size() && scan.frontEnd[index] != "--"; ++index)
            frontHasOut = frontHasOut || scan.frontEnd[index] == "--out";
        if (!frontHasOut)
        {
            AppendCompilerOptions(scan.frontEnd, { "--out", exePath });
        }
    }
    const fs::path exeAbsolute = fs::absolute(fs::u8path(exePath)).lexically_normal();
    exePath = exeAbsolute.u8string();
    /* 前端把 .quad / .data.bin / .layout 写在 exe 同目录：先建好目录，
     * `--execute-output` 指到还不存在的目录时也能用。 */
    {
        std::error_code exeDirError;
        fs::create_directories(exeAbsolute.parent_path(), exeDirError);
        if (exeDirError)
        {
            std::fprintf(stderr, "magel：无法创建输出目录：%s\n",
                         exeDirError.message().c_str());
            return 1;
        }
    }

    /* 前端出 .quad / .data.bin / .layout；前缀与可执行文件同名同目录。 */
    {
        const std::string resolved = exePath;
        std::vector<std::string> frontEnd;
        frontEnd.reserve(scan.frontEnd.size());
        for (std::size_t index = 0; index < scan.frontEnd.size(); ++index)
        {
            if (scan.frontEnd[index] == "--")
            {
                frontEnd.insert(frontEnd.end(), scan.frontEnd.begin() + index, scan.frontEnd.end());
                break;
            }
            if (scan.frontEnd[index] == "--out" && index + 1 < scan.frontEnd.size())
            {
                frontEnd.push_back("--out");
                frontEnd.push_back(resolved);
                ++index;
                continue;
            }
            frontEnd.push_back(scan.frontEnd[index]);
        }
        bool hasEmit = false;
        for (const std::string &argument : frontEnd)
        {
            if (argument == "--") break;
            hasEmit = hasEmit || argument == "--emit";
        }
        if (!hasEmit) AppendCompilerOptions(frontEnd, { "--emit" });
        const int code = runFrontEnd(frontEnd);
        if (code != 0) return code;
    }

    /* 记录本次由统一工具生成的前端中间文件（清单只写单层文件名）。 */
    const fs::path outDir = exeAbsolute.parent_path();
    const std::string exeName = exeAbsolute.filename().u8string();
    std::vector<std::string> toolFiles;
    for (const char *suffix : { ".quad", ".data.bin", ".layout", ".data", ".data.h", ".abi.cpp" })
        toolFiles.push_back(exeName + suffix);
    {
        std::string manifestError;
        if (!writeToolManifest(outDir, toolFiles, &manifestError))
        {
            std::fprintf(stderr, "magel：%s\n", manifestError.c_str());
            return 1;
        }
    }

    const std::string quadPath = exePath + ".quad";
    const std::string dataPath = exePath + ".data.bin";
    const std::string layoutPath = exePath + ".layout";
    std::vector<std::string> cppArgs;
    if (scan.cppOutput)
    {
        cppArgs.push_back("--cpp-output");
        if (!scan.cppOutputDir.empty())
        {
            cppArgs.push_back("--cpp-output-dir");
            cppArgs.push_back(scan.cppOutputDir);
        }
    }
    std::vector<std::string> emitArgs{
        "--quad",
        quadPath,
        "--data",
        dataPath,
        "--layout",
        layoutPath,
        "--target",
        "cpp-aot",
        "--out",
        exePath,
        "--vm-dir",
        (assetRoot / "magel" / "library" / "stackvm").u8string(),
        "--magel-dir",
        (assetRoot / "magel").u8string(),
        "--bigint-dir",
        (assetRoot / "magel" / "library" / "bignum").u8string(),
    };
    bool hidePanicDetails = haveProject && project.compressSymbols;
    for (const auto &argument : scan.frontEnd)
    {
        if (argument == "--compress-symbols") hidePanicDetails = true;
        if (argument == "--no-compress-symbols") hidePanicDetails = false;
    }
    if (hidePanicDetails) emitArgs.push_back("--no-panic-details");
    if (!scan.gccToolchain.empty())
    {
        emitArgs.push_back("--gcc-toolchain");
        emitArgs.push_back(scan.gccToolchain);
    }
    if (!scan.cxx.empty())
    {
        emitArgs.push_back("--cxx");
        emitArgs.push_back(scan.cxx);
    }
    if (!scan.cc.empty())
    {
        emitArgs.push_back("--cc");
        emitArgs.push_back(scan.cc);
    }
    if (!scan.makeTool.empty())
    {
        emitArgs.push_back("--make");
        emitArgs.push_back(scan.makeTool);
    }
    /* magel 的 `opt_level`（命令行没给 --opt-level / -O* 时）：透传给 magel-emit 的 AOT 编译。 */
    {
        bool emitHasOpt = false;
        for (const std::string &argument : scan.emit)
            emitHasOpt = emitHasOpt || argument == "--opt-level" || argument.rfind("--opt-level=", 0) == 0 || (argument.size() > 2 && argument[0] == '-' && argument[1] == 'O');
        if (!emitHasOpt && !scan.optLevel.empty())
        {
            emitArgs.push_back("--opt-level");
            emitArgs.push_back(scan.optLevel);
        }
    }
    emitArgs.insert(emitArgs.end(), scan.emit.begin(), scan.emit.end());
    emitArgs.insert(emitArgs.end(), cppArgs.begin(), cppArgs.end());
    const int emitCode = runEmitter(emitArgs);
    if (emitCode != 0)
    {
        std::fprintf(stderr, MAGEL_TEXT("magel：导出失败；中间文件保留在 %s\n"), dirOf(exePath).c_str());
        return emitCode;
    }

    if (!scan.keep)
    {
        std::size_t removed = 0u;
        std::string cleanupError;
        if (!deleteToolFiles(outDir, toolFiles, &removed, &cleanupError))
        {
            std::fprintf(stderr, "magel：%s\n", cleanupError.c_str());
            return 1;
        }
    }

    /* 最终产物名由导出器按 GCC 目标及输出类型报告，入口不再猜测 .exe 后缀。 */
    std::puts(MAGEL_TEXT("AOT 导出完成。"));
    if (scan.keep) std::printf(MAGEL_TEXT("中间文件已保留在：%s\n"), dirOf(exePath).c_str());
    return 0;
}
