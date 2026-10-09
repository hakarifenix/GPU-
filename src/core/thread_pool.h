// NOVA GPU - Software GPU
// src/core/thread_pool.h - persistent worker pool + fork/join parallel loops.
//
// One pool is created per Device (per frame the tiles are handed out from a
// single atomic counter, which keeps the per-frame synchronisation cost flat
// instead of spawning threads for every tile batch).
#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace nova::core {

// Minimal counting latch used to join a parallel loop.
class Latch {
public:
    explicit Latch(int count) : count_(count) {}

    void arrive() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (--count_ == 0) ready_.notify_all();
    }

    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        ready_.wait(lock, [this] { return count_ == 0; });
    }

private:
    std::mutex mutex_;
    std::condition_variable ready_;
    int count_;
};

class ThreadPool {
public:
    // `threads` is the TOTAL number of threads that should take part in a
    // parallel loop, including the calling thread. The pool therefore spawns
    // threads - 1 workers, and the caller runs one of the chunks itself.
    // threads == 0 means "one thread per hardware thread" (at least 1).
    // Nothing is spawned when threads <= 1.
    explicit ThreadPool(unsigned threads = 0);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Number of worker threads in the pool. 0 means "run everything inline".
    unsigned size() const { return workerCount_; }

    // Calls fn(begin, end) for consecutive chunks covering [0, count).
    // `grain` is the maximum number of elements per chunk. Blocks until done.
    // `fn` must be safe to call concurrently, and is captured by reference, so
    // it must stay alive for the duration of the call (it does, we block).
    template <typename F>
    void parallelFor(uint64_t count, uint64_t grain, F&& fn) {
        if (count == 0) return;
        if (grain == 0) grain = 1;

        const uint64_t chunkCount = (count + grain - 1) / grain;
        if (workerCount_ == 0 || chunkCount == 1) {
            for (uint64_t c = 0; c < chunkCount; ++c) {
                const uint64_t begin = c * grain;
                fn(begin, std::min(count, begin + grain));
            }
            return;
        }

        auto next = std::make_shared<std::atomic<uint64_t>>(0);
        auto body = [next, &fn, grain, count, chunkCount]() {
            uint64_t c = 0;
            while ((c = next->fetch_add(1, std::memory_order_relaxed)) < chunkCount) {
                const uint64_t begin = c * grain;
                fn(begin, std::min(count, begin + grain));
            }
        };

        // Leave one chunk stream for the calling thread so the main thread is
        // never idle while the workers finish the frame.
        const uint64_t helperJobs = std::min<uint64_t>(workerCount_, chunkCount - 1);
        auto latch = std::make_shared<Latch>(static_cast<int>(helperJobs));
        for (uint64_t j = 0; j < helperJobs; ++j) {
            enqueue([body, latch]() {
                body();
                latch->arrive();
            });
        }
        body();
        latch->wait();
    }

private:
    void enqueue(std::function<void()> job);
    void workerLoop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> jobs_;
    std::mutex mutex_;
    std::condition_variable workAvailable_;
    unsigned workerCount_ = 0;
    bool stopping_ = false;
};

}  // namespace nova::core
