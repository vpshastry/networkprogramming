#include "header.h"

static int start;
static int end;
static sliding_window_t *window;
static int window_size;

sliding_window_t *
queue_init(int queue_size)
{
  if (!(window = calloc(queue_size, sizeof(sliding_window_t *)))) {
      printf ("Failed to allocate new memory\n");
      exit(0);
  }
  start = -1;
  end = 0;

  return window;
}

int
queue_reset(int queue_size)
{
  if (queue_size != window_size) {
    if (!(window = realloc (window, queue_size * sizeof(sliding_window_t *)))) {
      printf ("Failed to allocate new memory\n");
      exit(0);
    }
    memset(window, 0, queue_size *sizeof(sliding_window_t));
  }
  start = -1;
  end = 0;
}

int
enqueue(sliding_window_t *new_data)
{
  if (((start +1)%window_size) ==  end) {
#ifdef DEBUG
    ERR(0, "Window full\n");
#endif
    return -1;
  }
  window[++start%window_size] = new_data;
}

sliding_window_t *
dequeue()
{
  if (start == -1 && end == 0) {
  }
}
