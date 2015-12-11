#include "header.h"

int
main()
{
  struct sockaddr_in needmacof = {
                                .sin_family = AF_INET,
  };
  struct hwaddr hwaddr;

  Inet_pton(AF_INET, "130.245.156.29", &needmacof.sin_addr);
  areq((struct sockaddr *)&needmacof, sizeof(struct sockaddr_in), &hwaddr);
}
