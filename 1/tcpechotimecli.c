#include <stdio.h>
#include <netdb.h>
#include "header.h"
#include <signal.h>
#include <errno.h>

static volatile int sigchild_received = 0;
static volatile int childpid = -1;

void
intrpthandler(int intrpt)
{
  printf("Received SIGINT, exiting\n");
  kill(childpid, SIGKILL);
  exit(0);
}

void
sigchildhanlder(int intrpt)
{
  sigchild_received = 1;
}

void
sigpipehandler(int intrpt)
{
  printf ("Handling SIGPIPE signal\n");
}

struct in_addr *
process_commandline(int argc, char *argv[])
{
  struct in_addr *inaddr = (struct in_addr *) malloc(sizeof(struct in_addr));
  char *ip_str = argv[1];
  char *fqdn = NULL;
  struct hostent *hostentry = NULL;
  struct hostent lhostent = {0,};
  int i = 0;
  int err;
  struct in_addr **addrlist;

  if ((err = inet_pton(AF_INET, ip_str, inaddr)) != 1) {
    if (err != 0) {
      fprintf (stderr, "inet pton error\n");
      return NULL;
    }

    if (!(hostentry = gethostbyname(ip_str))) {
      fprintf (stderr, "Passed argument is neither an IP nor a hostname\n");
      return NULL;
    }
  }

  if (!hostentry) {
    if (!(hostentry = gethostbyaddr((const void *)inaddr, sizeof (struct in6_addr),
                                    AF_INET)))
      fprintf (stderr, "Failed get host by addr\n");

  } else {
    inaddr = (struct in_addr *)hostentry->h_addr_list[0];
  }
  memcpy (&lhostent, hostentry, sizeof(struct hostent));

  printf ("The server host is:\nName: %s\nOther aliases: ", lhostent.h_name);
  for (i = 0; lhostent.h_aliases[i]; ++i)
    printf ("%s, ", lhostent.h_aliases[i]);
  printf ("\nIP addresses: ");
  addrlist = (struct in_addr **) lhostent.h_addr_list;
  for (i = 0; addrlist[i]; ++i)
    printf ("%s, ", inet_ntoa(*addrlist[i]));
  printf ("\n");

  return inaddr;
}

void
handle_request(struct in_addr *ip, char *binary)
{
  int pipefd[2] = {0,};
  int ret = 0;
  char buf[MAX_BUF_SIZE] = {0,};
  char *ip_str = (char *) malloc(1024);
  char *pipe_str = (char *) malloc (1024);
  fd_set rset;

  if (!inet_ntop(AF_INET, ip, ip_str, 1024)) {
    fprintf (stderr, "Binary to str ip conversion failed\n");
    return;
  }

  signal(SIGCHLD, sigchildhanlder);

  if (pipe(pipefd) == -1) {
    fprintf (stderr, "Pipe creation failed: %s\n", strerror(errno));
    return;
  }

  switch ((childpid = fork())) {
    case 0:
      close (pipefd[0]);

      snprintf (pipe_str, 1024, "%d", pipefd[1]);
      execlp("xterm", "xterm", "-e", binary, ip_str, pipe_str, (char *)0);

    case -1:
      fprintf (stderr, "Error forking child: %s\n", strerror(errno));
      break;

    default:
      close (pipefd[1]);

      memset(buf, 0, sizeof(buf));
      while (!sigchild_received && read(pipefd[0], buf, sizeof(buf)) > 0) {
        printf (buf);
        memset(buf, 0, sizeof(buf));
      }
      sigchild_received = 0;
      wait(NULL);

      close(pipefd[0]);
  }
  free(ip_str);
  free(pipe_str);
}

void
handle_input(struct in_addr *ip)
{
  int choice;
  int i = 0;
  int j;
  char readbuf[1024] = {0,};

  while (++i < 100) {
    printf ("1. Echo server\n2. Time server\n3. Quit\nEnter your choice: ");
    fflush(stdin);
    if (fgets(readbuf, 1024, stdin) != readbuf) {
      fprintf (stderr, "Error inputting\n");
      continue;
    }
    readbuf[1] = '\0';
    sscanf(readbuf, "%d", &choice);
    printf ("Your choice: %d\n", choice);
    if (!isdigit(choice+48))
      choice = 4;

    switch(choice) {
      case 1:
        printf ("Running echo cli. All input to this terminal will be ignored\n");
        handle_request(ip, "./echo_cli");
        break;

      case 2:
        printf ("Running time cli. All input to this terminal will be ignored\n");
        handle_request(ip, "./time_cli");
        break;

      case 3:
        printf ("Exiting...\n");
        exit(0);

      default:
        printf ("Wrong choice, try again\n");
    }
  }
}

int
main(int argc, char *argv[])
{
  if (argc != 2) {
    printf("Usage: %s <server-address>", argv[0]);
    return 0;
  }

  signal(SIGINT, intrpthandler);
  signal(SIGPIPE, sigpipehandler);
  signal(SIGCHLD, sigchildhanlder);

  handle_input(process_commandline(argc, argv));

  return 0;
}
