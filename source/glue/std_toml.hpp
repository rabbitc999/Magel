#pragma once
#include "std_binding.hpp"

namespace std_toml_glue
{
/**
 * @brief 注册标准库模块。
 * @param registry 注册表。
 * @param error 错误输出。
 * @return 是否成功。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error = nullptr);
}  // namespace std_toml_glue
