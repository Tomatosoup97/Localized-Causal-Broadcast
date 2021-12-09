#ifndef _BROADCAST_H_
#define _BROADCAST_H_

#include <vector>

#include "messages.hpp"
#include "tcp.hpp"
#include "udp.hpp"

void best_effort_broadcast(tcp_handler_t *tcp_handler, payload_t *payload);

void uniform_reliable_broadcast(tcp_handler_t *tcp_handler, payload_t *payload,
                                bool rebroadcast = true);

void broadcast_messages(tcp_handler_t *tcp_handler, node_t *sender_node,
                        uint32_t *enqueued_messages,
                        uint32_t msgs_to_send_count);
#endif
