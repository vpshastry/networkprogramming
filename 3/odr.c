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
  if (!queue_head)
    return NULL;

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

void send_pf_packet(int s, struct hwa_info vminfo, unsigned char* dest_mac, odr_packet_t odr_packet)
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
  *odr_packet_ptr = odr_packet;

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

void broadcast_to_all_interfaces(int pf_packet_sockfd, struct hwa_info* vminfo, int num_interfaces, char dest_ip[MAX_IP_LEN], odr_packet_t* fwd_odr_packet) {

  int i;
  odr_packet_t odr_packet;
  if (fwd_odr_packet == NULL) {
    odr_packet.type = RREQ;
    strcpy(odr_packet.source_ip, my_ip_addr);
    strcpy(odr_packet.dest_ip, dest_ip);
    odr_packet.broadcast_id = ++my_broadcast_id;
    odr_packet.rrep_already_sent = NO;
    odr_packet.force_discovery = NO;
    odr_packet.hop_count = 0;
  } else {
    memcpy(&odr_packet, fwd_odr_packet, sizeof(odr_packet_t));
  }
  for (i = 0; i < num_interfaces; i++) {
    send_pf_packet(pf_packet_sockfd, vminfo[i], broadcast_ip, odr_packet);
  }
}

int is_ip_in_route_table (char ip[MAX_IP_LEN], route_table_t* table_entry) {
  int i;
  for (i = 0; i < route_table_len; i++) {
    //printf("comparing Ip %s and %s\n", route_table[i].dest_ip, ip);
    if (strcmp(route_table[i].dest_ip, ip) == 0) {
      printf("This IP is in routing table\n");
      strcpy(table_entry->dest_ip, route_table[i].dest_ip);
      memcpy((void*)table_entry->next_hop, (void*)route_table[i].next_hop, ETH_ALEN);
      table_entry->if_index = route_table[i].if_index;
      table_entry->num_hops = route_table[i].num_hops;
      table_entry->last_bcast_id_seen = route_table[i].last_bcast_id_seen;
      return YES;
    }
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

int update_routing_table(odr_packet_t odr_packet, struct sockaddr_ll socket_address, unsigned char src_mac[6]) {

  int i;
  route_table_t table_entry;
  if (strcmp(odr_packet.source_ip, my_ip_addr) == 0) {
    //printf("I have only sent this request.\n");
    return SELF_ORIGIN;
  }
  if (is_ip_in_route_table(odr_packet.source_ip, &table_entry) == YES) {
    //printf("Routing information to this source is already there\n");
    if (table_entry.last_bcast_id_seen == odr_packet.broadcast_id) {
      //printf("Same bcast ID, Already seen this RREQ\n");
      if (table_entry.num_hops < (odr_packet.hop_count + 1)) {
        //printf("Greater hop_count. No update\n");
        return DUP_ENTRY;
      }
      else if (table_entry.num_hops > (odr_packet.hop_count + 1)) {
        //printf("Lesser hop_count. Update Table\n");
        for (i = 0; i < route_table_len; i++) {
          if (strcmp(route_table[i].dest_ip, table_entry.dest_ip) == 0) {
            memcpy((void*)route_table[i].next_hop, (void*) src_mac, ETH_ALEN);
            route_table[i].if_index = socket_address.sll_ifindex;
            route_table[i].num_hops = odr_packet.hop_count + 1; // +1 for the hop it made to get to me.
            route_table[i].last_bcast_id_seen = odr_packet.broadcast_id;
          }
        }
        return R_TABLE_UPDATED;
      } else {
        //printf("Same number of hops\n");
        if (strcmp(table_entry.next_hop, src_mac) == 0) {
          //printf("Same neightbour. No update\n.");
          return DUP_ENTRY;
        } else {
          //printf("Different neighbour, need to update\n"); // TO-DO
          return DUP_ENTRY; // only for now, TO-DO
        }
        return R_TABLE_UPDATED;
      }
    }
  } else {
    //printf("Adding new entry into routing table\n");
    strcpy(route_table[route_table_len].dest_ip, odr_packet.source_ip);
    memcpy((void*) route_table[route_table_len].next_hop, (void*) src_mac, ETH_ALEN);
    route_table[route_table_len].if_index = socket_address.sll_ifindex;
    route_table[route_table_len].num_hops = odr_packet.hop_count + 1; // +1 for the hop it made to get to me.
    route_table[route_table_len].last_bcast_id_seen = odr_packet.broadcast_id;
    route_table_len++;
    return R_TABLE_UPDATED;
  }
  print_routing_table();
}

void recv_pf_packet(int pf_packet_sockfd, struct hwa_info* vminfo, int num_interfaces, int odr_sun_path_sockfd)
{
  struct sockaddr_ll socket_address;
  socklen_t sock_len = sizeof(socket_address);
  unsigned char src_mac[6];
  unsigned char dest_mac[6];
  /*buffer for ethernet frame*/
  void* buffer = (void*)malloc(ETH_FRAME_LEN);
  odr_packet_t *odr_packet_ptr;
  odr_packet_t odr_packet;
  odr_packet_t rrep, app_payload;
  route_table_t table_entry;
  struct hwa_info sending_if_info;
  struct sockaddr_un serveraddr;
  sequence_t seq;
  int r_table_update;
  odr_packet_t *msgtosend;

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

  odr_packet_ptr = buffer+14;
  odr_packet = *odr_packet_ptr;
  printf("type = %d\n", odr_packet.type);
  printf("source IP = %s\n", odr_packet.source_ip);
  printf("dest IP = %s\n", odr_packet.dest_ip);
  printf("broadcast id = %d\n", odr_packet.broadcast_id);
  printf("hop count = %d\n", odr_packet.hop_count);
  if (odr_packet.type == RREQ) {
    if (strcmp (odr_packet.dest_ip, my_ip_addr) == 0) {
      printf("This packet is for me. Sending RREP.\n");
      // Update routing table first.
      r_table_update = update_routing_table(odr_packet, socket_address, src_mac);
      if (r_table_update == DUP_ENTRY) {
        printf("Already handled. IGNORE.\n");
        return;
      }
      // Now need to send RREP (if not already sent flag - TO-DO)
      // Building RREP

      rrep.type = RREP;
      strcpy(rrep.source_ip, odr_packet.dest_ip);
      strcpy(rrep.dest_ip, odr_packet.source_ip);
      rrep.dest_port = odr_packet.source_port;
      rrep.source_port = odr_packet.dest_port;
      rrep.hop_count = 0;
      // get the table entry to send it to.
      if (is_ip_in_route_table(rrep.dest_ip, &table_entry) == NO) {
        printf("Something is seriously wrong, no path in routing table for rrep?\n");
        exit(0);
      }
      //printf("Need to send to index: %d\n", table_entry.if_index);
      get_vminfo_by_ifindex(vminfo, num_interfaces, table_entry.if_index, &sending_if_info);
      send_pf_packet(pf_packet_sockfd, sending_if_info, table_entry.next_hop, rrep);
    } else {
      printf("This rreq is not for me.. check routing table\n");
      r_table_update = update_routing_table(odr_packet, socket_address, src_mac);
      if (r_table_update == DUP_ENTRY) {
        printf("IGNORING..\n");
        return; // recvd. a dupicate RREQ
      }
      if (r_table_update == SELF_ORIGIN) {
        return;
      }
      if (is_ip_in_route_table(odr_packet.dest_ip, &table_entry) == NO) {
        printf("Not in routing table... broadcasting.\n");
        odr_packet.hop_count++; // Increment hop_count for the hop it just made to get to me.
        broadcast_to_all_interfaces(pf_packet_sockfd, vminfo, num_interfaces, NULL, &odr_packet);
      } else if (odr_packet.rrep_already_sent != YES) {
          printf("I know path to dest, should send RREP\n");
          rrep.type = RREP;
          strcpy(rrep.source_ip, odr_packet.dest_ip);
          strcpy(rrep.dest_ip, odr_packet.source_ip);
          rrep.source_port = odr_packet.dest_port;
          rrep.dest_port = odr_packet.source_port;
          rrep.hop_count = table_entry.num_hops;
          // get the table entry to send it to.
          if (is_ip_in_route_table(rrep.dest_ip, &table_entry) == NO) {
            printf("Something is seriously wrong, no path in routing table for rrep?\n");
            exit(0);
          }
          //printf("Need to send to index: %d\n", table_entry.if_index);
          get_vminfo_by_ifindex(vminfo, num_interfaces, table_entry.if_index, &sending_if_info);
          send_pf_packet(pf_packet_sockfd, sending_if_info, table_entry.next_hop, rrep);
          printf("Intermediate RREP sent, Checking if I knew the source node\n");
          if (r_table_update == R_TABLE_UPDATED) {
            printf("This was a new/better path to the src node... continue-ing to flood RREQ with rrep_already_sent = YES;.\n");
            odr_packet.hop_count++; // Increment hop_count for the hop it just made to get to me.
            odr_packet.rrep_already_sent = YES;
            broadcast_to_all_interfaces(pf_packet_sockfd, vminfo, num_interfaces, NULL, &odr_packet);
          }
      } else {
        if (r_table_update == R_TABLE_UPDATED) {
          printf("rrep is already sent.\nThis was a new/better path to the src node... continue-ing to flood RREQ with rrep_already_sent = YES;.\n");
          odr_packet.hop_count++; // Increment hop_count for the hop it just made to get to me.
          //odr_packet.rrep_already_sent = YES;
          broadcast_to_all_interfaces(pf_packet_sockfd, vminfo, num_interfaces, NULL, &odr_packet);
        }
      }
    }
  }
  if (odr_packet.type == RREP) {
    if (strcmp (odr_packet.dest_ip, my_ip_addr) == 0) {
      printf("This RREP is for me. Check if I should send app_payload now.\n"); // TO-DO.
      // Update routing table first.
      if (update_routing_table(odr_packet, socket_address, src_mac) == DUP_ENTRY) {
        printf("RREP Already handled. IGNORE.\n");
        return;
      }
      // Send app_payload here.
      app_payload.type = APP_PAYLOAD;
      msgtosend = get_from_queue(odr_packet.source_ip);
      strcpy(app_payload.source_ip, my_ip_addr);
      strcpy(app_payload.dest_ip, msgtosend->dest_ip);
      app_payload.dest_port = msgtosend->dest_port;
      app_payload.source_port = msgtosend->source_port;
      app_payload.hop_count = 0;
      strcpy(app_payload.app_message, msgtosend->app_message);
      app_payload.app_req_or_rep = AREQ;
      free(msgtosend);

      if (is_ip_in_route_table(app_payload.dest_ip, &table_entry) == NO) {
        printf("Something is seriously wrong, no path in routing table for app_payload msg?\n");
        exit(0);
      }
      //printf("Need to send to index: %d\n", table_entry.if_index);
      get_vminfo_by_ifindex(vminfo, num_interfaces, table_entry.if_index, &sending_if_info);
      send_pf_packet(pf_packet_sockfd, sending_if_info, table_entry.next_hop, app_payload);

    } else {

      printf("This RREP is not for me.. Routing it after updating Routing table\n");
      if (update_routing_table(odr_packet, socket_address, src_mac) == DUP_ENTRY) {
        printf("INnfo already in routing table. No updated needed\n");
      }
      odr_packet.hop_count++;// Increment hop_count for the hop it just made to get to me.

      if (is_ip_in_route_table(odr_packet.dest_ip, &table_entry) == NO) {
        printf("Something is seriously wrong, no path in routing table for rrep?\n");
        exit(0);
      }

      get_vminfo_by_ifindex(vminfo, num_interfaces, table_entry.if_index, &sending_if_info);
      send_pf_packet(pf_packet_sockfd, sending_if_info, table_entry.next_hop, odr_packet);
      return;
    }
  }
  if (odr_packet.type == APP_PAYLOAD) {
    printf("Recvd application payload msg\n");
    if (strcmp (odr_packet.dest_ip, my_ip_addr) == 0) {
      printf("This APP_PAYLOAD MSG is for me.\n");
      // Update routing table first.
      r_table_update = update_routing_table(odr_packet, socket_address, src_mac);
      if (r_table_update == DUP_ENTRY) {
        printf("Info Already in routing table. IGNORE.\n");
        //return;
      }

      bzero(&serveraddr, sizeof(serveraddr));
      serveraddr.sun_family = AF_LOCAL;
      if (!get_sun_path_from_port(odr_packet.dest_port)) {
        fprintf (stderr, "Some random packet has received to me. %d\n", odr_packet.dest_port);
        exit(0);
      }
      strcpy(serveraddr.sun_path, get_sun_path_from_port(odr_packet.dest_port));
      strncpy(seq.ip, odr_packet.source_ip, MAX_IP_LEN);
      seq.port = odr_packet.source_port;
      strncpy(seq.buffer, odr_packet.app_message, sizeof(seq.buffer));
      //seq.reroute = reroute;

      if (odr_packet.app_req_or_rep == AREQ) {
        printf("Send to server\n");

      } else if (odr_packet.app_req_or_rep == AREP) {
        printf("msg from Server to client: %s\n", odr_packet.app_message);
        printf("Send to Client\n");
      }

      Sendto(odr_sun_path_sockfd, (void*) &seq, sizeof(seq), 0, (SA*) &serveraddr, sizeof(serveraddr));
      return;

    } else {
      printf("Not for me, fwd to next hop.\n");

      if (update_routing_table(odr_packet, socket_address, src_mac) == DUP_ENTRY) {
        printf("INnfo already in routing table. No updated needed\n");
      }
      odr_packet.hop_count++;// Increment hop_count for the hop it just made to get to me.

      if (is_ip_in_route_table(odr_packet.dest_ip, &table_entry) == NO) {
        printf("Something is seriously wrong, no path in routing table for app_payload msg?\n");
        exit(0);
      }

      get_vminfo_by_ifindex(vminfo, num_interfaces, table_entry.if_index, &sending_if_info);
      send_pf_packet(pf_packet_sockfd, sending_if_info, table_entry.next_hop, odr_packet);
      return;
    }
  }
}

void process_client_req(sequence_t recvseq, struct hwa_info* vminfo, int num_interfaces, int pf_packet_sockfd) {

  route_table_t table_entry;
  struct hwa_info sending_if_info;
  odr_packet_t *odr_packet = Calloc(1, sizeof(odr_packet_t));
  int ephemeral_port = add_to_peer_process_table(&my_client_addr);

  printf("New Message recvd for IP:%s, now routing...\n", recvseq.ip);

  odr_packet->type = APP_PAYLOAD;
  strcpy(odr_packet->dest_ip, recvseq.ip);
  strcpy(odr_packet->source_ip, my_ip_addr);
  odr_packet->dest_port = recvseq.port;
  odr_packet->source_port = ephemeral_port;
  odr_packet->hop_count = 0;
  strcpy(odr_packet->app_message, recvseq.buffer); //temp sending hi for now.
  //printf("Sending new app_messge: recvseq = %s, odr_pack = %s\n", recvseq.buffer, odr_packet.app_message);
  if (strcmp(my_client_addr.sun_path, SERVER_SUNPATH) == 0)
    odr_packet->app_req_or_rep = AREP;
  else odr_packet->app_req_or_rep = AREQ;

  printf("New Message recvd for IP:%s, now routing...\n", recvseq.ip);
  if (is_ip_in_route_table(recvseq.ip, &table_entry) == NO) {
    printf("IP not in routing table, broadcasting to all interfaces...\n");
    broadcast_to_all_interfaces(pf_packet_sockfd, vminfo, num_interfaces, recvseq.ip, NULL);
    add_to_queue(odr_packet);
  } else {

    get_vminfo_by_ifindex(vminfo, num_interfaces, table_entry.if_index, &sending_if_info);
    send_pf_packet(pf_packet_sockfd, sending_if_info, table_entry.next_hop, *odr_packet);
  }
  //printf("Message is:%s\n", recvseq.buffer);
  //get_mac_id_and_if_index_by_ip(recvseq, vminfo, num_interfaces);
}

int create_bind_pf_packet() {
  int s;
  s = socket(AF_PACKET, SOCK_RAW, htons(USID_PROTO));
  if (s == -1) { fprintf (stderr, "PF_PACKET creatation failed"); exit(0); }
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
  maxfdp1 = odr_sun_path_sockfd > pf_packet_sockfd ? (odr_sun_path_sockfd + 1) : (pf_packet_sockfd + 1);
  printf("FD's set, now going into select loop....\n");
  for (; ;) {
    cur_set = org_set;
    Select(maxfdp1, &cur_set, NULL, NULL, NULL);
    if (FD_ISSET(odr_sun_path_sockfd, &cur_set)) {
      printf("Data recvd from client/Server\n");
      n = recvfrom(odr_sun_path_sockfd, &recvseq, sizeof(recvseq), 0, (SA*) &my_client_addr, &client_addr_len);
      process_client_req(recvseq, vminfo, num_interfaces, pf_packet_sockfd);

    }
    if (FD_ISSET(pf_packet_sockfd, &cur_set)) {
      recv_pf_packet(pf_packet_sockfd, vminfo, num_interfaces, odr_sun_path_sockfd);
    }
  }
}
