#include "unpifiplus.h"

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

// Tcp header management libraries.
typedef unsigned int uint32;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef struct {
  uint32  source_port;
  uint32  dest_port;
  uint32  seq_nu;
  uint32  ack_nu;
  uint8 nack;
  uint8 ack;
  uint8 fin;
  uint8 padding_var;
  uint32 length;
} seq_header_t;

seq_header_t * get_header(char *buffer);
char * get_data(char *buffer);
char * prepare_buffer(seq_header_t *header, char *data, int size);
seq_header_t * get_header(uint32 source_port, uint32 dest_port, uint32 seq_nu,
                          uint32 ack_nu, uint8 nack, uint8 ack, uint8 fin);
// End tcp header management.
