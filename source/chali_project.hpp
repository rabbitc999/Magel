#include "chali_i18n.hpp"
/**
 * @file chali_project.hpp
 * @brief 内嵌 ChaLi 应用工程模板及新建命令，供统一工具链和独立编译器共享。
 */
#pragma once
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace chali_project
{
namespace fs = std::filesystem;

/**
 * @brief 创建可直接运行的 ChaLi 应用工程，拒绝覆盖任何现有目标。
 * @param argc 原始命令行参数数量。
 * @param argv 原始命令行参数，第二项为 new。
 * @return 成功或帮助返回 0，参数错误返回 2，文件系统错误返回 1。
 */
inline int runCli(int argc, char **argv)
{
    const char *usage = CHALI_TEXT("用法：chali new <目录> [name 工程名]\n"
                                   "创建 chali.toml、src/main.chl、tests/test_main.chl、README.md 和 .gitignore。\n"
                                   "目标目录必须不存在；父目录必须存在。工程名由英文字母开头，后接字母、数字、- 或 _。\n");
    std::string destination, name;
    bool positionalOnly = false, haveName = false;
    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (!positionalOnly && (arg == "--help" || arg == "-h"))
        {
            std::fputs(usage, stdout);
            return 0;
        }
        if (!positionalOnly && arg == "--")
        {
            positionalOnly = true;
            continue;
        }
        if (!positionalOnly && arg == "--name")
        {
            if (haveName || i + 1 == argc)
            {
                std::fputs(CHALI_TEXT("chali: name 必须且只能指定一个工程名\n"), stderr);
                return 2;
            }
            haveName = true;
            name = argv[++i];
        }
        else if ((!positionalOnly && !arg.empty() && arg[0] == '-') || !destination.empty() || arg.empty())
        {
            std::fputs(usage, stderr);
            return 2;
        }
        else
            destination = arg;
    }
    if (destination.empty())
    {
        std::fputs(usage, stderr);
        return 2;
    }

    fs::path target;
    bool created = false;
    std::vector<fs::path> written;
    try
    {
        // 不折叠尾部的 . / ..，防止将疑似当前目录路径解释成新的目录。
        fs::path requested = fs::u8path(destination);
        while (requested.filename().empty() && requested != requested.root_path()) requested = requested.parent_path();
        if (requested.filename() == "." || requested.filename() == "..")
            throw std::runtime_error(CHALI_TEXT("目标必须是新的工程目录"));
        target = fs::absolute(requested).lexically_normal();
        if (!haveName) name = target.filename().u8string();
        const auto letter = [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
        if (name.empty() || !letter(static_cast<unsigned char>(name[0])) || !std::all_of(name.begin(), name.end(), [&](unsigned char c) { return letter(c) || (c >= '0' && c <= '9') || c == '-' || c == '_'; }))
        {
            std::fputs(CHALI_TEXT("chali: 无效工程名；请用 name 指定字母开头的字母、数字、- 或 _ 组合\n"), stderr);
            return 2;
        }
        // create_directory 原子判定目标；已有空目录、文件和符号链接也不得覆盖。
        if (!fs::create_directory(target)) throw std::runtime_error(CHALI_TEXT("目标已存在，拒绝覆盖"));
        created = true;
        fs::create_directory(target / "src");
        fs::create_directory(target / "tests");
        const auto write = [&](const char *relative, const std::string &content) {
            const fs::path file = target / relative;
            written.push_back(file);
            std::ofstream output(file, std::ios::binary);
            output << content;
            output.close();
            if (!output) throw std::runtime_error(CHALI_TEXT("无法写入工程文件：") + file.u8string());
        };
        write("chali.toml", "[package]\nname = \"" + name + "\"\nversion = \"0.1.0\"\n\n[chali]\nrun = \"interpret\"\nmain = \"src/main.chl\"\n");
        write("src/main.chl", "void main() {\n    std::lang::println(\"Hello, ChaLi!\");\n    return;\n}\n");
        write("tests/test_main.chl", "void main() {\n    std::lang::assert(1 + 1 == 2);\n    return;\n}\n");
        write(".gitignore", "/build/\n/.chali/\n/chali_out*\n*.exe\n");
        write("README.md", "# " + name + CHALI_TEXT("\n\nChaLi 应用工程。以下命令在工程根目录执行：\n\n") + "```sh\nchali run src/main.chl\nchali check src/main.chl\nchali test tests\nchali svmb src/main.chl out build/main\n```\n\n" + CHALI_TEXT("运行示例输出 `Hello, ChaLi!`。开发命令原生内建，无需 Python；程序通过 VM 执行。\n") + CHALI_TEXT("`chali.toml` 配置项目包；`src/` 放应用源码，`tests/` 放普通 main 测试。\n"));
        /* 系统里装了 git 就顺手 `git init`，方便后续版本管理。
         * 没有 git、或初始化失败都不算创建失败（工程本身已经写好了）。 */
        const auto hasGit = []() {
#ifdef _WIN32
            return std::system("git --version > NUL 2>&1") == 0;
#else
            return std::system("git --version > /dev/null 2>&1") == 0;
#endif
        };
        if (hasGit())
        {
            /* `-C <目录>` 让 git 在目标工程里初始化，不必切换当前工作目录。 */
            /* 注意：`-C` 是 git 的**全局**选项，必须写在子命令 `init` 之前；
             * 写成 `git init -C ...` 会被当成 init 的开关，报 unknown switch。 */
            const std::string command = "git -C \"" + target.u8string() + "\" init";
            if (std::system(command.c_str()) == 0)
                std::fputs(CHALI_TEXT("已初始化 git 仓库（git init）\n"), stdout);
            else
                std::fputs(CHALI_TEXT("提示：检测到 git 但初始化失败，可稍后手动执行 git init\n"), stdout);
        }
        std::fprintf(stdout, CHALI_TEXT("已创建 ChaLi 工程 %s：%s\n"), name.c_str(), target.u8string().c_str());
        std::fputs(CHALI_TEXT("进入工程目录后执行：chali run src/main.chl\n"), stdout);
        return 0;
    } catch (const std::exception &error)
    {
        if (created)
        {
            // 只移除本次写入的文件及空目录，不递归删除目录中的其他内容。
            std::error_code ignored;
            for (const auto &file : written) fs::remove(file, ignored);
            fs::remove(target / "tests", ignored);
            fs::remove(target / "src", ignored);
            fs::remove(target, ignored);
        }
        std::fprintf(stderr, CHALI_TEXT("chali: 创建工程失败：%s\n"), error.what());
        return 1;
    }
}

}  // namespace chali_project
