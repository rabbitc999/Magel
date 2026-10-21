/**
 * @file thread_runtime.hpp
 * @brief 隔离任务、邮箱、通道与共享槽的运行期状态。
 */
#pragma once
#include "runtime_internal.hpp"
namespace stackvm
{
using runtimeDetail::TransferPacket;
#if STACKVM_WITH_THREADS
/**
 * @brief 线程运行时：每个隔离宿主线程一个 VM，消息在它们之间复制传递。
 *
 * 裁剪配置（`STACKVM_WITH_THREADS=0`）整段裁掉：不编 `std::thread` /
 * `std::mutex` / `std::condition_variable`，`std::thread::*` 与
 * `std::concurrency::Shared` 走"函数入口不存在"的普通错误。
 */
struct ThreadRuntime
{
    struct Worker
    {
        std::thread host;
        Memory::Deque<std::shared_ptr<TransferPacket>> inbox;
        RunResult result;
        std::shared_ptr<ExecutionControl> control = std::make_shared<ExecutionControl>();
        std::shared_ptr<TransferPacket> initial;
        std::shared_ptr<TransferPacket> reserved;
        bool inboxClosed = false;
        bool externalWaiting = false;
        bool done = false;
        bool joined = false;
        bool reported = false;
        bool receiving = false;
        bool joining = false;
        bool timedWaiting = false;  ///< 等待有截止时间，不能按永久等待判定死锁。
        std::size_t joinTarget = 0u;
    };

    std::mutex mutex;
    std::condition_variable changed;
    Memory::Map<std::size_t, std::shared_ptr<Worker>> workers;
    std::size_t nextId = 1u;
    struct Channel
    {
        Memory::Deque<std::shared_ptr<TransferPacket>> values;
        Memory::String type;
        std::size_t capacity = 0;
        bool closed = false;
    };
    Memory::Map<std::int64_t, std::shared_ptr<Channel>> channels;
    std::int64_t nextChannel = 1;
    std::atomic<bool> closing{ false };
    std::atomic<bool> panicked{ false };  ///< 发布快照后置位，所有 VM 安全点检查。
    Memory::String panicError;
    std::size_t panicThread = 0;
#if STACKVM_WITH_PANIC_TRACE
    Memory::Vector<DebugFrame> panicTrace;
    SourceLocation panicLocation;
#endif

    /**
     * @brief 调用方持有 mutex；全部活线程只在等待，且没有待收消息时形成死锁。
     * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
     */
    bool deadlocked() const
    {
        bool live = false;
        for (const auto &item : workers)
        {
            const Worker &worker = *item.second;
            if (worker.done) continue;
            live = true;
            if (worker.timedWaiting || worker.externalWaiting) return false;
            if (!worker.inbox.empty()) return false;
            if (worker.receiving) continue;
            if (!worker.joining) return false;
            const auto target = workers.find(worker.joinTarget);
            if (target != workers.end() && target->second->done) return false;
        }
        return live;
    }
};

struct SharedRuntime
{
    std::mutex mutex;
    std::int64_t next = 1;
    Memory::Map<std::int64_t, std::shared_ptr<TransferPacket>> values;
};
#endif /* STACKVM_WITH_THREADS */

namespace runtimeDetail
{
#if STACKVM_WITH_THREADS
/**
 * @brief 将非负毫秒等待转换为饱和截止时间；负一表示无限。
 * @param millis 等待预算。
 * @return 单调截止时间。
 */
std::chrono::steady_clock::time_point threadDeadline(std::int64_t millis);
#endif
}  // namespace runtimeDetail
}  // namespace stackvm
