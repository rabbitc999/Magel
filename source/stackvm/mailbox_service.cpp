/**
 * @file mailbox_service.cpp
 * @brief 线程邮箱、共享值与整数原子操作。
 */
#include "concurrent_call_state.hpp"

namespace stackvm
{
using namespace runtimeDetail;
/**
 * @brief 处理类型化线程邮箱及共享值的复制交付。
 * @param state 本次运行的借用状态与同步操作。
 * @param name 已解析的原生服务名称。
 * @param returnDest 接收结果的变量名称。
 * @param startupPacket 跨模块传递已复制的任务初始参数。
 * @param typedCreate 是否为带初始参数的任务创建。
 * @return 已处理时返回原调用结果，未匹配时返回空。
 */
std::optional<bool> Machine::callMailboxService(runtimeDetail::ConcurrentCallState &state, const Memory::String &name, const Memory::String &returnDest, std::shared_ptr<runtimeDetail::TransferPacket> &startupPacket, bool typedCreate)
{
    auto &pending = state.pending;
    auto &heap = state.heap;
    auto &objects = state.objects;
    auto &result = state.result;
    auto &nativeCallbackDepth = state.nativeCallbackDepth;
    auto &assign = state.assign;
    auto &fail = state.fail;
#if STACKVM_WITH_THREADS
    auto &failFromThread = state.failFromThread;
    /**
     * @brief 在并发等待时继续观察取消、失败与截止时间。
     * @param guard 当前持有的运行时锁。
     * @param ready 判断等待是否完成的条件。
     * @param deadline 最晚等待到的单调时刻。
     */
    const auto waitForChange = [&](std::unique_lock<std::mutex> &guard, const Memory::Function<bool()> &ready,
                                   std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::time_point::max()) {
        state.wait(guard, ready, deadline);
    };
#endif
    const bool messageSendThread = name == "__std_message_send_thread";
    const bool messageReceiveThread = name == "__std_message_receive_thread";
    const bool sharedCreate = name == "__std_shared_create";
    const bool sharedGet = name == "__std_shared_get";
    const bool sharedSet = name == "__std_shared_set";
    const bool sharedSwap = name == "__std_shared_swap";
    if (messageSendThread || messageReceiveThread || sharedCreate || sharedGet || sharedSet || sharedSwap)
    {
        const bool send = messageSendThread;
        const bool receive = messageReceiveThread;
        const bool withTarget = send || sharedGet || sharedSet || sharedSwap;
        const bool withValue = send || sharedCreate || sharedSet || sharedSwap;
        const std::size_t wanted = (withTarget ? 1u : 0u) + 3u + (withValue ? 1u : 0u);
        if (pending.size() != wanted)
        {
            fail("消息或共享值调用参数个数错误：" + name);
            pending.clear();
            return true;
        }
#if STACKVM_WITH_THREADS
        if ((messageSendThread || messageReceiveThread) && (!threadsEnabled_ || threadRuntime_ == nullptr))
        {
            fail("多线程未启用；解释运行或 C++ 导出时传 --threads");
            pending.clear();
            return true;
        }
#else
        /* 裁剪配置不编线程运行时与线程共享值（`STACKVM_WITH_THREADS=0`）：这几个名字明确报
         * "这个构建没有"，而不是掉到"函数入口不存在"。不提供其他执行上下文。 */
        if (messageSendThread || messageReceiveThread || sharedCreate || sharedGet || sharedSet || sharedSwap)
        {
            fail("这个构建没有多线程 / 线程共享值（STACKVM_WITH_THREADS=0）：" + name);
            pending.clear();
            return true;
        }
#endif
        if (nativeCallbackDepth != 0u && (send || receive))
        {
            fail("同步胶水回调中不能执行消息操作");
            pending.clear();
            return true;
        }
        const std::size_t base = withTarget ? 1u : 0u;
        const std::int64_t target = withTarget ? pending[0].value.integer : 0;
        const int kind = static_cast<int>(pending[base].value.integer);
        const std::int64_t layout = pending[base + 1u].value.integer;
        NativeContext context;
        context.heap = &heap;
        context.data = data_;
        context.objects = &objects;
        Memory::String type;
        const bool validType = nativeReadCString(context,
                                                 static_cast<std::size_t>(pending[base + 2u].value.integer), &type);
        if (!validType || kind < 0 || kind > 4)
        {
            fail("消息或共享值的类型描述无效");
            pending.clear();
            return true;
        }
        std::shared_ptr<TransferPacket> packet;
        if (withValue)
        {
            packet = Memory::makeShared<TransferPacket>();
            if (!captureTransfer(context, pending.back().value, kind, layout, type,
                                 packet.get()))
            {
                fail(context.error);
                pending.clear();
                return true;
            }
        }
        pending.clear();
#if STACKVM_WITH_THREADS
        if (sharedCreate || sharedGet || sharedSet || sharedSwap)
        {
            std::shared_ptr<TransferPacket> previous;
            std::int64_t handle = target;
            {
                std::lock_guard<std::mutex> guard(sharedRuntime_->mutex);
                if (sharedCreate)
                {
                    handle = sharedRuntime_->next++;
                    sharedRuntime_->values.emplace(handle, std::move(packet));
                }
                else
                {
                    const auto found = sharedRuntime_->values.find(handle);
                    if (found != sharedRuntime_->values.end() && found->second->type == type)
                    {
                        previous = found->second;
                        if (sharedSet || sharedSwap) found->second = std::move(packet);
                    }
                }
            }
            if (!sharedCreate && previous == nullptr)
            {
                fail("std::lang::Shared 类型不匹配或句柄无效：" + type);
                return true;
            }
            if (sharedCreate)
            {
                if (!returnDest.empty()) assign(returnDest, Slot::fromInt(handle));
                return true;
            }
            if (sharedGet || sharedSwap)
            {
                Slot value;
                if (!restoreTransfer(context, *previous, &value))
                    fail(context.error);
                else if (!returnDest.empty())
                    assign(returnDest, value);
            }
            return true;
        }
        if (messageSendThread)
        {
            {
                std::unique_lock<std::mutex> guard(threadRuntime_->mutex);
                const auto found = target < 0 ? threadRuntime_->workers.end() :
                                                threadRuntime_->workers.find(static_cast<std::size_t>(target));
                if (found == threadRuntime_->workers.end() || found->second->done || threadRuntime_->closing)
                {
                    fail("发送消息的线程句柄无效或已结束");
                    return true;
                }
                const auto receiver = found->second;
                const auto sender = threadRuntime_->workers.at(threadId_);
                sender->externalWaiting = true;
                waitForChange(guard, [&] { return receiver->inbox.size() < 256 || receiver->done || receiver->inboxClosed || threadRuntime_->closing; });
                sender->externalWaiting = false;
                if (!result.error.empty() || result.cancelled || result.recoverable) return true;
                if (receiver->done || receiver->inboxClosed || threadRuntime_->closing)
                {
                    fail("线程收件箱已关闭");
                    return true;
                }
                receiver->inbox.push_back(std::move(packet));
            }
            threadRuntime_->changed.notify_all();
            return true;
        }
        if (messageReceiveThread)
        {
            {
                std::unique_lock<std::mutex> guard(threadRuntime_->mutex);
                const auto self = threadRuntime_->workers.at(threadId_);
                self->receiving = true;
                threadRuntime_->changed.notify_all();
                waitForChange(guard, [&] {
                    return !self->inbox.empty() || self->inboxClosed || threadRuntime_->closing || threadRuntime_->deadlocked();
                });
                self->receiving = false;
                threadRuntime_->changed.notify_all();
                if (!result.error.empty() || result.cancelled || result.recoverable) return true;
                if (self->inbox.empty())
                {
                    Memory::String error = threadRuntime_->closing || self->inboxClosed ? "线程收件箱已关闭" :
                                                                                          "线程接收消息形成死锁";
                    for (const auto &worker : threadRuntime_->workers)
                        if (worker.second->done && !worker.second->result.ok && !worker.second->result.error.empty())
                        {
                            error = "线程 " + Memory::toString(worker.first) + " 失败：" + worker.second->result.error;
                            failFromThread(error, worker.second->result);
                            return true;
                        }
                    fail(error);
                    return true;
                }
                packet = self->inbox.front();
                if (packet->type != type)
                {
                    fail("线程消息类型不匹配：期望 " + type + "，收到 " + packet->type);
                    return true;
                }
                self->inbox.pop_front();
                threadRuntime_->changed.notify_all();
            }
        }
#endif /* STACKVM_WITH_THREADS */
        if (receive)
        {
            Slot value;
            if (!restoreTransfer(context, *packet, &value))
                fail(context.error);
            else if (!returnDest.empty())
                assign(returnDest, value);
            return true;
        }
    }

    return std::nullopt;
}
}  // namespace stackvm
