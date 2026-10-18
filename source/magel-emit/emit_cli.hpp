#pragma once
/**
 * @file emit_cli.hpp
 * @brief `magel-emit` 的命令行入口，供独立二进制与统一入口 `magel_tool.cpp` 复用。
 *
 * 独立构建：`magel-emit/main.cpp` 在未定义 `MAGEL_EMIT_NO_MAIN` 时提供 `main`。
 * 统一构建：`magel_tool.cpp` 定义 `MAGEL_EMIT_NO_MAIN` 并直接调用 `magel_emit::runCli`。
 */
namespace magel_emit
{

int runCli(int argc, char **argv);

} // namespace magel_emit
