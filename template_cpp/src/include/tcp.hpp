#ifndef _TCP_H_
#define _TCP_H_

#include <atomic>
#include <utility>
#include <vector>

#include "common.hpp"
#include "delivered_set.hpp"
#include "messages.hpp"
#include "udp.hpp"

#define MAX_PACKET_WAIT_MS 100
#define SENDING_CHUNK_SIZE (MILLION / 10)
#define RETRANSMISSION_OFFSET_MS 300

using namespace std::chrono;

typedef struct tcp_handler_s {
  int sockfd;
  std::atomic<bool> *finito;
  node_t *current_node;
  std::vector<node_t *> *nodes;
  DeliveredSet *delivered;
  MessagesQueue *sending_queue;
  MessagesQueue *retrans_queue;
  PayloadQueue *broadcasted_queue;
} tcp_handler_t;

void keep_receiving_messages(tcp_handler_t *tcp_handler);

void keep_sending_messages_from_queue(tcp_handler_t *tcp_handler);

void keep_retransmitting_messages(tcp_handler_t *tcp_handler);

bool should_start_retransmission(steady_clock::time_point sending_start);

void construct_message(message_t *message, payload_t *payload,
                       node_t *recipient);

void construct_payload(tcp_handler_t *h, payload_t *payload, node_t *sender,
                       uint32_t seq_num);

inline uint32_t causal_links_count(struct tcp_handler_s *h, uint32_t node_id) {
  return static_cast<uint32_t>(h->delivered->causality[node_id].size());
}

inline uint32_t my_causal_links_count(struct tcp_handler_s *h) {
  return causal_links_count(h, h->current_node->id);
}

inline uint32_t vector_clock_size(tcp_handler_t *h) {
  return static_cast<uint32_t>(h->nodes->size() + 1);
}

inline void show_vector_clock(uint32_t *vector_clock, uint32_t vc_size) {
  std::cout << "Vector clock: ";
  for (uint32_t i = 0; i < vc_size; i++) {
    std::cout << vector_clock[i] << " ";
  }
  std::cout << "\n";
}

#endif
