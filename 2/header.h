#ifndef __MYHEADER_H_
#define __MYHEADER_H_

#include "unp.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include "unpifiplus.h"
#include <pthread.h>
#include <limits.h>

#define RTT_DEBUG 1

extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern        void      free_ifi_info_plus(struct ifi_info *ifihead);

typedef struct {
  int sockfd;
  struct sockaddr_in *ip;
  struct sockaddr_in *netmask;
  struct in_addr subnet;
} interface_info_t;

void build_inferface_info(interface_info_t *ii, size_t *interface_info_len, int bind, unsigned int port);
void print_interface_info(interface_info_t *ii, size_t interface_info_len);

#define MAX_INTERFACE_INFO 16
#define MAX_PACKET_SIZE 512 // in bytes

// Tcp header management libraries.
typedef unsigned int uint32;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef struct {
  uint32 seq; // If it's ack, contains seq number of the ack.
  uint32 ts;
  uint8 ack;
  uint8 fin;
  uint32 rwnd;
  uint16 padding_var;
} seq_header_t;
#define FILE_READ_SIZE (MAX_PACKET_SIZE - sizeof(seq_header_t))

typedef struct {
  seq_header_t hdr;
  char payload[FILE_READ_SIZE];
  uint32 length;
} send_buffer_t;
typedef send_buffer_t cli_in_buff_t;

seq_header_t * get_header_from_buff(char *buffer);
char * get_data(char *buffer);
char * prepare_buffer(seq_header_t *header, char *data, int size);
seq_header_t * get_header(uint32 source_port, uint32 dest_port, uint32 seq_nu,
                          uint32 ack_nu, uint8 nack, uint8 ack, uint8 fin);
// End tcp header management.

int dg_send_recv(int fd, int filefd);
void safe_free(void **);

// Locks
typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  unsigned long long turn_head;
  unsigned long long turn_tail;
} fair_lock_t;

void fair_lock(fair_lock_t *fairlock);
void fair_unlock(fair_lock_t *fairlock);
// End locks

#endif
