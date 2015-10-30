/* include dgsendrecv1 */
#include "unprtt.h"
#include <setjmp.h>
#include "header.h"
#include "window.h"

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static int cur_window_size = 16;
static int updated_cur_window_size = -1;
static struct rtt_info rttinfo;
static int rttinit = 0;
long long seq = -1;
unsigned int max_window_size;
static void sig_alrm(int signo);
static sigjmp_buf jmpbuf, jmpbuf2;
int window_probe;
unsigned int window_probe_timer = 2;

int
prepare_window(window_t *window, int filefd)
{
  int i;
  int lastloop = 0;

  // TODO: This is a window management code. Try to move within window.c
  window->tail = window->head +1;
  window->head = window->tail + window->cwnd -1;

  //printf ("@prepare window before loop\n");
  //window->debug(window);

  // Below for loop initializes the data.
  for (i = 0; i < window->cwnd; ++i) {
	//printf ("Preparing for %d, seq: %ld\n", window->tail +i, window->seq+1);
    if ((lastloop = window->prepare_cur_datagram(window, window->tail +i, filefd))) {
      // TODO: Recheck this inconsistent window size update.
      window->cwnd = i +1;
	  window->head = window->tail + window->cwnd -1;
	  //printf ("@prepare window loop\n");
	  //window->debug(window);
      break;
    }
  }

  //printf ("Prepared buffer\n");
  return lastloop;
}

int
dg_send_recv(int fd, int filefd)
{
  ssize_t			n;
  uint32 tt;
  int i, done = 0, waiting_for_new_ack = 0;
  int lastloop = 0;
  send_buffer_t recvbuf, *sendbuf, *resendbuf;
  struct stat buf;
  window_t *window = &newwindow;
  int received_dup_ack = 0;
  int min_idx_acked = 0;
  int rwnd_from_ack = 1;

  if (rttinit == 0) {
    rtt_init(&rttinfo);		/* first time we're called */
    rttinit = 1;
    rtt_d_flag = 1;

    if (fstat(filefd, &buf))
      err_sys("Fstat on file failed\n");

    window->init(window, buf.st_size/FILE_READ_SIZE +1, 1/*cwnd*/);
  }

  while (!lastloop) {
    received_dup_ack = 0;
    Signal(SIGALRM, sig_alrm);
    rtt_newpack(&rttinfo);		/* initialize for this packet */
	
	if (rwnd_from_ack == 0) {
		printf("recv. window is full, waiting for a dupack, and setting window probe timer.\n");
		window_probe = 1;
		alarm(window_probe_timer);
	}
	if (sigsetjmp(jmpbuf2, 1) != 0) {
		printf("Timeout for window probe, Sending window probe\n");
		sendbuf->hdr.seq = min_idx_acked;
		Write(fd, sendbuf, sizeof(sendbuf[i]));
		window_probe_timer = window_probe_timer * 2;
		if (window_probe_timer > 60) window_probe_timer = 60;
		printf("window_probe_timer: %u\n", window_probe_timer);
		alarm(window_probe_timer);
	}
	while(rwnd_from_ack == 0) {
		n = Read(fd, &recvbuf, sizeof(recvbuf));
		if (recvbuf.hdr.ack == 2) {
			// ack = 2 is a special marker for recv window update.
			printf("Got a dup_ack of seq:%d, for rwnd update\n", recvbuf.hdr.seq);
			rwnd_from_ack = recvbuf.hdr.rwnd;
			printf("Now client has rwnd=%d\n, continue...\n", rwnd_from_ack);
			window_probe = 0;
			window_probe_timer = 2;
			alarm(0);
			break;
		}
		else {
			rwnd_from_ack = recvbuf.hdr.rwnd;
			printf("Now client has rwnd=%d\n, continue...\n", rwnd_from_ack);
			window_probe = 0;
			window_probe_timer = 2;
			alarm(0);
			break;
		}
	}
	window->cwnd = MIN(rwnd_from_ack, window->cwnd);
	if (window->cwnd >= max_window_size) {
		printf("Server/Sender window full\n ");
		window->cwnd = max_window_size;
	}
	printf("cwnd=%d\n", window->cwnd);
    if ((lastloop = prepare_window(window, filefd)) < 0) {
      fprintf (stderr, "Error preparing window\n");
      return NULL;
    }

// TODO: Rethink about placing this tag
sendagain:
    window->check_consistency(window);

	//window->debug(window);
    for (i = window->tail; i <= window->head; ++i) {
      if (!(sendbuf = window->get_buf(window, i)))
        fprintf (stderr, "Couldn't get the window %d. Yerror!!\n", i);
      sendbuf->hdr.ts = rtt_ts(&rttinfo);
      write(fd, sendbuf, sizeof(sendbuf[i]));

      fprintf (stdout, "---------------------------\nSent packet #%d\n", sendbuf->hdr.seq);
	  window->debug(window);
    }

    alarm(rtt_start(&rttinfo));	/* calc timeout value & start timer */

    //if (RTT_DEBUG) rtt_debug(&rttinfo);

    if (sigsetjmp(jmpbuf, 1) != 0) {
      /*if (rtt_timeout(&rttinfo) < 0) {
        err_msg("dg_send_recv: no response from server, giving up");
        rttinit = 0;	*//* reinit in case we're called again */
        /*errno = ETIMEDOUT;
        return(-1);
      }*/

      printf("Timeout, reduce ssthresh=cwnd/2, cwnd = 1, and slow start\n");
	  /* timeout - so make window size as 1 with last unacked data - send again */
	  // TODO: This is a window management code. Try to put it inside the window.c
	  window->ssthresh = window->cwnd / 2;
	  window->cwnd = 1;
	  received_dup_ack = 0;
	  window->head = window->tail = min_idx_acked;
	  if (lastloop) {
		  lastloop = 0;
	  }
	  //printf("Reducing window to 1, with #:%d to be resent.\n", min_idx_acked);
      goto sendagain;
    }

    for (i = window->tail; i <= window->head; ) {
      memset (&recvbuf, 0, sizeof(recvbuf));

      while ((n = Read(fd, &recvbuf, sizeof(recvbuf))) < sizeof(seq_header_t))
        if (RTT_DEBUG) fprintf (stderr, "Received data is smaller than header\n");
	  alarm(0);
	  alarm(rtt_start(&rttinfo));	/* calc timeout value & start timer */
      if (recvbuf.hdr.ack != 1) {
        //fprintf (stderr, "This is not an ack\n");
		if (recvbuf.hdr.ack == 2) {
			rwnd_from_ack = recvbuf.hdr.rwnd;
		}
        continue;
      }
	  if (recvbuf.hdr.fin == 1) {
		  printf("Recv Fins ACK\n");
		  lastloop = 1;
		  alarm(0);
		  //break;
		  printf("Bye Bye\n");
		  return 0;
		}

      if (RTT_DEBUG) fprintf(stdout, "Recvd ACK #%d\n", recvbuf.hdr.seq);

      switch (window->add_new_ack(window, recvbuf.hdr.seq)) {
        case ACK_DUP:
          received_dup_ack = 1;
		  waiting_for_new_ack = 1;
		  window->mode = MODE_FAST_RECOV;
		  window->ssthresh = window->cwnd / 2;
		  window->cwnd = window->ssthresh + 3;
          resendbuf = window->get_buf(window, recvbuf.hdr.seq);
		  printf("Fast retransmit of Seq:%d\n", recvbuf.hdr.seq);
		  window->debug(window);
		  rwnd_from_ack = recvbuf.hdr.rwnd;
          Write(fd, (void *)resendbuf, sizeof(send_buffer_t));
          break;

        case ACK_NONE:
          // Increase the window size and resend;
		  if (waiting_for_new_ack == 1) {
			  waiting_for_new_ack = 0;
			  window->mode = MODE_CAVOID;
			  window->cwnd = window->ssthresh;
			  received_dup_ack = 0;
		  }
		  if (window->mode == MODE_CAVOID) {
			  //printf("Increasing cwind size\n");
			  window->cwnd++;
		  }
          if (i < recvbuf.hdr.seq) i = recvbuf.hdr.seq;
		  min_idx_acked = recvbuf.hdr.seq;
		  rwnd_from_ack = recvbuf.hdr.rwnd;
          break;

        default:
          fprintf (stderr, "Something wrong here!!\n");
          break;
      }
    }

    alarm(0);			/* stop SIGALRM timer */
            /* 4calculate & store new RTT estimator values */
    rtt_stop(&rttinfo, rtt_ts(&rttinfo) - recvbuf.hdr.ts);
	printf("rwnd  = %d\n", rwnd_from_ack);
	//printf("Updated cwnd = %d\n", window->cwnd);
    window->clear(window);
    window->update_cwnd(window, received_dup_ack);
	received_dup_ack = 0;
  }

  printf ("Bye bye!!\n");
  return 0;
}

static void
sig_alrm(int signo)
{
	if (window_probe == 1) siglongjmp(jmpbuf2, 1);
	else siglongjmp(jmpbuf, 1);
}

int
send_file(char *filename, int client_sockfd, unsigned int maxslidewindowsize)
{
  int n;

  max_window_size = maxslidewindowsize;
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
