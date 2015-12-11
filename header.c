#include "header.h"

unsigned long gmy_ip_addr;
char gmy_hw_addr[IF_HADDR];
static sigjmp_buf waitbuf;
int gmy_if_idx;

// ----------------------- AREQ API ---------------------------------------

static void
sig_alrm(int signo)
{
  siglongjmp(waitbuf, 1);
}

int
create_connect_sun_path()
{
  int sockfd;
  socklen_t len;
  struct sockaddr_un addr;

  sockfd = Socket(AF_UNIX, SOCK_STREAM, 0);

  bzero(&addr, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, ARP_SUNPATH);

  Connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));

  return sockfd;
}

/* 1. For TIMEOUT from now, 0 for cancel. */
void
mysetitimer(int waittime /* in micro seconds */)
{
  struct itimerval timer;

  timer.it_value.tv_usec = waittime /1000000;
  timer.it_value.tv_sec = waittime % 1000000;
  timer.it_interval.tv_sec = timer.it_interval.tv_usec = 0;

  setitimer(ITIMER_REAL, &timer, NULL);
}

int
areq (struct sockaddr *IPaddress, socklen_t sockaddrlen, struct hwaddr *HWaddr)
{
  int uds_fd = create_connect_sun_path();
  struct hwaddr hwaddr;
  msg_t msg;
  static int firsttimeonly = 1;
  char ipstr[MAX_IP_LEN];
  struct sockaddr_in *IPaddr = (struct sockaddr_in *)IPaddress;

  snprintf(ipstr, MAX_IP_LEN, "%s", Sock_ntop_host((SA *)IPaddr,
            sizeof(struct sockaddr_in)));

  if (firsttimeonly) {
    firsttimeonly = 0;
    /* Install timer_handler as the signal handler for SIGALRM. */
    Signal(SIGALRM, sig_alrm);
  }

  bzero(&msg, sizeof(msg));
  memcpy(&msg.IPaddr, IPaddr, sockaddrlen);
  msg.sockaddrlen = sockaddrlen;

  Write(uds_fd, &msg, sizeof(msg));

  printf("TRACE: Requesting hardware address for %s\n", ipstr);

  mysetitimer(AREQ_TIMEOUT);

  if (sigsetjmp(waitbuf, 1) != 0) {
    fprintf (stderr, "TRACE: Timeout on request hardware address for %s\n", ipstr);
    return -1;
  }

  bzero(&msg, sizeof(msg));
  Read(uds_fd, &msg, sizeof(msg));
  mysetitimer(0);

  printf("TRACE: Received hardware address(");
  print_mac_adrr(msg.hwaddr.sll_addr);
  printf(") for %s\n", ipstr);

  return 0;
}
//-------------------------- END AREQ API ----------------------------------
//

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

    if (strcmp(INTERESTED_IF, hwa->if_name)) {
      //snprintf(gmy_ip_addr, MAX_IP_LEN, "%s", Sock_ntop_host(sa, sizeof(*sa)));
      //printf("MY IP = %s\n", my_ip_addr);
      if (DEBUG)
        fprintf (stdout, "Skipping %s\n", hwa->if_name);

      continue;
    }

    if (hwa->ip_alias != IP_ALIAS) {
      sa = hwa->ip_addr;
      gmy_ip_addr = ((struct sockaddr_in *)sa)->sin_addr.s_addr;
      memcpy(gmy_hw_addr, hwa->if_haddr, IF_HADDR);
      gmy_if_idx = hwa->if_index;
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

void
send_pf_packet(int s, struct hwa_info vminfo, const unsigned char* dest_mac,
                arp_t arp)
{
  int k = 0, j = 0, prflag;
  char* ptr;
  /*target address*/
  struct sockaddr_ll socket_address;

  /*buffer for ethernet frame*/
  void* buffer = (void*)malloc(ETH_FRAME_LEN);

  /*pointer to ethenet header*/
  unsigned char* etherhead = buffer;

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


  memcpy(buffer +14, &arp, sizeof(arp_t));

  memcpy((void*)buffer, (void*)dest_mac, ETH_ALEN);
  memcpy((void*)(buffer+ETH_ALEN), (void*)src_mac, ETH_ALEN);
  eh->h_proto = htons(USID_PROTO);

  /*set the frame header*/

  //printf("----Sending PF_PACKET----\n");
  //print_pf_packet(socket_address, buffer, *odr_packet);

  /*send the packet*/
  send_result = sendto(s, buffer, ETH_FRAME_LEN, 0, (struct sockaddr*)&socket_address, sizeof(socket_address));
  if (send_result == -1) { printf("PF_PACKET sending error\n");}

  //printf("Sent PF_PACKET\n");
}

void
get_ifnametovminfo(struct hwa_info *vminfo, int ninterfaces, char *find_if,
                    struct hwa_info *return_vminfo)
{
  int i;

  for (i = 0; i < ninterfaces; i++) {
    if(!strncmp(vminfo[i].if_name, find_if, IF_NAME)) {
      memcpy((void*)return_vminfo, (void*)&vminfo[i], sizeof(struct hwa_info));
      return;
    }
  }

  printf("\n\nERROR: interface %s not found\n\n", find_if);
}

void
print_mac_adrr(const char mac_addr[6])
{
  int i = 0;
  const char* ptr;
  i = IF_HADDR;
  ptr = mac_addr;
  do {
      printf("%.2x:", *ptr++ & 0xff);
  } while (--i > 0);
}

