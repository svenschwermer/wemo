/***************************************************************************
*
*
* controlledevice.h
*
* Created by Belkin International, Software Engineering on May 27, 2011
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
#ifndef CONTROLLEDEVICE_H_
#define CONTROLLEDEVICE_H_

#include <upnp.h>
#include "global.h"
#include "logger.h"

#define DEFAULT_WEB_DIR "/tmp/Belkin_settings"

//------------------------------
extern char *PluginDeviceServiceType[];

int ControlleeDeviceStart(char *ip_address,
               unsigned short port,
               char *desc_doc_name,
               char *web_dir_path);

int ControlleeDeviceStateTableInit(
        /*! [in] The description document URL. */
        char *DescDocURL);

int PluginDeviceHandleSubscriptionRequest(struct Upnp_Subscription_Request *);

int PluginDeviceHandleGetVarRequest(struct Upnp_State_Var_Request *);

int PluginDeviceHandleActionRequest(struct Upnp_Action_Request *);

#define PLUGIN_MAX_VARS                               16
#define PLUGIN_MAX_ACTIONS                            16

extern char* szServiceTypeInfo[];

typedef int (*upnp_action)(IXML_Document *request, IXML_Document **out, const char **errorString);

struct plugin_device_upnp_action
{
    const char*         actionName;
    upnp_action         pUpnpAction;
};

typedef struct plugin_device_upnp_action PluginDeviceUpnpAction, *pDeviceAction;


/** The data struct for device servuce */
struct plugin_service 
{
	char                           UDN[NAME_SIZE];
	char                           ServiceId[NAME_SIZE];
	char                           ServiceType[NAME_SIZE];
	const  char*                   VariableName[PLUGIN_MAX_VARS];
	char*                          VariableStrVal[PLUGIN_MAX_VARS];
	PluginDeviceUpnpAction*        ActionTable;
	int                            cntTableSize;
	int VariableCount;
};

typedef struct plugin_service PluginServiceT, *pPluginService;

/** Device profile */
typedef int UPnP_Device_handle;

struct plugin_device
{
    /** Deviec handler*/
	UPnP_Device_handle   device_handle;
	/** The service number device has*/
	int                  service_number;
	/** service table entry*/
	PluginServiceT       service_table[PLUGIN_MAX_SERVICES];
};

typedef struct plugin_device PluginDevice;

int CtrleeDeviceSetActionTable(PLUGIN_SERVICE_TYPE serviceType, pPluginService pOut);
int CtrleeDeviceHandleActionRequest(struct Upnp_Action_Request *pActionRequest);
int CtrleeDeviceSetServiceTable(int serviceType,
    const char *UDN,
    const char *serviceId,
    const char *serviceTypeS,
    pPluginService pService);

extern PluginDevice SocketDevice;

/**
 *
 */
int GetApList(IXML_Document *request, IXML_Document **out, const char **errorString);

/**
 *
 */

int ConnectHomeNetwork(IXML_Document *request, IXML_Document **out, const char **errorString);

int GetNetworkStatus(IXML_Document *request, IXML_Document **out, const char **errorString);

#endif /* CONTROLLEDEVICE_H_ */
