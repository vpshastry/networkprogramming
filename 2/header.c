#include "header.h"

int
mygetline(char *s, int lim, FILE *fd)
{
  int c, i;
  for (i = 0; i<lim-1 && (c=fgetc(fd))!=EOF && c!='\n'; ++i)
    s[i] = c;

  if (c == '\n') {
    s[i] = c;
    ++i;
  }
  s[i]='\0';
  return i;
}

char *
getnextline(int fd)
{
  char *line = calloc(MAX_LINE_SIZE, sizeof(char));
  int len;

  if ((len = mygetline(line, MAX_LINE_SIZE, (FILE *)fd)) == -1) {
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
  float num;

  if (!(curline = getnextline(fd))) {
    return -1;
  }
  errno = 0;
  num = strtof(curline, &tmp);
  if (errno != 0) {
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
}
