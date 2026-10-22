#pragma once
#include "std_binding.hpp"

/**
 * @brief 注册演示宿主的类型化接口。
 * @param registry 接收函数绑定的宿主注册表。
 * @param error 绑定失败时接收原始诊断。
 * @return 全部绑定成功时返回 true。
 */
bool bindChaLiGlue(chali_glue::Registry &registry, Memory::String *error);
