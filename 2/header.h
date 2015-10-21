#include "unpifiplus.h"

#define INFO(arg, params ...)         \
  do {                                \
    fprintf (stdout, params);         \
  } while (0)

#define ERR(arg, params ...)         \
  do {                                \
    fprintf (stderr, params);         \
  } while (0)

#define MAX_INTERFACE_INFO 16

//char * readipstr(int fd);
//struct in_addr * readip(int fd);

// In server.c
//int mod_get_ifi_info(int *sockfd);
