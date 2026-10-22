#pragma once
#include "std_binding.hpp"

namespace chali_standard_records
{
/**
 * @brief 为独立桌面宿主注册标准不透明记录，VM 核心不参与具体类型绑定。
 * @param registry 宿主拥有的胶水注册表。
 * @param error 注册失败的诊断输出。
 * @return 所有模块绑定成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error = nullptr);
}  // namespace chali_standard_records
