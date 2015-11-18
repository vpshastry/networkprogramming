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

#define ODR_SUNPATH "/tmp/serversunpath_40383.sock"
#define SOCK_DIR "~/workspace/var/"
#define MKOSTEMP_SFX "XXXXXX"

#define SERVER_PORT 51476

#define MAX_RESEND 1

#define MYID 1

// 3 * 4 + 3 + 5 Extra
#define MAX_IP_LEN 20
#define MAX_HWADDR_LEN IF_HADDR

#define DEBUG 1

#define MYID 1

//#define ETH_FRAME_LEN 1518
#define USID_PROTO 4383

typedef unsigned int uint32;
typedef unsigned char uint8;
typedef unsigned short uint16;

typedef struct CTX ctx_t;

typedef struct {
  int if_index;
  char ipstr[MAX_IP_LEN];
  char hwaddr[MAX_HWADDR_LEN];
  //struct sockaddr ip;

  //struct hwa_info hwa_info;
} vminfo_t;

typedef struct {
  int port;
  char ip[MAX_IP_LEN];
} peerinfo_t;

typedef struct {
  uint32 dummy;
} header_t;

#define PACKET_SIZE 512
#define PAYLOAD_SIZE (PACKET_SIZE - sizeof(header_t) - sizeof(uint32)) /*lenth*/

typedef struct {
  header_t header;
  unsigned char payload[PAYLOAD_SIZE];
  uint32 length;
} packet_t;

int msg_send(int, char *, int, char *, int);
int msg_recv(int, char *, char *, int *);
vminfo_t * get_vminfo(ctx_t *, int vmno);
int cleanup_sock_file(char *sockfile);

struct CTX {
  char sockfile[PATH_MAX];
  int sockfd;
  vminfo_t *vminfos;
};

typedef struct {
  char ip[MAX_IP_LEN];
  int port;
  char buffer[MAXLINE];
  int reroute;
} sequence_t;

#endif
