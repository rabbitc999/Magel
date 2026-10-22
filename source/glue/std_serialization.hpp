#pragma once
#include "std_binding.hpp"

namespace std_serialization_glue
{
/**
 * @brief 注册反射序列化使用的格式转换和字节封装函数。
 * @param registry 宿主注册表。
 * @param error 注册失败时的诊断。
 * @return 所有入口注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error);
}  // namespace std_serialization_glue
