/***************************************************************************
*
*
* timer.c
*
* Created by Belkin International, Software Engineering on XX/XX/XX.
* Copyright (c) 2012-2013 Belkin International, Inc. and/or its affiliates. All rights reserved.
*
* Belkin International, Inc. retains all right, title and interest (including all
* intellectual property rights) in and to this computer program, which is
* protected by applicable intellectual property laws.  Unless you have obtained
* a separate written license from Belkin International, Inc., you are not authorized
* to utilize all or a part of this computer program for any purpose (including
* reproduction, distribution, modification, and compilation into object code)
* and you must immediately destroy or return to Belkin International, Inc
* all copies of this computer program.  If you are licensed by Belkin International, Inc., your
* rights to utilize this computer program are limited by the terms of that license.
*
* To obtain a license, please contact Belkin International, Inc.
*
* This computer program contains trade secrets owned by Belkin International, Inc.
* and, unless unauthorized by Belkin International, Inc. in writing, you agree to
* maintain the confidentiality of this computer program and related information
* and to not disclose this computer program and related information to any
* other person or entity.
*
* THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND BELKIN INTERNATIONAL, INC.
* EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING THE WARRANTIES OF
* MERCHANTIBILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND NON-INFRINGEMENT.
*
*
***************************************************************************/

#ifdef PRODUCT_WeMo_LEDLight

#include "timer.h"
#include "ulog.h"

int create_timer(char *name, timer_t *timer_id, void (*handler)(int, siginfo_t*, void*),
                int expire_sec, int interval_sec)
{
  int nRet = 0;

  struct sigevent sev;
  struct itimerspec its;
  struct sigaction sa;
  int sig_no = SIGRTMAX;

  // Setup signal handler for timer
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = handler;
  sigemptyset(&sa.sa_mask);

  if( (nRet = sigaction(sig_no, &sa, NULL)) == -1 )
  {
    return nRet;
  }

  // Set and enable alarm
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = sig_no;
  sev.sigev_value.sival_ptr = timer_id;

  // Create the timer
  if( (nRet = timer_create(CLOCK_REALTIME, &sev, timer_id)) == -1 )
  {
    return nRet;
  }

  // Start the timer
  its.it_value.tv_sec = expire_sec;
  its.it_value.tv_nsec = 0;

  its.it_interval.tv_sec = interval_sec;
  its.it_interval.tv_nsec = 0;

  if( (nRet = timer_settime(*timer_id, 0, &its, NULL)) == -1 )
  {
    timer_delete(*timer_id);
  }

  return nRet;
}


void start_timer(timer_t timer_id, int expire_sec, int interval_sec)
{
  int nRet = 0;
  struct itimerspec its;

  // Start the timer
  its.it_value.tv_sec = expire_sec;
  its.it_value.tv_nsec = 0;

  its.it_interval.tv_sec = interval_sec;
  its.it_interval.tv_nsec = 0;

  if( (nRet = timer_settime(timer_id, 0, &its, NULL)) == -1 )
  {
    DEBUG_LOG(ULOG_SYSTEM, UL_DEBUG, "Failure: start_timer(%d, %d)", expire_sec, interval_sec);
  }
}

void stop_timer(timer_t timer_id)
{
  struct itimerspec its;

  memset(&its, 0, sizeof(its));
  timer_settime(timer_id, 0, &its, NULL);
}

int destroy_timer(timer_t timer_id)
{
  return timer_delete(timer_id);
}


#else  /* End of PRODUCT_WeMo_LEDLight */

#include <stdio.h> 
#include <malloc.h>
#include <memory.h>
#include <syslog.h>
#include <pthread.h>

#include "upnp/FreeList.h"
#include "upnp/LinkedList.h"
#include "upnp/ThreadPool.h"
#include "timer.h"
#include "logger.h"
#include "defines.h"
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

TimerThreadAttrs *pgTimerThread;
ListNode *ListMoveBefore( LinkedList * list, ListNode *insNode, ListNode * anode );
ListNode *ListMoveTail( LinkedList * list, ListNode *node );
int timerRunJob(int (*f)(void*), void *data);
static ThreadPool gltimerThreadPool;
/************************************************************************
 * Function: timerInitModule
 * 
 *   Initializes and starts timer module including timer thread 
 *   and associated link list
 *
 *  Input Parameters:
 *      None
 *  Return:
 *      0 on success, nonzero on failure
 ************************************************************************/
int
timerInitModule()
{

  int retVal = 0;
  pthread_t  timerEventsThread;
  pthread_attr_t PthreadAttr;
 
	ThreadPoolAttr attr;
	TPAttrInit(&attr);
	TPAttrSetMaxThreads(&attr, P_MAX_THREADS);
	TPAttrSetMinThreads(&attr, P_MIN_THREADS);
	TPAttrSetJobsPerThread(&attr, P_JOBS_PER_THREAD);
	TPAttrSetIdleTime(&attr, P_THREAD_IDLE_TIME);
	TPAttrSetMaxJobsTotal(&attr, P_MAX_JOBS_TOTAL);
	if (ThreadPoolInit(&gltimerThreadPool, &attr) != PLUGIN_SUCCESS) {
		APP_LOG("PLUGINDispatcher", LOG_ERR, " Timer Thread Pool Initialization Failed\n");
		return PLUGIN_FAILURE;
	}

  pgTimerThread = (TimerThreadAttrs *)ZALLOC(sizeof(TimerThreadAttrs)); 
		
  pthread_mutex_init( &pgTimerThread->mutex, NULL );
  pthread_mutex_lock( &pgTimerThread->mutex );
  pthread_cond_init( &pgTimerThread->conVar, NULL );
  
  pgTimerThread->cleanUp = 0;
  pgTimerThread->lastRegisteredEventId = 0;
  retVal = ListInit( &pgTimerThread->timerEventQueue, NULL, NULL );
	retVal += FreeListInit( &pgTimerThread->timerFreeEvents, sizeof(TimerEventAttrs) ,10 );
	
  if (retVal < 0) {
		APP_LOG("PluginTimer", LOG_ERR, " List Initialization failed <%d>\n", retVal);
  }

  pthread_mutex_unlock( &pgTimerThread->mutex );
	
  if( retVal != 0 ) {
    pthread_cond_destroy( &pgTimerThread->conVar );
    pthread_mutex_destroy( &pgTimerThread->mutex );
    ListDestroy( &pgTimerThread->timerEventQueue, 0 );
  } else {
     pthread_attr_init(&PthreadAttr);
     pthread_attr_setdetachstate(&PthreadAttr,PTHREAD_CREATE_DETACHED);

     retVal = pthread_create( &timerEventsThread, &PthreadAttr, timerSchedulerThread , NULL);	
  }
  
  APP_LOG("PluginTimer", LOG_DEBUG, " Timer Thread Initialized ");  
  return retVal;
}
/****************************************************************************
 * Function: timerCalculateTime
 *
 *   Calculates the appropriate eventTime in absolute seconds since
 *   Jan 1, 1970
 *
 * Parameters:
 *   time_t *eventTime - eventTime
 *   type - absolute or relative       
 *****************************************************************************/
static int timerCalculateTime( time_t * eventTime, eventTimeType type )
{
  time_t currentTime;

  if (eventTime == NULL )
	return ERROR_EVENT_TIME;

  if( type == ABSOLUTE )
    return 0;
  else if( type == RELATIVE ) {
    time( &currentTime );
    *eventTime += currentTime;
    return 0;
  }

  APP_LOG("PluginTimer", LOG_DEBUG, " Timer Event Calculted ");  
  return ERROR_EVENT_TIME;
}
/****************************************************************************
 * Function: timerCreateEvent 
 *
 *  Desc.:
 *    Adds a Timer Event to the link list.
 *  Input Parameters:
 *    timer – Pointer to TimerThreadAttrs structure.
 *    func  - thread function to run.
 *    arg   - argument to function.
 *    priority  - priority of job.
 *    eventTime - the absoule time of the event
 *          in seconds from Jan, 1970
 *    id - id of job
 *    
 *  Returns:
 *    TimerEventAttrs * on success, NULL on failure.
 ****************************************************************************/
static TimerEventAttrs * timerCreateEvent( TimerThreadAttrs * pgTimerThread, int (*job_func)(), void *job_data,
                                           int recurrence, int recurringInterval, time_t eventTime, int id )
{
  TimerEventAttrs *temp = NULL;

  if( pgTimerThread == NULL || job_func == NULL )
	return NULL;

	temp = ( TimerEventAttrs * ) FreeListAlloc( &pgTimerThread->timerFreeEvents );
  if( temp == NULL )
    return temp;
  memset(temp, 0x0,  sizeof(TimerEventAttrs));	
  
  temp->job_func = job_func;
  temp->job_data = job_data;
  temp->recurrence = recurrence;
  temp->recurringInterval = recurringInterval;
  temp->eventTime = eventTime;
  temp->id = id;

  APP_LOG("PluginTimer", LOG_DEBUG, " Timer Event Created with id %d ", temp->id ); 	
  return temp;
}

/****************************************************************************
 * Function: timerDeleteEvent
 *
 *    Deletes previously allocated TimerEventAttrs.
 *  Parameters:
 *    TimerEventAttrs *event - must be allocated with TimerCreateEvent
 *****************************************************************************/
static void timerDeleteEvent( TimerThreadAttrs * pgTimerThread, TimerEventAttrs * event )
{

	 if( pgTimerThread != NULL && event !=NULL){
     FreeListFree( &pgTimerThread->timerFreeEvents, event ); 
   }
   APP_LOG("PluginTimer", LOG_DEBUG, " Timer Event Deleted " ); 	
}
/****************************************************************************
 * Function: timerSchedulerThread 
 *
 *   Implements timer thread.
 *   Waits for next event to occur and schedules
 *   associated job_func into threadpool.
 *   Internal Only.
 *  Parameters:
 *    None
 *  Returns:
 *    None
 *****************************************************************************/
void *timerSchedulerThread ()
{
    
  ListNode *head = NULL;
  ListNode *tail = NULL;
  ListNode *tempNode = NULL;
  TimerEventAttrs *temp = NULL;
  TimerEventAttrs *nextEvent = NULL;
	
  time_t currentTime = 0;
  time_t nextEventTime = 0;
  struct timespec timeToWait;
  int found = 0;

  tu_set_my_thread_name( __FUNCTION__ );
	
  pthread_mutex_lock( &pgTimerThread->mutex );

  while( 1 )
  {
    found = 0;
    APP_LOG("PluginTimer", LOG_DEBUG, " Timer Thread Loop ");
	if( pgTimerThread->cleanUp ) 
    {
      pgTimerThread->cleanUp = 0;
      pthread_cond_signal( &pgTimerThread->conVar );
      pthread_mutex_unlock( &pgTimerThread->mutex );
      return NULL;
    }

    nextEvent = NULL;

	  APP_LOG("PluginTimer", LOG_DEBUG, " Total Events in the timerEventQueueList > %d ", pgTimerThread->timerEventQueue.size );
    if( pgTimerThread->timerEventQueue.size > 0 )
    {
      head = ListHead( &pgTimerThread->timerEventQueue );
      tail = ListTail( &pgTimerThread->timerEventQueue );
      nextEvent = ( TimerEventAttrs * ) head->item;
      nextEventTime = nextEvent->eventTime;
	    APP_LOG("PluginTimer", LOG_DEBUG, " Event found in list, event time < in abs seconds > %ld and id %d ", nextEventTime, nextEvent->id );
    }

    currentTime = time( NULL );
  	APP_LOG("PluginTimer", LOG_DEBUG, " Current time <in abs seconds> %ld ",currentTime );

    if( ( nextEvent != NULL ) && ( currentTime >= nextEventTime ) )
    {
			
      if( nextEvent->recurrence) {// logic for periodic 
     		timerRunJob( nextEvent->job_func, nextEvent->job_data );
		    nextEvent->eventTime += nextEvent->recurringInterval;
		    tempNode = ListNext( &pgTimerThread->timerEventQueue, head);
		    while( tempNode != NULL ) {
		      temp = ( TimerEventAttrs * ) tempNode->item;
		      if( temp->eventTime >= nextEvent->eventTime ){
	    			APP_LOG("PluginTimer", LOG_DEBUG, " Event found in list, event time < in abs seconds > %ld %ld and id %d %d", temp->eventTime,nextEvent->eventTime, temp->id, nextEvent->id );
		        ListMoveBefore( &pgTimerThread->timerEventQueue, head, tempNode );
		        found = 1;
		        break;
		      }	
		      tempNode = ListNext( &pgTimerThread->timerEventQueue, tempNode );
	    	}
		
		    if( !found ) {
		        ListMoveBefore( &pgTimerThread->timerEventQueue, head, tail->next );
	    			APP_LOG("PluginTimer", LOG_DEBUG, " Event moved to tail");
		    } 
				
        }else { 
          timerRunJob( nextEvent->job_func, nextEvent->job_data );
		      temp = ( TimerEventAttrs * ) head->item;
	    		APP_LOG("PluginTimer", LOG_DEBUG, " Event found in list, event time < in abs seconds > %ld %ld and id %d %d", temp->eventTime,nextEvent->eventTime, temp->id, nextEvent->id );
		      ListDelNode( &pgTimerThread->timerEventQueue, head, 0 );
          timerDeleteEvent( pgTimerThread, nextEvent );
					if (temp) {
						free(temp);
						temp = NULL;
					}
        }
      
        continue;
    }

    if( nextEvent != NULL ) {
      timeToWait.tv_nsec = 0;
      timeToWait.tv_sec = nextEvent->eventTime;
	    APP_LOG("PluginTimer", LOG_DEBUG, " Event not null before timedwait");
      pthread_cond_timedwait( &pgTimerThread->conVar, &pgTimerThread->mutex,
                  &timeToWait );
	    APP_LOG("PluginTimer", LOG_DEBUG, " Event not null after timedwait");
	    currentTime = time( NULL );
    } else {
	    APP_LOG("PluginTimer", LOG_DEBUG, " Event null before timedwait");
      pthread_cond_wait( &pgTimerThread->conVar, &pgTimerThread->mutex );
	    APP_LOG("PluginTimer", LOG_DEBUG, " Event null after timedwait");
    }
  }// end of while loop  

}
/************************************************************************
 *  Function: timerRegisterEvents
 * 
 *  Desc.:
 *    Registers and schedules an event to run at a specified time.
 *
 *  Input Parameters:
 *    time_t - time of event in absolute  or relative seconds.
 *    eventTimeType – event time in either absolute, or relative terms. 
 *                    for relative events, time considered is 
 *                    current system time + relative time.
 *    job_func -> valid function pointer
 *    job_data -> argument to function
 *    recurrence -> flag to state whether the function is recurring or not.
 *    outTid - Event id of timer event, out handle for API
 *  Return:
 *    success : 0 , failure : nonzero 
 ************************************************************************/
int timerRegisterEvents ( time_t eventTime, eventTimeType type, int (*job_func)(), void *job_data,
                          int recurrence, int recurringInterval, int *outTid )
{
  int retVal = 1;
  int found = 0;
  time_t currentTime;
   
  ListNode *tempNode = NULL;
  TimerEventAttrs *temp = NULL;
  TimerEventAttrs *newEvent = NULL;

  if( job_func == NULL )  
    return ERROR_INVALID_JOB_FUNC; 
  if( recurrence > 1 )  
    return ERROR_INVALID_PERIODICITY;   
  if( pgTimerThread == NULL )
  	return ERROR_INVALID_TIMER_THREAD;

  timerCalculateTime( &eventTime, type );
  currentTime = time(NULL);
  if(eventTime < currentTime)
    return ERROR_INVALID_EVENT_TIME;
  pthread_mutex_lock( &pgTimerThread->mutex );

  newEvent = timerCreateEvent( pgTimerThread, job_func, job_data, recurrence, recurringInterval, eventTime,
                               pgTimerThread->lastRegisteredEventId );
	
  if( newEvent == NULL ) {
    pthread_mutex_unlock( &pgTimerThread->mutex );
    return retVal;
  }

  tempNode = ListHead( &pgTimerThread->timerEventQueue );
 
  while( tempNode != NULL ) { 
    temp = ( TimerEventAttrs * ) tempNode->item;
    if( temp->eventTime >= eventTime ){
      if( ListAddBefore( &pgTimerThread->timerEventQueue, newEvent, tempNode ) != NULL ){
    		retVal = 0;}
        found = 1;
        break;
      }
    tempNode = ListNext( &pgTimerThread->timerEventQueue, tempNode );
  }

  if( !found ) {

    if( ListAddTail( &pgTimerThread->timerEventQueue, newEvent ) != NULL ){
      retVal = 0;
    } 
  }

  if( retVal == 0 ) {

    pthread_cond_signal( &pgTimerThread->conVar );
  	*outTid = pgTimerThread->lastRegisteredEventId++; 
  	APP_LOG("PluginTimer", LOG_DEBUG, " Timer Event Registered with id %d ", *outTid );
    pthread_mutex_unlock( &pgTimerThread->mutex );
  } else {
    timerDeleteEvent( pgTimerThread, newEvent );
  }
  
  return retVal;
}
/************************************************************************
 *  Function: timerRemoveEvent
 * 
 *  Deletes a timer  event from the timer linked list.
 *
 *  Input Parameters:
 *    id - id of event to remove.
 *
 *  Return:
 *    0 on success, nonZero on failure
 ************************************************************************/
int timerRemoveEvent( int id )
{
  int retVal = ERROR_INVALID_REMOVE_EVENT_ID;
  ListNode *tempNode = NULL;
  TimerEventAttrs *temp = NULL;

  if (pgTimerThread == NULL )
	return ERROR_INVALID_TIMER_THREAD;

  pthread_mutex_lock( &pgTimerThread->mutex );

  tempNode = ListHead( &pgTimerThread->timerEventQueue );
  while( tempNode != NULL ) {
    temp = ( TimerEventAttrs * ) tempNode->item;
    if( temp->id == id )
    {
      ListDelNode( &pgTimerThread->timerEventQueue, tempNode, 0 );
      timerDeleteEvent( pgTimerThread, temp );
			if (temp) {
				free(temp);
				temp = NULL;
			}
      retVal = 0;
	  break;
    }
    tempNode = ListNext( &pgTimerThread->timerEventQueue, tempNode );
  }
	
  if( retVal == ERROR_INVALID_REMOVE_EVENT_ID ){
    APP_LOG("PluginTimer", LOG_ERR, " Invalid Timer event id for deletion ");
  }else{        
    APP_LOG("PluginTimer", LOG_DEBUG, " Timer Event Removed ");
  }
  pthread_mutex_unlock( &pgTimerThread->mutex );
  return retVal;
}
/************************************************************************
 * Function: timerDeleteModule
 * 
 *  Description:
 *    Deletes the timer thread and the related members.
 *    Events scheduled in the future will NOT be run.
 *    
 *  Returns:
 *    returns 0 if succesfull,
 *    nonzero otherwise.
  ***********************************************************************/
int timerDeleteModule()
{
  ListNode *tempNode2 = NULL;
  ListNode *tempNode = NULL;

  if( pgTimerThread == NULL ) {
    return ERROR_INVALID_TIMER_THREAD;
  }

  pthread_mutex_lock( &pgTimerThread->mutex );

  pgTimerThread->cleanUp = 1;
  tempNode = ListHead( &pgTimerThread->timerEventQueue );

  while( tempNode != NULL ) {
    TimerEventAttrs *temp = ( TimerEventAttrs * ) tempNode->item;
    tempNode2 = ListNext( &pgTimerThread->timerEventQueue, tempNode );
    ListDelNode( &pgTimerThread->timerEventQueue, tempNode, 0 );
    timerDeleteEvent( pgTimerThread, temp );
		if (temp) {
			free(temp);
			temp = NULL;
		}
    tempNode = tempNode2;
  }

  ListDestroy( &pgTimerThread->timerEventQueue, 0 );
	FreeListDestroy( &pgTimerThread->timerFreeEvents );
  
  pthread_cond_broadcast( &pgTimerThread->conVar );

  while( pgTimerThread->cleanUp )  
  {
    pthread_cond_wait( &pgTimerThread->conVar, &pgTimerThread->mutex );
  }

  pthread_mutex_unlock( &pgTimerThread->mutex );

  while( pthread_cond_destroy( &pgTimerThread->conVar ) != 0 ) {
  }

  while( pthread_mutex_destroy( &pgTimerThread->mutex ) != 0 ) {
  }
  pgTimerThread = NULL;
  free(pgTimerThread); 
	
  return 0;
}

/****************************************************************************
 * Function: listMoveBefore
 *
 *    Moves a node to the tail of the list.
 *  Parameters:
 *    list  - Valid ptr to link list.
 *    insNode - node to add before
 *  Returns:
 *    0: success, NULL: failure.
 *****************************************************************************/
ListNode *ListMoveBefore( LinkedList * list, ListNode *insNode, ListNode * anode )
{
    ListNode *temp1, *temp2;
  if( ( list == NULL ) || ( anode == NULL ) || ( insNode == NULL) )
    return NULL;

  if ( insNode->next == anode )
    return insNode;

  if (list->size >= 2 ){
    
    temp1 = insNode->next;
    list->head.next = temp1;
    temp1->prev = &list->head;

    temp2 = anode->prev;
    insNode->prev = temp2; 
    anode->prev = insNode;
    insNode->next = anode;
    temp2->next = insNode;
      
  }  
  return insNode;
}
/****************************************************************************
 * Function: listMoveTail
 *
 *    Moves a node to the tail of the list.
 *  Parameters:
 *    list  - Valid ptr to link list.
 *    node - node to add before
 *  Returns:
 *    0: success, NULL: failure.
 *****************************************************************************/
ListNode *ListMoveTail( LinkedList * list, ListNode *node )
{
  if( list == NULL )
    return NULL;

  return ListMoveBefore( list, node, &list->tail );
}

int timerRunJob(int (*f)(void*), void *data) {
	int retVal = PLUGIN_SUCCESS;
	ThreadPoolJob job;
	if (f) {
		TPJobInit(&job, (start_routine)f, data);
		TPJobSetFreeFunction(&job, NULL);
		TPJobSetPriority(&job, P_THREAD_HIGH_PRIORITY);
		ThreadPoolAdd(&gltimerThreadPool, &job, NULL);
	}else return PLUGIN_FAILURE;
	return retVal;
}

#ifdef __DTIMER__// Timer list for debugging
void displayList ( )
{ 
  ListNode *tempNode = NULL;
  TimerEventAttrs *temp = NULL;

  pthread_mutex_lock( &pgTimerThread->mutex );
  tempNode = ListHead( &pgTimerThread->timerEventQueue );
 
  while( tempNode != NULL ) { 
    temp = ( TimerEventAttrs * ) tempNode->item;
    tempNode = ListNext( &pgTimerThread->timerEventQueue, tempNode );
  }
  pthread_mutex_unlock( &pgTimerThread->mutex );
}
#endif

#endif  /* PRODUCT_WeMo_LEDLight */
