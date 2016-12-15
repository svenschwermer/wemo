/***************************************************************************
*
* Created by Belkin International, Software Engineering on Mar 28, 2014
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

#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "defines.h"
#include "logger.h"

#include "belkin_api.h"

#define  _SD_MODULE_
#include "subdevice.h"
#include "sd_configuration.h"
#include "sd_event.h"
#include "sd_tracer.h"
#include "utility.h"

//-----------------------------------------------------------------------------

#define TRACE_MAX_COUNT DEVICE_MAX_COUNT

typedef volatile enum
{
    TS_STOP, TS_RUN,

} SM_TRACER_STATE;

typedef struct
{
    unsigned long       Time;
    SD_DeviceID         DeviceID;
    SD_CapabilityID     CapabilityID;
    char                Value[CAPABILITY_MAX_VALUE_LENGTH];
    unsigned            Event;

} SM_CapabilityUpdateData;

typedef SM_CapabilityUpdateData* SM_UpdateData;
typedef SM_UpdateData*           SM_UpdateList;

typedef struct
{
    bool                Configured;

    pthread_mutex_t*    SDLock;

    pthread_mutex_t     Lock;
    pthread_cond_t      Condition;
    pthread_t           TracerThread;
    pthread_t*          Tracer;
    SM_TRACER_STATE     State;
    SM_UpdateList       UpadeList;

} SM_TracerControl;

static SM_TracerControl sTracer;

static const char* sCapabilityOnOff         = "10006";
static const char* sCapabilitySleepFader    = "30008";
static const char* sCapabilitySensorIASZone = "10500";
static const char* sCapabilitySensorTest    = "30501";


//-----------------------------------------------------------------------------
static SM_UpdateData sm_CheckUpdate(unsigned* wait)
{
    SM_UpdateData   update = 0;
    SM_UpdateData   data;
    unsigned long   interval = 24*60*60;
    unsigned long   offset;
    unsigned long   current = GetUTCTime();
    int             i;

    for (i = 0; i < TRACE_MAX_COUNT; i++)
    {
        data = sTracer.UpadeList[i];

        if (!data->Time)
            continue;

        if (current + 1 < data->Time)
        {
            offset   = data->Time - current;
            interval = MIN(interval, offset);
        }
        else
        {
            update   = data;
            interval = 0;
            break;
        }
    }

    //ZZZ("Tracer Wait: %u \n", interval);

    *wait = interval;

    return update;
}
//-----------------------------------------------------------------------------
static void sm_Update(SM_UpdateData update)
{
    SD_Device           device;
    SD_CapabilityID     capability_id = {SD_CAPABILITY_ID};

    int  cacheValueInt = 0;
    char cacheValueStr[CAPABILITY_MAX_VALUE_LENGTH] = {0,};
    Debug("Inside sm_Update....dp\n");
    //ZZZ("%s\n", __func__);
    if(strncmp(update->CapabilityID.Data, sCapabilitySleepFader, strlen(sCapabilitySleepFader)) == 0) 
    {
        Debug("Sleep Fader Finished Notification, device ID:%s, Type:%d\n", update->DeviceID.Data, update->DeviceID.Type);

        if(strncmp(update->Value, "0:0", 3) == 0)
        {
            strcpy(capability_id.Data, sCapabilityOnOff);
            
            pthread_mutex_lock(sTracer.SDLock);
            SM_UpdateCapabilityValue(true, &update->DeviceID, &capability_id, "0", update->Event);
            pthread_mutex_unlock(sTracer.SDLock);
            
            SD_ClearSleepFaderCache(&update->DeviceID);
            update->Time = 0;
        }
    }
    else if(strncmp(update->CapabilityID.Data, sCapabilitySensorTest, strlen(sCapabilitySensorTest)) == 0)
    {
        Debug("Sensor test mode has been terminated, device ID:%s\n", update->DeviceID.Data);

        pthread_mutex_lock(sTracer.SDLock);
        SM_UpdateSensorCapabilityValue(true, &update->DeviceID, &update->CapabilityID, "0", update->Event);
        pthread_mutex_unlock(sTracer.SDLock);

        update->Time = 0;
    }
    else if(strncmp(update->CapabilityID.Data, sCapabilitySensorIASZone, strlen(sCapabilitySensorIASZone)) == 0)
    {
        Debug("IASZone alert sending: %s\n", update->DeviceID.Data);
        update->Time = 0;

        if(SM_GetDeviceAvailable(&update->DeviceID) == SD_ALIVE)
        {
            //Get 10500 capability
            if(SD_GetDeviceCapabilityValue(&update->DeviceID, &update->CapabilityID, cacheValueStr, 0, SD_CACHE))
            {
                sscanf(cacheValueStr, "%x", &cacheValueInt);
            }

            // Sensor is still in it's low battery status
            if((cacheValueInt & IASZONE_BIT_LOW_BATTERY))
            {
                // Send Low battery notification 
                SM_SendEvent(true, &update->DeviceID, &update->CapabilityID, update->Value, SE_LOCAL + SE_REMOTE);

                // Reserve again
                update->Time = GetUTCTime() + SENSOR_LOW_BATTERY_ALERT_INTERVAL;
            }

            // If still alarm is ON, then reserve notification again.  
            // Alarm Sensor?
            if( SD_GetDevice(&update->DeviceID, &device) )
            {
                if(0 == strncmp(device->ModelCode, SENSOR_MODELCODE_ALARM, strlen(SENSOR_MODELCODE_ALARM)))
                {
                    if((cacheValueInt & IASZONE_BIT_MAIN_EVENT))
                    {
                         Debug("Alarm still ON, Send Alarm alert notification....dp\n");
                        // Send Alarm alert notification 
                        SM_SendEvent(true, &update->DeviceID, &update->CapabilityID, update->Value, SE_LOCAL + SE_REMOTE);

                        // Reserve again
                        update->Time = GetUTCTime() + SENSOR_ALARM_ALERT_INTERVAL;
                    }
                }
            }

        }
    }
}

//-----------------------------------------------------------------------------
static void* sm_TracerThread(void* arg)
{
    SM_UpdateData       update;
    unsigned            wait;
    struct timespec     next = {0,0};

    while (sTracer.State == TS_RUN)
    {
        Debug("Inside sm_TracerThread....\n");
        //pthread_mutex_lock(sTracer.SDLock);
        update = sm_CheckUpdate(&wait);
        Debug("Inside sm_TracerThread....wait time %u\n", wait);
        if (update)
            sm_Update(update);

        //pthread_mutex_unlock(sTracer.SDLock);

        if (update)
            continue;

        pthread_mutex_lock(&sTracer.Lock);

        next.tv_sec = time(0) + wait;
        pthread_cond_timedwait(&sTracer.Condition, &sTracer.Lock, &next);

        pthread_mutex_unlock(&sTracer.Lock);
    }

    return NULL;
}
//-----------------------------------------------------------------------------
static bool sm_CreateTracerTask()
{
    if (sTracer.Tracer)
        return true;

    sTracer.State = TS_RUN;

    if (pthread_create(&sTracer.TracerThread, NULL, &sm_TracerThread, NULL) != 0)
    {
        Debug("CreateTracerTask() failed \n");
        return false;
    }

    sTracer.Tracer = &sTracer.TracerThread;
    return true;
}
//-----------------------------------------------------------------------------
static void sm_DestroyTracerTask()
{
    if (sTracer.Tracer)
    {
        sTracer.State = TS_STOP;

        sleep(1);

        pthread_mutex_lock(&sTracer.Lock);
        pthread_cond_signal(&sTracer.Condition);
        pthread_mutex_unlock(&sTracer.Lock);

        pthread_join(sTracer.TracerThread, NULL);

        sTracer.Tracer = 0;
    }
}
//-----------------------------------------------------------------------------
bool SM_LoadTracer(pthread_mutex_t* sd_lock)
{
    char*       error = "";
    int         once = 0;
    int         i = 0;

    if (sTracer.Configured)
        return true;

    pthread_mutex_init(&sTracer.Lock, NULL);
    pthread_cond_init(&sTracer.Condition, NULL);

    for (once=1; once; once--)
    {
        sTracer.UpadeList = (SM_UpdateList)calloc(TRACE_MAX_COUNT, sizeof(SM_UpdateData));

        if (sTracer.UpadeList)
        {
            for (i = 0; i < TRACE_MAX_COUNT; i++)
            {
                sTracer.UpadeList[i] = (SM_UpdateData)calloc(TRACE_MAX_COUNT, sizeof(SM_CapabilityUpdateData));

                if (!sTracer.UpadeList[i])
                    break;
            }
        }

        if (!sTracer.UpadeList || (i < TRACE_MAX_COUNT))
        {
            error = "Out of Memory";
            break;
        }

        //[WEMO-SIGSEGV] - should init before using pthread_mutex_lock
        sTracer.SDLock = sd_lock;

        if (!sm_CreateTracerTask())
        {
            error = "failed";
            break;
        }

        sTracer.Configured = true;
    }

    if (!sTracer.Configured)
    {
        Debug("LoadTracer() %s\n", error);
        return false;
    }

    return true;
}
//-----------------------------------------------------------------------------
void SM_FreeTracer()
{
    int     i;

    if (!sTracer.Configured)
        return;

    sm_DestroyTracerTask();

    if (sTracer.UpadeList)
    {
        for (i = 0; i < TRACE_MAX_COUNT; i++)
        {
            if (sTracer.UpadeList[i])
            {
                free(sTracer.UpadeList[i]);
                sTracer.UpadeList[i] = 0;
            }
        }

        free(sTracer.UpadeList);
        sTracer.UpadeList = 0;
    }

    sTracer.Configured = false;

    pthread_cond_destroy(&sTracer.Condition);
    pthread_mutex_destroy(&sTracer.Lock);
}
//-----------------------------------------------------------------------------
void SM_ReserveCapabilityUpdate(unsigned long update_time, const SD_DeviceID* device_id, const SD_CapabilityID* capability_id, const char* value, unsigned event)
{
    int             empty = -1;
    int             index = -1;
    SM_UpdateData   data;
    int             i;

    for (i = 0; i < TRACE_MAX_COUNT; i++)
    {
        data = sTracer.UpadeList[i];

        if (!data->Time)
        {
            if (empty < 0)
                empty = i;
        }
        else if( (strncasecmp(data->DeviceID.Data, device_id->Data, sizeof(device_id->Data)-1) == 0) &&
                 (strncasecmp(data->CapabilityID.Data, capability_id->Data, sizeof(capability_id->Data)-1) == 0) )
        {
            index = i;
            break;
        }
    }

    if ((index < 0) && (0 <= empty))
         index = empty;

    if (0 <= index)
    {
        data = sTracer.UpadeList[index];
        Debug("%s index:%d, device:%s, Cap:%s %s, at:%d \n",__func__, index, device_id->Data, capability_id->Data, value, update_time);

        SAFE_STRCPY(data->DeviceID.Data, device_id->Data);
        data->DeviceID.Type = device_id->Type;

        SAFE_STRCPY(data->CapabilityID.Data, capability_id->Data);
        data->CapabilityID.Type = capability_id->Type;

        strncpy(data->Value, value, sizeof(data->Value));

        data->Event = event;
        data->Time = update_time;
    }

    pthread_mutex_lock(&sTracer.Lock);
    pthread_cond_signal(&sTracer.Condition);
    pthread_mutex_unlock(&sTracer.Lock);
}
//-----------------------------------------------------------------------------
void SM_CancelReservedCapabilityUpdate(const SD_DeviceID* device_id)
{
    SM_UpdateData   data;
    int             i;

    if( (!device_id) || (!device_id->Data) ) {
        Debug("SM_CancelReservedCapabilityUpdate Error - null device id \n");
        return;
    }

    for (i = 0; i < TRACE_MAX_COUNT; i++)
    {
        data = sTracer.UpadeList[i];

        if (!data->Time)
            continue;

        if (strncasecmp(data->DeviceID.Data, device_id->Data, sizeof(device_id->Data)-1) == 0)
        {
            Debug("Notification canceled for %s \n", device_id->Data);
            data->Time = 0;
            break;
        }
    }
}
//-----------------------------------------------------------------------------
void SM_CancelReservedCapabilityUpdateById(char *idArg)
{
    SM_UpdateData   data;
    int             i;

    if(!idArg) {
        Debug("SM_CancelReservedCapabilityUpdateById Error - null device id \n");
        return;
    }

    for (i = 0; i < TRACE_MAX_COUNT; i++)
    {
        data = sTracer.UpadeList[i];

        if (!data->Time)
            continue;

        if (strcasecmp(data->DeviceID.Data, idArg) == 0)
        {
            Debug("Sleep Fader Notification canceled for %s \n", idArg);
            data->Time = 0;
            break;
        }
    }
}
//-----------------------------------------------------------------------------

