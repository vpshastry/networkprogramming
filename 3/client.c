#include "header.h"

static ctx_t ctx;
static sigjmp_buf waitbuf;
static void sig_alrm(int signo);
char sockfile[PATH_MAX];

static void
sig_alrm(int signo)
{
  siglongjmp(waitbuf, 1);
}

/* 1. For TIMEOUT from now, 0 for cancel. */
void
mysetitimer(int waittime /* in micro seconds */)
{
  struct itimerval timer;

  timer.it_value.tv_usec = waittime /1000000;
  timer.it_value.tv_sec = waittime % 1000000;
  timer.it_interval.tv_sec = timer.it_interval.tv_usec = 0;

  setitimer(ITIMER_REAL, &timer, NULL);
}

int
create_and_bind_socket()
{
  int                 sockfd;
  struct sockaddr_un  servaddr;

  sockfd = Socket (AF_LOCAL, SOCK_DGRAM, 0);

  strcpy(sockfile, tmpnam(NULL));
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sun_family = AF_LOCAL;
  strcpy(servaddr.sun_path, sockfile); /* TO-DO (@any) - use mkstemp instead of this deprecated tmpnam */
  printf ("Using tempfile %s\n", sockfile);

  Bind(sockfd, (SA *) &servaddr, sizeof(servaddr));

  return sockfd; /* TO-DO (@any) - see what pointers to return in arguments, if any.*/
}

int
do_repeated_task(int sockfd)
{
  int         vm                    = -1;
  int         vmno                  = -1;
  int         reroute               = 0;
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

  /* Install timer_handler as the signal handler for SIGVTALRM. */
  memset (&sa, 0, sizeof (sa));
  sa.sa_handler = &sig_alrm;
  sigaction (SIGALRM, &sa, NULL);

  while (42) {
    reroute = 0;
    resendcnt = 0;
    //fflush(stdin); why?
    fprintf (stdout, "Choose the server from vm1..vm10 (1-10 or e to exit): ");
    if (fgets(in, MAXLINE, stdin) != NULL){
      printf("You entered %s\n", in);
    } else {
      printf("fgets error\n");
      continue;
    }
    if (strncmp (in, "e", 1) == 0) {
      fprintf (stdout, "\nExiting... Bye.\n");
      return 0;
    }

    vmno = atoi(in);
resend:
    get_ip_of_vm(vmno, vm_ip, sizeof(vm_ip));
    gethostname(my_hostname, sizeof(my_hostname));

    fprintf (stdout, "TRACE: client at node %s sending request to server "
              "at vm%d(%s)\n", my_hostname, vmno, vm_ip);
    if (msg_send(sockfd, vm_ip, SERVER_PORT, "AB", reroute) < 0) {
      fprintf (stderr, "Failed to send message: %s\n", strerror(errno));
      return -1;
    }

    mysetitimer(CLIENT_TIMEOUT);

    if (sigsetjmp(waitbuf, 1) != 0) {
      if (resendcnt >= MAX_RESEND) {
        fprintf (stderr, "Max resend count reached, %d times. No more "
                  "trying.\n", resendcnt);
        mysetitimer(0);
        continue;
      }

      resendcnt ++;
      fprintf (stdout, "TRACE: client at node vm %d timeout on response from "
                "server at vm %d\n", MYID, vmno);
      reroute = 1;
      goto resend;
    }

    if (msg_recv(sockfd, buffer, src_ip, &src_port) < 0) {
      fprintf (stderr, "Failed to receive message: %s\n", strerror(errno));
      return -1;
    }
    printf("TRACE: I recvd %s\n", buffer);
    mysetitimer(0);
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
  Unlink(sockfile);
  return 0;
}
