
/***************************************************************************
*
*
* insightUPnPHandler.c
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
#ifdef PRODUCT_WeMo_Insight

#include "global.h"
#include "defines.h"
#include "controlledevice.h"
#include "plugin_ctrlpoint.h"
#include "gpio.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "watchDog.h"
#include "insight.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

char	     g_NoNotificationFlag=0;//Don't Send Local Push Notification When Sending State Notification
unsigned int g_ONFor=0;// Variable used to display on App
unsigned int g_ONForChangeFlag=0;// Used to see wheather to make ONFor variable 0 or not in the API statechangetimestamp
unsigned int g_RuleONFor=0;// This variable is used as rule condition and is seperate from ONFor as ONFor is not zeroed in SBY
unsigned int g_ONForTS=0;//used to calculate g_RuleONFor
unsigned int g_InSBYSince=0;// Used as rule condition
unsigned int g_TodayONTime=0;// Used in calculating parameters based on this
unsigned int g_TodayONTimeTS=0;// Used to display on App. can't use g_TodayONTime due to sleep delays in computation
unsigned int g_TodayKWHON=0;// Used in calculation as well as display on App
unsigned int g_TodaySBYTime=0;// Used for data export
unsigned int g_TodaySBYTimeTS=0;// Used for data export
unsigned int g_TodayKWHSBY=0;// Used for data export
unsigned int g_YesterdayONTime=0;// Used in IFTTT
unsigned int g_YesterdaySBYTime=0;// Used for IFTTT
unsigned int g_YesterdayKWHON=0;// Used in IFTTT
unsigned int g_YesterdayKWHSBY=0;// Used for IFTTT
time_t	     g_YesterdayTS=0;// Used for IFTTT
unsigned int g_PowerThreshold=0;// Used to detect SBY state
unsigned int g_StateChangeTS=0;// Send to App for their calculation
unsigned int g_TotalONTime14Days=0;// Used for calculation and for display on App
unsigned int g_HrsConnected=0;// Used in calculations this value is g_HrsConnected = current time - Setup timestamp
unsigned int g_AvgPowerON=0;// Calculated value displayed on App
double g_KWH14Days=0;// Used to calculate g_AvgPowerON
unsigned int g_AccumulatedWattMinute=0;// This is KWH 
unsigned int g_PowerNow=0;// Used to display on App
unsigned long int g_SetUpCompleteTS = 0;// Used to calculate g_HrsConnected
unsigned int g_perUnitCost=0;    //used to store per unit cost
unsigned int g_Currency; //to store currency type in integer form
unsigned int g_PerUnitCostVer=0;  //energy per unit cost Version
unsigned int g_ReStartDataExportScheduler = 0;// Used to export data
int g_InitialMonthlyEstKWH = 0;//Used to incorporate feature to start calculation of KWH when device is on for 10 minutes.
unsigned int g_Cost = 0;//Used as Insight Rule Parameter But no feature developed on top of this.
char g_s8EmailAddress[SIZE_256B];//Mail ID for data export
eDATA_EXPORT_TYPE g_s32DataExportType = E_TURN_OFF_SCHEDULER;// Type of Data export
unsigned int g_InsightHomeSettingsSend = 1;// Used to start sending Instant parameters once home related settings are updated on cloud.
unsigned int g_InsightCostCur = 0;// Used to start sending Instant parameters once home related settings are updated on cloud.
char 		g_InsightSettingsSend = 0;//Start sending instantaneous value once insight parameters are sent.
unsigned int g_APNSLastState = 0;// Used to restrict sending APNS from OFF->SBY and SBY->OFF.
unsigned int g_InsightDataRestoreState = 0;// Used to Restore Insight Data in case of wemoApp restart
extern UpnpDevice_Handle device_handle;
pthread_attr_t setHomeSettings_attr;
pthread_t setHomeSettings_thread=-1;
extern int gProcessData;


void LocalInsightParamsNotify(void)
{
    int curState=0x00;

    if (!IsUPnPNetworkMode())
    {
	//- Not report since not on router or internet
	APP_LOG("UPNP", LOG_DEBUG, "Notification:InsightParams: Not in home network, ignored");
	return;
    }

    curState = g_PowerStatus;

    char* szCurState[1];
    szCurState[0x00] = (char*)MALLOC(101);

    if (g_InitialMonthlyEstKWH){
	    snprintf(szCurState[0x00], 100, "%d|%u|%u|%u|%u|%u|%u|%u|%u|%d|%d", 
			    curState,g_StateChangeTS,g_ONFor,g_TodayONTimeTS ,g_TotalONTime14Days,g_HrsConnected,g_AvgPowerON,g_PowerNow,g_AccumulatedWattMinute,g_InitialMonthlyEstKWH,g_PowerThreshold);}
    else{
	    snprintf(szCurState[0x00], 100, "%d|%u|%u|%u|%u|%u|%u|%u|%u|%0.f|%d",
			    curState,g_StateChangeTS,g_ONFor,g_TodayONTimeTS ,g_TotalONTime14Days,g_HrsConnected,g_AvgPowerON,g_PowerNow,g_AccumulatedWattMinute,g_KWH14Days,g_PowerThreshold);}

    APP_LOG("UPNP: Device", LOG_HIDE, "Sending Insight Prams: %s", szCurState[0x00]);
    char* paramters[] = {"InsightParams"} ;

    UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_INSIGHT_SERVICE].UDN, 
	    SocketDevice.service_table[PLUGIN_E_INSIGHT_SERVICE].ServiceId,(const char **) paramters,(const char **)szCurState, 0x01);
    free(szCurState[0x00]);

}
void LocalPowerThresholdNotify(unsigned int curState)
{
    if (!IsUPnPNetworkMode())
    {
	//- Not report since not on router or internet
	APP_LOG("UPNP", LOG_DEBUG, "Notification:InstPower: Not in home network, ignored");
	return;
    }
	char* szCurState[1];
	szCurState[0x00] = (char*)MALLOC(SIZE_32B+1);
	snprintf(szCurState[0x00], 32, "%d", curState);


    char* paramters[] = {"PowerThreshold"} ;

    UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_INSIGHT_SERVICE].UDN, 
	    SocketDevice.service_table[PLUGIN_E_INSIGHT_SERVICE].ServiceId,(const char **) paramters,(const char **) szCurState, 0x01);


    APP_LOG("UPNP", LOG_DEBUG, "Notification:PowerThreshold: %d", curState);
    
    free(szCurState[0x00]);


}

void LocalInsightHomeSettingNotify()
{
	char value[SIZE_256B];
	char *paramSet[] = {"EnergyPerUnitCost"};
	char *valueSet[1];
	memset(value, 0x00, sizeof(value));

	if (!IsUPnPNetworkMode())
	{
		//- Not report since not on router or internet
		APP_LOG("UPNP", LOG_DEBUG, "Notification:InsightHomeSettingNotify: Not in home network, ignored");
		return;
	}

	snprintf(value, sizeof(value), "%d|%d|%d", g_PerUnitCostVer, g_perUnitCost, g_Currency);
	APP_LOG("UPNPDevice", LOG_DEBUG, "Notification:InsightHomeSettingNotify: value: %s", value);

	valueSet[0] = value;
	UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].UDN,SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].ServiceId,(const char **)paramSet,(const char **)valueSet,1);

	APP_LOG("UPNPDevice", LOG_DEBUG, "Notification:InsightHomeSettingNotify: paramVersion: %d, paramPerUnitcost: %d, paramCurrency: %d", g_PerUnitCostVer , g_perUnitCost ,g_Currency);
}

//------------------------------------------INSIGHT  -----------------

int GetPowerThreshold(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	char szResponse[SIZE_256B];
	char power_threshold[SIZE_16B];
	memset(szResponse, 0x00, sizeof(szResponse));
	memset(power_threshold, 0x00, sizeof(power_threshold));
	APP_LOG("UPNP: Device", LOG_DEBUG, "%s called", __FUNCTION__);

	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;
        snprintf (power_threshold,  sizeof(power_threshold), "%d",g_PowerThreshold);

	snprintf(szResponse, sizeof(szResponse), "%s",power_threshold);

	APP_LOG("UPNP: Device", LOG_DEBUG, "%s", szResponse);

	UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetPowerThreshold", CtrleeDeviceServiceType[PLUGIN_E_INSIGHT_SERVICE], "PowerThreshold", szResponse);  

	return UPNP_E_SUCCESS;
}


int ResetPowerThreshold(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	char szResponse[SIZE_256B];
	memset(szResponse, 0x00, sizeof(szResponse));
	APP_LOG("UPNP: Device", LOG_DEBUG, "%s called", __FUNCTION__);

	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;
	APP_LOG("UPNPDevice", LOG_DEBUG, "ResetPowerThreshold Setting PowerThresholda to default value");
        SetBelkinParameter (POWERTHRESHOLD,DEFAULT_POWERTHRESHOLD);
	g_PowerThreshold = atoi(DEFAULT_POWERTHRESHOLD);  
	AsyncSaveData();

	snprintf(szResponse, sizeof(szResponse), "%d",g_PowerThreshold);

	APP_LOG("UPNP: Device", LOG_DEBUG, "%s", szResponse);

	UpnpAddToActionResponse(&pActionRequest->ActionResult, "ResetPowerThreshold", CtrleeDeviceServiceType[PLUGIN_E_INSIGHT_SERVICE], "PowerThreshold", szResponse);  
        APP_LOG("UPNP: Device", LOG_DEBUG, "Sending Local Notification for SetAutoPowerThreshold: %d", g_PowerThreshold);
        //LocalPowerThresholdNotify(atoi(power_threshold));
	pMessage msg = 0x00;
	msg = createMessage(UPNP_MESSAGE_PWRTHRES_IND, 0x00, 0x00);
	SendMessage2App(msg);
	
	g_SendInsightParams = 1;

	return UPNP_E_SUCCESS;
}

int SetAutoPowerThreshold(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
        char Ret;
        DataValues Values = {0,0,0,0,0};
        char s8Threshold[SIZE_32B];
        int s32Threshold = 0;

        APP_LOG("UPNP: Device", LOG_DEBUG, "%s called", __FUNCTION__);
        if (0x00 == pActionRequest || 0x00 == request)
        {
                APP_LOG("UPNPDevice", LOG_DEBUG, "SetAutoPowerThreshold: command paramter invalid");
                return 0x01;
        }


        if((Ret = HAL_GetCurrentReadings(&Values)) != 0) {
                APP_LOG("UPNP: Device", LOG_DEBUG, "\n.........Current Value Not coming From Daemon\n");
                return 0x01;
        }
        APP_LOG("UPNP: Device", LOG_DEBUG, "Instantaneous Power Value: %d",Values.Watts);

        APP_LOG("UPNPDevice", LOG_DEBUG, "SetAutoPowerThreshold Setting PowerThreshold");
        memset(s8Threshold, 0, sizeof(s8Threshold));
        s32Threshold = Values.Watts + 2000;
        snprintf(s8Threshold, sizeof(s8Threshold), "%d", s32Threshold);
        SetBelkinParameter(POWERTHRESHOLD, s8Threshold);
	g_PowerThreshold = atoi(s8Threshold); 
	AsyncSaveData();

        //Send UPNP response
        pActionRequest->ActionResult = NULL;
        pActionRequest->ErrCode = 0x00;
        UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetAutoPowerThreshold",
                        CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"PowerThreshold" , s8Threshold);

        APP_LOG("UPNPDevice", LOG_DEBUG, "Sending Local Notification for SetAutoPowerThreshold: %d",s32Threshold);
	g_PowerThreshold = s32Threshold;
        //LocalPowerThresholdNotify(s32Threshold);
	pMessage msg = 0x00;
	msg = createMessage(UPNP_MESSAGE_PWRTHRES_IND, 0x00, 0x00);
	SendMessage2App(msg);
	g_SendInsightParams = 1;

        return UPNP_E_SUCCESS;
}

int SetPowerThreshold(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	int Threshold=0;
	APP_LOG("UPNP: Device", LOG_DEBUG, "%s called", __FUNCTION__);
	if (0x00 == pActionRequest || 0x00 == request)
	{
		APP_LOG("UPNPDevice", LOG_DEBUG, "SetPowerThreshold: command paramter invalid");
		return 0x01;
	}
	char* paramValue = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "PowerThreshold");
	if (0x00 == paramValue || 0x00 == strlen(paramValue))
	{
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x01;
		APP_LOG("UPNPDevice", LOG_DEBUG, "SetPowerThreshold: No Param Value");
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetPowerThreshold", 
				CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "PowerThreshold", "Error");  

		return 0x00;
	}
	APP_LOG("UPNPDevice", LOG_DEBUG, "SetPowerThreshold Setting PowerThreshold");
        SetBelkinParameter (POWERTHRESHOLD,paramValue);
	g_PowerThreshold = atoi(paramValue);     

	AsyncSaveData();
	if(g_PowerThreshold != 0){
	    pActionRequest->ActionResult = NULL;
	   pActionRequest->ErrCode = 0x00;
	    UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetPowerThreshold", 
			CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"PowerThreshold" , paramValue);  
	    APP_LOG("UPNPDevice", LOG_DEBUG, "Sending Local Notification for SetPowerThreshold: %d",g_PowerThreshold) ;
	    //LocalPowerThresholdNotify(Threshold);
	    pMessage msg = 0x00;
	    msg = createMessage(UPNP_MESSAGE_PWRTHRES_IND, 0x00, 0x00);
	    SendMessage2App(msg);
	    g_SendInsightParams = 1;
	}
	FreeXmlSource(paramValue);
	return UPNP_E_SUCCESS;
}

void* setHomeSettingAction(void *args)
{
    int	cntdiscoveryCounter = 2;
    char paramVersion[SIZE_16B] = {'\0'};
    char paramPerUnitCost[SIZE_16B] = {'\0'};	
    char paramCurrency[SIZE_16B] = {'\0'};

    snprintf (paramVersion,  sizeof(paramVersion), "%d",g_PerUnitCostVer);
    snprintf (paramPerUnitCost,  sizeof(paramPerUnitCost), "%d",g_perUnitCost);
    snprintf (paramCurrency,  sizeof(paramCurrency), "%d", g_Currency);

    char*  values[3];
    char* paramsList[] = {"EnergyPerUnitCost", "Currency", "EnergyPerUnitCostVersion"};

    values[0] = paramPerUnitCost;
    values[1] = paramCurrency;
    values[2] = paramVersion;

    simulatedStartControlPoint();

    while(cntdiscoveryCounter)
    {
	/* always discover in this case */
	CtrlPointDiscoverDevices();
	APP_LOG("Rule", LOG_DEBUG, "discovery done");

	/* sleep for a while to allow device discovery and creation of device list */
	pluginUsleep((MAX_DISCOVER_TIMEOUT + 1)*1000000);

	cntdiscoveryCounter--;
    }

    PluginCtrlPointSendActionAll(PLUGIN_E_EVENT_SERVICE, "UpdateInsightHomeSettings", paramsList, values, 0x03);

    if(!gProcessData)
	StopPluginCtrlPoint();

    return NULL;
}

int SetInsightHomeSettings(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	char *paramPerUnitcost = NULL,*paramCurrency = NULL;
	char NewVersion[SIZE_16B];
	int version = 0, cost = 0;
	char VersionStr[SIZE_16B] = {'\0'};
	char PerUnitcost[SIZE_16B] = {'\0'};
	char Currency[SIZE_16B] = {'\0'};
	int retVal = FAILURE;

	if (0x00 == pActionRequest || 0x00 == request)
	{
		APP_LOG("UPNPDevice", LOG_DEBUG, "SetInsightHomeSettings: command paramter invalid");
		return 0x01;
	}

	paramPerUnitcost = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "EnergyPerUnitCost");
	if (0x00 == paramPerUnitcost || 0x00 == strlen(paramPerUnitcost))
	{
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x01;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetInsightHomeSettings",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "EnergyPerUnitCost", paramPerUnitcost);
		return 0x00;
	}
	paramCurrency = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Currency");
	if (0x00 == paramCurrency || 0x00 == strlen(paramCurrency))
	{
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x01;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetInsightHomeSettings",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "Currency", paramCurrency);
		FreeXmlSource(paramPerUnitcost);
		return 0x00;
	}

	InsightHomeSettings(paramPerUnitcost, paramCurrency);

	snprintf (VersionStr,  sizeof(VersionStr), "%d",g_PerUnitCostVer);
	snprintf (PerUnitcost,  sizeof(PerUnitcost), "%d",g_perUnitCost);
	snprintf (Currency,  sizeof(Currency), "%d", g_Currency);

	APP_LOG("UPNPDevice", LOG_DEBUG, "SetInsightHomeSettings: VersionStr: %s,PerUnitcost: %s,Currency: %s", VersionStr,PerUnitcost,Currency);
	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetInsightHomeSettings",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"EnergyPerUnitCost" , PerUnitcost);
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetInsightHomeSettings", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "Currency", Currency);


	pthread_attr_init(&setHomeSettings_attr);
	pthread_attr_setdetachstate(&setHomeSettings_attr,PTHREAD_CREATE_DETACHED);
	retVal = pthread_create(&setHomeSettings_thread,&setHomeSettings_attr,(void*)&setHomeSettingAction, NULL);
	if(retVal < SUCCESS)
	{
	    APP_LOG("UPnPApp",LOG_ERR, "************Set Insight home setting thread not Created");
	    retVal = FAILURE;
	}

	FreeXmlSource(paramPerUnitcost);
	FreeXmlSource(paramCurrency);
	return SUCCESS;
}

int UpdateInsightHomeSettings(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	char *paramPerUnitcost = NULL,*paramCurrency = NULL, *paramVersion = NULL;
	char VersionStr[SIZE_16B] = {'\0'};
	char PerUnitcost[SIZE_16B] = {'\0'};
	char Currency[SIZE_16B] = {'\0'};
	char energyStr[SIZE_128B] = {'\0'};
	
	if (0x00 == pActionRequest || 0x00 == request)
	{
		APP_LOG("UPNPDevice", LOG_DEBUG, "UpdateInsightHomeSettings: command paramter invalid");
		return 0x01;
	}

	paramPerUnitcost = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "EnergyPerUnitCost");
	if (0x00 == paramPerUnitcost || 0x00 == strlen(paramPerUnitcost))
	{
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x01;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "UpdateInsightHomeSettings",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "EnergyPerUnitCost", paramPerUnitcost);
		return 0x00;
	}
	paramCurrency = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Currency");
	if (0x00 == paramCurrency || 0x00 == strlen(paramCurrency))
	{
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x01;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "UpdateInsightHomeSettings",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "Currency", paramCurrency);
		FreeXmlSource(paramPerUnitcost);
		return 0x00;
	}

	paramVersion = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "EnergyPerUnitCostVersion");
	if (0x00 == paramVersion || 0x00 == strlen(paramVersion))
	{
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x01;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "UpdateInsightHomeSettings",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "EnergyPerUnitCostVersion", paramVersion);
		FreeXmlSource(paramPerUnitcost);
		FreeXmlSource(paramCurrency);
		return 0x00;
	}

	snprintf(energyStr, SIZE_128B, "%s|%s|%s", paramVersion, paramPerUnitcost, paramCurrency);

	ProcessEnergyPerunitCostNotify(energyStr);
	//InsightHomeSettings(paramPerUnitcost, paramCurrency);

	snprintf (VersionStr,  sizeof(VersionStr), "%d",g_PerUnitCostVer);
	snprintf (PerUnitcost,  sizeof(PerUnitcost), "%d",g_perUnitCost);
	snprintf (Currency,  sizeof(Currency), "%d", g_Currency);

	APP_LOG("UPNPDevice", LOG_DEBUG, "UpdateInsightHomeSettings: VersionStr: %s,PerUnitcost: %s,Currency: %s", VersionStr,PerUnitcost,Currency);
	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "UpdateInsightHomeSettings",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"EnergyPerUnitCost" , PerUnitcost);
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "UpdateInsightHomeSettings",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"Currency", Currency);
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "UpdateInsightHomeSettings",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"EnergyPerUnitCostVersion", VersionStr);

	FreeXmlSource(paramPerUnitcost);
	FreeXmlSource(paramCurrency);
	FreeXmlSource(paramVersion);
	return UPNP_E_SUCCESS;


}

void InsightHomeSettings(char *paramPerUnitcost, char *paramCurrency)
{
	int cost = 0, retVal = FAILURE;
	char NewVersion[SIZE_16B];

	cost = atoi(paramPerUnitcost);
	if(cost)
	{
		APP_LOG("UPNPDevice", LOG_DEBUG, "SetInsightHomeSettings: Update cost Version");
		SetBelkinParameter(ENERGYPERUNITCOST,paramPerUnitcost);
		g_perUnitCost = atoi(paramPerUnitcost);
		SetBelkinParameter(CURRENCY,paramCurrency);
		g_Currency = atoi(paramCurrency);
	}
	else//reset to default 
	{
		APP_LOG("UPNPDevice", LOG_DEBUG, "SetInsightHomeSettings: Reset Version");
		SetBelkinParameter(ENERGYPERUNITCOST,DEFAULT_ENERGYPERUNITCOST);
		g_perUnitCost = atoi(DEFAULT_ENERGYPERUNITCOST); 
		SetBelkinParameter(CURRENCY,DEFAULT_CURRENCY);
		g_Currency = atoi(DEFAULT_CURRENCY);
	}

	memset(NewVersion, 0x0, sizeof(NewVersion));
	snprintf(NewVersion, sizeof(NewVersion), "%d", g_PerUnitCostVer+1);

	SetBelkinParameter(ENERGYPERUNITCOSTVERSION,NewVersion); 
	g_PerUnitCostVer++;

	AsyncSaveData();

	g_InsightHomeSettingsSend = 1;
}

int GetInsightHomeSettings(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	char paramCurrency[SIZE_16B] = {'\0'};
	char paramVersion[SIZE_16B] = {'\0'};
	char paramPerUnitcost[SIZE_16B] = {'\0'};
	
	snprintf(paramCurrency, sizeof(paramCurrency), "%d", g_Currency);
	snprintf(paramVersion,  sizeof(paramVersion), "%d",g_PerUnitCostVer);
	snprintf(paramPerUnitcost,  sizeof(paramPerUnitcost), "%d",g_perUnitCost);

	APP_LOG("UPNPDevice", LOG_DEBUG, "GetInsightHomeSettings: paramVersion: %s,paramPerUnitcost: %s,paramCurrency: %s", paramVersion,paramPerUnitcost,paramCurrency);

	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetInsightHomeSettings",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"HomeSettingsVersion" , paramVersion);
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetInsightHomeSettings",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"EnergyPerUnitCost" , paramPerUnitcost);
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetInsightHomeSettings",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"Currency", paramCurrency);

	return UPNP_E_SUCCESS;
}

int GetInsightParams(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
    char szResponse[SIZE_256B];
    int curState = 0x00;
    memset(szResponse, 0x00, sizeof(szResponse));
    APP_LOG("UPNP: Device", LOG_DEBUG, "%s called", __FUNCTION__);
    pActionRequest->ActionResult = NULL;
    pActionRequest->ErrCode = 0x00;

    curState = g_PowerStatus;
    if(curState)
    {
	    APP_LOG("UPNPDevice", LOG_ERR,"Switch State: ON");
    }
    else	
    {
	    APP_LOG("UPNPDevice", LOG_ERR,"Switch State: OFF");
    }
    
    if (g_InitialMonthlyEstKWH){    
	    snprintf(szResponse, sizeof(szResponse), "%d|%u|%u|%u|%u|%u|%u|%u|%u|%d|%d",
			    curState,g_StateChangeTS,g_ONFor,g_TodayONTimeTS ,g_TotalONTime14Days,g_HrsConnected,g_AvgPowerON,g_PowerNow,g_AccumulatedWattMinute,g_InitialMonthlyEstKWH,g_PowerThreshold);}
    else{
	    snprintf(szResponse, sizeof(szResponse), "%d|%u|%u|%u|%u|%u|%u|%u|%u|%f|%d", 
			    curState,g_StateChangeTS,g_ONFor,g_TodayONTimeTS ,g_TotalONTime14Days,g_HrsConnected,g_AvgPowerON,g_PowerNow,g_AccumulatedWattMinute,g_KWH14Days,g_PowerThreshold);}

    APP_LOG("UPNP: Device", LOG_DEBUG, "%s", szResponse);

    UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetInsightParams", CtrleeDeviceServiceType[PLUGIN_E_METAINFO_SERVICE], 
	    "InsightParams", szResponse);  

    return UPNP_E_SUCCESS;
}

int ScheduleDataExport(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	if (pActionRequest == 0x00)
	{
		APP_LOG("UPNP: Device", LOG_DEBUG,"ScheduleDataExport: paramter failure");
		return PLUGIN_ERROR_E_BASIC_EVENT;
	}

	char* szEmailAddress = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "EmailAddress");
	char* szDataExportType = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "DataExportType");

	if (0x00 == strlen(szEmailAddress))
	{
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = PLUGIN_ERROR_E_BASIC_EVENT;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "ScheduleDataExport", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"EmailAddress", "Parameter Error");
		APP_LOG("UPNP: Device", LOG_ERR,"ScheduleDataExport: parameter error, szEmailAddress");
		return PLUGIN_ERROR_E_BASIC_EVENT;
	}

	if (0x00 == strlen(szDataExportType))
	{
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = PLUGIN_ERROR_E_BASIC_EVENT;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "ScheduleDataExport", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"DataExportType", "Parameter Error");
		APP_LOG("UPNP: Device", LOG_ERR,"ScheduleDataExport: parameter error, szDataExportType");
		FreeXmlSource(szEmailAddress);
		return PLUGIN_ERROR_E_BASIC_EVENT;
	}

	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "ScheduleDataExport", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "EmailAddress", szEmailAddress);
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "ScheduleDataExport", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "DataExportType", szDataExportType);

	ExecuteInsightDataExport(szEmailAddress,szDataExportType);

	FreeXmlSource(szEmailAddress);
	FreeXmlSource(szDataExportType);
	return UPNP_E_SUCCESS;
}

int GetDataExportInfo(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	char * Value = NULL; 
	char szResponse[SIZE_256B];
	memset(szResponse, 0x00, sizeof(szResponse));
	APP_LOG("UPNP: Device", LOG_DEBUG, "%s called", __FUNCTION__);

	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;

	Value = GetBelkinParameter(DB_LAST_EXPORT_TS);
	APP_LOG("UPNP: Device", LOG_DEBUG, "Last Export TS:%s", Value);
	if(strlen(Value))
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetDataExportInfo", CtrleeDeviceServiceType[PLUGIN_E_INSIGHT_SERVICE], "LastDataExportTS", Value);
	else
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetDataExportInfo", CtrleeDeviceServiceType[PLUGIN_E_INSIGHT_SERVICE], "LastDataExportTS", "0");

	Value = GetBelkinParameter(DATA_EXPORT_EMAIL_ADDRESS);
	APP_LOG("UPNP: Device", LOG_DEBUG, "Email Address:%s", Value);
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetDataExportInfo", CtrleeDeviceServiceType[PLUGIN_E_INSIGHT_SERVICE], "EmailAddress", Value);

	snprintf(szResponse,sizeof(szResponse), "%d", g_s32DataExportType);
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetDataExportInfo", CtrleeDeviceServiceType[PLUGIN_E_INSIGHT_SERVICE], "DataExportType", szResponse);
	APP_LOG("UPNP: Device", LOG_DEBUG, "Data Export Type:%s", szResponse);

	return UPNP_E_SUCCESS;
}

int TokenParser(char *dst[], char *src, int noCount)
{
    int i =0;
    char *ptr = NULL;
    if(!src)
        return 0;
    for (i=0;i<noCount;i++)
    {
            ptr = dst[i];
            while((*src != '|') && (*src != '\0'))
            {
                    *ptr++ = *src++;
            }
            *ptr = '\0';
            if(src)
                    src++;
            else
                    break;
    }
    return i;
}

void ProcessEnergyPerunitCostNotify(char * apEnergyPerUnitCost)
{
	int SettingsVersion = 0, OldVersion = 0;

	char HomeSettingsVersion[MAX_OCT_LEN];
        char HomeSettingsCost[MAX_OCT_LEN];
        char HomeSettingsCurrency[MAX_OCT_LEN];
        char *valuestr[3] = {HomeSettingsVersion,HomeSettingsCost,HomeSettingsCurrency};

	if ((0x00 != apEnergyPerUnitCost) && (0x00 != strlen(apEnergyPerUnitCost)))
	{
		memset(HomeSettingsVersion, 0x0, sizeof(HomeSettingsVersion));
		memset(HomeSettingsCost, 0x0, sizeof(HomeSettingsCost));
		memset(HomeSettingsCurrency, 0x0, sizeof(HomeSettingsCurrency));

		TokenParser(valuestr, apEnergyPerUnitCost, 3);
		APP_LOG("ProcessEnergyPerunitCostNotify", LOG_DEBUG, "HomeSettingsVersion: %s,HomeSettingsCost: %s,HomeSettingsCurrency: %s", HomeSettingsVersion,HomeSettingsCost,HomeSettingsCurrency);

		SettingsVersion = atoi(HomeSettingsVersion);
		if(SettingsVersion)
		{
			OldVersion = g_PerUnitCostVer;
			if (SettingsVersion > OldVersion)
			{
				APP_LOG("ProcessEnergyPerunitCostNotify", LOG_DEBUG, "UPDATE ENERGY PER UNIT COST: <%d>", SettingsVersion);
				SetBelkinParameter(ENERGYPERUNITCOSTVERSION,HomeSettingsVersion); 
				g_PerUnitCostVer = atoi(HomeSettingsVersion);
				SetBelkinParameter(ENERGYPERUNITCOST,HomeSettingsCost);  
				g_perUnitCost = atoi(HomeSettingsCost);

				SetBelkinParameter(CURRENCY,HomeSettingsCurrency);
				g_Currency = atoi(HomeSettingsCurrency);
                
				AsyncSaveData();

			}
			else
				APP_LOG("ProcessEnergyPerunitCostNotify", LOG_DEBUG, "NO UPDATE ENERGY PER UNIT COST: <%d>", OldVersion);
		}
	}
}

void SendHomeSettingChangeMsg()
{
	pMessage msg = 0x00;
        msg = createMessage(UPNP_MESSAGE_ENERGY_COST_CHANGE_IND, NULL, 0);
        SendMessage2App(msg);
	APP_LOG("SendHomeSettingChangeMsg", LOG_DEBUG, "ITC message send for HomeSettingChange");
}

#endif//PRODUCT_WeMo_Insight
