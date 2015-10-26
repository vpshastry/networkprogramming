/* include dgsendrecv1 */
#include	"unprtt.h"
#include	<setjmp.h>
#include "header.h"

#define	RTT_DEBUG

static int cur_window_size = 8;
static int updated_cur_window_size = -1;
static struct rtt_info   rttinfo;
static int	rttinit = 0;
static struct msghdr	msgrecv;	/* assumed init to 0 */
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
dg_send_recv(int fd, const void *outbuff, size_t outbytes,
                     int filefd)
{
	ssize_t			n;
        uint32 tt;
        int i;
        int idx;
        seq_header_t recvhdr, sendhdr[get_cur_window_size()];
        int breaknow = 0;
        struct msghdr **window;
        struct iovec iovsend[get_cur_window_size()][2], iovrecv[2];
        struct msghdr msgsend[get_cur_window_size()];
        char *mybuf;
        int loop = 1;

	if (rttinit == 0) {
		rtt_init(&rttinfo);		/* first time we're called */
		rttinit = 1;
		rtt_d_flag = 1;
	}

        msgrecv.msg_name = NULL;
        msgrecv.msg_namelen = 0;
        msgrecv.msg_iov = iovrecv;
        msgrecv.msg_iovlen = 1;
        iovrecv[0].iov_base = (void *)&recvhdr;
        iovrecv[0].iov_len = sizeof(seq_header_t);

        while (42) {
          Signal(SIGALRM, sig_alrm);
          rtt_newpack(&rttinfo);		/* initialize for this packet */

sendagain:

          for (idx = 0; idx < get_cur_window_size(); ++idx) {
            msgsend[idx].msg_name = NULL;
            msgsend[idx].msg_namelen = 0;
            msgsend[idx].msg_iov = iovsend[idx];
            msgsend[idx].msg_iovlen = 2;
            iovsend[idx][0].iov_base = (void *)&sendhdr[idx];
            iovsend[idx][0].iov_len = sizeof(seq_header_t);

            n = 0;
            mybuf = malloc(FILE_READ_SIZE *sizeof(char));
            if ((n = read(filefd, mybuf, FILE_READ_SIZE)) <= 0) {
              if (n != 0) {
                err_sys("File read error");
                return NULL;
              } else {
                printf ("\n\n\n\n\n\n\n\n");
                printf ("Recieved no data\n");
                sendhdr[idx].fin = 1;
              }
            }

            printf ("Sending: %s\n", mybuf);
            iovsend[idx][1].iov_base = mybuf;
            iovsend[idx][1].iov_len = n;
          }

          tt = rtt_ts(&rttinfo);
          for (i = 0; i < get_cur_window_size(); ++i) {
            printf ("I; %d\n", i);
            sendhdr[i].seq = ++seq;
            ((seq_header_t *)iovsend[i][0].iov_base)->ts = tt;
            if (((seq_header_t *)iovsend[i][0].iov_base)->fin == 1) {
              printf ("Found fin\n");
              breaknow = 1;
            }

            printf ("Just before sending\n");
            Sendmsg(fd, &msgsend[i], 0);
          }
          printf ("%d set of messages sent\n", loop++);

          alarm(rtt_start(&rttinfo));	/* calc timeout value & start timer */
#ifdef	RTT_DEBUG
          rtt_debug(&rttinfo);
#endif

          if (sigsetjmp(jmpbuf, 1) != 0) {
                  if (rtt_timeout(&rttinfo) < 0) {
                          err_msg("dg_send_recv: no response from server, giving up");
                          rttinit = 0;	/* reinit in case we're called again */
                          errno = ETIMEDOUT;
                          return(-1);
                  }
#ifdef	RTT_DEBUG
                  err_msg("dg_send_recv: timeout, retransmitting");
#endif
                  goto sendagain;
          }

          for (i = 0; i < get_cur_window_size(); ++i) {
                  n = Recvmsg(fd, &msgrecv, 0);
#ifdef	RTT_DEBUG
                  fprintf(stderr, "recv %4d\n", recvhdr.seq);
#endif
                  if (!(recvhdr.seq > (seq -get_cur_window_size() +1) && recvhdr.seq <= seq+1)) {
                    fprintf (stderr, "Client is doing some BS!!\n");
                    exit(0);
                  }
                  printf ("Recieved ack for: %d\n", recvhdr.seq -1);
          }
          printf ("Received the ack as well\n");

          alarm(0);			/* stop SIGALRM timer */
                  /* 4calculate & store new RTT estimator values */
          rtt_stop(&rttinfo, rtt_ts(&rttinfo) - recvhdr.ts);

          if (breaknow)
            break;
        }

	return(n - sizeof(seq_header_t));	/* return size of received datagram */
}

static void
sig_alrm(int signo)
{
	siglongjmp(jmpbuf, 1);
}
/* end dgsendrecv2 */

ssize_t
Dg_send_recv(int fd, const void *outbuff, size_t outbytes,
                       int filefd)
{
	ssize_t	n;

	n = dg_send_recv(fd, outbuff, outbytes, filefd);
	if (n < 0)
		err_quit("dg_send_recv error");

	return(n);
}

int
send_file(char *filename, int client_sockfd)
{
  int n;

  printf ("in send file\n");
  if (access(filename, F_OK | R_OK)) {
    printf ("File not accessible\n");
    return -1;
  }

  int filefd;
  if ((filefd = open(filename, O_RDONLY)) < 0) {
    err_sys("opening file failed:");
    return -1;
  }

  char buf[1024];
  char inbuf[1024];
  n = Dg_send_recv(client_sockfd, buf, 512, filefd);
}
