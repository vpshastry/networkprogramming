#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include <fcntl.h>

#include "header.h"

int main(int argc, char *argv[])
{
  int listenfd = 0, connfd = 0;
  struct sockaddr_in serv_addr;
  fd_set rset, wset;
  int flags;

  char sendBuff[1025];
  time_t ticks;

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  memset(&serv_addr, '0', sizeof(serv_addr));
  memset(sendBuff, '0', sizeof(sendBuff));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(ECHO_PORT);

  bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

  listen(listenfd, 10);

  if ((flags = fcntl(listenfd, F_GETFL, 0)) < 0) {
    fprintf (stderr, "Error getfl\n");
    return 1;
  }

  if (fcntl(listenfd, F_SETFL, flags | O_NONBLOCK) < 0)
    fprintf (stderr, "Error setfl\n");

  FD_ZERO(&rset);
  FD_ZERO(&wset);
  FD_SET(listenfd, &rset);
  while(1)
  {
    printf ("Waiting on select\n");
    select(listenfd+1, &rset, NULL, NULL, NULL);

    printf ("Waiting on accept\n");
    connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);

    ticks = time(NULL);
    snprintf(sendBuff, sizeof(sendBuff), "%.24s\r\n", ctime(&ticks));

    printf ("Waiting on write\n");
    FD_SET(connfd, &wset);
    select(connfd+1, NULL, &wset, NULL, NULL);
    write(connfd, sendBuff, strlen(sendBuff));

    close(connfd);
    sleep(1);
  }
  return 0;
}
