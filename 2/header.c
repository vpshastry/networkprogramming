#include "header.h"

char *
getnextline(int fd)
{
  char *line;
  int len = 0;

  if ((read = getline(&line, &len, (FILE *)fd)) == -1) {
    ERR(NULL, "Error reading the line\n");
    return NULL;
  }
  return line;
}

int
readuintarg(int fd)
{
  char *curline;
  char *tmp;
  long num;

  if (!(curline = getnextline(fd))) {
    ERR();
    return -1;
  }
  if ((num = strtol(curline, &tmp, 10)) == LONG_MAX || num == LONG_MIN) {
    ERR(NULL, "Port number out of range\n");
    return -1;
  }
  if (*tmp != '\0' || (num == 0 && errno == ERANGE)) {
    ERR(NULL, "Input string doesn't convert to integer completely: %s\n", tmp);
    return -1;
  }

  free(curline);
  return num;
}

float
readfloatarg(int fd)
{
  char *curline;
  char *tmp;

  if (!(curline = getnextline(fd))) {
    ERR();
    return -1;
  }
  if ((num = strtof(curline, &tmp)) == HUGE_VAL || num == HUGE_VAL) {
    ERR(NULL, "Port number out of range\n");
    return -1;
  }
  if (*tmp != '\0' || (num == 0 && errno == ERANGE)) {
    ERR(NULL, "Input string doesn't convert to integer completely: %s\n", tmp);
    return -1;
  }

  free(curline);
  return num;
}

struct in_addr *
readip(int fd)
{
  char *curline;
  char *tmp;
  int err;
  struct in_addr *inaddr = (struct in_addr *) calloc(1, sizeof (struct in_addr));

  if (!(curline = getnextline(fd))) {
    ERR();
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
    ERR();
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
}
