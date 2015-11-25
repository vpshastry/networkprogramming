#include "header.h"

static ctx_t ctx;

int
do_repeated_task(int server_sock_fd)
{
  char        buffer[200];
  peerinfo_t  *peerincontact        = NULL;
  char        src_ip[MAX_IP_LEN];
  int         src_port;
  time_t ticks;

  while (42) {
    //memset(payload, 0, sizeof(payload));
    if (msg_recv(server_sock_fd, buffer, src_ip, &src_port) < 0) {
      fprintf (stderr, "Failed to receive message: %s\n", strerror(errno));
      return -1;
    }
    ticks = time(NULL);
    snprintf(buffer, sizeof(buffer), "%.24s\r\n", ctime(&ticks));
    printf("I will send %s\n", buffer);
    //peerincontact = get_peerinfo(&ctx, atoi(payload));

    /*fprintf (stdout, "Server at node vm%d responding to request from vm%d\n",
              MYID, atoi(payload));*/

    /*memset (payload, 0, sizeof(payload));
    sprintf (payload, "%d", MYID); */
    if (msg_send(server_sock_fd, src_ip, src_port, buffer, 0) < 0) {
      fprintf (stderr, "Failed to send message: %s\n", strerror(errno));
      return -1;
    }
  }
}

int
create_and_bind_socket()
{
  int                 sockfd;
  struct sockaddr_un  servaddr;

  sockfd = Socket (AF_LOCAL, SOCK_DGRAM, 0);

  unlink(SERVER_SUNPATH);
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sun_family = AF_LOCAL;
  strcpy(servaddr.sun_path, SERVER_SUNPATH);

  Bind(sockfd, (SA *) &servaddr, sizeof(servaddr));

  return sockfd; /* TO-DO (@any) - see what pointers to return in arguments, if any.*/
}
int
main(int *argc, char *argv[])
{
  int server_sock_fd;

  server_sock_fd = create_and_bind_socket();
  do_repeated_task(server_sock_fd);
  /*if ((ret = do_repeated_task()))
    goto out;

out:
  cleanup_sock_file(ctx.sockfile);*/
  return 0;
}
