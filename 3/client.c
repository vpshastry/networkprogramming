#include "header.h"

static ctx_t ctx;
static sigjmp_buf waitbuf;
char  sockfile[PATH_MAX]  = {0,};

int
create_and_bind_socket()
{
  int                 sockfd;
  struct sockaddr_un  servaddr;
  int                 fd = -1;

  sockfd = Socket (AF_LOCAL, SOCK_DGRAM, 0);

  strncpy(sockfile, TMP_TEMPLATE, strlen(TMP_TEMPLATE));
  if ((fd = mkstemp(sockfile)) < 0) {
    fprintf (stderr, "Failed to create directory(%s): %s\n",
              sockfile, strerror(errno));
    return -1;
  }

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sun_family = AF_LOCAL;
  strcpy(servaddr.sun_path, sockfile);

  Bind(sockfd, (SA *) &servaddr, sizeof(servaddr));

  close(fd);
  return sockfd;
}

static void
sig_alrm(int signo)
{
  siglongjmp(waitbuf, 1);
}

void
mysetitimer(int waittime /* in micro seconds */)
{
  struct itimerval timer;
  timer.it_value.tv_usec = waittime / 1000000;
  timer.it_value.tv_sec = waittime % 1000000;
  timer.it_interval.tv_sec = timer.it_interval.tv_usec = 0;
  setitimer(ITIMER_REAL, &timer, NULL);
}

int
do_repeated_task(int sockfd)
{
  int         vm                    = -1;
  int         vmno                  = -1;
  int         resendcnt             = 0;
  int         src_port;
  char        ch                    = '\0';
  char        in[MAXLINE +1]        = {0,};
  char        inmsg[PAYLOAD_SIZE]   = {0,};
  peerinfo_t  *pinfo                = NULL;
  char        payload[PAYLOAD_SIZE] = {0,};
  char        vm_ip[MAX_IP_LEN];
  char        my_hostname[MAXLINE];
  char        buffer[200];
  char        src_ip[MAX_IP_LEN];
  struct sigaction sa;

  sa.sa_handler = &sig_alrm;
  sigaction (SIGALRM, &sa, NULL);

  while (42) {
    //fflush(stdin); why?
    fprintf (stdout, "Choose the server from vm1..vm10 (1-10 or e to exit): ");
    if (fgets(in, MAXLINE, stdin) != NULL){
      printf("You entered %s\n", in);
    } else {
      printf("fgets error\n");
      continue;
    }
    if (strncmp (in, "e", strlen(in)) == 0) {
      fprintf (stdout, "\nExiting... Bye.\n");
      return 0;
    }

    vmno = atoi(in);

resend:
    get_ip_of_vm(vmno, vm_ip, sizeof(vm_ip));
    printf("IP of VM%d is %s\n", vmno, vm_ip);
    gethostname(my_hostname, sizeof(my_hostname));

    fprintf (stdout, "TRACE: client at node %s sending request to server "
              "at vm%d\n", my_hostname, vmno);
    mysetitimer(1 *1000000);
    if (msg_send(sockfd, vm_ip, SERVER_PORT, "AB", 0) < 0) {
      fprintf (stderr, "Failed to send message: %s\n", strerror(errno));
      return -1;
    }

    printf("Message Sent\n");
    if (sigsetjmp(waitbuf, 1) != 0) {
      if (resendcnt >= MAX_RESEND) {
        fprintf (stderr, "Max resend count reached, %d times. No more "
                  "trying.\n", resendcnt);
        continue;
      }

      resendcnt ++;
      fprintf (stdout, "TRACE: client at node vm %s timeout on response from "
                "server at vm %d\n", my_hostname, vmno);
      goto resend;
    }

    if (msg_recv(sockfd, buffer, src_ip, &src_port) < 0) {
      fprintf (stderr, "Failed to receive message: %s\n", strerror(errno));
      return -1;
    }
    printf("I recvd %s\n", buffer);
  }
}

int
main(int *argc, char *argv[])
{
  int ret, sockfd;

  if ((sockfd= create_and_bind_socket()) < 0) {
    fprintf (stdout, "Creating the socket file failed\n");
    goto out;
  }

  printf("Going to do_repeated_task()\n");
  do_repeated_task(sockfd);

out:
  cleanup_sock_file(sockfile);
  return 0;
}
