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
  /* Receive UDP packet from socket and return sender ip addr */

  struct sockaddr_in sender;
  socklen_t sender_len = sizeof(sender);

  std::cout << "Receiving the message...\n";
  ssize_t datagram_len =
      recvfrom(sockfd, buffer, buff_len, MSG_DONTWAIT,
               reinterpret_cast<struct sockaddr *>(&sender), &sender_len);
  std::cout << "Received!...\n";
  if (datagram_len < 0)
    throw std::runtime_error("recvfrom error");

  char sender_ip_str[20];
  inet_ntop(AF_INET, &(sender.sin_addr), sender_ip_str, sizeof(sender_ip_str));

  fflush(stdout);
}

void encode_udp_payload(payload_t *payload, uint8_t *buffer) {
  memcpy(buffer, &payload->message, 4);
  memcpy(buffer + 4, &payload->packet_uid, 4);
  memcpy(buffer + 8, &payload->sender_id, 4);
}

void decode_udp_payload(payload_t *payload, uint8_t *buffer) {
  memcpy(&payload->sender_id, buffer + 8, 4);
  memcpy(&payload->packet_uid, buffer + 4, 4);
  memcpy(&payload->message, buffer, 4);
}

void show_payload(payload_t *payload) {
  if (DEBUG) {
    uint64_t message;
    uint64_t packet_uid;
    uint64_t sender_id;

    std::cout << "Payload contents:\n"
              << "\tmessage: \t" << payload->message << "\n"
              << "\tpacket uid: \t" << payload->packet_uid << "\n"
              << "\tsender id: \t" << payload->sender_id << "\n";
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
