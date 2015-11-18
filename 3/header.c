#include "header.h"

int
msg_send(int sockfd, char *ip, int port, char *buffer, int reroute)
{
  struct sockaddr_un odraddr;
  sequence_t seq;

  bzero(&odraddr, sizeof(odraddr));
  odraddr.sun_family = AF_LOCAL;
  strcpy(odraddr.sun_path, ODR_SUNPATH); 

  strncpy(seq.ip, ip, sizeof(ip));
  seq.port = port;
  strncpy(seq.buffer, buffer, sizeof(buffer));
  seq.reroute = reroute;

  Sendto(sockfd, (void*) &seq, sizeof(seq), 0, (SA*) &odraddr, sizeof(odraddr));
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
build_vminfos(vminfo_t* vminfos)
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

    vminfos[i].if_index = hwa->if_index;
    //memcpy(vminfos[i].hwa_info, &hwa, sizeof(struct hwa_info));
    //memcpy(vminfos[i].ip, &hwa->ip_addr, sizeof(struct sockaddr));
    //memcpy(vminfos[i].hwaddr, hwa->if_haddr,
            //sizeof(vminfos[i].hwaddr));
    strncpy(vminfos[i].hwaddr, hwa->if_haddr, sizeof(hwa->if_haddr));
    strncpy(vminfos[i].ipstr,
            Sock_ntop_host(hwa->ip_addr, sizeof(hwa->ip_addr)),
            sizeof(vminfos[i].ipstr));
    ++i;
  }
  return i;
}

void print_vminfos(vminfo_t* vminfos, int num_interfaces)
{
  int i;
  for (i = 0; i < num_interfaces; i++) {
    printf("VM%d\t%s\n", vminfos[i].if_index, vminfos[i].ipstr);
  }
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

void get_ip_of_vm(int vmno, char* final_ip, int length){
  
  struct hostent *hptr;
  char **pptr;
  char str [INET_ADDRSTRLEN];
  char vmno_str[6];
  sprintf(vmno_str, "vm%d", vmno);
  if ( (hptr = gethostbyname (vmno_str) ) == NULL) {
      err_msg ("gethostbyname error for host: %s: %s",
               vmno_str, hstrerror (h_errno) );
      exit(0);
  }
  //printf ("The VM IP address is ");
  switch (hptr->h_addrtype) {
  case AF_INET:
      pptr = hptr->h_addr_list;
      for (; *pptr != NULL; pptr++) {
          printf("%s\n", Inet_ntop(hptr->h_addrtype, *pptr, str, sizeof (str)));
          strncpy(final_ip, str, length - 1);
          final_ip[length - 1] = '\0';
      }
      break;
  default:
      err_ret ("unknown address type");
      break;
  }
  
  //return final_ip;
}

