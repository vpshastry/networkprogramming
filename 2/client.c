#include "header.h"

typedef struct {
  char server_ip[100];
  unsigned int portnumber;
  char filename[100000];
  unsigned int recvslidewindowsize; // In # of datagrams
  unsigned int seedvalue;
  float p; // probability
  unsigned long mean; // In milliseconds
} input_t;

int
receive_file(int sockfd)
{
  int n;
  /* Remove and add proper receiver window logic here */
  static struct msghdr	msgsend, msgrecv;
  struct iovec	iovsend[2], iovrecv[2];
  char *outbuff = "done!";
  size_t outbytes = strlen(outbuff);
  char inbuff[4096];
  int inbytes = 4096;
  seq_header_t sendhdr, recvhdr;
  msgsend.msg_name = NULL;
  msgsend.msg_namelen = 0;
  msgsend.msg_iov = iovsend;
  msgsend.msg_iovlen = 1;
  iovsend[0].iov_base = (void *)&sendhdr;
  iovsend[0].iov_len = sizeof(seq_header_t);
  msgrecv.msg_name = NULL;
  msgrecv.msg_namelen = 0;
  msgrecv.msg_iov = iovrecv;
  msgrecv.msg_iovlen = 2;
  iovrecv[0].iov_base = (void *)&recvhdr;
  iovrecv[0].iov_len = sizeof(seq_header_t);
  iovrecv[1].iov_base = inbuff;
  iovrecv[1].iov_len = inbytes;

  printf ("Waiting for server to send something\n");
  while (42) {
    msgrecv.msg_name = NULL;
    msgrecv.msg_namelen = 0;
    msgrecv.msg_iov = iovrecv;
    msgrecv.msg_iovlen = 2;
    iovrecv[0].iov_base = (void *)&recvhdr;
    iovrecv[0].iov_len = sizeof(seq_header_t);
    iovrecv[1].iov_base = inbuff;
    iovrecv[1].iov_len = inbytes;

    n = Recvmsg(sockfd, &msgrecv, 0);
    printf ("Wait is over\n");
    inbuff[iovrecv[1].iov_len] = 0;
    printf ("%s\n", inbuff);
    sendhdr.seq = recvhdr.seq +1;
    if (recvhdr.fin)
      sendhdr.fin = 1;
    printf ("Sending ack for: %d\n", recvhdr.seq);

    Sendmsg(sockfd, &msgsend, 0);
    printf ("Sent\n");
    if (recvhdr.fin)
      break;
  }
  return 0;
}

int
readargsfromfile(input_t *input)
{
  FILE *fd;
  char line[512], ip[512];
  int i;
  if (!(fd = fopen("client.in", "r"))) {
    err_sys("Opening file failed"); // err_sys prints errno's explaination as well.
    return -1;
  }
  fgets(line, 512, fd);
  sscanf(line, "%s", input->server_ip);
  printf("%s\n", input->server_ip);
  fgets(line, 512, fd);
  sscanf(line, "%u", &input->portnumber);
  printf("%u\n", input->portnumber);
  fgets(line, 512, fd);
  sscanf(line, "%s", input->filename);
  printf("%s\n", input->filename);
  fgets(line, 512, fd);
  sscanf(line, "%u", &input->recvslidewindowsize);
  printf("%u\n", input->recvslidewindowsize);
  fgets(line, 512, fd);
  sscanf(line, "%u", &input->seedvalue);
  printf("%u\n", input->seedvalue);
  fgets(line, 512, fd);
  sscanf(line, "%f", &input->p);
  if (input->p > 1.0 || input->p < 0.0) {
	  err_quit("Probability has to be between 0.0 and 1.0, fix client.in and try again.");
  }
  printf("%f\n", input->p);
  fgets(line, 512, fd);
  sscanf(line, "%lu", &input->mean);
  printf("%lu\n", input->mean);
  return 0;
}

void set_ip_server_and_ip_client(struct sockaddr_in *servaddr, struct sockaddr_in *clientaddr, interface_info_t *ii, size_t interface_info_len, int *is_local) {

	int i;
	struct in_addr client_ip;
	//struct sockaddr_in netmask;
	struct in_addr temp, netmask, longest_prefix;
	char str[INET_ADDRSTRLEN];

	for (i = 0; i < interface_info_len; i++) {
		client_ip = ii[i].ip->sin_addr;
		if (compare_ips(client_ip, servaddr->sin_addr) == 1) {
			printf("Server and Client are on the same host\n");
			inet_pton(AF_INET, "127.0.0.1", &temp);
			servaddr->sin_addr = temp;
			clientaddr->sin_addr = temp;
			return;
		}
	}
	// Not on same host, check if they are on same subnet now.

	inet_pton(AF_INET, "0.0.0.0", &longest_prefix);
	*is_local = 0;
	for (i = 0; i < interface_info_len; i++) {
		client_ip = ii[i].ip->sin_addr;
		netmask = ii[i].netmask->sin_addr;
		if ((servaddr->sin_addr.s_addr & netmask.s_addr) == (client_ip.s_addr & netmask.s_addr)) {
				*is_local = 1;
				if (netmask.s_addr > longest_prefix.s_addr) {
					longest_prefix.s_addr = netmask.s_addr;
					clientaddr->sin_addr = client_ip;
				}
		}
	}
	if (*is_local == 1) {
		printf("Client and Server are on same subnet\n");
	}
	else {
		printf("Client and Server are not local\n");
		clientaddr->sin_addr = ii[i-1].ip->sin_addr; // is this correct. What about loopback, is that valid for clientIP? TODO(@aashray)
	}
}

int
main(int argc, char *argv[]) {
	input_t input = {0,};
	struct sockaddr_in servaddr, clientaddr, client_bind, serv_connect;
	interface_info_t ii[MAX_INTERFACE_INFO] = {0,};
	size_t interface_info_len;
	char str[INET_ADDRSTRLEN + 1], recvline[MAXLINE + 1];
	int n, is_local, sockfd, serv_sock_fd, temp_port;
	const int do_not_route = 1, on = 1;

	socklen_t len = sizeof(clientaddr);
	readargsfromfile(&input);

	bzero(&servaddr, sizeof(servaddr));
	bzero(&clientaddr, sizeof(clientaddr));
	build_inferface_info(ii, &interface_info_len, 0, -1);
  	print_interface_info(ii, interface_info_len);

	if(inet_pton(AF_INET, input.server_ip, &servaddr.sin_addr) != 1){
		err_quit("client.in does not cotain a valid server IP. Please correct and try again.");
	}

	set_ip_server_and_ip_client(&servaddr, &clientaddr, ii, interface_info_len, &is_local);

	printf("IPclient:%s\n", Inet_ntop(AF_INET, &clientaddr.sin_addr, str, INET_ADDRSTRLEN));
	printf("IPserver:%s\n", Inet_ntop(AF_INET, &servaddr.sin_addr, str, INET_ADDRSTRLEN));

	sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
	if (is_local == 1)
		Setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &do_not_route, sizeof(do_not_route));
	Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	clientaddr.sin_family = AF_INET;
	clientaddr.sin_port = htons(0);

	Bind(sockfd, (SA *) &clientaddr, sizeof(clientaddr));
    len = sizeof(clientaddr);
	Getsockname(sockfd, (SA*) &clientaddr, &len);
	printf("\nIPclient bound, protocol Address:%s\n", Sock_ntop((SA*) &clientaddr, len));

	// Connect to IPserver.
	//serv_sock_fd = Socket(AF_INET, SOCK_DGRAM, 0);
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(input.portnumber);
	Connect(sockfd, (SA*) &servaddr, sizeof(servaddr));

	// Getpeername
	len = sizeof(serv_connect);
	Getpeername(sockfd, (SA*) &serv_connect, &len);
	printf("\nIPserver connected, protocol Address:%s\n", Sock_ntop((SA*) &serv_connect, len));
	len = sizeof(clientaddr);
	printf("\nIPclient connected, protocol Address:%s\n", Sock_ntop((SA*) &clientaddr, len));
	// Send the filename
	Write(sockfd, input.filename, strlen(input.filename));
	n = Read(sockfd, recvline, MAXLINE);
	Fputs(recvline, stdout);
	//bzero(&servaddr, sizeof(serv_connect));
	//Sock_pton(recvline, &servaddr);
	sscanf(recvline, "%d", &temp_port);
	servaddr.sin_port = htons(temp_port);
	Connect(sockfd, (SA*) &servaddr, sizeof(servaddr));
	bzero(&serv_connect, sizeof(serv_connect));
	len = sizeof(serv_connect);
	Getpeername(sockfd, (SA*) &serv_connect, &len);
	printf("\nIPserver connected, protocol Address:%s\n", Sock_ntop((SA*) &serv_connect, len));

	Write(sockfd, input.filename, strlen(input.filename));
	n = Read(sockfd, recvline, MAXLINE);
	Fputs(recvline, stdout);


        if (receive_file(sockfd)) {
          printf ("Failed to receive file\n");
          return -1;
        }

	return 0;
}
