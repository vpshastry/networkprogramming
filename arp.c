#include "header.h"

cache_t gcache[CACHE_SIZE_GRAN];
int gcur_cache_len = -1;
extern const unsigned long gmy_ip_addr;
extern const char gmy_hw_addr[IF_HADDR];
const char gbroadcast_hwaddr[IF_HADDR] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

int
create_bind_pf_packet()
{
  int s;

  s = Socket(AF_PACKET, SOCK_RAW, htons(USID_PROTO));

  return s;
}

int
create_listen_sun_path()
{
  int sockfd;
  socklen_t len;
  struct sockaddr_un addr;

  sockfd = Socket(AF_UNIX, SOCK_STREAM, 0);

  unlink(ARP_SUNPATH);
  bzero(&addr, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, ARP_SUNPATH);

  Bind(sockfd, (SA*) &addr, sizeof(addr));

  Listen(sockfd, UDS_BACKLOG);

  return sockfd;
}

// ---------------------------------------------------------------
// Cache management
// ---------------------------------------------------------------
void
print_cache()
{
  int i = 0;
  char str[INET_ADDRSTRLEN];

  printf ("Cache entries.\n");
  printf ("IP\t\tHW\n");
  printf ("---------------------------------\n");
  for (i = 0; i <= gcur_cache_len; ++i) {
    printf ("%s", Sock_ntop((SA *)&gcache[i].IPaddr, sizeof(struct sockaddr_in)));
    printf ("\t");
    print_mac_adrr(gcache[i].hwaddr.sll_addr);
    printf ("\n");
  }
  printf ("---------------------------------\n");
}

void
cache_copy_from_args(cache_t *c, char hwaddr[IF_HADDR], int ipaddr,
                      int ifindex, int uds_fd)
{
  if (uds_fd != -1) {
    memcpy(c->hwaddr.sll_addr, hwaddr, IF_HADDR);
    c->hwaddr.sll_ifindex = ifindex;

    c->hwaddr.sll_hatype = ETH_TYPE;
  }

  c->IPaddr.sin_family = AF_INET;

  c->IPaddr.sin_addr.s_addr = ipaddr;

  c->uds_fd = uds_fd;
}

cache_t *
cache_already_exists_ip(unsigned long ip)
{
  int i = 0;

  for (i = 0; i <= gcur_cache_len; ++i)
    if (ip == IP_ADDR(gcache[i].IPaddr))
      return &gcache[i];

  if (TRACE)
    printf ("TRACE: Didn't find IP in cache for %s\n",
              inet_ntoa(*(struct in_addr *)&ip));
  return NULL;
}

void
update_cache(cache_t *c, arp_t arp)
{
  cache_copy_from_args(c, arp.senderhwaddr, arp.senderipaddr, 1/* TODO */, c->uds_fd);

  if (TRACE) print_cache();
}

void
append_to_cache(char hwaddr[IF_HADDR], unsigned long ip, int ifidx, int uds_fd)
{
  if (gcur_cache_len > CACHE_SIZE_GRAN || gcur_cache_len < 0)
    err_quit("So many IP addreses for 10 vms or problem with gcur_cachelen\n");

  cache_t *c = &gcache[++gcur_cache_len];
  bzero(c, sizeof(cache_t));

  cache_copy_from_args(c, hwaddr, ip, ifidx, uds_fd);

  if (TRACE) print_cache();
}

void
init_cache(struct hwa_info *vminfo, int ninterfaces)
{
  int i;
  char str[INET_ADDRSTRLEN];
  cache_t *c = NULL;

  for (i = 0; i < ninterfaces; ++i) {
    c = &gcache[++gcur_cache_len];
    bzero(c, sizeof(cache_t));

    cache_copy_from_args(c, vminfo[i].if_haddr,
                          ((struct sockaddr_in *)vminfo[i].ip_addr)->sin_addr.s_addr,
                          vminfo[i].if_index, 0);
  }

  if (TRACE) print_cache();
}

// ------------ END cache management ---------------------------

int
is_it_for_me(arp_t arp, struct hwa_info *vminfo, int ninterfaces)
{
  int i;

  for (i = 0; i < ninterfaces; ++i)
    if ((unsigned long)arp.targetipaddr == (unsigned long)((struct sockaddr_in *)vminfo[i].ip_addr)->sin_addr.s_addr)
      return 1;

  return 0;
}

void
print_arp(arp_t arp)
{
  if (arp.op == ARP_REQUEST) {
    printf ("TRACE: ARP REQUEST for MAC with\n\tHard type: %d\n\tProt type: %d\n"
	    "Op: %s\nSender HWaddr: ", arp.hard_type, arp.proto_type,
	    (arp.op == ARP_REQUEST)? "ARP_REQUEST": "ARP REPLY");
    print_mac_adrr(arp.senderhwaddr);
    printf ("\n\tSender IP: %s\n\tTarget IP: %s\n\t",
		    inet_ntoa(*(struct in_addr *)&arp.senderipaddr),
		    inet_ntoa(*(struct in_addr *)&arp.targetipaddr));

  } else {
    printf ("TRACE: ARP RESPONSE with\n\tHard type: %d\n\tProt type: %d\n\t"
	    "Op: %s\n\tSender HWaddr: ", arp.hard_type, arp.proto_type,
	    (arp.op == ARP_REQUEST)? "ARP_REQUEST": "ARP REPLY");
    print_mac_adrr(arp.senderhwaddr);
    printf ("\n\tSender IP: %s\n\tTarget HWaddr: ",
		    inet_ntoa(*(struct in_addr *)&arp.senderipaddr));
    print_mac_adrr(arp.targethwaddr);
    printf ("\n\tTarget IP: %s\n", 
		    inet_ntoa(*(struct in_addr *)&arp.targetipaddr));
  }
}


void
send_reply_and_close_conn(cache_t *cache_entry, int *fd)
{
  msg_t msg;
  hwaddr_t *hwaddrs = NULL;

  memcpy(&msg.IPaddr, &cache_entry->IPaddr, sizeof(struct sockaddr));
  msg.sockaddrlen = sizeof(struct sockaddr);
  memcpy(&msg.hwaddr, &cache_entry->hwaddr, sizeof(hwaddr_t));

  Write(*fd, &msg, sizeof(msg_t));
  printf ("TRACE: Sending reply for the query on %s -> ",
          Sock_ntop_host((struct sockaddr *)&msg.IPaddr, sizeof(struct sockaddr)));
  print_mac_adrr(msg.hwaddr.sll_addr);
  printf ("\n");

  close(*fd);
  *fd = -1;

  return;
}

void
send_arp_response(arp_t recvarp, int pf_fd, struct hwa_info *vminfo,
                  int ninterfaces)
{
  arp_t arp;
  struct hwa_info sending_vminfo;

  bzero(&arp, sizeof(arp_t));

  // New fill up.
  memcpy(arp.senderhwaddr, gmy_hw_addr, IF_HADDR);
  arp.op = ARP_REPLY;

  // Copy everything else.
  arp.hard_type = recvarp.hard_type;
  arp.proto_type = recvarp.proto_type;
  arp.hard_size = recvarp.hard_size;
  arp.proto_size = recvarp.proto_size;
  arp.senderipaddr = recvarp.targetipaddr;
  memcpy(arp.targethwaddr, recvarp.senderhwaddr, IF_HADDR);
  arp.targetipaddr = recvarp.senderipaddr;

  print_arp(arp);

  get_ifnametovminfo(vminfo, ninterfaces, INTERESTED_IF, &sending_vminfo);
  send_pf_packet(pf_fd, sending_vminfo, recvarp.senderhwaddr, arp);
}

void
send_arp_req(msg_t msg, int accepted_fd, int pf_fd, struct hwa_info *vminfo, int ninterfaces)
{
  arp_t arp;
  struct hwa_info sending_vminfo;

  bzero(&arp, sizeof(arp_t));

  arp.hard_type = ETH_TYPE;
  arp.proto_type = PROTO_TYPE;
  arp.hard_size = ETH_SIZE;
  arp.proto_size = PROTO_SIZE;
  arp.op = ARP_REQUEST;
  memcpy(arp.senderhwaddr, gmy_hw_addr, IF_HADDR);
  arp.senderipaddr = gmy_ip_addr;
  arp.targetipaddr = IP_ADDR(msg.IPaddr);

  print_arp(arp);

  get_ifnametovminfo(vminfo, ninterfaces, INTERESTED_IF, &sending_vminfo);
  send_pf_packet(pf_fd, sending_vminfo, gbroadcast_hwaddr, arp);

  append_to_cache(arp.targethwaddr, arp.targetipaddr,
                  sending_vminfo.if_index, accepted_fd);
}

void
recv_pf_packet(int pf_fd, struct hwa_info *vminfo, int ninterfaces,
                int accepted_fd)
{
  struct sockaddr_ll socket_address;
  socklen_t sock_len = sizeof(socket_address);
  /*buffer for ethernet frame*/
  char buffer[ETH_FRAME_LEN];
  int length = 0; /*length of the received frame*/
  arp_t arp;
  cache_t *cache_entry = NULL;

  length = recvfrom(pf_fd, buffer, ETH_FRAME_LEN, 0, (struct sockaddr*)&socket_address, &sock_len);

  memcpy(&arp, buffer+14, sizeof(arp_t));

  print_arp(arp);

  cache_entry = cache_already_exists_ip(arp.senderipaddr);

  switch(arp.op) {
    case ARP_REQUEST:
      if (is_it_for_me(arp, vminfo, ninterfaces)) {
        if (!cache_entry)
          append_to_cache(arp.senderhwaddr, arp.senderipaddr,
                          socket_address.sll_ifindex, 0);
        else
          update_cache(cache_entry, arp);

        send_arp_response(arp, pf_fd, vminfo, ninterfaces);

      } else if (cache_entry) {
        update_cache(cache_entry, arp);
      }
      break;

    case ARP_REPLY:
      if (!cache_entry)
        err_quit("This should not be the case.\n");

      update_cache(cache_entry, arp);

      send_reply_and_close_conn(cache_entry, &cache_entry->uds_fd);
      break;

    default:
      fprintf (stderr, "This is not yet implemented.\n");
      exit(0);
  }
}


void
process_client_req(int accepted_fd, int pf_fd,
                    struct hwa_info *vminfo, int ninterfaces)
{
  msg_t msg = {0,};
  cache_t *cache_entry;

  Read(accepted_fd, &msg, sizeof(msg));

  printf ("TRACE: Accepted a new request for %s\n",
              inet_ntoa(*(struct in_addr *)&IP_ADDR(msg.IPaddr)));

  if ((cache_entry = cache_already_exists_ip(IP_ADDR(msg.IPaddr)))) {
    printf ("TRACE: Found %s in cache. Replying right here.\n");
    send_reply_and_close_conn(cache_entry, &accepted_fd);
    return;
  }

  send_arp_req(msg, accepted_fd, pf_fd, vminfo, ninterfaces);
  return;
}

int
listen_on_fds(int pf_fd, int uds_fd, struct hwa_info *vminfo, int ninterfaces)
{
  fd_set cur_set;
  fd_set org_set;
  int maxfdp1 = 0;
  int accepted_fd = -1;

  FD_ZERO(&org_set);
  FD_ZERO(&cur_set);
  FD_SET(uds_fd, &org_set);
  FD_SET(pf_fd, &org_set);
  maxfdp1 = ((uds_fd > pf_fd)? uds_fd : pf_fd) + 1;
  printf("FD's set, now going into select loop....\n\n");

  while (1) {
    printf ("\n");

    cur_set = org_set;
    accepted_fd = -1;

    Select(maxfdp1, &cur_set, NULL, NULL, NULL);

    if (FD_ISSET(uds_fd, &cur_set)) {
      accepted_fd = Accept(uds_fd, NULL, 0);
      process_client_req(accepted_fd, pf_fd, vminfo, ninterfaces);
    }

    if (FD_ISSET(pf_fd, &cur_set))
      recv_pf_packet(pf_fd, vminfo, ninterfaces, uds_fd);

    printf ("\n");
  }
}

void
init_global_vars()
{
  if (ETH_FRAME_LEN < (14 +sizeof(arp_t)))
    err_quit("ARP is bigger than ETH_FRAME_LEN\n");

  if (TRACE) {
    printf ("TRACE: my ip addr: %s, my hwaddr: ",
              inet_ntoa(*(struct in_addr *)&gmy_ip_addr));
    print_mac_adrr(gmy_hw_addr);
    printf ("\n");
  }

  return;
}

int
main()
{
  struct hwa_info vminfo[50];
  int ninterfaces;

  ninterfaces = build_vminfos(vminfo);

  init_global_vars();
  init_cache(vminfo, ninterfaces);

  listen_on_fds(create_bind_pf_packet(), create_listen_sun_path(), vminfo, ninterfaces);

  return 0;
}
