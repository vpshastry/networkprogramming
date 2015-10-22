#include "header.h"

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

void build_fd_set(fd_set *rset, interface_info_t *ii, size_t interface_info_len, int *maxfdp1) {
	int i, maxfd = 0;
	FD_ZERO(rset);
	for (i = 0; i < interface_info_len; i++) {
		FD_SET(ii[i].sockfd, rset);
		if (ii[i].sockfd > maxfd) maxfd = ii[i].sockfd;
	}
	*maxfdp1 = maxfd + 1;
	
	printf("built fd_set, now going into select loop...\n");
}

int
main(int argc, char *argv[]) {
	
	unsigned int portnumber;
	unsigned int maxslidewindowsize;
	interface_info_t ii[MAX_INTERFACE_INFO] = {0,};
	size_t interface_info_len;
	fd_set rset, mainset;
	int maxfdp1, i, n;
	char str[INET_ADDRSTRLEN], msg[MAXLINE];
	struct sockaddr_in cliaddr;
	socklen_t len;

	//cliaddr = Malloc(sizeof(*(ii[0].ip)));

	if (readargsfromfile(&portnumber, &maxslidewindowsize) == -1) {
		err_quit("Read/parse error from server.in file");
		return -1;
	}
	printf("port num: %d\nmaxslidewinsize:%d\n\n", portnumber, maxslidewindowsize);
	
	build_inferface_info(ii, &interface_info_len, 1);
	print_interface_info(ii, interface_info_len);
	
	build_fd_set(&mainset, ii, interface_info_len, &maxfdp1);
	//printf("maxfdp1=%d", maxfdp1);
	for ( ; ; ) {
		rset = mainset;
		Select(maxfdp1, &rset, NULL, NULL, NULL);
		for (i = 0; i < interface_info_len; i++) {
			if (FD_ISSET(ii[i].sockfd, &rset)) {
				printf("Data recieved on %s\n", Inet_ntop(AF_INET, &(ii[i].ip->sin_addr), str, INET_ADDRSTRLEN));
				n = Recvfrom(ii[i].sockfd, msg, MAXLINE, 0, (SA*) &cliaddr, &len);
				printf("Data recieved from: %s\n", Sock_ntop((SA*) &cliaddr, len));
				printf("Data:%s\n", msg);
			}
		}
	}
	
	return 0;
}
