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
#include <assert.h>

// Includes for PF_PACKET
//#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

#define ODR_SUNPATH "/tmp/odrsunpath_40383.sock"
#define SERVER_SUNPATH "/tmp/serversunpath_40383_aashray.sock"
#define SOCK_DIR "~/workspace/var/"
#define MKOSTEMP_SFX "XXXXXX"
#define TMP_TEMPLATE "/tmp/tmp.XXXXXX"

#define SERVER_PORT 51476

#define MAX_RESEND 1

#define MYID 1

// 3 * 4 + 3 + 5 Extra
#define MAX_IP_LEN 20
#define MAX_HWADDR_LEN IF_HADDR
#define DEFAULT_TIME_TO_LIVE 120

#define PPTAB_DEBUG 1

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

  struct hwa_info h_info;
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

#define RREQ 0
#define RREP 1
#define APP_PAYLOAD 3
#define YES 0
#define NO 1
#define AREQ 0
#define AREP 1

typedef struct {
  int type;
  char source_ip[MAX_IP_LEN];
  char dest_ip[MAX_IP_LEN];
  int source_port;
  int dest_port;
  int broadcast_id;
  int rrep_already_sent;
  int force_discovery;
  int hop_count;
  char app_message[200];
  int app_req_or_rep;
} odr_packet_t;

int msg_send(int, char *, int, char *, int);
int msg_recv(int, char *, char *, int *);
vminfo_t * get_vminfo(ctx_t *, int vmno);
int cleanup_sock_file(char *sockfile);

typedef enum{
  EQUAL_NHOPS = 0,
  NEWONE_IS_BAD = 1,
  NEWONE_IS_BETTER = 2,
} hops_comp_t;
typedef enum {
  SAME_BROADID = 0,
  NEWBROAD_ID_RCVD = 1,
  OLDBROAD_ID_RCVD = 2,
} broad_id_t;

hops_comp_t CMP(int a, int b);

void recv_pf_packet(int pf_packet_sockfd, struct hwa_info* vminfo,
                    int num_interfaces, int odr_sun_path_sockfd);
void send_pf_packet(int s, struct hwa_info vminfo, unsigned char* dest_mac,
                    odr_packet_t *odr_packet);

struct CTX {
  char sockfile[PATH_MAX];
  int sockfd;
  vminfo_t *vminfos;
};

typedef struct {
  char ip[MAX_IP_LEN];
  int port;
  char buffer[200];
  int reroute;
} sequence_t;

char my_ip_addr[MAX_IP_LEN];

#endif
