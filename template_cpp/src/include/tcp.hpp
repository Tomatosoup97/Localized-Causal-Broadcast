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
  bool first_send = false;
} message_t;

typedef SafeQueue<message_t *> MessagesQueue;
typedef SafeQueue<payload_t *> PayloadQueue;

typedef struct {
  int sockfd;
  bool is_receiver;
  bool *finito;
  node_t *current_node;
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

void construct_message(message_t *message, node_t *sender, node_t *recipient,
                       uint32_t seq_num);

void copy_payload(payload_t *dest, payload_t *source);

void free_message(message_t *message);

bool should_start_retransmission(steady_clock::time_point sending_start);

uint32_t contract_pair(uint32_t k1, uint32_t k2);

std::pair<uint32_t, uint32_t> unfold_pair(uint32_t p);

#endif
