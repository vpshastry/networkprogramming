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
#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)
#include <netinet/ip_icmp.h>  // struct icmp, ICMP_ECHO

#define FRAME_TYPE 0x0806
#define PROTO_TYPE 0x0800
#define ETH_TYPE 1
#define ETH_SIZE 6
#define PROTO_SIZE 4

#define INTERESTED_IF "eth0"
#define OUR_ARP_ID 0xBE

#define IP_ADDR(addr) (((struct sockaddr_in *)&(addr))->sin_addr.s_addr)
#define DEBUG 1

#define CACHE_SIZE_GRAN 50

#define MAX_IP_LEN 20

#define USID_PROTO 4383
#define USID_PROTO2 147

#define ARP_SUNPATH "/tmp/ARP_SUNPATH_51476.sock"
#define UDS_BACKLOG 20
#define AREQ_TIMEOUT 5 /* 5 seconds */

// Let YES be 1 and NO be 0 as these represent logical yes no as well.
// For unix style success and failure we can define SUCCESS = 0 and FAILURE = -1/1.
#define YES 1
#define NO 0
#define MUL 9

//#define USID_PROTO 147

#define ETH_HDRLEN 14  // Ethernet header length
#define IP4_HDRLEN 20  // IPv4 header length
#define ICMP_HDRLEN 8  // ICMP header length for echo request, excludes data

// Multicast related, used only by first node of the list, rest will recv
// it as a part of the tour list.
#define MUL_GRP_IP "239.255.1.7"
#define MUL_PORT 40383

#define TRACE 1


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

typedef struct arp {
  unsigned char id;
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

struct tofrom_arp_tour {
  struct sockaddr_in IPaddr;
  socklen_t sockaddrlen;
  hwaddr_t hwaddr;
};
typedef struct tofrom_arp_tour msg_t;

int areq(struct sockaddr *IPaddress, socklen_t sockaddrlen,
          struct hwaddr *HWaddr);
char * get_mac(char mac[IF_HADDR]);
void send_pf_packet(int s, struct hwa_info vminfo,
                    const unsigned char* dest_mac, arp_t arp);
void print_mac_adrr(const char mac_addr[6]);
void copy_mac_addr(char *dst, char src[IF_HADDR]);
#endif
