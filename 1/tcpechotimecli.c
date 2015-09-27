#define _GNU_SOURCE
#include "header.h"

struct in_addr *
process_commandline(int argc, char *argv[])
{
  struct in_addr *inaddr = (struct in_addr *) malloc(sizeof(struct in_addr));
  char *ip_str = argv[1];
  char *fqdn = NULL;
  struct hostent *hostentry = NULL;
  struct hostent lhostent = {0,};
  int i = 0;

  if (inet_pton(AF_INET, ip_str, inaddr) != 1) {
    logit (ERROR, "I don't think passed argument is an IP, trying to resolve assuming as hostname");

    if (!(hostentry = gethostbyname(ip_str))) {
      logit (ERROR, "Passed argument is neither an IP nor a hostname");
      return NULL;
    }
  }

  if (!hostentry) {
    if (!(hostentry = gethostbyaddr((const void *)inaddr, sizeof (struct in6_addr),
                                    AF_INET)))
      logit (ERROR, "Failed get host by addr");

  } else {
    if (inet_pton(AF_INET, hostentry->h_addr_list[0], inaddr) != 1) {
      logit (ERROR, "Error converting hostentry to binary ip format");
      return NULL;
    }
  }
  memcpy (&lhostent, hostentry, sizeof(struct hostent));

  printf ("The server host is:\nName: %s\nOther aliases: ", lhostent.h_name);
  for (i = 0; lhostent.h_aliases[i]; ++i)
    printf ("%s, ", lhostent.h_aliases[i]);
  printf ("\nIP addresses: ");
  for (i = 0; lhostent.h_addr_list[i]; ++i)
    printf ("%s, ", lhostent.h_addr_list[i]);

  return inaddr;
}

void
handle_request(struct in_addr *ip, int choice)
{
  int pipefd[2] = {0,};
  int ret = 0;
  char *binary = NULL;
  char buf[MAX_BUF_SIZE] = {0,};
  int fd[2] = {-1,};
  char *ip_str = (char *) malloc (MAX_ARRAY_SIZE);
  char *pipe_str = (char *) malloc (MAX_ARRAY_SIZE);

  if (!inet_ntop(AF_INET, ip, ip_str, MAX_ARRAY_SIZE)) {
    logit (ERROR, "binary to text conversion of IP failed");
    return;
  }

  if (pipe(pipefd) == -1) {
    logit (ERROR, "pipe creation failed");
    return;
  }

  switch (fork()) {
    case 0:
      close(pipefd[0]);
      if (dup2(pipefd[1], 1) < 0)
        logit (ERROR, "Dup2 failed");

      switch (choice) {
        case 1:
          binary = "./echocli";
          break;

        case 2:
          binary = "./timecli";
          break;

        default:
          logit (ERROR, "Invalid choice %d. Exiting...");
          exit(-1);
      }

      snprintf (pipe_str, MAX_ARRAY_SIZE, "%d", pipefd[1]);
      execlp("xterm", "xterm", "-e", binary, ip_str, pipe_str, (char *) 0);

    case -1:
      // Error

    default:
      close (fd[1]);

      wait(NULL);
      /*
      while (read(pipefd[0], buf, MAX_BUF_SIZE) != 0 * EOF *)
        logit(INFO, buf);
        */

      close (pipefd[0]);
  }
  close(pipefd[1]);
}

int
process(struct in_addr *ip)
{
  int choice;

  while (42) {
    printf ("\n1. echo server\n2. time server\n3. Exit\nEnter your choice: ");
    if (scanf ("%d", &choice) == -100) {
      logit (ERROR, "User input error to scanf");
      continue;
    }

    switch (choice) {
      case 1:
      case 2:
        handle_request(ip, choice);
        break;

      case 3:
        printf ("Exiting...\n");
        exit(0);

      default:
        printf ("Invalid choice\n");
        break;
    }
    fflush (stdin);
  }
}

int
main (int argc, char *argv[])
{
  if (argc != 2) {
    printf("Usage: %s <server-addr>", argv[0]);
    return 0;
  }

  process(process_commandline (argc, argv));

  return 0;
}
