/***************************************************************************
*
*
* timer.h
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
#ifndef __TIMER_H
#define __TIMER_H

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "logger.h"

#ifdef PRODUCT_WeMo_LEDLight

int create_timer(char *name, timer_t *timer_id, void (*handler)(int, siginfo_t*, void*),
                int expire_sec, int interval_sec);

void start_timer(timer_t timer_id, int expire_sec, int interval_sec);

void stop_timer(timer_t timer_id);

int destroy_timer(timer_t timer_id);

#else /* End of PRODUCT_WeMo_LEDLight */

// Recurring time interval after which events execute currently predefined
#define PERIODIC_TIME 2

// Function return error codes
#define ERROR_EVENT_TIME              -1
#define ERROR_INVALID_JOB_FUNC        -2
#define ERROR_INVALID_EVENT_TIME      -3
#define ERROR_INVALID_PERIODICITY     -4
#define ERROR_INVALID_TIMER_THREAD    -5
#define ERROR_INVALID_REMOVE_EVENT_ID -6

typedef enum timeType {ABSOLUTE,RELATIVE} eventTimeType;

/****************************************************************************
 * Name: TimerThreadAttrs
 *
 *   Description:
 *     The timer thread is responsible for scheduling timer events
 *     and will continously poll for the expiry of a timer event.
 *
 *****************************************************************************/
typedef struct TIMERTHREADATTRS
{
  pthread_mutex_t mutex;
  pthread_cond_t conVar;
  int lastRegisteredEventId;
  LinkedList timerEventQueue;
	FreeList timerFreeEvents;
  int cleanUp;
}TimerThreadAttrs;

/****************************************************************************
 * Name: RecurrenceAttrs
 *
 *   Description:
 *     Describes the attributes of an event if its periodic <recurring > event
 *
 *****************************************************************************/
typedef struct RecurrenceATTRS
{
  int periodic;
  int periodicTime;
}RecurrenceAttrs;

/****************************************************************************
 * Name: TimerEventAttrs
 *
 *   Description:
 *
 *     Struct to contain information for a timer event.
 *     Internal to the TimerThread
 *
 *****************************************************************************/
typedef struct TIMEREVENTATTRS
{
  int (*job_func)();
  void *job_data;
  time_t eventTime; /* absolute time for event in seconds since Jan 1, 1970 */
  int recurrence;  /* perodic/non-periodic event */
  int recurringInterval;  /* perodic/non-periodic event Interval */
  int id;
} TimerEventAttrs;


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
int timerInitModule();


/************************************************************************
 *  Function: timerRegisterEvents
 *
 *  Desc.:
 *   Registers and schedules an event to run at a specified time.
 *
 *  Input Parameters:
 *       time_t - time of event in absolute  or relative seconds.
 *       eventTimeType – event time in either absolute, or relative terms.
 *                       for relative events, time considered is
 *                       current system time + relative time.
 *       job_func -> valid function pointer
 *       job_data -> argument to function
 *       recurrence -> flag to state whether the function is recurring or not.
 *       outTid - Event id of timer event, out handle for API
 *  Return:
 *      success : 0 , failure : nonzero
 ************************************************************************/
int timerRegisterEvents ( time_t eventTime, eventTimeType type, int (*job_func)(), void *job_data,
                          int recurrence, int recurringInterval, int *outTid );
/****************************************************************************
 * Function: timerSchedulerThread
 *
 *    Implements timer thread.
 *    Waits for next event to occur and schedules
 *    associated job_func into threadpool.
 *    Internal Only.
 *  Parameters:
 *    None
 *  Returns:
 *    None
 *****************************************************************************/
void *timerSchedulerThread ();
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
int timerRemoveEvent( int id  );
/****************************************************************************
 * Function: timerCreateEvent
 *
 *  Desc.:
 *    Adds a Timer Event to the link list.
 *  Input Parameters:
 *    timer – Pointer to TimerThreadAttrs structure.
 *    func - thread function to run.
 *    arg - argument to function.
 *    priority - priority of job.
 *    eventTime - the absoule time of the event
 *                in seconds from Jan, 1970
 *    id - id of job
 *
 *  Returns:
 *    EventData * on success, NULL on failure.
 ****************************************************************************/
#if 0
static TimerEventAttrs * timerCreateEvent( TimerThreadAttrs * pgTimerThread, int (*job_func)(), void *job_data,
                                           int recurrence, int recurringInterval, time_t eventTime, int id );
#endif
/************************************************************************
 * Function: timerDeleteModule
 *
 *  Description:
 *    Deletes the timer thread and the related members.
 *    Events scheduled in the future will NOT be run.
 *
 *  Returns:
 *    returns 0 if succesfull,
 *            nonzero otherwise.

 ***********************************************************************/
int timerDeleteModule();
int pluginUsleep (unsigned int delay);

#endif  /* EMS_TIMER_INFRASTRUCTURE_H */

#endif  /* PRODUCT_WeMo_LEDLight */
