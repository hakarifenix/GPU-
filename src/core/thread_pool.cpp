#include "core/thread_pool.h"

#include <algorithm>

namespace nova::core {

ThreadPool::ThreadPool(unsigned threads) {
    if (threads == 0) {
        threads = std::thread::hardware_concurrency();
    }
    if (threads <= 1) {
        // Nothing to parallelise: run every loop inline on the calling thread.
        workerCount_ = 0;
        return;
    }
    // `threads` counts the caller as well, so spawn one fewer worker. Spawning
    // `threads` workers would oversubscribe the machine by one thread and can
    // make a parallel loop slower than the serial version.
    workerCount_ = threads - 1;
    workers_.reserve(workerCount_);
    for (unsigned i = 0; i < workerCount_; ++i) {
        workers_.emplace_back([this] { workerLoop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    workAvailable_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
}

void ThreadPool::enqueue(std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.push(std::move(job));
    }
    workAvailable_.notify_one();
}

void ThreadPool::workerLoop() {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            workAvailable_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });
            if (stopping_ && jobs_.empty()) return;
            job = std::move(jobs_.front());
            jobs_.pop();
        }
        job();
    }
}

}  // namespace nova::core
