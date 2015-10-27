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
  window->queue[++window->head].data = (void *)newbuf;
  if (window->tail == -1)
    window->tail = 0;
}

send_buffer_t *
window_dequeue(window_t *window)
{
  if (window->tail == -1) {
    if (RTT_DEBUG) fprintf (stderr, "Nothing in window\n");
    return NULL;
  }

  send_buffer_t *retdata= window->queue[window->tail++].data;

  if (window->tail > window->head)
    window->reset(window);

  return retdata;
}

int
window_add_new_ack(window_t *window, int ack)
{
  if (ack <= 0) {
    if (RTT_DEBUG) fprintf (stderr, "Received ack with #%d\n", ack);
    return ACK_ERR;
  }

  if (ack-1 > window->head) {
    if (RTT_DEBUG) fprintf (stderr, "Ack received for packet that isn't even sent: %d\n", ack);
    return ACK_NOTSENT;
  }

  window->queue[ack-1].ack++;

  if (window->queue[ack-1].ack > 3)
    return ACK_DUP;
  return ACK_NONE;
}
