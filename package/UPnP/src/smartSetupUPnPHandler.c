/***************************************************************************
*
*
* smartSetupUPnPHandler.c
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

#ifdef WeMo_SMART_SETUP

#include "global.h"
#include "defines.h"
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
#include "mxml.h"
#include "smartSetupUPnPHandler.h"
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

extern int gBootReconnect;
extern int gAppCalledCloseAp;
extern char gUserKey[PASSWORD_MAX_LEN];
extern char gGatewayMAC[MAX_MAC_LEN];
int gSmartSetup = 0;
#ifdef WeMo_SMART_SETUP_V2
int g_customizedState = DEVICE_UNCUSTOMIZED;
int g_isAppConnected = 0;
#endif

void* StartPairAndRegisterThread(void* arg)
{
	tu_set_my_thread_name( __FUNCTION__ );

	char channel[CHANNEL_LEN] = {'\0',};
	char* pPairingXMLData=NULL,*pRegistrationXMLData=NULL;
	PPAIR_AND_REG_INFO pPairAndRegInfo = NULL;
	WIFI_PAIR_PARAMS pairInfo;
	RemoteAccessInfo regInfo;
	char *pPairingParamSet[6] = {"PairingData","ssid","auth","password","encrypt","channel"};
	char *pRegistrationParamSet[5] = {"RegistrationData","DeviceId","DeviceName","smartprivateKey","ReUnionKey"};
	char *pvalueSet[5] = {NULL,};
	int retVel = -1;
	
	/*check for null argument*/
	if(arg)
		pPairAndRegInfo = (PPAIR_AND_REG_INFO)arg;
	else
		goto CLEAN_UP;

	/*get pairing and registration info from argument*/
	if((pPairAndRegInfo->pPairingData) && (strlen(pPairAndRegInfo->pPairingData)) && (pPairAndRegInfo->pRegistrationData) && (strlen(pPairAndRegInfo->pRegistrationData)))
	{
		pPairingXMLData = pPairAndRegInfo->pPairingData;
		pRegistrationXMLData = pPairAndRegInfo->pRegistrationData;
	}
	else
	{
		goto CLEAN_UP;
	}

	//APP_LOG("UPNPDevice", LOG_DEBUG,"arg, pPairingXMLData:%s\npRegistrationXMLData:%s", pPairingXMLData,pRegistrationXMLData);

	/*parse pairing parameters*/
	memset(&pairInfo, 0, sizeof(WIFI_PAIR_PARAMS));
	pvalueSet[0]=pairInfo.SSID;
	pvalueSet[1]=pairInfo.AuthMode;
	pvalueSet[2]=pairInfo.Key;
	pvalueSet[3]=pairInfo.EncrypType;
	pvalueSet[4]=channel;

	if(FAILURE == parseXMLData(pPairingXMLData, pPairingParamSet, pvalueSet, 5))
	{
		APP_LOG("UPNPDevice", LOG_DEBUG,"parse Pairing Data failed");
		goto CLEAN_UP;
	}

	/*Fill in pairing channel information */
	pairInfo.channel = atoi(channel);
	APP_LOG("UPNPDevice", LOG_HIDE,"Pairing Params:\nssid:%s\nauth:%s\npassword:%s\nencrypt:%s\nchannel:%d", pairInfo.SSID,pairInfo.AuthMode,pairInfo.Key,pairInfo.EncrypType,pairInfo.channel);

	/*parse registration parameters*/
	memset(&regInfo, 0, sizeof(RemoteAccessInfo));
	pvalueSet[0]=regInfo.smartDeviceId;
	pvalueSet[1]=regInfo.smartDeviceName;
	pvalueSet[2]=regInfo.smartDevicePrivateKey;
	pvalueSet[3]=regInfo.reunionKey;

	if(FAILURE == parseXMLData(pRegistrationXMLData, pRegistrationParamSet, pvalueSet, 4))
	{
		APP_LOG("UPNPDevice", LOG_DEBUG,"parse Pairing Data failed");
		goto CLEAN_UP;
	}
	APP_LOG("UPNPDevice", LOG_HIDE,"Registartion Params:\nDeviceId:%s\nDeviceName:%s\nsmartprivateKey:%s\nReUnionKey:%s",regInfo.smartDeviceId,regInfo.smartDeviceName,regInfo.smartDevicePrivateKey,regInfo.reunionKey);
	
	/*fill pairing and register data and start pairing*/
	gAppCalledCloseAp=0;
	gBootReconnect=0; 
	/* set flag for smart Setup in progress */
	gSmartSetup = 1;

	setSetupRequested(1);
	UpdateUPnPNetworkMode(UPNP_LOCAL_MODE);

	if(!strcmp(pairInfo.AuthMode,"OPEN"))
	{
		memset(pairInfo.Key, 0, sizeof(pairInfo.Key));
		strncpy(pairInfo.Key,"NOTHING",sizeof(pairInfo.Key)-1);
	}

	/* Save the password in a global variable - to be used later by WifiConn thread */
	memset(gUserKey, 0, sizeof(gUserKey));
	memcpy(gUserKey, pairInfo.Key, sizeof(gUserKey));

	APP_LOG("UPNPDevice",LOG_DEBUG,"Starting pairing to home network");

	retVel = pairToRouter(&pairInfo);
	if(retVel == SUCCESS)
	{
		/* Setup successful, attempt registration */
		APP_LOG("UPNPDevice",LOG_DEBUG,"Setup successful, attempt registration");
		/* defer request for network reachability */
		pluginUsleep(2000000);

		/* create remote registration thread */
		retVel = createRemoteRegThread(regInfo);
		if(retVel != SUCCESS)
		{
			APP_LOG("UPNPDevice",LOG_ERR,"Setup successful, attempt registration");
		}
	}
	else
		APP_LOG("UPNPDevice",LOG_ERR,"Setup Failed, Exit thread!");

CLEAN_UP:
	if(pPairAndRegInfo->pPairingData){free(pPairAndRegInfo->pPairingData);pPairAndRegInfo->pPairingData=NULL;}
	if(pPairAndRegInfo->pRegistrationData){free(pPairAndRegInfo->pRegistrationData);pPairAndRegInfo->pRegistrationData=NULL;}
	if(pPairAndRegInfo){free(pPairAndRegInfo);pPairAndRegInfo=NULL;}
	pPairingXMLData = NULL;pRegistrationXMLData = NULL;
	return NULL;
}

int PairAndRegister(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	char* pPairingXMLData=NULL,*pRegistrationXMLData=NULL;
	pthread_t PairAndRegister_thread = -1;
	PPAIR_AND_REG_INFO pPairAndRegInfo = NULL;
	int retVal = 0;

	if(isSetupRequested())
	{
		APP_LOG("UPNPDevice", LOG_ERR, "Setup request already executed");
		goto CLEAN_UP;
	}

	pPairingXMLData = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "PairingData");
	pRegistrationXMLData = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "RegistrationData");

	/*validate parameters*/
	if((0x00 == pPairingXMLData|| 0x00 == strlen(pPairingXMLData)) || (0x00 == pRegistrationXMLData|| 0x00 == strlen(pRegistrationXMLData)))
	{
		APP_LOG("UPNPDevice", LOG_ERR, "Smart Setup parameter error");
		goto CLEAN_UP;
	}
	APP_LOG("UPNPDevice", LOG_HIDE,"pPairingXMLData:%s\npRegistrationXMLData:%s", pPairingXMLData,pRegistrationXMLData);

	/*file in bridgeinfo structure*/
	pPairAndRegInfo = (PPAIR_AND_REG_INFO)CALLOC(1, sizeof(PAIR_AND_REG_INFO));
	if(pPairAndRegInfo)
	{
		pPairAndRegInfo->pPairingData = (char*)CALLOC(1, (strlen(pPairingXMLData) + 1));
		if(!pPairAndRegInfo->pPairingData)
			goto CLEAN_UP;
		else
			strncpy(pPairAndRegInfo->pPairingData, pPairingXMLData, strlen(pPairingXMLData));  // <-----<<< WRONG

		pPairAndRegInfo->pRegistrationData = (char*)CALLOC(1, (strlen(pRegistrationXMLData) + 1));
		if(!pPairAndRegInfo->pRegistrationData)
			goto CLEAN_UP;
		else
			strncpy(pPairAndRegInfo->pRegistrationData, pRegistrationXMLData, strlen(pRegistrationXMLData));  // <-----<<< WRONG
	}
/*
	else
	{
		goto CLEAN_UP;
	}
*/
	/*Start Smart setup Thread*/
	retVal = pthread_create(&PairAndRegister_thread, NULL, StartPairAndRegisterThread, (void*)pPairAndRegInfo);
	if ((retVal < SUCCESS) || (-1 == PairAndRegister_thread))
	{
		APP_LOG("UPNPDevice",LOG_ERR, "Failed to Create Start Pair And Register Thread, Exit!");
		resetSystem();
	}
	else
		pthread_detach(PairAndRegister_thread);

	/*send UPnP connecting reseponse*/
	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "PairAndRegister", CtrleeDeviceServiceType[PLUGIN_E_SMART_SETUP_SERVICE],"PairingStatus", "Connecting");

	/*free memory*/
	if(pPairingXMLData){FreeXmlSource(pPairingXMLData);pPairingXMLData=NULL;}
	if(pRegistrationXMLData){FreeXmlSource(pRegistrationXMLData);pRegistrationXMLData=NULL;}
	return SUCCESS;

CLEAN_UP:
	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x01;
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "PairAndRegister", CtrleeDeviceServiceType[PLUGIN_E_SMART_SETUP_SERVICE], "PairingStatus", "Error");
	return FAILURE;
}

#define CDATA_DELIM "![CDATA["
#define CDATA_DELIM_SLEN 8
#define CDATA_DELIM_ELEN 2
int parseXMLData(char *apXMLData, char **apParameterToParse, char** apOutData, int aParameterCount)
{
	int retVal = PLUGIN_SUCCESS,paramParsedCount=0;
	mxml_node_t *tree=NULL,*chNode=NULL,*top_node=NULL;
	char *localXMLData = NULL;

	if (!apXMLData) 
		return PLUGIN_FAILURE;

	//APP_LOG("UPNP", LOG_DEBUG, "XML received is:\n %s\n", apXMLData);

	localXMLData = (char*)CALLOC(1, (strlen(apXMLData)+SIZE_64B));
	if (!localXMLData) 
		return PLUGIN_FAILURE;
	snprintf(localXMLData, (strlen(apXMLData)+SIZE_64B), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>%s", apXMLData);
	
	//APP_LOG("UPNP", LOG_DEBUG, "XML modified is:\n %s\n", localXMLData);

	tree = mxmlLoadString(NULL, localXMLData, MXML_OPAQUE_CALLBACK);
	if (tree)
	{
		APP_LOG("UPNP", LOG_HIDE, "XML Loaded is:\n%s", localXMLData);
		top_node = mxmlFindElement(tree, tree, apParameterToParse[0], NULL, NULL, MXML_DESCEND);
		if (!top_node)
		{
			APP_LOG("UPNP", LOG_DEBUG, "node index error");
			retVal = PLUGIN_FAILURE;
			goto out;
		}

		while(paramParsedCount < aParameterCount)
		{
			chNode = mxmlFindElement(tree, tree, apParameterToParse[paramParsedCount+1], NULL, NULL, MXML_DESCEND);
			if (chNode)
			{
				/*if ((chNode->child) && (chNode->child->value.opaque) && (strlen(chNode->child->value.opaque)))
					APP_LOG("UPNP", LOG_ERR, "tag value [%s]", chNode->child->value.opaque);*/
				if((chNode->child) && strlen(chNode->child->value.opaque))
				{
				  //APP_LOG("UPNP", LOG_ERR, "tag value....0 [%s]", chNode->child->value.opaque);
				  if (strstr(chNode->child->value.opaque, CDATA_DELIM)){
						memcpy(apOutData[paramParsedCount], (chNode->child->value.opaque+CDATA_DELIM_SLEN), \
								(strlen(chNode->child->value.opaque)-(CDATA_DELIM_SLEN+CDATA_DELIM_ELEN)));
						//APP_LOG("UPNP", LOG_ERR, "tag value....1 [%s]", apOutData[paramParsedCount]);
					}else {
						strncpy(apOutData[paramParsedCount], (chNode->child->value.opaque), strlen(chNode->child->value.opaque));  // <-----<<< WRONG
						//APP_LOG("UPNP", LOG_ERR, "tag value....2 [%s]", apOutData[paramParsedCount]);
					}
					APP_LOG("UPNP", LOG_HIDE,"The node <%s> with value is <%s>", apParameterToParse[paramParsedCount+1],apOutData[paramParsedCount]);
				}
				paramParsedCount++;
			}
			else
			{
				APP_LOG("UPNP", LOG_ERR, "tag <%s> or tag value not found",apParameterToParse[paramParsedCount+1]);
				paramParsedCount++;
			}
		}
		mxmlDelete(tree);
		APP_LOG("UPNP", LOG_DEBUG, "tree deleted...");
	}
	else
		APP_LOG("UPNP", LOG_ERR, "Could not load tree");
out:
	if (localXMLData)
	{
		free(localXMLData); 
		localXMLData=NULL;
	}
	return retVal;

}

int GetRegistrationData(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	char* pSmartDeviceXMLData = NULL,*status= NULL, *code=NULL, *pvtkey = "";
	char homeid[MAX_RVAL_LEN]={'\0',};
	char pRegistrationXMLData[SIZE_256B] = {'\0',};
	char smartDeviceId[MAX_PKEY_LEN],reunionKey[MAX_PKEY_LEN];
	char *pSmartDeviceData[3] = {"SmartDeviceData","SmartUniqueId","ReUnionKey"};
	char *pvalueSet[2];

	pSmartDeviceXMLData = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "SmartDeviceData");

	/*validate parameters*/
	if((0x00 == pSmartDeviceXMLData || 0x00 == strlen(pSmartDeviceXMLData)))
	{
		APP_LOG("UPNPDevice", LOG_ERR, "Get Registration Data parameter error");
		goto CLEAN_UP;
	}
	APP_LOG("UPNPDevice", LOG_HIDE,"pSmartDeviceXMLData from APP:\n%s", pSmartDeviceXMLData);

	/*parse registration parameters*/
	memset(smartDeviceId, 0, sizeof(smartDeviceId));
	memset(reunionKey, 0, sizeof(reunionKey));
	pvalueSet[0]=smartDeviceId;
	pvalueSet[1]=reunionKey;

	if(FAILURE == parseXMLData(pSmartDeviceXMLData, pSmartDeviceData, pvalueSet, 2))
	{
		APP_LOG("UPNPDevice", LOG_DEBUG,"SmartDeviceXMLData parsing Data failed");
		goto CLEAN_UP;
	}
	APP_LOG("UPNPDevice", LOG_HIDE,"Smart Device Params:\nsmartDeviceId:%s\nreunionKey:%s\ngszSetupSmartDevUniqueId:%s\ngszSetupReunionKey:%s",smartDeviceId,reunionKey, gszSetupSmartDevUniqueId,gszSetupReunionKey);

	if(pgSetupRegResp) 
	{
		if(!strcmp(gszSetupSmartDevUniqueId, smartDeviceId) && (!strcmp(gszSetupReunionKey,reunionKey)))
		{
			APP_LOG("UPNPDevice", LOG_DEBUG,"Device matched, status: %s, desc: %s, resCode: %s", pgSetupRegResp->statusCode, pgSetupRegResp->description, pgSetupRegResp->resultCode);
			/* our setup device and phone */
			status = pgSetupRegResp->statusCode;
			code = pgSetupRegResp->resultCode;

			if(!strcmp(status, "F"))
			{
				if(!strlen(code))
					code = "FAIL";
			}
			else
			{
				snprintf(homeid, sizeof(homeid), "%lu", pgSetupRegResp->pluginDevInf.homeId);
				pvtkey = pgSetupRegResp->smartDevInf.privateKey;
				APP_LOG("UPNPDevice", LOG_HIDE,"Success case, homeid: %s, pvtkey: %s", homeid, pvtkey);
			}
		}
		else
		{
			/* Not the smartphone that setup this device  */
			status="F";
			code = "NO_MATCH";
		}
	}
	else
	{
		/* No registration response from Cloud yet */
		status="F";
		code = "RETRY";
	}

	snprintf(pRegistrationXMLData, SIZE_256B, "<RegistrationData>\n<RegistrationStatus>%s</RegistrationStatus>\n<StatusCode>%s</StatusCode>\n<HomeId>%s</HomeId>\n<SmartPrivateKey>%s</SmartPrivateKey>\n<ArpMac>%s</ArpMac>\n</RegistrationData>", 
				status, code, homeid, pvtkey, gGatewayMAC);

	APP_LOG("UPNPDevice", LOG_HIDE,"pRegistrationXMLData:\n%s\nlen:%d" ,pRegistrationXMLData, strlen(pRegistrationXMLData));

CLEAN_UP:
	/*send UPnP connecting resepeonse*/
	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetRegistrationData", CtrleeDeviceServiceType[PLUGIN_E_SMART_SETUP_SERVICE], "RegistrationData", pRegistrationXMLData);

	return SUCCESS;
}

int GetRegistrationStatus(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	/*send UPnP connecting resepeonse*/
	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;

	if(pgSetupRegResp)
		APP_LOG("UPNPDevice", LOG_DEBUG,"status: %s, code: %s, desc: %s", pgSetupRegResp->statusCode, pgSetupRegResp->resultCode, pgSetupRegResp->description);

	if(pgSetupRegResp)
	{
		char result[MAX_RES_LEN] = {'\0',};
		char code[MAX_DVAL_LEN]  = {'\0',};

		strncpy(result, pgSetupRegResp->statusCode, sizeof(result) - 1);

		if((strcmp(result, "F") == 0) && !strlen(pgSetupRegResp->resultCode))
		{
			strncpy(code, "FAIL", sizeof(code) - 1);
				
		}
		else
			strncpy(code, pgSetupRegResp->resultCode, sizeof(code) - 1);

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetRegistrationStatus", CtrleeDeviceServiceType[PLUGIN_E_SMART_SETUP_SERVICE], "RegistrationStatus", result);
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetRegistrationStatus", CtrleeDeviceServiceType[PLUGIN_E_SMART_SETUP_SERVICE], "StatusCode", code);
	}
	else
	{
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetRegistrationStatus", CtrleeDeviceServiceType[PLUGIN_E_SMART_SETUP_SERVICE], "RegistrationStatus", "F");
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetRegistrationStatus", CtrleeDeviceServiceType[PLUGIN_E_SMART_SETUP_SERVICE], "StatusCode", "RETRY");
	}
	return SUCCESS;
}

#ifdef WeMo_SMART_SETUP_V2
void setCustomizedState(int state)
{
    char szState[SIZE_4B];

    g_customizedState = state;
    APP_LOG("UPNP",LOG_DEBUG, "Customized state: %d", g_customizedState);

    memset (szState, 0, sizeof(szState));
    snprintf(szState, sizeof(szState), "%d", state);
    SetBelkinParameter(CUSTOMIZED_STATE, szState);
    AsyncSaveData();
}

int SetCustomizedState(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
    char status[SIZE_4B] = {'1'};

    setCustomizedState(DEVICE_CUSTOMIZED_NOTIFY);
    sendCustomizedStateNotification();

    /*send UPnP response*/
    pActionRequest->ActionResult = NULL;
    pActionRequest->ErrCode = 0x00;
    UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetCustomizedState", CtrleeDeviceServiceType[PLUGIN_E_SMART_SETUP_SERVICE], "Status", status);

    return SUCCESS;
}

int GetCustomizedState(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
    char customizedState[SIZE_4B];
    memset(customizedState, 0, sizeof(customizedState));

    if(g_customizedState)
	snprintf(customizedState, sizeof(customizedState), "%d", DEVICE_CUSTOMIZED);
    else
	snprintf(customizedState, sizeof(customizedState), "%d", DEVICE_UNCUSTOMIZED);
    APP_LOG("SMART", LOG_DEBUG, "customized state: %s", customizedState);

    /*send UPnP response*/
    pActionRequest->ActionResult = NULL;
    pActionRequest->ErrCode = 0x00;
    UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetCustomizedState", CtrleeDeviceServiceType[PLUGIN_E_SMART_SETUP_SERVICE], "CustomizedState", customizedState);

    return SUCCESS;
}
#endif

#endif
