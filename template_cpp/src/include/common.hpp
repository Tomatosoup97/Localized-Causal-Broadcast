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
#define KEEP_ALIVE 1
#define DUMP_TO_FILE 1
#define PERFECT_LINKS_MODE 1
#define MILLION 1000000

#define IP_MAXPACKET 65535

typedef struct {
  uint32_t id;
  in_addr_t ip;
  unsigned short port;
} node_t;

#endif
