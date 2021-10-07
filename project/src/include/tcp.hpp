#ifndef _TCP_H_
#define _TCP_H_

#include "common.hpp"
#include "udp.hpp"

void send_messages(int sockfd, uint64_t msgs_to_send_count, bool *delivered,
                   node_t *receiver_node, node_t *sender_node,
                   uint64_t *first_undelivered);

void receive_message(int sockfd, bool *delivered, bool is_receiver,
                     std::vector<node_t> &nodes);

void keep_receiving_messages(int sockfd, bool *delivered, bool is_receiver,
                             std::vector<node_t> &nodes, bool *finito);

#endif
