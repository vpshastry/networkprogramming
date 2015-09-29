#define _GNU_SOURCE
#include "header.h"

static volatile int sigchild_received = 0;

void
intrpthandler(int intrpt)
{
  printf("Received SIGINT, exiting\n");
  exit(0);
}


void
sigchildhanlder(int sigchild)
{
  printf("Received SIGCHLD\n");
  sigchild_received = 1;
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
handle_request(struct in_addr *ip, char *binary)
{
  int pipefd[2] = {0,};
  int ret = 0;
  char buf[MAX_BUF_SIZE] = {0,};
  int fd[2] = {-1,};
  char *ip_str = (char *) malloc (MAX_ARRAY_SIZE);
  char *pipe_str = (char *) malloc (MAX_ARRAY_SIZE);

  if (!inet_ntop(AF_INET, ip, ip_str, MAX_ARRAY_SIZE)) {
    logit (ERROR, "binary to text conversion of IP failed");
    return;
  }

  signal(SIGINT, sigchildhanlder);

  if (pipe(pipefd) == -1) {
    logit (ERROR, "pipe creation failed");
    return;
  }

  switch (fork()) {
    case 0:
      close(pipefd[0]);
      if (dup2(pipefd[1], 1) < 0)
        logit (ERROR, "Dup2 failed");

      snprintf (pipe_str, MAX_ARRAY_SIZE, "%d", pipefd[1]);
      execlp("xterm", "xterm", "-e", binary, ip_str, pipe_str, (char *) 0);

    case -1:
      // Error

    default:
      close (fd[1]);
      close(pipefd[1]);

      while (!sigchild_received) {
        read(pipefd[0], buf, MAX_BUF_SIZE);
        logit(INFO, buf);
      }

      close (pipefd[0]);
  }
}

int
process(struct in_addr *ip)
{
  char choice;
  char *binary = NULL;
  char line[MAX_BUF_SIZE] = {0,};
  char c;

  while (42) {
    printf ("\ne. echo server\nt. time server\nq. Quit\nEnter your choice: ");
    fflush(stdin);
    scanf("%c", &choice);
    /*
    if (scanf("%c", &choice) == 0) {
      printf("Err. . .\n");
      do {
        c = getchar();
      }
      while (!isalpha(c));
      ungetc(c, stdin);
      printf ("Please enter your choice again: ");
      scanf("%c", &choice);
    }
    */
    while((c = getchar()) != '\n' && c != EOF);
    scanf("%c", &choice);

    printf ("Choice: %c\n", choice);
    switch (choice) {
      case 'e':
        binary = "./echo_cli";
        handle_request(ip, binary);
        break;

      case 't':
        binary = "./time_cli";
        handle_request(ip, binary);
        break;

      case 'q':
        printf ("Exiting...\n");
        exit(0);

      default:
        printf ("Invalid choice\n");
        break;
    }
  }
}

int
main (int argc, char *argv[])
{
  if (argc != 2) {
    printf("Usage: %s <server-addr>", argv[0]);
    return 0;
  }

  signal(SIGINT, intrpthandler);

  process(process_commandline (argc, argv));

  return 0;
}
