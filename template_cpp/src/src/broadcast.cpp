#include "broadcast.hpp"
#include "tcp.hpp"
#include "udp.hpp"

void best_effort_broadcast(tcp_handler_t *tcp_handler,
                           std::vector<node_t> &nodes, payload_t *payload) {
  for (node_t node : nodes) {
    message_t *message = new message_t;
    message->recipient = &node;
    message->payload = payload;
    tcp_handler->sending_queue->enqueue(message);
    // TODO: ensure message_t is freed
  }
}
