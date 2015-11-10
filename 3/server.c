#include "header.h"

static ctx_t ctx;

int
do_repeated_task()
{
  char      payload[PAYLOAD_SIZE] = {0,};
  vminfo_t  *vmincontact          = NULL;

  while (42) {
    memset(payload, 0, sizeof(payload));
    if (msg_recv(ctx.sockfd, payload, srcip, srcport) < 0) {
      fprintf (stderr, "Failed to receive message: %s\n", strerror(errno));
      return -1;
    }

    vmincontact = get_vminfo(atoi(payload));

    fprintf (stdout, "Server at node vm%d responding to request from vm%d\n",
              MYID, payload);

    memset (payload, 0, sizeof(payload));
    sprintf (payload, "%d", MYID);
    if (msg_send(ctx.sockfd, vmincontact->ip, vmincontact->port, payload, 0) < 0) {
      fprintf (stderr, "Failed to send message: %s\n", strerror(errno));
      return -1;
    }
  }
}

int
main(int *argc, char *argv[])
{
  int ret = -1;

  if ((ret = do_repeated_task()))
    goto out;

out:
  cleanup_sock_file(ctx.sockfile);
  return ret;
}
