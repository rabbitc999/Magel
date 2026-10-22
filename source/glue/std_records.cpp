#include "std_native_record.hpp"
#include "std_records.hpp"
#include "std_reference.hpp"
#include "std_string_methods.hpp"
#include "std_collection.hpp"
#include "../chali_glue.hpp"
#include "std_big_number.hpp"
#include "std_cas.hpp"
#include "std_file.hpp"
#include "std_process.hpp"
#include "std_print.hpp"

namespace chali_standard_records
{
/**
 * @brief 为独立桌面宿主注册标准不透明记录，VM 核心不参与具体类型绑定。
 * @param registry 宿主拥有的胶水注册表。
 * @param error 注册失败的诊断输出。
 * @return 所有模块绑定成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    return std_string_methods_glue::bind(registry, error) && std_reference_glue::bind(registry, error) && collection_glue::bind(registry.native(), error) && native_record_glue::bindString(registry.native(), error) && big_number_glue::bind(registry.native(), error) && std_cas_glue::bind(registry.native(), error) && file_native_glue::bind(registry.native(), error) && std_process_glue::bind(registry, error) && std_print_glue::bind(registry, error);
}
}  // namespace chali_standard_records
