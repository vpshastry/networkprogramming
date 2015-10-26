/* include dgsendrecv1 */
#include	"unprtt.h"
#include	<setjmp.h>
#include "header.h"

static int cur_window_size = 16;
static int updated_cur_window_size = -1;
static struct rtt_info   rttinfo;
static int	rttinit = 0;
static long long seq = 0;

static void	sig_alrm(int signo);
static sigjmp_buf	jmpbuf;

int
get_cur_window_size()
{
  if (updated_cur_window_size != -1)
    return updated_cur_window_size;
  return cur_window_size;
}

void
update_window_size(int i)
{
  updated_cur_window_size = i;
}

int
dg_send_recv(int fd, int filefd)
{
  ssize_t			n;
  uint32 tt;
  int i;
  int lastloop = 0;
  send_buffer_t recvbuf, sendbuf[get_cur_window_size()];

  if (rttinit == 0) {
    rtt_init(&rttinfo);		/* first time we're called */
    rttinit = 1;
    rtt_d_flag = 1;
  }

  while (!lastloop) {
    Signal(SIGALRM, sig_alrm);
    rtt_newpack(&rttinfo);		/* initialize for this packet */

    // Below for loop initializes the data.
    for (i = 0; i < get_cur_window_size(); ++i) {
      sendbuf[i].hdr.seq = ++seq;

      // Read from file to fill the buffer.
      n = 0;
      if ((n = read(filefd, sendbuf[i].payload, FILE_READ_SIZE)) <= 0) {
        if (n != 0) {
          err_sys("File read error");
          return NULL;
        }
      }

      if (n == 0 || n < FILE_READ_SIZE) {
        if (RTT_DEBUG) fprintf (stderr, "I think this is the last data gram\n");
        sendbuf[i].hdr.fin = 1;
        update_window_size(i +1);
      }
      sendbuf[i].length = n;

      if (RTT_DEBUG) printf ("Data for packet %d: %s\n", i, sendbuf[i].payload);
    }

sendagain:
    tt = rtt_ts(&rttinfo);
    for (i = 0; i < get_cur_window_size(); ++i) {
      sendbuf[i].hdr.ts = tt;
      Write(fd, &sendbuf[i], sizeof(sendbuf[i]));
    }

    alarm(rtt_start(&rttinfo));	/* calc timeout value & start timer */

    if (RTT_DEBUG) rtt_debug(&rttinfo);

    if (sigsetjmp(jmpbuf, 1) != 0) {
      if (rtt_timeout(&rttinfo) < 0) {
        err_msg("dg_send_recv: no response from server, giving up");
        rttinit = 0;	/* reinit in case we're called again */
        errno = ETIMEDOUT;
        return(-1);
      }

      if (RTT_DEBUG) err_msg("dg_send_recv: timeout, retransmitting");

      goto sendagain;
    }

    for (i = 0; i < get_cur_window_size(); ++i) {
      memset (&recvbuf, 0, sizeof(recvbuf));

      while ((n = Read(fd, &recvbuf, sizeof(recvbuf))) < sizeof(seq_header_t))
        if (RTT_DEBUG) fprintf (stderr, "Received data is smaller than header\n");

      if (RTT_DEBUG) fprintf(stderr, "recv %4d\n", recvbuf.hdr.seq);

      if (!(recvbuf.hdr.seq > (seq -get_cur_window_size() +1) && recvbuf.hdr.seq <= seq+1)) {
        fprintf (stderr, "Client is doing some BS!!\n");
        exit(0);
      }
    }

    alarm(0);			/* stop SIGALRM timer */
            /* 4calculate & store new RTT estimator values */
    rtt_stop(&rttinfo, rtt_ts(&rttinfo) - recvbuf.hdr.ts);
  }

  return 0;
}

static void
sig_alrm(int signo)
{
	siglongjmp(jmpbuf, 1);
}

int
send_file(char *filename, int client_sockfd)
{
  int n;

  if (access(filename, F_OK | R_OK)) {
    printf ("File not accessible\n");
    return -1;
  }

  int filefd;
  if ((filefd = open(filename, O_RDONLY)) < 0) {
    err_sys("opening file failed:");
    return -1;
  }

  if (RTT_DEBUG) fprintf (stderr, "Calling dg_send_recv\n");

  n = dg_send_recv(client_sockfd, filefd);
  if (n < 0)
          err_quit("dg_send_recv error");

  close(filefd);
}
