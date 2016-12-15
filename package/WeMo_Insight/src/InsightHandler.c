/***************************************************************************
*
*
*
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

/*
 * InsightHandler.c
 *
 *  Created on: Nov 5, 2012
 *      Author: Anand
 */

#include <sys/time.h>
#include <pthread.h>
#include "InsightHandler.h"
#include "insight.h"
#include "osUtils.h"
#include "controlledevice.h"
#include "global.h"
#include "logger.h"
#include "gpio.h"
#include "wifiHndlr.h"
#include "httpsWrapper.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "types.h"
#include "defines.h"
#include "remoteAccess.h"
#include "insightRemoteAccess.h"
#include "mxml.h"
#include "utils.h"
#include "WemoDB.h"
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */


#define MAX_STATE_COUNT			2
#define HRS_SEC_24			86400
#define DAYS_SEC_14			1209600
//static valiables
static unsigned long	s_lLastInsightUpdateTime 	= 0x00;
static unsigned long	s_lLastCloudUpdateTime	 	= 0x00;
static unsigned long	s_lLastFlashWrite		= 0x00;
static unsigned long	s_AdjustData			= 0x00;
static unsigned int     s_RunTimeWhenOn = 0;
static double 		s_LastKWHWhenOn = 0;
static double 		s_KWHONFor = 0;
static double  		s_TotalKWHSinceIsInsightUP = 0;
static unsigned int 	s_FirstTimeStateChange = 1;
static time_t 		s_PrevTime = 0;
static unsigned int     s_Last30MinKWH = 0;
static unsigned int     s_30MinDBEntryCntr = 0;
static unsigned int     s_PerDayDBEntryCntr = 0;
static unsigned int     s_StateChangeDBEntryCntr = 0;
static unsigned short   s_30MinDBEntryRollOverflag = 0;
static unsigned short   s_PerDayDBEntryRollOverflag = 0;
static unsigned short   s_StateChangeDBEntryRollOverflag = 0;
static time_t 		s_LastDBUpdatedTS = 0;
static int 		s_ClearUsageData = 0;
static unsigned int 	s_InsightONOnce = 0;
static unsigned int     s_TotalONTime13Days = 0;
static double 		s_KWH13Days = 0;
static double		s_ONForCalc = 0;
static unsigned long int s_LastTimeStamp=0;
static unsigned int s_LastRuntime=0;
static double s_LastKWHconsumed=0;
static unsigned int s_LastDeviceState=0;
static unsigned int s_StateChangeONCntr=0;
static unsigned int s_StateChangeSBYCntr=0;


//global variables
extern unsigned int g_Currency;
extern unsigned int g_PerUnitCostVer;
extern unsigned int g_perUnitCost;

unsigned int	g_PushToAppTime = 0;
unsigned int	g_TodayDate = 0;
unsigned int	g_WriteToFlashTime = 0;
unsigned int	g_PushToCloudTime = 0;
char 		g_SendToCloud = 1;//Send Info to cloud for first time when device connects to cloud
int 		gInsightHealthPunch = 0;

//thread variables
pthread_t	Insight_thread = -1;
pthread_t	Insight_ExportDatathread = -1;
pthread_t	DataExport_Send_thread = -1;
pthread_t	Insight_Eventthread = -1;

/*data export mutex*/
pthread_mutex_t s_DataExport_mutex;

void EventSleep(int EventDuration);

void FetchInsightParams(char *InsightParams)
{

    snprintf(InsightParams,SIZE_256B, "%u|%u|%u|%u|%d|%lu|%lu|%lu|%u|%d|%d|%d",s_30MinDBEntryRollOverflag,s_PerDayDBEntryRollOverflag,s_30MinDBEntryCntr,s_PerDayDBEntryCntr,g_HrsConnected,s_lLastFlashWrite,s_LastDBUpdatedTS,g_SetUpCompleteTS,g_EventEnable,g_InsightDataRestoreState,g_s32DataExportType,g_isInsightRuleActivated);

}
void PopulateDBCntrAndFlags(void)
{
	unsigned short NumberOfRowsWritten;

	/*Fetch Insight30MinData DB Cntr And Flags*/ 
	FetchDBCntrAndFlags("Insight30MinData", "ROWID", "TIMESTAMP", &s_30MinDBEntryCntr); 
	FetchDBCntrAndFlags("Insight30MinData", "ROWID", "ROWID", (unsigned int *)&NumberOfRowsWritten); 
	if(MAX_30MIN_ENTRY == NumberOfRowsWritten)
		s_30MinDBEntryRollOverflag = 1;

	/*Fetch InsightDayInfo DB Cntr And Flags*/ 
	FetchDBCntrAndFlags("InsightDayInfo", "ROWID", "TIMESTAMP", &s_PerDayDBEntryCntr); 
	FetchDBCntrAndFlags("InsightDayInfo", "ROWID", "ROWID", (unsigned int *)&NumberOfRowsWritten); 
	FetchDBCntrAndFlags("InsightDayInfo", "TODAYONTIME", "TIMESTAMP", &g_YesterdayONTime); 
	FetchDBCntrAndFlags("InsightDayInfo", "TODAYONKWH", "TIMESTAMP", &g_YesterdayKWHON); 
	FetchDBCntrAndFlags("InsightDayInfo", "TODAYSBYTIME", "TIMESTAMP", &g_YesterdaySBYTime); 
	FetchDBCntrAndFlags("InsightDayInfo", "TODAYSBYKWH", "TIMESTAMP", &g_YesterdayKWHSBY); 
	FetchDBCntrAndFlags("InsightDayInfo", "TIMESTAMP", "TIMESTAMP", (unsigned int*)&g_YesterdayTS); 
	if(g_YesterdayTS == 0)
	{
	    g_YesterdayTS = time(NULL) - 86400;// Today time in seconds - 24 hr seconds
	}
	if(MAX_PERDAY_ENTRY == NumberOfRowsWritten)
		s_PerDayDBEntryRollOverflag = 1;

	/*Fetch InsightStateChangeInfo DB Cntr And Flags*/ 
	FetchDBCntrAndFlags("InsightStateChangeInfo", "ROWID", "TIMESTAMP", &s_StateChangeDBEntryCntr); 
	FetchDBCntrAndFlags("InsightStateChangeInfo", "ROWID", "ROWID", (unsigned int *)&NumberOfRowsWritten); 
	if(MAX_STATE_CHANGE_ENTRY == NumberOfRowsWritten)
		s_StateChangeDBEntryRollOverflag = 1;
	
	APP_LOG("PopulateDBCntrAndFlags",LOG_DEBUG,"s_30MinDBEntryCntr: %d", s_30MinDBEntryCntr);
	APP_LOG("PopulateDBCntrAndFlags",LOG_DEBUG,"s_30MinDBEntryRollOverflag: %d", s_30MinDBEntryRollOverflag);
	APP_LOG("PopulateDBCntrAndFlags",LOG_DEBUG,"s_PerDayDBEntryCntr: %d", s_PerDayDBEntryCntr);
	APP_LOG("PopulateDBCntrAndFlags",LOG_DEBUG,"s_PerDayDBEntryRollOverflag: %d", s_PerDayDBEntryRollOverflag);
	APP_LOG("PopulateDBCntrAndFlags",LOG_DEBUG,"s_StateChangeDBEntryCntr: %d", s_StateChangeDBEntryCntr);
	APP_LOG("PopulateDBCntrAndFlags",LOG_DEBUG,"s_StateChangeDBEntryRollOverflag: %d", s_StateChangeDBEntryRollOverflag);
	APP_LOG("PopulateDBCntrAndFlags",LOG_DEBUG,"g_YesterdayKWHON: %d", g_YesterdayKWHON);
	APP_LOG("PopulateDBCntrAndFlags",LOG_DEBUG,"g_YesterdayONTime: %d", g_YesterdayONTime);
	APP_LOG("PopulateDBCntrAndFlags",LOG_DEBUG,"g_YesterdaySBYTime: %d", g_YesterdaySBYTime);
	APP_LOG("PopulateDBCntrAndFlags",LOG_DEBUG,"g_YesterdayKWHSBY: %d", g_YesterdayKWHSBY);
	APP_LOG("PopulateDBCntrAndFlags",LOG_DEBUG,"g_YesterdayTS: %u", g_YesterdayTS);
}

void FetchDBCntrAndFlags(char * pFromTable, char * pColumn, char * pOrderBy, unsigned int * pUpdateVariable)
{
        int s32NoOfRows=0,s32NoOfCols=0;
        char **s8ResultArray=NULL;
        char query[SIZE_256B] = {0,};

        snprintf(query, sizeof(query), "select %s from %s order by %s desc limit 1;", pColumn, pFromTable, pOrderBy);
	APP_LOG("PopulateDBCntrAndFlags", LOG_DEBUG, "query: %s", query);
	if(!WeMoDBGetTableData(&g_InsightDB,query,&s8ResultArray,&s32NoOfRows,&s32NoOfCols))
	{
		if(s32NoOfRows && s32NoOfCols)
			*pUpdateVariable = atoi(s8ResultArray[1]);
		else
			*pUpdateVariable = 0;
		WeMoDBTableFreeResult(&s8ResultArray,&s32NoOfRows,&s32NoOfCols);
        }
}

void Populate14DaysData(void)
{
	unsigned int TempOnTime = 0;
	double TempOnKWH = 0;
        int s32NoOfRows=0,s32NoOfCols=0;
        int s32RowCntr=0,s32ArraySize=0, MAX_ROWS = 13;
        char **s8ResultArray=NULL;
        char query[SIZE_256B];

	APP_LOG("APP", LOG_DEBUG, "Populate 14 days data");

	/*calculate last 13 days on time and KWH*/
        memset(query, 0, sizeof(query));
	snprintf(query, sizeof(query), "select TODAYONTIME,TODAYONKWH from InsightDayInfo order by TIMESTAMP desc limit %d;", MAX_ROWS);
	APP_LOG("APP", LOG_DEBUG, "query: %s", query);
	if(!WeMoDBGetTableData(&g_InsightDB,query,&s8ResultArray,&s32NoOfRows,&s32NoOfCols))
	{
		s32ArraySize = (s32NoOfRows + 1)*s32NoOfCols;
		if(s32ArraySize)
		{
			for(s32RowCntr = 2; s32RowCntr < s32ArraySize; s32RowCntr++)
			{
				TempOnTime += strtoul(s8ResultArray[s32RowCntr],NULL,0); 
				TempOnKWH  += atof(s8ResultArray[++s32RowCntr]);
			}
		}
		WeMoDBTableFreeResult(&s8ResultArray,&s32NoOfRows,&s32NoOfCols);
	}

	g_TotalONTime14Days = TempOnTime + g_TodayONTime;
	s_TotalONTime13Days = TempOnTime;

	g_KWH14Days = TempOnKWH + g_TodayKWHON + g_TodayKWHSBY;
	s_KWH13Days = TempOnKWH;
	APP_LOG("Populate14DaysData:", LOG_DEBUG, "TempOnTime: %u, TempOnKWH: %f, g_TotalONTime14Days: %u, s_TotalONTime13Days: %u, g_KWH14Days: %f, s_KWH13Days: %f", TempOnTime,TempOnKWH,g_TotalONTime14Days,s_TotalONTime13Days,g_KWH14Days,s_KWH13Days);
}

void PopulateAvgWhenOn(void)
{
	unsigned int TempOnTime = 0;
	double TempOnKWH = 0;
        int s32NoOfRows=0,s32NoOfCols=0;
        int s32RowCntr=0,s32ArraySize=0, MAX_ROWS = 2;
        char **s8ResultArray=NULL;
        char query[SIZE_256B];
        memset(query, 0, sizeof(query));

	APP_LOG("APP", LOG_DEBUG, "Populate Avrg When On");

	/*calculate last on time and KWH*/
	snprintf(query, sizeof(query), "select RUNTIME,KWHCONSUMED from InsightStateChangeInfo where STATE like %d order by TIMESTAMP desc limit %d;", POWER_ON, MAX_ROWS);
	APP_LOG("APP", LOG_DEBUG, "query: %s", query);
	if(!WeMoDBGetTableData(&g_InsightDB,query,&s8ResultArray,&s32NoOfRows,&s32NoOfCols))
	{
		s32ArraySize = (s32NoOfRows + 1)*s32NoOfCols;
		if(s32ArraySize)
		{
			for(s32RowCntr = 2; s32RowCntr < s32ArraySize; s32RowCntr++)
			{
				TempOnTime += strtoul(s8ResultArray[s32RowCntr],NULL,0);
				TempOnKWH += atof(s8ResultArray[++s32RowCntr]);
			}
		}
		WeMoDBTableFreeResult(&s8ResultArray,&s32NoOfRows,&s32NoOfCols);
	}
	s_RunTimeWhenOn = TempOnTime;
	s_LastKWHWhenOn = TempOnKWH; 

	APP_LOG("APP", LOG_DEBUG, "TempOnTime: %u, TempOnKWH: %f, s_RunTimeWhenOn: %u, s_LastKWHWhenOn: %f", TempOnTime,TempOnKWH,s_RunTimeWhenOn,s_LastKWHWhenOn);
	calculateAvgOn();
}

void PopulateInsightVariable(char * pParam, char * pDefaultParam, int aSetOrGet, unsigned int * pUpdateVariable)
{
	char* tempchar = NULL;

	tempchar = GetBelkinParameter(pParam);
	if((NULL == tempchar) || (0 == strlen(tempchar)))
	{
		APP_LOG("APP", LOG_DEBUG, "setting %s", pParam);
		SetBelkinParameter (pParam,pDefaultParam);
		if(aSetOrGet)
			*pUpdateVariable = strtoul(pDefaultParam, NULL, 0);
	}
	else
	{
		if(aSetOrGet)
		{
			*pUpdateVariable = strtoul(tempchar, NULL, 0);
		}
	}
}

void InitInsightVariables(void)
{
	char* tempchar = NULL;
	
	/*Load All the global variables from Memory Before starting the power monitoring thread.*/
	PopulateInsightVariable(POWERTHRESHOLD, DEFAULT_POWERTHRESHOLD, 1, &g_PowerThreshold);
	PopulateInsightVariable(PUSHTOAPPTIME, DEFAULT_PUSHTOAPPTIME, 1, &g_PushToAppTime);
	PopulateInsightVariable(PUSHTOCLOUDTIME, DEFAULT_PUSHTOCLOUDTIME, 1, &g_PushToCloudTime);
	PopulateInsightVariable(TODAYONTIME, DEFAULT_TODAYONTIME, 1, &g_TodayONTime);
	PopulateInsightVariable(TODAYONTIMETS, DEFAULT_TODAYONTIMETS, 1, &g_TodayONTimeTS);
	PopulateInsightVariable(TODAYSBYTIME, DEFAULT_TODAYSBYTIME, 1, &g_TodaySBYTime);
	PopulateInsightVariable(TODAYSBYTIMETS, DEFAULT_TODAYSBYTIME, 1, &g_TodaySBYTimeTS);
	PopulateInsightVariable(TODAYSDATE, DEFAULT_DAY, 1, &g_TodayDate);
	PopulateInsightVariable(WRITETOFLASHTIME, DEFAULT_FLASHWRITE, 1, &g_WriteToFlashTime);
	PopulateInsightVariable(CURRENCY, DEFAULT_CURRENCY, 1, &g_Currency);
	PopulateInsightVariable(ENERGYPERUNITCOST, DEFAULT_ENERGYPERUNITCOST, 1, &g_perUnitCost);
	PopulateInsightVariable(ENERGYPERUNITCOSTVERSION, DEFAULT_ENERGYPERUNITCOSTVER, 1, &g_PerUnitCostVer);
	PopulateInsightVariable(DAYSCOUNT, "0", 0, NULL);
	PopulateInsightVariable(INSIGHT_EVENT_ENABLE, DEFAULT_EVENT_STATUS, 1, &g_EventEnable);
	PopulateInsightVariable(INSIGHT_EVENT_DURATION, DEFAULT_EVENT_DURATION, 1, &g_EventDuration);
	PopulateInsightVariable(ENABLE_STATE_LOG,"0", 1, &g_StateLog);
	AsyncSaveData();
	
	/*Fetch Last DB update time stamp*/ 
	FetchDBCntrAndFlags("Insight30MinData", "TIMESTAMP", "TIMESTAMP", (unsigned int*)&s_LastDBUpdatedTS); 
	
	tempchar = GetBelkinParameter(TODAYKWH);
	if((tempchar != NULL) && (strlen(tempchar)!=0))
	{
		g_AccumulatedWattMinute = strtoul(tempchar, NULL, 0);
	}
	tempchar = GetBelkinParameter(TODAYKWHON);
	if((tempchar != NULL) && (strlen(tempchar)!=0))
	{
		g_TodayKWHON = strtoul(tempchar, NULL, 0);
	}
	tempchar = GetBelkinParameter(TODAYKWHSBY);
	if((tempchar != NULL) && (strlen(tempchar)!=0))
	{
		g_TodayKWHSBY = strtoul(tempchar, NULL, 0);
	}
	tempchar = GetBelkinParameter(SETUP_COMPLETE_TS);
	if((tempchar != NULL) && (strlen(tempchar)!=0))
	{
		g_SetUpCompleteTS = strtoul(tempchar, NULL, 0);
	}
	tempchar = GetBelkinParameter(DATA_EXPORT_EMAIL_ADDRESS);
	if((tempchar != NULL) && (strlen(tempchar)!=0))
	{
		memset(g_s8EmailAddress, 0x0, SIZE_256B);
		strncpy(g_s8EmailAddress, tempchar, sizeof(g_s8EmailAddress)-1);
	}
	tempchar = GetBelkinParameter(DATA_EXPORT_TYPE);
	if((tempchar != NULL) && (strlen(tempchar)!=0))
	{
		g_s32DataExportType = (eDATA_EXPORT_TYPE)atoi(tempchar);
	}

	APP_LOG("Insight", LOG_DEBUG, "s_LastDBUpdatedTS: %lu",s_LastDBUpdatedTS);
	APP_LOG("Insight", LOG_DEBUG, "PushToAppTime: %u",g_PushToAppTime);
	APP_LOG("Insight", LOG_DEBUG, "PushToCloudTime: %u",g_PushToCloudTime);
	APP_LOG("Insight", LOG_DEBUG, "TodayONTime: %u",g_TodayONTime);
	APP_LOG("Insight", LOG_DEBUG, "TodayONTimeTS: %u",g_TodayONTimeTS);
	APP_LOG("Insight", LOG_DEBUG, "TodaySBYTime: %u",g_TodaySBYTime);
	APP_LOG("Insight", LOG_DEBUG, "TodaySBYTimeTS: %u",g_TodaySBYTimeTS);
	APP_LOG("Insight", LOG_DEBUG, "TodaysDate: %u",g_TodayDate);
	APP_LOG("Insight", LOG_DEBUG, "WriteToFlashTime: %u",g_WriteToFlashTime);
	APP_LOG("Insight", LOG_DEBUG, "TodayKWH: %u",g_AccumulatedWattMinute);
	APP_LOG("Insight", LOG_DEBUG, "TODAYKWHON: %u",g_TodayKWHON);
	APP_LOG("Insight", LOG_DEBUG, "TODAYKWHSBY: %u",g_TodayKWHSBY);
	APP_LOG("Insight", LOG_DEBUG, "g_SetUpCompleteTS: %lu",g_SetUpCompleteTS);
	APP_LOG("Insight", LOG_DEBUG, "g_s8EmailAddress: %s",g_s8EmailAddress);
	APP_LOG("Insight", LOG_DEBUG, "g_s32DataExportType: %u",g_s32DataExportType);
	APP_LOG("Insight", LOG_DEBUG, "g_EventEnable: %u",g_EventEnable);
	APP_LOG("Insight", LOG_DEBUG, "g_Currency: %u", g_Currency);
	APP_LOG("Insight", LOG_DEBUG, "g_perUnitCost: %u", g_perUnitCost);
	APP_LOG("Insight", LOG_DEBUG, "g_PerUnitCostVer: %u", g_PerUnitCostVer);
	APP_LOG("Insight", LOG_CRIT, "Insight Params:%lu|%lu|%u|%d",s_LastDBUpdatedTS,g_SetUpCompleteTS,g_EventEnable,g_InsightDataRestoreState);
	memset(InsightActive,0,MAX_APNS_SUPPORTED*sizeof(InsightActiveState));
}

void FillGetStructure(InsightVars *InsightValues)
{
    g_TodayONTime = InsightValues->TodayONTime;   
    g_TodayONTimeTS = InsightValues->TodayONTimeTS;   
    g_TodaySBYTime = InsightValues->TodaySBYTime;   
    g_TodaySBYTimeTS = InsightValues->TodaySBYTimeTS;   
    g_AccumulatedWattMinute = InsightValues->AccumulatedWattMinute;   
    g_TodayKWHON = InsightValues->TodayKWHON;   
    g_TodayKWHSBY = InsightValues->TodayKWHSBY;   
    s_TotalKWHSinceIsInsightUP = InsightValues->TotalKWHSinceIsInsightUP;   
    s_Last30MinKWH =InsightValues->Last30MinKWH;   
    g_ONForTS = InsightValues->ONForTS;   
    g_StateChangeTS = InsightValues->StateChangeTS;   
    s_KWHONFor = InsightValues->KWHONFor;   
    s_ONForCalc = InsightValues->ONForCalc;   
    s_LastTimeStamp = InsightValues->LastTimeStamp;   
    s_LastRuntime = InsightValues->LastRuntime;   
    s_LastKWHconsumed = InsightValues->LastKWHconsumed;   
    s_LastDeviceState = InsightValues->LastDeviceState;   
    g_ONFor = InsightValues->ONFor;   
    s_lLastFlashWrite = InsightValues->LastFlashWrite;   
    g_PowerStatus = InsightValues->LastRelayState;   
    
}

void PrintInsightVariable(InsightVars *Cur)
{
    APP_LOG("Insight", LOG_DEBUG,"\n*************  INSIGHT VARIABLES ************\n");
    APP_LOG("Insight", LOG_DEBUG,"		 TodayONTime:	%d\n",Cur->TodayONTime);
    APP_LOG("Insight", LOG_DEBUG,"		 TodayONTimeTS:	%d\n",Cur->TodayONTimeTS);
    APP_LOG("Insight", LOG_DEBUG,"		 TodaySBYTime:	%d\n",Cur->TodaySBYTime);
    APP_LOG("Insight", LOG_DEBUG,"		 TodaySBYTimeTS:	%d\n",Cur->TodaySBYTimeTS);
    APP_LOG("Insight", LOG_DEBUG,"		 AccumulatedWattMinute:	%d\n",Cur->AccumulatedWattMinute);
    APP_LOG("Insight", LOG_DEBUG,"		 TodayKWHON:	%d\n",Cur->TodayKWHON);
    APP_LOG("Insight", LOG_DEBUG,"		 TodayKWHSBY:	%d\n",Cur->TodayKWHSBY);
    APP_LOG("Insight", LOG_DEBUG,"		 TotalKWHSinceIsInsightUP:  %f\n",Cur->TotalKWHSinceIsInsightUP);
    APP_LOG("Insight", LOG_DEBUG,"		 Last30MinKWH:	%d\n",Cur->Last30MinKWH);
    APP_LOG("Insight", LOG_DEBUG,"		 ONForTS:	%d\n",Cur->ONForTS);
    APP_LOG("Insight", LOG_DEBUG,"		 StateChangeTS:	%d\n",Cur->StateChangeTS);
    APP_LOG("Insight", LOG_DEBUG,"		 KWHONFor:	%f\n",Cur->KWHONFor);
    APP_LOG("Insight", LOG_DEBUG,"		 ONForCalc:	%f\n",Cur->ONForCalc);
    APP_LOG("Insight", LOG_DEBUG,"		 LastTimeStamp:	%ul\n",Cur->LastTimeStamp);
    APP_LOG("Insight", LOG_DEBUG,"		 LastRuntime:	%d\n",Cur->LastRuntime);
    APP_LOG("Insight", LOG_DEBUG,"		 LastKWHconsumed:%f\n",Cur->LastKWHconsumed);
    APP_LOG("Insight", LOG_DEBUG,"		 LastDeviceState:%d\n",Cur->LastDeviceState);
    APP_LOG("Insight", LOG_DEBUG,"		 ONFor:%d\n",Cur->ONFor);
    APP_LOG("Insight", LOG_DEBUG,"		 LastFlashWrite: %d\n",Cur->LastFlashWrite);
    APP_LOG("Insight", LOG_DEBUG,"		 LastReleayState: %d\n",Cur->LastRelayState);
    APP_LOG("Insight", LOG_DEBUG,"\n*********************************************\n");
    
}

/*this API is called once during initial device setup as well as on clear usage UPnP call*/
void WaitForInitialSetupAndTimeSync(void)
{
	time_t l_time, curTime;
	struct tm today_time;
	struct timeval tv;
	char CharTime[SIZE_32B] = {0,};
	unsigned short int CurState = POWER_OFF;
	char SetUpCompleteTS[SIZE_32B];

	/*check if setup is completed or not*/
	if(!g_SetUpCompleteTS)
	{
		/*wait for setup to get completed if not done already*/
		while (1)
		{
			/*Variable to monitor this thread health*/
			gInsightHealthPunch++;
			APP_LOG("Insight", LOG_DEBUG, "Waiting For SetUp To Get Complete!");
			/*Varibale is set when closing AP mode, and check if NTP time sync is done*/
			if(g_SetUpCompleteTS && IsNtpUpdate())
			{
				/*sleep for SETUP_COMPLETE_SLEEP_TIME as time sync happens asynchronous and IsNtpUpdate()
				  returns true while time sync still happening.*/
				//sleep(SETUP_COMPLETE_SLEEP_TIME);
				APP_LOG("Insight", LOG_DEBUG, "Time sync completed on initial setup!");
				
				/*check the local time to calculate day*/
				l_time = time(NULL);
				today_time = *localtime(&l_time);
				if(Y2K != (today_time.tm_year + 1900)){
				    APP_LOG("Insight", LOG_DEBUG, "Time sync completed on initial setup! Y2K = %d and Year:%d",Y2K,(today_time.tm_year+1900));

				    g_SetUpCompleteTS = GetUTCTime();
				    sprintf(SetUpCompleteTS, "%lu", g_SetUpCompleteTS);
				    SetBelkinParameter(SETUP_COMPLETE_TS, SetUpCompleteTS);
				    APP_LOG("Insight", LOG_ERR,"updated on setup complete g_SetUpCompleteTS---%lu, SetUpCompleteTS--------%s:", g_SetUpCompleteTS, SetUpCompleteTS);
				    /*set today's date*/
				    g_TodayDate = today_time.tm_mday;
				    memset(CharTime, 0x00, sizeof(CharTime));
				    snprintf(CharTime, sizeof(CharTime), "%u", g_TodayDate);
				    APP_LOG("Insight", LOG_DEBUG, "Setting TODAYSDATE: %s", CharTime);
				    SetBelkinParameter (TODAYSDATE, CharTime);
				    AsyncSaveData();

				    /*start calculating estimated monthly cost, by initializing it to 600 seconds. negative value				       will be send (local and remote notification) for 10 min when device is ON even once*/
				    g_InitialMonthlyEstKWH = -600;

				    /*setup and initialization completed break from loop*/
				    break;
				}
			}
			/*loop sleep for 1 second*/
			sleep(5);
		}
	}
	else
	{
		/*Setup already happened, only wait for NTP to sync time*/
		while (1)
		{
			/*Variable to monitor this thread health*/
			gInsightHealthPunch++;
			/*check if NTP time sync is done*/
			if(IsNtpUpdate())
			{
				/*check the local time to calculate day*/
				l_time = time(NULL);
				today_time = *localtime(&l_time);
				 APP_LOG("Insight", LOG_DEBUG, "***Waiting For Time Sync still Year is %d",  (today_time.tm_year+1900));
				if(Y2K != (today_time.tm_year + 1900)){
				    /*sleep for SETUP_COMPLETE_SLEEP_TIME as time sync happens asynchronous and IsNtpUpdate()
				      returns true while time sync still happening.But do not sleep in case of clearUsageData*/
				    //if(!s_ClearUsageData)
					//sleep(SETUP_COMPLETE_SLEEP_TIME);
				    APP_LOG("Insight", LOG_DEBUG, "Time sync completed!");
				    //Initialization completed break from loop
				    break;
				}
			}
			/*loop sleep for 1 second*/
			sleep(5);
		}

	}

	/*Calculating g_HrsConnected, which is the time since device setup*/
	//if(s_ClearUsageData)
		g_HrsConnected = GetUTCTime() - g_SetUpCompleteTS;
	//else
		//g_HrsConnected = GetUTCTime() - g_SetUpCompleteTS - SETUP_COMPLETE_SLEEP_TIME;

	APP_LOG("Insight", LOG_DEBUG, "Before g_HrsConnected : %d", g_HrsConnected);
	/*check hours connected,it should be less then 14dys * 24hrs * 60min * 60sec = 1209600sec*/
	if (g_HrsConnected > DAYS_SEC_14)
		g_HrsConnected = DAYS_SEC_14;
	APP_LOG("Insight", LOG_DEBUG, "g_HrsConnected : %d", g_HrsConnected);

	/*get current time in seconds*/
	gettimeofday(&tv, NULL); 
	curTime=tv.tv_sec;

	/*initialize notification flag vnd time variables by curTime*/
	s_lLastInsightUpdateTime = curTime;
	s_lLastCloudUpdateTime = curTime;
	s_lLastFlashWrite = curTime;
	s_PrevTime = curTime;

	/*check current device state*/
	CurState = GetCurBinaryState();
	
	/*initialize and start storing device state*/ 
	StateChangeTimeStamp(CurState);
	if(g_InsightDataRestoreState)
	{
	    int Ret=0;
	    InsightVars GetCurValues = {0,0,0,0,0,0,0,0.0,0,0,0,0.0,0.0,0,0,0.0,0,0,0};
	    APP_LOG("Insight", LOG_DEBUG, "\n***************Restoring Data from daemon\n");
	    /*Read Insight Backup Params from daemon*/ 
	    if((Ret = HAL_GetInsightVariables(&GetCurValues)) != 0) {
		APP_LOG("Insight", LOG_DEBUG, "\n.........Get Insight Variables Value Not coming From Daemon\n");
	    }
	    else
	    {
		APP_LOG("Insight", LOG_DEBUG, "\n.Get Insight Variables\n");
		FillGetStructure(&GetCurValues);
		PrintInsightVariable(&GetCurValues);
	    }
	    g_InsightDataRestoreState = 0;
	}

	/*Init data export mutex*/
        osUtilsCreateLock(&s_DataExport_mutex);

	/*Restart data export scheduler if it was scheduled*/
	ReStartDataExportScheduler();
}

/*This API will initialize and start insight task*/
void * StartInsightTask(void * args)
{
	int retVal;
        tu_set_my_thread_name( __FUNCTION__ );

	/*Initialize Insight Database*/
	InitInsightDB();

	/*Initialize Insight global variables*/
	InitInsightVariables();

	/*wait for initial device setup and initializations*/
	WaitForInitialSetupAndTimeSync();

	/*Populate Insight DB Cntr And Flags*/	
	PopulateDBCntrAndFlags();

	/*Populate 14 Days Data*/
	Populate14DaysData();
	APP_LOG("Insight", LOG_CRIT, "Insight DB Params:%u|%u|%u|%u|%u|%u|%d",s_30MinDBEntryCntr,s_PerDayDBEntryCntr,s_StateChangeDBEntryCntr,s_30MinDBEntryRollOverflag,s_PerDayDBEntryRollOverflag,s_StateChangeDBEntryRollOverflag,g_HrsConnected);

	/*Creating Insight GPIO task thread*/	
	retVal = pthread_create(&Insight_thread, NULL, InsightGPIOTask, NULL);
	if((retVal < SUCCESS) || (-1 == Insight_thread))
	{
		APP_LOG("Insight",LOG_ALERT, "Insight GPIO task can not be created, Exit!");
		//system("reboot");
		exit(0);
	}
	else
		pthread_detach(Insight_thread);

	/*Creating Insight send Home Settings thread*/	
	g_InsightHomeSettingsSend = 1;
	g_SendInsightParams = 1;

	/*Creating Insight Event thread*/	
	retVal = pthread_create(&Insight_Eventthread, NULL, InsightEventTask, NULL);
	if((retVal < SUCCESS) || (-1 == Insight_Eventthread))
	{
		APP_LOG("Insight",LOG_ALERT, "Insight Event task can not be created!");
	}
	else
		pthread_detach(Insight_Eventthread);
	
	APP_LOG("Insight",LOG_DEBUG, "Initialization Done! Insight GPIO task can take over!!!");
	return NULL;
}


void FillSetStructure(InsightVars *InsightValues)
{
 InsightValues->TodayONTime   = g_TodayONTime;   
 InsightValues->TodayONTimeTS = g_TodayONTimeTS;   
 InsightValues->TodaySBYTime = g_TodaySBYTime;   
 InsightValues->TodaySBYTimeTS = g_TodaySBYTimeTS;   
 InsightValues->AccumulatedWattMinute = g_AccumulatedWattMinute;   
 InsightValues->TodayKWHON = g_TodayKWHON;   
 InsightValues->TodayKWHSBY = g_TodayKWHSBY;   
 InsightValues->TotalKWHSinceIsInsightUP = s_TotalKWHSinceIsInsightUP;   
 InsightValues->Last30MinKWH = s_Last30MinKWH;   
 InsightValues->ONForTS = g_ONForTS;   
 InsightValues->StateChangeTS = g_StateChangeTS;   
 InsightValues->KWHONFor = s_KWHONFor;   
 InsightValues->ONForCalc = s_ONForCalc;   
 InsightValues->LastTimeStamp = s_LastTimeStamp;   
 InsightValues->LastRuntime = s_LastRuntime ;   
 InsightValues->LastKWHconsumed = s_LastKWHconsumed;   
 InsightValues->LastDeviceState = s_LastDeviceState;   
 InsightValues->ONFor = g_ONFor;   
 InsightValues->LastFlashWrite = s_lLastFlashWrite;   
 InsightValues->LastRelayState   = g_PowerStatus;   
    
}

void Update30MinDataOnFlash()
{
	char CharTime[SIZE_32B];

	/*Insert last 30 minute KWH consumed data*/
	Insert30MinDBData();

	memset(CharTime, 0x00, sizeof(CharTime));
	snprintf(CharTime, sizeof(CharTime), "%u", g_TodayONTime);
	SetBelkinParameter (TODAYONTIME, CharTime);
	APP_LOG("Insight", LOG_DEBUG, "\nToday Total ON Time Logged: %s\n",CharTime);
	
	memset(CharTime, 0x00, sizeof(CharTime));
	snprintf(CharTime, sizeof(CharTime), "%u", g_TodayONTimeTS);
	SetBelkinParameter (TODAYONTIMETS, CharTime);
	APP_LOG("Insight", LOG_DEBUG, "\nToday Total ON TS Time Logged: %s\n",CharTime);
	
	memset(CharTime, 0x00, sizeof(CharTime));
	snprintf(CharTime, sizeof(CharTime), "%u", g_TodaySBYTime);
	SetBelkinParameter (TODAYSBYTIME, CharTime);
	APP_LOG("Insight", LOG_DEBUG, "\nToday Total SBY Time Logged: %s\n",CharTime);
	
	memset(CharTime, 0x00, sizeof(CharTime));
	snprintf(CharTime, sizeof(CharTime), "%u", g_TodaySBYTimeTS);
	SetBelkinParameter (TODAYSBYTIMETS, CharTime);
	APP_LOG("Insight", LOG_DEBUG, "\nToday Total SBY Time TS Logged: %s\n",CharTime);
	
	memset(CharTime, 0x00, sizeof(CharTime));
	snprintf(CharTime, sizeof(CharTime), "%u",g_AccumulatedWattMinute);
	SetBelkinParameter (TODAYKWH, CharTime);
	APP_LOG("Insight", LOG_DEBUG, "\nToday Total KWH Logged: %s\n",CharTime);
	
	memset(CharTime, 0x00, sizeof(CharTime));
	snprintf(CharTime, sizeof(CharTime), "%u",g_TodayKWHON);
	SetBelkinParameter (TODAYKWHON, CharTime);
	APP_LOG("Insight", LOG_DEBUG, "\nToday Total KWH ON Logged: %s\n",CharTime);
	
	memset(CharTime, 0x00, sizeof(CharTime));
	snprintf(CharTime, sizeof(CharTime), "%u",g_TodayKWHSBY);
	SetBelkinParameter (TODAYKWHSBY, CharTime);
	APP_LOG("Insight", LOG_DEBUG, "\nToday Total KWH SBY Logged: %s\n",CharTime);
	AsyncSaveData();
}

/*Inisght main thread handling all calculation and processing*/
void *InsightGPIOTask(void *args)
{   
	time_t curTime=0;
	int Ret = 0;
	//char *ps8PowerThreshold;
	DataValues Values = {0,0,0,0,0};
	InsightVars SetCurValues = {0,0,0,0,0,0,0,0.0,0,0,0,0.0,0.0,0,0,0.0,0,0};
	time_t l_time;
	struct tm today_time;
	struct timeval tv;
	unsigned short int CurState = POWER_OFF;
	unsigned int MinuteCounter = 0;
	unsigned int BackupMinuteCounter = 0;
	unsigned int AccumulatedWatt = 0; 
        tu_set_my_thread_name( __FUNCTION__ );

	APP_LOG("Insight", LOG_DEBUG, "Insight task running");

	/*start insight task*/
	while (1)
	{
		/*Variable to monitor this thread health*/
		gInsightHealthPunch++;

		/*This variable is used clear all the power consumption related data from DB as well as Flash.*/
		if(s_ClearUsageData)
		{
			/*clear usage data before procedding with any further calculations*/
			APP_LOG("Insight", LOG_CRIT, "Clear Usage!!");
			ClearInsightUsageData();
			s_ClearUsageData = 0;
			g_NoNotificationFlag = 0;
			InsightParamUpdate();
			MinuteCounter = 0;
			AccumulatedWatt=0;
		}

		/*get current time in seconds*/
		gettimeofday(&tv, NULL); 
		curTime=tv.tv_sec;
		/*check the local time to calculate day*/
		l_time = time(NULL);
		today_time = *localtime(&l_time);

		/*check current device state*/
		CurState = GetCurBinaryState();
		if(g_isDSTApplied)
		{
		    APP_LOG("Insight", LOG_CRIT, "------- DST Enabled Restructuring DB and variables PrevTime:%d Current Time: %d",s_PrevTime,curTime);
		    Update30MinDataOnFlash();
		    s_lLastFlashWrite = curTime;
		    
		}
		if(s_PrevTime > curTime){
		    s_PrevTime = curTime;
		}
		if(s_lLastInsightUpdateTime > curTime)//handling DST
		    s_lLastInsightUpdateTime = curTime;
		if(s_lLastCloudUpdateTime > curTime)//handling DST
		    s_lLastCloudUpdateTime = curTime;
		

		if(POWER_ON == CurState)
		{
			/*similar to g_TodayONTime but calculated as per time stamp to fix bug of time mismatch on APP*/
			g_TodayONTimeTS = g_TodayONTimeTS + (curTime - s_PrevTime);
			if(g_TodayONTimeTS > HRS_SEC_24)
			    g_TodayONTimeTS = HRS_SEC_24;
			g_TodayONTime++;// Is used in calculations.
			if(g_TodayONTime > HRS_SEC_24)
			    g_TodayONTime = HRS_SEC_24;
			g_TotalONTime14Days = s_TotalONTime13Days + g_TodayONTime;
			if(!g_ONForTS)
			{
				g_ONForTS = curTime;
			}
			s_ONForCalc++;// Is used in calculations
			if(curTime > g_ONForTS)//handling DST
			{
			    g_ONFor = curTime - g_ONForTS;
			    g_RuleONFor = curTime - g_ONForTS;
			}
		}
	  else if(POWER_SBY == CurState)
		{
			/*similar to g_TodayONTime but calculated as per time stamp to fix bug of time mismatch on APP*/
			g_TodaySBYTimeTS = g_TodaySBYTimeTS + (curTime - s_PrevTime);
			if(g_TodaySBYTimeTS > HRS_SEC_24)
			    g_TodaySBYTimeTS = HRS_SEC_24;
			g_TodaySBYTime++;
			if(g_TodaySBYTime > HRS_SEC_24)
			    g_TodaySBYTime = HRS_SEC_24;
		}
		
		/*This if block will handle estimated monthly cost. this is done only once when device is freshly setup and    			device state made ON even once, this will send negative estimated value to APP for next 10 min, till theni		     it will keep on calculating actual estimated*/
		if (g_InitialMonthlyEstKWH && s_InsightONOnce)
		{
			g_InitialMonthlyEstKWH += 1;
		}

		/*check hours connected,it should be less then 14dys * 24hrs * 60min * 60sec = 1209600sec*/   
		if (g_HrsConnected < DAYS_SEC_14){
			g_HrsConnected = g_HrsConnected + (curTime - s_PrevTime);
		}
		else{
			g_HrsConnected = DAYS_SEC_14;
		}

		if((CurState == POWER_ON) || (CurState == POWER_SBY))
		{	    
			/*Read Instantaneous Power from daemon*/ 
			if((Ret = HAL_GetCurrentReadings(&Values)) != 0) {
				APP_LOG("Insight", LOG_EMERG, "\nMetering Daemon Not Responding!!!\n");
			}
			else
			{
				APP_LOG("Insight", LOG_HIDE, "\nPower From Daemon %d:%d\ng_AccumulatedWattMinute:%d,AccumulatedWatt:%d\n",Values.Watts,g_PowerNow,g_AccumulatedWattMinute,AccumulatedWatt);

				/*accumulate power for 60 second and convert into Watt Per Minute*/
				if(MinuteCounter == 60)	
				{
					/*today total KWH accumulated*/ 
					g_AccumulatedWattMinute +=(AccumulatedWatt/MinuteCounter);
					/*total KWH accumulated since insight device is powered on*/ 
					s_TotalKWHSinceIsInsightUP +=(AccumulatedWatt/MinuteCounter);
					if (CurState == POWER_ON)
					{
						/*total KWH when one*/
						s_KWHONFor +=(AccumulatedWatt/MinuteCounter);
						/*Today total KWH when one*/
						g_TodayKWHON +=(AccumulatedWatt/MinuteCounter);
					}
					if (CurState == POWER_SBY)
					{
						/*Today total KWH when SBY*/
						g_TodayKWHSBY +=(AccumulatedWatt/MinuteCounter);
					}

					/*calculate Average power when on*/ 
					calculateAvgOn();

					MinuteCounter = 0;
					AccumulatedWatt = 0;

					/*last 14 days KWH*/
					g_KWH14Days = s_KWH13Days + g_TodayKWHON + g_TodayKWHSBY;
				}
				/*update power variables*/
				g_PowerNow = Values.Watts;
				AccumulatedWatt += Values.Watts;
				MinuteCounter++;
				//ps8PowerThreshold = GetBelkinParameter(POWERTHRESHOLD);
				//g_PowerThreshold = atoi(ps8PowerThreshold);

				/*check and set device sate on threshold change*/
				CheckAndSetDevState(CurState);
			}

			/*Send Notification to APP for Instantaneous Power*/ 
			if (curTime - s_lLastInsightUpdateTime >= g_PushToAppTime)
			{
				s_lLastInsightUpdateTime = curTime;
				InsightParamUpdate();
				g_NoNotificationFlag =0;
				//- Add one more trace here indicating system status
			}  

			/*Send to cloud when push flag is 1*/
			if ((curTime - s_lLastCloudUpdateTime > g_PushToCloudTime)&&(g_SendToCloud == 1))
			{
				s_lLastCloudUpdateTime = curTime;
				APP_LOG("Insight", LOG_HIDE, "Time to notify Cloud %d",s_lLastCloudUpdateTime);
				//- Add one more trace here indicating system status
				APP_LOG("Insight", LOG_HIDE, "InstPower status changed, to notify");
				g_SendInsightParams = 1;
			}
		}//if((CurState == POWER_ON) || (CurState == POWER_SBY))

		/*Reset Today On time and Standby time when there is day change and write day in flash*/
		if(today_time.tm_mday != g_TodayDate)
		{
			APP_LOG("Insight", LOG_CRIT, "Day changed write data in DB:%lu",curTime);
			ResetDayInfo(today_time.tm_mday);
			s_lLastFlashWrite = curTime;
		}

		/*Write Today ON time and Standby Time in Flash, write in every 30 min*/
		if (curTime - s_lLastFlashWrite >= g_WriteToFlashTime)
		{
			APP_LOG("Insight", LOG_CRIT, "s_lLastFlashWrite : %d curTime: %d g_TodayONTime : %d g_TodayONTimeTS: %d g_TodaySBYTime : %d",s_lLastFlashWrite,curTime,g_TodayONTimeTS,g_TodayONTimeTS,g_TodaySBYTimeTS);
			Update30MinDataOnFlash();
			s_lLastFlashWrite = curTime;
		}
		
		if(BackupMinuteCounter == 60)	
		{
		    
		    /*Write Insight backup Params to daemon*/ 
		    FillSetStructure(&SetCurValues);
		    //APP_LOG("Insight", LOG_DEBUG, "\n--Set Insight Variables\n");
		    //PrintInsightVariable(&SetCurValues);
		    HAL_SetInsightVariables(&SetCurValues);
		    BackupMinuteCounter=0;
		}
		BackupMinuteCounter++;
		/*sleep loop by 1 second*/
		s_PrevTime = curTime;		
		sleep(1);
	}
}

/*This API is getting called from LocalBinaryStateNotify to time stamp the ONFor and INSBY from single place.*/
void StateChangeTimeStamp(unsigned short int toState)
{
    time_t curTime,UTCcurTime;
    struct timeval tv;


    char StateChangeTS[SIZE_64B];
    char State[SIZE_64B];
    char RunTime[SIZE_64B];
    char KWHConsumed[SIZE_64B];
    char RowID[SIZE_16B],Condition[SIZE_64B];
    InsightVars SetCurValues = {0,0,0,0,0,0,0,0.0,0,0,0,0.0,0.0,0,0,0.0,0,0};

    g_NoNotificationFlag = 1;
    UTCcurTime = GetUTCTime();
    gettimeofday(&tv, NULL); 
    curTime=tv.tv_sec;
    g_RuleONFor = 0;
    if(POWER_ON == toState)
    {
	/*Flag used in displaying time on App.*/
	g_ONForTS=curTime;
	
	if(g_ONForChangeFlag == 0) //Already done in setbinarystate UPnP call so no need to do it here
	{
	    g_ONFor = 0;
	    s_ONForCalc = 0;
	}
	else
	    g_ONForChangeFlag=0;
	
	s_KWHONFor = 0;
	g_InSBYSince=0;

	/*start calculating monthly estimated cost as devise is on*/	
	if(!s_InsightONOnce)
		s_InsightONOnce = 1;
	
	s_PrevTime = curTime;		
    }
    else if(POWER_SBY == toState)
    {
	g_InSBYSince=curTime;
    }
    else if(POWER_OFF == toState)
    {
	g_PowerNow = 0;// Forcefully doing it as meter driver is giving power value in OFF state for few seconds.
    }

    g_StateChangeTS = UTCcurTime;

    /*keep first time change variables*/ 		
    if (s_FirstTimeStateChange)
    {
	    s_LastTimeStamp = UTCcurTime;
	    s_LastDeviceState = toState;
	    s_LastRuntime = UTCcurTime;
	    s_LastKWHconsumed = s_TotalKWHSinceIsInsightUP;
	    s_FirstTimeStateChange = 0;

	    APP_LOG("StateChangeTimeStamp", LOG_DEBUG, "First time values: s_LastTimeStamp: %lu, s_LastDeviceState:%d, s_LastRuntime:%d, s_LastKWHconsumed:%f", s_LastTimeStamp,s_LastDeviceState,s_LastRuntime,s_LastKWHconsumed);
    }
    else//state changed, insert last state DB entry  
    {
	    memset(StateChangeTS, 0x00, sizeof(StateChangeTS));
	    memset(State, 0x00, sizeof(State));
	    memset(RunTime, 0x00, sizeof(RunTime));
	    memset(KWHConsumed, 0x00, sizeof(KWHConsumed));

	    s_LastRuntime = UTCcurTime - s_LastRuntime;
	    s_LastKWHconsumed = s_TotalKWHSinceIsInsightUP - s_LastKWHconsumed;

	    snprintf(StateChangeTS, sizeof(StateChangeTS), "%lu", s_LastTimeStamp);
	    snprintf(State, sizeof(State), "%u", s_LastDeviceState);
	    snprintf(RunTime, sizeof(RunTime), "%u", s_LastRuntime);
	    snprintf(KWHConsumed, sizeof(KWHConsumed), "%f", s_LastKWHconsumed);
	    
	    //check for database rollover after 500 entries
	    if (MAX_STATE_CHANGE_ENTRY == s_StateChangeDBEntryCntr)
	    {
		APP_LOG("StateChangeTimeStamp", LOG_DEBUG, "Database RollOver");
		s_StateChangeDBEntryRollOverflag = 1;
		s_StateChangeDBEntryCntr = 0;
	    }

	    s_StateChangeDBEntryCntr++;
	    memset(RowID, 0x00, sizeof(RowID));
	    snprintf(RowID, sizeof(RowID), "%u", s_StateChangeDBEntryCntr);

	    if(s_StateChangeDBEntryRollOverflag)
	    {
		    APP_LOG("StateChangeTimeStamp", LOG_DEBUG, "Updating Database on RowId : %d", s_StateChangeDBEntryCntr);
		    memset(Condition, 0x00, sizeof(Condition));
		    snprintf(Condition, sizeof(Condition), "ROWID = %u", s_StateChangeDBEntryCntr);
		    ColDetails InsightStateChangeColInfo[]={{"TIMESTAMP",StateChangeTS},{"STATE",State},{"RUNTIME",RunTime},{"KWHCONSUMED",KWHConsumed}};
		    WeMoDBUpdateTable(&g_InsightDB, "InsightStateChangeInfo", InsightStateChangeColInfo, MAX_STATE_CHANGE_INFO_COLUMN-1, Condition);
	    }
	    else
	    {
		    ColDetails InsightStateChangeColInfo[]={{"ROWID",RowID},{"TIMESTAMP",StateChangeTS},{"STATE",State},{"RUNTIME",RunTime},{"KWHCONSUMED",KWHConsumed}};
		    WeMoDBInsertInTable(&g_InsightDB, "InsightStateChangeInfo", InsightStateChangeColInfo, MAX_STATE_CHANGE_INFO_COLUMN);
	    }
	    
	    s_LastTimeStamp = UTCcurTime;
	    s_LastDeviceState = toState;
	    s_LastRuntime = UTCcurTime;
	    s_LastKWHconsumed = s_TotalKWHSinceIsInsightUP;
	    APP_LOG("StateChangeTimeStamp", LOG_DEBUG, "State Change Values Updated: s_LastTimeStamp: %lu, s_LastDeviceState:%d, s_LastRuntime:%d, s_LastKWHconsumed:%f", s_LastTimeStamp,s_LastDeviceState,s_LastRuntime,s_LastKWHconsumed);
    }
    
    /*polulate avg on state change*/
    PopulateAvgWhenOn();
    /*Write Insight backup Params to daemon*/ 
    FillSetStructure(&SetCurValues);
    APP_LOG("Insight", LOG_DEBUG, "\n--STATE CHANGE Set Insight Variables\n");
    //PrintInsightVariable(&SetCurValues);
    HAL_SetInsightVariables(&SetCurValues);
}

void CheckAndSetDevState(unsigned short int aCurState)
{
	int s32UPnPToggleUpdate = 0;
	if(g_PowerStatus != POWER_OFF)
	{
	    if(g_PowerNow < g_PowerThreshold )
	    {
		if(POWER_SBY != aCurState)
		{
		    //SetCurBinaryState(POWER_SBY);
		    if(MAX_STATE_COUNT == s_StateChangeSBYCntr){
			APP_LOG("Insight",LOG_DEBUG,"CHANGING STATE TO STANDBY");
			s_StateChangeSBYCntr =0;
			LockLED();
			g_APNSLastState = g_PowerStatus;//used to restrict sending APNS in case of OFF->SBY and SBY->OFF
			g_PowerStatus = POWER_SBY;
			UnlockLED();

            /* Stop the Countdown Timer rule thread if its running */
            stopCountdownTimer ();
            APP_LOG ("Insight", LOG_DEBUG, "CountdownTimer stopped");

			aCurState = POWER_SBY;
			s32UPnPToggleUpdate = 1;
		    }
		    else{
			s_StateChangeSBYCntr++;
		    }
		}
		s_StateChangeONCntr =0;
	    }
	    else
	    {
		if(POWER_SBY == aCurState)
		{
		    //SetCurBinaryState(POWER_ON);
		    if(MAX_STATE_COUNT == s_StateChangeONCntr){
			APP_LOG("Insight",LOG_DEBUG,"CHANGING STATE TO ON");
			s_StateChangeONCntr =0;
			LockLED();
			g_APNSLastState = g_PowerStatus;//used to restrict sending APNS in case of OFF->SBY and SBY->OFF
			g_PowerStatus = POWER_ON;
			UnlockLED();
			executeCountdownRule(POWER_ON);
			aCurState = POWER_ON;
			s32UPnPToggleUpdate = 1;
		    }
		    else{
			s_StateChangeONCntr++;
		    }
		}
		s_StateChangeSBYCntr =0;
	    }
	}
	if (s32UPnPToggleUpdate)
	{
		APP_LOG("CheckAndSetDevState", LOG_DEBUG, "Start UPnP toggle and Remote update-------");
		UPnPInternalToggleUpdate(aCurState);
		if (g_SendToCloud == 1)
		{
		    /*This flag is used to keep common API for sending all
		      the messages to cloud*/
		      g_SendInsightParams =1;
		}
	}
}

void calculateAvgOn(void)
{
	double convertToKWH = 0;
	double convertToHours = 0;

	/*convert Watt minute into KWH value is multiplied by 1000 to take care of decimal calculation*/
	convertToKWH = ((s_LastKWHWhenOn + s_KWHONFor)*1000)/60;
	APP_LOG("InsightGPIOTask", LOG_HIDE, "s_LastKWHWhenOn---%f, s_KWHONFor----%f, convertToKWH---%f \n", s_LastKWHWhenOn, s_KWHONFor, convertToKWH);
	/*convert second into hour value is multiplied by 1000 to take care of decimal calculation*/
	convertToHours = ((s_RunTimeWhenOn + s_ONForCalc)*10*1000)/36;
	APP_LOG("InsightGPIOTask", LOG_HIDE, "s_RunTimeWhenOn----%f, s_ONForCalc----%f, convertToHours---%f \n", s_RunTimeWhenOn, s_ONForCalc,  convertToHours);
	if (convertToHours)
	{
		/*calculate average power on*/
		g_AvgPowerON = ((convertToKWH / convertToHours) + 0.5);
		
	}
	APP_LOG("InsightGPIOTask", LOG_HIDE, "g_AvgPowerON---%d \n", g_AvgPowerON);
}

/*initiate clear usage on next iteration of main insight loop*/
void ClearUsageData(void)
{
	APP_LOG("ClearUsageData:", LOG_DEBUG, "Start Clearing Usage Data...");
	s_ClearUsageData = 1;
	g_NoNotificationFlag = 1;
}

//reset insight variables
void ClearInsightUsageData(void)
{
	struct stat FileInfo;
        char InsightDBPath[SIZE_256B];

        /*Checking for the existence of the Insight DB file.*/
        if(stat(DBURL, &FileInfo) != -1 )
        {
                memset(InsightDBPath, 0x0, sizeof(InsightDBPath));
                snprintf(InsightDBPath, sizeof(InsightDBPath), "rm -rf %s",DBURL);
                system(InsightDBPath);
                APP_LOG("ClearUsageData:", LOG_DEBUG, "INSIGHT DB deleted...");
                InitInsightDB();
        }
        APP_LOG("ClearUsageData:", LOG_DEBUG, "INSIGHT variable reset...");
        ReInitInsightVariables();
	WaitForInitialSetupAndTimeSync();
}

void ReInitInsightVariables(void)
{
	APP_LOG("ReInitInsightVariables", LOG_DEBUG, "Init Insight Variables");
        char StringValue[SIZE_32B];
	//reset global variable
	g_ONFor=0,g_ONForTS=0;
	g_RuleONFor = 0;
	g_InSBYSince=0;
	g_TodayONTime=0;
	g_TodayONTimeTS=0;
	g_TodaySBYTime=0;
	g_TodaySBYTimeTS=0;
	g_TotalONTime14Days=0;
	g_HrsConnected=0;
	g_AvgPowerON=0;
	g_KWH14Days=0;
	g_TodayKWHON=0;
	g_TodayKWHSBY=0;
	g_AccumulatedWattMinute=0;
	g_PowerNow=0;
	g_StateChangeTS = 0;

	//reset static variable
	s_RunTimeWhenOn = 0;
	s_LastKWHWhenOn = 0;
	s_KWHONFor = 0;
	s_TotalKWHSinceIsInsightUP = 0;
	s_FirstTimeStateChange = 1;
	s_PrevTime = 0;
	s_Last30MinKWH = 0;
	s_30MinDBEntryCntr = 0;
	s_PerDayDBEntryCntr = 0;
	s_StateChangeDBEntryCntr = 0;
	s_30MinDBEntryRollOverflag = 0;
	s_PerDayDBEntryRollOverflag = 0;
	s_StateChangeDBEntryRollOverflag = 0;
	s_LastDBUpdatedTS = 0;
	s_lLastFlashWrite = 0;
	s_lLastCloudUpdateTime = 0;
	s_InsightONOnce = 0;
	s_TotalONTime13Days=0;
	s_KWH13Days=0;
	s_ONForCalc=0;
	g_YesterdayTS = time(NULL) - 86400;// Today time in seconds - 24 hr seconds

        SetBelkinParameter(TODAYONTIME, "");
        SetBelkinParameter(TODAYONTIMETS, "");
        SetBelkinParameter(TODAYSBYTIME, "");
        SetBelkinParameter(TODAYKWH, "");
        SetBelkinParameter(TODAYKWHON, "");
        SetBelkinParameter(TODAYKWHSBY, "");

        memset(StringValue, 0, sizeof(StringValue));
        g_SetUpCompleteTS = GetUTCTime();
        snprintf(StringValue, sizeof(StringValue), "%lu", g_SetUpCompleteTS);
        SetBelkinParameter(SETUP_COMPLETE_TS, StringValue);
	AsyncSaveData();
        g_InitialMonthlyEstKWH = -600;

	APP_LOG("InitInsightVariables:", LOG_DEBUG,"g_SetUpCompleteTS---%lu, SetUpCompleteTS--------%s:", g_SetUpCompleteTS, StringValue);
//#ifdef BOARD_EVT
#if (defined(BOARD_EVT) || defined(BOARD_PVT))
	APP_LOG("InitInsightVariables:", LOG_DEBUG,"##############NOT RESTARTING DEAMON AS BOARD TYPE IS EVT");
#else
	system("killall -9 insightd");
	System("insightd");
	APP_LOG("InitInsightVariables:", LOG_DEBUG,"############## RESTARTING DEAMON AS BOARD TYPE IS NOT EVT");
#endif
}

/*this function returns no of days left for sunday*/
int GetDaysToSunday(char * TodayDay)
{
        if(!strncmp(TodayDay, "Mon", strlen("Mon")))return 6;
        else if(!strncmp(TodayDay, "Tue", strlen("Tue")))return 5;
        else if(!strncmp(TodayDay, "Wed", strlen("Wed")))return 4;
        else if(!strncmp(TodayDay, "Thu", strlen("Thu")))return 3;
        else if(!strncmp(TodayDay, "Fri", strlen("Fri")))return 2;
        else if(!strncmp(TodayDay, "Sat", strlen("Sat")))return 1;
        else if(!strncmp(TodayDay, "Sun", strlen("Sun")))return 0;
        else
                return 0;
}

/*this function will return time left in second to reach 12 mid night*/
unsigned long int GetDailyTS(time_t timeNow)
{
        struct tm ts;
        char buf[SIZE_100B];
        int sec = 0;

        ts = *localtime(&timeNow);
        sec = (24 - ts.tm_hour - 1)*60*60 + (60 - ts.tm_min - 1)*60 + (60 - ts.tm_sec);
        timeNow += sec;
	/*Delaying data export by 5 sec so that DB is updated for daily consumption*/
        timeNow += DAILY_DB_SYNC_TIME;
        ts = *localtime(&timeNow);
        memset(buf, 0x0, sizeof(buf));
        strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
	APP_LOG("GetDailyTS:", LOG_DEBUG,"GetDailyTS: %s",buf);
        return timeNow;
}

/*this function will return time left in second to reach 12 mid night of coming sunday*/
unsigned long int GetWeekSundayTS(time_t timeNow)
{
        struct tm ts;
        char buf[SIZE_100B];
        int noOfDays;
        int sec = 0;

        ts = *localtime(&timeNow);
        memset(buf, 0x0, sizeof(buf));
        strftime(buf, sizeof(buf), "%a", &ts);
        noOfDays = GetDaysToSunday(buf);
        sec = noOfDays*24*60*60 + (24 - ts.tm_hour - 1)*60*60 + (60 - ts.tm_min - 1)*60 + (60 - ts.tm_sec);
	timeNow += sec;
	/*Delaying data export by 5 sec so that DB is updated for daily consumption*/  
	timeNow += DAILY_DB_SYNC_TIME;
        ts = *localtime(&timeNow);
        memset(buf, 0x0, sizeof(buf));
        strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
	APP_LOG("GetWeekSundayTS:", LOG_DEBUG,"GetWeekSundayTS: %s",buf);
        return timeNow;
}

/*this function will return time left in second to reach 12 mid night of next month's first sunday*/
unsigned long int GetMonthSundayTS(void)
{
	time_t now;
	struct tm ts;
	time_t t_of_day;

	time(&now);
	ts = *localtime(&now);
	if(11 == ts.tm_mon)
	{
		ts.tm_mon = 1;
		ts.tm_year += 1;
	}
	else
	{
		ts.tm_mon += 1;
	}
	ts.tm_mday = 1;

	t_of_day = mktime(&ts);
	return GetWeekSundayTS(t_of_day);
}

void StartDataExportSendThread(void *pMessage)
{
	int retVal = -1;
	char *arg = NULL;
        tu_set_my_thread_name( __FUNCTION__ );

	arg = (char*)ZALLOC(SIZE_256B+SIZE_8B);
	if(!arg)
	{
		APP_LOG("StartDataExportSendThread", LOG_CRIT, "Out Of Memory!!!");
		return;
	}
	memcpy(arg, pMessage, SIZE_256B+SIZE_8B);

	APP_LOG("StartDataExportSendThread", LOG_DEBUG, "start new thread arg:%s", arg);
	/*Creating Compose And Send CSV File Thread*/
	retVal = pthread_create(&DataExport_Send_thread, NULL, ComposeAndSendCSVFileThread, (void*)arg);
	if((retVal < SUCCESS) || (-1 == DataExport_Send_thread))
	{
		APP_LOG("Insight",LOG_CRIT, "Failed to create Compose And Send CSV File Thread, Exit!");
		exit(0);
	}
	else
		pthread_detach(DataExport_Send_thread);
}

/*creates CSV file format device data*/
void *ComposeAndSendCSVFileThread(void *arg)
{
	/*compose CSV file and send data*/
	osUtilsGetLock(&s_DataExport_mutex);
	APP_LOG("ComposeCSVFile:", LOG_DEBUG, "Got Data Export Lock!");
	FILE *FileCSV = NULL;
	int RetVal = 1;	
	char TmpBuff[50];

	int DataExportscheduleType = E_TURN_OFF_SCHEDULER;
	char DataExportscheduleTypeStr[SIZE_8B] = {0};
	char *valuestr[1] = {DataExportscheduleTypeStr};
        tu_set_my_thread_name( __FUNCTION__ );

	if((0x00 != (char*)arg) && (0x00 != strlen((char*)arg)))
	{
		TokenParser(valuestr,(char*)arg, 1);
	}
	else
	{
		APP_LOG("ComposeAndSendCSVFileThread", LOG_CRIT,"Invalid Arguments.Exit Thread");
		return NULL;
	}

	DataExportscheduleType = atoi(DataExportscheduleTypeStr);

	do
	{
		APP_LOG("ComposeCSVFile:", LOG_DEBUG, "Open CSV file for data Export...");
		FileCSV = fopen(INSIGHT_DATA_FILE_URL, "w");
		if(FileCSV)
		{
			/*write Device info in CSV file*/
			if((RetVal = writeDeviceInfoInCVS(&FileCSV)))
				break;
			/*write daily insight info in CSV file*/
			if((RetVal = writeDailyUsageDataInCVS(&FileCSV, DataExportscheduleType)))
				break;
			/*write energy data info in CSV file*/
			if((RetVal = writeEnergyDataInCVS(&FileCSV, DataExportscheduleType)))
				break;
		}
		else
		{
			APP_LOG("ComposeCSVFile:", LOG_CRIT, "Insight Export Data file Open failed...");
			break;
		}

	}while(0);

	if(FileCSV)fclose(FileCSV);FileCSV = NULL;
	if(!RetVal)
		UploadInsightDataFile(arg);
	else
		APP_LOG("ComposeCSVFile:", LOG_CRIT, "Compose CSV File failed...");
	snprintf(TmpBuff,sizeof(TmpBuff),"rm -f %s",INSIGHT_DATA_FILE_URL);
	APP_LOG("ComposeCSVFile:", LOG_DEBUG, "calling %s",TmpBuff);
	system(TmpBuff);
	osUtilsReleaseLock(&s_DataExport_mutex);
	APP_LOG("ComposeCSVFile:", LOG_DEBUG, "Released Data Export Lock!");
	return NULL;
}

void SendDataExportMsg(int aDataExportType, char *pEmailAddress)
{
	char msgbuf[SIZE_256B+SIZE_8B] = {0}; 
        pMessage msg = 0x00;
	snprintf(msgbuf, sizeof(msgbuf), "%d|%s", aDataExportType, pEmailAddress);
	APP_LOG("SendDataExportMsg:", LOG_DEBUG, "Compose CSV File msgbuf:%s", msgbuf);
        msg = createMessage(UPNP_MESSAGE_DATA_EXPORT, (void*)msgbuf,  sizeof(msgbuf));
        SendMessage2App(msg);
}

/*this function is callled to restart scheduler if reboot occurs or time sync happens*/
void ReStartDataExportScheduler(void)
{
	if((E_SEND_DATA_NOW < g_s32DataExportType) && (E_TURN_OFF_SCHEDULER > g_s32DataExportType) && strlen(g_s8EmailAddress))
		StartDataExportScheduler();
}

/*create data export thread*/
void StartDataExportScheduler(void)
{
	int retVal;

	if(-1 != Insight_ExportDatathread)
	{
		retVal = pthread_cancel(Insight_ExportDatathread);
		if (retVal != 0)
		{
			APP_LOG("ScheduleDataExport", LOG_DEBUG, "Data export Error on pthread_cancel");
			//system("reboot");
			Insight_ExportDatathread = -1;
		}
		else
		{
			APP_LOG("ScheduleDataExport:", LOG_DEBUG, "thread canceled");
			Insight_ExportDatathread = -1;
		}
	}

	/*Check if UPnP requested for scheduler stop*/
	if(E_TURN_OFF_SCHEDULER == g_s32DataExportType)
	{
		APP_LOG("CreateInsightExportData:",LOG_DEBUG, "Data Export Scheduler Turned OFF");
		return;
	}

	APP_LOG("CreateInsightExportData:",LOG_DEBUG, "Creating Insight Export Data Thread");
	retVal = pthread_create(&Insight_ExportDatathread, NULL, CreateInsightExportData, NULL);
	if ((retVal < SUCCESS) || (-1 == Insight_ExportDatathread))
	{
		APP_LOG("CreateInsightExportData",LOG_CRIT, "Failed to Create Insight Export Data thread, Exit!");
		//system("reboot");
		exit(0);
	}
	else
		pthread_detach(Insight_ExportDatathread);
}

/*data export thread*/
void * CreateInsightExportData(void *arg)
{
	time_t now;
	int ScheduleAgain = 1;
	unsigned long int ScheduledTS = 0; 
	int ThreadSleepFor = 0;
	char * pEmailAddress = NULL;
	int DataExportType;
	int rndSleep = 0;
	PDATA_EXPORT_INFO pDataExportInfo = (PDATA_EXPORT_INFO)arg;
        tu_set_my_thread_name( __FUNCTION__ );

	/*check data export now is called with different email address*/
	if(pDataExportInfo)
	{
		DataExportType = pDataExportInfo->DataExportType;
		pEmailAddress = pDataExportInfo->EmailAddress;
	}
	/*data export is scheduled*/
	else
	{
		DataExportType = g_s32DataExportType;

		/*This flag was set when there is any time sync*/
		if(g_ReStartDataExportScheduler)
		{
			g_ReStartDataExportScheduler = 0;
			/*sleep for SETUP_COMPLETE_SLEEP_TIME sec to let time sync gets completed, as isNTPUpdate return true while                    time sync still happening*/
			sleep(SETUP_COMPLETE_SLEEP_TIME);
		}
	}

	while(ScheduleAgain)
	{
		time(&now);
		rndSleep = rand() % 3600;
		/*schedule data export with its appropriate scheduler type*/
		APP_LOG("ScheduleDataExport:", LOG_CRIT, "Insight Export Data Type: %d",DataExportType);
		switch(DataExportType)
		{
			case E_SEND_DATA_NOW:
				APP_LOG("ScheduleDataExport:", LOG_DEBUG, "Insight Export Data Type: %d",E_SEND_DATA_NOW);
				ScheduleAgain = 0;
				break;
			case E_SEND_DATA_DAILY:
				APP_LOG("ScheduleDataExport:", LOG_DEBUG, "Insight Export Data Type: %d",E_SEND_DATA_DAILY);
				ScheduledTS = GetDailyTS(now);
				ThreadSleepFor = ScheduledTS - now;
				APP_LOG("ScheduleDataExport:", LOG_DEBUG, "now: %d,ScheduledTS: %d,ThreadSleepFor: %d randomsleep: %d",now,ScheduledTS,ThreadSleepFor,rndSleep);
				sleep(ThreadSleepFor+rndSleep);
				break;
			case E_SEND_DATA_WEEKLY:
				APP_LOG("ScheduleDataExport:", LOG_DEBUG, "Insight Export Data Type: %d",E_SEND_DATA_WEEKLY);
				ScheduledTS = GetWeekSundayTS(now);
				ThreadSleepFor = ScheduledTS - now;
				APP_LOG("ScheduleDataExport:", LOG_DEBUG, "now: %d,ScheduledTS: %d,ThreadSleepFor: %d",now,ScheduledTS,ThreadSleepFor);
				sleep(ThreadSleepFor+rndSleep);
				break;
			case E_SEND_DATA_MONTHLY:
				APP_LOG("ScheduleDataExport:", LOG_DEBUG, "Insight Export Data Type: %d",E_SEND_DATA_MONTHLY);
				ScheduledTS = GetMonthSundayTS();
				ThreadSleepFor = ScheduledTS - now;
				APP_LOG("ScheduleDataExport:", LOG_DEBUG, "now: %d,ScheduledTS: %d,ThreadSleepFor: %d",now,ScheduledTS,ThreadSleepFor);
				sleep(ThreadSleepFor+rndSleep);
				break;
			default:
				APP_LOG("ScheduleDataExport:", LOG_DEBUG, "Insight Export Data Type incorrect...");
				ScheduleAgain = 0;
				break;
		}
		/*Data export now with new email address*/ 
		if(pDataExportInfo)
			SendDataExportMsg(DataExportType, pEmailAddress);
		/*Data export now with scheduled export type*/ 
		else
			SendDataExportMsg(g_s32DataExportType, g_s8EmailAddress);
	}

	return NULL;
}

void ExecuteInsightDataExport(char *szEmailAddress, char* szDataExportType)
{	
	eDATA_EXPORT_TYPE DataExportType = E_TURN_OFF_SCHEDULER;
        DATA_EXPORT_INFO dataExportInfo;

	if(NULL == szEmailAddress || NULL == szDataExportType)
	{
		APP_LOG("ScheduleDataExport", LOG_DEBUG,"ScheduleDataExport: Invalid export data!");
		return;
	}

	DataExportType = (eDATA_EXPORT_TYPE)atoi(szDataExportType);

        /*data export now*/
        if (DataExportType ==  E_SEND_DATA_NOW)
        {
                memset(&dataExportInfo, 0x0, sizeof(DATA_EXPORT_INFO));
                dataExportInfo.DataExportType = E_SEND_DATA_NOW;
                /*check if Email Address received is NULL string. In that case, just send export data to cloud with empty email address.*/
                if(!strlen(szEmailAddress))
                {
                        APP_LOG("ScheduleDataExport", LOG_DEBUG,"ScheduleDataExport: EmailAddress not present");
                        /*send email address as 'null', to avoid exception at cloud side*/
                        strncpy(dataExportInfo.EmailAddress, "null", sizeof(dataExportInfo.EmailAddress) - 1);
                }
                /*check if need to save email address in case data export is not scheduled*/
                else if((g_s32DataExportType == E_TURN_OFF_SCHEDULER) || (!(g_s32DataExportType > E_SEND_DATA_NOW) && (!(g_s32DataExportType < E_TURN_OFF_SCHEDULER))))
                {
                        memset(g_s8EmailAddress, 0x0, sizeof(g_s8EmailAddress));
                        strncpy(g_s8EmailAddress, szEmailAddress, sizeof(g_s8EmailAddress) - 1);
                        strncpy(dataExportInfo.EmailAddress, szEmailAddress, sizeof(dataExportInfo.EmailAddress) - 1);
                        SetBelkinParameter(DATA_EXPORT_EMAIL_ADDRESS, szEmailAddress);
                        APP_LOG("ScheduleDataExport", LOG_DEBUG,"ScheduleDataExport: Update EmailAddress!");
                }
                /*data Export now, while data export is scheduled already*/
                else
                        strncpy(dataExportInfo.EmailAddress, szEmailAddress, sizeof(dataExportInfo.EmailAddress) - 1);

                APP_LOG("ScheduleDataExport", LOG_DEBUG,"ScheduleDataExport: Export Now!");
                CreateInsightExportData((void*)&dataExportInfo);
        }
        /*schedule data export*/
        else if (strlen(szEmailAddress) && ((DataExportType > E_SEND_DATA_NOW) && (DataExportType <= E_TURN_OFF_SCHEDULER)))
        {
                memset(g_s8EmailAddress, 0x0, sizeof(g_s8EmailAddress));
                strncpy(g_s8EmailAddress, szEmailAddress, sizeof(g_s8EmailAddress) - 1);
                g_s32DataExportType = (eDATA_EXPORT_TYPE)atoi(szDataExportType);

                SetBelkinParameter(DATA_EXPORT_EMAIL_ADDRESS, szEmailAddress);
                SetBelkinParameter(DATA_EXPORT_TYPE, szDataExportType);
                AsyncSaveData();
                APP_LOG("ScheduleDataExport", LOG_DEBUG,"ScheduleDataExport: g_s8EmailAddress: %s, g_s32DataExportType: %d", g_s8EmailAddress, g_s32DataExportType);
                StartDataExportScheduler();
        }
        else
        {
                APP_LOG("ScheduleDataExport", LOG_DEBUG,"ScheduleDataExport: Incorrect parameter, No data export!");
	}
	return;
}


void EventSleep(int EventDuration)
{
    int WaitLoop=(EventDuration/60); 
    while(WaitLoop)
    {
	WaitLoop--;
	sleep(60);
	if(g_SendEvent)
	{
	    APP_LOG("EventSleep", LOG_DEBUG, "Periodic Event Enabled Again Sending Notification");
	    sendRemoteInsightEventParams();
	    g_SendEvent = 0;
	    
	}

    }
}
/*insight Event thread*/ 
void *InsightEventTask(void *args)
{
    int RetryCount=0;
    int EventrndSleep=0;
        tu_set_my_thread_name( __FUNCTION__ );

    APP_LOG("REMOTEACCESS", LOG_DEBUG, "********** Insight Periodic Event Notification Thread Started ***********");
    while(1)
    {
	if ((0x00 == strlen(g_szHomeId) ) || (0x00 == strlen(g_szPluginPrivatekey)) || (atoi(g_szRestoreState) == 0x1)) {
	    //APP_LOG("REMOTEACCESS", LOG_DEBUG, "Remote Access is not allowed ");
	    pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);
	    continue;
	}

	if(!isNotificationEnabled ()) {
	    APP_LOG("REMOTEACCESS", LOG_DEBUG, "Remote Notification Sending is disabled");
	    pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);
	    continue;
	}
	if(1 == g_EventEnable)
	{
	    EventrndSleep = rand() % 300;
	    if ((0x00 != strlen(g_szHomeId) ) && (0x00 != strlen(g_szPluginPrivatekey)) && (atoi(g_szRestoreState) == 0x0) && \
		    (gpluginRemAccessEnable == 1)) {
		    APP_LOG("REMOTEACCESS", LOG_DEBUG, "Periodic Event Sending Notification");
		if(sendRemoteInsightEventParams() == PLUGIN_SUCCESS){
		    APP_LOG("REMOTEACCESS", LOG_CRIT, "Periodic Event Sending Success sleeping for %d  minutes",(g_EventDuration+EventrndSleep));
		    RetryCount=0;
		    EventSleep(g_EventDuration+EventrndSleep);
		    APP_LOG("REMOTEACCESS", LOG_DEBUG, "Periodic Event Notification Time %d minutes sleep over",(g_EventDuration+EventrndSleep));
		}
		else{
		    if(RetryCount == 3)
		    {
			RetryCount=0;
			APP_LOG("REMOTEACCESS", LOG_DEBUG, "Periodic Event Failed and 3 Retry Done sleeping for 30  minutes");
			EventSleep(g_EventDuration+EventrndSleep);
		    }
		    RetryCount++;
		    APP_LOG("REMOTEACCESS", LOG_DEBUG, "Periodic Event Sending Failed");
		    
		}
	    }
	}
	else{
	    pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);
	    continue;
	}
	sleep(1);
    }

}

/*insert 30 min data in DB*/
void Insert30MinDBData(void)
{
		char RowID[SIZE_16B],TimeStamp[SIZE_32B],last30minKWH[SIZE_16B],Condition[SIZE_64B];
		unsigned int TempLast30MinKWH = 0;
		int s32NoOfRows=0,s32NoOfCols=0;
		int s32RowCntr=0,s32ArraySize=0;
		char **s8ResultArray=NULL;
		char query[SIZE_256B];
		struct timeval tv;
		time_t TempTS,CurTime;
		/*get current time in seconds*/
		gettimeofday(&tv, NULL); 
		if(g_isDSTApplied)
		{
		    CurTime=tv.tv_sec;
		    g_isDSTApplied=0;
		    memset(query, 0, sizeof(query));
		    snprintf(query, sizeof(query), "SELECT TIMESTAMP FROM Insight30MinData order by TIMESTAMP desc limit 2;");
		    if(!WeMoDBGetTableData(&g_InsightDB,query,&s8ResultArray,&s32NoOfRows,&s32NoOfCols))
		    {
			s32ArraySize = (s32NoOfRows + 1)*s32NoOfCols;
			if(s32ArraySize)
			{
			    for(s32RowCntr = 1; s32RowCntr < s32ArraySize; s32RowCntr++)
			    {
				TempTS = atol(s8ResultArray[s32RowCntr]);
				APP_LOG("Insert30MinDBData", LOG_HIDE, "DST DELETE FETCHED TimeStamp : %lu",TempTS );
				if(TempTS > CurTime)
				{
				    //Delete Previous entries.
				    APP_LOG("Insert30MinDBData", LOG_HIDE, "Deleting Database on TimeStamp : %lu",TempTS );
				    memset(Condition, 0x00, sizeof(Condition));
				    snprintf(Condition, sizeof(Condition), "TIMESTAMP = %lu", TempTS);
				    WeMoDBDeleteEntry(&g_InsightDB, "Insight30MinData",Condition);
				}
			    }
			}
		    }
		    

		}
		// check for database rollover for 45 days
		if (MAX_30MIN_ENTRY == s_30MinDBEntryCntr)
		{
		    APP_LOG("Insert30MinDBData", LOG_DEBUG, "Database RollOver");
		    s_30MinDBEntryRollOverflag = 1;
		    s_30MinDBEntryCntr = 0;
		}
		s_30MinDBEntryCntr++;
		memset(RowID, 0x00, sizeof(RowID));
		snprintf(RowID, sizeof(RowID), "%u", s_30MinDBEntryCntr);

		s_LastDBUpdatedTS=tv.tv_sec;
		
		if(s_AdjustData)
		{
			s_AdjustData = 0;
			APP_LOG("Insert30MinDBData", LOG_HIDE, "Adjusting time s_LastDBUpdatedTS: %d, s_LastDBUpdatedTS_Adjusted: %d",s_LastDBUpdatedTS,(s_LastDBUpdatedTS - 60));
			s_LastDBUpdatedTS = s_LastDBUpdatedTS - 60; // Changing timestamp to a backdate
		}
		memset(TimeStamp, 0x00, sizeof(TimeStamp));
		snprintf(TimeStamp, sizeof(TimeStamp), "%lu", s_LastDBUpdatedTS);

		TempLast30MinKWH = s_TotalKWHSinceIsInsightUP - s_Last30MinKWH;
		APP_LOG("Insert30MinDBData", LOG_HIDE, "TempLast30MinKWH: %d, s_TotalKWHSinceIsInsightUP: %f, s_Last30MinKWH: %d", TempLast30MinKWH,s_TotalKWHSinceIsInsightUP,s_Last30MinKWH);
		memset(last30minKWH, 0x00, sizeof(last30minKWH));
		snprintf(last30minKWH, sizeof(last30minKWH), "%u", TempLast30MinKWH);
		s_Last30MinKWH = s_TotalKWHSinceIsInsightUP;

		if (s_30MinDBEntryRollOverflag)
		{
				APP_LOG("Insert30MinDBData", LOG_HIDE, "Updating Database on RowId : %d", s_30MinDBEntryCntr);
				memset(Condition, 0x00, sizeof(Condition));
				snprintf(Condition, sizeof(Condition), "ROWID = %u", s_30MinDBEntryCntr);
				ColDetails Insight30MinData[]={{"TIMESTAMP",TimeStamp},{"KWHCONSUMED",last30minKWH}};
				WeMoDBUpdateTable(&g_InsightDB, "Insight30MinData", Insight30MinData, MAX_30_MIN_INFO_COLUMN-1, Condition);
		}
		else
		{
				ColDetails Insight30MinData[]={{"ROWID",RowID},{"TIMESTAMP",TimeStamp},{"KWHCONSUMED",last30minKWH}};
				WeMoDBInsertInTable(&g_InsightDB, "Insight30MinData", Insight30MinData, MAX_30_MIN_INFO_COLUMN);

		}
}   

/*insert per day data in DB*/
void InsertPerDayDBData(void)
{
        char RowID[SIZE_16B],TimeStamp[SIZE_32B],TodayOnTime[SIZE_16B],TodayKWH[SIZE_16B],TodaySBYTime[SIZE_16B];
	char TodaySBYKWH[SIZE_16B],Last14DaysOnTime[SIZE_16B],AvgPowerOn[SIZE_16B],Last14DaysKWH[SIZE_64B];
	char HoursConnected[SIZE_16B],Condition[SIZE_64B];
	char *PerUnitcostStr = NULL;
        unsigned int Last30MinEntryTS = 0;
	struct timeval tv;

        //UTCcurTime = GetUTCTime();
	gettimeofday(&tv, NULL);
        memset(RowID, 0x00, sizeof(RowID));
        memset(TimeStamp, 0x00, sizeof(TimeStamp));
        memset(TodayOnTime, 0x00, sizeof(TodayOnTime));
        memset(TodayKWH, 0x00, sizeof(TodayKWH));
        memset(TodaySBYTime, 0x00, sizeof(TodaySBYTime));
        memset(TodaySBYKWH, 0x00, sizeof(TodaySBYKWH));
        memset(AvgPowerOn, 0x00, sizeof(AvgPowerOn));
        memset(Last14DaysOnTime, 0x00, sizeof(Last14DaysOnTime));
        memset(Last14DaysKWH, 0x00, sizeof(Last14DaysKWH));
        memset(HoursConnected, 0x00, sizeof(HoursConnected));

	//check for database rollover
	APP_LOG("InsertPerDayDBData", LOG_DEBUG, "s_PerDayDBEntryCntr: %d and MAX_PERDAY_ENTRY: %d",s_PerDayDBEntryCntr,MAX_PERDAY_ENTRY);
	if (MAX_PERDAY_ENTRY == s_PerDayDBEntryCntr)
	{
	    APP_LOG("InsertPerDayDBData", LOG_CRIT, "Per Day Database RollOver");
	    s_PerDayDBEntryRollOverflag = 1;
	    s_PerDayDBEntryCntr = 0;
	}

	s_PerDayDBEntryCntr++;
	snprintf(RowID, sizeof(RowID), "%u", s_PerDayDBEntryCntr);

	/*fetch last 30 min DB entry for date*/
	FetchDBCntrAndFlags("Insight30MinData", "TIMESTAMP", "TIMESTAMP", &Last30MinEntryTS);
	if(Last30MinEntryTS)
	{
		snprintf(TimeStamp, sizeof(TimeStamp), "%lu", Last30MinEntryTS);
		g_YesterdayTS = Last30MinEntryTS;
	}
	else
	{
		/*No last 30 min DB entry, adjust current time by 60 seconde*/
		snprintf(TimeStamp, sizeof(TimeStamp), "%lu", tv.tv_sec-60);
		g_YesterdayTS =  tv.tv_sec-60;
	}

	snprintf(TodayOnTime, sizeof(TodayOnTime), "%u", g_TodayONTimeTS);
        snprintf(TodayKWH, sizeof(TodayKWH), "%u", g_TodayKWHON);
        snprintf(TodaySBYTime, sizeof(TodaySBYTime), "%u", g_TodaySBYTimeTS);
        snprintf(TodaySBYKWH, sizeof(TodaySBYKWH), "%u", g_TodayKWHSBY);
        snprintf(AvgPowerOn, sizeof(AvgPowerOn), "%u", g_AvgPowerON);
        snprintf(Last14DaysOnTime, sizeof(Last14DaysOnTime), "%u", g_TotalONTime14Days);
        snprintf(Last14DaysKWH, sizeof(Last14DaysKWH), "%f", g_KWH14Days);
        snprintf(HoursConnected, sizeof(HoursConnected), "%u", g_HrsConnected);

	PerUnitcostStr = GetBelkinParameter(ENERGYPERUNITCOST);

	if (s_PerDayDBEntryRollOverflag)
	{
		APP_LOG("InsertPerDayDBData", LOG_ALERT, "Insight Day Info Updating Database on RowId : %d", s_PerDayDBEntryCntr);
		memset(Condition, 0x00, sizeof(Condition));
		snprintf(Condition, sizeof(Condition), "ROWID = %u", s_PerDayDBEntryCntr);

		ColDetails InsightDayInfo[]={{"TIMESTAMP",TimeStamp},{"TODAYONTIME",TodayOnTime},{"TODAYONKWH",TodayKWH},{"TODAYSBYTIME",TodaySBYTime},{"TODAYSBYKWH",TodaySBYKWH},{"AVGPOWERON",AvgPowerOn},{"LAST14DAYSONTIME",Last14DaysOnTime},{"LAST14DAYSKWH",Last14DaysKWH},{"HOURSCONNECTED",HoursConnected},{"ENERGYCOST",PerUnitcostStr}};
		WeMoDBUpdateTable(&g_InsightDB, "InsightDayInfo", InsightDayInfo, MAX_DAY_INFO_COLUMN-1, Condition);
	}
        else
        {
		ColDetails InsightDayInfo[]={{"ROWID",RowID},{"TIMESTAMP",TimeStamp},{"TODAYONTIME",TodayOnTime},{"TODAYONKWH",TodayKWH},{"TODAYSBYTIME",TodaySBYTime},{"TODAYSBYKWH",TodaySBYKWH},{"AVGPOWERON",AvgPowerOn},{"LAST14DAYSONTIME",Last14DaysOnTime},{"LAST14DAYSKWH",Last14DaysKWH},{"HOURSCONNECTED",HoursConnected},{"ENERGYCOST",PerUnitcostStr}};
                WeMoDBInsertInTable(&g_InsightDB, "InsightDayInfo", InsightDayInfo, MAX_DAY_INFO_COLUMN);

        }
}

int writeDeviceInfoInCVS(FILE ** apFileCSV)
{
	FILE *FileCSV = *apFileCSV;
	char buffer[SIZE_256B];
	struct tm ts;
	time_t l_time;
	char Timebufer[SIZE_128B];
	char CurrencyStr[SIZE_32B];
	char *DeviceName = GetDeviceConfig("FriendlyName");
	char *EnergyCost = GetBelkinParameter(ENERGYPERUNITCOST);
	char *Currency   = GetBelkinParameter(CURRENCY);
	char *PowerThreshold = GetBelkinParameter(POWERTHRESHOLD);
	int Threshold = atoi(PowerThreshold);
	APP_LOG("writeDeviceInfoInCVS:", LOG_DEBUG, "Write Device Info in CSV file ...");
	memset(CurrencyStr, 0x0, sizeof(CurrencyStr));
	GetCurrencyString(atoi(Currency),CurrencyStr);
	APP_LOG("writeDeviceInfoInCVS", LOG_ERR, "\n Currency String: %s\n",CurrencyStr);

	if (!strlen(DeviceName))	
		DeviceName = "WeMo_Insight";

	APP_LOG("writeDeviceInfoInCVS:", LOG_DEBUG, "DeviceName:%s,MacAddress:%s,EnergyCost:%s,Currency:%s,PowerThreshold:%s", DeviceName,g_szWiFiMacAddress,EnergyCost,CurrencyStr,PowerThreshold);
	memset(buffer, 0x0, sizeof(buffer));
	snprintf(buffer, sizeof(buffer), "Exported Data for %s\n", DeviceName);
	APP_LOG("writeDeviceInfoInCVS:", LOG_DEBUG, "Written Line : %s", buffer);
	fputs(buffer, FileCSV);

	memset(buffer, 0x0, sizeof(buffer));
	//write header
	snprintf(buffer, sizeof(buffer), "Last Updated,Device Name,Device MAC,Signal Strength (%%),Energy Cost per kWh,Currency,Threshold (Watts)\n");
	fputs(buffer, FileCSV);
	APP_LOG("writeDeviceInfoInCVS:", LOG_DEBUG, "Written Line : %s", buffer);

	//ts = *localtime(&s_LastDBUpdatedTS);
	l_time = time(NULL);
	ts = *localtime(&l_time);
	memset(Timebufer, 0x0, sizeof(Timebufer));
	strftime(Timebufer, sizeof(Timebufer), "%Y/%m/%d %H:%M", &ts);
	APP_LOG("writeDeviceInfoInCVS:", LOG_DEBUG, "Written Line Timebufer: %s", Timebufer);

	memset(buffer, 0x0, sizeof(buffer));
	snprintf(buffer, sizeof(buffer), "%s,%s,%s,%d,%.5f,%s,%d\n", Timebufer,DeviceName,g_szWiFiMacAddress,gSignalStrength,(atoi(EnergyCost))/1000.0,CurrencyStr,Threshold/1000);
	APP_LOG("writeDeviceInfoInCVS:", LOG_DEBUG, "Written Line : %s", buffer);
	fputs(buffer, FileCSV);

	return 0;
}

int writeDailyUsageDataInCVS(FILE ** apFileCSV, int aDataExportscheduleType)
{
        FILE *FileCSV = *apFileCSV;
        char buffer[SIZE_256B];

	APP_LOG("writeDailyUsageDataInCVS:", LOG_DEBUG, "Write daily power data in CSV file ...");
	
	memset(buffer, 0x0, sizeof(buffer));
	snprintf(buffer, sizeof(buffer), "\nDaily Usage Summary\n");
	fputs(buffer, FileCSV);
	APP_LOG("writeDailyUsageDataInCVS:", LOG_DEBUG, "Written Line : %s", buffer);

	memset(buffer, 0x0, sizeof(buffer));
	snprintf(buffer, sizeof(buffer), "Date,Time ON (Hours:Minutes),Power Consumption ON (kWh),Time STANDBY (Hours:Minutes),Power Consumption STANDBY (kWh),Average Day Connected Device is ON (Hours),Average Power When ON (Watts),Estimated Monthly Cost,Day's Cost,Energy Cost per kWh\n");
	fputs(buffer, FileCSV);
	APP_LOG("writeDailyUsageDataInCVS:", LOG_DEBUG, "Written Line : %s", buffer);

	if(!s_PerDayDBEntryCntr)
		return 0;

	if(!s_PerDayDBEntryRollOverflag)
		InsertDailyUsageRowInCSV(apFileCSV, 1, s_PerDayDBEntryCntr, aDataExportscheduleType);
	else
	{
		if(MAX_PERDAY_ENTRY == s_PerDayDBEntryCntr)
			InsertDailyUsageRowInCSV(apFileCSV, 1, s_PerDayDBEntryCntr, aDataExportscheduleType);
		else
		{
			InsertDailyUsageRowInCSV(apFileCSV, 1, s_PerDayDBEntryCntr, aDataExportscheduleType);
			InsertDailyUsageRowInCSV(apFileCSV, s_PerDayDBEntryCntr + 1, MAX_PERDAY_ENTRY, aDataExportscheduleType);
		}
	}
	return 0;
}

void InsertDailyUsageRowInCSV(FILE ** apFileCSV, int StartIndx, int EndIndx, int aDataExportscheduleType)
{
	FILE *FileCSV = *apFileCSV;
	char buffer[SIZE_256B];
	struct tm ts;
	struct timeval tv;
	char Timebufer[SIZE_128B];
	int s32NoOfRows=0,s32NoOfCols=0;
	int s32RowCntr=0,s32ArraySize=0;
	char **s8ResultArray=NULL;
	char query[SIZE_256B];
	time_t TempTS;
	unsigned int TimeOn=0,KWHOn=0,TimeSBY=0,KWHSBY=0, Last14DaysOnTime = 0, HoursConnected =0;
	double Last14DaysKWH = 0.0;
	double AvgWhenON = 0.0;
	char *AvgPowerOn = NULL;
	int PerUnitCost = 0;
	unsigned int ONHr=0,ONMin=0,SBYHr=0,SBYMin=0;

	APP_LOG("InsertDailyUsageRowInCSV:", LOG_DEBUG, "Insert Daily Usage Row In CSV file, StartIndx: %d, EndIndx: %d", StartIndx, EndIndx);

	/*Insert today's entry*/
	time_t TimeNow;
	gettimeofday(&tv, NULL);
	TimeNow=tv.tv_sec;

	/*insert current usage data information in case of data export now.*/
	if(E_SEND_DATA_NOW == aDataExportscheduleType)
	{
		ts = *localtime(&TimeNow);
		memset(Timebufer, 0x0, sizeof(Timebufer));
		strftime(Timebufer, sizeof(Timebufer), "%Y/%m/%d", &ts);
		APP_LOG("writeDeviceInfoInCVS:", LOG_DEBUG, "Written Line Timebufer: %s", Timebufer);
		PerUnitCost = atoi(GetBelkinParameter(ENERGYPERUNITCOST));
		/*insert today's data*/
		memset(buffer, 0x0, sizeof(buffer));
		if(g_TodayONTimeTS > HRS_SEC_24)
		    g_TodayONTimeTS = HRS_SEC_24;
		if(g_TodaySBYTimeTS > HRS_SEC_24)
		    g_TodaySBYTimeTS = HRS_SEC_24;
		if(g_TodayONTimeTS != 0)
	        {
		    ONHr =  g_TodayONTimeTS/3600;
		    ONMin= (g_TodayONTimeTS%3600);
		    if(ONMin != 0)
			ONMin = ONMin/60;
		    else
			ONMin=0;
		}
		if(g_TodaySBYTimeTS != 0)
		{
		    SBYHr = g_TodaySBYTimeTS/3600;
		    SBYMin = (g_TodaySBYTimeTS%3600);
		    if(SBYMin != 0)
			SBYMin = SBYMin/60;
		    else
			SBYMin=0;
		}
		AvgWhenON = (g_TotalONTime14Days* 24.0)/g_HrsConnected;
		if(AvgWhenON > 24)
		    AvgWhenON = 24.00;
		snprintf(buffer, sizeof(buffer), "%s,%d:%d,%.5f,%d:%d,%.5f,%.5f,%u,%.5f,%.5f,%.5f\n", Timebufer,ONHr,ONMin,g_TodayKWHON/(1000*60000.0),SBYHr,SBYMin,g_TodayKWHSBY/(1000*60000.0),AvgWhenON,g_AvgPowerON,((g_KWH14Days/(1000*1000*60.0))/(g_HrsConnected/3600.0))*730*(PerUnitCost/1000.0),((g_TodayKWHON+g_TodayKWHSBY)/(1000*60000.0))*(PerUnitCost/1000.0),(PerUnitCost/1000.0));
		//insert row in CSV file
		APP_LOG("InsertDailyUsageRowInCSV:", LOG_DEBUG, "Written Line : %s", buffer);
		fputs(buffer, FileCSV);
	}

	memset(query, 0, sizeof(query));
	snprintf(query, sizeof(query), "select * from InsightDayInfo where ROWID between %d and %d order by TIMESTAMP desc;",StartIndx, EndIndx);
	if(WeMoDBGetTableData(&g_InsightDB,query,&s8ResultArray,&s32NoOfRows,&s32NoOfCols))
		return;
	s32ArraySize = (s32NoOfRows + 1)*s32NoOfCols;

	for(s32RowCntr = MAX_DAY_INFO_COLUMN; s32RowCntr < s32ArraySize; s32RowCntr++)
	{
		ONHr=0;
		ONMin=0;
		SBYHr=0;
		SBYMin=0;
		//skipping rowID
		TempTS = atol(s8ResultArray[++s32RowCntr]);
		APP_LOG("writeDeviceInfoInCVS:", LOG_DEBUG, "Time in DB: %d", TempTS);
		ts = *localtime(&TempTS);
		memset(Timebufer, 0x0, sizeof(Timebufer));
		strftime(Timebufer, sizeof(Timebufer), "%Y/%m/%d %H;%M", &ts);
		APP_LOG("writeDeviceInfoInCVS:", LOG_DEBUG, "Written Line Timebufer With Time: %s", Timebufer);
		memset(Timebufer, 0x0, sizeof(Timebufer));
		strftime(Timebufer, sizeof(Timebufer), "%Y/%m/%d", &ts);
		APP_LOG("writeDeviceInfoInCVS:", LOG_DEBUG, "Written Line Timebufer: %s", Timebufer);

		TimeOn = strtoul(s8ResultArray[++s32RowCntr],NULL,0);
		KWHOn = strtoul(s8ResultArray[++s32RowCntr],NULL,0); 
		TimeSBY = strtoul(s8ResultArray[++s32RowCntr],NULL,0);
		KWHSBY = strtoul(s8ResultArray[++s32RowCntr],NULL,0); 
		AvgPowerOn = s8ResultArray[++s32RowCntr];
		Last14DaysOnTime = strtoul(s8ResultArray[++s32RowCntr],NULL,0); 
		Last14DaysKWH = atof(s8ResultArray[++s32RowCntr]);
		HoursConnected = strtoul(s8ResultArray[++s32RowCntr],NULL,0); 
		PerUnitCost = atoi(s8ResultArray[++s32RowCntr]);
		APP_LOG("writeDeviceInfoInCVS:", LOG_DEBUG, "Date: %s TimeOn: %d;KWHOn: %d; TimeSBY: %d KWHSBY: %d;Last14DaysOnTime: %d;Last14DaysKWH: %f;HoursConnected: %d;PerUnitCost: %d",Timebufer,TimeOn,KWHOn,TimeSBY,KWHSBY,Last14DaysOnTime,Last14DaysKWH,HoursConnected,PerUnitCost);
		if(TimeOn > HRS_SEC_24)
		    TimeOn = HRS_SEC_24;
		if(TimeSBY > HRS_SEC_24)
		    TimeSBY = HRS_SEC_24;
		if(TimeOn != 0)
	        {
		    ONHr = TimeOn/3600;
		    ONMin= (TimeOn%3600);
		    if(ONMin != 0)
			ONMin = ONMin/60;
		    else
			ONMin=0;
		}
		if(TimeSBY != 0)
		{
		    SBYHr = TimeSBY/3600;
		    SBYMin = (TimeSBY%3600);
		    if(SBYMin != 0)
			SBYMin = SBYMin/60;
		    else
			SBYMin=0;
		}
		AvgWhenON = 0.0;
		AvgWhenON = (Last14DaysOnTime* 24.0)/HoursConnected;
		if(AvgWhenON > 24)
		    AvgWhenON = 24.00;
		memset(buffer, 0x0, sizeof(buffer));
		snprintf(buffer, sizeof(buffer), "%s,%d:%d,%.5f,%d:%d,%.5f,%.5f,%s,%.5f,%.5f,%.5f\n", Timebufer,ONHr,ONMin,KWHOn/(1000*60000.0),SBYHr,SBYMin,KWHSBY/(1000*60000.0),AvgWhenON,AvgPowerOn,((Last14DaysKWH/(1000*1000*60.0))/(HoursConnected/3600.0))*730*(PerUnitCost/1000.0),((KWHOn+KWHSBY)/(1000*60000.0))*(PerUnitCost/1000.0),(PerUnitCost/1000.0));
		//insert row in CSV filea
		APP_LOG("InsertDailyUsageRowInCSV:", LOG_DEBUG, "Written Line : %s", buffer);
		fputs(buffer, FileCSV);
	}
	WeMoDBTableFreeResult(&s8ResultArray,&s32NoOfRows,&s32NoOfCols);
	return;
}

int writeEnergyDataInCVS(FILE ** apFileCSV, int aDataExportscheduleType)
{
        FILE *FileCSV = *apFileCSV;
        char buffer[SIZE_256B];

	APP_LOG("writeEnergyDataInCVS:", LOG_DEBUG, "Write 30 min power data in CSV file ...");

	memset(buffer, 0x0, sizeof(buffer));
	snprintf(buffer, sizeof(buffer), "\nEnergy Data\n");
	fputs(buffer, FileCSV);
	APP_LOG("writeEnergyDataInCVS:", LOG_DEBUG, "Written Line : %s", buffer);

	memset(buffer, 0x0, sizeof(buffer));
	snprintf(buffer, sizeof(buffer), "Date & Time,Power Consumed for past 30 mins (kWh)\n");
	fputs(buffer, FileCSV);
	APP_LOG("writeEnergyDataInCVS:", LOG_DEBUG, "Written Line : %s", buffer);

	if(!s_30MinDBEntryCntr)
		return 0;

	if(!s_30MinDBEntryRollOverflag)
		Insert30MinRowInCSV(apFileCSV, 1, s_30MinDBEntryCntr, aDataExportscheduleType);
	else
	{
		if(MAX_30MIN_ENTRY == s_30MinDBEntryCntr)
			Insert30MinRowInCSV(apFileCSV, 1, s_30MinDBEntryCntr, aDataExportscheduleType);
		else
		{
			Insert30MinRowInCSV(apFileCSV, 1, s_30MinDBEntryCntr, aDataExportscheduleType);
			Insert30MinRowInCSV(apFileCSV, s_30MinDBEntryCntr + 1, MAX_30MIN_ENTRY, aDataExportscheduleType);
		}
	}
	return 0;
}

void Insert30MinRowInCSV(FILE ** apFileCSV, int StartIndx, int EndIndx, int aDataExportscheduleType)
{
	FILE *FileCSV = *apFileCSV;
        char buffer[SIZE_256B];
	struct tm ts;
	struct timeval tv;
	char Timebufer[SIZE_128B];
	int s32NoOfRows=0,s32NoOfCols=0;
	int s32RowCntr=0,s32ArraySize=0;
	char **s8ResultArray=NULL;
	char query[256];
	time_t TempTS = 0;
	int TempKWH = 0;

	APP_LOG("InsertDailyUsageRowInCSV:", LOG_DEBUG, "Insert 30 min power Usage Row In CSV file, StartIndx: %d, EndIndx: %d", StartIndx, EndIndx);
	
	/*Inser current entry*/
	time_t TimeNow;
	gettimeofday(&tv, NULL);
	TimeNow=tv.tv_sec;

	/*insert current usage data information in case of data export now.*/
	if(E_SEND_DATA_NOW == aDataExportscheduleType)
	{
		ts = *localtime(&TimeNow);
		memset(Timebufer, 0x0, sizeof(Timebufer));
		strftime(Timebufer, sizeof(Timebufer), "%Y/%m/%d %H:%M", &ts);
		APP_LOG("writeDeviceInfoInCVS:", LOG_DEBUG, "Written Line Timebufer: %s", Timebufer);

		TempKWH = s_TotalKWHSinceIsInsightUP - s_Last30MinKWH;

		memset(buffer, 0x0, sizeof(buffer));
		snprintf(buffer, sizeof(buffer), "%s,%.5f\n", Timebufer, TempKWH/(1000*60000.0));
		//insert row in CSV file
		APP_LOG("Insert30MinRowInCSV:", LOG_DEBUG, "Written Line : %s", buffer);
		fputs(buffer, FileCSV);
	}	

	memset(query, 0, sizeof(query));
	snprintf(query, sizeof(query), "select TIMESTAMP,KWHCONSUMED from Insight30MinData where ROWID between %d and %d order by TIMESTAMP desc;",StartIndx, EndIndx);
	if(WeMoDBGetTableData(&g_InsightDB,query,&s8ResultArray,&s32NoOfRows,&s32NoOfCols))
		return;
	s32ArraySize = (s32NoOfRows + 1)*s32NoOfCols;

	for(s32RowCntr = 2; s32RowCntr < s32ArraySize; s32RowCntr++)
	{
		TempTS = atol(s8ResultArray[s32RowCntr]);

		ts = *localtime(&TempTS);
		memset(Timebufer, 0x0, sizeof(Timebufer));
		strftime(Timebufer, sizeof(Timebufer), "%Y/%m/%d %H:%M", &ts);
		APP_LOG("writeDeviceInfoInCVS:", LOG_DEBUG, "Written Line Timebufer: %s", Timebufer);

		TempKWH = strtoul(s8ResultArray[++s32RowCntr],NULL,0); 

		memset(buffer, 0x0, sizeof(buffer));
		snprintf(buffer, sizeof(buffer), "%s,%.5f\n", Timebufer, TempKWH/(1000*60000.0));
		//insert row in CSV file
		APP_LOG("Insert30MinRowInCSV:", LOG_DEBUG, "Written Line : %s", buffer);
		fputs(buffer, FileCSV);
	}
	WeMoDBTableFreeResult(&s8ResultArray,&s32NoOfRows,&s32NoOfCols);
}

void ResetWiFiSetting(void)
{
	APP_LOG("ResetWiFiSetting:", LOG_DEBUG, "Reset WiFi Settings");
	/* Remove saved IP from flash */
	UnSetBelkinParameter ("wemo_ipaddr");
	ControlleeDeviceStop();
	setRemoteRestoreParam(0x1);
	SetBelkinParameter(WIFI_CLIENT_SSID,"");
	AsyncSaveData();
	APP_LOG("ResetWiFiSetting:", LOG_DEBUG, "Going To Self Reboot...");
	system("reboot");
}

void InsightParamUpdate(void)
{
	pMessage msg = 0x00;
	msg = createMessage(UPNP_MESSAGE_PWR_IND, 0x00, 0x00);
	SendMessage2App(msg);
}

void ResetDayInfo(int aTodayDate)
{
	char CharTime[SIZE_32B];
	
	/*insert per day power data*/
	InsertPerDayDBData();

	/*insert 30 min power data to avoid discrepency of data*/
	s_AdjustData = 1;
	/*Update 30 min data on falsh and DB*/
	Update30MinDataOnFlash();

	/*update today date*/
	g_TodayDate = aTodayDate;
	memset(CharTime, 0x00, sizeof(CharTime));
	snprintf(CharTime, sizeof(CharTime), "%u", g_TodayDate);
	APP_LOG("APP", LOG_DEBUG, "Setting TODAYSDATE: %s", CharTime);
	SetBelkinParameter (TODAYSDATE, CharTime);
	AsyncSaveData();

	/*Populate Yesterday variables*/
	g_YesterdayKWHON = g_TodayKWHON;
	g_YesterdayKWHSBY = g_TodayKWHSBY;
	g_YesterdayONTime= g_TodayONTime;
	g_YesterdaySBYTime= g_TodaySBYTime;

	/*reset today variables*/
	g_AccumulatedWattMinute = 0;
	g_TodayKWHON = 0;
	g_TodayKWHSBY = 0;
	g_TodayONTime=0;
	g_TodayONTimeTS=0;
	g_TodaySBYTime=0;
	g_TodaySBYTimeTS=0;

	/*Populate globals with 14Days Data*/
	Populate14DaysData();
}



