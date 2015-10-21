#include "header.h"

typedef struct {
  struct in_addr *ip;
  char *ipstr;
  int portnumber;
  char *filename;
  int recslidewindowsize; // In # of datagrams
  unsigned int seedvalue;
  float prob;
  long mean; // In milliseconds
} input_t;

/*int
readargsfromfile(input_t *input)
{
  int fd;
  FILE *fp;

  if (!(fp = fopen("client.in", "r"))) {
    ERR(NULL, "Opening file failed: %s\n", strerror(errno));
    return -1;
  }
  fd = (int)fp;*/

  /*
  TODO: Can't call without moving the fd back to original
  position.
  if (!(input->ipstr = readipstr(fd))) {
    ERR(NULL, "Failed reading ip\n");
    return -1;
  }
  */
/*
  if (!(input->ip = readip(fd))) {
    ERR(NULL, "Failed reading/converting ip\n");
    return -1;
  }

  if ((input->portnumber = readuintarg(fd)) == -1) {
    ERR(NULL, "Failed reading port number\n");
    return -1;
  }

  if (!(input->filename = getnextline(fd))) {
    ERR(NULL, "Error reading file name\n");
    return -1;
  }

  if ((input->recslidewindowsize = readuintarg(fd)) == -1) {
    ERR(NULL, "Failed reading max slide window size\n");
    return -1;
  }

  if ((input->seedvalue = readuintarg(fd)) == -1) {
    ERR(NULL, "Failed reading seed value\n");
    return -1;
  }

  if ((input->prob = readfloatarg(fd)) == -1) {
    ERR(NULL, "Error reading probability of datagram loss\n");
    return -1;
  }

  if ((input->mean = readuintarg(fd)) == -1) {
    ERR(NULL, "Error reading mean\n");
    return -1;
  }

  return 0;
}*/

int
main(int argc, char *argv[]) {
  input_t input = {0,};

  if (argc > 0) {
    ERR(NULL, "Usage: %s\n", argv[0]);
    return 0;
  }
/*
  if (readargsfromfile(&input) == -1) {
    ERR(NULL, "Failed to read from file\n");
    return -1;
  }
*/
	return 0;
}
