#include "std_print.hpp"
#include "../chali_glue.hpp"
#include <cstdio>
#include <mutex>
#include <utility>

namespace std_print_glue
{
namespace
{
/**
 * @brief 将已经格式化的文本完整写入标准错误流并刷新。
 * @param context 用于传播宿主写入失败的调用上下文。
 * @param text 不进行二次格式解析的 UTF-8 文本。
 * @return 写入成功返回零，失败时通过上下文保留错误。
 * @note 同一进程中的本入口按整次调用串行输出，不接入 VM 标准输出桶。
 */
std::int64_t writeError(chali_glue::Context &context, Memory::String text)
{
    static std::mutex outputMutex;
    const std::lock_guard<std::mutex> lock(outputMutex);
    if ((!text.empty() && std::fwrite(text.data(), 1u, text.size(), stderr) != text.size()) || std::fflush(stderr) != 0)
        context.fail("标准错误流写入失败");
    return 0;
}
/**
 * @brief 为已格式化文本追加换行，再通过同一互斥入口写入标准错误流。
 * @param context 用于传播写入失败的宿主上下文。
 * @param text 由胶水拥有的文本副本，追加换行后自动回收。
 * @return 写入成功返回零，失败通过上下文保留诊断。
 */
std::int64_t writeErrorLine(chali_glue::Context &context, Memory::String text)
{
    text += '\n';
    return writeError(context, std::move(text));
}
}  // namespace

/**
 * @brief 为标准库注册接收已格式化文本的 stderr 胶水。
 * @param registry 接收类型化入口的宿主注册表。
 * @param error 绑定失败时接收原始诊断，可为空。
 * @return 注册成功时返回 true。
 */
bool bind(chali_glue::Registry &registry, Memory::String *error)
{
    return registry.bindTypedWithContext<std::int64_t, Memory::String>("std::lang::__std_stderr_write", writeError, error) && registry.bindTypedWithContext<std::int64_t, Memory::String>("std::lang::__std_eprint", writeError, error) && registry.bindTypedWithContext<std::int64_t, Memory::String>("std::lang::__std_eprintln", writeErrorLine, error);
}
}  // namespace std_print_glue
