#include "header.h"

int
msg_send(int sock, char *ip, int port, char *buffer, int reroute)
{
  return 0;
}

int
msg_recv(int sock, char *buffer, char *srcip, int *srcport)
{
  return 0;
}

vminfo_t *
get_vminfo(ctx_t *ctx, int vmno)
{
  return &ctx->vminfos[vmno];
}

int
cleanup_sock_file(char *sockfile)
{
  if (unlink(sockfile) < 0) {
    fprintf (stderr, "unlink on sock file(%s) failed: %s\n",
              sockfile, strerror(errno));
    return -1;
  }
  return 0;
}
