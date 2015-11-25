#include "header.h"

#define DUP_ENTRY 1
#define R_TABLE_UPDATED 2
#define SELF_ORIGIN 3

static ctx_t ctx;
extern char my_ip_addr[MAX_IP_LEN];

unsigned char broadcast_ip[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

struct sockaddr_un my_client_addr; // temp, table will solve this
// Routing table entry- only ODR uses it, it is not common, so it is not in header.h
typedef struct {
  char dest_ip[MAX_IP_LEN];
  char next_hop[IF_HADDR];
  int if_index;
  int num_hops;
  int last_bcast_id_seen;
  // TO-DO (@any) - timestamp field.

  // Bookeeping info
  //int is_valid;
} route_table_t;

route_table_t route_table[20];
int route_table_len = 0;
int my_broadcast_id = 0;

parsecommandline(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf (stdout, "Usage: %s <staleness>\n", argv[0]);
    //exit(0);
    return 0;
  }

  return atoi(argv[1]);
}

// Peer proc table code start..
// ---------------------------------------------------------------------
struct struct_peer_proc_table {
  unsigned int port;
  char sun_path[PATH_MAX];
  unsigned long time_to_live;
  unsigned char ispermanent;
  struct struct_peer_proc_table *next;
};
typedef struct struct_peer_proc_table peer_proc_table_t;
struct queue {
  odr_packet_t *packet;
  struct queue *next;
};
static peer_proc_table_t *pptablehead, *pptabletail;
static struct queue *queue_head;

  odr_packet_t *
get_from_queue(char *ip)
{
  if (!queue_head) {
    printf ("Queue is empty. Blunder.!!\n");
    exit(0);
    return NULL;
  }

  struct queue *trav = queue_head;
  struct queue *prev = queue_head;
  odr_packet_t *ret;

  for (; trav; trav = trav->next) {
    if (strcmp(trav->packet->dest_ip, ip) == 0) {
      ret = trav->packet;
      prev->next = trav->next;

      if (trav == queue_head)
        queue_head = trav->next;

      break;
    }
    prev = trav;
  }

  free(trav);

  printf ("Remove queue: source: %d, dest: %d\n", ret->dest_port,
      ret->source_port);
  return ret;
}

  void
add_to_queue(odr_packet_t *newpckt)
{
  struct queue *newq = Calloc(1, sizeof(struct queue));
  newq->packet = newpckt;
  newq->next = queue_head;
  queue_head = newq;
  printf ("Add queue: source: %d, dest: %d\n", newpckt->dest_port, newpckt->source_port);
}

  int
get_rand_port()
{
  static unsigned char firsttimeonly = 1;
  peer_proc_table_t *trav;
  int rand_num;

  if (firsttimeonly) {
    firsttimeonly = 0;
    srand(time(NULL));
  }

repeat:
  rand_num = (((rand_num = rand() % 65000) < 1025)? rand_num +1025: rand_num);

  for (trav = pptablehead; trav; trav = trav->next)
    if (trav->port == rand_num)
      goto repeat;

  return rand_num;
}

// Currently only the server is well known port (TODO: or isn't it?).
  int
build_peer_process_table()
{
  pptabletail = pptablehead = Calloc(1, sizeof(peer_proc_table_t));

  pptablehead->port = SERVER_PORT;
  memcpy(pptablehead->sun_path, SERVER_SUNPATH, strlen(SERVER_SUNPATH));
  pptablehead->time_to_live = 0;
  pptablehead->ispermanent = 1;
}

  void
purge_if_time_expired()
{
  struct timeval tv;
  peer_proc_table_t *trav = pptablehead->next, *prev = pptablehead;

  gettimeofday(&tv, NULL);

  if (PPTAB_DEBUG)
    printf ("-----\nport: %d\nsunpath: %s\ntimetolive: %ld\ncurtime: %ld\n-----\n",
        prev->port, prev->sun_path, prev->time_to_live, tv.tv_sec);

  for (; trav; trav = trav->next) {
    if (PPTAB_DEBUG)
      printf ("-----\nport: %d\nsunpath: %s\ntimetolive: %ld\ncurtime: %ld\n-----\n",
          trav->port, trav->sun_path, trav->time_to_live, tv.tv_sec);

    if (trav->time_to_live < tv.tv_sec) {
      if (PPTAB_DEBUG)
        printf ("Purging the time expired peer with port(%d), path(%s)\n",
            trav->port, trav->sun_path);

      prev->next = trav->next;
      free(trav);
    } else {
      prev = trav;
    }
  }
}

  int
add_to_peer_process_table(struct sockaddr_un *cliaddr)
{
  int i = 0;
  struct timeval tv;
  unsigned char couldcopy = 0;
  peer_proc_table_t *pptableentry;
  peer_proc_table_t *trav;
  int ephemeral_port;

  gettimeofday(&tv, NULL);

  for (trav = pptablehead; trav; trav = trav->next) {
    if (!strncmp(trav->sun_path, cliaddr->sun_path, strlen(cliaddr->sun_path)+1))
      break;
  }
  if (trav) {
    if (PPTAB_DEBUG)
      printf ("Found existing peer with sun_path(%s)\n", trav->sun_path);

    trav->time_to_live = tv.tv_sec + DEFAULT_TIME_TO_LIVE;
    ephemeral_port = trav->port;
    goto out;
  }

  pptableentry = Calloc(1, sizeof(peer_proc_table_t));
  pptableentry->port = ephemeral_port = get_rand_port();
  pptableentry->ispermanent = 0;
  pptableentry->time_to_live = tv.tv_sec + DEFAULT_TIME_TO_LIVE;
  strncpy(pptableentry->sun_path, cliaddr->sun_path, strlen(cliaddr->sun_path));
  if (PPTAB_DEBUG)
    printf ("Adding new peer with sun_path(%s)\n", pptableentry->sun_path);

  pptabletail->next = pptableentry;
  pptabletail = pptableentry;

out:
  purge_if_time_expired();
  return ephemeral_port;
}

  char *
get_sun_path_from_port(int port)
{
  peer_proc_table_t *trav = pptablehead;

  for (; trav; trav = trav->next)
    if (trav->port == port)
      return trav->sun_path;

  for (trav = pptablehead; trav; trav = trav->next)
    printf ("-----\nPort(%d), path(%s)\n", trav->port, trav->sun_path);
  printf ("No sunpath found for %d. \n Some error. Exiting....\n", port);
  exit(0);
  return NULL;
}

// ---------------------------------------------------------------------
// Peer proc table code end.

int create_bind_odr_sun_path() {
  int sockfd;
  socklen_t len;
  struct sockaddr_un addr;
  sockfd = Socket(AF_LOCAL, SOCK_DGRAM, 0);

  unlink(ODR_SUNPATH);
  bzero(&addr, sizeof(addr));
  addr.sun_family = AF_LOCAL;
  strcpy(addr.sun_path, ODR_SUNPATH);

  Bind(sockfd, (SA*) &addr, sizeof(addr));

  return sockfd;
}

void print_mac_adrr(char mac_addr[6]) {
  int i = 0;
  char* ptr;
  i = IF_HADDR;
  ptr = mac_addr;
  do {
    printf("%.2x:", *ptr++ & 0xff);
  } while (--i > 0);
}

  void
broadcast_to_all_interfaces (int pf_packet_sockfd, struct hwa_info* vminfo,
    int num_interfaces, char dest_ip[MAX_IP_LEN],
    odr_packet_t* fwd_odr_packet)
{
  int i;
  odr_packet_t odr_packet;

  if (fwd_odr_packet == NULL) {
    odr_packet.type = RREQ;
    strcpy(odr_packet.source_ip, my_ip_addr);
    strcpy(odr_packet.dest_ip, dest_ip);
    odr_packet.broadcast_id = ++my_broadcast_id;
    odr_packet.rrep_already_sent = NO;
    odr_packet.force_discovery = 0;
    odr_packet.hop_count = 0;

  } else {
    memcpy(&odr_packet, fwd_odr_packet, sizeof(odr_packet_t));
  }

  for (i = 0; i < num_interfaces; i++)
    send_pf_packet(pf_packet_sockfd, vminfo[i], broadcast_ip, &odr_packet);
}

  int
is_ip_in_route_table (char ip[MAX_IP_LEN], route_table_t* table_entry)
{
  int i;

  for (i = 0; i < route_table_len; i++)
    if (strcmp(route_table[i].dest_ip, ip) == 0) {
      printf("This IP is in routing table\n");
      memset(table_entry, 0, sizeof(route_table_t));

      strcpy(table_entry->dest_ip, route_table[i].dest_ip);
      memcpy((void*)table_entry->next_hop, (void*)route_table[i].next_hop, ETH_ALEN);
      table_entry->if_index = route_table[i].if_index;
      table_entry->num_hops = route_table[i].num_hops;
      table_entry->last_bcast_id_seen = route_table[i].last_bcast_id_seen;

      return YES;
    }

  return NO;
}
void print_routing_table() {
  int i;
  printf("-------------ROUTING TABLE--------------\n");
  //printf("DEST_IP               NEXT_HOP            IF_INDEX\t\t\tNUM_HOPS\t\t\tLAST_BCAST_ID\n");
  for (i = 0; i < route_table_len; i++) {
    printf("%15s", route_table[i].dest_ip);
    printf("%15s", "");
    print_mac_adrr(route_table[i].next_hop);
    //printf("\t");
    printf("%15d", route_table[i].if_index);
    printf("%15d", route_table[i].num_hops);
    printf("%15d\n", route_table[i].last_bcast_id_seen);
  }
}
  void
print_odr_packet(odr_packet_t *odr_packet)
{
  printf("type = %d\n", odr_packet->type);
  printf("source IP = %s\n", odr_packet->source_ip);
  printf("dest IP = %s\n", odr_packet->dest_ip);
  printf("broadcast id = %d\n", odr_packet->broadcast_id);
  printf("hop count = %d\n", odr_packet->hop_count);
}

  int
update_routing_table(odr_packet_t odr_packet, struct sockaddr_ll socket_address, unsigned char src_mac[6])
{
  int i;
  route_table_t table_entry;
  int ret = DUP_ENTRY;

  if (strcmp(odr_packet.source_ip, my_ip_addr) == 0) {
    ret = SELF_ORIGIN;
    goto out;
  }

  // NON-existing dest. Add new entry.
  if (is_ip_in_route_table(odr_packet.source_ip, &table_entry) == NO) {
    memset(&route_table[route_table_len], 0, sizeof(route_table[route_table_len]));
    strcpy(route_table[route_table_len].dest_ip, odr_packet.source_ip);
    memcpy((void*) route_table[route_table_len].next_hop, (void*) src_mac, ETH_ALEN);
    route_table[route_table_len].if_index = socket_address.sll_ifindex;
    route_table[route_table_len].num_hops = odr_packet.hop_count + 1;

    if (odr_packet.type == RREQ)
      route_table[route_table_len].last_bcast_id_seen = odr_packet.broadcast_id;
    route_table_len++;

    printf ("4. Returning R_TABLE_UPDATED\n");
    ret = R_TABLE_UPDATED;
    goto out;
  }

  if (is_ip_in_route_table(odr_packet.source_ip, &table_entry) != YES) {
    fprintf (stderr, "This should never happen. != YES != NO\n");
    exit(0);
  }

  if (table_entry.last_bcast_id_seen == odr_packet.broadcast_id) {
    switch (HOPS_CMP(table_entry.num_hops, odr_packet.hop_count +1)) {
      case FIRST_ARG_LESSER:
        printf ("1. Returning DUB_ENTRY\n");
        ret = DUP_ENTRY;
        goto out;

      case FIRST_ARG_GREATER:
        printf("Lesser hop_count. Update Table\n");
        for (i = 0; i < route_table_len; i++) {
          if (strcmp(route_table[i].dest_ip, table_entry.dest_ip) == 0) {
            memset (&route_table[i], 0, sizeof(route_table[i]));
            memcpy((void*)route_table[i].next_hop, (void*) src_mac, ETH_ALEN);
            route_table[i].if_index = socket_address.sll_ifindex;
            route_table[i].num_hops = odr_packet.hop_count + 1; // +1 for the hop it made to get to me.
            if (odr_packet.type == RREQ)
              route_table[i].last_bcast_id_seen = odr_packet.broadcast_id;
            break;
          }
        }
        printf ("1. Returning R_TABLE_UPDATED\n");
        ret = R_TABLE_UPDATED;
        goto out;

      case FIRST_ARG_EQUAL:
        printf("Same number of hops\n");
        if (strcmp(table_entry.next_hop, src_mac) == 0) {
          //printf("Same neightbour. No update\n.");
          printf ("2. Returning DUP_ENTRY\n");
          ret = DUP_ENTRY;
          goto out;
        } else {
          //printf("Different neighbour, need to update\n"); // TO-DO
          printf ("3. Returning DUP_ENTRY\n");
          ret = DUP_ENTRY; // only for now, TO-DO
          goto out;
        }
        printf ("2. Returning R_TABLE_UPDATED\n");
        ret = R_TABLE_UPDATED;
        goto out;
    }

  } else if (table_entry.last_bcast_id_seen < odr_packet.broadcast_id) {
    if (table_entry.num_hops > (odr_packet.hop_count + 1)) {
      printf("Lesser hop_count. Update Table\n");
      for (i = 0; i < route_table_len; i++) {
        if (strcmp(route_table[i].dest_ip, table_entry.dest_ip) == 0) {
          printf ("Found\n");
          memset (&route_table[i], 0, sizeof(route_table[i]));
          memcpy((void*)route_table[i].next_hop, (void*) src_mac, ETH_ALEN);
          route_table[i].if_index = socket_address.sll_ifindex;
          route_table[i].num_hops = odr_packet.hop_count + 1; // +1 for the hop it made to get to me.
          if (odr_packet.type == RREQ)
            route_table[i].last_bcast_id_seen = odr_packet.broadcast_id;
          break;
        }
      }

    } else {
      for (i = 0; i < route_table_len; i++) {
        if (strcmp(route_table[i].dest_ip, table_entry.dest_ip) == 0) {
          if (odr_packet.type == RREQ)
            route_table[i].last_bcast_id_seen = odr_packet.broadcast_id;
          break;
        }
      }
    }

    printf ("3. Returning R_TABLE_UPDATED\n");
    ret = R_TABLE_UPDATED;
    goto out;
  }

out:
  print_routing_table();
  printf ("Returning %d\n", ret);
  return ret;
}

  int
send_rrep(odr_packet_t *odr_packet, int r_table_update, int pf_packet_sockfd,
    struct hwa_info *vminfo, int num_interfaces, int hops,
    route_table_t *table_entry)
{
  odr_packet_t rrep, app_payload;
  struct hwa_info sending_if_info;
  struct sockaddr_un serveraddr;
  sequence_t seq;

  memset(&rrep, 0, sizeof(rrep));
  rrep.type = RREP;
  strcpy(rrep.source_ip, odr_packet->dest_ip);
  strcpy(rrep.dest_ip, odr_packet->source_ip);
  rrep.dest_port = odr_packet->source_port;
  rrep.source_port = odr_packet->dest_port;
  rrep.hop_count = hops;
  rrep.force_discovery = odr_packet->force_discovery;

  get_vminfo_by_ifindex(vminfo, num_interfaces, table_entry->if_index, &sending_if_info);
  send_pf_packet(pf_packet_sockfd, sending_if_info, table_entry->next_hop, &rrep);
}

  int
handle_rreq(odr_packet_t *odr_packet, struct sockaddr_ll socket_address,
    unsigned char src_mac[6], int pf_packet_sockfd,
    struct hwa_info* vminfo, int num_interfaces,
    int odr_sun_path_sockfd)
{
  socklen_t sock_len = sizeof(socket_address);
  route_table_t table_entry;
  struct hwa_info sending_if_info;
  struct sockaddr_un serveraddr;
  sequence_t seq;
  int r_table_update;

  // Update routing table first.
  r_table_update = update_routing_table(*odr_packet, socket_address, src_mac);

  if (strcmp (odr_packet->dest_ip, my_ip_addr) == 0) {
    printf("This packet is for me. Sending RREP.\n");
    if (r_table_update == DUP_ENTRY) {
      printf("Already handled. IGNORE.\n");
      return 0;
    }

    if (is_ip_in_route_table(odr_packet->source_ip, &table_entry) == NO) {
      printf("Something is seriously wrong, no path in routing table for rrep?\n");
      exit(0);
    }

    // Do we
    send_rrep(odr_packet, r_table_update, pf_packet_sockfd,
        vminfo, num_interfaces, 0, &table_entry);

    // It's not for me.
  } else {
    printf("This rreq is not for me.\n");

    if (r_table_update == DUP_ENTRY || r_table_update == SELF_ORIGIN) {
      printf("Received(%s). IGNORING..\n",
          (r_table_update == DUP_ENTRY)? "DUP_ENTRY": "SELF_ORIGIN");
      return;
    }

    if (is_ip_in_route_table(odr_packet->dest_ip, &table_entry) == NO) {
      printf("Not in routing table... broadcasting.\n");

    } else if (odr_packet->rrep_already_sent == NO) {
      printf("I know path to dest, should send RREP\n");

      send_rrep(odr_packet, r_table_update, pf_packet_sockfd, vminfo,
          num_interfaces, table_entry.num_hops, &table_entry);

      printf("Intermediate RREP sent, Checking if I knew the source node\n");

      printf("This was a new/better path to the src node... continue-ing to flood RREQ with rrep_already_sent = YES;.\n");
      odr_packet->rrep_already_sent = YES;

      // Just an error check.
    } else if (odr_packet->rrep_already_sent != YES) {
      fprintf (stderr, "Blunder!! not = YES is not equal to NO\n");
      exit(0);

    } else {
      printf("rrep is already sent.\nThis was a new/better path to the src node... continue-ing to flood RREQ with rrep_already_sent = YES;.\n");
      //odr_packet->rrep_already_sent = YES;
    }

    odr_packet->hop_count++; // Increment hop_count for the hop it just made to get to me.
    broadcast_to_all_interfaces(pf_packet_sockfd, vminfo, num_interfaces, NULL, odr_packet);
  }
  return 0;
}

  odr_packet_t *
build_app_payload(odr_packet_t *app_payload, char *ip)
{
  odr_packet_t *msgtosend;

  msgtosend = get_from_queue(ip);

  memset(app_payload, 0, sizeof(odr_packet_t));
  app_payload->type = APP_PAYLOAD;
  strcpy(app_payload->source_ip, my_ip_addr);
  strcpy(app_payload->dest_ip, msgtosend->dest_ip);
  app_payload->dest_port = msgtosend->dest_port;
  app_payload->source_port = msgtosend->source_port;
  strcpy(app_payload->app_message, msgtosend->app_message);
  app_payload->app_req_or_rep = AREQ;
  app_payload->force_discovery = msgtosend->force_discovery;

  free(msgtosend);
  return app_payload;
}

  int
handle_rrep(odr_packet_t *odr_packet, struct sockaddr_ll socket_address,
    unsigned char src_mac[6], int pf_packet_sockfd,
    struct hwa_info* vminfo, int num_interfaces,
    int odr_sun_path_sockfd)
{
  socklen_t sock_len = sizeof(socket_address);
  odr_packet_t *send_packet;
  odr_packet_t app_payload;
  route_table_t table_entry;
  struct hwa_info sending_if_info;
  struct sockaddr_un serveraddr;
  sequence_t seq;
  int r_table_update;

  r_table_update = update_routing_table(*odr_packet, socket_address, src_mac);
  if (r_table_update == DUP_ENTRY) {
    printf("RREP Already handled. IGNORE.\n");
    return;
  }

  if (strcmp (odr_packet->dest_ip, my_ip_addr) == 0) {
    printf("This RREP is for me. Check if I should send app_payload now.\n");
    send_packet = build_app_payload(&app_payload, odr_packet->source_ip);

  } else {
    printf("This RREP is not for me.. Routing it after updating Routing table\n");
    odr_packet->hop_count++;// Increment hop_count for the hop it just made to get to me.

    send_packet = odr_packet;
  }

  if (is_ip_in_route_table(send_packet->dest_ip, &table_entry) == NO) {
    printf("Something is seriously wrong, no path in routing table for rrep?\n");
    exit(0);
  }

  get_vminfo_by_ifindex(vminfo, num_interfaces, table_entry.if_index, &sending_if_info);
  send_pf_packet(pf_packet_sockfd, sending_if_info, table_entry.next_hop, send_packet);
  return;
}

  int
handle_app_payload(odr_packet_t *odr_packet, struct sockaddr_ll socket_address,
    unsigned char src_mac[6], int pf_packet_sockfd,
    struct hwa_info* vminfo, int num_interfaces,
    int odr_sun_path_sockfd)
{
  socklen_t sock_len = sizeof(socket_address);
  route_table_t table_entry;
  struct hwa_info sending_if_info;
  struct sockaddr_un serveraddr;
  sequence_t seq;
  int r_table_update;

  printf("Recvd application payload msg\n");
  r_table_update = update_routing_table(*odr_packet, socket_address, src_mac);
  if (r_table_update == DUP_ENTRY) {
    printf("Info Already in routing table. IGNORE.\n");
    // TODO: return??
  }

  if (strcmp (odr_packet->dest_ip, my_ip_addr) == 0) {
    printf("This APP_PAYLOAD MSG is for me.\n");

    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sun_family = AF_LOCAL;
    if (!get_sun_path_from_port(odr_packet->dest_port)) {
      fprintf (stderr, "Some random packet has received to me. %d\n", odr_packet->dest_port);
      exit(0);
    }
    strcpy(serveraddr.sun_path, get_sun_path_from_port(odr_packet->dest_port));
    strncpy(seq.ip, odr_packet->source_ip, MAX_IP_LEN);
    seq.port = odr_packet->source_port;
    seq.reroute = odr_packet->force_discovery;
    strncpy(seq.buffer, odr_packet->app_message, sizeof(seq.buffer));
    //seq.reroute = reroute;

    if (odr_packet->app_req_or_rep == AREQ) {
      printf("Send to server\n");

    } else if (odr_packet->app_req_or_rep == AREP) {
      printf("Send to Client\n");
    }

    Sendto(odr_sun_path_sockfd, (void*) &seq, sizeof(seq), 0, (SA*) &serveraddr, sizeof(serveraddr));
    return;
  }

  printf("Not for me, fwd to next hop.\n");

  odr_packet->hop_count++;

  if (is_ip_in_route_table(odr_packet->dest_ip, &table_entry) == NO) {
    fprintf (stderr, "Blunder!!\n");
    exit(0);
  }

  get_vminfo_by_ifindex(vminfo, num_interfaces, table_entry.if_index, &sending_if_info);
  send_pf_packet(pf_packet_sockfd, sending_if_info, table_entry.next_hop, odr_packet);

  return;
}

  void
recv_pf_packet(int pf_packet_sockfd, struct hwa_info* vminfo,
    int num_interfaces, int odr_sun_path_sockfd)
{
  struct sockaddr_ll socket_address;
  socklen_t sock_len = sizeof(socket_address);
  unsigned char src_mac[6];
  unsigned char dest_mac[6];
  /*buffer for ethernet frame*/
  void* buffer = (void*)malloc(ETH_FRAME_LEN);
  odr_packet_t odr_packet;

  int length = 0; /*length of the received frame*/
  length = recvfrom(pf_packet_sockfd, buffer, ETH_FRAME_LEN, 0, (struct sockaddr*)&socket_address, &sock_len);
  if (length == -1) { printf("PF_PACKET recv error\n");}
  printf("----Recvd PF_PACKET----\n");
  printf("recving if_index = %d\n", socket_address.sll_ifindex);

  memcpy((void*)src_mac, (void *)buffer+ETH_ALEN, ETH_ALEN);
  memcpy((void*)dest_mac, (void *)buffer, ETH_ALEN);

  printf("src_mac =");
  print_mac_adrr(src_mac);
  printf("\n");

  printf("dest_mac =");
  print_mac_adrr(dest_mac);
  printf("\n");

  memcpy (&odr_packet, buffer+14, sizeof(odr_packet_t));
  print_odr_packet(&odr_packet);

  switch (odr_packet.type) {
    case RREQ:
      handle_rreq(&odr_packet, socket_address, src_mac, pf_packet_sockfd,
          vminfo, num_interfaces, odr_sun_path_sockfd);
      break;

    case RREP:
      handle_rrep(&odr_packet, socket_address, src_mac, pf_packet_sockfd,
          vminfo, num_interfaces, odr_sun_path_sockfd);
      break;

    case APP_PAYLOAD:
      handle_app_payload (&odr_packet, socket_address, src_mac,
          pf_packet_sockfd, vminfo, num_interfaces,
          odr_sun_path_sockfd);
      break;

    default:
      fprintf (stderr, "Unknown type: %d\n", odr_packet.type);
      exit(0);
  }
}

  void
process_client_req(sequence_t recvseq, struct hwa_info* vminfo,
    int num_interfaces, int pf_packet_sockfd,
    int odr_sun_path_sockfd)
{
  route_table_t table_entry;
  struct sockaddr_un servaddr;
  struct hwa_info sending_if_info;
  odr_packet_t *odr_packet;
  int ephemeral_port;
  sequence_t seq;

  printf("New Message recvd for IP:%s, now routing...\n", recvseq.ip);

  ephemeral_port = add_to_peer_process_table(&my_client_addr);

  // If it's local send it here it self.
  if (strcmp(recvseq.ip, my_ip_addr) == 0) {
    bzero(&servaddr, sizeof(servaddr));
    seq.port = ephemeral_port;
    seq.reroute = recvseq.reroute;
    strcpy(seq.ip, my_ip_addr);
    strncpy(seq.buffer, recvseq.buffer, sizeof(seq.buffer));
    //seq.reroute = reroute;

    servaddr.sun_family = AF_LOCAL;
    if (!get_sun_path_from_port(recvseq.port)) {
      fprintf (stderr, "Some random packet has received to me. %d\n", recvseq.port);
      exit(0);
    }
    strcpy(servaddr.sun_path, get_sun_path_from_port(recvseq.port));

    Sendto(odr_sun_path_sockfd, (void*)&seq, sizeof(seq), 0, (SA*)&servaddr, sizeof(servaddr));
    return;
  }

  odr_packet = Calloc(1, sizeof(odr_packet_t));
  odr_packet->type = APP_PAYLOAD;
  strcpy(odr_packet->dest_ip, recvseq.ip);
  strcpy(odr_packet->source_ip, my_ip_addr);
  odr_packet->dest_port = recvseq.port;
  odr_packet->source_port = ephemeral_port;
  odr_packet->hop_count = 0;
  strcpy(odr_packet->app_message, recvseq.buffer);
  odr_packet->app_req_or_rep = (strcmp(my_client_addr.sun_path, SERVER_SUNPATH) == 0)? AREP: AREQ;
  odr_packet->force_discovery = recvseq.reroute;

  printf("New Message recvd for IP:%s, now routing...\n", recvseq.ip);

  if (is_ip_in_route_table(recvseq.ip, &table_entry) == NO) {
    printf("IP not in routing table, broadcasting to all interfaces...\n");
    broadcast_to_all_interfaces(pf_packet_sockfd, vminfo, num_interfaces, recvseq.ip, NULL);
    add_to_queue(odr_packet);

  } else {
    get_vminfo_by_ifindex(vminfo, num_interfaces, table_entry.if_index, &sending_if_info);
    send_pf_packet(pf_packet_sockfd, sending_if_info, table_entry.next_hop, odr_packet);
  }
}

int create_bind_pf_packet()
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
main(int argc, char *argv[])
{
  //int             staleness = parsecommandline(argc, argv);
  struct hwa_info vminfo[50];
  int             num_interfaces, odr_sun_path_sockfd, pf_packet_sockfd, n, maxfdp1;
  fd_set cur_set, org_set;
  socklen_t client_addr_len = sizeof(my_client_addr);

  sequence_t recvseq;

  build_peer_process_table();

  num_interfaces = build_vminfos(vminfo);
  printf("----Print VMINFOS----\n");
  print_vminfos(vminfo, num_interfaces);

  odr_sun_path_sockfd = create_bind_odr_sun_path();
  pf_packet_sockfd = create_bind_pf_packet();

  FD_ZERO(&org_set);
  FD_ZERO(&cur_set);
  FD_SET(odr_sun_path_sockfd, &org_set);
  FD_SET(pf_packet_sockfd, &org_set);
  maxfdp1 = ((odr_sun_path_sockfd > pf_packet_sockfd)? odr_sun_path_sockfd : pf_packet_sockfd) + 1;
  printf("FD's set, now going into select loop....\n");

  for (; ;) {
    cur_set = org_set;
    Select(maxfdp1, &cur_set, NULL, NULL, NULL);

    if (FD_ISSET(odr_sun_path_sockfd, &cur_set)) {
      printf("Data recvd from client/Server\n");
      n = recvfrom(odr_sun_path_sockfd, &recvseq, sizeof(recvseq), 0, (SA*) &my_client_addr, &client_addr_len);
      process_client_req(recvseq, vminfo, num_interfaces, pf_packet_sockfd, odr_sun_path_sockfd);
    }

    if (FD_ISSET(pf_packet_sockfd, &cur_set))
      recv_pf_packet(pf_packet_sockfd, vminfo, num_interfaces, odr_sun_path_sockfd);
  }
}
