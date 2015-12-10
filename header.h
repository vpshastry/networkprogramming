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

#include <netdb.h>            // struct addrinfo
#include <sys/socket.h>       // needed for socket()
#include <netinet/in.h>       // IPPROTO_UDP, INET_ADDRSTRLEN
#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)
#include <netinet/udp.h>      // struct udphdr
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <sys/ioctl.h>        // macro ioctl is defined
#include <bits/ioctls.h>      // defines values for argument "request" of ioctl.

// Includes for PF_PACKET
//#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

#define FRAME_TYPE 0x0806
#define PROTO_TYPE 0x0800
#define ETH_TYPE 1
#define ETH_SIZE 6
#define PROTO_SIZE 4

#define IP_ADDR(addr) (((struct sockaddr_in *)&(addr))->sin_addr.s_addr)
#define DEBUG 1

#define MAX_IP_LEN 20

#define USID_PROTO 4383

#define ARP_SUNPATH "/tmp/ARP_SUNPATH_51476.sock"
#define UDS_BACKLOG 20
#define AREQ_TIMEOUT 5000000 /* 5 seconds */

// Let YES be 1 and NO be 0 as these represent logical yes no as well.
// For unix style success and failure we can define SUCCESS = 0 and FAILURE = -1/1.
#define YES 1
#define NO 0

#define SUCCESS 0
#define FAILURE -1


typedef struct  hwaddr {
  int sll_ifindex; /* Interface number */
  unsigned short sll_hatype; /* Hardware type */
  unsigned char sll_halen; /* Length of address */
  unsigned char sll_addr[IF_HADDR]; /* Physical layer address */
} __attribute__((__packed__)) hwaddr_t;

typedef struct {
  char node_ip[MAX_IP_LEN];
  int is_cur;
} tour_list_node_t;

struct cache {
  struct cache *next;

  struct sockaddr_in IPaddr;
  hwaddr_t hwaddr;

  int uds_fd;
};
typedef struct cache cache_t;


typedef enum {
  ARP_REQUEST = 1,
  ARP_REPLY = 2,
  RARP_REQUEST = 3,
  RARP_REPLY = 4
} op_t;

typedef struct eth_header {
  char desthwaddr[IF_HADDR];
  char srchwaddr[IF_HADDR];
  unsigned short frame_type;
} __attribute__((__packed__)) eth_header_t;

typedef struct arp {
  unsigned short hard_type;
  unsigned short proto_type;
  unsigned char hard_size;
  unsigned char proto_size;
  unsigned short op;
  char senderhwaddr[IF_HADDR];
  unsigned long senderipaddr;
  char targethwaddr[IF_HADDR];
  unsigned long targetipaddr;
} __attribute__((__packed__)) arp_t;

typedef struct buffer {
  eth_header_t ethhead;
  arp_t arp;
}__attribute__((__packed__)) buffer_t;

struct tofrom_arp_tour {
  struct sockaddr IPaddr;
  socklen_t sockaddrlen;
  hwaddr_t hwaddr;
};
typedef struct tofrom_arp_tour msg_t;

int areq (struct sockaddr *IPaddr, socklen_t sockaddrlen,
          struct hwaddr *HWaddr);
char * get_mac(char mac[IF_HADDR]);
void send_pf_packet(int s, struct hwa_info *vminfo, unsigned char* dest_mac,
                    buffer_t *buffer);
#endif
