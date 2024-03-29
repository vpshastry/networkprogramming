#include "header.h"

#define DUP_ENTRY 1
#define R_TABLE_UPDATED 2
#define SELF_ORIGIN 3

static ctx_t ctx;
extern char my_ip_addr[MAX_IP_LEN];
int staleness;
unsigned char broadcast_ip[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

struct sockaddr_un my_client_addr; // temp, table will solve this

typedef struct {
  char dest_ip[MAX_IP_LEN];
  char source_ip[MAX_IP_LEN];
  int force_discovery;
} fr_t;

// Routing table entry- only ODR uses it, it is not common, so it is not in header.h
typedef struct {
  char dest_ip[MAX_IP_LEN];
  char next_hop[IF_HADDR];
  int if_index;
  int num_hops;
  int last_bcast_id_seen;
  //struct timeval tv;

  // Bookeeping info
  fr_t fr;
  //int is_valid;
} route_table_t;

route_table_t route_table[20];
int route_table_len = 0;
int my_broadcast_id = 0;

int
parsecommandline(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf (stdout, "Usage: %s <staleness>\nCurrently taking default time to live.", argv[0]);
    return DEFAULT_TIME_TO_LIVE;
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
  odr_packet_t packet;
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
  odr_packet_t *ret = Calloc(1, sizeof(odr_packet_t));

  for (; trav; trav = trav->next) {
    if (strcmp(trav->packet.dest_ip, ip) == 0) {
      memcpy(ret, &trav->packet, sizeof(odr_packet_t));
      prev->next = trav->next;

      if (trav == queue_head)
        queue_head = trav->next;

      break;
    }
    prev = trav;
  }

  free(trav);

  //printf ("Remove queue: source: %d, dest: %d\n", ret->dest_port,
                                                  //ret->source_port);
  return ret;
}

void
add_to_queue(odr_packet_t newpckt)
{
  struct queue *newq = Calloc(1, sizeof(struct queue));
  memcpy(&newq->packet, &newpckt, sizeof(odr_packet_t));
  newq->next = queue_head;
  queue_head = newq;
  //printf ("Add queue: source: %d, dest: %d\n", newpckt.dest_port, newpckt.source_port);
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

// Currently only the server is well known port .
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

  /*
  if (PPTAB_DEBUG)
    printf ("TRACE: port: %d, sunpath: %s\n", prev->port, prev->sun_path);
    */

  for (; trav; trav = trav->next) {
    /*
    if (PPTAB_DEBUG)
      printf ("TRACE: port: %d, sunpath: %s\n", trav->port, trav->sun_path);
      */

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
      printf ("Found existing peer with port:%d, sun_path:%s\n",
                trav->port, trav->sun_path);

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

void
broadcast_to_all_interfaces (int pf_packet_sockfd, struct hwa_info* vminfo,
                              int num_interfaces, char dest_ip[MAX_IP_LEN],
                              odr_packet_t* fwd_odr_packet, int fr)
{
  int i;
  odr_packet_t odr_packet;

  if (fwd_odr_packet == NULL) {
    odr_packet.type = RREQ;
    strcpy(odr_packet.source_ip, my_ip_addr);
    strcpy(odr_packet.dest_ip, dest_ip);
    odr_packet.broadcast_id = ++my_broadcast_id;
    odr_packet.rrep_already_sent = NO;
    odr_packet.force_discovery = fr;
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
      //printf("This IP is in routing table\n");
      memset(table_entry, 0, sizeof(route_table_t));

      strcpy(table_entry->dest_ip, route_table[i].dest_ip);
      memcpy((void*)table_entry->next_hop, (void*)route_table[i].next_hop, ETH_ALEN);
      table_entry->if_index = route_table[i].if_index;
      table_entry->num_hops = route_table[i].num_hops;
      table_entry->last_bcast_id_seen = route_table[i].last_bcast_id_seen;
      //memcpy(&table_entry->tv, &route_table[i].tv, sizeof(struct timeval));

      memcpy(&table_entry->fr, &route_table[i].fr, sizeof(fr_t));

      return YES;
    }

  return NO;
}

void
print_routing_table()
{
  int i;
  printf("-------------ROUTING TABLE--------------\n");
  printf("DEST_IP\t\tNEXT_HOP\t\tIF_INDEX    NUM_HOPS    LAST_BCAST_ID\n");
  for (i = 0; i < route_table_len; i++) {
    printf("%s\t", route_table[i].dest_ip);
    printf("%s", "");
    print_mac_adrr(route_table[i].next_hop);
    //printf("\t");
    printf("%10d", route_table[i].if_index);
    printf("%10d", route_table[i].num_hops);
    printf("%10d", route_table[i].last_bcast_id_seen);
    //printf("   %lu, %lu", route_table[i].tv.tv_sec, route_table[i].tv.tv_usec);
    printf("\n");
    //printf ("%5d, %s, %s\n", route_table[i].fr.force_discovery, route_table[i].fr.source_ip, route_table[i].fr.dest_ip);
  }
}

route_table_t *
get_route_table_entry(char *ip)
{
  int i;

  for (i = 0; i < route_table_len; i++)
    if (strcmp(route_table[i].dest_ip, ip) == 0)
      return &route_table[i];

  printf ("This shouldn't happen.\n");
  exit(0);
  return NULL;
}

int
is_same_rediscovery_packet(route_table_t rtabentry, odr_packet_t odr_packet)
{
  if (odr_packet.type == RREP &&
      rtabentry.fr.force_discovery == 1 &&
      !strncmp(rtabentry.fr.dest_ip, odr_packet.source_ip, MAX_IP_LEN) &&
      !strncmp(rtabentry.fr.source_ip, odr_packet.dest_ip, MAX_IP_LEN))
    return 1;

  return 0;
}

int
update_routing_table(odr_packet_t odr_packet,
                      struct sockaddr_ll socket_address,
                      unsigned char src_mac[6])
{
  route_table_t table_entry;
  route_table_t *rtable = NULL;
  int ret = DUP_ENTRY;
  int exists = NO;
  int fr = 0;
  int i = 0;
  struct timeval tv = {0,};

  if (strcmp(odr_packet.source_ip, my_ip_addr) == 0) {
    ret = SELF_ORIGIN;
    goto out;
  }

  Gettimeofday(&tv, NULL);

  exists = is_ip_in_route_table(odr_packet.source_ip, &table_entry);

  // NON-existing dest. Add new entry.
  if (exists == NO ||
      //(exists == YES && timed_out(table_entry.tv, tv, staleness)) ||
      (fr = (exists == YES && odr_packet.force_discovery && !table_entry.fr.force_discovery))) {
    if (!fr)
      rtable = &route_table[route_table_len];
    else
      rtable = get_route_table_entry(odr_packet.source_ip);

    if (!rtable)
      printf ("Blunder @ NULL rtable\n");

    memset(rtable, 0, sizeof(route_table_t));
    strcpy(rtable->dest_ip, odr_packet.source_ip);
    memcpy((void*) rtable->next_hop, (void*) src_mac, ETH_ALEN);
    rtable->if_index = socket_address.sll_ifindex;
    rtable->num_hops = odr_packet.hop_count + 1;
    //memcpy(&rtable->tv, &tv, sizeof(struct timeval));

    if (odr_packet.type == RREQ && odr_packet.force_discovery) {
      rtable->fr.force_discovery = 1;
      memcpy(rtable->fr.source_ip, odr_packet.source_ip, MAX_IP_LEN);
      memcpy(rtable->fr.dest_ip, odr_packet.dest_ip, MAX_IP_LEN);
    }

    if (odr_packet.type == RREQ)
      rtable->last_bcast_id_seen = odr_packet.broadcast_id;

    if (!fr)
      route_table_len++;

    //printf ("Added table entry for %s\n", rtable->dest_ip);
    //print_routing_table();
    ret = R_TABLE_UPDATED;
    //printf ("4. Returning R_TABLE_UPDATED\n");
    goto out;
  }

  if (is_same_rediscovery_packet(table_entry, odr_packet))
    memset (&get_route_table_entry(table_entry.dest_ip)->fr, 0, sizeof(fr_t));

  // Just some integrity check.
  if (is_ip_in_route_table(odr_packet.source_ip, &table_entry) != YES) {
    fprintf (stderr, "This should never happen. != YES != NO\n");
    exit(0);
  }

  // If we're here, the dest ip is there in the table.
  switch (CMP(table_entry.last_bcast_id_seen, odr_packet.broadcast_id)) {
    case SAME_BROADID:
      switch (CMP(table_entry.num_hops, odr_packet.hop_count +1)) {
        case NEWONE_IS_BAD:
          //printf ("1. Returning DUB_ENTRY\n");
          ret = DUP_ENTRY;
          goto out;

        case NEWONE_IS_BETTER:
          printf("Lesser hop_count. Update Table\n");

          rtable = get_route_table_entry(table_entry.dest_ip);

          memcpy((void*)rtable->next_hop, (void*) src_mac, ETH_ALEN);
          rtable->if_index = socket_address.sll_ifindex;
          rtable->num_hops = odr_packet.hop_count + 1; // +1 for the hop it made to get to me.
          //memcpy(&rtable->tv, &tv, sizeof(struct timeval));

          if (odr_packet.type == RREQ)
            rtable->last_bcast_id_seen = odr_packet.broadcast_id;

          printf ("Modified table entry for %s\n", rtable->dest_ip);
          //print_routing_table();

          //printf ("1. Returning R_TABLE_UPDATED\n");
          ret = R_TABLE_UPDATED;
          goto out;

        default:
          printf("Same number of hops\n");
          if (strcmp(table_entry.next_hop, src_mac) == 0) {
            //printf("Same neightbour. No update\n.");
            //printf ("2. Returning DUP_ENTRY\n");
            ret = DUP_ENTRY;
            goto out;
          } else {
            //printf("Different neighbour, need to update\n"); // TO-DO
            //printf ("3. Returning DUP_ENTRY\n");
            ret = DUP_ENTRY; // only for now, TO-DO
            goto out;
          }
          printf ("2. Returning R_TABLE_UPDATED\n");
          ret = R_TABLE_UPDATED;
          goto out;
      }

    case NEWBROAD_ID_RCVD:
      switch (CMP(table_entry.num_hops, (odr_packet.hop_count +1))) {
        case NEWONE_IS_BETTER:
          printf("Lesser hop_count. Update Table\n");
          rtable = get_route_table_entry(table_entry.dest_ip);

          memcpy((void*)rtable->next_hop, (void*) src_mac, ETH_ALEN);
          rtable->if_index = socket_address.sll_ifindex;
          rtable->num_hops = odr_packet.hop_count + 1; // +1 for the hop it made to get to me.
          //memcpy(&rtable->tv, &tv, sizeof(struct timeval));

          if (odr_packet.type == RREQ)
            rtable->last_bcast_id_seen = odr_packet.broadcast_id;

          printf ("Modified table entry bcz of new bid for %s\n", rtable->dest_ip);
          //print_routing_table();

          break;

        default:
          if (odr_packet.type == RREQ) {
            get_route_table_entry(table_entry.dest_ip)->last_bcast_id_seen =
                                                      odr_packet.broadcast_id;
            //memcpy(&rtable->tv, &tv, sizeof(struct timeval));
          }
          break;
      }

      //printf ("3. Returning R_TABLE_UPDATED\n");
      ret = R_TABLE_UPDATED;
      goto out;
  }

out:
  if (ret == R_TABLE_UPDATED) {
    printf ("\nUpdated routing table for %s\n", odr_packet.source_ip);
    print_routing_table();
    printf ("\n");
  }
  //printf ("Returning %d\n", ret);
  return ret;
}

int
send_rrep(odr_packet_t odr_packet, int r_table_update, int pf_packet_sockfd,
          struct hwa_info *vminfo, int num_interfaces, int hops,
          route_table_t *table_entry)
{
  odr_packet_t rrep, app_payload;
  struct hwa_info sending_if_info;
  struct sockaddr_un serveraddr;
  sequence_t seq;

  memset(&rrep, 0, sizeof(rrep));
  rrep.type = RREP;
  strcpy(rrep.source_ip, odr_packet.dest_ip);
  strcpy(rrep.dest_ip, odr_packet.source_ip);
  rrep.dest_port = odr_packet.source_port;
  rrep.source_port = odr_packet.dest_port;
  rrep.hop_count = hops;
  rrep.force_discovery = odr_packet.force_discovery;

  get_vminfo_by_ifindex(vminfo, num_interfaces, table_entry->if_index, &sending_if_info);
  send_pf_packet(pf_packet_sockfd, sending_if_info, table_entry->next_hop, &rrep);
}

int
handle_rreq(odr_packet_t odr_packet, struct sockaddr_ll socket_address,
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

  printf("Recvd RREQ msg with source: %s, dest: %s, bid: %d, hops: %d\n",
          odr_packet.source_ip, odr_packet.dest_ip, odr_packet.broadcast_id,
          odr_packet.hop_count);

  // Update routing table first.
  r_table_update = update_routing_table(odr_packet, socket_address, src_mac);
  if (r_table_update == DUP_ENTRY) {
    printf("Already handled. IGNORE.\n");
    return 0;
  }

  if (strcmp (odr_packet.dest_ip, my_ip_addr) == 0) {
    printf("This packet is for me. Sending RREP.\n");
    if (is_ip_in_route_table(odr_packet.source_ip, &table_entry) == NO) {
      printf("Something is seriously wrong, no path in routing table for rrep?\n");
      exit(0);
    }

    send_rrep(odr_packet, r_table_update, pf_packet_sockfd,
              vminfo, num_interfaces, 0, &table_entry);

  // It's not for me.
  } else {
    printf("This rreq is not for me.\n");

    if (r_table_update == SELF_ORIGIN) {
      printf("Received SELF_ORIGIN RREQ. IGNORING..\n");
      return;
    }

    if (odr_packet.force_discovery)
      goto simply_broadcast;

    if (is_ip_in_route_table(odr_packet.dest_ip, &table_entry) == NO) {
      printf("Not in routing table... broadcasting.\n");

    } else if (odr_packet.rrep_already_sent == NO && !odr_packet.force_discovery) {
      printf("I know path to dest, should send RREP\n");

      send_rrep(odr_packet, r_table_update, pf_packet_sockfd, vminfo,
                num_interfaces, table_entry.num_hops, &table_entry);

      printf("Intermediate RREP sent, Checking if I knew the source node\n");

      printf("This was a new/better path to the src node... continue-ing to flood RREQ with rrep_already_sent = YES;.\n");
      odr_packet.rrep_already_sent = YES;

    } else {
      printf("rrep is already sent.\nThis was a new/better path to the src node... continue-ing to flood RREQ with rrep_already_sent = YES;.\n");
      //TODO: ??odr_packet.rrep_already_sent = YES;
    }

simply_broadcast:
    odr_packet.hop_count++;
    broadcast_to_all_interfaces(pf_packet_sockfd, vminfo, num_interfaces, NULL, &odr_packet, 0);
  }
  return 0;
}

odr_packet_t *
build_app_payload(odr_packet_t *app_payload, char *ip)
{
  odr_packet_t *msgtosend;

  msgtosend = get_from_queue(ip);

  memset(app_payload, 0, sizeof(odr_packet_t));
  memcpy(app_payload, msgtosend, sizeof(odr_packet_t));
  app_payload->app_req_or_rep = AREQ;

  free(msgtosend);
  return app_payload;
}

int
handle_rrep(odr_packet_t odr_packet, struct sockaddr_ll socket_address,
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

  printf("Recvd RREP msg with source: %s, dest: %s, bid: %d, hops: %d\n",
          odr_packet.source_ip, odr_packet.dest_ip, odr_packet.broadcast_id,
          odr_packet.hop_count);

  r_table_update = update_routing_table(odr_packet, socket_address, src_mac);
  if (r_table_update == DUP_ENTRY) {
    printf("RREP Already handled. IGNORE.\n");
    return;
  }

  if (strcmp (odr_packet.dest_ip, my_ip_addr) == 0) {
    printf("This RREP is for me. Check if I should send app_payload now.\n");
    send_packet = build_app_payload(&app_payload, odr_packet.source_ip);

  } else {
    printf("This RREP is not for me.. Routing it after updating Routing table\n");
    odr_packet.hop_count++;// Increment hop_count for the hop it just made to get to me.

    send_packet = &odr_packet;
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
handle_app_payload(odr_packet_t odr_packet, struct sockaddr_ll socket_address,
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

  printf("Recvd APP Payload msg with source: %s, dest: %s\n",
          odr_packet.source_ip, odr_packet.dest_ip);

  /*
  r_table_update = update_routing_table(odr_packet, socket_address, src_mac);
  if (r_table_update == DUP_ENTRY) {
    printf("Info Already in routing table.\n");
    // TODO: return??
  }
  */

  if (strcmp (odr_packet.dest_ip, my_ip_addr) == 0) {
    printf("This APP_PAYLOAD MSG is for me.\n");

    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sun_family = AF_LOCAL;
    if (!get_sun_path_from_port(odr_packet.dest_port)) {
      fprintf (stderr, "Some random packet has received to me. %d\n", odr_packet.dest_port);
      exit(0);
    }
    strcpy(serveraddr.sun_path, get_sun_path_from_port(odr_packet.dest_port));
    strncpy(seq.ip, odr_packet.source_ip, MAX_IP_LEN);
    seq.port = odr_packet.source_port;
    seq.reroute = odr_packet.force_discovery;
    strncpy(seq.buffer, odr_packet.app_message, sizeof(seq.buffer));
    //seq.reroute = reroute;

    Sendto(odr_sun_path_sockfd, (void*) &seq, sizeof(seq), 0, (SA*) &serveraddr, sizeof(serveraddr));
    return;
  }

  printf("Not for me, fwd to next hop.\n");

  if (is_ip_in_route_table(odr_packet.dest_ip, &table_entry) == NO) {
    printf ("I've lost my entry to dest(%s). Sending RREQ to rebuild.\n", odr_packet.dest_ip);
    broadcast_to_all_interfaces(pf_packet_sockfd, vminfo, num_interfaces, odr_packet.dest_ip, NULL, 0);
    add_to_queue(odr_packet);
    return;
  }

  odr_packet.hop_count++;

  get_vminfo_by_ifindex(vminfo, num_interfaces, table_entry.if_index, &sending_if_info);
  send_pf_packet(pf_packet_sockfd, sending_if_info, table_entry.next_hop, &odr_packet);

  return;
}

void
recv_pf_packet(int pf_packet_sockfd, struct hwa_info* vminfo,
                int num_interfaces, int odr_sun_path_sockfd)
{
  struct sockaddr_ll socket_address;
  socklen_t sock_len = sizeof(socket_address);
  unsigned char src_mac[6];
  /*buffer for ethernet frame*/
  void* buffer = (void*)malloc(ETH_FRAME_LEN);
  odr_packet_t odr_packet;

  int length = 0; /*length of the received frame*/
  length = recvfrom(pf_packet_sockfd, buffer, ETH_FRAME_LEN, 0, (struct sockaddr*)&socket_address, &sock_len);
  if (length == -1) { printf("PF_PACKET recv error\n");}
  memcpy (&odr_packet, buffer+14, sizeof(odr_packet_t));
  //printf("----Recvd PF_PACKET----\n");
  memcpy((void*)src_mac, (void *)buffer+ETH_ALEN, ETH_ALEN);
  //print_pf_packet(socket_address, buffer, odr_packet);

  switch (odr_packet.type) {
    case RREQ:
      handle_rreq(odr_packet, socket_address, src_mac, pf_packet_sockfd,
                  vminfo, num_interfaces, odr_sun_path_sockfd);
      break;

    case RREP:
      handle_rrep(odr_packet, socket_address, src_mac, pf_packet_sockfd,
                  vminfo, num_interfaces, odr_sun_path_sockfd);
      break;

    case APP_PAYLOAD:
      handle_app_payload (odr_packet, socket_address, src_mac,
                          pf_packet_sockfd, vminfo, num_interfaces,
                          odr_sun_path_sockfd);
      break;

    default:
      fprintf (stderr, "Unknown type: %d\n", odr_packet.type);
      exit(0);
  }
  //free(buffer);
}

void
process_client_req(sequence_t recvseq, struct hwa_info* vminfo,
                    int num_interfaces, int pf_packet_sockfd,
                    int odr_sun_path_sockfd)
{
  route_table_t table_entry;
  struct sockaddr_un servaddr;
  struct hwa_info sending_if_info;
  odr_packet_t odr_packet;
  int ephemeral_port;
  sequence_t seq;

  printf("New Message recvd for IP:%s, now routing...\n", recvseq.ip);

  ephemeral_port = add_to_peer_process_table(&my_client_addr);

  // If it's local send it here itself.
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

    printf ("Request server is local. Sending to %s\n", servaddr.sun_path);
    Sendto(odr_sun_path_sockfd, (void*)&seq, sizeof(seq), 0, (SA*)&servaddr, sizeof(servaddr));
    return;
  }

  odr_packet.type = APP_PAYLOAD;
  //printf ("strlen(recvseq.ip) = %lu\n", strlen(recvseq.ip));
  strncpy(odr_packet.dest_ip, recvseq.ip, MAX_IP_LEN);
  strncpy(odr_packet.source_ip, my_ip_addr, MAX_IP_LEN);
  odr_packet.dest_port = recvseq.port;
  odr_packet.source_port = ephemeral_port;
  odr_packet.hop_count = 0;
  strcpy(odr_packet.app_message, recvseq.buffer);
  odr_packet.app_req_or_rep = (strcmp(my_client_addr.sun_path, SERVER_SUNPATH) == 0)? AREP: AREQ;

  if (is_ip_in_route_table(recvseq.ip, &table_entry) == NO || recvseq.reroute == 1) {
    printf("IP not in routing table, broadcasting to all interfaces...\n");
    broadcast_to_all_interfaces(pf_packet_sockfd, vminfo, num_interfaces, recvseq.ip, NULL, recvseq.reroute);
    add_to_queue(odr_packet);

  } else {
    get_vminfo_by_ifindex(vminfo, num_interfaces, table_entry.if_index, &sending_if_info);
    send_pf_packet(pf_packet_sockfd, sending_if_info, table_entry.next_hop, &odr_packet);
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
  struct hwa_info vminfo[50];
  int             num_interfaces, odr_sun_path_sockfd, pf_packet_sockfd, n, maxfdp1;
  fd_set cur_set, org_set;
  socklen_t client_addr_len = sizeof(my_client_addr);

  sequence_t recvseq;

  build_peer_process_table();

  staleness = parsecommandline(argc, argv);

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
      memset(&my_client_addr, 0, sizeof(struct sockaddr_un));
      memset(&recvseq, 0, sizeof(sequence_t));
      n = recvfrom(odr_sun_path_sockfd, &recvseq, sizeof(recvseq), 0, (SA*) &my_client_addr, &client_addr_len);
      process_client_req(recvseq, vminfo, num_interfaces, pf_packet_sockfd, odr_sun_path_sockfd);
    }

    if (FD_ISSET(pf_packet_sockfd, &cur_set))
      recv_pf_packet(pf_packet_sockfd, vminfo, num_interfaces, odr_sun_path_sockfd);
  }
}
