/**
 * @file chali_tool.cpp
 * @brief 统一 VM 前端、工程生成器和原生开发命令的入口。
 */
#include "chali_runtime_allocator.hpp"
#include "lsp/language_server.hpp"
#include "tools/native_development.hpp"
#include "tools/abi_import.hpp"
#include "chali_project.hpp"
#include "chali_cli.hpp"
#include "generator/generator_cli.hpp"
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

int chali_main_entry(int argc, char **argv);

namespace fs = std::filesystem;

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
    throw std::runtime_error(CHALI_TEXT("无法定位 chali 可执行文件"));
}

/**
 * @brief 派发 VM 编译执行、工程生成及开发工具命令。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数数组。
 * @return 子命令的退出状态，未知文件或选项返回非零。
 */
int main(int argc, char **argv)
{
    auto originalArguments = chali_i18n::processArguments(argc, argv);
    std::vector<char *> originalValues;
    for (auto &argument : originalArguments) originalValues.push_back(argument.data());
    auto normalized = chali_cli::normalize(static_cast<int>(originalArguments.size()), originalValues.data());
    std::string localeError;
    if (!chali_i18n::configure(normalized, localeError))
    {
        std::fprintf(stderr, "chali: %s\n", localeError.c_str());
        return 2;
    }
    std::vector<char *> normalizedArgv;
    for (auto &argument : normalized) normalizedArgv.push_back(argument.data());
    normalizedArgv.push_back(nullptr);
    argc = static_cast<int>(normalized.size());
    argv = normalizedArgv.data();
    if (argc > 2 && std::strcmp(argv[1], "gen") == 0 && std::strcmp(argv[2], "abi") == 0) return chali_abi_import::run(argc, argv);
    if (argc > 1 && std::strcmp(argv[1], "new") == 0) return chali_project::runCli(argc, argv);
    /* `chali lsp --macros`：在**本进程内**跑 ChaLi 宏/块的 LSP（chali/source/lsp 编进了编译器，
     * 不需要 Python，也没有第二个可执行文件）；普通 `chali lsp` 同样使用原生 C++ 实现。 */
    if (argc > 2 && std::strcmp(argv[1], "lsp") == 0 && std::strcmp(argv[2], "--macros") == 0)
    {
        lsp::Server server;
        return lsp::runStdioServer(server, std::cin, std::cout, std::cerr);
    }
    if (argc > 1 && (std::strcmp(argv[1], "man") == 0 || std::strcmp(argv[1], "test") == 0 || std::strcmp(argv[1], "fmt") == 0 || std::strcmp(argv[1], "lsp") == 0 || std::strcmp(argv[1], "deps") == 0)) return chali_development::run(argc, argv, DevelopmentCompilerPath(argv[0]));
    if (argc > 1 && (std::strcmp(argv[1], "gen") == 0 || std::strcmp(argv[1], "clear") == 0))
    {
        const bool clear = std::strcmp(argv[1], "clear") == 0;
        auto root = DevelopmentCompilerPath(argv[0]).parent_path();
        while (!root.empty() && !fs::exists(root / "source/chali.gram"))
        {
            auto parent = root.parent_path();
            if (parent == root) break;
            root = parent;
        }
        if (!fs::exists(root / "source/chali.gram"))
        {
            std::fputs(CHALI_TEXT("chali: 找不到工程生成器源码根目录\n"), stderr);
            return 1;
        }
        std::vector<std::string> values{ argv[0], "--root", root.u8string() };
        if (clear)
            values.push_back("clear");
        else if (argc <= 2 || (std::strcmp(argv[2], "compiler") != 0 && std::strcmp(argv[2], "clear") != 0))
            values.push_back("compiler");
        for (int index = 2; index < argc; ++index) values.emplace_back(argv[index]);
        std::vector<char *> pointers;
        for (auto &value : values) pointers.push_back(value.data());
        return chali_generator::runCli(static_cast<int>(pointers.size()), pointers.data());
    }
    return chali_main_entry(argc, argv);
}
