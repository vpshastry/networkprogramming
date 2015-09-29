#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h>

#include "header.h"

int main(int argc, char *argv[])
{
  int sockfd = -1;
  int n = 0;
  char readbuf[1024];
  struct sockaddr_in serv_addr;
  int ret = 0;
  int err = 0;

  if(argc != 3) {
    printf("Usage: %s <ip-of-server> <pipe>\n", argv[0]);
    ret = 1;
    goto out;
  }

  int pipefd = atoi(argv[2]);
  if (dup2(pipefd, 2) < 0)
    fprintf (stderr, "dup2 failed: %s\n", strerror(errno));

  memset(readbuf, '0',sizeof(readbuf));
  memset(&serv_addr, '0', sizeof(serv_addr));

  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "Could not create socket: %s\n", strerror(errno));
    ret = 1;
    goto out;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(TIME_PORT);

  if((err = inet_pton(AF_INET, argv[1], &serv_addr.sin_addr)) <= 0) {
    fprintf(stderr, "Error inet_pton\n");
    ret = 1;
    goto out;
  }

  printf ("Waiting on connect\n");
  if((err = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)))
            < 0) {
    fprintf(stderr, "Connect Failed: %s\n", strerror(errno));
    ret = 1;
    goto out;
  }

  printf ("Waiting on read\n");
  while ((n = read(sockfd, readbuf, sizeof(readbuf)-1)) > 0) {
    readbuf[n] = 0;
    if (fputs(readbuf, stdout) == EOF) {
      fprintf (stderr, "Fputs error: %s\n", strerror(errno));
      ret = 1;
      goto out;
    }
  }
  if (n < 0) {
    fprintf(stderr, "Read error: %s\n", strerror(errno));
  }

out:
  if (sockfd != -1)
    close(sockfd);

  sleep(2);
  return ret;
}
