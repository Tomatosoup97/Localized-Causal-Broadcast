#include <algorithm>
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

#define MAX_PACKET_WAIT_MS 100
#define SENDING_CHUNK_SIZE (MILLION / 10)
#define RETRANSMISSION_OFFSET_MS 200

using namespace std::chrono;

void receive_message(tcp_handler_t *tcp_handler) {
  int is_socket_ready;
  bool was_sent;
  ssize_t buff_size;

  while ((is_socket_ready =
              select_socket(tcp_handler->sockfd, 0, MAX_PACKET_WAIT_MS))) {
    payload_t *payload = new payload_t;
    buff_size = receive_udp_payload(tcp_handler->sockfd, payload);

    if (buff_size < 0) {
      delete payload;
      continue;
    }

    if (!payload->is_ack) {
      payload->is_ack = true;
      node_t sender_node = (*tcp_handler->nodes)[get_node_idx_by_id(
          tcp_handler->nodes, payload->sender_id)];
      was_sent = send_udp_payload(tcp_handler->sockfd, &sender_node, payload,
                                  buff_size);
    }

    tcp_handler->delivered->insert(payload->sender_id, payload);

    // TODO: put that somewhere else
    if (!PERFECT_LINKS_MODE) {
      uniform_reliable_broadcast(tcp_handler, payload);
    }
  }
}

void keep_receiving_messages(tcp_handler_t *tcp_handler) {

  while (!*tcp_handler->finito)
    receive_message(tcp_handler);
}

void keep_sending_messages_from_queue(tcp_handler_t *tcp_handler) {
  bool was_sent;

  while (!*tcp_handler->finito) {

    message_t *message = tcp_handler->sending_queue->dequeue();
    was_sent = send_udp_payload(tcp_handler->sockfd, message->recipient,
                                message->payload, message->payload->buff_size);

    if (message->payload->is_ack) {
      // We no longer need it after ACK was sent
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

    if (tcp_handler->delivered->contains(message->payload->sender_id,
                                         message->payload)) {
      // already delivered - no need to retransmit
      free_message(message);
      continue;
    }

    if (DEBUG) {
      std::cout << "Retransmitting: ";
      show_payload(message->payload);
    }

    while (!should_start_retransmission(message->sending_time)) {
      // spin until we can retransmit again
    }

    tcp_handler->sending_queue->enqueue(message);
  }
}

void keep_enqueuing_messages(tcp_handler_t *tcp_handler, node_t *sender_node,
                             node_t *receiver_node, uint32_t *enqueued_messages,
                             uint32_t msgs_to_send_count) {
  payload_t *payload;
  message_t *message;
  node_t *recipient;

  while (*enqueued_messages < msgs_to_send_count) {
    if (tcp_handler->sending_queue->size() < SENDING_CHUNK_SIZE) {

      uint32_t enqueue_until =
          std::min(*enqueued_messages + SENDING_CHUNK_SIZE, msgs_to_send_count);

      while (*enqueued_messages < enqueue_until) {
        (*enqueued_messages)++;

        payload = new payload_t;
        message = new message_t;
        message->payload = payload;
        message->recipient = receiver_node;
        construct_message(message, sender_node, *enqueued_messages);

        tcp_handler->sending_queue->enqueue(message);
      }
    }
  }
}

void construct_message(message_t *message, node_t *sender, uint32_t seq_num) {
  std::string msg_content = std::to_string(seq_num);
  message->payload->buffer = new char[msg_content.length()];

  strcpy(message->payload->buffer, msg_content.c_str());
  message->payload->packet_uid = seq_num;

  message->payload->sender_id = sender->id;
  message->payload->owner_id = sender->id;
  message->payload->buff_size = msg_content.length();

  message->first_send = true;

  if (DEBUG) {
    std::cout << "Constructed ";
    show_payload(message->payload);
  }
}

bool should_start_retransmission(steady_clock::time_point sending_start) {
  time_point current_time = steady_clock::now();
  auto duration = duration_cast<microseconds>(current_time - sending_start);
  int64_t ms_since_last_sending = duration.count() / 1000;
  return ms_since_last_sending > RETRANSMISSION_OFFSET_MS;
}
