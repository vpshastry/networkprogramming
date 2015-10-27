#ifndef _WINDOW_HEADER_
#define _WINDOW_HEADER_

#include "header.h"

enum {
  ACK_NONE = 0,
  ACK_DUP = 1,
  ACK_NOTSENT = 2, // Packet not even sent.
  ACK_ERR = 3, // like 0/-ve # which is not at all possible.
} ack_type_t;

enum {
  MODE_SLOW_START = 1,
  MODE_CAVOID = 2,
} mode_t;

typedef struct {
  send_buffer_t *data;
  int ack;
} cqueue_t;

typedef struct mywindow {

  int seq;
  int tail;
  int head;
  int cwnd;
  mode_t mode;
  cqueue_t *queue;

  void (*init)(struct mywindow *, int);
  void (*reset)(struct mywindow *);
  int (*get_tail)(struct mywindow *);
  void (*append)(struct mywindow *, send_buffer_t *);
  send_buffer_t * (*dequeue)(struct mywindow *);
  int (*get_cwnd)(struct mywindow *);
  int (*add_new_ack)(struct mywindow *, int ack);
  void* (*get_buf)(struct mywindow *, int ack);
  int (*update_cwnd)(window_t *window);

} window_t;

int window_get_tail(window_t *window);
void window_reset(window_t *window);
void window_init(window_t *window, int size);
void window_append(window_t *window, send_buffer_t *newbuf);
send_buffer_t * window_dequeue(window_t *window);
int window_get_cwnd(window_t *window);
int window_add_new_ack(window_t *window, int ack);
send_buffer_t *window_get_buf(window_t *, int ack);
int window_update_cwnd(window_t *window);

static window_t newwindow = {
  .cwnd = 16,
  .seq = -1,
  .mode = AC
  .init = window_init,
  .reset = window_reset,
  .dequeue = window_dequeue,
  .append = window_append,
  .get_tail = window_get_tail,
  .add_new_ack = window_add_new_ack,
  .get_buf = window_get_buf,
  .update_cwnd = window_update_cwnd,
};
#endif
