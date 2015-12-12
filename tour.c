#include "header.h"

int rt, pg, pf_packet, multi_send, multi_recv;

typedef struct {
 char hostname[MAXLINE];
} ping_list_t;

ping_list_t ping_list[500];
int ping_list_len = 0;

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


// Build IPv4 ICMP pseudo-header and call checksum function.
uint16_t
icmp4_checksum (struct icmp icmphdr, uint8_t *payload, int payloadlen)
{
  char buf[IP_MAXPACKET];
  char *ptr;
  int chksumlen = 0;
  int i;

  ptr = &buf[0];  // ptr points to beginning of buffer buf

  // Copy Message Type to buf (8 bits)
  memcpy (ptr, &icmphdr.icmp_type, sizeof (icmphdr.icmp_type));
  ptr += sizeof (icmphdr.icmp_type);
  chksumlen += sizeof (icmphdr.icmp_type);

  // Copy Message Code to buf (8 bits)
  memcpy (ptr, &icmphdr.icmp_code, sizeof (icmphdr.icmp_code));
  ptr += sizeof (icmphdr.icmp_code);
  chksumlen += sizeof (icmphdr.icmp_code);

  // Copy ICMP checksum to buf (16 bits)
  // Zero, since we don't know it yet
  *ptr = 0; ptr++;
  *ptr = 0; ptr++;
  chksumlen += 2;

  // Copy Identifier to buf (16 bits)
  memcpy (ptr, &icmphdr.icmp_id, sizeof (icmphdr.icmp_id));
  ptr += sizeof (icmphdr.icmp_id);
  chksumlen += sizeof (icmphdr.icmp_id);

  // Copy Sequence Number to buf (16 bits)
  memcpy (ptr, &icmphdr.icmp_seq, sizeof (icmphdr.icmp_seq));
  ptr += sizeof (icmphdr.icmp_seq);
  chksumlen += sizeof (icmphdr.icmp_seq);

  // Copy payload to buf
  memcpy (ptr, payload, payloadlen);
  ptr += payloadlen;
  chksumlen += payloadlen;

  // Pad to the next 16-bit boundary
  for (i=0; i<payloadlen%2; i++, ptr++) {
    *ptr = 0;
    ptr++;
    chksumlen++;
  }

  return checksum ((uint16_t *) buf, chksumlen);
}


void print_tour_list(tour_list_node_t* tour_list_node, int length) {
  int i;
  printf("_________TOUR LIST_________\n");
  for (i = 0; i < length; i++) {
    if (tour_list_node[i].is_cur == YES) printf("CUR->");
    else if (tour_list_node[i].is_cur == MUL) printf("Multicast Address:");
    printf("%s\n", tour_list_node[i].node_ip);
  }
  printf("___________END_________\n");
}

void make_list(int argc, char* argv[]) {

  char vm_ip[MAX_IP_LEN], my_hostname[MAXLINE], str[MAXLINE];
  char *token;
  unsigned int mul_port;
  struct sockaddr_in multicast_sockaddr;
  socklen_t len;
  int i;
  tour_list_node_t tour_list_node[argc + 1];
  for (i = 1; i < argc; i++) {
    get_ip_of_vm(argv[i], vm_ip, MAX_IP_LEN);
    printf("%s: %s\n", argv[i], vm_ip);
    strcpy(tour_list_node[i + 1].node_ip, vm_ip);
    tour_list_node[i + 1].is_cur = NO;
  }
  gethostname(my_hostname, MAXLINE);
  printf("My hostname: %s\n", my_hostname);
  get_ip_of_vm(my_hostname, vm_ip, MAX_IP_LEN);
  strcpy(tour_list_node[1].node_ip, vm_ip);
  tour_list_node[1].is_cur = YES;
  strcpy(tour_list_node[0].node_ip, MUL_GRP_IP);
  strcat(tour_list_node[0].node_ip, ":");
  sprintf(str, "%d", MUL_PORT);
  strcat(tour_list_node[0].node_ip, str);
  tour_list_node[0].is_cur = MUL;
  print_tour_list(tour_list_node, argc + 1);

  // Join multicast grp
  bzero(&multicast_sockaddr, sizeof(multicast_sockaddr));
  multicast_sockaddr.sin_family = AF_INET;
  strcpy(vm_ip, tour_list_node[0].node_ip);
  token = strtok(vm_ip, ":");
  Inet_pton (AF_INET, token, &multicast_sockaddr.sin_addr.s_addr);
  token = strtok(NULL, ":");
  mul_port = atoi(token);
  printf("mul_port: %d\n", mul_port);
  multicast_sockaddr.sin_port = htons(mul_port);
  printf("sock_ntop: %s\n", Sock_ntop((SA*)&multicast_sockaddr, sizeof(multicast_sockaddr)));
  len = sizeof(multicast_sockaddr);
  Mcast_join(multi_recv, (SA*)&multicast_sockaddr, len, NULL, 0);
  printf("Mcast_join returned\n");
  send_rt (tour_list_node, argc + 1);
}

void
pinging()
{
  int sd, i, status, datalen, frame_length, bytes, recvsd, timeout, trycount, trylim, done;
  int ip_flags[4];
  struct ifreq ifr;
  char interface[MAXLINE];
  uint8_t src_mac[6], dst_mac[6], data[IP_MAXPACKET], ether_frame[IP_MAXPACKET];
  struct sockaddr_ll device;
  char src_ip[INET_ADDRSTRLEN], target[MAXLINE], dst_ip[INET_ADDRSTRLEN], recv_ether_frame[IP_MAXPACKET], rec_ip[INET_ADDRSTRLEN];
  struct addrinfo hints, *res;
  void *tmp;
  struct sockaddr_in *ipv4;
  struct ip iphdr, *recv_iphdr;
  struct icmp icmphdr, *recv_icmphdr;
  struct sockaddr from;
  socklen_t fromlen;
  char my_hostname[MAXLINE];

  // areq API call vars
  struct sockaddr_in needmacof = { .sin_family = AF_INET, };
  struct hwaddr hwaddr;

  struct timeval wait, t1, t2;
  struct timezone tz;
  double dt;
  int node;

  printf("pinging\n");

  if (ping_list_len == 0) return;

  for (node = 0; node < ping_list_len; node++) {

    printf("In for loop for pinging\n");

    sd = socket (PF_PACKET, SOCK_RAW, htons (ETH_P_ALL));


    // Interface to send packet through.
    strcpy (interface, INTERESTED_IF);

    // Use ioctl() to look up interface name and get its MAC address.
    memset (&ifr, 0, sizeof (ifr));
    snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", interface);
    if (ioctl (sd, SIOCGIFHWADDR, &ifr) < 0) {
      perror ("ioctl() failed to get source MAC address ");
      //return (EXIT_FAILURE);
    }
    close (sd);

    // Copy source MAC address.
    memcpy (src_mac, ifr.ifr_hwaddr.sa_data, 6);

    // Report source MAC address to stdout.
    printf ("MAC address for interface %s is ", interface);
    for (i=0; i<5; i++) {
      printf ("%02x:", src_mac[i]);
    }
    printf ("%02x\n", src_mac[5]);

    // Find interface index from interface name and store index in
    // struct sockaddr_ll device, which will be used as an argument of sendto().
    memset (&device, 0, sizeof (device));
    if ((device.sll_ifindex = if_nametoindex (interface)) == 0) {
      perror ("if_nametoindex() failed to obtain interface index ");
      //exit (EXIT_FAILURE);
    }
    printf ("Index for interface %s is %i\n", interface, device.sll_ifindex);

    // Source IPv4 address: you need to fill this out
    //strcpy (src_ip, );
    gethostname(my_hostname, sizeof(my_hostname));
    get_ip_of_vm(my_hostname, src_ip, INET_ADDRSTRLEN);
    printf("src_ip: %s\n", src_ip);
    // Destination URL or IPv4 address: you need to fill this out
    get_ip_of_vm(ping_list[node].hostname, target, INET_ADDRSTRLEN);
    //strcpy (target, "130.245.156.21");
    printf("target_ip:%s\n", target);

    Inet_pton(AF_INET, target, &needmacof.sin_addr);
    areq((struct sockaddr *)&needmacof, sizeof(struct sockaddr_in), &hwaddr);

    // Set destination MAC address: you need to fill these out
    memcpy(dst_mac, hwaddr.sll_addr, IF_HADDR);

    // Fill out hints for getaddrinfo().
    memset (&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = hints.ai_flags | AI_CANONNAME;

    // Resolve target using getaddrinfo().
    if ((status = getaddrinfo (target, NULL, &hints, &res)) != 0) {
      fprintf (stderr, "getaddrinfo() failed: %s\n", gai_strerror (status));
      exit (EXIT_FAILURE);
    }
    ipv4 = (struct sockaddr_in *) res->ai_addr;
    tmp = &(ipv4->sin_addr);
    if (inet_ntop (AF_INET, tmp, dst_ip, INET_ADDRSTRLEN) == NULL) {
      status = errno;
      fprintf (stderr, "inet_ntop() failed.\nError message: %s", strerror (status));
      //exit (EXIT_FAILURE);
    }
    freeaddrinfo (res);

    printf("freeaddrinfo called\n");
    // Fill out sockaddr_ll.
    device.sll_family = AF_PACKET;
    memcpy (device.sll_addr, src_mac, 6);
    device.sll_halen = 6;

    // ICMP data
    datalen = 4;
    data[0] = 'T';
    data[1] = 'e';
    data[2] = 's';
    data[3] = 't';

    // IPv4 header

    // IPv4 header length (4 bits): Number of 32-bit words in header = 5
    iphdr.ip_hl = IP4_HDRLEN / sizeof (uint32_t);

    // Internet Protocol version (4 bits): IPv4
    iphdr.ip_v = 4;

    // Type of service (8 bits)
    iphdr.ip_tos = 0;

    // Total length of datagram (16 bits): IP header + ICMP header + ICMP data
    iphdr.ip_len = htons (IP4_HDRLEN + ICMP_HDRLEN + datalen);

    // ID sequence number (16 bits): unused, since single datagram
    iphdr.ip_id = htons (0);

    // Flags, and Fragmentation offset (3, 13 bits): 0 since single datagram

    // Zero (1 bit)
    ip_flags[0] = 0;

    // Do not fragment flag (1 bit)
    ip_flags[1] = 0;

    // More fragments following flag (1 bit)
    ip_flags[2] = 0;

    // Fragmentation offset (13 bits)
    ip_flags[3] = 0;

    iphdr.ip_off = htons ((ip_flags[0] << 15)
                        + (ip_flags[1] << 14)
                        + (ip_flags[2] << 13)
                        +  ip_flags[3]);

    // Time-to-Live (8 bits): default to maximum value
    iphdr.ip_ttl = 255;

    // Transport layer protocol (8 bits): 1 for ICMP
    iphdr.ip_p = IPPROTO_ICMP;

    // Source IPv4 address (32 bits)
    if ((status = inet_pton (AF_INET, src_ip, &(iphdr.ip_src))) != 1) {
      fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
      exit (EXIT_FAILURE);
    }

    // Destination IPv4 address (32 bits)
    if ((status = inet_pton (AF_INET, dst_ip, &(iphdr.ip_dst))) != 1) {
      fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
      exit (EXIT_FAILURE);
    }

    // IPv4 header checksum (16 bits): set to 0 when calculating checksum
    iphdr.ip_sum = 0;
    iphdr.ip_sum = checksum ((uint16_t *) &iphdr, IP4_HDRLEN);

    printf("IP header done\n");
    // ICMP header

    // Message Type (8 bits): echo request
    icmphdr.icmp_type = ICMP_ECHO;

    // Message Code (8 bits): echo request
    icmphdr.icmp_code = 0;

    // Identifier (16 bits): usually pid of sending process - pick a number
    icmphdr.icmp_id = htons (1000);

    // Sequence Number (16 bits): starts at 0
    icmphdr.icmp_seq = htons (0);

    // ICMP header checksum (16 bits): set to 0 when calculating checksum
    icmphdr.icmp_cksum = icmp4_checksum (icmphdr, data, datalen);

    // Fill out ethernet frame header.

    // Ethernet frame length = ethernet header (MAC + MAC + ethernet type) + ethernet data (IP header + ICMP header + ICMP data)
    frame_length = 6 + 6 + 2 + IP4_HDRLEN + ICMP_HDRLEN + datalen;

    // Destination and Source MAC addresses
    memcpy (ether_frame, dst_mac, 6);
    memcpy (ether_frame + 6, src_mac, 6);

    // Next is ethernet type code (ETH_P_IP for IPv4).
    // http://www.iana.org/assignments/ethernet-numbers
    ether_frame[12] = ETH_P_IP / 256;
    ether_frame[13] = ETH_P_IP % 256;

    // Next is ethernet frame data (IPv4 header + ICMP header + ICMP data).

    // IPv4 header
    memcpy (ether_frame + ETH_HDRLEN, &iphdr, IP4_HDRLEN);

    // ICMP header
    memcpy (ether_frame + ETH_HDRLEN + IP4_HDRLEN, &icmphdr, ICMP_HDRLEN);

    // ICMP data
    memcpy (ether_frame + ETH_HDRLEN + IP4_HDRLEN + ICMP_HDRLEN, data, datalen);

    // Submit request for a raw socket descriptor.
    if ((sd = socket (PF_PACKET, SOCK_RAW, htons (ETH_P_ALL))) < 0) {
      perror ("socket() failed ");
      exit (EXIT_FAILURE);
    }

    // Submit request for a raw socket descriptor to receive packets.
    if ((recvsd = socket (PF_PACKET, SOCK_RAW, htons (ETH_P_ALL))) < 0) {
      perror ("socket() failed to obtain a receive socket descriptor ");
      exit (EXIT_FAILURE);
    }

    // Set maximum number of tries to ping remote host before giving up.
    trylim = 3;
    trycount = 0;

    // Cast recv_iphdr as pointer to IPv4 header within received ethernet frame.
    recv_iphdr = (struct ip *) (recv_ether_frame + ETH_HDRLEN);

    // Case recv_icmphdr as pointer to ICMP header within received ethernet frame.
    recv_icmphdr = (struct icmp *) (recv_ether_frame + ETH_HDRLEN + IP4_HDRLEN);

    done = 0;
    for (;;) {

      // SEND

      // Send ethernet frame to socket.
      if ((bytes = sendto (sd, ether_frame, frame_length, 0, (struct sockaddr *) &device, sizeof (device))) <= 0) {
        perror ("sendto() failed ");
        exit (EXIT_FAILURE);
      }
      break;
      /*
      // Start timer.
      (void) gettimeofday (&t1, &tz);

      // Set time for the socket to timeout and give up waiting for a reply.
      timeout = 2;
      wait.tv_sec  = timeout;
      wait.tv_usec = 0;
      setsockopt (recvsd, SOL_SOCKET, SO_RCVTIMEO, (char *) &wait, sizeof (struct timeval));

      // Listen for incoming ethernet frame from socket recvsd.
      // We expect an ICMP ethernet frame of the form:
      //     MAC (6 bytes) + MAC (6 bytes) + ethernet type (2 bytes)
      //     + ethernet data (IPv4 header + ICMP header)
      // Keep at it for 'timeout' seconds, or until we get an ICMP reply.

      // RECEIVE LOOP
      for (;;) {

        memset (recv_ether_frame, 0, IP_MAXPACKET * sizeof (uint8_t));
        memset (&from, 0, sizeof (from));
        fromlen = sizeof (from);
        if ((bytes = recvfrom (recvsd, recv_ether_frame, IP_MAXPACKET, 0, (struct sockaddr *) &from, &fromlen)) < 0) {

          status = errno;

          // Deal with error conditions first.
          if (status == EAGAIN) {  // EAGAIN = 11
            printf ("No reply within %i seconds.\n", timeout);
            trycount++;
            break;  // Break out of Receive loop.
          } else if (status == EINTR) {  // EINTR = 4
            continue;  // Something weird happened, but let's keep listening.
          } else {
            perror ("recvfrom() failed ");
            exit (EXIT_FAILURE);
          }
        }  // End of error handling conditionals.

        // Check for an IP ethernet frame, carrying ICMP echo reply. If not, ignore and keep listening.
        if ((((recv_ether_frame[12] << 8) + recv_ether_frame[13]) == ETH_P_IP) &&
           (recv_iphdr->ip_p == IPPROTO_ICMP) && (recv_icmphdr->icmp_type == ICMP_ECHOREPLY) && (recv_icmphdr->icmp_code == 0)) {

          // Stop timer and calculate how long it took to get a reply.
          (void) gettimeofday (&t2, &tz);
          dt = (double) (t2.tv_sec - t1.tv_sec) * 1000.0 + (double) (t2.tv_usec - t1.tv_usec) / 1000.0;

          // Extract source IP address from received ethernet frame.
          if (inet_ntop (AF_INET, &(recv_iphdr->ip_src.s_addr), rec_ip, INET_ADDRSTRLEN) == NULL) {
            status = errno;
            fprintf (stderr, "inet_ntop() failed.\nError message: %s", strerror (status));
            exit (EXIT_FAILURE);
          }

          // Report source IPv4 address and time for reply.
          printf ("%s  %g ms (%i bytes received)\n", rec_ip, dt, bytes);
          done = 1;
          break;  // Break out of Receive loop.
        }  // End if IP ethernet frame carrying ICMP_ECHOREPLY
      }  // End of Receive loop.

      // The 'done' flag was set because an echo reply was received; break out of send loop.
      if (done == 1) {
        break;  // Break out of Send loop.
      }

      // We ran out of tries, so let's give up.
      if (trycount == trylim) {
        printf ("Recognized no echo replies from remote host after %i tries.\n", trylim);
        break;
      } */

    }  // End of Send loop.

  }
  printf("Done pinging\n");

}

void process_recvd_tour_list(tour_list_node_t* tour_list_node, int list_len) {
  int i, cur;
  struct sockaddr_in multicast_sockaddr;
  char vm_ip[MAX_IP_LEN];
  char *token;
  unsigned int mul_port;
  socklen_t len;
  char mul_msg[100], my_hostname[MAXLINE];

  // Join multicast grp
  bzero(&multicast_sockaddr, sizeof(multicast_sockaddr));
  multicast_sockaddr.sin_family = AF_INET;
  strcpy(vm_ip, tour_list_node[0].node_ip);
  token = strtok(vm_ip, ":");
  Inet_pton (AF_INET, token, &multicast_sockaddr.sin_addr.s_addr);
  token = strtok(NULL, ":");
  mul_port = atoi(token);
  printf("mul_port: %d\n", mul_port);
  multicast_sockaddr.sin_port = htons(mul_port);
  printf("sock_ntop: %s\n", Sock_ntop((SA*)&multicast_sockaddr, sizeof(multicast_sockaddr)));
  len = sizeof(multicast_sockaddr);
  Mcast_join(multi_recv, (SA*)&multicast_sockaddr, len, NULL, 0);
  printf("Mcast_join returned\n");


  // start ICMP
  //pinging();

  // go to cur and update it.
  for (i = 0; i < list_len; i++) {
    if (tour_list_node[i].is_cur == YES) {
      cur = i + 1;
      tour_list_node[i].is_cur = NO;
    }
  }
  if (cur == list_len - 1) {
    // send msg to multicast grp.
    /*printf("Sending a multicast_msg\n");
    gethostname(my_hostname, sizeof(my_hostname));
    sprintf(mul_msg, "<<<<< This is node %s. Tour has ended. Group memebers please identify yourselves.>>>>>", my_hostname);
    mul_msg[strlen(mul_msg)] = 0;
    printf("Node %s. Sending: %s\n", my_hostname, mul_msg);
    Sendto(multi_send, mul_msg, strlen(mul_msg), 0, (SA*)&multicast_sockaddr, len);*/
    return;
  }
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
  ip_hdr.ip_id = htons (USID_PROTO2);

  ip_hdr.ip_off = 0;

  // Time-to-Live (8 bits): default to maximum value
  ip_hdr.ip_ttl = 255;

  // Transport layer protocol (8 bits): 1 for ICMP
  ip_hdr.ip_p = USID_PROTO2;

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

void add_host_to_ping_list(char* name) {
  printf("Add_host\n");
  int i;
  for (i = 0; i < ping_list_len; i++) {
    if (strcmp(name, ping_list[i].hostname) == 0) {
      return;
    } 
  }
  printf("Adding host\n");
  ping_list_len++;
  strcpy(ping_list[ping_list_len - 1].hostname, name);
}

int main(int argc, char *argv[]) {

  struct ip ip_hdr;
  void *buf;
  struct sockaddr_in recvd, recv_multi;
  struct hostent *hptr;
  const int on = 1;
  int status, maxfdp1, n, list_len, nready;
  fd_set cur_set, org_set;
  time_t ticks;
  socklen_t len;
  char str[INET_ADDRSTRLEN];
  char mul_msg[100], my_hostname[MAXLINE];
  struct timeval tv;
  tour_list_node_t* tour_list_node;
  rt = socket(AF_INET, SOCK_RAW, USID_PROTO2);
  pg = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  multi_send = socket(AF_INET, SOCK_DGRAM, 0);
  multi_recv = socket(AF_INET, SOCK_DGRAM, 0);
  if (setsockopt(rt, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
    printf("setsockpot for rt socket error.\n");
    exit(0);
  }
  Setsockopt(multi_send, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  Setsockopt(multi_recv, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  Setsockopt(rt, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  Setsockopt(pg, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  // Bind the multi_recv socket
  bzero(&recv_multi, sizeof(recv_multi));
  recv_multi.sin_family = AF_INET;
  recv_multi.sin_port = htons(MUL_PORT);
  Inet_pton (AF_INET, MUL_GRP_IP, &recv_multi.sin_addr.s_addr);
  Bind(multi_recv, (SA*)&recv_multi, sizeof(recv_multi));

  bzero(&recvd, sizeof(recvd));
  if (argc >= 2) {
    make_list(argc, argv);
  }
  FD_ZERO(&org_set);
  FD_ZERO(&cur_set);
  FD_SET(rt, &org_set);
  FD_SET(pg, &org_set);
  FD_SET(multi_recv, &org_set);
  maxfdp1 = rt > pg ? (rt) : (pg);
  maxfdp1 = multi_recv > maxfdp1 ? (multi_recv + 1) : (maxfdp1 + 1);
  

  
  for (; ;) {
    cur_set = org_set;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    printf("waiting in select\n");
    nready = Select(maxfdp1, &cur_set, NULL, NULL, &tv);
    if (nready == 0) {
      //printf("---Time_to_ping---\n");
      pinging();
    }
    if (FD_ISSET(rt, &cur_set)) {
      len = sizeof(recvd); // always initialize len

      n = recvfrom(rt, &ip_hdr, sizeof(ip_hdr), MSG_PEEK, (SA*) &recvd, &len);

      if (ntohs(ip_hdr.ip_id) == USID_PROTO2) {
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
        add_host_to_ping_list(hptr->h_name);
      }
    }

    /*if (FD_ISSET(multi_recv, &cur_set)) {
      len = sizeof(recvd); // always initialize len
      n = recvfrom(multi_recv, mul_msg, 100, 0, (SA*) &recvd, &len);
      gethostname(my_hostname, sizeof(my_hostname));
      printf("Node %s. Received %s\n", my_hostname, mul_msg);
      // stop pinging
      // send second/response multicast msg
      sprintf(mul_msg, "<<<<<Node %s. I am a member of the group.>>>>>", my_hostname);
      mul_msg[strlen(mul_msg)] = 0;
      printf("Node %s. Sending: %s\n", my_hostname, mul_msg);
      Sendto(multi_send, mul_msg, strlen(mul_msg), 0, (SA*)&recv_multi, len);
      break;
    }*/
  }
  printf("outside_first_select\n");
  FD_ZERO(&org_set);
  FD_ZERO(&cur_set);
  FD_SET(multi_recv, &org_set);
  maxfdp1 = multi_recv + 1;
  tv.tv_sec = 5;
  tv.tv_usec = 0;

  for (; ;) {
    cur_set = org_set;
    nready = Select(maxfdp1, &cur_set, NULL, NULL, &tv);

    if (nready == 0) {
      printf("5 seconds completed. Terminating Tour application. Goodbye!\n");
      return;
    }

    if (FD_ISSET(multi_recv, &cur_set)) {
      len = sizeof(recvd); // always initialize len
      n = recvfrom(multi_recv, mul_msg, 100, 0, (SA*) &recvd, &len);
      gethostname(my_hostname, sizeof(my_hostname));
      printf("Node %s. Received %s\n", my_hostname, mul_msg);
    }
  }
  return 0;
}
