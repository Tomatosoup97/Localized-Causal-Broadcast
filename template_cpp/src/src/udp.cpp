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
#include "messages.hpp"
#include "udp.hpp"

size_t get_node_idx_by_id(std::vector<node_t *> *nodes, uint32_t id) {
  for (size_t index = 0; index < nodes->size(); ++index) {
    node_t *node = (*nodes)[index];
    if (node->id == id) {
      return index;
    }
  }
  return -1;
}

bool send_udp_payload(struct tcp_handler_s *h, int sockfd, node_t *receiver,
                      payload_t *payload, ssize_t size) {
  char buffer[IP_MAXPACKET];
  bool was_sent = true;

  ssize_t payload_size = encode_udp_payload(h, payload, buffer, size);

  if (DEBUG_V)
    std::cout << "Low level sending...\n";
  ssize_t message_len = send_udp_packet(sockfd, receiver, buffer, payload_size);
  if (DEBUG_V)
    std::cout << "Low level sent!\n";

  if (message_len != payload_size) {
    if (errno == ENOTCONN || errno == ENETUNREACH || errno == EHOSTUNREACH) {
      was_sent = false;
    } else {
      std::cout << "\nERRNO: " << errno << "\n";
      throw std::runtime_error("sendto error");
    }
  }

  if (DEBUG) {
    if (was_sent) {
      std::cout << "Sent to node " << receiver->id << " ";
      show_payload(payload, h);
    } else {
      std::cout << "Could not deliver the message!\n";
    }
  }
  return was_sent;
}

ssize_t receive_udp_payload(struct tcp_handler_s *h, int sockfd,
                            payload_t *payload) {
  char buffer[IP_MAXPACKET];

  ssize_t datagram_len = receive_udp_packet(sockfd, buffer, IP_MAXPACKET);

  if (datagram_len < 0) {
    return datagram_len;
  }

  decode_udp_payload(h, payload, buffer, datagram_len);

  if (DEBUG) {
    std::cout << "Received ";
    show_payload(payload, h);
  }
  return datagram_len;
}

ssize_t send_udp_packet(int sockfd, node_t *receiver, const char *buffer,
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

ssize_t receive_udp_packet(int sockfd, char *buffer, ssize_t buff_len) {
  struct sockaddr_in sender;
  socklen_t sender_len = sizeof(sender);

  ssize_t datagram_len =
      recvfrom(sockfd, buffer, buff_len, MSG_DONTWAIT,
               reinterpret_cast<struct sockaddr *>(&sender), &sender_len);
  if (datagram_len < 0) {
    if (errno == EAGAIN) {
      return datagram_len;
    }
    if (DEBUG)
      std::cout << errno << "\n";
    throw std::runtime_error("recvfrom error");
  }

  char sender_ip_str[20];
  inet_ntop(AF_INET, &(sender.sin_addr), sender_ip_str, sizeof(sender_ip_str));

  fflush(stdout);
  return datagram_len;
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
  if (bind_res < 0) {
    std::cout << "\nERRNO: " << errno << "\n";
    throw std::runtime_error("bind error");
  }
  return sockfd;
}
