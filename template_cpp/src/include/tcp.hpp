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

typedef struct {
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

void keep_enqueuing_messages(tcp_handler_t *tcp_handler, node_t *sender_node,
                             node_t *receiver_node, uint32_t *enqueued_messages,
                             uint32_t msgs_to_send_count);

void construct_message(message_t *message, payload_t *payload,
                       node_t *recipient);

void construct_payload(payload_t *payload, node_t *sender, uint32_t seq_num);

#endif
