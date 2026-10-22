#pragma once
#include "std_binding.hpp"

namespace std_process_glue
{
/**
 * @brief 注册使用通用不透明记录的进程操作。
 * @param registry 胶水注册表。
 * @param error 绑定失败的诊断。
 * @return 全部注册成功时为 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error = nullptr);
}  // namespace std_process_glue
