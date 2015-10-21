#include "header.h"

typedef struct {
  char server_ip[100];
  unsigned int portnumber;
  char filename[100000];
  unsigned int recvslidewindowsize; // In # of datagrams
  unsigned int seedvalue;
  float p; // probability
  unsigned long mean; // In milliseconds
} input_t;

int
readargsfromfile(input_t *input)
{
  FILE *fd;
  char line[512], ip[512];
  int i;
  if (!(fd = fopen("client.in", "r"))) {
    err_sys("Opening file failed"); // err_sys prints errno's explaination as well.
    return -1;
  }
  fgets(line, 512, fd);
  sscanf(line, "%s", input->server_ip);
  printf("%s\n", input->server_ip);
  fgets(line, 512, fd);
  sscanf(line, "%u", &input->portnumber);
  printf("%u\n", input->portnumber);
  fgets(line, 512, fd);
  sscanf(line, "%s", input->filename);
  printf("%s\n", input->filename);
  fgets(line, 512, fd);
  sscanf(line, "%u", &input->recvslidewindowsize);
  printf("%u\n", input->recvslidewindowsize);
  fgets(line, 512, fd);
  sscanf(line, "%u", &input->seedvalue);
  printf("%u\n", input->seedvalue);
  fgets(line, 512, fd);
  sscanf(line, "%f", &input->p);
  if (input->p > 1.0 || input->p < 0.0) {
	  err_quit("Probability has to be between 0.0 and 1.0, fix client.in and try again.");
  }
  printf("%f\n", input->p);
  fgets(line, 512, fd);
  sscanf(line, "%lu", &input->mean);
  printf("%lu\n", input->mean);
  return 0;
}

int
main(int argc, char *argv[]) {
	
	input_t input = {0,};
	readargsfromfile(&input);
    return 0;
  }
