#include "window.h"

int
window_get_tail(window_t *window)
{
  return window->tail;
}

void
window_reset(window_t *window)
{
  window->tail = window->head = -1;
}

void
window_init(window_t *window, int size, int cwnd)
{
  if (!(window->queue = (cqueue_t *) calloc(size, sizeof(cqueue_t))))
    err_sys("New memory allocation failed\n");

  window->head = window->tail = -1;
  window->inuse = 0;
  window->cwnd = 1;
  printf ("init\n");
  window->queue_size = size;
}

void
window_append(window_t *window, send_buffer_t *newbuf)
{
	printf ("@append\n");
  //window->debug(window);
  /*
  if (newbuf->hdr.seq != (window->head +1)) {
    fprintf (stderr, "\n\n\nCan't append\n\n\n");
    return;
  }
  */

  if (window->queue[newbuf->hdr.seq].data)
	  printf ("Data was already there\n");
  window->queue[newbuf->hdr.seq].data = (void *)newbuf;
}

int
window_add_new_ack(window_t *window, int ackno)
{
  if (ackno <= 0) {
    if (RTT_DEBUG) fprintf (stderr, "Received ack with #%d\n", ackno);
    return ACK_ERR;
  }

  if (ackno-1 > window->head) {
    if (RTT_DEBUG)
      fprintf (stderr, "Ack received for packet that isn't even sent or out of order: %d\n",
                ackno);
    return ACK_NONE;
  }

  window->queue[ackno-1].ack++;

  if (window->queue[ackno-1].ack >= 50){
    printf("3 duplicate ACKs : Fast Retransmit");
	return ACK_DUP;
  }
  return ACK_NONE;
}

send_buffer_t *
window_get_buf(window_t *window, int bufno)
{
	printf ("In get buf: %d\n", bufno);
  if (bufno < 0 || bufno > (window->queue_size -1)) {
	  printf ("Shouldn't happen\n");
    return NULL;
  }

  return window->queue[bufno].data;
}

void
window_update_cwnd(window_t *window, int received_dup_ack)
{
  switch (window->mode) {
    case MODE_SLOW_START:
      window->cwnd = received_dup_ack? window->cwnd >> 1: window->cwnd *2;
	  //printf ("@slow start mode\n");
	  //window->debug(window);
      break;

    // TODO: Revisit this case.
    case MODE_CAVOID:
      window->cwnd += (received_dup_ack? 0: 1);
	  //printf ("@mode cavoid\n");
	  //window->debug(window);
      break;

    default:
      fprintf (stderr, "Window mode unknown, assuming slow start\n");
      window->cwnd = received_dup_ack? window->cwnd >> 1: window->cwnd *2;
	  printf ("@default mode\n");
	  window->debug(window);
      break;
  }
}

void
window_clear(window_t *window)
{
  int i;

  for (i = window->tail; i <= window->head; ++i) {
    free(window->queue[i].data);
	window->queue[i].data = NULL;
  }
  window->tail = window->head;
  window->inuse = 0;
}

void
window_check_consistency(window_t *window)
{
  if ((window->head - window->tail + 1) != window->cwnd) {
    fprintf (stderr, "head(%d) - tail(%d) != cwnd(%d)\n",
              window->head, window->tail, window->cwnd);
    exit(0);
  }
}

int
window_prepare_cur_datagram(window_t *window, int seq, int filefd)
{
  int n;
  send_buffer_t *newsendbuff;
  int lastloop = 0;
  send_buffer_t *buf;
  int debugvar = (seq);
  printf ("Entered prepare cur datagram: operating on %d\n", debugvar);

  if ((buf = window->get_buf(window, seq))) {
    printf ("DEBUG: %d already filled\n", buf->hdr.seq);
    return buf->hdr.fin;
  }

  if (!(newsendbuff = calloc(1, sizeof(send_buffer_t)))) {
    fprintf (stderr, "New memory allocation failed\n");
    exit(0);
  }

  newsendbuff->hdr.seq = seq;
  printf("DEBUG: filling:%d\n", newsendbuff->hdr.seq);

  // Read from file to fill the buffer.
  if ((n = read(filefd, newsendbuff->payload, FILE_READ_SIZE)) <= 0) {
    if (n != 0) {
      err_sys("File read error");
      return -1;
    }
  }

  if (n == 0 || n < FILE_READ_SIZE) {
    if (RTT_DEBUG) fprintf (stderr, "I think this is the last datagram : %d\n", newsendbuff->hdr.seq);
	printf("Contents:\n%s\n", newsendbuff->payload);
    newsendbuff->hdr.fin = 1;
    lastloop = 1;
  }
  newsendbuff->length = n;

  /* Add this to window */
  window->append(window, newsendbuff);

  return lastloop;
}

void
window_debug(window_t *window)
{
	if (1)
	printf ("Window vars:-\nqueue_size: %d\ncwnd: %d\nhead: %d\ntail: %d\n",
			window->queue_size, window->cwnd, window->head, window->tail);
}
