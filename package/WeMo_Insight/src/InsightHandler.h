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
 * InsightHandler.h
 *
 *  Created on: Nov 5, 2012
 *      Author: Anand
 */

#ifndef __INSIGHTHANDLER_H__
#define __INSIGHTHANDLER_H__
#include <stdio.h>
#include "insight.h"
#include "WemoDB.h"
#include "rule.h"

#define DBURL				"/tmp/Belkin_settings/Insight.db"
#define MAX_PERDAY_ENTRY		45
#define MAX_30MIN_ENTRY			45*24*2//2160, max 30 minute entries in 45 days
#define MAX_STATE_CHANGE_ENTRY		500
#define MAX_NO_OF_DAYS			13
#define DAILY_DB_SYNC_TIME		5
#define SETUP_COMPLETE_SLEEP_TIME	20
#define MAX_DAY_INFO_COLUMN		11
#define MAX_STATE_CHANGE_INFO_COLUMN	5
#define MAX_30_MIN_INFO_COLUMN		3
#define Y2K				2000

//external variables
extern sqlite3 *g_InsightDB;
extern char g_InsightDataSend;
extern void UploadInsightDataFile(void *arg);

extern unsigned int g_ONFor;
extern unsigned int g_InSBYSince;
extern unsigned int g_TodayONTime;
extern unsigned int g_TodaySBYTime;
extern char g_SendToCloud;
extern int gInsightHealthPunch;

void *InsightGPIOTask(void *args);
void *InsightEventTask(void *args);
void InitInsightDB(void);
void *StartInsightTask(void * args);
void Populate14DaysData(void);
void PopulateAvgWhenOn(void);
void PopulateDBCntrAndFlags(void);
void FetchDBCntrAndFlags(char * pFromTable, char * pColumn, char * pOrderBy, unsigned int * pUpdateVariable);
void PopulateInsightVariable(char * pParam, char * pDefaultParam, int aSetOrGet, unsigned int * pUpdateVariable);
void CheckAndSetDevState(unsigned short int aCurState);
void calculateAvgOn(void);
void ClearUsageData(void);
void ClearInsightUsageData(void);
void InitInsightVariables(void);
void ReInitInsightVariables(void);
void ExecuteInsightDataExport(char *szEmailAddress, char *szDataExportType);
void StartDataExportScheduler(void);
void ReStartDataExportScheduler(void);
void *CreateInsightExportData(void *arg);
void *ComposeAndSendCSVFileThread(void *arg);
void Insert30MinDBData(void);
void InsertPerDayDBData(void);
int writeDeviceInfoInCVS(FILE ** apFileCSV);
int writeDailyUsageDataInCVS(FILE ** apFileCSV, int aDataExportscheduleType);
int writeEnergyDataInCVS(FILE ** apFileCSV, int aDataExportscheduleType);
void Insert30MinRowInCSV(FILE ** apFileCSV, int StartIndx, int EndIndx, int aDataExportscheduleType);
void InsertDailyUsageRowInCSV(FILE ** apFileCSV, int StartIndx, int EndIndx, int aDataExportscheduleType);
int GetDaysToSunday(char * TodayDay);
unsigned long int GetDailyTS(time_t timeNow);
unsigned long int GetWeekSundayTS(time_t timeNow);
unsigned long int GetMonthSundayTS(void);
void InsightParamUpdate(void);
void WaitForInitialSetupAndTimeSync(void);
void ResetDayInfo(int aTodayDate);
void StateChangeTimeStamp(unsigned short int aCurState);

#endif

