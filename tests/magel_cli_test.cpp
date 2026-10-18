/**
 * @file magel_cli_test.cpp
 * @brief 场景参数归一化单元测试，覆盖值保护及拖入文件参数边界。
 */
#include "../source/magel_cli.hpp"
#include <cassert>
#include <initializer_list>

/**
 * @brief 检查一组输入参数的完整转换结果。
 * @param input 输入。
 * @param expected 期望输出。
 */
static void check(std::initializer_list<const char *> input, std::initializer_list<const char *> expected)
{
    std::vector<std::string> storage(input.begin(), input.end());
    std::vector<char *> args;
    for (auto &value : storage) args.push_back(value.data());
    assert(magel_cli::normalize(static_cast<int>(args.size()), args.data()) == std::vector<std::string>(expected.begin(), expected.end()));
}

/**
 * @brief 运行场景命令与参数保护用例。
 * @return 全部断言通过后返回 0。
 */
int main()
{
    check({ "magel", "demo.mgl", "out", "--check", "two words" }, { "magel", "run", "demo.mgl", "--", "out", "--check", "two words" });
    check({ "magel", "demo.mgl", "--", "debug" }, { "magel", "run", "demo.mgl", "--", "debug" });
    check({ "magel", "check", "demo.mgl", "json" }, { "magel", "--check", "demo.mgl", "--diagnostics=json" });
    check({ "magel", "new", "demo", "name", "help" }, { "magel", "new", "demo", "--name", "help" });
    check({ "magel", "new", "name" }, { "magel", "new", "name" });
    check({ "magel", "aot", "demo.mgl", "out", "debug", "opt", "0", "reflect" },
          { "magel", "aot", "demo.mgl", "--out", "debug", "--opt-level", "0", "--reflection" });
    check({ "magel", "run", "out", "--out", "help", "--", "reflect" },
          { "magel", "run", "out", "--out", "help", "--", "reflect" });
    check({ "magel", "test", "tests", "backend", "all", "json" }, { "magel", "test", "tests", "--backend", "all", "--json" });
    check({ "magel", "fmt", "check", "src" }, { "magel", "fmt", "--check", "src" });
    check({ "magel", "deps", "lock", "project", "out", "offline" }, { "magel", "deps", "lock", "--project", "out", "--offline" });
    check({ "magel", "gen", "compiler", "out", "tmp" }, { "magel", "gen", "compiler", "--out-dir", "tmp" });
    check({ "magel", "clear", "out", "tmp" }, { "magel", "clear", "--out-dir", "tmp" });
    check({ "magel", "help" }, { "magel", "--help" });
    check({ "magel", "--check", "demo.mgl", "--out", "help" }, { "magel", "--check", "demo.mgl", "--out", "help" });
}
