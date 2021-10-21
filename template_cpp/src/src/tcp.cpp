#include <algorithm>
#include <chrono>
#include <iostream>
#include <sys/types.h>
#include <thread>

#include "common.hpp"
#include "tcp.hpp"
#include "ts_queue.hpp"
#include "udp.hpp"

#define MAX_PACKET_WAIT_MS 100
#define SENDING_CHUNK_SIZE 100000
#define RETRANSMISSION_OFFSET_MS 100

using namespace std::chrono;

void receive_message(tcp_handler_t *tcp_handler, bool is_receiver,
                     std::vector<node_t> &nodes) {
  int is_socket_ready;
  bool was_sent;

  while ((is_socket_ready =
              select_socket(tcp_handler->sockfd, 0, MAX_PACKET_WAIT_MS))) {
    payload_t *payload = new payload_t;
    receive_udp_payload(tcp_handler->sockfd, payload);
    node_t sender_node = nodes[get_node_idx_by_id(nodes, payload->sender_id)];

    // TODO: consider doing all below as a separate thread
    if (is_receiver) {
      // Send back ACK
      was_sent = send_udp_payload(tcp_handler->sockfd, &sender_node, payload);
    }

    // If first seen -> add to received queue
    if (!tcp_handler->delivered->contains(payload->sender_id,
                                          payload->packet_uid)) {
      tcp_handler->received_queue->enqueue(payload);
    }

    // Mark the message as delivered
    tcp_handler->delivered->insert(payload->sender_id, payload->packet_uid);
  }
}

void keep_receiving_messages(tcp_handler_t *tcp_handler, bool is_receiver,
                             std::vector<node_t> &nodes) {

  while (!*tcp_handler->finito)
    receive_message(tcp_handler, is_receiver, nodes);
}

void put_to_retransmission(tcp_handler_t *tcp_handler, payload_t *payload) {
  steady_clock::time_point sending_time = steady_clock::now();
  retrans_unit_t *retrans_unit = new retrans_unit_t;
  *retrans_unit = {payload, sending_time};
  tcp_handler->retrans_queue->enqueue(retrans_unit);
}

void keep_sending_messages_from_queue(tcp_handler_t *tcp_handler,
                                      std::vector<node_t> &nodes,
                                      node_t *receiver_node) {
  bool was_sent;

  while (!*tcp_handler->finito) {
    payload_t *payload = tcp_handler->sending_queue->dequeue();
    was_sent = send_udp_payload(tcp_handler->sockfd, receiver_node, payload);

    put_to_retransmission(tcp_handler, payload);
  }
}

void keep_retransmitting_messages(tcp_handler_t *tcp_handler) {
  if (tcp_handler->is_receiver) {
    return;
  }

  while (!*tcp_handler->finito) {
    retrans_unit_t *retrans_unit = tcp_handler->retrans_queue->dequeue();
    uint32_t packet_uid = retrans_unit->payload->packet_uid;

    if (tcp_handler->delivered->contains(retrans_unit->payload->sender_id,
                                         packet_uid)) {
      // already delivered - no need to retransmit
      continue;
    }

    if (DEBUG) {
      std::cout << "Retransmitting: ";
      show_payload(retrans_unit->payload);
    }

    while (!should_start_retransmission(retrans_unit->sending_time)) {
      // spin until we can retransmit again
    }

    tcp_handler->sending_queue->enqueue(retrans_unit->payload);
  }
}

void keep_enqueuing_messages(tcp_handler_t *tcp_handler, node_t *sender_node,
                             uint32_t *enqueued_messages,
                             uint32_t msgs_to_send_count) {
  payload_t *payload;

  if (tcp_handler->is_receiver) {
    return;
  }

  while (*enqueued_messages < msgs_to_send_count) {
    if (tcp_handler->sending_queue->size() < SENDING_CHUNK_SIZE) {

      uint32_t enqueue_until =
          std::min(*enqueued_messages + SENDING_CHUNK_SIZE, msgs_to_send_count);

      while (*enqueued_messages < enqueue_until) {
        payload = new payload_t;
        uint32_t nr = (*enqueued_messages) + 1;
        *payload = {nr, nr, sender_node->id};

        tcp_handler->sending_queue->enqueue(payload);
        (*enqueued_messages)++;
      }
    }
  }
}

bool should_start_retransmission(steady_clock::time_point sending_start) {
  time_point current_time = steady_clock::now();
  auto duration = duration_cast<microseconds>(current_time - sending_start);
  int64_t ms_since_last_sending = duration.count() / 1000;
  return ms_since_last_sending > RETRANSMISSION_OFFSET_MS;
}
