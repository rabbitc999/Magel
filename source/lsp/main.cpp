/*
 * magel-lsp —— Magel 宏/块模型的编辑器语言服务器（`$macro` / `$replace` / `$for` …）。
 *
 * 用法：
 *   magel-lsp --stdio            在标准输入输出上跑 LSP（编辑器默认用法）
 *   magel-lsp --check FILE...    直接校验，输出 FILE:行:列: 级别: 消息 [码]
 *   magel-lsp --version / --help
 *
 * 与工具链自带的 `magel lsp`（真前端检查，内嵌在编译器里）**分工不同**：
 * 这一份只做宏 / 块的编辑期配对与引用（快、不依赖编译器），另外提供语义高亮。
 */

#include "server.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace
{

/**
 * @brief Windows 上把标准输入输出切成二进制模式：LSP 的 `Content-Length` 帧不能做换行转换。
 */
void useBinaryStdio()
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

/**
 * @brief 将文件内容读取到输出字符串。
 *
 * 读整个文件（去掉 UTF-8 BOM）。
 * @param path 路径。
 * @param ok 读出返回 true。
 * @return 文件内容。
 */
std::string readFile(const std::string &path, bool &ok)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        ok = false;
        return {};
    }
    std::ostringstream out;
    out << in.rdbuf();
    ok = true;
    std::string text = out.str();
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF)
    {
        text.erase(0, 3);
    }
    return text;
}

/**
 * @brief 打印帮助。
 */
void printHelp()
{
    std::cout << "magel-lsp 1.0.0 —— Magel 宏 / 块的编辑期语言服务器\n"
                 "\n"
                 "用法：\n"
                 "  magel-lsp --stdio            在标准输入输出上跑 LSP（编辑器默认用法）\n"
                 "  magel-lsp --check FILE...    直接校验宏 / 块，输出 FILE:行:列: 级别: 消息 [码]\n"
                 "  magel-lsp --version          版本\n"
                 "  magel-lsp --help             本帮助\n"
                 "\n"
                 "能力：诊断（宏配对 / 参数 / 调用 / 分支）、悬停、补全、跳转定义、文档大纲、\n"
                 "语义高亮。完整 Magel 前端检查（类型、语义）用编译器自带的 `magel lsp` 或\n"
                 "`magel --check`。\n";
}

}  // namespace

/**
 * @brief 运行本模块的命令行示例程序。
 * @param argc 命令行参数数量。
 * @param argv 命令行参数指针数组。
 * @retval 进程退出状态，0 表示成功，非零表示失败。
 */
int main(int argc, char **argv)
{
    useBinaryStdio();
    std::string mode = "--stdio";
    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h")
        {
            printHelp();
            return 0;
        }
        if (arg == "--version" || arg == "-v")
        {
            std::cout << "magel-lsp 1.0.0\n";
            return 0;
        }
        if (arg == "--check")
        {
            mode = "--check";
            continue;
        }
        if (arg == "--stdio")
        {
            mode = "--stdio";
            continue;
        }
        files.push_back(arg);
    }
    if (mode == "--check")
    {
        if (files.empty())
        {
            std::cerr << "用法：magel-lsp --check FILE...\n";
            return 2;
        }
        int errors = 0;
        for (const auto &file : files)
        {
            bool ok = false;
            const std::string text = readFile(file, ok);
            if (!ok)
            {
                std::cout << file << ": error: 无法读取文件\n";
                ++errors;
                continue;
            }
            const std::string report = lsp::Server::checkText(text, file);
            std::cout << report;
            if (report.find(": error:") != std::string::npos) ++errors;
        }
        return errors == 0 ? 0 : 1;
    }
    lsp::Server server;
    return lsp::runStdioServer(server, std::cin, std::cout, std::cerr);
}
