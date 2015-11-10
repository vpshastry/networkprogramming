#include "header.h"

int
msg_send(int, char *, int, char *, int)
{
  return 0;
}

int
msg_recv(int, char *, char *, int *)
{
  return 0;
}

struct vminfo_t *
get_vminfo(int vmno)
{
  return &vminfos[vmno];
}

int
cleanup_sock_file(char *sockfile)
{
  if (unlink(sockfile) < 0) {
    fprintf (stderr, "unlink on sock file(%s) failed: %s\n",
              ctx.sockfile, strerror(errno));
    return -1;
  }
  return 0;
}
