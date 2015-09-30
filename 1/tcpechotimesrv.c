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
sigpipehandler(int arg)
{
  printf ("SIGPIPE received.\n");
  sigpipe_recvd = 1;
  return;
}

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

  if ((err = read(connfd, readbuf, sizeof(readbuf))) <= 0) {
    fprintf (stderr, "Error reading from socket: %s\n", strerror(errno));
    goto clear;
  }

  FD_SET(connfd, &wset);
  if ((err = select(connfd +1, NULL, &wset, NULL, NULL)) < 0) {
    fprintf (stderr, "Error on select: %s\n", strerror (errno));
    goto clear;
  }

  if ((err = write(connfd, readbuf, sizeof(readbuf))) < 0) {
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
echo_cli_server(void *arg /* Currently NULL */)
{
  int listenfd = 0;
  int connfd = 0;
  fd_set rset;
  int err;
  int i = -1;
  pthread_t clients[MAX_CLIENTS] = {0,};
  pthread_attr_t attr;
  int maxfd = -1;

  FD_ZERO(&rset);
  pthread_attr_init(&attr);

  if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)))
    fprintf (stderr, "Failed to set detachable thread: %s\n", strerror(err));

  if ((listenfd = get_server_sock(ECHO_PORT)) == -1) {
    fprintf (stderr, "Failed to get fd\n");
    return NULL;
  }

  FD_SET(listenfd, &rset);
  while (42) {
    printf ("Waiting on select for clients...\n");
    if ((err = select(listenfd+1, &rset, NULL, NULL, NULL)) < 0) {
      fprintf (stderr, "Error on select: %s\n", strerror (errno));
      goto clear;
    }

    if ((connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) < 0) {
      fprintf (stderr, "Connection accept error: %s\n", strerror(errno));
      goto clear;
    }
    printf ("Accepted client\n");

    if ((err = pthread_create(&clients[++i], &attr,
                              echo_cli_serve_single_client, (void *)connfd))) {
      fprintf (stderr, "Thread creation for echo cli service failed: %s\n",
               strerror(err));
      close(connfd);
    }
    connfd = -1;
  }
clear:
  if (listenfd != -1)
    close(listenfd);
  if (connfd != -1)
    close(connfd);
  return NULL;
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

  FD_ZERO(&wset);
  FD_ZERO(&waitset);
  memset(sendBuff, '0', sizeof(sendBuff));

  FD_SET(connfd, &waitset);
  while (42) {
    if (select(connfd +1, &waitset, NULL, NULL, &tv) != 0) {
      printf ("Closing the connection with client\n");
      close(connfd);
      break;
    }

    ticks = time(NULL);
    snprintf(sendBuff, sizeof(sendBuff), "%.24s\r\n", ctime(&ticks));

    FD_SET(connfd, &wset);
    select(connfd+1, NULL, &wset, NULL, NULL);
    if (write(connfd, sendBuff, strlen(sendBuff)) < 0) {
      fprintf (stderr, "Write failure: %s\n", strerror(errno));
      close(connfd);
      break;
    }
    FD_CLR(connfd, &wset);
  }

  printf ("Closing connection with the client\n");
  close(connfd);
  return NULL;
}

void *
time_cli_server(void *arg /* Currently NULL */)
{
  int listenfd = -1;
  int connfd = -1;
  int err;
  fd_set rset;
  pthread_t clients[MAX_CLIENTS] = {0,};
  int i = -1;
  pthread_attr_t attr;

  FD_ZERO(&rset);
  pthread_attr_init(&attr);

  if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)))
    fprintf (stderr, "Failed to set detachable thread: %s\n", strerror(err));

  if ((listenfd = get_server_sock(TIME_PORT)) == -1) {
    fprintf (stderr, "Failed to get fd\n");
    return NULL;
  }

  FD_SET(listenfd, &rset);
  while(42) {
    printf ("Waiting on select for clients...\n");
    if ((err = select(listenfd+1, &rset, NULL, NULL, NULL)) < 0) {
      fprintf (stderr, "Error on select: %s\n", strerror (errno));
      goto out;
    }

    if ((connfd = accept(listenfd, (struct sockaddr*)NULL, NULL)) < 0) {
      fprintf (stderr, "Connection accept error: %s\n", strerror(errno));
      goto out;
    }

    printf ("Accepted client\n");

    if ((err = pthread_create(&clients[++i], &attr, time_cli_serve_single_client, (void *)connfd))) {
      fprintf (stderr, "Thread creation for time cli service failed: %s\n",
               strerror(err));
      close(connfd);
    }
    connfd = -1;
  }

out:
  if (listenfd != -1)
    close (listenfd);

  if (connfd != -1)
    close (connfd);

  return NULL;
}

int main(int argc, char *argv[])
{
  pthread_attr_t attr;
  pthread_t time_cli = {0,};
  pthread_t echo_cli = {0,};
  int err;

  pthread_attr_init(&attr);

  if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)))
    fprintf (stderr, "Failed to set detachable thread: %s\n", strerror(err));

  signal(SIGPIPE, sigpipehandler);
  signal(SIGINT, siginthandler);

  if ((err = pthread_create(&time_cli, &attr, time_cli_server, NULL)))
    fprintf (stderr, "Thread creation for time cli service failed: %s\n",
             strerror(err));

  if ((err = pthread_create(&echo_cli, &attr, echo_cli_server, NULL)))
    fprintf (stderr, "Thread creation for echo cli service failed: %s\n",
             strerror(err));

  pthread_exit(NULL);
}
