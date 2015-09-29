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

  if(argc != 2) {
    printf("Usage: %s <ip-of-server>\n", argv[0]);
    ret = 1;
    goto out;
  }

  memset(readbuf, '0',sizeof(readbuf));
  memset(&serv_addr, '0', sizeof(serv_addr));

  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "Could not create socket: %s\n", strerror(errno));
    ret = 1;
    goto out;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(ECHO_PORT);

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

  printf ("Enter the string to send: ");
  fgets (readbuf, 1024, stdin);
  if ((err = write(sockfd, readbuf, strlen(readbuf)+1)) <= 0) {
    fprintf (stderr, "Write failure: %s\n", strerror(errno));
    ret = 1;
    goto out;
  }

  if((err = read(sockfd, readbuf, sizeof(readbuf))) <= 0) {
    fprintf (stderr, "Read failure: %s\n", strerror(errno));
    ret = 1;
    goto out;
  }
  readbuf[err] = '\0';

  printf ("Received: ");
  if (fputs(readbuf, stdout) == EOF) {
    fprintf (stderr, "Fputs error: %s\n", strerror(errno));
    ret = 1;
    goto out;
  }
  printf ("\n");

out:
  if (sockfd != -1)
    close(sockfd);

  return ret;
}
