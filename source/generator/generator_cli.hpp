#pragma once
/**
 * @file generator_cli.hpp
 * @brief `chali-generator` 的命令行入口，供独立二进制与统一入口 `chali_tool.cpp` 复用。
 *
 * 支持前置 `--root DIR`：统一入口在进程内调用时显式指定仓库根，避免依赖 argv[0]
 * 的目录层级。独立构建在未定义 `CHALI_GENERATOR_NO_MAIN` 时提供 `main`。
 */
namespace chali_generator
{

/**
 * @brief 解析生成器参数并导出或清理独立编译器工程。
 * @param argc 参数个数，包含程序名。
 * @param argv 程序名及命令参数，支持前置 --root。
 * @return 成功返回零；参数或生成失败返回非零状态。
 */
int runCli(int argc, char **argv);

} // namespace chali_generator
