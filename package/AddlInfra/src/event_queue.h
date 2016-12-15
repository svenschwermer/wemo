#ifndef _EVENT_QUEUE_H_
#define _EVENT_QUEUE_H_

#include "sysevent.h"

typedef struct {
  char *name;
  tuple_flag_t flag;
  async_id_t aid;
} sysevent_t;

typedef struct {
  int nType;
  char name[80];
  char value[80];
  void* pData;
  int nDataLen;
} event_t;

int open_sysevent(char *sysevent_name, token_t *tid);
int register_sysevent(int sysevent_fd, token_t tid, sysevent_t *pEvents, int nEvent);
int wait_sysevent(int sysevent_fd, token_t tid, char *name, int *name_len, char *value, int *value_len);
int wait_nonblock_sysevent(int sysevent_fd, token_t tid, char *name, int *name_len, char *value, int *value_len);
int unregister_sysevent(int sysevent_fd, token_t tid, sysevent_t *pEvents, int nEvent);
int close_sysevent(int sysevent_fd, token_t tid);
void* init_eventqueue(char *pEventName);
void uninit_eventqueue(void* pEventQueue);
void* create_event(char *pEventName, void *pEvents, int nEvent);
void delete_event(void *pEventQueue, void *pEvents, int nEvent);
void clear_event(void *pEventQueue);
int push_event(void *pEventQueue, event_t *pEvent);
event_t* wait_event(void* pEventQueue, int wait_sec);
event_t* get_event(void *pEventQueue, int nType, char *name, char *value);
event_t* pop_event(void *pEventQueue);

#endif  /* _EVENT_QUEUE_H_ */

