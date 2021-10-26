#include "broadcast.hpp"
#include "messages.hpp"
#include "tcp.hpp"
#include "udp.hpp"

void best_effort_broadcast(tcp_handler_t *tcp_handler,
                           std::vector<node_t> &nodes, payload_t *payload) {
  for (node_t node : nodes) {
    message_t *message = new message_t;
    message->recipient = &node;
    message->payload = payload;
    message->payload->sender_id = tcp_handler->current_node->id;
    tcp_handler->sending_queue->enqueue(message);
    // TODO: ensure message_t is freed
  }
}

void uniform_reliable_broadcast(tcp_handler_t *tcp_handler,
                                std::vector<node_t> &nodes,
                                payload_t *payload) {
  if (!tcp_handler->delivered->was_seen(payload)) {
    tcp_handler->delivered->mark_as_seen(payload);
    best_effort_broadcast(tcp_handler, nodes, payload);
  }
}
