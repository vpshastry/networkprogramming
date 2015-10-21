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
	build_inferface_info(ii, &interface_info_len, 1);
	print_interface_info(ii, interface_info_len);
	return 0;
}
