#ifndef _REMOTE_EVENT_
#define _REMOTE_EVENT_
#ifdef PRODUCT_WeMo_LEDLight
#include "event_queue.h"

#define WORK_EVENT              100

#define EVENTTYPE_USEREVENT     1000

#define QUIT_THREAD             1001    //Quit thread event
#define INIT_REGISTER           1002
#define ADD_SUBDEVICE           1003    //Add subdevice event
#define DEL_SUBDEVICE           1004    //Delete subdevice event
#define DEL_SUBDEVICES          1005    //Delete all subdevices event
#define UPDATE_DEVICE           1006    //Update Device notify event
#define UPDATE_SUBDEVICE        1007    //Update SubDevice notify event
#define CREATE_GROUPDEVICE      1008    //Create group device event
#define UPDATE_GROUPDEVICE      1009    //Update group device event
#define DELETE_GROUPDEVICE      1010    //Delete group device event
#define UPDATE_CAPABILITY       1011    //Update new capability profile list
#define QUERY_CAPABILITY        1012    //Query capability profile list
#define GET_HOME_DEVICES        1013    //Query to get the all registered id of devices in home.

void init_remote_lock();
void destroy_remote_lock();
void remote_lock();
void remote_unlock();

void create_work_event();
void destroy_work_event();

event_t* wait_work_event(int wait_sec);

int trigger_remote_event(int receiver, int event_type, char *name, char *value, void *payload, size_t length);
#endif
#endif  /* _REMOTE_EVENT_ */
