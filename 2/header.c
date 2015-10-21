#include "header.h"

/*struct in_addr *
readip(int fd)
{
  char *curline;
  char *tmp;
  int err;
  struct in_addr *inaddr = (struct in_addr *) calloc(1, sizeof (struct in_addr));

  if (!(curline = getnextline(fd))) {
    free(inaddr);
    return NULL;
  }

  if ((err = inet_pton(AF_INET, curline, inaddr)) != 1) {
    ERR(NULL, "String is not an IP: %s\n", curline);
    free(curline);
    free(inaddr);
    return NULL;
  }

  free(curline);
  return inaddr;
}

char *
readipstr(int fd)
{
  char *curline;
  char *tmp;
  int err;
  struct in_addr *inaddr = (struct in_addr *) calloc(1, sizeof (struct in_addr));

  if (!(curline = getnextline(fd))) {
    free(inaddr);
    return NULL;
  }

  if ((err = inet_pton(AF_INET, curline, inaddr)) != 1) {
    ERR(NULL, "String is not an IP: %s\n", curline);
    free(curline);
    free(inaddr);
    return NULL;
  }

  free(inaddr);
  return curline;
}*/
