#pragma once
#include "stackvm.hpp"

namespace std_cas_glue
{
/**
 * @brief 注册 std::cas 的不可变 AST、精确算术及符号计算入口。
 * @param registry 宿主原生函数注册表。
 * @param error 注册失败时接收原始诊断。
 * @return 全部入口注册成功时为 true。
 */
bool bind(stackvm::NativeRegistry &registry, Memory::String *error = nullptr);
}  // namespace std_cas_glue
