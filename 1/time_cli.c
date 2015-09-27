#include "header.h"

int
main(int argc, char *argv[])
{
  int clientsock = 0;
  struct sockaddr_in server_addr = {0,};
  char recvbuf [MAX_BUF_SIZE] = {0,};
  char *srvip = argv[1];
  int fd = atoi(argv[2]);

  /* TODO : Check it does redirect all the stdout to pipe */
  if (dup2(fd, 1) < 0)
    logit(ERROR, "Dup2 failed");

  if ((clientsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    logit(ERROR, "Coudln't get socket");
    return -1;
  }

  memset (&server_addr, 0, sizeof (struct sockaddr_in));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = SERVER_TIME_PORT;
  if (inet_pton(AF_INET, srvip, &server_addr.sin_addr) <= 0) {
    logit(ERROR, "inet_pton error");
    return -1;
  }

  if (connect(clientsock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    logit(ERROR, "Failed to connect to socket");
    return -1;
  }

  int n = -1;
  while ((n = read(clientsock, recvbuf, sizeof(recvbuf)-1)) > 0) {
    recvbuf[n] = 0;
    if(fputs(recvbuf, stdout) == EOF)
      logit(ERROR, "Fputs error");
  }
  close(fd);
}
