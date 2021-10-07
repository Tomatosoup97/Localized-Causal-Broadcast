#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include <netinet/ip.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>

#include "common.hpp"
#include "udp.hpp"

size_t get_node_idx_by_id(std::vector<node_t> &nodes, unsigned long id) {
  for (size_t index = 0; index < nodes.size(); ++index) {
    node_t node = nodes[index];
    if (node.id == id) {
      return index;
    }
  }
  return -1;
}

bool send_udp_payload(int sockfd, node_t *receiver, payload_t *payload) {
  uint8_t buffer[IP_MAXPACKET];
  bool was_sent = true;

  encode_udp_payload(payload, buffer);

  ssize_t message_len =
      send_udp_packet(sockfd, receiver, buffer, sizeof(payload_t));

  if (message_len != sizeof(payload_t)) {
    if (errno == ENOTCONN || errno == ENETUNREACH || errno == EHOSTUNREACH)
      was_sent = false;
    else
      throw std::runtime_error("sendto error");
  }

  if (DEBUG) {
    if (was_sent) {
      std::cout << "Sent ";
      show_payload(payload);
    } else
      std::cout << "Could not deliver the message!\n";
  }
  return was_sent;
}

void receive_udp_payload(int sockfd, payload_t *payload) {
  uint8_t buffer[IP_MAXPACKET];

  receive_udp_packet(sockfd, buffer, IP_MAXPACKET);
  decode_udp_payload(payload, buffer);

  if (DEBUG) {
    std::cout << "Received ";
    show_payload(payload);
  }
}

ssize_t send_udp_packet(int sockfd, node_t *receiver, const uint8_t *buffer,
                        ssize_t buff_len) {
  struct sockaddr_in recipent_addr;
  bzero(&recipent_addr, sizeof(recipent_addr));
  recipent_addr.sin_family = AF_INET;
  recipent_addr.sin_port = htons(receiver->port);
  recipent_addr.sin_addr.s_addr = receiver->ip;

  return sendto(sockfd, buffer, buff_len, 0,
                reinterpret_cast<struct sockaddr *>(&recipent_addr),
                sizeof(recipent_addr));
}

void receive_udp_packet(int sockfd, uint8_t *buffer, size_t buff_len) {
  struct sockaddr_in sender;
  socklen_t sender_len = sizeof(sender);

  ssize_t datagram_len =
      recvfrom(sockfd, buffer, buff_len, MSG_DONTWAIT,
               reinterpret_cast<struct sockaddr *>(&sender), &sender_len);
  if (datagram_len < 0)
    throw std::runtime_error("recvfrom error");

  char sender_ip_str[20];
  inet_ntop(AF_INET, &(sender.sin_addr), sender_ip_str, sizeof(sender_ip_str));

  fflush(stdout);
}

void encode_udp_payload(payload_t *payload, uint8_t *buffer) {
  memcpy(buffer, &payload->message, 8);
  memcpy(buffer + 8, &payload->packet_uid, 8);
  memcpy(buffer + 16, &payload->sender_id, 8);
}

void decode_udp_payload(payload_t *payload, uint8_t *buffer) {
  memcpy(&payload->sender_id, buffer + 16, 8);
  memcpy(&payload->packet_uid, buffer + 8, 8);
  memcpy(&payload->message, buffer, 8);
}

/*
void encode_udp_payload_as_ack_packet(payload_t *payload, uint8_t *buffer) {
  memcpy(buffer, &payload->packet_uid, 4);
  memcpy(buffer + 4, &payload->sender_id, 4);
}

void decode_udp_ack_packet(ack_packet_t *ack_packet, uint8_t *buffer) {
  memcpy(&payload->sender_id, buffer + 4, 4);
  memcpy(&payload->packet_uid, buffer, 4);
}
*/

void show_payload(payload_t *payload) {
  if (DEBUG) {
    uint64_t message;
    uint64_t packet_uid;
    uint64_t sender_id;

    std::cout << "Payload: "
              << "{ message: " << payload->message
              << ", packet uid: " << payload->packet_uid
              << ", sender id: " << payload->sender_id << " }\n";
  }
}

int select_socket(int sockfd, int secs, int milisecs) {
  fd_set descriptors;
  FD_ZERO(&descriptors);
  FD_SET(sockfd, &descriptors);

  struct timeval tv;
  tv.tv_sec = secs;
  tv.tv_usec = milisecs * 1000;

  return select(sockfd + 1, &descriptors, NULL, NULL, &tv);
}

int init_socket() {
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
    throw std::runtime_error("socket error");
  return sockfd;
}

int bind_socket(unsigned short port) {
  int sockfd = init_socket();
  struct sockaddr_in server_address;
  bzero(&server_address, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port);
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);

  int bind_res =
      bind(sockfd, reinterpret_cast<struct sockaddr *>(&server_address),
           sizeof(server_address));
  if (bind_res < 0)
    throw std::runtime_error("bind error");
  return sockfd;
}
