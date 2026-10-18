/**
 * @file magel_cli.hpp
 * @brief 场景单词命令与拖入源码的参数边界，统一入口和独立前端共用。
 */
#pragma once
#include <string>
#include <vector>

namespace magel_cli
{
/**
 * @brief 将用户场景语法转换为内部选项；直接源码后的参数全部传入程序。
 * @param argc 原始参数数量。
 * @param argv 原始参数数组。
 * @return 拥有型参数列表，包含程序名；选项值及程序参数保持原样。
 */
inline std::vector<std::string> normalize(int argc, char **argv)
{
    std::vector<std::string> result;
    if (argc == 0) return result;
    result.emplace_back(argv[0]);
    if (argc == 1) return result;
    // 全局语言配置允许放在场景之前；文件后的参数仍由直接运行分支保护。
    const std::string first = argv[1];
    if (first == "lang" || first == "--lang" || first.compare(0, 7, "--lang=") == 0)
    {
        const bool pair = first == "lang" || first == "--lang";
        result.emplace_back(first == "lang" ? "--lang" : first);
        if (pair && argc > 2) result.emplace_back(argv[2]);
        const int consumed = pair ? 2 : 1;
        std::vector<char *> tail{ argv[0] };
        for (int i = 1 + consumed; i < argc; ++i) tail.push_back(argv[i]);
        auto rest = normalize(static_cast<int>(tail.size()), tail.data());
        result.insert(result.end(), rest.begin() + 1, rest.end());
        return result;
    }
    std::string command = argv[1];
    const bool sourceCommand = command == "run" || command == "interpret" || command == "aot" || command == "emit" || command == "svmb" || command == "smvb" || command == "check" || command == "new" || command == "gen";
    const bool scenario = sourceCommand || command == "help" || command == "clear" || command == "test" || command == "fmt" || command == "deps" || command == "lsp" || command == "man";
    if (!scenario && !command.empty() && command[0] != '-')
    {
        result.emplace_back("run");
        result.push_back(command);
        if (argc > 2 && std::string(argv[2]) != "--") result.emplace_back("--");
        for (int i = 2; i < argc; ++i) result.emplace_back(argv[i]);
        return result;
    }
    result.push_back(command == "check" ? "--check" : command == "help" ? "--help" :
                                                                          command);
    bool sourceSeen = !sourceCommand;
    for (int i = 2; i < argc; ++i)
    {
        std::string value = argv[i];
        if (value == "--")
        {
            for (; i < argc; ++i) result.emplace_back(argv[i]);
            break;
        }
        bool takesValue = false;
        if (scenario && (sourceSeen || value == "help" || value == "lang"))
        {
            if (value == "lang")
            {
                value = "--lang";
                takesValue = true;
            }
            else if (value == "help")
                value = "--help";
            else if (command == "new" && value == "name")
            {
                value = "--name";
                takesValue = true;
            }
            else if (command == "fmt" && (value == "check" || value == "write"))
                value = "--" + value;
            else if (command == "test")
            {
                if (value == "json" || value == "list")
                    value = "--" + value;
                else if (value == "backend" || value == "filter" || value == "timeout")
                {
                    value = "--" + value;
                    takesValue = true;
                }
            }
            else if (command == "deps")
            {
                if (value == "project")
                {
                    value = "--project";
                    takesValue = true;
                }
                else if (value == "offline")
                    value = "--offline";
            }
            else if (command != "new" && command != "fmt" && command != "lsp" && command != "man")
            {
                if (value == "out")
                {
                    value = command == "gen" || command == "clear" ? "--out-dir" : "--out";
                    takesValue = true;
                }
                else if (value == "opt")
                {
                    value = "--opt-level";
                    takesValue = true;
                }
                else if (value == "cache")
                {
                    value = "--module-cache";
                    takesValue = true;
                }
                else if (value == "modules")
                    value = "--separate-modules";
                else if (value == "reflect")
                    value = "--reflection";
                else if (value == "debug" || value == "release")
                    value = "--" + value;
                else if (value == "unchecked" || value == "checked")
                    value = "--" + value;
                else if (command == "check" && value == "json")
                    value = "--diagnostics=json";
            }
        }
        // 已有长选项的值同样跳过翻译，避免 out/name/help 等字面值被当作场景词。
        for (const char *option : { "--lang", "--out", "--out-dir", "--name", "--input", "--memory-size", "--step-limit",
                                    "--include-dir", "--include-path", "--import-dir", "--define", "--symbol-map", "-I", "-D",
                                    "--module-cache", "--opt-level", "--runtime-arena", "--template", "--vm-dir", "--magel-dir",
                                    "--bigint-dir", "--stack-slots", "--target", "--quad", "--data", "--layout", "--glue",
                                    "--cxx", "--cc", "--gcc", "--g++", "--gxx", "--make", "--execute-output", "--run-mode", "--cpp-output-dir", "--mode", "--backend", "--filter", "--timeout", "--project",
                                    "--search", "--cache-dir", "--source-overlays", "--analysis-deps" })
            if (value == option) takesValue = true;
        result.push_back(value);
        if (takesValue && i + 1 < argc)
            result.emplace_back(argv[++i]);
        else if (!value.empty() && value[0] != '-')
            sourceSeen = true;
    }
    return result;
}
}  // namespace magel_cli
