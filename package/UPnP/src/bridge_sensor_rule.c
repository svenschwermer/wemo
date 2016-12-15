/***************************************************************************
*
* Created by Belkin International, Software Engineering on Apr 24, 2014
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

#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <pthread.h>
#include <upnp.h>
#include <time.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "types.h"
#include "global.h"
#include "defines.h"
#include "ulog.h"
#include "mxml.h"
#include "utils.h"
#include "controlledevice.h"
#include "belkin_api.h"
#include "subdevice.h"
#include "logger.h"

#define  _BR_MODULE_

#include "bridge_sensor_rule.h"
//-----------------------------------------------------------------------------

#define REACTION_MAX_COUNT  DEVICE_MAX_COUNT
static const char*      scDelimiter         = ":";

typedef volatile enum
{
    BS_STOP, BS_RUN,

} BS_REACTOR_STATE;

typedef struct
{
    unsigned            Time;
    SD_DeviceID         DeviceID;
    SD_CapabilityID     CapabilityID;
    char                CapabilityValue[CAPABILITY_MAX_VALUE_LENGTH];

} BS_CommandData;

typedef BS_CommandData*     BS_Command;
typedef BS_Command*         BS_CommandList;

typedef struct
{
    bool                Configured;

    pthread_mutex_t     CtrLock;
    pthread_mutex_t     DatLock;
    pthread_cond_t      Condition;
    pthread_t           Thread;
    pthread_t*          Started;
    BS_REACTOR_STATE    State;

    BS_CommandList      CommandList;

} BS_ReactionControl;

static BS_ReactionControl sReactor;

static SD_CapabilityID  sCapabilityOnOff    = {SD_CAPABILITY_ID, "10006"};
static SD_CapabilityID  sCapabilityLevel    = {SD_CAPABILITY_ID, "10008"};
static const char*      scValueOn           = "1";
static const char*      scValueOff          = "0";

static const char*      scSuccess           = "Success";
static const char*      scFail              = "Fail";

//-----------------------------------------------------------------------------
static BS_Command bs_CheckCommand(unsigned* wait)
{
    BS_Command      process = 0;
    BS_Command      command;
    unsigned        interval = 24*60*60;
    unsigned        offset;
    unsigned        current = time(0);
    int             i;

    for (i = 0; i < REACTION_MAX_COUNT; i++)
    {
        command = sReactor.CommandList[i];

        if (!command->Time)
            continue;

        if (current + 1 < command->Time)
        {
            offset   = command->Time - current;
            interval = MIN(interval, offset);
        }
        else
        {
            process  = command;
            interval = 0;
            break;
        }
    }

    *wait = interval;

    return process;
}
//-----------------------------------------------------------------------------
static void bs_ProcessCommand(BS_Command command)
{
    APP_LOG("UPNP",LOG_DEBUG,"ProcessCommand() %s\n", command->DeviceID.Data);

    SD_SetCapabilityValue(&command->DeviceID, &command->CapabilityID, command->CapabilityValue, SE_LOCAL + SE_REMOTE);

    command->Time = 0;
}
//-----------------------------------------------------------------------------
static void* bs_ReactorThread(void* arg)
{
    BS_Command          command;
    unsigned            wait;
    struct timespec     next = {0,0};

    while (sReactor.State == BS_RUN)
    {
        pthread_mutex_lock(&sReactor.DatLock);

        command = bs_CheckCommand(&wait);

        if (command)
            bs_ProcessCommand(command);

        pthread_mutex_unlock(&sReactor.DatLock);

        if (command)
            continue;

        pthread_mutex_lock(&sReactor.DatLock);

        next.tv_sec = time(0) + wait;
        pthread_cond_timedwait(&sReactor.Condition, &sReactor.DatLock, &next);

        pthread_mutex_unlock(&sReactor.DatLock);
    }

    return NULL;
}
//-----------------------------------------------------------------------------
static bool bs_CreateReactorThread()
{
    if (sReactor.Started)
        return true;

    sReactor.State = BS_RUN;

    if (pthread_create(&sReactor.Thread, NULL, &bs_ReactorThread, NULL) != 0)
    {
        APP_LOG("UPNP",LOG_DEBUG,"CreateReactorThread() failed \n");
        return false;
    }

    sReactor.Started = &sReactor.Thread;
    return true;
}
//-----------------------------------------------------------------------------
static void bs_DestroyReactorThread()
{
    if (sReactor.Started)
    {
        sReactor.State = BS_STOP;

        pthread_mutex_lock(&sReactor.DatLock);
        pthread_cond_signal(&sReactor.Condition);
        pthread_mutex_unlock(&sReactor.DatLock);

        pthread_join(sReactor.Thread, NULL);

        sReactor.Started = 0;
    }
}
//-----------------------------------------------------------------------------
void bs_ReserveCommand(unsigned when, const SD_DeviceID* device_id, const SD_CapabilityID* capability_id, const char* value)
{
    int             empty = -1;
    int             index = -1;
    BS_Command      data;
    int             i;

    pthread_mutex_lock(&sReactor.DatLock);

    for (i = 0; i < REACTION_MAX_COUNT; i++)
    {
        data = sReactor.CommandList[i];

        if (!data->Time)
        {
            if (empty < 0)
                empty = i;
        }
        else if (strncasecmp(data->DeviceID.Data, device_id->Data, sizeof(device_id->Data)-1) == 0)
        {
            index = i;
            break;
        }
    }

    if ((index < 0) && (0 <= empty))
         index = empty;

    if (0 <= index)
    {
        // ZZZ("ReserveCommand() [%d] %u, %s, %s, %s\n", index, when, device_id->Data, capability_id->Data, value);

        data = sReactor.CommandList[index];

        data->Time         =  when;
        data->DeviceID     = *device_id;
        data->CapabilityID = *capability_id;
        strncpy(data->CapabilityValue, value, sizeof(data->CapabilityValue));
    }

    pthread_cond_signal(&sReactor.Condition);
    pthread_mutex_unlock(&sReactor.DatLock);
}
//-----------------------------------------------------------------------------
bool CheckBridgeUDN(const char* udn, bool* sub_device)
{
    *sub_device = (strcmp(udn, g_szUDN) != 0);

    return (strstr(udn, g_szUDN) != 0);
}
//-----------------------------------------------------------------------------
bool GetSubDeviceID(const char* udn, SD_DeviceID* id, bool check_group)
{
    char            buffer[128];
    char*           node;
    SD_Device       sd;
    SD_Group        group;

    strncpy(buffer, udn, sizeof(buffer)-1);

    node = strtok(buffer, scDelimiter);
    node = strtok(NULL,  scDelimiter);
    node = strtok(NULL,  scDelimiter);

    if (!node)
        return false;

    id->Type = SD_DEVICE_EUID;
    strncpy(id->Data, node, sizeof(id->Data)-1);

    if (SD_GetDevice(id, &sd))
        return true;

    if (check_group)
    {
        id->Type = SD_GROUP_ID;

        return SD_GetGroup(id, &group);
    }

    return false;
}
//-----------------------------------------------------------------------------
int bs_SetReaction(char* udn, char* start, char* duration, char* end_action, const char** error_string)
{
    int             error = UPNP_E_SUCCESS;
    SD_DeviceList   device_list;
    SD_Device       device;
    SD_DeviceID     sd_id;
    bool            sd;
    unsigned        current;
    int             level, offset, count, d;
    const char*     on_off;
    char            value[CAPABILITY_MAX_VALUE_LENGTH] = {0,};
    int             once;

    for (once=1; once; once--)
    {
        if (!udn || !strlen(udn))
             udn = g_szUDN;
        if (!CheckBridgeUDN(udn, &sd))
        {
            error = UPNP_E_INVALID_ARGUMENT;
           *error_string = "Invalid Device ID";
            break;
        }
        current = time(0);

        sscanf(start, "%d", &level);
        sscanf(duration, "%d", &offset);

        if (level)
            on_off = scValueOn;
        else
            on_off = scValueOff;

        if (level == 1)
            level = 255;

        sprintf(value, "%d:0", level);

        if (sd)
        {
            if (!GetSubDeviceID(udn, &sd_id, true))
            {
                error = UPNP_E_INVALID_ARGUMENT;
               *error_string = "Device Not Found";
                break;
            }

            SD_SetCapabilityValue(&sd_id, &sCapabilityOnOff, on_off,   SE_LOCAL + SE_REMOTE);

            usleep(100*1000);

            if (level)
                SD_SetCapabilityValue(&sd_id, &sCapabilityLevel, value, SE_LOCAL + SE_REMOTE);

            if (offset)
                bs_ReserveCommand(current + offset, &sd_id, &sCapabilityOnOff, end_action);
        }
        else // Bridge
        {
            count = SD_GetDeviceList(&device_list);

            APP_LOG("UPNP",LOG_DEBUG,"Bridge Devices %d\n", count);

            for (d = 0; d < count; d++)
            {
                device = device_list[d];

                SD_SetDeviceCapabilityValue(&device->EUID, &sCapabilityOnOff, on_off,   SE_LOCAL + SE_REMOTE);
            }

            usleep(100*1000);

            for (d = 0; d < count; d++)
            {
                device = device_list[d];

                if (level)
                    SD_SetDeviceCapabilityValue(&device->EUID, &sCapabilityLevel, value, SE_LOCAL + SE_REMOTE);

                if (offset)
                    bs_ReserveCommand(current + offset, &sd_id, &sCapabilityOnOff, end_action);
            }
        }
    }

    return error;
}
//-----------------------------------------------------------------------------
bool BS_LoadReactor()
{
    char*       error = "";
    int         once;
    int         i = 0;

    if (sReactor.Configured)
        return true;

    pthread_mutex_init(&sReactor.CtrLock, NULL);
    pthread_mutex_init(&sReactor.DatLock, NULL);
    pthread_cond_init(&sReactor.Condition, NULL);

    for (once=1; once; once--)
    {
        sReactor.CommandList = (BS_CommandList)calloc(REACTION_MAX_COUNT, sizeof(BS_CommandList));

        if (sReactor.CommandList)
        {
            for (i = 0; i < REACTION_MAX_COUNT; i++)
            {
                sReactor.CommandList[i] = (BS_CommandData*)calloc(REACTION_MAX_COUNT, sizeof(BS_CommandData));

                if (!sReactor.CommandList[i])
                    break;
            }
        }

        if (!sReactor.CommandList || (i < REACTION_MAX_COUNT))
        {
            error = "Out of Memory";
            break;
        }

        if (!bs_CreateReactorThread())
        {
            error = "failed";
            break;
        }

        sReactor.Configured = true;
    }

    if (!sReactor.Configured)
    {
        APP_LOG("UPNP",LOG_DEBUG,"LoadReactor() %s\n", error);
        return false;
    }

    return true;
}
//-----------------------------------------------------------------------------
void BS_FreeReactor()
{
    int     i;

    if (!sReactor.Configured)
        return;

    pthread_mutex_lock(&sReactor.CtrLock);

    bs_DestroyReactorThread();

    if (sReactor.CommandList)
    {
        for (i = 0; i < REACTION_MAX_COUNT; i++)
        {
            if (sReactor.CommandList[i])
            {
                free(sReactor.CommandList[i]);
                sReactor.CommandList[i] = 0;
            }
        }

        free(sReactor.CommandList);
        sReactor.CommandList = 0;
    }

    sReactor.Configured = false;

    pthread_mutex_unlock(&sReactor.CtrLock);

    pthread_mutex_destroy(&sReactor.CtrLock);
    pthread_mutex_destroy(&sReactor.DatLock);
    pthread_cond_destroy(&sReactor.Condition);
}
//-----------------------------------------------------------------------------
int BS_SetBinaryState(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
    int         ret = UPNP_E_INTERNAL_ERROR;
    char*       udn;
    char*       start;
    char*       duration;
    char*       end_action;
    const char* result;
    int         once;

    pthread_mutex_lock(&sReactor.CtrLock);

    *out = 0;
    *errorString = 0;

    udn         = Util_GetFirstDocumentItem(request, "UDN");
    start       = Util_GetFirstDocumentItem(request, "BinaryState");
    duration    = Util_GetFirstDocumentItem(request, "Duration");
    end_action  = Util_GetFirstDocumentItem(request, "EndAction");

    APP_LOG("UPNP",LOG_DEBUG,"SetBinaryState() %s (%s,%s,%s)\n", udn, start, duration, end_action);

    for (once=1; once; once--)
    {
        if (!sReactor.Configured)
        {
            *errorString = "Internal Error";
            break;
        }

        if (!start || !duration || !end_action)
            break;

        ret = bs_SetReaction(udn, start, duration, end_action, errorString);
    }

    if (ret == UPNP_E_SUCCESS)
        result = scSuccess;
    else
        result = scFail;

    ret = UpnpAddToActionResponse(out, "SetBinaryState", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "BinaryState", result);

    if (*errorString)
        APP_LOG("UPNP",LOG_DEBUG,"SetBinaryState(): %s\n\n", *errorString);

    FreeXmlSource(udn);
    FreeXmlSource(start);
    FreeXmlSource(duration);
    FreeXmlSource(end_action);

    pthread_mutex_unlock(&sReactor.CtrLock);

    return UPNP_E_SUCCESS;
}
//-----------------------------------------------------------------------------
#endif

