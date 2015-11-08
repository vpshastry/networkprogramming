#ifndef _HEADER_H_
#define _HEADER_H_

#define SOCK_DIR "~/workspace/var/"
#define MKOSTEMP_SFX "XXXXXX"

#define MYID 1
typedef unsigned int uint32;
typedef unsigned char uint8;
typedef unsigned short uint16;

typedef struct {
  uint32 dummy;
} header_t;

#define PAYLOAD_SIZE (PACKET_SIZE - sizeof(header_t) - sizeof(uint32)) /*lenth*/

typedef struct {
  header_t header;
  unsigned char payload[PAYLOAD_SIZE];
  uint32 length;
} packet_t;

int msg_send(int, char *, int, char *, int);
int msg_recv(int, char *, char *, int *);
#endif
