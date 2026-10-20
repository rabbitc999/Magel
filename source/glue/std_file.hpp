#pragma once
#include "stackvm.hpp"

namespace file_native_glue
{
/**
 * @brief 将文件操作注册为普通宿主函数。
 * @param registry 宿主注册表。
 * @param error 注册失败诊断。
 * @return 全部注册成功时为 true。
 */
bool bind(stackvm::NativeRegistry &registry, Memory::String *error = nullptr);
}  // namespace file_native_glue
