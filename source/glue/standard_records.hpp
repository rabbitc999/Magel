#ifndef MAGEL_STANDARD_RECORDS_HPP
#define MAGEL_STANDARD_RECORDS_HPP
#include "../magel_glue.hpp"
#include "big_number_natives.hpp"
#include "file_natives.hpp"
#include "std_process.hpp"

namespace magel_standard_records
{
/**
 * @brief 为独立桌面宿主注册标准不透明记录，VM 核心不参与具体类型绑定。
 * @param registry 宿主拥有的胶水注册表。
 * @param error 注册失败的诊断输出。
 * @return 所有模块绑定成功时返回 true。
 */
inline bool bind(magel_glue::Registry &registry, Memory::String *error = nullptr)
{
    return native_record_glue::bindString(registry.native(), error) && big_number_glue::bind(registry.native(), error) && file_native_glue::bind(registry.native(), error) && std_process_glue::bind(registry, error);
}
}  // namespace magel_standard_records
#endif
