#pragma once
#include "std_binding.hpp"

namespace std_image_glue
{
/**
 * @brief 注册图片处理胶水。
 * @param registry 注册表。
 * @param error 绑定失败说明。
 * @return 是否全部成功。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error = nullptr);
}  // namespace std_image_glue
