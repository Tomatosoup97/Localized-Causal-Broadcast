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

using namespace std::chrono;

void receive_message(tcp_handler_t *tcp_handler) {
  int is_socket_ready;
  bool was_sent;
  ssize_t buff_size;
  message_t *message;

  while ((is_socket_ready =
              select_socket(tcp_handler->sockfd, 0, MAX_PACKET_WAIT_MS))) {
    payload_t *payload = new payload_t;
    buff_size = receive_udp_payload(tcp_handler->sockfd, payload);

    if (buff_size < 0) {
      delete payload;
      continue;
    }

    if (!payload->is_ack) {
      node_t *sender_node = (*tcp_handler->nodes)[get_node_idx_by_id(
          tcp_handler->nodes, payload->sender_id)];

      payload_t *ack_payload = new payload_t;
      copy_payload(ack_payload, payload);
      ack_payload->is_ack = true;

      message = new message_t;
      message->recipient = sender_node;
      message->payload = ack_payload;
      tcp_handler->sending_queue->enqueue(message);
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
    message->payload->sender_id = tcp_handler->current_node->id;

    if (DEBUG_V) std::cout << "Trying to send...\n";
    was_sent = send_udp_payload(tcp_handler->sockfd, message->recipient,
                                message->payload, message->payload->buff_size);
    if (DEBUG_V) std::cout << "Sent!\n";

    if (message->payload->is_ack) {
      // We no longer need it after ACK was sent
      if (DEBUG_V) std::cout << "Sending ACK: freeing message...\n";
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
      if (DEBUG_V) std::cout << "Retransmission: freeing message \n";
      show_payload(message->payload);
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
        construct_payload(payload, sender_node, *enqueued_messages);

        message = new message_t;
        construct_message(message, payload, receiver_node);

        tcp_handler->sending_queue->enqueue(message);
      }
    }
  }
}

void construct_message(message_t *message, payload_t *payload,
                       node_t *recipient) {
  message->first_send = true;
  message->recipient = recipient;
  message->payload = payload;
}

void construct_payload(payload_t *payload, node_t *sender, uint32_t seq_num) {
  std::string msg_content = std::to_string(seq_num);

  payload->buffer = new char[msg_content.length()];
  strcpy(payload->buffer, msg_content.c_str());
  payload->packet_uid = seq_num;

  payload->sender_id = sender->id;
  payload->owner_id = sender->id;
  payload->buff_size = msg_content.length();

  if (DEBUG) {
    std::cout << "Constructed ";
    show_payload(payload);
  }
}

bool should_start_retransmission(steady_clock::time_point sending_start) {
  time_point current_time = steady_clock::now();
  auto duration = duration_cast<microseconds>(current_time - sending_start);
  int64_t ms_since_last_sending = duration.count() / 1000;
  return ms_since_last_sending > RETRANSMISSION_OFFSET_MS;
}
