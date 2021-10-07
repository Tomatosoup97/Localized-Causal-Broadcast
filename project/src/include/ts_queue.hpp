#ifndef SAFE_QUEUE
#define SAFE_QUEUE

#include <condition_variable>
#include <mutex>
#include <queue>

// Inspired by:
// https://gist.github.com/murphypei/d59cbcdf6c8485ed98510dc1f0b3ddca#file-safe_queue-h-L6

template <class T> class SafeQueue {

private:
  std::queue<T> q;
  mutable std::mutex mtx;
  std::condition_variable cond_var;

public:
  SafeQueue() : q(), mtx(), cond_var() {}

  ~SafeQueue() {}

  void enqueue(T t) {
    std::lock_guard<std::mutex> lock(mtx);
    q.push(t);
    cond_var.notify_one();
  }

  T dequeue() {
    std::unique_lock<std::mutex> lock(mtx);

    while (q.empty()) {
      cond_var.wait(lock);
    }

    T value = q.front();
    q.pop();
    return value;
  }
};

#endif
