#include "header.h"

int
main(int argc, char *argv[])
{
  int clientsock = 0;
  struct sockaddr_in server_addr = {0,};
  char recvbuf [MAX_BUF_SIZE] = {0,};
  char *srvip = argv[1];
  int fd = atoi(argv[2]);
  printf ("fd: %d\n", fd);
  int ret = -1;
  char err_msg[MAX_BUF_SIZE] = {0,};

  printf("Waiting for you\n");
  sleep(15);

  /* TODO : Check it does redirect all the stdout to pipe */
  if (dup2(fd, 1) < 0)
    logit(ERROR, "Dup2 failed");

  if ((clientsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    logit(ERROR, "Coudln't get socket");
    ret = -1;
    goto errBforesockcreation;
  }

  memset (&server_addr, 0, sizeof (struct sockaddr_in));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = SERVER_TIME_PORT;
  if (inet_pton(AF_INET, srvip, &server_addr.sin_addr) <= 0) {
    logit(ERROR, "inet_pton error");
    ret = -1;
    goto sockerror;
  }

  if (connect(clientsock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    logit(ERROR, "Failed to connect to socket");
    ret = -1;
    goto sockerror;
  }

  int n = -1;
  recvbuf[MAX_BUF_SIZE] = 0;
  write(clientsock, PING_MSG, strlen(PING_MSG));
  if (read(clientsock, recvbuf, MAX_BUF_SIZE -1) < 0) {
    logit (ERROR, "Read from the socket failed");
    ret = -1;
    goto sockerror;
  }

  if(fputs(recvbuf, stdout) == EOF)
    logit(ERROR, "Fputs error");

  ret = 0;
sockerror:
  close(clientsock);
errBforesockcreation:
  close(fd);
  return ret;
}
