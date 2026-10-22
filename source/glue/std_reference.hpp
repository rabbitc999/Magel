#pragma once
#include "std_binding.hpp"

namespace std_reference_glue
{
/**
 * @brief 注册无拥有权引用标识的默认构造、复制和释放协议。
 * @param registry 当前宿主注册表。
 * @param error 接收绑定失败的诊断。
 * @return 所有入口绑定成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error);
}  // namespace std_reference_glue
