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

int find_if_client_local(struct sockaddr_in servaddr, struct sockaddr_in clientaddr, interface_info_t *ii, size_t interface_info_len, struct sockaddr_in recv_netmask) {

// return 1 -> loopback;
// return 2 -> local;
// else return 3.

    int i;
    struct in_addr client_ip;
    //struct sockaddr_in netmask;
    struct in_addr temp, netmask, longest_prefix;
    char str[INET_ADDRSTRLEN];

	inet_pton(AF_INET, "127.0.0.1", &temp);

    if (compare_ips(servaddr.sin_addr, temp) == 1) {
		return 1;
    }

    // Not on same host, check if they are on same subnet now.

	netmask = recv_netmask.sin_addr;
    if ((servaddr.sin_addr.s_addr & netmask.s_addr) == (clientaddr.sin_addr.s_addr & netmask.s_addr)) {
		return 2;
	}
	return 3;
}

int
main(int argc, char *argv[]) {

	unsigned int portnumber;
	unsigned int maxslidewindowsize;
	interface_info_t ii[MAX_INTERFACE_INFO] = {0,};
	size_t interface_info_len;
	fd_set rset, mainset;
	int maxfdp1, i, n, mysockfd, is_local, pid, client_sockfd, temp_port, new_client_conn;
	const int do_not_route = 1, on = 1;
	char str[INET_ADDRSTRLEN], msg[MAXLINE];
	struct sockaddr_in cliaddr, my_recv_addr, my_recv_netmask, cli_conn;
	socklen_t len = sizeof(cliaddr);
        char filename[4096] = {0,};

	bzero(&cliaddr, sizeof(cliaddr));
	bzero(&cli_conn, sizeof(cli_conn));
	bzero(&my_recv_addr, sizeof(my_recv_addr));

	//cliaddr = Malloc(sizeof(*(ii[0].ip)));

	if (readargsfromfile(&portnumber, &maxslidewindowsize) == -1) {
		err_quit("Read/parse error from server.in file");
		return -1;
	}
	printf("port num: %d\nmaxslidewinsize:%d\n\n", portnumber, maxslidewindowsize);

	build_inferface_info(ii, &interface_info_len, 1, portnumber);
	print_interface_info(ii, interface_info_len);

	build_fd_set(&mainset, ii, interface_info_len, &maxfdp1);
	//printf("maxfdp1=%d", maxfdp1);
	for ( ; ; ) {
		rset = mainset;
		Select(maxfdp1, &rset, NULL, NULL, NULL);
		for (i = 0; i < interface_info_len; i++) {
			if (FD_ISSET(ii[i].sockfd, &rset)) {
				printf("Data recieved on %s\n", Sock_ntop((SA*) (ii[i].ip), sizeof(*(ii[i].ip))));
				mysockfd = ii[i].sockfd;
				my_recv_addr = *(ii[i].ip);
				my_recv_netmask = *(ii[i].netmask);
				n = Recvfrom(ii[i].sockfd, msg, MAXLINE, 0, (SA*) &cliaddr, &len);
				printf("Data recieved from: %s\n", Sock_ntop((SA*) &cliaddr, len));
                                memcpy(filename, msg, 4096);
                                if (strchr(filename, '\n'))
                                *strchr(filename, '\n') = '\0';
				printf("Data:%s\n", msg);
				//Sendto(ii[i].sockfd, msg, n, 0,(SA*) &cliaddr, len);
				if ((pid = Fork()) == 0) {
					// Child
					// CLose all other sockets.
					for (i = 0; i < interface_info_len; i++)
						if (ii[i].sockfd != mysockfd) close(ii[i].sockfd);
					// Find out if client is loopback or local or not.
					is_local = find_if_client_local(my_recv_addr, cliaddr, ii, interface_info_len, my_recv_netmask);
					if (is_local < 3)
						printf("Client host is local\n");
					else printf("Client host is not local\n");

					// Create UDP socket to handle file transfer with this client."connection socket"
					client_sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
 					if (is_local < 3)
     					Setsockopt(client_sockfd, SOL_SOCKET, SO_DONTROUTE, &do_not_route, sizeof(do_not_route));
 					Setsockopt(client_sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
					cli_conn.sin_addr = my_recv_addr.sin_addr;
 					cli_conn.sin_family = AF_INET;
 					cli_conn.sin_port = htons(0);
					Bind(client_sockfd, (SA *) &cli_conn, sizeof(cli_conn));
					len = sizeof(cli_conn);
					Getsockname(client_sockfd, (SA*) &cli_conn, &len);
					printf("\nConnection socket bound, protocol Address:%s\n", Sock_ntop((SA*) &cli_conn, len));
					//new_client_conn = Socket(AF_INET, SOCK_DGRAM, 0);
					Connect(client_sockfd, (SA*) &cliaddr, sizeof(cliaddr));
					len = sizeof(cliaddr);
					Getpeername(client_sockfd, (SA*) &cliaddr, &len);
					printf("\nServer child connected to client, protocol Address:%s\n", Sock_ntop((SA*) &cliaddr, len));
					temp_port = ntohs(cli_conn.sin_port);
					sprintf(msg, "%d", temp_port);
					Sendto(mysockfd, msg, strlen(msg), 0,(SA*) &cliaddr, len);

					printf("New port sent.");
					//close(mysockfd);
					n = Read(client_sockfd, msg, MAXLINE);
					printf("Msg (ACK) on new port:%s\n", msg);
					sprintf(msg, "Aashray\n");
					Write(client_sockfd, msg, strlen(msg));
					printf("Sent from new port!");

                                        printf ("Trying to read\n");
                                        char mybuf[100] = {0,};
                                        n = Read(client_sockfd, &mybuf, 100);
                                        mybuf[n] = '\0';
                                        printf ("\nRead:%s\n", mybuf);

                                        if (access(filename, F_OK | R_OK)) {
                                          printf ("File not accessible\n");
                                          exit(0);
                                        }

                                        int filefd;
                                        if ((filefd = open(filename, O_RDONLY)) < 0) {
                                          printf ("opening file failed\n");
                                          exit(0);
                                        }

                                        printf ("File opened\n");
                                        char buf[1024];
                                        char inbuf[1024];
                                        while ((n = read(filefd, buf, 512)) != EOF) {
                                          n = Dg_send_recv(client_sockfd, buf, 512, inbuf, 10, (SA *) &cli_conn, sizeof(cli_conn));
                                          inbuf[n] = '\0';
                                          printf ("Received: %s\n", inbuf);
                                        }

					exit(0);// exit child
				}
			}
		}


	}

	return 0;
}
