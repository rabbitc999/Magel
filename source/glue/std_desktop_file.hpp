#pragma once
#include "std_file_api.hpp"

namespace magel_file
{
/**
 * @brief 取得当前平台默认的文件后端接口。
 * @retval 对应对象的引用。
 */
FileApi &defaultFileApi();
}  // namespace magel_file
