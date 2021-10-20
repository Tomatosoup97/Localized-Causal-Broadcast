#ifndef _COMMON_H_
#define _COMMON_H_

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

// Program flags
#define DEBUG 0
#define KEEP_ALIVE 0
#define DUMP_TO_FILE 1

#define IP_MAXPACKET 65535

typedef struct {
  char ip[32];
} ip_addr_v;

typedef struct in_addr ip_addr_t;

typedef struct {
  uint32_t message;
  uint32_t packet_uid;
  uint32_t sender_id;
} payload_t;

typedef struct {
  uint32_t packet_uid;
  uint32_t receiver_id;
} ack_packet_t;

typedef struct {
  uint32_t id;
  in_addr_t ip;
  unsigned short port;
} node_t;

#define print_buff(buffer, buff_len)                                           \
  ({                                                                           \
    for (ssize_t i = 0; i < buff_len; i++)                                     \
      printf("%d ", buffer[i]);                                                \
    printf("\n");                                                              \
  })

#endif
