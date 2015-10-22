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

void set_ip_server_and_ip_client(struct sockaddr_in *servaddr, struct sockaddr_in *clientaddr, interface_info_t *ii, size_t interface_info_len) {

	int i, is_local;
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
	is_local = 0;
	for (i = 0; i < interface_info_len; i++) {
		client_ip = ii[i].ip->sin_addr;
		netmask = ii[i].netmask->sin_addr;
		if ((servaddr->sin_addr.s_addr & netmask.s_addr) == (client_ip.s_addr & netmask.s_addr)) {
				is_local = 1;
				if (netmask.s_addr > longest_prefix.s_addr) {
					longest_prefix.s_addr = netmask.s_addr;
					clientaddr->sin_addr = client_ip;
				}
		}
	}
	if (is_local == 1) {
		printf("Client and Server are on same subnet\n");
	}
	else {
		printf("Client and Server are not local");
		clientaddr->sin_addr = ii[i-1].ip->sin_addr; // is this correct. What about loopback, is that valid for clientIP? TODO(@aashray)
	}
}

int
main(int argc, char *argv[]) {	
	input_t input = {0,};
	struct sockaddr_in servaddr, clientaddr;
	interface_info_t ii[MAX_INTERFACE_INFO] = {0,};
	size_t interface_info_len;
	char str[INET_ADDRSTRLEN];
	readargsfromfile(&input);
	
	bzero(&servaddr, sizeof(servaddr));
	bzero(&clientaddr, sizeof(clientaddr));
	
	build_inferface_info(ii, &interface_info_len, 0);
  	print_interface_info(ii, interface_info_len);

	if(inet_pton(AF_INET, input.server_ip, &servaddr.sin_addr) != 1){
		err_quit("client.in does not cotain a valid server IP. Please correct and try again.");
	}
	
	set_ip_server_and_ip_client(&servaddr, &clientaddr, ii, interface_info_len);
	
	printf("IPclient:%s\n", Inet_ntop(AF_INET, &clientaddr.sin_addr, str, INET_ADDRSTRLEN));
	printf("IPserver:%s\n", Inet_ntop(AF_INET, &servaddr.sin_addr, str, INET_ADDRSTRLEN));
    
	return 0;
}
