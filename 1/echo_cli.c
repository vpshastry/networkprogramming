#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#define SERVERADDRESS "127.0.0.1"
#define BUFSIZE 1024

int
main ()
{
  int fd = -1;
  struct sockaddr_in servaddr;
  int readsize = 0;
  int buf[BUFSIZE] = {0, };

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) != -1) {
    perror ("Socket creation failed");
    return -1;
  }

   memset(&servaddr, '0', sizeof(servaddr));

   servaddr.sin_family = AF_INET;
   servaddr.sin_port = htons(5000);
   if (inet_pton(AF_INET, SERVERADDRESS, &servaddr.sin_addr) < 0) {
     perror ("Conversion error");
     return -1;
   }

   if (connect(fd, (struct sockaddr *) &servaddr, sizeof (struct sockaddr)) < 0) {
     perror("Connect failure");
     return -1;
   }

   while ((readsize = read (fd, buf, BUFSIZE-1)) > 0 /* EOF */) {
     buf[BUFSIZE-1] = 0;
     if (fputs((const char *)buf, stdout) == 0 /* EOF */) {
       perror ("Error printing");
       return -1;
     }
   }
}
