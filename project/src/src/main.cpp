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

#define RETRANSMISSION_OFFSET_MS 150

bool *delivered;
uint32_t first_undelivered = 0;
uint32_t msgs_to_send_count;
bool finito = false;

const char *output_path;
bool is_receiver;

static bool all_delivered() {
  return first_undelivered == (msgs_to_send_count - 1) &&
         delivered[first_undelivered];
}

static void release_memory() {
  if (DEBUG)
    std::cout << "Cleaning up memory...\n";

  delete[] delivered;
}

static void dump_to_output() {
  if (DEBUG)
    std::cout << "Dumping to file...\n";

  std::ofstream output_file(output_path);

  for (uint32_t i = 0; i < msgs_to_send_count; i++) {
    if (!delivered[i]) {
      continue;
    }

    if (is_receiver) {
      output_file << "d "
                  << "1"
                  << " " << i << "\n";
    } else {
      output_file << "b " << i << "\n";
    }
  }

  output_file.close();
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

  if (DUMP_TO_FILE)
    dump_to_output();
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

  output_path = parser.outputPath();
  std::ifstream configFile(parser.configPath());
  size_t receiver_id;

  configFile >> msgs_to_send_count;
  configFile >> receiver_id;

  configFile.close();

  delivered = new bool[msgs_to_send_count]{false};

  payload_t payload;
  ssize_t message_len;
  uint8_t buffer[IP_MAXPACKET];

  node_t receiver_node = nodes[get_node_idx_by_id(nodes, receiver_id)];
  node_t myself_node = nodes[get_node_idx_by_id(nodes, parser.id())];

  bool should_send_messages = receiver_id != parser.id();
  is_receiver = !should_send_messages;
  bool was_sent;

  int sockfd = bind_socket(myself_node.port);

  std::chrono::steady_clock::time_point sending_start;
  std::chrono::steady_clock::time_point current_time;

  // Spawn thread for receiving messages
  std::thread receiver_thread(keep_receiving_messages, sockfd, delivered,
                              is_receiver, std::ref(nodes), &finito);

  while (should_send_messages) {
    current_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        current_time - sending_start);
    int64_t ms_since_last_sending = duration.count() / 1000;

    if (should_send_messages &&
        ms_since_last_sending > RETRANSMISSION_OFFSET_MS) {

      sending_start = std::chrono::steady_clock::now();

      send_messages(sockfd, msgs_to_send_count, delivered, &receiver_node,
                    &myself_node, &first_undelivered);

      if (!KEEP_ALIVE && all_delivered()) {
        finito = true;
        if (DEBUG)
          std::cout << "\n\nAll done, no more messages to send! :)\n\n";
        break;
      }
    }
  }

  receiver_thread.join();

  stop(0);
  return 0;
}
