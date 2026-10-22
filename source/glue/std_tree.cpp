#include "std_tree.hpp"
#include "std_tree_storage.hpp"
#include "std_red_black_tree.hpp"
namespace std_tree_glue
{
/**
 * @brief 注册持久树、最小堆和红黑树入口。
 * @param registry 当前宿主注册表。
 * @param error 接收绑定失败的诊断。
 * @return 所有入口注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    return std_tree_storage_glue::bind(registry, error) && std_red_black_tree_glue::bind(registry, error);
}
}  // namespace std_tree_glue
