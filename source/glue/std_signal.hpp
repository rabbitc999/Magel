#pragma once
#include "std_binding.hpp"

namespace std_signal_glue
{
/**
 * @brief 为宿主注册 std_signal 模块的标准库接口。
 * @param registry 接收类型化函数绑定的宿主注册表。
 * @param error 绑定失败时接收原始诊断，可为空。
 * @return 全部入口绑定成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error = nullptr);
}  // namespace std_signal_glue
