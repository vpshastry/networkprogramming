#include "header.h"

extern char my_ip_addr[MAX_IP_LEN];

int
msg_send(int sockfd, char *ip, int port, char *buffer, int reroute)
{
  struct sockaddr_un odraddr;
  sequence_t seq;

  bzero(&odraddr, sizeof(odraddr));
  odraddr.sun_family = AF_LOCAL;
  strcpy(odraddr.sun_path, ODR_SUNPATH);

  strncpy(seq.ip, ip, MAX_IP_LEN);
  seq.port = port;
  strcpy(seq.buffer, buffer);
  //printf("msg_send msg = %s, in seq is %s\n", buffer, seq.buffer);
  seq.reroute = reroute;
  //printf("msg_send: dest IP = %s\n, Redv_dest_ip=%s\n", seq.ip, ip);
  Sendto(sockfd, (void*) &seq, sizeof(seq), 0, (SA*) &odraddr, sizeof(odraddr));
  return 0;
}

int
msg_recv(int sockfd, char *buffer, char *src_ip, int *src_port)
{
  sequence_t seq;

  Read(sockfd, (void*) &seq, sizeof(seq));

  strncpy(src_ip, seq.ip, MAX_IP_LEN);
  strncpy(buffer, seq.buffer, sizeof(seq.buffer));
  *src_port = seq.port;
  printf("msg_recvd : %s\n", buffer);
  return 0;
}

vminfo_t *
get_vminfo(ctx_t *ctx, int vmno)
{
  return &ctx->vminfos[vmno];
}

int
build_vminfos(struct hwa_info* vminfo)
{
  struct hwa_info	*hwa, *hwahead;
  struct sockaddr	*sa;
  char   *ptr;
  int    i, prflag;
  int vm_count = 0, index;

  printf("\n");

  for (hwahead = hwa = Get_hw_addrs(), vm_count = 0; hwa != NULL; hwa = hwa->hwa_next) {

    if (!strcmp("lo", hwa->if_name)) {
      if (DEBUG)
        fprintf (stdout, "Skipping %s\n", hwa->if_name);
      continue;
    }

    if (!strcmp("eth0", hwa->if_name)) {
      sa = hwa->ip_addr;
      sprintf(my_ip_addr, "%s", Sock_ntop_host(sa, sizeof(*sa)));
      //printf("MY IP = %s\n", my_ip_addr);
      if (DEBUG)
        fprintf (stdout, "Skipping %s\n", hwa->if_name);

      continue;
    }
    printf("%s :%s", hwa->if_name, ((hwa->ip_alias) == IP_ALIAS) ? " (alias)\n" : "\n");
    //index = hwa->if_index; // I may need this later.
    index = vm_count;
    strncpy(vminfo[index].if_name, hwa->if_name, IF_NAME);
    vminfo[index].ip_alias = hwa->ip_alias;

    vminfo[index].ip_addr = (struct sockaddr *) Calloc(1, sizeof(struct sockaddr)); /* Init memory for IP address */

    memcpy(vminfo[index].ip_addr, hwa->ip_addr, sizeof(struct sockaddr));	/* IP address */

    if ( (sa = hwa->ip_addr) != NULL)
      printf("         IP addr = %s\n", Sock_ntop_host(sa, sizeof(*sa)));

    prflag = 0;
    i = 0;
    do {
      if (hwa->if_haddr[i] != '\0') {
        prflag = 1;
        break;
      }
    } while (++i < IF_HADDR);

    if (prflag) {
      printf("         HW addr = ");
      ptr = hwa->if_haddr;
      i = IF_HADDR;
      do {
        printf("%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
      } while (--i > 0);
    }
    memcpy(vminfo[index].if_haddr, hwa->if_haddr, IF_HADDR);
    printf("\n         interface index = %d\n\n", hwa->if_index);
    vminfo[index].if_index = hwa->if_index;
    vm_count++;
  }

  free_hwa_info(hwahead);

  return vm_count;
}

void print_vminfos(struct hwa_info* vminfos, int num_interfaces)
{
  int i, prflag, j = 0;
  char   *ptr;
  struct sockaddr	*sa;
  for (i = 0; i < num_interfaces; i++) {
    if ( (sa = vminfos[i].ip_addr) != NULL) {
      printf("Interface index = %d\t%s\t", vminfos[i].if_index, Sock_ntop_host(sa, sizeof(*sa)), vminfos[i]);
      do {
        if (vminfos[i].if_haddr[j] != '\0') {
          prflag = 1;
          break;
        }
      } while (++j < IF_HADDR);

      if (prflag) {
        printf("\tHW addr = ");
        ptr = vminfos[i].if_haddr;
        j = IF_HADDR;
        do {
          printf("%.2x%s", *ptr++ & 0xff, (j == 1) ? " " : ":");
        } while (--j > 0);
      }
    }
    printf("\n");
  }
}

void
get_vminfo_by_ifindex(struct hwa_info* vminfo, int num_interfaces,
                      int find_index, struct hwa_info* return_vminfo)
{
  int i;

  for (i = 0; i < num_interfaces; i++) {
    if(vminfo[i].if_index == find_index) {
      memcpy((void*)return_vminfo, (void*)&vminfo[i], sizeof(struct hwa_info));
      return;
    }
  }

  printf("ERROR: interface index not found in get_vminfo_by_index(%d)\n",
          find_index);
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

void
send_pf_packet(int s, struct hwa_info vminfo, unsigned char* dest_mac,
                odr_packet_t *odr_packet)
{
  int k = 0, j = 0, prflag;
  char* ptr;
  /*target address*/
  struct sockaddr_ll socket_address;

  /*buffer for ethernet frame*/
  void* buffer = (void*)malloc(ETH_FRAME_LEN);

  /*pointer to ethenet header*/
  unsigned char* etherhead = buffer;

  /*userdata in ethernet frame*/
  odr_packet_t* odr_packet_ptr = buffer + 14;
  memcpy(odr_packet_ptr, odr_packet, sizeof(odr_packet_t));

  /*another pointer to ethernet header*/
  struct ethhdr *eh = (struct ethhdr *)etherhead;

  int send_result = 0;

  /*our MAC address*/
  unsigned char src_mac[6];

  do {
    if (vminfo.if_haddr[j] != '\0') {
      prflag = 1;
      break;
    }
  } while (++j < IF_HADDR);

  if (prflag) {
    ptr = vminfo.if_haddr;
    j = IF_HADDR;
    do {
      src_mac[k] = *ptr++ & 0xff;
      k++;
    } while (--j > 0);
  }

  /*other host MAC address*/
  //unsigned char dest_mac[6] = {0x00, 0x0c, 0x29, 0xd9, 0x08, 0xf6};

  /*prepare sockaddr_ll*/

  /*RAW communication*/
  socket_address.sll_family   = PF_PACKET;
  /*we don't use a protocoll above ethernet layer->just use anything here*/
  socket_address.sll_protocol = htons(USID_PROTO);
  /*index of the network device see full code later how to retrieve it*/
  socket_address.sll_ifindex  = vminfo.if_index;
  /*ARP hardware identifier is ethernet*/
  socket_address.sll_hatype   = ARPHRD_ETHER;

  /*target is another host*/
  socket_address.sll_pkttype  = PACKET_OTHERHOST;

  /*address length*/
  socket_address.sll_halen    = ETH_ALEN;
  /*MAC - begin*/
  socket_address.sll_addr[0]  = dest_mac[0];
  socket_address.sll_addr[1]  = dest_mac[1];
  socket_address.sll_addr[2]  = dest_mac[2];
  socket_address.sll_addr[3]  = dest_mac[3];
  socket_address.sll_addr[4]  = dest_mac[4];
  socket_address.sll_addr[5]  = dest_mac[5];
  /*MAC - end*/
  socket_address.sll_addr[6]  = 0x00;/*not used*/
  socket_address.sll_addr[7]  = 0x00;/*not used*/


  /*set the frame header*/
  memcpy((void*)buffer, (void*)dest_mac, ETH_ALEN);
  memcpy((void*)(buffer+ETH_ALEN), (void*)src_mac, ETH_ALEN);
  eh->h_proto = htons(USID_PROTO);

  /*send the packet*/
  send_result = sendto(s, buffer, ETH_FRAME_LEN, 0, (struct sockaddr*)&socket_address, sizeof(socket_address));
  if (send_result == -1) { printf("PF_PACKET sending error\n");}

  printf("Sent PF_PACKET\n");
}
