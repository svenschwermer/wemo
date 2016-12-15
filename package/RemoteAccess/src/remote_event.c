#ifdef PRODUCT_WeMo_LEDLight
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "logger.h"
#include "event_queue.h"
#include "remote_event.h"

static pthread_mutex_t remote_mutex_lock;

void *pWorkEventQueue = NULL;

sysevent_t worker_events[] = {
  {"worker",          TUPLE_FLAG_EVENT, {0,0}},
  {"register",        TUPLE_FLAG_EVENT, {0,0}},
  {"subdevice",       TUPLE_FLAG_EVENT, {0,0}},
  {"group",           TUPLE_FLAG_EVENT, {0,0}},
  {"capability",      TUPLE_FLAG_EVENT, {0,0}},
  {"query",           TUPLE_FLAG_EVENT, {0,0}},
  {"device",          TUPLE_FLAG_EVENT, {0,0}},
};


void init_remote_lock()
{
  pthread_mutex_init(&remote_mutex_lock, NULL);
}

void destroy_remote_lock()
{
  pthread_mutex_destroy(&remote_mutex_lock);
}

void remote_lock()
{
  pthread_mutex_lock(&remote_mutex_lock);
}

void remote_unlock()
{
  pthread_mutex_unlock(&remote_mutex_lock);
}

void create_work_event()
{
  int nEvent = sizeof(worker_events) / sizeof(worker_events[0]);
  pWorkEventQueue = create_event("WorkEvent", worker_events, nEvent);
}

void destroy_work_event()
{
  int nEvent = sizeof(worker_events) / sizeof(worker_events[0]);
  if( pWorkEventQueue )
  {
    delete_event(pWorkEventQueue, worker_events, nEvent);
  }

  pWorkEventQueue = NULL;
}

event_t* wait_work_event(int wait_sec)
{
  return wait_event(pWorkEventQueue, wait_sec);
}

int trigger_remote_event(int receiver, int event_type, char *name, char *value, void *payload, size_t length)
{
  int rc = 0;
  event_t event;

  remote_lock();

  memset(&event, 0x00, sizeof(event_t));

  event.nType = event_type;
  event.pData = NULL;

  if( name )
  {
    strncpy(event.name, name, sizeof(event.name));
  }

  if( value )
  {
    strncpy(event.value, value, sizeof(event.value));
  }

  if( payload && length )
  {
    event.pData = calloc(1, length);
    if( event.pData )
    {
      memcpy(event.pData, payload, length);
      event.nDataLen = length;
    }
  }

  if( receiver == WORK_EVENT )
  {
    if( pWorkEventQueue )
    {
      push_event(pWorkEventQueue, &event);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "push_work_event(%s, %s, %p, %d)",
                event.name, event.value, event.pData, event.nDataLen);
    }
  }

  if( event.pData )
  {
    free(event.pData);
  }

  remote_unlock();

  return rc;
}
#endif //#ifdef PRODUCT_WeMo_LEDLight
