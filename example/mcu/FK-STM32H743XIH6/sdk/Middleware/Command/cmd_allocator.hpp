#ifndef __CMD_ALLOCATOR_HPP
#define __CMD_ALLOCATOR_HPP

#include <stddef.h>

namespace Command
{
/* Caller-owned allocator; parsers never call malloc or global new. */
/**
 * @brief 调用方拥有的内存池接口，解析器借用且不调用全局 new 或 malloc。
 */
struct Allocator
{
    void *context;
    void *(*allocate)(void *context, size_t bytes);
    void (*release)(void *context, void *pointer);
};
}  // namespace Command

#endif
