#include "header.h"

static ctx_t ctx;
extern char my_ip_addr[MAX_IP_LEN];

unsigned char broadcast_ip[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// Routing table entry- only ODR uses it, it is not common, so it is not in header.h
typedef struct {
  char dest_ip[MAX_IP_LEN];
  char next_hop[IF_HADDR];
  int if_index;
  int num_hops;
  // TO-DO (@any) - timestamp field.

  // Bookeeping info
  int is_valid;
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
void recv_pf_packet(int pf_packet_sockfd, struct hwa_info* vminfo, int num_interfaces)
{
  struct sockaddr_ll socket_address;
  socklen_t sock_len = sizeof(socket_address);
  /*buffer for ethernet frame*/
  void* buffer = (void*)malloc(ETH_FRAME_LEN);
  odr_packet_t *odr_packet_ptr;
  odr_packet_t odr_packet;

  int length = 0; /*length of the received frame*/ 
  length = recvfrom(pf_packet_sockfd, buffer, ETH_FRAME_LEN, 0, (struct sockaddr*)&socket_address, &sock_len);
  if (length == -1) { printf("PF_PACKET recv error\n");}
  printf("----Recvd PF_PACKET----\n");
  odr_packet_ptr = buffer+14;
  odr_packet = *odr_packet_ptr;
  printf("type = %d\n", odr_packet.type);
  printf("source IP = %s\n", odr_packet.source_ip);
  printf("dest IP = %s\n", odr_packet.dest_ip);
  printf("broadcast id = %d\n", odr_packet.broadcast_id);
  printf("hop count = %d\n", odr_packet.hop_count);
  if (strcmp (odr_packet.dest_ip, my_ip_addr) == 0) {
    printf("This packet is for me\n");
  } else {
    printf("This packet is not for me.. check routing table\n");
    if (route_table_len == 0) {
      printf("Not in routing table... broadcasting.");
      odr_packet.hop_count++;
      broadcast_to_all_interfaces(pf_packet_sockfd, vminfo, num_interfaces, NULL, &odr_packet);
    }
  }
}

/*void get_mac_id_and_if_index_by_ip(sequence_t recvseq, struct hwa_info* vminfo, int num_interfaces){
  int i, j, prflag;
  struct sockaddr	*sa;
  char ip[MAX_IP_LEN];
  for (i = 0; i < num_interfaces; i++) {
    sa = vminfo[i].hwa->ip_addr;
    sprintf(ip, "%s", Sock_ntop_host(sa, sizeof(*sa)));
    if (strcmp(ip, recvseq.ip) == 0) {
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
  }
   
}*/



void process_client_req(sequence_t recvseq, struct hwa_info* vminfo, int num_interfaces, int pf_packet_sockfd) {

  printf("New Message recvd for IP:%s, now routing...\n", recvseq.ip);  
  if (route_table_len == 0) {
    printf("IP not in routing table, broadcasting to all interfaces...\n");
    broadcast_to_all_interfaces(pf_packet_sockfd, vminfo, num_interfaces, recvseq.ip, NULL);
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

  sequence_t recvseq;

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
      printf("Data recvd from client\n");
      n = Read(odr_sun_path_sockfd, &recvseq, sizeof(recvseq));
      process_client_req(recvseq, vminfo, num_interfaces, pf_packet_sockfd);

    }
    if (FD_ISSET(pf_packet_sockfd, &cur_set)) {
      recv_pf_packet(pf_packet_sockfd, vminfo, num_interfaces);
    }
  }
}
