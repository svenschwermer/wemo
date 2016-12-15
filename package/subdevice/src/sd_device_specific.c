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
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <belkin_diag.h>

#include "defines.h"
#include "belkin_api.h"

#include "logger.h"
#include "belkin_api.h"
#include "zigbee_api.h"
#include "remote_event.h"

#include "jsmn.h"
#include "utlist.h"

#define  _SD_MODULE_
#include "subdevice.h"
#include "sd_configuration.h"
#include "sd_device_specific.h"
#include "sd_tracer.h"
#include "utility.h"

//-----------------------------------------------------------------------------

#define SEND_ZB_GET_ENDPOINT    1
#define SEND_ZB_GET_CLUSTER     2

#define ZD_MAX_PARAM_COUNT      4
#define ZD_MAX_CLUSTER_COUNT    24

#define JSON_TOKENS             256

#define SM_MEMCPY(target, source) memcpy(target, source, MIN(sizeof(target), sizeof(source)))
#define SM_STRNCPY(target, source) strncpy(target, source, MIN(sizeof(target), sizeof(source)))

typedef enum {
    START,
    OBJECT,
    ARRAY,
    NAME,
    VALUE,
    STOP
} parse_state;

typedef struct
{
    TZBID               DeviceID;

} SM_JoinedZigBeeData;

typedef SM_JoinedZigBeeData*    SM_JoinedDevice;
typedef SM_JoinedDevice*        SM_JoinedDeviceList;

typedef struct SM_JoiningDevice {
    SM_JoinedZigBeeData         device;
    struct SM_JoiningDevice     *prev, *next;
} SM_JoiningDevice;

typedef struct
{
    char*               ListFile;
    int                 ListProcessed;

} SM_JoinedDeviceListInfo;

typedef struct
{
    char*               ListFile;
    int                 ListSaved;

} SM_CapabilityListInfo;

typedef enum
{
    RETRY_0, RETRY_1, RETRY_2, RETRY_3

} SM_RETRY;

typedef struct
{
    TZBID               DeviceID;
    unsigned            Type;

    int                 ClusterCount;
    unsigned            ClusterList[ZD_MAX_CLUSTER_COUNT];

    unsigned            CommandSuccess;
    unsigned            CommandFail;
    unsigned            CommandStatus[ZB_CMD_TERMINATOR-ZB_CMD_NULL+1];

    bool                DoNotZbRouteRepairing;
} SM_ZigBeeSpecificData;

typedef SM_ZigBeeSpecificData*  SM_DeviceData;
typedef SM_DeviceData*          SM_DeviceDataList;

typedef struct
{
    unsigned            Cluster;
    int                 Set;
    int                 Get;

} SM_ZigBeeSpecificControl;

typedef struct
{
    unsigned            Cluster;
    unsigned            Extension;

} SM_ZigBeeClusterExtension;

typedef SM_ZigBeeSpecificControl* SM_DeviceControl;
typedef SM_DeviceControl*         SM_DeviceControlList;

typedef struct
{
    SD_CapabilityProperty       Capability;
    SM_ZigBeeSpecificControl    Control;

} SM_ZigBeeCapabilityPreset;

typedef struct
{
    int                         Type;
    char                        Name[DEVICE_MAX_FRIENDLY_NAME_LENGTH];

} SM_ZigBeeTypeNamePreset;

typedef struct
{
    unsigned            ZCLVersion;
    unsigned            ApplicationVersion;
    unsigned            StackVersion;
    unsigned            HWVersion;
    char                ManufacturerName[32];
    char                ModelIdentifier[32];
    char                DateCode[16];
    unsigned            PowerSource;

} SM_ZigBeeDeviceDetails;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//  Storage Record
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define SD_REOCRD_VERSION   0x20140124

typedef struct
{
    unsigned            Version;
    unsigned            Attributes;                 // SD_DeviceAttribute

    unsigned            ________space1[16];

    // DeviceSpecific

    TZBID               DeviceID;
    unsigned            Type;
    int                 ClusterCount;
    unsigned            ClusterList[24];            // ZD_MAX_CLUSTER_COUNT

    unsigned            ________space2[32];

    // DeviceProperty

    char                EUID[32];
    char                RegisteredID[32];
    char                DeviceType[32];
    char                ModelCode[32];
    char                Serial[32];
    char                FirmwareVersion[32];
    char                FriendlyName[32];
    char                ManufacturerName[32];
    unsigned            Certified;
    unsigned            Available;

} SM_DeviceRecord;

typedef union
{
    char                data[1024];
    SM_DeviceRecord     Record;

} SM_DeviceRecordBlock;

typedef struct
{
    unsigned            Version;

    // DeviceSpecific

    unsigned            Cluster;
    unsigned            ________space[32];

    // CapabilityProperty

    char                CapabilityID[32];
    char                RegisteredID[32];
    char                Spec[16];
    char                ProfileName[32];
    char                Control[8];
    char                DataType[80];
    char                AttrName[128];
    char                NameValue[256];

} SM_CapabilityRecord;

typedef union
{
    char                data[1024];
    SM_CapabilityRecord Record;

} SM_CapabilityRecordBlock;



typedef struct
{
    SD_DeviceID         EUID;
    unsigned            Available;
    char                CapabilityValue[DEVICE_MAX_CAPABILITIES][CAPABILITY_MAX_VALUE_LENGTH];

    unsigned            Attributes;
    unsigned            Properties;

    unsigned long       LastReportedTime;
    unsigned long       LastReportedTimeExt;        // For Key FOB key pressed time.

    unsigned long ReservedTime;
    SD_CapabilityID ReservedCapID;
    char ReservedCapabilityValue[CAPABILITY_MAX_VALUE_LENGTH];
    unsigned int ReservedEvent;

    unsigned int Reserved;
    char ReservedStr[CAPABILITY_MAX_VALUE_LENGTH]

} SD_DeviceSimpleProperty;

typedef union
{
    char                data[2048];
    SD_DeviceSimpleProperty Record;

} SM_DevicePropertyRecordBlock;



//-----------------------------------------------------------------------------

SM_JoiningDevice *JoiningDevice_Head = NULL;


static SM_JoinedDeviceListInfo  sJoinedListInfo     = { ZB_JOIN_LIST, 0 };
static SM_CapabilityListInfo    sCapabilityListInfo = { SD_SAVE_DIR "sd_capabilities", 0 };

static const char* scDeviceUDNPrefix    = "uuid:ZD-1_0-";

static const char* scFileNetworkForming = SD_SAVE_DIR "sd_network_forming";
static const char* scFileDeviceList     = SD_SAVE_DIR "sd_devices";

static const char* scFileDevicePropertyList     = SD_SAVE_DIR "sd_devices_property";


static const char* scUnknown        = "Unknown";

static const char* scZigBee     = "ZigBee";
static const char* scDelimiter  = ",:";


static const SM_ZigBeeTypeNamePreset  scZigBeeTypeNamePreset[]=
{
    {   ZB_DEV_UNKNOWN,             ZB_UNKNOWN
    },
    {   ZB_DEV_HOME_GATEWAY,        ZB_WEMOLINK
    },
    {   ZB_DEV_LIGHT_ONOFF,         ZB_ONOFFBULB
    },
    {   ZB_DEV_LIGHT_DIMMABLE,      ZB_LIGHTBULB
    },
    {   ZB_DEV_LIGHT_COLOR,         ZB_COLORBULB
    },
    {   ZB_DEV_IAS_ZONE,            ZB_SENSOR
    },
};

/// ZCL Extension, briefly ZCE

#define ZCE_COLOR_TEMPERATURE           (CAPABILITY_ID_EXTENSION + ZCL_COLOR_CONTROL_CLUSTER_ID + 1)
#define ZCE_SLEEP_FADER                 (CAPABILITY_ID_EXTENSION + ZCL_LEVEL_CONTROL_CLUSTER_ID + 0)
#define ZCE_LEVEL_MOVE                  (CAPABILITY_ID_EXTENSION + ZCL_LEVEL_CONTROL_CLUSTER_ID + 1)
#define ZCE_LEVEL_STOP                  (CAPABILITY_ID_EXTENSION + ZCL_LEVEL_CONTROL_CLUSTER_ID + 2)
#define ZCE_SENSOR_CONFIG               (CAPABILITY_ID_EXTENSION + ZCL_IAS_ZONE_CLUSTER_ID + 0)
#define ZCE_SENSOR_TESTMODE             (CAPABILITY_ID_EXTENSION + ZCL_IAS_ZONE_CLUSTER_ID + 1)
#define ZCE_SENSOR_KEYPRESS             (CAPABILITY_ID_EXTENSION + ZCL_IAS_ZONE_CLUSTER_ID + 2)

static const SM_ZigBeeClusterExtension  scZigBeeClusterExtension[]=
{
    { ZCL_COLOR_CONTROL_CLUSTER_ID,      ZCE_COLOR_TEMPERATURE },

    { ZCL_LEVEL_CONTROL_CLUSTER_ID,      ZCE_SLEEP_FADER },
    { ZCL_LEVEL_CONTROL_CLUSTER_ID,      ZCE_LEVEL_MOVE },
    { ZCL_LEVEL_CONTROL_CLUSTER_ID,      ZCE_LEVEL_STOP },

    { ZCL_IAS_ZONE_CLUSTER_ID,           ZCE_SENSOR_CONFIG },
    { ZCL_IAS_ZONE_CLUSTER_ID,           ZCE_SENSOR_TESTMODE },
    { ZCL_IAS_ZONE_CLUSTER_ID,           ZCE_SENSOR_KEYPRESS },
};

/// Capability mapping with ZCL, ZCE

static const SM_ZigBeeCapabilityPreset  scZigBeeCapabilityPreset[]=
{
    /*
        { CapabilityID  RegisteredID Spec   ProfileName           Control  DataType                     AttrName                        NameValue
        },
        {          Cluster                     Set                  Get
        }
    */
    {
        {   {0,{0}},     {0,{0}},   {0},    "Identify",             "W",  "Integer",                    "Count",                        "{0~65535}"
        },
        {   ZCL_IDENTIFY_CLUSTER_ID,        ZB_CMD_SET_IDENTIFY,
        }
    },
    {
        {   {0,{0}},     {0,{0}},   {0},    "OnOff",                "RW", "IntegerSet",                 "OnOff",                        "{Off:0,On:1,Toggle:2}"
        },
        {   ZCL_ON_OFF_CLUSTER_ID,          ZB_CMD_SET_ONOFF,   ZB_CMD_GET_ONOFF
        }
    },
    {
        {   {0,{0}},     {0,{0}},   {0},    "LevelControl",         "RW", "Integer,Integer",            "Level,TransitionTime",         "{0~255},{0~65535}"
        },
        {   ZCL_LEVEL_CONTROL_CLUSTER_ID,   ZB_CMD_SET_LEVEL,   ZB_CMD_GET_LEVEL
        }
    },
    {
        {   {0,{0}},     {0,{0}},   {0},    "ColorControl",         "RW", "Integer,Integer,Integer",    "ColorX,ColorY,TransitionTime", "{0~65535},{0~65535},{0~65535}"
        },
        {   ZCL_COLOR_CONTROL_CLUSTER_ID,   ZB_CMD_SET_COLOR,   ZB_CMD_GET_COLOR
        }
    },
    {
        {   {0,{0}},     {0,{0}},    {0},   "IASZone",              "RW", "Intger",                     "ZoneStatus",                   "{0~4294967295}"
        },
        {   ZCL_IAS_ZONE_CLUSTER_ID,        0,                  ZB_CMD_GET_IAS_ZONE_STATUS
        }
    },

    //// ZCL Extension ////

    {
        {   {0,{0}},     {0,{0}},   {0},    "ColorTemperature",     "RW", "Integer,Integer",            "Color,TransitionTime",         "{0~65279},{0~65535}"
        },
        {   ZCE_COLOR_TEMPERATURE,          ZB_CMD_SET_COLOR_TEMP,  ZB_CMD_GET_COLOR_TEMP
        }
    },
    {
        {   {0,{0}},     {0,{0}},   {0},    "SleepFader",           "RW", "IntegerSet,Integer",         "TransitionTime,ReferenceTime", "{0~65535},{0~4294967295}"
        },
        {   ZCE_SLEEP_FADER,                ZB_CMD_SET_LEVEL,       0
        }
    },
    {
        {   {0,{0}},     {0,{0}},   {0},    "LevelControl.Move",    "W",  "IntegerSet,Integer",         "Direction,Rates",              "{Up:0,Down:1},{0~255}"
        },
        {   ZCE_LEVEL_MOVE,                 ZB_CMD_SET_LEVEL_MOVE,  0
        }
    },
    {
        {   {0,{0}},     {0,{0}},   {0},    "LevelControl.Stop",    "W",  "None",                       "None",                         "None"
        },
        {   ZCE_LEVEL_STOP,                 ZB_CMD_SET_LEVEL_STOP,  0
        }
    },
    {
        {   {0,{0}},     {0,{0}},   {0},    "SensorConfig",         "RW", "Integer",                    "SensorConfig",                 "{0:Diable, 1:Enable}"
        },
        {   ZCE_SENSOR_CONFIG,              0,                      0
        }
    },
    {
        {   {0,{0}},     {0,{0}},   {0},    "SensorTestMode",       "RW", "Integer",                    "SensorTestMode",               "{1:Start Test mode}"
        },
        {   ZCE_SENSOR_TESTMODE,            ZB_CMD_SET_TESTMODE,    0
        }
    },
    {
        {   {0,{0}},     {0,{0}},   {0},    "SensorKeyPress",       "RW", "Integer",                    "SensorKeyPress",               "{1:Key pressed short, 2: key pressed long}"
        },
        {   ZCE_SENSOR_KEYPRESS,            0,                      0
        }
    }
};


int json_tokenise(char *js, jsmntok_t **tokens)
{
    jsmntok_t *pTokens = NULL;

    jsmn_parser parser;

    jsmn_init(&parser);

    unsigned int n = JSON_TOKENS;

    //[WEMO-43532] - Use CALLOC() to initialize buffer
    pTokens = (jsmntok_t *)CALLOC(n, sizeof(jsmntok_t));

    int ret = jsmn_parse(&parser, js, strlen(js), pTokens, n);

    while (ret == JSMN_ERROR_NOMEM)
    {
        n = n * 2 + 1;
        //[WEMO-43532] - Use REALLOC() and initialize buffer after re-allocating
        pTokens = REALLOC(pTokens, sizeof(jsmntok_t) * n);
        memset(pTokens, 0x00, sizeof(jsmntok_t) * n);

        ret = jsmn_parse(&parser, js, strlen(js), pTokens, n);
    }

    if (ret == JSMN_ERROR_INVAL)
        Debug("jsmn_parse: invalid JSON string\n");
    if (ret == JSMN_ERROR_PART)
        Debug("jsmn_parse: truncated JSON string\n");

    *tokens = pTokens;

    return ret;
}

bool json_token_streq(char *js, jsmntok_t *t, char *s)
{
    return (strncmp(js + t->start, s, t->end - t->start) == 0
            && strlen(s) == (size_t) (t->end - t->start));
}

char * json_token_tostr(char *js, jsmntok_t *t)
{
    js[t->end] = '\0';
    return js + t->start;
}

char* substring(const char *src, size_t begin, size_t len)
{
    if( src == NULL || strlen(src) == 0 || strlen(src) < begin || strlen(src) < (begin + len) )
    {
        return NULL;
    }

    return strndup(src + begin, len);
}

void convert_fw_version(char *fw_version, size_t fw_len)
{
    size_t begin = 0;
    size_t len = 0;
    char *pHex = NULL;

    if( (pHex = strstr(fw_version, "0x")) || (pHex = strstr(fw_version, "0X")) )
    {
        begin = 2;
        len = strlen(fw_version) - begin;

        char *fw_ver = substring(fw_version, begin, len);
        if( fw_ver )
        {
            strncpy(fw_version, fw_ver, fw_len-1);
            fw_version[fw_len-1] = '\0';
            free(fw_ver);
        }
    }
}

//static const char*  scCapabilityOnOff = "10006";

//-----------------------------------------------------------------------------
static bool sm_ZigBeeSendCommand(TZBID *device_id, TZBParam *param, bool recovery)
{
    int             code;
    TZBParam        update;
    SD_DeviceID     euid;

    bool            mtorr = false;
    bool            wait  = false;

    code = zbSendCommand(device_id, param);

    if( code == ZB_RET_SUCCESS )
        return true;

    switch (code)
    {
        //case ZB_RET_DELIVERY_FAILED:      // -3
        case ZB_RET_NO_RESPONSE:            // -4
            if( param->command == ZB_CMD_NET_LEAVE_REQUEST )
            {
                mtorr = true;
            }
            break;

        case ZB_RET_OUT_OF_ORDER:
            wait = true;
            break;

        default:
            break;
    }

    //  MTORR will be done inside of Zigbee module.
    if( mtorr )
    {
        Debug("sleep 5 for MTORR in sm_ZigBeeSendCommand \n");
        memset(&update, 0, sizeof(update));

        update.command = ZB_CMD_NET_MTORR;

        zbSendCommand(device_id, &update);

        sleep(1);

        update.command = ZB_CMD_GET_NODEID;

        if (zbSendCommand(device_id, &update) != ZB_RET_SUCCESS)
            return false;

        if (device_id->node_id != update.param1)
        {
            Debug("Device Updated: %s:%04X >> %04X \n", EUI64ToString(device_id->node_eui, euid.Data), device_id->node_id, update.param1);
            device_id->node_id = update.param1;
        }

        sleep(4);
    }


    if( wait )
    {
        Debug("sleep 4 for out of order in sm_ZigBeeSendCommand \n");

        sleep(4);
    }

    return false;
}
//-----------------------------------------------------------------------------
static bool sm_DeviceSendCommand(SM_DeviceData device_data, TZBParam* param, int command, int param1, int param2, int param3, int param4, int retry, int retry_interval)
{
    bool            result = false;
    TZBID           zb_id;
    TZBParam        zb_param;
    TZBID*          device_id;
    int             retrial;
    SD_DeviceID     euid;

    if (device_data)
        device_id = &device_data->DeviceID;
    else
        device_id = &zb_id;

    if (!param)
         param = &zb_param;

    memset(param, 0, sizeof(zb_param));

    param->command = command;
    param->param1  = param1;
    param->param2  = param2;
    param->param3  = param3;
    param->param4  = param4;

    for (retrial = 0; retrial < retry + 1; retrial++)
    {
        // Zigbee MTORR will be done inside of Zigbee module.
        if( sm_ZigBeeSendCommand(device_id, param, false) )
        {
            result = true;
            break;
        }

        if( command == ZB_CMD_NET_LEAVE_REQUEST )
        {
            continue;
        }

        // sleep 5 seconds for MTORR
        if( retry > 0 )
        {
            Debug("    sleep 5seconds for MTORR in sm_DeviceSendCommand \n");
            sleep(5);
        }
    }

    if (result)
    {
        if (device_data)
        {
            device_data->CommandSuccess++;
            device_data->CommandFail += retrial;
        }
    }
    else
    {
        if (device_data)
            device_data->CommandFail += retrial;

        Debug("sm_DeviceSendCommand(%d) failed \n", command);
    }

    if (retrial && device_data)
    {
        Debug("Device %s:%04X command status {success:%d, fail:%d} \n",
            EUI64ToString(device_data->DeviceID.node_eui, euid.Data), device_data->DeviceID.node_id,
            device_data->CommandSuccess, device_data->CommandFail);
    }

    return result;
}
//-----------------------------------------------------------------------------
static bool sm_DeviceSendCommandParam(SM_DeviceData device_data, TZBParam* param, SM_RETRY retry)
{
    bool            result = false;
    TZBID*          device_id;
    int             retrial, wait;
    SD_DeviceID     euid;

    if (!device_data || !param)
        return false;

    device_id = &device_data->DeviceID;

    wait = 100 * 1000;

    for (retrial = 0; retrial < retry + 1; retrial++)
    {
        if (sm_ZigBeeSendCommand(device_id, param, false))
        {
            result = true;
            break;
        }
        usleep(wait);
    }

    if (result)
    {
        device_data->CommandSuccess++;
        device_data->CommandFail += retrial;
    }
    else
    {
        device_data->CommandFail += retrial;

        Debug("sm_DeviceSendCommandParam(%d) failed \n", param->command);
    }

    if (retrial)
    {
        Debug("Device:%s, command status {success:%d, fail:%d} \n",
            EUI64ToString(device_data->DeviceID.node_eui, euid.Data), device_data->CommandSuccess,
            device_data->CommandFail);
    }

    return result;
}

//-----------------------------------------------------------------------------
static bool sm_NetworkForming()
{
    bool        result = false;
    FILE*       file = 0;
    int         once;

    for (once=1; once; once--)
    {
        file = fopen(scFileNetworkForming, "r");

        if (!file)
        {
            Debug("sm_NetworkForming(): File is not existed and then make zigbee network forming...\n");
            if (!sm_DeviceSendCommand(0, 0, ZB_CMD_NET_FORM, 0,0,0,0, RETRY_0, 0))
            {
                Debug("sm_NetworkForming(): ZB_CMD_NET_FORM is failed...\n");
                break;
            }

            file = fopen(scFileNetworkForming, "w");
        }

        if (!file)
        {
            Debug("sm_NetworkForming(): File operation failed\n");
            break;
        }

        result = true;
    }

    if (file)
        fclose(file);

    return result;
}

//-----------------------------------------------------------------------------
static int sm_LoadJoinedDeviceList(SM_JoinedDeviceList* list)
{
    SM_JoinedDeviceList device_list = 0;
    SM_JoinedDevice     device;
    int                 max_load_count, count = 0;
    FILE*               file = 0;
    char                field[3][256];
    char*               error = 0;
    int                 once, s;

    for (once=1; once; once--)
    {
        file = fopen(sJoinedListInfo.ListFile, "r");

        if (!file)
            break;

        max_load_count = DEVICE_MAX_COUNT * 2;

        device_list = (SM_JoinedDeviceList)calloc(max_load_count, sizeof(SM_JoinedDevice));

        if (!device_list)
        {
            error = "Out of memory";
            break;
        }

        // skipping already checked list
        memset(&field, 0x00, sizeof(field));

        for (s = 0; s < sJoinedListInfo.ListProcessed; s++)
        {
            fgets(field[0], sizeof(field), file);
        }

        Debug("sm_LoadJoinedDeviceList(): ListProcessed = [%d]\n", sJoinedListInfo.ListProcessed);
        Debug("sm_LoadJoinedDeviceList(): field[0] = [%s]\n", field[0]);

        // loading newly added list

        while (fscanf(file, "%s %s %s", field[0], field[1], field[2]) == 3)
        {
            if (max_load_count <= count)
                break;

            device = (SM_JoinedDevice)calloc(1, sizeof(SM_JoinedZigBeeData));

            if (!device)
            {
                error = "Out of memory";
                break;
            }

            sscanf(field[0], "%x", &device->DeviceID.node_id);
            HexStringToBin(field[1], device->DeviceID.node_eui, sizeof(TEUID_64));

            Debug("sm_LoadJoinedDeviceList(): field[0] = [%s], field[1] = [%s]\n", field[0], field[1]);

            device_list[count] = device;
            count++;
        }
    }

    if (file)
        fclose(file);

    *list = device_list;

    if (error)
        Debug("sm_LoadJoinedDeviceList() %d:%d %s \n", sJoinedListInfo.ListProcessed, count, error);

    return count;
}
//-----------------------------------------------------------------------------
static void sm_FreeJoinedDeviceList(SM_JoinedDeviceList list, int count)
{
    int         i;

    if (!list)
        return;

    for (i = 0; i < count; i++)
    {
        if (list[i])
            free(list[i]);
    }

    free(list);
}
//-----------------------------------------------------------------------------
static bool sm_CheckDeviceCluster(SM_DeviceData device_data, unsigned cluster)
{
    int         c;

    for (c = 0; c < device_data->ClusterCount; c++)
    {
        if (cluster == device_data->ClusterList[c])
            break;
    }

    return (c < device_data->ClusterCount);
}
//-----------------------------------------------------------------------------
static void sm_SetDeviceClusterExtension(SM_DeviceData device_data)
{
    unsigned    cluster, extension;
    int         count, c, ce;

    count = device_data->ClusterCount;

    if (ZD_MAX_CLUSTER_COUNT <= count)
    {
        Debug("sm_SetDeviceClusterExtension: cluster count = %d\n", count);
        return;
    }

    for (c = 0; c < count; c++)
    {
        cluster = device_data->ClusterList[c];

        for (ce = 0; ce < sizeof(scZigBeeClusterExtension)/sizeof(SM_ZigBeeClusterExtension); ce++)
        {
            if (cluster == scZigBeeClusterExtension[ce].Cluster)
            {
                extension = scZigBeeClusterExtension[ce].Extension;

                if (sm_CheckDeviceCluster(device_data, extension))
                    continue;

                Debug("Cluster Extension[%d] %04X:%04X \n", device_data->ClusterCount, cluster, extension);

                device_data->ClusterList[device_data->ClusterCount] = extension;
                device_data->ClusterCount++;
            }
        }
    }
}
//-----------------------------------------------------------------------------
static void sm_SetDeviceUDN(SM_Device device)
{
    device->UDN.Type = SD_DEVICE_UDN;
    snprintf(device->UDN.Data, DEVICE_MAX_ID_LENGTH, "%s%s", scDeviceUDNPrefix, device->EUID.Data);
}

//-----------------------------------------------------------------------------
static char* sm_GetOSRAMFriendlyName(char *pModelCode, char *pManufacturerName)
{
  if( ((0 == strcmp(pManufacturerName, "Osram Lightify")) || (strstr(pManufacturerName, "OSRAM"))) &&
      ((0 == strcmp(pModelCode, "LIGHTIFY Gardenspot RGB")) || (0 == strcmp(pModelCode, "Gardenspot RGB"))) )
  {
    return OSRAM_LIGHTIFY_GARDEN_RGB;
  }
  else if( ((0 == strcmp(pManufacturerName, "Osram Lightify")) || (strstr(pManufacturerName, "OSRAM"))) &&
           ((0 == strcmp(pModelCode, "LIGHTIFY A19 Tunable White")) || (0 == strcmp(pModelCode, "Classic A60 TW"))) )
  {
    return OSRAM_LIGHTIFY_TW60;
  }
  else if( ((0 == strcmp(pManufacturerName, "Osram Lightify")) || (strstr(pManufacturerName, "OSRAM"))) &&
           ((0 == strcmp(pModelCode, "LIGHTIFY Flex RGBW")) || (0 == strcmp(pModelCode, "Flex RGBW"))) )
  {
    return OSRAM_LIGHTIFY_FLEX_RGBW;
  }
  else if( ((0 == strcmp(pManufacturerName, "Osram Lightify")) || (strstr(pManufacturerName, "OSRAM"))) &&
           (0 == strcmp(pModelCode, "iQBR30")) )
  {
    return OSRAM_LIGHTIFY_BR30;
  }

  return NULL;
}

//-----------------------------------------------------------------------------
static bool sm_CheckZigbeeFile(const char *pEUID, char *zbFileName, size_t fNameSize, int *end_point)
{
    bool result = false;
    struct stat st;
    char zbid_name[80] = {0,};
    int i = 0, wait = 0, retry_cnt = 0;

    wait = 100 * 1000; //0.1sec

    for( retry_cnt = 2; retry_cnt; retry_cnt-- )
    {
        for( i = 0; i <= 16; i++ )
        {
            snprintf(zbid_name, sizeof(zbid_name), "/tmp/Belkin_settings/zbid.%s.%02X", pEUID, i);

            if( stat(zbid_name, &st) == 0 )
            {
                Debug("sm_CheckZigbeeFile: %s file is existed...\n", zbid_name);
                strncpy(zbFileName, zbid_name, fNameSize-1);
                zbFileName[fNameSize-1] = '\0';
                *end_point = i;
                return true;
            }
            else
            {
                Debug("sm_CheckZigbeeFile: %s file is not existed...\n", zbid_name);
            }
        }
        usleep(wait);
    }

    return result;
}

static bool sm_ValidateJSONFile(char *pZBFileName, char *js)
{
    struct stat st;
    int nRead = 0;
    bool result = false;

    if( stat(pZBFileName, &st) != 0 )
    {
        return result;
    }

    FILE *fp = fopen(pZBFileName, "rb");
    if( fp )
    {
        nRead = fread(js, 1, 1024, fp);
        printf("======== %s --> json : read byte %d ==========\n", pZBFileName, nRead);
        if( nRead )
        {
            if( strchr(js, '}') )
            {
                result = true;
            }
        }
        fclose(fp);
    }

    return result;
}

static int sm_ParseZigbeeClusterJSON(char *pEUID, TZBParam *param, char *device_type, size_t device_type_len, unsigned ClusterList[], int nCluster)
{
    char *js = NULL;
    jsmntok_t *tokens = NULL;

    int nRead = 0;
    int i = 0, j = 0, id = 0, end_point = 0;
    int device_id_flag = 0;

    char zbFileName[80] = {0,};

    int result = 0;

    // check if "/tmp/Belkin_settings/zbid.EUID.END_POINT" --> "/tmp/Belkin_settings/zbid.EUID.01"
    if( !sm_CheckZigbeeFile(pEUID, zbFileName, sizeof(zbFileName), &end_point) )
    {
        Debug("sm_ParseZigbeeClusterJSON: %s.node_id file is not existed...\n", pEUID);
        Debug("sm_ParseZigbeeClusterJSON: Return SEND_ZB_GET_ENDPOINT = [%d]...\n", end_point);
        result = SEND_ZB_GET_ENDPOINT;
        return result;
    }

    js = (char *)calloc(1024, sizeof(char));

    if( NULL == js )
    {
        return -1;
    }

    param->ret = end_point;

    if( !sm_ValidateJSONFile(zbFileName, js) )
    {
        free(js);
        Debug("sm_ParseZigbeeClusterJSON: %s.%d file is not valid...\n", pEUID, end_point);
        Debug("sm_ParseZigbeeClusterJSON: Return SEND_ZB_GET_CLUSTER = [%d]...\n", end_point);
        result = SEND_ZB_GET_CLUSTER;
        return result;
    }

    /*
    {                           --> object
        "device id": "0x0402",  --> name:value
        "clusters": [           --> array
            "0x0000",           --> value
            "0x0001",
            "0x0003",
            "0x0500",
            "0xff00"
        ],
        "zone type": "0x0000"   --> name:value
    }
    */

    parse_state state = START;

    size_t name_in_object_tokens = 0;
    size_t value_in_array_tokens = 0;

    int ret = json_tokenise(js, &tokens);

    if( ret == JSMN_ERROR_INVAL || ret == JSMN_ERROR_PART || ret == JSMN_ERROR_NOMEM )
    {
        Debug("sm_ParseZigbeeClusterJSON: json file format is not valid...\n");
        Debug("sm_ParseZigbeeClusterJSON: Return SEND_ZB_GET_CLUSTER = [%d]...\n", end_point);
        result = SEND_ZB_GET_CLUSTER;
        return result;
    }

    //parse the tokens
    for( i = 0, j = 1; j >= 0; i++, j-- )
    {
        jsmntok_t *t = &tokens[i];
        //Check if tokens is uninitialized (t->start = -1, t->end = -1)
        if( t->type == JSMN_OBJECT )
        {
            j += t->size;
            name_in_object_tokens = t->size;
            state = OBJECT;
            Debug("Json Object = %d\n", j);
        }
        else if( t->type == JSMN_ARRAY )
        {
            j += t->size;
            value_in_array_tokens = t->size;
            state = ARRAY;
            Debug("Json Array = %d\n", j);
        }
        switch( state )
        {
        case START:
            if( t->type != JSMN_OBJECT )
            {
              Debug("Invalid response: root elemnet must be object...\n");
              state = STOP;
            }
            else
            {
              state = OBJECT;
              name_in_object_tokens = t->size;
            }
            break;
        case OBJECT:
            if( name_in_object_tokens == 0 )
            {
              state = STOP;
            }
            else
            {
              state = NAME;
            }
            break;
        case ARRAY:
            if( value_in_array_tokens == 0 )
            {
                state = STOP;
            }
            else
            {
                state = VALUE;
            }
            break;
        case NAME:
            name_in_object_tokens--;
            //Keys are odd-numbered tokens within the object
            if( name_in_object_tokens % 2 == 1 )
            {
              if( t->type == JSMN_STRING )
              {
                char *key = json_token_tostr(js, t);
                Debug("key = %s\n", key);
                if( strcmp(key, "device id") == 0 )
                {
                    device_id_flag = 1;
                }
                state = NAME;
              }
            }
            else if ( name_in_object_tokens % 2 == 0 )
            {
              if( t->type == JSMN_STRING ||  t->type == JSMN_PRIMITIVE  )
              {
                char *value = json_token_tostr(js, t);
                Debug("value = %s\n", value);

                if( device_id_flag )
                {
                    int dev_type = (int)strtol(value, NULL, 0);

                    if( dev_type == 0x0402 )
                    {
                        param->device_type = ZB_DEV_IAS_ZONE;
                        strncpy(device_type, ZB_SENSOR, device_type_len-1);
                        device_type[device_type_len-1] = '\0';
                    }
                    else if( dev_type == 0x0050 )
                    {
                        param->device_type = ZB_DEV_HOME_GATEWAY;
                        strncpy(device_type, ZB_WEMOLINK, device_type_len-1);
                        device_type[device_type_len-1] = '\0';
                    }
                    else if( dev_type == 0x0100 )
                    {
                        param->device_type = ZB_DEV_LIGHT_ONOFF;
                        strncpy(device_type, ZB_ONOFFBULB, device_type_len-1);
                        device_type[device_type_len-1] = '\0';
                    }
                    else if( dev_type == 0x0101 )
                    {
                        param->device_type = ZB_DEV_LIGHT_DIMMABLE;
                        strncpy(device_type, ZB_LIGHTBULB, device_type_len-1);
                        device_type[device_type_len-1] = '\0';
                    }
                    else if( dev_type == 0x0102 )
                    {
                        param->device_type = ZB_DEV_LIGHT_COLOR;
                        strncpy(device_type, ZB_COLORBULB, device_type_len-1);
                        device_type[device_type_len-1] = '\0';
                    }
                    else
                    {
                        param->device_type = ZB_DEV_UNKNOWN;
                        strncpy(device_type, ZB_UNKNOWN, device_type_len-1);
                        device_type[device_type_len-1] = '\0';
                    }

                    device_id_flag = 0;
                }

                state = NAME;
              }
            }

            //Last object value
            if( name_in_object_tokens ==  0 )
            {
              state = STOP;
            }
            break;
        case VALUE:
            value_in_array_tokens--;
            if( t->type == JSMN_STRING )
            {
                char *value = json_token_tostr(js, t);
                Debug("array value = %s\n", value);
                ClusterList[id++] = (unsigned)strtol(value, NULL, 0);
                state = VALUE;
            }
            if( value_in_array_tokens == 0 )
            {
                state = STOP;
            }
            break;
        case STOP:
            break;
        }
    }

    if( tokens )
    {
        free(tokens);
    }

    free(js);

    param->param1 = id;

    Debug("sm_ParseZigbeeClusterJSON: EUID = %s, nCluster = %d, device_type = %x\n",
            pEUID, param->param1, param->device_type);

    return result;
}

static bool sm_ParseZigbeeBasicClusterJSON(char *pEUID, char *app_ver, size_t app_ver_len, char *manufacturer, size_t manufacturer_len, char *model_code, size_t model_code_len)
{
    char *js = NULL;
    bool result = false;
    jsmntok_t *tokens = NULL;
    struct stat st;

    int i = 0, j = 0, end_point = 1;

    char key_object[80] = {0,};
    char zbFileName[80] = {0,};
    char zbBasicFileName[80] = {0,};

    if( !sm_CheckZigbeeFile(pEUID, zbFileName, sizeof(zbFileName), &end_point) )
    {
        Debug("sm_ParseZigbeeBasicClusterJSON: %s.node_id file is not existed...\n", pEUID);
        return result;
    }

    snprintf(zbBasicFileName, sizeof(zbBasicFileName), "/tmp/Belkin_settings/zbid.%s.%02X.0000", pEUID, end_point);

    if( stat(zbBasicFileName, &st) != 0 )
    {
        Debug("sm_ParseZigbeeBasicClusterJSON: %s file is not existed...\n", zbBasicFileName);
        return result;
    }

    js = (char *)calloc(1024, sizeof(char));

    if( NULL == js )
    {
        return result;
    }

    if( !sm_ValidateJSONFile(zbBasicFileName, js) )
    {
        free(js);
        Debug("sm_ParseZigbeeBasicClusterJSON: %s file is not valid...\n", zbBasicFileName);
        return result;
    }

    /* smaple json data
    {                       --> object
        "0x0001": null ,    --> name:value
        "0x0004": "Belkin" ,
        "0x0005": "Sensor" ,
    },
    {
        "name": value,
        ...
    }
    */

    parse_state state = START;

    size_t name_in_object_tokens = 0;
    size_t value_in_array_tokens = 0;

    int ret = json_tokenise(js, &tokens);
    if( ret == JSMN_ERROR_INVAL || ret == JSMN_ERROR_PART || ret == JSMN_ERROR_NOMEM )
    {
        Debug("sm_ParseZigbeeBasicClusterJSON: json file format is not valid...\n");
        return result;
    }

    //parse the tokens
    for( i = 0, j = 1; j >= 0; i++, j-- )
    {
        jsmntok_t *t = &tokens[i];
        //Check if tokens is uninitialized (t->start = -1, t->end = -1)
        if( t->type == JSMN_OBJECT )
        {
            j += t->size;
            name_in_object_tokens = t->size;
            state = OBJECT;
            Debug("Json Object = %d\n", j);
            Debug("sm_ParseZigbeeBasicClusterJSON : name_in_object_tokens = %d \n", name_in_object_tokens);
        }
        else if( t->type == JSMN_ARRAY )
        {
            j += t->size;
            value_in_array_tokens = t->size;
            state = ARRAY;
            Debug("Json Array = %d\n", j);
        }

        switch( state )
        {
        case START:
            if( t->type != JSMN_OBJECT )
            {
              Debug("Invalid response: root elemnet must be object...\n");
              state = STOP;
            }
            else
            {
              state = OBJECT;
              name_in_object_tokens = t->size;
              Debug("sm_ParseZigbeeBasicClusterJSON : name_in_object_tokens = %d \n", name_in_object_tokens);
            }
            break;

        case OBJECT:
            if( name_in_object_tokens == 0 )
            {
              state = STOP;
            }
            else
            {
              state = NAME;
            }
            break;

        case ARRAY:
            break;

        case VALUE:
            break;

        case NAME:
            name_in_object_tokens--;
            //Keys are odd-numbered tokens within the object
            if( name_in_object_tokens % 2 == 1 )
            {
              if( t->type == JSMN_STRING )
              {
                char *key = json_token_tostr(js, t);
                Debug("key = %s\n", key);
                SAFE_STRCPY(key_object, key);
                state = NAME;
              }
            }
            else if ( name_in_object_tokens % 2 == 0 )
            {
              if( t->type == JSMN_STRING ||  t->type == JSMN_PRIMITIVE  )
              {
                char *value = json_token_tostr(js, t);
                Debug("value = %s\n", value);

                if( 0 == strcmp(key_object, "0x0001") )
                {
                    //APP_VERSION => Firmware Version
                    strncpy(app_ver, value, app_ver_len-1);
                    app_ver[app_ver_len-1] = '\0';
                }
                else if( 0 == strcmp(key_object, "0x0004") )
                {
                    strncpy(manufacturer, value, manufacturer_len-1);
                    manufacturer[manufacturer_len-1] = '\0';
                }
                else if( 0 == strcmp(key_object, "0x0005") )
                {
                    strncpy(model_code, value, model_code_len-1);
                    model_code[model_code_len-1] = '\0';
                }

                state = NAME;
              }
            }

            //Last object value
            if( name_in_object_tokens ==  0 )
            {
              state = STOP;
            }
            break;

        case STOP:
            break;
        }
    }


    if( tokens )
    {
        free(tokens);
    }

    free(js);

    result = true;

    return result;
}

static bool sm_GetEndPoint(const char *pEUID, int *endpoint)
{
    bool result = true;
    char zbFileName[80] = {0,};
    int end_point = 0;

    if( !(result = sm_CheckZigbeeFile(pEUID, zbFileName, sizeof(zbFileName), &end_point)) )
    {
        return result;
    }

    if( end_point == 0 )
    {
        return false;
    }

    *endpoint = end_point;

    Debug("sm_GetEndPoint: EUID = %s, endpoint = %d\n", pEUID, *endpoint);

    return result;
}

static char* sm_GetDefaultFriendlyName(char *pDeviceType, char *pModelCode)
{
    if( strcmp(pDeviceType, ZB_SENSOR) == 0 )
    {
      if ( 0 == pModelCode)
      {
        Debug("sm_GetDefaultFriendlyName: DeviceType = %s, ModelCode = NULL\n", pDeviceType);
        return SENSOR_DW;
      }

      if ( 0 == strcmp (pModelCode, SENSOR_MODELCODE_DW))
      {
        return SENSOR_DW;
	  }
	  else if ( 0 == strcmp (pModelCode, SENSOR_MODELCODE_FOB))
      {
        return SENSOR_FOB;
	  }
	  else if ( 0 == strcmp (pModelCode, SENSOR_MODELCODE_ALARM))
	  {
	    return SENSOR_ALARM;
	  }
	  else if ( 0 == strcmp (pModelCode, SENSOR_MODELCODE_PIR))
	  {
	    return SENSOR_PIR;
	  }

	  return ZB_SENSOR;
    }
    else if( strcmp(pDeviceType, ZB_WEMOLINK) == 0 )
    {
        return ZB_WEMOLINK;
    }
    else if( strcmp(pDeviceType, ZB_ONOFFBULB) == 0 )
    {
        return ZB_ONOFFBULB;
    }
    else if( strcmp(pDeviceType, ZB_LIGHTBULB) == 0 )
    {
        return ZB_LIGHTBULB;
    }
    else if( strcmp(pDeviceType, ZB_COLORBULB) == 0 )
    {
        return ZB_COLORBULB;
    }
    else if( strcmp(pDeviceType, ZB_UNKNOWN) == 0 )
    {
        return ZB_UNKNOWN;
    }
}

//-----------------------------------------------------------------------------
static unsigned sm_GetDeviceInfo(SM_Device device, SM_DeviceData device_data, SM_DeviceScanOption* option)
{
    bool        result = false;
    unsigned    updated = 0;
    TZBParam    param;
    int         endpoint = 1;
    int         rc = 0;
    int         once = 0, i = 0, p = 0;
    char        *pOSRAMBulbName = NULL;

    Debug("sm_GetDeviceInfo Entry.... \n");

    for (once = 1; once; once--)
    {
        if( !(device->Attributes & DA_JOINED) )
        {
            device->EUID.Type = SD_DEVICE_EUID;
            EUI64ToString(device_data->DeviceID.node_eui, device->EUID.Data);

            // checking zbid.EUID.01(e.g: zbid.000D6F0003E2BB14.01) to determine whether subdevice is Link or Bulb or Sensors
            memset(&param, 0x00, sizeof(TZBParam));

            if( (rc = sm_ParseZigbeeClusterJSON(device->EUID.Data, &param, device->DeviceType,
                sizeof(device->DeviceType), device_data->ClusterList, ZD_MAX_CLUSTER_COUNT)) )
            {
                if( rc == SEND_ZB_GET_ENDPOINT )
                {
                    memset(&param, 0x00, sizeof(TZBParam));

                    if( sm_DeviceSendCommand(device_data, &param, ZB_CMD_GET_ENDPOINT, (int)&endpoint,
                        1, 0, 0, RETRY_0, 100) )
                    {
                        Debug("sm_GetDeviceInfo:DA_JOINED: ZB_CMD_GET_ENDPOINT cmd : endpoint = [%d] \n", endpoint);

                        device_data->DeviceID.endpoint = endpoint;

                        if( sm_DeviceSendCommand(device_data, &param, ZB_CMD_GET_CLUSTERS,
                            (int)device_data->ClusterList, ZD_MAX_CLUSTER_COUNT, 0, 0, option->CommandRetry,
                            option->CommandRetryInterval) )
                        {
                            device_data->ClusterCount = MIN(param.param1, ZD_MAX_CLUSTER_COUNT);
                            device_data->Type = param.device_type;
                            switch( param.device_type )
                            {
                                case ZB_DEV_IAS_ZONE:
                                    SAFE_STRCPY(device->DeviceType, ZB_SENSOR);
                                    SAFE_STRCPY(device->FriendlyName, sm_GetDefaultFriendlyName(device->DeviceType, device->ModelCode));
                                    break;
                                case ZB_DEV_HOME_GATEWAY:
                                    SAFE_STRCPY(device->DeviceType, ZB_WEMOLINK);
                                    SAFE_STRCPY(device->FriendlyName, ZB_WEMOLINK);
                                    break;
                                case ZB_DEV_LIGHT_ONOFF:
                                    SAFE_STRCPY(device->DeviceType, ZB_ONOFFBULB);
                                    SAFE_STRCPY(device->FriendlyName, ZB_ONOFFBULB);
                                    break;
                                case ZB_DEV_LIGHT_DIMMABLE:
                                    SAFE_STRCPY(device->DeviceType, ZB_LIGHTBULB);
                                    SAFE_STRCPY(device->FriendlyName, ZB_LIGHTBULB);
                                    break;
                                case ZB_DEV_LIGHT_COLOR:
                                    SAFE_STRCPY(device->DeviceType, ZB_COLORBULB);
                                    SAFE_STRCPY(device->FriendlyName, ZB_COLORBULB);
                                    break;
                                case ZB_DEV_UNKNOWN:
                                    SAFE_STRCPY(device->DeviceType, ZB_UNKNOWN);
                                    SAFE_STRCPY(device->FriendlyName, ZB_UNKNOWN);
                                    break;
                            }

                            Debug("sm_GetDeviceInfo:DA_JOINED: ZB_CMD_GET_CLUSTER cmd : cluster count = [%d] \n",
                                device_data->ClusterCount);
                        }
                        else
                        {
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
                else if( rc == SEND_ZB_GET_CLUSTER )
                {
                    //Return endpoint as param.ret from sm_ParseZigbeeClusterJSON().
                    device_data->DeviceID.endpoint = param.ret;

                    Debug("sm_GetDeviceInfo:DA_JOINED: ZB_CMD_GET_CLUSTER cmd : endpoint = [%d] \n",
                            device_data->DeviceID.endpoint);

                    memset(&param, 0x00, sizeof(TZBParam));

                    if( sm_DeviceSendCommand(device_data, &param, ZB_CMD_GET_CLUSTERS, (int)device_data->ClusterList, ZD_MAX_CLUSTER_COUNT, 0, 0, option->CommandRetry, option->CommandRetryInterval) )
                    {
                        device_data->ClusterCount = MIN(param.param1, ZD_MAX_CLUSTER_COUNT);
                        device_data->Type = param.device_type;
                        switch( param.device_type )
                        {
                            case ZB_DEV_IAS_ZONE:
                                SAFE_STRCPY(device->DeviceType, ZB_SENSOR);
                                SAFE_STRCPY(device->FriendlyName, sm_GetDefaultFriendlyName(device->DeviceType, device->ModelCode));
                                break;
                            case ZB_DEV_HOME_GATEWAY:
                                SAFE_STRCPY(device->DeviceType, ZB_WEMOLINK);
                                SAFE_STRCPY(device->FriendlyName, ZB_WEMOLINK);
                                break;
                            case ZB_DEV_LIGHT_ONOFF:
                                SAFE_STRCPY(device->DeviceType, ZB_ONOFFBULB);
                                SAFE_STRCPY(device->FriendlyName, ZB_ONOFFBULB);
                                break;
                            case ZB_DEV_LIGHT_DIMMABLE:
                                SAFE_STRCPY(device->DeviceType, ZB_LIGHTBULB);
                                SAFE_STRCPY(device->FriendlyName, ZB_LIGHTBULB);
                                break;
                            case ZB_DEV_LIGHT_COLOR:
                                SAFE_STRCPY(device->DeviceType, ZB_COLORBULB);
                                SAFE_STRCPY(device->FriendlyName, ZB_COLORBULB);
                                break;
                            case ZB_DEV_UNKNOWN:
                                SAFE_STRCPY(device->DeviceType, ZB_UNKNOWN);
                                SAFE_STRCPY(device->FriendlyName, ZB_UNKNOWN);
                                break;
                        }

                        Debug("sm_GetDeviceInfo:DA_JOINED: ZB_CMD_GET_CLUSTER cmd : cluster count = [%d] \n",
                                device_data->ClusterCount);
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else
            {
                device_data->ClusterCount = MIN(param.param1, ZD_MAX_CLUSTER_COUNT);
                device_data->Type = param.device_type;
            }

            if( device->DeviceType[0] && !device->FriendlyName[0] )
            {
                SAFE_STRCPY(device->FriendlyName, sm_GetDefaultFriendlyName(device->DeviceType, device->ModelCode));
            }
            SAFE_STRCPY(device->ManufacturerName, scUnknown);

            Debug("sm_GetDeviceInfo:DA_JOINED device->DeviceType = [%s] \n", device->DeviceType);
            Debug("sm_GetDeviceInfo:DA_JOINED device->FriendlyName = [%s] \n", device->FriendlyName);
            Debug("sm_GetDeviceInfo:DA_JOINED device->ManufacturerName = [%s] \n", device->ManufacturerName);

            //Get an end point of Zigbee device
            result = sm_GetEndPoint(device->EUID.Data, &endpoint);

            if( result )
            {
                device_data->DeviceID.endpoint = endpoint;
            }
            else
            {
                break;
            }

            //Set a device udn of zigbee device
            sm_SetDeviceUDN(device);

            Debug("sm_GetDeviceInfo:DA_ENDPOINT device_data->DeviceID.endpoint = [%d] \n",
                    device_data->DeviceID.endpoint);

            //Set cluster info to the cluster list
            sm_SetDeviceClusterExtension(device_data);

            device->Attributes |= (DA_JOINED | DA_ENDPOINT | DA_CLUSTER);
            updated |= (DA_JOINED | DA_ENDPOINT | DA_CLUSTER);
        }

        if( option->DeviceInfo != SCAN_MINIMUM )
        {
            if( !(device->Attributes & DA_BASIC_CLUSTER) )
            {
                result = sm_ParseZigbeeBasicClusterJSON(device->EUID.Data,
                        device->FirmwareVersion, sizeof(device->FirmwareVersion),
                        device->ManufacturerName, sizeof(device->ManufacturerName),
                        device->ModelCode, sizeof(device->ModelCode));
                if( !result )
                {
                    memset(&param, 0x00, sizeof(TZBParam));
                    if( sm_DeviceSendCommand(device_data, &param, ZB_CMD_GET_MODELCODE, 1, 0, 0, 0, RETRY_0, 100) )
                    {
                        Debug("sm_GetDeviceInfo:DA_BASIC_CLUSTER: send ZB_CMD_GET_MODELCODE command... \n");
                    }
                    break;
                }

                //Getting a model code of CREE bulb if it is not existed after pairing : WEMO-43517
                if( !device->ModelCode[0] )
                {
                    memset(&param, 0x00, sizeof(TZBParam));
                    if( sm_DeviceSendCommand(device_data, &param, ZB_CMD_GET_MODELCODE,
                                            0,0,0,0, RETRY_1, 100) )
                    {
                        SAFE_STRCPY(device->ModelCode, (char*)param.str);
                    }
                }

                Debug("sm_ParseZigbeeBasicClusterJSON: %s, %s, %s ...\n",
                        device->FirmwareVersion, device->ManufacturerName, device->ModelCode);

                p = 0;

                for( i = 0; i < sizeof(scZigBeeTypeNamePreset)/sizeof(SM_ZigBeeTypeNamePreset); i++ )
                {
                    if( device_data->Type == scZigBeeTypeNamePreset[i].Type )
                    {
                        p = i;
                        break;
                    }
                }

                SAFE_STRCPY(device->DeviceType, scZigBeeTypeNamePreset[p].Name);
                SAFE_STRCPY(device->FriendlyName, scZigBeeTypeNamePreset[p].Name);

                if( 0 == strcmp(device->DeviceType, ZB_WEMOLINK) )
                {
                    SAFE_STRCPY(device->ModelCode, "Bridge");
                    SAFE_STRCPY(device->ManufacturerName, "Belkin");
                    SAFE_STRCPY(device->FirmwareVersion, "0x01");
                }
                else if( 0 == strcmp(device->DeviceType, ZB_SENSOR) )
                {
                    SAFE_STRCPY(device->FriendlyName, sm_GetDefaultFriendlyName(device->DeviceType, device->ModelCode));
                }

                if( (pOSRAMBulbName = sm_GetOSRAMFriendlyName(device->ModelCode, device->ManufacturerName)) )
                {
                    SAFE_STRCPY(device->FriendlyName, pOSRAMBulbName);
                }

                convert_fw_version(device->FirmwareVersion, sizeof(device->FirmwareVersion));

                Debug("sm_GetDeviceInfo:DA_CLUSTER DeviceType = [%s], FirmwareVersion = [%s], ManufacturerName = [%s],ModelCode = [%s], FriendlyName = [%s] \n",
                        device->DeviceType, device->FirmwareVersion, device->ManufacturerName,
                        device->ModelCode, device->FriendlyName);

                device->Attributes |= (DA_FIRMWARE_VERSION | DA_MFG_NAME | DA_BASIC_CLUSTER);
                updated |= (DA_FIRMWARE_VERSION | DA_MFG_NAME | DA_BASIC_CLUSTER);
            }

            if( !(device->Attributes & DA_FIRMWARE_VERSION) )
            {
                if( sm_DeviceSendCommand(device_data, &param, ZB_CMD_GET_APPVERSION, 0,0,0,0,
                                        option->CommandRetry, option->CommandRetryInterval) )
                {
                    sprintf(device->FirmwareVersion, "%02X", param.param1);

                    convert_fw_version(device->FirmwareVersion, sizeof(device->FirmwareVersion));

                    device->Attributes |= DA_FIRMWARE_VERSION;
                    updated |= DA_FIRMWARE_VERSION;
                }
            }
        }
    }

    Debug("sm_GetDeviceInfo Exit : updated = [0x%x]... \n", updated);

    return updated;
}
//-----------------------------------------------------------------------------
static unsigned sm_AddDeviceProperty(SM_DeviceConfiguration* configuration, SM_JoinedDevice joined, int* index, bool allow_rejoining)
{
    unsigned        updated = 0;
    char*           error = 0;
    SM_Device       device = 0;
    SM_DeviceData   device_data = 0;
    SD_DeviceID     euid;
    int             once, d;

    // checking device ID

    for (d = 0; d < configuration->DeviceCount; d++)
    {
        device      = configuration->DeviceList[d];
        device_data = (SM_DeviceData)device->DeviceSpecific;

        if (memcmp(device_data->DeviceID.node_eui, joined->DeviceID.node_eui, sizeof(TEUID_64)) == 0)
        {
            Debug("Device Rejoined: %s:%04X \n", EUI64ToString(joined->DeviceID.node_eui, euid.Data), joined->DeviceID.node_id);

            *index = d;

            if (allow_rejoining)
            {
                Debug("Device Rejoined: allow_rejoining, all device are removed... \n");

                SM_RemoveDeviceFile(device);
                SM_RemoveDeviceFromGroup(device);

                device->Paired = false;
                device->Attributes = 0;
            }
            else
            {
                if( (device->Attributes & DA_JOINED) && (device->Attributes & DA_BASIC_CLUSTER) )
                {
                    Debug("Device Rejoined: DA_JOINED and DA_BASIC_CLUSTER \n");
                    updated |= DA_BASIC_CLUSTER;
                    return updated;
                }
            }

            device_data->DeviceID = joined->DeviceID;

            updated = sm_GetDeviceInfo(device, device_data, &configuration->DeviceScanOption);

            return updated;
        }
    }

    Debug("Device Joined: %s:%04X \n", EUI64ToString(joined->DeviceID.node_eui, euid.Data), joined->DeviceID.node_id);

    // allocating new SD_DeviceProperty + SM_ZigBeeSpecificData

    device      = 0;
    device_data = 0;

    for (once=1; once; once--)
    {
        if (DEVICE_MAX_COUNT <= d)
        {
            error = "DEVICE_MAX_COUNT";

            //// SD_TODO - Rejecting Device

            break;
        }

        device      = (SM_Device)calloc(1, sizeof(SD_DeviceProperty));
        device_data = (SM_DeviceData)calloc(1, sizeof(SM_ZigBeeSpecificData));

        if (!device || !device_data)
        {
            if (device)
                free(device);

            if (device_data)
                free(device_data);

            error = "Out of Memory";
            break;
        }

        device_data->DeviceID = joined->DeviceID;

        updated = sm_GetDeviceInfo(device, device_data, &configuration->DeviceScanOption);

        if( updated )
        {
            Debug("sm_AddDeviceProperty : sm_GetDeviceInfo = 0x%x", updated);

            device->DeviceSpecific = device_data;

            // updating DeviceList

            *index = configuration->DeviceCount;

            configuration->DeviceList[configuration->DeviceCount] = device;
            configuration->DeviceCount++;
        }
        else
        {
            if( device )
                free(device);
            if( device_data )
                free(device_data);
        }
    }

    if (error)
        Debug("sm_AddDeviceProperty(): %s \n", error);

    Debug("sm_AddDeviceProperty : updated = 0x%x", updated);

    return updated;
}
//-----------------------------------------------------------------------------
static void sm_MakeCapabilityID(SD_CapabilityID* capability_id, SM_DeviceControl control)
{
    unsigned    cluster = control->Cluster;

    if (control->Set || control->Get)
        cluster |= CAPABILITY_ID_ZIGBEE;

    capability_id->Type = SD_CAPABILITY_ID;
    sprintf(capability_id->Data, "%X", cluster);
}
//-----------------------------------------------------------------------------
static void sm_AddCapabilityProperty(SM_DeviceConfiguration* configuration, SM_Device device)
{
    char*               error = 0;
    SM_DeviceData       device_data;
    SM_Capability       capability;
    SM_DeviceControl    control;
    unsigned            cluster;
    int                 cl, cp;
    bool                result, update_capability = false;

    device_data = (SM_DeviceData)device->DeviceSpecific;

    if (!(device->Attributes & DA_CLUSTER))
        return;

    for (cl = 0; cl < device_data->ClusterCount; cl++)
    {
        // checking Capability in list

        cluster = device_data->ClusterList[cl];

        Debug("Cluster[%d]: %04X \n", cl, cluster);

        // checking Belkin Certified Device

        if (cluster == ZCL_BELKIN_MFG_SPECIFIC_CLUSTER_ID)
        {
            Debug("Belkin Certified Device \n");
            device->Certified = cluster;
            continue;
        }

        // checking current Capability list

        for (cp = 0; cp < configuration->CapabilityCount; cp++)
        {
            control = (SM_DeviceControl)configuration->CapabilityList[cp]->DeviceSpecific;

            if (cluster == control->Cluster) // found
            {
                Debug("Capability[%d] %s\n", cp, configuration->CapabilityList[cp]->ProfileName);
                break;
            }
        }

        if (cp < configuration->CapabilityCount)
            continue;

        result = false;

        if (CAPABILITY_MAX_COUNT <= cp)
        {
            error = "CAPABILITY_MAX_COUNT";
            break;
        }

        // checking scZigBeeCapabilityPreset

        for (cp = 0; cp < sizeof(scZigBeeCapabilityPreset)/sizeof(SM_ZigBeeCapabilityPreset); cp++)
        {
            if (cluster != scZigBeeCapabilityPreset[cp].Control.Cluster)
                continue;

            // allocating new SD_CapabilityProperty + SM_CapabilitySpecificControl

            capability  = (SM_Capability)calloc(1, sizeof(SD_CapabilityProperty));
            control     = (SM_DeviceControl)calloc(1, sizeof(SM_ZigBeeSpecificControl));

            if (!capability || !control)
            {
                error = "Out of Memory";

                if (capability)
                    free(capability);
                if (control)
                    free(control);
                break;
            }

            // loading default preset

            *capability = scZigBeeCapabilityPreset[cp].Capability;
            *control    = scZigBeeCapabilityPreset[cp].Control;

            // updating SD_CapabilityProperty

            sm_MakeCapabilityID(&capability->CapabilityID, control);

            sprintf(capability->Spec, "%s", scZigBee);

            // updating CapabilityList

            capability->DeviceSpecific = control;

            Debug("Capability[%d]: %s, %s\n", configuration->CapabilityCount, capability->CapabilityID.Data, capability->ProfileName);

            configuration->CapabilityList[configuration->CapabilityCount] = capability;
            configuration->CapabilityCount++;

            update_capability = true;
            result = true;
            break;
        }

        if (!result)
        {
            Debug("Capability Not Found \n");
        }
    }

    if( update_capability )
    {
        trigger_remote_event(WORK_EVENT, QUERY_CAPABILITY, "query", "capability", NULL, 0);
    }

    if (error)
        Debug("sm_AddCapabilityProperty(): %s \n", error);
}
//-----------------------------------------------------------------------------
static void sm_UpdateNewCapability(SM_DeviceConfiguration* configuration, SM_Device device)
{
    SM_DeviceData       device_data;
    SM_Capability       capability;
    SM_DeviceControl    control;
    unsigned            cluster;
    int                 cl, cp;

    device_data = (SM_DeviceData)device->DeviceSpecific;

    if (!(device->Attributes & DA_CLUSTER))
        return;

    sm_SetDeviceClusterExtension(device_data);

    for (cl = 0; cl < device_data->ClusterCount; cl++)
    {
        cluster = device_data->ClusterList[cl];

        // checking current CapabilityLst

        for (cp = 0; cp < configuration->CapabilityCount; cp++)
        {
            control = (SM_DeviceControl)configuration->CapabilityList[cp]->DeviceSpecific;

            if (cluster == control->Cluster) // loaded
                break;
        }

        if (cp < configuration->CapabilityCount)
            continue;

        if (CAPABILITY_MAX_COUNT <= cp)
            break;

        // checking scZigBeeCapabilityPreset in new firmware

        for (cp = 0; cp < sizeof(scZigBeeCapabilityPreset)/sizeof(SM_ZigBeeCapabilityPreset); cp++)
        {
            if (cluster != scZigBeeCapabilityPreset[cp].Control.Cluster)
                continue;

            // adding new SD_CapabilityProperty + SM_CapabilitySpecificControl

            capability  = (SM_Capability)calloc(1, sizeof(SD_CapabilityProperty));
            control     = (SM_DeviceControl)calloc(1, sizeof(SM_ZigBeeSpecificControl));

            if (!capability || !control)
            {
                Debug("Out of Memory\n");

                if (capability)
                    free(capability);
                if (control)
                    free(control);
                break;
            }

            *capability = scZigBeeCapabilityPreset[cp].Capability;
            *control    = scZigBeeCapabilityPreset[cp].Control;

            sm_MakeCapabilityID(&capability->CapabilityID, control);

            sprintf(capability->Spec, "%s", scZigBee);

            capability->DeviceSpecific = control;

            Debug("New Capability[%d]: %s, %s\n", configuration->CapabilityCount, capability->CapabilityID.Data, capability->ProfileName);

            configuration->CapabilityList[configuration->CapabilityCount] = capability;
            configuration->CapabilityCount++;
        }
    }
}
//-----------------------------------------------------------------------------
static void sm_LinkDeviceCapability(SM_DeviceConfiguration* configuration, SM_Device device)
{
    SM_DeviceData       device_data;
    SM_Capability       capability;
    SM_DeviceControl    control;
    unsigned            cluster;
    int                 cl, cp;

    device_data = (SM_DeviceData)device->DeviceSpecific;

    if (!(device->Attributes & DA_CLUSTER))
        return;

    device->CapabilityCount = 0;

    for (cl = 0; cl < device_data->ClusterCount; cl++)
    {
        cluster = device_data->ClusterList[cl];
        for (cp = 0; cp < configuration->CapabilityCount; cp++)
        {
            capability  = configuration->CapabilityList[cp];
            control     = (SM_DeviceControl)capability->DeviceSpecific;
            if (cluster == control->Cluster)
            {
                if(cluster == ZCE_SENSOR_KEYPRESS)
                {
                    if( 0 == strncmp(device->ModelCode, SENSOR_MODELCODE_FOB, strlen(SENSOR_MODELCODE_FOB)) )
                    {
                        device->Capability[device->CapabilityCount] = capability;
                        device->CapabilityCount++;
                    }
                }
                else
                {
                    device->Capability[device->CapabilityCount] = capability;
                    device->CapabilityCount++;
                }
                break;
            }
        }

        if (DEVICE_MAX_CAPABILITIES <= device->CapabilityCount)
        {
            Debug("sm_LinkDeviceCapability(): DEVICE_MAX_CAPABILITIES \n");
            break;
        }
    }
}

//-----------------------------------------------------------------------------
static void sm_SetCapabilityValueDefault(SM_Device device)
{
    SD_Capability       capability;
    SM_DeviceControl    control;
    int                 cp;
    unsigned            initialValue = 0;
    char                initialValueStr[CAPABILITY_MAX_VALUE_LENGTH] = {0,};

    Debug("%s \n", __func__);
    for(cp = 0; cp < device->CapabilityCount; cp++)
    {
        capability = device->Capability[cp];
        control = (SM_DeviceControl)capability->DeviceSpecific;

        //
        // if Capability empty, then we will fill it with a default value. This is only for Sensors.
        //
        if(device->CapabilityValue[cp][0] == 0)
        {
            switch (control->Cluster)
            {
                case ZCL_IAS_ZONE_CLUSTER_ID:
                    initialValue = IASZONE_BIT_INITIAL_STATUS;

                    // DW will be 0x11. others are 0x10.
                    if( 0 == strncmp(device->ModelCode, SENSOR_MODELCODE_DW, strlen(SENSOR_MODELCODE_DW)) )
                    {
                        initialValue |= IASZONE_BIT_MAIN_EVENT;
                    }

                    snprintf(initialValueStr, sizeof(initialValueStr), "%x", initialValue);
                    SAFE_STRCPY(device->CapabilityValue[cp], initialValueStr);
                    break;

                case ZCE_SENSOR_CONFIG:
                    /*Setting default manual control notification with freq of 15 min for PIR */
                    if (0 == strncmp(device->ModelCode, SENSOR_MODELCODE_PIR, strlen(SENSOR_MODELCODE_PIR)))
                    {
                        SAFE_STRCPY(device->CapabilityValue[cp], "1:900");
                    }
                    else
                    {
                        SAFE_STRCPY(device->CapabilityValue[cp], "1");
                    }
                    break;

                case ZCE_SENSOR_TESTMODE:
                    SAFE_STRCPY(device->CapabilityValue[cp], "0");
                    break;

               case ZCE_SENSOR_KEYPRESS:
                    initialValue = IASZONE_BIT_INITIAL_STATUS;
                    snprintf(initialValueStr, sizeof(initialValueStr), "%x", initialValue);
                    SAFE_STRCPY(device->CapabilityValue[cp], initialValueStr);
                    break;

            default:;
            }
        }
    }
}
//-----------------------------------------------------------------------------
static bool sm_SetDeviceParam(TZBParam* param, const char* data_type, const char* data_value)
{
    char    type[CAPABILITY_MAX_DATA_TYPE_LENGTH];
    char    value[CAPABILITY_MAX_VALUE_LENGTH];
    char*   word_value;
    int     data[ZD_MAX_PARAM_COUNT];
    int     count = 0;

    if (!param || !data_type || !data_value)
        return false;

    memset(param, 0, sizeof(TZBParam));
    memset(data,  0, sizeof(data));

    SAFE_STRCPY(type, data_type);
    SAFE_STRCPY(value, data_value);

    //// SD_TODO - Checking DataType - String(?)

    word_value = strtok(value, scDelimiter);

    while (word_value)
    {
        sscanf(word_value, "%d", &data[count]);
        count++;

        if (ZD_MAX_PARAM_COUNT <= count)
            break;

        word_value = strtok(NULL, scDelimiter);
    }

    param->param1 = data[0];
    param->param2 = data[1];
    param->param3 = data[2];
    param->param4 = data[3];

    return true;
}
//-----------------------------------------------------------------------------
static void sm_GetDeviceParam(TZBParam* param, const char* data_type, char* value)
{
    char    type[CAPABILITY_MAX_DATA_TYPE_LENGTH];
    char*   word;
    char    format[CAPABILITY_MAX_DATA_TYPE_LENGTH/2];
    int     index = 0;

    SAFE_STRCPY(type, data_type);

    word = strtok(type, scDelimiter);

    while (word)
    {
        //// SD_TODO - Checking DataType

        format[index] = ':'; index++;
        format[index] = '%'; index++;
        format[index] = 'd'; index++;

        word = strtok(NULL, scDelimiter);
    }

    format[index] = 0;

    if (index)
        snprintf(value, CAPABILITY_MAX_VALUE_LENGTH, &format[1], param->param1, param->param2, param->param3, param->param4);
    else
        value[0] = 0;
}
//-----------------------------------------------------------------------------
bool SM_LoadDeviceLibrary()
{
    bool    result = true;
    int     once;

    for (once=1; once; once--)
    {
        if (zbInit() != ZB_RET_SUCCESS)
        {
            Debug("SM_LoadDeviceLibrary() zbInit() failed\n");
            result = false;
            break;
        }
    }

    return result;
}
//-----------------------------------------------------------------------------
bool SM_LoadDeviceListFromFile(SM_DeviceConfiguration* configuration)
{
    char*           error = 0;
    SM_Device       device = 0;
    SM_DeviceData   device_data = 0;
    FILE*           file = 0;
    DIR*            dir;
    struct  dirent* entry;
    int             once, read;
    char            filename[256];

    SM_DeviceRecordBlock    block;
    SM_DeviceRecord*        record = &block.Record;

    for (once=1; once; once--)
    {
        dir = opendir(scFileDeviceList);

        if (!dir)
            break;

        while ((entry = readdir(dir)) && entry)
        {
            if ((strcmp(".", entry->d_name) == 0) || (strcmp("..", entry->d_name) == 0))
                continue;

            sprintf(filename, "%s/%s", scFileDeviceList, entry->d_name);

            file = fopen(filename, "rb");

            if (!file)
            {
                error = "Open Failed";
                break;
            }

            read = fread(&block, 1, sizeof(block), file);

            if (read != sizeof(block))
            {
                Debug("SM_LoadDeviceListFromFile(): Read Error <%s> \n", filename);

                fclose(file);
                file = 0;
                continue;
            }

            if (record->Version != SD_REOCRD_VERSION)
            {
                error = "Version Mismatch";
                break;
            }

            // allocating new SD_DeviceProperty + SM_ZigBeeSpecificData

            device      = (SM_Device)calloc(1, sizeof(SD_DeviceProperty));
            device_data = (SM_DeviceData)calloc(1, sizeof(SM_ZigBeeSpecificData));

            if (!device || !device_data)
            {
                if (device)
                    free(device);
                if (device_data)
                    free(device_data);

                error = "Out of Memory";
                break;
            }

            device_data->DeviceID                 = record->DeviceID;
            device_data->Type                     = record->Type;
            device_data->ClusterCount             = record->ClusterCount;
            SM_MEMCPY(device_data->ClusterList,     record->ClusterList);

            SM_STRNCPY(device->EUID.Data,           record->EUID);
                       device->EUID.Type          = SD_DEVICE_EUID;
            SM_STRNCPY(device->RegisteredID.Data,   record->RegisteredID);
                       device->RegisteredID.Type  = SD_DEVICE_REGISTERED_ID;
            device->Paired                        = true;
            SM_STRNCPY(device->DeviceType,          record->DeviceType);
            SM_STRNCPY(device->ModelCode,           record->ModelCode);
            SM_STRNCPY(device->Serial,              record->Serial);
            SM_STRNCPY(device->FirmwareVersion,     record->FirmwareVersion);
            SM_STRNCPY(device->FriendlyName,        record->FriendlyName);
            SM_STRNCPY(device->ManufacturerName,    record->ManufacturerName);
            device->Certified                     = record->Certified;
            device->Available                     = record->Available;

            device->Attributes                    = record->Attributes;
            device->Properties                    = record->Attributes;
            device->DeviceSpecific                = device_data;

            // updating DeviceList

            configuration->DeviceList[configuration->DeviceCount] = device;
            configuration->DeviceCount++;

            fclose(file);
            file = 0;

            if (DEVICE_MAX_COUNT <= configuration->DeviceCount)
                break;
        }

        if (file)
            fclose(file);

        closedir(dir);
    }

    if (error)
        Debug("SM_LoadDeviceListFromFile(): %s \n", error);
    else
        Debug("DeviceList Loaded: %d \n", configuration->DeviceCount);

    return !error;
}
//-----------------------------------------------------------------------------
bool SM_SaveDeviceListToFile(SM_DeviceConfiguration* configuration)
{
    bool            result = false;
    SM_Device       device;
    SM_DeviceData   device_data;
    FILE*           file = 0;
    int             once, i, size;
    struct stat     s;
    char            filename[256];

    SM_DeviceRecordBlock    block;
    SM_DeviceRecord*        record = &block.Record;

    //// SD_TODO - transaction

    for (once=1; once; once--)
    {
        if ((stat(scFileDeviceList, &s) == 0) && !S_ISDIR(s.st_mode))
            remove(scFileDeviceList);

        mkdir(scFileDeviceList, 0755);

        for (i = 0; i < configuration->DeviceCount; i++)
        {
            device      = configuration->DeviceList[i];
            device_data = (SM_DeviceData)device->DeviceSpecific;

            if (!device->Paired || (device->Attributes == device->Properties))
                continue;

            sprintf(filename, "%s/%s", scFileDeviceList, device->EUID.Data);

            file = fopen(filename, "wb");

            if (!file)
            {
                Debug("SM_SaveDeviceListToFile(): Open Failed <%s>\n", filename);
                break;
            }

            memset(&block, 0, sizeof(block));

            record->Version                   = SD_REOCRD_VERSION;
            record->Attributes                = device->Attributes | DA_SAVED;

            record->DeviceID                  = device_data->DeviceID;
            record->Type                      = device_data->Type;
            record->ClusterCount              = device_data->ClusterCount;
            SM_MEMCPY(record->ClusterList,      device_data->ClusterList);

            SM_STRNCPY(record->EUID,            device->EUID.Data);
            SM_STRNCPY(record->RegisteredID,    device->RegisteredID.Data);
            SM_STRNCPY(record->DeviceType,      device->DeviceType);
            SM_STRNCPY(record->ModelCode,       device->ModelCode);
            SM_STRNCPY(record->Serial,          device->Serial);
            SM_STRNCPY(record->FirmwareVersion, device->FirmwareVersion);
            SM_STRNCPY(record->FriendlyName,    device->FriendlyName);
            SM_STRNCPY(record->ManufacturerName,device->ManufacturerName);
            record->Certified                 = device->Certified;
            record->Available                 = device->Available;

            size = fwrite(&block, 1, sizeof(block), file);

            if (!size)
            {
                Debug("SM_SaveDeviceListToFile(): Write Failed \n");
                break;
            }

            device->Attributes = record->Attributes;
            device->Properties = record->Attributes;

            fclose(file);
            file = 0;
        }

        if (file)
            fclose(file);

        if (i == configuration->DeviceCount)
            result = true;
    }

    return result;
}

static bool IsSleepyDevice(char *model_code)
{
    if ((strcmp(model_code, SENSOR_MODELCODE_PIR) == 0) || (strcmp(model_code, SENSOR_MODELCODE_ALARM) == 0) ||  (strcmp(model_code, SENSOR_MODELCODE_FOB) == 0)  || (strcmp(model_code, SENSOR_MODELCODE_DW) == 0))
        return true;


    return false;
}

bool SM_SaveDevicePropertiesListToFile(SM_DeviceConfiguration* configuration)
{
    bool            result = false;
    SM_Device       device;
    //SM_DeviceData   device_data;
    FILE*           file = 0;
    int             once, i, size;
    struct stat     s;
    char            filename[256];

    SM_DevicePropertyRecordBlock    block;
    SD_DeviceSimpleProperty*        record = &block.Record;

    //// SD_TODO - transaction

    printf("[Subdeivce] is called %s, %d \n", __FUNCTION__, __LINE__);

    for (once=1; once; once--)
    {
        if ((stat(scFileDevicePropertyList, &s) == 0) && !S_ISDIR(s.st_mode))
            remove(scFileDevicePropertyList);

        mkdir(scFileDevicePropertyList, 0755);
        //printf("[Subdeivce] Debugging %s, %d \n", __FILE__, __LINE__);

        for (i = 0; i < configuration->DeviceCount; i++)
        {
            //printf("[Subdeivce] Debugging %s, %d \n", __FILE__, __LINE__);
            device      = configuration->DeviceList[i];
            //device_data = (SM_DeviceData)device->DeviceSpecific;

            if (!device->Paired )
            {
                Debug("[Subdevice] %s %d device %s is not paired  \n",  __FUNCTION__, __LINE__, device->EUID.Data);

                continue;
            }
            if (IsSleepyDevice(device->ModelCode) == false)
            {
                Debug("[Subdevice] %s %d device %s is not sleepy device  \n",  __FUNCTION__, __LINE__, device->EUID.Data);

                continue;
            }


            result = true;
            sprintf(filename, "%s/%s", scFileDevicePropertyList, device->EUID.Data);


            file = fopen(filename, "wb");

            if (!file)
            {
                Debug("SM_SaveDevicePropertyListToFile(): Open Failed <%s>\n", filename);
                break;
            }

            memset(&block, 0, sizeof(block));

            SM_STRNCPY(record->EUID.Data,            device->EUID.Data);
            record->EUID.Type = device->EUID.Type;
            record->Available =       device->Available;
            record->Attributes =       device->Attributes;
            record->Properties =       device->Properties;
            record->LastReportedTime =       device->LastReportedTime;
            record->LastReportedTimeExt =       device->LastReportedTimeExt;

            // copy the capability
            memcpy(&record->CapabilityValue[0][0], &device->CapabilityValue[0][0], DEVICE_MAX_CAPABILITIES*CAPABILITY_MAX_VALUE_LENGTH);


            // write 2 blocks, which is 1024 in bytes for each block
            // It would accelarate writing performance other than write 2 block at a same time
            size = fwrite(&block, 1, sizeof(block), file);

            if (size != sizeof(block))
            {
                Debug("SM_SaveDeviceListToFile(): Write Failed \n");
                break;
            }
            printf("[Subdeivce] Save Sensor's property, whose EUID: %s to file name: %s \n",  device->EUID.Data, filename);

            fclose(file);
            file = 0;
        }

        if (file)
            fclose(file);

    }

    return result;
}

static SD_Device SM_searchDeviceBasedOnEUID(SM_DeviceConfiguration* configuration, char *EUID)
{

    SD_Device device = 0;


    configuration->DeviceList[configuration->DeviceCount] = device;


    int i = 0;
    for (; i < configuration->DeviceCount; i++)
    {
        if (strncmp(configuration->DeviceList[i]->EUID.Data, EUID, sizeof(configuration->DeviceList[i]->EUID.Data)) == 0)
        {
            device = configuration->DeviceList[i];
            break;
        }
    }
    return device;

}


bool SM_LoadDevicePropertiesListFromFile(SM_DeviceConfiguration* configuration)
{
    char*           error = 0;
    SM_Device       device = 0;
    FILE*           file = 0;
    DIR*            dir;
    struct  dirent* entry;
    int             once, read;
    char            filename[256];

    SM_DevicePropertyRecordBlock    block;
    SD_DeviceSimpleProperty*        record = &block.Record;

    printf("[Subdeivce] is called %s, %d \n", __FUNCTION__, __LINE__);

    for (once=1; once; once--)
    {
        dir = opendir(scFileDevicePropertyList);

        if (!dir)
            break;

        while ((entry = readdir(dir)) && entry)
        {

            if ((strcmp(".", entry->d_name) == 0) || (strcmp("..", entry->d_name) == 0))
                continue;



            sprintf(filename, "%s/%s", scFileDevicePropertyList, entry->d_name);


            file = fopen(filename, "rb");

            if (!file)
            {
                error = "Open Failed";
                break;
            }


            read = fread(&block, 1, sizeof(block), file);

            if (read != sizeof(block))
            {
                Debug("SM_LoadDeviceListFromFile(): Read Error <%s> \n", filename);

                fclose(file);
                file = 0;
                continue;
            }


            // Update device property to corresponding sensor device in device list
            // step 1: searching device base on EUID
            device = SM_searchDeviceBasedOnEUID(configuration, record->EUID.Data);
            if (!device)
            {
                // can not search the device with EUID
                Debug("[SubDevice][Warning]: Can not searching EUID = %s  in the list of device \n",
                            record->EUID.Data);
                fclose(file);
                file = 0;
                continue;
            }



            // Step 2: Update the device property to a sensor device in device list
            SM_STRNCPY(device->EUID.Data, record->EUID.Data);
            device->EUID.Type = record->EUID.Type;
            device->Available = record->Available;
            device->Attributes = record->Attributes;
            device->Properties = record->Properties;
            device->LastReportedTime = record->LastReportedTime;
            device->LastReportedTimeExt = record->LastReportedTimeExt;

            // copy the capability
            memcpy(&device->CapabilityValue[0][0], &record->CapabilityValue[0][0] , DEVICE_MAX_CAPABILITIES*CAPABILITY_MAX_VALUE_LENGTH);



            printf("[Subdevice] Load sensor's property %s from file %s \n", device->EUID.Data, filename);

            fclose(file);
            file = 0;

            if (DEVICE_MAX_COUNT <= configuration->DeviceCount)
                break;
        }

        if (file)
            fclose(file);

        closedir(dir);

        char cmd_rm[256];
        sprintf(cmd_rm, "rm -rf %s", scFileDevicePropertyList);
        system(cmd_rm);
    }

    if (error)
        Debug("SM_LoadDeviceListFromFile(): %s \n", error);
    else
        Debug("DeviceList Loaded: %d \n", configuration->DeviceCount);

    return !error;
}


//-----------------------------------------------------------------------------
void SM_RemoveDeviceFile(SD_Device device)
{
    char    filename[256];

    sprintf(filename, "%s/%s", scFileDeviceList, device->EUID.Data);
    remove(filename);
}
//-----------------------------------------------------------------------------
bool SM_LoadCapabilityListFromFile(SM_DeviceConfiguration* configuration)
{
    bool                result = false;
    SM_Capability       capability = 0;
    SM_DeviceControl    control = 0;
    FILE*               file = 0;
    char*               error = 0;
    int                 once, count, i, read;
    struct stat         s;

    SM_CapabilityRecordBlock    block;
    SM_CapabilityRecord*        record = &block.Record;

    if (stat(sCapabilityListInfo.ListFile, &s) != 0)
        return false;

    count = s.st_size / sizeof(block);

    if (CAPABILITY_MAX_COUNT < count)
        count = CAPABILITY_MAX_COUNT;

    for (once=1; once; once--)
    {
        file = fopen(sCapabilityListInfo.ListFile, "rb");

        if (!file)
            break;

        for (i = 0; i < count; i++)
        {
            read = fread(&block, 1, sizeof(block), file);

            if (read != sizeof(block))
            {
                error = "Read Error";
                break;
            }

            if (record->Version != SD_REOCRD_VERSION)
            {
                error = "Version Mismatch";
                break;
            }

            // allocating new SD_CapabilityProperty + SM_CapabilitySpecificControl

            capability  = (SM_Capability)calloc(1, sizeof(SD_CapabilityProperty));
            control     = (SM_DeviceControl)calloc(1, sizeof(SM_ZigBeeSpecificControl));

            if (!capability || !control)
            {
                if (capability)
                    free(capability);
                if (control)
                    free(control);

                error = "Out of Memory";
                break;
            }

            control->Cluster                          = record->Cluster;

            SM_STRNCPY(capability->CapabilityID.Data,   record->CapabilityID);
                       capability->CapabilityID.Type  = SD_CAPABILITY_ID;
            SM_STRNCPY(capability->RegisteredID.Data,   record->RegisteredID);
                       capability->RegisteredID.Type  = SD_CAPABILITY_REGISTERED_ID;
            SM_STRNCPY(capability->Spec,                record->Spec);
            SM_STRNCPY(capability->ProfileName,         record->ProfileName);
            SM_STRNCPY(capability->Control,             record->Control);
            SM_STRNCPY(capability->DataType,            record->DataType);
            SM_STRNCPY(capability->AttrName,            record->AttrName);
            SM_STRNCPY(capability->NameValue,           record->NameValue);

            // updating CapabilityList

            capability->DeviceSpecific = control;

            configuration->CapabilityList[configuration->CapabilityCount] = capability;
            configuration->CapabilityCount++;
        }

        sCapabilityListInfo.ListSaved = configuration->CapabilityCount;

        if (i == count)
        {
            Debug("CapabilityList Loaded: %d \n", count);
            result = true;
        }
    }

    if (file)
        fclose(file);

    if (error)
        Debug("SM_LoadCapabilityListFromFile(): %s \n", error);

    return result;;
}
//-----------------------------------------------------------------------------
bool SM_SaveCapabilityListToFile(SM_DeviceConfiguration* configuration, bool force_update)
{
    bool                result = false;
    SM_Capability       capability = 0;
    SM_DeviceControl    control = 0;
    FILE*               file = 0;
    int                 once, i, size;

    SM_CapabilityRecordBlock    block;
    SM_CapabilityRecord*        record = &block.Record;

    if (!force_update && (configuration->CapabilityCount == sCapabilityListInfo.ListSaved))
        return true;

    //// SD_TODO - transaction

    for (once=1; once; once--)
    {
        file = fopen(sCapabilityListInfo.ListFile, "wb");

        if (!file)
        {
            Debug("SM_SaveCapabilityListToFile(): Open Failed \n");
            break;
        }

        memset(&block, 0, sizeof(block));
        record->Version = SD_REOCRD_VERSION;

        for (i = 0; i < configuration->CapabilityCount; i++)
        {
            capability  = configuration->CapabilityList[i];
            control     = (SM_DeviceControl)capability->DeviceSpecific;

            record->Cluster                   = control->Cluster;

            SM_STRNCPY(record->CapabilityID,    capability->CapabilityID.Data);
            SM_STRNCPY(record->RegisteredID,    capability->RegisteredID.Data);
            SM_STRNCPY(record->Spec,            capability->Spec);
            SM_STRNCPY(record->ProfileName,     capability->ProfileName);
            SM_STRNCPY(record->Control,         capability->Control);
            SM_STRNCPY(record->DataType,        capability->DataType);
            SM_STRNCPY(record->AttrName,        capability->AttrName);
            SM_STRNCPY(record->NameValue,       capability->NameValue);

            size = fwrite(&block, 1, sizeof(block), file);

            if (!size)
            {
                Debug("SM_SaveCapabilityListToFile(): Write Failed \n");
                break;
            }
        }

        sCapabilityListInfo.ListSaved = i;

        if (i == configuration->CapabilityCount)
            result = true;
    }

    if (file)
        fclose(file);

    return result;
}



void SM_UpdateDeviceAvailable(SM_DeviceConfiguration* configuration, const SD_DeviceID* device_id, unsigned nAvailable)
{
    SM_Device device;
    int cnt = 0;

    for( cnt = 0; cnt < configuration->DeviceCount; cnt++ )
    {
        device = configuration->DeviceList[cnt];
        if( 0 == strcmp(device->EUID.Data, device_id->Data) )
        {
            const char * device_alive = "Available";
            const char * device_dead  = "Dead";
            const char * device_init  = "InitialStatus";
            const char * device_status_str = device_init;

            device->Available = nAvailable;

            if( device->Available == SD_ALIVE )
                device_status_str = device_alive;
            else if( device->Available == SD_DEAD )
                device_status_str = device_dead;

            Debug("SM_UpdateDeviceAvailable: device %s is %s.\n", device_id->Data, device_status_str);
        }
    }
}

//-----------------------------------------------------------------------------
void SM_UpdateDeviceIDs(SM_DeviceConfiguration* configuration)
{
    SM_Device           device;
    int                 d;

    for (d = 0; d < configuration->DeviceCount; d++)
    {
        device = configuration->DeviceList[d];

        sm_SetDeviceUDN(device);
    }
}
//-----------------------------------------------------------------------------
void SM_UpdateCapabilityList(SM_DeviceConfiguration* configuration)
{
    SM_DeviceControl    control;
    int                 c, p;

    for (c = 0; c < configuration->CapabilityCount; c++)
    {
        control = (SM_DeviceControl)configuration->CapabilityList[c]->DeviceSpecific;

        for (p = 0; p < sizeof(scZigBeeCapabilityPreset)/sizeof(SM_ZigBeeCapabilityPreset); p++)
        {
            if (control->Cluster == scZigBeeCapabilityPreset[p].Control.Cluster)
            {
                *control = scZigBeeCapabilityPreset[p].Control;
                break;
            }
        }
    }
}
//-----------------------------------------------------------------------------
void SM_UpdateDeviceConfiguration(SM_DeviceConfiguration* configuration)
{
    int     d;

    Debug("%s \n", __func__);
    for (d = 0; d < configuration->DeviceCount; d++)
    {
        sm_UpdateNewCapability(configuration, configuration->DeviceList[d]);
        sm_LinkDeviceCapability(configuration, configuration->DeviceList[d]);
        sm_SetCapabilityValueDefault(configuration->DeviceList[d]);
    }
}
//-----------------------------------------------------------------------------
void SM_RemoveStoredDeviceList()
{
    char    command[128];

    remove(sCapabilityListInfo.ListFile);
    sCapabilityListInfo.ListSaved = 0;

    sprintf(command, "rm -rf %s\n", scFileDeviceList);
    system(command);
}
//-----------------------------------------------------------------------------
bool SM_PermitJoin(int duration)
{
    return sm_DeviceSendCommand(0, 0, ZB_CMD_NET_PERMITJOIN, duration, 0, 0, 0, RETRY_1, 100);
}
//-----------------------------------------------------------------------------
void SM_CloseJoin()
{
    sm_DeviceSendCommand(0, 0, ZB_CMD_NET_PERMITJOIN_CLOSE, 0, 0, 0, 0, RETRY_0, 100);
}
//-----------------------------------------------------------------------------
bool SM_RejectDevice(void* device_specific, SM_DeviceScanOption* option)
{
    bool            result = false;
    SM_DeviceData   device_data = (SM_DeviceData)device_specific;
    SD_DeviceID     euid;

    if (device_data)
    {
        if( device_data->Type == ZB_DEV_IAS_ZONE )
        {
            Debug("Rejecting Sensor Device %s:%04X, leave=[%d] \n", EUI64ToString(device_data->DeviceID.node_eui, euid.Data), device_data->DeviceID.node_id, option->LeaveRequest);
            result = sm_DeviceSendCommand(device_data, 0, ZB_CMD_NET_LEAVE_REQUEST, 0, option->LeaveRequest, 0, 0,
                                            option->CommandRetry, option->CommandRetryInterval);
        }
        else
        {
            Debug("Rejecting Device %s:%04X \n", EUI64ToString(device_data->DeviceID.node_eui, euid.Data),
                device_data->DeviceID.node_id);
            result = sm_DeviceSendCommand(device_data, 0, ZB_CMD_NET_LEAVE_REQUEST, 0, 0, 0, 0,
                                            option->CommandRetry, option->CommandRetryInterval);
        }
    }

    return result;
}

void sm_AddJoiningDeviceList(SM_JoinedDevice device)
{
    SD_DeviceID      euid;
    SM_JoiningDevice *JoiningDevice;

    JoiningDevice = calloc(1, sizeof(SM_JoiningDevice));

    if( JoiningDevice )
    {
        memcpy(&JoiningDevice->device.DeviceID, &device->DeviceID, sizeof(SM_JoinedZigBeeData));
        LL_APPEND(JoiningDevice_Head, JoiningDevice);
        Debug("sm_AddJoiningDeviceList: device = %s\n",
                EUI64ToString(JoiningDevice->device.DeviceID.node_eui, euid.Data));
    }
}

int joining_device_cmp(SM_JoiningDevice *a, SM_JoiningDevice *b)
{
    return memcmp(a->device.DeviceID.node_eui, b->device.DeviceID.node_eui, sizeof(TEUID_64));
}

void sm_RemoveJoiningDeviceList(SM_JoinedDevice device)
{
    SD_DeviceID      euid;

    SM_JoiningDevice *elt;
    SM_JoiningDevice etmp;

    memcpy(&etmp.device.DeviceID, &device->DeviceID, sizeof(SM_JoinedZigBeeData));

    LL_SEARCH(JoiningDevice_Head, elt, &etmp, joining_device_cmp);
    if( elt )
    {
        Debug("sm_RemoveJoiningDeviceList: Found = %s\n", EUI64ToString(elt->device.DeviceID.node_eui, euid.Data));
        LL_DELETE(JoiningDevice_Head, elt);
    }
    else
    {
        Debug("sm_RemoveJoiningDeviceList: Not Found = %s\n", EUI64ToString(device->DeviceID.node_eui, euid.Data));
    }
}

void sm_RejoinDevices(SM_DeviceConfiguration *configuration)
{
    int                 index = 0;
    unsigned            updated = 0;
    SD_DeviceID         euid;
    SM_Device           device;
    SM_JoiningDevice    *joining_device;

    LL_FOREACH(JoiningDevice_Head, joining_device) {
        Debug("sm_RejoinDevices: device = %s\n", EUI64ToString(joining_device->device.DeviceID.node_eui, euid.Data));
        // updating device
        if( (updated = sm_AddDeviceProperty(configuration, &joining_device->device, &index, false)) & DA_CLUSTER )
        {
            device = configuration->DeviceList[index];

            sm_AddCapabilityProperty(configuration, device);
            sm_LinkDeviceCapability(configuration, device);
            sm_SetCapabilityValueDefault(device);
        }

        Debug("sm_RejoinDevices: updated = 0x%x\n", updated);

        if( (updated & DA_BASIC_CLUSTER) )
        {
            LL_DELETE(JoiningDevice_Head, joining_device);
            break;
        }
    }
}

//-----------------------------------------------------------------------------
void SM_AddJoinedDeviceList(SM_DeviceConfiguration* configuration, unsigned duration, bool allow_rejoining)
{
    SM_JoinedDeviceList     list;
    SM_Device               device;
    int                     count = 0, i = 0, index = 0, check = 0;
    SD_DeviceID             euid;
    unsigned                start, current, passed;
    unsigned                updated = 0;
    int                     wait;

    wait = 100 * 1000; //0.1sec

    start = (unsigned)time(0);

    count = sm_LoadJoinedDeviceList(&list);
    Debug("SM_AddJoinedDeviceList : LoadJoinedDevice count = %d\n", count);

    if( count == 0 )
    {
        //Rejoin process
        sm_RejoinDevices(configuration);
    }

    for (i = 0; i < count; i++)
    {
        Debug("Joined Device List = [%d]\n", sJoinedListInfo.ListProcessed);

        usleep(wait);

        // checking update

        for (check = i+1 ; check < count; check++)
        {
            if (memcmp(list[i]->DeviceID.node_eui, list[check]->DeviceID.node_eui, sizeof(TEUID_64)) == 0)
                break;
        }

        if (check < count)
        {
            Debug("Skipping Invalid Node %s:%04X \n", EUI64ToString(list[i]->DeviceID.node_eui, euid.Data),
                    list[i]->DeviceID.node_id);

            sJoinedListInfo.ListProcessed++;
            continue;
        }

        Debug("SM_AddJoinedDeviceList : list[%d]->DeviceID.node_id = [%s]\n",
            i, EUI64ToString(list[i]->DeviceID.node_eui, euid.Data));

        sm_AddJoiningDeviceList(list[i]);

        // updating device
        if( (updated = sm_AddDeviceProperty(configuration, list[i], &index, allow_rejoining)) & DA_CLUSTER )
        {
            device = configuration->DeviceList[index];

            sm_AddCapabilityProperty(configuration, device);
            sm_LinkDeviceCapability(configuration, device);
            sm_SetCapabilityValueDefault(device);
        }

        if( (updated & DA_BASIC_CLUSTER) )
        {
            sJoinedListInfo.ListProcessed++;

            sm_RemoveJoiningDeviceList(list[i]);
        }

        current = (unsigned)time(0);
        passed  = current - start;

        if (duration <= passed)
        {
            Debug("Loading Device - Timeout:%d (%d/%d)\n", passed, i+1, count);
            break;
        }
    }

    sm_FreeJoinedDeviceList(list, count);
}
//-----------------------------------------------------------------------------
void SM_RemoveJoinedDeviceList()
{
    // removing joined device list File

    sJoinedListInfo.ListProcessed = 0;

    remove(sJoinedListInfo.ListFile);
}
//-----------------------------------------------------------------------------
void SM_ClearJoinedDeviceList()
{
    system("rm " SD_SAVE_DIR "zbid.*");
}
//-----------------------------------------------------------------------------
void SM_UpdateDeviceList(SM_DeviceConfiguration* configuration, unsigned duration)
{
    SM_Device       device;
    SM_DeviceData   device_data;
    int             d;
    unsigned        start, current, passed;

    start = (unsigned)time(0);

    Debug("%s \n", __func__);
    for (d = 0; d < configuration->DeviceCount; d++)
    {
        device      = configuration->DeviceList[d];
        device_data = (SM_DeviceData)device->DeviceSpecific;

        if (sm_GetDeviceInfo(device, device_data, &configuration->DeviceScanOption) & DA_CLUSTER)
        {
            sm_AddCapabilityProperty(configuration, device);
            sm_LinkDeviceCapability(configuration, device);
            sm_SetCapabilityValueDefault(device);
        }

        current = (unsigned)time(0);
        passed  = current - start;

        if (duration <= passed)
        {
            Debug("Updating Device - Elapsed:%d \n", passed);
            break;
        }
    }
}
//-----------------------------------------------------------------------------
bool SM_UpdateDeviceInfo(SM_Device device, unsigned update)
{
    SM_DeviceData       device_data;
    unsigned            updated;
    SM_DeviceScanOption option = {SCAN_NORMAL, RETRY_2, 100};

    device_data = (SM_DeviceData)device->DeviceSpecific;

    updated = sm_GetDeviceInfo(device, device_data, &option);

    Debug("SM_UpdateDeviceInfo(update = 0x%x, updated = 0x%x)\n", update, updated);

    return (update == updated);
}
//-----------------------------------------------------------------------------
bool SM_CheckDeviceCapabilityValue(SD_Capability capability, const char* value, const char** checked_value)
{
    static char         sCheckedValue[CAPABILITY_MAX_VALUE_LENGTH] = {0, };

    bool                result = true;
    SM_DeviceControl    control;
    int                 level, transitionTime, referenceTime;

    control = (SM_DeviceControl)capability->DeviceSpecific;

    *checked_value = value;

    switch (control->Cluster)
    {
        case ZCL_LEVEL_CONTROL_CLUSTER_ID:
            if (sscanf(value, "%d", &level) < 1)
                result = false;
            break;

        case ZCE_SLEEP_FADER: // 600:1398816678
            if (1 < sscanf(value, "%d:%d", &transitionTime, &referenceTime)) {
                unsigned long utcTime = GetUTCTime();
                int     diff = utcTime - referenceTime;

                Debug("Check Sleep Fader diff: %d, now:%d (%d:%d) \n", diff, utcTime, transitionTime, referenceTime);

                if( (transitionTime == 0) && (referenceTime == 0) ) {
                    // We should return as true for the future - To update cache.
                    // This happens when: Group created, On/Off button pressed.
                    break;
                }

                // Transfer to Level Control capability format - level:TransitionTime
                if( diff < 0 ) {
                    // There might be a little bit time difference b/w WeMo Link with iPhone, but we will ignore it.
                    sprintf(sCheckedValue, "0:%d", transitionTime);
                } else {
                    if ( (diff*10) < transitionTime )  {
                        // Sleep fader time adjustment
                        sprintf(sCheckedValue, "0:%d", transitionTime - (diff*10));
                    } else {
                        //
                        //  This is none-error case. We just clear cache.
                        //
                        Debug("    Sleep Fader time is already has been passed. We will clear cache value. \n");
                        sprintf(sCheckedValue, "0:0");
                   }
                }
                *checked_value = sCheckedValue;
            }
            else
            {
                //
                // if value is "" then sscanf returns -1
                // We will deal this with "No Error" case. because app can send null string for capability value when create group
                // e.g, 254:0,1,0,,,,153:0
                //
                sprintf(sCheckedValue, "0:0");
                *checked_value = sCheckedValue;
                Debug("    Sleep Fader value will be cleared \n");
            }

            break;

        default:;
    }

    if (!result)
        Debug("Invalid Capability Value (%s, %s)\n", capability->CapabilityID.Data, value);

    return result;
}
//-----------------------------------------------------------------------------
bool SM_SetDeviceCapabilityValue(SD_Device device, SD_Capability capability, const char* value)
{
    bool group_mode = false;

    SM_ZBCommand_Enque(ZB_CMD_SET, group_mode, (void *)device, capability, (char*)value);
    return true;
}


bool SM_SetDeviceBasicCapabilityValue(SD_Device device, SD_Capability capability, const char *value)
{
    SM_DeviceData       device_data;
    SM_DeviceControl    control;
    TZBParam            param;

    device_data = (SM_DeviceData)device->DeviceSpecific;
    control     = (SM_DeviceControl)capability->DeviceSpecific;

    /*
    if(control->Cluster == ZCE_SENSOR_TESTMODE)
    {
        Debug("SM_GetDeviceBasicCapabilityValue: Sensor Test mode, return\n");
        return true;
    }
    */

    if (!control->Set)
        return true;

    if (!sm_SetDeviceParam(&param, capability->DataType, value))
        return false;

    param.command = control->Set;

    return sm_DeviceSendCommandParam(device_data, &param, RETRY_1);
}

bool SM_GetDeviceBasicCapabilityValue(SD_Device device, SD_Capability capability)
{
    SM_DeviceData       device_data;
    SM_DeviceControl    control;
    TZBParam            param;

    device_data = (SM_DeviceData)device->DeviceSpecific;
    control     = (SM_DeviceControl)capability->DeviceSpecific;

    if( ( control->Get == ZB_CMD_GET_ONOFF ) || ( control->Get == ZB_CMD_GET_LEVEL ) ||
        ( control->Get == ZB_CMD_GET_COLOR ) || ( control->Get == ZB_CMD_GET_COLOR_TEMP ) )
    {
        //data_type --> SD_REAL(0:Synchrous), SD_CACHE(1:Asynchrous)
        if( device_data->CommandStatus[control->Get] == STARTED ||
            device_data->CommandStatus[control->Get] == FINISHED )
        {
            device_data->CommandStatus[control->Get] = PENDING;
            if( sm_DeviceSendCommand(device_data, &param, control->Get, SD_CACHE, 0,0,0, RETRY_0, 100) )
            {
                Debug("SM_GetDeviceBasicCapabilityValue: command[%d] is running as Cache Mode...\n", control->Get);
                return true;
            }
        }
        else if( device_data->CommandStatus[control->Get] == PENDING )
        {
            Debug("SM_GetDeviceBasicCapabilityValue: command[%d] is PENDING...\n", control->Get);
            return true;
        }
    }

    return false;
}

//-----------------------------------------------------------------------------
bool SM_GetDeviceCapabilityValue(SD_Device device, SD_Capability capability, char value[CAPABILITY_MAX_VALUE_LENGTH], int retryCnt, unsigned data_type)
{
    SM_DeviceData       device_data;
    SM_DeviceControl    control;
    TZBParam            param;
    bool                result = false;
    bool                group_mode = false;
    device_data = (SM_DeviceData)device->DeviceSpecific;
    control     = (SM_DeviceControl)capability->DeviceSpecific;

    value[0] = 0;

    if (!control->Get)
    {
        value[0] = 0;
        return true;
    }

    device_data->CommandStatus[control->Get] = STARTED;

    if( ( control->Get == ZB_CMD_GET_ONOFF ) || ( control->Get == ZB_CMD_GET_LEVEL ) ||
        ( control->Get == ZB_CMD_GET_COLOR ) || ( control->Get == ZB_CMD_GET_COLOR_TEMP ) )
    {
        //data_type --> SD_REAL(0:Synchrous), SD_CACHE(1:Asynchrous)
        if( data_type == SD_REAL )
        {
            if (sm_DeviceSendCommand(device_data, &param, control->Get, data_type,0,0,0, RETRY_0, 100))
            {
                sm_GetDeviceParam(&param, capability->DataType, value);
                result = true;
            }
        }
        else if( data_type == SD_CACHE )
        {
            //The async ZB_CMD_GET_ONOFF & ZB_CMD_GET_LEVEL command are running in command thread.
            // SM_ZBCommand_Enque(ZB_CMD_GET, device, capability, NULL);
            SM_ZBCommand_Enque(ZB_CMD_GET, group_mode, (void *)device, capability, NULL);
            return true;
        }
    }
    else
    {
        if (sm_DeviceSendCommand(device_data, &param, control->Get, 0,0,0,0, RETRY_0, 100))
        {
            sm_GetDeviceParam(&param, capability->DataType, value);
            result = true;
        }
    }

    Debug("SM_GetDeviceCapabilityValue: command [%d] is FINISHED in Real Mode...\n", control->Get);

    device_data->CommandStatus[control->Get] = FINISHED;

    return result;
}
//-----------------------------------------------------------------------------
bool SM_CheckAllowToSendZigbeeCommand(const SD_Capability capability, const char* value)
{
    bool                ret = true;
    SM_DeviceControl    control;
    int                 transitionTime=0, referenceTime=0;
    int                 aaa=0, bbb=0, ccc=0;

    if(!value)
        value = "";

    control = (SM_DeviceControl)capability->DeviceSpecific;

    switch (control->Cluster)
    {
        case ZCE_SLEEP_FADER: // 600:1398816678
            if (0 < sscanf(value, "%d:%d", &transitionTime, &referenceTime)) {
                if(transitionTime == 0) {
                    Debug("    Sleep Fader time 0: Will not send ZB \n");
                    ret = false;
                }
            } else {
                // if value is "", then sscanf will return -1
                Debug("    Sleep Fader cache will be cleared, will not send ZB \n");
                ret = false;
            }
            break;

        case ZCL_IAS_ZONE_CLUSTER_ID: //IAS Zone sensor cluster
        case ZCE_SENSOR_CONFIG:
        case ZCE_SENSOR_KEYPRESS:
            ret = false;
            break;

        case ZCE_SENSOR_TESTMODE:
            ret = true;
            break;

        case ZCE_COLOR_TEMPERATURE:
            // Cache will be cleared for 3 cases:
            //  - value ""
            //  - value "0"
            //  - value "0:0"
            sscanf(value, "%d:%d", &aaa, &bbb);
            if( (0 == aaa) && (0 == bbb) ) {
                Debug("    Clear cache 30301 \n");
                ret = false;
            }
            break;

        case ZCL_COLOR_CONTROL_CLUSTER_ID:
            // Cache will be cleared for 4 cases:
            //  - value ""
            //  - value "0"
            //  - value "0:0"
            //  - value "0:0:0"
            sscanf(value, "%d:%d:%d", &aaa, &bbb, &ccc);
            if( (0 == aaa) && (0 == bbb) && (0 == ccc) ) {
                Debug("    Clear cache 10300 \n");
                ret = false;
            }
            break;

        default:;
    }

    return ret;
}
//-----------------------------------------------------------------------------
bool SM_CheckNotifiableCapability(const SD_DeviceID* device_id, const SD_Capability capability, const char* value, unsigned event)
{
    bool                notify = false;
    SM_DeviceControl    control;
    int                 level, transitionTime;

    control = (SM_DeviceControl)capability->DeviceSpecific;
    if(!control) {
        Debug("CheckValue: Null deviceSpecific \n");
        return false;
    }

    switch (control->Cluster)
    {
    case ZCL_ON_OFF_CLUSTER_ID:
    case ZCL_COLOR_CONTROL_CLUSTER_ID:
    case ZCE_COLOR_TEMPERATURE:
            notify = true;
            break;

    case ZCL_LEVEL_CONTROL_CLUSTER_ID:
            // if level is 0, then shouldn't be reported, nor updated to cache.
            if (1 <= sscanf(value, "%d:%d", &level, &transitionTime)) {
                if(level != 0) {
                    notify = true;
                }
            }
            break;

    case ZCE_SLEEP_FADER: // 600:1398816678
            notify = true;
            break;

    case ZCE_SENSOR_CONFIG:
    case ZCE_SENSOR_TESTMODE:
    case ZCE_SENSOR_KEYPRESS:
            notify = true;
            break;

    default:;
    }

    return notify;
}
//-----------------------------------------------------------------------------
bool SM_ReserveNotification(const SD_DeviceID* device_id, const SD_Capability capability, const char* value, unsigned event)
{
    bool                reserved = false;
    SM_DeviceControl    control;
    int                 level, transitionTime;
    unsigned long       update_time;
    unsigned            sensor_test_mode = 0;

    control = (SM_DeviceControl)capability->DeviceSpecific;

    if(!control) {
        Debug("ReserveNotification: Null deviceSpecific \n");
        return false;
    }

    Debug("%s \n", __func__);
    switch (control->Cluster)
    {
        case ZCE_SLEEP_FADER: // transition time : reference time , e.g)  600:1398816678
            if (1 <= sscanf(value, "%d:%d",&level, &transitionTime)) {
                if(transitionTime > 0) {
                    //Debug("Reserve UPnP Notify after %d seconds. bulb id:%s \n", (transitionTime/10), device_id->Data);
                    update_time = GetUTCTime() + (transitionTime / 10);
                    SM_ReserveCapabilityUpdate(update_time, device_id, &capability->CapabilityID, "0:0", SE_LOCAL + SE_REMOTE);
                    reserved = true;
                }
            }
            break;

        case ZCE_SENSOR_TESTMODE:
            if(sscanf(value, "%d", &sensor_test_mode) > 0)
            {
                if(sensor_test_mode == 1)
                {
                    //Debug("Start Sensor Test mode. id:%s \n", device_id->Data);
                    update_time = GetUTCTime() + (60*5);    // Test mode will be terminated after 5 minutes.
                    SM_ReserveCapabilityUpdate(update_time, device_id, &capability->CapabilityID, "0", SE_LOCAL + SE_REMOTE);
                    reserved = true;
                }
            }
            break;

        default:
            break;
    }

    return reserved;
}
//-----------------------------------------------------------------------------
bool SM_ManipulateNotificationValue(const SD_Capability capability, const char* value, const char ** noti_value)
{
    static char         sNotiValue[CAPABILITY_MAX_VALUE_LENGTH] = {0, };
    bool                changed = false;
    SM_DeviceControl    control;
    int                 level = 0, transitionTime = 0;

    control = (SM_DeviceControl)capability->DeviceSpecific;

    *noti_value = value;

    switch (control->Cluster)
    {
        case ZCL_LEVEL_CONTROL_CLUSTER_ID:
            // We will always notifiy transition time as '0' to the App.
            if (1 <= sscanf(value, "%d:%d", &level, &transitionTime)) {
                memset(sNotiValue, 0, CAPABILITY_MAX_VALUE_LENGTH);
                sprintf(sNotiValue, "%d:0", level);
                *noti_value = sNotiValue;
                changed = true;
            }
            break;

        default:
            break;
    }

    return changed;
}
//-----------------------------------------------------------------------------
bool SM_ManipulateCacheUpdateValue(const SD_Capability capability, const char* value, const char ** cache_value)
{
    static char         sCacheValue[CAPABILITY_MAX_VALUE_LENGTH] = {0, };
    bool                changed = false;
    SM_DeviceControl    control;
    int                 level = 0, transitionTime = 0;

    control = (SM_DeviceControl)capability->DeviceSpecific;

    *cache_value = value;

    switch (control->Cluster)
    {
        case ZCL_LEVEL_CONTROL_CLUSTER_ID:
            // We will always notifiy transition time as '0' to the App.
            if (1 <= sscanf(value, "%d:%d", &level, &transitionTime)) {
                memset(sCacheValue, 0, CAPABILITY_MAX_VALUE_LENGTH);
                sprintf(sCacheValue, "%d:0", level);
                *cache_value = sCacheValue;
                changed = true;
            }
            break;

        default:
            break;
    }

    return changed;
}
//-----------------------------------------------------------------------------
bool SM_CheckCacheUpdateCapability(const SD_Capability capability, const char* value)
{
    bool                updateCache = true;
    SM_DeviceControl    control;
    int                 level = 0, transitionTime = 0;

    control = (SM_DeviceControl)capability->DeviceSpecific;

    switch (control->Cluster)
    {
    case ZCL_LEVEL_CONTROL_CLUSTER_ID:
            // if level is 0, then shouldn't be reported, nor updated to cache.
            if (1 <= sscanf(value, "%d:%d", &level, &transitionTime)) {
                if(level == 0) {
                    updateCache = false;
                }
            }
            break;

    default:;
    }

    return updateCache;
}
//-----------------------------------------------------------------------------
bool SM_CheckFilteredCapability(SD_Capability capability)
{
    bool                result = false;
    SM_DeviceControl    control;

    control = (SM_DeviceControl)capability->DeviceSpecific;

    switch (control->Cluster)
    {
    // case ZCL_ON_OFF_CLUSTER_ID:
    case ZCL_LEVEL_CONTROL_CLUSTER_ID:
    case ZCE_SLEEP_FADER:
    case ZCL_IAS_ZONE_CLUSTER_ID:
    case ZCE_SENSOR_CONFIG:
    case ZCE_COLOR_TEMPERATURE:
    case ZCL_COLOR_CONTROL_CLUSTER_ID:
    case ZCE_SENSOR_TESTMODE:
    case ZCE_SENSOR_KEYPRESS:
            result = true;
            break;

    default:;
    }

    return result;
}
//-----------------------------------------------------------------------------
bool SM_CheckSleepFaderCapability(SD_Capability capability)
{
    bool                result = false;
    SM_DeviceControl    control;

    control = (SM_DeviceControl)capability->DeviceSpecific;

    switch (control->Cluster)
    {
        case ZCE_SLEEP_FADER:
            result = true;
            break;

        default:;
    }

    return result;
}
//-----------------------------------------------------------------------------
bool SM_CheckSensorConfigCapability(SD_Capability capability)
{
    bool                result = false;
    SM_DeviceControl    control;

    control = (SM_DeviceControl)capability->DeviceSpecific;

    switch (control->Cluster)
    {
    case ZCE_SENSOR_CONFIG:
            result = true;
            break;

    default:;
    }

    return result;
}
//-----------------------------------------------------------------------------
bool SM_SetSensorConfig(SM_Device device, SD_Capability capability, const char * value)
{
    bool                result = false;
    SM_DeviceControl    control;
    int                 v;

    control = (SM_DeviceControl)capability->DeviceSpecific;

    switch (control->Cluster)
    {
    case ZCE_SENSOR_CONFIG:
            if (sscanf(value, "%d", &v) < 1)
                break;

            Debug("%s: %s\n", __func__, value);

            if(v == SD_SENSOR_DISABLE)
            {
                result = true;
                device->Attributes &= ~DA_SENSOR_ENABLED;
                //Debug("    Sensor disabled \n");
            }
            else if(v == SD_SENSOR_ENABLE)
            {
                result = true;
                device->Attributes |= DA_SENSOR_ENABLED;
                //Debug("    Sensor enabled \n");
            }
            break;

    default:;
    }

    return result;
}
//-----------------------------------------------------------------------------
void SM_UpdateFilteredCapabilityValue(SD_Capability capability, char value[CAPABILITY_MAX_VALUE_LENGTH])
{
    SM_DeviceControl    control;
    int                 v;

    control = (SM_DeviceControl)capability->DeviceSpecific;

    switch (control->Cluster)
    {
    case ZCL_LEVEL_CONTROL_CLUSTER_ID:
            if (sscanf(value, "%d", &v) < 1)
                break;
            if (v == 0)
                strcpy(value, "255:0");
            break;

    default:;
    }
}
//-----------------------------------------------------------------------------
int SM_GenerateMulticastID(SM_DeviceConfiguration* configuration)
{
    int         base, offset, id, g;

    base = configuration->GroupReferenceID ;

    for (offset = 1; offset < 0xFFFF; offset++)
    {
        id = (base + offset) & 0xFFFF;

        if (!id)
            continue;

        for (g = 0; g < configuration->GroupCount; g++)
        {
             if (id == configuration->GroupList[g]->MulticastID)
                break;
        }

        if (g == configuration->GroupCount)
            break;
    }

    configuration->GroupReferenceID = id;

    return id;
}
//-----------------------------------------------------------------------------
bool SM_AddDeviceMulticastID(SD_Device device, int multicast_id)
{
    SM_DeviceData       device_data = (SM_DeviceData)device->DeviceSpecific;

    return sm_DeviceSendCommand(device_data, 0, ZB_CMD_SET_GROUP, ZB_GROUP_ADD, ZB_BITMASK_MULTICAST + multicast_id,0,0, RETRY_1, 100);
}
//-----------------------------------------------------------------------------
bool SM_RemoveDeviceMulticastID(SD_Device device, int multicast_id)
{
    SM_DeviceData       device_data = (SM_DeviceData)device->DeviceSpecific;

    return sm_DeviceSendCommand(device_data, 0, ZB_CMD_SET_GROUP, ZB_GROUP_REMOVE, ZB_BITMASK_MULTICAST + multicast_id,0,0, RETRY_0, 100);
}
//-----------------------------------------------------------------------------
bool SM_CheckDeviceMulticast(SD_Device device)
{
    SM_DeviceData       device_data = (SM_DeviceData)device->DeviceSpecific;
    int                 c;

    for (c = 0; c < device_data->ClusterCount; c++)
    {
        if (device_data->ClusterList[c] == ZCL_GROUPS_CLUSTER_ID)
            break;
    }

    return (c < device_data->ClusterCount);
}

//-----------------------------------------------------------------------------
// bool SM_SetGroupCapabilityValue(int multicast_id, SD_Capability capability, const char* value)
bool SM_SetGroupCapabilityValue(SD_Group group, SD_Capability capability, const char* value)
{
    SM_GroupDevice          group_device;
    SM_DeviceData           device_data;
    bool                    group_mode = true;

    group_device = (SM_GroupDevice)calloc(1, sizeof(SM_GroupData));
    device_data = (SM_DeviceData)calloc(1, sizeof(SM_ZigBeeSpecificData));

    if( !group_device || !device_data )
        return false;

    device_data->DeviceID.node_id = ZB_BITMASK_MULTICAST + group->MulticastID;

    group_device->Group = group;
    group_device->DeviceSpecific = device_data;

    SM_ZBCommand_Enque(ZB_CMD_SET, group_mode, (void *)group_device, capability, (char*)value);

    return true;
}

bool SM_SetGroupBasicCapabilityValue(SM_GroupDevice group_device, SD_Capability capability, const char *value)
{
    SM_DeviceData           device_data;
    SM_DeviceControl        control;
    TZBParam                param;
    bool                    result;

    device_data = (SM_DeviceData)group_device->DeviceSpecific;
    control = (SM_DeviceControl)capability->DeviceSpecific;

    if (!control->Set)
        return true;

    if (!sm_SetDeviceParam(&param, capability->DataType, value))
        return false;

    param.command = control->Set;

    result = sm_DeviceSendCommandParam(device_data, &param, RETRY_1);
    return result;
}
//-----------------------------------------------------------------------------
bool SM_GetDeviceDetails(SD_Device device, const char** text)
{
    static char             sText[1024];

    SM_DeviceData           device_data;
    TZBParam                param;
    SM_ZigBeeDeviceDetails  details;

    memset(&details, 0, sizeof(details));

    device_data = (SM_DeviceData)device->DeviceSpecific;

    if (sm_DeviceSendCommand(device_data, &param, ZB_CMD_GET_ZCLVERSION, 0,0,0,0, RETRY_1, 100))  // Mandatory Field
        details.ZCLVersion = param.param1;
    else
        return false;

    if (sm_DeviceSendCommand(device_data, &param, ZB_CMD_GET_APPVERSION, 0,0,0,0, RETRY_1, 100))
        details.ApplicationVersion = param.param1;

    if (sm_DeviceSendCommand(device_data, &param, ZB_CMD_GET_STACKVERSION, 0,0,0,0, RETRY_1, 100))
        details.StackVersion = param.param1;

    if (sm_DeviceSendCommand(device_data, &param, ZB_CMD_GET_HWVERSION, 0,0,0,0, RETRY_1, 100))
        details.HWVersion = param.param1;

    if (sm_DeviceSendCommand(device_data, &param, ZB_CMD_GET_MFGNAME, 0,0,0,0, RETRY_1, 100))
        SAFE_STRCPY(details.ManufacturerName, (char*)param.str);

    if (sm_DeviceSendCommand(device_data, &param, ZB_CMD_GET_MODELCODE, 0,0,0,0, RETRY_1, 100))
        SAFE_STRCPY(details.ModelIdentifier, (char*)param.str);

    if (sm_DeviceSendCommand(device_data, &param, ZB_CMD_GET_DATE, 0,0,0,0, RETRY_1, 100))
        SAFE_STRCPY(details.DateCode, (char*)param.str);

    if (sm_DeviceSendCommand(device_data, &param, ZB_CMD_GET_POWERSOURCE, 0,0,0,0, RETRY_1, 100))
        details.PowerSource= param.param1;

    sprintf(sText,
        "ZCLVersion: 0x%02X\n"
        "ApplicationVersion: 0x%02X\n"
        "StackVersion: 0x%02X\n"
        "HWVersion: 0x%02X\n"
        "ManufacturerName: %s\n"
        "ModelIdentifier: %s\n"
        "DateCode: %s\n"
        "PowerSource: 0x%02X\n",
        details.ZCLVersion,
        details.ApplicationVersion,
        details.StackVersion,
        details.HWVersion,
        details.ManufacturerName,
        details.ModelIdentifier,
        details.DateCode,
        details.PowerSource);

    *text = sText;

    return true;
}
//-----------------------------------------------------------------------------
bool SM_SetDeviceOTA(SD_Device device, int upgrade_policy)
{
    SM_DeviceData   device_data = (SM_DeviceData)device->DeviceSpecific;

    return sm_DeviceSendCommand(device_data, 0, ZB_CMD_SET_OTA_NOTIFY, 3, upgrade_policy, 0, 0, RETRY_1, 100);
}

void SM_UpdateDeviceCommandStatus(SM_Device device, SM_Capability capability, unsigned CommandStatus)
{
    SM_DeviceData       device_data;
    SM_DeviceControl    control;

    device_data = (SM_DeviceData)device->DeviceSpecific;
    control = (SM_DeviceControl)capability->DeviceSpecific;

    if( control->Get )
    {
        device_data->CommandStatus[control->Get] = CommandStatus;
        Debug("SM_UpdateDeviceCommandStatus: command[%d] is status [%d] \n", control->Get, CommandStatus);
    }
    else if( control->Set )
    {
        device_data->CommandStatus[control->Set] = CommandStatus;
    }
}

//-----------------------------------------------------------------------------------------------------
// ZB_CMD_NET_SCANJOIN : Scan command to join to the network when Link is changed as "Zigbee Router"
//-----------------------------------------------------------------------------------------------------
bool SM_ScanJoin()
{
    return sm_DeviceSendCommand(0, 0, ZB_CMD_NET_SCANJOIN, 0, 0, 0, 0, RETRY_2, 100);
}

//--------------------------------------------------------------------------------------------------------------
// ZB_CMD_NET_LEAVE : Leave command to leave itself fromthe network when Link is changed as "Zigbee Coordinate"
//--------------------------------------------------------------------------------------------------------------
bool SM_LeaveNetwork()
{
    return sm_DeviceSendCommand(0, 0, ZB_CMD_NET_LEAVE, 0, 0, 0, 0, RETRY_1, 100);
}

bool SM_NetworkForming()
{
    return sm_NetworkForming();
}

