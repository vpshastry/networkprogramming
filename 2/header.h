#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "unp.h"
#include "unpifi.h"

#define MAX_SOCKETS 64
#define MAX_INTERFACE_INFO 16

#define INFO(arg, params ...)         \
  do {                                \
    fprintf (stdout, params);         \
  } while (0)

#define ERR(arg, params ...)         \
  do {                                \
    fprintf (stderr, params);         \
  } while (0)

char * getnextline(int fd);
float readfloatarg(int fd);
int readuintarg(int fd);
char * readipstr(int fd);
struct in_addr * readip(int fd);

// In server.c
int mod_get_ifi_info(int *sockfd);
