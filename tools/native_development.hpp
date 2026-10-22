/**
 * @file native_development.hpp
 * @brief 原生编辑器与开发命令入口；运行时不启动 Python。
 */
#pragma once
#include <filesystem>
namespace chali_development
{
/**
 * @brief 运行原生开发命令。
 * @param argc 参数数量。
 * @param argv 完整命令行。
 * @param compiler 当前编译器绝对路径。
 * @return 命令退出码。
 */
int run(int argc, char **argv, const std::filesystem::path &compiler);
}  // namespace chali_development
