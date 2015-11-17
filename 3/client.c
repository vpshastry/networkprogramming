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
  int                 sockfd      = -1;
  int                 err         = -1;
  struct sockaddr_in  clientaddr  = {0,};

  clientaddr.sin_family = AF_INET;
  clientaddr.sin_port = htons(0);
  if ((err = inet_pton(AF_INET, "0.0.0.0", &clientaddr.sin_addr)) != 1) {
    fprintf (stderr, "inet pton failed for 0.0.0.0\n");
    return -1;
  }

  sockfd = Socket (AF_INET, SOCK_DGRAM, 0);

  Bind(sockfd, (SA *) &clientaddr, sizeof(clientaddr));

  return sockfd;
}

int
do_repeated_task()
{
  int       vm                    = -1;
  int       vmno                  = -1;
  int       resendcnt             = 0;
  int       srcport               = 0;
  char      ch                    = '\0';
  char      in[MAXLINE +1]        = {0,};
  char      inmsg[PAYLOAD_SIZE]   = {0,};
  vminfo_t  *vminfo               = NULL;
  char      payload[PAYLOAD_SIZE] = {0,};

  while (42) {
    fflush(stdin);
    fprintf (stdin, "Choose the server from vm1..vm10 (1-10 or e to exit): ");
    Fgets(in, MAXLINE, stdin);

    if (strncmp (in, "e", strlen("e"))) {
      fprintf (stdout, "\nExiting... Bye.\n");
      return 0;
    }

    vmno = atoi(in);
    if (!(vminfo = get_vminfo(&ctx, vmno))) {
      fprintf (stderr, "Entered value out of range. Try again.\n");
      continue;
    }

resend:
    if (msg_send(ctx.sockfd, vminfo->ip, vminfo->port, payload, 0) < 0) {
      fprintf (stderr, "Failed to send message: %s\n", strerror(errno));
      return -1;
    }

    fprintf (stdout, "TRACE: client at node vm %d sending request to server "
              "at vm %d\n", MYID, vmno);

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

    if (msg_recv(ctx.sockfd, inmsg, vminfo->ip, &srcport) < 0) {
      fprintf (stderr, "Failed to receive message: %s\n", strerror(errno));
      return -1;
    }
  }
}

int
main(int *argc, char *argv[])
{
  int ret = -1;

  if ((ret = create_and_bind_socket()) < 0) {
    fprintf (stderr, "Creating the socket file failed\n");
    goto out;
  }

  if ((ret = create_sock_file(ctx.sockfd)) < 0) {
    fprintf (stderr, "Failed to create and bind sock fd.\n");
    ret = ctx.sockfd;
    goto out;
  }

  if ((ret = do_repeated_task()))
    goto out;

out:
  cleanup_sock_file(ctx.sockfile);
  return ret;
}
