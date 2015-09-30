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
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>

#include "header.h"

static volatile int sigpipe_recvd = 0;

void
siginthandler(int arg)
{
  printf ("SIGINT received.\n");
  exit(0);
}

void
set_nonblocking(int listenfd)
{
  int flags;

  if ((flags = fcntl(listenfd, F_GETFL, 0)) < 0) {
    fprintf (stderr, "Error getfl\n");
    return;
  }

  if (fcntl(listenfd, F_SETFL, flags | O_NONBLOCK) < 0)
    fprintf (stderr, "Error setting to NONblock mode\n");
}

int
get_server_sock(int port)
{
  int listenfd = 0;
  struct sockaddr_in serv_addr;
  int one = 1;
  int err;

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  memset(&serv_addr, '0', sizeof(serv_addr));

  if ((err = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one))) != 0)
    fprintf (stderr, "Error setting REUSEADDR: %s\n", strerror(err));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(port);

  set_nonblocking(listenfd);

  if ((err = bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) != 0) {
    fprintf (stderr, "Error binding port and address: %s\n", strerror(errno));
    return -1;
  }

  if ((err = listen(listenfd, 10)) != 0) {
    fprintf (stderr, "Error listening on socket: %s\n", strerror(errno));
    return -1;
  }

  return listenfd;
}

void *
echo_cli_serve_single_client(void *arg)
{
  int connfd = (int)arg;
  fd_set otherrset;
  fd_set wset;
  char readbuf[1025];
  int err;

  FD_ZERO(&otherrset);
  FD_ZERO(&wset);
  memset(readbuf, '0', sizeof(readbuf));

  printf ("Waiting for read on socket\n");
  FD_SET(connfd, &otherrset);
  if ((err = select(connfd +1, &otherrset, NULL, NULL, NULL)) < 0) {
    fprintf (stderr, "Error on select: %s\n", strerror (errno));
    goto clear;
  }

  while ((err = read(connfd, readbuf, sizeof(readbuf))) <= 0 && errno == EAGAIN);
  if (err <= 0) {
    fprintf (stderr, "Error reading from socket: %s\n", strerror(errno));
    goto clear;
  }

  FD_SET(connfd, &wset);
  if ((err = select(connfd +1, NULL, &wset, NULL, NULL)) < 0) {
    fprintf (stderr, "Error on select: %s\n", strerror (errno));
    goto clear;
  }

  while ((err = write(connfd, readbuf, sizeof(readbuf))) < 0 && errno == EAGAIN);
  if (err < 0) {
    fprintf (stderr, "Write failure: %s\n", strerror(errno));
    goto clear;
  }
  printf ("Written client data back to socket\n");

clear:
  FD_CLR(connfd, &wset);
  FD_CLR(connfd, &otherrset);
  close (connfd);
}

void *
time_cli_serve_single_client(void *arg)
{
  int connfd = (int)arg;
  fd_set waitset;
  char sendBuff[1024];
  time_t ticks;
  fd_set wset;
  struct timeval tv = {5, 0};
  int err;

  memset(sendBuff, '0', sizeof(sendBuff));

  while (42) {
    printf ("Waiting for 5 mins\n");

    FD_ZERO(&waitset);
    FD_SET(connfd, &waitset);
    if (select(connfd+1, &waitset, NULL, NULL, &tv) != 0) {
      fprintf (stderr, "Select failed: %s\n", strerror(errno));
      goto out;
    }

    ticks = time(NULL);
    snprintf(sendBuff, sizeof(sendBuff), "%.24s\r\n", ctime(&ticks));

    FD_ZERO(&wset);
    FD_SET(connfd, &wset);
    if (select(connfd+1, NULL, &wset, NULL, &tv) < 0) {
      fprintf (stderr, "Select failed: %s\n", strerror(errno));
      goto out;
    }

    while ((err = write(connfd, sendBuff, strlen(sendBuff))) < 0 && errno == EAGAIN);
    if (err < 0) {
      fprintf (stderr, "Write failure: %s\n", strerror(errno));
      goto out;
    }
    FD_CLR(connfd, &wset);
    FD_CLR(connfd, &waitset);
  }

out:
  printf ("Closing connection with the client\n");
  close(connfd);
  return NULL;
}

void
serve(void *arg /* Currently NULL */)
{
  int connfd = -1;
  int err;
  fd_set rset;
  pthread_t clients[MAX_CLIENTS] = {0,};
  int i = 0;
  int j = 0;
  int curclnt = -1;
  pthread_attr_t attr;
  int maxfd = -1;

  struct {
    int fd;
    void *(*server)(void *);
    int port;
    char *name;
  } services[2] = {
    { .fd = -1, .port = ECHO_PORT, .server = echo_cli_serve_single_client, .name = "ECHO" },
    { .fd = -1, .port = TIME_PORT, .server = time_cli_serve_single_client, .name = "TIME" }
  };

  FD_ZERO(&rset);
  pthread_attr_init(&attr);

  if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)))
    fprintf (stderr, "Failed to set detachable thread: %s\n", strerror(err));

  for (i = 0; i < 2; i++) {
    if ((services[i].fd = get_server_sock(services[i].port)) == -1) {
      fprintf (stderr, "Failed to get fd\n");
      return;
    }
  }

  while (42) {
    maxfd = -1;
    FD_ZERO(&rset);
    for (i = 0; i < 2; i++) {
      FD_SET(services[i].fd, &rset);
      maxfd = (services[i].fd > maxfd)? services[i].fd: maxfd;
    }

    printf ("Waiting on select for clients...\n");
    if ((err = select(maxfd+1, &rset, NULL, NULL, NULL)) < 0) {
      fprintf (stderr, "Error on select: %s\n", strerror (errno));
      goto out;
    }

    for (i = 0; i < 2; i++) {
      if (FD_ISSET(services[i].fd, &rset)) {
        printf ("Client connected to %s service\n", services[i].name);

        while ((connfd = accept(services[i].fd, (struct sockaddr*)NULL, NULL)) < 0 && errno == EAGAIN);
        if (connfd < 0) {
          fprintf (stderr, "Connection accept error: %s\n", strerror(errno));
          break;
        }

        printf ("Accepted client\n");

        if ((err = pthread_create(&clients[++curclnt], &attr, services[i].server, (void *)connfd))) {
          fprintf (stderr, "Thread creation for time cli service failed: %s\n",
                   strerror(err));
          close(connfd);
        }
        connfd = -1;
      }
    }

    for (i = 0; i < 2; i++)
      FD_CLR(services[i].fd, &rset);
  }

out:
  for (i = 0; i < 2; i++)
    if (services[i].fd != -1)
      close(services[i].fd);

  if (connfd != -1)
    close (connfd);

  return;
}

int main(int argc, char *argv[])
{
  signal(SIGINT, siginthandler);

  serve(NULL);

  fflush(stdout);
  sleep(1);
  return 0;
}
