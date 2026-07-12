#ifndef DOMI_THREAD_POOL_H
#define DOMI_THREAD_POOL_H

#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstddef>

namespace domi {

// A simple fixed-size worker thread pool for C++11.
// Submit callable tasks and optionally wait for all pending work to finish.
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();

    // Submit a task to be executed by a worker thread.
    void submit(std::function<void()> task);

    // Wait until all submitted tasks have finished executing.
    void wait();

    // Number of worker threads.
    size_t size() const { return workers_.size(); }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable finishedCv_;

    bool stop_;
    size_t activeTasks_;

    ThreadPool(const ThreadPool&);
    ThreadPool& operator=(const ThreadPool&);
};

} // namespace domi

#endif
