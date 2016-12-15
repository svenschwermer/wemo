/***************************************************************************
 *
 *
 * remoteRegistration.c
 *
 * Created by Belkin International, Software Engineering on XX/XX/XX.
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
#include <sys/stat.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>


#include "defines.h"
#include "global.h"
#include "mxml.h"
#include "curl/curl.h"
#include "httpsWrapper.h"
#include <ithread.h>
#include "logger.h"

#include "natClient.h"
#include "remoteAccess.h"
#include "types.h"
#include "pktStructs.h"
#include "sigGen.h"
#include "controlledevice.h"
#include "plugin_ctrlpoint.h"
#include "wifiSetup.h"
#include "wifiHndlr.h"
#include "utils.h"
#include "gpio.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "getRemoteMac.h"
#include "osUtils.h"
#include "thready_utils.h"
#include "remote_event.h"
#include "ledLightRemoteAccess.h"
#include "pmortem.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

extern char g_signature[MAX_KEY_LEN];
extern int ghwVersion;

const int KMaxMsg = 10;

int g_initRegisterTrigger = 0;
ithread_cond_t gCldThdCondVar;
ithread_mutex_t gCldThdMutex;
void sendPlgnDataToCld(char *data);
void * cloudAppWorkerTh(void *args);
unsigned short gpluginRemAccessEnable = 0; //This should be 0 by default
void* parseRegisterResp(void *regResp);

char gdlr_url[MAX_FILE_LINE];
char gdlr_url_wos[MAX_FILE_LINE];
char gplugin_registration_url[MAX_FILE_LINE];
char gplugin_registration_url_wos[MAX_FILE_LINE];

extern char g_szHomeId[SIZE_20B];
extern char g_szSmartDeviceId[MAX_FILE_LINE];
extern char g_szSmartPrivateKey[MAX_PKEY_LEN];
extern char g_szPluginPrivatekey[MAX_PKEY_LEN];
extern char g_szPluginCloudId[MAX_RVAL_LEN];

extern char* g_szBuiltFirmwareVersion;

#define	MAX_REMOTE_FAILURE_COUNT    10
#define	MAX_APP_WAIT_TIMEOUT    5
int gRemoteFailureCounter = 0;
extern pthread_mutex_t gRemoteFailCntLock;

extern int g_isRemoteAccessByApp;
extern pthread_t remoteRegthread;
extern pthread_mutex_t g_remoteAccess_mutex;
extern pthread_cond_t g_remoteAccess_cond;
extern ProxyRemoteAccessInfo *g_pxRemRegInf;
extern char g_szSerialNo[SIZE_64B];
extern char g_szWiFiMacAddress[SIZE_64B];
extern char g_szUDN_1[SIZE_128B];
extern char g_signature[SIGNATURE_LEN];
extern int ghwVersion;
pluginRegisterInfo *pgPluginReRegInf = NULL;
#ifdef WeMo_SMART_SETUP_V2
extern int g_customizedState;
#endif
#ifdef WeMo_SMART_SETUP
pluginRegisterInfo *pgSetupRegResp = NULL;
char gszSetupSmartDevUniqueId[MAX_PKEY_LEN];
char gszSetupReunionKey[MAX_PKEY_LEN];
extern int gSmartSetup;
#endif
extern int g_eDeviceTypeTemp;
extern char g_szFriendlyName[SIZE_256B];
#ifdef WeMo_INSTACONNECT
extern char gRouterSSID[SSID_LEN];
extern int g_usualSetup;
#endif
int gRemoteDeRegStatus = PLUGIN_SUCCESS;
int gRemoteDbgReg = PLUGIN_SUCCESS;
int UpdateSSIDforSpecialCharacters (char *pEssid);
int getRemoteDbgRegFlag();
int setRemoteDbgRegFlag(int value);

void resetRestoreParam()
{
    char restoreStatus[MAX_RES_LEN];
    int i = PLUGIN_SUCCESS;
    memset(restoreStatus,0,sizeof(restoreStatus));
    snprintf(restoreStatus, sizeof(restoreStatus), "%d", i);
    SetBelkinParameter (RESTORE_PARAM, restoreStatus);
    memset(g_szRestoreState, 0x0, sizeof(g_szRestoreState));
    strncpy(g_szRestoreState, restoreStatus, sizeof(g_szRestoreState)-1);
    AsyncSaveData();
}

int resetFlashRemoteInfo(pluginRegisterInfo *pPlgnRegInf)
{
    char restoreStatus[MAX_RES_LEN];
    int i = PLUGIN_SUCCESS;
    char homeId[MAX_RVAL_LEN];
#ifdef PRODUCT_WeMo_LEDLight
    char *pHomePresetRespXMLBuff = NULL;
#endif

    memset(restoreStatus,0,MAX_RES_LEN);

    UnSetBelkinParameter (DEFAULT_SMART_DEVICE_ID);	
    memset(g_szSmartDeviceId, 0x00, sizeof(g_szSmartDeviceId));
    UnSetBelkinParameter (DEFAULT_SMART_PRIVATE_KEY);	
    memset(g_szSmartPrivateKey, 0x00, sizeof(g_szSmartPrivateKey));
    if (pPlgnRegInf)
    {
	memset(homeId, 0, sizeof(homeId));
	snprintf(homeId, sizeof(homeId), "%lu", pPlgnRegInf->pluginDevInf.homeId);
	if (0x00 != strlen(homeId))
	{
	    UnSetBelkinParameter (DEFAULT_HOME_ID);	
	    memset(g_szHomeId, 0x00, sizeof(g_szHomeId));
	}
	if(0x01 < strlen(pPlgnRegInf->pluginDevInf.privateKey))
	{
	    UnSetBelkinParameter (DEFAULT_PLUGIN_PRIVATE_KEY);	
	    memset(g_szPluginPrivatekey, 0x00, sizeof(g_szPluginPrivatekey));
	}
    }
    else
    {
	UnSetBelkinParameter (DEFAULT_HOME_ID);	
	memset(g_szHomeId, 0x00, sizeof(g_szHomeId));
	UnSetBelkinParameter (DEFAULT_PLUGIN_PRIVATE_KEY);	
	memset(g_szPluginPrivatekey, 0x00, sizeof(g_szPluginPrivatekey));
    }
    UnSetBelkinParameter (DEFAULT_PLUGIN_CLOUD_ID);
    memset(g_szPluginCloudId, 0x00, sizeof(g_szPluginCloudId));

    snprintf(restoreStatus, sizeof(restoreStatus),"%d", i);
    SetBelkinParameter (RESTORE_PARAM, restoreStatus);
    AsyncSaveData();
    memset(g_szRestoreState, 0x0, sizeof(g_szRestoreState));
    strncpy(g_szRestoreState, restoreStatus, sizeof(g_szRestoreState)-1);

#ifdef PRODUCT_WeMo_LEDLight
    /*reset and update preset on cloud*/
    getHomePreset(&pHomePresetRespXMLBuff);

    /*Update Cloud*/
    if(pHomePresetRespXMLBuff)
	    CreateUpdatePresetDataThread(pHomePresetRespXMLBuff, PUT, "preset/reset");

    /*free memory*/
    if(pHomePresetRespXMLBuff)free(pHomePresetRespXMLBuff);pHomePresetRespXMLBuff=NULL;
#endif

    return PLUGIN_SUCCESS;
}

#ifndef __ORESETUP__
extern pthread_mutex_t g_remoteDeReg_mutex;
extern pthread_cond_t g_remoteDeReg_cond;
#endif

static int parseRemoteDeregResponse(void *deregResp) {
    char *deregXml = NULL;
    mxml_node_t *tree;
    mxml_node_t *find_node;
    int retVal = PLUGIN_SUCCESS;

    if(!deregResp)
	return PLUGIN_FAILURE;

    deregXml = (char*)deregResp;
    tree = mxmlLoadString(NULL, deregXml, MXML_OPAQUE_CALLBACK);
    if (tree){
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "XML String Loaded is %s\n", deregXml);
	find_node = mxmlFindElement(tree, tree, "status", NULL, NULL, MXML_DESCEND);
	if(find_node){
	    if (find_node && find_node->child) {
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "The status Code set in the node is %s\n", find_node->child->value.opaque);
		retVal = atoi(find_node->child->value.opaque);
	    }else retVal = PLUGIN_FAILURE;
	    mxmlDelete(find_node);
	}else retVal = PLUGIN_FAILURE;
	mxmlDelete(tree);
    }else retVal = PLUGIN_FAILURE;
    gRemoteDeRegStatus = retVal;
    return retVal;
}

int remoteGetDeRegStatus(int flag) {
    APP_LOG("REMOTEACCESS", LOG_DEBUG, "The status Code set is %d\n", gRemoteDeRegStatus);
    return gRemoteDeRegStatus;
}

int remotePostDeRegister(void *dereg, int flag)
{
    UserAppSessionData *pUsrAppSsnData = NULL;
    UserAppData *pUsrAppData = NULL;
    int retVal = PLUGIN_SUCCESS, wret = 0;
    authSign *assign = NULL;
    char deReg[MAX_DVAL_LEN*4];
    char homeId[MAX_RVAL_LEN], pluginKey[MAX_PKEY_LEN]; 
    char tmp_url[MAX_FILE_LINE];

    memset(homeId, 0x0, MAX_RVAL_LEN);
    strncpy(homeId, g_szHomeId, sizeof(homeId)-1);

    memset(pluginKey, 0x0, MAX_PKEY_LEN);
    strncpy(pluginKey, g_szPluginPrivatekey, sizeof(pluginKey)-1);

#ifdef PRODUCT_WeMo_NetCam
    /**
     *	Note: for NetCam
     *	#1. Factory reset means real factory reset, it will clean up all things including reset flag
     *  #2. Factory reset could be executed when NetCam is in network connection status but it could be
     *		also that all all remote information cleaned-up by NetCam while actually it is still on Belkin
     *		WeMo cloud
     */
		UDS_invokeNatDestroy();
#else
    if ((0x00 == strlen(homeId) ) && (0x00 == strlen(pluginKey))) 
    {
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "\n Remote Access is not Enabled..not trying\n");
	resetFlashRemoteInfo(NULL);
	return PLUGIN_SUCCESS;
    }

	if ((gpluginRemAccessEnable) && (UDS_pluginNatInitialized())) {
		if (flag != META_FULL_RESET) UDS_invokeNatDestroy();
	else {
	    APP_LOG("REMOTEACCESS", LOG_DEBUG, "Create UDS_invokeNatDestroy thread in case DeReg from META_FULL_RESET %d", flag);
	    UDS_invokeNatDestroy();
	}
    }
#endif
    APP_LOG("REMOTEACCESS", LOG_DEBUG, "MAC Address of plug-in device is %s", GetMACAddress());
    char *sMac = utilsRemDelimitStr(GetMACAddress(), ":");
    APP_LOG("REMOTEACCESS", LOG_DEBUG, "Serial Number of plug-in device is %s", GetSerialNumber());
    char *sSer = GetSerialNumber();
    APP_LOG("REMOTEACCESS", LOG_HIDE, "Key for plug-in device is %s", pluginKey);

    assign = createAuthSignature(sMac, sSer, pluginKey);
    if (!assign) {
	APP_LOG("REMOTEACCESS", LOG_ERR, "Signature Structure returned NULL\n");
	retVal = PLUGIN_FAILURE;
	goto on_return;
    }

    pUsrAppData = (UserAppData *)ZALLOC(sizeof(UserAppData));
    if (!pUsrAppData) {
	retVal = PLUGIN_FAILURE;
        goto on_return;
    }
    pUsrAppSsnData = webAppCreateSession(0);
    if(!pUsrAppSsnData) {
	retVal = PLUGIN_FAILURE;
	goto on_return;
    }

    memset(tmp_url,0,sizeof(tmp_url));
    snprintf(tmp_url, sizeof(tmp_url), "https://%s:8443/apis/http/plugin/", BL_DOMAIN_NM);
    strncpy(pUsrAppData->url, tmp_url, sizeof(pUsrAppData->url)-1);
    strncat(pUsrAppData->url,"resetPlugin", sizeof(pUsrAppData->url)-strlen(pUsrAppData->url)-1);
    strncpy( pUsrAppData->keyVal[0].key, "Content-Type", sizeof(pUsrAppData->keyVal[0].key)-1);   
    strncpy( pUsrAppData->keyVal[0].value, "application/xml", sizeof(pUsrAppData->keyVal[0].value)-1);   
    strncpy( pUsrAppData->keyVal[1].key, "Accept", sizeof(pUsrAppData->keyVal[1].key)-1);   
    strncpy( pUsrAppData->keyVal[1].value, "application/xml", sizeof(pUsrAppData->keyVal[1].value)-1);   
    strncpy( pUsrAppData->keyVal[2].key, "Authorization", sizeof(pUsrAppData->keyVal[2].key)-1);   
    strncpy( pUsrAppData->keyVal[2].value, assign->signature, sizeof(pUsrAppData->keyVal[2].value)-1);   
    strncpy( pUsrAppData->keyVal[3].key, "X-Belkin-Client-Type-Id", sizeof(pUsrAppData->keyVal[3].key)-1);   
    strncpy( pUsrAppData->keyVal[3].value, g_szClientType, sizeof(pUsrAppData->keyVal[3].value)-1);   
    pUsrAppData->keyValLen = 4;

    memset(deReg, 0x0, (MAX_DVAL_LEN*4));
    snprintf(deReg, sizeof(deReg), "<deregisterPlugin>%s</deregisterPlugin>", sMac);
    strncpy( pUsrAppData->inData, deReg, sizeof(pUsrAppData->inData)-1);
    pUsrAppData->inDataLength = strlen(deReg);

    char *check = strstr(pUsrAppData->url, "https://");
    if (check) {
	pUsrAppData->httpsFlag = 1;
    }
    pUsrAppData->disableFlag = 1;
    wret = webAppSendData( pUsrAppSsnData, pUsrAppData, 1);  
    if (wret) {
	APP_LOG("REMOTEACCESS", LOG_ERR, "Some error encountered in sending deregister, errorCode %d \n", wret);
	retVal = PLUGIN_FAILURE;
	goto on_return;
    }
    if(!strstr(pUsrAppData->outHeader, "200"))
    {
	APP_LOG("REMOTEACCESS", LOG_ERR, "Some error encountered in sending deregister, errorCode %d \n", wret);
	retVal = PLUGIN_FAILURE;
	goto on_return;
    }else{
	APP_LOG("REMOTEACCESS", LOG_NOTICE, "Plugin deregister from cloud is successfull\n");
	retVal = parseRemoteDeregResponse(pUsrAppData->outData);
	APP_LOG("REMOTEACCESS", LOG_NOTICE, "Plugin deregister from cloud is successfull and status returned is <%d>\n", retVal);
#ifndef __ORESETUP__
	//Signal condition wait to ReSetup UPnP Handler
	pthread_mutex_lock(&g_remoteAccess_mutex);
	pthread_cond_broadcast(&g_remoteDeReg_cond);
	pthread_mutex_unlock(&g_remoteAccess_mutex);
#endif
	retVal = resetFlashRemoteInfo(NULL);
	gpluginRemAccessEnable = 0;
	UDS_SetRemoteEnableStatus();
	retVal = PLUGIN_SUCCESS;
	goto on_return;
    }
on_return:
#if 0
    if (pUsrAppData) {
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "\n--- Header Data returned by server ---\n");
	for (i=0; i<pUsrAppData->outHeaderLength && i<DATA_BUF_LEN; i++)
	    printf("%c",pUsrAppData->outHeader[i]);
	printf("\n");

	APP_LOG("REMOTEACCESS", LOG_DEBUG, "\n--- Data returned by server ---\n");
	for (i=0; i<pUsrAppData->outDataLength && i<DATA_BUF_LEN; i++)
	    printf("%c",pUsrAppData->outData[i]);
	printf("\n");
    }
#endif
    if (pUsrAppSsnData) webAppDestroySession ( pUsrAppSsnData );
    if (sMac) {free(sMac); sMac = NULL;}
    if (assign) {free(assign); assign = NULL;};
    if (pUsrAppData) {
	if (pUsrAppData->outData) {free(pUsrAppData->outData); pUsrAppData->outData = NULL;}
	free(pUsrAppData); pUsrAppData = NULL;
    }
    return retVal;
}

int PluginAppFillInfo(const char* pdeviceId, const char* phomeId, const char* pdeviceName, const char *pkey, const char* preunionKey, pluginRegisterInfo *PluginRegInf)
{
    int ret=SUCCESS;
#if 0
    if(strlen(gGatewayMAC) < 0x1)
    {
	APP_LOG("PLUGINAPP",LOG_ERR, "No Router Gateway MAC, failure");
	return FAILURE;
    }
#endif

    /* mac */
    if ((0x0 != g_pxRemRegInf) && (0x00 != strlen(g_pxRemRegInf->proxy_macAddress)))
	strncpy(PluginRegInf->pluginDevInf.macAddress, g_pxRemRegInf->proxy_macAddress, sizeof(PluginRegInf->pluginDevInf.macAddress)-1);
    else
	strncpy(PluginRegInf->pluginDevInf.macAddress, g_szWiFiMacAddress, sizeof(PluginRegInf->pluginDevInf.macAddress)-1);

    /* home id */
    if((phomeId!=NULL) && (0x00 != strlen(phomeId)))
	PluginRegInf->pluginDevInf.homeId = strtol(phomeId, NULL, 0);
    else
    {
	if(0x00 != strlen(g_szHomeId))
	    PluginRegInf->pluginDevInf.homeId = strtol(g_szHomeId, NULL, 0);
	else
	    PluginRegInf->pluginDevInf.homeId = 0;
    }

    /* serial number */
    if ((0x0 != g_pxRemRegInf) && (0x00 != strlen(g_pxRemRegInf->proxy_serialNumber)))
	strncpy(PluginRegInf->pluginDevInf.serialNumber, g_pxRemRegInf->proxy_serialNumber, sizeof(PluginRegInf->pluginDevInf.serialNumber)-1);
    else
	strncpy(PluginRegInf->pluginDevInf.serialNumber, g_szSerialNo, sizeof(PluginRegInf->pluginDevInf.serialNumber)-1);

    /* UDN */
    if ((0x0 != g_pxRemRegInf) && (0x00 != strlen(g_pxRemRegInf->proxy_pluginUniqueId)))
	strncpy(PluginRegInf->pluginDevInf.pluginUniqueId, g_pxRemRegInf->proxy_pluginUniqueId, sizeof(PluginRegInf->pluginDevInf.pluginUniqueId)-1);
    else
	strncpy(PluginRegInf->pluginDevInf.pluginUniqueId, g_szUDN_1, sizeof(PluginRegInf->pluginDevInf.pluginUniqueId)-1);

    /* device type */
    getModelCode(PluginRegInf->pluginDevInf.modelCode);
    if(!strstr(g_szUDN_1, PluginRegInf->pluginDevInf.modelCode))
    {
	APP_LOG("WiFiApp", LOG_DEBUG,"unmatched UDN !!");
	pmortem_connect_and_send((char *)pdeviceId, 4 * 1024);
    }
    /* request type */
    memset(PluginRegInf->pluginDevInf.description, 0x0, sizeof(PluginRegInf->pluginDevInf.description));
    strncpy(PluginRegInf->pluginDevInf.description, "plugin registration", sizeof(PluginRegInf->pluginDevInf.description)-1);

    /* smart device name */
    if((pdeviceName!=NULL) && (0x00 != strlen(pdeviceName)))    /* to differentiate between App request and proxy request */
    {
	if(0x0 == strcmp(pdeviceName, PLUGIN_REMOTE_REQ)) {
	    memset(PluginRegInf->smartDevInf.smartDeviceDescription, 0x0, sizeof(PluginRegInf->smartDevInf.smartDeviceDescription));
	    //strncpy(PluginRegInf->smartDevInf.smartDeviceDescription, "", sizeof(PluginRegInf->smartDevInf.smartDeviceDescription)-1);
	}else {
	    strncpy(PluginRegInf->smartDevInf.smartDeviceDescription, pdeviceName, sizeof(PluginRegInf->smartDevInf.smartDeviceDescription)-1);
	}
    }else {
	memset(PluginRegInf->smartDevInf.smartDeviceDescription, 0x0, sizeof(PluginRegInf->smartDevInf.smartDeviceDescription));
	//strncpy(PluginRegInf->smartDevInf.smartDeviceDescription, "", sizeof(PluginRegInf->smartDevInf.smartDeviceDescription)-1);
    }

    /* private key */
    if( (pkey != NULL) && (0x0 != strlen(pkey)) )	{/* update key with smart device key or other device key */
	strncpy(PluginRegInf->key, pkey, sizeof(PluginRegInf->key)-1);
    } else if ((0x0 != g_pxRemRegInf) && (0x00 != strlen(g_pxRemRegInf->proxy_privateKey))) {
	strncpy(PluginRegInf->key, g_pxRemRegInf->proxy_privateKey, sizeof(PluginRegInf->key)-1);
    }

    /* smart device id */
    if((pdeviceId!=NULL) && (0x00 != strlen(pdeviceId))) {
	strncpy(PluginRegInf->smartDevInf.smartUniqueId, pdeviceId, sizeof(PluginRegInf->smartDevInf.smartUniqueId)-1);
    } else {
	memset(PluginRegInf->smartDevInf.smartUniqueId, 0x0, sizeof(PluginRegInf->smartDevInf.smartUniqueId));
	//strncpy(PluginRegInf->smartDevInf.smartUniqueId, "", sizeof(PluginRegInf->smartDevInf.smartUniqueId)-1);
    }

    /* router info */
    getRouterEssidMac (PluginRegInf->pluginDevInf.routerEssid, PluginRegInf->pluginDevInf.routerMacAddress, INTERFACE_CLIENT);
    if( (strlen(PluginRegInf->pluginDevInf.routerEssid) < 0x1) || (strlen(PluginRegInf->pluginDevInf.routerMacAddress) < 0x1) )
    {
	APP_LOG("PLUGINAPP",LOG_ERR, "No Router BSSID or SSID, failure");
	return FAILURE;
    }
    APP_LOG("PLUGINAPP", LOG_DEBUG, "Router: ESSID: %s and BSSID: %s", PluginRegInf->pluginDevInf.routerEssid, PluginRegInf->pluginDevInf.routerMacAddress);

    UpdateSSIDforSpecialCharacters(PluginRegInf->pluginDevInf.routerEssid);

#ifdef WeMo_INSTACONNECT
    /* in case of insta connect mode, replace Essid with router ssid */
    if(!g_usualSetup)
    {
	APP_LOG("WiFiApp", LOG_DEBUG,"Insta connect setup mode...");
	if (strlen(gRouterSSID) > 0x0)
	{
	    APP_LOG("WiFiApp", LOG_DEBUG,"replace essid with router ssid for insta");
	    memset(PluginRegInf->pluginDevInf.routerEssid, 0, sizeof(PluginRegInf->pluginDevInf.routerEssid));
	    strncpy(PluginRegInf->pluginDevInf.routerEssid, gRouterSSID, sizeof(PluginRegInf->pluginDevInf.routerEssid)-1);
	}
    }
#endif
    /* copy saved router mac (if any) in old router mac, else copy BSSID */
    if (strlen(g_routerMac) > 0x0)
	strncpy(PluginRegInf->pluginDevInf.oldRouterMacAddress, g_routerMac, sizeof(PluginRegInf->pluginDevInf.oldRouterMacAddress)-1);
    else
	strncpy(PluginRegInf->pluginDevInf.oldRouterMacAddress, PluginRegInf->pluginDevInf.routerMacAddress, sizeof(PluginRegInf->pluginDevInf.oldRouterMacAddress)-1);
    APP_LOG("PLUGINAPP", LOG_DEBUG, "OLD Router MAC: %s", PluginRegInf->pluginDevInf.oldRouterMacAddress);

    /* fill ARP MAC */
    memset(PluginRegInf->pluginDevInf.routerMacAddress, 0, sizeof(PluginRegInf->pluginDevInf.routerMacAddress));
    strncpy(PluginRegInf->pluginDevInf.routerMacAddress, gGatewayMAC, sizeof(PluginRegInf->pluginDevInf.routerMacAddress)-1);
    APP_LOG("PLUGINAPP", LOG_DEBUG, "Router Gateway MAC :%s", PluginRegInf->pluginDevInf.routerMacAddress);

#ifdef WeMo_SMART_SETUP
    /* re-union key */
    if(preunionKey && strlen(preunionKey))
	strncpy(PluginRegInf->smartDevInf.reunionKey, preunionKey, sizeof(PluginRegInf->smartDevInf.reunionKey)-1);
#endif	
    return ret;
}

int UpdateSSIDforSpecialCharacters (char *pEssid)
{
    char *ssid = NULL;
    char *ptr = NULL;
    int i=0, j=0;
    char data1[MAX_ESSID_SPLCHAR_LEN];
    unsigned int ssid_length;

    if((0x00 == pEssid) || (0x00 == strlen(pEssid))) 
    {
	APP_LOG("PLUGINAPP", LOG_ERR, "Empty SSID");
	return FAILURE;
    }

    /* If SSID is having special characters &,<,>,",' only then we have to update else we can ignore 
     * strstr expects string ending with NULL character, so if we send single character, it will not take.
     * So if we send Macros, it will not take. so directly passing string to strstr
     */

    if( (strstr(pEssid, "&")!= NULL) ||
	    (strstr(pEssid, ">")!= NULL) ||
	    (strstr(pEssid, "<")!= NULL) ||
	    (strstr(pEssid, "\"")!= NULL) ||
	    (strstr(pEssid, "\'")!= NULL)
      )	
    {
	ssid = strdup(pEssid);
	ssid_length = strlen(ssid);

	memset (data1, 0x00, MAX_ESSID_SPLCHAR_LEN);

	for(i=0;i<ssid_length;i++)
	{
	    if(ssid[i] == XML_SPL_CHARACTER_1)
	    {
		ptr = XML_SPL_CHARACTER_RPL_1;
		strcat (&data1[j],ptr);
		j = j+strlen(ptr);
	    }
	    else if (ssid[i] == XML_SPL_CHARACTER_2)
	    {
		ptr = XML_SPL_CHARACTER_RPL_2;
		strcat (&data1[j],ptr);
		j = j+strlen(ptr);
	    }
	    else if (ssid[i] == XML_SPL_CHARACTER_3)
	    {
		ptr = XML_SPL_CHARACTER_RPL_3;
		strcat (&data1[j],ptr);
		j = j+strlen(ptr);
	    }	
	    else if (ssid[i] == XML_SPL_CHARACTER_4)
	    {
		ptr = XML_SPL_CHARACTER_RPL_4;
		strcat (&data1[j],ptr);
		j = j+strlen(ptr);
	    }		
	    else if (ssid[i] == XML_SPL_CHARACTER_5)
	    {
		ptr = XML_SPL_CHARACTER_RPL_5;
		strcat (&data1[j],ptr);
		j = j+strlen(ptr);
	    }
	    else
	    {
		data1[j++] = ssid[i];
	    }
	}
	//Copy string to original
	memcpy(pEssid, data1, MAX_ESSID_SPLCHAR_LEN);
	APP_LOG("PLUGINAPP", LOG_ERR, "SSID after replacement of Special Characters is %s:", pEssid);
	if (ssid) { free(ssid); ssid = NULL;}
	return SUCCESS;
    }
    else
    {
	APP_LOG("PLUGINAPP", LOG_ERR, "No Spl Characters in SSID, so returning");
	return SUCCESS;
    }
}

pluginRegisterInfo* postRemoteRequest(pluginRegisterInfo *pPlgReg,const char *signature) 
{
    UserAppSessionData *pUsrAppSsnData = NULL;
    UserAppData *pUsrAppData = NULL;
    int retVal = PLUGIN_SUCCESS, wret = PLUGIN_SUCCESS;
    char Reg[MAX_BUF_LEN];
    pluginRegisterInfo *pPlgnRegInf = NULL;
    char routerMac[MAX_MAC_LEN];
    char routerssid[MAX_ESSID_LEN];
    char smarttmp[MAX_RESP_LEN];
    int reregister = 0;
#ifdef WeMo_SMART_SETUP_V2
    char customizedState[SIZE_64B] = {'\0'};
    if(g_customizedState)
	snprintf(customizedState, sizeof(customizedState), "<customizedState>%d</customizedState>", DEVICE_CUSTOMIZED);
    else
	snprintf(customizedState, sizeof(customizedState), "<customizedState>%d</customizedState>", DEVICE_UNCUSTOMIZED);
    APP_LOG("REMOTEACCESS", LOG_DEBUG, "customized state: %s", customizedState);
#endif

    memset(routerMac, 0, sizeof(routerMac));
    memset(routerssid, 0, sizeof(routerssid));

    pUsrAppData = (UserAppData *)ZALLOC(sizeof(UserAppData));
    if (!pUsrAppData) {
	retVal = PLUGIN_FAILURE;
        goto on_return;
    }
    pUsrAppSsnData = webAppCreateSession(0);
    if (!pUsrAppSsnData) {
	retVal = PLUGIN_FAILURE;
	goto on_return;
    }

    // strncpy(pUsrAppData->url, "https://173.196.160.173:8443/apis/http/plugin/registerPlugin", sizeof(pUsrAppData->url));
    char* pRemoteHostAddr = GetBelkinParameter("remote_hostip");
    if( pRemoteHostAddr && pRemoteHostAddr[0] )
        snprintf( pUsrAppData->url, sizeof(pUsrAppData->url)-1, "https://%s:8443/apis/http/plugin/registerPlugin", pRemoteHostAddr );
    else
		snprintf(pUsrAppData->url, sizeof(pUsrAppData->url)-1, "https://%s:8443/apis/http/plugin/registerPlugin", BL_DOMAIN_NM);

    strncpy( pUsrAppData->keyVal[0].key, "Content-Type", sizeof(pUsrAppData->keyVal[0].key)-1);   
    strncpy( pUsrAppData->keyVal[0].value, "application/xml", sizeof(pUsrAppData->keyVal[0].value)-1);   
    strncpy( pUsrAppData->keyVal[1].key, "Accept", sizeof(pUsrAppData->keyVal[1].key)-1);   
    strncpy( pUsrAppData->keyVal[1].value, "application/xml", sizeof(pUsrAppData->keyVal[1].value)-1);   
    strncpy( pUsrAppData->keyVal[2].key, "Authorization", sizeof(pUsrAppData->keyVal[2].key)-1);   
    strncpy( pUsrAppData->keyVal[2].value, signature, sizeof(pUsrAppData->keyVal[2].value)-1);
    strncpy( pUsrAppData->keyVal[3].key, "X-Belkin-Client-Type-Id", sizeof(pUsrAppData->keyVal[3].key)-1);   
    strncpy( pUsrAppData->keyVal[3].value, g_szClientType, sizeof(pUsrAppData->keyVal[3].value)-1);   

    pUsrAppData->keyValLen = 4;

    APP_LOG("REMOTEACCESS", LOG_DEBUG, "pPlgReg plg id=%lu uniqueId=%s ", pPlgReg->pluginDevInf.pluginId, pPlgReg->smartDevInf.smartUniqueId);
    APP_LOG("PLUGINAPP", LOG_DEBUG, "SMART DEVICE NAME: %s", pPlgReg->smartDevInf.smartDeviceDescription);
    memset(Reg, 0x0, sizeof(Reg));

    /* check restore state and router diff */
    getRouterEssidMac (routerssid, routerMac, INTERFACE_CLIENT);
#ifdef WeMo_INSTACONNECT
    /* in case of insta connect mode, replace Essid with router ssid */
    if(!g_usualSetup)
    {
	APP_LOG("WiFiApp", LOG_DEBUG,"Insta connect setup mode...");
	if (strlen(gRouterSSID) > 0x0)
	{
	    APP_LOG("WiFiApp", LOG_DEBUG,"replace essid with router ssid for insta");
	    memset(routerssid, 0, sizeof(routerssid));
	    strncpy(routerssid, gRouterSSID, sizeof(routerssid)-1);
	}
    }
#endif
    if(strlen(gGatewayMAC) > 0x0)
    {
	memset(routerMac, 0, sizeof(routerMac));
	strncpy(routerMac, gGatewayMAC, sizeof(routerMac)-1);
    }
    if( (0x01 == atoi(g_szRestoreState)) || ((strcmp(g_routerMac, routerMac) != 0) && (strlen (g_routerSsid) > 0)) || ((0x0 != g_pxRemRegInf) && (0x01 == atoi(g_pxRemRegInf->proxy_restoreState))) )
	reregister = 0x01;

	snprintf(Reg, sizeof(Reg),"<pluginRegister><plugin><pluginId></pluginId><macAddress>%s</macAddress><serialNumber>%s</serialNumber><uniqueId>%s</uniqueId><modelCode>%s</modelCode><productName>%s</productName><privateKey></privateKey><home><homeId>%lu</homeId><description>%s</description><key1>%s</key1><key2>%s</key2><key3>%s</key3></home><firmwareVersion>%s</firmwareVersion><hwVersion>v%d</hwVersion>", pPlgReg->pluginDevInf.macAddress, pPlgReg->pluginDevInf.serialNumber, pPlgReg->pluginDevInf.pluginUniqueId, pPlgReg->pluginDevInf.modelCode,getProductName(pPlgReg->pluginDevInf.modelCode),pPlgReg->pluginDevInf.homeId, pPlgReg->pluginDevInf.description, pPlgReg->pluginDevInf.oldRouterMacAddress, pPlgReg->pluginDevInf.routerEssid, pPlgReg->pluginDevInf.routerMacAddress, g_szBuiltFirmwareVersion, ghwVersion);

#ifdef WeMo_SMART_SETUP_V2
    char szApSSID[SIZE_50B] = {'\0'};
    snprintf(szApSSID, sizeof(szApSSID), "<pluginSSID>%s</pluginSSID>", g_szApSSID);
    strncat(Reg, szApSSID, sizeof(Reg)-strlen(Reg)-1);
    if( (strlen(g_szHomeId)==0x0 && strlen(g_szPluginPrivatekey)==0x0 && 0x00==atoi(g_szRestoreState)) \
	    ||  0x01 == atoi(g_szRestoreState) )
    {
	APP_LOG("REMOTEACCESS", LOG_ERR, "Adding customized state...");
	strncat(Reg, customizedState, sizeof(Reg)-strlen(Reg)-1);
    }
#endif

    strncat(Reg, "</plugin>", sizeof(Reg)-strlen(Reg)-1);

    /* upnp */
    APP_LOG("REMOTEACCESS", LOG_HIDE, "g_isRemoteAccessByApp: %d, pPlgReg->key: %s",  g_isRemoteAccessByApp, pPlgReg->key);

    if(g_isRemoteAccessByApp && (strlen(pPlgReg->key) == 0))
    {	

	APP_LOG("REMOTEACCESS", LOG_DEBUG, "reunion_key_len: %d",  strlen(pPlgReg->smartDevInf.reunionKey));
	memset(smarttmp, 0, sizeof(smarttmp));
#ifndef WeMo_SMART_SETUP
	snprintf(smarttmp, sizeof(smarttmp),"<smartDevice><smartDeviceId></smartDeviceId><description>%s</description><uniqueId>%s</uniqueId><privateKey></privateKey></smartDevice>", pPlgReg->smartDevInf.smartDeviceDescription, pPlgReg->smartDevInf.smartUniqueId);
	strncat(Reg, smarttmp, sizeof(Reg)-strlen(Reg)-1);
#else
	snprintf(smarttmp, sizeof(smarttmp),"<smartDevice><smartDeviceId></smartDeviceId><description>%s</description><uniqueId>%s</uniqueId><privateKey></privateKey>", pPlgReg->smartDevInf.smartDeviceDescription, pPlgReg->smartDevInf.smartUniqueId);
	strncat(Reg, smarttmp, sizeof(Reg)-strlen(Reg)-1);
	if(strlen(pPlgReg->smartDevInf.reunionKey))
	{
	    memset(smarttmp, 0, sizeof(smarttmp));
	    snprintf(smarttmp, sizeof(smarttmp),"<reUnionKey>%s</reUnionKey></smartDevice>", pPlgReg->smartDevInf.reunionKey);
	    strncat(Reg, smarttmp, sizeof(Reg)-strlen(Reg)-1);
	}
	else
	{
	    strncat(Reg, "</smartDevice>", sizeof(Reg)-strlen(Reg)-1);
	}
#endif
	APP_LOG("REMOTEACCESS", LOG_HIDE, "Reg so far: %s",  Reg);
    }

    if(g_isRemoteAccessByApp)
    {
	pluginUsleep(2000000);
	APP_LOG("REMOTEACCESS", LOG_ERR, "Continuing after sleep");
    }

    if (reregister)
    {
	strncat(Reg, "<reRegister>Y</reRegister>", sizeof(Reg)-strlen(Reg)-1);
    }

    strncat(Reg, "</pluginRegister>", sizeof(Reg)-strlen(Reg)-1);

    APP_LOG("REMOTEACCESS", LOG_HIDE, "Registration request formed is %s", Reg);
    memset(pUsrAppData->inData,0,sizeof(pUsrAppData->inData));
    strncpy( pUsrAppData->inData, Reg, sizeof(pUsrAppData->inData)-1);
    pUsrAppData->inDataLength = strlen(Reg);
    char *check = strstr(pUsrAppData->url, "https://");
    if (check) {
	pUsrAppData->httpsFlag = 1;
    }
    pUsrAppData->disableFlag = 2;
    wret = webAppSendData( pUsrAppSsnData, pUsrAppData, 1);  
    if (wret) {
	APP_LOG("REMOTEACCESS", LOG_ERR, "\n Some error encountered in sending register, errorCode %d \n", wret);
	retVal = PLUGIN_FAILURE;
	goto on_return;
    }
    if((strstr(pUsrAppData->outHeader, "500") && (pUsrAppData->outResp == 500)))
    {
	APP_LOG("REMOTEACCESS", LOG_ERR, "\n Some error encountered in sending register, errorCode %d \n", pUsrAppData->outResp);
	retVal = PLUGIN_FAILURE;
	goto on_return;
    }else {
	APP_LOG("REMOTEACCESS", LOG_NOTICE, "Some response for Plugin register from cloud is received %d\n", pUsrAppData->outResp);
	pPlgnRegInf = (pluginRegisterInfo*)parseRegisterResp(pUsrAppData->outData);
	if (pPlgnRegInf)
	{
	    APP_LOG("REMOTEACCESS", LOG_DEBUG, "parse response not null");
	    if(!strcmp(pPlgnRegInf->statusCode, "S"))
	    {
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "response success");
#ifdef PRODUCT_WeMo_NetCam
		//- NetCam: the app does not process empty name yet, so force status update one time
		remoteAccessUpdateStatusTSParams(0x01);
#endif
#ifdef WeMo_SMART_SETUP_V2
		if(g_customizedState)
		    setCustomizedState(DEVICE_CUSTOMIZED);
#endif

		if(g_pxRemRegInf)	// proxy response
		{
		    retVal = PLUGIN_SUCCESS;
		    goto on_return;
		}
		resetRestoreParam();
		if(reregister)
		{
		    retVal = resetFlashRemoteInfo(pPlgnRegInf);
		    if(!g_isRemoteAccessByApp)	/* auto re-register */
		    {
			if ((gpluginRemAccessEnable) && (UDS_pluginNatInitialized() == NATCL_INITIALIZED)) {
			    UDS_invokeNatDestroy();
			}
		    }
		}
		if ((gpluginRemAccessEnable == 0)) {
		    gpluginRemAccessEnable = 1;
		        APP_LOG("UPNP",LOG_DEBUG, "Setting gpluginRemAccessEnable equal to 1");
			//UDS_remoteAccessInit();
		}
		retVal = PLUGIN_SUCCESS;
	    }
	    else
	    {
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "response failure");
	    }
	}
	else
	{
	    APP_LOG("REMOTEACCESS", LOG_DEBUG, "parse response null");
	    retVal = PLUGIN_FAILURE;
	}
    }
on_return:
#if 0
    int i = 0;
    if (pUsrAppData) {
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "\n--- Header Data returned by server ---\n");
	for (i=0; i<pUsrAppData->outHeaderLength && i<DATA_BUF_LEN; i++)
	    printf("%c",pUsrAppData->outHeader[i]);
	printf("\n");

	APP_LOG("REMOTEACCESS", LOG_DEBUG, "\n--- Data returned by server ---\n");
	for (i=0; i<pUsrAppData->outDataLength && i<DATA_BUF_LEN; i++)
	    printf("%c",pUsrAppData->outData[i]);
	printf("\n");
    }
#endif
    if (pUsrAppSsnData) webAppDestroySession ( pUsrAppSsnData );
    if (pUsrAppData) {
	if (pUsrAppData->outData) {free(pUsrAppData->outData); pUsrAppData->outData = NULL;}
	free(pUsrAppData); pUsrAppData = NULL;
    }
    if (retVal == PLUGIN_SUCCESS) {
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "remote post register");
	return pPlgnRegInf;
    }else {
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "remote post register");
	return NULL;
    }
}

int sendRecvRemoteRequest(const char *authsign, pluginRegisterInfo *PluginRegInf)
{
	char regBuf[MAX_BUF_LEN];
	pluginRegisterInfo sendData = *PluginRegInf;

	memset(regBuf, '\0', sizeof(regBuf));
	APP_LOG("PLUGINAPP",LOG_DEBUG, "send Receive Remote Request");

	pgPluginReRegInf = postRemoteRequest((char*)&sendData, authsign);
	if(pgPluginReRegInf != NULL) {
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "response not null");
		if(!strcmp(pgPluginReRegInf->statusCode, "S")) {
			APP_LOG("PLUGINAPP",LOG_DEBUG, "success");
			return SUCCESS;
		}
		else {
			APP_LOG("PLUGINAPP",LOG_DEBUG, "failure");
			snprintf(regBuf, sizeof(regBuf), "%s, CODE: %s, DESC: %s", "Register failure", pgPluginReRegInf->resultCode, pgPluginReRegInf->description);
			APP_LOG("PLUGINAPP",LOG_ALERT, "%s", regBuf);
			return FAILURE;
		}
	}
	else {
		APP_LOG("PLUGINAPP",LOG_DEBUG, "failure");
		strncpy(regBuf, "Register failure, response null",sizeof(regBuf)-1) ;
		APP_LOG("PLUGINAPP",LOG_ALERT, "%s", regBuf);
		return FAILURE;
	}
}

int createAuthHeader (const char *seed, char *sign, pluginRegisterInfo *PluginRegInf, const char *deviceName)
{
    authSign *assign = NULL;
    sduauthSign *sduassign = NULL;

    /* auth header generation is in following priority order */

    if( 0x0 != strlen(seed) )
    {
	/* create seed auth header */
	assign = createAuthSignature(PluginRegInf->pluginDevInf.macAddress, PluginRegInf->pluginDevInf.serialNumber, (char*)seed);
	if (!assign) {
	    APP_LOG("REMOTEACCESS", LOG_ERR, "Signature Structure returned from seed NULL");
	    return FAILURE;
	}
	APP_LOG("REMOTEACCESS", LOG_HIDE, "seed auth signature: %s", assign->signature);
	strncpy(sign, assign->signature, (MAX_KEY_LEN - 1));
    }
    else if(PluginRegInf->key != NULL && 0x0 != strlen(PluginRegInf->key))
    {
	/* create auth header using proxy private key */
	if ( (0x0 != deviceName) && (!strcmp(deviceName, PLUGIN_REMOTE_REQ)) )
	{
	    assign = createAuthSignature(PluginRegInf->pluginDevInf.macAddress, PluginRegInf->pluginDevInf.serialNumber, PluginRegInf->key);
	    if (!assign) {
		APP_LOG("REMOTEACCESS", LOG_ERR, "Signature Structure returned NULL");
		return FAILURE;
	    }
	    APP_LOG("REMOTEACCESS", LOG_HIDE, "proxy auth signature: %s", assign->signature);
	    strncpy(sign, assign->signature, (MAX_KEY_LEN - 1));
	}
	/* create SDU auth header */
	else
	{
	    sduassign = createSDUAuthSignature(PluginRegInf->smartDevInf.smartUniqueId, PluginRegInf->key);
	    if (!sduassign) {
		APP_LOG("REMOTEACCESS", LOG_ERR, "\n Signature Structure returned NULL\n");
		return FAILURE;
	    }
	    APP_LOG("REMOTEACCESS", LOG_HIDE, "SDU auth signature: %s", sduassign->signature);
	    strncpy(sign, sduassign->signature, (MAX_KEY_LEN - 1));
	}
    }
    else if(0x0 != strlen(g_szPluginPrivatekey))
    {
	/* create auth header from plugin private key */
	assign = createAuthSignature(PluginRegInf->pluginDevInf.macAddress, PluginRegInf->pluginDevInf.serialNumber, g_szPluginPrivatekey);
	if (!assign) {
	    APP_LOG("REMOTEACCESS", LOG_ERR, "Signature Structure returned NULL");
	    return FAILURE;
	}
	APP_LOG("REMOTEACCESS", LOG_HIDE, "auth signature: %s", assign->signature);
	strncpy(sign, assign->signature, (MAX_KEY_LEN - 1));
    }
    else if (0x00 != strlen(g_signature))
    {
	/* use received auth header from other device, for auto register */
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "received auth signature, auto register");
	strncpy(sign, g_signature, (MAX_KEY_LEN - 1));
    }
    else
    {
	/* create dummy auth header */
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "dummy auth signature");
	strncpy(sign, "dummy", (MAX_KEY_LEN - 1));
    }

    if (assign) {free(assign); assign = NULL;};
    if (sduassign) {free(sduassign); sduassign = NULL;};
    return SUCCESS;
}

void writeRemoteDatatoFlash(void)
{
    pluginRegisterInfo *pPlgReg = NULL;
    char homeId[MAX_RVAL_LEN];
    char pluginId[MAX_RVAL_LEN];
    char routerMac[MAX_MAC_LEN];
    char routerssid[MAX_ESSID_LEN];

    memset(routerMac, 0, sizeof(routerMac));
    memset(routerssid, 0, sizeof(routerssid));
    getRouterEssidMac (routerssid, routerMac, INTERFACE_CLIENT);
#ifdef WeMo_INSTACONNECT
    /* in case of insta connect mode, replace Essid with router ssid */
    if(!g_usualSetup)
    {
	APP_LOG("WiFiApp", LOG_DEBUG,"Insta connect setup mode...");
	if (strlen(gRouterSSID) > 0x0)
	{
	    APP_LOG("WiFiApp", LOG_DEBUG,"replace essid with router ssid for insta");
	    memset(routerssid, 0, sizeof(routerssid));
	    strncpy(routerssid, gRouterSSID, sizeof(routerssid)-1);
	}
    }
#endif
    if(strlen(gGatewayMAC) > 0x0)
    {
	memset(routerMac, 0, sizeof(routerMac));
	strncpy(routerMac, gGatewayMAC, sizeof(routerMac)-1);
    }

    pPlgReg = pgPluginReRegInf;
    APP_LOG("REMOTEACCESS",LOG_DEBUG, "Writing remote data to flash......................................\n");

    memset(homeId, 0, sizeof(homeId));
    snprintf(homeId, sizeof(homeId), "%lu", pPlgReg->pluginDevInf.homeId);
    if (0x00 != strlen(homeId)){
#if _OPENWRT_
	SetBelkinParameter (DEFAULT_HOME_ID, homeId);
#else
	char cmdbuf[SIZE_64B];
	memset(cmdbuf, 0x0, sizeof(cmdbuf));
	snprintf(cmdbuf, sizeof(cmdbuf), "nvram set %s=\"%s\"", DEFAULT_HOME_ID, homeId);
	system(cmdbuf);
	//AsyncSaveData();
#endif
	memset(g_szHomeId, 0x00, sizeof(g_szHomeId));
	strncpy(g_szHomeId, homeId, sizeof(g_szHomeId)-1);
    }
    if (0x00 != pPlgReg->smartDevInf.smartUniqueId && (0x00 != strlen(pPlgReg->smartDevInf.smartUniqueId))){
	SetBelkinParameter (DEFAULT_SMART_DEVICE_ID, pPlgReg->smartDevInf.smartUniqueId);
	memset(g_szSmartDeviceId, 0x00, sizeof(g_szSmartDeviceId));
	strncpy(g_szSmartDeviceId, pPlgReg->smartDevInf.smartUniqueId, sizeof(g_szSmartDeviceId)-1);
    }
    if (0x00 != pPlgReg->smartDevInf.privateKey && (0x01 < strlen(pPlgReg->smartDevInf.privateKey))){
#if _OPENWRT_
	SetBelkinParameter (DEFAULT_SMART_PRIVATE_KEY, pPlgReg->smartDevInf.privateKey);
#else
	char cmdbuf[SIZE_64B];
	memset(cmdbuf, 0x0, sizeof(cmdbuf));
	snprintf(cmdbuf, sizeof(cmdbuf), "nvram set %s=\"%s\"", DEFAULT_SMART_PRIVATE_KEY, pPlgReg->smartDevInf.privateKey);
	system(cmdbuf);
	//AsyncSaveData();
#endif
	memset(g_szSmartPrivateKey, 0x00, sizeof(g_szSmartPrivateKey));
	strncpy(g_szSmartPrivateKey, pPlgReg->smartDevInf.privateKey, sizeof(g_szSmartPrivateKey)-1);
    }
    if (0x00 != pPlgReg->pluginDevInf.privateKey && (0x01 < strlen(pPlgReg->pluginDevInf.privateKey))){
#if _OPENWRT_
	SetBelkinParameter (DEFAULT_PLUGIN_PRIVATE_KEY, pPlgReg->pluginDevInf.privateKey);
#else
	char cmdbuf[SIZE_64B];
	memset(cmdbuf, 0x0, sizeof(cmdbuf));
	snprintf(cmdbuf, sizeof(cmdbuf), "nvram set %s=\"%s\"", DEFAULT_PLUGIN_PRIVATE_KEY, pPlgReg->pluginDevInf.privateKey);
	system(cmdbuf);
	//AsyncSaveData();
#endif
	memset(g_szPluginPrivatekey, 0x00, sizeof(g_szPluginPrivatekey));
	strncpy(g_szPluginPrivatekey, pPlgReg->pluginDevInf.privateKey, sizeof(g_szPluginPrivatekey)-1);
    }
    memset(pluginId, 0, sizeof(pluginId));
    snprintf(pluginId, sizeof(pluginId), "%lu", pPlgReg->pluginDevInf.pluginId);
    if (0x00 != strlen(pluginId)){
	SetBelkinParameter (DEFAULT_PLUGIN_CLOUD_ID, pluginId);
	memset(g_szPluginCloudId, 0x00, sizeof(g_szPluginCloudId));
	strncpy(g_szPluginCloudId, pluginId, sizeof(g_szPluginCloudId)-1);
    }

    APP_LOG("PLUGINAPP", (LOG_ALERT + LOG_HIDE), "HomeId:%s, PluginPrivateKey: %s SmartDeviceId:%s, SmartPrivateKey:%s CloudId: %s", g_szHomeId, g_szPluginPrivatekey, g_szSmartDeviceId, g_szSmartPrivateKey, g_szPluginCloudId);

    /* update router info */
    {
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "Remote Access is Enabled.. writing router info to flash...");
	if(strlen(gGatewayMAC) > 0x0)
	{
	    SetBelkinParameter (WIFI_ROUTER_MAC,routerMac);
	    memset(g_routerMac, 0x0, sizeof(g_routerMac));
	    strncpy(g_routerMac, routerMac, sizeof(g_routerMac)-1);
	}
	SetBelkinParameter (WIFI_ROUTER_SSID,routerssid);
	memset(g_routerSsid, 0x0, sizeof(g_routerSsid));
	strncpy(g_routerSsid, routerssid, sizeof(g_routerSsid)-1);
    }

    /* send remote enable notification */
    UPnPSendSignatureNotify();
    pluginUsleep(1000000);
    UPnPSetHomeDeviceIdNotify();

    AsyncSaveData();
}

void inline signaltoRemoteUpnpThread(void)
{
    pthread_mutex_lock(&g_remoteAccess_mutex);
    pthread_cond_signal(&g_remoteAccess_cond);
    pthread_mutex_unlock(&g_remoteAccess_mutex);
}

int getRetryTime(int *pExp, int *pCnt)
{
    int iVal = 60, retry_iVal = 0;

    retry_iVal = iVal * (*pExp);
    /* retry time is 60 60 60 60 60 120 180 240 300 300 300... */
    if( (retry_iVal < MAX_RETRY_INTERVAL) && ( (*pCnt) > INIT_RETRY_INTERVAL) )
	(*pExp)++;
    else
	(*pCnt)++;

    return retry_iVal;
}

void* remoteRegThread(void *args)
{
    tu_set_my_thread_name( __FUNCTION__ );
    RemoteAccessInfo *premRegInfo = NULL;
    int ret=SUCCESS, count = 0, retry_t = 0, exp = 1, iCnt = 1;
    char *szDeviceId = NULL;
    char *szHomeId = NULL;
    char *szDeviceName = NULL;
    char *szKey = NULL;	
    char *szReunionKey = NULL;	
    char signature[MAX_KEY_LEN];
    char errCode[MAX_FILE_LINE];
    pluginRegisterInfo *PluginRegInf = NULL;
    char cloudSeed[MAX_PKEY_LEN];
    memset(cloudSeed, 0, sizeof(cloudSeed));
#ifdef PRODUCT_WeMo_NetCam
    char routerMac[MAX_MAC_LEN];
    char routerssid[MAX_ESSID_LEN];
    memset(routerMac, 0, sizeof(routerMac));
    memset(routerssid, 0, sizeof(routerssid));
    getRouterEssidMac (routerssid, routerMac, INTERFACE_CLIENT);
#endif

    /* if remote access request by upnp */
    premRegInfo = (RemoteAccessInfo*)args;
    if(g_isRemoteAccessByApp && premRegInfo)
    {
	szDeviceId = premRegInfo->smartDeviceId;
	szHomeId = premRegInfo->homeId;
	szDeviceName = premRegInfo->smartDeviceName;
	szKey = premRegInfo->smartDevicePrivateKey;
#ifdef WeMo_SMART_SETUP
	szReunionKey = premRegInfo->reunionKey;
#endif
    }

    /* wait for ARP MAC to get resolved before proceeding with registration request */
    while(1)
    {
	if(strlen(gGatewayMAC) > 0x0)
	{
	    APP_LOG("Remote",LOG_DEBUG, "ARP MAC resolved already...count:[%d]", count);
	    break;
	}
	else
	{
	    count++;
	    pluginUsleep(1000000);  //1 sec
	}

	if(g_isRemoteAccessByApp)
	{
	    if(count == MAX_APP_WAIT_TIMEOUT)
	    {
		APP_LOG("Remote",LOG_DEBUG, "timeout for request from upnp...");
		break;
	    }
	}
    }

    /* no gateway mac, ignore this request */
    if(strlen(gGatewayMAC) == 0x0)
    {
	APP_LOG("UPNPApp",LOG_DEBUG,"ARP MAC not resolved...registration failure!!");
	goto on_return;
    }

    /* device registered AND no router change AND auto registration, FALSE trigger, ignore this request */
#ifdef PRODUCT_WeMo_NetCam
    if( ((strlen(g_szHomeId) > 0x0) && (strlen(g_szPluginPrivatekey) > 0x0) && (0x00 == atoi(g_szRestoreState))) \
	&& (strcmp(g_routerMac, gGatewayMAC) == 0) && (strcmp(g_routerSsid, routerssid) == 0) && (!g_isRemoteAccessByApp) && (gRemoteFailureCounter < MAX_REMOTE_FAILURE_COUNT) \
	    && (getRemoteDbgRegFlag() == PLUGIN_SUCCESS))
#else
    if( ((strlen(g_szHomeId) > 0x0) && (strlen(g_szPluginPrivatekey) > 0x0) && (0x00 == atoi(g_szRestoreState))) \
	    && (strcmp(g_routerMac, gGatewayMAC) == 0)  && (!g_isRemoteAccessByApp) && (gRemoteFailureCounter < MAX_REMOTE_FAILURE_COUNT) \
	    && (getRemoteDbgRegFlag() == PLUGIN_SUCCESS))
#endif
    {
	APP_LOG("UPNPApp",LOG_DEBUG,"device already registered... false auto registration request...ignore it!!");
	goto on_return;
    }

    setRemoteDbgRegFlag(PLUGIN_SUCCESS);
    gRemoteFailureCounter = 0;

    count = 0;

    /* allocate registration request structure */
    PluginRegInf = (pluginRegisterInfo *)CALLOC(1, sizeof(pluginRegisterInfo));
    if(PluginRegInf == NULL)
    {
	APP_LOG("PLUGINAPP",LOG_ERR, "PluginRegInf mem allocation FAIL");
         goto on_return;
    }

    /* Fill device and smart device request structure */
    ret = PluginAppFillInfo(szDeviceId, szHomeId, szDeviceName, szKey, szReunionKey, PluginRegInf);
    if(ret != SUCCESS)
    {
	APP_LOG("UPNPApp",LOG_DEBUG,"Plugin fill info failed...");
	goto on_return;
    }

    while(1)
    {
	memset(signature, 0, sizeof(signature));

	/* loop until we find Internet connected */
	while(1)
	{
	    if (getCurrentClientState() == STATE_CONNECTED)
		break;

	    pluginUsleep(1000000);  //1 sec: to defer for upnp thread scheduling
	    /* upnp */
	    if(g_isRemoteAccessByApp)
	    {
		APP_LOG("Remote",LOG_DEBUG, "send signal to App");
		signaltoRemoteUpnpThread();
	    }

	    pluginUsleep(10000000);  //10 sec
	}

	/* create auth header */
	ret = createAuthHeader(cloudSeed, signature, PluginRegInf, szDeviceName);
	if(ret != SUCCESS)
	{
	    APP_LOG("UPNPApp",LOG_DEBUG,"create Auth Header failed...");
	    break;
	}

	/* send and receive remote request to cloud */
	if((ret = sendRecvRemoteRequest(signature, PluginRegInf)) != SUCCESS)
	{
	    APP_LOG("UPNPApp",LOG_DEBUG, "Failed in sendRecvRemoteRequest: %d",ret);

	    /* create a local copy of seed, if any */
	    if( (pgPluginReRegInf != NULL) && (pgPluginReRegInf->seed != NULL && 0x0 != strlen(pgPluginReRegInf->seed)) )
	    {
		APP_LOG("Remote",LOG_DEBUG, "Seed: %s", pgPluginReRegInf->seed);
		memset(cloudSeed, 0, sizeof(cloudSeed));
		strncpy(cloudSeed, pgPluginReRegInf->seed, sizeof(cloudSeed)-1);
	    }


	    /* upnp */
	    if(g_isRemoteAccessByApp)
	    {
		/* break after one retry or no seed case */
		if(count || ( (0x0 == strlen(cloudSeed)) && (PluginRegInf->key == NULL && 0x0 == strlen(PluginRegInf->key)) ))
		{
		    APP_LOG("Remote",LOG_DEBUG, "count reached or no retry");
		    signaltoRemoteUpnpThread();
		}
		else if( (0x0 == strlen(cloudSeed)) && (PluginRegInf->key != NULL && 0x0 != strlen(PluginRegInf->key)))
		{
		    APP_LOG("Remote",LOG_DEBUG, "no seed but have key");
		    /* Proxy */
		    if ( (0x0 != szDeviceName) && (!strcmp(szDeviceName, PLUGIN_REMOTE_REQ)) )
		    {
			APP_LOG("Remote",LOG_DEBUG, "retry once using self key");
			/* reset received key, to retry using plugin private key */
			memset(PluginRegInf->key, 0, sizeof(PluginRegInf->key));
		    }
		    /* App: retry */
		    else
		    {
			APP_LOG("Remote",LOG_DEBUG, "send signal to App");
			if(pgPluginReRegInf != NULL && !strcmp(pgPluginReRegInf->resultCode, CLOUD_AUTH_FAIL_ERR_CODE))  //403
			{
			    /* reset received smart private key */
			    APP_LOG("REMOTEACCESS", LOG_HIDE, "reset received smart private key: %s", PluginRegInf->key);
			    memset(PluginRegInf->key, 0, sizeof(PluginRegInf->key));
			    APP_LOG("REMOTEACCESS", LOG_HIDE, "after reset received smart private key: %s", PluginRegInf->key);
			}
			signaltoRemoteUpnpThread();
		    }
		}
	    }

	    /* create local copy of error code */
	    if(pgPluginReRegInf != NULL)
	    {
		APP_LOG("Remote",LOG_DEBUG, "Error: %s", pgPluginReRegInf->resultCode);
		memset(errCode, 0, sizeof(errCode));
		strncpy(errCode, pgPluginReRegInf->resultCode, sizeof(errCode)-1);
	    }

	    /* free registration response, before retry*/
	    if(pgPluginReRegInf != NULL){free(pgPluginReRegInf); pgPluginReRegInf = NULL;}

	    /* break in case of SDU Auth Expiration error */
	    if(!strcmp(errCode, CLOUD_AUTH_EXPIRE_ERR_CODE))
	    {
		APP_LOG("Remote",LOG_DEBUG, "SDU auth ERR_006 case");
		memset(g_signature, 0, sizeof(g_signature));
		break;	
	    }

	    /* break in case of seed auth fail */
	    if( (0x0 != strlen(cloudSeed)) && (!strcmp(errCode, CLOUD_AUTH_FAIL_ERR_CODE)) )
	    {
		APP_LOG("Remote",LOG_DEBUG, "seed auth ERR_002 case");
	    }

	    /* sleep for retry */
#ifdef WeMo_SMART_SETUP
	    if(!g_isRemoteAccessByApp || count)
#else
		if(!g_isRemoteAccessByApp)
#endif
		{
		    retry_t = getRetryTime(&exp, &iCnt);
		    APP_LOG("Remote",LOG_ERR, "RE-TRYING in <%d> seconds..", retry_t);
		    pluginUsleep(retry_t * 1000000);
		}

	    count = 1;
	}
	else
	{
	    APP_LOG("UPNPApp",LOG_DEBUG, "sendRecvRemoteRequest success: %d",ret);
	    break;
	}
    }

on_return:

    /* save data on flash */
    if( (pgPluginReRegInf != NULL) && (PluginRegInf != NULL) && !(strcmp(PluginRegInf->pluginDevInf.macAddress, g_szWiFiMacAddress)) )
    {
	if(!strcmp(pgPluginReRegInf->statusCode, "S"))
	    writeRemoteDatatoFlash();
#ifdef PRODUCT_WeMo_LEDLight
				APP_LOG("RemoteEnable", LOG_DEBUG, "call pushEvent(INIT_REGISTER)...");
				trigger_remote_event(WORK_EVENT, INIT_REGISTER, "register", "subdevices", NULL, 0);
    g_initRegisterTrigger = 1;
#endif
		APP_LOG("WiFiApp", LOG_DEBUG,"set nat Data"); 
		UDS_SetNatData();
		UDS_SetRemoteEnableStatus();
		UDS_remoteAccessInit();

#ifdef WeMo_SMART_SETUP
	APP_LOG("WiFiApp", LOG_DEBUG,"Valid Reg Response, pvt key len in req: %d, reunion_key_len: %d", 
		strlen(PluginRegInf->smartDevInf.privateKey), strlen(PluginRegInf->smartDevInf.reunionKey));

	/* Send UDID, reunion key to cloud */
	/* NOT REQUIRED: CLOUD the create the mapping from registration request and add an expiry time for reunionKey */
	//sendReunionKeyToCloud(PluginRegInf->smartDevInf.smartUniqueId, PluginRegInf->smartDevInf.privateKey, PluginRegInf->smartDevInf.reunionKey);
	/* registration response to be stored only for smart setup leg */
	if(gSmartSetup)
	{
	    if(!pgSetupRegResp)
	    {
		APP_LOG("WiFiApp", LOG_DEBUG,"allocating reg data for later use, status: %s, desc: %s, resCode: %s", 
			pgPluginReRegInf->statusCode, pgPluginReRegInf->description, pgPluginReRegInf->resultCode);

		pgSetupRegResp = CALLOC(1, sizeof(pluginRegisterInfo));
		if(pgSetupRegResp)
		{
		    memcpy(pgSetupRegResp,pgPluginReRegInf,sizeof(pluginRegisterInfo));

		    /*over-write FAILURE status code as in case of ERR_006, this field is empty */ 
		    if(strcmp(pgPluginReRegInf->statusCode, "S"))
		    {
			APP_LOG("WiFiApp", LOG_DEBUG,"Updating status code: %s to F", pgPluginReRegInf->statusCode);
			strcpy(pgSetupRegResp->statusCode, "F");
		    }

		    strncpy(gszSetupSmartDevUniqueId, PluginRegInf->smartDevInf.smartUniqueId, sizeof(gszSetupSmartDevUniqueId)-1);
		    strncpy(gszSetupReunionKey, PluginRegInf->smartDevInf.reunionKey, sizeof(gszSetupReunionKey)-1);
		    APP_LOG("WiFiApp", LOG_HIDE,"gszSetupSmartDevUniqueId: %s, gszSetupReunionKey: %s", gszSetupSmartDevUniqueId, gszSetupReunionKey);
		    APP_LOG("WiFiApp", LOG_DEBUG,"stored status: %s, desc: %s, resCode: %s", 
			    pgSetupRegResp->statusCode, pgSetupRegResp->description, pgSetupRegResp->resultCode);
		}
		else
		{
		    APP_LOG("WiFiApp", LOG_DEBUG,"Memory allocation failure...rebooting..");
		    system("reboot");
		}
	    }
	    else
	    {
		APP_LOG("WiFiApp", LOG_DEBUG,"saving reg data for later use such as try again request ...");
		memset(pgSetupRegResp,0,sizeof(pluginRegisterInfo));

		memcpy(pgSetupRegResp,pgPluginReRegInf,sizeof(pluginRegisterInfo));

		/*over-write FAILURE status code as in case of ERR_006, this field is empty */ 
		if(strcmp(pgPluginReRegInf->statusCode, "S"))
		{
		    APP_LOG("WiFiApp", LOG_DEBUG,"Updating status code: %s to F", pgPluginReRegInf->statusCode);
		    strcpy(pgSetupRegResp->statusCode, "F");
		}

		strncpy(gszSetupSmartDevUniqueId, PluginRegInf->smartDevInf.smartUniqueId, sizeof(gszSetupSmartDevUniqueId)-1);
		strncpy(gszSetupReunionKey, PluginRegInf->smartDevInf.reunionKey, sizeof(gszSetupReunionKey)-1);
		APP_LOG("WiFiApp", LOG_HIDE,"Updated gszSetupSmartDevUniqueId: %s, gszSetupReunionKey: %s", gszSetupSmartDevUniqueId, gszSetupReunionKey);
	    }
	}
#endif
    }

    /* App: signal to remote access upnp thread */
#ifdef WeMo_SMART_SETUP
    if(g_isRemoteAccessByApp && !gSmartSetup)
	signaltoRemoteUpnpThread();
#else
  if( g_isRemoteAccessByApp )
      signaltoRemoteUpnpThread();
#endif

    /* free res */
    if(premRegInfo){free(premRegInfo); premRegInfo = NULL;}
    if(PluginRegInf){free(PluginRegInf); PluginRegInf = NULL;}
    if(!g_isRemoteAccessByApp)
	if (pgPluginReRegInf) {free(pgPluginReRegInf); pgPluginReRegInf= NULL;}
    memset(g_signature, 0x0, sizeof(g_signature));
#ifdef WeMo_INSTACONNECT
    APP_LOG("WiFiApp", LOG_DEBUG,"reset connectFromApp flag...");
    g_usualSetup = 0x00;    //unset flag
#endif
    g_isRemoteAccessByApp = 0;
    remoteRegthread = -1;
    return NULL;
}

void CheckUpdateRemoteFailureCount(void)
{
    osUtilsGetLock(&gRemoteFailCntLock);
    gRemoteFailureCounter++;
    if(gRemoteFailureCounter >= MAX_REMOTE_FAILURE_COUNT)
    {
	createAutoRegThread();    //try auto re-register
	//gRemoteFailureCounter = 0;
    }
    APP_LOG("REMOTEACCESS",LOG_DEBUG, "gRemoteFailureCounter updated: %d", gRemoteFailureCounter);
    osUtilsReleaseLock(&gRemoteFailCntLock);
}

int getRemoteDbgRegFlag() {
    return gRemoteDbgReg;
}

int setRemoteDbgRegFlag(int value) {
    gRemoteDbgReg = value;
    return gRemoteDbgReg;
}

//This interface should only be called from DebugFramwork, not to 
//be used from any other place to trigger re-register of the device
void reRegisterDevice(void)
{
    gRemoteDbgReg = 1;
    createAutoRegThread();    //try auto re-register
}

/* this function parses registration response xml in case of 200, 400 or 403 response code 
 *  Also, this function returns pointer to registration response structure (success or failure)
 */
void* parseRegisterResp(void *regResp)
{
    pluginRegisterInfo *pPlgnRegInf = NULL;
    char *reRespXml = NULL;
    mxml_node_t *tree;
    mxml_node_t *find_node, *smart_dev_node;
    mxml_node_t *result_nodes, *result_node, *plugin_node, *home_node;

    if(regResp == NULL)
    {
	APP_LOG("REMOTEACCESS", LOG_ERR, "Reg: Out data is null");
	return NULL;
    }
    pPlgnRegInf = (pluginRegisterInfo *)ZALLOC(sizeof(pluginRegisterInfo));
    if(pPlgnRegInf == NULL){
	APP_LOG("REMOTEACCESS", LOG_ERR, "%s:%d pPluginRegInf mem allocation fail\n", __FILE__, __LINE__);
	return NULL;
    }
    reRespXml = (char*)regResp;	

    tree = mxmlLoadString(NULL, reRespXml, MXML_OPAQUE_CALLBACK);
    if (tree)
    {
	APP_LOG("REMOTEACCESS", LOG_HIDE, "XML String Loaded is %s\n", reRespXml);
	find_node = mxmlFindElement(tree, tree, "response", NULL, NULL, MXML_DESCEND);
	if (find_node)
	{
	    find_node = mxmlFindElement(tree, tree, "statusCode", NULL, NULL, MXML_DESCEND);
	    if (find_node && find_node->child) {
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "The statusCode set in the node is %s\n", find_node->child->value.opaque);
		strncpy(pPlgnRegInf->statusCode, (find_node->child->value.opaque), sizeof(pPlgnRegInf->statusCode)-1);
	    }

	    result_nodes = mxmlFindElement(tree, tree, "resultCodes", NULL, NULL, MXML_DESCEND);
	    if (result_nodes){
		result_node = mxmlFindElement(result_nodes, tree, "resultCode", NULL, NULL, MXML_DESCEND);
		if (result_node){
		    find_node = mxmlFindElement(result_node, tree, "code", NULL, NULL, MXML_DESCEND);
		    if (find_node && find_node->child){
			APP_LOG("REMOTEACCESS", LOG_DEBUG, "resultCode = %s\n", find_node->child->value.opaque);
			strncpy(pPlgnRegInf->resultCode, (find_node->child->value.opaque), sizeof(pPlgnRegInf->resultCode)-1);
		    }
		    find_node = mxmlFindElement(result_node, tree, "description", NULL, NULL, MXML_DESCEND);
		    if (find_node && find_node->child ){
			APP_LOG("REMOTEACCESS", LOG_DEBUG, "The description set in the node is %s\n", find_node->child->value.opaque);
			strncpy(pPlgnRegInf->description, (find_node->child->value.opaque), sizeof(pPlgnRegInf->description)-1);
		    }else {
			find_node = mxmlFindElement(tree, tree, "message", NULL, NULL, MXML_DESCEND);
			if (find_node && find_node->child){
			    APP_LOG("REMOTEACCESS", LOG_DEBUG, "The message set in the node is %s\n", find_node->child->value.opaque);
			    strncpy(pPlgnRegInf->message, (find_node->child->value.opaque), sizeof(pPlgnRegInf->message)-1);
			}
		    }
		}
	    }
	    /* cloud will send the seed */
	    find_node = mxmlFindElement(tree, tree, "seed", NULL, NULL, MXML_DESCEND);
	    if (find_node && find_node->child) {
		APP_LOG("REMOTEACCESS", LOG_HIDE, "seed in the node is %s\n", find_node->child->value.opaque);
		strncpy(pPlgnRegInf->seed, (find_node->child->value.opaque), sizeof(pPlgnRegInf->seed)-1);
	    }


	    find_node = mxmlFindElement(tree, tree, "message", NULL, NULL, MXML_DESCEND);
	    if (find_node && find_node->child){
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "The message set in the node is %s\n", find_node->child->value.opaque);
		strncpy(pPlgnRegInf->message, (find_node->child->value.opaque), sizeof(pPlgnRegInf->message)-1);
	    }
	    find_node = mxmlFindElement(tree, tree, "pluginRegister", NULL, NULL, MXML_DESCEND);
	    if (find_node){
		plugin_node = mxmlFindElement(tree, tree, "plugin", NULL, NULL, MXML_DESCEND);
		if (plugin_node){
		    find_node = mxmlFindElement(plugin_node, tree, "pluginId", NULL, NULL, MXML_DESCEND);
		    if (find_node && find_node->child){
			APP_LOG("REMOTEACCESS", LOG_DEBUG, "The pluginId set in the node is %s\n", find_node->child->value.opaque);
			pPlgnRegInf->pluginDevInf.pluginId = atol(find_node->child->value.opaque);
		    }
		    find_node = mxmlFindElement(plugin_node, tree, "macAddress", NULL, NULL, MXML_DESCEND);
		    if (find_node && find_node->child){
			APP_LOG("REMOTEACCESS", LOG_DEBUG, "The macaddress set in the node is %s\n", find_node->child->value.opaque);
			strncpy(pPlgnRegInf->pluginDevInf.macAddress, (find_node->child->value.opaque), sizeof(pPlgnRegInf->pluginDevInf.macAddress)-1);
		    }
		    find_node = mxmlFindElement(plugin_node, tree, "serialNumber", NULL, NULL, MXML_DESCEND);
		    if (find_node && find_node->child){
			APP_LOG("REMOTEACCESS", LOG_DEBUG, "The serial number set in the node is %s\n", find_node->child->value.opaque);
			strncpy(pPlgnRegInf->pluginDevInf.serialNumber, (find_node->child->value.opaque), sizeof(pPlgnRegInf->pluginDevInf.serialNumber)-1);
		    }
		    find_node = mxmlFindElement(plugin_node, tree, "uniqueId", NULL, NULL, MXML_DESCEND);
		    if (find_node && find_node->child){
			APP_LOG("REMOTEACCESS", LOG_HIDE, "The plugin uniqueId set in the node is %s\n", find_node->child->value.opaque);
			strncpy(pPlgnRegInf->pluginDevInf.pluginUniqueId, (find_node->child->value.opaque), sizeof(pPlgnRegInf->pluginDevInf.pluginUniqueId)-1);
		    }
		    find_node = mxmlFindElement(plugin_node, tree, "modelCode", NULL, NULL, MXML_DESCEND);
		    if (find_node && find_node->child){
			APP_LOG("REMOTEACCESS", LOG_DEBUG, "The modelCode set in the node is %s\n", find_node->child->value.opaque);
			strncpy(pPlgnRegInf->pluginDevInf.modelCode, (find_node->child->value.opaque), sizeof(pPlgnRegInf->pluginDevInf.modelCode)-1);
		    }
		    find_node = mxmlFindElement(plugin_node, tree, "privateKey", NULL, NULL, MXML_DESCEND);
		    if (find_node && find_node->child){
			APP_LOG("REMOTEACCESS", LOG_HIDE, "The privateKey set in the node is %s\n", find_node->child->value.opaque);
			strncpy(pPlgnRegInf->pluginDevInf.privateKey, (find_node->child->value.opaque), sizeof(pPlgnRegInf->pluginDevInf.privateKey)-1);
		    }
		    home_node = mxmlFindElement(plugin_node, tree, "home", NULL, NULL, MXML_DESCEND);
		    if (home_node){
			find_node = mxmlFindElement(home_node, tree, "homeId", NULL, NULL, MXML_DESCEND);
			if (find_node && find_node->child){
			    APP_LOG("REMOTEACCESS", LOG_HIDE, "The homeId set in the node is %s\n", find_node->child->value.opaque);
			    pPlgnRegInf->pluginDevInf.homeId = atol(find_node->child->value.opaque);
			    if( pPlgnRegInf->pluginDevInf.homeId == 0)
			    {
				APP_LOG("REMOTEACCESS", LOG_ALERT, "ERROR!! HomeID is zero: %s\n", find_node->child->value.opaque);
				goto on_return;
			    }
			}
			find_node = mxmlFindElement(home_node, tree, "description", NULL, NULL, MXML_DESCEND);
			if (find_node && find_node->child){
			    APP_LOG("REMOTEACCESS", LOG_DEBUG, "The description set in the node is %s\n", find_node->child->value.opaque);
			    strncpy(pPlgnRegInf->pluginDevInf.description, (find_node->child->value.opaque), sizeof(pPlgnRegInf->pluginDevInf.description)-1);
			}
		    }
		    find_node = mxmlFindElement(plugin_node, tree, "status", NULL, NULL, MXML_DESCEND);
		    if (find_node && find_node->child){
			APP_LOG("REMOTEACCESS", LOG_DEBUG, "The status set in the node is %s\n", find_node->child->value.opaque);
			strncpy(pPlgnRegInf->pluginDevInf.status, (find_node->child->value.opaque), sizeof(pPlgnRegInf->pluginDevInf.status)-1);
		    }
		}
		smart_dev_node = mxmlFindElement(tree, tree, "smartDevice", NULL, NULL, MXML_DESCEND);
		if (smart_dev_node){
		    find_node = mxmlFindElement(smart_dev_node, tree, "smartDeviceId", NULL, NULL, MXML_DESCEND);
		    if (find_node && find_node->child){
			APP_LOG("REMOTEACCESS", LOG_HIDE, "The smart device id set in the node is %s\n", find_node->child->value.opaque);
			pPlgnRegInf->smartDevInf.smartDeviceId = atol(find_node->child->value.opaque);
		    }
		    find_node = mxmlFindElement(smart_dev_node, tree, "description", NULL, NULL, MXML_DESCEND);
		    if (find_node && find_node->child){
			APP_LOG("REMOTEACCESS", LOG_DEBUG, "The smart dev description set in the node is %s\n", find_node->child->value.opaque);
			strncpy(pPlgnRegInf->smartDevInf.smartDeviceDescription, (find_node->child->value.opaque), sizeof(pPlgnRegInf->smartDevInf.smartDeviceDescription)-1);
		    }
		    find_node = mxmlFindElement(smart_dev_node, tree, "uniqueId", NULL, NULL, MXML_DESCEND);
		    if (find_node && find_node->child){
			APP_LOG("REMOTEACCESS", LOG_HIDE, "The uniqueId set in the node is %s\n", find_node->child->value.opaque);
			strncpy(pPlgnRegInf->smartDevInf.smartUniqueId, (find_node->child->value.opaque), sizeof(pPlgnRegInf->smartDevInf.smartUniqueId)-1);
		    }
		    find_node = mxmlFindElement(smart_dev_node, tree, "privateKey", NULL, NULL, MXML_DESCEND);
		    if (find_node && find_node->child){
			APP_LOG("REMOTEACCESS", LOG_HIDE, "The private key set in the node is %s\n", find_node->child->value.opaque);
			strncpy(pPlgnRegInf->smartDevInf.privateKey, (find_node->child->value.opaque), sizeof(pPlgnRegInf->smartDevInf.privateKey)-1);
		    }
#ifdef WeMo_SMART_SETUP
		    find_node = mxmlFindElement(smart_dev_node, tree, "reunionKey", NULL, NULL, MXML_DESCEND);
		    if (find_node && find_node->child){
			APP_LOG("REMOTEACCESS", LOG_HIDE, "The reunioun key set in the node is %s\n", find_node->child->value.opaque);
			strncpy(pPlgnRegInf->smartDevInf.reunionKey, (find_node->child->value.opaque), sizeof(pPlgnRegInf->smartDevInf.reunionKey)-1);
		    }
#endif
		}
	    }

	}
	else
	{  //403 forbidden case
	    find_node = mxmlFindElement(tree, tree, "Error", NULL, NULL, MXML_DESCEND);
	    if(find_node){
		find_node = mxmlFindElement(tree, tree, "code", NULL, NULL, MXML_DESCEND);
		if (find_node && find_node->child) {
		    APP_LOG("REMOTEACCESS", LOG_DEBUG, "The Error Code set in the node is %s\n", find_node->child->value.opaque);
		    strncpy(pPlgnRegInf->resultCode, (find_node->child->value.opaque), sizeof(pPlgnRegInf->resultCode)-1);
		}
	    }
	    else
	    {
		goto on_return;
	    }
	}
    }
    else
    {
	goto on_return;
    }
    mxmlDelete(tree);
    return pPlgnRegInf;
on_return:
    mxmlDelete(tree);
    free(pPlgnRegInf);
    pPlgnRegInf = NULL;
    return pPlgnRegInf;
}

int clearMemPartitions() 
{
    int ret=0;
    ret = system("rm /tmp/Belkin_settings/icon.jpg");

    return ret;
}

int resetFNGlobalsToDefaults(int type) 
{
    if (type != 0x01) {
	return SUCCESS;
    }
    GetDeviceType();
    if(g_eDeviceType == DEVICE_SENSOR) 
    {
	strncpy(g_szFriendlyName, DEFAULT_SENSOR_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
    }
#ifdef PRODUCT_WeMo_Insight 
    else if(g_eDeviceTypeTemp == DEVICE_INSIGHT) 
    {
	strncpy(g_szFriendlyName, DEFAULT_INSIGHT_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
    } 
#endif
#ifdef PRODUCT_WeMo_Light 
    else if(g_eDeviceTypeTemp == DEVICE_LIGHTSWITCH) 
    {
	strncpy(g_szFriendlyName, DEFAULT_LIGHTSWITCH_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
    } 
#endif
#ifdef PRODUCT_WeMo_Jarden
    else if (DEVICE_CROCKPOT == g_eDeviceType)
    {
	strncpy(g_szFriendlyName, DEFAULT_CROCKPOT_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
    }
#endif /*#ifdef PRODUCT_WeMo_CrockPot*/
#ifdef PRODUCT_WeMo_LEDLight
    else if( g_eDeviceTypeTemp == DEVICE_BRIDGE )
    {
        strncpy(g_szFriendlyName, DEFAULT_BRIDGE_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
    }
#endif /* #ifdef PRODUCT_WeMo_LEDLight */
    else 
    {
	strncpy(g_szFriendlyName, DEFAULT_SOCKET_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
    }
    return SUCCESS; 
}

int resetToDefaults() 
{
    GetDeviceType();

    if(g_eDeviceType == DEVICE_SENSOR) 
    {
	APP_LOG("PLUGINAPP", LOG_DEBUG, "############### Restore to sensor profile");
	strncpy(g_szFriendlyName, DEFAULT_SENSOR_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
	system("cp -f /etc/sensor.jpg /tmp/Belkin_settings/icon.jpg");
	SaveDeviceConfig("FriendlyName", DEFAULT_SENSOR_FRIENDLY_NAME);
	UnSetBelkinParameter (ICON_VERSION_KEY);
	gWebIconVersion=1;
    }
#ifdef PRODUCT_WeMo_Insight 
    else if(g_eDeviceTypeTemp == DEVICE_INSIGHT) 
    {
	APP_LOG("PLUGINAPP", LOG_DEBUG, "############### Restore to Insight profile");
	strncpy(g_szFriendlyName, DEFAULT_INSIGHT_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
	system("cp -f /etc/icon.jpg /tmp/Belkin_settings/icon.jpg");
	SaveDeviceConfig("FriendlyName", DEFAULT_INSIGHT_FRIENDLY_NAME);
	UnSetBelkinParameter (ICON_VERSION_KEY);
	gWebIconVersion=0;
    } 
#endif
#ifdef PRODUCT_WeMo_Light 
    else if(g_eDeviceTypeTemp == DEVICE_LIGHTSWITCH) 
    {
	APP_LOG("PLUGINAPP", LOG_DEBUG, "############### Restore to Light Switch profile");
	strncpy(g_szFriendlyName, DEFAULT_LIGHTSWITCH_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
	system("cp -f /etc/icon.jpg /tmp/Belkin_settings/icon.jpg");
	SaveDeviceConfig("FriendlyName", DEFAULT_LIGHTSWITCH_FRIENDLY_NAME);
	UnSetBelkinParameter (ICON_VERSION_KEY);
	gWebIconVersion=0;
    } 
#endif
#ifdef PRODUCT_WeMo_Jarden
    else if (DEVICE_CROCKPOT == g_eDeviceType)
    {
	APP_LOG("PLUGINAPP", LOG_DEBUG, "############### Restore to crockpot profile");
	strncpy(g_szFriendlyName, DEFAULT_CROCKPOT_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
	system("cp -f /etc/sensor.jpg /tmp/Belkin_settings/icon.jpg");
	SaveDeviceConfig("FriendlyName", DEFAULT_CROCKPOT_FRIENDLY_NAME);
    }
    else if (DEVICE_SBIRON == g_eDeviceType)
    {
    APP_LOG("PLUGINAPP", LOG_DEBUG, "############### Restore to sunbeam iron profile");
    strncpy(g_szFriendlyName, DEFAULT_SBIRON_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
    system("cp -f /etc/icon.jpg /tmp/Belkin_settings/icon.jpg");
    SaveDeviceConfig("FriendlyName", DEFAULT_SBIRON_FRIENDLY_NAME);
    UnSetBelkinParameter (ICON_VERSION_KEY);
    gWebIconVersion=0;
    }
    else if (DEVICE_MRCOFFEE == g_eDeviceType)
    {
    APP_LOG("PLUGINAPP", LOG_DEBUG, "############### Restore to Mr. Coffee profile");
    strncpy(g_szFriendlyName, DEFAULT_MRCOFFEE_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
    system("cp -f /etc/icon.jpg /tmp/Belkin_settings/icon.jpg");
    SaveDeviceConfig("FriendlyName", DEFAULT_MRCOFFEE_FRIENDLY_NAME);
    UnSetBelkinParameter (ICON_VERSION_KEY);
    gWebIconVersion=0;
    }
    else if (DEVICE_PETFEEDER == g_eDeviceType)
    {
    APP_LOG("PLUGINAPP", LOG_DEBUG, "############### Restore to Pet Feeder profile");
    strncpy(g_szFriendlyName, DEFAULT_PETFEEDER_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
    system("cp -f /etc/icon.jpg /tmp/Belkin_settings/icon.jpg");
    SaveDeviceConfig("FriendlyName", DEFAULT_PETFEEDER_FRIENDLY_NAME);
    UnSetBelkinParameter (ICON_VERSION_KEY);
    gWebIconVersion=0;
    }
    else if (DEVICE_SMART == g_eDeviceType)
    {
    APP_LOG("PLUGINAPP", LOG_DEBUG, "############### Restore to Smart profile");
    strncpy(g_szFriendlyName, DEFAULT_SMART_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
    system("cp -f /etc/icon.jpg /tmp/Belkin_settings/icon.jpg");
    SaveDeviceConfig("FriendlyName", DEFAULT_SMART_FRIENDLY_NAME);
    UnSetBelkinParameter (ICON_VERSION_KEY);
    gWebIconVersion=0;
    }
#endif /*#ifdef PRODUCT_WeMo_CrockPot*/
#ifdef PRODUCT_WeMo_LEDLight
  else if( g_eDeviceTypeTemp == DEVICE_BRIDGE )
  {
    APP_LOG("PLUGINAPP", LOG_DEBUG, "############### Restore to LEDLight profile");
    strncpy(g_szFriendlyName, DEFAULT_BRIDGE_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
    system("cp -f /etc/icon.jpg /tmp/Belkin_settings/icon.jpg");
    SaveDeviceConfig("FriendlyName", DEFAULT_BRIDGE_FRIENDLY_NAME);
    UnSetBelkinParameter(ICON_VERSION_KEY);
    gWebIconVersion=0;
  }
 #endif /* #ifdef PRODUCT_WeMo_LEDLight */
    else 
    {
	APP_LOG("PLUGINAPP", LOG_DEBUG, "############### Restore to socket profile");
	strncpy(g_szFriendlyName, DEFAULT_SOCKET_FRIENDLY_NAME, sizeof(g_szFriendlyName)-1);
	system("cp -f /etc/icon.jpg /tmp/Belkin_settings");
	SaveDeviceConfig("FriendlyName", DEFAULT_SOCKET_FRIENDLY_NAME);
	UnSetBelkinParameter (ICON_VERSION_KEY);
	gWebIconVersion=0;
    }

    return SUCCESS; 
}


