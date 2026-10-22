#pragma once
#include "std_binding.hpp"

namespace std_compress_glue
{
/**
 * @brief 注册 `std::compress` 的宿主绑定。
 * @param registry 宿主函数注册表。
 * @param error 注册失败时写入中文错误信息。
 * @return 全部绑定成功返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error);
}  // namespace std_compress_glue
