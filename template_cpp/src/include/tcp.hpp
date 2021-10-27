#ifndef _TCP_H_
#define _TCP_H_

#include <utility>
#include <vector>

#include "common.hpp"
#include "delivered_set.hpp"
#include "messages.hpp"
#include "ts_queue.hpp"
#include "udp.hpp"

using namespace std::chrono;

typedef SafeQueue<message_t *> MessagesQueue;
typedef SafeQueue<payload_t *> PayloadQueue;

typedef struct {
  int sockfd;
  bool *finito;
  node_t *current_node;
  std::vector<node_t> *nodes;
  DeliveredSet *delivered;
  MessagesQueue *sending_queue;
  MessagesQueue *retrans_queue;
  PayloadQueue *received_queue;
} tcp_handler_t;

void receive_message(tcp_handler_t *tcp_handler);

void keep_receiving_messages(tcp_handler_t *tcp_handler);

void keep_sending_messages_from_queue(tcp_handler_t *tcp_handler);

void keep_retransmitting_messages(tcp_handler_t *tcp_handler);

bool should_start_retransmission(steady_clock::time_point sending_start);

void keep_enqueuing_messages(tcp_handler_t *tcp_handler, node_t *sender_node,
                             node_t *receiver_node, uint32_t *enqueued_messages,
                             uint32_t msgs_to_send_count);

void construct_message(message_t *message, node_t *sender, uint32_t seq_num);

#endif
