#ifndef _MY_HEADER_H
#define _MY_HEADER_H
/*
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
*/
#include "unp.h"

#define MAX_BUF_SIZE 1024
#define MAX_ARRAY_SIZE 256

#define SERVER_TIME_PORT 51476
#define SERVER_ECHO_PORT 51477

#define _True 1
#define _False 0

#define MAX_CLIENTS 32

#define SERVER_ADDR "127.0.0.1"

#define PING_MSG "PING"
#define PONG_MSG "PONG"

typedef enum {
  NONE,
  ERROR,
  INFO
} level_t;

void
logit(level_t level, char *msg)
{
  switch (level) {
    case ERROR:
      perror (msg);
      break;
    case NONE:
    case INFO:
    default:
      printf ("%s", msg);
      printf("\n");
  }
}
#endif
