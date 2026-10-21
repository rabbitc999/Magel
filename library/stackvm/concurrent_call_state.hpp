/**
 * @file concurrent_call_state.hpp
 * @brief 并发原生服务借用的执行状态和同步操作，不持有跨运行的 VM 地址。
 */
#pragma once
#include "runtime_internal.hpp"
#include "thread_runtime.hpp"

namespace stackvm::runtimeDetail
{
/**
 * @brief 并发服务仅在当前调用内使用的执行环境。
 */
struct ConcurrentCallState
{
    const Program &program;
    Memory::Vector<PendingArg> &pending;
    HeapAllocator &heap;
    ObjectStore &objects;
    RunResult &result;
    std::size_t &nativeCallbackDepth;
    std::size_t stepLimit;
    Memory::Function<void(const Memory::String &, const Slot &)> assign;
    Memory::Function<void(const Memory::String &)> fail;
    Memory::Function<void(const char *, std::size_t)> emitText;
    Memory::Function<void()> &checkExecution;
#if STACKVM_WITH_THREADS
    RuntimeEvent &receivedEvent;
    Memory::Function<void(const Memory::String &, const RunResult &)> failFromThread;
    Memory::Function<void(std::unique_lock<std::mutex> &, const Memory::Function<bool()> &, std::chrono::steady_clock::time_point)> wait;
#endif
};
}  // namespace stackvm::runtimeDetail
