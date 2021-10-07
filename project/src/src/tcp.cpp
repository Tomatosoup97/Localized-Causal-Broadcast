#include <iostream>
#include <sys/types.h>
#include <thread>

#include "common.hpp"
#include "tcp.hpp"
#include "udp.hpp"

#define MAX_PACKET_WAIT_MS 100
#define SENDING_CHUNK_SIZE 100000

void send_messages(int sockfd, uint32_t msgs_to_send_count, bool *delivered,
                   node_t *receiver_node, node_t *sender_node,
                   uint32_t *first_undelivered) {
  payload_t payload;
  bool was_sent;
  bool all_delivered_so_far = true;
  int sent_messages = 0;

  for (uint32_t i = *first_undelivered; i < msgs_to_send_count; i++) {
    if (delivered[i])
      continue;
    else if (all_delivered_so_far) {
      all_delivered_so_far = false;
      *first_undelivered = i;
    } else if (sent_messages > SENDING_CHUNK_SIZE) {
      break;
    }

    sent_messages++;

    payload = {i, i, sender_node->id};
    was_sent = send_udp_payload(sockfd, receiver_node, &payload);
  }
  if (all_delivered_so_far)
    *first_undelivered = msgs_to_send_count - 1;
}

void receive_message(int sockfd, bool *delivered, bool is_receiver,
                     std::vector<node_t> &nodes) {
  int is_socket_ready;
  bool was_sent;
  payload_t payload;

  while ((is_socket_ready = select_socket(sockfd, 0, MAX_PACKET_WAIT_MS))) {
    receive_udp_payload(sockfd, &payload);
    node_t sender_node = nodes[get_node_idx_by_id(nodes, payload.sender_id)];

    delivered[payload.packet_uid] = true;

    if (is_receiver) {
      // Sending back ACK
      was_sent = send_udp_payload(sockfd, &sender_node, &payload);
    }
  }
}

void keep_receiving_messages(int sockfd, bool *delivered, bool is_receiver,
                             std::vector<node_t> &nodes, bool *finito) {

  while (!*finito)
    receive_message(sockfd, delivered, is_receiver, nodes);
}
