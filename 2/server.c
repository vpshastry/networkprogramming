#include "header.h"

extern struct ifi_info *Get_ifi_info_plus(int family, int doaliases);
extern        void      free_ifi_info_plus(struct ifi_info *ifihead);

typedef struct {
  int sockfd;
  struct sockaddr_in *ip;
  struct sockaddr_in *netmask;
  struct in_addr subnet;
} interface_info_t;

void build_inferface_info(interface_info_t *ii, size_t *interface_info_len) {
	int                 sockfd;
	int curii = 0;
	const int           on = 1;
	struct ifi_info     *ifi, *ifihead;
	struct sockaddr_in  *sa, cliaddr, wildaddr;

	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
		 ifi != NULL; ifi = ifi->ifi_next) {

			/*4bind unicast address */
		ii[curii].sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
		Setsockopt(ii[curii].sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

		ii[curii].ip = (struct sockaddr_in *) ifi->ifi_addr;
		ii[curii].netmask = (struct sockaddr_in *) ifi->ifi_ntmaddr;
		//ii[curii].subnet = (struct sockaddr_in *) ifi->ifi_addr;
		ii[curii].subnet.s_addr = ii[curii].ip->sin_addr.s_addr & ii[curii].netmask->sin_addr.s_addr;
		ii[curii].ip->sin_family = AF_INET;
		ii[curii].ip->sin_port = htons(SERV_PORT);
		Bind(ii[curii].sockfd, (SA *) ii[curii].ip, sizeof(*(ii[curii].ip)));
		//printf("bound %s\n", Sock_ntop((SA *) ii[curii].ip, sizeof(*(ii[curii].ip))));
		curii++;
	}
	*interface_info_len = curii;
	return;
}

int
readargsfromfile(unsigned int *portnumber, unsigned int *maxslidewindowsize)
{
  FILE *fd;
  char line[512];
  if (!(fd = fopen("server.in", "r"))) {
    err_sys("Opening file failed"); // err_sys prints errno's explaination as well.
    return -1;
  }
  fgets(line, 512, fd);
  sscanf(line, "%d", portnumber);
  fgets(line, 512, fd);
  sscanf(line, "%d", maxslidewindowsize);
  return 0;
}
void print_interface_info(interface_info_t *ii, size_t interface_info_len) {
	
	int i;
	char str[INET_ADDRSTRLEN];
	for (i = 0; i < interface_info_len; i++) {
		//printf("fd:%d\n", ii[i].sockfd);
		printf("IP addr: %s\n",
                        Sock_ntop_host((SA *)ii[i].ip, sizeof(*(ii[i].ip))));
		printf("Network Mask: %s\n",
                        Sock_ntop_host((SA *)ii[i].netmask, sizeof(*(ii[i].netmask))));
		
		printf("Subnet Address: %s\n\n",
                        Inet_ntop(AF_INET, &ii[i].subnet, str, INET_ADDRSTRLEN));
	}
}

/*
int
listen(interface_info_t *ii, int length)
{
  int i = 0;
  fd_set rset;
  int maxfd = -100;

  FD_ZERO(rset);
  for (i = 0; i <= length; i++) {
    FD_SET(ii[i].sockfd, &rset);

    if (ii[i].sockfd > maxfd)
      maxfd = ii[i].sockfd;
  }

  // TODO: Listen.
  if (select(maxfd +1, &rset, NULL, NULL, NULL) < 0) {
  }
}
*/

int
main(int argc, char *argv[]) {
	
	unsigned int portnumber;
	unsigned int maxslidewindowsize;
	interface_info_t ii[MAX_INTERFACE_INFO] = {0,};
	size_t interface_info_len;
	if (readargsfromfile(&portnumber, &maxslidewindowsize) == -1) {
		err_quit("Read/parse error from server.in file");
		return -1;
	}
	printf("port num: %d\nmaxslidewinsize:%d\n\n", portnumber, maxslidewindowsize);
	build_inferface_info(ii, &interface_info_len);
	print_interface_info(ii, interface_info_len);
	return 0;
}
