#pragma once
#include "std_binding.hpp"

namespace lang_numeric_glue
{
/**
 * @brief 注册核心数值和 IEEE 754 字节转换。
 * @param registry 注册表。
 * @param error 注册错误。
 * @return 是否成功。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error = nullptr);
}  // namespace lang_numeric_glue
