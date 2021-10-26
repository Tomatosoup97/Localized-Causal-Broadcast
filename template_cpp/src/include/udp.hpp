#ifndef _UDP_H_
#define _UDP_H_

#include "common.hpp"
#include <vector>

#define PAYLOAD_META_SIZE 8

typedef struct {
  ssize_t buff_size;
  uint32_t packet_uid;
  uint32_t sender_id;
  bool is_ack = false;
  char *buffer;
} payload_t;

size_t get_node_idx_by_id(std::vector<node_t> &nodes, unsigned long id);

bool send_udp_payload(int sockfd, node_t *receiver, payload_t *payload,
                      ssize_t size);
ssize_t receive_udp_payload(int sockfd, payload_t *payload);

ssize_t send_udp_packet(int sockfd, node_t *receiver, const char *buffer,
                        ssize_t buff_len);
ssize_t receive_udp_packet(int sockfd, char *buffer, ssize_t buff_len);

void encode_udp_payload(payload_t *payload, char *buffer, ssize_t buff_size);
void decode_udp_payload(payload_t *payload, char *buffer, ssize_t buff_size);
void show_payload(payload_t *payload);

int select_socket(int sockfd, int secs, int milisecs);
int init_socket();
int bind_socket(unsigned short port);

std::string buff_as_str(char *buffer, ssize_t size);

#endif
