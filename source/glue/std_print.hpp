#pragma once
#include "std_binding.hpp"

/**
 * @brief 标准错误流的操作系统宿主绑定。
 */
namespace std_print_glue
{
/**
 * @brief 注册标准错误文本写入入口，格式化由语言层完成。
 * @param registry 接收标准错误流函数的注册表。
 * @param error 注册失败时接收诊断，可为空。
 * @return 绑定成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error);
}  // namespace std_print_glue
