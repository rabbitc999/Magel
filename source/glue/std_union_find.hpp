#pragma once
#include "std_binding.hpp"

namespace std_union_find_glue
{
/**
 * @brief 注册持久并查集的原生记录及原地操作。
 * @param registry 当前宿主的函数注册表。
 * @param error 接收注册失败的诊断。
 * @return 所有入口注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error);
}  // namespace std_union_find_glue
