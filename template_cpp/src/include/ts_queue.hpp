#ifndef SAFE_QUEUE
#define SAFE_QUEUE

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

template <class T> class SafeQueue {
private:
  std::queue<T> q;
  mutable std::mutex mtx;
  std::condition_variable cond_var;
  std::atomic<uint32_t> q_size = 0;

public:
  SafeQueue() : q(), mtx(), cond_var() {}

  ~SafeQueue() {}

  uint32_t size() { return q_size; }

  void enqueue(T t) {
    std::lock_guard<std::mutex> lock(mtx);
    q.push(t);
    q_size++;
    cond_var.notify_one();
  }

  T dequeue() {
    std::unique_lock<std::mutex> lock(mtx);

    while (q.empty()) {
      cond_var.wait(lock);
    }

    T value = q.front();
    q.pop();
    q_size--;
    return value;
  }
};

#endif
