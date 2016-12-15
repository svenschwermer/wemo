/***************************************************************************
*
*
* insightUPnPHandler.h
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
#ifndef INSIGHT_UPNP_HANDLER_H_
#define INSIGHT_UPNP_HANDLER_H_

#define POWERTHRESHOLD                  "PowerThreshold"
#define PUSHTOAPPTIME			"PushToAppTime"
#define PUSHTOCLOUDTIME			"PushToCloudTime"
#define TODAYKWH			"TodayKWH"
#define TODAYKWHON 			"TodayKWHON"
#define TODAYKWHSBY			"TodayKWHSBY"
#define TODAYONTIME			"TodayONTime"
#define TODAYONTIMETS			"TodayONTimeTS"
#define TODAYSBYTIMETS			"TodaySBYTimeTS"
#define TODAYSBYTIME			"TodaySBYTime"
#define TODAYSDATE	    		"TodayDate"
#define DAYSCOUNT	       		"DaysCount"
#define WRITETOFLASHTIME	    	"WriteToFlashTime"
#define	DEFAULT_POWERTHRESHOLD		"8000"	// 8 watt
#define	DEFAULT_PUSHTOAPPTIME		"2"	// 2 seconds
#define	DEFAULT_PUSHTOCLOUDTIME		"60"	// 60 seconds
#define	DEFAULT_FLASHWRITE		"1800"	// 30 minutes
#define	DEFAULT_DAY			"1"	// 1st day of Month
#define	DEFAULT_TODAYONTIME		"0"	// 0 Seconds
#define	DEFAULT_TODAYONTIMETS		"0"	// 0 Seconds
#define	DEFAULT_TODAYSBYTIME		"0"	// 0 Seconds
#define	DEFAULT_ENERGYPERUNITCOST	"111"	// 0.111$//per KWH
#define	DEFAULT_ENERGYPERUNITCOSTVER	"1"	// 1
#define	DEFAULT_CURRENCY		"1"	// $
#define	DEFAULT_EVENT_STATUS		"0"	// Disable
#define	DEFAULT_EVENT_DURATION		"1800"	// 30 Minutes
#define ENERGYPERUNITCOSTVERSION	"EnergyPerUnitCostVersion"
#define ENERGYPERUNITCOST	    	"EnergyPerUnitCost"
#define CURRENCY		    	"Currency"
#define SETUP_COMPLETE_TS		"SetupCompleteTS"
//only for testing
#define INSIGHT_DATA_FILE_URL           "/tmp/InsightData.csv"
#define DB_LAST_EXPORT_TS  		"DBLastExportTS"
#define DATA_EXPORT_EMAIL_ADDRESS	"DataExportEmailAddress"
#define DATA_EXPORT_TYPE		"DataExportType"
#define INSIGHT_EVENT_ENABLE		"EventEnable"
#define INSIGHT_EVENT_DURATION		"EventDuration"
#define ENABLE_STATE_LOG                "StateLog"

extern char	    g_NoNotificationFlag;//Don't Send Local Push Notification When Sending State Notification
extern unsigned int g_ONFor;
extern unsigned int g_ONForChangeFlag;
extern unsigned int g_RuleONFor;
extern unsigned int g_ONForTS;
extern unsigned int g_InSBYSince;
extern unsigned int g_TodayONTime;
extern unsigned int g_TodayONTimeTS;
extern unsigned int g_TodayKWHON;
extern unsigned int g_TodaySBYTime;
extern unsigned int g_TodaySBYTimeTS;
extern unsigned int g_TodayKWHSBY;
extern unsigned int g_YesterdayONTime;
extern unsigned int g_YesterdaySBYTime;
extern unsigned int g_YesterdayKWHON;
extern unsigned int g_YesterdayKWHSBY;
extern time_t	    g_YesterdayTS;
extern unsigned int g_PowerThreshold;
extern unsigned int g_StateChangeTS;
extern unsigned int g_TotalONTime14Days;
extern unsigned int g_HrsConnected;
extern unsigned int g_AvgPowerON;
extern double 	    g_KWH14Days;
extern unsigned int g_TodayTotalKWH;
extern unsigned int g_AccumulatedWattMinute;
extern unsigned int g_PowerNow;
extern unsigned long int g_SetUpCompleteTS;
extern 		int g_InitialMonthlyEstKWH;
extern unsigned int g_ReStartDataExportScheduler;
extern unsigned int g_Cost;
extern char g_s8EmailAddress[256];
extern unsigned int g_APNSLastState;
extern unsigned int g_InsightDataRestoreState;

/** Typedef enum for data export type */
typedef enum {
        E_SEND_DATA_NOW = 0x0,
        E_SEND_DATA_DAILY,
        E_SEND_DATA_WEEKLY,
        E_SEND_DATA_MONTHLY,
        E_TURN_OFF_SCHEDULER
}eDATA_EXPORT_TYPE;

typedef struct DATA_EXPORT_INFO_
{
       char EmailAddress[SIZE_256B];
       eDATA_EXPORT_TYPE DataExportType;
}DATA_EXPORT_INFO,*PDATA_EXPORT_INFO;

extern eDATA_EXPORT_TYPE g_s32DataExportType;
extern unsigned int g_InsightHomeSettingsSend;
extern unsigned int g_InsightCostCur;
extern char 	    g_InsightSettingsSend;

int SetInsightHomeSettings(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetInsightHomeSettings(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetPower(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetONFor(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetInSBYSince(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetTodayONTime(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetTodaySBYTime(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetTodayKWH(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetInsightInfo(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetInsightParams(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetPowerThreshold(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int SetPowerThreshold(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int SetAutoPowerThreshold(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int ResetPowerThreshold(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int ScheduleDataExport(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetDataExportInfo(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int UpdateInsightHomeSettings(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
void LocalInstPowerNotify(int curState);
void LocalPowerThresholdNotify(unsigned int curState);
void LocalInsightHomeSettingNotify();
void SendHomeSettingChangeMsg();
void LocalInsightParamsNotify(void);
void UPnPInternalToggleUpdate(int curState);
int TokenParser(char *dst[], char *src, int noCount);
void ProcessEnergyPerunitCostNotify(char * apEnergyPerUnitCost);

#endif//INSIGHT_UPNP_HANDLER_H_

#endif//PRODUCT_WeMo_Insight
