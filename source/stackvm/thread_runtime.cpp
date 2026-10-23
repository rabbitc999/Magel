/**
 * @file thread_runtime.cpp
 * @brief 共享线程运行状态与截止时间计算。
 */
#include "runtime_internal.hpp"

namespace stackvm
{
using namespace runtimeDetail;
namespace runtimeDetail
{
#if STACKVM_WITH_THREADS
/**
 * @brief 将非负毫秒等待转换为饱和截止时间；负一表示无限。
 * @param millis 等待预算。
 * @return 单调截止时间。
 */
std::chrono::steady_clock::time_point threadDeadline(std::int64_t millis)
{
    const auto limit = std::chrono::steady_clock::time_point::max();
    const auto now = std::chrono::steady_clock::now();
    const auto available = std::chrono::duration_cast<std::chrono::milliseconds>(limit - now).count();
    return millis < 0 || millis >= available ? limit : now + std::chrono::milliseconds(millis);
}
#endif
}  // namespace runtimeDetail
}  // namespace stackvm
