#include "header.h"

int rt, pg, pf_packet, multi;

void get_ip_of_vm(char vmno_str[6], char* final_ip, int length){

  struct hostent *hptr;
  char **pptr;
  char str [INET_ADDRSTRLEN];
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
          Inet_ntop(hptr->h_addrtype, *pptr, str, sizeof (str));
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

// Computing the internet checksum (RFC 1071).
// Note that the internet checksum does not preclude collisions.
// Source : link given in assignment spec.
uint16_t
checksum (uint16_t *addr, int len)
{
  int count = len;
  register uint32_t sum = 0;
  uint16_t answer = 0;

  // Sum up 2-byte values until none or only one byte left.
  while (count > 1) {
    sum += *(addr++);
    count -= 2;
  }

  // Add left-over byte, if any.
  if (count > 0) {
    sum += *(uint8_t *) addr;
  }

  // Fold 32-bit sum into 16 bits; we lose information by doing this,
  // increasing the chances of a collision.
  // sum = (lower 16 bits) + (upper 16 bits shifted right 16 bits)
  while (sum >> 16) {
    sum = (sum & 0xffff) + (sum >> 16);
  }

  // Checksum is one's compliment of sum.
  answer = ~sum;

  return (answer);
}

void print_tour_list(tour_list_node_t* tour_list_node, int length) {
  int i;
  printf("_________TOUR LIST_________\n");
  for (i = 0; i < length; i++) {
    if (tour_list_node[i].is_cur == YES) printf("CUR->");
    printf("%s\n", tour_list_node[i].node_ip);
  }
  printf("___________END_________\n");
}

void make_list(int argc, char* argv[]) {
  
  char vm_ip[MAX_IP_LEN], my_hostname[MAXLINE];
  int i;
  tour_list_node_t tour_list_node[argc];
  for (i = 1; i < argc; i++) {
    get_ip_of_vm(argv[i], vm_ip, MAX_IP_LEN);
    printf("%s: %s\n", argv[i], vm_ip);
    strcpy(tour_list_node[i].node_ip, vm_ip);
    tour_list_node[i].is_cur = NO;
  }
  gethostname(my_hostname, MAXLINE);
  printf("My hostname: %s\n", my_hostname);
  get_ip_of_vm(my_hostname, vm_ip, MAX_IP_LEN);
  strcpy(tour_list_node[0].node_ip, vm_ip);
  tour_list_node[0].is_cur = YES;
  print_tour_list(tour_list_node, argc);
  send_rt (tour_list_node, argc);
}

void process_recvd_tour_list(tour_list_node_t* tour_list_node, int list_len) {
  int i, cur;
  // go to cur
  for (i = 0; i < list_len; i++) {
    if (tour_list_node[i].is_cur == YES) {
      cur = i + 1;
      tour_list_node[i].is_cur = NO;
    }
  }
  if (cur == list_len - 1) return;
  else {
    // send to next
    tour_list_node[cur].is_cur = YES;
    send_rt(tour_list_node, list_len);
  }
}

send_rt(tour_list_node_t* tour_list_node, int list_len) {

  struct ip ip_hdr;
  socklen_t len;
  int status, i;
  struct sockaddr_in dest;
  void *buf;

  // tour list related
  tour_list_node_t cur_node;
  tour_list_node_t dest_node;

  // init operations
  bzero(&dest, sizeof(dest));

  // make ip_header
  // IPv4 header length (4 bits): Number of 32-bit words in header = 5
  ip_hdr.ip_hl = (IP4_HDRLEN / sizeof (uint32_t)); //+ sizeof(tour_list_node_t * list_len);

  //print_tour_list(tour_list_node, list_len);
  
  // Internet Protocol version (4 bits): IPv4
  ip_hdr.ip_v = 4;

  // Type of service (8 bits)
  ip_hdr.ip_tos = 0;

  // Total length of datagram (16 bits): IP header + data
  ip_hdr.ip_len = htons ((IP4_HDRLEN) + (sizeof(tour_list_node_t) * list_len));

  // ID sequence number (16 bits): unused, since single datagram
  ip_hdr.ip_id = htons (USID_PROTO);

  ip_hdr.ip_off = 0; 

  // Time-to-Live (8 bits): default to maximum value
  ip_hdr.ip_ttl = 255;

  // Transport layer protocol (8 bits): 1 for ICMP
  ip_hdr.ip_p = USID_PROTO;

  // Get cur element from list: this should be me. And that should be
  // the src addr.
  for (i = 0; i < list_len; i++) {
    if (tour_list_node[i].is_cur == YES) {
      memcpy(&cur_node, &tour_list_node[i], sizeof(tour_list_node_t));
      //TODO : make sure i+1 is valid and it is not end of list.
      memcpy(&dest_node, &tour_list_node[i+1], sizeof(tour_list_node_t));
    }
  }
  // Source IPv4 address (32 bits)
  if ((status = inet_pton (AF_INET, cur_node.node_ip, &(ip_hdr.ip_src))) != 1) {
    fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
    exit (EXIT_FAILURE);
  }

  // Destination IPv4 address (32 bits)
  if ((status = inet_pton (AF_INET, dest_node.node_ip, &(ip_hdr.ip_dst))) != 1) {
    fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
    exit (EXIT_FAILURE);
  }

  // IPv4 header checksum (16 bits): set to 0 when calculating checksum
  ip_hdr.ip_sum = 0;
  ip_hdr.ip_sum = checksum ((uint16_t *) &ip_hdr, IP4_HDRLEN);

  // build the dest sockaddr
  dest.sin_family = AF_INET;
  memcpy(&dest.sin_addr.s_addr, &ip_hdr.ip_dst, sizeof(dest.sin_addr.s_addr));
  len = sizeof(dest);

  // Build data_ ip_hdr
  buf = malloc(sizeof(ip_hdr) + (sizeof(tour_list_node_t) * list_len));
  memcpy(buf, &ip_hdr, sizeof(ip_hdr));
  memcpy(buf + sizeof(ip_hdr), tour_list_node, sizeof(tour_list_node_t) * list_len);
  
  Sendto(rt, buf, sizeof(ip_hdr) + (sizeof(tour_list_node_t) * list_len), 0, (SA*) &dest, len);
  printf("Sent\n"); // DEBUG
}

int main(int argc, char *argv[]) {

  struct ip ip_hdr;
  void *buf;
  struct sockaddr_in recvd;
  struct hostent *hptr;
  const int on = 1;
  int status, maxfdp1, n, list_len;
  fd_set cur_set, org_set;
  time_t ticks;
  socklen_t len;
  char str[INET_ADDRSTRLEN];
  tour_list_node_t* tour_list_node;
  rt = socket(AF_INET, SOCK_RAW, USID_PROTO);
  pg = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if (setsockopt(rt, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
    printf("setsockpot for rt socket error.\n");
    exit(0);
  }
  bzero(&recvd, sizeof(recvd));
  if (argc >= 2) {
    make_list(argc, argv);
  }
  FD_ZERO(&org_set);
  FD_ZERO(&cur_set);
  FD_SET(rt, &org_set);
  FD_SET(pg, &org_set);
  maxfdp1 = rt > pg ? (rt + 1) : (pg + 1);
  for (; ;) {
    cur_set = org_set;
    Select(maxfdp1, &cur_set, NULL, NULL, NULL);
    if (FD_ISSET(rt, &cur_set)) {
      len = sizeof(recvd); // always initialize len
      n = recvfrom(rt, &ip_hdr, sizeof(ip_hdr), MSG_PEEK, (SA*) &recvd, &len);
      if (ntohs(ip_hdr.ip_id) == USID_PROTO) {
        ticks = time(NULL);
        hptr = gethostbyaddr (&recvd.sin_addr, sizeof(recvd.sin_addr), AF_INET);
        printf("%.24s recieved source routing packet from %s\n", ctime(&ticks), hptr->h_name);
        buf = malloc(ip_hdr.ip_len);
        n = recvfrom(rt, buf, ip_hdr.ip_len, 0, (SA*) &recvd, &len);
        memcpy(&ip_hdr, buf, sizeof(ip_hdr));
        list_len = (ntohs(ip_hdr.ip_len) - sizeof(ip_hdr)) / sizeof(tour_list_node_t);
        printf("tour_list_len = %d\n", list_len);
        tour_list_node = malloc(sizeof(tour_list_node_t) * list_len);
        memcpy(tour_list_node, buf + sizeof(ip_hdr), ntohs(ip_hdr.ip_len) - sizeof(ip_hdr));
        print_tour_list(tour_list_node, list_len);
        process_recvd_tour_list(tour_list_node, list_len);
      }
    }
  }
  return 0;
}
