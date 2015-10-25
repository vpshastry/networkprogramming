#include "header.h"

int
send_file(char *path)
{
  packets = break_into_fixed_size_packets();

  for (i = 0; i < get_cur_window_size(); ++i) {
    {
      // TODO: Form request.
      signal(SIGALRM, sig_alrm); /* establish signal handler */
      rtt_newpack();             /* initialize rexmt counter to 0 */
sendagain:
      while () {// to all packets in window
        sendto();

      alarm(rtt_start());        /* set alarm for RTO seconds */
      if (sigsetjmp(jmpbuf, 1) != 0) {
        if (rtt_timeout())     /* double RTO, retransmitted enough? */
          give up
        goto sendagain;        /* retransmit */
      }

      alarm(0);                  /* turn off alarm */
      rtt_stop();                /* calculate RTT and update estimators */

      do {
        recvfrom();
      } while (wrong sequence#);
    }
  }
}
