/***************************************************************************
*
*
* SmartRemoteHandler.c
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

#ifdef PRODUCT_WeMo_Smart

#include "global.h"
#include "defines.h"
#include "types.h"
#include "pthread.h"
#include "remoteAccess.h"
#include "mxml.h"
#include "logger.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "gpio.h"
#include "cloudIF.h"
#include "utils.h"
#include "httpsWrapper.h"
#include "controlledevice.h"
#include "natTravIceIntDef.h"

#include "SmartRemoteAccess.h"

#include "wasp_api.h"
#include "wasp.h"

#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

extern char g_szPluginCloudId[SIZE_16B];
extern char g_szFirmwareVersion[SIZE_64B];
extern char g_szWiFiMacAddress[SIZE_64B];
extern int gSignalStrength;
extern char g_szFirmwareVersion[SIZE_64B];
extern char g_NotificationStatus[MAX_RES_LEN];

extern WaspVarDesc **g_attrVarDescArray;
extern int g_attrCount;

#ifdef USE_ICE
extern char gIcePrevTransactionId[PLUGIN_MAX_REQUESTS][TRANSACTION_ID_LEN];
extern int gIceRunning;
extern int gDataInProgress;
extern nattrav_cand_offer_Info gRemoteInfo;
#endif

#define INVALID_THREAD_ID -1

static pthread_mutex_t remote_mutex_lock;

pthread_t remote_worker_tid = INVALID_THREAD_ID;
pthread_t update_device_tid = INVALID_THREAD_ID;

void *pWorkEventQueue = NULL;
void *pUpdateEventQueue = NULL;
char g_szErrorCode[128] = {0};


char* getElementValue(mxml_node_t *curr_node, mxml_node_t *start_node, mxml_node_t **pfind_node, char *pszElementName)
{
  char *pValue = NULL;

  *pfind_node = mxmlFindElement(curr_node, start_node, pszElementName, NULL, NULL, MXML_DESCEND);

  if( NULL == *pfind_node )
  {
    APP_LOG("REMOTEACCESS", LOG_DEBUG, "%s tag not present in xml payload", pszElementName);
    return pValue;
  }

  pValue = (char *)mxmlGetOpaque(*pfind_node);
  if( pValue )
  {
    APP_LOG("REMOTEACCESS", LOG_DEBUG, "%s value is %s", pszElementName, pValue);
  }

  return pValue;
}

char* CreatGetDevicesDetailsRespXML(struct pluginDeviceStatus *pDevStatus)
{
	int state=0;
	int ret = -1;
  
	mxml_node_t *pXml = NULL;
	mxml_node_t *pNodeDeviceStatus = NULL;
	mxml_node_t *pNodePluginId = NULL;
	mxml_node_t *pNodePluginMacAddr = NULL;
	mxml_node_t *pNodePluginStatus = NULL;
	mxml_node_t *pNodePluginFrndlyName = NULL;
	mxml_node_t *pdbVersion = NULL;
	mxml_node_t *prOverriden = NULL;
	mxml_node_t *pNotificationType = NULL;
	mxml_node_t *pNodePluginStatusTS = NULL;
	mxml_node_t *pNodePluginFirmwareVersion = NULL;
	mxml_node_t *pNodePluginFWUpgradeStatus = NULL;
	mxml_node_t *pNodePluginSignalStrength = NULL;
	mxml_node_t *pNodeAttributes = NULL;
	mxml_node_t *pNodeAttribute = NULL;
	
	int i = 0;
	char *pszXML = NULL;
	SmartParameters SmartParams;
	
	pXml = mxmlNewXML("1.0");
	if( NULL == pXml )
	{
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "CreatGetDevicesDetailsRespXML: XML is NULL");
		return pszXML;
	}

	pNodeDeviceStatus = mxmlNewElement(pXml, "plugin");
	
	pNodePluginId = mxmlNewElement(pNodeDeviceStatus, "recipientId");
	mxmlNewText(pNodePluginId, 0, pDevStatus->pluginId);
	
	pNodePluginMacAddr = mxmlNewElement(pNodeDeviceStatus, "macAddress");
	mxmlNewText(pNodePluginMacAddr, 0, pDevStatus->macAddress);

	pNodePluginStatus = mxmlNewElement(pNodeDeviceStatus, "status");
	mxmlNewText(pNodePluginStatus, 0, pDevStatus->status);

	pNodePluginFrndlyName = mxmlNewElement(pNodeDeviceStatus, "friendlyName");
	mxmlNewText(pNodePluginFrndlyName, 0, g_szFriendlyName);
	
	pdbVersion = mxmlNewElement(pNodeDeviceStatus, "dbVersion");
	mxmlNewText(pdbVersion, 0, "-1");
	
	prOverriden = mxmlNewElement(pNodeDeviceStatus, "rOverriden");
	mxmlNewText(prOverriden, 0, "");

	pNotificationType = mxmlNewElement(pNodeDeviceStatus, "notificationType");
	mxmlNewText(pNotificationType, 0, g_NotificationStatus);

	pNodePluginStatusTS = mxmlNewElement(pNodeDeviceStatus, "statusTS");
	mxmlNewText(pNodePluginStatusTS, 0, pDevStatus->statusTS);
	
	pNodePluginFirmwareVersion = mxmlNewElement(pNodeDeviceStatus, "firmwareVersion");
	mxmlNewText(pNodePluginFirmwareVersion, 0, g_szFirmwareVersion);

	state = getCurrFWUpdateState();
	pNodePluginFWUpgradeStatus = mxmlNewElement(pNodeDeviceStatus, "fwUpgradeStatus");
	mxmlNewText(pNodePluginFWUpgradeStatus, 0, state);	
	
	pNodePluginSignalStrength = mxmlNewElement(pNodeDeviceStatus, "signalStrength");
	mxmlNewText(pNodePluginSignalStrength, 0, gSignalStrength);	
	
	pNodeAttributes = mxmlNewElement(pNodeDeviceStatus, "genericAttributes");
	
	for (i = 0; i < g_attrCount; i++)
	{
		memset(&SmartParams, 0x00, sizeof(SmartParameters));
		ret = CloudGetAttributes(&SmartParams,i);	
		if(ret > 0)
		{
			pNodeAttribute = mxmlNewElement(pNodeAttributes, SmartParams.Name);
			mxmlNewText(pNodeAttribute, 0, SmartParams.status);	
			APP_LOG("REMOTEACCESS", LOG_DEBUG, "Smart Parameters updated to XML are %s  and %s", SmartParams.Name, SmartParams.status);
		}
		else
		{
			return pszXML;
		}
		
	}
	
#if 1
  {
    FILE *fp;

    fp = fopen("/tmp/respGetDevicesDetails.xml", "w");
    mxmlSaveFile(pXml, fp, MXML_NO_CALLBACK);
    fclose(fp);
  }
#endif
  pszXML = mxmlSaveAllocString(pXml, MXML_NO_CALLBACK);

  mxmlDelete(pXml);

  return pszXML;

}

// Get Devices List
int GetDeviceStatusHandler(void *hndl, char *xmlBuf, unsigned int buflen, void* data_sock , void* remoteInfo, char* transaction_id)
{
  int retVal = PLUGIN_SUCCESS;
  mxml_node_t *tree = NULL;
  mxml_node_t *find_node = NULL;

  char *dVer = NULL;
  char *pPluginId = NULL, *pMacAddress = NULL;
  char szErrorResponse[MAX_DATA_LEN];

  struct pluginDeviceStatus *pDevStatus = NULL;
 
  if( !xmlBuf )
  {
    retVal = PLUGIN_FAILURE;
    return retVal;
  }

  pDevStatus = (struct pluginDeviceStatus *)CALLOC(1, sizeof(struct pluginDeviceStatus));
  if( !pDevStatus )
  {
    APP_LOG("REMOTEACCESS", LOG_ERR, "Failed to allocate DeviceStatus");
    retVal = PLUGIN_FAILURE;
    return retVal;
  }

  if( buflen )
    CHECK_PREV_TSX_ID(E_PLUGINDEVICESTATUS, transaction_id, retVal);

  tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

  if (tree){
	dVer	 = (char*)CALLOC(1, MAX_RVAL_LEN);
	if (!dVer) {
	    APP_LOG("REMOTEACCESS", LOG_ERR, "dVer malloc failed");
	    retVal = PLUGIN_FAILURE;
	    mxmlDelete(tree);
	    goto handler_exit1;
	}
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "XML String loaded is %s", xmlBuf);
	 //Get infomation of WeMo Bridge device.
	//pPluginId = getElementValue(tree, tree, &find_node, "pluginId");
	pMacAddress = getElementValue(tree, tree, &find_node, "macAddress");

	/*if( pPluginId )
	{
		strncpy(pDevStatus->pluginId, pPluginId, sizeof(pDevStatus->pluginId));
	}*/
	
	pDevStatus->pluginId = strtol(g_szPluginCloudId, NULL, 0);
	
	if( pMacAddress )
	{
		strncpy(pDevStatus->macAddress, pMacAddress, sizeof(pDevStatus->macAddress));
	}

	APP_LOG("REMOTEACCESS", LOG_DEBUG, "pluginDeviceStatus xml parsing is Success...");
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "Start Creating GetDevicesDetails Response XML...")

	if (!strcmp(pDevStatus->macAddress, g_szWiFiMacAddress)) 
	{
		//use gpio interfaces for device status
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "Current Binary state of plug-in %d", GetCurBinaryState());
		pDevStatus->status = CloudGetBinaryState();
		pDevStatus->statusTS = getPluginStatusTS();
		
		//Create Response XML
		char *pszResponse = CreatGetDevicesDetailsRespXML(pDevStatus);

		if( pszResponse )
		{
			APP_LOG("REMOTEACCESS", LOG_DEBUG, "GetDevicesDetails Response XML is %s", pszResponse);

			if( buflen )
			retVal = SendNatPkt(hndl, pszResponse, remoteInfo, transaction_id, data_sock, E_PLUGINDEVICESTATUS);

			free(pszResponse);
		}
		else
		{
			if( pDevStatus->pluginId && pDevStatus->macAddress[0] )
			{
				memset(szErrorResponse, 0x00, MAX_DATA_LEN);
				snprintf(szErrorResponse, MAX_DATA_LEN, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><plugin><statusCode>F</statusCode> \
					<pluginId>%lu</pluginId><macAddress>%s</macAddress></plugin>", pDevStatus->pluginId, pDevStatus->macAddress);

				if( buflen )
					retVal = SendNatPkt(hndl, szErrorResponse, remoteInfo, transaction_id, data_sock, E_PLUGINDEVICESTATUS);
			}
			else
			{
				retVal = PLUGIN_FAILURE;
			}
		}
	}
	else {
		APP_LOG("REMOTEACCESS", LOG_ERR, "Mac received %s doesn't match with plugin Mac %s", pDevStatus->macAddress, g_szWiFiMacAddress);
		retVal = PLUGIN_FAILURE;
	    }
	    mxmlDelete(find_node);
	    find_node = NULL;
  }
  else
  {
	APP_LOG("REMOTEACCESS", LOG_ERR, "XML String %s couldn't be loaded", xmlBuf);
	retVal = PLUGIN_FAILURE;
  }
handler_exit1:
  if( tree )
    mxmlDelete(tree);

  if( pDevStatus )
    free(pDevStatus);

  return retVal;
}

/*
char* CreateNotificationXML()
{
	int ret = -1;
  
	mxml_node_t *pXml = NULL;
	mxml_node_t *pNodeDeviceStatus = NULL;
	mxml_node_t *pNodePluginId = NULL;
	mxml_node_t *pNodePluginMacAddr = NULL;
    mxml_node_t *pNodePluginSrAddr = NULL;
    mxml_node_t *pNodePluginFrndlyName = NULL;
    mxml_node_t *pNodePluginUDNName = NULL;
    mxml_node_t *pNodePluginHomeId = NULL;
    mxml_node_t *pNodePluginDeviceType = NULL;
    mxml_node_t *pNodePluginStatusTS = NULL;
    mxml_node_t *pNodePluginFirmwareVersion = NULL;
    mxml_node_t *pNodePluginFWUpgradeStatus = NULL;
    mxml_node_t *pNodePluginSigStrength = NULL;
	mxml_node_t *pNodeAttributes = NULL;
	mxml_node_t *pNodeAttribute = NULL;
	
	int i = 0;
	char *pszXML = NULL;
	SmartParameters SmartParams;
	
	pXml = mxmlNewXML("1.0");
	if( NULL == pXml )
	{
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "CreateUpdateDeviceStatusRespXML: XML is NULL");
		return pszXML;
	}

	pNodeDeviceStatus = mxmlNewElement(pXml, "plugin");
	
    pNodePluginMacAddr = mxmlNewElement(pNodeDeviceStatus, "macAddress");
	mxmlNewText(pNodePluginMacAddr, 0, g_szWiFiMacAddress);
    
    pNodePluginSrAddr = mxmlNewElement(pNodeDeviceStatus, "serialNumber");
	mxmlNewText(pNodePluginSrAddr, 0, g_szSerialNo);
	
	pNodePluginFrndlyName = mxmlNewElement(pNodeDeviceStatus, "friendlyName");
	mxmlNewText(pNodePluginFrndlyName, 0, g_DevName);
    
    pNodePluginUDNName = mxmlNewElement(pNodeDeviceStatus, "udnName");
	mxmlNewText(pNodePluginUDNName, 0, g_szUDN_1);
    
    pNodePluginHomeId = mxmlNewElement(pNodeDeviceStatus, "homeId");
	mxmlNewText(pNodePluginHomeId, 0, g_szHomeId);
    
    pNodePluginDeviceType = mxmlNewElement(pNodeDeviceStatus, "deviceType");
	mxmlNewText(pNodePluginDeviceType, 0, g_DevName);
    
    pNodePluginStatusTS = mxmlNewElement(pNodeDeviceStatus, "statusTS");
	mxmlNewText(pNodePluginStatusTS, 0, time_stamp);
    
    pNodePluginFirmwareVersion = mxmlNewElement(pNodeDeviceStatus, "firmwareVersion");
	mxmlNewText(pNodePluginFirmwareVersion, 0, g_szFirmwareVersion);
    
    pNodePluginFWUpgradeStatus = mxmlNewElement(pNodeDeviceStatus, "fwUpgradeStatus");
	mxmlNewText(pNodePluginFWUpgradeStatus, 0, state);

	pNodePluginSigStrength = mxmlNewElement(pNodeDeviceStatus, "signalStrength");
	mxmlNewText(pNodePluginSigStrength, 0, gSignalStrength);
	
	pNodeAttributes = mxmlNewElement(pNodeDeviceStatus, "genericAttributes");
	
	for (i = 0; i < g_attrCount; i++)
	{
		memset(&SmartParams, 0x00, sizeof(SmartParameters));
		ret = CloudGetAttributes(&SmartParams,i);	
		if(ret > 0)
		{
			pNodeAttribute = mxmlNewElement(pNodeAttributes, SmartParams.Name);
			mxmlNewText(pNodeAttribute, 0, SmartParams.status);	
		}
		else
		{
			return pszXML;
		}
		
	}
	
  pszXML = mxmlSaveAllocString(pXml, MXML_NO_CALLBACK);

  mxmlDelete(pXml);

  return pszXML;

}
*/
char* CreateUpdateDeviceStatusRespXML(struct pluginDeviceStatus *pDevStatus)
{
	int ret = -1;
  
	mxml_node_t *pXml = NULL;
	mxml_node_t *pNodeDeviceStatus = NULL;
	mxml_node_t *pNodePluginId = NULL;
	mxml_node_t *pNodePluginMacAddr = NULL;
	mxml_node_t *pNodePluginStatus = NULL;
	mxml_node_t *pNodePluginFrndlyName = NULL;
	mxml_node_t *pNodePluginStatusTS = NULL;
	mxml_node_t *pNodeAttributes = NULL;
	mxml_node_t *pNodeAttribute = NULL;
	
	int i = 0;
	char *pszXML = NULL;
	SmartParameters SmartParams;
	
	pXml = mxmlNewXML("1.0");
	if( NULL == pXml )
	{
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "CreateUpdateDeviceStatusRespXML: XML is NULL");
		return pszXML;
	}

	pNodeDeviceStatus = mxmlNewElement(pXml, "plugin");
	
	pNodePluginId = mxmlNewElement(pNodeDeviceStatus, "recipientId");
	mxmlNewText(pNodePluginId, 0, pDevStatus->pluginId);
	
	pNodePluginMacAddr = mxmlNewElement(pNodeDeviceStatus, "macAddress");
	mxmlNewText(pNodePluginMacAddr, 0, pDevStatus->macAddress);

	pNodePluginStatus = mxmlNewElement(pNodeDeviceStatus, "status");
	mxmlNewText(pNodePluginStatus, 0, pDevStatus->status);

	pNodePluginFrndlyName = mxmlNewElement(pNodeDeviceStatus, "friendlyName");
	mxmlNewText(pNodePluginFrndlyName, 0, g_szFriendlyName);
	
	pNodePluginStatusTS = mxmlNewElement(pNodeDeviceStatus, "statusTS");
	mxmlNewText(pNodePluginStatusTS, 0, pDevStatus->statusTS);
	
	pNodeAttributes = mxmlNewElement(pNodeDeviceStatus, "genericAttributes");
	
	for (i = 0; i < g_attrCount; i++)
	{
		memset(&SmartParams, 0x00, sizeof(SmartParameters));
		ret = CloudGetAttributes(&SmartParams,i);	
		if(ret > 0)
		{
			pNodeAttribute = mxmlNewElement(pNodeAttributes, SmartParams.Name);
			mxmlNewText(pNodeAttribute, 0, SmartParams.status);	
		}
		else
		{
			return pszXML;
		}
		
	}
	
#if 1
  {
    FILE *fp;

    fp = fopen("/tmp/respGetDevicesDetails.xml", "w");
    mxmlSaveFile(pXml, fp, MXML_NO_CALLBACK);
    fclose(fp);
  }
#endif
  pszXML = mxmlSaveAllocString(pXml, MXML_NO_CALLBACK);

  mxmlDelete(pXml);

  return pszXML;

}

// Set Device Parameters
int SetDevStatusHandler(void *hndl, char *xmlBuf, unsigned int buflen, void* data_sock , void* remoteInfo, char* transaction_id)
{
  int retVal = PLUGIN_SUCCESS;
  mxml_node_t *tree = NULL;
  mxml_node_t *find_node = NULL;
  mxml_node_t *device_node = NULL;

  int i=0, j =0, count =0;
  int ret = -1;
  char *pPluginId = NULL, *pMacAddress = NULL;
  char szErrorResponse[MAX_DATA_LEN];

  struct pluginDeviceStatus *pDevStatus = NULL;
  char *pAttributeValue = NULL;
  char *pszResponse = NULL;

  #if 0
  struct SmartParameters SmartParams[];
  #else
  SmartParameters SmartParams;
  #endif
  
  if( !xmlBuf )
  {
    retVal = PLUGIN_FAILURE;
    return retVal;
  }

  pDevStatus = (struct pluginDeviceStatus *)CALLOC(1, sizeof(struct pluginDeviceStatus));
  if( !pDevStatus )
  {
    APP_LOG("REMOTEACCESS", LOG_ERR, "Failed to allocate DeviceStatus");
    retVal = PLUGIN_FAILURE;
    return retVal;
  }

  if( buflen )
    CHECK_PREV_TSX_ID(E_PLUGINDEVICESTATUS, transaction_id, retVal);

  tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

  if (tree){
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "XML String loaded is %s", xmlBuf);
	
	//pPluginId = getElementValue(tree, tree, &find_node, "pluginId");
	pMacAddress = getElementValue(tree, tree, &find_node, "macAddress");

	/*if( pPluginId )
	{
		strncpy(pDevStatus->pluginId, pPluginId, sizeof(pDevStatus->pluginId));
	}*/
	
	pDevStatus->pluginId = strtol(g_szPluginCloudId, NULL, 0);
	
	if( pMacAddress )
	{
		strncpy(pDevStatus->macAddress, pMacAddress, sizeof(pDevStatus->macAddress));
	}

	if (!strcmp(pDevStatus->macAddress, g_szWiFiMacAddress)) 
	{
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "Current Binary state of plug-in %d", GetCurBinaryState());
		pDevStatus->status = CloudGetBinaryState();
		pDevStatus->statusTS = getPluginStatusTS();
		
		for( device_node = mxmlFindElement(tree, tree, "genericAttributes", NULL, NULL, MXML_DESCEND);
			 device_node != NULL;
			 device_node = mxmlFindElement(device_node, tree, "genericAttributes", NULL, NULL, MXML_DESCEND) )
		{
			for (i = 0; i < g_attrCount; i++)
			{
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "SetDevStatusHandler: i is %d, g_attrVarDescArray[i]->Name is %s g_attrVarDescArray[i]->Usage is %d", 
													i, g_attrVarDescArray[i]->Name,g_attrVarDescArray[i]->Usage);
				
				if(g_attrVarDescArray[i]->Usage == 4 || g_attrVarDescArray[i]->Usage == 3)
				{ 
					pAttributeValue = getElementValue(device_node, tree, &find_node, g_attrVarDescArray[i]->Name);
					if( pAttributeValue )
					{
						#if 0
						/* Will be useful to call set all parameters at a time */
						strncpy(SmartParams[j]->status, pAttributeValue, sizeof(SmartParams[j]->status));
						j++;
						SmartParams[j]->AttrID = g_attrVarDescArray[i]->ID;
						
						APP_LOG("REMOTEACCESS", LOG_DEBUG, "SetDevStatusHandler: Name of Attribute is %s, Attribute ID is %c and to set status to %s", 
								g_attrVarDescArray[i]->Name, SmartParams[j]->AttrID, SmartParams[j]->status);
						#else
						memset(&SmartParams, 0x00, sizeof(SmartParams));
						strncpy(SmartParams.status, pAttributeValue, sizeof(SmartParams.status));
						strncpy(SmartParams.Name, g_attrVarDescArray[i]->Name, sizeof(SmartParams.Name));
						SmartParams.AttrID = g_attrVarDescArray[i]->ID;
						SmartParams.AttrType = g_attrVarDescArray[i]->Type;
						
						APP_LOG("REMOTEACCESS", LOG_DEBUG, "SetDevStatusHandler: Name of Attribute is %s, Attribute ID is %c and to set status to %s", 
								SmartParams.Name, SmartParams.AttrID, SmartParams.status);
						#endif
						ret = CloudSetAttributes(&SmartParams);
						if (ret == 0x00)
						{
							/* Proceed to Next variable */
						}
						else
						{
							retVal = PLUGIN_FAILURE;
							return retVal;
						}
					}
				}
			}
			count = j;
		}
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "pluginSetStatus xml parsing is Success...");
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "Start Creating SetDevicesDetails Response XML...");
		
		//Create Response XML
		pszResponse = CreateUpdateDeviceStatusRespXML(pDevStatus);

		if( pszResponse )
		{
			APP_LOG("REMOTEACCESS", LOG_DEBUG, "SetDevStatusHandler Response XML is %s", pszResponse);

			if( buflen )
				retVal = SendNatPkt(hndl, pszResponse, remoteInfo, transaction_id, data_sock, E_PLUGINSETDEVICESTATUS);

			free(pszResponse);
		}
		else
		{		
			if( pDevStatus->pluginId && pDevStatus->macAddress[0] )
			{
				memset(szErrorResponse, 0x00, MAX_DATA_LEN);
				snprintf(szErrorResponse, MAX_DATA_LEN, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><plugin><statusCode>F</statusCode> \
				<pluginId>%lu</pluginId><macAddress>%s</macAddress></plugin>", pDevStatus->pluginId, pDevStatus->macAddress);

				if( buflen )
					retVal = SendNatPkt(hndl, szErrorResponse, remoteInfo, transaction_id, data_sock, E_PLUGINSETDEVICESTATUS);
			}
			else
			{
				retVal = PLUGIN_FAILURE;
			}
		}
	}
	else {
		APP_LOG("REMOTEACCESS", LOG_ERR, "Mac received %s doesn't match with plugin Mac %s", pDevStatus->macAddress, g_szWiFiMacAddress);
		retVal = PLUGIN_FAILURE;
	    }
	    mxmlDelete(find_node);
	    find_node = NULL;
  }
  else
  {
	APP_LOG("REMOTEACCESS", LOG_ERR, "XML String %s couldn't be loaded", xmlBuf);
	retVal = PLUGIN_FAILURE;
  }
handler_exit1:
  if( tree )
    mxmlDelete(tree);

  if( pDevStatus )
    free(pDevStatus);

  return retVal;
}

#endif
