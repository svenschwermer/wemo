/***************************************************************************
*
* Created by Belkin International, Software Engineering on Aug 13, 2013
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#include "defines.h"
#include "logger.h"
#include "utils.h"
#include "defines.h"
#include "utlist.h"
#include "queue.h"
#include "event_queue.h"

#include "zigbee_api.h"
#include "rule.h"

#define  _SD_MODULE_
#include "subdevice.h"
#include "sd_configuration.h"
#include "sd_device_specific.h"
#include "sd_event.h"
#include "sd_tracer.h"
#include "utility.h"

#include <belkin_diag.h> 
//-----------------------------------------------------------------------------

#define JOINING_BROADCAST_INTERVAL  15
#define AVAILABLE_CHECK_LIMIT       3

#define PIR_REPORT_INTERVAL         60     // seconds
#define LOW_BATTERY_THRESHOLD_INDEX 3
#define LOW_SIGNAL_THRESHOLD_INDEX  4

#define UTC_2015_1_1_GMT            1420070400

typedef struct
{
    bool                Configured;
    unsigned            NetworkOpened;
    pthread_mutex_t     Lock;

} SM_Control;

struct ZBCommand {
    bool            bGroup;         //True, False;
    int             nControl;       //ZB_CMD_GET / ZB_CMD_SET
    SM_GroupDevice  group_device;
    SD_Device       device;
    SD_Capability   capability;
    char            value[CAPABILITY_MAX_VALUE_LENGTH];
};

//-----------------------------------------------------------------------------

static SM_DeviceConfiguration   sDevConfig;
static SM_Control               sControl;

// static const char* scPristineZC         = SD_SAVE_DIR "pristine_zc";
// static const char* scZBCoordinate       = SD_SAVE_DIR "zb_coordinate";
// static const char* scZBRouter           = SD_SAVE_DIR "zb_router";
static const char* scFileGroupList      = SD_SAVE_DIR "sd_groups";
static const char* scDelimiter          = ":";
static const char* scGroupID            = "GroupID";
static const char* scGroupName          = "GroupName";
static const char* scGroupMulticastID   = "GroupMulticastID";
static const char* scGroupDevice        = "Device";

static const char* scWriteFailed        = "Write Failed";

static const char* sCapabilityOnOff             = "10006";
static const char* sCapabilityLevel             = "10008";
static const char* sCapabilityColorCtrl         = "10300";
static const char* sCapabilityColorTemp         = "30301";
static const char* sCapabilitySensor            = "10500";
static const char* sCapabilitySensorKeyPress    = "20502";
char* sSensorNotificationEnabled   = "2:900";

static pthread_t sEventThreadID;
static pthread_t sCommandThreadID;

static bool sEventRunning = true;
static bool sCommandRunning = true;

static int sEventID = 0;
static token_t sEventToken;

sysevent_t sd_events[] = {
  {"zb_onoff_status", TUPLE_FLAG_EVENT, {0,0}},
  {"zb_level_status", TUPLE_FLAG_EVENT, {0,0}},
  {"zb_color_temp",   TUPLE_FLAG_EVENT, {0,0}},
  {"zb_color_xy",     TUPLE_FLAG_EVENT, {0,0}},
  {"zb_error_status", TUPLE_FLAG_EVENT, {0,0}},
  {"zb_zone_status",  TUPLE_FLAG_EVENT, {0,0}},
  {"zb_node_rssi",    TUPLE_FLAG_EVENT, {0,0}},
  {"zb_node_voltage", TUPLE_FLAG_EVENT, {0,0}},
  {"zb_node_present", TUPLE_FLAG_EVENT, {0,0}},
  {"zb_save_status",  TUPLE_FLAG_EVENT, {0,0}},
};


static SD_DeviceID* ota_device_id = NULL;

typedef struct
{
   int      voltage;
   int      index;
} SensorVoltageIndex;


SensorVoltageIndex volIndexTable[] =
{
    {21, 1},
    {22, 2},
    {23, 3},
    {24, 4},
    {25, 5},
    {26, 6},
    {27, 7},
    {28, 8},
    {29, 9},
    {30, 10},
    {31, 10},
    {32, 10},
    {33, 10},
    {34, 10},
    {35, 10},
    {36, 10},
    {37, 10},
    {38, 10},
    {39, 10},
    {40, 10}
};


struct Queue *g_pCmdQueue = NULL;

extern unsigned long int GetUTCTime(void);

//-----------------------------------------------------------------------------
static bool sm_FindCapability(const SD_CapabilityID* p_id, SM_Capability* p_capability)
{
    bool             result = false;
    SD_CapabilityID* id;
    SM_Capability    capability;
    int              c;

    if (!p_id || !p_capability)
    {
        Debug("sm_FindCapability(): Invalid Parameters \n");
        return false;
    }

    for (c = 0; c < sDevConfig.CapabilityCount; c++)
    {
        capability = sDevConfig.CapabilityList[c];

        switch (p_id->Type)
        {
        case SD_CAPABILITY_ID:            id = &capability->CapabilityID; break;
        case SD_CAPABILITY_REGISTERED_ID: id = &capability->RegisteredID; break;
        default:                          id = 0;
        }

        if (!id)
            break;

        if (strncasecmp(p_id->Data, id->Data, sizeof(id->Data)-1) == 0)
        {
            *p_capability = capability;

            result = true;
            break;
        }
    }

    if (!result)
        Debug("sm_FindCapability(%d:%s): Capability Not Found \n", p_id->Type, p_id->Data);

    return result;
}
//-----------------------------------------------------------------------------
static bool sm_FindDevice(const SD_DeviceID* p_id, SM_Device* p_device)
{
    bool            result = false;
    SD_DeviceID*    id;
    SM_Device       device;
    int             d;

    if (!p_id || !p_device)
    {
        Debug("sm_FindDevice(): Invalid Parameters \n");
        return false;
    }

    for (d = 0; d < sDevConfig.DeviceCount; d++)
    {
        device = sDevConfig.DeviceList[d];

        switch (p_id->Type)
        {
        case SD_DEVICE_EUID:          id = &device->EUID;         break;
        case SD_DEVICE_REGISTERED_ID: id = &device->RegisteredID; break;
        case SD_DEVICE_UDN:           id = &device->UDN;          break;
        default:                      id = 0;
        }

        if (!id)
            break;

        if (strncasecmp(p_id->Data, id->Data, sizeof(id->Data)-1) == 0)
        {
            *p_device = device;

            result = true;
            break;
        }
    }

    if (!result)
        Debug("sm_FindDevice(%d:%s): Device Not Found or it's an GroupID \n", p_id->Type, p_id->Data);

    return result;
}

//-----------------------------------------------------------------------------
static bool sm_FindDeviceInOtherGroup(const SD_GroupID* self_group_id, const SD_DeviceID* p_id, bool debug)
{
    bool            result = true;
    SM_Group        group;
    int             g=0,i=0,count=0;

    if (!p_id)
    {
        Debug("sm_FindDeviceInOtherGroup(): Invalid Parameters \n");
        return false;
    }

    for (g = 0; g < sDevConfig.GroupCount; g++)
    {
	    group  = sDevConfig.GroupList[g];
	    count  = group->DeviceCount;
	    for(i=0;i<count;i++)
	    {
		    /*check if device IDs are not present in already created groups*/
		    if((strncasecmp(self_group_id->Data, group->ID.Data, DEVICE_MAX_ID_LENGTH-1) != 0)
		    && (strncasecmp(p_id->Data,group->Device[i]->EUID.Data,DEVICE_MAX_ID_LENGTH-1) == 0)) {
			    result = false;
			    break;
		    }
	    }

    }

    if (!result && debug)
        Debug("sm_FindDeviceInOtherGroup(%s): is already a part of group [%s] and group id [%s])\n", p_id->Data,group->Name,group->ID.Data);

    return result;
}

//-----------------------------------------------------------------------------
static bool sm_FindGroup(const SD_GroupID* p_id, SM_Group* p_group, bool debug)
{
    bool            result = false;
    SD_GroupID*     id;
    SM_Group        group;
    int             g;

    if (!p_id || !p_group)
    {
        Debug("sm_FindGroup(): Invalid Parameters \n");
        return false;
    }

    for (g = 0; g < sDevConfig.GroupCount; g++)
    {
        group = sDevConfig.GroupList[g];
        id    = &group->ID;

        if (strncasecmp(p_id->Data, id->Data, sizeof(id->Data)-1) == 0)
        {
            *p_group = group;

            result = true;
            break;
        }
    }

    if (!result && debug)
        Debug("sm_FindGroup(%s): Group Not Found \n", p_id->Data);

    return result;
}
//-----------------------------------------------------------------------------
bool sm_GetGroupID(SD_DeviceID* device_id, SD_GroupID* group_id)
{
    int g, d;
    SM_Group group;

    for (g = 0; g < sDevConfig.GroupCount; g++)
    {
        group = sDevConfig.GroupList[g];

        for(d = 0; d < group->DeviceCount; d++)
        {
            if (strncmp(device_id->Data, group->Device[d]->EUID.Data, DEVICE_MAX_ID_LENGTH) == 0)
            {
                SAFE_STRCPY(group_id->Data, group->ID.Data);
                return true;
            }
        }
    }
    return false;
}


//-----------------------------------------------------------------------------
static void sm_RemoveDeviceProperty(SM_Device device)
{
    int         d, i;

    if (device->DeviceSpecific)
        free(device->DeviceSpecific);

    free(device);

    // rearranging DeviceList

    for (d = 0; d < sDevConfig.DeviceCount; d++)
    {
        if (device == sDevConfig.DeviceList[d])
            break;
    }

    for (i = d; i < sDevConfig.DeviceCount-1; i++)
    {
        sDevConfig.DeviceList[i] = sDevConfig.DeviceList[i+1];
    }

    sDevConfig.DeviceList[i] = 0;
    sDevConfig.DeviceCount--;
}
//-----------------------------------------------------------------------------
static void sm_RemoveUnpairedDevices()
{
    // rejecting unpaired devices

    SM_Device   device;
    int         d;

    if (!sDevConfig.DeviceCount)
        return;

    for (d = sDevConfig.DeviceCount-1; 0 <= d; d--)
    {
        device = sDevConfig.DeviceList[d];

        if (!device->Paired)
        {
            SM_RejectDevice(device->DeviceSpecific, &sDevConfig.DeviceScanOption);
            sm_RemoveDeviceProperty(device);
        }
    }
}
//-----------------------------------------------------------------------------
static bool sm_CheckDeviceCapability(SD_Device device, SD_Capability capability, char* control, int* index, bool debug)
{
    bool        result = false;
    int         c;

    for (c = 0; c < device->CapabilityCount; c++)
    {
        DEBUG_LOG(ULOG_CORE, UL_DEBUG, "sm_CheckDeviceCapability....dp",c);
        if (capability == device->Capability[c])
        {
            DEBUG_LOG(ULOG_CORE, UL_DEBUG, "capability == device->Capability[%d], capability->Control=%s....dp",c, capability->Control);
            if (strstr(capability->Control, control))
            {
                DEBUG_LOG(ULOG_CORE, UL_DEBUG, "---------------------....dp");
                *index = c;
                result = true;
            }
            
            break;
        }
    }

    if (!result && debug)
        Debug("sm_CheckDeviceCapability(%s,%s,'%s'): Capability Not Supported \n", device->EUID.Data, capability->CapabilityID.Data, control);

    return result;
}
//-----------------------------------------------------------------------------
static bool sm_GetDeviceCapabilityIndex(SD_Device device, SD_Capability capability, int* index, bool debug)
{
    bool        result = false;
    int         c;

    for (c = 0; c < device->CapabilityCount; c++)
    {
        if (capability == device->Capability[c])
        {
            *index = c;
            result = true;
            break;
        }
    }

    if (!result && debug)
        Debug("%s(%s,%s): Capability Not Supported \n",__func__, device->EUID.Data, capability->CapabilityID.Data);

    return result;
}
static bool sm_CheckGroupDevice(SD_Group group, SD_Device device)
{
    bool        result = false;
    int         d;

    for (d = 0; d < group->DeviceCount; d++)
    {
        if (device == group->Device[d])
        {
            result = true;
            break;
        }
    }

    return result;
}
//-----------------------------------------------------------------------------
static bool sm_CheckGroupCapability(SD_Group group, SD_Capability capability, const char* control, int* index, bool debug)
{
    bool        result = false;
    int         c;

    for (c = 0; c < group->CapabilityCount; c++)
    {
        if (capability == group->Capability[c])
        {
            if (strstr(capability->Control, control))
            {
                *index = c;
                result = true;
            }

            break;
        }
    }

    if (!result && debug)
        Debug("sm_CheckGroupCapability(%s,%s,'%s'): Capability Not Supported \n", group->ID.Data, capability->CapabilityID.Data, control);

    return result;
}
//-----------------------------------------------------------------------------
static bool sm_GetGroupCapabilityIndex(SD_Group group, SD_Capability capability, int* index, bool debug)
{
    bool        result = false;
    int         c;

    for (c = 0; c < group->CapabilityCount; c++)
    {
        if (capability == group->Capability[c])
        {
            *index = c;
            result = true;
            break;
        }
    }

    if (!result && debug)
        Debug("%s(%s,%s): Capability Not Supported \n",__func__, group->ID.Data, capability->CapabilityID.Data);

    return result;
}
static void sm_UpdateGroupCapability(SM_Group group)
{
    SD_Device       device;
    SD_Capability   capability;
    int             d, c, gc;

    group->CapabilityCount = 0;

    for (d = 0; d < group->DeviceCount; d++)
    {
        device = group->Device[d];

        for (c = 0; c < device->CapabilityCount; c++)
        {
            capability = device->Capability[c];

            if (!sm_CheckGroupCapability(group, capability, capability->Control, &gc, false))
            {
                if (group->CapabilityCount < GROUP_MAX_CAPABILITIES)
                {
                    group->Capability[group->CapabilityCount] = capability;
                    group->CapabilityCount++;
                }
            }
        }
    }
}
//-----------------------------------------------------------------------------
static void sm_UpdateGroupConfiguration()
{
    int         g;

    for (g = 0; g < sDevConfig.GroupCount; g++)
    {
        sm_UpdateGroupCapability(sDevConfig.GroupList[g]);
    }
}
//-----------------------------------------------------------------------------
static bool sm_RemoveGroupDevice(SM_Device device)
{
    bool            updated = false;
    SM_Group        group;
    int             g, d, i;

    for (g = 0; g < sDevConfig.GroupCount; g++)
    {
        group = sDevConfig.GroupList[g];

        // checking Group has device

        for (d = 0; d < group->DeviceCount; d++)
        {
            if (device == group->Device[d])
                break;
        }

        if (d < group->DeviceCount)
        {
            // rearranging Group Device

            for (i = d; i < group->DeviceCount-1; i++)
            {
                group->Device[i] = group->Device[i+1];
            }

            group->Device[i] = 0;
            group->DeviceCount--;

            // updating Group CapabilityList

            sm_UpdateGroupCapability(group);

            updated = true;
        }
    }

    return updated;
}
//-----------------------------------------------------------------------------
static bool sm_LoadGroupListFromFile()
{
    bool        result = false;
    FILE*       file = 0;
    SM_Group    group = 0;
    SD_DeviceID device_id = {SD_DEVICE_EUID, {0}};
    SM_Device   device;
    int         once;
    char*       error = 0;
    char*       key;
    char*       value;
    char        buffer[GROUP_MAX_ID_LENGTH*8];

    for (once=1; once; once--)
    {
        file = fopen(scFileGroupList, "r");

        if (!file)
            break;

        while (fgets(buffer, sizeof(buffer), file))
        {
            if (GROUP_MAX_COUNT <= sDevConfig.GroupCount)
            {
                error = "GROUP_MAX_COUNT";
                break;
            }

            key = strtok(buffer, scDelimiter);

            if (!key)
                break;

            value = strtok(NULL, "\n");

            if (!value)
                continue;

            if (strcmp(scGroupID, key) == 0)
            {
                group = (SM_Group)calloc(1, sizeof(SD_GroupProperty));

                if (!group)
                {
                    error = "Out of Memory";
                    break;
                }

                sDevConfig.GroupList[sDevConfig.GroupCount] = group;
                sDevConfig.GroupCount++;

                SAFE_STRCPY(group->ID.Data, value);
            }
            else if (group && (strcmp(scGroupName, key) == 0))
            {
                SAFE_STRCPY(group->Name, value);
            }
            else if (group && (strcmp(scGroupMulticastID, key) == 0))
            {
                sscanf(value, "%X", &group->MulticastID);
            }
            else if (group && (strcmp(scGroupDevice, key) == 0))
            {
                SAFE_STRCPY(device_id.Data, value);

                if (!sm_FindDevice(&device_id, &device))
                    continue;

                if (GROUP_MAX_DEVICES <= group->DeviceCount)
                {
                    error = "GROUP_MAX_DEVICES";
                    break;
                }
                if (sm_CheckGroupDevice(group, device))
                {
                    Debug("Group %s - Duplicate Device: %s\n", group->ID.Data, device_id.Data);
                    continue;
                }

                group->Device[group->DeviceCount] = device;
                group->DeviceCount++;
            }
            else
            {
                error = "Invalid Key";
                break;
            }
        }

        result = true;
    }

    if (file)
        fclose(file);

    if (error)
    {
        Debug("sm_LoadGroupListFromFile(): %s \n", error);
    }
    else
    {
        Debug("GroupList Loaded: %d \n", sDevConfig.GroupCount);
    }

    return result;
}
//-----------------------------------------------------------------------------
static bool sm_SaveGroupListToFile()
{
    bool        result = false;
    FILE*       file = 0;
    SM_Group    group;
    int         once, g, d;
    const char* error = 0;

    for (once=1; once; once--)
    {
        file = fopen(scFileGroupList, "w");

        if (!file)
        {
            error = "Open Failed";
            break;
        }

        for (g = 0; g < sDevConfig.GroupCount; g++)
        {
            group = sDevConfig.GroupList[g];

            if (!fprintf(file, "%s%s%s\n", scGroupID, scDelimiter, group->ID.Data))
            {
                error = scWriteFailed;
                break;
            }

            if (strlen(group->Name) && !fprintf(file, "%s%s%s\n", scGroupName, scDelimiter, group->Name))
            {
                error = scWriteFailed;
                break;
            }

            if (!fprintf(file, "%s%s%X\n", scGroupMulticastID, scDelimiter, group->MulticastID))
            {
                error = scWriteFailed;
                break;
            }

            for (d = 0; d < group->DeviceCount; d++)
            {
                if (!fprintf(file, "%s%s%s\n", scGroupDevice, scDelimiter, group->Device[d]->EUID.Data))
                {
                    error = scWriteFailed;
                    break;
                }
            }
        }

        result = true;
    }

    if (file)
        fclose(file);

    if (error)
        Debug("sm_SaveGroupListToFile(): %s \n", error);

    return result;
}
//-----------------------------------------------------------------------------
static void sm_SetGroupReferenceID()
{
    int         reference = 0;
    SM_Group    group;
    int         g;

    for (g = 0; g < sDevConfig.GroupCount; g++)
    {
        group = sDevConfig.GroupList[g];

        reference = MAX(reference, group->MulticastID);
    }

    if (reference)
        sDevConfig.GroupReferenceID = reference;
}
//-----------------------------------------------------------------------------
static void sm_RemoveGroupMulticastID(SD_Group group)
{
    int             result = 0;
    SD_Device       device;
    int             d;

    for (d = 0; d < group->DeviceCount; d++)
    {
        device = group->Device[d];

        if (SM_CheckDeviceMulticast(device))
        {
            Debug("- %s (%04X)\n", device->EUID.Data, group->MulticastID);

            if (SM_RemoveDeviceMulticastID(device, group->MulticastID))
                result++;
            else
                Debug("RemoveDeviceMulticastID() failed\n");
        }
    }
}
//-----------------------------------------------------------------------------
static void sm_FreeCapabilityList()
{
    int         i;

    if (!sDevConfig.CapabilityList)
        return;

    for (i = 0; i < sDevConfig.CapabilityCount; i++)
    {
        if (!sDevConfig.CapabilityList[i])
            continue;

        if (sDevConfig.CapabilityList[i]->DeviceSpecific)
            free(sDevConfig.CapabilityList[i]->DeviceSpecific);

        free(sDevConfig.CapabilityList[i]);
        sDevConfig.CapabilityList[i] = 0;
    }

    free(sDevConfig.CapabilityList);
    sDevConfig.CapabilityList = 0;

    sDevConfig.CapabilityCount = 0;
}
//-----------------------------------------------------------------------------
static bool sm_LoadCapabilityList()
{
    int         once;

    if (sDevConfig.CapabilityList)
        return true;

    for (once=1; once; once--)
    {
        sDevConfig.CapabilityList = (SM_CapabilityList)calloc(CAPABILITY_MAX_COUNT, sizeof(SM_Capability));

        if (!sDevConfig.CapabilityList)
        {
            Debug("sm_LoadCapabilityList(): Out of Memory \n");
            break;
        }

        SM_LoadCapabilityListFromFile(&sDevConfig);
        SM_UpdateCapabilityList(&sDevConfig);
    }

    return (sDevConfig.CapabilityList != 0);
}
//-----------------------------------------------------------------------------
static void sm_FreeDeviceList()
{
    int         i;

    if (!sDevConfig.DeviceList)
        return;

    for (i = 0; i < sDevConfig.DeviceCount; i++)
    {
        if (!sDevConfig.DeviceList[i])
            break;

        if (sDevConfig.DeviceList[i]->DeviceSpecific)
            free(sDevConfig.DeviceList[i]->DeviceSpecific);

        free(sDevConfig.DeviceList[i]);
        sDevConfig.DeviceList[i] = 0;
    }

    free(sDevConfig.DeviceList);
    sDevConfig.DeviceList = 0;

    sDevConfig.DeviceCount = 0;
}
//-----------------------------------------------------------------------------
static bool sm_LoadDeviceList()
{
    int         once;

    if (sDevConfig.DeviceList)
        return true;

    for (once=1; once; once--)
    {
        sDevConfig.DeviceList = (SM_DeviceList)calloc(DEVICE_MAX_COUNT, sizeof(SM_Device));

        if (!sDevConfig.DeviceList)
        {
            Debug("sm_LoadDeviceList(): Out of Memory \n");
            break;
        }

        SM_LoadDeviceListFromFile(&sDevConfig);
        SM_UpdateDeviceIDs(&sDevConfig);
        SM_LoadDevicePropertiesListFromFile(&sDevConfig);
    }

    return (sDevConfig.DeviceList != 0);
}
//-----------------------------------------------------------------------------
static void sm_FreeGroupList()
{
    int         i;

    if (!sDevConfig.GroupList)
        return;

    for (i = 0; i < sDevConfig.GroupCount; i++)
    {
        if (!sDevConfig.GroupList[i])
            break;

        free(sDevConfig.GroupList[i]);
        sDevConfig.GroupList[i] = 0;
    }

    free(sDevConfig.GroupList);
    sDevConfig.GroupList = 0;

    sDevConfig.GroupCount = 0;
}
//-----------------------------------------------------------------------------
static bool sm_LoadGroupList()
{
    int         once;

    if (sDevConfig.GroupList)
        return true;

    for (once=1; once; once--)
    {
        sDevConfig.GroupList = (SM_GroupList)calloc(GROUP_MAX_COUNT, sizeof(SM_Group));

        if (!sDevConfig.GroupList)
        {
            Debug("sm_LoadGroupList(): Out of Memory \n");
            break;
        }

        sm_LoadGroupListFromFile();
        sm_SetGroupReferenceID();
    }

    return (sDevConfig.GroupList != 0);
}
//-----------------------------------------------------------------------------
static void sm_FreeConfiguration()
{
    SM_FreeTracer();

    sm_FreeGroupList();
    sm_FreeDeviceList();
    sm_FreeCapabilityList();

    pthread_mutex_destroy(&sControl.Lock);

    sControl.Configured = 0;
}
//-----------------------------------------------------------------------------
static bool sm_LoadConfiguration()
{
    int         once;

    if (sControl.Configured)
        return true;

    for (once=1; once; once--)
    {
        if (pthread_mutex_init(&sControl.Lock, NULL) != 0)
            break;

        if (!SM_LoadDeviceLibrary())
            break;

        if (!sm_LoadCapabilityList())
            break;

        if (!sm_LoadDeviceList())
            break;

        SM_UpdateDeviceConfiguration(&sDevConfig);

        if (!sm_LoadGroupList())
            break;

        sm_UpdateGroupConfiguration();

        if (!SM_LoadTracer(&sControl.Lock))
            break;

        sDevConfig.DeviceScanOption.DeviceInfo              = SCAN_NORMAL;
        sDevConfig.DeviceScanOption.CommandRetry            = 1;
        sDevConfig.DeviceScanOption.CommandRetryInterval    = 100;

        sControl.Configured = true;
    }

    if (!sControl.Configured)
        sm_FreeConfiguration();

    return sControl.Configured;
}
//-----------------------------------------------------------------------------
bool sm_QAClearAllDeviceList()
{
    SM_Device       device;
    int             g;

    // removes stored DeivceList, CapabilityList

    while (sDevConfig.DeviceCount)
    {
        device = sDevConfig.DeviceList[0];

        sm_RemoveDeviceProperty(device);
    }

    // removes stored GroupList

    for (g = 0; g < sDevConfig.GroupCount; g++)
    {
        if (sDevConfig.GroupList[g])
        {
            free(sDevConfig.GroupList[g]);
            sDevConfig.GroupList[g] = 0;
        }
    }

    sDevConfig.GroupCount = 0;

    // removes storage files

    SM_RemoveStoredDeviceList();
    SM_RemoveJoinedDeviceList();
    remove(scFileGroupList);

    SM_ClearJoinedDeviceList();

    return true;
}
//-----------------------------------------------------------------------------
bool sm_QASetDeviceScanOption(const char* parameter)
{
    bool        result = false;
    int         once;
    char*       option;
    char*       value;
    int         number;
    char        buffer[128] = {0,};

    for (once=1; once; once--)
    {
        if (!parameter || !strlen(parameter))
            break;

        SAFE_STRCPY(buffer, parameter);

        option = strtok(buffer, scDelimiter);
        if (!option || !strlen(option))
            break;

        value = strtok(NULL, scDelimiter);
        if (!value || !strlen(value))
            break;

        if (strcmp("DeviceInfo", option) == 0)
        {
            if (strcmp("Minimum", value) == 0)
            {
                sDevConfig.DeviceScanOption.DeviceInfo = SCAN_MINIMUM;
                result = true;
                break;
            }
        }

        if (strcmp("CommandRetry", option) == 0)
        {
            sscanf(value, "%d", &number);

            sDevConfig.DeviceScanOption.CommandRetry = number;
            result = true;
            break;
        }

        if (strcmp("CommandRetryInterval", option) == 0)
        {
            sscanf(value, "%d", &number);

            sDevConfig.DeviceScanOption.CommandRetryInterval = number;
            result = true;
            break;
        }

        if (strcmp("LeaveRequest", option) == 0)
        {
            sscanf(value, "%d", &number);

            sDevConfig.DeviceScanOption.LeaveRequest = number;
            result = true;
            break;
        }
    }

    return result;
}
//-----------------------------------------------------------------------------
bool sm_QASetRulesOption(const char* parameter)
{
    bool        result = false;
    int         once;
    char*       option;
    char*       value;
    int         number;
    char        buffer[128] = {0,};

    for (once=1; once; once--)
    {
        if (!parameter || !strlen(parameter))
            break;

        SAFE_STRCPY(buffer, parameter);

        option = strtok(buffer, scDelimiter);
        if (!option || !strlen(option))
            break;

        value = strtok(NULL, scDelimiter);
        if (!value || !strlen(value))
            break;

        if (strcmp("TimeScale", option) == 0)
        {
            sscanf(value, "%d", &number);

            if ((0 < number) && (number <= 300))
            {
                sprintf(buffer, "echo BA_Scale:%d > /tmp/qa_config", number);
                system(buffer);

                result = true;
            }
            break;
        }
    }

    return result;
}

/*
static bool sm_GetDeviceAvailable(const SD_DeviceID* device_id)
{
    SM_Device device;
    bool result = false;
    int nAvailable = 0;

    if (!sm_FindDevice(device_id, &device))
        return result;

    nAvailable = device->Available;

    Debug("------------------------\n");
    Debug("sm_GetDeviceAvailable(%s, %d) \n", device_id->Data, nAvailable);

    if( nAvailable == SD_ALIVE || nAvailable == 0 )
    {
        result = true;
    }

    return result;
}
*/

int SM_GetDeviceAvailable(const SD_DeviceID* device_id)
{
    SM_Device device;

    Debug("%s \n", __func__);
    if (!sm_FindDevice(device_id, &device))
        return -1;

    return device->Available;
}


//-----------------------------------------------------------------------------
void SM_RemoveDeviceFromGroup(SM_Device device)
{
    if (sm_RemoveGroupDevice(device))
        sm_SaveGroupListToFile();
}

//-----------------------------------------------------------------------------
void SM_UpdateCapabilityValue(bool bAvailable, const SD_DeviceID* device_id, const SD_CapabilityID* capability_id, const char* value, unsigned event)
{
    SM_Capability   capability;
    SM_Device       device;
    SM_Group        group;
    int             i=0, cv=0, dc=0;
    int             once;
    bool            statusHasBeenChanged = false;

    for (once=1; once; once--)
    {
        if (!sm_FindCapability(capability_id, &capability))
            break;

        if (device_id->Type == SD_DEVICE_EUID)
        {
            DEBUG_LOG(ULOG_CORE, UL_DEBUG, "device_id->Type == SD_DEVICE_EUID....dp");
            if (!sm_FindDevice(device_id, &device))
                break;

            if (!sm_CheckDeviceCapability(device, capability, "W", &cv, false))
                break;

            //Debug("Update %s of %s \n", capability_id->Data, device_id->Data);
            if(value[0])
            {
                if( 0 != strcmp(value, device->CapabilityValue[cv]) )
                {
                    DEBUG_LOG(ULOG_CORE, UL_DEBUG, "update Capability value cv=%d....dp", cv);
                    statusHasBeenChanged = true;
                    SAFE_STRCPY(device->CapabilityValue[cv], value);
                }
            }

            if(bAvailable == true)
            {
                // We should send change notification only recovered from DEAD status.
                if(SD_DEAD == device->Available)
                {
                    statusHasBeenChanged = true;
                }
                device->Available = SD_ALIVE;
                device->AvailableCheckCount = 0;
            }
            else
            {
                // We should send change notification when changed into DEAD status.
                if(SD_DEAD != device->Available)
                {
                    statusHasBeenChanged = true;
                }
                device->Available = SD_DEAD;
            }
        }
        else if (device_id->Type == SD_GROUP_ID)
        {
            DEBUG_LOG(ULOG_CORE, UL_DEBUG, "device_id->Type == SD_GROUP_ID....dp");
            if (!sm_FindGroup(device_id, &group, true))
                break;

            if (!sm_CheckGroupCapability(group, capability, "W", &cv, true))
                break;

            if(value[0])
            {
                if( 0 != strcmp(value, group->CapabilityValue[cv]) )
                {
                    DEBUG_LOG(ULOG_CORE, UL_DEBUG, "update Capability value cv=%d....dp", cv);
                    statusHasBeenChanged = true;
                    SAFE_STRCPY(group->CapabilityValue[cv], value);

                    //Debug("Update %s of Group  %s \n", capability->Data, device_id->Data);
                    for(i=0; i<group->DeviceCount; i++)
                    {
                        device = (SM_Device)group->Device[i];
                        if (sm_CheckDeviceCapability(device, capability, "W", &dc, true))
                        {
                            SAFE_STRCPY(device->CapabilityValue[dc], value);
                        }
                    }
                }
            }
        }
        else
        {
            break;
        }

        // Link will send notification if there is status change, to reduce traffic.
        if( statusHasBeenChanged == true )
        {
            SM_SendEvent(bAvailable, device_id, capability_id, value, event);
        }
    }
}

//-----------------------------------------------------------------------------
void SM_UpdateSensorCapabilityValue(bool bAvailable, const SD_DeviceID* device_id, const SD_CapabilityID* capability_id, const char* value, unsigned event)
{
    SM_Capability   capability;
    SM_Device       device;
    SM_Group        group;
    int             i=0, cv=0, dc=0;
    int             once;
    bool            statusHasBeenChanged = false;

    for (once=1; once; once--)
    {
        if (!sm_FindCapability(capability_id, &capability))
            break;

        if (device_id->Type == SD_DEVICE_EUID)
        {
            if (!sm_FindDevice(device_id, &device))
                break;

            if (!sm_GetDeviceCapabilityIndex(device, capability, &cv, true))
                break;

            Debug("%s %s %s:%s \n",__func__, device_id->Data, capability_id->Data, value);
            if(value[0])
            {
                if( 0 != strcmp(value, device->CapabilityValue[cv]) )
                {
                    statusHasBeenChanged = true;
                    SAFE_STRCPY(device->CapabilityValue[cv], value);
                }
            }

            if(bAvailable == true)
            {
                // We should send change notification only recovered from DEAD status.
                if(SD_DEAD == device->Available)
                {
                    statusHasBeenChanged = true;
                }
                device->Available = SD_ALIVE;
                device->AvailableCheckCount = 0;
            }
            else
            {
                // We should send change notification when changed into DEAD status.
                if(SD_DEAD != device->Available)
                {
                    statusHasBeenChanged = true;
                }
                device->Available = SD_DEAD;
            }
        }
        else if (device_id->Type == SD_GROUP_ID)
        {
            if (!sm_FindGroup(device_id, &group, true))
                break;

            if (!sm_GetGroupCapabilityIndex(group, capability, &cv, true))
                break;

            if(value[0])
            {
                if( 0 != strcmp(value, group->CapabilityValue[cv]) )
                {
                    statusHasBeenChanged = true;
                    SAFE_STRCPY(group->CapabilityValue[cv], value);

                    //Debug("Update %s of Group  %s \n", capability->Data, device_id->Data);
                    for(i=0; i<group->DeviceCount; i++)
                    {
                        device = (SM_Device)group->Device[i];
                        if (sm_GetDeviceCapabilityIndex(device, capability, &dc, true))
                        {
                            //Debug("+-- Update %s of %s \n", capability->Data, device->EUID.Data);
                            SAFE_STRCPY(device->CapabilityValue[dc], value);
                        }
                    }
                }
            }
        }
        else
        {
            break;
        }

        // Link will send notification if there is status change, to reduce traffic.
        if( statusHasBeenChanged == true )
        {
            SM_SendEvent(bAvailable, device_id, capability_id, value, event);
        }
    }
}

int sm_GetSensorRSSIIndex(int lqiValue)
{
    int index = 0;

    if(lqiValue >= 0)
        index = 0;
    else if(lqiValue >= -30)
        index = 10;
    else if (lqiValue >= -40)
        index = 9;
    else if (lqiValue >= -50)
        index = 8;
    else if (lqiValue >= -60)
        index = 7;
    else if (lqiValue >= -70)
        index = 6;
    else if (lqiValue >= -80)
        index = 5;
    else if (lqiValue >= -90)
        index = 4;
    else if (lqiValue >= -100)
        index = 3;
    else if (lqiValue >= -110)
        index = 2;
    else
        index = 1;

    return index;
}


int sm_GetSensorVoltageIndex(int voltage)
{
    int index = 0;
    int tableCount = sizeof(volIndexTable) / sizeof(SensorVoltageIndex);
    int i = 0;

    if((voltage < 21) || (voltage > 45))
    {
        index = 0;
    }
    else if(voltage >= 31)
    {
        index = 10;
    }
    else
    {
        for(i=0; i<tableCount; i++)
        {
            if(volIndexTable[i].voltage == voltage)
            {
                index = volIndexTable[i].index;
                break;
            }
        }
    }
    return index;
}

void sm_Clear1stKeyPressBitOfKeyFOBs(void)
{
    SM_Device       device;
    SD_DeviceID     deviceID = {SD_DEVICE_EUID, {0}};
    SD_CapabilityID capabilityID = {SD_CAPABILITY_ID, {0}};
    int             d;

    SAFE_STRCPY(capabilityID.Data, sCapabilitySensorKeyPress);

    for (d = 0; d < sDevConfig.DeviceCount; d++)
    {
        device = sDevConfig.DeviceList[d];

        if(0 == strncmp(device->ModelCode, SENSOR_MODELCODE_FOB, strlen(SENSOR_MODELCODE_FOB)))
        {
            memset(deviceID.Data, 0x00, sizeof(deviceID.Data));
            SAFE_STRCPY(deviceID.Data, device->EUID.Data);

            SM_UpdateSensorCapabilityValue(true, &deviceID, &capabilityID, "0", SE_NONE);
        }
    }
}

//-----------------------------------------------------------------------------
static void sm_RegisterEvents(void)
{
    int nEvent = 0;

    sEventID = open_sysevent("sysevent_sdevents", &sEventToken);
    DEBUG_LOG(ULOG_CORE, UL_DEBUG, "sysevent_sdevents = %d, token = %d", sEventID, sEventToken);
    nEvent = sizeof(sd_events) / sizeof(sd_events[0]);
    register_sysevent(sEventID, sEventToken, sd_events, nEvent);
}

static void sm_UnregisterEvents(void)
{
    int nEvent = sizeof(sd_events) / sizeof(sd_events[0]);

    if( sEventID )
    {
        unregister_sysevent(sEventID, sEventToken, sd_events, nEvent);
        close_sysevent(sEventID, sEventToken);
    }
}

void SM_ZBCommand_Enque(int nControl, bool group, void* device, SD_Capability capability, char *value)
{
    struct ZBCommand *pZBCommand = (struct ZBCommand *)calloc(1, sizeof(struct ZBCommand));

    pZBCommand->nControl = nControl;
    pZBCommand->bGroup = group;
    if( group )
    {
        pZBCommand->group_device = (SM_GroupDevice)device;
    }
    else
    {
        pZBCommand->device = (SD_Device)device;
    }
    pZBCommand->capability = capability;

    if( value )
    {
        SAFE_STRCPY(pZBCommand->value, value);
    }

    queue_enq(g_pCmdQueue, pZBCommand);
}


static struct ZBCommand* sm_ZBCommand_Deque()
{
    if( queue_empty(g_pCmdQueue) )
    {
        return (struct ZBCommand *)NULL;
    }
    return (struct ZBCommand *)queue_deq(g_pCmdQueue);
}

static void sm_ZBCommand_Run(struct ZBCommand *pZBCommand)
{
    int dc = 0;
    int wait = 300 * 1000;

    if( pZBCommand )
    {
        if( (pZBCommand->bGroup == false) && (pZBCommand->nControl == ZB_CMD_GET) )
        {
            if( 0 == strcmp(pZBCommand->capability->CapabilityID.Data, sCapabilityOnOff) )
            {
                Debug("sm_CommandThread: SM_GetDeviceBasicCapabilityValue(%s:%s) \n",
                    pZBCommand->device->EUID.Data, pZBCommand->capability->CapabilityID.Data);

                SM_GetDeviceBasicCapabilityValue(pZBCommand->device, pZBCommand->capability);
            }
            else if( 0 == strcmp(pZBCommand->capability->CapabilityID.Data, sCapabilityLevel) ||
                    0 == strcmp(pZBCommand->capability->CapabilityID.Data, sCapabilityColorCtrl) ||
                    0 == strcmp(pZBCommand->capability->CapabilityID.Data, sCapabilityColorTemp) )
            {
                if( sm_CheckDeviceCapability(pZBCommand->device, pZBCommand->capability, "R", &dc, true) )
                {
                    if( !pZBCommand->device->CapabilityValue[dc][0] )
                    {
                        Debug("sm_CommandThread: SM_GetDeviceBasicCapabilityValue(%s:%s) \n",
                            pZBCommand->device->EUID.Data, pZBCommand->capability->CapabilityID.Data);
                        SM_GetDeviceBasicCapabilityValue(pZBCommand->device, pZBCommand->capability);
                    }
                }
            }
        }
        if( (pZBCommand->bGroup == false) && (pZBCommand->nControl == ZB_CMD_SET) )
        {
            Debug("sm_CommandThread: SM_SetDeviceBasicCapabilityValue(%s:%s, value=%s) \n",
                pZBCommand->device->EUID.Data, pZBCommand->capability->CapabilityID.Data, pZBCommand->value);
            SM_SetDeviceBasicCapabilityValue(pZBCommand->device, pZBCommand->capability, pZBCommand->value);
        }
        if( (pZBCommand->bGroup == true) && (pZBCommand->nControl == ZB_CMD_SET) )
        {
            if( pZBCommand->group_device )
            {
                Debug("sm_CommandThread: SM_SetGroupBasicCapabilityValue(%s:%s, value=%s) \n",
                    pZBCommand->group_device->Group->ID.Data,
                    pZBCommand->capability->CapabilityID.Data,
                    pZBCommand->value);
                SM_SetGroupBasicCapabilityValue(pZBCommand->group_device, pZBCommand->capability, pZBCommand->value);
                usleep(wait);
            }
            if( pZBCommand->group_device->DeviceSpecific )
            {
                free(pZBCommand->group_device->DeviceSpecific);
            }
            if( pZBCommand->group_device )
            {
                free(pZBCommand->group_device);
            }
        }
        free(pZBCommand);
    }
}


static void* sm_CommandThread(void *arg)
{
    struct ZBCommand *pZBCommand = NULL;

    while( sCommandRunning )
    {
        usleep(10000);
        pZBCommand = sm_ZBCommand_Deque();
        sm_ZBCommand_Run(pZBCommand);
    }

    pthread_exit(NULL);
}


static void sm_OnOff_Handler(SD_DeviceID *pDeviceID, SD_CapabilityID *pCapabilityID, char *pEventValue)
{
    SM_Device       device;
    SM_Capability   capability;

    SD_GroupID      group_id =  {SD_GROUP_ID,    {0}};

    SAFE_STRCPY(pCapabilityID->Data, sCapabilityOnOff);

    Debug("sm_EventHandler : device_id = %s, on_off_status = %s\n", pDeviceID->Data, pEventValue);

    pthread_mutex_lock(&sControl.Lock);

    // Check if this EUID is child of Group.
    if( sm_GetGroupID(pDeviceID, &group_id) )
    {
        SM_UpdateCapabilityValue(true, pDeviceID, pCapabilityID, pEventValue, SE_LOCAL + SE_REMOTE);
        SM_UpdateCapabilityValue(true, &group_id, pCapabilityID, pEventValue, SE_LOCAL + SE_REMOTE);
    }
    else
    {
        SM_UpdateCapabilityValue(true, pDeviceID, pCapabilityID, pEventValue, SE_LOCAL + SE_REMOTE);
    }

    if( sm_FindDevice(pDeviceID, &device) && sm_FindCapability(pCapabilityID, &capability) )
    {
        SM_UpdateDeviceCommandStatus(device, capability, FINISHED);
    }

    pthread_mutex_unlock(&sControl.Lock);
}

static void sm_Level_Handler(SD_DeviceID *pDeviceID, SD_CapabilityID *pCapabilityID, char *pEventValue)
{
    SM_Device       device;
    SM_Capability   capability;

    SD_GroupID      group_id =  {SD_GROUP_ID,    {0}};

    SAFE_STRCPY(pCapabilityID->Data, sCapabilityLevel);

    // Add transition time
    strncat(pEventValue, ":0", 2);

    Debug("sm_EventHandler : device_id = %s, zb_level_status = %s\n", pDeviceID->Data, pEventValue);

    pthread_mutex_lock(&sControl.Lock);

    // Check if this EUID is child of Group.
    if(sm_GetGroupID(pDeviceID, &group_id) )
    {
        SM_UpdateCapabilityValue(true, pDeviceID, pCapabilityID, pEventValue, SE_LOCAL + SE_REMOTE);
        SM_UpdateCapabilityValue(true, &group_id, pCapabilityID, pEventValue, SE_LOCAL + SE_REMOTE);
    }
    else
    {
        SM_UpdateCapabilityValue(true, pDeviceID, pCapabilityID, pEventValue, SE_LOCAL + SE_REMOTE);
    }

    if( sm_FindDevice(pDeviceID, &device) && sm_FindCapability(pCapabilityID, &capability) )
    {
        SM_UpdateDeviceCommandStatus(device, capability, FINISHED);
    }

    pthread_mutex_unlock(&sControl.Lock);
}

static void sm_ColorTemp_Handler(SD_DeviceID *pDeviceID, SD_CapabilityID *pCapabilityID, char *pEventValue)
{
    SM_Device       device;
    SM_Capability   capability;

    SD_GroupID      group_id =  {SD_GROUP_ID,    {0}};

    SAFE_STRCPY(pCapabilityID->Data, sCapabilityColorTemp);

    // Add transition time
    strncat(pEventValue, ":0", 2);

    pthread_mutex_lock(&sControl.Lock);

    // Check if this EUID is child of Group.
    if(sm_GetGroupID(pDeviceID, &group_id) == true)
    {
        SM_UpdateCapabilityValue(true, pDeviceID, pCapabilityID, pEventValue, SE_LOCAL + SE_REMOTE);
        SM_UpdateCapabilityValue(true, &group_id, pCapabilityID, pEventValue, SE_LOCAL + SE_REMOTE);
    }
    else
    {
        SM_UpdateCapabilityValue(true, pDeviceID, pCapabilityID, pEventValue, SE_LOCAL + SE_REMOTE);
    }

    if( sm_FindDevice(pDeviceID, &device) && sm_FindCapability(pCapabilityID, &capability) )
    {
        SM_UpdateDeviceCommandStatus(device, capability, FINISHED);
    }

    pthread_mutex_unlock(&sControl.Lock);
}

static void sm_ColorXY_Handler(SD_DeviceID *pDeviceID, SD_CapabilityID *pCapabilityID, char *pEventValue)
{
    int colorx = 0, colory = 0;
    int colorxy = 0;
    char noti_value[CAPABILITY_MAX_VALUE_LENGTH] = {0,};

    SM_Device       device;
    SM_Capability   capability;

    SD_GroupID      group_id =  {SD_GROUP_ID,    {0}};

    SAFE_STRCPY(pCapabilityID->Data, sCapabilityColorCtrl);

    if( pEventValue[0] )
    {
        if(sscanf(pEventValue, "%d", &colorxy))
        {
            colorx = colorxy & 0x0000FFFF;
            colory = (colorxy >> 16) & 0x0000FFFF;
            sprintf(noti_value, "%d:%d:0", colorx, colory);

            pthread_mutex_lock(&sControl.Lock);

            // Check if this EUID is child of Group.
            if(sm_GetGroupID(pDeviceID, &group_id) == true)
            {
                SM_UpdateCapabilityValue(true, pDeviceID, pCapabilityID, noti_value, SE_LOCAL + SE_REMOTE);
                SM_UpdateCapabilityValue(true, &group_id, pCapabilityID, noti_value, SE_LOCAL + SE_REMOTE);
            }
            else
            {
                SM_UpdateCapabilityValue(true, pDeviceID, pCapabilityID, noti_value, SE_LOCAL + SE_REMOTE);
            }

            if( sm_FindDevice(pDeviceID, &device) && sm_FindCapability(pCapabilityID, &capability) )
            {
                SM_UpdateDeviceCommandStatus(device, capability, FINISHED);
            }

            pthread_mutex_unlock(&sControl.Lock);
        }
    }
}

static void sm_Error_Handler(SD_DeviceID *pDeviceID, SD_CapabilityID *pCapabilityID, char *pEventValue, char *pClusterID)
{
    int dc = 0;
    // issue fixed: cache was filled with "03" if it gets "zb_error_status -3 6"
    char value[CAPABILITY_MAX_VALUE_LENGTH] = {0,};

    SM_Device       device;
    SM_Capability   capability;

    if( pClusterID[0] )
    {
        int nCapabilityID = (CAPABILITY_ID_ZIGBEE | atoi(pClusterID));
        snprintf(pCapabilityID->Data, sizeof(pCapabilityID->Data), "%x", nCapabilityID);
        Debug("sm_EventHandler : zb_error_status, device_id = %s, capability_id = %s \n",
                pDeviceID->Data, pCapabilityID->Data);
    }
    else
    {
        SAFE_STRCPY(pCapabilityID->Data, sCapabilityOnOff);
    }

    /* Check if Bulb OTA is true, ZB error would be ignored */
    if( (NULL != ota_device_id) &&
        ( 0 == strcmp(pDeviceID->Data, ota_device_id->Data)) )
    {
        Debug("sm_EventHandler : zb_error_status is IGNORED for bulb OTA, device_id = %s, error_status = %s\n",
                pDeviceID->Data, pEventValue);
        return;
    }

    // if sensors, then return. 
    if(SD_IsSensor(pDeviceID) == true)
    {
        Debug("sensor error: we will not handle this\n");
        return;
    }

    if( sm_FindDevice(pDeviceID, &device) )
    {
        if( 0 == strcmp(pEventValue, "0") )
        {
            //Get OnOff Command
            memset(pCapabilityID->Data, 0x00, sizeof(pCapabilityID->Data));
            SAFE_STRCPY(pCapabilityID->Data, sCapabilityOnOff);
            sm_FindCapability(pCapabilityID, &capability);

            SM_ZBCommand_Enque(ZB_CMD_GET, false, device, capability, NULL);

            Debug("sm_EventHandler : ZB recovered, send async cmd again.\n");
        }
        else if( 0 == strcmp(pEventValue, "-3") )
        {
            Debug("sm_EventHandler : ZB error handling...\n");

            //
            // WEMO-39103: Link should send GetDeviceStatus 10006 internally when it recovered from temp. ZB error:
            //             This code block has been moved from above.
            //
            // We will handle only these: 10006, 10008, 10300, 30301
            // - Ajay reported this issue by email at 3/24/2015 19:44 KST.
            // - Issue: Right after pairing some bulbs show up as Unavailable.
            // - Root cause: After pairing, some bulbs sends ZB error for 10003(Identify),
            //   We should ignore this error case.
            if(  (0 != strcmp(pCapabilityID->Data, sCapabilityOnOff)) &&
                 (0 != strcmp(pCapabilityID->Data, sCapabilityLevel)) &&
                 (0 != strcmp(pCapabilityID->Data, sCapabilityColorCtrl)) &&
                 (0 != strcmp(pCapabilityID->Data, sCapabilityColorTemp)) )
            {
                return;
            }

            ++device->AvailableCheckCount;

            if( sm_FindCapability(pCapabilityID, &capability) )
            {
                SM_UpdateDeviceCommandStatus(device, capability, FINISHED);

                if( sm_CheckDeviceCapability(device, capability, "R", &dc, true) )
                {
                    if( device->CapabilityValue[dc][0] )
                    {
                        SAFE_STRCPY(value, device->CapabilityValue[dc]);
                    }
                    else
                    {
                        SAFE_STRCPY(value, "0");
                    }
                }
                else
                {
                    SAFE_STRCPY(value, "0");
                }

                if( device->AvailableCheckCount == 1 )
                {
                    pthread_mutex_lock(&sControl.Lock);
                    SM_UpdateCapabilityValue(false, pDeviceID, pCapabilityID, value, SE_LOCAL + SE_REMOTE);
                    pthread_mutex_unlock(&sControl.Lock);
                    Debug("sm_EventHandler : UPnP notification with Unavailable device...\n");
                }

                if( device->AvailableCheckCount < AVAILABLE_CHECK_LIMIT )
                {
                    SM_ZBCommand_Enque(ZB_CMD_GET, false, device, capability, NULL);
                }
                else if( device->AvailableCheckCount >= AVAILABLE_CHECK_LIMIT )
                {
                    device->Available = SD_DEAD;
                }
            }

            Debug("sm_EventHandler : device[%s] : AvailableCheckCount = [%d] \n",
                    pDeviceID->Data, device->AvailableCheckCount);
        }
    }
}


static void sm_ZoneStatus_Handler(SD_DeviceID *pDeviceID, SD_CapabilityID *pCapabilityID, char *pEventValue, char *pZoneID)
{
    SM_Device   device;
    int   capCashValue = 0;
    char  capCashStr[CAPABILITY_MAX_VALUE_LENGTH] = {0,};
    int   eventValueInt = 0;
    int   eventValueIntTemp = 0;
    int   eventValueMixed = 0;
    int   eventValueCleared = 0;
    char  eventValueMixedStr[CAPABILITY_MAX_VALUE_LENGTH] = {0,};
    char  eventValueClearedStr[CAPABILITY_MAX_VALUE_LENGTH] = {0,};

    bool  eventMainDetected = false;
    bool  eventSubDetected = false;

    unsigned long nowUtcTime = 0;
    unsigned long timeDiff = 0;

    DEBUG_LOG(ULOG_CORE, UL_DEBUG, "ZoneStatus_Handler");

    if( sm_FindDevice(pDeviceID, &device) )
    {
         DEBUG_LOG(ULOG_CORE, UL_DEBUG, "inside sm_FindDevice...dp");
        SAFE_STRCPY(pCapabilityID->Data, sCapabilitySensor);

        // Get previous 10500 value: It already contains LQI, Voltage and event status.
        if( SD_GetCapabilityValue(pDeviceID, pCapabilityID, capCashStr, SD_CACHE) )
        {
            // string to int
            sscanf(capCashStr, "%x", &capCashValue);
        }

        // Get the event value(bit 1,0) and Low battery alert(bit 3): xxxx xxxx xxxx 1x11
        sscanf(pEventValue, "%x", &eventValueIntTemp);
        eventValueInt = eventValueIntTemp & (IASZONE_BIT_SUB_EVENT | IASZONE_BIT_MAIN_EVENT);

        // Clear bit1 bit0. Keep Low battery alert bit.
        // Clear bit4: D/W initial status bit.
        eventValueCleared = capCashValue & ~(IASZONE_BIT_INITIAL_STATUS | IASZONE_BIT_SUB_EVENT | IASZONE_BIT_MAIN_EVENT );

        // Combine LQI | Voltage | Event
        eventValueMixed = eventValueCleared | eventValueInt;
        snprintf(eventValueMixedStr, sizeof(eventValueMixedStr), "%x", eventValueMixed);
        snprintf(eventValueClearedStr, sizeof(eventValueClearedStr), "%x", eventValueCleared);

        nowUtcTime = GetUTCTime();
        if(nowUtcTime > UTC_2015_1_1_GMT)
        {
        timeDiff = nowUtcTime - device->LastReportedTime;
        }
        else
        {
            nowUtcTime = 0;
        }

        // check main/sub event bit
        if(eventValueInt & IASZONE_BIT_MAIN_EVENT)
        {
            eventMainDetected = true;
        }

        if(eventValueInt & IASZONE_BIT_SUB_EVENT)
        {
            eventSubDetected = true;
        }

        // Cache update policy
        //  - Key FOB: Key press event will be cleared right after being triggered.
        //  - PIR, Alarm: Event will be cleared by "0" event from PIR/Alarm Sensor.
        //  - D/W: Event status will be maintained until next event comming.
        //
        // For PIR, we will not report to App and Cloud no more than every xx mins.
        //
        if(0 == strncmp(device->ModelCode, SENSOR_MODELCODE_PIR, strlen(SENSOR_MODELCODE_PIR)))
        {
                if(eventMainDetected == true)
                {
                    if(timeDiff >= PIR_REPORT_INTERVAL)
                    {
                        //
                        // Update to cache.
                        //
                        SM_UpdateSensorCapabilityValue(true, pDeviceID, pCapabilityID, eventValueMixedStr, SE_NONE);

                        //
                        // Sending PIR sensed event
                        //
                        device->LastReportedTime = nowUtcTime;
                        SM_SendEvent(true, pDeviceID, pCapabilityID, "1", SE_LOCAL + SE_REMOTE);
                    }
                    else
                    {
                        DEBUG_LOG(ULOG_CORE, UL_DEBUG, "PIR Sensed but reporting time is not coming yet. Ignore this event. diff:%d", timeDiff);
                    }
                }
                else
                {
                    // event value is 0 --> Motion off
                    //
                    // Update to cache.
                    //
                    SM_UpdateSensorCapabilityValue(true, pDeviceID, pCapabilityID, eventValueMixedStr, SE_NONE);

                    //
                    // Sending PIR cleared event
                    //
                    if((capCashValue & IASZONE_BIT_MAIN_EVENT) != 0)
                    {
                        SM_SendEvent(true, pDeviceID, pCapabilityID, "0", SE_LOCAL + SE_REMOTE);
                    }
                }
        }
        else if(0 == strncmp(device->ModelCode, SENSOR_MODELCODE_FOB, strlen(SENSOR_MODELCODE_FOB)))
        {
            if((capCashValue & 0x01) == 0)
            {
                //  We will assume this as Key FOB has been arrived.
                eventValueMixed = eventValueCleared | IASZONE_BIT_MAIN_EVENT;
                memset(eventValueMixedStr, 0x00, sizeof(eventValueMixedStr));
                snprintf(eventValueMixedStr, sizeof(eventValueMixedStr), "%x", eventValueMixed);

                //
                // Update to Cache
                //
                SM_UpdateSensorCapabilityValue(true, pDeviceID, pCapabilityID, eventValueMixedStr, SE_NONE);

                //
                // Send event
                //
                device->LastReportedTime = nowUtcTime;
                SM_SendEvent(true, pDeviceID, pCapabilityID, "1", SE_LOCAL + SE_REMOTE);
            }

            if( (eventMainDetected || eventSubDetected))
            {
                int  tempEventValue = 0;
                int  tempCacheValue = 0;
                char tempEventStr[CAPABILITY_MAX_VALUE_LENGTH] = {0,};

                device->LastReportedTimeExt = nowUtcTime;

                memset(pCapabilityID->Data, 0x00, sizeof(pCapabilityID->Data));
                SAFE_STRCPY(pCapabilityID->Data, sCapabilitySensorKeyPress);

                memset(capCashStr, 0x00, sizeof(capCashStr));
                if( SD_GetCapabilityValue(pDeviceID, pCapabilityID, capCashStr, SD_CACHE) )
                {
                    // string to int
                    sscanf(capCashStr, "%x", &tempCacheValue);
                }

                if(tempCacheValue & IASZONE_BIT_INITIAL_STATUS)
                {
                    tempEventValue |= IASZONE_BIT_INITIAL_STATUS;
                }

                if(eventMainDetected == true)
                    tempEventValue |= IASZONE_BIT_MAIN_EVENT;

                if(eventSubDetected == true)
                    tempEventValue |= IASZONE_BIT_SUB_EVENT;

                snprintf(tempEventStr, sizeof(tempEventStr), "%x", tempEventValue);

                //
                // Send event
                //
                SM_SendEvent(true, pDeviceID, pCapabilityID, tempEventStr, SE_LOCAL + SE_REMOTE);

                //
                // Clear event value right after sending status change notification, for the next event.
                //
                SM_UpdateSensorCapabilityValue(true, pDeviceID, pCapabilityID, "0", SE_NONE);

                if(tempEventValue & IASZONE_BIT_INITIAL_STATUS)
                {
                    // Then clear 1st Key pressed bit of other Key FOBs
                    sm_Clear1stKeyPressBitOfKeyFOBs();
                }
            }
        }
        else if(0 == strncmp(device->ModelCode, SENSOR_MODELCODE_DW, strlen(SENSOR_MODELCODE_DW)))
        {
            //
            // Update to cache.
            //
            SM_UpdateSensorCapabilityValue(true, pDeviceID, pCapabilityID, eventValueMixedStr, SE_NONE);

            //
            // Sending event: Always for initial status.
            //
            if( (capCashValue & IASZONE_BIT_INITIAL_STATUS) ||
                ((capCashValue & IASZONE_BIT_MAIN_EVENT) != (eventValueInt & IASZONE_BIT_MAIN_EVENT)) ) 
            {
                device->LastReportedTime = nowUtcTime;
                if(eventMainDetected)
                {
                    SM_SendEvent(true, pDeviceID, pCapabilityID, "1", SE_LOCAL + SE_REMOTE);
                }
                else
                {
                    SM_SendEvent(true, pDeviceID, pCapabilityID, "0", SE_LOCAL + SE_REMOTE);
                }
            }
        }
        else if(0 == strncmp(device->ModelCode, SENSOR_MODELCODE_ALARM, strlen(SENSOR_MODELCODE_ALARM)))
        {
            //
            // WEMO-42263: Android: Status message changes from Alarm Detected to All quiet, once the alarm beep stops.
            //
            if(eventMainDetected == true)
            {
                unsigned long reservedTime;

                device->LastReportedTime = nowUtcTime;

                //
                // Update to cache.
                //
                SM_UpdateSensorCapabilityValue(true, pDeviceID, pCapabilityID, eventValueMixedStr, SE_NONE);

                SM_SendEvent(true, pDeviceID, pCapabilityID, "1", SE_LOCAL + SE_REMOTE);

                //
                // Reserving Alarm alert
                //
                reservedTime = nowUtcTime + SENSOR_ALARM_ALERT_INTERVAL;
                Debug("Alarm alert reserved at: %d, diff:%d\n", reservedTime, SENSOR_ALARM_ALERT_INTERVAL);

                SM_ReserveCapabilityUpdate(reservedTime, pDeviceID, pCapabilityID, "1", SE_LOCAL + SE_REMOTE);
            }
            else
            {
                //
                // Update to cache.
                //
                SM_UpdateSensorCapabilityValue(true, pDeviceID, pCapabilityID, eventValueMixedStr, SE_NONE);

                if((capCashValue & IASZONE_BIT_MAIN_EVENT) != 0)
                {
                    SM_SendEvent(true, pDeviceID, pCapabilityID, "0", SE_LOCAL + SE_REMOTE);
                }
            }
        }
        else
        {
            DEBUG_LOG(ULOG_CORE, UL_DEBUG, "IAS zone status event ignored, ModelCode :%s", device->ModelCode);
        }
    }
}

static void sm_NodeRssi_Handler(SD_DeviceID *pDeviceID, SD_CapabilityID *pCapabilityID, char *pEventValue)
{
    SM_Device   device;
    int rssiValue = 0;
    int rssiIndex = 0;

    int cacheValueInt = 0;
    int mixedValueInt = 0;
    int notiValueInt = 0;

    unsigned long nowUtcTime = 0;

    char cacheValueStr[CAPABILITY_MAX_VALUE_LENGTH] = {0,};
    char mixedValueStr[CAPABILITY_MAX_VALUE_LENGTH] = {0,};
    char notiValueStr[CAPABILITY_MAX_VALUE_LENGTH] = {0,};

    if(pEventValue)
    {
        sscanf(pEventValue, "%d", &rssiValue);
    }

    if( sm_FindDevice(pDeviceID, &device) == false )
    {
        return;
    }

    nowUtcTime = GetUTCTime();
    if(nowUtcTime < UTC_2015_1_1_GMT)     // Invalid if UTC time is earlier than 2015.1.1 GMT
        nowUtcTime = 0;
    if(device->LastReportedTime == 0)
    {
        device->LastReportedTime = nowUtcTime;
    }
    if(device->LastReportedTimeExt == 0)
    {
        device->LastReportedTimeExt = nowUtcTime;
    }

    rssiIndex = sm_GetSensorRSSIIndex(rssiValue);
    // 0 : meaningless value
    if(rssiIndex == 0) 
    {
        return;
    }

    SAFE_STRCPY(pCapabilityID->Data, sCapabilitySensor);

    //Get 10500 capability
    if(SD_GetCapabilityValue(pDeviceID, pCapabilityID, cacheValueStr, SD_CACHE))
    {
        sscanf(cacheValueStr, "%x", &cacheValueInt);
    }

    if(rssiIndex <= LOW_SIGNAL_THRESHOLD_INDEX)
    {
        if(cacheValueInt & IASZONE_BIT_LOW_SIGNAL)
        {
            // Notification value
            notiValueInt = (rssiIndex << IASZONE_BITSHIFT_RSSI_LEVEL);

            // Value to be update cache 
            mixedValueInt = cacheValueInt & ~(IASZONE_BIT_LQI_LEVEL | IASZONE_BIT_INITIAL_STATUS);
            mixedValueInt |= (rssiIndex << IASZONE_BITSHIFT_RSSI_LEVEL);
        }
        else
        {
            // Notification value
            notiValueInt = IASZONE_BIT_LOW_SIGNAL;

            // Value to be update cache 
            mixedValueInt = cacheValueInt & ~(IASZONE_BIT_LQI_LEVEL | IASZONE_BIT_INITIAL_STATUS);
            mixedValueInt |= (rssiIndex << IASZONE_BITSHIFT_RSSI_LEVEL);
            mixedValueInt |= IASZONE_BIT_LOW_SIGNAL;
        }
    }
    else
    {
        // Notification value
        notiValueInt = (rssiIndex << IASZONE_BITSHIFT_RSSI_LEVEL);

        // Value to be update cache 
        mixedValueInt = cacheValueInt & ~(IASZONE_BIT_LQI_LEVEL | IASZONE_BIT_INITIAL_STATUS | IASZONE_BIT_LOW_SIGNAL);
        mixedValueInt |= (rssiIndex << IASZONE_BITSHIFT_RSSI_LEVEL);
    }

    // int to string
    snprintf(notiValueStr, sizeof(notiValueStr), "%x", notiValueInt);
    snprintf(mixedValueStr, sizeof(mixedValueStr), "%x", mixedValueInt);

    //
    // Update to Cache
    //
    SM_UpdateSensorCapabilityValue(true, pDeviceID, pCapabilityID, mixedValueStr, SE_NONE);

    //
    // Send event
    //
    SM_SendEvent(true, pDeviceID, pCapabilityID, notiValueStr, SE_LOCAL + SE_REMOTE);
}

static void sm_NodeVoltage_Handler(SD_DeviceID *pDeviceID, SD_CapabilityID *pCapabilityID, char *pEventValue)
{
    SM_Device   device;

    int voltageValue = 0;
    int voltageIndex = 0;
    bool sendLowBatteryAlert = false;

    int cacheValueInt = 0;
    int mixedValueInt = 0;
    int notiValueInt = 0;

    unsigned long nowUtcTime = 0;

    char cacheValueStr[CAPABILITY_MAX_VALUE_LENGTH] = {0,};
    char mixedValueStr[CAPABILITY_MAX_VALUE_LENGTH] = {0,};
    char notiValueStr[CAPABILITY_MAX_VALUE_LENGTH] = {0,};

    Debug("%s %s \n",__func__, pEventValue);

    // Change pVoltage into int value
    if( pEventValue )
    {
        sscanf(pEventValue, "%d", &voltageValue);
    }

    if( sm_FindDevice(pDeviceID, &device) == false )
    {
        return;
    }

    nowUtcTime = GetUTCTime();
    if(nowUtcTime < UTC_2015_1_1_GMT)     // Invalid if UTC time is earlier than 2015.1.1 GMT
        nowUtcTime = 0;
    if(device->LastReportedTime == 0)
    {
        device->LastReportedTime = nowUtcTime;
    }
    if(device->LastReportedTimeExt == 0)
    {
        device->LastReportedTimeExt = nowUtcTime;
    }

    // Get table index
    voltageIndex = sm_GetSensorVoltageIndex(voltageValue);

    // 0 : meaningless value
    if(voltageIndex == 0)
    {
        return;
    }

    SAFE_STRCPY(pCapabilityID->Data, sCapabilitySensor);

    //Get 10500 capability
    if(SD_GetCapabilityValue(pDeviceID, pCapabilityID, cacheValueStr, SD_CACHE))
    {
        sscanf(cacheValueStr, "%x", &cacheValueInt);
    }

    // if this is the first Low battery alert, we should send it to App and Cloud
    // Low battery threshold: 2.3V
    if( voltageIndex <= LOW_BATTERY_THRESHOLD_INDEX) 
    {
        if(cacheValueInt & IASZONE_BIT_LOW_BATTERY)
        {
            // Notification value
            notiValueInt = (voltageIndex << IASZONE_BITSHIFT_BATTERY_LEVEL);

            // Value to be update cache 
            mixedValueInt = cacheValueInt & ~(IASZONE_BIT_BATTERY_LEVEL | IASZONE_BIT_INITIAL_STATUS);
            mixedValueInt |= (voltageIndex << IASZONE_BITSHIFT_BATTERY_LEVEL);
        }
        else
        {
            sendLowBatteryAlert = true;
            
            // Notification value
            notiValueInt = IASZONE_BIT_LOW_BATTERY;

            // Value to be update cache 
            mixedValueInt = cacheValueInt & ~(IASZONE_BIT_BATTERY_LEVEL | IASZONE_BIT_INITIAL_STATUS);
            mixedValueInt |= (voltageIndex << IASZONE_BITSHIFT_BATTERY_LEVEL);
            mixedValueInt |= IASZONE_BIT_LOW_BATTERY;
        }
    }
    else
    {
        // Notification value
        notiValueInt = (voltageIndex << IASZONE_BITSHIFT_BATTERY_LEVEL);

        // Value to be update cache 
        mixedValueInt = cacheValueInt & ~(IASZONE_BIT_BATTERY_LEVEL | IASZONE_BIT_INITIAL_STATUS | IASZONE_BIT_LOW_BATTERY);
        mixedValueInt |= (voltageIndex << IASZONE_BITSHIFT_BATTERY_LEVEL);
    }

    // int to string
    snprintf(notiValueStr, sizeof(notiValueStr), "%x", notiValueInt);
    snprintf(mixedValueStr, sizeof(mixedValueStr), "%x", mixedValueInt);

    //
    // Update to Cache
    //
    SM_UpdateSensorCapabilityValue(true, pDeviceID, pCapabilityID, mixedValueStr, SE_NONE);

    //
    // Send event
    //
    SM_SendEvent(true, pDeviceID, pCapabilityID, notiValueStr, SE_LOCAL + SE_REMOTE);

    if(sendLowBatteryAlert == true)
    {
        unsigned long updateTime;
        unsigned long interval = SENSOR_LOW_BATTERY_ALERT_INTERVAL;
    
        updateTime = nowUtcTime + interval;
        Debug("Low Battery alert reserved at: %d, diff:%d\n", updateTime, interval);

        SM_ReserveCapabilityUpdate(updateTime, pDeviceID, pCapabilityID, notiValueStr, SE_LOCAL + SE_REMOTE);
    }
}

static void sm_NodePresence_Handler(SD_DeviceID *pDeviceID, SD_CapabilityID *pCapabilityID, char *pEventValue)
{
    SM_Device   device;
    int cacheValueInt = 0;
    int mixedValueInt = 0;
    int eventValueInt = 0;

    unsigned long nowUtcTime = 0;

    char cacheValueStr[CAPABILITY_MAX_VALUE_LENGTH] = {0,};
    char mixedValueStr[CAPABILITY_MAX_VALUE_LENGTH] = {0,};
    char eventValueStr[CAPABILITY_MAX_VALUE_LENGTH] = {0,};


    Debug("%s\n", __func__);

    if( sm_FindDevice(pDeviceID, &device) == false )
    {
        return;
    }

    if(pEventValue)
    {
        sscanf(pEventValue, "%d", &eventValueInt);
    }

    SAFE_STRCPY(pCapabilityID->Data, sCapabilitySensor);
    if( SD_GetCapabilityValue(pDeviceID, pCapabilityID, cacheValueStr, SD_CACHE) )
    {
        // string to int
        sscanf(cacheValueStr, "%x", &cacheValueInt);
    }

    if(0 == strncmp(device->ModelCode, SENSOR_MODELCODE_FOB, strlen(SENSOR_MODELCODE_FOB)))
    {
        nowUtcTime = GetUTCTime();
        if(nowUtcTime < UTC_2015_1_1_GMT)     // Invalid if UTC time is earlier than 2015.1.1 GMT
            nowUtcTime = 0;

        // Clear initial status bit and main event bit of cache value, and then...
        mixedValueInt = (cacheValueInt & ~(IASZONE_BIT_INITIAL_STATUS | IASZONE_BIT_MAIN_EVENT)) | eventValueInt;

        // int to string
        snprintf(eventValueStr, sizeof(eventValueStr), "%x", eventValueInt);
        snprintf(mixedValueStr, sizeof(mixedValueStr), "%x", mixedValueInt);

        //
        // Update to Cache
        //
        SM_UpdateSensorCapabilityValue(true, pDeviceID, pCapabilityID, mixedValueStr, SE_NONE);

        //
        // Sending event: Always for initial status.
        //
        if( (cacheValueInt & IASZONE_BIT_INITIAL_STATUS) ||
            ((cacheValueInt & IASZONE_BIT_MAIN_EVENT) != (eventValueInt & IASZONE_BIT_MAIN_EVENT)) ) 
        {
            device->LastReportedTime = nowUtcTime;
            SM_SendEvent(true, pDeviceID, pCapabilityID, eventValueStr, SE_LOCAL + SE_REMOTE);
        }
    }
    else
    {
        if(eventValueInt == 0)
        {
            SM_UpdateSensorCapabilityValue(false, pDeviceID, pCapabilityID, cacheValueStr, SE_LOCAL + SE_REMOTE);
        }
        else
        {
            SM_UpdateSensorCapabilityValue(true, pDeviceID, pCapabilityID, cacheValueStr, SE_LOCAL + SE_REMOTE);
        }
    }
}


static void* sm_EventHandler(void *arg)
{
    int rc = 0;
    int name_len = 0, value_len = 0;

    char name[CAPABILITY_MAX_VALUE_LENGTH] = {0,};
    char value[CAPABILITY_MAX_VALUE_LENGTH] = {0,};
    char event_value[CAPABILITY_MAX_VALUE_LENGTH] = {0,};
    char cluster_id[CAPABILITY_MAX_VALUE_LENGTH] = {0,};
    char zone_id[10] = {0,};

    char *tofree, *cloneString;
    char *pEUID, *pEventValue, *pParameterID;

    SD_DeviceID     device_id = {SD_DEVICE_EUID, {0}};
    SD_CapabilityID capability_id = {SD_CAPABILITY_ID, {0}};

    while( sEventRunning )
    {
        name_len = sizeof(name);
        value_len = sizeof(value);

        rc = wait_nonblock_sysevent(sEventID, sEventToken, name, &name_len, value, &value_len);

        if( rc == 0 )
        {
            Debug("sm_EventHandler : Zigbee event received, raw string: %s %s\n", name, value);

            tofree = cloneString = strdup(value);

            // Extract token with " " delimmiter
            pEUID         = strsep(&cloneString, " ");
            pEventValue   = strsep(&cloneString, " ");
            pParameterID  = strsep(&cloneString, " ");

            memset(device_id.Data,      0x00, sizeof(device_id.Data));
            memset(capability_id.Data,  0x00, sizeof(capability_id.Data));
            memset(event_value,         0x00, sizeof(event_value));
            memset(cluster_id,          0x00, sizeof(cluster_id));
            memset(zone_id,             0x00, sizeof(zone_id));

            if( pEUID )
                SAFE_STRCPY(device_id.Data, pEUID);

            if( pEventValue )
                SAFE_STRCPY(event_value, pEventValue);

            if( pParameterID )
            {
              if( 0 == strcmp(name, "zb_error_status") )
              {
                  SAFE_STRCPY(cluster_id, pParameterID);
              }
              else if( 0 == strcmp(name, "zb_zone_status") )
              {
                  SAFE_STRCPY(zone_id, pParameterID);
              }
            }


            if( 0 == strcmp(name, "zb_onoff_status") )
            {
                sm_OnOff_Handler(&device_id, &capability_id, event_value);
            }
            else if( 0 == strcmp(name, "zb_level_status") )
            {
                sm_Level_Handler(&device_id, &capability_id, event_value);
            }
            else if( 0 == strcmp(name, "zb_color_temp") )
            {
                sm_ColorTemp_Handler(&device_id, &capability_id, event_value);
            }
            else if( 0 == strcmp(name, "zb_color_xy") )
            {
                sm_ColorXY_Handler(&device_id, &capability_id, event_value);
            }
            else if( 0 == strcmp(name, "zb_error_status") )
            {
                sm_Error_Handler(&device_id, &capability_id, event_value, cluster_id);
            }
            else if( 0 == strcmp(name, "zb_zone_status") )
            {
                sm_ZoneStatus_Handler(&device_id, &capability_id, event_value, zone_id);
                checkAndExecuteSensorRule(device_id.Data, atoi(capability_id.Data), event_value);
            }
            else if( 0 == strcmp(name, "zb_node_rssi") )
            {
                sm_NodeRssi_Handler(&device_id, &capability_id, event_value);
            }
            else if( 0 == strcmp(name, "zb_node_voltage") )
            {
                sm_NodeVoltage_Handler(&device_id, &capability_id, event_value);
            }
            else if( 0 == strcmp(name, "zb_node_present") )
            {
                sm_NodePresence_Handler(&device_id, &capability_id, event_value);
		/*For FOB Arrive/Leave*/
                checkAndExecuteSensorRule(device_id.Data, atoi(capability_id.Data), event_value);
            }
            else if( 0 == strcmp(name, "zb_save_status") )
            {
                SD_SaveSensorPropertyToFile();
            }
            else
            {
                Debug("sm_EventHandler : Unknown Event:%s \n", name);
            }

            free(tofree);
        }
    }

    pthread_exit(NULL);
}

bool sm_CreateThreadJob(void)
{
    bool event_result = false;
    bool command_result = false;

    DEBUG_LOG(ULOG_CORE, UL_DEBUG, "call sm_RegisterEvents() in sm_CreateThreadJob()");

    sm_RegisterEvents();

    g_pCmdQueue = queue_init();
    queue_limit(g_pCmdQueue, 300);

    if( 0 == pthread_create(&sEventThreadID, NULL, sm_EventHandler, NULL) )
    {
        pthread_detach(sEventThreadID);
        event_result = true;
    }

    if( 0 == pthread_create(&sCommandThreadID, NULL, sm_CommandThread, NULL) )
    {
        pthread_detach(sCommandThreadID);
        command_result = true;
    }

    return (event_result && command_result);
}

void sm_DestroyThreadJob(void)
{
    if( sEventThreadID )
    {
        //Stop and destroy sensor thread handler...
        sEventRunning = false;
        sEventThreadID = 0;
    }

    if( sCommandThreadID )
    {
        //Stop and destroy sensor thread handler...
        sCommandRunning = false;
        sCommandThreadID = 0;
    }

    queue_destroy(g_pCmdQueue);

    sm_UnregisterEvents();
}


//-----------------------------------------------------------------------------
//
//      Module
//
//-----------------------------------------------------------------------------
bool SD_OpenInterface()
{
    bool            result = false;

    Debug("OpenInterface()\n");

    result = sm_LoadConfiguration();

    if (!result)
    {
        Debug("OpenInterface() failed \n");
        DEBUG_LOG(ULOG_CORE, UL_DEBUG, "OpenInterface() failed...");
    }

    system("touch /tmp/Belkin_settings/pristine_zc");

    return result;
}

//-----------------------------------------------------------------------------
void SD_CloseInterface()
{
    Debug("CloseInterface()\n");

    sm_FreeConfiguration();
}

bool SD_CreateThreadJob()
{
    bool result = false;

    result = sm_CreateThreadJob();

    if( !result )
    {
        DEBUG_LOG(ULOG_CORE, UL_DEBUG, "sm_CreateThreadJob() failed...");
    }
    else
    {
        DEBUG_LOG(ULOG_CORE, UL_DEBUG, "sm_CreateThreadJob() success...");
    }

    return result;
}

void SD_DestroyThreadJob()
{
    sm_DestroyThreadJob();
}

//-----------------------------------------------------------------------------
bool SD_NetworkForming()
{
    return SM_NetworkForming();
}

//-----------------------------------------------------------------------------
//
//      Capability
//
//-----------------------------------------------------------------------------
int SD_GetCapabilityList(SD_CapabilityList* list)
{
    if (!sControl.Configured)
        return 0;

   if (!list)
        return 0;

    *list = (SD_CapabilityList)sDevConfig.CapabilityList;

    return sDevConfig.CapabilityCount;
}
//-----------------------------------------------------------------------------
bool SD_GetCapability(const SD_CapabilityID* id, SD_Capability* capability)
{
    if (!sControl.Configured)
        return false;

    if (sm_FindCapability(id, (SM_Capability*)capability))
        return true;

    Debug("GetCapability() failed \n");
    return false;
}
//-----------------------------------------------------------------------------
bool SD_SetCapabilityRegisteredID(const SD_CapabilityID* capability_id, const SD_CapabilityID* registered_id)
{
    bool            result = false;
    SM_Capability   capability;
    int             once;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!registered_id)
            break;

        if (!sm_FindCapability(capability_id, &capability))
            break;

        Debug("SetCapabilityRegisteredID(%s,%s)\n", capability_id->Data, registered_id->Data);

        capability->RegisteredID = *registered_id;
        capability->RegisteredID.Data[CAPABILITY_MAX_ID_LENGTH-1] = 0;

        if (!SM_SaveCapabilityListToFile(&sDevConfig, true))
            break;

        result = true;
    }

    if (!result)
        Debug("SetCapabilityRegisteredID() failed \n");

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}
//-----------------------------------------------------------------------------
//
//      Device
//
//-----------------------------------------------------------------------------
bool SD_OpenNetwork()
{
    bool result = false;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    Debug("OpenNetwork()\n");

    if (SM_PermitJoin(255))
    {
        sControl.NetworkOpened = time(0);
        result = true;
    }

    if (!result)
        Debug("OpenNetwork() failed\n");

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}
//-----------------------------------------------------------------------------
void SD_CloseNetwork()
{
    if (!sControl.Configured)
        return;

    pthread_mutex_lock(&sControl.Lock);

    Debug("CloseNetwork()\n");

    SM_CloseJoin();

    if (sControl.NetworkOpened)
    {
        // reloading joined devices from file for rejecting unpaired devices

        SM_AddJoinedDeviceList(&sDevConfig, 30, false);
        sm_RemoveUnpairedDevices();
        SM_RemoveJoinedDeviceList();

        // Updating device information

        SM_UpdateDeviceList(&sDevConfig, 45);

        SM_SaveDeviceListToFile(&sDevConfig);
        SM_SaveCapabilityListToFile(&sDevConfig, false);
    }

    sControl.NetworkOpened = 0;

    pthread_mutex_unlock(&sControl.Lock);
}
//-----------------------------------------------------------------------------
int SD_GetDeviceList(SD_DeviceList* list)
{
    unsigned    start, current, passed, duration = 25;

    if (!sControl.Configured)
        return 0;

    if (!list)
        return 0;

    pthread_mutex_lock(&sControl.Lock);

    Debug("GetDeviceList() %d\n", sDevConfig.DeviceCount);

    start = (unsigned)time(0);

    if (sControl.NetworkOpened)
    {
        if (JOINING_BROADCAST_INTERVAL <= (start - sControl.NetworkOpened))
        {
            Debug("PermitJoin()....dp\n");

            SM_PermitJoin(255);

            sControl.NetworkOpened = start;
        }
        Debug("SM_AddJoinedDeviceList()....dp\n");
        SM_AddJoinedDeviceList(&sDevConfig, duration, false);

        current = (unsigned)time(0);
        passed  = current - start;

        if (passed < duration){
            Debug("SM_UpdateDeviceList()....dp\n");
            SM_UpdateDeviceList(&sDevConfig, duration - passed);
        }
        if (!passed)
            sleep(1);
    }
    Debug("*list = (SD_DeviceList)sDevConfig.DeviceList;....dp\n");
    *list = (SD_DeviceList)sDevConfig.DeviceList;

    pthread_mutex_unlock(&sControl.Lock);

    return sDevConfig.DeviceCount;
}
//-----------------------------------------------------------------------------
bool SD_GetDevice(const SD_DeviceID* id, SD_Device* device)
{
    if (!sControl.Configured)
        return false;

    if (sm_FindDevice(id, (SM_Device*)device))
        return true;

    return false;
}
//-----------------------------------------------------------------------------
bool SD_AddDevice(const SD_DeviceID* id)
{
    bool            result = false;
    SM_Device       device;
    int             once;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!sm_FindDevice(id, &device))
            break;

        Debug("%s (%s)\n",__func__, device->EUID.Data);

        if (!device->Paired)
        {
            device->Paired = true;
            device->Available = SD_ALIVE;
            device->AvailableCheckCount = 0;
            device->Attributes |= DA_SENSOR_ENABLED;        // It's not problem to set this bit for bulb,
                                                            // Since they don't use it.

            // SM_UpdateDeviceAvailable(&sDevConfig, id, device->Available);

            if (!SM_SaveDeviceListToFile(&sDevConfig))
                break;

            if (!SM_SaveCapabilityListToFile(&sDevConfig, false))
                break;
        }

        result = true;
    }

    if (!result)
        Debug("AddDevice() failed \n");

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}
//-----------------------------------------------------------------------------
bool SD_RemoveDevice(const SD_DeviceID* id)
{
    bool            result = false;
    SM_Device       device;
    int             once;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!sm_FindDevice(id, &device))
            break;

        Debug("RemoveDevice(%s)\n", device->EUID.Data);

        if (device->Paired)
        {
            SM_RejectDevice(device->DeviceSpecific, &sDevConfig.DeviceScanOption);

            // removing from Groups

            if (sm_RemoveGroupDevice(device))
                sm_SaveGroupListToFile();

            // removing from DeviceList

            SM_RemoveDeviceFile(device);
            sm_RemoveDeviceProperty(device);

            // saving DeviceList

            SM_SaveDeviceListToFile(&sDevConfig);
        }

        result = true;
    }

    if (!result)
        Debug("RemoveDevice() failed \n");

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}
//-----------------------------------------------------------------------------
bool SD_SetDeviceRegisteredID(const SD_DeviceID* euid, const SD_DeviceID* registered_id)
{
    bool            result = false;
    SM_Device       device;
    int             once;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!sm_FindDevice(euid, &device))
            break;

        if (!device->Paired)
            break;

        Debug("SetDeviceRegisteredID(%s,%s)\n", euid->Data, registered_id->Data);

        device->RegisteredID = *registered_id;
        device->RegisteredID.Data[DEVICE_MAX_ID_LENGTH-1] = 0;
        device->Properties |=  DA_SAVED;
        device->Attributes &= ~DA_SAVED;

        if (!SM_SaveDeviceListToFile(&sDevConfig))
            break;

        result = true;
    }

    if (!result)
        Debug("SetDeviceRegisteredID() failed \n");

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}

bool SD_SetDeviceConfig(const SD_DeviceID* device_id, int enable_mode)
{
    int             once;
    int             nAvailable = 0;
    bool            result = false;
    SM_Device       device;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!sm_FindDevice(device_id, &device))
            break;

        if( 0 != strcmp(device->DeviceType, ZB_SENSOR) )
        {
            result = false;
            break;
        }

        nAvailable = device->Available;
        Debug("------------------------\n");
        Debug("SD_SetDeviceConfig(%s, %d) \n", device_id->Data, nAvailable);

        if( enable_mode )
        {
            device->Available = SD_ALIVE;
            device->AvailableCheckCount = 0;
        }
        else
        {
            device->Available = SD_DEAD;
        }

        result = true;
    }

    if( result && (nAvailable != device->Available) )
    {
        SM_UpdateDeviceAvailable(&sDevConfig, device_id, device->Available);
    }

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}

bool SD_GetDeviceConfig(const SD_DeviceID* device_id, int *enable_mode)
{
    int             once;
    int             nAvailable = 0;
    bool            result = false;
    SM_Device       device;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!sm_FindDevice(device_id, &device))
            break;

        nAvailable = device->Available;
        Debug("------------------------\n");
        Debug("SD_GetDeviceConfig(%s, %d) \n", device_id->Data, nAvailable);

        result = true;
    }

    if( result )
    {
        if( nAvailable == SD_ALIVE || nAvailable == 0 )
        {
            *enable_mode = 1;
        }
        else if( nAvailable == SD_DEAD )
        {
            *enable_mode = 0;
        }
    }
    else
    {
        *enable_mode = -1;
    }

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}

//-----------------------------------------------------------------------------
bool SD_SetDeviceCapabilityValue(const SD_DeviceID* device_id, const SD_CapabilityID* capability_id, const char* value, unsigned event)
{
    bool            result = false;
    SM_Capability   capability;
    SM_Device       device;
    const char*     checked_value;
    const char*     noti_value;     // value for notification
    const char*     cache_value;
    int             once, dc;
    int             manualnotifystatus = 0;
    int             freq = 0;
    char            *sensorctrlvalue = NULL;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!value)
             value = "";

        if (!sm_FindDevice(device_id, &device))
            break;

        if (!sm_FindCapability(capability_id, &capability))
            break;

        if (!sm_CheckDeviceCapability(device, capability, "W", &dc, true))
            break;

        if (!SM_CheckDeviceCapabilityValue(capability, value, &checked_value))
            break;

        Debug("------------------------\n");
        Debug("SetDeviceCapabilityValue(%s,%s,(%s), %d) \n", device_id->Data, capability_id->Data, value, device->Available);

        result = true;

        if( SM_CheckAllowToSendZigbeeCommand(capability, value) )
        {
            if( SM_SetDeviceCapabilityValue(device, capability, checked_value) == false )
            {
                result = false;
            }
        }
        else
        {
            if( 0 == strcmp(device->DeviceType, ZB_SENSOR) )
            {
                device->Available = SD_ALIVE;
                device->AvailableCheckCount = 0;
                Debug("SD_SetDeviceCapabilityValue : device is Zigbee Sensor...\n");

                if (true == SM_SetSensorConfig(device, capability, value))
                {
                    if ((device->Attributes & DA_SENSOR_ENABLED) == 0)
                    {
                         pthread_mutex_unlock(&sControl.Lock);
                         disableSensornNotifyRule(device, &manualnotifystatus, &freq);
                         if (SENSOR_NOTIFY_STATUS == manualnotifystatus)
                         {
                             sensorctrlvalue = (char*)ZALLOC(SIZE_32B);
			     Debug("SD_SetDeviceCapabilityValue : manualnotifystatus\n");
                             snprintf(sensorctrlvalue, SIZE_32B, "2:%d", freq);
                             value = sensorctrlvalue;
                         }
                         pthread_mutex_lock(&sControl.Lock);     
                    }
                }
            }
            result = true;
        }
    }

    if (result)
    {
        Debug("If a user press on/off button manually....dp\n");
        // If a user press on/off button manually, then reserved update should be canceled.
        if( 0 != strcmp(device->DeviceType, ZB_SENSOR) )
        {
            SM_CancelReservedCapabilityUpdate(device_id);
        }

        if( SM_CheckNotifiableCapability(device_id, capability, value, event) && (device->Available == SD_ALIVE) )
        {
            Debug("To reserve notification, put manipulated value(checkd_value) for Sleep Fader.....dp\n");
            // To reserve notification, put manipulated value(checkd_value) for Sleep Fader.
            SM_ReserveNotification(device_id, capability, checked_value, event);

            // We change transition time as 0 for 10008 capability.
            SM_ManipulateNotificationValue(capability, value, &noti_value);
            Debug("calling SM_SendEvent....dp");
            SM_SendEvent(true, device_id, capability_id, noti_value, event);
        }
        else
        {
            Debug("  Won't be reported. \n");
        }

        if( SM_CheckCacheUpdateCapability(capability, value) )
        {
            SM_ManipulateCacheUpdateValue(capability, value, &cache_value);
            SAFE_STRCPY(device->CapabilityValue[dc], cache_value);
            Debug("  Cache updated(%s,%s,%s)\n", device_id->Data, capability_id->Data, device->CapabilityValue[dc]);
        }
        else
        {
            Debug("  Won't be updated to cache. \n");
        }
    }

    pthread_mutex_unlock(&sControl.Lock);
   
    if (sensorctrlvalue)
    {
        free(sensorctrlvalue);
    }
 
    return result;
}

//-----------------------------------------------------------------------------
bool SD_GetDeviceCapabilityValue(const SD_DeviceID* device_id, const SD_CapabilityID* capability_id,
                                char value[CAPABILITY_MAX_VALUE_LENGTH], int retryCnt, unsigned data_type)
{
    bool            result = false;
    SM_Capability   capability;
    SM_Device       device;
    int             once, dc;
    char            buffer[CAPABILITY_MAX_VALUE_LENGTH];
    int             nAvailable = 0;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    value[0] = 0x00;

    for (once=1; once; once--)
    {
        if (!value)
            break;

        if (!sm_FindDevice(device_id, &device))
            break;

        if (!sm_FindCapability(capability_id, &capability))
            break;

        if (!sm_CheckDeviceCapability(device, capability, "R", &dc, true))
            break;

        nAvailable = device->Available;

        Debug("------------------------\n");
        Debug("GetDeviceCapabilityValue(%s,%s, Available=%d) \n",
                device_id->Data, capability_id->Data, nAvailable);

        memset(buffer, 0x00, sizeof(buffer));

        // At initial time, we have to fill out cache data first, and return it.
        // 140820 woody, add SleepFader filtering condition due to issue from Steven team.
        if (!device->CapabilityValue[dc][0] && !SM_CheckSleepFaderCapability(capability))
        {
            if (sControl.NetworkOpened)
            {
                if (SM_GetDeviceCapabilityValue(device, capability, buffer, 0, SD_REAL))
                {
                    // make level 255:0 if 0:0
                    SM_UpdateFilteredCapabilityValue(capability, buffer);

                    // update cache
                    SAFE_STRCPY(device->CapabilityValue[dc], buffer);

                    // fill in return value
                    strncpy(value, device->CapabilityValue[dc], CAPABILITY_MAX_VALUE_LENGTH-1);
                    value[CAPABILITY_MAX_VALUE_LENGTH-1] = '\0';

                    // Update status
                    device->Available = SD_ALIVE;
                    device->AvailableCheckCount = 0;

                    Debug("  Cache Empty, read from Zigbee(%s, %s, %s)\n",
                        device_id->Data, capability_id->Data, value);

                    result = true;
                }
                else
                {
                    Debug("  SD_GetDeviceCapabilityValue Error(%s,%s)\n", device_id->Data, capability_id->Data);
                    result = false;
                    break;
                }
            }
            else
            {
                if (SM_GetDeviceCapabilityValue(device, capability, buffer, 0, data_type))
                {
                    if( data_type == SD_REAL )
                    {
                        Debug("  1st Read from zigbee:(%s,%s,%s) in Real mode\n",
                            device_id->Data, capability_id->Data, buffer);

                        // make level 255:0 if 0:0
                        SM_UpdateFilteredCapabilityValue(capability, buffer);

                        // update cache
                        SAFE_STRCPY(device->CapabilityValue[dc], buffer);

                        // fill in return value
                        strncpy(value, device->CapabilityValue[dc], CAPABILITY_MAX_VALUE_LENGTH-1);
                        value[CAPABILITY_MAX_VALUE_LENGTH-1] = '\0';

                        // Update status
                        device->Available = SD_ALIVE;
                        device->AvailableCheckCount = 0;
                    }
                    else if( data_type == SD_CACHE )
                    {
                        Debug("  1st Read from zigbee:(%s,%s,%s) in Cache mode\n",
                            device_id->Data, capability_id->Data, buffer);

                        // fill in return value
                        strncpy(value, buffer, CAPABILITY_MAX_VALUE_LENGTH-1);
                        value[CAPABILITY_MAX_VALUE_LENGTH-1] = '\0';

                    }
                    result = true;
                }
                else
                {
                    Debug("  SD_GetDeviceCapabilityValue Error(%s,%s)\n", device_id->Data, capability_id->Data);
                    result = false;
                    break;
                }
            }
        }
        else
        {
            //
            // Decide if we read cached value or trigger ZB cmd.
            //
            if (SM_CheckFilteredCapability(capability))
            {
                // fill in return value with cache.
                strncpy(value, device->CapabilityValue[dc], CAPABILITY_MAX_VALUE_LENGTH-1);
                value[CAPABILITY_MAX_VALUE_LENGTH-1] = '\0';

                Debug("  Read from cache:(%s,%s,%s)\n",
                    device_id->Data, capability_id->Data, value);
                result = true;
            }
            else
            {
                if( SM_GetDeviceCapabilityValue(device, capability, buffer, 0, data_type))
                {
                    if( data_type == SD_REAL )
                    {
                        // Update cache data.
                        SAFE_STRCPY(device->CapabilityValue[dc], buffer);

                        device->AvailableCheckCount = 0;
                        device->Available = SD_ALIVE;

                        Debug("  2nd Read from Zigbee:(%s,%s,%s)\n",
                            device_id->Data, capability_id->Data, buffer);

                        strncpy(value, device->CapabilityValue[dc], CAPABILITY_MAX_VALUE_LENGTH-1);
                        value[CAPABILITY_MAX_VALUE_LENGTH-1] = '\0';
                    }
                    else if( data_type == SD_CACHE )
                    {
                        // Return from Zigbee layer is meaningless, so we fill in return value with cached data.
                        Debug("  2nd Read from zigbee:(%s,%s,%s) in Cache mode\n",
                            device->EUID.Data, capability->CapabilityID.Data, device->CapabilityValue[dc]);


                        // fill in return value
                        strncpy(value, device->CapabilityValue[dc], CAPABILITY_MAX_VALUE_LENGTH-1);
                        value[CAPABILITY_MAX_VALUE_LENGTH-1] = '\0';

                        Debug("  2nd Read from cache(%s,%s,%s)\n",
                            device_id->Data, capability_id->Data, value);
                    }
                    result = true;
                }
                else
                {
                    Debug("  SD_GetDeviceCapabilityValue Error(%s,%s)\n",
                        device_id->Data, capability_id->Data);
                    result = false;
                    break;
                }
            }
        }
    }

    if( result )
    {
        if( nAvailable != device->Available )
        {
           SM_UpdateDeviceAvailable(&sDevConfig, device_id, device->Available);
        }

        if( device->Available == SD_DEAD)
        {
            result = false;
        }
    }
    else
    {
        Debug("  SD_GetDeviceCapabilityValue(%s) failed \n", device_id->Data);
    }

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}

//-----------------------------------------------------------------------------
bool SD_SetDeviceProperty(const SD_DeviceID* device_id, SD_PROPERTY property, const char* value)
{
    bool            result = false;
    bool            update = true;
    SM_Device       device;
    int             once;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!device_id || !value)
            break;

        if (!sm_FindDevice(device_id, &device))
            break;

        if (!device->Paired)
            break;

        Debug("SetDeviceProperty(%s,%d,%s)\n", device->EUID.Data, property, value);

        switch (property)
        {
        case SD_DEVICE_FRIENDLY_NAME:
                SAFE_STRCPY(device->FriendlyName, value);
                device->Attributes &= ~DA_SAVED;
                break;
        default:
                update = false;
                break;
        }

        if (!update || !SM_SaveDeviceListToFile(&sDevConfig))
            break;

        result = true;
    }

    if (!result)
        Debug("SetDeviceProperty() failed \n");

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}
//-----------------------------------------------------------------------------
bool SD_GetDeviceDetails(const SD_DeviceID* device_id, const char** text)
{
    bool            result = false;
    SM_Device       device;
    int             once;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!sm_FindDevice(device_id, &device))
            break;

        result = SM_GetDeviceDetails(device, text);
    }

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}
//-----------------------------------------------------------------------------
bool SD_SetDeviceOTA(const SD_DeviceID* device_id, int upgrade_policy)
{
    bool            result = false;
    SM_Device       device;
    int             once;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!sm_FindDevice(device_id, &device))
            break;

        Debug("SetDeviceOTA(%s, %d)\n", device->EUID.Data, upgrade_policy);

        result = SM_SetDeviceOTA(device, upgrade_policy);
        ota_device_id = (SD_DeviceID *)device_id;
    }

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}
//-----------------------------------------------------------------------------
bool SD_UnSetDeviceOTA(void)
{
    bool            result = true;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    ota_device_id = NULL;

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}
//-----------------------------------------------------------------------------
bool SD_ReloadDeviceInfo(const SD_DeviceID* device_id)
{
    bool            result = false;
    unsigned        update = 0;
    SM_Device       device;
    int             once;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!sm_FindDevice(device_id, &device))
            break;

        Debug("ReloadDeviceInfo(%s, 0x%x)\n", device->EUID.Data, device->Attributes);

        update = DA_FIRMWARE_VERSION;

        device->Attributes &= ~update;

        if (!SM_UpdateDeviceInfo(device, update))
            break;

        Debug("FirmwareVersion:%s\n", device->FirmwareVersion);

        device->Properties |=  DA_SAVED;
        device->Attributes &= ~DA_SAVED;

        // Available flag clear
        device->Available = SD_ALIVE;
        device->AvailableCheckCount = 0;

        if (!SM_SaveDeviceListToFile(&sDevConfig))
            break;

        result = true;
    }

    if (!result)
        Debug("ReloadDeviceInfo() failed \n");

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}
//-----------------------------------------------------------------------------
//
//      Group
//
//-----------------------------------------------------------------------------
int  SD_GetGroupList(SD_GroupList* list)
{
    if (!sControl.Configured)
        return 0;

    if (!list)
        return 0;

    *list = (SD_GroupList)sDevConfig.GroupList;

    return sDevConfig.GroupCount;
}
//-----------------------------------------------------------------------------
bool SD_GetGroup(const SD_GroupID* id, SD_Group* group)
{
    if (!sControl.Configured)
        return false;

    if (sm_FindGroup(id, (SM_Group*)group, false))
        return true;

    return false;
}
//-----------------------------------------------------------------------------
int SD_SetGroupDevices(const SD_GroupID* group_id, const char* name, const SD_DeviceID device_id[], int count)
{
    int             result = 0;
    SM_Group        group;
    SM_Device       device;
    int             once, d, add;
    int             wait = 100 * 1000;

    if (!sControl.Configured)
        return 0;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        // check Devices
        if (!device_id)
            break;

        for (d = 0; d < count; d++)
        {
            if (!sm_FindDevice(&device_id[d], &device))
                break;

            if (!sm_FindDeviceInOtherGroup(group_id, &device_id[d], true))
                break;
        }

        if (d < count)
            break;

        Debug("SetGroupDevices(%s,%s,%d,[])\n", group_id->Data, name, count);

        // check Group

        if (!sm_FindGroup(group_id, &group, false))
        {
            if (GROUP_MAX_COUNT <= sDevConfig.GroupCount)
            {
                Debug("GROUP_MAX_COUNT \n");
                break;
            }

            // allocating new SD_GroupProperty

            group = (SM_Group)calloc(1, sizeof(SD_GroupProperty));

            if (!group)
            {
                Debug("SetGroupDevice(): Out of Memory \n");
                break;
            }

            group->ID = *group_id;
            group->ID.Data[GROUP_MAX_ID_LENGTH-1] = 0;

            sDevConfig.GroupList[sDevConfig.GroupCount] = group;
            sDevConfig.GroupCount++;
        }
        else
        {
            sm_RemoveGroupMulticastID(group);
            memset(group->CapabilityValue, 0, sizeof(group->CapabilityValue));
        }

        if (name)
            SAFE_STRCPY(group->Name, name);

        // updating Group DeviceList

        group->MulticastID = SM_GenerateMulticastID(&sDevConfig);
        group->DeviceCount = 0;

        for (d = 0; d < count; d++)
        {
            if (!sm_FindDevice(&device_id[d], &device))
                continue;

            if (!device->Paired)
            {
                Debug("Skip unpaired device %s\n", device->EUID.Data);
                continue;
            }

            add = false;

            if (SM_CheckDeviceMulticast(device))
            {
                // setting MulticastID to Devices

                Debug("+ %s (%04X)\n", device->EUID.Data, group->MulticastID);

                if (SM_AddDeviceMulticastID(device, group->MulticastID))
                    add = true;
                else
                    Debug("AddDeviceMulticastID() failed\n");

                usleep(wait);
            }
            else
            {
                Debug("+ %s\n", device->EUID.Data);
                add = true;
            }

            if (add)
            {
                group->Device[group->DeviceCount] = device;
                group->DeviceCount++;
                result++;
            }
        }

        // updating Group CapabilityList

        sm_UpdateGroupCapability(group);

        // saving GroupList

        sm_SaveGroupListToFile();
    }

    if (result != count)
        Debug("SetGroupDevices() %d/%d \n", result, count);

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}
//-----------------------------------------------------------------------------
bool SD_DeleteGroup(const SD_GroupID* group_id)
{
    int             result = false;
    SM_Group        group;
    int             g, i;
    int             once;

    if (!sControl.Configured)
        return 0;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!sm_FindGroup(group_id, &group, true))
            break;

        Debug("DeleteGroup(%s) \n", group->ID.Data);

        sm_RemoveGroupMulticastID(group);

        free(group);

        // rearranging GroupList

        for (g = 0; g < sDevConfig.GroupCount; g++)
        {
            if (group == sDevConfig.GroupList[g])
                break;
        }

        for (i = g; i < sDevConfig.GroupCount-1; i++)
        {
            sDevConfig.GroupList[i] = sDevConfig.GroupList[i+1];
        }

        sDevConfig.GroupList[i] = 0;
        sDevConfig.GroupCount--;

        // saving GroupList

        sm_SaveGroupListToFile();

        result = true;
    }

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}
//-----------------------------------------------------------------------------
bool SD_SetGroupCapabilityValue(const SD_GroupID* group_id, const SD_CapabilityID* capability_id, const char* value, unsigned event)
{
    bool            result = false;
    SM_Group        group;
    SM_Capability   capability;
    SM_Device       device;
    const char*     checked_value;
    const char*     noti_value;     // value for notification
    const char*     cache_value;
    int             once, gc, dc, d;
    int             multicast = 0, unicast = 0, fail = 0;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!value)
             value = "";

        if (!sm_FindGroup(group_id, &group, true))
            break;

        if (!group->DeviceCount)
            break;

        if (!sm_FindCapability(capability_id, &capability))
            break;

        if (!sm_CheckGroupCapability(group, capability, "W", &gc, true))
            break;

        if (!SM_CheckDeviceCapabilityValue(capability, value, &checked_value))
            break;

        Debug("SetGroupCapabilityValue(%s,%s,(%s)) \n", group_id->Data, capability_id->Data, value);

        if(SM_CheckAllowToSendZigbeeCommand(capability, value) == true) {
            // multicast
            if (!SM_SetGroupCapabilityValue(group, capability, checked_value))
                break;
        }

        multicast++;

        //
        // unicast & updating cache for individual device respectively.
        //
        for (d = 0; d < group->DeviceCount; d++)
        {
            device = (SM_Device)group->Device[d];

            if (sm_CheckDeviceCapability(device, capability, "W", &dc, false))
            {
                if (SM_CheckDeviceMulticast(device))
                {
                    if( SM_CheckCacheUpdateCapability(capability, value) )
                    {
                        SM_ManipulateCacheUpdateValue(capability, value, &cache_value);
                        SAFE_STRCPY(device->CapabilityValue[dc], cache_value);
                    }
                    else
                    {
                        Debug("  Won't be updated to cache. \n");
                    }
                }
                else
                {
                    bool tempResult = true;

                    Debug("Unicast in group:%d\n", d);

                    if(SM_CheckAllowToSendZigbeeCommand(capability, value) == true)
                    {
                        tempResult =  SM_SetDeviceCapabilityValue(device, capability, checked_value);
                    }
                    else
                    {
                        // Update cache
                        tempResult = true;
                    }

                    if(tempResult == true)
                    {
                        if( SM_CheckCacheUpdateCapability(capability, value) )
                        {
                            SM_ManipulateCacheUpdateValue(capability, value, &cache_value);
                            SAFE_STRCPY(device->CapabilityValue[dc], cache_value);
                        }
                        else
                        {
                            Debug("  Won't be updated to cache. \n");
                        }
                        unicast++;
                    }
                    else
                    {
                        fail++;
                        break;
                    }
                }

                if( 0 == strcmp(device->DeviceType, ZB_SENSOR) )
                {
                    SM_SetSensorConfig(device, capability, value);
                }
            }
        }

        Debug("Group %s (%04X) [multicast:%d, unicast:%d, fail:%d] \n",
                group->ID.Data, group->MulticastID, multicast, unicast, fail);

        result = (multicast && !fail);
    }

    if (result)
    {
        ((SD_GroupID*)group_id)->Type = SD_GROUP_ID;

        // If a user press on/off button manually, then reserved update should be canceled.
        SM_CancelReservedCapabilityUpdate(group_id);

        if (SM_CheckNotifiableCapability(group_id, capability, value, event))
        {
            // To reserve notification, put manipulated value(checkd_value) for Sleep Fader.
            SM_ReserveNotification(group_id, capability, checked_value, event);

            // We change transition time as 0 for 10008 capability.
            SM_ManipulateNotificationValue(capability, value, &noti_value);

            SM_SendEvent(true, group_id, capability_id, noti_value, event);
        }
        else
        {
            Debug("  Won't be reported. \n");
        }

        if( SM_CheckCacheUpdateCapability(capability, value))
        {
            SM_ManipulateCacheUpdateValue(capability, value, &cache_value);
            SAFE_STRCPY(group->CapabilityValue[gc], cache_value);
        }
        else
        {
            Debug("  Won't be updated to cache. \n");
        }
    }

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}
//-----------------------------------------------------------------------------
bool SD_GetGroupCapabilityValue(const SD_GroupID* group_id, const SD_CapabilityID* capability_id,
                                char value[CAPABILITY_MAX_VALUE_LENGTH], unsigned data_type)
{
    bool            result = false;
    SM_Group        group;
    SM_Capability   capability;
    SM_Device       device;
    int             once, gc, dc, d;
    char            buffer[CAPABILITY_MAX_VALUE_LENGTH];

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!value)
            break;

        if (!sm_FindGroup(group_id, &group, true))
            break;

        if (!group->DeviceCount)
            break;

        if (!sm_FindCapability(capability_id, &capability))
            break;

        if (!sm_CheckGroupCapability(group, capability, "R", &gc, true))
            break;

        Debug("GetGroupCapabilityValue(%s,%s) \n", group_id->Data, capability_id->Data);

        //
        // check if this capability should be read from Cache.
        //
        memset(buffer, 0x00, sizeof(buffer));

        if (SM_CheckFilteredCapability(capability))
        {
            if (group->CapabilityValue[gc][0])
            {
                strncpy(value, group->CapabilityValue[gc], CAPABILITY_MAX_VALUE_LENGTH-1);
                value[CAPABILITY_MAX_VALUE_LENGTH-1] = '\0';
                result = true;
            }
            else
            {
                for (d = 0; d < group->DeviceCount; d++)
                {
                    device = (SM_Device)group->Device[d];

                    if (!sm_CheckDeviceCapability(device, capability, "R", &dc, false))
                        continue;

                    if (!device->CapabilityValue[dc][0])
                    {
                        if (SM_GetDeviceCapabilityValue(device, capability, buffer, 0, data_type))
                        {
                            SM_UpdateFilteredCapabilityValue(capability, buffer);
                            SAFE_STRCPY(device->CapabilityValue[dc], buffer);
                        }
                        else
                        {
                            break;
                        }
                    }

                    strncpy(value, device->CapabilityValue[dc], CAPABILITY_MAX_VALUE_LENGTH-1);
                    value[CAPABILITY_MAX_VALUE_LENGTH-1] = '\0';

                    SAFE_STRCPY(group->CapabilityValue[gc], value);
                    result = true;
                    break;
                }
            }
        }
        else
        {
            //
            // We should trigger ZB cmd... Currently, 10006 capability only.
            //
            for (d = 0; d < group->DeviceCount; d++)
            {
                device = (SM_Device)group->Device[d];

                if (!sm_CheckDeviceCapability(device, capability, "R", &dc, false))
                    continue;

                if (SM_GetDeviceCapabilityValue(device, capability, value, 0, data_type))
                {
                    if( data_type == SD_REAL )
                    {
                        SAFE_STRCPY(group->CapabilityValue[gc], value);
                        result = true;
                    }
                    else if( data_type == SD_CACHE )
                    {
                        strncpy(value, device->CapabilityValue[dc], CAPABILITY_MAX_VALUE_LENGTH-1);
                        value[CAPABILITY_MAX_VALUE_LENGTH-1] = '\0';

                        SAFE_STRCPY(group->CapabilityValue[gc], value);
                        result = true;
                    }
                }
            }
        }

        // updating devices cache

        if (result)
        {
            for (d = 0; d < group->DeviceCount; d++)
            {
                device = (SM_Device)group->Device[d];

                if (sm_CheckDeviceCapability(device, capability, "R", &dc, false))
                    SAFE_STRCPY(device->CapabilityValue[dc], value);
            }
        }
    }

    if (result)
        Debug("%s %s\n", group->ID.Data, value);
    else
        Debug("GetGroupCapabilityValue() failed \n");

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}
//-----------------------------------------------------------------------------
bool SD_GetGroupCapabilityValueCache(const SD_GroupID* group_id, const SD_CapabilityID* capability_id,
                                char value[CAPABILITY_MAX_VALUE_LENGTH], unsigned data_type)
{
    bool            result = false;
    SM_Group        group;
    SM_Capability   capability;
    int             once, gc;

    if (!sControl.Configured)
        return false;

    for (once=1; once; once--)
    {
        if (!value)
            break;

        if (!sm_FindGroup(group_id, &group, true))
            break;

        if (!group->DeviceCount)
            break;

        if (!sm_FindCapability(capability_id, &capability))
            break;

        if (!sm_CheckGroupCapability(group, capability, "R", &gc, true))
            break;

        Debug("GetGroupCapabilityValueCache(%s,%s) \n", group_id->Data, capability_id->Data);

        if (!group->CapabilityValue[gc][0])
        {
            // Add default values
            if(0 == strcmp(group->Capability[gc]->ProfileName, "OnOff") )
            {
                SAFE_STRCPY(group->CapabilityValue[gc], "0");
            }
            else if(0 == strcmp(group->Capability[gc]->ProfileName, "LevelControl") )
            {
                SAFE_STRCPY(group->CapabilityValue[gc], "255:0");
            }
            else if(0 == strcmp(group->Capability[gc]->ProfileName, "SleepFader") )
            {
                SAFE_STRCPY(group->CapabilityValue[gc], "0:0");
            }
            else if(0 == strcmp(group->Capability[gc]->ProfileName, "ColorControl") )
            {
                SAFE_STRCPY(group->CapabilityValue[gc], "32768:32768:0");
            }
            else if(0 == strcmp(group->Capability[gc]->ProfileName, "ColorTemperature") )
            {
                SAFE_STRCPY(group->CapabilityValue[gc], "250:0");
            }
        }

        strncpy(value, group->CapabilityValue[gc], CAPABILITY_MAX_VALUE_LENGTH-1);
        value[CAPABILITY_MAX_VALUE_LENGTH-1] = '\0';


        // for now, we will return cache always for Group. 150317 woody
        result = true;
   }

    if (result)
        Debug("%s %s\n", group->ID.Data, value);
    else
        Debug("GetGroupCapabilityValue() failed \n");

    return result;
}
//-----------------------------------------------------------------------------
bool SD_SetGroupProperty(const SD_GroupID* group_id, SD_PROPERTY property, const char* value)
{
    bool            result = false;
    bool            update = true;
    SM_Group        group;
    int             once;

    if (!sControl.Configured)
        return false;

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!value)
            break;

        if (!sm_FindGroup(group_id, &group, true))
            break;

        Debug("SetGroupProperty(%s,%d,%s)\n", group->ID.Data, property, value);

        switch (property)
        {
        case SD_GROUP_NAME:
                SAFE_STRCPY(group->Name, value);
                break;
        default:
                update = false;
                break;
        }

        if (!update || !sm_SaveGroupListToFile())
            break;

        result = true;
    }

    if (!result)
        Debug("SetGroupProperty() failed \n");

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}
//-----------------------------------------------------------------------------
bool SD_GetGroupOfDevice(const SD_DeviceID* device_id, SD_Group* p_group)
{
    bool            result = false;
    SM_Group        group;
    SM_Device       device;
    int             once, g;

    if (!sControl.Configured)
        return false;

    for (once=1; once; once--)
    {
        if (!p_group)
            break;

        if (!sm_FindDevice(device_id, &device))
            break;

        for (g = 0; g < sDevConfig.GroupCount; g++)
        {
            group = sDevConfig.GroupList[g];

            if (sm_CheckGroupDevice(group, device))
            {
                *p_group = group;
                break;
            }
        }

        result = (g < sDevConfig.GroupCount);
    }

    return result;
}
//-----------------------------------------------------------------------------
//
//      Device or Group
//
//-----------------------------------------------------------------------------
bool SD_SetCapabilityValue(const SD_DeviceID* device, const SD_CapabilityID* capability, const char* value, unsigned event_type)
{
    bool    result = false;

    if (device)
    {
        if (device->Type == SD_GROUP_ID)
            result = SD_SetGroupCapabilityValue(device, capability, value, event_type);
        else
            result = SD_SetDeviceCapabilityValue(device, capability, value, event_type);
    }

    return result;
}
//-----------------------------------------------------------------------------
bool SD_GetCapabilityValue(const SD_DeviceID* device, const SD_CapabilityID* capability, char value[CAPABILITY_MAX_VALUE_LENGTH], unsigned data_type)
{
    bool    result = false;

    if (device)
    {
        if (device->Type == SD_GROUP_ID)
            result = SD_GetGroupCapabilityValueCache(device, capability, value, data_type);
        else
            result = SD_GetDeviceCapabilityValue(device, capability, value, 0, data_type);
    }

    return result;
}
//-----------------------------------------------------------------------------
void SD_ReserveNotificationiForDeleteGroup(SD_GroupID *groupid)
{
    SD_Group        group;
    SD_DeviceID     deviceID;
    SD_Device       device;

    SM_Capability   capability;
    SD_CapabilityID capability_id = {SD_CAPABILITY_ID, "30008"};
    const char*     checked_value;
    int             dc, once;
    int             i;

    if( (!groupid) || (!groupid->Data) ) {
        Debug("SD_ReserveNotificationiForDeleteGroup error - null groupID \n");
        return;
    }

    if( SD_GetGroup(groupid, &group) ) {

        for(i=0; i<group->DeviceCount; i++) {
            device = group->Device[i];

            SAFE_STRCPY(deviceID.Data, device->EUID.Data);
            deviceID.Type = SD_DEVICE_EUID;

            for (once=1; once; once--)
            {
                if (!sm_FindCapability(&capability_id, &capability))
                    break;

                if (!sm_CheckDeviceCapability(device, capability, "W", &dc, true))
                    break;

                if (!SM_CheckDeviceCapabilityValue(capability, device->CapabilityValue[dc], &checked_value))
                    break;

                SM_ReserveNotification(&deviceID, capability, checked_value, 0);
            }
        }
    }
}
//-----------------------------------------------------------------------------
void SD_ClearSleepFaderCache(SD_DeviceID *device_id)
{
    SM_Capability   capability;
    SD_CapabilityID capability_id = {SD_CAPABILITY_ID, "30008"};
    SM_Device       device;
    SM_Group        group;
    int             once, dc=0, gc=0;

    if( (!device_id) || (!device_id->Data) ) {
        Debug("SD_ClearSleepFaderCache error - null device ID \n");
        return;
    }

    for (once=1; once; once--)
    {
        if(device_id->Type == SD_GROUP_ID) {
            if (!sm_FindGroup(device_id, &group, true))
                break;

            if (!group->DeviceCount)
                break;

            if (!sm_FindCapability(&capability_id, &capability))
                break;

            if (!sm_CheckGroupCapability(group, capability, "W", &gc, true))
                break;

            SAFE_STRCPY(group->CapabilityValue[gc], "0:0");
            Debug("Clear 30008 of Group %s\n", device_id->Data);
        } else {
            if (!sm_FindDevice(device_id, &device))
                break;

            // if Sensors, then return.
            if( 0 == strcmp(device->DeviceType, ZB_SENSOR) )
                break;

            if (!sm_FindCapability(&capability_id, &capability))
                break;

            if (!sm_CheckDeviceCapability(device, capability, "W", &dc, true))
                break;

            SAFE_STRCPY(device->CapabilityValue[dc], "0:0");
            Debug("Clear 30008 of %s\n", device_id->Data);
        }
    }
}
//-----------------------------------------------------------------------------
//
//      QA / Maintenance
//
//-----------------------------------------------------------------------------
void SD_ClearAllStoredList()
{
    SM_Device       device;
    int             g;

    Debug("ClearAllStoredList() \n");

    if (sControl.Configured)
    {
        pthread_mutex_lock(&sControl.Lock);

        // removes stored DeivceList, CapabilityList

        while (sDevConfig.DeviceCount)
        {
            device = sDevConfig.DeviceList[0];

            SM_RejectDevice(device->DeviceSpecific, &sDevConfig.DeviceScanOption);
            sm_RemoveDeviceProperty(device);
        }

        // removes stored GroupList

        for (g = 0; g < sDevConfig.GroupCount; g++)
        {
            if (sDevConfig.GroupList[g])
            {
                free(sDevConfig.GroupList[g]);
                sDevConfig.GroupList[g] = 0;
            }
        }

        sDevConfig.GroupCount = 0;

        pthread_mutex_unlock(&sControl.Lock);
    }

    SM_RemoveStoredDeviceList();
    SM_RemoveJoinedDeviceList();
    remove(scFileGroupList);
}
//-----------------------------------------------------------------------------
bool SD_SetQAControl(const char* command, const char* parameter)
{
    bool    result = false;
    int     once;

    if (!sControl.Configured)
        return false;

    Debug("SetQAControl(%s,%s)\n", command, parameter);

    pthread_mutex_lock(&sControl.Lock);

    for (once=1; once; once--)
    {
        if (!command || !strlen(command))
            break;

        if (strcmp("ClearAllDeviceList", command) == 0)
        {
            result = sm_QAClearAllDeviceList();
            break;
        }

        if (strcmp("SetDeviceScanOption", command) == 0)
        {
            result = sm_QASetDeviceScanOption(parameter);
            break;
        }

        if (strcmp("SetRulesOption", command) == 0)
        {
            result = sm_QASetRulesOption(parameter);
            break;
        }
    }

    pthread_mutex_unlock(&sControl.Lock);

    if (!result)
        Debug("SD_SetQAControl() failed * \n");

    return result;
}
//-----------------------------------------------------------------------------

bool SD_ScanJoin()
{
    bool result = false;

    if( !sControl.Configured )
    {
        return false;
    }

    pthread_mutex_lock(&sControl.Lock);

    Debug("ScanJoin()\n");

    if( SM_ScanJoin() )
    {
        result = true;
    }

    if( !result )
    {
        Debug("ScanJoin() failed\n");
    }

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}

bool SD_LeaveNetwork()
{
    bool result = false;

    if( !sControl.Configured )
    {
        return false;
    }

    pthread_mutex_lock(&sControl.Lock);

    Debug("LeaveNetwork()\n");

    if( SM_LeaveNetwork() )
    {
        result = true;
    }

    if( !result )
    {
        Debug("LeaveNetwork() failed\n");
    }

    pthread_mutex_unlock(&sControl.Lock);

    return result;
}

void SD_SetZBMode(int ZigbeeMode)
{
    if( ZigbeeMode == WEMO_LINK_PRISTINE_ZC )
    {
        system("rm -f /tmp/Belkin_settings/zb_coordinate");
        system("rm -f /tmp/Belkin_settings/zb_router");
        system("touch /tmp/Belkin_settings/pristine_zc");
    }
    else if( ZigbeeMode == WEMO_LINK_ZC )
    {
        system("rm -f /tmp/Belkin_settings/pristine_zc");
        system("touch /tmp/Belkin_settings/zb_coordinate");
    }
    else if( ZigbeeMode == WEMO_LINK_ZR )
    {
        system("rm -f /tmp/Belkin_settings/pristine_zc");
        system("touch /tmp/Belkin_settings/zb_router");
    }
}

int SD_GetZBMode()
{
    struct stat st;
    int ZigbeeMode = WEMO_LINK_PRISTINE_ZC;

    if( 0 == stat("/tmp/Belkin_settings/pristine_zc", &st) )
    {
        ZigbeeMode = WEMO_LINK_PRISTINE_ZC;
    }
    else if( 0 == stat("/tmp/Belkin_settings/zb_coordinate", &st) )
    {
        ZigbeeMode = WEMO_LINK_ZC;
    }
    else if( 0 == stat("/tmp/Belkin_settings/zb_router", &st) )
    {
        ZigbeeMode = WEMO_LINK_ZR;
    }

    return ZigbeeMode;
}

bool SD_SetDataStores(char* pDataStoreName, char* pDataStoreValue)
{
  char szFile[128] = {0,};
  bool retVal = false;
  int status = 0;
  FILE *fp;

  DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Entering %s", __FUNCTION__);
  status = mkdir( DATA_STORE_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH );
  DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "Status of mkdir operation: %d, errno = %d", status, errno );

  if( ( 0 == status ) || ( ( -1 == status ) && ( EEXIST == errno ) ) )
  {
    snprintf(szFile, sizeof(szFile), "%s%s.txt", DATA_STORE_PATH, pDataStoreName);
    DEBUG_LOG ( ULOG_UPNP, UL_DEBUG, "szFile = %s\n", szFile );
    fp = fopen(szFile, "w");

    if( NULL == fp )
    {
      DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "Unable to open file %s for writing", pDataStoreName );
    }

    else
    {
      fprintf( fp, pDataStoreValue );
      DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s: File %s written successfully", __FUNCTION__, pDataStoreName);
      fclose( fp );
      retVal = true;
    }
  }

  DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Leaving %s", __FUNCTION__);
  return retVal;
}


char *SD_GetDataStores(char* pDataStoreName, char* pDataStoreValue, size_t read_byte)
{
  FILE *fp;
  int nRead = 0;
  char szFile[128] = {0,};

  if (strstr (pDataStoreName, ".txt"))
  {
      snprintf (szFile, sizeof (szFile), "%s%s", DATA_STORE_PATH, pDataStoreName);
  }
  else
  {
      snprintf (szFile, sizeof (szFile), "%s%s.txt", DATA_STORE_PATH, pDataStoreName);
  }

  fp = fopen(szFile, "r");
  if( fp )
  {
    nRead = fread(pDataStoreValue, 1, read_byte, fp);
    fclose(fp);

    if( nRead <= 0 )
    {
        return NULL;
    }
  }
  else
  {
    return NULL;
  }

  return pDataStoreValue;
}

bool SD_DeleteDataStores(char* pDataStoreName)
{
  char szFile[128] = {0,};
  int ret =0;

  snprintf(szFile, sizeof(szFile), "%s%s.txt", DATA_STORE_PATH, pDataStoreName);
  ret = unlink(szFile);
  if(ret == 0)
  {
    return true;
  }

  return false;
}

bool SD_IsSensor(SD_DeviceID *pDeviceID)
{
    bool ret = false;
    SM_Device   device;
    
    if( sm_FindDevice(pDeviceID, &device) )
    {
        if( (0 == strncmp(device->ModelCode, SENSOR_MODELCODE_PIR, strlen(SENSOR_MODELCODE_PIR))) || 
            (0 == strncmp(device->ModelCode, SENSOR_MODELCODE_FOB, strlen(SENSOR_MODELCODE_FOB))) ||
            (0 == strncmp(device->ModelCode, SENSOR_MODELCODE_DW, strlen(SENSOR_MODELCODE_DW)))   ||
            (0 == strncmp(device->ModelCode, SENSOR_MODELCODE_ALARM, strlen(SENSOR_MODELCODE_ALARM))) )
        {
            ret = true;
        }
    }
    return ret;
}

bool SD_SaveSensorPropertyToFile()
{
    return SM_SaveDevicePropertiesListToFile(&sDevConfig);
}
