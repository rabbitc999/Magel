#ifndef CHALI_EVENT_SERVICE_HPP
#define CHALI_EVENT_SERVICE_HPP
#include "stackvm.hpp"
#include <atomic>
#if STACKVM_WITH_THREADS
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <vector>
#endif
namespace stackvm
{
/**
 * @brief 线程安全的协作停止标志，独立于用户回调与调试钩子。
 */
struct ExecutionControl
{ std::atomic<bool> requested{ false }; };
}  // namespace stackvm
#if STACKVM_WITH_THREADS
namespace stackvm
{
/**
 * @brief 单个接收队列的定时源，仅由共享事件服务计算截止时间，不创建宿主线程。
 */
struct EventTimer
{
    std::int64_t id = 0;
    std::chrono::steady_clock::time_point next;
    std::chrono::nanoseconds period{ 0 };
    bool repeat = false;
    bool active = true;
};
/**
 * @brief 宿主持有的有界事件队列，独立于 VM 内存池，投递器通过弱引用访问。
 */
struct EventQueue
{
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<RuntimeEvent> pending;
    std::vector<EventTimer> timers;
    std::size_t capacity = 256;
    std::int64_t nextTimer = 1;
    bool closed = false;
};
/**
 * @brief 将队列关闭并停止所有定时源，已排队事件仍可排空。
 * @param queue 需要关闭的队列。
 */
void closeEventQueue(const std::shared_ptr<EventQueue> &queue);
/**
 * @brief 从任意宿主线程投递事件，队列满时立即返回。
 * @param queue 目标队列的弱引用。
 * @param event 按值复制的事件。
 * @return 接收、关闭、队列满或宿主分配失败状态。
 */
TransferStatus postEvent(const std::weak_ptr<EventQueue> &queue, RuntimeEvent event);
/**
 * @brief 显式接收事件并计算到期定时源，周期积压合并为 missed。
 * @param queue 当前运行队列。
 * @param event 接收成功时写入事件。
 * @param millis 等待毫秒数，负一表示无限，零表示立即尝试。
 * @param cancelled 无锁读取停止状态的检查器。
 * @return 接收、超时、关闭或取消状态。
 */
TransferStatus receiveEvent(const std::shared_ptr<EventQueue> &queue, RuntimeEvent &event,
                            std::int64_t millis, const Memory::Function<bool()> &cancelled);
/**
 * @brief 登记一次性或周期定时源，立即开始计时。
 * @param queue 接收事件的队列。
 * @param nanos 正纳秒周期。
 * @param repeat 是否在到期后继续运行。
 * @return 成功返回正句柄，参数无效、关闭或资源预算耗尽返回零。
 */
std::int64_t createEventTimer(const std::shared_ptr<EventQueue> &queue, std::int64_t nanos, bool repeat);
/**
 * @brief 停止并删除定时源及其尚未接收的通知。
 * @param queue 所属队列。
 * @param id 定时源句柄，重复删除安全。
 */
void destroyEventTimer(const std::shared_ptr<EventQueue> &queue, std::int64_t id);
}  // namespace stackvm
#endif
#endif
