#include "header.h"

void build_inferface_info(interface_info_t *ii, size_t *interface_info_len, int bind, unsigned int port) {
	int                 sockfd;
	int curii = 0;
	const int           on = 1;
	struct ifi_info     *ifi, *ifihead;
	struct sockaddr_in  *sa, cliaddr, wildaddr;

	for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 1);
		 ifi != NULL; ifi = ifi->ifi_next) {

			/*4bind unicast address */
		if(bind == 1) ii[curii].sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
		if(bind == 1) Setsockopt(ii[curii].sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

		ii[curii].ip = (struct sockaddr_in *) ifi->ifi_addr;
		ii[curii].netmask = (struct sockaddr_in *) ifi->ifi_ntmaddr;
		//ii[curii].subnet = (struct sockaddr_in *) ifi->ifi_addr;
		ii[curii].subnet.s_addr = ii[curii].ip->sin_addr.s_addr & ii[curii].netmask->sin_addr.s_addr;
		ii[curii].ip->sin_family = AF_INET;
		ii[curii].ip->sin_port = htons(port);
		if (bind == 1) Bind(ii[curii].sockfd, (SA *) ii[curii].ip, sizeof(*(ii[curii].ip)));
		//printf("bound %s\n", Sock_ntop((SA *) ii[curii].ip, sizeof(*(ii[curii].ip))));
		curii++;
	}
	*interface_info_len = curii;
	return;
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

int compare_ips(struct in_addr a, struct in_addr b) {
	if (a.s_addr == b.s_addr) return 1;
	else return 0;
}

void
safe_free(void **var)
{
  free(*var);
  *var = NULL;
}

void
fair_lock(fair_lock_t *fairlock)
{
  unsigned long long myturn;

  pthread_mutex_lock(&fairlock->mutex);
  myturn = fairlock->turn_head++;
  while (myturn != fairlock->turn_tail)
    pthread_cond_wait(&fairlock->cond, &fairlock->mutex);
  pthread_mutex_unlock(&fairlock->mutex);
}

void
fair_unlock(fair_lock_t *fairlock)
{
  pthread_mutex_lock(&fairlock->mutex);
  fairlock->turn_tail++;
  pthread_cond_broadcast(&fairlock->cond);
  pthread_mutex_unlock(&fairlock->mutex);
}
