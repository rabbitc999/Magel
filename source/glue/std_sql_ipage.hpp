#pragma once
#include "std_binding.hpp"

namespace std_sql_ipage_glue
{
/**
 * @brief 注册使用反射元数据的 ipage SQL 生成入口。
 * @param registry 宿主注册表。
 * @param error 注册失败时的诊断。
 * @return 所有入口注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error);
}  // namespace std_sql_ipage_glue
