#include <cassert>
#include <iostream>
#include <math.h>
#include <string>

#include "common.hpp"
#include "messages.hpp"
#include "tcp.hpp"

std::string buff_as_str(char *buffer, ssize_t size) {
  std::string str(buffer, size);
  return str;
}

ssize_t encode_udp_payload(struct tcp_handler_s *h, payload_t *payload,
                           char *buffer, ssize_t buff_size) {

  uint32_t vc_size = vector_clock_size(h);

  if (DEBUG_V) {
    std::cout << "Encoding...\n";
    show_vector_clock(payload->vector_clock, vc_size);
  }

  memcpy(buffer, &payload->packet_uid, 4);
  memcpy(buffer + 4, &payload->sender_id, 4);
  memcpy(buffer + 8, &payload->owner_id, 4);
  memcpy(buffer + 12, &payload->is_ack, 1);
  memcpy(buffer + 13, payload->buffer, buff_size);
  memcpy(buffer + 13 + buff_size, payload->vector_clock, vc_size * 4);

  if (DEBUG_V)
    std::cout << "Encoded!\n";

  return 13 + buff_size + vc_size * 4;
}

void decode_udp_payload(struct tcp_handler_s *h, payload_t *payload,
                        char *buffer, size_t datagram_len) {
  if (DEBUG_V)
    std::cout << "Decoding...\n";
  memcpy(&payload->packet_uid, buffer, 4);
  memcpy(&payload->sender_id, buffer + 4, 4);
  memcpy(&payload->owner_id, buffer + 8, 4);
  memcpy(&payload->is_ack, buffer + 12, 1);

  uint32_t vc_size = vector_clock_size(h);
  payload->vector_clock = new uint32_t[vc_size];

  ssize_t buff_size = datagram_len - PAYLOAD_META_SIZE - vc_size * 4;
  payload->buffer = new char[buff_size];

  memcpy(payload->buffer, buffer + 13, buff_size);
  memcpy(payload->vector_clock, buffer + 13 + buff_size, vc_size * 4);

  payload->buff_size = buff_size;

  if (DEBUG_V) {
    show_vector_clock(payload->vector_clock, vc_size);
    std::cout << "Decoded!\n";
  }
}

void copy_payload(payload_t *dest, payload_t *source, uint32_t vc_size) {
  if (DEBUG_V)
    std::cout << "Copying...\n";
  dest->buffer = new char[source->buff_size];
  dest->vector_clock = new uint32_t[vc_size];
  dest->buff_size = source->buff_size;
  dest->packet_uid = source->packet_uid;
  dest->sender_id = source->sender_id;
  dest->owner_id = source->owner_id;
  dest->is_ack = source->is_ack;
  memcpy(dest->buffer, source->buffer, source->buff_size);
  memcpy(dest->vector_clock, source->vector_clock, vc_size * 4);

  if (DEBUG_V)
    std::cout << "Copied!\n";
}

void free_message(message_t *message) {
  free_payload(message->payload);
  delete message;
}

void free_payload(payload_t *payload) {
  delete[] payload->buffer;
  delete[] payload->vector_clock;
  delete payload;
}

void show_payload(payload_t *payload, struct tcp_handler_s *h) {
  if (DEBUG) {
    if (payload->is_ack) {
      std::cout << "ACK ";
    }
    std::cout << "Payload: "
              << "{ message: "
              << buff_as_str(payload->buffer, payload->buff_size)
              << ", packet uid: " << payload->packet_uid
              << ", owner id: " << payload->owner_id
              << ", sender id: " << payload->sender_id;

    if (h != NULL) {
      std::cout << ", VC: ";

      uint32_t vc_size = vector_clock_size(h);
      for (uint32_t i = 0; i < vc_size; i++) {
        std::cout << payload->vector_clock[i] << " ";
      }
    }

    std::cout << " }\n";
  }
}
