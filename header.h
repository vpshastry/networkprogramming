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
#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)

#define MAX_IP_LEN 20

// Let YES be 1 and NO be 0 as these represent logical yes no as well.
// For unix style success and failure we can define SUCCESS = 0 and FAILURE = -1/1
#define YES 1
#define NO 0

#define SUCCESS 0
#define FAILURE -1

#define USID_PROTO 147

#define ETH_HDRLEN 14  // Ethernet header length
#define IP4_HDRLEN 20  // IPv4 header length
#define ICMP_HDRLEN 8  // ICMP header length for echo request, excludes data

typedef struct {
  char node_ip[MAX_IP_LEN];
  int is_cur;
} tour_list_node_t;

#endif
