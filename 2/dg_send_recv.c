/* include dgsendrecv1 */
#include	"unprtt.h"
#include	<setjmp.h>
#include "header.h"
#include "window.h"

static int cur_window_size = 16;
static int updated_cur_window_size = -1;
static struct rtt_info   rttinfo;
static int	rttinit = 0;
static long long seq = -1;

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
prepare_window(window_t *window, int filefd)
{
  send_buffer_t tmpsendbuf, *newsendbuff;
  int i;
  int n;
  int lastloop = 0;

  //if (RTT_DEBUG) fprintf (stderr, "Current window size: %s\n", window->cwnd);

  // Below for loop initializes the data.
  for (i = 0; i < window->cwnd; ++i) {
    memset(&tmpsendbuf, 0, sizeof(send_buffer_t));

    tmpsendbuf.hdr.seq = ++seq;

    // Read from file to fill the buffer.
    n = 0;
    if ((n = read(filefd, tmpsendbuf.payload, FILE_READ_SIZE)) <= 0) {
      if (n != 0) {
        err_sys("File read error");
        return -1;
      }
    }

    if (n == 0 || n < FILE_READ_SIZE) {
      if (RTT_DEBUG) fprintf (stderr, "I think this is the last datagram\n");
      tmpsendbuf.hdr.fin = 1;
      window->cwnd = i+1;
      lastloop = 1;
    }
    tmpsendbuf.length = n;

    /* Add this to window */
    newsendbuff = malloc(sizeof(send_buffer_t));
    memcpy (newsendbuff, &tmpsendbuf, sizeof(send_buffer_t));
    window->append(window, newsendbuff);
  }

  return lastloop;
}

int
dg_send_recv(int fd, int filefd)
{
  ssize_t			n;
  uint32 tt;
  int i;
  int lastloop = 0;
  send_buffer_t recvbuf, *sendbuf, *resendbuf;
  struct stat buf;
  window_t *window = &newwindow;
  int received_dup_ack = 0;

  if (rttinit == 0) {
    rtt_init(&rttinfo);		/* first time we're called */
    rttinit = 1;
    rtt_d_flag = 1;

    if (fstat(filefd, &buf))
      err_sys("Fstat on file failed\n");

    window->init(window, buf.st_size/FILE_READ_SIZE +1);
  }

  while (!lastloop) {
    received_dup_ack = 0;
    Signal(SIGALRM, sig_alrm);
    rtt_newpack(&rttinfo);		/* initialize for this packet */

    if ((lastloop = prepare_window(window, filefd)) < 0) {
      fprintf (stderr, "Error preparing window\n");
      return NULL;
    }

// TODO: Rethink about placing this tag
sendagain:
    if ((window->head - window->tail + 1) != window->cwnd) {
      fprintf (stderr, "head - tail != cwnd\n");
      exit(0);
    }

    tt = rtt_ts(&rttinfo);
    for (i = window->tail; i <= window->head; ++i) {
      if (!(sendbuf = window->get_buf(window, i)))
        fprintf (stderr, "Couldn't get the window. Yerror!!\n");
      sendbuf->hdr.ts = tt;
      Write(fd, sendbuf, sizeof(sendbuf[i]));
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

    for (i = 0; i < window->cwnd; ++i) {
      memset (&recvbuf, 0, sizeof(recvbuf));

      while ((n = Read(fd, &recvbuf, sizeof(recvbuf))) < sizeof(seq_header_t))
        if (RTT_DEBUG) fprintf (stderr, "Received data is smaller than header\n");

      if (recvbuf.hdr.ack != 1) {
        fprintf (stderr, "This is not an ack\n");
        continue;
      }

      if (RTT_DEBUG) fprintf(stderr, "recv %4d\n", recvbuf.hdr.seq);

      switch (window->add_new_ack(window, recvbuf.hdr.seq)) {
        case ACK_DUP:
          received_dup_ack = 1;
          resendbuf = window->get_buf(window, recvbuf.hdr.seq);
          Write(fd, (void *)resendbuf, sizeof(send_buffer_t));
          break;

        case ACK_NONE:
          // Increase the window size and resend;
          break;
      }
    }

    alarm(0);			/* stop SIGALRM timer */
            /* 4calculate & store new RTT estimator values */
    rtt_stop(&rttinfo, rtt_ts(&rttinfo) - recvbuf.hdr.ts);

    window->clear(window);
    window->update_cwnd(window, received_dup_ack);
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
  return 0;
}
