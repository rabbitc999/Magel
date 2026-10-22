#include "../chali_glue.hpp"
#include "std_glue.hpp"
#include "std_registry.hpp"

/**
 * @brief 为独立宿主注册与编译器一致的标准库胶水。
 * @param registry 接收绑定的宿主注册表。
 * @param error 绑定失败时接收原始诊断。
 * @return 全部标准库模块注册成功时返回 true。
 */
bool bindChaLiStandardGlue(chali_glue::Registry &registry, Memory::String *error)
{
    return std_registry::bindStandardGlue(registry, error);
}
