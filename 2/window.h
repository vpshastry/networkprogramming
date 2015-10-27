#ifndef _WINDOW_HEADER_
#define _WINDOW_HEADER_

#include "header.h"

enum {
  ACK_NONE = 0,
  ACK_DUP = 1,
  ACK_NOTSENT = 2, // Packet not even sent.
  ACK_ERR = 3, // like 0/-ve # which is not at all possible.
} ack_type_t;

typedef struct {
  send_buffer_t *data;
  int ack;
} cqueue_t;

typedef struct mywindow {
  enum {
    MODE_SLOW_START = 1,
    MODE_SLOW_CAVOID = 2,
  } mode_t;

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
  int (*selective_repeat)(struct mywindow *, int ack);

} window_t;

int window_get_tail(window_t *window);
void window_reset(window_t *window);
void window_init(window_t *window, int size);
void window_append(window_t *window, send_buffer_t *newbuf);
send_buffer_t * window_dequeue(window_t *window);
int window_get_cwnd(window_t *window);
int window_add_new_ack(window_t *window, int ack);

static window_t newwindow = {
  .cwnd = 16,
  .init = window_init,
  .reset = window_reset,
  .dequeue = window_dequeue,
  .append = window_append,
  .get_tail = window_get_tail,
  .add_new_ack = window_add_new_ack,
};
static window_t *window = &newwindow;
#endif
