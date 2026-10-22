#pragma once
#include "std_binding.hpp"

namespace std_red_black_tree_glue
{
/**
 * @brief 注册拥有独立存储的红黑树及原地插入、删除和快照操作。
 * @param registry 宿主原生函数注册表。
 * @param error 接收注册失败的诊断。
 * @return 所有入口注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error);
}  // namespace std_red_black_tree_glue
