/***************************************************************************
*
*
* plugin_cmd.h
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
#ifndef PLUGIN_CMD_H_
#define PLUGIN_CMD_H_

typedef void (*cmd_callback)(const char* payload, int deviceIndex);

struct plugin_cmd_callback
{
    const char*         cmdName;
    cmd_callback        cmdCallbackIf;
};

typedef struct plugin_cmd_callback PluginCmdCallback, *pPluginCmdCallback;
extern PluginCmdCallback g_ctrlCmdIF[];

//-- Create the test Interface here:

void WiFiSetup(const char* payload, int deviceIndex);
void ProcessEvent(const char* payload, int deviceIndex);
void SetSensorEvent(const char* payload, int deviceIndex);
void ProcessSync(const char* payload, int deviceIndex);

void CmdCtrlPointGetApList(int deviceIndex);

void CmdProcessCtrlPoint(const char* commandLine);

void  CmdCtrlPointConnectHomeNetwork(int deviceIndex, int channel, const char* ssid, const char* Auth, const char* Encrypt, const char* password);


void  CmdCtrlSetFriendlyName(int deviceIndex, const char* name);

void ProcessFirmware(const char* payload, int deviceIndex);
void ProcessRule(const char* payload, int deviceIndex);
void SetRemoteAccess(const char* payload, int deviceIndex);

#endif /* PLUGIN_CMD_H_ */
