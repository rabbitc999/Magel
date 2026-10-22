#pragma once
#include "std_binding.hpp"

namespace std_net_glue
{
/**
 * @brief 注册网络 opcode 的独占生命周期及所有连接操作。
 * @param registry 宿主函数注册表。
 * @param error 注册失败时接收诊断。
 * @return 全部入口注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error = nullptr);
}  // namespace std_net_glue
