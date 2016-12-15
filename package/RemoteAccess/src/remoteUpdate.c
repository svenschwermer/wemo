/***************************************************************************
 *
 *
 * remoteUpdate.c
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
#include <dirent.h>

#include "types.h"
#include "defines.h"
#include "curl/curl.h"
#include "httpsWrapper.h"
#include <ithread.h>
#include "natClient.h"
#include "remoteAccess.h"
#include "controlledevice.h"
#include "remote_event.h"
#include "ledLightRemoteAccess.h"
#include "logger.h"
#include "controlledevice.h"
#include "sigGen.h"
#include "gpio.h"
#include "osUtils.h"
#include "utils.h"
#include "wifiSetup.h"
#include "wifiHndlr.h"
#include "mxml.h"
#include "rule.h"
#include "plugin_ctrlpoint.h"
#ifdef SIMULATED_OCCUPANCY
#include "simulatedOccupancy.h"
#endif
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

#ifdef __NFTOCLOUD__
# define MAX_NAT_STATUS_COUNT 3
#endif

#define SENDNF_TIMESYNC_AUTH_FAIL_ERR_CODE "ERR_006"
#define MAX_AUTH_FAILURE_RETRY_ATTEMPTS		5

#define SEND_JARDEN_APNS_NOTIFICATION		54

#ifdef PRODUCT_WeMo_Insight
char g_InstantPowerSend = 0;
unsigned int g_EventEnable =0;
int g_SendEvent =0;
unsigned int g_EventDuration =0;
#endif

#ifdef WeMo_SMART_SETUP_V2
pthread_t customizedstate_thread = -1;
pthread_attr_t customizedstate_attr;
extern int g_customizedState;
#endif

extern int g_uploadDataUsage;
extern unsigned short gpluginRemAccessEnable;

extern char g_szFriendlyName[];
extern char g_szHomeId[SIZE_20B];
extern char g_szPluginPrivatekey[MAX_PKEY_LEN];
extern int g_eDeviceType;
extern char  g_szUDN_1[SIZE_128B];
extern char g_szRestoreState[MAX_RES_LEN];
extern char g_szFirmwareVersion[SIZE_64B];
extern char g_szSerialNo[SIZE_64B];
extern char g_szWiFiMacAddress[SIZE_64B];
extern unsigned int g_configChange;
#ifdef PRODUCT_WeMo_Smart
extern char* g_attrName;
extern char* g_prevValue;
extern char* g_curValue;

extern char g_remotebuff[1024];
#endif

int gpluginPrevStatus = 0;
int gpluginPrevStatusTS = 0;
ithread_mutex_t gRemoteUpdThdMutex;

static int gSendNotifyHealthPunch=0;
extern ithread_t sendremoteupdstatus_thread;
#define SEND_REM_UPD_STAT_TH_MON_TIMEOUT    120	//secs

static int gpluginStatus = 0;
pthread_mutex_t gTsLock;
pthread_mutex_t gRemoteFailCntLock;
pthread_mutex_t gStatusLock;
pthread_mutex_t g_devConfig_mutex1;
pthread_cond_t g_devConfig_cond1;
extern int gpluginStatusTS;
extern int gSignalStrength;
int sendConfigChangeDevStatus();
#if defined(LONG_PRESS_SUPPORTED)
extern int gLongPressTriggered;
#endif
static UserAppSessionData *pUsrAppSsnData = NULL;
static UserAppData *pUsrAppData = NULL;
//static authSign *assign = NULL;

char gNotificationBuffer[MAX_STATUS_BUF] = {'\0',};
extern int gNTPTimeSet;

extern pthread_t dbFile_thread;

int g_urlDataUsage = 0; //Flag set to 1 if the xml prepared is to be uploaded to the datausage url. It is reset in sendRemoteAccessUpdateDevStatus

void initRemoteUpdSync() {
		ithread_mutex_init(&gRemoteUpdThdMutex, 0);
		APP_LOG("REMOTEACCESS",LOG_ERR, "********** Remote Status Update Notification Lock Init*************");
}

void LockRemoteUpdSync() {
		ithread_mutex_lock(&gRemoteUpdThdMutex);
		APP_LOG("REMOTEACCESS",LOG_ERR, "********** Remote Status Update Notification Lock*************");
}

void UnLockRemoteUpdSync() {
		ithread_mutex_unlock(&gRemoteUpdThdMutex);
		APP_LOG("REMOTEACCESS",LOG_ERR, "********** Remote Status Update Notification UnLock*************");
}

void initRemoteStatusTS()
{
		osUtilsCreateLock(&gTsLock);
		osUtilsCreateLock(&gStatusLock);
		osUtilsCreateLock(&gRemoteFailCntLock);
}

void initdevConfiglock()
{
		pthread_cond_init (&g_devConfig_cond1, NULL);
		pthread_mutex_init(&g_devConfig_mutex1, NULL);
}

void setPluginStatusTS()
{
		osUtilsGetLock(&gTsLock);
		gpluginStatusTS = (int)GetUTCTime();
		osUtilsReleaseLock(&gTsLock);
		APP_LOG("REMOTEACCESS",LOG_DEBUG, "gpluginStatusTS updated: %d", gpluginStatusTS);

}

void setPluginStatus(int status)
{
		osUtilsGetLock(&gStatusLock);
		gpluginStatus = status;
		osUtilsReleaseLock(&gStatusLock);
		APP_LOG("REMOTEACCESS",LOG_DEBUG, "gpluginStatus updated: %d", gpluginStatus);
}

int getPluginStatusTS(void)
{
		int ts;
		osUtilsGetLock(&gTsLock);
		ts = gpluginStatusTS;
		osUtilsReleaseLock(&gTsLock);
		return ts;
}

int getPluginStatus(void)
{
		int status;
		osUtilsGetLock(&gStatusLock);
		status = gpluginStatus;
		osUtilsReleaseLock(&gStatusLock);
		return status;
}
void* parseSendNotificationResp(void *sendNfResp, char **errcode) {

		char *snRespXml = NULL;
		mxml_node_t *tree;
		mxml_node_t *find_node;
		int fail_return = 0;
		char *pcode = NULL;

		if(!sendNfResp)
				return NULL;

		snRespXml = (char*)sendNfResp;

		tree = mxmlLoadString(NULL, snRespXml, MXML_OPAQUE_CALLBACK);
		if (tree){
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "XML String Loaded is %s\n", snRespXml);
				find_node = mxmlFindElement(tree, tree, "Error", NULL, NULL, MXML_DESCEND);
				if(find_node){
						find_node = mxmlFindElement(tree, tree, "code", NULL, NULL, MXML_DESCEND);
						if (find_node && find_node->child) {
								APP_LOG("REMOTEACCESS", LOG_DEBUG, "The Error Code set in the node is %s\n", find_node->child->value.opaque);
								fail_return = 1;
								pcode = (char*)CALLOC(1, SIZE_256B);
								if (fail_return) {
										if (pcode) {
												strncpy(pcode, (find_node->child->value.opaque), SIZE_256B-1);
												*errcode = pcode;
												APP_LOG("REMOTEACCESS", LOG_DEBUG, "The error code set from cloud is %s\n", *errcode);
										}
								}
						}
						if (fail_return) {
								goto on_return;
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


#ifdef PRODUCT_WeMo_NetCam
/* NetCam needs Login inforamtion, but the cloud is too inflexible to add
   a new tag. We have to "pretend" the friendly name contains also the login
   information, and NetCam app will know to parse it out */
extern char g_szNetCamLogName[128];
#endif

char gFirmwareVersion[SIZE_64B];

void UpdateStatusTSHttpData(char *httpPostStatus, SRulesQueue *psRuleQ)
{
    int time_stamp, status, state=0;
    char rulesBuffer[SIZE_512B] = {'\0',};
    char *subStr = NULL;
    char modelCode[SIZE_32B]=" ";
#ifdef PRODUCT_WeMo_NetCam
    char *sz_NetCamLoginName;
#ifdef NETCAM_LS
    sz_NetCamLoginName = GetBelkinParameter("SeedonkOwnerMail");
#else
    sz_NetCamLoginName = GetBelkinParameter("SeedonkOwner");
#endif
#endif

    status = getPluginStatus();
    state = getCurrFWUpdateState();
    time_stamp = getPluginStatusTS();
    gpluginPrevStatusTS = time_stamp;
   
    getModelCode(modelCode); 

    APP_LOG("REMOTEACCESS", LOG_DEBUG, "state firmwware: %s timestamp: %d, status: %d", gFirmwareVersion, time_stamp, status);

#ifndef PRODUCT_WeMo_NetCam
    snprintf(httpPostStatus, MAX_STATUS_BUF, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><serialNumber>%s</serialNumber><friendlyName>%s</friendlyName><productName>%s</productName><udnName>%s</udnName><homeId>%s</homeId><deviceType>%s</deviceType><status>%d</status><statusTS>%d</statusTS><firmwareVersion>%s</firmwareVersion><fwUpgradeStatus>%d</fwUpgradeStatus><signalStrength>%d</signalStrength><attributeLists action=\"notify\"><attribute><name>RuleAutoOffTime</name><value>%lu</value></attribute><attribute><name>deviceCurrentTime</name><value>%lu</value></attribute></attributeLists>", g_szWiFiMacAddress, g_szSerialNo, g_szFriendlyName,getProductName(modelCode),g_szUDN_1,g_szHomeId, getDeviceUDNString(), status, time_stamp, g_szFirmwareVersion, state, gSignalStrength, gCountdownEndTime, GetUTCTime());
#else
    snprintf(httpPostStatus, MAX_STATUS_BUF, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><serialNumber>%s</serialNumber><friendlyName>%s</friendlyName><productName>%s</productName><udnName>%s</udnName><homeId>%s</homeId><deviceType>%s</deviceType><status>%d</status><statusTS>%d</statusTS><firmwareVersion>%s</firmwareVersion><fwUpgradeStatus>%d</fwUpgradeStatus><signalStrength>%d</signalStrength>", g_szWiFiMacAddress, g_szSerialNo, g_szFriendlyName,getProductName(modelCode), g_szUDN_1, g_szHomeId, getDeviceUDNString(), status, time_stamp, g_szFirmwareVersion, state, gSignalStrength);
#endif

    /* Keep device type Switch for Socket */
    if((subStr = strstr(httpPostStatus, "<deviceType>Socket</deviceType>")))
	strncpy(subStr, "<deviceType>Switch</deviceType>", SIZE_32B-2);

#ifdef PRODUCT_WeMo_Insight
    if(g_eDeviceTypeTemp == DEVICE_INSIGHT)
    {
	char *paramVersion = NULL,*paramPerUnitcost = NULL,*paramCurrency = NULL;
	if(g_InstantPowerSend == 1)
	{
	    LockLED();
	    status = GetCurBinaryState();
	    UnlockLED();
	    memset(httpPostStatus, 0, MAX_STATUS_BUF);
	    if (g_InitialMonthlyEstKWH)
	    {
		snprintf(httpPostStatus, MAX_STATUS_BUF, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><applianceConsumption><macAddress>%s</macAddress><status>%d</status><instantaneousPower>%u</instantaneousPower><todayTotalTimeOn>%u</todayTotalTimeOn><timeConnected>%u</timeConnected><stateChangeTS>%d</stateChangeTS><lastONFor>%u</lastONFor><avgPowerON>%u</avgPowerON><powerThreshold>%u</powerThreshold><todayTotalKWH>%u</todayTotalKWH><past14DaysKWH>%d</past14DaysKWH><past14TotalTime>%u</past14TotalTime></applianceConsumption>", \
			g_szWiFiMacAddress,status,g_PowerNow,g_TodayONTimeTS,g_HrsConnected,g_StateChangeTS,g_ONFor,g_AvgPowerON,g_PowerThreshold,g_AccumulatedWattMinute,g_InitialMonthlyEstKWH,g_TotalONTime14Days);
	    }
	    else
	    {
		snprintf(httpPostStatus, MAX_STATUS_BUF, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><applianceConsumption><macAddress>%s</macAddress><status>%d</status><instantaneousPower>%u</instantaneousPower><todayTotalTimeOn>%u</todayTotalTimeOn><timeConnected>%u</timeConnected><stateChangeTS>%d</stateChangeTS><lastONFor>%u</lastONFor><avgPowerON>%u</avgPowerON><powerThreshold>%u</powerThreshold><todayTotalKWH>%u</todayTotalKWH><past14DaysKWH>%0.f</past14DaysKWH><past14TotalTime>%u</past14TotalTime></applianceConsumption>", \
			g_szWiFiMacAddress,status,g_PowerNow,g_TodayONTimeTS,g_HrsConnected,g_StateChangeTS,g_ONFor,g_AvgPowerON,g_PowerThreshold,g_AccumulatedWattMinute,g_KWH14Days,g_TotalONTime14Days);
	    }

	}
	else if(1 == g_InsightCostCur)
	{
	    paramVersion = GetBelkinParameter(ENERGYPERUNITCOSTVERSION);
	    paramPerUnitcost = GetBelkinParameter(ENERGYPERUNITCOST);
	    paramCurrency = GetBelkinParameter(CURRENCY);
	    APP_LOG("REMOTEACCESS", LOG_ERR, "Energy cost push: paramVersion: %s, paramPerUnitcost: %s,paramCurrency: %s",paramVersion,paramPerUnitcost,paramCurrency);
	    memset(httpPostStatus, 0, MAX_STATUS_BUF);
	    snprintf(httpPostStatus, MAX_STATUS_BUF, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><homeSettings><homeId>%s</homeId><homeSettingsVersion>%s</homeSettingsVersion><energyPerUnitCost>%s</energyPerUnitCost><currency>%s</currency></homeSettings>",g_szHomeId,paramVersion,paramPerUnitcost,paramCurrency);

	}
}
#endif

    if(g_eDeviceTypeTemp == DEVICE_LIGHTSWITCH)
    {
#if defined(LONG_PRESS_SUPPORTED)
	if(gLongPressTriggered)
	{
	    char lpStatus[SIZE_256B];
	    memset(lpStatus, 0, sizeof(lpStatus));
	    snprintf(lpStatus, sizeof(lpStatus), "<longPressStatus>%d</longPressStatus>", gLongPressTriggered);
	    strncat(httpPostStatus, lpStatus, MAX_STATUS_BUF-strlen(httpPostStatus)-1);
	}
#endif
    }

    /* rule queued */
    if(psRuleQ)
    {
	snprintf(rulesBuffer, sizeof(rulesBuffer), "<ruleID>%d</ruleID><ruleMsg><![CDATA[%s]]></ruleMsg><ruleFreq>%d</ruleFreq><ruleExecutionTS>%lu</ruleExecutionTS><productCode>WeMo</productCode>", psRuleQ->notifyRuleID, psRuleQ->ruleMSG, psRuleQ->ruleFreq, psRuleQ->ruleTS);
	strncat(httpPostStatus, rulesBuffer, MAX_STATUS_BUF-strlen(httpPostStatus)-1);
	SetRuleIDFlag(SET_RULE_FLAG);
    }

							if(strstr(httpPostStatus, "<plugin>"))
        strncat(httpPostStatus, "</plugin>", MAX_STATUS_BUF-strlen(httpPostStatus)-1);
    strncpy(gFirmwareVersion, "", sizeof(gFirmwareVersion)-1);

    APP_LOG ("REMOTEACCESS", LOG_DEBUG, "httpPostStatus: len-%d,[%s]", strlen(httpPostStatus), httpPostStatus);

    if(g_eDeviceType == DEVICE_UNKNOWN)
    {
	APP_LOG("REMOTEACCESS", LOG_ERR, "\n Does not look to be a valid device type\n");
    }
}

#define MSGS_FILE		"/var/log/messages.0"
#define LAST_MSGS_FILES_DIR 	"/tmp"
#define LAST_MSGS_FILES_PREFIX 	"messages-"
#define LAST_MSGS_FILES_PREFIX_LEN 9

int uploadLogFileDevStatus(const char *filename, const char *uploadURL)
{
		int retVal = PLUGIN_SUCCESS;
		UserAppSessionData *pWDSsnData = NULL;
		UserAppData *pWDData = NULL;
		char tmp_url[MAX_FW_URL_LEN];

  if( getCurrFWUpdateState() == FM_STATUS_UPDATE_STARTING )
  {
    APP_LOG("REMOTEACCESS", LOG_ERR, "\n F/W is updating, not sent\n");
    return FAILURE;
  }
		if(!filename || !uploadURL)
				return FAILURE;

		pWDData = (UserAppData *)ZALLOC(sizeof(UserAppData));
		if(!pWDData) {
				APP_LOG("WiFiApp", LOG_ERR, "\n Malloc failed, returned NULL, exiting \n");
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}

		pWDSsnData = webAppCreateSession(0);
		if(!pWDSsnData) {
				APP_LOG("WiFiApp", LOG_ERR, "\n Failed to create session, returned NULL \n");
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}

		memset(tmp_url,0,sizeof(tmp_url));
		strncpy(tmp_url, uploadURL, sizeof(tmp_url));

		strncpy(pWDData->keyVal[0].key, "Content-Type", sizeof(pWDData->keyVal[0].key)-1);
		strncpy(pWDData->keyVal[0].value, "multipart/octet-stream", sizeof(pWDData->keyVal[0].value)-1);
		strncpy(pWDData->keyVal[1].key, "Accept", sizeof(pWDData->keyVal[1].key)-1);
		strncpy(pWDData->keyVal[1].value, "application/xml", sizeof(pWDData->keyVal[1].value)-1);
		strncpy(pWDData->keyVal[2].key, "Authorization", sizeof(pWDData->keyVal[2].key)-1);
		strncpy(pWDData->keyVal[3].key, "X-Belkin-Messages-Type-Id", sizeof(pWDData->keyVal[3].key)-1);
		strncpy(pWDData->keyVal[3].value, g_szClientType, sizeof(pWDData->keyVal[3].value)-1);
		pWDData->keyValLen = 4;

		strncpy(pWDData->mac, g_szWiFiMacAddress, sizeof(pWDData->mac)-1);

		/* Sending the log file
		 * url: upload_url
		 */
		strncpy(pWDData->url, tmp_url, sizeof(pWDData->url)-1);
		strncpy(pWDData->inData, filename, sizeof(pWDData->inData)-1);
		pWDData->inDataLength = 0;

		char *check = strstr(pWDData->url, "https://");
		if (check) {
		    pWDData->httpsFlag = 1;
		}

		APP_LOG("WiFiApp",LOG_DEBUG, "Sending... %s\n", filename);
		retVal = webAppSendData(pWDSsnData, pWDData, 2);

		if(retVal == ERROR_INVALID_FILE) {
		    APP_LOG("WiFiApp", LOG_ERR, "\n File \'%s\' doesn't exists\n", filename);
		}else if(retVal) {
		    APP_LOG("WiFiApp", LOG_ERR, "\n Some error encountered in send status to cloud  , errorCode %d \n", retVal);
		    retVal = PLUGIN_FAILURE;
		    goto on_return;
		}

on_return:
		if (pWDSsnData) webAppDestroySession (pWDSsnData);
		if (pWDData) {
				if (pWDData->outData) {free(pWDData->outData); pWDData->outData = NULL;}
				free(pWDData); pWDData = NULL;
		}
		return retVal;
}

/* API to parse the deviceConfig return in response of SendNotification */

void* parseConfigNotificationResp(void *sendNfResp)
{

		char *snRespXml = NULL;
		mxml_node_t *tree;
		mxml_node_t *find_node;
		int ConfigFlag=0;

		if(!sendNfResp)
				return NULL;

		snRespXml = (char*)sendNfResp;
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "Entered parseConfigNotificationResp");

		tree = mxmlLoadString(NULL, snRespXml, MXML_OPAQUE_CALLBACK);
		if (tree){
				APP_LOG("REMOTEACCESS", LOG_HIDE, "XML String Loaded is %s\n", snRespXml);
				find_node = mxmlFindElement(tree, tree, "sendNotification", NULL, NULL, MXML_DESCEND);
				if(find_node){
						find_node = mxmlFindElement(tree, tree, "configChange", NULL, NULL, MXML_DESCEND);
						if (find_node && find_node->child) {
								APP_LOG("REMOTEACCESS", LOG_DEBUG, "The ConfigChange Flag is found as %s\n", find_node->child->value.opaque);
								ConfigFlag = atoi(find_node->child->value.opaque);


								if(ConfigFlag == 1)
								{
										g_configChange = 1;
										APP_LOG("REMOTEACCESS", LOG_DEBUG, "Config Chage Flag is %d\n", g_configChange);
								}
#if defined(PRODUCT_WeMo_Insight) || defined(PRODUCT_WeMo_SNS) || defined(PRODUCT_WeMo_NetCam) || defined(PRODUCT_WeMo_Light)
								else if(ConfigFlag == 2)
								{
										SetRuleIDFlag(RESET_RULE_FLAG);
								}
#endif
								else
								{
										/* Do not set variable   g_configChange = 0; */
										APP_LOG("REMOTEACCESS", LOG_DEBUG, "Config Change  Flag is found as 0\n");
								}
						}
						else
						{
								APP_LOG("REMOTEACCESS", LOG_DEBUG, "No configChange tag in response");
								goto on_return;
						}
				}else{
						APP_LOG("REMOTEACCESS", LOG_DEBUG, "No sendNotification tag in response");
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

void* sendConfigChangeStatusThread(void *arg)
{
                tu_set_my_thread_name( __FUNCTION__ );

		APP_LOG("REMOTEACCESS", LOG_DEBUG, "sendConfigChangeStatusThread Thread Started");

		while(1){


				/*Wait to get signal that devConfig flag is rcvd as true */
				pthread_mutex_lock(&g_devConfig_mutex1);
				pthread_cond_wait(&g_devConfig_cond1,&g_devConfig_mutex1);
				pthread_mutex_unlock(&g_devConfig_mutex1);
				if (g_configChange == 1) {

						if(sendConfigChangeDevStatus() < PLUGIN_SUCCESS){
								APP_LOG("REMOTEACCESS", LOG_DEBUG, "Send Config Change device status fail");
						}else{
								APP_LOG("REMOTEACCESS", LOG_DEBUG, "Send Config Change device status success...");
						}
				}else {
				}
				sleep(6);
		}
		pthread_exit(0);
		return NULL;
}


void parseMergeHomeResponse(void *sendNfResp)
{
		char *snRespXml = NULL;
		mxml_node_t *tree;
		mxml_node_t *find_node;
		char* paramNames[] = {"HomeId"};
		char *homeid = NULL;

		if(!sendNfResp)
		{
		    APP_LOG("SNS", LOG_DEBUG, "sendNfResp is null");
		    return;
		}

		snRespXml = (char*)sendNfResp;
		APP_LOG("SNS", LOG_HIDE, "XML received in response is %s",snRespXml);
		tree = mxmlLoadString(NULL, snRespXml, MXML_OPAQUE_CALLBACK);
		if (tree)
		{
				find_node = mxmlFindElement(tree, tree, "homeId", NULL, NULL, MXML_DESCEND);
				if(find_node)
				{
						if (find_node && find_node->child)
						{
								APP_LOG("SNS", LOG_HIDE, "The homeid value is  %s\n", find_node->child->value.opaque);
								/* update self home_id */
								SetBelkinParameter("home_id", find_node->child->value.opaque);
								AsyncSaveData();
								/* update the global variable to contain updated home_id */
								memset(g_szHomeId, 0, sizeof(g_szHomeId));
								strncpy(g_szHomeId, find_node->child->value.opaque, sizeof(g_szHomeId)-1);

								homeid = (char *)ZALLOC(SIZE_64B);
								if(homeid)
								{
										encryptWithMacKeydata(g_szHomeId, homeid);
								}

								PluginCtrlPointSendActionAll(PLUGIN_E_EVENT_SERVICE, "SetHomeId", (const char **)paramNames, (char **)&homeid, 0x01);
								if(homeid)
										free(homeid);
						}
				}
				mxmlDelete(tree);
		}
		else
				APP_LOG("SNS", LOG_ERR, "Could not load tree");

}

/* parse the home id list and prepare the XML in the following format:
 ** <homeIds>
 <homeId>1</homeId>
 <homeId>2</homeId>
 ...
 </homeIds>
 */

void updateMergeHomesRequestXml(char *httpPostStatus, char *homeidlist, char *signaturelist, int len)
{
		char *home_id=NULL;
		char sign[SIZE_256B];
		int cnt=0;
		char tmp[SIZE_256B];
		char *p=NULL;
		char *strtok_r_temp;

		APP_LOG("Parse", LOG_HIDE, "Input home id list: %s", homeidlist);
		APP_LOG("Parse", LOG_HIDE, "Input signature list: %s", signaturelist);
		/* prepare the header */
		snprintf(httpPostStatus, MAX_STATUS_BUF, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><homes>");

		/* parse list and add to XML string */
		while((home_id = strtok_r(homeidlist, "-",&strtok_r_temp)))
		{
				strncat(httpPostStatus, "<home>", MAX_STATUS_BUF-strlen(httpPostStatus)-1);

				cnt ++;
				APP_LOG("Parse", LOG_HIDE, "Extracted home_id: %s, cnt: %d", home_id, cnt);
				homeidlist = NULL;

				memset(tmp, 0, sizeof(tmp));
				snprintf(tmp,	sizeof(tmp), "<homeId>%s</homeId>", home_id);
				strncat(httpPostStatus, tmp, MAX_STATUS_BUF-strlen(httpPostStatus)-1);

				/* using sscanf to avoid hindering the other list parsing */
				memset(tmp, 0, sizeof(tmp));
				memset(sign, 0, sizeof(sign));

				sscanf(signaturelist, "%[^-]-%s", sign, tmp);
				p = strchr(signaturelist, '-');
				if(p)
						signaturelist  = p+1;


				APP_LOG("Parse", LOG_HIDE, "Extracted sign: %s, updated signature list: %s", sign, signaturelist);

				memset(tmp, 0, sizeof(tmp));
				snprintf(tmp,	sizeof(tmp), "<sign>%s</sign>", sign);
				strncat(httpPostStatus, tmp, MAX_STATUS_BUF-strlen(httpPostStatus)-1);

				strncat(httpPostStatus, "</home>", MAX_STATUS_BUF-strlen(httpPostStatus)-1);
		}

		/* finally, append the trailer to complete the XML */
		strncat(httpPostStatus, "</homes>", MAX_STATUS_BUF-strlen(httpPostStatus)-1);
		APP_LOG("Parse", LOG_HIDE, "XML formed: %s", httpPostStatus);
}


void sendHomeIdListToCloud(char *homeidlist, char *signaturelist)
{
		char httpPostData[MAX_STATUS_BUF];
		int retVal = PLUGIN_SUCCESS;
		UserAppSessionData *pUserAppSsnData = NULL;
		UserAppData *pUserAppData = NULL;
		authSign *sign = NULL;

		/* Drop the response in case there is no Internet connection - Rare case */
		if (getCurrentClientState() != STATE_CONNECTED) {
				APP_LOG("REMOTEACCESS", LOG_ERR, "NO Internet connection");
				return;
		}

		pUserAppData = (UserAppData *)CALLOC(1,sizeof(UserAppData));
		if (!pUserAppData) {
				APP_LOG("REMOTEACCESS", LOG_ERR, "Malloc Failed");
				goto on_return;
		}

		sign = createAuthSignature(g_szWiFiMacAddress, GetSerialNumber(), g_szPluginPrivatekey);
		if (!sign) {
				APP_LOG("REMOTEACCESS", LOG_ERR, "\n Signature Structure returned NULL\n");
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}

		memset(httpPostData, 0x0, MAX_STATUS_BUF);
		updateMergeHomesRequestXml(httpPostData, homeidlist, signaturelist, MAX_STATUS_BUF);


		pUserAppSsnData = webAppCreateSession(0);
		if(!pUserAppSsnData)
		{
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "Websession Creation failed");
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}
		else
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "Websession Created\n");


		snprintf( pUserAppData->url, sizeof(pUserAppData->url), "https://%s:8443/apis/http/plugin/mergeHomes", BL_DOMAIN_NM);
		strncpy( pUserAppData->keyVal[0].key, "Content-Type", sizeof(pUserAppData->keyVal[0].key)-1);
		strncpy( pUserAppData->keyVal[0].value, "application/xml", sizeof(pUserAppData->keyVal[0].value)-1);
		strncpy( pUserAppData->keyVal[1].key, "Accept", sizeof(pUserAppData->keyVal[1].key)-1);
		strncpy( pUserAppData->keyVal[1].value, "application/xml", sizeof(pUserAppData->keyVal[1].value)-1);
		strncpy( pUserAppData->keyVal[2].key, "Authorization", sizeof(pUserAppData->keyVal[2].key)-1);
		strncpy( pUserAppData->keyVal[2].value, sign->signature, sizeof(pUserAppData->keyVal[2].value)-1);
		strncpy( pUserAppData->keyVal[3].key, "X-Belkin-Client-Type-Id", sizeof(pUserAppData->keyVal[3].key)-1);
		strncpy( pUserAppData->keyVal[3].value, g_szClientType, sizeof(pUserAppData->keyVal[3].value)-1);
		pUserAppData->keyValLen = 4;
                
                pUserAppData->outData=NULL;
		strncpy( pUserAppData->inData, httpPostData, sizeof(pUserAppData->inData)-1);
		pUserAppData->inDataLength = strlen(httpPostData);
		char *check = strstr(pUserAppData->url, "https://");
		if (check) {
				pUserAppData->httpsFlag = 1;
		}
		pUserAppData->disableFlag = 1;
		APP_LOG("REMOTEACCESS", LOG_DEBUG, " **********Sending Cloud Response XML\n");

		retVal = webAppSendData( pUserAppSsnData, pUserAppData, 1);
		if (retVal)
		{
				APP_LOG("REMOTEACCESS", LOG_ERR, "\n Some error encountered in send status to cloud  , errorCode %d \n", retVal);
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}

		if( (strstr(pUserAppData->outHeader, "500")) || (strstr(pUserAppData->outHeader, "503")) ){
				APP_LOG("REMOTEACCESS", LOG_ERR, "Cloud is not reachable, error: 500 or 503");
				retVal = PLUGIN_FAILURE;
		}else if(strstr(pUserAppData->outHeader, "200")){
				APP_LOG("REMOTEACESS", LOG_DEBUG, "send status to cloud success");
				parseMergeHomeResponse(pUserAppData->outData);
		}else if(strstr(pUserAppData->outHeader, "403")){
				APP_LOG("REMOTEACCESS", LOG_ERR, "Cloud error: 403");
		}else{
				APP_LOG("REMOTEACESS", LOG_DEBUG, "Some error encountered: Error response from cloud");
				retVal = PLUGIN_FAILURE;
		}

on_return:
		if (pUserAppSsnData) {webAppDestroySession ( pUserAppSsnData ); pUserAppSsnData = NULL;}
		if (sign) {free(sign); sign = NULL;};
		if (pUserAppData) {
				if (pUserAppData->outData) {free(pUserAppData->outData); pUserAppData->outData = NULL;}
				free(pUserAppData); pUserAppData = NULL;
		}

}

int getTimeZone(char *szTimezone)
{
	float localTZ=0.0;
	char *LocalTimeZone = GetBelkinParameter(SYNCTIME_LASTTIMEZONE);
	char *LastDstValue = GetBelkinParameter(LASTDSTENABLE);
    	int DstVal=-1;

	if(!szTimezone)
		return PLUGIN_FAILURE;

	if((LocalTimeZone != NULL) && (strlen(LocalTimeZone) != 0))
	{
		localTZ = atof(LocalTimeZone);
	}
	else
	{
		APP_LOG("REMOTEACCESS",LOG_ERR,"Invalid Timezone stored on flash");
		return PLUGIN_FAILURE;
	}

	if((LastDstValue != NULL) && (strlen(LastDstValue) != 0))
	{
		DstVal = atoi(LastDstValue);
	}
	else
	{
		APP_LOG("REMOTEACCESS",LOG_DEBUG,"Invalid DST value stored on flash");
	}

	APP_LOG("REMOTEACCESS",LOG_DEBUG," ---localTZ: %f,LocalTimeZone:%s, DSTVal: %d",localTZ,LocalTimeZone,!DstVal);
	if(DstVal == 0)
		localTZ = localTZ + 1.0;

	memset(szTimezone, 0, SIZE_16B);

	if (DEVICE_SENSOR == g_eDeviceType)
		snprintf(szTimezone, SIZE_16B, "TimeZone: %f", localTZ);
	else
		snprintf(szTimezone, SIZE_16B, ";TimeZone: %f", localTZ);

	return PLUGIN_SUCCESS;
}

#ifdef WeMo_SMART_SETUP_V2
void* sendCustomizedStateThread(void *args)
{
    APP_LOG("UPNP",LOG_DEBUG, "send customized state Thread");
    int retVal = PLUGIN_SUCCESS;
    char tmp_url[MAX_FW_URL_LEN];
    UserAppSessionData *pUsrAppSsnData = NULL;
    UserAppData *pUsrAppData = NULL;
    authSign *assign = NULL;
    int customizedState = 0;

    pUsrAppData = (UserAppData *)CALLOC(1, sizeof(UserAppData));
    if(!pUsrAppData)
    {
	APP_LOG("REMOTEACCESS", LOG_ERR, "Malloc Failed\n");
	resetSystem();
    }

    pUsrAppSsnData = webAppCreateSession(0);
    APP_LOG("REMOTEACCESS", LOG_DEBUG, "Websession Created\n");

    while(1)
    {
	/* loop until device not registered */
	if ((0x00 == strlen(g_szHomeId) ) || (0x00 == strlen(g_szPluginPrivatekey)) || (atoi(g_szRestoreState) == 0x1))
	{
	    pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);
	    continue;
	}

	/* loop until we find Internet connected */
	while(1)
	{
	    if (getCurrentClientState() == STATE_CONNECTED) {
		break;
	    }
	    pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);  //30 sec
	}

	assign = createAuthSignature(g_szWiFiMacAddress, g_szSerialNo, g_szPluginPrivatekey);
	if(!assign)
	{
	    APP_LOG("REMOTEACCESS", LOG_ERR, "Signature returned NULL\n");
	    retVal = PLUGIN_FAILURE;
	    goto on_return;
	}

	if(g_customizedState)
	    customizedState = DEVICE_CUSTOMIZED;
	else
	    customizedState = DEVICE_UNCUSTOMIZED;
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "customized state: %d", customizedState);

	memset(tmp_url,0,sizeof(tmp_url));
	snprintf(tmp_url,sizeof(tmp_url), "https://%s:8443/apis/http/plugin/property/%s/customizedState/%d", BL_DOMAIN_NM, g_szWiFiMacAddress, customizedState);
	strncpy(pUsrAppData->url, tmp_url, sizeof(pUsrAppData->url)-1);
	strncpy( pUsrAppData->keyVal[0].key, "Content-Type", sizeof(pUsrAppData->keyVal[0].key)-1);
	strncpy( pUsrAppData->keyVal[0].value, "application/xml", sizeof(pUsrAppData->keyVal[0].value)-1);
	strncpy( pUsrAppData->keyVal[1].key, "Accept", sizeof(pUsrAppData->keyVal[1].key)-1);
	strncpy( pUsrAppData->keyVal[1].value, "application/xml", sizeof(pUsrAppData->keyVal[1].value)-1);
	strncpy( pUsrAppData->keyVal[2].key, "Authorization", sizeof(pUsrAppData->keyVal[2].key)-1);
	strncpy( pUsrAppData->keyVal[2].value, assign->signature, sizeof(pUsrAppData->keyVal[2].value)-1);
	strncpy( pUsrAppData->keyVal[3].key, "X-Belkin-Client-Type-Id", sizeof(pUsrAppData->keyVal[3].key)-1);
	strncpy( pUsrAppData->keyVal[3].value, g_szClientType, sizeof(pUsrAppData->keyVal[3].value)-1);
	pUsrAppData->keyValLen = 4;

	strncpy( pUsrAppData->inData, " ", sizeof(pUsrAppData->inData)-1);
	pUsrAppData->inDataLength = 1;
	char *check = strstr(pUsrAppData->url, "https://");
	if(check)
	    pUsrAppData->httpsFlag = 1;

	pUsrAppData->disableFlag = 1;
	APP_LOG("REMOTEACCESS", LOG_DEBUG, " **********Sending customized state notification to Cloud");

	retVal = webAppSendData( pUsrAppSsnData, pUsrAppData, 1);
	if (retVal)
	{
	    APP_LOG("REMOTEACCESS", LOG_ERR, "\n Curl error encountered in send status to cloud  , errorCode %d \n", retVal);
	    retVal = PLUGIN_FAILURE;
	    break;
	}

	if( (strstr(pUsrAppData->outHeader, "500")) || (strstr(pUsrAppData->outHeader, "503")) )
	{
	    APP_LOG("REMOTEACCESS", LOG_DEBUG, "Cloud is not reachable, error: 500 or 503");
	    retVal = PLUGIN_FAILURE;
	    pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);  //30 sec
	    if (assign) {free(assign); assign = NULL;};
	    continue;
	}
	else if(strstr(pUsrAppData->outHeader, "200"))
	{
	    APP_LOG("REMOTEACESS", LOG_DEBUG, "send customized state notification to cloud success");
	    setCustomizedState(DEVICE_CUSTOMIZED);
	    break;
	}
	else if(strstr(pUsrAppData->outHeader, "403"))
	{
	    APP_LOG("REMOTEACCESS", LOG_DEBUG, "Cloud error: 403");
	    break;
	}
	else
	{
	    APP_LOG("REMOTEACESS", LOG_DEBUG, "Some error encountered: Error response from cloud");
	    retVal = PLUGIN_FAILURE;
	    pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT/3);  //10 sec
	    if (assign) {free(assign); assign = NULL;};
	    continue;
	}

    }

on_return:
    if (pUsrAppSsnData) {webAppDestroySession ( pUsrAppSsnData ); pUsrAppSsnData = NULL;}
    if (assign) {free(assign); assign = NULL;};
    if (pUsrAppData) {
	if (pUsrAppData->outData) {free(pUsrAppData->outData); pUsrAppData->outData = NULL;}
	free(pUsrAppData); pUsrAppData = NULL;
    }
    customizedstate_thread = -1;
    APP_LOG("UPNP",LOG_DEBUG, "send customized state Thread done...");
    return NULL;
}

void sendCustomizedStateNotification(void)
{
    if (-1 != customizedstate_thread)
    {
	APP_LOG("UPNP: Device", LOG_DEBUG, "send customized state Thread already created");
	return;
    }
    APP_LOG("UPNP",LOG_DEBUG, "***************Create send customized state Thread ***************");
    int retVal;
    pthread_attr_init(&customizedstate_attr);
    pthread_attr_setdetachstate(&customizedstate_attr, PTHREAD_CREATE_DETACHED);
    retVal = pthread_create(&customizedstate_thread, &customizedstate_attr, &sendCustomizedStateThread, NULL);
    if(retVal < SUCCESS)
    {
	APP_LOG("Remote",LOG_DEBUG, "!! send customized thread not Created !!");
	resetSystem();
    }
    else
    {
	APP_LOG("Remote",LOG_DEBUG, "send customized thread created successfully");
    }
}
#endif

int sendRemoteAccessUpdateDevStatus (SRulesQueue **ppsRuleQ)
{
		char httpPostStatus[MAX_STATUS_BUF];
		int retVal = PLUGIN_SUCCESS;
		char *pcode = NULL;
		char regBuf[MAX_BUF_LEN];
		char tmp_url[MAX_FW_URL_LEN];
		//The flags are used to log WD error message once in WD logfile
		int deferWDLogging = 0, deferWDLogging2 = 0;
		char szTimeZone[SIZE_16B];
		char szLogData[SIZE_256B];
		authSign *assign = NULL;

#ifdef PRODUCT_WeMo_Insight
		int InsightParseFlag=0;//Variable taken to handle Insight Parameter 200OK success condition.
#endif
		gSendNotifyHealthPunch++;

		while(1)
		{
				/* loop until we find Internet connected */
#if 0
				while(1)
				{
						gSendNotifyHealthPunch++;
						if (getCurrentClientState() == STATE_CONNECTED) {
								break;
						}
						pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);  //30 sec
				}
#endif
				while(getCurrentClientState() != STATE_CONNECTED)
				{
						gSendNotifyHealthPunch++;
						pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);  //30 sec
				}
#ifndef PRODUCT_WeMo_NetCam
				while(gNTPTimeSet != 1)
				{
						gSendNotifyHealthPunch++;
						pluginUsleep((REMOTE_STATUS_SEND_RETRY_TIMEOUT/2));  //15 sec
				}
#endif
				assign = createAuthSignature(g_szWiFiMacAddress, g_szSerialNo, g_szPluginPrivatekey);
				if (!assign) {
						APP_LOG("REMOTEACCESS", LOG_ERR, "\n Signature Structure returned NULL\n");
						retVal = PLUGIN_FAILURE;
						goto on_return;
				}

				gSendNotifyHealthPunch++;
				memset(httpPostStatus, 0x0, MAX_STATUS_BUF);
				UpdateStatusTSHttpData(httpPostStatus, *ppsRuleQ);

				//APP_LOG("REMOTEACCESS", LOG_HIDE, "gNotificationBuffer=%s len=%d\n",gNotificationBuffer, strlen(gNotificationBuffer));
				if (pUsrAppData->outData) {
						free(pUsrAppData->outData);
				}
				memset( pUsrAppData, 0x0, sizeof(UserAppData));

				APP_LOG("REMOTEACCESS", LOG_DEBUG, "Memset Done\n");
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "Websession Created\n");
				memset(tmp_url,0,sizeof(tmp_url));
#ifdef PRODUCT_WeMo_Insight
				if(g_InstantPowerSend == 1)
				{
						snprintf(tmp_url, sizeof(tmp_url), "https://%s:8443/apis/http/plugin/insight/insightNotification",BL_DOMAIN_NM);
						APP_LOG("REMOTEACCESS", LOG_DEBUG, "tmp_url =%s \n", tmp_url);
						strncpy(pUsrAppData->url,tmp_url, sizeof(pUsrAppData->url)-1);
						//g_InstantPowerSend = 0;
						InsightParseFlag = 1;
						APP_LOG("REMOTEACCESS", LOG_DEBUG, " InsightParseFlag = 1\n");
				}
				else if(1 == g_InsightCostCur){
						snprintf(tmp_url, sizeof(tmp_url), "https://%s:8443/apis/http/plugin/insight/sendInsightParams", BL_DOMAIN_NM);
						APP_LOG("REMOTEACCESS", LOG_DEBUG, "tmp_url =%s \n", tmp_url);
						strncpy(pUsrAppData->url,tmp_url, sizeof(pUsrAppData->url)-1);
						//g_InsightCostCur = 0;

				}
				else
#endif
				{
						snprintf(tmp_url,sizeof(tmp_url), "https://%s:8443/apis/http/plugin/sendNotification", BL_DOMAIN_NM);
						strncpy(pUsrAppData->url, tmp_url, sizeof(pUsrAppData->url)-1);
				}

				if(g_urlDataUsage == 1){

				  //sprintf(tmp_url, "https:// %s:8443/apis/http/plugin/sc/scNotification", BL_DOMAIN_NM);
				  //sprintf(tmp_url, "http://10.20.90.173:8080/PluginService-0.0.1/plugin/usageData");
				  sprintf(tmp_url, "https://%s:8443/apis/http/plugin/usageData", BL_DOMAIN_NM);
				  strncpy(pUsrAppData->url, tmp_url, sizeof(pUsrAppData->url)-1);
				  g_urlDataUsage = 0;
				}

				memset(szLogData, 0, sizeof(szLogData));

				if(strlen(g_szActuation))
					snprintf(szLogData, sizeof(szLogData), "Actuation: %s", g_szActuation);

				APP_LOG("REMOTEACCESS", LOG_DEBUG, "log data: %s", szLogData);

				if(strlen(g_szRemote))
				{
					strncat(szLogData, ";Remote: ", sizeof(szLogData) - strlen(szLogData) - 1);
					strncat(szLogData, g_szRemote, sizeof(szLogData) - strlen(szLogData) - 1);
					APP_LOG("REMOTEACCESS", LOG_DEBUG, "log data: %s", szLogData);
				}

				if(getTimeZone(szTimeZone) == PLUGIN_SUCCESS)
				{
					strncat(szLogData, szTimeZone, sizeof(szLogData) - strlen(szLogData) - 1);
					APP_LOG("REMOTEACCESS", LOG_DEBUG, "log data: %s", szLogData);
				}

				strncpy( pUsrAppData->keyVal[0].key, "Content-Type", sizeof(pUsrAppData->keyVal[0].key)-1);
				strncpy( pUsrAppData->keyVal[0].value, "application/xml", sizeof(pUsrAppData->keyVal[0].value)-1);
				strncpy( pUsrAppData->keyVal[1].key, "Accept", sizeof(pUsrAppData->keyVal[1].key)-1);
				strncpy( pUsrAppData->keyVal[1].value, "application/xml", sizeof(pUsrAppData->keyVal[1].value)-1);
				strncpy( pUsrAppData->keyVal[2].key, "Authorization", sizeof(pUsrAppData->keyVal[2].key)-1);
				strncpy( pUsrAppData->keyVal[2].value, assign->signature, sizeof(pUsrAppData->keyVal[2].value)-1);
				strncpy( pUsrAppData->keyVal[3].key, "Log-Data", sizeof(pUsrAppData->keyVal[3].key)-1);
				strncpy( pUsrAppData->keyVal[3].value, szLogData, sizeof(pUsrAppData->keyVal[3].value)-1);
				strncpy( pUsrAppData->keyVal[4].key, "X-Belkin-Client-Type-Id", sizeof(pUsrAppData->keyVal[4].key)-1);
				strncpy( pUsrAppData->keyVal[4].value, g_szClientType, sizeof(pUsrAppData->keyVal[4].value)-1);
				pUsrAppData->keyValLen = 5;

				strncpy( pUsrAppData->inData, httpPostStatus, sizeof(pUsrAppData->inData)-1);
                                pUsrAppData->inDataLength = strlen(httpPostStatus);

				//strncpy( pUsrAppData->inData, gNotificationBuffer, sizeof(pUsrAppData->inData)-1);
				//pUsrAppData->inDataLength = strlen(gNotificationBuffer);

				char *check = strstr(pUsrAppData->url, "https://");
				if (check) {
						pUsrAppData->httpsFlag = 1;
				}
				pUsrAppData->disableFlag = 1;
				APP_LOG("REMOTEACCESS", LOG_DEBUG, " **********Sending Cloud XML\n");

				retVal = webAppSendData( pUsrAppSsnData, pUsrAppData, 1);
				if (retVal)
				{
						APP_LOG("REMOTEACCESS", LOG_ERR, "\n Some error encountered in send status to cloud  , errorCode %d \n", retVal);
						APP_LOG("REMOTEACCESS", LOG_ALERT, "Send Notification Status to Cloud failed, CURL error");
						retVal = PLUGIN_FAILURE;
						break;
				}

				if( (strstr(pUsrAppData->outHeader, "500")) || (strstr(pUsrAppData->outHeader, "503")) ){
						APP_LOG("REMOTEACESS", LOG_DEBUG, "Some error encountered: Cloud is not reachable");
						if(!deferWDLogging2) {
								APP_LOG("REMOTEACCESS", LOG_ALERT, "Cloud is not reachable, error: 500 or 503");
						} else {
								APP_LOG("REMOTEACCESS", LOG_DEBUG, "Cloud is not reachable, error: 500 or 503");
						}
						deferWDLogging2++;
						retVal = PLUGIN_FAILURE;
						enqueueBugsense("REMOTE_NOTIFY_FAIL_5XX");
						pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);  //30 sec
						if (assign) {free(assign); assign = NULL;};
						continue;
				}else if(strstr(pUsrAppData->outHeader, "200")){
#ifdef PRODUCT_WeMo_Insight
						if(InsightParseFlag == 1)
						{
								parseInsightNotificationResp(pUsrAppData->outData, &pcode);
								InsightParseFlag = 0;
								g_InstantPowerSend = 0;
						}
						if(g_InsightCostCur)
						    g_InsightCostCur =0;
#endif

						parseConfigNotificationResp(pUsrAppData->outData);

#if defined(PRODUCT_WeMo_Insight) || defined(PRODUCT_WeMo_SNS) || defined(PRODUCT_WeMo_NetCam) || defined(PRODUCT_WeMo_Light)
						if(RESET_RULE_FLAG == GetRuleIDFlag())
						{
							if(*ppsRuleQ)
							{
								APP_LOG("REMOTEACESS", LOG_DEBUG, "Freeing ruleid: %d node", (*ppsRuleQ)->ruleID);
								free((*ppsRuleQ)->ruleMSG);
								free(*ppsRuleQ);
								*ppsRuleQ = NULL;
							}
						}
#endif
						APP_LOG("REMOTEACESS", LOG_DEBUG, "send status to cloud success");
						memset(g_szActuation, 0, sizeof(g_szActuation));
						memset(g_szRemote, 0, sizeof(g_szRemote));
#if defined(LONG_PRESS_SUPPORTED)
						LockLongPress();
						gLongPressTriggered = 0x00;
						UnlockLongPress();
#endif
						break;
				}else if(strstr(pUsrAppData->outHeader, "403")){
						parseSendNotificationResp(pUsrAppData->outData, &pcode);
						if ((pcode)){
								memset(regBuf, '\0', MAX_BUF_LEN);
								APP_LOG("REMOTEACESS", LOG_DEBUG, "########AUTH FAILURE (403) : %s: Not sending this Event ########", pcode);
								snprintf(regBuf, sizeof(regBuf), "###AUTH FAILURE(403): Not sending this Event ##, CODE: %s", pcode);
								APP_LOG("REMOTEACCESS", LOG_ALERT, "%s", regBuf);
								if (!strncmp(pcode, "ERR_002", strlen("ERR_002"))) {
										CheckUpdateRemoteFailureCount();
								}
								if (pcode) {free(pcode); pcode= NULL;}
						}
						enqueueBugsense("REMOTE_NOTIFY_FAIL_403");
						parseConfigNotificationResp(pUsrAppData->outData);
#ifdef PRODUCT_WeMo_Insight
						if(g_InsightCostCur)
						{
						    g_InsightCostCur =0;
						    g_InsightHomeSettingsSend=1;
						}
						if(g_InstantPowerSend)
						{
						    g_InstantPowerSend  =0;
						    g_SendInsightParams =1;
						}
#endif
#if defined(LONG_PRESS_SUPPORTED)
						LockLongPress();
						gLongPressTriggered = 0x00;
						UnlockLongPress();
#endif
						break;
				} else {
						APP_LOG("REMOTEACESS", LOG_DEBUG, "Some error encountered: Error response from cloud");
						if(!deferWDLogging) {
								APP_LOG("REMOTEACCESS", LOG_ALERT, "Cloud not reachable, unknown error");
						} else {
								APP_LOG("REMOTEACCESS", LOG_DEBUG, "Cloud not reachable, unknown error");
						}
						deferWDLogging++;
						retVal = PLUGIN_FAILURE;
						enqueueBugsense("REMOTE_NOTIFY_FAIL_OTHER");
						pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT/3);  //10 sec
						if (assign) {free(assign); assign = NULL;};
						continue;
				}

		}
on_return:

		if (assign) {free(assign); assign = NULL;};
		if (g_configChange) {
				/* Condition variable to trigger
					 session creation for deviceConfig */
				pthread_mutex_lock(&g_devConfig_mutex1);
				pthread_cond_signal(&g_devConfig_cond1);
				pthread_mutex_unlock(&g_devConfig_mutex1);
		}
		return retVal;
}

int uploadDbFile(char *file)
{
        int fileExists = 1;
        struct stat fileInfo;
        int ret = 0;
        char tmp_url[MAX_FW_URL_LEN];
        char buf[MAX_KEY_LEN];

        /* Check for the timer value to push the log file to the cloud */
        while(1)
        {
                if (IsNtpUpdate())
                {
                        break;
                }
                sleep(60);
        }

        memset(tmp_url, 0x0, MAX_FW_URL_LEN);

        snprintf(tmp_url, sizeof(tmp_url), "http://%s:%s/%s/", BL_DOMAIN_NM_WD, BL_DOMAIN_PORT_WD, BL_DOMAIN_URL_DB);
	strncat(tmp_url,basename(file),sizeof(tmp_url)-strlen(tmp_url)-1);

        /* Checking for the existence of the watchdog log files.
         * If none of the file exists, then exiting the LOG thread
         */
        if((stat(file, &fileInfo) == -1 && errno == ENOENT) || ((off_t)fileInfo.st_size == 0))
                fileExists = 0;

        if(fileExists == 0)
        {
                APP_LOG("WiFiApp", LOG_ERR, "Db file: %s doesn't exists", file);
                return FAILURE;
        }

        /* loop until we find Internet connected */
        while(1)
        {
                if (getCurrentClientState() == STATE_CONNECTED)
                        break;
                pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);  //30 sec
        }

        ret = uploadLogFileDevStatus(file, tmp_url);

        if(ret < 0)
        {
         APP_LOG("WiFiApp", LOG_DEBUG, "Upload db File status fail");
        }
        else
        {
                APP_LOG("WiFiApp", LOG_DEBUG, "Upload db File: %s success...", file);

                //Clearing the log file if Uploaded successfully.
                memset(buf, 0x0, sizeof(buf));
                snprintf(buf, sizeof(buf), "rm %s", file);
                system(buf);
                
                /* messages file writing would have some logs but this is earliest we can push wdLog opening statement */
                if(strcmp(file, MSGS_FILE) == 0)
                        openFile(1);
        }

        return ret;
                                                     
}

int uploadMessageFile(char *file)
{
	int fileExists = 1;
	struct stat fileInfo;
	int ret = 0;
	char tmp_url[MAX_FW_URL_LEN];
	char buf[MAX_KEY_LEN];

	/* Check for the timer value to push the log file to the cloud */
	while(1)
	{
		if (IsNtpUpdate())
		{
			break;
		}
		sleep(60);
	}

	memset(tmp_url, 0x0, MAX_FW_URL_LEN);

	snprintf(tmp_url, sizeof(tmp_url), "http://%s:%s/%s/", BL_DOMAIN_NM_WD, BL_DOMAIN_PORT_WD, BL_DOMAIN_URL_WD);
	strncat(tmp_url, g_szWiFiMacAddress, sizeof(tmp_url)-strlen(tmp_url)-1);

	/* Checking for the existence of the watchdog log files.
	 * If none of the file exists, then exiting the LOG thread
	 */
	if((stat(file, &fileInfo) == -1 && errno == ENOENT) || ((off_t)fileInfo.st_size == 0))
		fileExists = 0;

	if(fileExists == 0)
	{
		APP_LOG("WiFiApp", LOG_ERR, "Messages log file: %s doesn't exists", file);
		return FAILURE;
	}

	/* loop until we find Internet connected */
	while(1)
	{
		if (getCurrentClientState() == STATE_CONNECTED)
			break;
		pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);  //30 sec
	}

	ret = uploadLogFileDevStatus(file, tmp_url);

	if(ret < 0)
	{
		APP_LOG("WiFiApp", LOG_DEBUG, "Upload Log File status fail");
	}
	else
	{
		APP_LOG("WiFiApp", LOG_DEBUG, "Upload Log File: %s success...", file);

		//Clearing the log file if Uploaded successfully.
		memset(buf, 0x0, sizeof(buf));
		snprintf(buf, sizeof(buf), "rm %s", file);
		system(buf);

		/* messages file writing would have some logs but this is earliest we can push wdLog opening statement */
		if(strcmp(file, MSGS_FILE) == 0)
			openFile(1);
	}

	return ret;
}

int messages_files(const struct dirent *entry)
{
   return (!(strncmp(entry->d_name, LAST_MSGS_FILES_PREFIX, LAST_MSGS_FILES_PREFIX_LEN)));
}

void upload_last_msgs_file(void)
{
    struct dirent **nameList = NULL;
    int dirEntries = -1;
    char filePath[SIZE_64B];

    dirEntries = scandir(LAST_MSGS_FILES_DIR, &nameList, messages_files, alphasort);
    if (dirEntries < 0)
    {
	APP_LOG("Remote",LOG_ERR,"scan dir failed: %s",strerror(errno));
	if(nameList != NULL)
	    free(nameList);
	return;
    }

    while (dirEntries--)
    {
	APP_LOG("Remote",LOG_DEBUG,"messages file is: %s", nameList[dirEntries]->d_name);
	memset(filePath, 0, sizeof(filePath));
	snprintf(filePath, sizeof(filePath), "%s/%s", LAST_MSGS_FILES_DIR, nameList[dirEntries]->d_name);
	APP_LOG("Remote",LOG_DEBUG,"messages file to post is: %s", filePath);
	free(nameList[dirEntries]);
	uploadMessageFile(filePath);
    }

    free(nameList);
}

void* uploadLogFileThread(void *arg)
{
  int nFWStatus = 0;
  char *pUploadEnable = NULL;

	tu_set_my_thread_name( __FUNCTION__ );

  /* upload last messages file, if any */
  pUploadEnable = GetBelkinParameter(LOG_UPLOAD_ENABLE);

  /* upload last messages file, if any */
  if( (NULL != pUploadEnable) && (0 != strlen(pUploadEnable)) )
  {
	upload_last_msgs_file();
  }
  else
  {
    APP_LOG("WiFiApp",LOG_DEBUG, "Uploading log files not enabled");
  }

  while( 1 )
  {
    nFWStatus = getCurrFWUpdateState();

    if( nFWStatus == FM_STATUS_DEFAULT )
    {
    if( (NULL != pUploadEnable) && (0x00 != strlen(pUploadEnable)))
    {
      /* keep uploading messages file whenever it is backed up */
      uploadMessageFile(MSGS_FILE);
    }

    sleep(60);
    pUploadEnable = GetBelkinParameter(LOG_UPLOAD_ENABLE);
    }
    else
    {
      sleep(60);
    }
  }

	APP_LOG("WiFiApp",LOG_DEBUG, "***************EXITING LOG Thread***************\n");
	pthread_exit(0);
	return NULL;
}

void * uploadDbFileThread(void *arg)
{
  int nFWStatus = 0;
  char *pUploadEnable = NULL;

  tu_set_my_thread_name( __FUNCTION__ );
  nFWStatus = getCurrFWUpdateState();

  pUploadEnable = GetBelkinParameter(DB_UPLOAD_ENABLE);
  if( nFWStatus == FM_STATUS_DEFAULT )
   {
    if( (NULL != pUploadEnable) && (0x00 != strlen(pUploadEnable)))
     {
      /* keep uploading rules db file */
      FILE *fp;
      char path[256];

      /* Open the command for reading. */
      fp = popen("ls -1 /tmp/rules_*.db", "r");
      if (fp == NULL) 
       {
	printf("Failed to run command\n" );
	return;
       }
      /* Read the output a line at a time - upload it. */
       while(fscanf(fp,"%s\n",path) == 1)
       {
	printf("%s", path);
	uploadDbFile(path);
       }
      /* close */
      pclose(fp);
     }
    else
     {
      APP_LOG("WiFiApp",LOG_DEBUG, "Uploading db files not enabled");
     }
   }

  APP_LOG("WiFiApp",LOG_DEBUG, "***************EXITING DB Thread***************\n");
  dbFile_thread = -1;
  pthread_exit(0);
  return NULL;
}

void *sendRemAccessUpdStatusThdMonitor(void *arg)
{
                tu_set_my_thread_name( __FUNCTION__ );
		APP_LOG("REMOTEACCESS",LOG_DEBUG,"sendRemAccessUpdStatusThread Monitor started...");

		while(1)
		{
				pluginUsleep(SEND_REM_UPD_STAT_TH_MON_TIMEOUT * 1000000);
				if(gSendNotifyHealthPunch == 0)
				{
						APP_LOG("REMOTEACCESS",LOG_CRIT,"sendRemAccessUpdStatusThread monitor detected bad health ...");
						if(sendremoteupdstatus_thread)
								pthread_kill(sendremoteupdstatus_thread, SIGRTMIN);
						sendremoteupdstatus_thread = -1;

						APP_LOG("REMOTEACCESS",LOG_DEBUG,"Removed sendremoteupdstatus thread...");

						//Again create a sendremoteupdstatus thread
						ithread_create(&sendremoteupdstatus_thread, NULL, sendRemoteAccessUpdateStatusThread, NULL);
						ithread_detach (sendremoteupdstatus_thread);

						APP_LOG("REMOTEACCESS",LOG_DEBUG,"sendremoteupdstatus thread created again...");

				}
				else
				{
						APP_LOG("REMOTEACCESS",LOG_DEBUG,"sendremoteupdstatus thread health OK [%d]...", gSendNotifyHealthPunch);
						gSendNotifyHealthPunch = 0;
				}
		}
		return NULL;
}

void* sendRemoteAccessUpdateStatusThread(void *arg)
{
                tu_set_my_thread_name( __FUNCTION__ );
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "sendRemoteAccessUpdateStatusThread Thread Started");
		SRulesQueue *psRuleQ=NULL;

		pUsrAppData = (UserAppData *)CALLOC(1, sizeof(UserAppData));
		if (!pUsrAppData) {
				APP_LOG("REMOTEACCESS", LOG_ERR, "\n Malloc Failed\n");
				return NULL;
		}
		pUsrAppSsnData = webAppCreateSession(0);

		while(1){
				gSendNotifyHealthPunch++;
				if ((0x00 == strlen(g_szHomeId) ) || (0x00 == strlen(g_szPluginPrivatekey)) || (atoi(g_szRestoreState) == 0x1)) {
						pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);
						continue;
				}

				if(!isNotificationEnabled ()) {
						APP_LOG("REMOTEACCESS", LOG_DEBUG, "Remote Notification Sending is disabled");
						pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);
						continue;
				}

				/*check for pending notifications */
				if(!psRuleQ)
				{
					psRuleQ = dequeueRuleQ();
				}

				if(psRuleQ)
					remoteAccessUpdateStatusTSParams (0xFF);

				if((gpluginPrevStatusTS == getPluginStatusTS()) && (0 == strlen(gFirmwareVersion))){
#ifndef PRODUCT_WeMo_Insight
						pluginUsleep(5000000);
						continue;
#else
						if((g_InsightHomeSettingsSend == 0)&&(g_SendInsightParams == 0))
						{
								pluginUsleep(5000000);
								continue;
						}
						else{
								if(g_InsightHomeSettingsSend)
								{
										APP_LOG("REMOTEACCESS", LOG_DEBUG, "Sending Insight Home Settings");
										g_InsightHomeSettingsSend=0;
										g_InsightCostCur = 1;

								}
								else if(g_SendInsightParams)
								{
										APP_LOG("REMOTEACCESS", LOG_DEBUG, "Sending Insight Parameters");
										g_SendInsightParams = 0;
										g_InstantPowerSend = 1;
								}
						}
#endif
				}

				if ((0x00 != strlen(g_szHomeId) ) && (0x00 != strlen(g_szPluginPrivatekey)) && (atoi(g_szRestoreState) == 0x0) && \
								(gpluginRemAccessEnable == 1)) {
						if(sendRemoteAccessUpdateDevStatus(&psRuleQ) < PLUGIN_SUCCESS){
								APP_LOG("REMOTEACCESS", LOG_DEBUG, "Send Remote Update device status fail");
						}else{
								APP_LOG("REMOTEACCESS", LOG_DEBUG, "Send Remote Update device status success...");
						}
				}else {
						APP_LOG("REMOTEACCESS", LOG_DEBUG, "Remote Access does not look to be enabled");
				}

		}
		if (pUsrAppSsnData) {webAppDestroySession ( pUsrAppSsnData ); pUsrAppSsnData = NULL;}
		if (pUsrAppData) {
				if (pUsrAppData->outData) {free(pUsrAppData->outData); pUsrAppData->outData = NULL;}
				free(pUsrAppData); pUsrAppData = NULL;
		}
		pthread_exit(0);
		return NULL;
}

void remoteAccessUpdateStatusTSParams(int status)
{

		if(0xFF == status) {
				if (DEVICE_SENSOR == g_eDeviceType)
						status = 1;
#ifdef PRODUCT_WeMo_LEDLight
                else if( (DEVICE_SOCKET == g_eDeviceType) && (DEVICE_BRIDGE == g_eDeviceTypeTemp) )
                {
                  trigger_remote_event(WORK_EVENT, UPDATE_DEVICE, "device", "update", NULL, 0);
                }
#endif
				else
						status = GetDeviceStateIF();

				memset(gFirmwareVersion, 0, SIZE_64B);
				strncpy(gFirmwareVersion, g_szFirmwareVersion, sizeof(gFirmwareVersion)-1);
		}
		else {
				if( (DEVICE_SENSOR == g_eDeviceType) && (status) ) {
						setPluginStatusTS();
				}
				else if(DEVICE_SOCKET == g_eDeviceType)  {
						setPluginStatusTS();
				}
				else if(DEVICE_CROCKPOT == g_eDeviceType) {
						setPluginStatusTS();
						APP_LOG("REMOTEACCESS", LOG_DEBUG, "TS status changed to %d",status);
	}
	else if((DEVICE_SBIRON == g_eDeviceType) || (DEVICE_MRCOFFEE == g_eDeviceType) ||
	        (DEVICE_PETFEEDER == g_eDeviceType)) {
	    setPluginStatusTS();
	    APP_LOG("REMOTEACCESS", LOG_DEBUG, "TS status changed to %d",status);
	}
	else
	{
		/* WeMo Smart, Do Nothing */
	}

				if(strcmp(gFirmwareVersion, "") == 0)
						memset(gFirmwareVersion, 0, SIZE_64B);

				if( ((DEVICE_SENSOR == g_eDeviceType) && status)  || (DEVICE_SOCKET == g_eDeviceType) )
						setPluginStatus(status);
		}

}

#ifdef __NFTOCLOUD__
int PrepareSendNotificationData(char *sMac_N, char *httpPostStatus, int ts, int natStatus)
{
		int  status = 0;
		int retVal = PLUGIN_SUCCESS;

		status = getPluginStatus();
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "timestamp: %d, status: %d, natStatus: %d", ts, status, natStatus);
		if (g_eDeviceType == DEVICE_SOCKET) {
				snprintf(httpPostStatus, MAX_STATUS_BUF, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><serialNumber>%s</serialNumber><friendlyName>%s</friendlyName><udnName>%s</udnName><homeId>%s</homeId><deviceType>Switch</deviceType><networkStatus>%d</networkStatus><status>%d</status><statusTS>%d</statusTS><firmwareVersion>%s</firmwareVersion></plugin>", sMac_N, g_szSerialNo, g_szFriendlyName, g_szUDN_1, g_szHomeId, natStatus, status, ts, g_szFirmwareVersion);
				retVal = PLUGIN_SUCCESS;

		} else if (g_eDeviceType == DEVICE_SENSOR) {
				snprintf(httpPostStatus, MAX_STATUS_BUF, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><serialNumber>%s</serialNumber><friendlyName>%s</friendlyName><udnName>%s</udnName><homeId>%s</homeId><deviceType>Sensor</deviceType><networkStatus>%d</networkStatus><status>%d</status><statusTS>%d</statusTS><firmwareVersion>%s</firmwareVersion></plugin>", sMac_N, g_szSerialNo, g_szFriendlyName, g_szUDN_1, g_szHomeId, natStatus, status, ts, g_szFirmwareVersion);
				retVal = PLUGIN_SUCCESS;
		} else {
				APP_LOG("REMOTEACCESS", LOG_ERR, "\n Does not look to be a valid device type\n");
				retVal = PLUGIN_FAILURE;
		}
		return retVal;
}

int  sendNATStatusI (int natStatus, int ts) {
		char httpPostStatus[MAX_STATUS_BUF];
		int retVal = PLUGIN_SUCCESS;
		authSign *assign_N = NULL;
		UserAppSessionData *pUsrAppSsnData_N = NULL;
		UserAppData *pUsrAppData_N = NULL;
		char *sMac_N = NULL;
		char tmp_url[MAX_FW_URL_LEN];

		APP_LOG("REMOTEACCESS", LOG_DEBUG, " **********In sendNATStatusI nat init failure.......\n");
		sMac_N = g_szWiFiMacAddress;
		if (!sMac_N) {
				APP_LOG("REMOTEACCESS", LOG_ERR, "\n delimited sMac_N returned NULL\n");
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}
		assign_N = createAuthSignature(sMac_N, g_szSerialNo, g_szPluginPrivatekey);
		if (!assign_N) {
				APP_LOG("REMOTEACCESS", LOG_ERR, "\n Signature Structure returned NULL\n");
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}

		memset(httpPostStatus, 0x0, MAX_STATUS_BUF);
		retVal = PrepareSendNotificationData(sMac_N, httpPostStatus, ts, natStatus);
		if (retVal != PLUGIN_SUCCESS) {
				APP_LOG("REMOTEACCESS", LOG_ERR, "\n PrepareSendNotificationData returned failure\n");
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}

		APP_LOG("REMOTEACCESS", LOG_HIDE, "httpPostStatus=%s len=%d\n",httpPostStatus, strlen(httpPostStatus));

		APP_LOG("REMOTEACCESS", LOG_DEBUG, "Memset Done\n");
		pUsrAppSsnData_N = webAppCreateSession(0);

		APP_LOG("REMOTEACCESS", LOG_DEBUG, "Websession Created\n");

		pUsrAppData_N = (UserAppData *)ZALLOC(sizeof(UserAppData));
		if (!pUsrAppData_N) {
				APP_LOG("REMOTEACCESS", LOG_ERR, "\n Malloc Failed\n");
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}
		memset( tmp_url, 0x0, sizeof(tmp_url));
		snprintf(tmp_url, sizeof(tmp_url), "https://%s:8443/apis/http/plugin/sendNotification", BL_DOMAIN_NM);
		strncpy(pUsrAppData_N->url, tmp_url, sizeof(pUsrAppData_N->url)-1);
		strncpy( pUsrAppData_N->keyVal[0].key, "Content-Type", sizeof(pUsrAppData_N->keyVal[0].key)-1);
		strncpy( pUsrAppData_N->keyVal[0].value, "application/xml", sizeof(pUsrAppData_N->keyVal[0].value)-1);
		strncpy( pUsrAppData_N->keyVal[1].key, "Accept", sizeof(pUsrAppData_N->keyVal[1].key)-1);
		strncpy( pUsrAppData_N->keyVal[1].value, "application/xml", sizeof(pUsrAppData_N->keyVal[1].value)-1);
		strncpy( pUsrAppData_N->keyVal[2].key, "Authorization", sizeof(pUsrAppData_N->keyVal[2].key)-1);
		strncpy( pUsrAppData_N->keyVal[2].value, assign_N->signature, sizeof(pUsrAppData_N->keyVal[2].value)-1);
		strncpy( pUsrAppData_N->keyVal[3].key, "X-Belkin-Client-Type-Id", sizeof(pUsrAppData_N->keyVal[3].key)-1);
		strncpy( pUsrAppData_N->keyVal[3].value, g_szClientType, sizeof(pUsrAppData_N->keyVal[3].value)-1);
		pUsrAppData_N->keyValLen = 4;

		strncpy( pUsrAppData_N->inData, httpPostStatus, sizeof(pUsrAppData_N->inData)-1);
		pUsrAppData_N->inDataLength = strlen(httpPostStatus);
		char *check = strstr(pUsrAppData_N->url, "https://");
		if (check) {
				pUsrAppData_N->httpsFlag = 1;
		}
		pUsrAppData_N->disableFlag = 1;
		APP_LOG("REMOTEACCESS", LOG_DEBUG, " **********Sending Cloud XML\n");

		retVal = webAppSendData( pUsrAppSsnData_N, pUsrAppData_N, 1);
		if (retVal) {
				APP_LOG("REMOTEACCESS", LOG_ERR, "\n Some error encountered in send status to cloud  , errorCode %d \n", retVal);
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}

		if(strstr(pUsrAppData_N->outHeader, "200")){
				APP_LOG("REMOTEACESS", LOG_DEBUG, "send status to cloud success");
				retVal = PLUGIN_SUCCESS;
		}else {
				APP_LOG("REMOTEACESS", LOG_DEBUG, "send status to cloud failure");
				retVal = PLUGIN_FAILURE;
		}

on_return:
		if (pUsrAppSsnData_N) webAppDestroySession ( pUsrAppSsnData_N );
		if (assign_N) {free(assign_N); assign_N = NULL;};
		if (pUsrAppData_N) {
				if (pUsrAppData_N->outData) {free(pUsrAppData_N->outData); pUsrAppData_N->outData = NULL;}
				free(pUsrAppData_N); pUsrAppData_N = NULL;
		}

		return retVal;
}

int  sendNATStatus (int natStatus) {
		int retry_count=0;
		int ts = 0;
		int retVal = PLUGIN_FAILURE;

		APP_LOG("REMOTEACCESS", LOG_DEBUG, " **********In sendNATStatus nat init failure.......\n");
		ts=GetUTCTime();
		while(retry_count < MAX_NAT_STATUS_COUNT) {
				retry_count++;
				retVal = sendNATStatusI (natStatus, ts);
				if (retVal != PLUGIN_SUCCESS) {
						retVal = PLUGIN_FAILURE;
				}else {
						retVal = PLUGIN_SUCCESS;
						break;
				}
				pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);  //30 sec
		}
		return retVal;
}
#endif
int sendConfigChangeDevStatus()
{
		UserAppSessionData *pUsrAppSsnData = NULL;
		UserAppData *pUsrAppData = NULL;
		authSign *assign = NULL;
		int retVal = PLUGIN_SUCCESS;
		char *ptr = NULL;
		char *dev_Mac=NULL;
		char tmp_url[MAX_FW_URL_LEN];
		APP_LOG("REMOTEACCESS", LOG_DEBUG,"Entered sendConfigChangeDevStatus API\n");
		//Resolve LB domain name to get SIP Server and other server IPs
		dev_Mac = utilsRemDelimitStr(GetMACAddress(), ":");
		assign = createAuthSignature(dev_Mac, g_szSerialNo, g_szPluginPrivatekey);
		if (!assign) {
				APP_LOG("REMOTEACCESS", LOG_ERR,"\n Signature Structure returned NULL\n");
				retVal = PLUGIN_FAILURE;
				goto exit_below;
		}
		APP_LOG("deviceconfig",LOG_HIDE, "assign->signature... %s", assign->signature);
		pUsrAppData = (UserAppData *)ZALLOC(sizeof(UserAppData));
		if (!pUsrAppData) {
				APP_LOG("REMOTEACCESS", LOG_ERR, "\n Malloc Failed\n");
				retVal = PLUGIN_FAILURE;
				goto exit_below;
		}
		pUsrAppSsnData = webAppCreateSession(0);
		if (!pUsrAppSsnData) {
				APP_LOG("REMOTEACCESS", LOG_ERR, "\n Malloc Failed\n");
				retVal = PLUGIN_FAILURE;
				goto exit_below;
		}
		APP_LOG("REMOTEACCESS", LOG_DEBUG,"Sending deviceConfig to Cloud  \n");
		memset(tmp_url,0,sizeof(tmp_url));
		/* prepare REST header*/
		{
				snprintf(tmp_url, sizeof(tmp_url), "https://%s:8443/apis/http/plugin/ext/deviceConfig",BL_DOMAIN_NM);
				strcpy( pUsrAppData->url, tmp_url);
				strcpy( pUsrAppData->keyVal[0].key, "Content-Type");
				strcpy( pUsrAppData->keyVal[0].value, "application/xml");
				strcpy( pUsrAppData->keyVal[1].key, "Authorization");
				strcpy( pUsrAppData->keyVal[1].value,  assign->signature);
				strncpy(pUsrAppData->keyVal[2].key, "X-Belkin-Client-Type-Id", sizeof(pUsrAppData->keyVal[2].key)-1);
				strncpy(pUsrAppData->keyVal[2].value, g_szClientType, sizeof(pUsrAppData->keyVal[2].value)-1);
				pUsrAppData->keyValLen = 3;
				/* enable SSL if auth URL is on HTTPS*/
				ptr = strstr(tmp_url,"https" );
				if(ptr != NULL)
						pUsrAppData->httpsFlag =  1;
				else
						pUsrAppData->httpsFlag =  0;
		}


		pUsrAppData->disableFlag = 1;
		pUsrAppData->inDataLength = 0;
		retVal = webAppSendData( pUsrAppSsnData, pUsrAppData, 0);
		if (retVal)
		{
				APP_LOG("REMOTEACCESS", LOG_ERR,"Some error encountered while sending to %s errorCode %d \n", tmp_url, retVal);
				g_configChange=0;/*Resetting the config change status */
				retVal = PLUGIN_FAILURE;
				goto exit_below;
		}
		/* check response header to see if user is authorized or not*/
		{
				if(strstr(pUsrAppData->outHeader, "200"))
				{
						APP_LOG("REMOTEACCESS", LOG_DEBUG,"Response 200 OK received from %s\n", tmp_url);
						DespatchSendNotificationResp(pUsrAppData->outData);
						g_configChange=0;/*Resetting the config change status */
						/*Parse the received xml and store in a FIFO */
				}
				else if(strstr(pUsrAppData->outHeader, "403"))
				{
						APP_LOG("REMOTEACCESS", LOG_DEBUG,"Response 403 received from %s\n", tmp_url);
						DespatchSendNotificationResp(pUsrAppData->outData);
						g_configChange=0;
				}else {
						APP_LOG("REMOTEACCESS", LOG_DEBUG,"Response other than 200 OK received from %s\n", tmp_url);
						g_configChange=0;/*Resetting the config change status */
						retVal = PLUGIN_FAILURE;
						goto exit_below;
				}
		}
exit_below:
		if (pUsrAppSsnData) {webAppDestroySession ( pUsrAppSsnData ); pUsrAppSsnData = NULL;}
		if (pUsrAppData) {
				if (pUsrAppData->outData) {free(pUsrAppData->outData); pUsrAppData->outData = NULL;}
				free(pUsrAppData); pUsrAppData = NULL;
		}
		if (dev_Mac) {free(dev_Mac); dev_Mac = NULL;}
		if (assign) { free(assign); assign = NULL; }

		return retVal;
}

int UploadIcon(char *UploadURL,char *fsFileUrl,char*cookie)
{
		int retVal = PLUGIN_SUCCESS;
		UserAppSessionData *pIconSsnData = NULL;
		UserAppData *pIconData = NULL;
		struct stat fileInfo;
		char pluginKey[MAX_PKEY_LEN];
		authSign *assign = NULL;
		char tmp_url[MAX_FW_URL_LEN];
		char *ptr = NULL;

		memset(pluginKey, 0x0, MAX_PKEY_LEN);
		strncpy(pluginKey, g_szPluginPrivatekey, sizeof(pluginKey)-1);

		assign = createAuthSignature(g_szWiFiMacAddress, g_szSerialNo, pluginKey);
		if (!assign) {
				APP_LOG("UploadIcon", LOG_DEBUG,"Signature Structure returned NULL");
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}

		pIconData = (UserAppData *)ZALLOC(sizeof(UserAppData));
		if(!pIconData) {
				APP_LOG("UploadIcon", LOG_ERR, "Malloc failed, returned NULL, exiting");
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}

		pIconSsnData = webAppCreateSession(0);
		if(!pIconSsnData) {
				APP_LOG("UploadIcon", LOG_ERR, "Failed to create session, returned NULL");
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}

		strncpy(pIconData->keyVal[0].key, "Content-Type", sizeof(pIconData->keyVal[0].key)-1);
		strncpy(pIconData->keyVal[0].value, "multipart/octet-stream", sizeof(pIconData->keyVal[0].value)-1);
		strncpy(pIconData->keyVal[1].key, "Accept", sizeof(pIconData->keyVal[1].key)-1);
		strncpy(pIconData->keyVal[1].value, "application/xml", sizeof(pIconData->keyVal[1].value)-1);
		strncpy(pIconData->keyVal[2].key, "Authorization", sizeof(pIconData->keyVal[2].key)-1);
		strncpy(pIconData->keyVal[2].value, assign->signature, sizeof(pIconData->keyVal[2].key)-1);
		strncpy(pIconData->keyVal[3].key, "X-Belkin-Client-Type-Id", sizeof(pIconData->keyVal[3].key)-1);
		strncpy(pIconData->keyVal[3].value, g_szClientType, sizeof(pIconData->keyVal[3].value)-1);
		pIconData->keyValLen = 4;

		APP_LOG("UploadIcon",LOG_HIDE, "assign->signature... %s", assign->signature);

		strncpy(pIconData->mac, g_szWiFiMacAddress, sizeof(pIconData->mac)-1);

		//Sending the Insight data file
		memset(&fileInfo, '\0', sizeof(struct stat));
		lstat(ICON_FILE_URL, &fileInfo);
		memset(tmp_url,0,sizeof(tmp_url));
		snprintf(tmp_url, sizeof(tmp_url), "%s", UploadURL);
		if((off_t)fileInfo.st_size != 0) {
				memset(pIconData->url, 0, sizeof(pIconData->url)/sizeof(char));
				memset(pIconData->inData, 0, sizeof(pIconData->inData)/sizeof(char));
				strncpy(pIconData->url, tmp_url, sizeof(pIconData->url)-1);
				strncpy(pIconData->inData, ICON_FILE_URL, sizeof(pIconData->inData)-1);
				pIconData->inDataLength = 0;

				char *check = strstr(pIconData->url, "https://");
				if (check) {
						pIconData->httpsFlag = 1;
				}

				APP_LOG("UploadIcon",LOG_DEBUG, "Sending... %s", ICON_FILE_URL);
				retVal = webAppSendData(pIconSsnData, pIconData, 2);

				if(retVal == ERROR_INVALID_FILE) {
						APP_LOG("UploadIcon", LOG_ERR, "File \'%s\' doesn't exists", ICON_FILE_URL);
				}
				ptr = strstr(pIconData->outHeader,"200 OK" );
				if(ptr != NULL)
				{
						APP_LOG("UploadIcon", LOG_DEBUG,"Response 200 OK received from %s\n", UploadURL);
						memset(cookie,0,SIZE_512B);
						strncpy(cookie,pIconData->cookie_data,SIZE_512B-1);
						APP_LOG("REMOTEACCESS", LOG_HIDE,"Cookie is =%s\n", cookie);
						APP_LOG("UPLOAD ICON", LOG_HIDE," Response data received from %s\n", pIconData->outData);
						if(NULL !=  pIconData->outData){
								strcpy(fsFileUrl, pIconData->outData);
								APP_LOG("UPLOAD ICON", LOG_HIDE,"NC: fsFileUrl=%s\n", fsFileUrl);
						}
						else{
								APP_LOG("UPLOAD ICON", LOG_DEBUG,"ERROR: #####Unable to Upload icon\n");
						}
						retVal = PLUGIN_SUCCESS;
				}else {
						APP_LOG("UploadIcon", LOG_DEBUG,"Response other than 200 OK received from %s\n", UploadURL);
						retVal = PLUGIN_FAILURE;
						goto on_return;
				}
				if(retVal) {
						APP_LOG("UploadIcon", LOG_ERR, "Some error encountered in send status to cloud  , errorCode %d", retVal);
						retVal = PLUGIN_FAILURE;
						goto on_return;
				}
		}

on_return:
		if (pIconSsnData) webAppDestroySession (pIconSsnData);
		if (pIconData) {
				if (pIconData->outData) {free(pIconData->outData); pIconData->outData = NULL;}
				free(pIconData); pIconData = NULL;
		}
		if (assign) {free(assign); assign = NULL;}

		return retVal;
}
