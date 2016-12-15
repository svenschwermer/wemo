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

#ifndef _BRIDGE_AWAY_RULE_
#define _BRIDGE_AWAY_RULE_
//-----------------------------------------------------------------------------
#include <stdbool.h>

#include "subdevice.h"

#define BR_MAX_WEEKLY_RULE      128
#define BR_MAX_DAILY_RULE       128
#define BR_MAX_COMMAND_LENGTH   64
#define BR_MAX_LOACTION_LENGTH  32      // Sunrise,Sunset
#define BR_MAX_DEVICES          64      // manual toggle managed devices max

typedef volatile enum
{
    TS_PAUSE, TS_RUN, TS_STOP

} BR_TASK_STATE;

typedef enum
{
    BR_DEVICE_ID, BR_GROUP_ID, BR_AWAY_MODE,

} BR_ID_TYPE;

typedef struct
{
    BR_ID_TYPE          Type;
    char                Data[DEVICE_MAX_ID_LENGTH];

} BR_ID;

typedef enum
{
    BR_TIMER            = 0,
    BR_SUNRISE          = 0x0001,
    BR_SUNSET           = 0x0002,
    BR_AWAY             = 0x0004,


} BR_RuleType;

typedef struct
{
    int                 Yesterday, Today, Tomorrow;
    unsigned            Time;
} BR_Time;

typedef struct
{
    BR_RuleType         Type;
    unsigned            Time;
    int                 Offset;
    char                Command[BR_MAX_COMMAND_LENGTH];

} BR_Rule;

typedef struct
{
    BR_RuleType         Types;
    int                 RuleCount;
    BR_Rule*            Rule[BR_MAX_DAILY_RULE];
    char                Loacation[BR_MAX_LOACTION_LENGTH];
    double              Latitude, Longitude;

} BR_DailyRule;

typedef struct
{
    BR_ID               ID;
    BR_DailyRule*       Daily[7];
    int                 Process[7];

} BR_WeeklyRule;

// Manual toggle management for individual devices.
typedef struct
{
    time_t              manualToggleTm;
    BR_ID               ID;
} BR_ManualToggle;

bool    BA_LoadRule();
void    BA_FreeRule();
void	putSubDevicesOff();
void	BA_FreeRuleTask();

int     BA_SimulatedRuleData(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int     BA_GetSimulatedRuleData(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
void    BA_GetSimulatedRuleDataResponse(struct Upnp_Action_Complete *args);
int     BA_GetNotifyManualToggle(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);

bool    BA_CheckManualToggle(unsigned int ruleTime);
void    BA_SetManualToggle(void);

bool    BA_CheckBridgeUDN(const char* udn, bool* sub_device);
bool    BA_GetSubDeviceID(const char* udn, SD_DeviceID* id, bool check_group);
void    BA_ClearManualToggle();

//void    BA_SetLinkedAwayMode(const char* value, unsigned offset);
void 	BA_SetLinkedAwayMode(void);
void    BA_StopLinkedAwayMode(void);

void    BA_SetDeviceAwayMode(SD_DeviceID* device_id, const char* value, unsigned offset);
void    BA_StopDeviceAwayMode(SD_DeviceID* sd_id);

bool    BA_CheckDeviceInAwayMode(char* id);
int     BA_GetDevicesInAwayMode();

void    BA_ClearDeviceListFromStorage();

bool    BA_RemoteUpdateSimulatedRuleData(const char* device_list);

//-----------------------------------------------------------------------------
#endif
#endif

