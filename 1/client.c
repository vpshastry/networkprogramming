#include <stdio.h>
#include <fnmatch.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define ValidIpAddressRegex "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$"
#define ValidHostnameRegex "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\-]*[A-Za-z0-9])$"

#define _True 1
#define _False 0

#define MAX_BUF_SIZE 1024

#define SERVER_ADDR 127.0.0.1

typedef union {
  struct in_addr inaddr;
  struct in6_addr in6addr;
} in_addr_t;

void
log(level_t level, char *msg)
{
  perror (msg);
}

void *
process_commandline(int argc, char *argv[])
{
  if (fnmatch(argv[1], ValidIpAddressRegex, NULL) != FNM_NOMATCH) {
    ip_str = argv[1];
    ip = &inaddrt.inaddr;
    if (strchr(ip, ':')) {
      v6 = _True;
      ip = &inaddrt.in6addr;
    }
  }

  if (!ip_str && fnmatch(argv[1], ValidHostnameRegex, NULL) != FNM_NOMATCH) {
    fqdn = argv[1];
  }

  if (!ip_str && !fqdn) {
    log (ERROR, "Argument 1 is neither an IP nor a FQDN");
    return -1;
  }

  if (inet_pton(v6? AF_INET6 : AF_INET, ip_str, ip) != 1) {
    log (ERROR, "Failed to convert the ip from string to in_addr/in6_addr");
    return -1;
  }

  if (ip) {
    if (!(hostentry = gethostbyaddr((const void *)ip,
                                    sizeof(v6? struct in6_addr: struct in_addr),
                                    v6? AF_INET : AF_INET6)))
      log (ERROR, "Failed get host by addr");
  } else {
    if (!(hostentry = gethostbyname(fqdn)))
      log (ERROR, "Failed get host by name");
  }
  memcpy (lhostent, hostentry, sizeof(struct hostent));

  printf ("The server host is:\nName: %s\nOther aliases: ", lhostent.h_name);
  for (i = 0; lhostent.h_aliases[i]; ++i)
    printf ("%s, ", lhostent.h_aliases[i]);
  printf ("\nIP addresses: ");
  for (i = 0; lhostent.h_addr_list[i]; ++i)
    printf ("%s, ", lhostent.h_addr_list[i]);
}

void
handle_request(int choice)
{
  int pipefd[2] = {0,};
  int ret = 0;

  if (!pipe2(pipefd, O_CLOEXEC) == -1) {
    log (ERROR, "pipe creation failed");
    return -1;
  }

  switch (fork()) {
    case 0:
      close(pipefd[0]);
      dup2(pipefd[1], 1);

      switch (choice) {
        case 1:
          binary = "./echocli";
          break;

        case 2:
          binary = "./timecli";
          break;

        default:
          log (ERROR, "Invalid choice %d. Exiting...");
          exit(-1);
      }

      execlp("xterm", "xterm", "-e", binary, SERVER_ADDR, (char *) 0);

    case -1:
      // Error

    default:
      close (fd[1]);

      while (read(pipefd[0], buf, MAX_BUF_SIZE) != 0 /* EOF */)
        log(INFO, buf);

      wait(NULL);
      close (pipefd[0]);
  }
}

process()
{
  int choice;

  while (42) {
    printf ("\n1. echo server\n2. time server\n3. Exit\nEnter your choice: ");
    scanf ("%d", &choice);

    switch (choice) {
      case 1:
      case 2:
        handle_request(choice);
        break;

      case 3:
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
  char *ip_str = NULL;
  char *fqdn = NULL;
  in_addr_t inaddrt = {0, };
  void *ip = &inaddrt.inaddr;
  int v6;

  process_commandline (argc, argv);

  process();
}
