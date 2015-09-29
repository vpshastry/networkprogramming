#include <stdio.h>

int
main(int argc, char *argv[])
{
  int choice;
  int i = 0;
  int j;
  char readbuf[1024] = {0,};

  while (++i < 100) {
    printf ("1. Echo server\n2. Time server\n3. Quit\nEnter your choice: ");
    fflush(stdin);
    if (fgets(readbuf, 1024, stdin) != readbuf) {
      fprintf (stderr, "Error inputting\n");
      continue;
    }
    readbuf[1] = '\0';
    sscanf(readbuf, "%d", &choice);
    printf ("Your choice: %d\n", choice);
    if (!isdigit(choice+48))
      choice = 4;

    switch(choice) {
      case 1:
        printf ("Trying to run echo server\n");
        break;

      case 2:
        printf ("Trying to run time server\n");
        break;

      case 3:
        printf ("Exiting...\n");
        exit(0);

      default:
        printf ("Wrong choice, try again\n");
    }
  }
  return 0;
}
