/***************************************************************************
*
* Created by Belkin International, Software Engineering on Apr 17, 2014
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
#include "mxml.h"
#include "utils.h"
#include "plugin_ctrlpoint.h"
#include "controlledevice.h"
#include "belkin_api.h"
#include "subdevice.h"
#include "logger.h"
#define  _BR_MODULE_
//#include "bridge_rule.h"
#include "bridge_away_rule.h"
#include "bridge_sensor_rule.h"
//-----------------------------------------------------------------------------
#define BA_MAX_DEVICE           128
#define BA_MAX_TURN_ON_DEVICES  3
#define BA_MIN_SCAN_INTERVAL    15
#define BA_MAX_SCAN_INTERVAL    60
#define BA_DEVICE_RESPONSE_WAIT 10

#define BA_RANGE_MIN            (60*1)
#define BA_RANGE_FIRST          (60*(15-1))

#if 0
#define BA_RANGE_FROM           (60*30)
#define BA_RANGE_TO             (60*(180-30))
#endif

#define BA_RANGE_FROM           (60*20)
#define BA_RANGE_TO             (60*40)

typedef unsigned long long      UINT64;

typedef enum
{
    BT_START, 
	BT_ALL_OFF,
	BT_NORMAL

} BA_TOGGLE_MODE;

typedef volatile enum
{
    BA_STOP, BA_RUN,

} BA_STATE;

typedef volatile int    BA_FLAG;
//--------this structure need to be populated------------
typedef struct
{
    char                UDN[64];
    int                 Index;
    SD_DeviceID         SubDeviceID;

    bool                Running;
    int                 OnOff;
    unsigned            NextToggle;
    unsigned            AllOffToggle;

} BA_DeviceData;

typedef BA_DeviceData*  BA_Device;

typedef struct
{
    bool                Configured;

    pthread_mutex_t     CtrLock;
    pthread_mutex_t     DatLock;
    pthread_t           Thread;
    pthread_t*          Started;
    BA_STATE            State;
    unsigned            Time;

    unsigned            ManualToggle;
    BA_FLAG             Active;
    unsigned            Scale;

    bool                Linked;

    int                 OtherDevices;
    int                 OtherDevicesUpdated;
    int                 TurnedOnOtherDevices;
    unsigned            TurnedOnScannedTime;

    int                 RunningSubDevices;
    int                 TurnedOnSubDevices;

    int                 DeviceCount;
    BA_Device           DeviceList[BA_MAX_DEVICE];
    int                 UpdateCount;
    BA_Device           UpdateList[BA_MAX_DEVICE];

} BA_Control;

static BA_Control       sControl;

static const char*      scBridge            = "Bridge";

#if 1
#ifndef BR_LOCAL_TEST
    #define BR_SAVE_DIR  "/tmp/Belkin_settings/"
    #define BR_TEMP_DIR  "/tmp/"
#else
    #define BR_SAVE_DIR  "./"
    #define BR_TEMP_DIR  "./"
#endif
static const char*      scFileDeviceList    = BR_SAVE_DIR "ba_rule";
static const char*      scFileQAConfig      = BR_TEMP_DIR "qa_config";
#endif
static const char*      scQAScale           = "BA_Scale";

static const char*      scDelimiter         = ":";

static const char*      scSimulatedDevice   = "Device";
static const char*      scSimulatedUDN      = "UDN";
static const char*      scSimulatedIndex    = "index";

static const char*      scInvalidList       = "Invalid List";
static const char*      scOutOfMemory       = "Out of Memory";

static SD_CapabilityID  sCapabilityOnOff    = {SD_CAPABILITY_ID, "10006"};
static SD_CapabilityID  sCapabilityLevel    = {SD_CAPABILITY_ID, "10008"};
static const char*      scValueOn           = "1";
static const char*      scValueOff          = "0";
static const char*      scValueLevel        = "200:10";

static const char*      scResult            = "Result";
static const char*      scSuccess           = "Success";
static const char*      scFail              = "Fail";


#ifdef PRODUCT_WeMo_LEDLight
//-----------------------------------------------------------------------------
bool BR_CheckTimeIsToday(time_t chkTime)
{
    time_t      nowTime;  // current time
    struct tm*  nowtm;
    struct tm*  chktm;

    if(chkTime == 0)
        return false;

    nowTime = time(0);  // current time
    nowtm = localtime(&nowTime);

    chktm = localtime(&chkTime);

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "BR_CheckTimeIsToday chktm:%d, nowtm:%d ", (int)chkTime, (int)nowTime);

    // Did manual toggle happen today?
    if( (chktm->tm_year == nowtm->tm_year) &&
        (chktm->tm_mon  == nowtm->tm_mon ) &&
        (chktm->tm_mday == nowtm->tm_mday) ) {
        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "%s: today!", __func__);
        return true;
    }

    return false;
}
//-----------------------------------------------------------------------------
static bool br_GetTime(time_t ts, BR_Time* bt)
{
    struct tm*  lt;

	#if 0
    // We should compensate 1 day time gap for different time zone. 
    if ( (ts < (sControl.BuildDate - (24*3600)) ) || (ts == 0) ) {
        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "%s error! ", __func__);
        bt->Time = 0;
        bt->Yesterday = 0;
        bt->Today = 1;
        bt->Tomorrow = 2;
        return false;
    }
	#endif

    lt = localtime(&ts);

    bt->Today   = lt->tm_wday;
    bt->Time    = lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec;

    bt->Yesterday = bt->Today - 1;
    if (bt->Yesterday < 0)
        bt->Yesterday = 6;

    bt->Tomorrow = bt->Today + 1;
    if (6 < bt->Tomorrow)
        bt->Tomorrow = 0;

    return true;
}
//-----------------------------------------------------------------------------
bool BR_CheckManualToggleTimeAvailable(time_t mToggleTime, unsigned int ruleTime)
{
    bool        ret = false;
    BR_Time     mtgBrTime;
    BR_Time     nowBrTime;
    time_t      nowTime = time(0);  // current time

    // Did manual toggle happen today?
    if(BR_CheckTimeIsToday(mToggleTime) == true) {

        // Get manual toggle time: being converted as seconds passed from 00:00
        br_GetTime(mToggleTime, &mtgBrTime);
        br_GetTime(nowTime, &nowBrTime);

        // If manual toggle happened after rule was triggered...
        if( ( mtgBrTime.Time > ruleTime ) && (mtgBrTime.Time < nowBrTime.Time) ) {
            // Don't do trigger rule.
            APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "br_CheckManualToggleAvailable - There has been Manual toggling ! rule:%d, mtg:%d , now:%d ", ruleTime, mtgBrTime.Time, nowBrTime.Time);
            ret = true;
        }
    }
    return ret;
}
#endif

//-----------------------------------------------------------------------------
static void ba_UpdateSubDevices()
{
    int         running   = 0;
    int         turned_on = 0;
    BA_Device   device;
    int         d;

    for (d = 0; d < sControl.DeviceCount; d++)
    {
        device = sControl.DeviceList[d];

        if (device->Running)
        {
            running++;

            if (device->OnOff)
                turned_on++;
        }
    }

    sControl.RunningSubDevices  = running;
    sControl.TurnedOnSubDevices = turned_on;

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "sControl.TurnedOnSubDevices:%d sControl.RunningSubDevices:%d", turned_on,running);
}
//-----------------------------------------------------------------------------
static void bs_SelectOtherDevices()
{
    pCtrlPluginDeviceNode dev_node;
    BA_Device             device;

    int         checked = 0;
    int         search, d;
    bool        found;

    for (search = 0; search < 2; search++)
    {
    	APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "bs_SelectOtherDevices search =%d",search);
        CtrlPointDiscoverDevices();

        sleep(1);

        LockDeviceSync();

        dev_node = g_pGlobalPluginDeviceList;

        while (dev_node)
        {
            found = false;

            for (d = 0; d < sControl.DeviceCount; d++)
            {
                device = sControl.DeviceList[d];

                if (strcmp(dev_node->device.UDN, device->UDN) == 0)  //a simulated device
                {
                    found = true;
                    checked++;
                    break;
                }
            }

            if (!found)
                dev_node->Skip = true;

            dev_node = dev_node->next;
        }

        UnlockDeviceSync();

        if (sControl.OtherDevices <= checked)
            break;
    }
}
//-----------------------------------------------------------------------------
static void bs_UnselectOtherDevices()
{
    pCtrlPluginDeviceNode dev_node;

    LockDeviceSync();

    dev_node = g_pGlobalPluginDeviceList;

    while (dev_node)
    {
        dev_node->Skip = false;
        dev_node = dev_node->next;
    }

    UnlockDeviceSync();
}
//-----------------------------------------------------------------------------
static void ba_UpdateOtherDevices(unsigned interval)
{
    pCtrlPluginDeviceNode 	dev_node;
    BA_Device   			device;
    int         			turned_on = 0;
    int        		 		d;
    int         			wait;
    unsigned    			passed;

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "ba_UpdateOtherDevices() ");
    passed = sControl.Time - sControl.TurnedOnScannedTime;

    if (passed < interval)
		return;

    bs_SelectOtherDevices();

    for (d = 0; d < sControl.DeviceCount; d++)
    {
        device = sControl.DeviceList[d];

        if (device->SubDeviceID.Type)
            continue;

        device->OnOff = 0;
    }

    pthread_mutex_unlock(&sControl.DatLock);

    dev_node = g_pGlobalPluginDeviceList;

    while (dev_node) {
		if(!dev_node->Skip) {

			// Clear other device found counter.
    		sControl.OtherDevicesUpdated = 0;

    		APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "Sending uPnP request - GetSimulatedRuleData ");
			// Send UPnP Event "GetSimulatedRuleData"
			PluginCtrlPointGetSimulatedRuleData(dev_node->device.UDN);

			// Waiting for responding from other devices...
			for (wait = 0; wait < BA_DEVICE_RESPONSE_WAIT; wait++) {
				APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "Waiting for scanning other devices... %d ", wait);
				sleep(1);

				if (sControl.OtherDevicesUpdated > 0)
					break;
			}
		}
        dev_node = dev_node->next;
    }

    pthread_mutex_lock(&sControl.DatLock);

    bs_UnselectOtherDevices();

    for (d = 0; d < sControl.DeviceCount; d++)
    {
        device = sControl.DeviceList[d];

        if (device->SubDeviceID.Type)
            continue;

        if (device->OnOff)
            turned_on++;
    }

    sControl.TurnedOnOtherDevices = turned_on;
    sControl.TurnedOnScannedTime  = sControl.Time;

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "UpdateOtherDevices() %d, sControl.TurnedOnOtherDevices:%d", sControl.OtherDevices, turned_on);
}
//-----------------------------------------------------------------------------
#if 0
static void ba_LoadScale()
{
    FILE*       file = 0;
    int         scale = 0;
    char*       key;
    char*       value;
    int         once;
    char        buffer[256];

    for (once=1; once; once--)
    {
        file = fopen(scFileQAConfig, "r");

        if (!file)
            break;

        while (fgets(buffer, sizeof(buffer), file))
        {
            key = strtok(buffer, scDelimiter);

            if (!key)
                break;

            if (strcmp(scQAScale, key) == 0)
            {
                value = strtok(NULL, "\n");

                if (value)
                    sscanf(value, "%d", &scale);

                break;
            }
        }
    }

    if (file)
        fclose(file);

    pthread_mutex_lock(&sControl.DatLock);

    if ((0 < scale) && (scale <= 60*5))
        sControl.Scale = scale;
		sControl.Scale = 1;   // Dont need this scale  functions

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "Scale:%d", sControl.Scale);

    pthread_mutex_unlock(&sControl.DatLock);
}
#endif
//-----------------------------------------------------------------------------
static unsigned ba_GetNextToggle(int index, BA_TOGGLE_MODE mode)
{
    struct timeval  tv;
    unsigned        random;
    unsigned        next;
    UINT64          range;

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "ba_GetNextToggle");
    gettimeofday(&tv, NULL);
    srand(tv.tv_usec);
    random = rand();

    switch (mode)
    {
    case BT_START:
    case BT_ALL_OFF:
            range = BA_RANGE_FIRST;
            break;
    default:
            range = BA_RANGE_TO;
            break;
    }

    next = (range * random) / RAND_MAX;

    switch (mode)
    {
    case BT_START:
    case BT_ALL_OFF:
            next += BA_RANGE_MIN;
            break;
    default:
            next += BA_RANGE_FROM;
            break;
    }
    next /= sControl.Scale;
    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "Next Toggle [%d] + %u ", index, next);

    return next;
}
//-----------------------------------------------------------------------------
static void ba_SetSchedule(SD_DeviceID* sd_id, bool run, BA_TOGGLE_MODE mode)
{
    int             d;
    BA_Device       device;
    SD_Device       sd;
    SD_Group        sg;
    char            sd_value[CAPABILITY_MAX_VALUE_LENGTH];
    int             value = 0;
    unsigned        current;
    unsigned        next;

    if (!sd_id->Type)
        return;

    for (d = 0; d < sControl.DeviceCount; d++)
    {
        device = sControl.DeviceList[d];

        if ((device->SubDeviceID.Type == sd_id->Type) && (strcmp(device->SubDeviceID.Data, sd_id->Data) == 0))
            break;
    }
    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "SetSchedule(): Device type[%d:%s]", sd_id->Type, sd_id->Data);
    if ((sControl.DeviceCount <= d)
    ||  ((sd_id->Type == SD_DEVICE_EUID) && !SD_GetDevice(sd_id, &sd))
    ||  ((sd_id->Type == SD_GROUP_ID)    && !SD_GetGroup (sd_id, &sg)))
    {
        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "SetSchedule(): Device Not Found %d:%s", sd_id->Type, sd_id->Data);
        return;
    }

    if (!run)
    {
    	APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "(!run)");
        SD_SetCapabilityValue(sd_id, &sCapabilityOnOff, scValueOff, SE_LOCAL + SE_REMOTE);

        device->Running = false;
        device->OnOff   = 0;
        return;
    }

  	if (SD_GetCapabilityValue(sd_id, &sCapabilityOnOff, sd_value, SD_REAL))
  	{
  		sscanf(sd_value, "%d", &value);
    }
    device->OnOff = value;
    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "capability device->OnOff[ %d] device->Index [%d] mode[%d]",value,device->Index,mode);
    if ((mode == BT_START) && device->Index)
         mode =  BT_NORMAL; /* 2 */

    current = time(0);
    next    = current + ba_GetNextToggle(device->Index, mode);
    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "Mode --- : %d",mode);

    if (mode == BT_ALL_OFF)
        device->AllOffToggle = next;
    else
        device->NextToggle = next;

    device->Running = true;
    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "device->NextToggle:%d device->AllOffToggle:%u ",device->NextToggle,device->AllOffToggle);
}
//-----------------------------------------------------------------------------
static bool ba_ProcessToggle(BA_Device device)
{
    bool        toggle = true;
    bool        result = true;
    const char* on_off;
    char        value[CAPABILITY_MAX_VALUE_LENGTH];
    int         level;
    int Pon_off = -1;


    if (!device->OnOff)
    {
        ba_UpdateOtherDevices(BA_MIN_SCAN_INTERVAL);

        if (BA_MAX_TURN_ON_DEVICES <= (sControl.TurnedOnOtherDevices + sControl.TurnedOnSubDevices))
        {
            APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "Skipping Toggle (%d + %d)", sControl.TurnedOnOtherDevices, sControl.TurnedOnSubDevices);
            toggle = false;
        }
        on_off = scValueOn;
    }
    else
        on_off = scValueOff;
    sscanf(on_off , "%d", &Pon_off);

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "toggle :%d  on_off %d",toggle,Pon_off);

    if (toggle)
    {
    	APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "SD_SetCapabilityValue(&device->SubDeviceID, &sCapabilityOnOff, on_off, SE_LOCAL + SE_REMOTE);");
        result = SD_SetCapabilityValue(&device->SubDeviceID, &sCapabilityOnOff, on_off, SE_LOCAL + SE_REMOTE);

        if (result && (on_off == scValueOn))
        {
            result = false;

            if (SD_GetCapabilityValue(&device->SubDeviceID, &sCapabilityLevel, value, SD_REAL))
            {
                if (sscanf(value, "%d", &level))
                {
                    if (level < 16) {
                        result = SD_SetCapabilityValue(&device->SubDeviceID, &sCapabilityLevel, scValueLevel, SE_LOCAL + SE_REMOTE);
                        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "if scValueLevel = 200:10; %s",scValueLevel);
                    }
                    else {
                        result = SD_SetCapabilityValue(&device->SubDeviceID, &sCapabilityLevel, value, SE_LOCAL + SE_REMOTE);
                        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "else value %s",value);
                    }
                }
            }
        }
    }

    ba_SetSchedule(&device->SubDeviceID, true, BT_NORMAL);
    ba_UpdateSubDevices();

    return result;
}
//-----------------------------------------------------------------------------
static void ba_CheckAllOffState()
{
    BA_Device       device;
    int             d;

    //APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "ba_CheckAllOffState() ");
    if (!sControl.RunningSubDevices)
        return;

    if (!sControl.TurnedOnSubDevices)
        ba_UpdateOtherDevices(BA_MAX_SCAN_INTERVAL);

    if (sControl.TurnedOnOtherDevices + sControl.TurnedOnSubDevices)
    {
        for (d = 0; d < sControl.DeviceCount; d++)
        {
            device = sControl.DeviceList[d];

            if (!device->Running)
                continue;

            if (device->AllOffToggle)
                device->AllOffToggle = 0;
        }
    }
    else
    {
        for (d = 0; d < sControl.DeviceCount; d++)
        {
            device = sControl.DeviceList[d];

            if (!device->Running)
                continue;

            if (!device->AllOffToggle) {
            	APP_LOG("DEVICE:rule", LOG_DEBUG, "!!!!--AllOffToggle-!!!!");
                ba_SetSchedule(&device->SubDeviceID, true, BT_ALL_OFF);
            }
        }
    }
}
//-----------------------------------------------------------------------------
static void* ba_RuleThread(void* arg)
{
    BA_Device       device;
    unsigned        current;
    bool            toggle;
    int             d;

    APP_LOG("DEVICE:rule", LOG_DEBUG, "In ba_RuleThread");

    while (sControl.State == BA_RUN)
    {
        sleep(4);
        current = time(0);
        sControl.Time = current;

        if (!sControl.Active)
            continue;

        pthread_mutex_lock(&sControl.DatLock);

        //APP_LOG("DEVICE:rule", LOG_DEBUG, "*******************\n\n");
        for (d = 0; d < sControl.DeviceCount; d++)
        {
             device = sControl.DeviceList[d];

            if (!device->Running)
                continue;

            toggle = false;

            //APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "device->Index[%d] device->NextToggle[%u] current[%u] AllOffToggle[%u]", device->Index,device->NextToggle,current,device->AllOffToggle);
            if (sControl.TurnedOnOtherDevices + sControl.TurnedOnSubDevices)
            {
                if (device->NextToggle <= current) {
                    toggle = true;
                    APP_LOG("DEVICE:rule", LOG_DEBUG, "if : device->NextToggle <= current");
                }
            }
            else
            {
                if (device->AllOffToggle && (device->AllOffToggle <= current)) {
                    toggle = true;
                    APP_LOG("DEVICE:rule", LOG_DEBUG, "else : (device->AllOffToggle && (device->AllOffToggle <= current))");
                }
            }

            if (toggle)
            {
                APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "ProcessToggle()-- [%d]", device->Index);

                ba_ProcessToggle(device);
            }
        }

        ba_CheckAllOffState();

        pthread_mutex_unlock(&sControl.DatLock);
    }

    sControl.State = BA_STOP;

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "ba_RuleThread() Finished ");

    return NULL;
}
//-----------------------------------------------------------------------------
static bool ba_CreateRuleThread()
{
    if (sControl.Started)
        return true;

    sControl.State = BA_RUN;

#if 0
    sControl.Active = true; //added
    sControl.Linked = 1; // added
#endif

    if (pthread_create(&sControl.Thread, NULL, &ba_RuleThread, NULL) != 0)
    {
        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "ba_CreateRuleTask() failed ");
        return false;
    }
    APP_LOG("DEVICE:rule", LOG_DEBUG, "ba_CreateRuleThread()");

    sControl.Started = &sControl.Thread;
    return true;
}
//-----------------------------------------------------------------------------
static void ba_DestroyRuleThread()
{
    if (sControl.Started)
    {
        sControl.State = BA_STOP;

        pthread_join(sControl.Thread, NULL);
        APP_LOG("DEVICE:rule", LOG_DEBUG, "ba_DestroyRuleThread()");
        sControl.Started = 0;
    }
}
//-----------------------------------------------------------------------------
static void ba_DeleteDeviceData()
{
    int     d;
	if(sControl.DeviceCount)
	{
		APP_LOG("DEVICE:rule", LOG_DEBUG, "ba_DeleteDeviceData()-");
	}
		
    for (d = 0; d < sControl.DeviceCount; d++)
    {
        if (sControl.DeviceList[d])
        {
            free(sControl.DeviceList[d]);
            sControl.DeviceList[d] = 0;
        }
    }
	
    sControl.DeviceCount = 0;
}
//-----------------------------------------------------------------------------
static void ba_ApplyUpdatedData()
{
    int     others = 0;
    int     u;
#if 0
    //ba_LoadScale();
#endif

    pthread_mutex_lock(&sControl.DatLock);

    ba_DeleteDeviceData();

    for (u = 0; u < sControl.UpdateCount; u++)
    {
        sControl.DeviceList[u] = sControl.UpdateList[u];
        sControl.UpdateList[u] = 0;

        if (!sControl.DeviceList[u]->SubDeviceID.Type)
            others++;
    }

    sControl.DeviceCount = sControl.UpdateCount;
    sControl.UpdateCount = 0;

    sControl.OtherDevices = others;

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "sControl.OtherDevices = [%d]  sControl.DeviceCount = [%d]", others, sControl.DeviceCount);
    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "sControl.Active = [%d]  sControl.Linked = [%d]", sControl.Active, sControl.Linked);

#if 0
    if (sControl.Active && sControl.Linked)
    {
        for (u = 0; u < sControl.DeviceCount; u++)
        {
            ba_SetSchedule(&sControl.DeviceList[u]->SubDeviceID, true, BT_START);
        }
    }
#endif

    ba_UpdateSubDevices();

    pthread_mutex_unlock(&sControl.DatLock);
}
//-----------------------------------------------------------------------------
static void ba_CancelUpdateData()
{
    int     u;

    for (u = 0; u < sControl.UpdateCount; u++)
    {
        if (sControl.DeviceList[u])
        {
            free(sControl.DeviceList[u]);
            sControl.UpdateList[u] = 0;
        }
    }

    sControl.UpdateCount = 0;
}
//-----------------------------------------------------------------------------
static bool ba_UpdateDeviceData(const char* udn, const char* index, const char** error_text)
{
    bool            result = false;
    bool            found  = false;
    SD_DeviceID     sd_id  = {0};
    bool            sd;
    BA_Device       device;
    int             once, d;

    // updating data

    for (d = 0; d < sControl.UpdateCount; d++)
    {
        device = sControl.UpdateList[d];

        if (strcmp(device->UDN, udn) == 0)
        {
            APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "Duplicated ID: %s", udn);
            found = true;
            break;
        }
    }

    for (once=1; once; once--)
    {
        if (BA_MAX_DEVICE <= d)
        {
           *error_text = "BA_MAX_DEVICE";
            break;
        }

        // checking Sub Device
        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "    udn: %s", udn);
        if (strstr(udn, scBridge))
        {
            if (BA_CheckBridgeUDN(udn, &sd))
            {
                if (BA_GetSubDeviceID(udn, &sd_id, true))
                {
                    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "    SD: %s", sd_id.Data);
                }
                else
                {
                    *error_text = "Invalid Device ID";
                    break;
                }
            }
            else
            {
            	APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "   Other Link SD: %s", sd_id.Data);
            }

        }

        if (!found)
        {

            device = (BA_DeviceData*)calloc(1, sizeof(BA_DeviceData));

            if (!device)
            {
                *error_text = scOutOfMemory;
                break;
            }

            sControl.UpdateList[d] = device;
            sControl.UpdateCount++;
            APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "New Link allocate memory");
        }

        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "sControl.UpdateCount : %d", sControl.UpdateCount);
        strncpy(device->UDN, udn, sizeof(device->UDN)-1);

        if (sd_id.Type)
            device->SubDeviceID = sd_id;

        sscanf(index, "%d", &device->Index);

        result = true;
    }

    if (!result)
        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "UpdateDeviceData(): %s", *error_text);

    return result;
}
//-----------------------------------------------------------------------------
static bool ba_SaveDeviceListToStorage()
{
    char*           error = 0;
    FILE*           file  = 0;
    BA_Device       device;
    int             once, d;
    char            buffer[256];

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "SaveDeviceList()");

    pthread_mutex_lock(&sControl.DatLock);

    for (once=1; once; once--)
    {
        file = fopen(scFileDeviceList, "w");

        if (!file)
        {
            error = "Open Failed";
            break;
        }

        for (d = 0; d < sControl.DeviceCount; d++)
        {
            device = sControl.DeviceList[d];
            snprintf(buffer, sizeof(buffer), "%d|%s\n", device->Index, device->UDN);

            if (!fprintf(file, "%s", buffer))
            {
                error = "Write Failed";
                break;
            }
        }
    }

    if (file)
        fclose(file);

    pthread_mutex_unlock(&sControl.DatLock);

    if (error)
    {
        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "SaveDeviceList(): %s ", error);
        return false;
    }

    return true;
}
//-----------------------------------------------------------------------------
static int ba_UpdateDeviceList(const char* device_list, const char** error_text)
{
    int             error = 0;
    int             once;

    mxml_node_t     *tree, *device, *node_udn, *node_index;
    const char      *udn,  *index;

    if (!device_list || !strlen(device_list))
    {
        *error_text = scInvalidList;
        return UPNP_E_INVALID_ARGUMENT;
    }

    tree = mxmlLoadString(NULL, device_list, MXML_OPAQUE_CALLBACK);

    if (!tree)
    {
        *error_text = scInvalidList;
        return UPNP_E_INVALID_ARGUMENT;
    }

    for (once=1; once; once--)
    {
        device = mxmlFindElement(tree, tree, scSimulatedDevice, NULL, NULL, MXML_DESCEND);

        if (!device)
        {
           *error_text = scInvalidList;
            error = UPNP_E_INVALID_ARGUMENT;
            break;
        }

        for( ;device; device = mxmlFindElement(device, tree, scSimulatedDevice, NULL, NULL, MXML_DESCEND))
        {
            node_udn    = mxmlFindElement(device, device, scSimulatedUDN, NULL, NULL, MXML_DESCEND);
            node_index  = mxmlFindElement(device, device, scSimulatedIndex,  NULL, NULL, MXML_DESCEND);

            if (!node_udn || !node_index)
            {
               *error_text = "Missing Device ID or Index";
                error = UPNP_E_INVALID_ARGUMENT;
                break;
            }

            udn   = mxmlGetOpaque(node_udn);
            index = mxmlGetOpaque(node_index);

            if (!udn || !index)
            {
               *error_text = scInvalidList;
                error = UPNP_E_INVALID_ARGUMENT;
                break;
            }

            APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "Device [%s] <%s> ", udn, index); // uuid:Bridge-1_0-231407B0100076:B4750E1B957801D1 , 1

            if (!ba_UpdateDeviceData(udn, index, error_text))
            {
                error = UPNP_E_INVALID_ARGUMENT;
                break;
            }
        }
    }

    mxmlDelete(tree);

    if (error)
    {
    	APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "error");
        ba_CancelUpdateData();
        return error;
    }

    ba_ApplyUpdatedData();
 
    if (!ba_SaveDeviceListToStorage())
        return UPNP_E_INTERNAL_ERROR;

   return UPNP_E_SUCCESS;
}
//-----------------------------------------------------------------------------
static bool ba_LoadDeviceListFromStorage()
{
    const char*     error = 0;
    FILE*           file  = 0;
    int             once;
    struct stat     s;
    char            buffer[256];
    char           *node, *index, *udn;

    if (stat(scFileDeviceList, &s) != 0)
        return false;

    for (once=1; once; once--)
    {
        file = fopen(scFileDeviceList, "r");

        if (!file)
        {
            error = "Open Failed";
            break;
        }

        while (fgets(buffer, sizeof(buffer), file))
        {
            node = strtok(buffer, "|");
            if (!node || !strlen(node))
                continue;

            index = node;

            node = strtok(NULL, "|\n");
            if (!node || !strlen(node))
                continue;

            udn = node;

            APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "%s|%s\n", index, udn);

            ba_UpdateDeviceData(udn, index, &error);
        }
    }

    if (file)
        fclose(file);

    if (error)
    {
        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG,"LoadDeviceList(): %s \n", error);
        ba_CancelUpdateData();
    }
    else
        ba_ApplyUpdatedData();

    return !error;
}
//-----------------------------------------------------------------------------
static bool ba_GetDeviceStatus(char* udn, int* index, int* on_off, unsigned* next_toggle)
{
    bool        result = false;
    BA_Device   device;
    int         once, d;

    pthread_mutex_lock(&sControl.DatLock);

    for (once=1; once; once--)
    {
        if (!udn || !strlen(udn))
            break;

        for (d = 0; d < sControl.DeviceCount; d++)
        {
            device = sControl.DeviceList[d];

            if (strcmp(udn, device->UDN) == 0)
                break;
        }
        if (sControl.DeviceCount <= d)
        {
            APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "GetDeviceStatus(): Device Not Found %s", udn);
            break;
        }

        // if the device stops doing Away mode, then break.
        if(device->Running == false)
            break;

        *index       = device->Index;
        *on_off      = device->OnOff;
        *next_toggle = device->NextToggle - time(0);

        result = true;
    }

    pthread_mutex_unlock(&sControl.DatLock);

    return result;
}

//-----------------------------------------------------------------------------
bool BA_LoadRule()
{
    int         once;

#if 0
    if (sControl.Configured)
        return true;
#endif
    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "g_szUDN = %s", g_szUDN);

    pthread_mutex_init(&sControl.CtrLock, NULL);
    pthread_mutex_init(&sControl.DatLock, NULL);

    for (once=1; once; once--)
    {
        sControl.Scale = 1;

        ba_LoadDeviceListFromStorage();

        if (!ba_CreateRuleThread())
            break;

        sControl.Configured = true;
    }
    if (!sControl.Configured)
    {
        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "BA_LoadRule() failed");
        return false;
    }
#if 1
    BA_SetLinkedAwayMode();
#endif

    return true;
}
//-----------------------------------------------------------------------------
void putSubDevicesOff()
{
	int d;
	const char* on_off;
	BA_Device       device;
	on_off = scValueOff;

	for (d = 0; d < sControl.DeviceCount; d++)
    {
		device = sControl.DeviceList[d];

		if (sControl.DeviceList[d]->SubDeviceID.Type)
		{
			APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "Switch OFF subdevice id [%d]", d);
			SD_SetCapabilityValue(&device->SubDeviceID, &sCapabilityOnOff, on_off, SE_LOCAL + SE_REMOTE);
			device->Running = false;
			device->OnOff   = 0;
		}
		else{
			APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "other device id [%d]", d);
		}
	}
}
void BA_FreeOldRule()
{
	APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "BA_FreeOldRule");
	if(sControl.State || sControl.Started)
	{
		ba_DestroyRuleThread();

	}
	APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "ba_DeleteDeviceData");	
	ba_DeleteDeviceData();
	
	if(sControl.Configured)
	{
	    	sControl.Configured = false;

	}

}

void BA_FreeRuleTask()
{
	pthread_mutex_lock(&sControl.CtrLock);
    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "-BA_FreeRuleTask -");
	if(sControl.State)
	{
		sControl.State = BA_STOP;


		ba_DestroyRuleThread();


		sControl.Started = 0;

	}

    pthread_mutex_unlock(&sControl.CtrLock);

   //pthread_mutex_destroy(&sControl.CtrLock);
   //pthread_mutex_destroy(&sControl.DatLock);


}

void BA_FreeRule()
{

	pthread_mutex_lock(&sControl.CtrLock);
	putSubDevicesOff();
    ba_DestroyRuleThread();

    ba_DeleteDeviceData();

    sControl.Configured = false;

    pthread_mutex_unlock(&sControl.CtrLock);

    pthread_mutex_destroy(&sControl.CtrLock);
    pthread_mutex_destroy(&sControl.DatLock);
    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "BA_FreeRule()");
}
//-----------------------------------------------------------------------------
//
//  <DeviceList>
//    <Device>
//       <UDN>uuid:Lightswitch-1_0-221143K130001C</UDN>
//        <index>0</index>
//    </Device>
//    <Device>
//        <UDN>uuid:Socket-1_0-221212K0100996</UDN>
//        <index>1</index>
//    </Device>
//    <Device>
//        <UDN>uuid:Bridge-1_0-231404B0100090:B4750E1B9578103B</UDN>
//        <index>2</index>
//    </Device>
//    <Device>
//        <UDN>uuid:Bridge-1_0-231404B0100090:C303138455200033</UDN>
//        <index>3</index>
//    </Device>
//  </DeviceList>
//
//-----------------------------------------------------------------------------
int BA_SimulatedRuleData(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
    int         ret = UPNP_E_INTERNAL_ERROR;
    const char* result;
    char*       device_list;
    int         once;

    *out = 0;
    *errorString = 0;


    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "New XML came :BA_FreeOldRule()");
    BA_FreeOldRule();
    sControl.Configured = true; /* As br_away task not started at init() */


    device_list = Util_GetFirstDocumentItem(request, "DeviceList");
    pthread_mutex_lock(&sControl.CtrLock);

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "SimulatedRuleData()");

    for (once=1; once; once--)
    {

    	if (!sControl.Configured)
        {
            *errorString = "Internal Error";

            break;
        }
		printf("local DeviceList:--------%s", device_list);

        ret = ba_UpdateDeviceList(device_list, errorString);
    }

    if (ret == UPNP_E_SUCCESS)
        result = scSuccess;
    else
        result = scFail;

    UpnpAddToActionResponse(out, "SimulatedRuleData", CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE], scResult, result);

    if (*errorString)
        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "SimulatedRuleData(): %s", *errorString);

    pthread_mutex_unlock(&sControl.CtrLock);

    FreeXmlSource(device_list);

    return ret;
}
#if 1
//-----------------------------------------------------------------------------
int BA_GetSimulatedRuleData(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
    int         ret = UPNP_E_INVALID_PARAM;
    char*       udn;
    char        data[256] = "0";
    int         index = 0;
    int         on_off = 0;
    unsigned    next_toggle = 0;
    int         once;

    *out = 0;
    *errorString = 0;

    pthread_mutex_lock(&sControl.CtrLock);

    udn = Util_GetFirstDocumentItem(request, "UDN");

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "GetSimulatedRuleData(%s)", udn);

    for (once=1; once; once--)
    {

        if (!sControl.Configured)
        {
            *errorString = "Internal Error";
            break;
        }

        if (ba_GetDeviceStatus(udn, &index, &on_off, &next_toggle))
        {
            snprintf(data, sizeof(data), "%d|%d|%d|%s", index, on_off, next_toggle, udn);
            ret = UPNP_E_SUCCESS;
        }
    }

    UpnpAddToActionResponse(out, "GetSimulatedRuleData", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "RuleData", data);

    if (*errorString)
        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "GetSimulatedRuleData(): %s", *errorString);

    FreeXmlSource(udn);

    pthread_mutex_unlock(&sControl.CtrLock);

    return ret;
}
#endif
#if 1
//-----------------------------------------------------------------------------
void BA_GetSimulatedRuleDataResponse(struct Upnp_Action_Complete *args)
{
    char*       rule_data;
    char       *index, *state, *next, *udn;

    BA_Device   device;
    int         once, d;

    pthread_mutex_lock(&sControl.CtrLock);

    rule_data = Util_GetFirstDocumentItem(args->ActionResult, "RuleData");

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "GetSimulatedRuleDataResponse() %s", rule_data);

    for (once=1; once; once--)
    {
        if (!rule_data)
            break;

        if (!sControl.Configured)
            break;

		index = strtok(rule_data, "|");
		state = strtok(NULL, "|");
		next  = strtok(NULL, "|");
		udn   = strtok(NULL, "|");

        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "    Device: Index %s, State %s, Next Toggle %s, UDN %s", index, state, next, udn);

        if (!index || !state || !next || !udn)
            break;

        pthread_mutex_lock(&sControl.DatLock);

        for (d = 0; d < sControl.DeviceCount; d++)
        {
            device = sControl.DeviceList[d];

            if (strcmp(udn, device->UDN) == 0)
            {
                device->OnOff      = atoi(state);
                device->NextToggle = atoi(next);

                sControl.OtherDevicesUpdated++;
            }
        }

        pthread_mutex_unlock(&sControl.DatLock);
    }

    FreeXmlSource(rule_data);

    pthread_mutex_unlock(&sControl.CtrLock);
}

//-----------------------------------------------------------------------------
int BA_GetNotifyManualToggle(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
    int         ret = UPNP_E_INTERNAL_ERROR;
    char*       udn;
    const char* result;
    int         once, d;

    *out = 0;
    *errorString = 0;

    pthread_mutex_lock(&sControl.CtrLock);

    udn = Util_GetFirstDocumentItem(request, "UDN");

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "NotifyManualToggle(%s)", udn);

    for (once=1; once; once--)
    {

        if (!sControl.Configured)
        {
            *errorString = "Internal Error";
            break;
        }

        sControl.ManualToggle = time(0);
        sControl.Active = false;

        //// TODO - saving ManualToggle

        pthread_mutex_lock(&sControl.DatLock);

        for (d = 0; d < sControl.DeviceCount; d++)
        {
			// If Away mode stopped by Manual toggle, then just stop Away mode. keep the bulb status.
            //ba_SetSchedule(&sControl.DeviceList[d]->SubDeviceID, false, BT_NORMAL);
			sControl.DeviceList[d]->Running = false;
        }

        pthread_mutex_unlock(&sControl.DatLock);

        ret = UPNP_E_SUCCESS;
    }

    if (ret == UPNP_E_SUCCESS)
        result = scSuccess;
    else
        result = scFail;

    ret = UpnpAddToActionResponse(out, "NotifyManualToggle", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "ManualToggle", result);

    if (*errorString)
        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "NotifyManualToggle(): %s", *errorString);

    FreeXmlSource(udn);

    pthread_mutex_unlock(&sControl.CtrLock);

    return UPNP_E_SUCCESS;
}
#endif
//-----------------------------------------------------------------------------
void ba_SendNotifyManualToggle(void)
{
    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "Send NotifyManualToggle ");
    PluginCtrlPointSendActionAll(PLUGIN_E_EVENT_SERVICE, "NotifyManualToggle", 0x00, 0x00, 0x00);
}
//-----------------------------------------------------------------------------
// return true: There was an manual toggle.
bool BA_CheckManualToggle(unsigned int ruleTime)
{
    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "BA_CheckManualToggle rule:%d, sControl.ManualToggle:%d ", ruleTime, sControl.ManualToggle);
    if(BR_CheckManualToggleTimeAvailable(sControl.ManualToggle, ruleTime) == true) {
        return true;
    }
    return false;
}
//-----------------------------------------------------------------------------
// bulb was on/off manually during Away mode, so we will cancel it.
void BA_SetManualToggle(void)
{
    int         once, d;

    pthread_mutex_lock(&sControl.CtrLock);

    for (once=1; once; once--)
    {

        if (!sControl.Configured)
        {
            //*errorString = "Internal Error";
            break;
        }

        sControl.ManualToggle = time(0);
        sControl.Active = false;
        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "%s tm:%d", __func__, sControl.ManualToggle);

        pthread_mutex_lock(&sControl.DatLock);

        for (d = 0; d < sControl.DeviceCount; d++)
        {
			// If Away mode stopped by Manual toggle, then just stop Away mode. keep the bulb status.
			sControl.DeviceList[d]->Running = false;
        }

        pthread_mutex_unlock(&sControl.DatLock);
    }

    pthread_mutex_unlock(&sControl.CtrLock);

    // Notify to neighbor devices.
    ba_SendNotifyManualToggle();
}
//-----------------------------------------------------------------------------
bool BA_CheckBridgeUDN(const char* udn, bool* sub_device)
{
    *sub_device = (strcmp(udn, g_szUDN) != 0);

    return (strstr(udn, g_szUDN) != 0);
}
//-----------------------------------------------------------------------------
bool BA_GetSubDeviceID(const char* udn, SD_DeviceID* id, bool check_group)
{
    char            buffer[128];
    char*           node;
    SD_Device       sd;
    SD_Group        group;

    strncpy(buffer, udn, sizeof(buffer)-1); // udn = udn = udn = uuid:Bridge-1_0-231407B0100076:B4750E1B957801D1

    node = strtok(buffer, scDelimiter); //scDelimiter = ":"
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
void BA_ClearManualToggle()
{
    pthread_mutex_lock(&sControl.CtrLock);

    sControl.ManualToggle = 0;

    pthread_mutex_unlock(&sControl.CtrLock);
}
//-----------------------------------------------------------------------------
//void BA_SetLinkedAwayMode(const char* value, unsigned offset)
void BA_SetLinkedAwayMode()
{
    int         d;

    pthread_mutex_lock(&sControl.CtrLock);
#if 0
    sscanf(value, "%d", &mode);

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "Away Mode (%d,%u)", mode, offset);
#endif
    sControl.Active = true;

    pthread_mutex_lock(&sControl.DatLock);

    for (d = 0; d < sControl.DeviceCount; d++)
    {
        ba_SetSchedule(&sControl.DeviceList[d]->SubDeviceID, true, BT_START);
    }

    ba_UpdateSubDevices();

    sControl.Linked = 1;

    pthread_mutex_unlock(&sControl.DatLock);
    pthread_mutex_unlock(&sControl.CtrLock);
}
//-----------------------------------------------------------------------------
void BA_StopLinkedAwayMode(void)
{
    int d;

    if (!sControl.Active)
        return;

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "Stop Linked Away Mode ");
    pthread_mutex_lock(&sControl.DatLock);

    for (d = 0; d < sControl.DeviceCount; d++)
    {
        sControl.DeviceList[d]->Running = false;
    }

    sControl.Active = false;
    sControl.Linked = 0;

    pthread_mutex_unlock(&sControl.DatLock);
}

#if 0
//-----------------------------------------------------------------------------
void BA_SetDeviceAwayMode(SD_DeviceID* device_id, const char* value, unsigned int ruleTime)
{
    int         mode;

    if(BA_CheckManualToggle(ruleTime) == true) {
        // There was an manual toggle, so don't start away mode today.
        return;
    }

    pthread_mutex_lock(&sControl.CtrLock);

    sscanf(value, "%d", &mode);

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "Away Mode %s (%d,%u)", device_id->Data, mode, ruleTime);

    sControl.Active = true;

    pthread_mutex_lock(&sControl.DatLock);

    ba_SetSchedule(device_id, mode, BT_START);
    ba_UpdateSubDevices();

    pthread_mutex_unlock(&sControl.DatLock);
    pthread_mutex_unlock(&sControl.CtrLock);
}
#endif

//-----------------------------------------------------------------------------
void BA_StopDeviceAwayMode(SD_DeviceID* sd_id)
{
    BA_Device       device;
    bool    found = false;
    int     d = 0;

    if (!sControl.Active)
        return;

    if(sControl.DeviceCount == 0)
        return;


    pthread_mutex_lock(&sControl.DatLock);

    for (d = 0; d < sControl.DeviceCount; d++)
    {
        device = sControl.DeviceList[d];

        if ((device->SubDeviceID.Type == sd_id->Type) && (strcmp(device->SubDeviceID.Data, sd_id->Data) == 0)) {
            found = true;
            break;
        }
    }

    if (found == true)
    {
        bool oneOfDeviceIsInAwayMode = false;

        APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "Stop Device Away Mode %s ", sd_id->Data);
        device->Running = false;

        // check if one of device is in away mode.
        for (d = 0; d < sControl.DeviceCount; d++)
        {
            if(sControl.DeviceList[d]->Running == true) {
                oneOfDeviceIsInAwayMode = true;
            }
        }

        if(oneOfDeviceIsInAwayMode == false) {
            APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "All the devices are not in Away mode, so we will turn off Away mode.");
            sControl.Active = false;
        }
    }

    pthread_mutex_unlock(&sControl.DatLock);
}
//-----------------------------------------------------------------------------
bool BA_CheckDeviceInAwayMode(char* id)
{
    bool            running = false;
    BA_Device       device;
    int             once, d;

    if (!sControl.Active)
        return false;

    pthread_mutex_lock(&sControl.DatLock);

    for (once=1; once; once--)
    {
        for (d = 0; d < sControl.DeviceCount; d++)
        {
            device = sControl.DeviceList[d];

            if (device->SubDeviceID.Type && (strcmp(device->SubDeviceID.Data, id) == 0))
                break;
        }
        if (sControl.DeviceCount <= d)
            break;

        running = device->Running;
    }

    pthread_mutex_unlock(&sControl.DatLock);

    return running;
}
//-----------------------------------------------------------------------------
int BA_GetDevicesInAwayMode()
{
    return sControl.RunningSubDevices;
}
//-----------------------------------------------------------------------------
#if 0
void BA_ClearDeviceListFromStorage()
{
    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "BA_ClearDeviceListFromStorage() ");

    BA_SetLinkedAwayMode("0", 0);

    if (sControl.Configured)
    {
        pthread_mutex_lock(&sControl.CtrLock);
        pthread_mutex_lock(&sControl.DatLock);

        sControl.Active = false;

        ba_DeleteDeviceData();

        pthread_mutex_unlock(&sControl.DatLock);
        pthread_mutex_unlock(&sControl.CtrLock);
    }

    remove(scFileDeviceList);
}
#endif
//-----------------------------------------------------------------------------
bool BA_RemoteUpdateSimulatedRuleData(const char* device_list)
{
    const char* error = 0;
    int         once;
    int ret = false;


    pthread_mutex_init(&sControl.CtrLock, NULL);
    pthread_mutex_init(&sControl.DatLock, NULL);
    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "New XML Remote :BA_FreeOldRule()");
    BA_FreeOldRule();
    sControl.Configured = true; /* As br_away task not started at init() */

    pthread_mutex_lock(&sControl.CtrLock);

    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "RemoteUpdateSimulatedRuleData()");

    for (once=1; once; once--)
    {
        if (!sControl.Configured)
        {
            error = "Internal Error";
            break;
        }

        ret = ba_UpdateDeviceList(device_list, &error);
    }
    pthread_mutex_unlock(&sControl.CtrLock);
    APP_LOG("DEVICE:br_away_rule", LOG_DEBUG, "ret %d", ret);
    return ret;
}
//-----------------------------------------------------------------------------
#endif

