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
#ifndef _SUB_DEVICE_H_
#define _SUB_DEVICE_H_
//-----------------------------------------------------------------------------
//
//  SubDevice Interface
//
//-----------------------------------------------------------------------------
#include <stdbool.h>

#define WEMO_LINK_PRISTINE_ZC               0
#define WEMO_LINK_ZC                        1
#define WEMO_LINK_ZR                        2

#define ZB_UNKNOWN                          "ZigBee Device"
#define ZB_ONOFFBULB                        "OnOff Light"
#define ZB_LIGHTBULB                        "Lightbulb"
#define ZB_COLORBULB                        "Color Light"
#define ZB_SENSOR                           "ZigBeeSensor"
#define ZB_WEMOLINK                         "WemoLink"

#define OSRAM_LIGHTIFY_TW60                 "Osram Lightify TW60"
#define OSRAM_LIGHTIFY_GARDEN_RGB           "Osram Lightify Garden RGB"
#define OSRAM_LIGHTIFY_FLEX_RGBW            "Osram Lightify Flex RGBW"
#define OSRAM_LIGHTIFY_BR30                 "OSRAM Lightify BR30"

#define SENSOR_PIR                          "WeMo Room Motion Sensor"
#define SENSOR_ALARM                        "WeMo Alarm Sensor"
#define SENSOR_FOB                          "WeMo Home/Away Sensor"
#define SENSOR_DW                           "WeMo Door/Window Sensor"

#define SENSOR_MODELCODE_PIR                "F7C041"
#define SENSOR_MODELCODE_ALARM              "F7C040"
#define SENSOR_MODELCODE_FOB                "F7C039"
#define SENSOR_MODELCODE_DW                 "F7C038"


#define IASZONE_BITSHIFT_BATTERY_LEVEL      8
#define IASZONE_BITSHIFT_RSSI_LEVEL         12

#define IASZONE_BIT_MAIN_EVENT              (1 << 0)
#define IASZONE_BIT_SUB_EVENT               (1 << 1)
#define IASZONE_BIT_LOW_SIGNAL              (1 << 2)
#define IASZONE_BIT_LOW_BATTERY             (1 << 3)
#define IASZONE_BIT_INITIAL_STATUS          (1 << 4)
#define IASZONE_BIT_BATTERY_LEVEL           (0xF << IASZONE_BITSHIFT_BATTERY_LEVEL)
#define IASZONE_BIT_LQI_LEVEL               (0xF << IASZONE_BITSHIFT_RSSI_LEVEL)

#define SENSOR_LOW_BATTERY_ALERT_INTERVAL   (7 * 24 * 60 * 60)
//#define SENSOR_LOW_BATTERY_ALERT_INTERVAL   (5 * 60)
#define SENSOR_ALARM_ALERT_INTERVAL         (5 * 60)

#define CAPABILITY_MAX_COUNT                1024
#define CAPABILITY_MAX_ID_LENGTH            32
#define CAPABILITY_MAX_SPEC_LENGTH          16
#define CAPABILITY_MAX_PROFILE_NAME_LENGTH  32
#define CAPABILITY_MAX_DATA_TYPE_LENGTH     80
#define CAPABILITY_MAX_ATTR_NAME_LENGTH     128
#define CAPABILITY_MAX_NAME_VALUE_LENGTH    256
#define CAPABILITY_MAX_CONTROL_LENGTH       8
#define CAPABILITY_MAX_VALUE_LENGTH         80

#define DEVICE_MAX_COUNT                    64
#define DEVICE_MAX_ID_LENGTH                32
#define DEVICE_MAX_CAPABILITIES             16
#define DEVICE_MAX_DEVICE_TYPE_LENGTH       32
#define DEVICE_MAX_MODEL_CODE_LENGTH        32
#define DEVICE_MAX_SERIAL_LENGTH            32
#define DEVICE_MAX_FIRMWARE_VERSION_LENGTH  32
#define DEVICE_MAX_FRIENDLY_NAME_LENGTH     32
#define DEVICE_MAX_MANUFACTURER_NAME_LENGTH 32
#define DEVICE_MAX_WEMO_CERTIFIED_LENGTH 	4

#define GROUP_MAX_COUNT                     DEVICE_MAX_COUNT
#define GROUP_MAX_ID_LENGTH                 DEVICE_MAX_ID_LENGTH
#define GROUP_MAX_DEVICES                   DEVICE_MAX_COUNT
#define GROUP_MAX_NAME_LENGTH               DEVICE_MAX_ID_LENGTH
#define GROUP_MAX_CAPABILITIES              DEVICE_MAX_CAPABILITIES

#define DATA_STORE_PATH "/tmp/Belkin_settings/data_stores/"

typedef enum
{
    SD_ID_NONE = 0,

    SD_DEVICE_EUID,
    SD_DEVICE_REGISTERED_ID,
    SD_DEVICE_UDN,

    SD_CAPABILITY_ID,
    SD_CAPABILITY_REGISTERED_ID,

    SD_GROUP_ID,

} SD_ID_TYPE;

typedef enum
{
    SD_PROPERTY_NONE = 0,

    SD_DEVICE_FRIENDLY_NAME,

    SD_GROUP_NAME,

} SD_PROPERTY;

typedef struct
{
    SD_ID_TYPE          Type;
    char                Data[DEVICE_MAX_ID_LENGTH];

} SD_DeviceID, SD_GroupID;

typedef struct
{
    SD_ID_TYPE          Type;
    char                Data[CAPABILITY_MAX_ID_LENGTH];

} SD_CapabilityID;

typedef struct
{
    SD_CapabilityID     CapabilityID;
    SD_CapabilityID     RegisteredID;
    char                Spec[CAPABILITY_MAX_SPEC_LENGTH];
    char                ProfileName[CAPABILITY_MAX_PROFILE_NAME_LENGTH];
    char                Control[CAPABILITY_MAX_CONTROL_LENGTH];
    char                DataType[CAPABILITY_MAX_DATA_TYPE_LENGTH];
    char                AttrName[CAPABILITY_MAX_ATTR_NAME_LENGTH];
    char                NameValue[CAPABILITY_MAX_NAME_VALUE_LENGTH];
    void*               DeviceSpecific;

} SD_CapabilityProperty;

typedef const SD_CapabilityProperty* SD_Capability;
typedef const SD_Capability*         SD_CapabilityList;

typedef struct
{
    SD_DeviceID         EUID;
    SD_DeviceID         RegisteredID;
    SD_DeviceID         UDN;
    bool                Paired;
    char                DeviceType[DEVICE_MAX_DEVICE_TYPE_LENGTH];
    char                ModelCode[DEVICE_MAX_MODEL_CODE_LENGTH];
    char                Serial[DEVICE_MAX_SERIAL_LENGTH];
    char                FirmwareVersion[DEVICE_MAX_FIRMWARE_VERSION_LENGTH];
    char                FriendlyName[DEVICE_MAX_FRIENDLY_NAME_LENGTH];
    char                ManufacturerName[DEVICE_MAX_MANUFACTURER_NAME_LENGTH];
    unsigned            Certified;
    unsigned            Available;
    unsigned            AvailableCheckCount;
    int                 CapabilityCount;
    SD_Capability       Capability[DEVICE_MAX_CAPABILITIES];
    char                CapabilityValue[DEVICE_MAX_CAPABILITIES][CAPABILITY_MAX_VALUE_LENGTH];

    unsigned            Attributes;
    unsigned            Properties;

    unsigned long       LastReportedTime;
    unsigned long       LastReportedTimeExt;        // For Key FOB key pressed time.

    void*               DeviceSpecific;

} SD_DeviceProperty;

typedef const SD_DeviceProperty* SD_Device;
typedef const SD_Device*         SD_DeviceList;

typedef struct
{
    SD_GroupID          ID;
    int                 MulticastID;
    char                Name[GROUP_MAX_NAME_LENGTH];
    int                 DeviceCount;
    SD_Device           Device[GROUP_MAX_DEVICES];
    int                 CapabilityCount;
    SD_Capability       Capability[GROUP_MAX_CAPABILITIES];
    char                CapabilityValue[GROUP_MAX_CAPABILITIES][CAPABILITY_MAX_VALUE_LENGTH];

} SD_GroupProperty;

typedef const SD_GroupProperty* SD_Group;
typedef const SD_Group*         SD_GroupList;


typedef enum
{
    SE_NONE     = 0x00,
    SE_LOCAL    = 0x01,
    SE_REMOTE   = 0x02,

} SD_EventType;

typedef enum
{
    SD_REAL     = 0,
    SD_CACHE    = 1,
} SD_DataType;

typedef enum
{
    SD_ALIVE    = 1,
    SD_DEAD     = 2,
} SD_DeviceStatus;

typedef enum
{
    SD_SENSOR_DISABLE    = 0,
    SD_SENSOR_ENABLE     = 1,
} SD_SensorConfig;


typedef struct deviceId {
  char szDeviceID[34];
  struct deviceId *next, *prev;
} deviceId;


//-----------------------------------------------------------------------------
//      Module
//-----------------------------------------------------------------------------

bool    SD_OpenInterface();
//
//      start of sub device interface
//
//      return: true (success), false (failed)
//
void    SD_CloseInterface();
//
//      end of sub device interface
//

bool    SD_CreateThreadJob();

void    SD_DestroyThreadJob();


//      Create network forming to make a Link as zigbee coordinate
bool    SD_NetworkForming();

//-----------------------------------------------------------------------------
//      Capability
//-----------------------------------------------------------------------------

int     SD_GetCapabilityList(SD_CapabilityList* list);
//
//      gets capability list as an array
//
//      return: number of capabilities
//      list:   pointer to receive address of SD_Capability[]
//      usage:
//              SD_CapabilityList   list;
//              int                 count, i;
//
//              count = SD_GetCapabilityList(&list);
//
//              for (i = 0; i < count; i++) {
//                  printf("%s\n", list[i]->ProfileName);
//              }
//
bool    SD_GetCapability(const SD_CapabilityID* id, SD_Capability* capability);
//
//      gets capability information as a pointer type
//
//      usage:
//              SD_CapabilityID     id = xx;
//              SD_Capability       capability;
//
//              if (SD_GetCapability(&id, &capability) {
//                  printf("%s\n", capability->ProfileName);
//              }
//
bool    SD_SetCapabilityRegisteredID(const SD_CapabilityID* capability_id, const SD_CapabilityID* registered_id);
//
//      sets capability registered ID
//

//-----------------------------------------------------------------------------
//      Device
//-----------------------------------------------------------------------------

bool    SD_OpenNetwork();
//
//      allows sub devices to be joined - unit seconds
//      ZigBee max:255
//
void    SD_CloseNetwork();
//
//      closes sub devices joining
//
int     SD_GetDeviceList(SD_DeviceList* list);
//
//      gets sub device list as an array
//
//      return: number of devices
//      list:   pointer to receive address of SD_Device[]
//      usage:
//              SD_DeviceList   list;
//              int             count, i;
//
//              count = SD_GetDeviceList(&list);
//
//              for (i = 0; i < count; i++) {
//                  if (list[i]->Paired)
//                      ...
//              }
//
bool    SD_GetDevice(const SD_DeviceID* id, SD_Device* device);
//
//      gets sub device information as a pointer type
//
//      usage:
//              SD_DeviceID     id = xx;
//              SD_Device       device;
//
//              if (SD_GetDevice(&id, &device) {
//                  printf("%s\n", device->FriendlyName);
//              }
//
bool    SD_AddDevice(const SD_DeviceID* id);
//
//      accepts sub device to be paired
//
bool    SD_RemoveDevice(const SD_DeviceID* id);
//
//      unpairs and makes sub device to leave from network
//
bool    SD_SetDeviceRegisteredID(const SD_DeviceID* euid, const SD_DeviceID* registered_id);
//
//      sets sub device registered ID
//

bool    SD_SetDeviceConfig(const SD_DeviceID* device, int enable_mode);

bool    SD_GetDeviceConfig(const SD_DeviceID* device, int *enable_mode);

bool    SD_SetDeviceCapabilityValue(const SD_DeviceID* device, const SD_CapabilityID* capability, const char* value, unsigned event_type);
//
//      sets sub device capability value
//
//      event_type: SE_NONE or SE_LOCAL + SE_REMOTE
//
bool    SD_GetDeviceCapabilityValue(const SD_DeviceID* device, const SD_CapabilityID* capability, char value[CAPABILITY_MAX_VALUE_LENGTH], int retry_cnt, unsigned data_type);

#if 0
bool    SD_GetDeviceCapabilityValues(const SD_Device Device, char *pszCapIDs, char *pszCapValues, unsigned data_type);
#endif

//
//      gets sub device capability value
//
bool    SD_SetDeviceProperty(const SD_DeviceID* device, SD_PROPERTY property, const char* value);
//
//      sets sub device property
//
bool    SD_GetDeviceDetails(const SD_DeviceID* device, const char** text);
//
//      gets device details
//
//      usage:
//              SD_DeviceID     id = xx;
//              const char*     text;
//
//              if (SD_GetDeviceDetails(&id, &text) {
//                  printf("%s\n", text);
//              }
//
bool    SD_SetDeviceOTA(const SD_DeviceID* device, int upgrade_policy);
//
//      sets device OTA notification
//
bool    SD_UnSetDeviceOTA(void);
//
//      unsets device OTA notification
//
bool    SD_ReloadDeviceInfo(const SD_DeviceID* device);
//
//      reloads device infomation after OTA
//

//-----------------------------------------------------------------------------
//      Group
//-----------------------------------------------------------------------------

int     SD_GetGroupList(SD_GroupList* list);
//
//      gets group list as an array
//
//      return: number of groups
//      list:   pointer to receive address of SD_Group[]
//      usage:
//              SD_GroupList    list;
//              int             count, i;
//
//              count = SD_GetGroupList(&list);
//
//              for (i = 0; i < count; i++) {
//                  printf("%d\n", list[i]->DeviceCount);
//              }
//
bool    SD_GetGroup(const SD_GroupID* id, SD_Group* group);
//
//      gets group information as a pointer type
//
//      usage:
//              SD_GropuID      id = xx;
//              SD_Group        group;
//
//              if (SD_GetGroup(&id, &group) {
//                  printf("%s\n", group->Name);
//              }
//
int     SD_SetGroupDevices(const SD_GroupID* group, const char* name, const SD_DeviceID device[], int count);
//
//      sets group devices
//
//      return: number of devices that set as a group
//      usage:
//              SD_GroupID      group = xx;
//              SD_DeviceID     device[4] = { xx1, xx2, xx3, xx4 };
//
//              if (SD_SetGroupDevices(&group, name, device, 4)) {
//                  printf("OK\n");
//              }
//
bool    SD_DeleteGroup(const SD_GroupID* id);
//
//      deletes group
//
bool    SD_SetGroupCapabilityValue(const SD_GroupID* group, const SD_CapabilityID* capability, const char* value, unsigned event_type);
//
//      sets group capability value
//
//      event_type: SE_NONE or SE_LOCAL + SE_REMOTE
//
bool    SD_GetGroupCapabilityValue(const SD_GroupID* group, const SD_CapabilityID* capability, char value[CAPABILITY_MAX_VALUE_LENGTH], unsigned data_type);
//
//      gets group capability value
//
bool    SD_SetGroupProperty(const SD_GroupID* group, SD_PROPERTY property, const char* value);
//
//      sets group property
//
bool    SD_GetGroupOfDevice(const SD_DeviceID* device, SD_Group* group);
//
//      gets group that a device belongs to
//
//      usage:
//              SD_DeviceID     device = xx;
//              SD_Group        group;
//
//              if (SD_GetGroupOfDevice(&device, &group) {
//                  printf("%s\n", group->Name);
//              }
//
bool    SD_GetGroupCapabilityValueCache(const SD_GroupID* group_id, const SD_CapabilityID* capability_id,
                                char value[CAPABILITY_MAX_VALUE_LENGTH], unsigned data_type);

//-----------------------------------------------------------------------------
//      Device or Group
//-----------------------------------------------------------------------------

bool    SD_SetCapabilityValue(const SD_DeviceID* device, const SD_CapabilityID* capability, const char* value, unsigned event_type);
//
//      sets sub device or group capability value
//
bool    SD_GetCapabilityValue(const SD_DeviceID* device, const SD_CapabilityID* capability, char value[CAPABILITY_MAX_VALUE_LENGTH], unsigned data_type);
//
//      gets sub device or group capability value

void    SD_ReserveNotificationiForDeleteGroup(SD_GroupID *groupid);
//      If the Group is going to be removed, then inherits it's Sleep Fader value to individual bubl.

void    SD_ClearSleepFaderCache(SD_DeviceID *device_id);
//      Set "0:0" for cache of Sleep Fader Capability.


//-----------------------------------------------------------------------------
//      QA / Maintenance
//-----------------------------------------------------------------------------

void    SD_ClearAllStoredList();
//
//      unpairs all devices and clears all stored list
//
bool    SD_SetQAControl(const char* command, const char* parameter);
//
//      ClearAllDeviceList
//      SetDeviceScanOption, DeviceInfo:Minimum
//      SetDeviceScanOption, CommandRetry:1
//      SetDeviceScanOption, CommandRetryInterval:100
//      SetDeviceScanOption, LeaveRequest:1 (keep requesting every 5s until left upto 1 min 30 sec), 0 (default)
//      SetRulesOption,      TimeSacle:60

//-----------------------------------------------------------------------------------------
// Supporting Multiple WeMo Link (Enable to change WeMo Link from the ZC to ZR
//                                when existing the mutiple WeMo Link in the same network)
//-----------------------------------------------------------------------------------------

// When WeMo Link is changing mode from ZC to ZR, Link should scan a ZC and then join to the ZC
bool    SD_ScanJoin();

// When WeMo Link is changing mode from ZR to ZC, Link should leave itself from the zigbee network.
bool    SD_LeaveNetwork();

// Set zigbee mode of WeMo Link (ZC or ZR)
void    SD_SetZBMode(int ZigbeeMode);
// Checking whether Link is Coordinate or Router mode
int     SD_GetZBMode();

bool    SD_SetDataStores(char* pDataStoreName, char* pDataStoreValue);
char *  SD_GetDataStores(char* pDataStoreName, char* pDataStoreValue, size_t read_byte);
bool    SD_DeleteDataStores(char* pDataStoreName);


bool    SD_SaveSensorPropertyToFile();
bool    SD_LoadandPrintSensorPropertyToFile();


//-----------------------------------------------------------------------------
#endif


