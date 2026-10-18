#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <memory>

/**
 * @brief 一个提示类别的有界工作队列；协议线程只负责提交和取消。
 */
class LspQueue
{
    struct Work
    {
        std::string key;
        std::function<void()> run;
        std::shared_ptr<std::atomic<bool>> cancelled;
        std::chrono::steady_clock::time_point due;
    };
    std::mutex mutex_;
    std::condition_variable changed_, idle_;
    std::deque<Work> work_;
    bool stopping_ = false, active_ = false;
    std::thread thread_;
    void loop()
    {
        for (;;)
        {
            Work item;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                for (;;)
                {
                    if (stopping_ && work_.empty()) return;
                    if (work_.empty())
                    {
                        changed_.wait(lock);
                        continue;
                    }
                    auto next = std::min_element(work_.begin(), work_.end(), [](const Work &a, const Work &b) { return a.due < b.due; });
                    if (!stopping_ && next->due > std::chrono::steady_clock::now())
                    {
                        const auto due = next->due;
                        changed_.wait_until(lock, due);
                        continue;
                    }
                    item = std::move(*next);
                    work_.erase(next);
                    active_ = true;
                    break;
                }
            }
            try
            {
                item.run();
            } catch (...) { /* 单次任务异常由请求入口报告；线程继续服务。 */ }
            {
                std::lock_guard<std::mutex> lock(mutex_);
                active_ = false;
                if (work_.empty()) idle_.notify_all();
            }
        }
    }

public:
    /**
     * @brief 启动一个独立提示线程。
     */
    LspQueue() : thread_([this] { loop(); }) {}
    /**
     * @brief 等待队列任务结束并释放线程。
     */
    ~LspQueue() { stop(); }
    /**
     * @brief 提交最新任务，相同键替换旧任务；队列最多保留 128 项。
     * @param key 合并键。
     * @param run 执行函数。
     * @param cancelled 取消标记。
     * @param delay 防抖时间。
     */
    void post(std::string key, std::function<void()> run, std::shared_ptr<std::atomic<bool>> cancelled, int delay = 0)
    {
        std::vector<Work> removed;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto it = work_.begin(); it != work_.end();)
            {
                if (!key.empty() && it->key == key)
                {
                    it->cancelled->store(true);
                    removed.push_back(std::move(*it));
                    it = work_.erase(it);
                }
                else
                    ++it;
            }
            if (work_.size() >= 128)
            {
                work_.front().cancelled->store(true);
                removed.push_back(std::move(work_.front()));
                work_.pop_front();
            }
            work_.push_back({ std::move(key), std::move(run), std::move(cancelled), std::chrono::steady_clock::now() + std::chrono::milliseconds(delay) });
        }
        for (auto &item : removed) item.run();
        changed_.notify_one();
    }
    /**
     * @brief 等待已提交的请求全部完成，用于 JSON-RPC shutdown 屏障。
     */
    void drain()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        idle_.wait(lock, [&] { return !active_ && work_.empty(); });
    }
    /**
     * @brief 移除尚未开始的取消请求，立即释放其文档快照并回送取消响应。
     * @param flag 请求的取消标记。
     */
    void cancel(const std::shared_ptr<std::atomic<bool>> &flag)
    {
        std::vector<Work> removed;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto it = work_.begin(); it != work_.end();)
            {
                if (it->cancelled == flag)
                {
                    removed.push_back(std::move(*it));
                    it = work_.erase(it);
                }
                else
                    ++it;
            }
            if (!active_ && work_.empty()) idle_.notify_all();
        }
        for (auto &item : removed) item.run();
        changed_.notify_one();
    }
    /**
     * @brief 停止调度并等待工作线程。
     */
    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        changed_.notify_all();
        if (thread_.joinable()) thread_.join();
    }
};
