#include <chrono>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <thread>

#include "common.hpp"
#include "parser.hpp"
#include "udp.hpp"
#include <signal.h>

#define MAX_PACKET_WAIT_MS 150

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing.\n";

  // write/flush output file if necessary
  std::cout << "Writing output.\n";

  // exit directly from signal handler
  exit(0);
}

int main(int argc, char **argv) {
  signal(SIGTERM, stop);
  signal(SIGINT, stop);

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = true;

  Parser parser(argc, argv);
  parser.parse();

  auto hosts = parser.hosts();
  std::vector<node_t> nodes;

  if (DEBUG) {
    std::cout << std::endl;

    std::cout << "My PID: " << getpid() << "\n";
    std::cout << "From a new terminal type `kill -SIGINT " << getpid()
              << "` or `kill -SIGTERM " << getpid()
              << "` to stop processing packets\n\n";

    std::cout << "My ID: " << parser.id() << "\n\n";

    std::cout << "List of resolved hosts is:\n";
    std::cout << "==========================\n";

    for (auto &host : hosts) {
      std::cout << host.id << "\n";
      std::cout << "Human-readable IP: " << host.ipReadable() << "\n";
      std::cout << "Machine-readable IP: " << host.ip << "\n";
      std::cout << "Human-readbale Port: " << host.portReadable() << "\n";
      std::cout << "Machine-readbale Port: " << host.port << "\n";
      std::cout << "\n";
    }

    std::cout << "\n";

    std::cout << "Path to output:\n";
    std::cout << "===============\n";
    std::cout << parser.outputPath() << "\n\n";

    std::cout << "Path to config:\n";
    std::cout << "===============\n";
    std::cout << parser.configPath() << "\n\n";

    std::cout << "Initializing...\n\n";
  }

  for (auto &host : hosts) {
    node_t node;
    node.id = host.id;
    node.ip = host.ip;
    node.port = host.port;
    nodes.push_back(node);
  }

  std::ifstream configFile(parser.configPath());
  uint64_t msgs_to_send_count;
  size_t receiver_id;

  configFile >> msgs_to_send_count;
  configFile >> receiver_id;

  node_t receiver_node = nodes[get_node_idx_by_id(nodes, receiver_id)];
  node_t myself_node = nodes[get_node_idx_by_id(nodes, parser.id())];
  payload_t payload;
  ssize_t message_len;
  uint8_t buffer[IP_MAXPACKET];
  int sockfd;

  bool should_send_messages = receiver_id != parser.id();
  bool is_receiver = !should_send_messages;

  if (should_send_messages) {
    if (DEBUG) {
      std::cout << "Sending messages...\n\n";
    }

    sockfd = init_socket();

    for (uint64_t i = 0; i < msgs_to_send_count; i++) {
      if (DEBUG) {
        std::cout << "Sending message to..." << ntohs(receiver_node.port)
                  << "\n";
      }

      payload.message = i;
      payload.packet_uid = 42;
      payload.sender_id = myself_node.id;
      encode_udp_payload(&payload, buffer);

      message_len =
          send_udp_packet(sockfd, &receiver_node, buffer, sizeof(payload_t));
    }
  }

  if (DEBUG) {
    std::cout << "Delivering messages...\n\n";
  }

  sockfd = bind_socket(myself_node.port);

  while (true) {
    int is_socket_ready;

    while ((is_socket_ready = select_socket(sockfd, 0, MAX_PACKET_WAIT_MS))) {
      receive_udp_packet(sockfd, buffer, IP_MAXPACKET);
      decode_udp_payload(&payload, buffer);

      show_payload(&payload);
      if (DEBUG)
        std::cout << "Got the message! from: " << payload.sender_id << "\n\n";

      node_t sender_node = nodes[get_node_idx_by_id(nodes, payload.sender_id)];

      if (is_receiver) {
        if (DEBUG)
          std::cout << "Sending back ACK...\n";
        // encode_udp_payload_as_ack_packet(&payload, buffer);

        message_len =
            send_udp_packet(sockfd, &sender_node, buffer, sizeof(ack_packet_t));
      }
    }
  }

  return 0;
}
