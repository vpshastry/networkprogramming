#include "unpifiplus.h"

extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern        void      free_ifi_info_plus(struct ifi_info *ifihead);

typedef struct {
  int sockfd;
  struct sockaddr_in *ip;
  struct sockaddr_in *netmask;
  struct in_addr subnet;
} interface_info_t;

void build_inferface_info(interface_info_t *ii, size_t *interface_info_len, int bind);
void print_interface_info(interface_info_t *ii, size_t interface_info_len); 

#define MAX_INTERFACE_INFO 16
