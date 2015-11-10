#ifndef _HEADER_H_
#define _HEADER_H_

#include "unp.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include "unpifiplus.h"
#include <pthread.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#define SERVER_SUNPATH "~/workspace/var/serversunpath.sock"
#define SOCK_DIR "~/workspace/var/"
#define MKOSTEMP_SFX "XXXXXX"

#define SERVER_PORT 51476

#define MYID 1

// 3 * 4 + 3 + 5 Extra
#define MAX_IP_LEN 20

#define DEBUG 1

#define MYID 1

typedef unsigned int uint32;
typedef unsigned char uint8;
typedef unsigned short uint16;

typedef struct {
  char ip[MAX_IP_LEN];
  int port;
} vminfo_t;

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
struct vminfo_t * get_vminfo(int vmno);
int cleanup_sock_file(char *sockfile);

typedef struct {
  char sockfile[PATH_MAX];
  int sockfd;
  vminfo_t *vminfos;
} ctx_t;
#endif
