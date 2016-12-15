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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "logger.h"

#define  _SD_MODULE_
#include "subdevice.h"
#include "sd_configuration.h"
#include "sd_event.h"

//#include "ledLightUPnPHandler.h"
extern void SendDeviceEvent(bool result, bool group, char *device_id, char *capabiliy_id, char *value, unsigned event);

//-----------------------------------------------------------------------------
void SM_SendEvent(bool result, const SD_DeviceID* device_id, const SD_CapabilityID* capability_id, const char *value, unsigned event)
{
    bool    group;
    char    device[DEVICE_MAX_ID_LENGTH];
    char    capability[CAPABILITY_MAX_ID_LENGTH];

    group = (device_id->Type == SD_GROUP_ID);
    strncpy(device, device_id->Data, DEVICE_MAX_ID_LENGTH);
    strncpy(capability, capability_id->Data, CAPABILITY_MAX_ID_LENGTH);

    SendDeviceEvent(result, group, device, capability, (char*)value, event);
}
//-----------------------------------------------------------------------------

