#pragma once
/**
 * @file generator_cli.hpp
 * @brief `magel-generator` 的命令行入口，供独立二进制与统一入口 `magel_tool.cpp` 复用。
 *
 * 支持前置 `--root DIR`：统一入口在进程内调用时显式指定仓库根，避免依赖 argv[0]
 * 的目录层级。独立构建在未定义 `MAGEL_GENERATOR_NO_MAIN` 时提供 `main`。
 */
namespace magel_generator
{

int runCli(int argc, char **argv);

} // namespace magel_generator
