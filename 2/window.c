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
  window->queue_size = size;
}

void
window_append(window_t *window, send_buffer_t *newbuf)
{
  if (newbuf->hdr.seq != (window->head +1)) {
    fprintf (stderr, "\n\n\nCan't append\n\n\n");
    return;
  }

  window->queue[++window->head].data = (void *)newbuf;
  if (!window->inuse) {
    window->tail = window->head;
    window->inuse = 1;
  }
}

send_buffer_t *
window_dequeue(window_t *window, int ackno)
{
  if ((ackno -1) < 0) {
    if (RTT_DEBUG) fprintf (stderr, "Ack number(%d) is < 0\n", ackno);
    return NULL;
  }

  send_buffer_t *retdata= window->queue[ackno -1].data;
  window->queue[ackno -1].data = NULL;

  /*
  if (window->tail > window->head)
    window->reset(window);
    */

  return retdata;
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
      fprintf (stderr, "Ack received for packet that isn't even sent: %d\n",
                ackno);
    return ACK_NOTSENT;
  }

  window->queue[ackno-1].ack++;

  if (window->queue[ackno-1].ack > 1)
    return ACK_DUP;
  return ACK_NONE;
}

send_buffer_t *
window_get_buf(window_t *window, int bufno)
{
  if (bufno < 0 || bufno > window->queue_size)
    return NULL;

  return window->queue[bufno].data;
}

void
window_update_cwnd(window_t *window, int received_dup_ack)
{
  switch (window->mode) {
    case MODE_SLOW_START:
      window->cwnd = received_dup_ack? window->cwnd >> 1: window->cwnd *2;
      break;

    // TODO: Revisit this case.
    case MODE_CAVOID:
      window->cwnd += (received_dup_ack? 0: 1);
      break;

    default:
      fprintf (stderr, "Window mode unknown, assuming slow start\n");
      window->cwnd = received_dup_ack? window->cwnd >> 1: window->cwnd *2;
      break;
  }
}

void
window_clear(window_t *window)
{
  int i;

  for (i = window->tail; i <= window->head; ++i) {
    free(window->queue[i].data);
    memset(&window->queue[i], 0, sizeof(cqueue_t));
  }
  window->tail = window->head;
  window->inuse = 0;
}

void
window_check_consistency(window_t *window)
{
  if (window->inuse && ((window->head - window->tail + 1) != window->cwnd)) {
    fprintf (stderr, "head(%d) - tail(%d) != cwnd(%d)\n",
              window->head, window->tail, window->cwnd);
    exit(0);
  }
}

int
window_prepare_cur_datagram(window_t *window, long long *seq, int filefd)
{
  int n;
  send_buffer_t *newsendbuff;
  int lastloop = 0;
  send_buffer_t *buf;

  if ((buf = window->get_buf(window, (*seq) +1))) {
    if (!window->inuse) {
      window->inuse = 1;
      window->tail = window->head;
    }

    return buf->hdr.fin;
  }

  if (!(newsendbuff = calloc(1, sizeof(send_buffer_t)))) {
    fprintf (stderr, "New memory allocation failed\n");
    exit(0);
  }

  newsendbuff->hdr.seq = ++(*seq);

  // Read from file to fill the buffer.
  if ((n = read(filefd, newsendbuff->payload, FILE_READ_SIZE)) <= 0) {
    if (n != 0) {
      err_sys("File read error");
      return -1;
    }
  }

  if (n == 0 || n < FILE_READ_SIZE) {
    if (RTT_DEBUG) fprintf (stderr, "I think this is the last datagram\n");
    newsendbuff->hdr.fin = 1;
    lastloop = 1;
  }
  newsendbuff->length = n;

  /* Add this to window */
  window->append(window, newsendbuff);

  return lastloop;
}
