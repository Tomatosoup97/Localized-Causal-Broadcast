#ifndef DELIVERED_SET
#define DELIVERED_SET

#include "common.hpp"
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

class DeliveredSet {

private:
  // :: Sender Process -> ReceivedMessages
  std::unordered_map<uint32_t, std::unordered_set<uint32_t> *> s;

  // :: Message -> # Acked Processes
  std::unordered_map<uint32_t, std::atomic<uint32_t>> acked_counter;

  mutable std::mutex mtx;
  uint32_t keys;
  node_t *current_node;

  // points where we have the first "hole" in delivered
  std::unordered_map<uint32_t, std::atomic<uint32_t>> received_up_to;

  bool contains_unsafe(uint32_t node_id, uint32_t packet_id) {
    return s[node_id]->count(packet_id) == 1;
  }

public:
  DeliveredSet(node_t *current_node_in, size_t keys_in) : s(), mtx() {
    keys = static_cast<uint32_t>(keys_in);
    current_node = current_node_in;

    for (uint32_t i = 0; i <= keys; i++) {
      // initialize delivered sets
      std::unordered_set<uint32_t> *messages = new std::unordered_set<uint32_t>;
      s[i] = messages;

      // initialize all 'received up to' map
      received_up_to[i] = 1;
    }
  }

  ~DeliveredSet() {
    for (uint32_t i = 0; i <= keys; i++) {
      delete s[i];
    }
  }

  void insert(uint32_t node_id, uint32_t packet_id) {
    std::lock_guard<std::mutex> lock(mtx);

    if (!contains_unsafe(node_id, packet_id)) {
      acked_counter[packet_id]++;
      s[node_id]->insert(packet_id);
    }

    if (received_up_to[node_id] == packet_id) {
      while (contains_unsafe(node_id, received_up_to[node_id])) {
        s[node_id]->erase(received_up_to[node_id]);
        received_up_to[node_id]++;
      }
    }
  }

  bool contains(uint32_t node_id, uint32_t packet_id) {
    if (packet_id < received_up_to[node_id]) {
      return true;
    }
    std::lock_guard<std::mutex> lock(mtx);
    return s[node_id]->count(packet_id) == 1;
  }

  bool can_urb_deliver(uint32_t packet_id) {
    return acked_counter[packet_id] > (keys / 2);
  }

  void mark_as_seen(uint32_t packet_id) { insert(current_node->id, packet_id); }

  bool was_seen(uint32_t packet_id) {
    return contains(current_node->id, packet_id);
  }

  std::unordered_set<uint32_t> *get_set(uint32_t node_id) { return s[node_id]; }
};

#endif
