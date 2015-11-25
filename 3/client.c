#include "header.h"

static ctx_t ctx;
static sigjmp_buf waitbuf;

int
create_sock_file(int sockfd)
{
  FILE  *fp                 = NULL;
  char  sockfile[PATH_MAX]  = {0,};
  int   fd                  = -1;

  strncpy(sockfile, SOCK_DIR, strlen(SOCK_DIR));
  strncat(sockfile, MKOSTEMP_SFX, strlen(MKOSTEMP_SFX));

  if ((fd = mkostemp(sockfile, O_TRUNC)) < 0) {
    fprintf (stderr, "Failed to create directory(%s): %s\n",
              sockfile, strerror(errno));
    return -1;
  }

  strncpy (ctx.sockfile, sockfile, strlen(sockfile));

  if (!(fp = fdopen (fd, "w"))) {
    fprintf (stderr, "Error opening file(%s): %s\n",
              sockfile, strerror(errno));
    return -1;
  }

  if (lockf(fileno(fp), F_TLOCK, 0) < 0) {
    fprintf (stderr, "Error locking the file(%s): %s\n",
              sockfile, strerror(errno));
    return -1;
  }

  fprintf (fp, "%d\n", sockfd);

  if (lockf(fileno(fp), F_ULOCK, 0) < 0) {
    fprintf (stderr, "Error unlocking the file(%s): %s\n",
              sockfile, strerror(errno));
    return -1;
  }

  fclose(fp);
  close(fd);
  return 0;
}

int
create_and_bind_socket()
{
  int                 sockfd;
  struct sockaddr_un  servaddr;

  sockfd = Socket (AF_LOCAL, SOCK_DGRAM, 0);

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sun_family = AF_LOCAL;
  strcpy(servaddr.sun_path, tmpnam(NULL)); /* TO-DO (@any) - use mkstemp instead of this deprecated tmpnam */

  Bind(sockfd, (SA *) &servaddr, sizeof(servaddr));

  return sockfd; /* TO-DO (@any) - see what pointers to return in arguments, if any.*/
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
    /*if (!(pinfo = get_peerinfo(&ctx, vmno))) {
      fprintf (stderr, "Entered value out of range. Try again.\n");
      continue;
    }*/
resend:
    get_ip_of_vm(vmno, vm_ip, sizeof(vm_ip));
    printf("IP of VM%d is %s\n", vmno, vm_ip);
    gethostname(my_hostname, sizeof(my_hostname));
    //printf("My hostname = %s\n", my_hostname);

    fprintf (stdout, "TRACE: client at node %s sending request to server "
              "at vm%d\n", my_hostname, vmno);
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
      fprintf (stdout, "TRACE: client at node vm %d timeout on response from "
                "server at vm %d\n", MYID, vmno);
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
    //goto out;
  }

  /*if ((ret = create_sock_file(ctx.sockfd)) < 0) {
    fprintf (stderr, "Failed to create and bind sock fd.\n");
    ret = ctx.sockfd;
    goto out;
  }*/
  printf("Going to do_repeated_task()\n");
  do_repeated_task(sockfd);

//out:
  //cleanup_sock_file(ctx.sockfile);
  return 0;
}
