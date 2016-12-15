#ifdef PRODUCT_WeMo_LEDLight

#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <pthread.h>
#include <sys/queue.h>

STAILQ_HEAD(QueueHead, QueueEntry);

struct Queue {
  pthread_mutex_t mutex;
  pthread_cond_t cv;
  pthread_cond_t enq_wait_cv;
  int enq_waiters;
  int length;
  int limit;
  struct QueueHead queue;
};

struct QueueEntry {
  void *item;
  STAILQ_ENTRY(QueueEntry) entries;
};

struct Queue* queue_init();
int queue_destroy(struct Queue *q);
int queue_empty(struct Queue *q);
int queue_full(struct Queue *q);
int queue_enq(struct Queue *q, void *item);
int queue_length(struct Queue *q);
void queue_limit(struct Queue *q, int limit);
void *queue_deq(struct Queue *q);

#endif

#endif   // #ifdef PRODUCT_WeMo_LEDLight
