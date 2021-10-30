#ifndef _MESSAGES_H_
#define _MESSAGES_H_

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "common.hpp"
#include "ts_queue.hpp"

using namespace std::chrono; // noqa

#define PAYLOAD_META_SIZE 9

typedef struct {
  ssize_t buff_size;
  uint32_t owner_id;
  uint32_t packet_uid;
  uint32_t sender_id;
  bool is_ack = false;
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

void encode_udp_payload(payload_t *payload, char *buffer, ssize_t buff_size);
void decode_udp_payload(payload_t *payload, char *buffer, ssize_t buff_size);

void copy_payload(payload_t *dest, payload_t *source);
void free_message(message_t *message);

void show_payload(payload_t *payload);

uint32_t contract_pair(uint32_t k1, uint32_t k2);
std::pair<uint32_t, uint32_t> unfold_pair(uint32_t p);

#endif
