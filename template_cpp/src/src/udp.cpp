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

std::string buff_as_str(char *buffer, ssize_t size) {
  std::string str(buffer, size);
  return str;
}

size_t get_node_idx_by_id(std::vector<node_t> &nodes, unsigned long id) {
  for (size_t index = 0; index < nodes.size(); ++index) {
    node_t node = nodes[index];
    if (node.id == id) {
      return index;
    }
  }
  return -1;
}

bool send_udp_payload(int sockfd, node_t *receiver, payload_t *payload,
                      ssize_t size) {
  char buffer[IP_MAXPACKET];
  bool was_sent = true;

  encode_udp_payload(payload, buffer, size);
  ssize_t payload_size = PAYLOAD_META_SIZE + size;

  ssize_t message_len = send_udp_packet(sockfd, receiver, buffer, payload_size);

  if (message_len != payload_size) {
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

ssize_t receive_udp_payload(int sockfd, payload_t *payload) {
  char buffer[IP_MAXPACKET];

  ssize_t datagram_len = receive_udp_packet(sockfd, buffer, IP_MAXPACKET);

  if (datagram_len < 0) {
    return datagram_len;
  }

  ssize_t buff_size = datagram_len - PAYLOAD_META_SIZE;
  payload->buffer = new char[buff_size];
  decode_udp_payload(payload, buffer, buff_size);

  if (DEBUG) {
    std::cout << "Received ";
    show_payload(payload);
  }
  return buff_size;
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

void encode_udp_payload(payload_t *payload, char *buffer, ssize_t buff_size) {
  memcpy(buffer, &payload->packet_uid, 4);
  memcpy(buffer + 4, &payload->sender_id, 4);
  memcpy(buffer + 8, payload->buffer, buff_size);
}

void decode_udp_payload(payload_t *payload, char *buffer, ssize_t buff_size) {
  memcpy(&payload->packet_uid, buffer, 4);
  memcpy(&payload->sender_id, buffer + 4, 4);
  memcpy(payload->buffer, buffer + 8, buff_size);
  payload->buff_size = buff_size;
}

void show_payload(payload_t *payload) {
  if (DEBUG) {
    std::cout << "Payload: "
              << "{ message: "
              << buff_as_str(payload->buffer, payload->buff_size)
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
