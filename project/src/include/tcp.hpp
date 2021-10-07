#ifndef _TCP_H_
#define _TCP_H_

#include "common.hpp"
#include "ts_queue.hpp"
#include "udp.hpp"

void send_messages(int sockfd, uint32_t msgs_to_send_count, bool *delivered,
                   node_t *receiver_node, node_t *sender_node,
                   uint32_t *first_undelivered);

void receive_message(int sockfd, bool *delivered, bool is_receiver,
                     std::vector<node_t> &nodes);

void keep_receiving_messages(int sockfd, bool *delivered, bool is_receiver,
                             std::vector<node_t> &nodes, bool *finito);

void keep_sending_messages_from_queue(int sockfd,
                                      SafeQueue<payload_t *> &messages_queue,
                                      std::vector<node_t> &nodes, bool *finito);

bool should_start_retransmission(
    std::chrono::steady_clock::time_point sending_start);

#endif
