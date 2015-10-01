#include <stdio.h>
#include <fcntl.h>

#define ECHO_PORT 51476
#define TIME_PORT 51477
#define MAX_BUF_SIZE 1024
#define MAX_CLIENTS 16

#define ERR(params ...) \
  fprintf (stderr, params);

#define INFO(params ...) \
  fprintf (stdout, params);

#define FDWRITE(fd, msg) \
  do {                            \
    char *message = msg;          \
    write(fd, msg, strlen(msg));  \
  } while(0)

void
set_nonblocking(int listenfd)
{
  int flags;

  if ((flags = fcntl(listenfd, F_GETFL, 0)) < 0) {
    fprintf (stderr, "Error getfl\n");
    return;
  }

  if (fcntl(listenfd, F_SETFL, flags | O_NONBLOCK) < 0)
    fprintf (stderr, "Error setting to NONblock mode\n");
}

