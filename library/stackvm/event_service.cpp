#include "event_service.hpp"
#if STACKVM_WITH_THREADS
#include <algorithm>
#include <limits>
namespace stackvm
{
/**
 * @brief 根据相对时长计算饱和的单调截止时间。
 * @param nanos 非负相对时长。
 * @return 可表示的截止时间，溢出时饱和到最大时间点。
 */
static std::chrono::steady_clock::time_point eventDeadline(std::chrono::nanoseconds nanos)
{
    const auto now = std::chrono::steady_clock::now();
    const auto limit = std::chrono::steady_clock::time_point::max();
    return nanos >= limit - now ? limit : now + nanos;
}
/**
 * @brief 关闭队列并唤醒事件接收者。
 * @param queue 被关闭的队列。
 */
void closeEventQueue(const std::shared_ptr<EventQueue> &queue)
{
    if (!queue) return;
    std::lock_guard<std::mutex> lock(queue->mutex);
    queue->closed = true;
    queue->timers.clear();
    queue->changed.notify_all();
}
/**
 * @brief 在容量允许时复制宿主事件到目标队列。
 * @param queue 目标弱引用，运行结束后失效。
 * @param event 不含运行时地址的事件值。
 * @return 完成、关闭、队列满或宿主分配失败状态。
 */
TransferStatus postEvent(const std::weak_ptr<EventQueue> &queue, RuntimeEvent event)
{
    const auto target = queue.lock();
    if (!target) return TransferStatus::Closed;
    try
    {
        std::lock_guard<std::mutex> lock(target->mutex);
        if (target->closed) return TransferStatus::Closed;
        if (target->pending.size() >= target->capacity) return TransferStatus::Full;
        target->pending.push_back(event);
        target->changed.notify_all();
        return TransferStatus::Ready;
    } catch (...)
    {
        return TransferStatus::Failed;
    }
}
/**
 * @brief 显式消费事件，到期的周期通知合并而不无限积压。
 * @param queue 被消费的队列。
 * @param event 成功时写入通知。
 * @param millis 毫秒等待预算，负一表示无限。
 * @param cancelled 读取运行停止请求的检查器。
 * @return 操作状态。
 */
TransferStatus receiveEvent(const std::shared_ptr<EventQueue> &queue, RuntimeEvent &event,
                            std::int64_t millis, const Memory::Function<bool()> &cancelled)
{
    auto deadline = std::chrono::steady_clock::time_point::max();
    if (millis >= 0 && millis <= std::numeric_limits<std::int64_t>::max() / 1000000)
        deadline = eventDeadline(std::chrono::nanoseconds(millis * 1000000));
    std::unique_lock<std::mutex> lock(queue->mutex);
    for (;;)
    {
        if (cancelled()) return TransferStatus::Cancelled;
        const auto now = std::chrono::steady_clock::now();
        auto wake = std::min(deadline, now + std::chrono::milliseconds(1));
        for (auto &timer : queue->timers)
        {
            if (!timer.active) continue;
            if (timer.next <= now && queue->pending.size() < queue->capacity)
            {
                const auto missed = (now - timer.next) / timer.period;
                auto found = std::find_if(queue->pending.begin(), queue->pending.end(),
                                          [&](const RuntimeEvent &item) { return item.timer == timer.id; });
                const auto count = static_cast<std::uint64_t>(missed);
                if (found == queue->pending.end())
                    queue->pending.push_back(RuntimeEvent{ timer.id, 0, 0, count });
                else
                    found->missed += count + 1;
                timer.active = timer.repeat;
                timer.next = eventDeadline(timer.period - (now - timer.next) % timer.period);
            }
            if (timer.active) wake = std::min(wake, timer.next);
        }
        if (!queue->pending.empty())
        {
            event = queue->pending.front();
            queue->pending.pop_front();
            return TransferStatus::Ready;
        }
        if (queue->closed) return TransferStatus::Closed;
        if (now >= deadline) return TransferStatus::Timeout;
        queue->changed.wait_until(lock, wake);
    }
}
/**
 * @brief 分配有界定时源槽，避免为每个定时器建立线程。
 * @param queue 定时源归属队列。
 * @param nanos 正纳秒间隔。
 * @param repeat 周期运行标志。
 * @return 正句柄或失败零值。
 */
std::int64_t createEventTimer(const std::shared_ptr<EventQueue> &queue, std::int64_t nanos, bool repeat)
{
    std::lock_guard<std::mutex> lock(queue->mutex);
    if (nanos <= 0 || queue->closed || queue->timers.size() >= 1024) return 0;
    const auto id = queue->nextTimer++;
    const auto period = std::chrono::nanoseconds(nanos);
    queue->timers.push_back(EventTimer{ id, eventDeadline(period), period, repeat, true });
    queue->changed.notify_all();
    return id;
}
/**
 * @brief 删除指定定时源与排队通知，重复删除无副作用。
 * @param queue 所属事件队列。
 * @param id 要删除的定时源。
 */
void destroyEventTimer(const std::shared_ptr<EventQueue> &queue, std::int64_t id)
{
    if (!queue || id <= 0) return;
    std::lock_guard<std::mutex> lock(queue->mutex);
    queue->timers.erase(std::remove_if(queue->timers.begin(), queue->timers.end(),
                                       [&](const EventTimer &timer) { return timer.id == id; }),
                        queue->timers.end());
    queue->pending.erase(std::remove_if(queue->pending.begin(), queue->pending.end(),
                                        [&](const RuntimeEvent &event) { return event.timer == id; }),
                         queue->pending.end());
    queue->changed.notify_all();
}
}  // namespace stackvm
#endif
