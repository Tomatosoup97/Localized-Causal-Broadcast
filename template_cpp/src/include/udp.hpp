#ifndef _UDP_H_
#define _UDP_H_

#include "common.hpp"
#include <vector>

typedef struct {
  uint32_t message;
  uint32_t packet_uid;
  uint32_t sender_id;
} payload_t;

size_t get_node_idx_by_id(std::vector<node_t> &nodes, unsigned long id);

bool send_udp_payload(int sockfd, node_t *receiver, payload_t *payload);
void receive_udp_payload(int sockfd, payload_t *payload);
ssize_t send_udp_packet(int sockfd, node_t *receiver, const uint8_t *buffer,
                        ssize_t buff_len);
void receive_udp_packet(int sockfd, uint8_t *buffer, size_t buff_len);

void encode_udp_payload(payload_t *payload, uint8_t *buffer);
void decode_udp_payload(payload_t *payload, uint8_t *buffer);
void show_payload(payload_t *payload);

int select_socket(int sockfd, int secs, int milisecs);
int init_socket();
int bind_socket(unsigned short port);

#endif
