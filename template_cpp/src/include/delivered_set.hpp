#ifndef DELIVERED_SET
#define DELIVERED_SET

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

class DeliveredSet {

private:
  // TODO: we need <packet_id, sender_id> pair
  std::unordered_map<uint32_t, std::unordered_set<uint32_t>> s;
  mutable std::mutex mtx;

public:
  DeliveredSet(size_t keys) : s(), mtx() {
    for (size_t i = 0; i < keys; i++) {
      std::unordered_set<uint32_t> *messages = new std::unordered_set<uint32_t>;
      s[static_cast<uint32_t>(i)] = *messages;
    }
  }

  ~DeliveredSet() {}

  void insert(uint32_t node_id, uint32_t packet_id) {
    std::lock_guard<std::mutex> lock(mtx);

    s[node_id].insert(packet_id);
  }

  void remove(uint32_t node_id, uint32_t packet_id) {
    std::unique_lock<std::mutex> lock(mtx);
    s[node_id].erase(packet_id);
  }

  bool contains(uint32_t node_id, uint32_t packet_id) {
    std::unique_lock<std::mutex> lock(mtx);
    return s[node_id].count(packet_id) == 1;
  }

  std::unordered_set<uint32_t> get_set(uint32_t node_id) { return s[node_id]; }
};

#endif
