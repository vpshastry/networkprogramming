#include "header.h"

typedef struct {
  int portnumber;
  int maxslidewindowsize;
} input_t;

typedef struct {
  int sockfd;
  struct sockaddr_in ip;
  struct sockaddr_in netmask;
  struct sockaddr_in subnet;
} interface_info_t;

int
readargsfromfile(input_t *input)
{
  FILE *fd;

  if (!(fd = fopen("server.in", "r"))) {
    ERR(NULL, "Opening file failed: %s\n", strerror(errno));
    return -1;
  }


  if ((input->portnumber = readuintarg((int)fd)) == -1) {
    ERR(NULL, "Failed reading port number\n");
    return -1;
  }

  if ((input->maxslidewindowsize = readuintarg((int)fd)) == -1) {
    ERR(NULL, "Failed reading max slide window size\n");
    return -1;
  }

  return 0;
}

/*
int
mod_get_ifi_info(interface_info_t *ii)
{
  const int on = 1;
  pid_t	pid;
  int curi = -1;
  struct ifi_info *ifi;
  struct ifi_info *ifihead;
  struct sockaddr_in *sa, cliaddr, wildaddr;

  for (ifihead = ifi = Get_ifi_info(AF_INET, 1);
      ifi != NULL; ifi = ifi->next) {

    ii[++curi].sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

    Setsockopt(ii[curi].sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    sa = (struct sockaddr_in *) ifi->ifi_addr;
    memcpy (ii[curi].ip, ifi->ifi_addr, sizeof(struct sockaddr_in));
    sa->sin_family = AF_INET;
    sa->sin_port = htons(SERV_PORT);
    Bind(ii[curi].sockfd, (SA *) sa, sizeof(*sa));
    printf("bound %s\n", Sock_ntop((SA *) sa, sizeof(*sa)));
  }

  return curi;
}

int
listen(interface_info_t *ii, int length)
{
  int i = 0;
  fd_set rset;
  int maxfd = -100;

  FD_ZERO(rset);
  for (i = 0; i <= length; i++) {
    FD_SET(ii[i].sockfd, &rset);

    if (ii[i].sockfd > maxfd)
      maxfd = ii[i].sockfd;
  }

  // TODO: Listen.
  if (select(maxfd +1, &rset, NULL, NULL, NULL) < 0) {
  }
}
*/

int
main(int argc, char *argv[]) {
  input_t input;
  interface_info_t ii[MAX_INTERFACE_INFO] = {0,};

  if (argc > 0) {
    ERR(NULL, "Usage: %s\n", argv[0]);
    return 0;
  }

  if (readargsfromfile(&input) == -1) {
    ERR(NULL, "Read/parse error from the input file\n");
    return -1;
  }

  /*
  if (mod_get_ifi_info(ii) < 0) {
    ERR(NULL, "Failed to get ifi infos\n");
    return -1;
  }
  */
}
