#include "domi/thread_pool.h"

namespace domi {

ThreadPool::ThreadPool(size_t numThreads)
    : stop_(false), activeTasks_(0)
{
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this]() {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });
                    if (stop_ && tasks_.empty()) {
                        return;
                    }
                    task = std::move(tasks_.front());
                    tasks_.pop();
                    ++activeTasks_;
                }

                task();

                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    --activeTasks_;
                    finishedCv_.notify_all();
                }
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (size_t i = 0; i < workers_.size(); ++i) {
        if (workers_[i].joinable()) {
            workers_[i].join();
        }
    }
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stop_) return;
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    finishedCv_.wait(lock, [this]() { return tasks_.empty() && activeTasks_ == 0; });
}

} // namespace domi
