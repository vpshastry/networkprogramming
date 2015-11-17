#include "header.h"

static ctx_t ctx;

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
  int             staleness = parsecommandline(argc, argv);
  struct hwa_info *hwaddrs  = NULL;
  int             ret       = -1;

  ret = build_vminfos();
  prhwaddrs();
}
