#include "tcp.hpp"
#include "udp.hpp"

void best_effort_broadcast(tcp_handler_t *tcp_handler,
                           std::vector<node_t> &nodes, payload_t *payload);
