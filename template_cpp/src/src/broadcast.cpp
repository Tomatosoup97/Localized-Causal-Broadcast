#include <iostream>

#include "broadcast.hpp"
#include "messages.hpp"
#include "tcp.hpp"
#include "udp.hpp"

void best_effort_broadcast(tcp_handler_t *tcp_handler, payload_t *payload) {
  for (node_t *node : *tcp_handler->nodes) {
    if (DEBUG_V)
      std::cout << "Broadcasting to " << node->id << "\n";
    if (node->id == tcp_handler->current_node->id) {
      continue; // don't send to yourself
    }
    payload_t *broadcast_payload = new payload_t;
    copy_payload(broadcast_payload, payload);
    message_t *message = new message_t;
    message->recipient = node;
    message->payload = broadcast_payload;
    tcp_handler->sending_queue->enqueue(message);
    // TODO:  ensure message_t is freed
  }
}

void uniform_reliable_broadcast(tcp_handler_t *tcp_handler,
                                payload_t *payload) {
  if (!tcp_handler->delivered->was_seen(payload)) {
    payload->sender_id = tcp_handler->current_node->id;

    if (DEBUG) {
      std::cout << "Broadcasting: ";
      show_payload(payload);
    }

    tcp_handler->delivered->mark_as_seen(payload);
    best_effort_broadcast(tcp_handler, payload);
  }
}

void broadcast_messages(tcp_handler_t *tcp_handler, node_t *sender_node,
                        uint32_t *enqueued_messages,
                        uint32_t msgs_to_send_count) {
  payload_t *payload;
  payload_t *log_payload;

  while (*enqueued_messages < msgs_to_send_count) {
    if (tcp_handler->sending_queue->size() < SENDING_CHUNK_SIZE) {

      uint32_t enqueue_until = std::min(
          *enqueued_messages + (SENDING_CHUNK_SIZE / 10), msgs_to_send_count);

      while (*enqueued_messages < enqueue_until) {
        (*enqueued_messages)++;

        payload = new payload_t;

        construct_payload(payload, sender_node, *enqueued_messages);

        uniform_reliable_broadcast(tcp_handler, payload);
        tcp_handler->broadcasted_queue->enqueue(payload);
      }
    }
  }
}
