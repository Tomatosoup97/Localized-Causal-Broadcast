#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <sys/types.h>
#include <thread>

#include "broadcast.hpp"
#include "common.hpp"
#include "delivered_set.hpp"
#include "messages.hpp"
#include "parser.hpp"
#include "tcp.hpp"
#include "udp.hpp"

/* #define DUMP_WHEN_ABOVE (MILLION / 5) */
#define DUMP_WHEN_ABOVE 0
#define DUMPING_CHUNK (MILLION / 10)
#define RECEIVER_THREADS_COUNT 1

uint32_t msgs_to_send_count;
uint32_t enqueued_messages = 0;
std::atomic<bool> finito = false;
node_t *myself_node;

tcp_handler_t tcp_handler;

std::thread enqueuer_thread;
std::thread retransmiter_thread;
std::thread writer_thread;
std::thread sender_thread;
std::thread receiver_thread_pool[RECEIVER_THREADS_COUNT];

const char *output_path;

static bool all_delivered() {
  return enqueued_messages >= msgs_to_send_count &&
         tcp_handler.sending_queue->size() == 0 &&
         tcp_handler.retrans_queue->size() == 0;
}

static void join_threads() {
  for (int i = 0; i < RECEIVER_THREADS_COUNT; i++) {
    receiver_thread_pool[i].join();
  }

  sender_thread.join();
  retransmiter_thread.join();
  writer_thread.join();
  enqueuer_thread.join();
}

static void dump_to_output(uint32_t until_size = 0) {
  if (DEBUG)
    std::cout << "Dumping to file...\n";

  std::ofstream output_file(output_path, std::ios_base::app);

  uint32_t zero = 0; // ensuring types match in max
  until_size = std::max(until_size, zero);

  while (tcp_handler.broadcasted_queue->size() > until_size) {
    payload_t *payload = tcp_handler.broadcasted_queue->dequeue();

    output_file << "b " << buff_as_str(payload->buffer, payload->buff_size)
                << "\n";
    free_payload(payload);
  }

  while (tcp_handler.delivered->deliverable->size() > until_size) {
    payload_t *payload = tcp_handler.delivered->deliverable->dequeue();

    output_file << "d " << payload->owner_id << " "
                << buff_as_str(payload->buffer, payload->buff_size) << "\n";
    free_payload(payload);
  }

  output_file.close();
}

static void keep_dumping_to_output() {
  while (!*tcp_handler.finito) {
    uint32_t current_size = tcp_handler.delivered->deliverable->size();

    uint32_t until_size = current_size - DUMPING_CHUNK;

    if (until_size > current_size) {
      until_size = 0;
    }

    if ((current_size > DUMP_WHEN_ABOVE) && DUMP_TO_FILE) {
      dump_to_output(until_size);
    }
  }
}

static void stop(int) {
  // reset signal handlers to default

  if (DEBUG)
    std::cout << "Stopping...\n";

  finito = true;

  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  if (DEBUG)
    std::cout << "Dumping...\n";

  if (DUMP_TO_FILE)
    dump_to_output();

  if (DEBUG)
    std::cout << "Joining...\n";

  join_threads();

  if (DEBUG)
    std::cout << "Releasing memory...\n";

  for (size_t index = 0; index < tcp_handler.nodes->size(); ++index) {
    node_t *node = (*tcp_handler.nodes)[index];
    delete node;
  }

  exit(0);
}

int main(int argc, char **argv) {
  signal(SIGTERM, stop);
  signal(SIGINT, stop);

  if (DEBUG)
    std::cout << "Initializing...\n";

  Parser parser(argc, argv);
  parser.parse();

  auto hosts = parser.hosts();
  std::vector<node_t *> nodes;

  for (auto &host : hosts) {
    node_t *node = new node_t;
    node->id = host.id;
    node->ip = host.ip;
    node->port = host.port;
    nodes.push_back(node);
  }

  output_path = parser.outputPath();

  // clean output file
  std::ofstream output_file;
  output_file.open(output_path, std::ofstream::out | std::ofstream::trunc);
  output_file.close();

  std::ifstream configFile(parser.configPath());
  uint32_t receiver_id;
  bool should_send_messages = true;
  node_t *receiver_node;

  MessagesQueue sending_queue;
  MessagesQueue retrans_queue;
  PayloadQueue deliverable;
  PayloadQueue broadcasted_queue;
  CausalityMap causality;

  configFile >> msgs_to_send_count;

  std::string line;
  std::istringstream iss;
  uint32_t enter_number;

  // burn empty line with \n character
  getline(configFile, line);

  for (auto &node : nodes) {
    getline(configFile, line);
    iss = std::istringstream(line);

    if (DEBUG)
      std::cout << "\nLine: " << line << "\n"
                << "Node: " << node->id;

    while (iss >> enter_number) {
      causality[node->id].push_back(enter_number);
      if (DEBUG)
        std::cout << ", num: " << enter_number;
    }
    std::cout << "\n";
  }

  configFile.close();

  uint32_t my_id = static_cast<uint32_t>(parser.id());
  myself_node = nodes[get_node_idx_by_id(&nodes, my_id)];

  DeliveredSet delivered = DeliveredSet(myself_node, nodes.size());
  delivered.deliverable = &deliverable;
  delivered.causality = &causality;

  tcp_handler.sockfd = bind_socket(myself_node->port);
  tcp_handler.finito = &finito;
  tcp_handler.current_node = myself_node;
  tcp_handler.nodes = &nodes;

  tcp_handler.sending_queue = &sending_queue;
  tcp_handler.retrans_queue = &retrans_queue;
  tcp_handler.broadcasted_queue = &broadcasted_queue;
  tcp_handler.delivered = &delivered;

  if (DEBUG)
    std::cout << "Spawning threads...\n";

  // Spawn threads for receiving messages
  for (int i = 0; i < RECEIVER_THREADS_COUNT; i++) {
    receiver_thread_pool[i] =
        std::thread(keep_receiving_messages, &tcp_handler);
  }

  // Spawn thread for sending messages
  sender_thread = std::thread(keep_sending_messages_from_queue, &tcp_handler);

  // Spawn thread for enqueuing messages
  enqueuer_thread = std::thread(broadcast_messages, &tcp_handler, myself_node,
                                &enqueued_messages, msgs_to_send_count);

  // Spawn thread for retransmitting messages
  retransmiter_thread = std::thread(keep_retransmitting_messages, &tcp_handler);

  // Spawn thread for dumping messages
  writer_thread = std::thread(keep_dumping_to_output);

  if (!KEEP_ALIVE) {
    while (!all_delivered()) {
      std::this_thread::sleep_for(200ms);
    }
    if (DEBUG)
      std::cout << "\nAll done, no more messages to send! :)\n";
  }

  join_threads();
  return 0;
}
