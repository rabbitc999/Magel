#pragma once
#include "stackvm.hpp"

namespace big_number_glue
{
/**
 * @brief 将全部大数操作注册为普通胶水函数。
 * @param registry 目标宿主注册表。
 * @param error 接收绑定失败原因。
 * @return 全部注册成功时为 true。
 */
bool bind(stackvm::NativeRegistry &registry, Memory::String *error = nullptr);
}  // namespace big_number_glue
