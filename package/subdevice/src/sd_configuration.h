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
#ifndef _SD_CONFIGURATION_H_
#define _SD_CONFIGURATION_H_
//-----------------------------------------------------------------------------
#ifdef _SD_MODULE_

#define DEBUG

#ifdef DEBUG
    #define Debug(fmt, ...)         DEBUG_BRIEF("[SD]", fmt, ## __VA_ARGS__)
#else
    #define Debug(fmt, ...)
#endif

#define ZZZ Debug


#ifndef SD_LOCAL_TEST
    #define SD_SAVE_DIR  "/tmp/Belkin_settings/"
#else
    #define SD_SAVE_DIR  "./"
#endif


#define CAPABILITY_ID_ZIGBEE        0x00010000
#define CAPABILITY_ID_EXTENSION     0x00020000

enum {
    ZB_CMD_GET = 0,
    ZB_CMD_SET = 1,
};

typedef enum
{
    DA_ENDPOINT                 = 0x00000001,
    DA_CLUSTER                  = 0x00000002,
    DA_MODEL_CODE               = 0x00000100,
    DA_FIRMWARE_VERSION         = 0x00000200,
    DA_MFG_NAME                 = 0x00000400,
    DA_BASIC_CLUSTER            = 0x00000800,

    DA_SENSOR_ENABLED           = 0x00001000, 

    DA_JOINED                   = 0x01000000,
    DA_SAVED                    = 0x10000000,
    
    DA_SENSOR_NOTIFY_CONTROL    = 0x00010000,

} SD_DeviceAttribute;

typedef SD_CapabilityProperty*  SM_Capability;
typedef SM_Capability*          SM_CapabilityList;

typedef SD_DeviceProperty*      SM_Device;
typedef SM_Device*              SM_DeviceList;

typedef SD_GroupProperty*       SM_Group;
typedef SM_Group*               SM_GroupList;

typedef struct
{
    enum { SCAN_NORMAL, SCAN_MINIMUM }
                        DeviceInfo;

    int                 CommandRetry;
    int                 CommandRetryInterval;       // milisecond
    int                 LeaveRequest;

} SM_DeviceScanOption;

typedef struct
{
    int                 CapabilityCount;
    SM_CapabilityList   CapabilityList;

    int                 DeviceCount;
    SM_DeviceList       DeviceList;

    int                 GroupCount;
    SM_GroupList        GroupList;
    int                 GroupReferenceID;

    SM_DeviceScanOption DeviceScanOption;

} SM_DeviceConfiguration;

typedef struct
{
    SD_Group    Group;
    void*       DeviceSpecific;
} SM_GroupData;

typedef SM_GroupData* SM_GroupDevice;

void    SM_RemoveDeviceFromGroup(SM_Device device);
void    SM_UpdateCapabilityValue(bool bAvailable, const SD_DeviceID* device_id, const SD_CapabilityID* capability_id, const char* value, unsigned event);
void    SM_UpdateSensorCapabilityValue(bool bAvailable, const SD_DeviceID* device_id, const SD_CapabilityID* capability_id, const char* value, unsigned event);
void    SM_ZBCommand_Enque(int nControl, bool group, void *device, SD_Capability capability, char *value);
int     SM_GetDeviceAvailable(const SD_DeviceID* device_id);
bool    SD_IsSensor(SD_DeviceID *pDeviceID);

#endif
//-----------------------------------------------------------------------------
#endif


