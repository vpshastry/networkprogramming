#include "header.h"

typedef unsigned int uint32;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef struct {
  uint32  source_port;
  uint32  dest_port;
  uint32  seq_nu;
  uint32  ack_nu;
  uint8 nack;
  uint8 ack;
  uint8 fin;
  uint8 padding_var;
  uint32 length;
} seq_header_t;

/* Mallocs a new header variable. Caller has to free it. */
seq_header_t *
get_header(char *buffer)
{
  seq_header_t *header = calloc(1, sizeof(seq_header_t));
  memcpy(header, buffer, sizeof(seq_header_t));
  return header;
}

char *
get_data(char *buffer)
{
  return (buffer + sizeof(seq_header_t));
}

/*
   Creates a new buffer and the onus of freeing it is on the caller.
   */
char *
prepare_buffer(seq_header_t *header, char *data, int size)
{
  char *buffer = malloc(sizeof(seq_header_t) +size);

  memcpy(buffer, header, sizeof(seq_header_t));
  memcpy(buffer +sizeof(seq_header_t), data, size);

  return buffer;
}

seq_header_t *
get_header(uint32 source_port, uint32 dest_port, uint32 seq_nu,
          uint32 ack_nu, uint8 nack, uint8 ack, uint8 fin)
{
  seq_header_t *new_head = calloc(1, sizeof(seq_header_t));
  new_head->source_port = source_port;
  new_head->dest_port = dest_port;
  new_head->seq_nu = seq_nu;
  new_head->ack_nu = ack_nu;
  new_head->nack = nack;
  new_head->ack = ack;
  new_head->fin = fin;
  return new_head;
}

int
send_file(char *path)
{
  if (access(path, F_OK | R_OK)) {
    err_sys("Requested file doesn't exist\n");
    return -1;
  }

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
