/**
 * @file chali_cli_test.cpp
 * @brief 场景参数归一化单元测试，覆盖值保护及拖入文件参数边界。
 */
#include "../source/chali_cli.hpp"
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
    assert(chali_cli::normalize(static_cast<int>(args.size()), args.data()) == std::vector<std::string>(expected.begin(), expected.end()));
}

/**
 * @brief 运行场景命令与参数保护用例。
 * @return 全部断言通过后返回 0。
 */
int main()
{
    check({ "chali", "demo.chl", "out", "--check", "two words" }, { "chali", "run", "demo.chl", "--", "out", "--check", "two words" });
    check({ "chali", "demo.chl", "--", "debug" }, { "chali", "run", "demo.chl", "--", "debug" });
    check({ "chali", "check", "demo.chl", "json" }, { "chali", "--check", "demo.chl", "--diagnostics=json" });
    check({ "chali", "new", "demo", "name", "help" }, { "chali", "new", "demo", "--name", "help" });
    check({ "chali", "new", "name" }, { "chali", "new", "name" });
    check({ "chali", "aot" }, { "chali", "run", "aot" });
    check({ "chali", "emit", "demo.chl", "out", "debug", "reflect" },
          { "chali", "emit", "demo.chl", "--out", "debug", "--reflection" });
    check({ "chali", "run", "out", "--out", "help", "--", "reflect" },
          { "chali", "run", "out", "--out", "help", "--", "reflect" });
    check({ "chali", "test", "tests", "backend", "interpret", "json" }, { "chali", "test", "tests", "--backend", "interpret", "--json" });
    check({ "chali", "fmt", "check", "src" }, { "chali", "fmt", "--check", "src" });
    check({ "chali", "deps", "lock", "project", "out", "offline" }, { "chali", "deps", "lock", "--project", "out", "--offline" });
    check({ "chali", "gen", "compiler", "out", "tmp" }, { "chali", "gen", "compiler", "--out-dir", "tmp" });
    check({ "chali", "clear", "out", "tmp" }, { "chali", "clear", "--out-dir", "tmp" });
    check({ "chali", "help" }, { "chali", "--help" });
    check({ "chali", "--check", "demo.chl", "--out", "help" }, { "chali", "--check", "demo.chl", "--out", "help" });
}
