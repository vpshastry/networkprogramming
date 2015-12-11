#include "header.h"

unsigned long gmy_ip_addr;
char gmy_hw_addr[IF_HADDR];
static sigjmp_buf waitbuf;

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

char *
get_mac(char mac[IF_HADDR])
{
  int prflag = 0;
  int i = 0;
  char *ptr;
  char tmp[10] = {0,};
  static char macstr[30];
  int curlen =0;

  do {
    if (mac[i] != '\0') {
      prflag = 1;
      break;
    }
  } while (++i < IF_HADDR);

  if (prflag) {
    memset(macstr, 0, sizeof(macstr));

    ptr = mac;
    i = IF_HADDR;
    do {
      memset(tmp, 0, sizeof(tmp));
      sprintf(tmp, "%.2x%s", *ptr++ & 0xff, (i == 1) ? " " : ":");
      strncat(macstr, tmp, sizeof(macstr)-curlen-1);
      curlen += strlen(tmp);
    } while (--i > 0);
  }

  return macstr;
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
areq (struct sockaddr *IPaddr, socklen_t sockaddrlen, struct hwaddr *HWaddr)
{
  int uds_fd = create_connect_sun_path();
  struct hwaddr hwaddr;
  msg_t msg;
  struct sigaction sa;
  static int firsttimeonly = 1;
  char ipstr[MAX_IP_LEN];

  snprintf(ipstr, MAX_IP_LEN, "%s", Sock_ntop_host(IPaddr, sizeof(*IPaddr)));

  if (firsttimeonly) {
    firsttimeonly = 0;
    /* Install timer_handler as the signal handler for SIGALRM. */
    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = &sig_alrm;
    sigaction (SIGALRM, &sa, NULL);
  }

  memset(&msg, 0, sizeof(msg));
  memcpy(&msg.IPaddr, IPaddr, sizeof(msg.IPaddr));
  msg.sockaddrlen = sockaddrlen;
  msg.hwaddr.sll_ifindex = if_nametoindex(INTERESTED_IF);
  msg.hwaddr.sll_hatype = ETH_TYPE;
  msg.hwaddr.sll_halen = IF_HADDR;

  Write(uds_fd, &msg, sizeof(msg));

  printf("TRACE: Requesting hardware address for %s\n", ipstr);

  mysetitimer(AREQ_TIMEOUT);

  if (sigsetjmp(waitbuf, 1) != 0) {
    fprintf (stderr, "TRACE: Timeout on request hardware address for %s\n", ipstr);
    return -1;
  }

  memset(&msg, 0, sizeof(msg));
  Read(uds_fd, &hwaddr, sizeof(hwaddr));
  mysetitimer(0);

  printf("TRACE: Received hardware address(%s) for %s\n", get_mac(msg.hwaddr.sll_addr), ipstr);

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
      sa = hwa->ip_addr;
      gmy_ip_addr = ((struct sockaddr_in *)sa)->sin_addr.s_addr;
      //snprintf(gmy_ip_addr, MAX_IP_LEN, "%s", Sock_ntop_host(sa, sizeof(*sa)));
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

void
send_pf_packet(int s, struct hwa_info vminfo, unsigned char* dest_mac,
                buffer_t *buf)
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

  memcpy((void*)buf->ethhead.desthwaddr, (void*)dest_mac, ETH_ALEN);
  memcpy((void*)buf->ethhead.srchwaddr, (void*)src_mac, ETH_ALEN);
  buf->ethhead.frame_type = FRAME_TYPE;
  eh->h_proto = htons(USID_PROTO);

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


  memcpy(buffer, buf, sizeof(buffer_t));

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
print_mac_adrr(char mac_addr[6])
{
  int i = 0;
  char* ptr;
  i = IF_HADDR;
  ptr = mac_addr;
  do {
      printf("%.2x:", *ptr++ & 0xff);
  } while (--i > 0);
}

