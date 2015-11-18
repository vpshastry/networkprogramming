#include "header.h"

static ctx_t ctx;

int
parsecommandline(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf (stdout, "Usage: %s <staleness>\n", argv[0]);
    //exit(0);
	return 0;
  }

  return atoi(argv[1]);
}

int create_bind_odr_sun_path() {
  int sockfd;
  socklen_t len;
  struct sockaddr_un addr;
  sockfd = Socket(AF_LOCAL, SOCK_DGRAM, 0);
  
  unlink(ODR_SUNPATH);
  bzero(&addr, sizeof(addr));
  addr.sun_family = AF_LOCAL;
  strcpy(addr.sun_path, ODR_SUNPATH);

  Bind(sockfd, (SA*) &addr, sizeof(addr));
  
  return sockfd;
}

int
main(int argc, char *argv[])
{
  //int             staleness = parsecommandline(argc, argv);
  struct hwa_info *hwaddrs  = NULL;
  int             num_interfaces, odr_sun_path_sockfd, n;
  sequence_t recvseq;

  vminfo_t vminfo[10];
  num_interfaces = build_vminfos(vminfo);
  prhwaddrs();
  printf("----Print VMINFOS----\n");
  print_vminfos(vminfo, num_interfaces);
  odr_sun_path_sockfd = create_bind_odr_sun_path();
  n = Read(odr_sun_path_sockfd, &recvseq, sizeof(recvseq));
  printf("Finished Reading\n");
  printf("Message is:%s\n", recvseq.buffer);
  // Trying random PF_PACKET stuff.

  // Create a PF_PACKET socket
  int s, j; /*socketdescriptor*/

  /*s = socket(AF_PACKET, SOCK_RAW, htons(USID_PROTO));
  if (s == -1) { fprintf (stderr, "PF_PACKET creatation failed"); exit(0); }

  printf("PF_PACKET socket created\n");*/
}
