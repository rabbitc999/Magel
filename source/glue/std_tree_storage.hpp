#pragma once
#include "std_binding.hpp"
namespace std_tree_storage_glue
{
/**
 * @brief 注册二叉搜索树、通用树和最小堆的持久状态协议。
 * @param registry 当前宿主注册表。
 * @param error 接收绑定失败的诊断。
 * @return 所有入口注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error);
}  // namespace std_tree_storage_glue
