#ifndef _HEADER_H_
#define _HEADER_H_

#include "unp.h"
#include "hw_addrs.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <setjmp.h>

// Includes for PF_PACKET
//#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

#define MAX_IP_LEN 20

#define YES 0
#define NO 1

typedef struct {
  char node_ip[MAX_IP_LEN];
  int is_cur;
} tour_list_node_t;

#endif
