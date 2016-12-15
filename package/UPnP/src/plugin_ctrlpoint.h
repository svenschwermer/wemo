/***************************************************************************
*
*
* plugin_ctrlpoint.h
*
* Created by Belkin International, Software Engineering on Jun 13, 2011
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
#ifndef CTRLPOINT_H_
#define CTRLPOINT_H_
#include <upnp.h>

#ifndef CONTROLLEDEVICE_H_
#include "controlledevice.h"
#endif

#define         PLUGIN_ERROR            1
#define 	PLUGIN_REMOTE_REQ	"pluginremoterequest"
#define MAX_DISCOVER_TIMEOUT 2//-seconds

#define         BRIDGE_UDN_LEN                          30
#define         MAKER_UDN_LEN                           29

extern int gProcessData;

int StartPluginCtrlPoint(char* szAddress, unsigned short port);

int CtrlPointCallbackEventHandler(Upnp_EventType EventType,
                                 void *Event,
                                 void *Cookie);


int CtrlPointDiscoverDevices(void);

void CtrlPointPrintDevice();


void CtrlPointProcessDeviceDiscovery(IXML_Document *DescDoc, const char *location, int expires);
void CtrlPointProcessDeviceDiscoveryEx(IXML_Document *DescDoc, const char *location, int expires, int isAdv);

//- Following data struct are to manage plugin device, so all have prefix CtrlPoint
struct ctrlpoint_plugin_service
{
    char ServiceId[SIZE_256B];
    char ServiceType[SIZE_256B];
    char EventURL[SIZE_256B];
    char ControlURL[SIZE_256B];
    char SID[SIZE_256B];
};

typedef struct ctrlpoint_plugin_service CtrlPointPluginService, *pCtrlPointPluginService;

struct ctrlpoint_plugin_device
{
    char        UDN[SIZE_256B];
    char        DescDocURL[SIZE_256B];
    char        FriendlyName[SIZE_256B];
    char        PresURL[SIZE_256B];
    int         AdvrTimeOut;
    CtrlPointPluginService services[PLUGIN_MAX_SERVICES];
};

typedef struct ctrlpoint_plugin_device CtrlPointPluginDevice;

struct ctrlpoint_plugin_device_node
{
    CtrlPointPluginDevice device;
    struct ctrlpoint_plugin_device_node *next;
	int IsDeviceRequestUpdate;		//- In default, will not be requested
#if defined(SIMULATED_OCCUPANCY) || defined(PRODUCT_WeMo_LEDLight)
    int		Skip;
#endif
};

typedef struct ctrlpoint_plugin_device_node CtrlPluginDeviceNode, *pCtrlPluginDeviceNode;


//-----

int PluginCtrlPointGetApList(int devnum);
int PluginCtrlPointCloseAp(int devnum);
int PluginGetNetworkStatus(int devnum);

int PluginCtrlPointGetMetaInfo(int devnum);

int PluginCtrlPointGetBinary (int deviceIndex, const char* name);
int PluginCtrlPointSetBinaryState (int deviceIndex, const char** name);


int PluginCtrlPointSendAction(int service, int devnum, const char *actionname, const char **param_name, char **param_val, int param_count);

int PluginCtrlPointGetDevice(int devnum, pCtrlPluginDeviceNode *devnode);

int  PluginCtrlPointConnectHomeNetwork(int deviceIndex, int channel, const char* ssid, const char* Auth, const char* Encrypt, const char* password);

int PluginCtrlPointSetSensorEvent(int deviceIndex, const char* UDN, int eventStatus, const char* eventFriendlyName, int triggerDuration);

int PluginCtrlPointSyncTime(int deviceIndex, int utc, int TimeZone, int dstEnable);
int PluginCtrlPointRemoteAccess(int deviceIndex, const char* devId, int dstEnable, const char *hmId, const char* devName);
int PluginCtrlPointSetSmartDevUrl(int deviceIndex);
int PluginCtrlPointShareHWInfo(int deviceIndex);

int PluginCtrlPointSetFriendlyName(int deviceIndex, const char* name);

int PluginCtrlPointGetFriendlyName(int deviceIndex, const char* name);
int PluginCtrlPointGetHomeId(int deviceIndex, const char* name);
int PluginCtrlPointGetDeviceId(int deviceIndex, const char* name);
int PluginCtrlPointUpdateFirmware(int deviceIndex, const char* URL);
int PluginCtrlPointGetFirmware(int deviceIndex);

int PluginCtrlPointAddRule(int deviceIndex);

int GetDeviceIndexByUDN(const char* udn);
char* GetUDNByDeviceIndex(int nIndex);
int GetDeviceIndexNumber(void* arg);


int PluginCtrlPointSensorEventInd(int deviceIndex, int nowAction, int duration, int endAction, const char* udn);

int PluginCtrlPointChangeBinaryState (int deviceIndex, int newValue);


/**! This is for the auto test**/
void *AutoCtrlPointTestLoop(void *args);


/*
 *	Function set of device management sync
 *
 *
 *
 *
 *********************************/
void initDeviceSync();
void LockDeviceSync();
void UnlockDeviceSync();

void initSignNotify();
void LockSignNotify();
void UnlockSignNotify();

/*
 *	Function set of Control point stop
 *
 *
 *
 *
 *********************************/
int StopPluginCtrlPoint(void);
int CtrlPointRemoveAll(void);
int CtrlPointDeleteNode( CtrlPluginDeviceNode *node );

void ProcessUpNpNotify(struct Upnp_Event *event);
pCtrlPluginDeviceNode GetDeviceNodeBySID(const char* SID);

int CtrlPointRediscoverCallBack(void);

void EnableContrlPointRediscover(int isEnable);

void CtrlPointProcessDeviceByebye(IXML_Document *DescDoc);

void  AcquireHomeIdListLock(void);
void  ReleaseHomeIdListLock(void);
void  intHomeIdListLock(void);
int PluginCtrlPointSendActionAll(int service, const char *actionname, const char **param_name, char **param_val, int param_count);
#ifdef SIMULATED_OCCUPANCY
int PluginCtrlPtSendActionToAllSimDev(int service, const char *actionname, const char **param_name, char **param_val, int param_count);
int PluginCtrlPointSendActionSimulated(int service, const char *actionname);
#endif
inline void initHomeIdListLock();

extern int ctrlpt_handle;
extern pCtrlPluginDeviceNode g_pGlobalPluginDeviceList;

extern int PluginCtrlPointGetSimulatedRuleData(char *deviceUdn);
#endif /* CTRLPOINT_H_ */
