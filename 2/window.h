#ifndef _WINDOW_HEADER_
#define _WINDOW_HEADER_

#include "header.h"

typedef enum {
  ACK_NONE = 0,
  ACK_DUP = 1,
  ACK_NOTSENT = 2, // Packet not even sent.
  ACK_ERR = 3 // like 0/-ve # which is not at all possible.
} ack_type_t;

typedef enum {
  MODE_SLOW_START = 1,
  MODE_CAVOID = 2,
  MODE_FAST_RECOV = 3
} window_mode_t;

typedef struct {
  send_buffer_t *data;
  int ack;
} cqueue_t;
typedef cqueue_t buffer_t;

typedef struct mywindow {

  int seq;
  int tail;
  int head;
  int cwnd;
  int ssthresh;
  int inuse;
  window_mode_t mode;
  cqueue_t *queue;
  int queue_size;

  void (*init)(struct mywindow *, int, int);
  void (*reset)(struct mywindow *);
  int (*get_tail)(struct mywindow *);
  void (*append)(struct mywindow *, send_buffer_t *);
  int (*add_new_ack)(struct mywindow *, int ack);
  send_buffer_t* (*get_buf)(struct mywindow *, int ack);
  void (*update_cwnd)(struct mywindow *, int);
  void (*clear)(struct mywindow *);
  void (*check_consistency)(struct mywindow *);
  int (*prepare_cur_datagram)(struct mywindow *, int , int);
  void (*debug)(struct mywindow *);

} window_t;

int window_get_tail(window_t *window);
void window_reset(window_t *window);
void window_init(window_t *window, int size, int cwnd);
void window_append(window_t *window, send_buffer_t *newbuf);
int window_add_new_ack(window_t *window, int ack);
send_buffer_t *window_get_buf(window_t *, int ack);
void window_update_cwnd(window_t *window, int);
void window_check_consistency(window_t *window);
void window_clear(window_t *window);
int window_prepare_cur_datagram(window_t *window, int, int filefd);
void window_debug(window_t *window);

static window_t newwindow = {
  .seq = -1,
  .mode = MODE_SLOW_START,
  .init = window_init,
  .reset = window_reset,
  .append = window_append,
  .get_tail = window_get_tail,
  .add_new_ack = window_add_new_ack,
  .get_buf = window_get_buf,
  .update_cwnd = window_update_cwnd,
  .clear = window_clear,
  .check_consistency = window_check_consistency,
  .prepare_cur_datagram = window_prepare_cur_datagram,
  .debug = window_debug,
};
#endif
