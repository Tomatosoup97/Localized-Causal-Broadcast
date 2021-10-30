#include <iostream>
#include <math.h>
#include <string>

#include "common.hpp"
#include "messages.hpp"

std::string buff_as_str(char *buffer, ssize_t size) {
  std::string str(buffer, size);
  return str;
}

void encode_udp_payload(payload_t *payload, char *buffer, ssize_t buff_size) {
  if (DEBUG_V) std::cout << "Encoding...\n";
  uint32_t encoded_packet_id =
      contract_pair(payload->owner_id, payload->packet_uid);
  memcpy(buffer, &encoded_packet_id, 4);
  memcpy(buffer + 4, &payload->sender_id, 4);
  memcpy(buffer + 8, &payload->is_ack, 1);
  memcpy(buffer + 9, payload->buffer, buff_size);
  if (DEBUG_V) std::cout << "Encoded!\n";
}

void decode_udp_payload(payload_t *payload, char *buffer, ssize_t buff_size) {
  if (DEBUG_V) std::cout << "Decoding...\n";
  memcpy(&payload->packet_uid, buffer, 4);
  memcpy(&payload->sender_id, buffer + 4, 4);
  memcpy(&payload->is_ack, buffer + 8, 1);
  memcpy(payload->buffer, buffer + 9, buff_size);
  payload->buff_size = buff_size;

  auto decoded_packet_id = unfold_pair(payload->packet_uid);
  payload->owner_id = decoded_packet_id.first;
  payload->packet_uid = decoded_packet_id.second;
  if (DEBUG_V) std::cout << "Decoded!\n";
}

void copy_payload(payload_t *dest, payload_t *source) {
  if (DEBUG_V) std::cout << "Copying...\n";
  dest->buffer = new char[source->buff_size];
  dest->buff_size = source->buff_size;
  dest->packet_uid = source->packet_uid;
  dest->sender_id = source->sender_id;
  dest->owner_id = source->owner_id;
  dest->is_ack = source->is_ack;
  memcpy(dest->buffer, source->buffer, source->buff_size);
  if (DEBUG_V) std::cout << "Copied!\n";
}

void free_message(message_t *message) {
  delete message->payload->buffer;
  delete message->payload;
  delete message;
}

void show_payload(payload_t *payload) {
  if (DEBUG) {
    if (payload->is_ack) {
      std::cout << "ACK ";
    }
    std::cout << "Payload: "
              << "{ message: "
              << buff_as_str(payload->buffer, payload->buff_size)
              << ", packet uid: " << payload->packet_uid
              << ", owner id: " << payload->owner_id
              << ", sender id: " << payload->sender_id << " }\n";
  }
}

uint32_t contract_pair(uint32_t k1, uint32_t k2) {
  // Cantor pairing bijective function
  uint32_t s = k1 + k2;
  return s * (s + 1) / 2 + k2;
}

std::pair<uint32_t, uint32_t> unfold_pair(uint32_t p) {
  // Inversion of Cantor pairing bijective function
  // TODO: is it numerically correct?
  double wd;
  uint32_t w, t, x, y;
  wd = std::sqrt(8 * p + 1) - 1;
  w = static_cast<uint32_t>(std::floor(wd / 2));
  t = static_cast<uint32_t>((std::pow(w, 2) + w) / 2);
  y = p - t;
  x = w - y;
  return std::make_pair(x, y);
}
