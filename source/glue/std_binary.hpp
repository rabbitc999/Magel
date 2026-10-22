#pragma once
#include "std_binding.hpp"

namespace std_binary_glue
{
/**
 * @brief 注册二进制流标准库。
 * @param registry 胶水注册表。
 * @param error 注册错误。
 * @return 是否成功。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error = nullptr);
}  // namespace std_binary_glue
