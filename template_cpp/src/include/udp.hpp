#ifndef _UDP_H_
#define _UDP_H_

#include <vector>

#include "common.hpp"

size_t get_node_idx_by_id(std::vector<node_t> &nodes, uint32_t id);

bool send_udp_payload(int sockfd, node_t *receiver, payload_t *payload,
                      ssize_t size);
ssize_t receive_udp_payload(int sockfd, payload_t *payload);

ssize_t send_udp_packet(int sockfd, node_t *receiver, const char *buffer,
                        ssize_t buff_len);
ssize_t receive_udp_packet(int sockfd, char *buffer, ssize_t buff_len);

int select_socket(int sockfd, int secs, int milisecs);
int init_socket();
int bind_socket(unsigned short port);

#endif
