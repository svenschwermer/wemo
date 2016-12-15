
/***************************************************************************
*
*
* instaConnectUPnPHandler.c
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
#ifdef WeMo_INSTACONNECT

#include "global.h"
#include "controlledevice.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "logger.h"
#include "wifiHndlr.h"
#include "osUtils.h"
#include "utils.h"
#include "remoteAccess.h"
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */
#include "thready_utils.h"

#define	MAX_INSTA_CLOSE_WAITING_TIME	10*60 //10mins
#define SITESURV_BSSID_LEN	12

pthread_t closeInstaWaiting_thread = -1;
pthread_attr_t closeInsta_attr;
int g_instaClosed = 0;
extern char gBridgeNodeList[SIZE_768B];
char gRouterSSID[SSID_LEN];

extern int g_connectInstaStatus;
extern PMY_SITE_SURVEY pAvlAPList;
extern int g_cntApListNumber;
extern pthread_mutex_t gSiteSurveyStateLock;
extern int gAppCalledCloseAp; 
extern int gBootReconnect; 
extern char gUserKey[PASSWORD_MAX_LEN];
extern int g_usualSetup;
extern WIFI_PAIR_PARAMS gWiFiParams;
extern char g_szWiFiMacAddress[];
extern pthread_mutex_t g_remoteAccess_mutex;
extern pthread_cond_t g_remoteAccess_cond;
extern pluginRegisterInfo *pgPluginReRegInf;
extern int g_isRemoteAccessByApp;

char* changeBssidToUpper(char *bssid)
{
    char mac[MAX_MAC_LEN];
    int i = 0;
    char *newbssid=NULL;
    //char *newbssid=NULL, *p=NULL;

    memset(mac, 0, sizeof(mac));

    char *pBssid = utilsRemDelimitStr(bssid,":");
    strncpy(mac, pBssid, sizeof(mac)-1);
    if(pBssid)free(pBssid);

    //p = newbssid= strdup(mac);
    //while(*p++ = toupper(*p));

    newbssid= (char*)CALLOC(1, (strlen(mac) + 1));
    if(!newbssid)
    {
	APP_LOG("UpnpDevice", LOG_ERR,"CALLOC Failed....");
	resetSystem();
    }
    strncpy(newbssid, mac, strlen(mac));   // <-----<<< WRONG
    for(i = 0; newbssid[i]; i++){
	newbssid[i] = toupper(newbssid[i]);
    }

    newbssid[SITESURV_BSSID_LEN] = '\0';

    //APP_LOG("UPNP", LOG_DEBUG,"updated bssid: %s", newbssid);
    return newbssid;
}

int getSerialBssid(char *ssid, char *serial, char *bssid)
{
    long int lchar = 0, ret = SUCCESS;
    char lastchar[SIZE_2B];
    char *p = NULL;
    memset(lastchar, 0 , sizeof(lastchar));

    /* serial */ 
    p = strchr(ssid,'-');
    strncpy(serial, ssid, (p-ssid)); 

    /* bssid */
    p++;
    strncpy(bssid, p, MAX_MAC_LEN-1);

    /* compare this bssid with its own bssid, to skip this entry*/
    if(!strcmp(bssid, g_szWiFiMacAddress))
    { 
	APP_LOG("UPNP", LOG_DEBUG,"bssid match case");
        return FAILURE;
    }

    /* update bssid */
    lastchar[0] = bssid[strlen(bssid)-1];
    if(lastchar[0] == 'E' || lastchar[0] == 'F')
    {
	APP_LOG("UPNP", LOG_DEBUG,"bssid error case");
        return FAILURE;
    }

    lchar = strtol(lastchar,NULL,16);
    lchar = lchar + 2;
    if(lchar >= 10)
        bssid[strlen(bssid)-1] = 'A' + (lchar-10);
    else
        bssid[strlen(bssid)-1] = (char)(((int)'0')+lchar);

    APP_LOG("UPNP", LOG_DEBUG,"serial: %s and bssid: %s", serial, bssid);

    return ret;
}

/**
 * selectNodeFromList:
 *  Node selection algorithm to find best node in network
 *
 *  Based on: Rssi 
 * 
 * *****************************************************************************************************************/
int selectNodeFromList(char *pSSID, int nodeCount, char *pNodeList, char *pBestNode, int *bestChannel)
{
    int bestRssi = 0x00, count = 0, i=0, nCount = 0, ret = SUCCESS;
    int bestNodeChannel = -1, retryCount = BN_SITESURV_RETRY_CNT;
    char bestnode[SSID_LEN];
    char node[SSID_LEN];
    PMY_SITE_SURVEY pAvlAPList = NULL;
    char *pNode = NULL;
    char *plist = NULL;
    char *updBssid = NULL;
    char bssid[MAX_MAC_LEN];
    char serial[SIZE_16B];
    char szBuff[MAX_APSSID_LEN];
    int scanFailCnt=0;
    int retVal = SUCCESS, isRouterNode = 0x00, brlistLen = 0;
	 char *strtok_r_temp;

    pAvlAPList = (PMY_SITE_SURVEY) CALLOC(1, sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE);
    if(!pAvlAPList)
    {
	APP_LOG("UpnpDevice", LOG_ERR,"Malloc Failed....");
	resetSystem();
    }

    brlistLen = strlen(pNodeList)+1; 
    plist = (char*)ZALLOC(brlistLen); 
    if(!plist)
    {
	APP_LOG("UpnpDevice", LOG_ERR,"Malloc Failed....");
	free(pAvlAPList);
	//resetSystem();
        return FAILURE;
    }

    memset(bestnode, 0, sizeof(bestnode));

    while(retryCount)
    {
	EnableSiteSurvey();
	pluginUsleep(50000);	//50 ms
	getCompleteAPList(pAvlAPList, &count);

	APP_LOG("UPNP", LOG_DEBUG,"Avl network list cnt: %d", count);

	if(count == 0)
	{
	    scanFailCnt++;
	    APP_LOG("UpnpDevice", LOG_CRIT,"No networks found in SiteScan: %d...", scanFailCnt);
	}
	else
		scanFailCnt=0;

	if(scanFailCnt == BN_SITESURV_RETRY_CNT)
	{
	    APP_LOG("UpnpDevice", LOG_ALERT,"No networks found in SiteScan for consecutive %d times..., rebooting!!!", scanFailCnt);
	    if (gpluginRemAccessEnable && UDS_pluginNatInitialized()) {
		invokeNatDestroy();
	    }
	    system("reboot");
	}

	/* parse bridge list */
	memset(plist, 0, brlistLen);
	strncpy(plist, pNodeList, brlistLen-1);   // <-----<<< WRONG

	pNode = strtok_r(plist, "|",&strtok_r_temp);

	while(pNode != NULL)
	{
	    memset(node, 0, sizeof(node));
	    memset(szBuff, 0, sizeof(szBuff));
	    memset(bssid, 0, sizeof(bssid));
	    memset(serial, 0, sizeof(serial));
	    ret = SUCCESS;

	    if(!strchr(pNode, '-'))
	    {
		APP_LOG("UPNP", LOG_DEBUG,"Router Node: %s", pNode);
		isRouterNode = 0x01;
		strncpy(bssid, pNode, sizeof(bssid)-1);
	    }
	    else
	    {
		APP_LOG("UPNP", LOG_DEBUG,"Bridge Node: %s", pNode);
		isRouterNode = 0x00;
		ret = getSerialBssid(pNode, serial, bssid);
	    }

	    if(ret == SUCCESS)
	    {
		for (i=0;i<count;i++) 
		{
		    updBssid = changeBssidToUpper( (char*)(pAvlAPList[i].bssid) );
		    //APP_LOG("UPNP", LOG_DEBUG,"updbssid: %s and len: %d, bssid: %s and len: %d", updBssid, strlen(updBssid), bssid, strlen(bssid));
		    if (!strcmp(updBssid, bssid)) 
		    {
			    APP_LOG("UPNPDevice", LOG_DEBUG,
					    "pAvlAPList[%d].channel: %s,pAvlAPList[%d].bssid: %s,(pAvlAPList[%d].signal:%d)", 
					    i, pAvlAPList[i].channel, i, pAvlAPList[i].bssid, i, atoi(pAvlAPList[i].signal));

			    /* if best rssi */
			    if(atoi(pAvlAPList[i].signal) > bestRssi)
			    {
				bestRssi = atoi(pAvlAPList[i].signal);
				bestNodeChannel = atoi((char*)(pAvlAPList[i].channel));
				if(isRouterNode)
				    strncpy(bestnode, pSSID, sizeof(bestnode)-1);
				else
				{
				    GetBridgeTrailSerial(serial, szBuff);
				    snprintf(node, sizeof(node), "WeMo.Bridge.%s", szBuff);
				    strncpy(bestnode, node, sizeof(bestnode)-1);
				}
				APP_LOG("UPNP", LOG_DEBUG,"Bestnode: %s with Rssi: %d", bestnode, bestRssi);
			    }

			    nCount++;   //node count
			    //break;
		    }
		    if(updBssid){free(updBssid); updBssid=NULL;}
		}
	    }
	    else
	    {
		APP_LOG("UPNP", LOG_DEBUG,"skipping this entry...");
	    }

	    pNode = strtok_r(NULL, "|",&strtok_r_temp);
	}
	
	/* break if total node count reached */
	if(nCount == nodeCount)
	    break;

	retryCount--;
    }

    /* copy the final best node and channel values */
    if(strlen(bestnode))
    {
	memset(pBestNode, 0, SSID_LEN);
	strncpy(pBestNode, bestnode, sizeof(bestnode)-1);
	(*bestChannel) = bestNodeChannel; 
	APP_LOG("UPNP", LOG_DEBUG,"Final Bestnode: %s and Channel: %d", pBestNode, (*bestChannel));
    }

    if(pAvlAPList!= NULL)
    {
	free (pAvlAPList);
	pAvlAPList = NULL;
    }

    if(plist != NULL)
    {
	free (plist);
	plist = NULL;
    }

    return retVal;
}

/**
 * getBestNode:
 *  method to get best node 
 * 
 * *****************************************************************************************************************/
int getBestNode(char *pSsid, char *pBrList, char *pNode, int *bChannel)
{
    int brCount = 0x00;
    char br_count[SIZE_4B];
    char brList[SIZE_768B];
    char *pblist = NULL;
    int ret = SUCCESS;

    if(!pBrList || !strlen(pBrList))
    {
	APP_LOG("UPnPDevice",LOG_DEBUG, "**********Bridge list is empty");
	return FAILURE;
    }
    APP_LOG("UPNPDevice",LOG_DEBUG,"Received bridge list: %s", pBrList);

    memset(brList, 0, sizeof(brList));
    memset(br_count, 0, sizeof(br_count));

    /* bridge count */
    pblist = strchr(pBrList, '|');
    strncpy(br_count, pBrList, (pblist - pBrList));
    brCount = atoi(br_count);
    APP_LOG("UPnPDevice",LOG_DEBUG, "Bridge count: %d", brCount);

    /* bridge list */
    pblist++;
    pBrList = pblist; 
    strncpy(brList, pBrList, sizeof(brList)-1);
    APP_LOG("UPnPDevice",LOG_DEBUG, "Bridge list: %s", brList);

    ret = selectNodeFromList(pSsid, brCount, brList, pNode, bChannel);

    return ret;
}

/**
 * closeInsta:
 *  Helper function to close the Insta
 * 
 * *****************************************************************************************************************/
static void closeInsta()
{
    char szBuff[MAX_BUF_LEN];

    memset(szBuff, 0x0, MAX_BUF_LEN);
    snprintf(szBuff, MAX_BUF_LEN, "brctl delif %s %s", INTERFACE_BR, INTERFACE_INSTA);
    APP_LOG("UPNP: Insta", LOG_DEBUG, ": %s", szBuff);
    /* system call to delete the bridging interface and make interface Down */
    system(szBuff);

    memset(szBuff, 0x0, MAX_BUF_LEN);
    snprintf(szBuff, MAX_BUF_LEN, "ifconfig %s down", INTERFACE_INSTA);
    APP_LOG("UPNP: Insta", LOG_DEBUG, ": %s", szBuff);
    /* system call to delete the bridging interface and make interface Down */
    system(szBuff);

    g_instaClosed = 1;
}

void *closeInstaWaitingThread(void *args)
{
    int k = 0;
    tu_set_my_thread_name( __FUNCTION__ );

    while (k++ < MAX_INSTA_CLOSE_WAITING_TIME) {
	if(g_instaClosed)
	    break;
	pluginUsleep(1000000);
    }
    if(!g_instaClosed)
        closeInsta();

    closeInstaWaiting_thread = -1;
    return NULL;
}

/**
 * OpenInstaAP:
 * 	Callback to Open the Insta AP interface
 * 
 * 
 * *****************************************************************************************************************/
int OpenInstaAP(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
    char szBuff[MAX_BUF_LEN];

    memset(szBuff, 0x0, MAX_BUF_LEN);

    if(g_connectInstaStatus != APCLI_CONFIGURED)
    {
	APP_LOG("UPNP: Insta", LOG_DEBUG, "Device is not configured. Rejecting the call");
	return UPNP_E_SUCCESS;
    }

    snprintf(szBuff, MAX_BUF_LEN, "ifconfig %s up", INTERFACE_INSTA);
    APP_LOG("UPNP: Insta", LOG_DEBUG, ": %s", szBuff);
    /* system call to make interface UP */
    system(szBuff);

    memset(szBuff, 0x0, MAX_BUF_LEN);

    snprintf(szBuff, MAX_BUF_LEN, "brctl addif %s %s", INTERFACE_BR, INTERFACE_INSTA);
    APP_LOG("UPNP: Insta", LOG_DEBUG, ": %s", szBuff);
    /* system call to bridge the interface */
    system(szBuff);

    g_instaClosed = 0;
    pActionRequest->ActionResult = NULL;
    pActionRequest->ErrCode = 0x00;

    UpnpAddToActionResponse(&pActionRequest->ActionResult, "OpenInstaAP", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], "OpenInsta", "success");

    if(closeInstaWaiting_thread == -1) {
	pthread_attr_init(&closeInsta_attr);
	pthread_attr_setdetachstate(&closeInsta_attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&closeInstaWaiting_thread, &closeInsta_attr, closeInstaWaitingThread, NULL);
	APP_LOG("UPNP: Insta", LOG_DEBUG, "Insta closing in %d seconds .......", MAX_INSTA_CLOSE_WAITING_TIME);
    } else {
	APP_LOG("UPNP: Insta", LOG_DEBUG, "Close Insta thread is already running");
    }

    return UPNP_E_SUCCESS;
}

/**
 * CloseInstaAP:
 * 	Callback to Close the Insta AP interface
 * 
 * 
 * *****************************************************************************************************************/
int CloseInstaAP(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
    if(g_connectInstaStatus != APCLI_CONFIGURED) {
	APP_LOG("UPNP: Insta", LOG_DEBUG, "Device is not configured. Rejecting the call");
	return UPNP_E_SUCCESS;
    }

    closeInsta();
    pActionRequest->ActionResult = NULL;
    pActionRequest->ErrCode = 0x00;

    UpnpAddToActionResponse(&pActionRequest->ActionResult, "CloseInstaAP", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], "CloseInsta", "success");

    return UPNP_E_SUCCESS;
}

/**
 * GetConfigureState:
 * 	Callback to return the state of the WeMo
 * 
 *
 * Check the list of the configured ClientSSID. If this list is empty
 * then the WeMo is un-configured, otherwise it is configured
 *
 * Return Value: un-configured = 0, configured = 1
 * 
 * *****************************************************************************************************************/
int GetConfigureState(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
    char szBuff[SIZE_16B];

    memset(szBuff, 0x0, SIZE_16B);

    APP_LOG("UPNP: Insta", LOG_DEBUG, "g_connectInstaStatus: %d", g_connectInstaStatus);

    snprintf(szBuff, SIZE_16B, "%d", g_connectInstaStatus);
    APP_LOG("UPNP: Insta", LOG_DEBUG, ": %s", szBuff);

    pActionRequest->ActionResult = NULL;
    pActionRequest->ErrCode = 0x00;

    UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetConfigureState", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], "ConfigureState", szBuff);

    return UPNP_E_SUCCESS;
}

/**
 * InstaConnectHomeNetwork:
 * 	Callback to connect to best node in home network 
 * 
 *
 * Use node selection algo to select best node
 *
 *
 * Return Value: success = 0, failure = -1
 * 
 * *****************************************************************************************************************/
int InstaConnectHomeNetwork(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	int channel;    int rect = 0x00, ret = SUCCESS;
	char bestNode[SSID_LEN];
	char* paramValue = NULL;
	char* password=NULL;

	APP_LOG("UPNPDevice", LOG_CRIT,"%s", __FUNCTION__); 
	if(isSetupRequested())
	{
		APP_LOG("UPNPDevice", LOG_ERR, "#### Setup request already executed ######");
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x01;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "InstaConnectHomeNetwork", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], "PairingStatus", "unsuccess");
		return FAILURE;
	}

	gAppCalledCloseAp=1;
	g_connectInstaStatus = APCLI_CONFIGURING;	//connectToInstaApTask do nothing
	g_usualSetup = 0x00;

	setSetupRequested(1);

	//UpdateUPnPNetworkMode(UPNP_LOCAL_MODE);

	paramValue = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "channel");
	channel = atoi(paramValue);
	
	char* ssid 		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "ssid");
	char* auth 		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "auth");
	char* encrypt		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "encrypt");
	char* brlist		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "brlist");

	if(strcmp(auth,"OPEN"))
		password = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "password");
	else
		password = "NOTHING";

	APP_LOG("UPNPDevice",LOG_HIDE,"insta connect network: %s, channel: %d, auth:%s, encrypt:%s, password:%s brlist: %s", 
			ssid, channel, auth, encrypt, password, brlist);

	if (0x00 == brlist || 0x00 == strlen(brlist))
	{
		APP_LOG("UPNPDevice", LOG_ERR, "received bridge list is (null)");
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x01;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "InstaConnectHomeNetwork", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], "PairingStatus", "unsuccess");
		return FAILURE;
	}

	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "InstaConnectHomeNetwork", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE],"PairingStatus", "Connecting");

	/* Save the password in a global variable - to be used later by WifiConn thread */
	memset(gUserKey, 0, sizeof(gUserKey));
	memcpy(gUserKey, password, sizeof(gUserKey));

	/* Save the router ssid in a global variable - to be used later in wifi save data */
	memset(gRouterSSID, 0, sizeof(gRouterSSID));
	memcpy(gRouterSSID, ssid, sizeof(gRouterSSID));

	/* save bridge node list */
	memset(gBridgeNodeList, 0, sizeof(gBridgeNodeList));
	strncpy(gBridgeNodeList, brlist, sizeof(gBridgeNodeList)-1);

	/* get best node, intialize best node with router ssid */
	memset(bestNode, 0, sizeof(bestNode));
	strncpy(bestNode, ssid, sizeof(bestNode)-1);
	ret = getBestNode(ssid, brlist, bestNode, &channel);

	APP_LOG("UPNPDevice",LOG_HIDE,"Attempting to connect network: %s, channel: %d, auth:%s, encrypt:%s, password:%s", 
			bestNode, channel, auth, encrypt, password);

	if(CHAN_HIDDEN_NW == channel) 
	{
		PWIFI_PAIR_PARAMS pHiddenNwParams=NULL;

		APP_LOG("UPNPDevice",LOG_DEBUG,"Connect request for hidden network: %s......", bestNode);
		pHiddenNwParams = ZALLOC(sizeof(WIFI_PAIR_PARAMS));

		strncpy(pHiddenNwParams->SSID, bestNode, sizeof(pHiddenNwParams->SSID)-1);
		strncpy(pHiddenNwParams->AuthMode, auth, sizeof(pHiddenNwParams->AuthMode)-1);
		strncpy(pHiddenNwParams->EncrypType, encrypt, sizeof(pHiddenNwParams->EncrypType)-1);
		strncpy(pHiddenNwParams->Key, password, sizeof(pHiddenNwParams->Key)-1);
		pHiddenNwParams->channel = CHAN_HIDDEN_NW;

		rect = threadConnectHiddenNetwork(pHiddenNwParams);
	}
	else
	{
		APP_LOG("UPNPDevice",LOG_DEBUG,"connect to selected network: %s", bestNode);
		rect = threadConnectHomeNetwork(channel, bestNode, auth, encrypt, password);
	}

	if(strcmp(auth,"OPEN"))
		FreeXmlSource(password);
	FreeXmlSource(paramValue);
	FreeXmlSource(ssid);
	FreeXmlSource(auth);
	FreeXmlSource(encrypt);
	FreeXmlSource(brlist);

	return rect;

}

/**
 *
 * UpdateBridgeList: Callback to update bridge list in memory and
 *			flash
 *
 *
 *
 *
 *
 *********************************************************************/
int UpdateBridgeList(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{

	char* szBrList = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "BridgeList");
	if (szBrList && strlen(szBrList))
	{
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		APP_LOG("UPNP: Device", LOG_DEBUG, "New bridge list: %s", szBrList);  

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "UpdateBridgeList", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], 
				"BridgeList", "success");

		/* update bridge node list */
		memset(gBridgeNodeList, 0, sizeof(gBridgeNodeList));
		strncpy(gBridgeNodeList, szBrList, sizeof(gBridgeNodeList)-1);

		SetBelkinParameter (WIFI_BRIDGE_LIST,gBridgeNodeList);
		AsyncSaveData();
	}
	else
	{
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x01;
		APP_LOG("UPNP: Device", LOG_ERR, "parameters error: bridge list empty");  
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "UpdateBridgeList", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], 
				"BridgeList", "unsuccess");  
	}

	FreeXmlSource(szBrList);
	return UPNP_E_SUCCESS;
}

void sendRouterInfoResponse(pUPnPActionRequest pActionRequest, const char *ssid, const char *mac, const char *auth, const char *password, const char *encrypt, const char *channel)
{
    APP_LOG("UPnPDevice", LOG_ERR, "Sending router info upnp response...");
    pActionRequest->ActionResult = NULL;
    pActionRequest->ErrCode = 0x00;
    UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetRouterInformation", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], "ssid", ssid);
    UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetRouterInformation", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], "mac", mac);
    UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetRouterInformation", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], "auth", auth);
    UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetRouterInformation", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], "password", password);
    UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetRouterInformation", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], "encrypt", encrypt);
    UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetRouterInformation", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], "channel", channel);
}

/**
 *
 * GetRouterInformation: Callback to send router information to App 
 *
 *
 *
 *
 *
 *********************************************************************/
int GetRouterInformation(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
    char *pSSID=NULL;
    int ret = 0, chan=-1, i = 0;
    char mac[MAX_MAC_LEN];
    char ssid[MAX_ESSID_LEN];
    char *pAuth=NULL,*pEnc=NULL,*pPass=NULL;
    char channel[CHANNEL_LEN];

    memset(mac, 0, sizeof(mac));
    memset(ssid, 0, sizeof(ssid));
    memset(channel, 0, sizeof(channel));

    pSSID = GetBelkinParameter (WIFI_CLIENT_SSID);
    APP_LOG("UpnpDevice", LOG_DEBUG, "SSID:%s",pSSID);
    if(pSSID && strlen (pSSID) > 0)
    {
	ret = isAPConnected(pSSID);
	if(ret == SUCCESS)
	{
	    APP_LOG("UpnpDevice", LOG_DEBUG, "Router SSID matched...");
	    getRouterEssidMac (ssid, mac, INTERFACE_CLIENT);
	    for(i = 0; mac[i]; i++)
		mac[i] = toupper(mac[i]);
	    pAuth=gWiFiParams.AuthMode;
	    pEnc=gWiFiParams.EncrypType;
	    chan = gWiFiParams.channel;
	    snprintf(channel, sizeof(channel),"%d", chan);
	    pPass=gWiFiParams.Key;

	    APP_LOG("UpnpDevice", LOG_HIDE, "SSID:%s, mac: %s, auth: %s, enc: %s, chan: %s, pass: %s",pSSID,mac,pAuth,pEnc,channel,pPass);
	    sendRouterInfoResponse(pActionRequest, pSSID, mac, pAuth, pPass, pEnc, channel);
	}
	else
	{
	    APP_LOG("UpnpDevice", LOG_DEBUG, "Router SSID not matched... sending invalid response");
	    sendRouterInfoResponse(pActionRequest, "INVALID", "INVALID", "INVALID", "INVALID", "INVALID", "INVALID");
	    return FAILURE;
	}
    }
    else
    {
	APP_LOG("UpnpDevice", LOG_DEBUG, "No saved SSID... sending invalid response");
	sendRouterInfoResponse(pActionRequest, "INVALID", "INVALID", "INVALID", "INVALID", "INVALID", "INVALID");
	return FAILURE;
    }
 
    return UPNP_E_SUCCESS;
}

/**
 * InstaRemoteAccess:
 * 	Callback to Set Insta Remote Access
 * 
 * 
 * *****************************************************************************************************************/
int InstaRemoteAccess(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	pluginRegisterInfo *pPlgReg = NULL;
	RemoteAccessInfo remAcInf;
	char* szMacAddress = NULL;
	char* szDevicePrivKey = NULL;
	char* szDeviceId = NULL;
	char* szDeviceName = NULL;
	char* szHomeId = NULL;
	int ret = SUCCESS;

	APP_LOG("UPNPDevice", LOG_ERR,"Cloud Connection: NO REMOTE");

	/* sanitize remote access request parameters */
	if (pActionRequest == 0x00) 
	{
		APP_LOG("UPNP: Device", LOG_ERR,"INVALID PARAMETERS"); 
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = PLUGIN_ERROR_E_REMOTE_ACCESS;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "InstaRemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE],"statusCode", "F");
		goto on_return;
	}

	memset(&remAcInf, 0x00, sizeof(RemoteAccessInfo));

	szDeviceId = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "DeviceId");
	if(0x0 != szDeviceId && 0x00 == strlen(szDeviceId))
	{
		APP_LOG("UPNP: Device", LOG_ERR,"smart device udid param is null"); 
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = PLUGIN_ERROR_E_REMOTE_ACCESS;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "InstaRemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE],"statusCode", "F");
		goto on_return;
	}

	APP_LOG("UPNPDevice", LOG_HIDE,"SMART DEVICE ID: %s received in upnp action request", szDeviceId);
	strncpy(remAcInf.smartDeviceId, szDeviceId, sizeof(remAcInf.smartDeviceId)-1);

	szDeviceName = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "DeviceName");
	if(0x0 != szDeviceName && 0x00 != strlen(szDeviceName))
	{
		APP_LOG("UPNP: Device", LOG_HIDE,"smart device name is: %s", szDeviceName); 
		strncpy(remAcInf.smartDeviceName, szDeviceName, sizeof(remAcInf.smartDeviceName)-1);
	}
	else
	{
		APP_LOG("UPNP: Device", LOG_ERR,"smart device name param is null"); 
	}


	szHomeId = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "HomeId");
	if (0x00 != szHomeId && (0x00 != strlen(szHomeId)))
	    strncpy(remAcInf.homeId, szHomeId, sizeof(remAcInf.homeId)-1);

	szDevicePrivKey = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "smartprivateKey");
	if (0x00 != szDevicePrivKey && (0x00 != strlen(szDevicePrivKey)))
	{
	    APP_LOG("UPNP: Device", LOG_HIDE,"smart device private key received in request: %s", szDevicePrivKey);
	    strncpy(remAcInf.smartDevicePrivateKey, szDevicePrivKey, sizeof(remAcInf.smartDevicePrivateKey)-1);
	}
	else
	{
	    APP_LOG("UPNP: Device", LOG_DEBUG,"smart device private key received in request is null");
	}

	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;
        UpnpAddToActionResponse(&pActionRequest->ActionResult, "InstaRemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE],"statusCode", "S");

	/* defer request for network reachability */
	pluginUsleep(2000000);

	/* create remote registration thread */	
	ret = createRemoteRegThread(remAcInf);
	if(ret != SUCCESS)
	{
	    goto on_return;
	}

	/* wait here for response */
	pthread_mutex_lock(&g_remoteAccess_mutex);
	pthread_cond_wait(&g_remoteAccess_cond,&g_remoteAccess_mutex);
	pthread_mutex_unlock(&g_remoteAccess_mutex);

	if(pgPluginReRegInf != NULL)
	{
		/* success response */
		if(!strcmp(pgPluginReRegInf->statusCode, "S"))
		{
		    APP_LOG("UPNPDevice", LOG_ERR,"Cloud Connection: AUTHENTICATED");
		    pPlgReg = pgPluginReRegInf;
		}
		/* failure response */
		else
		{
		    APP_LOG("UPnP: Device", LOG_ERR, "insta remote access error");
		    goto on_return;
		}
	}
	/* null response */
	else
	{
	    APP_LOG("UPnP: Device", LOG_ERR, "insta remote access error");
	    goto on_return;
	}

	/* send success response */
	APP_LOG("UPnP: Device", LOG_ERR, "insta remote access success");

on_return:

	/* free registration response, request & xml res */
	if (pgPluginReRegInf) {free(pgPluginReRegInf); pgPluginReRegInf = NULL;}
	freeRemoteXMLRes(szHomeId, szMacAddress, szDeviceId, szDeviceName, szDevicePrivKey);
	/* reset remote request by App flag */
	g_isRemoteAccessByApp = 0;
	return UPNP_E_SUCCESS;
}

#endif//WeMo_INSTACONNECT
