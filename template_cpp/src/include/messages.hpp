#ifndef _MESSAGES_H_
#define _MESSAGES_H_

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "common.hpp"
#include "ts_queue.hpp"

using namespace std::chrono; // noqa

#define PAYLOAD_META_SIZE 13

struct tcp_handler_s;

typedef struct {
  ssize_t buff_size;
  uint32_t owner_id;
  uint32_t packet_uid;
  uint32_t sender_id;
  bool is_ack = false;
  uint32_t *vector_clock;
  char *buffer;
} payload_t;

typedef struct {
  payload_t *payload;
  node_t *recipient;
  steady_clock::time_point sending_time;
  bool first_send = false;
} message_t;

typedef SafeQueue<message_t *> MessagesQueue;
typedef SafeQueue<payload_t *> PayloadQueue;

std::string buff_as_str(char *buffer, ssize_t size);

ssize_t encode_udp_payload(struct tcp_handler_s *h, payload_t *payload,
                           char *buffer, ssize_t buff_size);
void decode_udp_payload(struct tcp_handler_s *h, payload_t *payload,
                        char *buffer, size_t datagram_len);

void copy_payload(payload_t *dest, payload_t *source, uint32_t vc_size);
void free_payload(payload_t *payload);
void free_message(message_t *message);

void show_payload(payload_t *payload, struct tcp_handler_s *h);

#endif
