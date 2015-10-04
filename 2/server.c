#include "header.h"

typdef struct {
  int portnumber;
  int maxslidewindowsize;
} input_t;

int
readargsfromfile()
{
  FILE *fd;

  if (!(fd = fopen("server.in", "r"))) {
    ERR(NULL, "Opening file failed: %s\n", strerror(errno));
    return -1;
  }


  if ((input->portnumber = readuintarg((int)fd)) == -1) {
    ERR(NULL, "Failed reading port number\n");
    return -1;
  }

  if ((input->maxslidewindowsize = readuintarg((int)fd)) == -1) {
    ERR(NULL, "Failed reading max slide window size\n");
    return -1;
  }

  return 0;
}

int
main(int argc, char *argv[]) {
  input_t input;

  if (argc > 0) {
    ERR(NULL, "Usage: %s\n", argv[0]);
    return 0;
  }

  readargsfromfile(&input);
}
