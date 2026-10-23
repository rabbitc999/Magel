/**
 * @file thread_service.cpp
 * @brief 隔离线程的创建、取消、等待与完成结果。
 */
#include "concurrent_call_state.hpp"

namespace stackvm
{
using namespace runtimeDetail;
/**
 * @brief 处理隔离任务创建、等待、字符串邮箱和完成回收。
 * @param state 本次运行的借用状态与同步操作。
 * @param name 已解析的原生服务名称。
 * @param returnDest 接收结果的变量名称。
 * @param startupPacket 跨模块传递已复制的任务初始参数。
 * @param typedCreate 是否为带初始参数的任务创建。
 * @return 已处理时返回原调用结果，未匹配时返回空。
 */
std::optional<bool> Machine::callThreadService(runtimeDetail::ConcurrentCallState &state, const Memory::String &name, const Memory::String &returnDest, std::shared_ptr<runtimeDetail::TransferPacket> &startupPacket, bool typedCreate)
{
    const auto &program = state.program;
    auto &pending = state.pending;
    auto &heap = state.heap;
    auto &result = state.result;
    auto &nativeCallbackDepth = state.nativeCallbackDepth;
    const auto stepLimit = state.stepLimit;
    auto &assign = state.assign;
    auto &fail = state.fail;
    auto &emitText = state.emitText;
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
#if STACKVM_WITH_THREADS
    if (name.rfind("std::thread::", 0u) == 0u || typedCreate)
    {
        const bool resultWait = name == "std::thread::__waitResult";
        const bool timedWait = name == "std::thread::waitFor" || resultWait;
        const bool create = name == "std::thread::create" || typedCreate;
        const std::size_t wanted = create || name == "std::thread::wait" ? 1u : name == "std::thread::send" || timedWait ? 2u :
                                                                                                                           0u;
        if (!create && name != "std::thread::wait" && !timedWait && name != "std::thread::send" && name != "std::thread::receive" && name != "std::thread::current" && name != "std::thread::parent")
            return false;
        if (!threadsEnabled_ || threadRuntime_ == nullptr)
        {
            fail("多线程未启用；解释运行或 C++ 导出时传 --threads");
            pending.clear();
            return true;
        }
        if (nativeCallbackDepth != 0u)
        {
            fail("同步胶水回调中不能执行线程操作");
            pending.clear();
            return true;
        }
        if (pending.size() != wanted)
        {
            fail("线程函数 " + name + " 需要 " + Memory::toString(wanted) + " 个参数");
            pending.clear();
            return true;
        }
        if (name == "std::thread::current" || name == "std::thread::parent")
        {
            pending.clear();
            if (!returnDest.empty()) assign(returnDest, Slot::fromInt(
                                                            static_cast<std::int64_t>(name == "std::thread::current" ? threadId_ : parentThreadId_)));
            return true;
        }
        if (create)
        {
            if (heapAllocator_ != nullptr && !threadHeapFactory_)
            {
                fail("自定义 VM 字节堆需要为子线程设置独立堆工厂");
                pending.clear();
                return true;
            }
            const std::size_t address = static_cast<std::size_t>(pending[0].value.integer);
            pending.clear();
            if (isObjectHandle(static_cast<std::int64_t>(address)))
            {
                fail("线程异步入口不支持带捕获的 lambda");
                return true;
            }
            const Memory::String label = data_ == nullptr ? Memory::String() : program.symbolKey(data_->readCString(address));
            if (address == 0u || program.labels.find(label) == program.labels.end())
            {
                fail("std::thread::create 收到了无效任务函数指针");
                return true;
            }
            if (data_ == nullptr)
            {
                fail("std::thread::create 需要数据区");
                return true;
            }
            DataArea snapshot = *data_;
            const auto worker = Memory::makeShared<ThreadRuntime::Worker>();
            worker->initial = startupPacket;
            std::size_t id = 0u;
            std::unique_lock<std::mutex> creationGuard(threadRuntime_->mutex);
            if (threadRuntime_->closing)
            {
                fail("主线程已经结束，不能再创建线程");
                return true;
            }
            id = threadRuntime_->nextId++;
            threadRuntime_->workers.emplace(id, worker);
            const auto group = threadRuntime_;
            const auto shared = sharedRuntime_;
            const auto registry = nativeRegistry_;
            const auto heapFactory = threadHeapFactory_;
            auto *const workerAllocator = Memory::currentRuntimeAllocator();
            const std::size_t parentId = threadId_;
            const std::size_t slots = stackLimit_;
            const auto services = hostServices_;
            const auto clock = timeCallback_;
            const bool childPanicDetails = panicDetailsEnabled_;
            try
            {
                worker->host = std::thread([worker, group, shared, registry, heapFactory, &program, label,
                                            snapshot = std::move(snapshot), parentId, id,
                                            slots, stepLimit, workerAllocator, childPanicDetails, services, clock]() mutable {
                    Memory::AllocationScope allocationScope(workerAllocator);
                    RunResult childResult;
                    try
                    {
                        Machine child;
                        child.setPanicDetailsEnabled(childPanicDetails);
                        child.control_ = worker->control;
                        child.setHostServices(services);
                        child.setTimeCallback(clock);
                        Memory::UniquePtr<HeapAllocator> workerHeap = heapFactory ? heapFactory() : nullptr;
                        child.setHeapAllocator(workerHeap.get());
                        child.setThreadHeapFactory(heapFactory);
                        child.setData(&snapshot);
                        child.setNativeRegistry(registry);
                        child.setStackLimit(slots);
                        child.setThreadsEnabled(true);
                        child.threadRuntime_ = group;
                        child.sharedRuntime_ = shared;
                        child.threadId_ = id;
                        child.parentThreadId_ = parentId;
                        child.entryLabel_ = label;
                        childResult = child.run(program, {}, stepLimit);
                    } catch (const std::exception &error)
                    {
                        childResult.error = error.what();
                    } catch (...)
                    {
                        childResult.error = "线程运行时发生未知宿主异常";
                    }
                    {
                        std::lock_guard<std::mutex> guard(group->mutex);
                        worker->result = std::move(childResult);
                        worker->done = true;
                    }
                    group->changed.notify_all();
                });
            } catch (const std::exception &error)
            {
                threadRuntime_->workers.erase(id);
                fail(Memory::String("创建宿主线程失败：") + error.what());
                return true;
            }
            creationGuard.unlock();
            if (!returnDest.empty()) assign(returnDest, Slot::fromInt(static_cast<std::int64_t>(id)));
            return true;
        }
        if (name == "std::thread::send")
        {
            const std::int64_t target = pending[0].value.integer;
            NativeContext context;
            context.heap = &heap;
            context.data = data_;
            Memory::String message;
            const bool validText = nativeReadCString(context,
                                                     static_cast<std::size_t>(pending[1].value.integer), &message);
            pending.clear();
            if (!validText)
            {
                fail("std::thread::send 收到了无效字符串");
                return true;
            }
            {
                std::unique_lock<std::mutex> guard(threadRuntime_->mutex);
                const auto found = target < 0 ? threadRuntime_->workers.end() :
                                                threadRuntime_->workers.find(static_cast<std::size_t>(target));
                if (found == threadRuntime_->workers.end() || found->second->done || threadRuntime_->closing)
                {
                    fail("std::thread::send 收到了无效或已结束的线程句柄");
                    return true;
                }
                auto packet = Memory::makeShared<TransferPacket>();
                packet->type = "std::lang::String";
                packet->value.isText = true;
                packet->value.text = std::move(message);
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
        if (name == "std::thread::receive")
        {
            pending.clear();
            Memory::String message;
            {
                std::unique_lock<std::mutex> guard(threadRuntime_->mutex);
                const auto self = threadRuntime_->workers.at(threadId_);
                self->receiving = true;
                threadRuntime_->changed.notify_all();
                /* 等待期间继续观察停止、panic 和看门狗。 */
                waitForChange(guard, [&] {
                    return !self->inbox.empty() || self->inboxClosed || threadRuntime_->closing || threadRuntime_->deadlocked();
                });
                self->receiving = false;
                threadRuntime_->changed.notify_all();
                if (!result.error.empty() || result.cancelled || result.recoverable) return true;
                if (self->inbox.empty())
                {
                    if (threadRuntime_->closing || self->inboxClosed)
                    {
                        fail("线程收件箱已关闭");
                        return true;
                    }
                    /* 死锁只是"谁都等不到"的表象：真有线程先失败，报它的错误，
                     * 否则真正的原因（比如线程里的运行期错误）会被死锁盖掉。 */
                    for (const auto &item : threadRuntime_->workers)
                    {
                        if (item.first == 0u || item.second->reported) continue;
                        ThreadRuntime::Worker &worker = *item.second;
                        if (worker.done && !worker.result.ok && !worker.result.error.empty())
                        {
                            worker.reported = true;
                            failFromThread("线程 " + Memory::toString(item.first) + " 失败：" + worker.result.error, worker.result);
                            return true;
                        }
                    }
                    fail("线程接收消息形成死锁");
                    return true;
                }
                if (self->inbox.front()->type != "std::lang::String")
                {
                    fail("线程消息类型不匹配：期望 string，收到 " + self->inbox.front()->type);
                    return true;
                }
                message = self->inbox.front()->value.text;
                self->inbox.pop_front();
                threadRuntime_->changed.notify_all();
            }
            NativeContext context;
            context.heap = &heap;
            const std::size_t address = nativeAllocString(context, message);
            if (address == 0u)
            {
                fail(context.error);
                return true;
            }
            if (!returnDest.empty()) assign(returnDest, Slot::fromInt(static_cast<std::int64_t>(address)));
            return true;
        }
        const std::int64_t raw = pending[0].value.integer;
        const std::int64_t millis = timedWait ? pending[1].value.integer : 0;
        pending.clear();
        if (timedWait && millis < (resultWait ? -1 : 0))
        {
            fail("std::thread::waitFor 的超时毫秒数不能为负数");
            return true;
        }
        auto deadline = std::chrono::steady_clock::time_point::max();
        if (timedWait)
        {
            const auto now = std::chrono::steady_clock::now();
            const auto available = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            if (millis >= 0 && millis < available) deadline = now + std::chrono::milliseconds(millis);
        }
        std::shared_ptr<ThreadRuntime::Worker> worker;
        {
            std::unique_lock<std::mutex> guard(threadRuntime_->mutex);
            const auto found = raw <= 0 ? threadRuntime_->workers.end() :
                                          threadRuntime_->workers.find(static_cast<std::size_t>(raw));
            if (found == threadRuntime_->workers.end() || static_cast<std::size_t>(raw) == threadId_ || found->second->joined)
            {
                fail("std::thread::wait 收到了无效、自身或已等待的线程句柄");
                return true;
            }
            worker = found->second;
            worker->joined = true;
            const auto self = threadRuntime_->workers.at(threadId_);
            self->joining = true;
            self->timedWaiting = timedWait;
            self->joinTarget = static_cast<std::size_t>(raw);
            threadRuntime_->changed.notify_all();
            waitForChange(guard, [&] { return worker->done || threadRuntime_->closing || (threadRuntime_->deadlocked() && !worker->receiving); }, deadline);
            self->joining = false;
            self->timedWaiting = false;
            self->joinTarget = 0u;
            threadRuntime_->changed.notify_all();
            if (!result.error.empty() || result.cancelled || result.recoverable)
            {
                worker->joined = false;
                return true;
            }
            if (!worker->done)
            {
                worker->joined = false;
                if (timedWait && !threadRuntime_->closing)
                {
                    if (!returnDest.empty()) assign(returnDest, Slot::fromInt(resultWait ? 1 : 0));
                    return true;
                }
                fail(threadRuntime_->closing ? "线程等待被主程序结束中断" : "线程等待形成死锁");
                return true;
            }
        }
        if (worker->host.joinable()) worker->host.join();
        emitText(worker->result.text.data(), worker->result.text.size());
        if (!worker->result.output.empty())
        {
            if (!result.output.empty()) result.output += ' ';
            result.output += worker->result.output;
        }
        {
            std::lock_guard<std::mutex> guard(threadRuntime_->mutex);
            worker->reported = true;
        }
        if (resultWait)
        {
            if (!returnDest.empty()) assign(returnDest, Slot::fromInt(worker->result.cancelled ? 3 : worker->result.ok ? 0 :
                                                                                                                         5));
        }
        else if (!worker->result.ok && !worker->result.cancelled)
            failFromThread("线程 " + Memory::toString(raw) + " 失败：" + worker->result.error, worker->result);
        else if (timedWait && !returnDest.empty())
            assign(returnDest, Slot::fromInt(1));
        return true;
    }
#endif /* STACKVM_WITH_THREADS */

    return std::nullopt;
}
}  // namespace stackvm
