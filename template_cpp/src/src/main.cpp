#include <chrono>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <thread>

#include "common.hpp"
#include "delivered_set.hpp"
#include "parser.hpp"
#include "tcp.hpp"
#include "udp.hpp"
#include <signal.h>

uint32_t msgs_to_send_count;
uint32_t enqueued_messages = 0;
bool finito = false;
node_t myself_node;

tcp_handler_t tcp_handler;

const char *output_path;
bool is_receiver;

static bool all_delivered() {
  // TODO
  return false;
}

static void release_memory() {
  if (DEBUG)
    std::cout << "Cleaning up memory...\n";
}

static void dump_to_output() {
  if (DEBUG)
    std::cout << "Dumping to file...\n";

  std::ofstream output_file(output_path);

  if (is_receiver) {
    while (tcp_handler.received_queue->size() > 0) {
      payload_t *payload = tcp_handler.received_queue->dequeue();
      output_file << "d " << payload->sender_id << " " << payload->message
                  << "\n";
    }
  } else {
    for (auto i : tcp_handler.delivered->get_set(myself_node.id)) {
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

  payload_t payload;
  ssize_t message_len;
  uint8_t buffer[IP_MAXPACKET];

  node_t receiver_node = nodes[get_node_idx_by_id(nodes, receiver_id)];
  myself_node = nodes[get_node_idx_by_id(nodes, parser.id())];

  bool should_send_messages = receiver_id != parser.id();
  is_receiver = !should_send_messages;
  bool was_sent;

  int sockfd = bind_socket(myself_node.port);

  SafeQueue<payload_t *> sending_queue;
  SafeQueue<payload_t *> received_queue;
  SafeQueue<retrans_unit_t *> retrans_queue;
  DeliveredSet delivered = DeliveredSet(nodes.size());

  tcp_handler.sockfd = sockfd;
  tcp_handler.finito = &finito;
  tcp_handler.is_receiver = is_receiver;

  tcp_handler.sending_queue = &sending_queue;
  tcp_handler.received_queue = &received_queue;
  tcp_handler.retrans_queue = &retrans_queue;
  tcp_handler.delivered = &delivered;

  // Spawn thread for receiving messages
  std::thread receiver_thread(keep_receiving_messages, &tcp_handler,
                              is_receiver, std::ref(nodes));

  // Spawn thread for sending messages
  std::thread sender_thread(keep_sending_messages_from_queue, &tcp_handler,
                            std::ref(nodes), &receiver_node);

  // Spawn thread for enqueuing messages
  std::thread enqueuer_thread(keep_enqueuing_messages, &tcp_handler,
                              &myself_node, &enqueued_messages,
                              msgs_to_send_count);

  // Spawn thread for retransmitting messages
  std::thread retransmiter_thread(keep_retransmitting_messages, &tcp_handler);

  /* if (!KEEP_ALIVE && all_delivered()) { */
  /*   finito = true; */
  /*   if (DEBUG) */
  /*     std::cout << "\n\nAll done, no more messages to send! :)\n\n"; */
  /* } */

  receiver_thread.join();
  sender_thread.join();
  enqueuer_thread.join();
  retransmiter_thread.join();

  stop(0);
  return 0;
}