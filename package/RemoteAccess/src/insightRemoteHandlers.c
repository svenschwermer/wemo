/***************************************************************************
*
*
* insightRemoteHandler.c
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

#ifdef PRODUCT_WeMo_Insight

#include "global.h"
#include "defines.h"
#include "gpio.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "httpsWrapper.h"
#include "remoteAccess.h"
#include "logger.h"
#include "controlledevice.h"
#include "wifiHndlr.h"
#include "insightRemoteAccess.h"
#include "insight.h"
#include "utils.h"
#include "InsightHandler.h"
#include "natClient.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

extern int g_PushToCloudTime;
extern char g_SendToCloud;//Send Info to cloud for first time when device connects to cloud
extern char g_szPluginCloudId[MAX_RVAL_LEN];
extern char g_InsightSettingsSend;//Start sending instantaneous value once insight parameters are sent.
extern  int gIceRunning;
extern char gIcePrevTransactionId[PLUGIN_MAX_REQUESTS][TRANSACTION_ID_LEN];
extern int g_perUnitCost;   //per unit cost 
extern int g_PerUnitCostVer; //energy per unit cost version
extern int g_Currency; //currency for calculation

char * GetDataExportTypeStr(eDATA_EXPORT_TYPE aDataExportType)
{
        char * returnStr = "/";;
        switch(aDataExportType)
        {
                case    E_SEND_DATA_NOW:
                        returnStr = "/now";
                        break;
                case    E_SEND_DATA_DAILY:
                        returnStr =  "/daily";
                        break;
                case    E_SEND_DATA_WEEKLY:
                        returnStr = "/weekly";
                        break;
                case    E_SEND_DATA_MONTHLY:
                        returnStr = "/monthly";
                        break;
                default:
                        returnStr = "/";
        }
        return returnStr;
}

int UploadInsightDataFileStatus(void *arg)
{
	int retVal = PLUGIN_SUCCESS;
	UserAppSessionData *pInsightSsnData = NULL;
	UserAppData *pInsightData = NULL;
	char *mac_addr = NULL;
	struct stat fileInfo;
	char *serial_num;
	char pluginKey[MAX_PKEY_LEN];
	authSign *assign = NULL;
	char * shceduleType = "/";
	char tmp_url[MAX_FILE_LINE];

	int DataExportscheduleType = E_SEND_DATA_NOW; 
	char EmailAddress[SIZE_256B] = {0};
	char DataExportscheduleTypeStr[SIZE_8B] = {0};
	char *valuestr[2] = {DataExportscheduleTypeStr,EmailAddress};
	if((0x00 != (char*)arg) && (0x00 != strlen((char*)arg)))
	{
		TokenParser(valuestr,(char*)arg, 2);
	}
	else
	{
		APP_LOG("UploadInsightDataFileStatus", LOG_DEBUG,"Invalid Arguments.");
		retVal = PLUGIN_FAILURE;
		goto on_return;
	}

	DataExportscheduleType = atoi(DataExportscheduleTypeStr);
	APP_LOG("UploadInsightDataFileStatus", LOG_HIDE,"arg:%s,DataExportscheduleType:%d,EmailAddress:%s", (char*)arg,DataExportscheduleType,EmailAddress);

	mac_addr = utilsRemDelimitStr(GetMACAddress(), ":");
	serial_num = GetSerialNumber();
	memset(pluginKey, 0x0, MAX_PKEY_LEN);
	strncpy(pluginKey, g_szPluginPrivatekey, sizeof(pluginKey)-1);
	
	assign = createAuthSignature(mac_addr, serial_num, pluginKey);
	if (!assign) {
		APP_LOG("UploadInsightDataFileStatus", LOG_DEBUG,"Signature Structure returned NULL");
		retVal = PLUGIN_FAILURE;
		goto on_return;
	}

	pInsightData = (UserAppData *)ZALLOC(sizeof(UserAppData));
	if(!pInsightData) {
		APP_LOG("UploadInsightDataFileStatus:", LOG_ERR, "Malloc failed, returned NULL, exiting");
		retVal = PLUGIN_FAILURE;
		goto on_return;
	}

	pInsightSsnData = webAppCreateSession(0);
	if(!pInsightSsnData) {
		APP_LOG("UploadInsightDataFileStatus:", LOG_ERR, "Failed to create session, returned NULL");
		retVal = PLUGIN_FAILURE;
		goto on_return;
	}

	//memset(gplugin_Insight_Data_upload_url, 0, sizeof(gplugin_Insight_Data_upload_url)/sizeof(char));
	strncpy(pInsightData->keyVal[0].key, "Content-Type", sizeof(pInsightData->keyVal[0].key)-1);
	strncpy(pInsightData->keyVal[0].value, "multipart/octet-stream", sizeof(pInsightData->keyVal[0].value)-1);
	strncpy(pInsightData->keyVal[1].key, "Accept", sizeof(pInsightData->keyVal[1].key)-1);
	strncpy(pInsightData->keyVal[1].value, "application/xml", sizeof(pInsightData->keyVal[1].value)-1);
	strncpy(pInsightData->keyVal[2].key, "Authorization", sizeof(pInsightData->keyVal[2].key)-1);
	strncpy(pInsightData->keyVal[2].value, assign->signature, sizeof(pInsightData->keyVal[2].key)-1);
	strncpy( pInsightData->keyVal[3].key, "X-Belkin-Client-Type-Id", sizeof(pInsightData->keyVal[3].key)-1);   
	strncpy( pInsightData->keyVal[3].value, g_szClientType, sizeof(pInsightData->keyVal[3].value)-1);   
	pInsightData->keyValLen = 4;

	APP_LOG("UploadInsightDataFileStatus:",LOG_HIDE, "assign->signature... %s", assign->signature);

	strncpy(pInsightData->mac, mac_addr, sizeof(pInsightData->mac)-1);

	//Sending the Insight data file 
	memset(&fileInfo, '\0', sizeof(struct stat));
	lstat(INSIGHT_DATA_FILE_URL, &fileInfo);
	memset(tmp_url,0,sizeof(tmp_url));
	snprintf(tmp_url, sizeof(tmp_url), "https://%s:8443/apis/http/plugin/insight/sendMail/", BL_DOMAIN_NM);
	if((off_t)fileInfo.st_size != 0) {
		memset(pInsightData->url, 0, sizeof(pInsightData->url)/sizeof(char));
		memset(pInsightData->inData, 0, sizeof(pInsightData->inData)/sizeof(char));
		//strncpy(pInsightData->url, gplugin_Insight_Data_upload_url, sizeof(pInsightData->url)-1);
		strncpy(pInsightData->url, tmp_url, sizeof(pInsightData->url)-1);
		strncat(pInsightData->url, EmailAddress, sizeof(pInsightData->url)-strlen(pInsightData->url)-1);
		shceduleType = GetDataExportTypeStr(DataExportscheduleType);
		strncat(pInsightData->url, shceduleType,sizeof(pInsightData->url)- strlen(pInsightData->url)-1);
		strncpy(pInsightData->inData, INSIGHT_DATA_FILE_URL, sizeof(pInsightData->inData)-1);
		pInsightData->inDataLength = 0;

		char *check = strstr(pInsightData->url, "https://");
		if (check) {
			pInsightData->httpsFlag = 1;
		}

		APP_LOG("UploadInsightDataFileStatus:",LOG_DEBUG, "Sending... %s", INSIGHT_DATA_FILE_URL);
		retVal = webAppSendData(pInsightSsnData, pInsightData, 2);

		if(retVal == ERROR_INVALID_FILE) {
			APP_LOG("UploadInsightDataFileStatus:", LOG_ERR, "File \'%s\' doesn't exists", INSIGHT_DATA_FILE_URL);
		}
		if(!strstr(pInsightData->outHeader, "200"))
		{
			APP_LOG("UploadInsightDataFileStatus", LOG_ERR, "Some error encountered in sending, errorCode %d", retVal);
			retVal = PLUGIN_FAILURE;
			goto on_return;
		}
		else if(retVal) {
			APP_LOG("UploadInsightDataFileStatus", LOG_ERR, "Some error encountered in send status to cloud  , errorCode %d", retVal);
			retVal = PLUGIN_FAILURE;
			goto on_return;
		}
	}

on_return:
	if (pInsightSsnData) webAppDestroySession (pInsightSsnData);
	if (mac_addr) {free(mac_addr); mac_addr = NULL;}
	if (pInsightData) 
	{
		if (pInsightData->outData) {free(pInsightData->outData); pInsightData->outData = NULL;}
		free(pInsightData); pInsightData = NULL;
	}
	if (assign) {free(assign); assign = NULL;}

	return retVal;	
}

#define MIN_EXPORT_SLEEP 5
#define MAX_EXPORT_RETRY_COUNT 3
void UploadInsightDataFile(void *arg)
{
	struct stat FileInfo;
 	char ExportTS[SIZE_32B];
	int ExportRetry=0;

	/*Checking for the existence of the InsightExportData files.*/
	if(stat(INSIGHT_DATA_FILE_URL, &FileInfo) == -1)
	{
		APP_LOG("UploadInsightDataFile:", LOG_ERR, "InsightExportData file doesn't exists");
		return;
	}

	if (getCurrentClientState() != STATE_CONNECTED)
	{
		APP_LOG("UploadInsightDataFile", LOG_DEBUG, "Exit, State Not Connected!");
	}
	else
	{
		APP_LOG("UploadInsightDataFile", LOG_DEBUG, "State Connected");
		while(ExportRetry <= MAX_EXPORT_RETRY_COUNT)
		{
		    if(UploadInsightDataFileStatus(arg) < 0) {
			APP_LOG("UploadInsightDataFile", LOG_DEBUG, "Upload Insight Data File status fail");
			ExportRetry++;
			pluginUsleep(MIN_EXPORT_SLEEP*1000000);
		    }
		    else
		    {
			memset(ExportTS, 0, sizeof(ExportTS));
			snprintf(ExportTS, sizeof(ExportTS), "%lu", GetUTCTime());
			SetBelkinParameter(DB_LAST_EXPORT_TS, ExportTS);
			APP_LOG("UploadInsightDataFile", LOG_DEBUG, "Upload Insight Data File status success, ExportTS: %s", ExportTS);
			break;
		    }
		}
	}
	APP_LOG("UploadInsightDataFile",LOG_DEBUG, "EXITING Upload Insight Data File");
	if((char*)arg) {free(arg); arg = NULL;}
	return;
}

int remoteAccessGetPluginDetails(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void* remoteInfo,char* transaction_id){
	int retVal = PLUGIN_SUCCESS;
	mxml_node_t *tree=NULL;
	mxml_node_t *find_node=NULL;
	char statusResp[MAX_DATA_LEN];
	char *sMac = NULL;
	char macAddress[MAX_MAC_LEN];
	char s8RefreshTime[SIZE_32B];
	int status=0x00;

	LockLED();
	status = GetCurBinaryState();
	UnlockLED();
	if (!xmlBuf) {
		retVal = PLUGIN_FAILURE;
		goto handler_exit1;
	}
	CHECK_PREV_TSX_ID(E_GETPLUGINDETAILS,transaction_id,retVal);
	tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);
	
if (tree){
  	APP_LOG("REMOTEACCESS", LOG_DEBUG, "XML String loaded is %s", xmlBuf);
		find_node = mxmlFindElement(tree, tree, "macAddress", NULL, NULL, MXML_DESCEND);
		if (find_node) {
  		//APP_LOG("REMOTEACCESS", LOG_DEBUG, "Mac Address value is %s", find_node->child->value.opaque);
			strncpy(macAddress, (find_node->child->value.opaque), sizeof(macAddress)-1);
			//Get mac address of the device
  		//APP_LOG("REMOTEACCESS", LOG_DEBUG, "Mac Address of plug-in is %s", GetMACAddress());
			sMac = utilsRemDelimitStr(GetMACAddress(), ":");
			if (!strncmp(macAddress, sMac, strlen(sMac))) {
				//Get plug-in details as well as device status
				//use gemtek apis to get plugin details
				//Create xml response
			    find_node = mxmlFindElement(tree, tree, "refreshTimer", NULL, NULL, MXML_DESCEND);
			    if (find_node) {
				    strncpy(s8RefreshTime, (find_node->child->value.opaque), sizeof(s8RefreshTime)-1);
				    //APP_LOG("REMOTEACCESS", LOG_DEBUG, "Setting PushToCloudTime(RefreshTime) %s", s8RefreshTime);
				    g_PushToCloudTime = atoi(s8RefreshTime);
				    SetBelkinParameter (PUSHTOCLOUDTIME, s8RefreshTime);
				    memset(statusResp, 0x0, MAX_DATA_LEN);
				    if (g_InitialMonthlyEstKWH){
					    snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><refreshTimer>%d</refreshTimer><applianceConsumption><status>%d</status><instantaneousPower>%u</instantaneousPower><todayTotalTimeOn>%u</todayTotalTimeOn><timeConnected>%u</timeConnected><stateChangeTS>%u</stateChangeTS><lastONFor>%u</lastONFor><avgPowerON>%u</avgPowerON><powerThreshold>%u</powerThreshold><todayTotalKWH>%u</todayTotalKWH><past14DaysKWH>%d</past14DaysKWH><past14TotalTime>%u</past14TotalTime></applianceConsumption></plugin>", \
							    sMac,g_PushToCloudTime,status,g_PowerNow,g_TodayONTimeTS,g_HrsConnected,g_StateChangeTS,g_ONFor,g_AvgPowerON,g_PowerThreshold,g_AccumulatedWattMinute,g_InitialMonthlyEstKWH,g_TotalONTime14Days);
				    }
				    else
				    {
					    snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><refreshTimer>%d</refreshTimer><applianceConsumption><status>%d</status><instantaneousPower>%u</instantaneousPower><todayTotalTimeOn>%u</todayTotalTimeOn><timeConnected>%u</timeConnected><stateChangeTS>%u</stateChangeTS><lastONFor>%u</lastONFor><avgPowerON>%u</avgPowerON><powerThreshold>%u</powerThreshold><todayTotalKWH>%u</todayTotalKWH><past14DaysKWH>%0.f</past14DaysKWH><past14TotalTime>%u</past14TotalTime></applianceConsumption></plugin>", \
							    sMac,g_PushToCloudTime,status,g_PowerNow,g_TodayONTimeTS,g_HrsConnected,g_StateChangeTS,g_ONFor,g_AvgPowerON,g_PowerThreshold,g_AccumulatedWattMinute,g_KWH14Days,g_TotalONTime14Days);
				    }

				APP_LOG("REMOTEACCESS", LOG_DEBUG, "Insight dev Details response created is %s", statusResp);
				//Send this response towards cloud synchronously using same data socket
	retVal=SendNatPkt(hndl,statusResp,remoteInfo,transaction_id,data_sock,E_GETPLUGINDETAILS);
				if (retVal < PLUGIN_SUCCESS) {
				APP_LOG("REMOTEACCESS", LOG_ERR, "Insight dev status response data sock send is failed %d", retVal);
					retVal = PLUGIN_FAILURE;
				}else {
  				APP_LOG("REMOTEACCESS", LOG_DEBUG, "Insight dev status response data sock total bytes send is %d", retVal);
					retVal = PLUGIN_SUCCESS;
					g_SendToCloud=1;
				}
			    
			    } else {
				APP_LOG("REMOTEACCESS", LOG_ERR, "Insight refreshTimer Tag Not Recieved", macAddress, sMac);
				retVal = PLUGIN_FAILURE;
			    }
			} else {
  			APP_LOG("REMOTEACCESS", LOG_ERR, "Mac received %s doesn't match with Insight Mac %s", macAddress, sMac);
				retVal = PLUGIN_FAILURE;
			}
			mxmlDelete(find_node);
		}else {
			APP_LOG("REMOTEACCESS", LOG_ERR, "Insight macAddress tag not present in xml payload");
			retVal = PLUGIN_FAILURE;
		}
		mxmlDelete(tree);
	}else{
  	APP_LOG("REMOTEACCESS", LOG_ERR, "XML String %s couldn't be loaded", xmlBuf);
		retVal = PLUGIN_FAILURE;
	}
handler_exit1:
	if (sMac) {free(sMac); sMac = NULL;}
	return retVal;
}

//API to parse Insight response coming from cloud to check If Push Flag is 1 or 0 to start stop sending Notification to cloud
void* parseInsightNotificationResp(void *sendNfResp, char **errcode) {
        
        char *snRespXml = NULL;
        mxml_node_t *tree;
        mxml_node_t *find_node;
	int s32PushFlag=0;
        
        snRespXml = (char*)sendNfResp;

        tree = mxmlLoadString(NULL, snRespXml, MXML_OPAQUE_CALLBACK);
        if (tree){
	    APP_LOG("REMOTEACCESS", LOG_DEBUG, "XML String Loaded is %s\n", snRespXml);
	    find_node = mxmlFindElement(tree, tree, "applianceConsumption", NULL, NULL, MXML_DESCEND);
	    if(find_node){
		    find_node = mxmlFindElement(tree, tree, "pushFlag", NULL, NULL, MXML_DESCEND);
		    if (find_node && find_node->child) {
			//APP_LOG("REMOTEACCESS", LOG_DEBUG, "The Push Flag set in the node is %s\n", find_node->child->value.opaque);
			s32PushFlag = atoi(find_node->child->value.opaque);
			//APP_LOG("REMOTEACCESS", LOG_DEBUG, "True Push Flag s32PushFlag is %d\n", s32PushFlag);
			if(s32PushFlag)
			{
			    g_SendToCloud = 1;		    
			    //APP_LOG("REMOTEACCESS", LOG_DEBUG, "True Push Flag is %d\n", g_SendToCloud);
    			}
			else
			{
			    g_SendToCloud = 0;		    
			    //APP_LOG("REMOTEACCESS", LOG_DEBUG, "False Push Flag is %d\n", g_SendToCloud);
			}
		    }
	    }else{
		    goto on_return;
	    }
	}else {
                goto on_return;
        }
        mxmlDelete(tree);
        return NULL;
on_return:
        mxmlDelete(tree);
        return NULL;
}

int remoteAccessSetPowerThreshold(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void* remoteInfo,char* transaction_id){ 
	int retVal = PLUGIN_SUCCESS;
	mxml_node_t *tree=NULL;
	mxml_node_t *find_node=NULL;
	char statusResp[MAX_DATA_LEN];
	char *sMac = NULL;
	char macAddress[MAX_MAC_LEN];
	char threshold[SIZE_32B];
	int s32DefaultThreshold=0;
	DataValues Values = {0,0,0,0,0};
        char s8Threshold[SIZE_32B];
        int s32Threshold = 0;

	if (!xmlBuf) {
		retVal = PLUGIN_FAILURE;
		goto handler_exit1;
	}
	CHECK_PREV_TSX_ID(E_SETPOWERTHRESHOLD,transaction_id,retVal);
	tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);
	
if (tree){
  	APP_LOG("REMOTEACCESS", LOG_DEBUG, "XML String loaded is %s", xmlBuf);
		find_node = mxmlFindElement(tree, tree, "macAddress", NULL, NULL, MXML_DESCEND);
		if (find_node) {
  		//APP_LOG("REMOTEACCESS", LOG_DEBUG, "Mac Address value is %s", find_node->child->value.opaque);
			strncpy(macAddress, (find_node->child->value.opaque), sizeof(macAddress)-1);
			//Get mac address of the device
  		//APP_LOG("REMOTEACCESS", LOG_DEBUG, "Mac Address of plug-in is %s", GetMACAddress());
			sMac = utilsRemDelimitStr(GetMACAddress(), ":");
			if (!strncmp(macAddress, sMac, strlen(sMac))) {
				//Get plug-in details as well as device status
				//use gemtek apis to get plugin details
				//Create xml response
			    find_node = mxmlFindElement(tree, tree, "defaultPowerThreshold", NULL, NULL, MXML_DESCEND);
			    if (find_node) {
				s32DefaultThreshold = atoi(find_node->child->value.opaque);
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "defaultPowerThreshold value is %d", s32DefaultThreshold);
			    }
			    find_node = mxmlFindElement(tree, tree, "powerThreshold", NULL, NULL, MXML_DESCEND);
			    if (find_node) {
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "PowerThreshold From App is %s", find_node->child->value.opaque);
				 strncpy(threshold, (find_node->child->value.opaque), sizeof(threshold)-1);
				 memset(statusResp, 0x0, MAX_DATA_LEN);
				 if(1 == s32DefaultThreshold)
				 {
					 //APP_LOG("REMOTEACCESS", LOG_DEBUG, "Setting PowerThreshold value to %s",DEFAULT_POWERTHRESHOLD);
					 SetBelkinParameter (POWERTHRESHOLD,DEFAULT_POWERTHRESHOLD);
					 g_PowerThreshold = atoi(DEFAULT_POWERTHRESHOLD);   
				 }
				 else if(2 == s32DefaultThreshold)
				 {
					 if(HAL_GetCurrentReadings(&Values) == 0)
					 {
						 APP_LOG("REMOTEACCESS", LOG_DEBUG, "Instantaneous Power Value: %d",Values.Watts);
						 memset(s8Threshold, 0, sizeof(s8Threshold));
						 s32Threshold = Values.Watts + 2000;
						 snprintf(s8Threshold, sizeof(s8Threshold), "%d", s32Threshold);
						 SetBelkinParameter(POWERTHRESHOLD, s8Threshold);
						 g_PowerThreshold = atoi(s8Threshold);  
					 }
					 else
						 APP_LOG("REMOTEACCESS", LOG_DEBUG, "Current Value Not coming From Daemon\n");
				 }
				 else
				 {
					 //APP_LOG("REMOTEACCESS", LOG_DEBUG, "Setting PowerThreshold value to %s",threshold);
					 SetBelkinParameter (POWERTHRESHOLD,threshold);
					 g_PowerThreshold = atoi(threshold);  
				 }
					
					
				    snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><applianceConsumption><powerThreshold>%d</powerThreshold></applianceConsumption></plugin>", \
				    sMac,g_PowerThreshold);

				    APP_LOG("REMOTEACCESS", LOG_DEBUG, "Insight SetpowerThreshold response created is %s", statusResp);
				    pMessage msg = 0x00;
				    msg = createMessage(UPNP_MESSAGE_PWRTHRES_IND, 0x00, 0x00);
				    SendMessage2App(msg);
				    //Threshold = atoi(tempchar);
				    //APP_LOG("UPNPDevice", LOG_DEBUG, "Sending Local Notification for Remote SetPowerThreshold: %d",Threshold);
				    //LocalPowerThresholdNotify(Threshold);
				//Send this response towards cloud synchronously using same data socket
	retVal=SendNatPkt(hndl,statusResp,remoteInfo,transaction_id,data_sock,E_SETPOWERTHRESHOLD);
			    }else {
				APP_LOG("REMOTEACCESS", LOG_ERR, "Insight powerThreshold tag not present in xml payload");
				retVal = PLUGIN_FAILURE;
			    }
			} else {
  			APP_LOG("REMOTEACCESS", LOG_ERR, "Mac received %s doesn't match with Insight Mac %s", macAddress, sMac);
				retVal = PLUGIN_FAILURE;
			}
			mxmlDelete(find_node);
		}else {
			APP_LOG("REMOTEACCESS", LOG_ERR, "Insight macAddress tag not present in xml payload");
			retVal = PLUGIN_FAILURE;
		}
		mxmlDelete(tree);
	}else{
  	APP_LOG("REMOTEACCESS", LOG_ERR, "XML String %s couldn't be loaded", xmlBuf);
		retVal = PLUGIN_FAILURE;
	}
handler_exit1:
	if (sMac) {free(sMac); sMac = NULL;}
	return retVal;
}

int remoteAccessGetPowerThreshold(void*hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void*remoteInfo,char*transaction_id) {
	int retVal = PLUGIN_SUCCESS;
	mxml_node_t *tree=NULL;
	mxml_node_t *find_node=NULL;
	char statusResp[MAX_DATA_LEN];
	char *sMac = NULL;
	char macAddress[MAX_MAC_LEN];
	int Threshold=0;

	if (!xmlBuf) {
		retVal = PLUGIN_FAILURE;
		goto handler_exit1;
	}
	CHECK_PREV_TSX_ID(E_GETPOWERTHRESHOLD,transaction_id,retVal);
	tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);
	
if (tree){
  	APP_LOG("REMOTEACCESS", LOG_DEBUG, "XML String loaded is %s", xmlBuf);
		find_node = mxmlFindElement(tree, tree, "macAddress", NULL, NULL, MXML_DESCEND);
		if (find_node) {
  		//APP_LOG("REMOTEACCESS", LOG_DEBUG, "Mac Address value is %s", find_node->child->value.opaque);
			strncpy(macAddress, (find_node->child->value.opaque), sizeof(macAddress)-1);
			//Get mac address of the device
  		//APP_LOG("REMOTEACCESS", LOG_DEBUG, "Mac Address of plug-in is %s", GetMACAddress());
			sMac = utilsRemDelimitStr(GetMACAddress(), ":");
			if (!strncmp(macAddress, sMac, strlen(sMac))) {
				//Create xml response

				    snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><applianceConsumption><powerThreshold>%d</powerThreshold></applianceConsumption></plugin>", \
				    sMac,g_PowerThreshold);

				    APP_LOG("REMOTEACCESS", LOG_DEBUG, "Insight GetpowerThreshold response created is %s", statusResp);
				   // APP_LOG("UPNPDevice", LOG_DEBUG, "Sending Local Notification for Remote SetPowerThreshold: %d",Threshold);
				//Send this response towards cloud synchronously using same data socket
	retVal=SendNatPkt(hndl,statusResp,remoteInfo,transaction_id,data_sock,E_GETPOWERTHRESHOLD);
			} else {
  			APP_LOG("REMOTEACCESS", LOG_ERR, "Get Threshold Mac received %s doesn't match with Insight Mac %s", macAddress, sMac);
				retVal = PLUGIN_FAILURE;
			}
			mxmlDelete(find_node);
		}else {
			APP_LOG("REMOTEACCESS", LOG_ERR, "Insight macAddress tag not present in xml payload");
			retVal = PLUGIN_FAILURE;
		}
		mxmlDelete(tree);
	}else{
  	APP_LOG("REMOTEACCESS", LOG_ERR, "XML String %s couldn't be loaded", xmlBuf);
		retVal = PLUGIN_FAILURE;
	}
handler_exit1:
	if (sMac) {free(sMac); sMac = NULL;}
	return retVal;
}

int remoteAccessSetClearDataUsage(void* hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void *remoteInfo,char* transaction_id) {
	int retVal = PLUGIN_SUCCESS;
	mxml_node_t *tree=NULL;
	mxml_node_t *find_node=NULL;
	char statusResp[MAX_DATA_LEN];
	char *sMac = NULL;
	char macAddress[MAX_MAC_LEN];

	if (!xmlBuf) {
		retVal = PLUGIN_FAILURE;
		goto handler_exit1;
	}
	CHECK_PREV_TSX_ID(E_SETCLEARDATAUSAGE,transaction_id,retVal);
	tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);
	
if (tree){
  	APP_LOG("REMOTEACCESS", LOG_DEBUG, "XML String loaded is %s", xmlBuf);
		find_node = mxmlFindElement(tree, tree, "macAddress", NULL, NULL, MXML_DESCEND);
		if (find_node) {
  		//APP_LOG("REMOTEACCESS", LOG_DEBUG, "Mac Address value is %s", find_node->child->value.opaque);
			strncpy(macAddress, (find_node->child->value.opaque), sizeof(macAddress)-1);
			//Get mac address of the device
  		//APP_LOG("REMOTEACCESS", LOG_DEBUG, "Mac Address of plug-in is %s", GetMACAddress());
			sMac = utilsRemDelimitStr(GetMACAddress(), ":");
			if (!strncmp(macAddress, sMac, strlen(sMac))) {
				//Create xml response
				    ExecuteReset(0x04);
				    snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><status>1</status></plugin>",  sMac);

						APP_LOG("REMOTEACCESS", LOG_DEBUG, "Insight SetClearDataUsage response created is %s", statusResp);
						//Send this response towards cloud synchronously using same data socket
						retVal=SendNatPkt(hndl,statusResp,remoteInfo,transaction_id,data_sock,E_SETCLEARDATAUSAGE);
				} else {
						APP_LOG("REMOTEACCESS", LOG_ERR, "SetClearDataUsage Mac received %s doesn't match with Insight Mac %s", macAddress, sMac);
						retVal = PLUGIN_FAILURE;
				}
				mxmlDelete(find_node);
		}else {
			APP_LOG("REMOTEACCESS", LOG_ERR, "SetClearDataUsage Insight macAddress tag not present in xml payload");
			retVal = PLUGIN_FAILURE;
		}
		mxmlDelete(tree);
	}else{
  	APP_LOG("REMOTEACCESS", LOG_ERR, "SetClearDataUsage XML String %s couldn't be loaded", xmlBuf);
		retVal = PLUGIN_FAILURE;
	}
handler_exit1:
	if (sMac) {free(sMac); sMac = NULL;}
	return retVal;
}

int remoteAccessGetDataExportInfo(void*hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void *remoteInfo,char* transaction_id) {
	int retVal = PLUGIN_SUCCESS;
	mxml_node_t *tree=NULL;
	mxml_node_t *find_node=NULL;
	char statusResp[MAX_DATA_LEN];
	char *sMac = NULL;
	char macAddress[MAX_MAC_LEN];
	char * LastDataExportTS = NULL;
	char * DataExportType = NULL;

	if (!xmlBuf) {
		retVal = PLUGIN_FAILURE;
		goto handler_exit1;
	}
	CHECK_PREV_TSX_ID(E_GETDATAEXPORTINFO,transaction_id,retVal);
	tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

	if (tree){
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "XML String loaded is %s", xmlBuf);
		find_node = mxmlFindElement(tree, tree, "macAddress", NULL, NULL, MXML_DESCEND);
		if (find_node) {
			strncpy(macAddress, (find_node->child->value.opaque), sizeof(macAddress)-1);
			sMac = utilsRemDelimitStr(GetMACAddress(), ":");
			if (!strncmp(macAddress, sMac, strlen(sMac))) {
				//Create xml response
				LastDataExportTS = GetBelkinParameter(DB_LAST_EXPORT_TS);
				DataExportType = GetBelkinParameter(DATA_EXPORT_TYPE);
				snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><recipientId>%lu</recipientId><macAddress>%s</macAddress><applianceConsumption><lastDataExportTs>%lu</lastDataExportTs><emailAddress>%s</emailAddress><dataExportType>%d</dataExportType><status>1</status></applianceConsumption></plugin>", strtol(g_szPluginCloudId, NULL, 0), sMac, atol(LastDataExportTS), g_s8EmailAddress, atoi(DataExportType));

				APP_LOG("REMOTEACCESS", LOG_DEBUG, "Insight remoteAccessGetDataExportInfo response created is %s", statusResp);
	retVal=SendNatPkt(hndl,statusResp,remoteInfo,transaction_id,data_sock,E_GETDATAEXPORTINFO);
			} else {
				APP_LOG("REMOTEACCESS", LOG_ERR, "Mac received %s doesn't match with Insight Mac %s", macAddress, sMac);
				retVal = PLUGIN_FAILURE;
			}
			mxmlDelete(find_node);
		}else {
			APP_LOG("REMOTEACCESS", LOG_ERR, "Insight macAddress tag not present in xml payload");
			retVal = PLUGIN_FAILURE;
		}
		mxmlDelete(tree);
	}else{
		APP_LOG("REMOTEACCESS", LOG_ERR, "XML String %s couldn't be loaded", xmlBuf);
		retVal = PLUGIN_FAILURE;
	}
handler_exit1:
	if (sMac) {free(sMac); sMac = NULL;}
	return retVal;
}

int remoteAccessScheduleDataExport(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void*remoteInfo,char*transaction_id)
{
	int retVal = PLUGIN_SUCCESS;
	mxml_node_t *tree=NULL;
	mxml_node_t *find_node=NULL;
	char statusResp[MAX_DATA_LEN];

	char *sMac = NULL;
	char macAddress[MAX_MAC_LEN];
	char szEmailAddress[SIZE_256B] = {0};
	char szDataExportType[SIZE_4B] ={0};


	if (!xmlBuf) {
		retVal = PLUGIN_FAILURE;
		goto handler_exit1;
	}
	CHECK_PREV_TSX_ID(E_SCHEDULEDATAEXPORT,transaction_id,retVal);
	tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

	if (tree){
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "XML String loaded is %s", xmlBuf);
		find_node = mxmlFindElement(tree, tree, "macAddress", NULL, NULL, MXML_DESCEND);
		if (find_node) {
			strncpy(macAddress, (find_node->child->value.opaque), sizeof(macAddress)-1);
			sMac = utilsRemDelimitStr(GetMACAddress(), ":");
			if (!strncmp(macAddress, sMac, strlen(sMac))) {
				find_node = mxmlFindElement(tree, tree, "emailAddress", NULL, NULL, MXML_DESCEND);
				if (find_node){
					if(find_node->child)
						strncpy(szEmailAddress, find_node->child->value.opaque, sizeof(szEmailAddress)-1);

					find_node = mxmlFindElement(tree, tree, "dataExportType", NULL, NULL, MXML_DESCEND);
					if (find_node) {
					        strncpy(szDataExportType, find_node->child->value.opaque, sizeof(szDataExportType)-1);
					
					ExecuteInsightDataExport(szEmailAddress,szDataExportType);

					snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><status>1</status></plugin>",sMac);

	retVal=SendNatPkt(hndl,statusResp,remoteInfo,transaction_id,data_sock,E_SCHEDULEDATAEXPORT);
				}else {
					APP_LOG("REMOTEACCESS", LOG_ERR, "DataExport tag not present in xml payload");
					retVal = PLUGIN_FAILURE;
				}
				}else {
					APP_LOG("REMOTEACCESS", LOG_ERR, "EmailAddress tag not present in xml payload");
					retVal = PLUGIN_FAILURE;
				}
			} else {
				APP_LOG("REMOTEACCESS", LOG_ERR, "Mac received %s doesn't match with Insight Mac %s", macAddress, sMac);
				retVal = PLUGIN_FAILURE;
			}
			mxmlDelete(find_node);
		}else {
			APP_LOG("REMOTEACCESS", LOG_ERR, "Insight macAddress tag not present in xml payload");
			retVal = PLUGIN_FAILURE;
		}
		mxmlDelete(tree);
	}else{
		APP_LOG("REMOTEACCESS", LOG_ERR, "XML String %s couldn't be loaded", xmlBuf);
		retVal = PLUGIN_FAILURE;
	}
handler_exit1:
	if (sMac) {free(sMac); sMac = NULL;}
	return retVal;
}

int remoteAccessSetInsightHomeSettings(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void* remoteInfo,char* transaction_id)
{
	int retVal = PLUGIN_SUCCESS;
	mxml_node_t *tree=NULL;
	mxml_node_t *find_node=NULL,*find_node1=NULL,*find_node2=NULL;
	char statusResp[MAX_DATA_LEN];

	char *sMac = NULL;
	char macAddress[MAX_MAC_LEN];
	char NewVersion[SIZE_16B];
	int version = 0,cost = 0;
	char DeviceVersion[SIZE_16B];
	char PerUnitCost[SIZE_16B];


	if (!xmlBuf) {
		retVal = PLUGIN_FAILURE;
		goto handler_exit1;
	}
	CHECK_PREV_TSX_ID(E_SETINSIGHTHOMESETTINGS,transaction_id,retVal);
	tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

	if (tree){
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "XML String loaded is %s", xmlBuf);
		find_node = mxmlFindElement(tree, tree, "macAddress", NULL, NULL, MXML_DESCEND);
		if (find_node) {
			strncpy(macAddress, (find_node->child->value.opaque), sizeof(macAddress)-1);
			sMac = utilsRemDelimitStr(GetMACAddress(), ":");
			if (!strncmp(macAddress, sMac, strlen(sMac)))
			{
				find_node1 = mxmlFindElement(tree, tree, "energyPerUnitCost", NULL, NULL, MXML_DESCEND);
				find_node2 = mxmlFindElement(tree, tree, "currency", NULL, NULL, MXML_DESCEND);

				if (find_node1 && find_node2)
				{
					cost = atoi(find_node1->child->value.opaque);
					if(cost)
					{
						APP_LOG("REMOTEACCESS", LOG_DEBUG, "Update cost Version");
						SetBelkinParameter(ENERGYPERUNITCOST,find_node1->child->value.opaque);
						g_perUnitCost = atoi(find_node1->child->value.opaque); 
						SetBelkinParameter(CURRENCY,find_node2->child->value.opaque);
                        g_Currency = atoi(find_node2->child->value.opaque);
					}
					else//reset to default
					{
						APP_LOG("REMOTEACCESS", LOG_DEBUG, "Reset cost Version");
						SetBelkinParameter(ENERGYPERUNITCOST,DEFAULT_ENERGYPERUNITCOST);
						g_perUnitCost = atoi(DEFAULT_ENERGYPERUNITCOST); 
						SetBelkinParameter(CURRENCY,DEFAULT_CURRENCY);
                        g_Currency = atoi(DEFAULT_CURRENCY);
					}

					snprintf (DeviceVersion,  sizeof(DeviceVersion), "%d", g_PerUnitCostVer);
					memset(NewVersion, 0x0, sizeof(NewVersion));
					snprintf(NewVersion, sizeof(NewVersion), "%d", (g_PerUnitCostVer+1));
					SetBelkinParameter(ENERGYPERUNITCOSTVERSION,NewVersion); 
					g_PerUnitCostVer++;

					g_InsightHomeSettingsSend = 1;

					snprintf (PerUnitCost,  sizeof(PerUnitCost), "%d", g_perUnitCost);

					//send itc msg for local notification
					SendHomeSettingChangeMsg();

					snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><homeSettings><homeId>%s</homeId><homeSettingsVersion>%s</homeSettingsVersion><energyPerUnitCost>%s</energyPerUnitCost><currency>%d</currency><status>1</status></homeSettings></plugin>",sMac,g_szHomeId,DeviceVersion,PerUnitCost,g_Currency);

					retVal=SendNatPkt(hndl,statusResp,remoteInfo,transaction_id,data_sock,E_SETINSIGHTHOMESETTINGS);
				}else {
					APP_LOG("REMOTEACCESS", LOG_ERR, "tag not present in xml payload");
					retVal = PLUGIN_FAILURE;
				}
			} else {
				APP_LOG("REMOTEACCESS", LOG_ERR, "Mac received %s doesn't match with Insight Mac %s", macAddress, sMac);
				retVal = PLUGIN_FAILURE;
			}
			mxmlDelete(find_node);
			mxmlDelete(find_node1);
			mxmlDelete(find_node2);
		}else {
			APP_LOG("REMOTEACCESS", LOG_ERR, "Insight macAddress tag not present in xml payload");
			retVal = PLUGIN_FAILURE;
		}
		mxmlDelete(tree);
	}else{
		APP_LOG("REMOTEACCESS", LOG_ERR, "XML String %s couldn't be loaded", xmlBuf);
		retVal = PLUGIN_FAILURE;
	}
handler_exit1:
	if (sMac) {free(sMac); sMac = NULL;}
	return retVal;
}

void  GetCurrencyString(int Code,char *Str){

    switch(Code)
    {
	case 1:
	case 2:
	case 8:
	case 9:
	case 11:
	case 14:
	case 22:
	    strncpy(Str,"$",SIZE_16B);
	break;
	case 3:
	    strncpy(Str,"Pound",SIZE_16B);
	break;
	case 4:
	case 5:
	case 6:
	case 16:
	case 17:
	case 18:
	case 20:
	case 21:
	    strncpy(Str,"€", SIZE_16B);
	break;
	case 7:
	    strncpy(Str,"Peso/Ruble",SIZE_16B);
	break;
	case 10:
	case 12:
	    strncpy(Str,"¥", SIZE_16B);
	break;
	case 13:
	    strncpy(Str,"Won", SIZE_16B);
	break;
	case 15:
	    strncpy(Str,"Baht", SIZE_16B);
	break;
	case 19:
	    strncpy(Str,"kr", SIZE_16B);
	break;
	case 23:
	    strncpy(Str,"RM", SIZE_16B);
	break;
	default:
	    strncpy(Str,"$", SIZE_16B);
	break;
    }
}
void  GetEventCurrencyString(int Code,char *Str){

    switch(Code)
    {
	case 1:
	    strncpy(Str,"USD", sizeof(Str)-1);
	break;
	case 2:
	    strncpy(Str,"CAD", sizeof(Str)-1);
	break;
	case 3:
	    strncpy(Str,"GBP", sizeof(Str)-1);
	break;
	case 4:
	case 5:
	case 6:
	    strncpy(Str,"EUR", sizeof(Str)-1);
	break;
	case 7:
	    strncpy(Str,"RUB", sizeof(Str)-1);
	break;
	case 8:
	    strncpy(Str,"AUD", sizeof(Str)-1);
	break;
	case 9:
	    strncpy(Str,"NZD", sizeof(Str)-1);
	break;
	case 10:
	    strncpy(Str,"JPY", sizeof(Str)-1);
	break;
	case 11:
	    strncpy(Str,"TWD", sizeof(Str)-1);
	break;
	case 12:
	    strncpy(Str,"CNY", sizeof(Str)-1);
	break;
	case 13:
	    strncpy(Str,"KRW", sizeof(Str)-1);
	break;
	case 14:
	    strncpy(Str,"HKD", sizeof(Str)-1);
	break;
	case 15:
	    strncpy(Str,"THB", sizeof(Str)-1);
	break;
	case 16:
	    strncpy(Str,"EUR", sizeof(Str)-1);
	break;
	case 17:
	    strncpy(Str,"EUR", sizeof(Str)-1);
	break;
	case 18:
	    strncpy(Str,"EUR", sizeof(Str)-1);
	break;
	case 19:
	    strncpy(Str,"NOK", sizeof(Str)-1);
	break;
	case 20:
	    strncpy(Str,"EUR", sizeof(Str)-1);
	break;
	case 21:
	    strncpy(Str,"EUR", sizeof(Str)-1);
	break;
	case 22:
	    strncpy(Str,"SGD", sizeof(Str)-1);
	break;
	case 23:
	    strncpy(Str,"MYR", sizeof(Str)-1);
	break;
	default:
	    strncpy(Str,"USD", sizeof(Str)-1);
	break;
    }
}
int sendRemoteInsightEventParams (void) {
	char httpPostStatus[MAX_STATUS_BUF];
	int retVal = PLUGIN_SUCCESS;
	char *pcode = NULL;
	authSign *Insightassign = NULL;
	UserAppData *pInsightUsrAppData = NULL;
	UserAppSessionData *pInsightUsrAppSsnData = NULL;
	char tmp_url[MAX_FILE_LINE];
	char YesterdayTimebufer[SIZE_128B];
	char Timebufer[SIZE_128B];
	char HappenedAt[SIZE_128B];
	char CurrencyStr[SIZE_32B];
	struct tm ts;
	time_t TimeNow;
	char EnergyCost[SIZE_16B];
	
	memset(EnergyCost, 0x00, sizeof(EnergyCost));

    	snprintf (EnergyCost,  sizeof(EnergyCost), "%d",g_perUnitCost);

	char *TimeZone = GetBelkinParameter(SYNCTIME_LASTTIMEZONE);
	unsigned int ONHr=0,ONMin=0,SBYHr=0,SBYMin=0;
	unsigned int YesONHr=0,YesONMin=0,YesSBYHr=0,YesSBYMin=0;

	memset(CurrencyStr, 0x0, sizeof(CurrencyStr));
	GetEventCurrencyString(g_Currency, CurrencyStr);
	APP_LOG("REMOTEACCESS", LOG_ERR, "\n Currency String: %s\n",CurrencyStr);
	/* loop until we find Internet connected */	
	while(1)
	{
	    if (getCurrentClientState() == STATE_CONNECTED) {
		break;
	    }
	    pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);  //30 sec
	}

	Insightassign = createAuthSignature(g_szWiFiMacAddress, g_szSerialNo, g_szPluginPrivatekey);
	if (!Insightassign) {
		APP_LOG("REMOTEACCESS", LOG_ERR, "\n Signature Structure returned NULL\n");
		retVal = PLUGIN_FAILURE;
		goto on_return;
	}

	pInsightUsrAppData = (UserAppData *)MALLOC(sizeof(UserAppData));

	ts = *localtime(&g_YesterdayTS);
	memset(YesterdayTimebufer, 0x0, sizeof(YesterdayTimebufer));
	strftime(YesterdayTimebufer, sizeof(YesterdayTimebufer), "%Y/%m/%d", &ts);
	APP_LOG("Insight Send Periodic Event:", LOG_DEBUG, "Yesterday's Time in YesterdayTimebufer: %s", YesterdayTimebufer);
	TimeNow = time(NULL);
	ts = *localtime(&TimeNow);
	memset(Timebufer, 0x0, sizeof(Timebufer));
	strftime(Timebufer, sizeof(Timebufer), "%Y/%m/%d", &ts);
	APP_LOG("Insight Send Periodic Event:", LOG_DEBUG, " Time Now in Timebufer: %s", Timebufer);
	time_t UTCcurTime;
	UTCcurTime = GetUTCTime();
	ts = *localtime(&UTCcurTime);
	memset(HappenedAt, 0x0, sizeof(HappenedAt));
	strftime(HappenedAt, sizeof(HappenedAt), "%FT%TZ", &ts);
	APP_LOG("Insight Send Periodic Event:", LOG_DEBUG, " Happened At: %s", HappenedAt);
	APP_LOG("Insight Send Periodic Event:", LOG_DEBUG, " TimeZone: %s", TimeZone);
	if(g_TodayONTimeTS != 0)
	{
	    ONHr =  g_TodayONTimeTS/3600;
	    ONMin= (g_TodayONTimeTS%3600);
	    if(ONMin != 0)
		ONMin = ONMin/60;
	    else
		ONMin=0;
	}
	if(g_TodaySBYTime != 0)
	{
	    SBYHr = g_TodaySBYTime/3600;
	    SBYMin = (g_TodaySBYTime%3600);
	    if(SBYMin != 0)
		SBYMin = SBYMin/60;
	    else
		SBYMin=0;
	}
	if(g_YesterdayONTime != 0)
	{
	    YesONHr =  g_YesterdayONTime/3600;
	    YesONMin= (g_YesterdayONTime%3600);
	    if(YesONMin != 0)
		YesONMin = YesONMin/60;
	    else
		YesONMin=0;
	}
	if(g_YesterdaySBYTime != 0)
	{
	    YesSBYHr = g_YesterdaySBYTime/3600;
	    YesSBYMin = (g_YesterdaySBYTime%3600);
	    if(YesSBYMin != 0)
		YesSBYMin = YesSBYMin/60;
	    else
		YesSBYMin=0;
	}

	while(1)
	{
	    memset(httpPostStatus, 0x0, MAX_STATUS_BUF);
	    snprintf(httpPostStatus, sizeof(httpPostStatus), "<?xml version=\"1.0\" encoding=\"UTF-8\"?><event xmlns=\"http://schemas.cisco.com/homenetworking/2011\"><network><networkId>%s</networkId></network><device><deviceId>%s</deviceId><password>%s</password></device><eventType>WEMO_INSIGHT_DATA</eventType><happenedAt>%s</happenedAt><payload><![CDATA[<insightUsageSummary><homeId>%s</homeId><macAddress>%s</macAddress><timeZoneOffset>%s</timeZoneOffset><energyPerUnitCost>%.5f</energyPerUnitCost><currency>%s</currency><estimateMonthlyCost>%.5f</estimateMonthlyCost><applianceConsumptions><applianceConsumption><date>%s</date><totalTimeOn>%d:%d</totalTimeOn><totalTimeStandBy>%d:%d</totalTimeStandBy><totalKWHOn>%.5f</totalKWHOn><totalKWHStandBy>%5f</totalKWHStandBy></applianceConsumption><applianceConsumption><date>%s</date><totalTimeOn>%d:%d</totalTimeOn><totalTimeStandBy>%d:%d</totalTimeStandBy><totalKWHOn>%.5f</totalKWHOn><totalKWHStandBy>%.5f</totalKWHStandBy></applianceConsumption></applianceConsumptions></insightUsageSummary>]]></payload></event>",\
		    g_szHomeId,g_szWiFiMacAddress,Insightassign->signature,HappenedAt,g_szHomeId,g_szWiFiMacAddress,TimeZone,(atoi(EnergyCost))/1000.00,CurrencyStr,((g_KWH14Days/(1000*1000*60.0))/(g_HrsConnected/3600.0))*730*((atoi(EnergyCost))/1000.0),YesterdayTimebufer,YesONHr,YesONMin,YesSBYHr,YesSBYMin,g_YesterdayKWHON/(1000*60000.0),g_YesterdayKWHSBY/(1000*60000.0),Timebufer,ONHr,ONMin,SBYHr,SBYMin,g_TodayKWHON/(1000*60000.0),g_TodayKWHSBY/(1000*60000.0));
	    APP_LOG("REMOTEACCESS", LOG_HIDE, "len=%d httpPostStatus=%s \n",strlen(httpPostStatus),httpPostStatus);
	    //printf("\n%s\n",httpPostStatus);
	    memset(pInsightUsrAppData, 0x0, sizeof(UserAppData));
	    pInsightUsrAppSsnData = webAppCreateSession(0);
	    memset(tmp_url,0,sizeof(tmp_url));
	    snprintf(tmp_url, sizeof(tmp_url), "https://%s/event-service/rest/events",LINK_DOMAIN_NM);
	    strncpy(pInsightUsrAppData->url, tmp_url,sizeof(pInsightUsrAppData->url)-1);
	    strncpy(pInsightUsrAppData->keyVal[0].key, "X-Linksys-Client-Type-Id",sizeof(pInsightUsrAppData->keyVal[0].key)-1);   
	    strncpy(pInsightUsrAppData->keyVal[0].value, "AA296AC6-61D1-4D3F-83FA-96D7486A09CF",sizeof(pInsightUsrAppData->keyVal[0].value)-1);   
	    strncpy(pInsightUsrAppData->keyVal[1].key, "Content-Type",sizeof(pInsightUsrAppData->keyVal[1].key)-1);   
	    strncpy(pInsightUsrAppData->keyVal[1].value, "application/xml",sizeof(pInsightUsrAppData->keyVal[1].value)-1);   
	    strncpy(pInsightUsrAppData->keyVal[2].key, "Accept",sizeof(pInsightUsrAppData->keyVal[2].key)-1);   
	    strncpy(pInsightUsrAppData->keyVal[2].value, "application/xml",sizeof(pInsightUsrAppData->keyVal[2].value)-1);   
	    strncpy( pInsightUsrAppData->keyVal[3].key, "X-Belkin-Client-Type-Id", sizeof(pInsightUsrAppData->keyVal[3].key)-1);   
	    strncpy( pInsightUsrAppData->keyVal[3].value, g_szClientType, sizeof(pInsightUsrAppData->keyVal[3].value)-1);   
	    pInsightUsrAppData->keyValLen = 4;

	    strncpy(pInsightUsrAppData->inData, httpPostStatus,sizeof(pInsightUsrAppData->inData)-1);
	    pInsightUsrAppData->inDataLength = strlen(httpPostStatus);
	    char *check = strstr(pInsightUsrAppData->url, "https://");
	    if (check) {
		pInsightUsrAppData->httpsFlag = 1;
	    }
	    pInsightUsrAppData->disableFlag = 1;    

	    retVal = webAppSendData( pInsightUsrAppSsnData, pInsightUsrAppData, 1);  
	    if (retVal)
	    {
		APP_LOG("REMOTEACCESS", LOG_ERR, "\n Some error encountered in send status to cloud  , errorCode %d \n", retVal);
		retVal = PLUGIN_FAILURE;
		break;
	    }

	    if( (strstr(pInsightUsrAppData->outHeader, "500")) || (strstr(pInsightUsrAppData->outHeader, "503")) ){
		APP_LOG("REMOTEACESS", LOG_DEBUG, "Some error encountered: Cloud is not reachable");
		retVal = PLUGIN_FAILURE;
		if (pInsightUsrAppSsnData) webAppDestroySession ( pInsightUsrAppSsnData );
		pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);  //30 sec
		continue;
	    }else if(strstr(pInsightUsrAppData->outHeader, "200")){
		APP_LOG("REMOTEACESS", LOG_DEBUG, "send status to cloud success");
		break;
	    }else if(strstr(pInsightUsrAppData->outHeader, "403")){
		retVal = PLUGIN_FAILURE;
		parseSendNotificationResp(pInsightUsrAppData->outData, &pcode);
		if ((pcode)){
		    APP_LOG("REMOTEACESS", LOG_DEBUG, "########AUTH FAILURE (403) : %s: Not sending this Event ########", pcode);
				if (!strncmp(pcode, "ERR_002", strlen("ERR_002"))) {
						CheckUpdateRemoteFailureCount();
				}
		    if (pcode) {free(pcode); pcode= NULL;}
		}
		//CheckUpdateRemoteFailureCount();
		break;
	    }else{
		APP_LOG("REMOTEACESS", LOG_DEBUG, "Some error encountered: Error response from cloud");
		writeLogs(CRITICAL, "RemoteNotifications", "Cloud not reachable, unknown error");
		retVal = PLUGIN_FAILURE;
		break;
	    }

	}
on_return:

	if (pInsightUsrAppSsnData) webAppDestroySession ( pInsightUsrAppSsnData );
	if (Insightassign) {free(Insightassign); Insightassign = NULL;};
	if (pInsightUsrAppData) {
	    if (pInsightUsrAppData->outData) {free(pInsightUsrAppData->outData); pInsightUsrAppData->outData = NULL;}
	    free(pInsightUsrAppData); pInsightUsrAppData = NULL;
	}
	return retVal;
}


#endif
