#pragma once
#include "std_binding.hpp"

namespace std_string_methods_glue
{
/**
 * @brief 注册 String 的查询、转换、分割和 Unicode 字符读取方法。
 * @param registry 当前宿主注册表。
 * @param error 接收绑定失败的诊断。
 * @return 全部入口注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error);
}  // namespace std_string_methods_glue
