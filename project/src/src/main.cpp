#include <chrono>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <thread>

#include "common.hpp"
#include "parser.hpp"
#include "tcp.hpp"
#include "udp.hpp"
#include <signal.h>

#define RETRANSMISSION_OFFSET_MS 100

bool *delivered;
uint64_t first_undelivered = 0;
uint64_t msgs_to_send_count;

static bool all_delivered() {
  return first_undelivered == (msgs_to_send_count - 1) &&
         delivered[first_undelivered];
}

static void show_init_info(Parser parser, std::vector<Host> hosts) {
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
}

static void release_memory() {
  if (DEBUG)
    std::cout << "Cleaning up memory...\n";

  delete[] delivered;
}

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
  if (DEBUG)
    std::cout << "Immediately stopping network packet processing.\n";

  // write/flush output file if necessary
  if (DEBUG)
    std::cout << "Writing output.\n";

  release_memory();
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

  for (auto &host : hosts) {
    node_t node;
    node.id = host.id;
    node.ip = host.ip;
    node.port = host.port;
    nodes.push_back(node);
  }

  std::ifstream configFile(parser.configPath());
  size_t receiver_id;

  configFile >> msgs_to_send_count;
  configFile >> receiver_id;

  delivered = new bool[msgs_to_send_count]{false};

  payload_t payload;
  ssize_t message_len;
  uint8_t buffer[IP_MAXPACKET];

  node_t receiver_node = nodes[get_node_idx_by_id(nodes, receiver_id)];
  node_t myself_node = nodes[get_node_idx_by_id(nodes, parser.id())];

  bool should_send_messages = receiver_id != parser.id();
  bool is_receiver = !should_send_messages;
  bool was_sent;

  int sockfd = bind_socket(myself_node.port);

  std::chrono::steady_clock::time_point sending_start;
  std::chrono::steady_clock::time_point current_time;

  if (DEBUG) {
    if (is_receiver)
      std::cout << "Waiting for delivering messages...\n\n";
    else
      std::cout << "Sending messages to node " << receiver_id << "...\n\n";
  }

  while (true) {
    current_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        current_time - sending_start);
    int64_t ms_since_last_sending = duration.count() / 1000;

    if (should_send_messages &&
        ms_since_last_sending > RETRANSMISSION_OFFSET_MS) {

      sending_start = std::chrono::steady_clock::now();

      send_messages(sockfd, msgs_to_send_count, delivered, &receiver_node,
                    &myself_node, &first_undelivered);

      if (DEBUG && all_delivered()) {
        std::cout << "\n\nAll done, no more messages to send! :)\n\n";
        break;
      }
    }

    receive_message(sockfd, delivered, is_receiver, nodes);
  }

  release_memory();
  return 0;
}
