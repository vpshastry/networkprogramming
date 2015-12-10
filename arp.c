#include "header.h"

cache_t *gcache = NULL;
extern unsigned long gmy_ip_addr;
extern char gmy_hw_addr[IF_HADDR];
char gbroadcast_hwaddr[IF_HADDR] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

int
create_bind_pf_packet()
{
  int s;

  s = socket(AF_PACKET, SOCK_RAW, htons(USID_PROTO));
  if (s == -1) {
    fprintf (stderr, "PF_PACKET creatation failed");
    exit(0);
  }

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
cache_copy_from_arp(cache_t *c, arp_t arp, int uds_fd)
{
  if (uds_fd != -1) {
    memcpy(c->hwaddr.sll_addr, arp.senderhwaddr, IF_HADDR);
    c->hwaddr.sll_ifindex = 1;//TODO
    c->hwaddr.sll_hatype = arp.hard_type;
  }

  c->IPaddr.sin_family = AF_INET;
  c->IPaddr.sin_addr.s_addr = arp.senderipaddr;

  c->uds_fd = uds_fd;
}

cache_t *
cache_already_exists_ip(unsigned long ip)
{
  cache_t *trav = NULL;

  for (trav = gcache; trav; trav = trav->next)
    if (ip == ((struct sockaddr_in *)&trav->IPaddr)->sin_addr.s_addr)
      return trav;

  return NULL;
}

cache_t *
cache_already_exists_hw(char hwaddr[IF_HADDR], cache_t **save)
{
  cache_t *trav = NULL;

  for (trav = (*save)? (*save)->next: gcache; trav; trav = trav->next)
    if (!memcmp(trav->hwaddr.sll_addr, hwaddr, IF_HADDR)) {
      *save = trav;
      return trav;
    }

  return NULL;
}

cache_t *
cache_get_list_by_hw(char hwaddr[IF_HADDR])
{
  cache_t *cache_entry = NULL;
  cache_t *saveptr = NULL;
  cache_t *newhead = NULL;
  cache_t *tmp = NULL;

  for (; (cache_entry = cache_already_exists_hw(hwaddr, &saveptr));) {
    tmp = Calloc(1, sizeof(cache_t));
    memcpy(tmp, cache_entry, sizeof(cache_t));

    tmp->next = newhead;
    newhead = tmp;
  }

  return newhead;
}

void
update_cache(cache_t *cache_entry, arp_t arp)
{
  cache_copy_from_arp(cache_entry, arp, cache_entry->uds_fd);
}

void
append_to_cache(arp_t arp, int uds_fd)
{
  cache_t *list_head;

  cache_t *newcache = Calloc(1, sizeof(cache_t));

  cache_copy_from_arp(newcache, arp, uds_fd);

  newcache->next = gcache;
  gcache = newcache;
}

void
free_list(cache_t **head)
{
  cache_t *next = NULL;
  cache_t *trav = NULL;

  for (trav = *head; trav; trav = next) {
    next = trav->next;
    free(trav);
  }

  *head = NULL;
}

int
listlen(cache_t *trav)
{
  int i = 0;
  for (i = 0; trav; trav = trav->next, ++i);
  return i;
}

// ------------ END cache management ---------------------------

int
is_it_for_me(arp_t arp, struct hwa_info *vminfo, int ninterfaces)
{
  int i;

  for (i = 0; i < ninterfaces; ++i)
    if ((arp.targetipaddr == ((struct sockaddr_in *)&vminfo[i].ip_addr[0])->sin_addr.s_addr) &&
        !memcmp(arp.targethwaddr, vminfo[i].if_haddr, IF_HADDR))
      return 1;

  return 0;
}

void
print_buffer_t(buffer_t buffer)
{
  printf ("Printing.\n");
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
  printf ("TRACE: Sending reply for the query on %s->%s\n",
          Sock_ntop_host((struct sockaddr *)&cache_entry->IPaddr, sizeof(struct sockaddr)),
          get_mac(cache_entry->hwaddr.sll_addr));

  close(*fd);
  *fd = -1;

  return;
}

void
send_arp_response(buffer_t recvbuffer, int pf_fd, struct hwa_info *vminfo,
                  int ninterfaces)
{
  buffer_t buffer;

  // New fill up.
  memcpy(buffer.arp.senderhwaddr, gmy_hw_addr, IF_HADDR);
  buffer.arp.op = ARP_REPLY;
  memcpy(buffer.ethhead.srchwaddr, gmy_hw_addr, IF_HADDR);

  // Copy everything else.
  memcpy(buffer.ethhead.desthwaddr, recvbuffer.ethhead.srchwaddr, IF_HADDR);
  buffer.ethhead.frame_type = recvbuffer.ethhead.frame_type;

  buffer.arp.hard_type = recvbuffer.arp.hard_type;
  buffer.arp.proto_type = recvbuffer.arp.proto_type;
  buffer.arp.hard_size = recvbuffer.arp.hard_size;
  buffer.arp.proto_size = recvbuffer.arp.proto_size;
  buffer.arp.senderipaddr = recvbuffer.arp.targetipaddr;
  memcpy(buffer.arp.targethwaddr, recvbuffer.arp.senderhwaddr, IF_HADDR);
  buffer.arp.targetipaddr = recvbuffer.arp.senderipaddr;

  send_pf_packet(pf_fd, vminfo, gbroadcast_hwaddr, &buffer);
}

void
send_arp_req(msg_t msg, int accepted_fd, int pf_fd, struct hwa_info *vminfo, int ninterfaces)
{
  buffer_t buffer;

  memcpy(buffer.ethhead.desthwaddr, gbroadcast_hwaddr, IF_HADDR);
  memcpy(buffer.ethhead.srchwaddr, gmy_hw_addr, IF_HADDR);
  buffer.ethhead.frame_type = FRAME_TYPE;

  buffer.arp.hard_type = ETH_TYPE;
  buffer.arp.proto_type = PROTO_TYPE;
  buffer.arp.hard_size = ETH_SIZE;
  buffer.arp.proto_size = PROTO_SIZE;
  buffer.arp.op = ARP_REQUEST;
  memcpy(buffer.arp.senderhwaddr, gmy_hw_addr, IF_HADDR);
  buffer.arp.senderipaddr = gmy_ip_addr;
  buffer.arp.targetipaddr = IP_ADDR(msg.IPaddr);

  send_pf_packet(pf_fd, vminfo, gbroadcast_hwaddr, &buffer);

  append_to_cache(buffer.arp, accepted_fd);
}

void
recv_pf_packet(int pf_fd, struct hwa_info *vminfo, int ninterfaces,
                int accepted_fd)
{
  struct sockaddr_ll socket_address;
  socklen_t sock_len = sizeof(socket_address);
  /*buffer for ethernet frame*/
  buffer_t *buffer = Calloc(1, sizeof(buffer_t));
  int length = 0; /*length of the received frame*/
  arp_t arp;
  eth_header_t eth_head;
  cache_t *cache_entry = NULL;

  length = recvfrom(pf_fd, buffer, ETH_FRAME_LEN, 0, (struct sockaddr*)&socket_address, &sock_len);

  memcpy(&eth_head, &buffer->ethhead, sizeof(eth_header_t));
  memcpy(&arp, &buffer->arp, sizeof(arp_t));

  print_buffer_t(*buffer);

  cache_entry = cache_already_exists_ip(arp.senderipaddr);

  switch(arp.op) {
    case ARP_REQUEST:
      if (is_it_for_me(arp, vminfo, ninterfaces)) {
        if (!cache_entry)
          append_to_cache(arp, -1);
        else
          update_cache(cache_entry, arp);

        send_arp_response(*buffer, pf_fd, vminfo, ninterfaces);

      } else if (cache_entry) {
        update_cache(cache_entry, arp);
      }
      break;

    case ARP_REPLY:
      if (!cache_entry)
        append_to_cache(arp, -1);
      else
        update_cache(cache_entry, arp);

      if (!(cache_entry = cache_already_exists_ip(arp.senderipaddr)))
        err_quit("Something wrong. No cache entry soon after it's insertion.\n");

      send_reply_and_close_conn(cache_entry, arp, &cache_entry->uds_fd);
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

  if ((cache_entry = cache_already_exists_ip(IP_ADDR(msg.IPaddr)))) {
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
  printf("FD's set, now going into select loop....\n");

  while (1) {
    cur_set = org_set;
    accepted_fd = -1;

    Select(maxfdp1, &cur_set, NULL, NULL, NULL);

    if (FD_ISSET(uds_fd, &cur_set)) {
      accepted_fd = Accept(uds_fd, NULL, 0);

      process_client_req(accepted_fd, pf_fd, vminfo, ninterfaces);
    }

    if (FD_ISSET(pf_fd, &cur_set))
      recv_pf_packet(pf_fd, vminfo, ninterfaces, uds_fd);
  }
}

void
init_global_vars()
{
  return;
}

int
main()
{
  struct hwa_info vminfo[50];
  int ninterfaces;

  init_global_vars();

  ninterfaces = build_vminfos(vminfo);

  listen_on_fds(create_bind_pf_packet(), create_listen_sun_path(), vminfo, ninterfaces);

  return 0;
}
