#include <vector>

#include "messages.hpp"
#include "tcp.hpp"
#include "udp.hpp"

void best_effort_broadcast(tcp_handler_t *tcp_handler, payload_t *payload);

void uniform_reliable_broadcast(tcp_handler_t *tcp_handler, payload_t *payload);
