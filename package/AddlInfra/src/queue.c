#ifdef PRODUCT_WeMo_LEDLight

#include <stdio.h>
#include <stdlib.h>

#include "queue.h"

struct Queue* queue_init()
{
  struct Queue *q = NULL;

  q = (struct Queue *)calloc(1, sizeof(struct Queue));

  if( q )
  {
    q->length = 0;
    q->limit = -1;
    q->enq_waiters = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cv, NULL);
    pthread_cond_init(&q->enq_wait_cv, NULL);
    STAILQ_INIT(&q->queue);
  }

  return (q);
}

int queue_destroy(struct Queue *q)
{
  struct QueueEntry *qi;

  while( !STAILQ_EMPTY(&q->queue) )
  {
    qi = STAILQ_FIRST(&q->queue);
    STAILQ_REMOVE_HEAD(&q->queue, entries);
    q->length--;
    free(qi);
  }

  pthread_cond_destroy(&q->cv);
  pthread_cond_destroy(&q->enq_wait_cv);
  pthread_mutex_destroy(&q->mutex);

  free(q);

  return 1;
}

int queue_empty(struct Queue *q)
{
  return (STAILQ_EMPTY(&q->queue));
}

int queue_full(struct Queue *q)
{
  return (q->limit > 0 && q->length >= q->limit);
}

int queue_enq(struct Queue *q, void *item)
{
  struct QueueEntry *qi;

  pthread_mutex_lock(&q->mutex);

  if( queue_full(q) )
  {
    q->enq_waiters++;
    while( queue_full(q) )
    {
      pthread_cond_wait(&q->enq_wait_cv, &q->mutex);
    }
    q->enq_waiters--;
  }

  if( !(qi = (struct QueueEntry *)calloc(1, sizeof(struct QueueEntry))) )
  {
    return 0;
  }

  qi->item = item;

  STAILQ_INSERT_TAIL(&q->queue, qi, entries);
  q->length++;
  pthread_cond_signal(&q->cv);
  pthread_mutex_unlock(&q->mutex);

  return 1;
}


void *queue_deq(struct Queue *q)
{
  void *ret = NULL;

  struct QueueEntry *qi;

  pthread_mutex_lock(&q->mutex);

  while( STAILQ_EMPTY(&q->queue) )
  {
    pthread_cond_wait(&q->cv, &q->mutex);
  }

  qi = STAILQ_FIRST(&q->queue);
  STAILQ_REMOVE_HEAD(&q->queue, entries);
  q->length--;
  ret = qi->item;
  free(qi);

  if( q->enq_waiters > 0 )
  {
    pthread_cond_signal(&q->enq_wait_cv);
  }

  pthread_mutex_unlock(&q->mutex);
  return ret;
}

int queue_length(struct Queue *q)
{
  return (q->length);
}

void queue_limit(struct Queue *q, int limit)
{
  q->limit = limit;
}


#endif   // #ifdef PRODUCT_WeMo_LEDLight
