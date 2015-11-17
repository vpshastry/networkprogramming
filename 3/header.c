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
build_vminfos(ctx_t *ctx)
{
  struct hwa_info *hwa;
  struct hwa_info *hwahead;
  int             i = 0;

  for (hwahead = hwa = Get_hw_addrs(), i = 0;
        hwa != NULL;
        hwa = hwa->hwa_next) {

    if (!strcmp("lo", hwa->if_name) || !strcmp("eth0", hwa->if_name)) {
      if (DEBUG)
        fprintf (stdout, "Skipping %s\n", hwa->if_name);
      continue;
    }

    ctx->vminfos[i].if_index = hwa->if_index;
    memcpy(&ctx->vminfos[i].hwa_info, &hwa, sizeof(struct hwa_info));
    memcpy(&ctx->vminfos[i].ip, &hwa->ip_addr, sizeof(struct sockaddr));
    memcpy(ctx->vminfos[i].hwaddr, hwa->if_haddr,
            sizeof(ctx->vminfos[i].hwaddr));
    strncpy(ctx->vminfos[i].ipstr,
            Sock_ntop_host(hwa->ip_addr, sizeof(hwa->ip_addr)),
            sizeof(ctx->vminfos[i].ipstr));
    ++i;
  }
  return i;
}

peerinfo_t *
get_peerinfo(ctx_t *ctx, int no)
{
  return NULL;
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
