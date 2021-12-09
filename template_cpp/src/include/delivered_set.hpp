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

typedef Map<SenderID, std::vector<uint32_t>> CausalityMap;

class DeliveredSet {

private:
  Map<SenderID, Map<OwnerID, Set<PacketID> *>> acked;

  Map<OwnerID, Map<PacketID, Counter>> acked_counter;
  Map<OwnerID, Map<PacketID, payload_t *>> undelivered;

  mutable std::mutex mtx;
  mutable std::mutex received_mtx;
  uint32_t keys;
  node_t *current_node;

  // points where we have the first "hole" in delivered
  Map<OwnerID, Counter> received_up_to;

  bool contains_unsafe(SenderID sender_id, payload_t *payload) {
    return acked[sender_id][payload->owner_id]->count(payload->packet_uid) == 1;
  }

public:
  PayloadQueue *deliverable;
  uint32_t *vector_clock;
  CausalityMap *causality;
  CausalityMap *reverse_causality;

  DeliveredSet(node_t *current_node_in, size_t keys_in)
      : acked(), acked_counter(), undelivered(), mtx(), received_mtx() {
    keys = static_cast<uint32_t>(keys_in);
    current_node = current_node_in;
    vector_clock = new uint32_t[keys + 1];

    for (uint32_t sender_id = 0; sender_id <= keys; sender_id++) {
      for (uint32_t owner_id = 0; owner_id <= keys; owner_id++) {
        // initialize delivered sets
        Set<PacketID> *packets = new Set<PacketID>;
        acked[sender_id][owner_id] = packets;
      }
      received_up_to[sender_id] = 1;
      vector_clock[sender_id] = 0;
    }
  }

  ~DeliveredSet() {
    for (uint32_t sender_id = 0; sender_id <= keys; sender_id++) {
      for (uint32_t owner_id = 0; owner_id <= keys; owner_id++) {
        delete acked[sender_id][owner_id];
      }
    }
    delete[] vector_clock;
  }

  void insert(SenderID sender_id, payload_t *payload) {
    std::lock_guard<std::mutex> lock(mtx);
    payload_t *log_payload;
    bool seen = contains_unsafe(sender_id, payload);

    if (!seen) {
      acked[sender_id][payload->owner_id]->insert(payload->packet_uid);

      if (payload->packet_uid < received_up_to[payload->owner_id]) {
        return;
      } else {

        acked_counter[payload->owner_id][payload->packet_uid]++;

        if (acked_counter[payload->owner_id][payload->packet_uid] == 1) {
          log_payload = new payload_t;
          copy_payload(log_payload, payload, keys);
          undelivered[payload->owner_id][payload->packet_uid] = log_payload;
        }
      }
    }

    if (received_up_to[payload->owner_id] == payload->packet_uid) {
      for (uint32_t affected_node_id : (*causality)[payload->owner_id]) {
        while (can_lcb_deliver(payload->vector_clock,
                               affected_node_id)) {
          uint32_t packet_uid = received_up_to[affected_node_id];

          log_payload = undelivered[affected_node_id][packet_uid];
          deliverable->enqueue(log_payload);
          undelivered[affected_node_id].erase(packet_uid);
          acked_counter[affected_node_id].erase(packet_uid);

          received_up_to[affected_node_id]++;
          vector_clock[affected_node_id]++;
        }
      }
    }
  }

  bool contains(SenderID sender_id, payload_t *payload) {
    std::lock_guard<std::mutex> lock(mtx);
    return contains_unsafe(sender_id, payload);
  }

  bool can_lcb_deliver(uint32_t *recv_vector_clock, uint32_t node_id) {
    bool lcb_happy = true;
    for (uint32_t dependency : (*causality)[node_id]) {
      // TODO: VC on left side
      if (vector_clock[dependency] < recv_vector_clock[dependency]) {
        lcb_happy = false;
        break;
      }
    }
    bool urb_happy = can_urb_deliver(node_id, received_up_to[node_id]);
    return lcb_happy && urb_happy;
  }

  bool can_urb_deliver(uint32_t owner_id, uint32_t packet_uid) {
    if (packet_uid < received_up_to[owner_id]) {
      return true;
    }
    return acked_counter[owner_id][packet_uid] > (keys / 2);
  }

  void mark_as_seen(payload_t *payload) { insert(current_node->id, payload); }

  bool was_seen(payload_t *payload) {
    return contains(current_node->id, payload);
  }
};

#endif
