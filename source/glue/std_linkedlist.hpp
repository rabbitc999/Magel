#pragma once
#include "std_binding.hpp"

namespace std_linkedlist_glue
{
/**
 * @brief 注册 std::linkedlist 的全部胶水入口。
 * @param registry 函数注册表。
 * @param error 注册失败时的错误说明。
 * @return 全部注册成功返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error);
}  // namespace std_linkedlist_glue
