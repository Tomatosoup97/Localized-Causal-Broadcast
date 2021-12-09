#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <sys/types.h>
#include <thread>
#include <utility>

#include "broadcast.hpp"
#include "common.hpp"
#include "messages.hpp"
#include "tcp.hpp"
#include "ts_queue.hpp"
#include "udp.hpp"

#undef NDEBUG

using namespace std::chrono;

void keep_receiving_messages(tcp_handler_t *tcp_handler) {
  bool should_alloc = true;
  ssize_t buff_size;
  message_t *message;
  payload_t *payload;

  while (!*tcp_handler->finito)
    if (select_socket(tcp_handler->sockfd, 0, MAX_PACKET_WAIT_MS)) {

      if (should_alloc)
        payload = new payload_t;

      buff_size =
          receive_udp_payload(tcp_handler, tcp_handler->sockfd, payload);

      bool should_alloc = buff_size < 0;
      if (buff_size < 0) {
        continue;
      }

      if (!payload->is_ack) {
        node_t *sender_node = (*tcp_handler->nodes)[get_node_idx_by_id(
            tcp_handler->nodes, payload->sender_id)];

        show_payload(payload, tcp_handler);
        payload_t *ack_payload = new payload_t;

        uint32_t vc_size = vector_clock_size(tcp_handler);
        copy_payload(ack_payload, payload, vc_size);
        ack_payload->is_ack = true;
        show_payload(ack_payload, tcp_handler);

        message = new message_t;
        message->recipient = sender_node;
        message->payload = ack_payload;
        tcp_handler->sending_queue->enqueue(message);
      }

      tcp_handler->delivered->insert(payload->sender_id, payload);

      uniform_reliable_broadcast(tcp_handler, payload);
    }
}

void keep_sending_messages_from_queue(tcp_handler_t *tcp_handler) {
  bool was_sent;

  while (!*tcp_handler->finito) {

    message_t *message = tcp_handler->sending_queue->dequeue();
    message->payload->sender_id = tcp_handler->current_node->id;

    show_payload(message->payload, tcp_handler);

    if (DEBUG_V)
      std::cout << "Trying to send...\n";
    was_sent =
        send_udp_payload(tcp_handler, tcp_handler->sockfd, message->recipient,
                         message->payload, message->payload->buff_size);
    if (DEBUG_V)
      std::cout << "Sent!\n";

    if (message->payload->is_ack) {
      // We no longer need it after ACK was sent
      if (DEBUG_V)
        std::cout << "Sending ACK: freeing message...\n";
      free_message(message);
    } else {
      // Retransmitting
      message->sending_time = steady_clock::now();
      message->first_send = false;
      tcp_handler->retrans_queue->enqueue(message);
    }
  }
}

void keep_retransmitting_messages(tcp_handler_t *tcp_handler) {
  while (!*tcp_handler->finito) {
    message_t *message = tcp_handler->retrans_queue->dequeue();
    uint32_t packet_uid = message->payload->packet_uid;

    if (tcp_handler->delivered->contains(message->recipient->id,
                                         message->payload)) {
      // already delivered - no need to retransmit
      if (DEBUG_V)
        std::cout << "Retransmission: freeing message \n";
      show_payload(message->payload, tcp_handler);
      free_message(message);
      continue;
    }

    if (DEBUG) {
      std::cout << "Retransmitting: ";
      show_payload(message->payload, tcp_handler);
    }

    while (!should_start_retransmission(message->sending_time)) {
      // spin until we can retransmit again
    }

    tcp_handler->sending_queue->enqueue(message);
  }
}

void construct_message(message_t *message, payload_t *payload,
                       node_t *recipient) {
  message->first_send = true;
  message->recipient = recipient;
  message->payload = payload;
}

void construct_payload(tcp_handler_t *h, payload_t *payload, node_t *sender,
                       uint32_t seq_num) {
  std::string msg_content = std::to_string(seq_num);

  payload->buffer = new char[msg_content.length()];
  strncpy(payload->buffer, msg_content.c_str(), msg_content.length());
  payload->packet_uid = seq_num;

  payload->sender_id = sender->id;
  payload->owner_id = sender->id;
  payload->buff_size = msg_content.length();

  uint32_t vc_size = vector_clock_size(h);
  payload->vector_clock = new uint32_t[vc_size];
  memcpy(payload->vector_clock, h->delivered->vector_clock, 4 * vc_size);

  if (DEBUG) {
    std::cout << "Constructed ";
    show_payload(payload, h);
  }
}

bool should_start_retransmission(steady_clock::time_point sending_start) {
  time_point current_time = steady_clock::now();
  auto duration = duration_cast<microseconds>(current_time - sending_start);
  int64_t ms_since_last_sending = duration.count() / 1000;
  return ms_since_last_sending > RETRANSMISSION_OFFSET_MS;
}
