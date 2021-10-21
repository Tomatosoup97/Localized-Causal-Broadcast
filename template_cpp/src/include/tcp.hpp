#ifndef _TCP_H_
#define _TCP_H_

#include "common.hpp"
#include "delivered_set.hpp"
#include "ts_queue.hpp"
#include "udp.hpp"
#include <utility>

using namespace std::chrono;

typedef struct {
  payload_t *payload;
  node_t *recipient;
  steady_clock::time_point sending_time;
  bool is_ack = false;
} message_t;

typedef SafeQueue<message_t *> MessagesQueue;
typedef SafeQueue<payload_t *> PayloadQueue;

typedef struct {
  int sockfd;
  bool is_receiver;
  bool *finito;
  DeliveredSet *delivered;
  MessagesQueue *sending_queue;
  MessagesQueue *retrans_queue;
  PayloadQueue *received_queue;
} tcp_handler_t;

void receive_message(tcp_handler_t *tcp_handler, bool is_receiver,
                     std::vector<node_t> &nodes);

void keep_receiving_messages(tcp_handler_t *tcp_handler, bool is_receiver,
                             std::vector<node_t> &nodes);

void keep_sending_messages_from_queue(tcp_handler_t *tcp_handler,
                                      std::vector<node_t> &nodes);

void keep_retransmitting_messages(tcp_handler_t *tcp_handler);

void keep_enqueuing_messages(tcp_handler_t *tcp_handler, node_t *sender_node,
                             node_t *receiver_node, uint32_t *enqueued_messages,
                             uint32_t msgs_to_send_count);

bool should_start_retransmission(steady_clock::time_point sending_start);

#endif
