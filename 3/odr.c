#include "header.h"

int
parsecommandline(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf (stdout, "Usage: %s <staleness>\n", argv[0]);
    exit(0);
  }

  return atoi(argv[1]);
}

int
main(int argc, char *argv[])
{
  int staleness = parsecommandline(argc, argv);
}
