#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <type_traits>

namespace concurrency {

class ThreadPool {
 public:
  static ThreadPool &instance() {
    static ThreadPool pool;
    return pool;
  }

  uint32_t threadCount() const { return numThreads_; }

  // Submit work and wait for completion. Each task is a range [start, end).
  void parallelFor(uint32_t numIterations, const std::function<void(int, int)> &work) {
    if (numIterations == 0) return;

    uint32_t workers = std::min(numThreads_, numIterations);
    if (workers <= 1) {
      work(0, static_cast<int>(numIterations));
      return;
    }

    uint32_t segmentSize = numIterations / workers;
    uint32_t remainder = numIterations % workers;

    std::atomic<uint32_t> completedCount{0};
    std::mutex doneMutex;
    std::condition_variable doneCv;

    // Distribute tasks to pool threads (workers - 1 of them)
    {
      std::lock_guard<std::mutex> lock(mutex_);
      uint32_t offset = segmentSize + (remainder > 0 ? 1 : 0); // first chunk for caller
      for (uint32_t i = 1; i < workers; ++i) {
        uint32_t start = offset;
        uint32_t extra = (i < remainder) ? 1 : 0;
        uint32_t end = start + segmentSize + extra;
        offset = end;

        tasks_.push_back([&work, &completedCount, &doneMutex, &doneCv, start, end, workers]() {
          work(static_cast<int>(start), static_cast<int>(end));
          uint32_t prev = completedCount.fetch_add(1, std::memory_order_release);
          if (prev + 1 == workers - 1) {
            std::lock_guard<std::mutex> lk(doneMutex);
            doneCv.notify_one();
          }
        });
      }
    }
    taskCv_.notify_all();

    // Caller runs first chunk
    uint32_t firstEnd = segmentSize + (remainder > 0 ? 1 : 0);
    work(0, static_cast<int>(firstEnd));

    // Wait for pool threads to finish
    std::unique_lock<std::mutex> lk(doneMutex);
    doneCv.wait(lk, [&]() {
      return completedCount.load(std::memory_order_acquire) >= workers - 1;
    });
  }

 private:
  ThreadPool() {
    unsigned hw = std::thread::hardware_concurrency();
    numThreads_ = (hw > 0) ? std::min(hw, 8u) : 4u;
    numThreads_ = std::max(numThreads_, 1u);

    shutdown_ = false;
    for (uint32_t i = 0; i < numThreads_ - 1; ++i) {
      threads_.emplace_back([this]() { workerLoop(); });
    }
  }

  ~ThreadPool() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      shutdown_ = true;
    }
    taskCv_.notify_all();
    for (auto &t : threads_) {
      if (t.joinable()) t.join();
    }
  }

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  void workerLoop() {
    for (;;) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        taskCv_.wait(lock, [this]() { return shutdown_ || !tasks_.empty(); });
        if (shutdown_ && tasks_.empty()) return;
        task = std::move(tasks_.back());
        tasks_.pop_back();
      }
      task();
    }
  }

  uint32_t numThreads_;
  std::vector<std::thread> threads_;
  std::vector<std::function<void()>> tasks_;
  std::mutex mutex_;
  std::condition_variable taskCv_;
  bool shutdown_;
};

template<typename Function, typename... Args>
void parallel_for(const uint32_t numThreads, const uint32_t numIterations, Function &&func, Args &&... args) {
  static_assert(std::is_invocable_v<Function, int, Args...>, "func must take an int parameter for iteration id");

  (void) numThreads; // ignored, pool uses hardware_concurrency

  auto work = [&](int start, int end) {
    for (int y = start; y < end; ++y) {
      std::invoke(func, y, args...);
    }
  };

  ThreadPool::instance().parallelFor(numIterations, work);
}

template<typename Function, typename... Args>
void parallel_for_with_thread_id(const int numThreads, const int numIterations, Function &&func, Args &&... args) {
  static_assert(std::is_invocable_v<Function, int, int, Args...>, "func must take an int parameter for threadId, and iteration Id");

  (void) numThreads;

  auto work = [&](int start, int end) {
    // Thread ID is not meaningful with a pool; pass 0 for compatibility
    for (int y = start; y < end; ++y) {
      std::invoke(func, 0, y, args...);
    }
  };

  ThreadPool::instance().parallelFor(static_cast<uint32_t>(numIterations), work);
}

}
