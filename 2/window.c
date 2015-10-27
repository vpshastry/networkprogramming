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
window_init(window_t *window, int size)
{
  if (!(window->queue = (cqueue_t *) calloc(size, sizeof(cqueue_t))))
    err_sys("New memory allocation failed\n");
  window->head = window->tail = -1;
}

void
window_append(window_t *window, send_buffer_t *newbuf)
{
  if (newbuf->hdr.seq != (window->head +1)) {
    fprintf (stderr, "\n\n\nCan't append\n\n\n");
    return;
  }

  window->queue[++window->head].data = (void *)newbuf;
  if (window->tail == -1)
    window->tail = 0;
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

  if (window->queue[ackno-1].ack > 3)
    return ACK_DUP;
  return ACK_NONE;
}

send_buffer_t *
window_get_buf(window_t *window, int bufno)
{
  if (bufno < window->tail || bufno > window->head) {
    fprintf (stderr, "Can't find buf for ack: %d while window is %d-%d\n",
              bufno, window->tail, window->head);
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

  if ((window->head - window->tail + 1) != window->cwnd) {
    fprintf (stderr, "head - tail != cwnd\n");
    exit(0);
  }

  for (i = window->tail; i <= window->head; ++i) {
    free(window->queue[i].data);
    memset(&window->queue[i], 0, sizeof(cqueue_t));
  }
  window->tail = window->head +1;
}
