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
#ifndef _SD_DEVICE_SPECIFIC_H_
#define _SD_DEVICE_SPECIFIC_H_
//-----------------------------------------------------------------------------
#ifdef _SD_MODULE_

typedef enum
{
    STARTED     = 0,
    PENDING     = 1,
    FINISHED    = 2,
} SM_COMMAND;

bool    SM_LoadDeviceLibrary();

bool    SM_LoadDeviceListFromFile(SM_DeviceConfiguration* configuration);
bool    SM_SaveDeviceListToFile(SM_DeviceConfiguration* configuration);
void    SM_RemoveDeviceFile(SD_Device device);
bool    SM_LoadCapabilityListFromFile(SM_DeviceConfiguration* configuration);
bool    SM_SaveCapabilityListToFile(SM_DeviceConfiguration* configuration, bool force_update);
void    SM_UpdateDeviceIDs(SM_DeviceConfiguration* configuration);
void    SM_UpdateCapabilityList(SM_DeviceConfiguration* configuration);
void    SM_UpdateDeviceConfiguration(SM_DeviceConfiguration* configuration);
void    SM_RemoveStoredDeviceList();

bool    SM_PermitJoin(int duration);
void    SM_CloseJoin();
bool    SM_RejectDevice(void* device_specific, SM_DeviceScanOption* option);

void    SM_AddJoinedDeviceList(SM_DeviceConfiguration* configuration, unsigned duration, bool allow_rejoining);
void    SM_RemoveJoinedDeviceList();
void    SM_ClearJoinedDeviceList();

void    SM_UpdateDeviceList(SM_DeviceConfiguration* configuration, unsigned duration);
bool    SM_UpdateDeviceInfo(SM_Device device, unsigned update);

bool    SM_CheckDeviceCapabilityValue(SD_Capability capability, const char* value, const char** checked_value);
bool    SM_SetDeviceCapabilityValue(SD_Device device, SD_Capability capability, const char* value);
bool    SM_GetDeviceCapabilityValue(SD_Device device, SD_Capability, char value[CAPABILITY_MAX_VALUE_LENGTH], int retryCnt, unsigned data_type);
bool    SM_CheckAllowToSendZigbeeCommand(const SD_Capability capability, const char* value);
bool    SM_CheckNotifiableCapability(const SD_DeviceID* device_id, const SD_Capability capability, const char* value, unsigned event);
bool    SM_ReserveNotification(const SD_DeviceID* device_id, const SD_Capability capability, const char* value, unsigned event);
bool    SM_ManipulateNotificationValue(const SD_Capability capability, const char* value, const char ** noti_value);
bool    SM_ManipulateCacheUpdateValue(const SD_Capability capability, const char* value, const char ** cache_value);

bool    SM_CheckCacheUpdateCapability(const SD_Capability capability, const char* value);
bool    SM_CheckFilteredCapability(SD_Capability capability);
bool    SM_CheckSleepFaderCapability(SD_Capability capability);
void    SM_UpdateFilteredCapabilityValue(SD_Capability capability, char value[CAPABILITY_MAX_VALUE_LENGTH]);

bool    SM_CheckSensorConfigCapability(SD_Capability capability);
bool    SM_SetSensorConfig(SM_Device device, SD_Capability capability, const char * value);

int     SM_GenerateMulticastID(SM_DeviceConfiguration* configuration);
bool    SM_AddDeviceMulticastID(SD_Device device, int multicast_id);
bool    SM_RemoveDeviceMulticastID(SD_Device device, int multicast_id);
bool    SM_CheckDeviceMulticast(SD_Device device);
bool    SM_SetGroupCapabilityValue(SD_Group group, SD_Capability capability, const char* value);

bool    SM_GetDeviceDetails(SD_Device device, const char** text);
bool    SM_SetDeviceOTA(SD_Device device, int upgrade_policy);

void    SM_UpdateDeviceAvailable(SM_DeviceConfiguration* configuration, const SD_DeviceID* device_id, unsigned nAvailable);

bool    SM_SetDeviceBasicCapabilityValue(SD_Device device, SD_Capability capability, const char* value);
bool    SM_GetDeviceBasicCapabilityValue(SD_Device device, SD_Capability capability);
bool    SM_SetGroupBasicCapabilityValue(SM_GroupDevice group_device, SD_Capability capability, const char *value);

void    SM_UpdateDeviceCommandStatus(SM_Device device, SM_Capability capability, unsigned CommandStatus);

bool    SM_ScanJoin();
bool    SM_LeaveNetwork();

bool    SM_NetworkForming();

bool    SM_SaveDevicePropertiesListToFile(SM_DeviceConfiguration* configuration);
bool    SM_LoadDevicePropertiesListFromFile(SM_DeviceConfiguration* configuration);

#endif
//-----------------------------------------------------------------------------
#endif


