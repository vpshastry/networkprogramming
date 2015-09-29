#include "header.h"

#define ECHO_REPLY "PONG"

int
get_server_socket(int port) {
  int flags = 0;
  int serversock = -1;
  struct sockaddr_in addr;
  int yes = 1;

  if ((serversock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    logit(ERROR, "Socket error");
    return -1;
  }

  printf ("port: %d\n", port);
  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (setsockopt(serversock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
    logit(ERROR, "Set sock opt error");
    return -1;
  }

  if (bind (serversock, (struct sockaddr *)&addr, sizeof (struct sockaddr)) < 0) {
    logit(ERROR, "bind error");
    return -1;
  }

  if (listen (serversock, 1024) < 0) {
    logit(ERROR, "listen failures");
    return -1;
  }

  if ((flags = fcntl(serversock, F_GETFL, 0)) < 0) {
    logit(ERROR, "get fl error");
    return -1;
  }

  if (fcntl(serversock, F_SETFL, flags | O_NONBLOCK) < 0) {
    logit(ERROR, "setting to non blocking mode error");
    return -1;
  }

  return 0;
}

void *
echo_cli_service(void *arg)
{
  char readbuf[MAX_BUF_SIZE] = {0,};
  size_t readsize;
  int serversock = -1;
  int fd;
  int *passfd = NULL;
  int err = -1;
  int i = 0;
  fd_set read_set;

  if ((serversock = get_server_socket(SERVER_ECHO_PORT)) < 0) {
    logit (ERROR, "Couldn't get socket");
    return NULL;
  }

  FD_ZERO(&read_set);
  FD_SET(serversock, &read_set);
  while (ANSWER_TO_THE_LIFE) {
    logit(INFO, "Waiting on select");
    err = select(serversock+1, &read_set, NULL, NULL, NULL);

    logit(INFO, "Waiting on accept");
    if ((fd = accept(serversock, NULL, NULL)) < 0) {
      logit(ERROR, "Couldn't accept the connection from client");
      close(fd);
      read(serversock, readbuf, MAX_BUF_SIZE);
      continue;
    }

    logit(INFO, "Waiting on read");
    if ((readsize = read(fd, readbuf, MAX_BUF_SIZE)) == EOF) {
      logit(ERROR, "Client closed the connection");
      goto closeit;
    }

    logit(INFO, "Writing the same message back");
    write(fd, readbuf, readsize);

closeit:
    close(fd);
  }

  return NULL;
}

void *
timecli_serve_single_client(void *arg)
{
  int fd = *(int *)arg;
  free(arg);
  struct timeval *tv = (struct timeval *) calloc (1, sizeof (struct timeval));
  /*struct timeval tv;
  memset (&tv, 0, sizeof(struct timeval)); */
  char readbuf[MAX_BUF_SIZE] = {0,};
  char buf[MAX_BUF_SIZE] = {0,};
  int err;
  struct tm tm = {0,};
  time_t     now;

  /* TODO: Look into this select function again */
  /*while (select (fd, NULL, NULL, NULL, tv) >= 0) { */
  while (sleep (10)) {
    if (read(fd, readbuf, MAX_BUF_SIZE) == EOF) {
      logit (0, "Client closed connection");
      break;
    }

    /*
    if ((err = gettimeofday(tv, NULL)) < 0) {
      logit(ERROR, "failed to fetch time");
      memset (tv, 0, sizeof(*tv));
    }

    snprintf (buf, MAX_BUF_SIZE, "%lld", (long long)tv->tv_sec);
    */
    time(&now);
    tm = *localtime(&now);
    //strptime (buf, "%s", &tm);
    memset(buf, 0, MAX_BUF_SIZE);
    strftime (buf, MAX_BUF_SIZE,  "%b %d %H:%M %Y", &tm);

    if (EPIPE == write(fd, buf, strlen(buf)))
      return NULL;

    //tv->tv_sec += 5;
    memset(&tm, 0, sizeof(struct tm));
  }

  return NULL;
}

void *
time_cli_service(void *arg)
{
  int serversock = -1;
  int flags = 0;
  int fd = -1;
  int err = -1;
  int i = 0;
  pthread_t threads[MAX_CLIENTS] = {0,};
  int *passfd = NULL;

  if ((serversock = get_server_socket(SERVER_TIME_PORT)) < 0) {
    logit (ERROR, "Couldn't get socket");
    return NULL;
  }

  while (select(serversock, NULL, NULL, NULL, NULL) >= 0) {
    if ((fd = accept(serversock, NULL, NULL)) < 0) {
      logit(ERROR, "Accept error");
      continue;
    }

    passfd = (int *)malloc(sizeof(fd));
    *passfd = fd;
    if ((err = pthread_create(&threads[i], NULL, &timecli_serve_single_client, passfd)) != 0) {
      logit(ERROR, "Error creating echo cli");
      return NULL;
    }

    if ((err = pthread_detach(threads[i++])) != 0) {
      logit(ERROR, "Error detaching thread echo cli");
      return NULL;
    }

    if (fcntl(serversock, F_SETFL, flags & (~O_NONBLOCK)) < 0) {
      logit(ERROR, "Setting to blocking mode failure");
      return NULL;
    }
  }

  return NULL;
}

static volatile int intrpt_received = 0;

void
intrpthandler(int intrpt)
{
  printf("Received SIGINT, exiting\n");
  intrpt_received = 1;
}

int
main()
{
  int err;
  pthread_t echocliID = {0,};
  pthread_t timecliID = {0,};
  pthread_attr_t attr = {0,};

  signal(SIGINT, intrpthandler);

  if ((err = pthread_attr_init(&attr)) != 0) {
    printf ("ERROR: %s. ", strerror(err));
    logit(ERROR, "Attr init failed for pthread");
    return -1;
  }

  if ((err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0) {
    printf ("ERROR: %s. ", strerror(err));
    logit(ERROR, "set detach state failed for pthread");
    return -1;
  }

  logit(INFO, "Starting thread for echo cli service");
  if ((err = pthread_create(&echocliID, &attr, echo_cli_service, &err)) != 0) {
    printf("ERROR: %s. ", strerror(err));
    logit(ERROR, "Error creating echo cli");
    return -1;
  }

  /*
  if ((err = pthread_detach(echocliID)) != 0) {
    logit(ERROR, "Error detaching thread echo cli");
    return -1;
  }
  */

  logit(INFO, "Starting thread for time cli service");
  if ((err = pthread_create(&timecliID, &attr, time_cli_service, &err)) != 0) {
    logit(ERROR, "Error creating echo cli");
    return -1;
  }

  /*
  if ((err = pthread_detach(timecliID)) != 0) {
    logit(ERROR, "Error detaching thread echo cli");
    return -1;
  }
  */

  while(!intrpt_received)
    sleep (5);
  return 0;
}
