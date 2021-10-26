#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <thread>

#include "common.hpp"
#include "delivered_set.hpp"
#include "messages.hpp"
#include "parser.hpp"
#include "tcp.hpp"
#include "udp.hpp"
#include <signal.h>

#define DUMP_WHEN_ABOVE (MILLION / 2)
#define DUMPING_CHUNK (MILLION / 10)
#define RECEIVER_THREADS_COUNT 4

uint32_t msgs_to_send_count;
uint32_t enqueued_messages = 0;
bool finito = false;
node_t myself_node;

tcp_handler_t tcp_handler;

const char *output_path;
bool is_receiver;

static bool all_delivered() {
  return enqueued_messages >= msgs_to_send_count &&
         tcp_handler.sending_queue->size() == 0 &&
         tcp_handler.retrans_queue->size() == 0;
}

static void dump_to_output(uint32_t until_size = 0) {
  if (DEBUG)
    std::cout << "Dumping to file...\n";

  std::ofstream output_file(output_path, std::ios_base::app);

  uint32_t zero = 0; // ensuring types match in max
  until_size = std::max(until_size, zero);
  while (tcp_handler.received_queue->size() > until_size) {
    payload_t *payload = tcp_handler.received_queue->dequeue();

    if (is_receiver) {
      output_file << "d " << payload->sender_id << " "
                  << buff_as_str(payload->buffer, payload->buff_size) << "\n";
    } else {
      output_file << "b " << buff_as_str(payload->buffer, payload->buff_size)
                  << "\n";
    }
    delete payload->buffer;
    delete payload;
  }

  output_file.close();
}

static void keep_dumping_to_output() {
  while (!*tcp_handler.finito) {
    uint32_t current_size = tcp_handler.received_queue->size();

    if (current_size > DUMP_WHEN_ABOVE) {
      dump_to_output(current_size - DUMPING_CHUNK);
    }
  }
}

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  if (DUMP_TO_FILE)
    dump_to_output();
  exit(0);
}

int main(int argc, char **argv) {
  signal(SIGTERM, stop);
  signal(SIGINT, stop);

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = true;

  if (DEBUG)
    std::cout << "Initializing...\n";

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

  // clean output file
  std::ofstream output_file;
  output_file.open(output_path, std::ofstream::out | std::ofstream::trunc);
  output_file.close();

  std::ifstream configFile(parser.configPath());
  size_t receiver_id;

  configFile >> msgs_to_send_count;
  configFile >> receiver_id;

  configFile.close();

  node_t receiver_node = nodes[get_node_idx_by_id(nodes, receiver_id)];
  myself_node = nodes[get_node_idx_by_id(nodes, parser.id())];

  bool should_send_messages = receiver_id != parser.id();
  is_receiver = !should_send_messages;
  bool was_sent;

  int sockfd = bind_socket(myself_node.port);

  MessagesQueue sending_queue;
  MessagesQueue retrans_queue;
  PayloadQueue received_queue;
  DeliveredSet delivered = DeliveredSet(&myself_node, nodes.size());

  tcp_handler.sockfd = sockfd;
  tcp_handler.finito = &finito;
  tcp_handler.is_receiver = is_receiver;
  tcp_handler.current_node = &myself_node;

  tcp_handler.sending_queue = &sending_queue;
  tcp_handler.retrans_queue = &retrans_queue;
  tcp_handler.received_queue = &received_queue;
  tcp_handler.delivered = &delivered;

  if (DEBUG)
    std::cout << "Spawning threads...\n";

  std::thread receiver_thread_pool[RECEIVER_THREADS_COUNT];

  // Spawn threads for receiving messages
  for (int i = 0; i < RECEIVER_THREADS_COUNT; i++) {
    receiver_thread_pool[i] = std::thread(keep_receiving_messages, &tcp_handler,
                                          is_receiver, std::ref(nodes));
  }

  // Spawn thread for sending messages
  std::thread sender_thread(keep_sending_messages_from_queue, &tcp_handler,
                            std::ref(nodes));

  // Spawn thread for enqueuing messages
  std::thread enqueuer_thread(keep_enqueuing_messages, &tcp_handler,
                              &myself_node, &receiver_node, &enqueued_messages,
                              msgs_to_send_count);

  // Spawn thread for retransmitting messages
  std::thread retransmiter_thread(keep_retransmitting_messages, &tcp_handler);

  // Spawn thread for dumping messages
  std::thread writer_thread(keep_dumping_to_output);

  if (!KEEP_ALIVE) {
    while (!all_delivered()) {
      std::this_thread::sleep_for(200ms);
    }
    finito = true;
    if (DEBUG)
      std::cout << "\nAll done, no more messages to send! :)\n";
    stop(0);
  }

  // Spawn threads for receiving messages
  for (int i = 0; i < RECEIVER_THREADS_COUNT; i++) {
    receiver_thread_pool[i].join();
  }

  sender_thread.join();
  enqueuer_thread.join();
  retransmiter_thread.join();
  writer_thread.join();

  stop(0);
  return 0;
}
