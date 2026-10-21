/**
 * @file transfer_service.cpp
 * @brief 值载荷、泛型通道及显式事件通信。
 */
#include "concurrent_call_state.hpp"

namespace stackvm
{
using namespace runtimeDetail;
/**
 * @brief 处理复制载荷、有界通道、原子槽、事件和任务控制服务。
 * @param state 本次运行的借用状态与同步操作。
 * @param name 已解析的原生服务名称。
 * @param returnDest 接收结果的变量名称。
 * @param startupPacket 跨模块传递已复制的任务初始参数。
 * @param typedCreate 是否为带初始参数的任务创建。
 * @return 已处理时返回原调用结果，未匹配时返回空。
 */
std::optional<bool> Machine::callTransferService(runtimeDetail::ConcurrentCallState &state, const Memory::String &name, const Memory::String &returnDest, std::shared_ptr<runtimeDetail::TransferPacket> &startupPacket, bool typedCreate)
{
    auto &pending = state.pending;
    auto &heap = state.heap;
    auto &objects = state.objects;
    auto &result = state.result;
    auto &nativeCallbackDepth = state.nativeCallbackDepth;
    auto &assign = state.assign;
    auto &fail = state.fail;
    auto &checkExecution = state.checkExecution;
#if STACKVM_WITH_THREADS
    auto &receivedEvent = state.receivedEvent;
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
    if (typedCreate || name == "__std_channel_send" || name == "__std_mail_send" || name == "__std_channel_take")
    {
        const bool take = name == "__std_channel_take";
        const bool mail = name == "__std_mail_send";
        const std::size_t expected = take ? 3 : typedCreate ? 5 :
                                                              6;
        if (!threadsEnabled_ || !threadRuntime_ || nativeCallbackDepth || pending.size() != expected)
        {
            fail("跨线程传值需要启用线程、正确参数且不位于同步胶水回调内");
            pending.clear();
            return true;
        }
        NativeContext context;
        context.heap = &heap;
        context.data = data_;
        context.objects = &objects;
        const std::size_t base = take ? 0 : 1;
        Memory::String type;
        if (!nativeReadCString(context, static_cast<std::size_t>(pending[base + 2].value.integer), &type))
        {
            fail(context.error);
            pending.clear();
            return true;
        }
        if (take)
        {
            std::shared_ptr<TransferPacket> packet;
            {
                std::lock_guard<std::mutex> lock(threadRuntime_->mutex);
                packet = threadRuntime_->workers.at(threadId_)->reserved;
            }
            pending.clear();
            if (!packet || packet->type != type)
            {
                fail("线程或通道消息类型不匹配：期望 " + type);
                return true;
            }
            Slot value;
            if (!restoreTransfer(context, *packet, &value))
                fail(context.error);
            else if (!returnDest.empty())
                assign(returnDest, value);
            {
                std::lock_guard<std::mutex> lock(threadRuntime_->mutex);
                threadRuntime_->workers.at(threadId_)->reserved.reset();
            }
            return true;
        }
        const auto target = pending[0].value.integer;
        const auto timeout = typedCreate ? -1 : pending[5].value.integer;
        auto packet = Memory::makeShared<TransferPacket>();
        if (timeout < -1 || !captureTransfer(context, pending[4].value, static_cast<int>(pending[1].value.integer), pending[2].value.integer, type, packet.get()))
        {
            fail(context.error.empty() ? Memory::String("等待毫秒数不能小于 -1") : context.error);
            pending.clear();
            return true;
        }
        if (packet->value.objectIdentity == 0 && type.find("fn(") != Memory::String::npos && isObjectHandle(packet->value.integer))
        {
            fail("异步任务与消息不能传递带捕获的函数值");
            pending.clear();
            return true;
        }
        if (typedCreate)
        {
            startupPacket = packet;
            pending.resize(1);
        }
        else
        {
            pending.clear();
            std::unique_lock<std::mutex> lock(threadRuntime_->mutex);
            const auto self = threadRuntime_->workers.at(threadId_);
            std::shared_ptr<ThreadRuntime::Channel> channel;
            std::shared_ptr<ThreadRuntime::Worker> worker;
            if (mail)
            {
                const auto found = target < 0 ? threadRuntime_->workers.end() : threadRuntime_->workers.find(static_cast<std::size_t>(target));
                if (found != threadRuntime_->workers.end()) worker = found->second;
            }
            else
            {
                const auto found = threadRuntime_->channels.find(target);
                if (found != threadRuntime_->channels.end()) channel = found->second;
            }
            if ((!mail && !channel) || (mail && !worker))
            {
                if (!returnDest.empty()) assign(returnDest, Slot::fromInt(!mail && target > 0 && target < threadRuntime_->nextChannel ? 2 : 6));
                return true;
            }
            self->externalWaiting = true;
            waitForChange(lock, [&] { return mail ? worker->done || worker->inboxClosed || worker->inbox.size() < 256 || threadRuntime_->closing : channel->closed || channel->values.size() < channel->capacity || threadRuntime_->closing; }, threadDeadline(timeout));
            self->externalWaiting = false;
            std::int64_t status = 0;
            if (result.cancelled)
                status = 3;
            else if (threadRuntime_->closing || (mail ? worker->done || worker->inboxClosed : channel->closed))
                status = 2;
            else if (mail ? worker->inbox.size() >= 256 : channel->values.size() >= channel->capacity)
                status = timeout == 0 ? 4 : 1;
            else if (!mail && !channel->type.empty() && channel->type != type)
            {
                fail("通道消息类型不匹配");
                return true;
            }
            else if (mail)
                worker->inbox.push_back(std::move(packet));
            else
            {
                channel->type = type;
                channel->values.push_back(std::move(packet));
            }
            threadRuntime_->changed.notify_all();
            if (!returnDest.empty()) assign(returnDest, Slot::fromInt(status));
            return true;
        }
    }
    if (name == "std::concurrency::__channelCreate" || name == "std::concurrency::__channelClose" || name == "std::concurrency::__poll" || name == "std::concurrency::__fetchAdd")
    {
        const bool create = name == "std::concurrency::__channelCreate";
        const bool close = name == "std::concurrency::__channelClose";
        const bool add = name == "std::concurrency::__fetchAdd";
        if (!threadsEnabled_ || !threadRuntime_ || nativeCallbackDepth || pending.size() != (create || close ? 1u : 2u))
        {
            fail("通道与共享原子操作的运行环境或参数无效");
            pending.clear();
            return true;
        }
        const auto handle = pending[0].value.integer;
        const auto millis = create || close ? 0 : pending[1].value.integer;
        pending.clear();
        if (add)
        {
            std::lock_guard<std::mutex> lock(sharedRuntime_->mutex);
            const auto found = sharedRuntime_->values.find(handle);
            if (found == sharedRuntime_->values.end() || found->second->type != "int64")
            {
                fail("fetchAdd 仅支持有效的 Shared<int64> 槽");
                return true;
            }
            const auto old = found->second->value.integer;
            auto replacement = Memory::makeShared<TransferPacket>(*found->second);
            replacement->value.integer = static_cast<std::int64_t>(static_cast<std::uint64_t>(old) + static_cast<std::uint64_t>(millis));
            found->second = std::move(replacement);
            if (!returnDest.empty()) assign(returnDest, Slot::fromInt(old));
            return true;
        }
        std::unique_lock<std::mutex> lock(threadRuntime_->mutex);
        if (create)
        {
            if (handle <= 0 || handle > 1048576 || threadRuntime_->channels.size() >= 1024)
            {
                fail("通道容量须为 1..1048576，活动通道预算为 1024");
                return true;
            }
            const auto id = threadRuntime_->nextChannel++;
            auto channel = Memory::makeShared<ThreadRuntime::Channel>();
            channel->capacity = static_cast<std::size_t>(handle);
            threadRuntime_->channels.emplace(id, std::move(channel));
            if (!returnDest.empty()) assign(returnDest, Slot::fromInt(id));
            return true;
        }
        const auto self = threadRuntime_->workers.at(threadId_);
        std::shared_ptr<ThreadRuntime::Channel> channel;
        if (handle != 0)
        {
            const auto found = threadRuntime_->channels.find(handle);
            if (found == threadRuntime_->channels.end())
            {
                if (!returnDest.empty()) assign(returnDest, Slot::fromInt(handle > 0 && handle < threadRuntime_->nextChannel ? 2 : 6));
                return true;
            }
            channel = found->second;
        }
        if (close)
        {
            if (channel)
            {
                channel->closed = true;
                if (channel->values.empty()) threadRuntime_->channels.erase(handle);
            }
            else
                self->inboxClosed = true;
            threadRuntime_->changed.notify_all();
            return true;
        }
        if (millis < -1)
        {
            fail("等待毫秒数不能小于 -1");
            return true;
        }
        self->externalWaiting = true;
        waitForChange(lock, [&] { return channel ? !channel->values.empty() || channel->closed || threadRuntime_->closing : !self->inbox.empty() || self->inboxClosed || threadRuntime_->closing; }, threadDeadline(millis));
        self->externalWaiting = false;
        std::int64_t status = 0;
        if (result.cancelled)
            status = 3;
        else if (channel ? !channel->values.empty() : !self->inbox.empty())
        {
            auto &values = channel ? channel->values : self->inbox;
            self->reserved = values.front();
            values.pop_front();
            if (channel && channel->closed && values.empty()) threadRuntime_->channels.erase(handle);
        }
        else
            status = (threadRuntime_->closing || (channel ? channel->closed : self->inboxClosed)) ? 2 : 1;
        threadRuntime_->changed.notify_all();
        if (!returnDest.empty()) assign(returnDest, Slot::fromInt(status));
        return true;
    }
    if (name.rfind("std::event::__", 0) == 0)
    {
        const bool receive = name == "std::event::__receive";
        const bool timer = name == "std::event::__timer";
        const bool destroy = name == "std::event::__destroy";
        const bool source = name == "std::event::__source";
        const bool tag = name == "std::event::__tag";
        const bool missed = name == "std::event::__missed";
        const bool id = name == "std::event::__id";
        const bool post = name == "std::event::__post";
        const bool close = name == "std::event::__close";
        if (!receive && !timer && !destroy && !source && !tag && !missed && !id && !post && !close) return std::nullopt;
        const auto wanted = timer || post ? 2u : receive || destroy ? 1u :
                                                                      0u;
        if (nativeCallbackDepth || pending.size() != wanted)
        {
            fail("事件操作的参数或同步回调上下文无效");
            pending.clear();
            return true;
        }
        const auto first = wanted ? pending[0].value.integer : 0;
        const auto second = wanted > 1 ? pending[1].value.integer : 0;
        pending.clear();
        std::int64_t value = 0;
        if (receive)
        {
            if (first < -1)
            {
                fail("事件等待毫秒数不能小于 -1");
                return true;
            }
            value = static_cast<std::int64_t>(receiveEvent(events_, receivedEvent, first, [&] { checkExecution(); return !result.error.empty() || result.cancelled || result.recoverable; }));
            checkExecution();
        }
        else if (timer)
            value = createEventTimer(events_, first, second != 0);
        else if (destroy)
            destroyEventTimer(events_, first);
        else if (post)
            value = static_cast<std::int64_t>(postEvent(events_, RuntimeEvent{ 0, first, second, 0 }));
        else if (close)
            closeEventQueue(events_);
        else if (source)
            value = receivedEvent.source;
        else if (tag)
            value = receivedEvent.tag;
        else if (missed)
            value = static_cast<std::int64_t>(receivedEvent.missed);
        else
            value = receivedEvent.timer;
        if (!returnDest.empty()) assign(returnDest, Slot::fromInt(value));
        return true;
    }
    if (name == "std::thread::requestStop" || name == "std::thread::stopRequested" || name == "std::thread::state" || name == "std::thread::__error" || name == "std::thread::__trace" || name == "std::thread::sleepFor")
    {
        const bool requested = name == "std::thread::stopRequested";
        if (!threadsEnabled_ || !threadRuntime_ || nativeCallbackDepth || pending.size() != (requested ? 0u : 1u))
        {
            fail("线程控制操作的环境或参数无效");
            pending.clear();
            return true;
        }
        const auto target = requested ? static_cast<std::int64_t>(threadId_) : pending[0].value.integer;
        pending.clear();
        if (name == "std::thread::sleepFor")
        {
            if (target < 0)
            {
                fail("睡眠毫秒数不能为负数");
                return true;
            }
            std::unique_lock<std::mutex> lock(threadRuntime_->mutex);
            const auto self = threadRuntime_->workers.at(threadId_);
            self->externalWaiting = true;
            waitForChange(lock, [&] { return threadRuntime_->closing.load(); }, threadDeadline(target));
            self->externalWaiting = false;
            if (!returnDest.empty()) assign(returnDest, Slot::fromInt(result.cancelled ? 3 : 0));
            return true;
        }
        std::lock_guard<std::mutex> lock(threadRuntime_->mutex);
        const auto found = target < 0 ? threadRuntime_->workers.end() : threadRuntime_->workers.find(static_cast<std::size_t>(target));
        if (found == threadRuntime_->workers.end())
        {
            fail("线程控制操作收到无效句柄");
            return true;
        }
        const auto worker = found->second;
        if (name == "std::thread::requestStop")
        {
            if (!worker->done) worker->control->requested.store(true, std::memory_order_release);
            threadRuntime_->changed.notify_all();
            return true;
        }
        if (name == "std::thread::__error" || name == "std::thread::__trace")
        {
            Memory::String message = worker->done ? worker->result.error : Memory::String();
            if (name == "std::thread::__trace")
            {
                message.clear();
                if (worker->done)
                    for (const auto &frame : worker->result.sourceTrace) message += frame.function + " " + frame.location.toString() + "\n";
            }
            NativeContext context;
            context.heap = &heap;
            const auto address = nativeAllocString(context, message);
            if (!context.error.empty())
                fail(context.error);
            else if (!returnDest.empty())
                assign(returnDest, Slot::fromInt(static_cast<std::int64_t>(address)));
            return true;
        }
        const auto value = requested ? (worker->control->requested.load(std::memory_order_acquire) ? 1 : 0) : !worker->done        ? 0 :
                                                                                                          worker->result.cancelled ? 3 :
                                                                                                          worker->result.ok        ? 1 :
                                                                                                                                     2;
        if (!returnDest.empty()) assign(returnDest, Slot::fromInt(value));
        return true;
    }
#endif

    return std::nullopt;
}
}  // namespace stackvm
