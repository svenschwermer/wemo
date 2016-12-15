#ifdef PRODUCT_WeMo_LEDLight
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "logger.h"
#include "event_queue.h"
#include "ulog.h"

#define USE_SYSEVENT

#define MAX_EVENTS 256

#ifdef USE_SYSEVENT
typedef struct {
  char event_name[80];
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  event_t* events[MAX_EVENTS];
  int eventno;
  int sysevent_fd;
  token_t tid;
} eventqueue_t;
#else
typedef struct {
  char event_name[80];
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  event_t* events[MAX_EVENTS];
  int eventno;
} eventqueue_t;
#endif


/******************************************************
* IN  : name of sysevent
* OUT : token_id
* RET : file descriptor of sysevent
******************************************************/
int open_sysevent(char *sysevent_name, token_t *tid)
{
  int sysevent_fd = 0;

  if( (sysevent_fd = sysevent_local_open(UDS_PATH, SE_VERSION, sysevent_name, tid)) < 0 )
  {
    if( (sysevent_fd = sysevent_local_open(UDS_PATH, SE_VERSION, sysevent_name, tid)) < 0 )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "update sysevent_local_open(%s) is failed", UDS_PATH);
      return -1;
    }
  }

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "%s sysevent_local_open: fd(%d), token(%d)", sysevent_name, sysevent_fd, *tid);

  return sysevent_fd;
}

/******************************************************
* IN  : event name that will be registered in sysevent
* IN  : event count
******************************************************/
int register_sysevent(int sysevent_fd, token_t tid, sysevent_t *pEvents, int nEvent)
{
  int i = 0, rc = 0;

  if( sysevent_fd <= 0 )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "register_sysevent: sysevent is not opened");
    return -1;
  }

  for( i = 0; i < nEvent; i++ )
  {
    rc = sysevent_set_options(sysevent_fd, tid, pEvents[i].name, pEvents[i].flag);
    if( rc != 0 )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "register sysevent_set_option: (%s, 0x%x) is failed: %d",
                pEvents[i].name, pEvents[i].flag, rc);
    }

    rc = sysevent_setnotification(sysevent_fd, tid, pEvents[i].name, &(pEvents[i].aid));
    if( rc != 0 )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "register sysevent_setnotification: (%s, %d) is failed: %d",
                pEvents[i].name, pEvents[i].aid, rc);
    }

    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "register_sysevent: %s", pEvents[i].name);
  }

  return rc;
}

int wait_sysevent(int sysevent_fd, token_t tid, char *name, int *name_len, char *value, int *value_len)
{
  async_id_t aid;
  int rc = -1;

  if( sysevent_fd <= 0 )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "wait_sysevent: sysevent is not opened");
    return rc;
  }

  //sysevent_getnotification() is blocking api until receiving an event...
  rc = sysevent_getnotification(sysevent_fd, tid, name, name_len, value, value_len, &aid);
  if( rc != 0 )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "wait_sysevent: sysevent_getnotification is failed: %d", rc);
  }

  return rc;
}

int wait_nonblock_sysevent(int sysevent_fd, token_t tid, char *name, int *name_len, char *value, int *value_len)
{
  async_id_t aid;
  int rc = -1;

  if( sysevent_fd <= 0 )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "wait_sysevent: sysevent is not opened");
    return rc;
  }

  //sysevent_nonblock_getnotification() is non blocking call, it should return within 1 sec.
  rc = sysevent_nonblock_getnotification(sysevent_fd, tid, name, name_len, value, value_len, &aid);
  if( rc != 0 )
  {
    if(rc != ERR_CANNOT_SET_STRING)
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "wait_sysevent: sysevent_nonblock_getnotification is failed: %d", rc);
    }
  }

  return rc;
}

int unregister_sysevent(int sysevent_fd, token_t tid, sysevent_t *pEvents, int nEvent)
{
  int i = 0, rc = 0;

  if( sysevent_fd <= 0 )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "unregister_sysevent: sysevent is not opened");
    return -1;
  }

  for( i = 0; i < nEvent; i++ )
  {
    rc = sysevent_rmnotification(sysevent_fd, tid, pEvents[i].aid);
    if( rc != 0 )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "unregister_sysevent: sysevent_rmnotification(aid: %d) is failed: %d",
                pEvents[i].aid, rc);
    }
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "unregister_sysevent: %s", pEvents[i].name);
  }

  return rc;
}

int close_sysevent(int sysevent_fd, token_t tid)
{
  int rc = 0;

  if( sysevent_fd <= 0 )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "close_sysevent: sysevent is not opened");
    return -1;
  }

  rc = sysevent_close(sysevent_fd, tid);

  if( rc != 0 )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "close_sysevent: sysevent_close(%d, %d) is failed: %d",
              rc, sysevent_fd, tid);
  }

  return rc;
}


void* create_event(char *pEventName, void *pEvents, int nEvent)
{
#ifdef USE_SYSEVENT
  int sysevent_fd = 0;
  token_t tid = 0;
#endif

  eventqueue_t *pEventQueue = init_eventqueue(pEventName);

#ifdef USE_SYSEVENT
  sysevent_fd = open_sysevent(pEventName, &tid);
  register_sysevent(sysevent_fd, tid, (sysevent_t*)pEvents, nEvent);
  pEventQueue->sysevent_fd = sysevent_fd;
  pEventQueue->tid = tid;
#endif

  return pEventQueue;
}

void delete_event(void *pEventQueue, void *pEvents, int nEvent)
{
#ifdef USE_SYSEVENT
  eventqueue_t *pEventQue = (eventqueue_t *)pEventQueue;
  unregister_sysevent(pEventQue->sysevent_fd, pEventQue->tid, (sysevent_t*)pEvents, nEvent);
  close_sysevent(pEventQue->sysevent_fd, pEventQue->tid);
#endif

  uninit_eventqueue(pEventQueue);
}

void* init_eventqueue(char *pEventName)
{
  eventqueue_t* queue = (eventqueue_t*)calloc(1, sizeof(eventqueue_t));

  if( queue )
  {
    memset(queue, 0, sizeof(eventqueue_t));

    strncpy(queue->event_name, pEventName, sizeof(queue->event_name));
    queue->eventno = 0;

    pthread_mutex_init(&queue->mutex, NULL);

#ifndef USE_SYSEVENT
    pthread_cond_init(&queue->cond, NULL);
#endif
  }

  return queue;
}

void uninit_eventqueue(void* pEventQueue)
{
  int n = 0;
  eventqueue_t* queue = (eventqueue_t*)pEventQueue;

  if( queue )
  {
    for( n = 0; n < queue->eventno; n++ )
    {
      if( queue->events[n]->pData )
      {
        free(queue->events[n]->pData);
      }
      free(queue->events[n]);
    }

    queue->eventno = 0;

#ifndef USE_SYSEVENT
    pthread_mutex_lock(&queue->mutex);
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
#endif

    pthread_mutex_destroy(&queue->mutex);

#ifndef USE_SYSEVENT
    pthread_cond_destroy(&queue->cond);
#endif
  }
}

void clear_event(void *pEventQueue)
{
  int n = 0;
  eventqueue_t* queue = (eventqueue_t*)pEventQueue;

  if( queue )
  {
    pthread_mutex_lock(&queue->mutex);

    for( n = 0; n < queue->eventno; n++ )
    {
      if( queue->events[n]->pData )
      {
        free(queue->events[n]->pData);
      }
      free(queue->events[n]);
    }

    queue->eventno = 0;

#ifndef USE_SYSEVENT
    pthread_cond_signal(&queue->cond);
#endif

    pthread_mutex_unlock(&queue->mutex);
  }
}

int push_event(void *pEventQueue, event_t *pEvent)
{
  int rc = 0;
  char szCommand[80] = {0,};

  eventqueue_t* queue = (eventqueue_t*)pEventQueue;

  if( queue == NULL )
  {
    return 0;
  }

  if( queue->eventno >= MAX_EVENTS )
  {
    return 0;
  }

  pthread_mutex_lock(&queue->mutex);

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "push_event: eventno = [%d]", queue->eventno);

  if( queue->events[queue->eventno] )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "push_event: queue->events[%d] is not empty...", queue->eventno);
    if( queue->events[queue->eventno]->pData )
    {
      free(queue->events[queue->eventno]->pData);
    }
    free(queue->events[queue->eventno]);
  }

  queue->events[queue->eventno] = calloc(1, sizeof(event_t));
  if( queue->events[queue->eventno] )
  {
    queue->events[queue->eventno]->nType = pEvent->nType;
    strncpy(queue->events[queue->eventno]->name, pEvent->name, sizeof(pEvent->name));
    strncpy(queue->events[queue->eventno]->value, pEvent->value, sizeof(pEvent->value));

    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "push_event: event_queue[%d] address = %p",
              queue->eventno, queue->events[queue->eventno]);
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "push_event: event_queue[%d] --> name = %s, value = %s",
              queue->eventno, queue->events[queue->eventno]->name, queue->events[queue->eventno]->value);

    if( pEvent->pData && pEvent->nDataLen )
    {
      queue->events[queue->eventno]->pData = calloc(1, pEvent->nDataLen+1);
      memcpy(queue->events[queue->eventno]->pData, pEvent->pData, pEvent->nDataLen);
      queue->events[queue->eventno]->nDataLen = pEvent->nDataLen;
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "push_event: event_queue[%d] --> data = %p, len = %d",
                queue->eventno, queue->events[queue->eventno]->pData, queue->events[queue->eventno]->nDataLen);
    }

    queue->eventno++;
  }

#ifdef USE_SYSEVENT
  memset(szCommand, 0x00, sizeof(szCommand));
  snprintf(szCommand, sizeof(szCommand), "sysevent set %s %s", pEvent->name, pEvent->value);

  rc = system(szCommand);
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "sysevent(%s, %s) : %d", pEvent->name, pEvent->value, rc);
#else
  pthread_cond_signal(&queue->cond);
#endif

  pthread_mutex_unlock(&queue->mutex);

  return 0;
}

event_t* get_event(void *pEventQueue, int nType, char *name, char *value)
{
  int n = 0;
  eventqueue_t* queue = (eventqueue_t*)pEventQueue;
  event_t* event = NULL;

  if( queue )
  {
    pthread_mutex_lock(&queue->mutex);

    for( n = 0; n < queue->eventno; n++ )
    {
      if( (queue->events[n]->nType == nType) ||
          (0 == strcmp(queue->events[n]->name, name) &&
           0 == strcmp(queue->events[n]->value, value)) )
      {
        event = queue->events[n];
        break;
      }
    }

    pthread_mutex_unlock(&queue->mutex);
  }

  return event;
}

event_t* pop_event(void *pEventQueue)
{
  int n = 0;
  eventqueue_t* queue = (eventqueue_t*)pEventQueue;
  event_t* event = NULL;

  if( queue && queue->eventno )
  {
    pthread_mutex_lock(&queue->mutex);

    event = queue->events[0];

    if( event )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pop_event: event_queue[0] --> event address = %p", event);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pop_event: event->pData = %p", event->pData);
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pop_event: event_queue[0] is NULL");
    }

    for( n = 1; n < queue->eventno; n++ )
    {
      queue->events[n-1] = queue->events[n];
    }

    queue->events[queue->eventno-1] = NULL;
    queue->eventno--;

    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pop_event: event_queue_no = %d", queue->eventno);

    pthread_mutex_unlock(&queue->mutex);
  }

  return event;
}

event_t* wait_event(void* pEventQueue, int wait_sec)
{
  int n = 0, rc = 0;
  eventqueue_t* queue = (eventqueue_t*)pEventQueue;
  event_t* event = NULL;

#ifdef USE_SYSEVENT

  int name_len = 0;
  int value_len = 0;
  char event_name[80];
  char event_value[80];

  int sysevent_fd = queue->sysevent_fd;
  token_t tid = queue->tid;

  name_len = sizeof(event_name);
  value_len = sizeof(event_value);

  if( wait_sec )
  {
    rc = wait_nonblock_sysevent(sysevent_fd, tid, event_name, &name_len, event_value, &value_len);
  }
  else
  {
    rc = wait_sysevent(sysevent_fd, tid, event_name, &name_len, event_value, &value_len);
  }

  if( rc == 0 )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "wait_event: name = %s, value = %s", event_name, event_value);
    event = pop_event(pEventQueue);
    if( event )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pop_event(name = %s, value = %s)", event->name, event->value);

      if( 0 == strcmp(event->name, event_name) &&
          0 == strcmp(event->value, event_value) )
      {
        return event;
      }
    }
    else
    {
      //Run command "sysevent set worker quit" in console mode to kill remote worker thread
      if( (0 == strcmp(event_name, "worker")) && (0 == strcmp(event_value, "quit")) )
      {
        event = calloc(1, sizeof(event_t));
        if( event )
        {
          event->nType = 1001; //QUIT_THREAD
        }
      }
    }
  }

#else

  struct timespec time_to_wait = {0, 0};
  struct timeval now;

  if( queue && queue->eventno )
  {
    pthread_mutex_lock(&queue->mutex);

    if( wait_sec )
    {
      time_to_wait.tv_sec = time(NULL) + wait_sec;
      rc = pthread_cond_timedwait(&queue->cond, &queue->mutex, &time_to_wait);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pthread_cond_timedwait: ret = %d",rc);

    }
    else
    {
      rc = pthread_cond_wait(&queue->cond, &queue->mutex);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pthread_cond_wai: ret = %d", rc);
    }

    event = queue->events[0];

    if( event )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "wait_event: event_queue[0] --> event address = %p", event);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "wait_event: event->pData = %p", event->pData);
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "wait_event: event_queue[0] is NULL");
    }

    for( n = 1; n < queue->eventno; n++ )
    {
      queue->events[n-1] = queue->events[n];
    }

    queue->events[queue->eventno-1] = NULL;
    queue->eventno--;

    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "wait_event: queue eventno = %d", queue->eventno);

    pthread_mutex_unlock(&queue->mutex);
  }

#endif

  return event;
}
#endif //#ifdef PRODUCT_WeMo_LEDLight
