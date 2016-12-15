/***************************************************************************
*
*
* plugin_cmd.c
*
* Created by Belkin International, Software Engineering on Jun 20, 2011
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
#include "global.h"
#include "logger.h"
#include "defines.h"
#include "plugin_cmd.h"
#include "plugin_ctrlpoint.h"
#include "common.h"
#include <sys/time.h>
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

//- For test
PluginCmdCallback g_ctrlCmdIF[] = {{"WiFi", WiFiSetup}, {"SetSensorEvent", SetSensorEvent}, {"Event", ProcessEvent}, {"TimeSync", ProcessSync},
                {"Firmware", ProcessFirmware}, {"RemoteAccess", SetRemoteAccess}};

#define CMD_TABLE_SIZE sizeof(g_ctrlCmdIF)/sizeof(PluginCmdCallback)

void CmdProcessCtrlPoint(const char* commandLine)
{
    int cmd_size = CMD_TABLE_SIZE;

    int deviceLoop = 0x00;

    APP_LOG("UPnPCtrPt",LOG_DEBUG, "command request: %s", commandLine);

    char szCommand[SIZE_64B];
    char szPlayload[SIZE_256B];
    int  cntDeviceIndex = 0x00;
    memset(szCommand, 0x00, sizeof(szCommand));
    memset(szPlayload, 0x00, sizeof(szPlayload));

    sscanf(commandLine, "%s %d %s", szCommand, &cntDeviceIndex, szPlayload);

    APP_LOG("UPnPCtrPt",LOG_DEBUG, "command [%s], device Index[%d], payload[%s]", szCommand, cntDeviceIndex, szPlayload);

    for (deviceLoop = 0x00; deviceLoop < cmd_size; deviceLoop++)
    {
        if (0x00 == strcmp(szCommand, g_ctrlCmdIF[deviceLoop].cmdName))
            break;
    }

    if (cmd_size == deviceLoop)
    {
        APP_LOG("UPnPCtrPt",LOG_E_ERROR, "command entry not found");

        return;
    }

    g_ctrlCmdIF[deviceLoop].cmdCallbackIf(szPlayload, cntDeviceIndex);

}

void CmdCtrlPointGetApList(int deviceIndex)
{
    PluginCtrlPointGetApList(deviceIndex);
}

void  CmdCtrlPointConnectHomeNetwork(int deviceIndex, int channel, const char* ssid, const char* Auth, const char* Encrypt, const char* password)
{
    PluginCtrlPointConnectHomeNetwork(deviceIndex, channel, ssid, Auth, Encrypt, password);
}

void  CmdCtrlSetFriendlyName(int deviceIndex, const char* name)
{
    PluginCtrlPointSetFriendlyName(deviceIndex, name);
}
void CmdCtrlSetBinary (int deviceIndex, const char* name)
{
    PluginCtrlPointSetBinaryState(deviceIndex, (const char**)name);
}


void  CmdCtrlGetFriendlyName(int deviceIndex, const char* name)
{
    PluginCtrlPointGetFriendlyName(deviceIndex, name);
}
void  CmdCtrlGetBinary(int deviceIndex, const char* name)
{
    PluginCtrlPointGetBinary(deviceIndex, name);
}

void SetSensorEvent(const char* payload, int deviceIndex)
{
    //- Now for test purpose only
    PluginCtrlPointSetSensorEvent(deviceIndex, 0x00, 0x00, 0x00, 0x00);
}

void ProcessFirmware(const char* payload, int deviceIndex)
{
   APP_LOG("UPnPCtrPt",LOG_DEBUG, "ProcessFirmware request......:%s", payload);
   if (0x00 != strstr(payload, "update"))
   {
       char szURL[SIZE_256B];
       memset(szURL, 0x00, sizeof(szURL));
       sscanf(payload, "update:%s", szURL);

       APP_LOG("UPnPCtrPt",LOG_DEBUG, "**************%s", payload);
       PluginCtrlPointUpdateFirmware(deviceIndex, szURL);
   }
   else if (0x00 != strstr(payload, "Get"))
   {
       APP_LOG("UPnPCtrPt",LOG_DEBUG, "Reading embedded firmware version ......");
       PluginCtrlPointGetFirmware(deviceIndex);
   }
}

void ProcessRule(const char* payload, int deviceIndex)
{
    APP_LOG("UPnPCtrPt",LOG_DEBUG, "ProcessFirmware request......:%s", payload);
    if (0x00 != strstr(payload, "add"))
    {
        char szURL[SIZE_256B];
        memset(szURL, 0x00, sizeof(szURL));
        PluginCtrlPointAddRule(deviceIndex);
    }
    else if (0x00 != strstr(payload, "remove"))
    {
        APP_LOG("UPnPCtrPt",LOG_DEBUG, "Reading embedded firmware version ......");
        PluginCtrlPointAddRule(deviceIndex);
    }
    else if (0x00 != strstr(payload, "edit"))
    {
        PluginCtrlPointAddRule(deviceIndex);
    }
}

void ProcessSync(const char* payload, int deviceIndex)
{
    APP_LOG("UPnPCtrPt",LOG_DEBUG, "Time sync request......");
    if (0x00 != strstr(payload, "Set"))
    {
        struct timeval now;
        gettimeofday(&now, NULL);

        PluginCtrlPointSyncTime(deviceIndex, now.tv_sec, -8, 0x01);
    }
    else if (0x00 != strstr(payload, "Get"))
    {

    }
}

void ProcessEvent(const char* payload, int deviceIndex)
{
   if (0x00 != strstr(payload, "SetName"))
   {
        char szPayload[SIZE_256B];
        memset(szPayload, 0x00, sizeof(szPayload));
        sscanf(payload, "SetName:%s", szPayload);
        APP_LOG("UPnPCtrPt",LOG_DEBUG, "set  device: %d name to :%s", deviceIndex, szPayload);
        CmdCtrlSetFriendlyName(deviceIndex, szPayload);
   }
 else if (0x00 != strstr(payload, "SetBinary")) {
        char szPayload[SIZE_256B];
        memset(szPayload, 0x00, sizeof(szPayload));
        sscanf(payload, "SetBinary:%s", szPayload);
        APP_LOG("UPnPCtrPt",LOG_DEBUG, "set  device: %d name to :%s", deviceIndex, szPayload);
        CmdCtrlSetBinary(deviceIndex, szPayload);
   }

   else if (0x00 != strstr(payload, "GetName"))
   {
        CmdCtrlGetFriendlyName(deviceIndex, 0x00);
   }
   else if (0x00 != strstr(payload, "GetName"))
   {

   }else if (0x00 != strstr(payload, "GetBinary")) {
	CmdCtrlGetBinary (deviceIndex, 0x00);
	}
   else if (0x00 != strstr(payload, "ON"))
   {
       PluginCtrlPointSetBinaryState(deviceIndex,&payload);
   }
      else if (0x00 != strstr(payload, "OFF"))
      {
         PluginCtrlPointSetBinaryState(deviceIndex, &payload);
      }
      else if (0x00 != strstr(payload, "metainfo"))
            {
               PluginCtrlPointGetMetaInfo(deviceIndex);
            }
   else
   {
       APP_LOG("UPnPCtrPt",LOG_DEBUG, "Wrong event command");
   }



}
void SetRemoteAccess(const char* payload, int deviceIndex)
{
	APP_LOG("UPnPCtrPt",LOG_DEBUG, "********************Handling set remote access request********************");
	if (0x00 != strstr(payload, "Enable"))
	{
		char szDeviceId[SIZE_128B];
		char szHomeId[SIZE_50B];
		memset(szDeviceId, 0x00, sizeof(szDeviceId));
		memset(szHomeId, 0x00, sizeof(szHomeId));
		sscanf(payload, "Enable:%s %s", szDeviceId,szHomeId);
		APP_LOG("UPnPCtrPt",LOG_HIDE, "setting  DeviceID: <%s> HomeId:<%s> enable", szDeviceId,szHomeId);

		PluginCtrlPointRemoteAccess(deviceIndex, szDeviceId, 0x01, szHomeId, NULL);
	}
}
void WiFiSetup(const char* payload, int deviceIndex)
{
	char *strtok_r_temp;
    if (0x00 != strstr(payload, "GetApList"))
    {
        APP_LOG("UPnPCtrPt",LOG_DEBUG, "command: GetApList");
        CmdCtrlPointGetApList(deviceIndex);
    }
    else if (0x00 != strstr(payload, "pairing"))
    {
        int varListLoop = 0x00;

        char szPayload[SIZE_256B];
        memset(szPayload, 0x00, sizeof(szPayload));

        char szValues[SIZE_10B][SIZE_32B];

        memset(szPayload, 0x00, sizeof(szPayload));

        sscanf(payload, "pairing:%s", szPayload);

        char delims[] = ";";
        char *result = NULL;

        result = strtok_r(szPayload, delims,&strtok_r_temp );
        while((result != NULL) && (varListLoop < SIZE_10B))
        {
            strncpy(szValues[varListLoop], result, sizeof(szValues[varListLoop])-1);
            APP_LOG("UPnPCtrPt",LOG_DEBUG, "%s", szValues[varListLoop]);
            varListLoop++;
            result = strtok_r( NULL, delims,&strtok_r_temp);
        }


       CmdCtrlPointConnectHomeNetwork(deviceIndex, atoi(szValues[0x00]), szValues[0x01], szValues[0x02], szValues[0x03], szValues[0x04]);
    }
    else if (0x00 != strstr(payload, "CloseSetup"))
    {
        PluginCtrlPointCloseAp(deviceIndex);
    }
    else if (0x00 != strstr(payload, "NetworkStatus"))
        {
        PluginGetNetworkStatus(deviceIndex);
        }
    else
    {
        APP_LOG("UPnPCtrPt",LOG_DEBUG, "Wrong command");
    }
}
