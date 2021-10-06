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

  std::cout << std::endl;

  std::cout << "My PID: " << getpid() << "\n";
  std::cout << "From a new terminal type `kill -SIGINT " << getpid()
            << "` or `kill -SIGTERM " << getpid()
            << "` to stop processing packets\n\n";

  std::cout << "My ID: " << parser.id() << "\n\n";

  std::cout << "List of resolved hosts is:\n";
  std::cout << "==========================\n";
  auto hosts = parser.hosts();
  std::vector<node_t> nodes;

  for (auto &host : hosts) {
    std::cout << host.id << "\n";
    std::cout << "Human-readable IP: " << host.ipReadable() << "\n";
    std::cout << "Machine-readable IP: " << host.ip << "\n";
    std::cout << "Human-readbale Port: " << host.portReadable() << "\n";
    std::cout << "Machine-readbale Port: " << host.port << "\n";
    std::cout << "\n";

    node_t node;
    node.id = host.id;
    node.ip = host.ip;
    node.port = host.port;
    nodes.push_back(node);
  }
  std::cout << "\n";

  std::cout << "Path to output:\n";
  std::cout << "===============\n";
  std::cout << parser.outputPath() << "\n\n";

  std::cout << "Path to config:\n";
  std::cout << "===============\n";
  std::cout << parser.configPath() << "\n\n";

  std::cout << "Initializing...\n\n";

  std::ifstream configFile(parser.configPath());
  long unsigned int msgs_to_send_count;
  size_t receiver_id;

  configFile >> msgs_to_send_count;
  configFile >> receiver_id;

  node_t receiver_node = nodes[get_node_idx_by_id(nodes, receiver_id)];

  if (receiver_id == parser.id()) {
    // it's me! don't do anything, just receive
    std::cout << "I'm the receiver! Delivering messages only...\n\n";

    int receive_sockfd = bind_socket(receiver_node.port);

    while (true) {
      int is_socket_ready;
      int sockfd = receive_sockfd;

      /* while ((is_socket_ready = select_socket(sockfd, 0,
       * MAX_PACKET_WAIT_MS))) { */
      uint8_t buffer[1024];
      in_addr sender_ip_addr = receive_udp_packet(sockfd, buffer, 1024);
      std::cout << "Got the message! from: " << inet_ntoa(sender_ip_addr)
                << "\n\n";
      /* } */
    }
  } else {
    std::cout << "Broadcasting and delivering messages...\n\n";

    int receive_sockfd = init_socket();
    ssize_t message_len;

    uint8_t buffer[1024];
    const char* hello = "Hello";

    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      message_len = send_udp_packet(receive_sockfd, &receiver_node,
                                    reinterpret_cast<const uint8_t*>(hello), strlen(hello));

      std::cout << "Sending message to..." << ntohs(receiver_node.port) << "\n";
    }
  }

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }

  return 0;
}
