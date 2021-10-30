#ifndef DELIVERED_SET
#define DELIVERED_SET

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "common.hpp"
#include "messages.hpp"

typedef std::atomic<uint32_t> Counter;
typedef uint32_t SenderID;
typedef uint32_t OwnerID;
typedef uint32_t PacketID;

template <typename A, typename B> using Map = std::unordered_map<A, B>;
template <typename A> using Set = std::unordered_set<A>;

class DeliveredSet {

private:
  Map<SenderID, Map<OwnerID, Set<PacketID> *>> acked;

  Map<OwnerID, Map<PacketID, Counter>> acked_counter;

  mutable std::mutex mtx;
  uint32_t keys;
  node_t *current_node;

  // points where we have the first "hole" in delivered
  Map<OwnerID, Counter> received_up_to;

  bool contains_unsafe(SenderID sender_id, payload_t *payload) {
    return acked[sender_id][payload->owner_id]->count(payload->packet_uid) == 1;
  }

public:
  PayloadQueue *urb_deliverable;

  DeliveredSet(node_t *current_node_in, size_t keys_in)
      : acked(), acked_counter(), mtx() {
    keys = static_cast<uint32_t>(keys_in);
    current_node = current_node_in;

    for (uint32_t sender_id = 0; sender_id <= keys; sender_id++) {
      for (uint32_t owner_id = 0; owner_id <= keys; owner_id++) {
        // initialize delivered sets
        Set<PacketID> *packets = new Set<PacketID>;
        acked[sender_id][owner_id] = packets;
      }
      received_up_to[sender_id] = 1;
    }
  }

  ~DeliveredSet() {
    for (uint32_t sender_id = 0; sender_id <= keys; sender_id++) {
      for (uint32_t owner_id = 0; owner_id <= keys; owner_id++) {
        delete acked[sender_id][owner_id];
      }
    }
  }

  void insert(SenderID sender_id, payload_t *payload) {
    payload_t *log_payload;
    mtx.lock();
    bool seen = contains_unsafe(sender_id, payload);

    if (!seen) {
      acked[sender_id][payload->owner_id]->insert(payload->packet_uid);
    }
    mtx.unlock();

    if (!seen) {
      acked_counter[payload->owner_id][payload->packet_uid]++;

      if (can_urb_deliver(payload)) {

        log_payload = new payload_t;
        copy_payload(log_payload, payload);
        urb_deliverable->enqueue(log_payload);
      }
    }

    // TODO: rethink this optimalization later
    /*
    if (received_up_to[payload->owner_id] == payload->packet_uid) {
      while (contains_unsafe(sender_id, received_up_to[sender_id])) {
        s[sender_id]->erase(received_up_to[sender_id]);
        received_up_to[payload->owner_id]++;
      }
    }
    */
  }

  bool contains(SenderID sender_id, payload_t *payload) {
    // TODO: rethink this optimalization later
    /*
    if (payload->packet_uid < received_up_to[sender_id]) {
      return true;
    }
    */
    std::lock_guard<std::mutex> lock(mtx);
    return contains_unsafe(sender_id, payload);
  }

  bool can_urb_deliver(payload_t *payload) {
    uint32_t current_counter =
        acked_counter[payload->owner_id][payload->packet_uid];
    return (keys / 2) + 1 >= current_counter && current_counter > (keys / 2);
  }

  void mark_as_seen(payload_t *payload) { insert(current_node->id, payload); }

  bool was_seen(payload_t *payload) {
    return contains(current_node->id, payload);
  }
};

#endif
