#ifndef DELIVERED_SET
#define DELIVERED_SET

#include <mutex>
#include <unordered_set>

class DeliveredSet {

private:
  // TODO: we need <packet_id, sender_id> pair
  std::unordered_set<uint32_t> s;
  mutable std::mutex mtx;

public:
  DeliveredSet() : s(), mtx() {}

  ~DeliveredSet() {}

  void insert(uint32_t v) {
    std::lock_guard<std::mutex> lock(mtx);
    s.insert(v);
  }

  void remove(uint32_t v) {
    std::unique_lock<std::mutex> lock(mtx);
    s.erase(v);
  }

  bool contains(uint32_t v) {
    std::unique_lock<std::mutex> lock(mtx);
    return s.count(v) == 1;
  }

  std::unordered_set<uint32_t> get_set() { return s; }
};

#endif
