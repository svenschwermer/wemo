/***************************************************************************
 *
 *
 * rule.c
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
#include <sys/wait.h>
#include <sys/time.h>
#include "utils.h"
#include "osUtils.h"
#include "rule.h"
#include "global.h"
#include "logger.h"
#include <time.h>
#include <stdbool.h>
#include "upnpCommon.h"
#include "gpio.h"
#include "wifiSetup.h"
#include "controlledevice.h"
#include "remoteAccess.h"
#include "plugin_ctrlpoint.h"
#include "sunriset.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#ifdef SIMULATED_OCCUPANCY
#include "simulatedOccupancy.h"
#endif
#if defined(PRODUCT_WeMo_Jarden) || defined(PRODUCT_WeMo_Smart)
#include "wasp_api.h"
#include "wasp.h"
#endif
#include "sqlite3.h"
#include "WemoDB.h"
#ifdef PRODUCT_WeMo_LEDLight
#include "subdevice.h"
#include "sd_configuration.h"
#include "bridge_away_rule.h"
#include "bridge_sensor_rule.h"
static SD_CapabilityID sCapabilitymanualnotify = {SD_CAPABILITY_ID, "20500"};
#endif
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */
#if 1
ithread_t g_handlerSchedulerTask = INVALID_THREAD_HANDLE;
#endif
extern pthread_t dbFile_thread;
extern unsigned long int GetNTPUpdatedTime(void);
extern void remoteAccessUpdateStatusTSParams(int status);
#ifdef PRODUCT_WeMo_Jarden
extern int g_phoneFlag;
#endif

#ifdef PRODUCT_WeMo_Smart
int g_SmartAttributeID = 0x00;
#endif

extern unsigned int g_ONFor;
extern int gDstSupported;
sqlite3 *g_RulesDB=NULL;
int current_date=0;
int gSubscribeBridgeService=0;

fpRuleCallback gfpRuleThreadFn[e_MAX_RULE] = 
{
	RulesTask,
	TimerTask,
	NULL,
	InsightTask,
	AwayTask,
	NULL,
	NULL,
	CrockpotTask,
	MakersensorTask
};

unsigned int g_SendRuleID=0x00;
	
SRuleInfo *gpsRuleList = NULL, *gpsRuleListTail = NULL;
STimerList *gpsTimerList = NULL;
SRulesQueue *gRuleQHead = NULL, *gRuleQTail = NULL;
volatile int gCountdownRuleInLastMinute = 0;
unsigned long gCountdownEndTime = 0;

#define ONE_DAY_SECONDS 86400 //-24 * 60 * 60
static pthread_mutex_t   s_timer_rule_mutex;
static pthread_mutex_t   s_rule_mutex;
static pthread_mutex_t	 s_ruleQ_mutex;
#ifndef PRODUCT_WeMo_LEDLight
static pthread_t	 s_countdown_rule_thread = -1;
#endif

void *RulesNtpTimeCheckTask(void *args);
ithread_t g_handlerRuleNtpTimeCheckTask = INVALID_THREAD_HANDLE;

SRuleHandle gRuleHandle[e_MAX_RULE]; /* Includes thread handle for Scheduler at index 0, simple rules are executed by scheduler */

char* g_szRuleTypeStrings[] = 
{ 
	"Simple Switch",
	"Time Interval",
	"Motion Controlled",
	"Insight Rule",
	"Away Mode",
	"Notify Me",
	"Countdown Rule",
	"Crockpot Schedule",
	"Maker Sensor Rule"
};

char* szWeekDayName[] = 
{
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
    "Sunday"
};

int daySeconds(void)
{
    time_t rawTime;
    struct tm * timeInfo;
    time(&rawTime);
    timeInfo = localtime (&rawTime);
    int seconds = timeInfo->tm_hour * 60 * 60 + timeInfo->tm_min * 60 + timeInfo->tm_sec;
    return seconds;
}


void setActuation(char *str)
{
	if(!str)
		return;

	memset(g_szActuation, 0, sizeof(g_szActuation));
	strncpy(g_szActuation, str, sizeof(g_szActuation)-1);
}

void setRemote(char *str)
{
	if(!str)
		return;

	memset(g_szRemote, 0, sizeof(g_szRemote));
	strncpy(g_szRemote, str, sizeof(g_szRemote)-1);
}

void GetCalendarDayInfo(int* dayIndex, int* monthIndex, int* year, int* nowSeconds)
{
	time_t rawTime;
	struct tm * timeInfo;
	time(&rawTime);
	timeInfo = localtime (&rawTime);

	*year                = timeInfo->tm_year + 1900;
	*monthIndex          = timeInfo->tm_mon;
	int dayOfWeek        = timeInfo->tm_wday;
	current_date = timeInfo->tm_mday;

	//-map the
	if (0x00 == dayOfWeek)
		dayOfWeek = 0x06;
	else
		dayOfWeek -=1;

	*dayIndex = dayOfWeek;

	*nowSeconds = timeInfo->tm_hour * 60 * 60 + timeInfo->tm_min * 60 + timeInfo->tm_sec;

}


int GetRuleDBHandle()
{
	struct stat FileInfo;
	char tmpBuff[200];

	/*Close the previous g_RulesDB*/
	CloseDB(g_RulesDB);

	APP_LOG("GetRuleDBHandle:", LOG_DEBUG, "Removing OLD Extracted DB File");
	snprintf(tmpBuff,sizeof(tmpBuff),"rm -rf %s",RULE_EXTRACT_DIR);
	system(tmpBuff);

	/*Checking for the existence of the rules DB files in ZIPPED Form*/
	if(stat(RULE_DB_FILE_PATH, &FileInfo) == -1)
	{
		APP_LOG("GetRuleDBHandle:", LOG_CRIT, "!!!!Rules DB file doesn't exists");
		return 0x01;
	}

	APP_LOG("GetRuleDBHandle:", LOG_DEBUG, "Extract New DB File");
	snprintf(tmpBuff,sizeof(tmpBuff),"unzip %s -d %s",RULE_DB_FILE_PATH,RULE_EXTRACT_DIR);
	system(tmpBuff);

	/*Checking for the existence of the rules DB files.*/
	if(stat(RULE_DB_URL, &FileInfo) == -1)
	{
		APP_LOG("GetRuleDBHandle:", LOG_CRIT, "!!Extracted Rules DB file doesn't exists");
		return 0x01;
	}
	APP_LOG("GetRuleDBHandle:", LOG_DEBUG, " Init Rule DB");

	if(InitDB(RULE_DB_URL,&g_RulesDB))
	{
		APP_LOG("GetRuleDBHandle:", LOG_CRIT, "Cannot Init Rules DB");
		return 0x01;

	}
	APP_LOG("GetRuleDBHandle:", LOG_DEBUG, " Init Rule DB done...");
	return 0x00;
}

int ActivateRuleEngine()
{
	/*Rule task main thread*/
	int ret = ithread_create(&gRuleHandle[e_SIMPLE_RULE].ruleThreadId, 0x00, gfpRuleThreadFn[e_SIMPLE_RULE], 0x00);
	if (0x00 != ret)
	{
		APP_LOG("UPNP: Rule", LOG_CRIT, "ithread_create: Can not create rule task thread");
		resetSystem();
	}
	else
		APP_LOG("UPNP: Rule", LOG_DEBUG, "scheduler thread created");
	
	return 0;
}

void initRule()
{
	int i;

	osUtilsCreateLock(&s_rule_mutex);
	osUtilsCreateLock(&s_timer_rule_mutex);
	osUtilsCreateLock(&s_ruleQ_mutex);

	/*set rule main thread to RULE_ENGINE_RELOAD state*/
	gRestartRuleEngine = RULE_ENGINE_RELOAD;
	
	/*init rule Handles*/
	for(i = 0; i < e_MAX_RULE; i++)
	{
		gRuleHandle[i].ruleThreadId = INVALID_THREAD_HANDLE;
		gRuleHandle[i].ruleCnt = 0;
	}

#ifdef PRODUCT_WeMo_LEDLight
	if(!BS_LoadReactor())
	{
		APP_LOG("UPNP: Rule", LOG_ERR, "Bridge sensor load reactor intialization failed");
		resetSystem();
	}
	else
		APP_LOG("UPNP: Rule", LOG_DEBUG, "Bridge sensor load reactor intialized");
#endif
	/*NTP check should from here since need to cover the service subscription issue because of time sync*/
	ithread_create(&g_handlerRuleNtpTimeCheckTask, 0x00, RulesNtpTimeCheckTask, 0x00);
	APP_LOG("UPNP: Rule", LOG_DEBUG, "Ntp time check thread is created");
}

void lockRuleQueue()
{
	osUtilsGetLock(&s_ruleQ_mutex);
}

void unlockRuleQueue()
{
	osUtilsReleaseLock(&s_ruleQ_mutex);
}

void lockTimerRule()
{
	osUtilsGetLock(&s_timer_rule_mutex);
}

void unlockTimerRule()
{
	osUtilsReleaseLock(&s_timer_rule_mutex);
}

void lockRule()
{
	osUtilsGetLock(&s_rule_mutex);
}

void unlockRule()
{
	osUtilsReleaseLock(&s_rule_mutex);
}

int GetRuleIDFlag()
{
	return  g_SendRuleID;
}

void SetRuleIDFlag(int FlagState)
{
	g_SendRuleID = FlagState;
}

extern int g_isTimeSyncByMobileApp;
extern int g_eDeviceType;

void *RulesNtpTimeCheckTask(void *args)
{
		tu_set_my_thread_name( __FUNCTION__ );
	sleep(DELAY_3SEC);

	/*Loop time NTP time sync*/
	while(1)
	{
		sleep(DELAY_3SEC);
		
		/*check if NTP is updated and device is connected*/
		if(IsNtpUpdate() && getCurrentClientState())
		{
			APP_LOG("Rule", LOG_DEBUG, "NTP updated, rule NTP time check task stop");
			
			/*Activate Rule Engine*/
			ActivateRuleEngine();
			break;
		}
	}
	return NULL;
}




#define		NTP_UPDATE_TIMEOUT     86400	//24 hours

extern int gNTPTimeSet;

int IsNtpUpdate()
{
    if (IsTimeUpdateByMobileApp() == 0x01)
        return 0x01;
    int year = 0x00, monthIndex = 0x00, seconds = 0x00, dayIndex = 0x00;

    GetCalendarDayInfo(&dayIndex, &monthIndex, &year, &seconds);

    if(year != 2000)
    {
        gNTPTimeSet = 1;
	UDS_SetNTPTimeSyncStatus();
	APP_LOG("DEVICE:rule", LOG_DEBUG, "************* ISNTPUPDATE NOW YEAR IS NOT 2000");
	return 0x01;
    }
    else
    {
	return 0x00;
    }

}

int stopAllExecutorThreads()
{
	APP_LOG("UPNP: Rule", LOG_DEBUG, "Stopping All Executor Threads");

	ERuleType i;
	int ret;

	/*stop all type of rule executers whoes count is atleast one*/
	for(i = e_TIMER_RULE; i < e_MAX_RULE; i++)
	{
		/*check if rule ndle is valide*/
		if (INVALID_THREAD_HANDLE != gRuleHandle[i].ruleThreadId)
		{
			/*cancel thread*/
			ret = ithread_cancel(gRuleHandle[i].ruleThreadId);
			if (0x00 != ret)
			{
				APP_LOG("UPNP: Rule", LOG_DEBUG, "################### ithread_cancel: Couldnt stop rule thread %d #########################", i);
			}
			else
			{
				APP_LOG("UPNP: Rule", LOG_DEBUG, "################### ithread_cancel: Successfully stop rule thread %d ####################", i);
			}

			gRuleHandle[i].ruleThreadId = INVALID_THREAD_HANDLE;
		}
		//[WEMO-36379] - clear ruleCnt in case thread was already invalid
		gRuleHandle[i].ruleCnt = 0;
	}
	
	/*mark simple rule count to zero as simple rule are executed by main RulesTask thread which cannot be stopped*/
	gRuleHandle[e_SIMPLE_RULE].ruleCnt = 0;

	return SUCCESS;
}

void freeTimerList()
{
	APP_LOG("UPNP: Rule", LOG_DEBUG, "Free Timer Rule List Memory");
	STimerList *psTimerList = NULL, *psTimerNext = NULL;

	lockTimerRule();

	/*free timer list*/
	psTimerList = gpsTimerList;
	while(psTimerList)
	{
		psTimerNext = psTimerList->nextTimer;
		free(psTimerList);
		psTimerList = psTimerNext;
	}
	gpsTimerList=NULL;
	APP_LOG("UPNP: Rule", LOG_DEBUG, "Freed timer list");

	unlockTimerRule();
	return;
}

int freeRuleList()
{
	APP_LOG("UPNP: Rule", LOG_DEBUG, "Free Rule Memory");
			
	SRuleInfo *psRule = NULL, *psNext = NULL;

	lockRule();

	/*free RuleList*/
	psRule = gpsRuleList;
	while(psRule)
	{
		psNext = psRule->psNext;
		free(psRule);
		psRule = psNext;
	}
	gpsRuleList = NULL;
	APP_LOG("UPNP: Rule", LOG_DEBUG, "Freed Rule list");

	unlockRule();

	return SUCCESS;	
}

int StopRuleEngine()
{
	/*stop all running rule executer threads*/
	stopAllExecutorThreads();
	/*Free all rule memory*/
	freeRuleList();
	/*Free all timer rule memory*/
	freeTimerList();
	/*Free rule queue*/
	destroyRuleQueue();
#ifdef PRODUCT_WeMo_LEDLight
	APP_LOG("UPNP: Rule", LOG_DEBUG, "#### rule deleted BA_FreeRule####");
	BA_FreeRuleTask();
	if (gRuleHandle[e_AWAY_RULE].ruleCnt)
		gRuleHandle[e_AWAY_RULE].ruleCnt = 0;
#else
#ifdef SIMULATED_OCCUPANCY
	/*cleanup Away Rule data*/
	cleanupAwayRule(1);
#endif
#endif
	return 0;
}

int loadLatLongValues(double *lat, double *lon)
{
	int rowsRules=0,colsRules=0;
	char **ppsRulesArray=NULL;
	char query[SIZE_256B];

	memset(query, 0, sizeof(query));
	snprintf(query, sizeof(query), "SELECT latitude, longitude FROM LOCATIONINFO limit 1;");

	if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
	{
		if(rowsRules && colsRules)
		{
			APP_LOG("DEVICE:rule", LOG_DEBUG, "Fetched %d rows, %d columns", rowsRules, rowsRules);
		}
		else
		{
			APP_LOG("DEVICE:rule", LOG_ERR, "No entry found");
			WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
			return FAILURE;
		}

		APP_LOG("DEVICE:rule", LOG_DEBUG, "lat: %s, lon: %s", ppsRulesArray[2],  ppsRulesArray[3]);
		*lat = atof(ppsRulesArray[2]);
		*lon = atof(ppsRulesArray[3]);

		WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
	}
	return SUCCESS;
}

int loadOffsetValues(int ruleId, int *start, int *end)
{
	int rowsRules=0,colsRules=0;
	char **ppsRulesArray=NULL;
	char query[SIZE_256B];

	memset(query, 0, sizeof(query));
	snprintf(query, sizeof(query), "SELECT OnModeOffset, OffModeOffset FROM RULEDEVICES where RuleID=\"%d\" limit 1;", ruleId);
	APP_LOG("DEVICE:rule", LOG_DEBUG, "query:%s", query);

	if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
	{
		if(rowsRules && colsRules)
		{
			APP_LOG("DEVICE:rule", LOG_DEBUG, "Fetched %d rows, %d columns", rowsRules, rowsRules);
		}
		else
		{
			APP_LOG("DEVICE:rule", LOG_ERR, "No entry found");
			WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
			return FAILURE;
		}

		APP_LOG("DEVICE:rule", LOG_DEBUG, "start offset: %s, end offset: %s", ppsRulesArray[2],  ppsRulesArray[3]);
		*start = atof(ppsRulesArray[2]);
		*end = atof(ppsRulesArray[3]);

		WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
	}
	return SUCCESS;
}

int calculateSunRiseSetTimes(int date_offset, double lat, double lon, int *rise_hours, int *rise_mins, int *set_hours, int *set_mins)
{
	double trise=0x00, tset=0x00;
	int nowSeconds=0, year=0, month_index=0, dayIndex=0;
	int month;

	GetCalendarDayInfo(&dayIndex, &month_index, &year, &nowSeconds);

	if(month_index == 11)
	{
		month = 0;
	}
	else
	{
		month= month_index+1;
	}
	APP_LOG("Rules", LOG_DEBUG, "year: %d, month: %d, date: %d, day index: %d", year, month, current_date, dayIndex);
	/* calculate sunrise/set -- call the macro from sunriset.h */
	sun_rise_set(year, month, current_date + date_offset, lon, lat, &trise, &tset);

	APP_LOG("Rules", LOG_DEBUG, "trise:%lf, tset:%lf", trise, tset);

	*rise_hours = HOURS(trise);
	*rise_mins = MINUTES(trise);
	APP_LOG("Rules", LOG_DEBUG, "Sunrise timings for the day are: time: %d, minutes is %d, date is %d", *rise_hours, 
			*rise_mins, current_date + date_offset);

	*set_hours = HOURS(tset);
	*set_mins = MINUTES(tset);
	APP_LOG("Rules", LOG_DEBUG, "Sunset timings for the day are: time: %d, minutes is %d, date is %d", *set_hours, 
			*set_mins, current_date+ date_offset);
	return SUCCESS;
}

int UpdateSunriseSunset (SRuleInfo *psRule)
{
	int rise_hours = 0x00;
	int rise_mins = 0x00;
	int set_hours = 0x00;
	int set_mins = 0x00;
	int timezone_offset = 0x00;
	int time_hours = 0x00;
	int start_offset = 0, end_offset = 0;
	float time_minutes = 0.0;
	float adjusted_time_zone = 0.0;
	int start_time = psRule->startTime;
	int end_time = 0;
	double lon=0x00, lat=0x00;
	int x=0;

	if(!psRule->endTime)
	{
		end_time = psRule->startTime + psRule->ruleDuration; /* Old DB, No EndTime */
	}
	else {
		end_time = psRule->endTime; /* EndTime from new DB */
	}
	APP_LOG("Rules", LOG_DEBUG, "*************UpdateSunriseSunset***************");
	APP_LOG("DEVICE:rule", LOG_DEBUG, "initial psRule->startTime: %d, psRule->ruleDuration: %d, end_time: %d,psRule->endTime: %d",\
			psRule->startTime, psRule->ruleDuration,end_time,psRule->endTime);

	loadLatLongValues(&lat, &lon);
	APP_LOG("Rules", LOG_DEBUG, "Latitude is %lf, Longitude is %lf", lat, lon);

	loadOffsetValues(psRule->ruleId, &start_offset, &end_offset);

	/* Time zone adjustment based on stored value */
	/* As we are storing value in float ex. 5.5 for 5:30, we need to convert .5 into minutes,
	 * So need to multiply with 60 before converting into Sec. 
	 */
	APP_LOG("DEVICE:rule", LOG_DEBUG, "g_lastTimeZone: %f, gDstSupported: %d, gDstEnable: %f", g_lastTimeZone, gDstSupported, gDstEnable);

	if(gDstSupported && !gDstEnable)
	{
		adjusted_time_zone =  g_lastTimeZone + 1;

		APP_LOG("DEVICE:rule", LOG_DEBUG, "g_lastTimeZone: %f, adjusted_time_zone: %f", g_lastTimeZone, adjusted_time_zone);
	}
	else
		adjusted_time_zone =  g_lastTimeZone;

	time_hours =  (int)adjusted_time_zone;
	time_minutes = adjusted_time_zone - time_hours;
	timezone_offset = ((time_hours * 60) +(time_minutes*60)) * 60;

	calculateSunRiseSetTimes(0, lat, lon, &rise_hours, &rise_mins, &set_hours, &set_mins);

	
	APP_LOG("Rules", LOG_DEBUG, "/* startTime calculation */");
	/* Check whether Sunrise timer and update new sunrise time*/
	if(start_time % UNITS_DIGIT_DET == 1)
	{
		psRule->startTime = timezone_offset +(((rise_hours * 60) +rise_mins) * 60);
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Updated Sunrise time: %d", psRule->startTime);

		psRule->startTime += start_offset + 1; /* +1 to satisfy the condition for recalculation */
		APP_LOG("DEVICE:rule", LOG_DEBUG, "After Updating Offset time: %d", psRule->startTime);
	}
	else if(start_time % UNITS_DIGIT_DET == 2)
	{
		/* Check whether Sunset timer and update new sunset time*/
		psRule->startTime = timezone_offset +(((set_hours * 60) +set_mins) * 60);
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Updated Sunset time: %d", psRule->startTime);

		psRule->startTime += start_offset + 2; /* +2 to satisfy the condition for recalculation */
		APP_LOG("DEVICE:rule", LOG_DEBUG, "After Updating Offset time: %d", psRule->startTime);
	}

        APP_LOG("DEVICE:rule", LOG_DEBUG, "Overnight flag set before calculating sunrise time is :%d", psRule->isOvernight);

        /*Resetting Overnight flag value, This to fix issue where 1.11 iOS was sending wrong start time in case of sunrise sunset rules*/
        psRule->isOvernight = (((psRule->startTime + psRule->ruleDuration) > ONE_DAY_SECONDS) && (psRule->ruleDuration != ONE_DAY_SECONDS))?1:0;

        APP_LOG("DEVICE:rule", LOG_DEBUG, "Overnight flag set is:%d", psRule->isOvernight);

	APP_LOG("Rules", LOG_DEBUG, "/* End time calculation */");
	if(end_time % UNITS_DIGIT_DET == 1)
	{
		if(psRule->isOvernight)
		{
			calculateSunRiseSetTimes(1, lat, lon, &rise_hours, &rise_mins, &set_hours, &set_mins);
			x = ONE_DAY_SECONDS;
		}
		end_time = timezone_offset +(((rise_hours * 60) +rise_mins) * 60); /* sunrise time */
		psRule->ruleDuration = end_time + x - psRule->startTime;
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Updated Sunrise time: %d, duration: %d", end_time, psRule->ruleDuration);

		psRule->ruleDuration += end_offset + 1; /* comply with duration based calculation */
		APP_LOG("DEVICE:rule", LOG_DEBUG, "After Updating Offset, end time: %d, duration: %d", (end_time+end_offset), psRule->ruleDuration);
	}
	else if(end_time % UNITS_DIGIT_DET == 2)
	{
		/* Check whether Sunset timer and update new sunset time*/
		if(psRule->isOvernight)
		{
			calculateSunRiseSetTimes(1, lat, lon, &rise_hours, &rise_mins, &set_hours, &set_mins);
			x = ONE_DAY_SECONDS;
		}
		end_time = timezone_offset +(((set_hours * 60) +set_mins) * 60);
		psRule->ruleDuration = end_time + x - psRule->startTime;
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Updated Sunset time: %d, duration: %d", end_time, psRule->ruleDuration);

		psRule->ruleDuration += end_offset + 2; /* comply with duration based calculation */
		psRule->endTime = end_time + end_offset + 2;
		APP_LOG("DEVICE:rule", LOG_DEBUG, "After Updating Offset, end time: %d, duration: %d", (end_time+end_offset), psRule->ruleDuration);
	}
	else 
	{
		APP_LOG("DEVICE:rule", LOG_DEBUG, "fixed end time");
		if(psRule->endTime != 0) /* New DB */
		{
			APP_LOG("DEVICE:rule", LOG_DEBUG, "/* New DB */");
			if(psRule->isOvernight)
			{
				x = ONE_DAY_SECONDS;
			}
			psRule->ruleDuration = psRule->endTime - psRule->startTime + x; /* in case of fixed end time */
		}
		else /* Old DB */
		{
			APP_LOG("DEVICE:rule", LOG_DEBUG, "/* Using Old DB */");
			
		}
	}
	
	APP_LOG("DEVICE:rule", LOG_DEBUG, "psRule->endTime: %d, psRule->ruleDuration: %d, psRule->startTime: %d, X: %d, end_time: %d",\
    		psRule->endTime, psRule->ruleDuration, psRule->startTime, x, end_time);
	APP_LOG("Rules", LOG_DEBUG, "*************UpdateSunriseSunset***************");

	return SUCCESS;
}

SRuleInfo* getNewRuleListNode()
{
	SRuleInfo *psRule=NULL;

	psRule = (SRuleInfo *)CALLOC(1, sizeof(SRuleInfo));
	if(!psRule)
	{
		APP_LOG("Rule", LOG_CRIT, "Rule node allocation failure");
		resetSystem();
	}

	lockRule();

        psRule->enablestatus = 1;

	if(gpsRuleList == NULL)
	{
		gpsRuleList = psRule;
		gpsRuleListTail = psRule;
	}
	else
	{
		gpsRuleListTail->psNext = psRule;
		gpsRuleListTail = psRule;
	}
	
	unlockRule();

	return psRule;	
}

int getRuleType(char *str, ERuleType *ruleType)
{
	int i=0;
	int retVal = FAILURE;

	for(i=e_SIMPLE_RULE; i<e_MAX_RULE; i++)
	{
		if(!strcmp(str, g_szRuleTypeStrings[i]))
		{
			*ruleType = i;
			retVal = SUCCESS;
			APP_LOG("Rule", LOG_DEBUG, "Rule type: %d for %s", i, g_szRuleTypeStrings[i]);
			break;
		}
	}

	return retVal;
		
}

/*
FW -  0: Mon 6: Sun
App - 1: SUN 7: SAT
*/
inline int FW_DAY_INDEX(int appDayIndex)
{
        if(appDayIndex == 1) //Sunday
                return 6;
        else
                return appDayIndex-2;
}

inline int APP_DAY_INDEX(int FwDayIndex)
{
        if(FwDayIndex == 6) //Sunday
                return 1;
        else
                return FwDayIndex+2;
}

inline int DAY_INDEX_MASK(int x)
{
        if(x<0)
                return (1<<6);
        else
                return (1<<(x));
}


int LoadRulesTable(char *pszruleId, char *pszruleType, char *pszDeviceId)
{
	char query[SIZE_256B] = {0,};
	char processDB = TRUE;
	ERuleType ruleType=0, j=0;
	SRuleInfo* psRule=NULL;
	int ruleDevicesArraySize=0, rowsRuleDevices=0, colsRuleDevices=0;
	char  **ppsRuleDevicesArray=NULL;
	int ret = -1;
    	unsigned char queryMod = FALSE;

	if(!pszDeviceId)
	{
		APP_LOG("DEVICE:rule", LOG_ERR, "Invalid UDN");
		return FAILURE;
	}

	memset(query, 0, sizeof(query));
	snprintf(query, sizeof(query), 
#ifndef PRODUCT_WeMo_LEDLight
			"SELECT DayID, StartTime, RuleDuration, StartAction, EndAction, EndTime FROM RULEDEVICES WHERE RuleID=\"%s\" AND DeviceId=\"%s\";",
#else
			"SELECT DISTINCT DayID, StartTime, RuleDuration, StartAction, EndAction, EndTime FROM RULEDEVICES WHERE RuleID=\"%s\" AND DeviceId LIKE \"%s%%\";",
#endif
			pszruleId, pszDeviceId);
	APP_LOG("DEVICE:rule", LOG_ERR, "query:%s", query);

	if(FAILURE == getRuleType(pszruleType, &ruleType))
	{
		APP_LOG("DEVICE:rule", LOG_ERR, "Get Rule type failed");
		return FAILURE;
	}
	
	ret = WeMoDBGetTableData(&g_RulesDB, query, &ppsRuleDevicesArray,&rowsRuleDevices,&colsRuleDevices);
	
	/* If 1st query fails */
	if (ret)
	{
	    APP_LOG("DEVICE:rule", LOG_ERR, "Get Table data failed with EndTime. Changing the query");
		
		/* modify query */
		memset(query, 0, sizeof(query));
		snprintf(query, sizeof(query), 
#ifndef PRODUCT_WeMo_LEDLight
				"SELECT DayID, StartTime, RuleDuration, StartAction, EndAction FROM RULEDEVICES WHERE RuleID=\"%s\" AND DeviceId=\"%s\";",
#else
				"SELECT DISTINCT DayID, StartTime, RuleDuration, StartAction, EndAction FROM RULEDEVICES WHERE RuleID=\"%s\" AND DeviceId LIKE \"%s%%\";",
#endif
				pszruleId, pszDeviceId);
		APP_LOG("DEVICE:rule", LOG_ERR, "query:%s", query);
        	queryMod = TRUE;
		
		if (WeMoDBGetTableData (&g_RulesDB, query, &ppsRuleDevicesArray,&rowsRuleDevices,&colsRuleDevices)) {
		    processDB = FALSE;
			APP_LOG ("DEVICE:rule", LOG_ERR, "Get Table data failed without EndTime");
		}
	}
	
	if (TRUE == processDB)
    {
		if(rowsRuleDevices && colsRuleDevices)	
		{
			psRule = getNewRuleListNode();

			if(psRule)
			{
				j=colsRuleDevices;
				psRule->ruleType = ruleType;
				psRule->ruleId = atoi(pszruleId);
				psRule->startTime = atoi(ppsRuleDevicesArray[++j]);
				psRule->ruleDuration = atoi(ppsRuleDevicesArray[++j]);
#ifdef SIMULATED_OCCUPANCY
				if(ruleType == e_AWAY_RULE)
				{
				    /* init is required here because this is place where we get Away mode rule type for first time */
				    simulatedOccupancyInit();
				}
#endif
				psRule->isOvernight = (((psRule->startTime + psRule->ruleDuration) > ONE_DAY_SECONDS) && (psRule->ruleDuration != ONE_DAY_SECONDS))?1:0;


				psRule->startAction = atoi(ppsRuleDevicesArray[++j]);
				psRule->endAction = atoi(ppsRuleDevicesArray[++j]);
				if (TRUE == queryMod)
				{
					/* Initializing the endTime to 0 when EndTime is not read
					 * from the DB since further calculation depends this.
					 */
					psRule->endTime = 0;
				}
				else
				{
					psRule->endTime = atoi(ppsRuleDevicesArray[++j]);
				}
				
				psRule->isSunriseSunset = ((psRule->startTime % UNITS_DIGIT_DET == 1) || (psRule->startTime % UNITS_DIGIT_DET == 2) \
						|| (psRule->ruleDuration % UNITS_DIGIT_DET == 1)|| (psRule->ruleDuration % UNITS_DIGIT_DET == 2) \
						|| (psRule->endTime % UNITS_DIGIT_DET == 1)|| (psRule->endTime % UNITS_DIGIT_DET == 2));

				ruleDevicesArraySize = (rowsRuleDevices+1) * colsRuleDevices;
				for(j=colsRuleDevices; j < ruleDevicesArraySize;j+=colsRuleDevices)
				{
					psRule->activeDays |= (1<<FW_DAY_INDEX((atoi(ppsRuleDevicesArray[j]))));
				}

				APP_LOG("DEVICE:rule", LOG_DEBUG,\
						"RULE id: %d, type: %d, start: %d, dur: %d, stAct: %d, endAct: %d, night: %d, sun: %d, active:%02X, endTime : %d",\
						psRule->ruleId,  psRule->ruleType, psRule->startTime, psRule->ruleDuration,\
						psRule->startAction, psRule->endAction, psRule->isOvernight, psRule->isSunriseSunset, psRule->activeDays,psRule->endTime);
			}
		}

		WeMoDBTableFreeResult(&ppsRuleDevicesArray,&rowsRuleDevices,&colsRuleDevices);
	}

	
	return SUCCESS;
}

int FetchTargetDeviceId(char *psRuleId, char *psDeviceId)
{
	int rowsRules=0,colsRules=0, rulesArraySize=0;
	char **ppsRulesArray=NULL;
	char query[SIZE_256B];

	memset(query, 0, sizeof(query));
	snprintf(query, sizeof(query), "SELECT DeviceID FROM devicecombination WHERE SensorID=\"%s\" AND RuleID=\"%s\" limit 1;", g_szUDN_1, psRuleId);

	if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
	{
		if(rowsRules && colsRules)
		{
			rulesArraySize = (rowsRules + 1)*colsRules;
		}
		else
		{
			APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
			WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
			return FAILURE;
		}

		APP_LOG("DEVICE:rule", LOG_DEBUG, "Device Id found: %s", ppsRulesArray[1]);
		strncpy(psDeviceId, ppsRulesArray[1], SIZE_256B);  // <-----<<< DANGEROUS
		
		WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
	}

	return SUCCESS;
}

#ifndef PRODUCT_WeMo_LEDLight
void addNewTimerInSortedTimerList(int time, int action)
#else
void addNewTimerInSortedTimerList(int time, int action, int id)
#endif
{
	STimerList *psTimerList = NULL;
	STimerList *psNewTimerNode = NULL;

	lockTimerRule();
	psNewTimerNode = (STimerList*)CALLOC(1, sizeof(STimerList));
	if(!psNewTimerNode)
	{
		APP_LOG("Rule", LOG_CRIT, "Memory allocation failure");
		resetSystem();
	}

	psNewTimerNode->time = time;
	psNewTimerNode->action = action;
#ifdef PRODUCT_WeMo_LEDLight	
	psNewTimerNode->ruleIdTimer = id;
#endif	
	psNewTimerNode->nextTimer = NULL;

	psTimerList = gpsTimerList;
	/*add first node*/
	if(NULL == psTimerList)
	{
		gpsTimerList = psNewTimerNode;
		goto RETURN;
	}
	/*add at the begining*/
	else if(time < psTimerList->time)
	{
		psNewTimerNode->nextTimer = psTimerList;
		gpsTimerList = psNewTimerNode;
		goto RETURN;
	}
	/*add in between nodes*/
	else
	{
		while(NULL != psTimerList->nextTimer)
		{
			if((time >= psTimerList->time) && (time < psTimerList->nextTimer->time))
			{
				psNewTimerNode->nextTimer = psTimerList->nextTimer;
				psTimerList->nextTimer = psNewTimerNode;
				goto RETURN;
			}
			psTimerList = psTimerList->nextTimer;
		}
	}
	/*add at the end*/
	psTimerList->nextTimer = psNewTimerNode;

RETURN:
	unlockTimerRule();
	return;
}

int createTimerRuleList(int dayIndex)
{
	SRuleInfo *psRuleInfo = NULL;
	STimerList *psTimerList = NULL;
	int time = 0, action = 0;

	/*Free all timer rule memory*/
	freeTimerList();

	/*initialise ruleinfo pointers with global pointer*/
	psRuleInfo = gpsRuleList;

	/*iterate till last node and it is timer rule type*/
	while(psRuleInfo)
	{
		/*check if it is timer rule*/
		if(e_TIMER_RULE == psRuleInfo->ruleType)
		{
			/*check if overnight rule present*/
			if(psRuleInfo->isOvernight)
			{
				APP_LOG("DEVICE:rule", LOG_DEBUG, "overnight timer rule");
				/*check if overnight rule coming for previous day*/
				if(psRuleInfo->activeDays & DAY_INDEX_MASK(dayIndex-1))
				{
					/*add only end time and action in timer list*/
					time = (psRuleInfo->startTime + psRuleInfo->ruleDuration) - ONE_DAY_SECONDS;
					action = psRuleInfo->endAction;
#ifndef PRODUCT_WeMo_LEDLight
					APP_LOG("DEVICE:rule", LOG_DEBUG, "overnight: time:%d, action:%d", time,action);
					addNewTimerInSortedTimerList(time, action);
#else
					APP_LOG("DEVICE:rule", LOG_DEBUG, "overnight: time:%d, action:%d", time,action);
					addNewTimerInSortedTimerList(time, action+LED_OVERNIGHT_ACTION, psRuleInfo->ruleId);
#endif
				}
			}
			
			/*if rule present in active day*/
			if(psRuleInfo->activeDays & DAY_INDEX_MASK(dayIndex))
			{
				/*add start time and action in timer list*/
				time = psRuleInfo->startTime;
				action = psRuleInfo->startAction;
#ifndef PRODUCT_WeMo_LEDLight
				APP_LOG("DEVICE:rule", LOG_DEBUG, "start: time:%d, action:%d", time,action);
				addNewTimerInSortedTimerList(time, action);
#else
				APP_LOG("DEVICE:rule", LOG_DEBUG, "start: time:%d, action:%d", time,action+LED_START_CAPABILITIES);
				addNewTimerInSortedTimerList(time, action+LED_START_CAPABILITIES,psRuleInfo->ruleId);
#endif //PRODUCT_WeMo_LEDLight


				if (psRuleInfo->endTime) {
				
					if (psRuleInfo->startTime <= psRuleInfo->endTime) {						
						psRuleInfo->ruleDuration =  psRuleInfo->endTime - psRuleInfo->startTime;
						APP_LOG("DEVICE:rule", LOG_DEBUG, "startTime %d endTime %d ruleDuration %d", 
						psRuleInfo->startTime,psRuleInfo->endTime, psRuleInfo->ruleDuration);
					}
					else {
						psRuleInfo->ruleDuration =  ONE_DAY_SECONDS - (psRuleInfo->startTime - psRuleInfo->endTime);
						APP_LOG("DEVICE:rule", LOG_DEBUG, "startTime %d endTime %d ruleDuration %d", 
						psRuleInfo->startTime,psRuleInfo->endTime, psRuleInfo->ruleDuration);
					}
					
				}

				/*add end time and action in timer list*/
				time = psRuleInfo->startTime + psRuleInfo->ruleDuration;
				action = psRuleInfo->endAction;
#ifndef PRODUCT_WeMo_LEDLight
				APP_LOG("DEVICE:rule", LOG_DEBUG, "end: time:%d, action:%d", time,action);
				addNewTimerInSortedTimerList(time, action);
#else
				APP_LOG("DEVICE:rule", LOG_DEBUG, "end: time:%d, action:%d", time,action+LED_END_CAPABILITIES);
				addNewTimerInSortedTimerList(time, action+LED_END_CAPABILITIES,psRuleInfo->ruleId);
#endif //PRODUCT_WeMo_LEDLight
			}
		}
		/*Move to next rule node*/
		psRuleInfo = psRuleInfo->psNext;
	}

	/*printf gpsTimerList*/
	psTimerList = gpsTimerList;
	while(psTimerList)
	{
		APP_LOG("DEVICE:rule", LOG_DEBUG, "TimerList:  time:%d, action:%d", psTimerList->time,psTimerList->action);
		psTimerList = psTimerList->nextTimer;
	}

	return SUCCESS;
}

int LoadRulesInMemory(void)
{
	int rowsRules=0,colsRules=0;
	int rulesArraySize=0;
	char **ppsRulesArray=NULL;
	char deviceId[SIZE_256B];
	int i=0;
	char query[SIZE_256B];
	char *UDN=NULL;

	memset(query, 0, sizeof(query));
	snprintf(query, sizeof(query), "SELECT Type, RuleID FROM RULES WHERE STATE=\"1\";");
	APP_LOG("DEVICE:rule", LOG_DEBUG, "query:%s", query);

	/*Read RULES table to get all rules in the DB*/
	if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
	{
		if(rowsRules && colsRules)
		{
			rulesArraySize = (rowsRules + 1)*colsRules;
		}
		else
		{
			APP_LOG("DEVICE:rule", LOG_DEBUG, "Empty RULES table");
			WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
			return FAILURE;
		}

		/*Start loading Active days rules*/
		for(i=colsRules; i<rulesArraySize; i+=colsRules)
		{
			UDN = NULL;

			/*If device type is sensor*/
    			if (DEVICE_SENSOR == g_eDeviceType)
			{
				/*if rule type is notification*/
				if(!strcmp(g_szRuleTypeStrings[e_NOTIFICATION_RULE], ppsRulesArray[i]))
				{
					UDN = g_szUDN_1;
				}
			}
			/*If device type is not sensor*/
			else
			{
					UDN = g_szUDN_1;
			}

			LoadRulesTable(ppsRulesArray[i+1], ppsRulesArray[i], UDN);
		}

		/*free memory*/
		WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
	}
	else 
	{
		APP_LOG("DEVICE:rule", LOG_ERR, "Get Table data failed");
		return FAILURE;
	}

	return SUCCESS;
}

int prepareLinkDeviceList(int ruleId, char ***devList, int *num)
{
	char query[SIZE_256B];
	int rowsRules=0,colsRules=0;
	char **ppsRulesArray=NULL;
	char **deviceIds = NULL;
	int numDevices = 0;
	int ret=0;
        int i=1;

	/* Find out how many end devices are part of this rule */
	memset(query, 0, sizeof(query));
	snprintf(query, sizeof(query), "SELECT DISTINCT DeviceID FROM RULEDEVICES where RuleID=\"%d\";", ruleId);
	APP_LOG("DEVICE:rule", LOG_DEBUG, "query:%s", query);

	/*execute database query*/
	if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
	{
		/*check if we got the data*/
		if(rowsRules && colsRules)
		{
			numDevices = rowsRules;

			/* allocate size for device Id array */
    			deviceIds =(char **) malloc(sizeof(char *)* numDevices);
		
			if(!deviceIds)
			{
				APP_LOG("DEVICE:rule", LOG_ERR, "Out of memory for %d pointers", numDevices);
				ret = FAILURE;
				goto exit_fn;
			}
			else	
				APP_LOG("DEVICE:rule", LOG_ERR, "Allocated memory for %d pointers, deviceIds: %p", numDevices, deviceIds);

			/* save the device ids */
			while(i <= rowsRules)
			{
				deviceIds[i-1]  = calloc(1, SIZE_256B);
				if(! deviceIds[i-1])
				{
					APP_LOG("DEVICE:rule", LOG_ERR, "Out of memory");
					ret = FAILURE;
					goto exit_fn;

				}
				else
				{
					strncpy(deviceIds[i-1], ppsRulesArray[i], SIZE_256B-1);
					APP_LOG("DEVICE:rule", LOG_DEBUG, "Saved device%d as %s at %p", i-1, deviceIds[i-1], deviceIds[i-1]);
					i++;
				}
			}
		}
		else
		{
			APP_LOG("DEVICE:rule", LOG_ERR, "No devices found");
			ret = FAILURE;
			goto exit_fn;
		}
	}
	else
	{
		APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
		return FAILURE;
	}

exit_fn:
	/*free database buffer*/
	if(ppsRulesArray)
	{
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Freeing DB table");
		WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
		ppsRulesArray = NULL;
	}

	if(ret)
	{
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Freeing device list");
		for(i=0;i<numDevices;i++)
			if(deviceIds[i])
				free(deviceIds[i]);

		if(deviceIds)
			free(deviceIds);

		return FAILURE;
	}

	*num = numDevices;
	*devList = deviceIds;

	
	APP_LOG("DEVICE:rule", LOG_DEBUG, "num: %p, devList: %p, deviceIds: %p ", num, devList, deviceIds);
	return SUCCESS;
}

int checkAndExecuteSRule(int aDayIndex, int aNowTime)
{
	SRuleInfo* psRule = gpsRuleList;
	int sleepTime=0;

	while(psRule != NULL)
	{
	    //APP_LOG("DEVICE:rule", LOG_ERR, "rule- type:%d|startTime:%d|aNowTime:%d|isActive:%d", psRule->ruleType, psRule->startTime,
		//    aNowTime, psRule->isActive);
		if((e_SIMPLE_RULE == psRule->ruleType) && (aNowTime >= psRule->startTime) && (aNowTime  < (psRule->startTime + RULE_TASK_FREQUENCY)) && 
				(psRule->activeDays & DAY_INDEX_MASK(aDayIndex))) 
		{
			sleepTime = psRule->startTime - aNowTime;
			
			if(sleepTime > 0)
				sleep(sleepTime);
#if !defined(PRODUCT_WeMo_Insight) && !defined(PRODUCT_WeMo_LEDLight)
            /* Do this only for products other than Insight, since Insight has
             * separate functionality to invoke the countdown timer
             * functionality when the state changes from STDBY to ON.
             */
            /* Check if Countdown timer rule is active and not in its last
             * minute. If so, start the Countdown timer thread.
             */

            APP_LOG ("DEVICE:rule", LOG_DEBUG,
                     "*** gCountdownRuleInLastMinute *** = [%d]"
                     "*** ruleCnt *** = [%d]",
                     gCountdownRuleInLastMinute,
                     gRuleHandle[e_COUNTDOWN_RULE].ruleCnt);

            if (gRuleHandle[e_COUNTDOWN_RULE].ruleCnt &&
               (1 == gCountdownRuleInLastMinute))
            {
                APP_LOG ("DEVICE:rule", LOG_DEBUG,
                         "Countdown timer was in last minute, Do not toggle!");
            }
            else
            {
                /* Check the current state of the device. If the current state
                 * is the same as the requested state then do not reset the
                 * Countdown Timer.
                 */
                int deviceCurrState = -1;
                deviceCurrState = GetCurBinaryState();

                APP_LOG ("DEVICE:rule", LOG_DEBUG, "deviceCurrState = [%d]",
                         deviceCurrState);

                if (deviceCurrState != psRule->startAction)
                    checkAndExecuteCountdownTimer (psRule->startAction);
            }
#endif

#ifndef PRODUCT_WeMo_LEDLight
            /* Perform timer action */
            SetRuleAction (psRule->startAction, ACTUATION_TIME_RULE);
#else 
	    RunLinkSimpleTimerRule(aDayIndex, psRule->ruleId, ACTUATION_TIME_RULE, 0);
#endif
            APP_LOG ("DEVICE:rule", LOG_DEBUG, "Executed Timer rule "
                     "action:%d after %d secs", psRule->startAction, sleepTime);

			break;
		}
		psRule = psRule->psNext;
	}

	return sleepTime;
}

void startControlPointForSensorRule()
{
	if(isSensorRuleForZBActive("Bridge", NULL, NULL))
		gSubscribeBridgeService = 1;
	if (INVALID_THREAD_HANDLE != ctrlpt_handle)
	{
		APP_LOG("UPNP: Rule", LOG_DEBUG,"Start CtrlPointDiscoverDevices for sensor rule");
		CtrlPointDiscoverDevices();	
		return;	
	}
	initControlPoint();
}

void startExecutorThread(ERuleType i)
{
	if((i != e_SIMPLE_RULE) && (i != e_SENSOR_RULE) && (i != e_NOTIFICATION_RULE) && (i != e_COUNTDOWN_RULE) \
	    && (gRuleHandle[i].ruleThreadId == INVALID_THREAD_HANDLE))
	{
		int ret = ithread_create(&gRuleHandle[i].ruleThreadId, 0x00, gfpRuleThreadFn[i], 0x00);
		if (0x00 != ret)
		{
			APP_LOG("UPNP: Rule", LOG_DEBUG, "Could not create rule thread: %d", i);
			resetSystem();
		}
		else
			APP_LOG("UPNP: Rule", LOG_DEBUG, "Rule task : %d created", i);
	}

	if(i==e_SENSOR_RULE)
	{
	    APP_LOG("UPNP: Rule", LOG_DEBUG,"enable sensor control point inside sensor rule");
	    initControlPoint(); 
	}
}

void stopExecutorThread(ERuleType i)
{

	if((i != e_SIMPLE_RULE) && (i != e_SENSOR_RULE) && (i != e_NOTIFICATION_RULE) && (i != e_COUNTDOWN_RULE) \
				&& (gRuleHandle[i].ruleThreadId != INVALID_THREAD_HANDLE))
	{
		int ret = ithread_cancel(gRuleHandle[i].ruleThreadId);

		if (0x00 != ret)
		{
			APP_LOG("UPNP: Rule", LOG_DEBUG, "################### ithread_cancel: Couldnt stop rule thread %d #########################", i);
		}
		else
		{
			APP_LOG("UPNP: Rule", LOG_DEBUG, "################### ithread_cancel: Successfully stop rule thread %d ####################", i);
		}
	}

	/* Invalidate the handle for all threads so that stopEngine doesnt invoke cancel for simple/sensor thread */
	gRuleHandle[i].ruleThreadId = INVALID_THREAD_HANDLE;

	if((i == e_SENSOR_RULE) && (0 == (gRuleHandle[e_SENSOR_RULE].ruleCnt)))
	{
	    if(!gProcessData)
	    {
		APP_LOG("UPNP: Rule", LOG_DEBUG,"stop control point in maker sensor rule");
		StopPluginCtrlPoint();
	    }
	}

	if(i == e_AWAY_RULE)
	{
#ifndef PRODUCT_WeMo_LEDLight
#ifdef SIMULATED_OCCUPANCY
	    cleanupAwayRule(0);
#endif
	    if(gRuleHandle[e_TIMER_RULE].ruleCnt) 
		startExecutorThread(e_TIMER_RULE);
#else
		APP_LOG("UPNP: Rule", LOG_DEBUG, "################### BA_FreeRule i = %d ####################", i);
		BA_FreeRule();
		gRuleHandle[e_AWAY_RULE].ruleCnt = 0;
#endif
	}
}

int isRuleIdApplicableForZb(int ruleNo, char *productName,int capability, int value)
{
	/*For Wemo Motion Always Return 1*/
	if(!strcmp(productName,MOTIONSENSOR))
		return 1;
	int rowsRules=0,colsRules=0;
	char **ppsRulesArray=NULL;
	int ret=0;
	int level=-1;
	char query[SIZE_256B];
	memset(query, 0, sizeof(query));
#ifndef PRODUCT_WeMo_LEDLight
	char dummyArr[SIZE_100B];
        memset(dummyArr,0x00,sizeof(dummyArr));
#endif
	if(strstr(productName,MAKERSENSOR))
		snprintf(query, sizeof(query), "SELECT DISTINCT Value FROM RULEDEVICES where RuleID=\"%d\" AND DeviceID=\"%s\";", ruleNo,productName);
	else
		snprintf(query, sizeof(query), "SELECT DISTINCT Level FROM RULEDEVICES where RuleID=\"%d\" AND productName=\"%s\";", ruleNo,productName);
	APP_LOG("DEVICE:rule", LOG_DEBUG, "query:%s", query);
	/*execute database query*/
	if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
	{
		/*check if we got the data*/
		if(rowsRules && colsRules)
		{
			level=atoi(ppsRulesArray[1]);
			if(!strcmp(productName,SENSORFOB))
			{
				if(capability == FOBKEYPRESS && level == FOB_BUTTON_PRESS)
				{
#ifndef PRODUCT_WeMo_LEDLight
					changeDeviceState(!g_PowerStatus,-1,-1,dummyArr,dummyArr);
					ret=0;
#else
					ret=1;
#endif
				}
				else if(capability == SENSORCAPABILITY && (level == value || level == FOB_ARRIVES_OR_LEAVES))
					ret=1;
				else if(capability == SENSORCAPABILITY && (value != FOB_LEAVES && level == FOB_ARRIVES))
					ret=1;
				goto exit_fn;
			}
			if(!strcmp(productName,SENSORPIR) && (value == MOTION))
			{
				ret=1;
				goto exit_fn;
			}
			if(!strcmp(productName,SENSORDW))
			{
				if((value == level) || (level == OPENS_OR_CLOSES))	
					ret =1;
				goto exit_fn;
			}
			if(strstr(productName,MAKERSENSOR) && (value == level))
			{
				ret=1;
				goto exit_fn;
			}

		}
		else
		{
			APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
			/*free database buffer*/
			WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
		}
	}
	else  
	{
		APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
		goto exit_fn;
	}
exit_fn:
	/*free database buffer*/
	if(ppsRulesArray)
	{
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Freeing DB table");
		WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
		ppsRulesArray = NULL;
	}
	return ret;
}

/*
*This function is to check if there is any configured ZB sensor rule to trigger this device. 
*Return values:
*If sensorID parameter is NULL, it returns all configured sensor trigger rules.
*If sensorID parameter is ZB sensor ID, it returns ZB sensor trigger rules for given sensor ID.
*If sensorID parameter doesn't contain ":" pattern, it returns all configured  ZB sensor trigger rules.
*/
int isSensorRuleForZBActive(const char* sensorID, int **ruleID, int *count)
{
	int rowsRules=0,colsRules=0;
	char **ppsRulesArray=NULL;
	int ret=0;
	unsigned int *ruleIds =NULL;
	int i=0;
#ifndef PRODUCT_WeMo_LEDLight
	int pattern=1;
#endif
	char query[SIZE_256B];
	if(gRuleHandle[e_SENSOR_RULE].ruleCnt || gRuleHandle[e_MAKERSENSOR_RULE].ruleCnt)
	{
		/*get rule IDs*/
		memset(query, 0, sizeof(query));
#ifdef PRODUCT_WeMo_LEDLight
                snprintf(query, sizeof(query), "SELECT DISTINCT RuleID FROM DEVICECOMBINATION where SensorID=\"%s\" AND DeviceID LIKE \"%s%%\";", sensorID,g_szUDN_1);
#else
		if(strchr(sensorID, ':'))
			pattern=0;
		if(sensorID !=NULL)
		{
			if(pattern)
				snprintf(query, sizeof(query), "SELECT DISTINCT RuleID FROM DEVICECOMBINATION where SensorID LIKE \"%%%s%%\" AND DeviceID=\"%s\";", sensorID,g_szUDN_1);
			else
				snprintf(query, sizeof(query), "SELECT DISTINCT RuleID FROM DEVICECOMBINATION where SensorID=\"%s\" AND DeviceID=\"%s\";", sensorID,g_szUDN_1);
		}
		else
			snprintf(query, sizeof(query), "SELECT DISTINCT RuleID FROM DEVICECOMBINATION where DeviceID=\"%s\";", g_szUDN_1);
#endif
		APP_LOG("DEVICE:rule", LOG_DEBUG, "query:%s", query);

		/*execute database query*/
		if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
		{
			/*check if we got the data*/
			if(rowsRules && colsRules)
			{
				ret =1;

				if(ruleID)
				{
					ruleIds =(unsigned int *) MALLOC(sizeof(unsigned int)* rowsRules);
					if(!ruleIds)
					{
						APP_LOG("DEVICE:rule", LOG_ERR, "Out of memory for %d pointers", rowsRules);
						goto exit_fn;
					}
					while(i < rowsRules)
					{
						ruleIds[i]=atoi(ppsRulesArray[i+1]);
						APP_LOG("DEVICE:rule", LOG_DEBUG, "ruleIds:%d",ruleIds[i]);
						i++;
					}
					*ruleID=ruleIds;
				}

				if(count)
					*count = rowsRules;	
			}
			else
			{
				APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
				/*free database buffer*/
				WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
			}
		}
		else  
		{
			APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
			goto exit_fn;
		}
	}
exit_fn:
	/*free database buffer*/
	if(ppsRulesArray)
	{
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Freeing DB table");
		WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
		ppsRulesArray = NULL;
	}

	return ret;
}



int updateRuleActiveStatus(int aDayIndex, int aNowSeconds)
{
	int delta=-1;
	SRuleInfo* psRule=NULL;
	int ret=0;

	lockRule();
	psRule = gpsRuleList;
	

	while(psRule != NULL)
	{
		delta = -1;

		if(psRule->isDayChange && (psRule->activeDays & DAY_INDEX_MASK(aDayIndex-1)))
                {
		        APP_LOG("DEVICE:rule", LOG_DEBUG, "Day is changed");
                        psRule->enablestatus = 1;
			delta = ONE_DAY_SECONDS;

                }
		else if(!psRule->isDayChange && (psRule->activeDays & DAY_INDEX_MASK(aDayIndex)))
			delta = 0;

		if((delta == -1)||(!psRule->enablestatus))
		{
			psRule->isActive = 0;
			psRule->isDayChange = 0;
		}
		else
		{
			/*
			    This if condition handles the end timer. One second delay helps in marking the status as inactive
			    in this iteration itself avoiding delay of 10 seconds. It also allows the individual rules tasks
			    like Timer Task to exit gracefully
			*/
			if(!ret && (e_SIMPLE_RULE != psRule->ruleType) && ((aNowSeconds + delta) == (psRule->startTime + psRule->ruleDuration)))
			{
				sleep(1);
				aNowSeconds++;
				ret=1;
			}

			/* start time of sunrise/sunset rule has 1/2 respectively. 
			   It is okay to delay marking the rule as inactive as timer task will do
			   its job at the right time 
			*/

			if(( (aNowSeconds + delta) >= (psRule->startTime - (psRule->startTime%10)))
				&& ((aNowSeconds + delta) <= (psRule->startTime + psRule->ruleDuration)) ) 
			{
				if(!psRule->isActive)
				{
					psRule->isActive = 1;
                                        psRule->enablestatus = 1;

					/*update rule count corresponding to its rule type*/
					gRuleHandle[psRule->ruleType].ruleCnt++;
					if(gRuleHandle[psRule->ruleType].ruleCnt == 1)
					{
						startExecutorThread(psRule->ruleType);
						if((g_eDeviceType != DEVICE_SENSOR) && ((psRule->ruleType == e_SENSOR_RULE) || (psRule->ruleType == e_MAKERSENSOR_RULE)))
							startControlPointForSensorRule();
					}
					APP_LOG("DEVICE:rule", LOG_DEBUG, "Ruleid: %d going active", psRule->ruleId);
#ifdef PRODUCT_WeMo_LEDLight
                                        if (e_NOTIFICATION_RULE == psRule->ruleType)
                                        {
					    enableRuleNotification(psRule->ruleId);
                                        }
#endif

				}
			}
			else
			{
				if(psRule->isActive)
				{
					psRule->isActive = 0;
					if(psRule->isOvernight)
					{
						APP_LOG("DEVICE:rule", LOG_DEBUG, "Ruleid: %d dayChange zero", psRule->ruleId);
						psRule->isDayChange = 0;
					}

					/*update rule count corresponding to its rule type*/
					if(gRuleHandle[psRule->ruleType].ruleCnt > 0)
						gRuleHandle[psRule->ruleType].ruleCnt--;

					if(gRuleHandle[psRule->ruleType].ruleCnt == 0)
					{
						stopExecutorThread(psRule->ruleType);
					}
			
					APP_LOG("DEVICE:rule", LOG_DEBUG, "Ruleid: %d going inactive", psRule->ruleId);
				}
			}
		}

		psRule = psRule->psNext;
	}
	unlockRule();

	return ret;
}

void updateOnActiveDayChange()
{
	SRuleInfo* psRule=NULL;

	lockRule();
	psRule = gpsRuleList;
	while(psRule != NULL)
	{
		/* update start time and duration for sunrise sunset rules */
		if(psRule->isSunriseSunset)
		{
			APP_LOG("DEVICE:rule", LOG_DEBUG, "Update sunrise sunset time for the day");
			UpdateSunriseSunset(psRule);
		}

		if(psRule->isOvernight)
		{
			psRule->isDayChange=TRUE;
		}	

		psRule = psRule->psNext;
	}
	unlockRule();
}

void *RulesTask(void *args)
{
	int nowSeconds=0, last_seconds=0, year=0, monthIndex=0, dayIndex=0;
	SRuleInfo* psRule=NULL;
	int sleepTime=0;
	int delay=0;

	APP_LOG("DEVICE:rule", LOG_DEBUG, "starting rules task");

	/* 
	 ** Load all rules from DB in to data structure and create the linked list of RuleInfo nodes
	 ** Take care to load only supported rules in memory - for eg. sensor device should load just
	 ** motion sensor and APNS rules in the list based on rule type check
	 */

	while(1)
	{
		if(gRestartRuleEngine == RULE_ENGINE_RELOAD)
		{
			/*Kill all running executor threads*/
			StopRuleEngine();

			/*get rule DB handle*/
			if(GetRuleDBHandle())
			{
				APP_LOG("DEVICE:rule", LOG_CRIT, "InitDB failed...");
				gRestartRuleEngine = RULE_ENGINE_DEFAULT;
				continue;
			}

			/* upload all available rules db (it should be only one db anyways)to webserver creating a new detached thread*/
			if (dbFile_thread == -1)
  			{
					char *pUploadEnable = GetBelkinParameter(DB_UPLOAD_ENABLE);
                			if((NULL != pUploadEnable) && (((0x00 != strlen(pUploadEnable)) && (atoi(pUploadEnable) == 1)) ))
                			{
						int status = system("if ls /tmp/rules_*.db 1> /dev/null 2>&1; then return 1; else return 0; fi ");
						status = WEXITSTATUS(status);
						if(status == 1)
						{
							pthread_attr_t db_attr;
			  				pthread_attr_init(&db_attr);
	        				        pthread_attr_setdetachstate(&db_attr, PTHREAD_CREATE_DETACHED);
        	        				ithread_create(&dbFile_thread, &db_attr, uploadDbFileThread, NULL);
						}
					}
					else
					{
						/* delete  rules_*.db file if DB_UPLOAD_ENABLE is 0 */
						system("rm -f /tmp/rules_*.db");
						APP_LOG("DEVICE:rule", LOG_DEBUG, "Upload Ruled DB Disabled");
					}
   			}
			else
			{
	                        APP_LOG("DEVICE:rule", LOG_DEBUG, "uploadDbFileThread is already running.");
			}		
			/*Load all active day rules*/
			LoadRulesInMemory();
			APP_LOG("DEVICE:rule", LOG_DEBUG, "RELOADED RULES DONE");

			nowSeconds = daySeconds();

			/* update start time and duration for sunrise sunset rules */
			psRule = gpsRuleList;
			while(psRule != NULL)
			{
				/* update start time and duration for sunrise sunset rules */
				if(psRule->isSunriseSunset)
				{
					APP_LOG("DEVICE:rule", LOG_DEBUG, "Update sunrise sunset time for the day");
					UpdateSunriseSunset(psRule);
				}

				/* take care of overnight timer rule */
				if(psRule->isOvernight)
				{ 
					int end = psRule->startTime + psRule->ruleDuration - ONE_DAY_SECONDS;

					if((nowSeconds < psRule->startTime) && (nowSeconds <= end))
					{
						APP_LOG("DEVICE:rule", LOG_DEBUG, "Set dayChange, ruleid: %d, start: %d, end: %d, now: %d", 
							psRule->ruleId,  psRule->startTime, end, nowSeconds);

						psRule->isDayChange=1;
					}
				}
				psRule = psRule->psNext;
			}

			gRestartRuleEngine = RULE_ENGINE_RUNNING;

			/* adjust scheduler close to 10 second boundary every time */
			nowSeconds = daySeconds();
			/*
			Below code is compiled off to avoid additional delay in rule trigger.
			*/
			#if 0
			if(nowSeconds%10)
			{
				APP_LOG("DEVICE:rule", LOG_DEBUG, "Adjusting Rule boundary: %d", nowSeconds);
				sleep(10-(nowSeconds%10));
			}
			#endif
			APP_LOG("DEVICE:rule", LOG_DEBUG, "Rule engine running now");
		}

		if(gRestartRuleEngine == RULE_ENGINE_RUNNING)
		{
			last_seconds = nowSeconds;

			GetCalendarDayInfo(&dayIndex, &monthIndex, &year, &nowSeconds);

			if(last_seconds > nowSeconds)
			{
				APP_LOG("DEVICE:rule", LOG_DEBUG, "Update sunset/sunrise values and overnight flag on active day change...");
				updateOnActiveDayChange();
			}

			delay = updateRuleActiveStatus(dayIndex, nowSeconds);

			/*Execute Simple timer*/
#ifdef SIMULATED_OCCUPANCY
			if((NULL == gpSimulatedDevice) || (0 == gpSimulatedDevice->ruleEndTime))
#endif
			    sleepTime  = checkAndExecuteSRule(dayIndex, nowSeconds);

		}

		nowSeconds = daySeconds();
		//sleep(RULE_TASK_FREQUENCY - sleepTime - delay);
		//APP_LOG("DEVICE:rule", LOG_DEBUG, "Adjusting Rule boundary: %d", nowSeconds);
		sleep(RULE_TASK_FREQUENCY-(nowSeconds%10));

	}
	return NULL;
}

void ClearRuleFromFlash(void)
{
	char buf[SIZE_64B];

	StopRuleEngine();
#ifndef PRODUCT_WeMo_LEDLight
	/*stop Countdown Timer*/
	stopCountdownTimer();
#endif
	memset (buf,0,SIZE_64B);
	sprintf(buf,"rm -f %s",RULE_DB_PATH);
	System(buf);
        pluginUsleep(500000);
	APP_LOG("Rule", LOG_DEBUG, "Remove rule db");

	SaveDeviceConfig(RULE_DB_VERSION_KEY, "");
}

void moveToTimerNodeToExecute(unsigned int nowSeconds)
{
	STimerList *psTimerList = NULL;

	if(!gpsTimerList)
	{
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Timer List Empty!");
		return;
	}

	lockTimerRule();
	psTimerList = gpsTimerList;
	/*first node*/
	if((nowSeconds <= psTimerList->time))
	{
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Node Set: nowSeconds:%d, node time:%d, node action:%d", nowSeconds,psTimerList->time,psTimerList->action);
		goto RETURN;
	}
	else
	{
		while(NULL != psTimerList)
		{
			APP_LOG("DEVICE:rule", LOG_DEBUG, "Timer Node: nowSeconds:%d, node time:%d, node action:%d", nowSeconds,psTimerList->time,psTimerList->action);
			/* take care of entries missed in the last iteration */	
			if(nowSeconds > psTimerList->time + RULE_TASK_FREQUENCY)
			{
					APP_LOG("DEVICE:rule", LOG_DEBUG, "Move to next Timer node");
					/*move to next timer*/
					gpsTimerList = gpsTimerList->nextTimer;
					APP_LOG("DEVICE:rule", LOG_DEBUG, "Node Set: nowSeconds:%d, node time:%d, node action:%d", nowSeconds,psTimerList->time,psTimerList->action);
					free(psTimerList);
					psTimerList = gpsTimerList;
			}
			else
				goto RETURN;
		}

	}

RETURN:
	unlockTimerRule();
	return;
}

void *TimerTask(void *args)
{
	APP_LOG("DEVICE:rule", LOG_DEBUG, "In Timer Task");
	int nowSeconds = 0, year = 0, monthIndex = 0, dayIndex = 0;
	int seconds = 0;
	int sleepTime;
	int skip=0;
	unsigned char action;
        STimerList *psTimerList = NULL;

#ifdef SIMULATED_OCCUPANCY
	if(gpSimulatedDevice && gpSimulatedDevice->ruleEndTime)
	{
	    APP_LOG("DEVICE:rule", LOG_DEBUG, "Away Task is running");
	    return NULL;
	} 
#endif

#ifndef PRODUCT_WeMo_LEDLight
	/*stop countdown timer task if it is running*/
	stopCountdownTimer();

#endif
	/*main loop*/
	while(1)
	{
		/*get todays time in seconds*/
		GetCalendarDayInfo(&dayIndex, &monthIndex, &year, &nowSeconds);
		APP_LOG("DEVICE:rule", LOG_DEBUG, "createTimerRuleList for day index:%d, nowSeconds:%d", dayIndex,nowSeconds);

		/*create the sorted timer list*/
		createTimerRuleList(dayIndex);

		APP_LOG("DEVICE:rule", LOG_DEBUG, "created Timer Rule List...");

		/*move to start node*/
		moveToTimerNodeToExecute(nowSeconds);

		psTimerList = gpsTimerList;
		/*This loop execute all timer rules for active day*/
		while(1)
		{
			/*check if all timer rules executed*/
			if(psTimerList)
			{
				/*calculate sleep time*/
				sleepTime = psTimerList->time - nowSeconds;
				APP_LOG("DEVICE:rule", LOG_DEBUG, "sleepTime:%d,psTimerList->time:%d,nowSeconds:%d", sleepTime,psTimerList->time,nowSeconds);

				if((sleepTime + nowSeconds) > ONE_DAY_SECONDS)
				{
					/*overnight rule is active, break! create and reload timerList*/
					break;
				}

				/*check if rule start time has already passed*/
				if(0 > sleepTime)
				{
					/*No sleep, get start timer rule action*/
					action = psTimerList->action;
					APP_LOG("DEVICE:rule", LOG_DEBUG, "No sleep execute timer rule:%d", action);

					if(g_iDstNowTimeStatus)
					{
					    skip=1;
					    g_iDstNowTimeStatus = 0x00;
					}
				}
				else
				{
#ifndef PRODUCT_WeMo_LEDLight
					APP_LOG("DEVICE:rule", LOG_DEBUG, "Sleep for %d seconds", sleepTime);
					/*sleep for sleepTime seconds*/
					sleep(sleepTime);
#else
					GetCalendarDayInfo(&dayIndex, &monthIndex, &year, &seconds);
					seconds = seconds % 60;
					APP_LOG("DEVICE:rule", LOG_DEBUG, "Adjusted sleep time %d by %d seconds", sleepTime, seconds);
					/*sleep for sleepTime seconds*/
					sleep(sleepTime-seconds-5);
#endif 
					/*get timer rule action*/
					action = psTimerList->action;
					/*update now seconds*/
					nowSeconds = psTimerList->time;
					APP_LOG("DEVICE:rule", LOG_DEBUG, "Continue after sleep and execute timer action:%d", action);
				}

				if(!skip)
				{
#ifndef PRODUCT_WeMo_LEDLight
				    APP_LOG("DEVICE:rule", LOG_DEBUG, "Execute Timer rule action!");
				    /*Perform timer action*/
				    SetRuleAction(action, ACTUATION_TIME_RULE);
#else
					APP_LOG("DEVICE:rule", LOG_DEBUG, "Execute Timer LED rule action!");
					SetRuleActionLED(action, psTimerList->ruleIdTimer);
#endif //PRODUCT_WeMo_LEDLight
				}
				skip = 0;
				/*check and update timer list*/
				lockTimerRule();
				gpsTimerList = gpsTimerList->nextTimer;
				free(psTimerList);
				psTimerList = gpsTimerList;
				unlockTimerRule();
			}
			else
			{
				/*all timer rule for the day executed*/
				break;
			}
		}
		/*time left for active day change plus 10 second time to load active days timerlist*/
		sleepTime = ONE_DAY_SECONDS - nowSeconds + DELAY_10SEC + 1;
		/*all active days timer rules got executed sleep till active day change.*/
		APP_LOG("DEVICE:rule", LOG_DEBUG, "All timer rules executed! sleep for leftover sleepTime:%d", sleepTime);
		sleep(sleepTime);
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Continue after day change sleep time!");
	}
	return NULL;
}

int getSensorRuleData(int ruleId, char *deviceId, char *startAction, char *endAction, char *duration)
{
	int rowsRules=0,colsRules=0;
	char **ppsRulesArray=NULL;
	char query[SIZE_256B];

	/*get sensor duration this rule id*/
	memset(query, 0, sizeof(query));
	snprintf(query, sizeof(query), "SELECT StartAction, EndAction, SensorDuration FROM RULEDEVICES WHERE RuleID=\"%d\" AND DeviceID=\"%s\" limit 1;", ruleId,deviceId);
	APP_LOG("DEVICE:rule", LOG_DEBUG, "query:%s", query);

	/*execute database query*/
	if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
	{
		/*check if we got the data*/
		if(rowsRules && colsRules)
		{
			/*calculate sensor duration*/
			snprintf(startAction, SIZE_8B, "%d", atoi(ppsRulesArray[colsRules]));
			snprintf(endAction, SIZE_8B, "%d", atoi(ppsRulesArray[colsRules+1]));
			snprintf(duration, SIZE_8B, "%d", atoi(ppsRulesArray[colsRules+2]));
		}
		else
		{
			APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
			/*free database buffer*/
			WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
			return FAILURE;
		}
	}
	else
	{
		APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
		return FAILURE;
	}

	return SUCCESS;
}

void executeSensorRule()
{
	char startAction[SIZE_8B],duration[SIZE_8B],endAction[SIZE_8B], fullUdn[SIZE_256B];
	char *paramNames[] = {"BinaryState", "Duration", "EndAction", "UDN"};
	char *values[4] = {startAction,duration,endAction, fullUdn};
	int i=0,rowsRules=0,colsRules=0,rulesArraySize=0,deviceIndex=0,retVal=-1;
	char **ppsRulesArray=NULL;
	char query[SIZE_256B];
	SRuleInfo *psRule = gpsRuleList;
	char szUDN[SIZE_256B] = {'\0'};
	char *udn=NULL;

	if(gRuleHandle[e_SENSOR_RULE].ruleCnt)
	{
		while(psRule != NULL)
		{
			if((e_SENSOR_RULE == psRule->ruleType) && (psRule->isActive))
			{
				APP_LOG("DEVICE:rule", LOG_DEBUG, "Rule ID %d is Active, Execute it!", psRule->ruleId);

				/*get device IDs of all devices which are controlled by this rule id or by this sensor*/
				memset(query, 0, sizeof(query));
				snprintf(query, sizeof(query), "SELECT DeviceID FROM devicecombination where RuleID=\"%d\";", psRule->ruleId);
				APP_LOG("DEVICE:rule", LOG_DEBUG, "query:%s", query);
                
				/*execute database query*/
				if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
				{
					/*check if we got the data*/
					if(rowsRules && colsRules)
					{
						/*calculate array size*/
						rulesArraySize = (rowsRules + 1)*colsRules;
					}
					else
					{
						APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
						/*free database buffer*/
						WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
						return;
					}
				}
				else
				{
					APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
					return;
				}

				/*send sensor notify to all device IDS*/
				for(i = 1; i < rulesArraySize; i++)
				{
					APP_LOG("DEVICE:rule", LOG_DEBUG, "Try to send notify to Device: %s", ppsRulesArray[i]);
					udn = ppsRulesArray[i];

					memset(szUDN, 0, sizeof(szUDN));
					if(strstr(udn, "uuid:Bridge") != NULL)
					{
						strncpy(szUDN, udn, BRIDGE_UDN_LEN);
					}
					else if(strstr(udn, "uuid:Maker") != NULL)
					{
						strncpy(szUDN, udn, MAKER_UDN_LEN);
						strncat(szUDN, ":sensor:switch", sizeof(szUDN)-strlen(szUDN)-1);
					}
					else
						strncpy(szUDN, udn, sizeof(szUDN)-1);
					
					APP_LOG("DEVICE:rule", LOG_DEBUG, "Input UDN: %s, Converted UDN: %s", ppsRulesArray[i], szUDN);

					LockDeviceSync();
					/*get the device ID index in deive discoved list */
					deviceIndex = GetDeviceIndexByUDN(szUDN);
					UnlockDeviceSync();

					/*check if device id discovered*/
					if (0x00 != deviceIndex)
					{
						memset(startAction, 0, sizeof(startAction));
						memset(duration, 0, sizeof(duration));
						memset(endAction, 0, sizeof(endAction));
						memset(fullUdn, 0, sizeof(fullUdn));

						/*get sensor duration*/
						if(SUCCESS != getSensorRuleData(psRule->ruleId, ppsRulesArray[i], startAction, endAction, duration))
						{
							APP_LOG("DEVICE:rule", LOG_ERR, "Fetching sensor rule data failed");
						}

						/*make notification data*/
						strncpy(fullUdn, udn, sizeof(fullUdn)-1);

						APP_LOG("DEVICE:rule", LOG_DEBUG, "start action: %s, sensor duration: %s, stop action: %s",
								startAction, duration, endAction);

						/*send sensor notify*/
						retVal = PluginCtrlPointSendAction(PLUGIN_E_EVENT_SERVICE, deviceIndex, "SetBinaryState", (const char **)&paramNames, (char **)&values, 4);
						if (retVal != UPNP_E_SUCCESS)
						{
							APP_LOG("DEVICE:rule", LOG_DEBUG, "Sensor command sending failed: %s", ppsRulesArray[i]);
						}
					}
					else
					{
						APP_LOG("DEVICE:rule", LOG_DEBUG, "Device:%s  not in the device list", ppsRulesArray[i]);
						CtrlPointDiscoverDevices();
					}

				}

				/*free database buffer*/
				WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
			}
			psRule = psRule->psNext;
		}
	}
	return;
}

void enqueueRuleQ(SRulesQueue *qNode)
{
	SRulesQueue *tempQNode = NULL;

	if(NULL == qNode)
	{
		APP_LOG("DEVICE:rule",LOG_ERR,"Invalide node to queue, Not adding!");
		return;
	}

	lockRuleQueue();
	/*update only timestamp if rule id already in the queue to avoide sending stale notifications*/
	tempQNode = gRuleQHead;
	while(NULL != tempQNode)
	{
		if(tempQNode->ruleID == qNode->ruleID)
		{
			/*update timestamp*/
			tempQNode->ruleTS = qNode->ruleTS;	
			unlockRuleQueue();
			APP_LOG("DEVICE:rule",LOG_DEBUG,"Found Rule ID %d, Updated TS!", qNode->ruleID);
			return;
		}
		tempQNode = tempQNode->next;
	}

	/*empty queue*/
	if(NULL == gRuleQHead)
	{
		gRuleQHead = qNode;
		gRuleQTail = gRuleQHead;
	}
	/*queue it*/
	else
	{
		gRuleQTail->next = qNode;
		gRuleQTail = qNode;
	}
	unlockRuleQueue();
	APP_LOG("DEVICE:rule",LOG_DEBUG,"rule ID %d queued!", qNode->ruleID);
}

void destroyRuleQueue(void)
{
	SRulesQueue *qNode = NULL;

	lockRuleQueue();
	while(gRuleQHead)
	{
		qNode = gRuleQHead;
		gRuleQHead = gRuleQHead->next;
		free(qNode);
	}
	gRuleQTail = NULL;
	unlockRuleQueue();
	APP_LOG("DEVICE:rule",LOG_DEBUG,"Rule Queue Destroyed!");
}

SRulesQueue *dequeueRuleQ(void)
{
	SRulesQueue *qNode = NULL;

	lockRuleQueue();
	qNode = gRuleQHead;
	if(NULL != gRuleQHead)
	{
		if(gRuleQHead == gRuleQTail)
		{
			gRuleQHead = NULL;
			gRuleQTail = NULL;
		}
		else
		{
			gRuleQHead = gRuleQHead->next;
		}
		APP_LOG("DEVICE:rule",LOG_DEBUG,"dequeued rule id %d", qNode->ruleID);
	}
	unlockRuleQueue();
	return qNode;
}

void *AwayTask(void *args)
{
    APP_LOG("DEVICE:rule", LOG_DEBUG, "In Away Task");
#ifdef PRODUCT_WeMo_LEDLight
	if(!BA_LoadRule())
	{
		APP_LOG("UPNP: Rule", LOG_ERR, "Bridge away rule intialization failed");
		resetSystem();
	}
	else
		APP_LOG("UPNP: Rule", LOG_DEBUG, "Bridge away rule intialized");
#endif
#ifdef SIMULATED_OCCUPANCY
    SRuleInfo *psRule = gpsRuleList;
    int starttime = 0, endtime = 0, sleepTime = 0;

    APP_LOG("DEVICE:rule", LOG_DEBUG, "In Away Task");

    /* init is required here because we free all away task memory on exit of this task, so everytime when checker 
    starts this task, a new memory is required */
    simulatedOccupancyInit();

    if(parseSimulatedRule() != SUCCESS)
    {
	APP_LOG("Rule", LOG_DEBUG, "Simulated rule file parse failed...");
	return NULL;
    }

    while(psRule != NULL)
    {
	if((e_AWAY_RULE == psRule->ruleType) && (psRule->isActive))
	{
	    APP_LOG("DEVICE:rule", LOG_DEBUG, "Rule ID %d is Active, Execute it!", psRule->ruleId);
	    break;
	}
	psRule = psRule->psNext;
    }

    starttime = psRule->startTime;

    endtime = psRule->startTime + psRule->ruleDuration;
    /* update end time, in case of across day rule */
    if(psRule->isDayChange)
    {
	APP_LOG("DEVICE:rule", LOG_DEBUG, "across day rule update endtime");
	endtime = endtime - ONE_DAY_SECONDS;
    }		

    APP_LOG("DEVICE:rule", LOG_DEBUG, "start time: %d and end time: %d", starttime, endtime);

    gpSimulatedDevice->ruleEndTime = endtime;

    /* saved manual toggle state check */
    if(!checkLastManualToggleState())
    {
	    APP_LOG("DEVICE:rule", LOG_DEBUG, "Rule last manually toggled...");
	    goto on_return;
    }

    if(gRuleHandle[e_TIMER_RULE].ruleCnt)
    { 
	stopExecutorThread(e_TIMER_RULE);
	freeTimerList();
    }

#ifndef PRODUCT_WeMo_LEDLight
    /*check if countdown timer task is running, if yes stop it*/
    stopCountdownTimer();
#endif

    /* first random sleepTime seconds */
    sleepTime = adjustFirstTimer(starttime, endtime); 
    simulatedStartControlPoint();

    APP_LOG("DEVICE:rule", LOG_DEBUG, "Sleep for %d seconds", sleepTime);
    sleep(sleepTime);

    sleepTime = 1;
    while(psRule->isActive)
    {
	starttime = daySeconds();
	/* update start time, in case of 12 AM */
	if(!starttime)
	{
	    APP_LOG("DEVICE:rule", LOG_DEBUG, "12 AM case update starttime");
	    starttime = (3*SIMULATED_DURATION_ADDLTIME);
	    sleep(starttime);
	}

	/* update end time, in case of across day rule */
	if(psRule->isDayChange && endtime > ONE_DAY_SECONDS)
	{
	    APP_LOG("DEVICE:rule", LOG_DEBUG, "across day rule update endtime");
	    endtime = endtime - ONE_DAY_SECONDS;
	    gpSimulatedDevice->ruleEndTime = endtime;
	}		

	APP_LOG("DEVICE:rule", LOG_DEBUG, "now start time: %d and end time: %d", starttime, endtime);

	sleepTime = evaluateNextActions(starttime, endtime);

	/*sleep for later random sleepTime seconds*/
	if(!sleepTime)
	{
	    APP_LOG("DEVICE:rule", LOG_DEBUG, "manual toggled or rule over... break");
	    break;
	}
	else
	{
	    APP_LOG("DEVICE:rule", LOG_DEBUG, "Sleep for %d seconds", sleepTime);
	    sleep(sleepTime);
	}
    }

on_return:
    cleanupAwayRule(0);

    if(gRuleHandle[e_TIMER_RULE].ruleCnt) 
	startExecutorThread(e_TIMER_RULE);

#endif
#ifdef PRODUCT_WeMo_LEDLight
    while (1)
    {
    	sleep(10);
    }
#endif
    APP_LOG("DEVICE:rule", LOG_DEBUG, "Away Task done...");
    return NULL;
}


void enqueNotificationRule(int ruleId, int ruleType)
{
	SRulesQueue *qNode = NULL;
	int rowsRules=0,colsRules=0;
        char **ppsRulesArray=NULL;
        char query[SIZE_256B];

        /*get Notification Data for this rule id*/
        memset(query, 0, sizeof(query));

	if(ruleType == e_NOTIFICATION_RULE)
		snprintf(query, sizeof(query), "SELECT NotifyRuleID, NotificationMessage, NotificationDuration FROM SENSORNOTIFICATION where RuleID=\"%d\" limit 1;", ruleId);
	else
       		snprintf(query, sizeof(query), "SELECT NotifyRuleID, Message,Frequency FROM RULESNOTIFYMESSAGE WHERE RuleID=\"%d\" limit 1;", ruleId);

        APP_LOG("DEVICE:rule", LOG_DEBUG, "query:%s", query);

        /*execute database query*/
        if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
        {
                /*check if we got the data*/
                if(rowsRules && colsRules)
		{
			/*fill notification data*/
			qNode =  (SRulesQueue*)CALLOC(1, sizeof(SRulesQueue)); 
			if(NULL == qNode)
			{
				APP_LOG("DEVICE:rule", LOG_DEBUG, "Memory Allocation Failed!");
				resetSystem();
			}

			qNode->ruleID = ruleId;
			qNode->notifyRuleID = atoi(ppsRulesArray[colsRules]);
                        
                        if( !(qNode->ruleMSG = STRDUP( ppsRulesArray[colsRules+1] ))) {
                            APP_LOG("DEVICE:rule", LOG_ERR, "Memory Allocation Failed!");
				resetSystem();
			}
			qNode->ruleFreq = atoi(ppsRulesArray[colsRules+2]);

			qNode->ruleTS =  GetUTCTime();
		}
                else
                {
                	APP_LOG("DEVICE:rule", LOG_ERR, "No valid data");
                        /*free database buffer*/
                        WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
			return;
                }
        }
        else
        {
                APP_LOG("DEVICE:rule", LOG_ERR, "No valid data");
		return;
        }


	APP_LOG("DEVICE:rule", LOG_DEBUG, "Rule Queue Data\nruleID:%d\nnotifyRuleID:%d\nruleMSG:%s\nruleFreq:%d\nruleTS:%lu\n", qNode->ruleID,qNode->notifyRuleID,qNode->ruleMSG,qNode->ruleFreq,qNode->ruleTS);
	enqueueRuleQ(qNode);

	return;
}

int isExecuteInsightRule(int curValue, int ruleValue, int condition)
{
#ifdef PRODUCT_WeMo_Insight
    // compare the value of the parameter with condition value using the OPCode mentioned in the rule.
    switch(condition)
    {
        case E_EQUAL:
            if(curValue == ruleValue)
            {
                return(1);
            }
        break;
        case E_LARGER:
            if((curValue >= ruleValue) && (curValue < (ruleValue + INSIGHT_TASK_POLL_FREQ)))
            {
                return(1);
            }
        break;
        case E_LESS:
            if(curValue < ruleValue)
            {
                return(1);
            }
        break;
        case E_EQUAL_OR_LARGER:
            if(curValue >= ruleValue)
            {
                return(1);
            }
        break;
        case E_EQUAL_OR_LESS:
            if(curValue <= ruleValue)
            {
                return(1);
            }
        break;
        default:
            APP_LOG("DEVICE:rule", LOG_DEBUG, "Wrong Insight condition: %d",condition);
            return (0);
        break;
    }
#endif
    return(0);
}

#ifdef PRODUCT_WeMo_Insight
void processInsightNotification(int insightRuleType, int ruleCurValue)
{
	int rowsRules=0,colsRules=0;
	unsigned int type, value, condition;
	char **ppsRulesArray=NULL;
	char query[SIZE_256B];
	SRuleInfo *psRule = gpsRuleList;

	if(gRuleHandle[e_INSIGHT_RULE].ruleCnt)
	{
		int currentState = GetCurBinaryState();
		while(psRule != NULL)
		{
			if((e_INSIGHT_RULE == psRule->ruleType) && (psRule->isActive))
			{
				//APP_LOG("DEVICE:rule", LOG_DEBUG, "Rule ID %d is Active, Execute it!", psRule->ruleId);

				/*get rule type for this insight rule*/
				memset(query, 0, sizeof(query));
				snprintf(query, sizeof(query), "SELECT Type,Value,Level FROM RULEDEVICES where RuleID=\"%d\" AND DayID=-1;", psRule->ruleId);
				//APP_LOG("DEVICE:rule", LOG_DEBUG, "query:%s", query);

				/*execute database query*/
				if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
				{
					/*check if we got the data*/
					if(rowsRules && colsRules)
					{
						/*get rule ID values*/
						type =  atoi(ppsRulesArray[colsRules]);
						condition =  atoi(ppsRulesArray[colsRules+1]);
						value =  atoi(ppsRulesArray[colsRules+2]);

						//[WEMO-29853] - Append E_STATE case for APNS rule
						//APNS rule case for POWER ONFOR
						//if(type == insightRuleType)
						if(E_ON_DURATION == type)
 						{
							if(isExecuteInsightRule(g_RuleONFor, value, condition))
 							{
								APP_LOG("DEVICE:rule", LOG_DEBUG, "Type and condition Matched, insightRuleType:%d,ruleCurValue:%d,RuleValue:%d", type, g_RuleONFor, value);
 								enqueNotificationRule(psRule->ruleId, psRule->ruleType);
 							}
 						}
 						//APNS rule case for Sensing POWER ON/OFF
						else if (E_STATE == type)
						{
							
							//Restriction to have 1 notification for each state change
							if (currentState != g_APNSLastState)
							{
								//[WEMO-31158] - Added case for Rule Off when change state from ON -> SBY
								if ((g_APNSLastState == POWER_ON) && (currentState == POWER_SBY))
								{
									if(value == POWER_OFF)
									{
										value = POWER_SBY;
									}
								}
								//[WEMO-31158] - Ignore the case state change from SBY -> OFF
								if((currentState == POWER_OFF) && (g_APNSLastState == POWER_SBY)){
									condition = E_WRONG_OPCODE;
								}									
								if (isExecuteInsightRule(currentState, value, condition))
								{
									APP_LOG("DEVICE:rule", LOG_DEBUG, "Matched, currentState: %d, currentValue:%d, condition:%d", currentState, value, condition);
									enqueNotificationRule(psRule->ruleId, psRule->ruleType);
								}
							}
						}
					}
					else
					{
						APP_LOG("DEVICE:rule", LOG_ERR, "No entry for rule: %d", psRule->ruleId);
						/*free database buffer*/
						WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
					}
				}
				else
				{
					APP_LOG("DEVICE:rule", LOG_ERR, "No entry for rule: %d", psRule->ruleId);
				}

				/*free database buffer*/
				WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
			}
			psRule = psRule->psNext;
		}
		//update state to make sure 1 notification sent
		g_APNSLastState = currentState;
	}
	return;
}
#endif

void *InsightTask(void *args)
{
#ifdef PRODUCT_WeMo_Insight
	int nowSeconds=0, year=0, month_index=0, dayIndex=0;

	GetCalendarDayInfo(&dayIndex, &month_index, &year, &nowSeconds);

	if(nowSeconds%INSIGHT_TASK_POLL_FREQ)
	{
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Adjusting Rule boundary: %d", nowSeconds);
		sleep(INSIGHT_TASK_POLL_FREQ-(nowSeconds%INSIGHT_TASK_POLL_FREQ));
	}


	while(1)
	{

		//[WEMO-29853] - Update called method to add more APNS rules case
		/*check and process if any insight rule is active*/
		processInsightNotification(0, 0);	
	
		/*sleep for INSIGHT_TASK_POLL_FREQ second*/
		sleep(INSIGHT_TASK_POLL_FREQ);
	}
#endif
	return NULL;
}

void executeNotifyRule(void)
{
	SRuleInfo *psRule = gpsRuleList;

	if(gRuleHandle[e_NOTIFICATION_RULE].ruleCnt)
	{
		while(psRule != NULL)
		{
			if((e_NOTIFICATION_RULE == psRule->ruleType) && (psRule->isActive))
			{
				APP_LOG("DEVICE:rule", LOG_DEBUG, "Rule ID %d is Active, Execute it!", psRule->ruleId);
				enqueNotificationRule(psRule->ruleId, psRule->ruleType);
			}
			psRule = psRule->psNext;
		}
	}
}
#ifndef PRODUCT_WeMo_LEDLight
int checkAndExecuteCountdownTimer(int powerStatus)
{
	int retVal = 0;

	/*get contdown timer last minute running state*/
	int countdownRuleLastMinStatus =  gCountdownRuleInLastMinute;
	/*this API will start/restart/stop countdown timer dependng upon if coundown timer is not_running/runing_in_last_minute/running_not_in_last_ minute*/
	executeCountdownRule(powerStatus);
	/*check if it was running in last minute, if yes do not toggle relay, countdown timer restarted*/
	if(gRuleHandle[e_COUNTDOWN_RULE].ruleCnt && countdownRuleLastMinStatus)
	{
		APP_LOG("Button", LOG_DEBUG, "Countdown timer was in last minute, Do not toggle!");
#ifdef PRODUCT_WeMo_Insight
        if (POWER_OFF == g_PowerStatus)
            g_PowerStatus = POWER_ON;
#endif
		retVal = 1;
	}

	return retVal;
}

void stopCountdownTimer()
{
	int ret = -1;

	/*check if thread not exists*/
	if(s_countdown_rule_thread == INVALID_THREAD_HANDLE)
	{
		APP_LOG("UPNP: Rule", LOG_DEBUG, "Countdown _rule thread not running!");
		return;
	}

	/*clear gCountdownEndTime*/
	gCountdownEndTime = 0;

	/*cancel thread*/
	ret = pthread_cancel(s_countdown_rule_thread);
	if (0x00 != ret)
	{
		APP_LOG("UPNP: Rule", LOG_DEBUG, "thread_cancel: Could not stop countdown rule thread");
	}
	else
	{
		/*send local UPnP notification*/
		LocalCountdownTimerNotify();
		APP_LOG("UPNP: Rule", LOG_DEBUG, "thread_cancel: Successfully stopped countdown rule thread");
	}
	
	/*mark invalid handle*/
	s_countdown_rule_thread = INVALID_THREAD_HANDLE;
	
	/*check the last minute status, if in last minute set power led  status to ON */
	if(gCountdownRuleInLastMinute)
	{
		/*set LED status to on*/
		/*start LED toggle by 1s ON 500ms OFF*/
		SetActivityLED(0x03);
		gCountdownRuleInLastMinute = 0;
	}

	return;
}

int getCountdownDuration(unsigned int ruleId)
{
	int rowsRules=0,colsRules=0;
	char **ppsRulesArray=NULL;
	char query[SIZE_256B];
	unsigned int duration = 0;

	/*get sensor duration this rule id*/
	memset(query, 0, sizeof(query));
	snprintf(query, sizeof(query), "SELECT CountdownTime FROM RULEDEVICES WHERE RuleID=\"%d\" AND DeviceID=\"%s\" limit 1;", ruleId,g_szUDN_1);
	APP_LOG("DEVICE:rule", LOG_DEBUG, "query:%s", query);

	/*execute database query*/
	if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
	{
		/*check if we got the data*/
		if(rowsRules && colsRules)
		{
			/*calculate sensor duration*/
			duration = atoi(ppsRulesArray[colsRules]);
			APP_LOG("DEVICE:rule", LOG_ERR, "duration:%d", duration);
		}
		else
		{
			APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
			/*free database buffer*/
			WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
		}
	}
	else
	{
		APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
	}
	return duration;
}

void *startCountdownTimer(void *args)
{
	unsigned int ruleId = 0;
	unsigned int duration = 0;
	int sleepTime = 0;

	if(NULL != args)
	{
		/*get rule id from argument*/
		ruleId = *((int*)args);
		free(args);
		args = NULL;
	}
	else
		return NULL;

	APP_LOG("DEVICE:rule", LOG_DEBUG, "In Start Countdown Timer for rule Id:%d", ruleId);
	gCountdownRuleInLastMinute = 0;
	
	/*get countdown duration from DB*/
	duration = getCountdownDuration(ruleId);

	/*save countdown timer end utc time*/
	gCountdownEndTime = GetUTCTime() + duration;

	/*send local UPnP notification*/
	LocalCountdownTimerNotify();

	/*calculate sleep time less then get countdown duration from DB*/
	sleepTime = duration - DELAY_60SEC;
	if(0 < sleepTime)
	{	
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Sleeping countdown rule for %d seconds", sleepTime);
		sleep(sleepTime);
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Continue after sleep");
	}

	gCountdownRuleInLastMinute = 1;
	
	APP_LOG("DEVICE:rule", LOG_DEBUG, "Start last minute LED toggle for 60 second");
	/*start LED toggle by 1s ON 500ms OFF*/
	SetActivityLED(0x02);
	sleep(DELAY_60SEC);
	APP_LOG("DEVICE:rule", LOG_DEBUG, "last minute LED toggle done");

	/*stop power monitor thread*/
	StopPowerMonitorTimer();

	/*clear globals*/
	gCountdownRuleInLastMinute = 0;
	gCountdownEndTime = 0;

	/*power off device*/
	SetRuleAction(POWER_OFF, ACTUATION_COUNTDOWN_TIMER_RULE);
			
	/*mark invalid handle*/
	s_countdown_rule_thread = INVALID_THREAD_HANDLE;
	
	APP_LOG("DEVICE:rule", LOG_DEBUG, "Exiting countdown timer rule thread!");
	return NULL;
}

void startCountdownTimerThread(unsigned int ruleId)
{
	int *arg = (int*)CALLOC(1, sizeof(int));
	if(NULL == arg)
	{
		APP_LOG("UPNP: Rule", LOG_DEBUG, "Memory allocation failed");
		resetSystem();
	}
	*arg = ruleId;

	if(s_countdown_rule_thread != INVALID_THREAD_HANDLE)
	{
		APP_LOG("UPNP: Rule", LOG_DEBUG, "Countdown rule thread already running, Stopping It!");
		stopCountdownTimer();
	}

	int ret = pthread_create(&s_countdown_rule_thread, 0x00, startCountdownTimer, arg);
	if (0x00 != ret)
	{
		APP_LOG("UPNP: Rule", LOG_DEBUG, "Could not create countdown rule thread");
		resetSystem();
	}
	else
		APP_LOG("UPNP: Rule", LOG_DEBUG, "countdown Rule thread created");

	return;
}

void executeCountdownRule(int deviceNewState)
{
	SRuleInfo *psRule = gpsRuleList;
	int countdownRuleLastMinStatus = 0;

	APP_LOG ("DEVICE:rule", LOG_DEBUG, "In executeCountdownRule");

    APP_LOG ("DEVICE:rule", LOG_DEBUG, "gCountdownRuleInLastMinute = [%d]",
             gCountdownRuleInLastMinute);

#ifdef PRODUCT_WeMo_Insight
    /* If the Insight device state is POWER_SBY, do not trigger the countdown
     * timer rule.
     */
    if ((POWER_SBY == deviceNewState) && (0 == gCountdownRuleInLastMinute))
        return;
#endif

	if(gRuleHandle[e_COUNTDOWN_RULE].ruleCnt && !(gRuleHandle[e_TIMER_RULE].ruleCnt) && !(gRuleHandle[e_AWAY_RULE].ruleCnt))
	{
		while(psRule != NULL)
		{
			if((e_COUNTDOWN_RULE == psRule->ruleType) && (psRule->isActive))
			{
				APP_LOG("DEVICE:rule", LOG_DEBUG, "Rule ID %d is Active, Execute it! deviceNewState:%d", psRule->ruleId, deviceNewState);
				
				/*if new state is POWER_OFF*/
				if((POWER_ON != deviceNewState)) 
				{
					/*get contdown timer last minute running state*/
					countdownRuleLastMinStatus =  gCountdownRuleInLastMinute;

					/*stop countdown timer thread*/
					stopCountdownTimer();

					/*check if it was running in last minute, if yes restart countdown timer thread*/
					if(countdownRuleLastMinStatus)
						startCountdownTimerThread(psRule->ruleId);
				}
				else 
				{
					/*restart countdown timer again*/
					startCountdownTimerThread(psRule->ruleId);
				}
		
				break;
			}
			psRule = psRule->psNext;
		}
	}
	else
		stopCountdownTimer();

	APP_LOG ("DEVICE:rule", LOG_DEBUG, "Exiting executeCountdownRule");
	return;
}

#endif //#ifndef PRODUCT_WeMo_LEDLight
void *CrockpotTask(void *args)
{
	return NULL;
}

void *MakersensorTask(void *args)
{
	return NULL;
}

void RuleToggleLed(int curState)
{
	pMessage msg = 0x00;

	if (0x00 == curState)
	{
		msg = createMessage(RULE_MESSAGE_OFF_IND, 0x00, 0x00);
	}
	else if (0x01 == curState)
	{
		msg = createMessage(RULE_MESSAGE_ON_IND, 0x00, 0x00);
	}

	SendMessage(PLUGIN_E_RELAY_THREAD, msg);
	SetLastUserActionOnState(curState);

	APP_LOG("Button", LOG_DEBUG, "rule ON/OFF message sent out");
}

int SetRuleAction(unsigned int ruleAction, char *actuation)
{
	int retVal;
	unsigned int ruleActionToApply = ruleAction;

	APP_LOG("DEVICE:rule", LOG_DEBUG, "set actuation to %s", actuation);
	setActuation(actuation);

	retVal = ChangeBinaryState(ruleActionToApply);
	if (0 == retVal)
	{
#ifdef PRODUCT_WeMo_Insight
		if(POWER_ON == ruleActionToApply)
		{
			ruleActionToApply = POWER_SBY;
			APP_LOG("DEVICE:rule", LOG_DEBUG, "Changed ON State To %d", ruleActionToApply);
		}
#endif
		UPnPInternalToggleUpdate(ruleActionToApply);
	}
	return retVal;
}
#ifdef PRODUCT_WeMo_LEDLight
bool isRuleActiveOnDevice(const char* device_id, ERuleType ruleType)
{
	SRuleInfo* psRule= gpsRuleList;
	char query[SIZE_256B];
	char **ppsRuleIdArray=NULL;
	unsigned int *ruleIds =NULL;
	bool ret = false;
	int i=0,numIds;
	int rowsRules=0,colsRules=0;
	/* Find out how ruleID's for  */
	memset(query, 0, sizeof(query));
	snprintf(query, sizeof(query), "SELECT DISTINCT RuleID FROM RULEDEVICES where DeviceID like \"%%%s\";", device_id);
	APP_LOG("DEVICE:rule", LOG_DEBUG, "query:%s", query);
	/*execute database query*/
	if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRuleIdArray,&rowsRules,&colsRules))
	{
		/*check if we got the data*/
		if(rowsRules && colsRules)
		{
			numIds = rowsRules;

			/* allocate size for device Id array */
			ruleIds =(unsigned int *) MALLOC(sizeof(unsigned int)* numIds);

			if(!ruleIds)
			{
				APP_LOG("DEVICE:rule", LOG_ERR, "Out of memory for %d pointers", numIds);
				ret = false;
				goto exit_fn;
			}
			else
				APP_LOG("DEVICE:rule", LOG_ERR, "Allocated memory for %d pointers, ruleIds: %p", numIds, ruleIds);

			/* save the rule ids */
			APP_LOG("DEVICE:rule", LOG_DEBUG, "i:%d rowsRules:%d",i,rowsRules);
			while(i < rowsRules)
			{
				ruleIds[i]=atoi(ppsRuleIdArray[i+1]);
				APP_LOG("DEVICE:rule", LOG_DEBUG, "ruleIds:%d",ruleIds[i]);
				i++;
			}
		}
		else
		{
			APP_LOG("DEVICE:rule", LOG_ERR, "No ruleid found");
			ret = false;
			goto exit_fn;
		}
	}
	else
	{
		APP_LOG("DEVICE:rule", LOG_ERR, "No ruleid found");
		return false;
	}

	APP_LOG("DEVICE:rule", LOG_DEBUG, "Device numIds %d",numIds);
	while(psRule != NULL) {
		for(i=0;i<numIds;i++) {
			if((ruleType == psRule->ruleType) && (psRule->ruleId == ruleIds[i]) && psRule->isActive) {
				APP_LOG("DEVICE:rule", LOG_DEBUG, "Away/Timer rule configured %d",psRule->ruleId);
				ret =true;
				goto exit_fn;
			}
		}
		psRule=psRule->psNext;
	}

exit_fn:
	/*free database buffer*/
	if(ppsRuleIdArray)
	{
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Freeing DB table");
		WeMoDBTableFreeResult(&ppsRuleIdArray,&rowsRules,&colsRules);
		ppsRuleIdArray = NULL;
	}

	if(ruleIds)
		free(ruleIds);
	return ret;
}

int RunLinkSimpleTimerRule(int dayIndex, unsigned int ruleId, char *actuation, int action)
{
	char query[SIZE_256B];
	int rowsRules=0,colsRules=0;
	char **ppsRulesArray=NULL;
	char **deviceIds = NULL;
	int numDevices = 0;
	int ret = 0;
	int i=0;
	/* Reverse Map FW day index to App day Index */
	int appDayId = APP_DAY_INDEX(dayIndex);
		
	APP_LOG("DEVICE:rule", LOG_DEBUG, "&deviceIds: %x, &numDevices: %x", &deviceIds, &numDevices);
	ret = prepareLinkDeviceList(ruleId, &deviceIds, &numDevices);

	if(ret)
	{
		APP_LOG("DEVICE:rule", LOG_ERR, "prepareLinkDeviceList failed!!");
		return FAILURE;
	}
	else
	{
		APP_LOG("DEVICE:rule", LOG_DEBUG, "deviceIds: %p, *deviceIds: %p", deviceIds, *deviceIds);

	}

	/* Fetch CapabilityStart values for this rule and day index */

	for(i=0;i<numDevices;i++)
	{
		/* seperate group id / bulb id from Bridge UDN and identify between them 
		   user strrchr for : and  extract last portion of deviceId, check its length
		   to determine whether its group or bulb then call SD_SetCapabilityValue
		 */

		char *targetId=NULL;
		int isGroup = 0;

		
		APP_LOG("DEVICE:rule", LOG_DEBUG, "device: %s, i: %d, num: %d", deviceIds[i], i, numDevices);

		if(strstr(deviceIds[i], "Bridge") != NULL)
		{
			APP_LOG("DEVICE:rule", LOG_DEBUG, "--Bridge device--");
		}
		else
		{
			APP_LOG("DEVICE:rule", LOG_DEBUG, "Not Bridge device--");
			continue;
		}

		targetId = strrchr(deviceIds[i], ':');
		if(targetId)
			targetId++;
		else
		{
			goto exit_dev_list;
			ret = FAILURE;
		}
		APP_LOG("DEVICE:rule", LOG_DEBUG, "target device: %s length %d ", targetId, strlen(targetId));
		if((strlen(targetId) != GROUP_ID_LEN) && (strlen(targetId) != DEVICE_ID_LEN)) {
			APP_LOG("DEVICE:rule", LOG_DEBUG, "Wrong groupID or wrong deviceID");
			continue;
		}
		if(strlen(targetId) == GROUP_ID_LEN)
			isGroup=1;
		else
			isGroup=0;

		APP_LOG("DEVICE:rule", LOG_DEBUG, "target device: %s, isGoup: %d", targetId, isGroup);


		memset(query, 0, sizeof(query));

		/*WEMO-42731: Day ID will not be changed for overnight rules so fetch the values from previous day rule.
		*/
		if(action >= LED_OVERNIGHT_ACTION)
		{
			if(action % LED_OVERNIGHT_ACTION)
				snprintf(query, sizeof(query), "SELECT ZBCapabilityStart FROM RULEDEVICES where RuleID=\"%d\" AND DayID=\"%d\" AND DeviceID=\"%s\";",ruleId, appDayId-1, deviceIds[i]);
			else
				snprintf(query, sizeof(query), "SELECT ZBCapabilityEnd FROM RULEDEVICES where RuleID=\"%d\" AND DayID=\"%d\" AND DeviceID=\"%s\";",ruleId, appDayId-1, deviceIds[i]);
		}
		else
		{
			if((action == LED_START_CAPABILITIES) || (action == LED_START_CAPABILITIES+1) || (0 == action)) { /* Simple rule and timer rule Start */
				snprintf(query, sizeof(query), "SELECT ZBCapabilityStart FROM RULEDEVICES where RuleID=\"%d\" AND DayID=\"%d\" AND DeviceID=\"%s\";", 
						ruleId, appDayId, deviceIds[i]);
			}
			else if((action == LED_END_CAPABILITIES) || (action == LED_END_CAPABILITIES+1)) { /* Timer rule End */
				snprintf(query, sizeof(query), "SELECT ZBCapabilityEnd FROM RULEDEVICES where RuleID=\"%d\" AND DayID=\"%d\" AND DeviceID=\"%s\";", 
						ruleId, appDayId, deviceIds[i]);
			}
		}
		APP_LOG("DEVICE:rule", LOG_DEBUG, "query:%s", query);

		/*execute database query*/
		if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
		{
			/*check if we got the data*/
			if(rowsRules && colsRules)
			{
				/*Apply capability actions*/
				int j=0;
				char *capability = NULL, *value = NULL;
				SD_CapabilityID sCapability = {SD_CAPABILITY_ID};
				SD_DeviceID     sDevice;

				while(j<rowsRules)
				{
					/* 
						Separate capablities from their values and then
						call the SetDeviceCapabilityValue(00158D0000317A41,10006,(1), 1) API
						for each capability value
					*/
					capability = ppsRulesArray[j+1];
                                        
                                        /* If the capability is "0" then ignore passing it to the SD
                                         * API.
                                         */
                                        if (0 == strcmp (capability, "0"))
                                        {
                                             j++;
					     APP_LOG ("DEVICE:rule", LOG_DEBUG, "Capability: %s, j: %d",
                                                      capability, j);
                                             continue;
                                        }

					value = strchr(ppsRulesArray[j+1], '=');
					if(!value)
					{
						APP_LOG("DEVICE:rule", LOG_ERR, "/*free database buffer*/");
						WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
						ret = FAILURE;
						goto exit_dev_list;
					}
					
					/* Updating with a hope that we'll not require these values within this loop again */
					*value = '\0'; // terminate capbility
					strncpy(sCapability.Data, capability, sizeof(sCapability.Data));

					value++;

					APP_LOG("DEVICE:rule", LOG_DEBUG, "Capability: %s, Value: %s, j: %d", capability, value, j);
					j++;

					if(isGroup)
						sDevice.Type = SD_GROUP_ID;
					else
						sDevice.Type = SD_DEVICE_EUID;

					strncpy(sDevice.Data, targetId, sizeof(sDevice.Data));
					if(isRuleActiveOnDevice(sDevice.Data,e_AWAY_RULE)) {	
						APP_LOG("DEVICE:rule", LOG_DEBUG, "Away Task is running on device [%s]",sDevice.Data);
					}
					else {
						ret = SD_SetCapabilityValue(&sDevice, &sCapability, value, SE_LOCAL + SE_REMOTE);
						if(!ret){
							APP_LOG("DEVICE:rule", LOG_ERR, "SD_SetCapabilityValue failed!!");
						}
						else{
							APP_LOG("DEVICE:rule", LOG_ERR, "SD_SetCapabilityValue Succeed!!");
						}
					}
				}

				/*free database buffer*/
				WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
			}
			else
			{
				APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
				/*free database buffer*/
				WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
				ret = FAILURE;
				goto exit_dev_list;
			}
		}
		else
		{
			APP_LOG("DEVICE:rule", LOG_ERR, "No Link device actions");
			ret = FAILURE;
			goto exit_dev_list;
		}
	}

	/* TODO: Set actuation value */

exit_dev_list:
	/* Free memory taken up by Device ID array */
	//if(ret)
	{
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Freeing device list");
		for(i=0;i<numDevices;i++)
			if(deviceIds[i])
				free(deviceIds[i]);

		if(deviceIds)
			free(deviceIds);

		return FAILURE;
	}

	return ret;
}

int SetRuleActionLED(int action, int ruleId)
{
	int nowSeconds=0, last_seconds=0, year=0, monthIndex=0, dayIndex=0;
	APP_LOG("DEVICE:rule", LOG_ERR, "SetRuleActionLED!!");
	GetCalendarDayInfo(&dayIndex, &monthIndex, &year, &nowSeconds);
	APP_LOG("DEVICE:rule", LOG_ERR, "RunLinkSimpleTimerRule starts!!");
	RunLinkSimpleTimerRule(dayIndex, ruleId, ACTUATION_TIME_RULE,action); /* only timer rule ID is passed */
}
int isNotifyTrue (SD_Device device, int status, int level)
{
    int ret_val = FAILURE;
	
    APP_LOG("DEVICE:isNotifyTrue", LOG_DEBUG, "Entering fn:isNotifyTrue");

    if (0 == strcmp (device->ModelCode, SENSOR_MODELCODE_DW))
    {
        APP_LOG("DEVICE:isNotifyTrue", LOG_DEBUG, "DW sensor level:%d status:%d", level, status);

        if (((DW_OPENS == level) && (DW_OPENS == status)) ||
	    ((DW_CLOSES == level) && (DW_CLOSES == status)) ||
	    ((DW_OPENS_OR_CLOSES == level)&&((DW_OPENS ==status)|| (DW_CLOSES == status))))
	{
            ret_val = SUCCESS;
	}
    }
    else if (0 == strcmp (device->ModelCode, SENSOR_MODELCODE_FOB))
    {
        APP_LOG("DEVICE:isNotifyTrue", LOG_DEBUG, "Key Fob sensor level:%d status:%d", level, status);

        if (((FOB_ARRIVES == level) && (FOB_ARRIVES == status)) ||
	    ((FOB_LEAVES == level) && (FOB_LEAVES == status)) ||
	    ((FOB_ARRIVES_OR_LEAVES == level) && ((FOB_ARRIVES == status) || (FOB_LEAVES == status))))
	{
            ret_val = SUCCESS;
	}
    }
    else if (0 == strcmp (device->ModelCode, SENSOR_MODELCODE_PIR))
    {
        APP_LOG("DEVICE:isNotifyTrue", LOG_DEBUG, "Motion sensor level:%d status:%d", level, status);
	if ((SENSORING_ON == level ) && (SENSORING_ON == status)) 
	{
		ret_val = SUCCESS;
	}
    }
    else
    {
	APP_LOG("DEVICE:isNotifyTrue", LOG_DEBUG, "Invalid Device");	    
    }

    APP_LOG("DEVICE:isNotifyTrue", LOG_DEBUG, "Exiting fn:isNotifyTrue");
    return ret_val;
}

int doesNotificationRuleExist(SD_Device deviceID,int status, int *ruleId)
{
    char query[SIZE_256B];
    int rowsRules = 0,colsRules = 0;
    char **ppsRulesArray = NULL;
    char **deviceIds = NULL;
    int numDevices = 0;
    int ret = FAILURE;
    int i = 0;
    char *targetId = NULL;
    int level = 0xff;
    SRuleInfo *psRule = gpsRuleList;

    APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "Entering fn:doesNotificationRuleExist");
    
    if (!((0 == strcmp (deviceID->ModelCode, SENSOR_MODELCODE_DW))||
          (0 == strcmp (deviceID->ModelCode, SENSOR_MODELCODE_FOB)) ||
          (0 == strcmp (deviceID->ModelCode, SENSOR_MODELCODE_PIR))))
    {
        APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "No notification rule exist for this device");
        return FAILURE;
    }

    if (gRuleHandle[e_NOTIFICATION_RULE].ruleCnt)
    {
        while(psRule != NULL)
	{
            if((e_NOTIFICATION_RULE == psRule->ruleType) && (psRule->isActive))
	    {
                APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "Rule ID %d is Active, Execute it!", psRule->ruleId);
		ret = prepareLinkDeviceList(psRule->ruleId, &deviceIds, &numDevices);
		
                APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "&deviceIds: %x, numDevices: %d", &deviceIds, numDevices);

		if(ret)
		{
                    APP_LOG("DEVICE:doesNotificationRuleExist", LOG_ERR, "prepareLinkDeviceList failed!!");
		    return FAILURE;
		}
		else
		{
                    ret = FAILURE;

		    APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "deviceIds: %p, *deviceIds: %p", deviceIds, *deviceIds);
		    for(i = 0; i < numDevices; i++)
		    {
                        APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "device: %s, i: %d, num: %d", deviceIds[i], i, numDevices);

			if(strstr(deviceIds[i], "Bridge") != NULL)
			{
			    APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "--Bridge device--");
			}
			else
			{
			    APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "Not Bridge device--");
			    continue;
			}

			targetId = strrchr(deviceIds[i], ':');
			if(targetId)
			{
			    targetId++;
			    APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "target device: %s length %d ", targetId, strlen(targetId));

			    if((strlen(targetId) != GROUP_ID_LEN) && (strlen(targetId) != DEVICE_ID_LEN))
			    {
		                 APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "Wrong groupID or wrong deviceID");
				 continue;
			    }
			    if (0 == strcmp(targetId, deviceID->EUID.Data))
			    {
				 /* Find out the level for this rule */
				 memset(query, 0, sizeof(query));
				 snprintf(query, sizeof(query), "SELECT Level FROM RULEDEVICES where RuleID=\"%d\" AND DeviceId=\"%s\" limit 1", psRule->ruleId, deviceIds[i]);

				 APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "query:%s", query);

				 /*execute database query*/
				 if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
				 {
			             level = atoi(ppsRulesArray[1]);
				     *ruleId = psRule->ruleId;
				     APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "level:%d ruleId:%d", level, *ruleId);
				     ret = isNotifyTrue(deviceID, status, level);
                                     if (SUCCESS == ret)
                                     {
				         APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "isNotify returned true");
				         break;
                                     } 
                                     else
                                     {
				         APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "isNotify returned false, verify for next rule data");
				         continue;
                                     } 
				 }
				 else
				 {
					 APP_LOG("DEVICE:rule", LOG_DEBUG, "Freeing DB table");
					 ret = FAILURE;
					 break;
				 }
			    }
			    else
			    {
				 continue;
			    }
			}
			else
			{
				break;
			}
		    }
		}
	    }
            if (SUCCESS == ret)
            {
               break;
            }
            else
            {
	       psRule = psRule->psNext;
            }
	}
    }

    /*free database buffer*/
    if(ppsRulesArray)
    {
	    APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "Freeing DB table");
	    WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
	    ppsRulesArray = NULL;
    }

    APP_LOG("DEVICE:rule", LOG_DEBUG, "Freeing device list");

    for(i = 0;i < numDevices; i++)
    {
        if(deviceIds[i])
	{
	    free(deviceIds[i]);
	}
    }

    if(deviceIds)
    {
        free(deviceIds);
    }

    APP_LOG("DEVICE:doesNotificationRuleExist", LOG_DEBUG, "Exiting fn:isValidRuleExists");
    return ret;
}

bool makerulenode(mxml_node_t *pNode, SD_Device device, int status,int ruleId)
{
    mxml_node_t *pNoderule = NULL;
    mxml_node_t *pNodeDevice = NULL;
    SRulesQueue *psRuleQ = NULL;
    bool bResult = false;
    int rowsRules=0,colsRules=0;
    char **ppsRulesArray=NULL;
    char query[SIZE_256B];

    APP_LOG("DEVICE:rule", LOG_DEBUG, "Entering fn:makerulenode");

    snprintf(query, sizeof(query), "SELECT NotifyRuleID, NotificationMessage, NotificationDuration FROM SENSORNOTIFICATION where RuleID=\"%d\" limit 1;", ruleId);
    /*execute database query*/
    if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
    {
        /*check if we got the data*/
	if(rowsRules && colsRules)
	{
	    /*fill notification data*/
	    psRuleQ =  (SRulesQueue*)CALLOC(1, sizeof(SRulesQueue)); 
	    if (NULL == psRuleQ)
	    {
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Memory Allocation Failed!");
		resetSystem();
	    }
	    psRuleQ->ruleID = ruleId;
	    psRuleQ->notifyRuleID = atoi(ppsRulesArray[colsRules]);
	    psRuleQ->ruleTS =  GetUTCTime();
	    
            if (0 == strcmp (device->ModelCode, SENSOR_MODELCODE_PIR)) 
	    {
	        psRuleQ->ruleFreq = atoi(ppsRulesArray[colsRules+2]);
		if( !(psRuleQ->ruleMSG = STRDUP( ppsRulesArray[colsRules+1] ))) 
		{
		     APP_LOG("DEVICE:rule", LOG_ERR, "Memory Allocation Failed!");
		     resetSystem();
		}

	    }
	}
	else
	{
	    APP_LOG("DEVICE:rule", LOG_ERR, "No valid data");
	    /*free database buffer*/
	    WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);

	    APP_LOG("DEVICE:rule", LOG_DEBUG, "Exiting fn:makerulenode");
	    return bResult;
	}
	APP_LOG("DEVICE:rule", LOG_DEBUG, "RuleID:%d ruleTS:%lu",psRuleQ->notifyRuleID,psRuleQ->ruleTS);

	pNodeDevice = mxmlNewElement(pNode, "rule");

	pNoderule = mxmlNewElement(pNodeDevice, "ruleID");
	mxmlNewInteger(pNoderule, psRuleQ->notifyRuleID);

	if (0 == strcmp (device->ModelCode, SENSOR_MODELCODE_PIR)) 
        {
	    if (psRuleQ->ruleMSG)
	    {
	        pNoderule = mxmlNewElement(pNodeDevice, "ruleMsg");
	        mxmlNewCDATA (pNoderule,psRuleQ->ruleMSG);
            }

	    pNoderule = mxmlNewElement(pNodeDevice, "ruleFreq");
	    mxmlNewInteger(pNoderule, psRuleQ->ruleFreq);
        }

	pNoderule = mxmlNewElement(pNodeDevice, "ruleExecutionTS");
	mxmlNewInteger(pNoderule, psRuleQ->ruleTS);

	bResult = true;
    }
    else
    {
	APP_LOG("DEVICE:rule", LOG_ERR, "No valid data");
    }

    if (ppsRulesArray)
    { 
	/*free database buffer*/
	WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
    }

    if(psRuleQ)
    {
        APP_LOG("RULE:makerulenode", LOG_DEBUG, "Freeing ruleid: %d node", psRuleQ->ruleID)
        if (psRuleQ->ruleMSG)
	{
	    free(psRuleQ->ruleMSG);
	    psRuleQ->ruleMSG = NULL;
	}
	free(psRuleQ);
	psRuleQ = NULL;
    }

    APP_LOG("DEVICE:rule", LOG_DEBUG, "Exiting fn:makerulenode");
    return bResult;
}

void executeSensorRuleForLed(int ruleNo,int capabilityId)
{
      	int i=0, rowsRules=0,colsRules=0;
        char **ppsRulesArray=NULL;
        char query[SIZE_256B];
	char startAction[SIZE_8B],duration[SIZE_8B],endAction[SIZE_8B];
        const char* errorString = NULL;
        char prevCap[CAPABILITY_MAX_VALUE_LENGTH]={0};
        char capID[CAPABILITY_MAX_ID_LENGTH]={0};
        SD_DeviceID     sDevice;
        SD_CapabilityID sCapability = {SD_CAPABILITY_ID};
        char *devID =NULL;
	
	strncpy(sCapability.Data, "10006", sizeof(sCapability.Data));
	memset(startAction, 0, sizeof(startAction));
	memset(duration, 0, sizeof(duration));
	memset(endAction, 0, sizeof(endAction));
        memset(query, 0, sizeof(query));
        
	snprintf(query, sizeof(query), "SELECT DISTINCT DeviceID from RULEDEVICES where RuleID=\"%d\" AND DayID != -1",ruleNo);
        APP_LOG("DEVICE:rule", LOG_DEBUG, "query:%s", query);

        /*execute database query*/
        if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
        {
		/*check if we got the data*/
		if(rowsRules && colsRules)
		{
			for(i=0;i<rowsRules;i++)
			{
				if(!strstr(ppsRulesArray[i+1],"Bridge"))
					continue;
				if(capabilityId == FOBKEYPRESS)
				{
					devID = strrchr(ppsRulesArray[i+1],':');
					devID++;
					strncpy(sDevice.Data, devID, sizeof(sDevice.Data));
					if(strlen(devID) == GROUP_ID_LEN)
					{
						sDevice.Type = SD_GROUP_ID;
						SD_GetGroupCapabilityValueCache(&sDevice, &sCapability, prevCap, SD_CACHE);
					}
					else
					{
						sDevice.Type = SD_DEVICE_EUID;
						GetCacheDeviceOnOff(sDevice, capID, prevCap);
					}
					if(atoi(prevCap))
						snprintf(startAction, SIZE_8B, "%d",0);
					else
						snprintf(startAction, SIZE_8B, "%d",1);
					snprintf(endAction, SIZE_8B, "%d",0);
					snprintf(duration, SIZE_8B, "%d", 0);
					APP_LOG("DEVICE:rule", LOG_ERR, "Sensor rule data found targetUDN %s",ppsRulesArray[i+1]);
					bs_SetReaction(ppsRulesArray[i+1], startAction, duration, endAction, &errorString);
				}
				else if(SUCCESS == getSensorRuleData(ruleNo, ppsRulesArray[i+1], startAction, endAction, duration))
				{
					APP_LOG("DEVICE:rule", LOG_ERR, "Sensor rule data found targetUDNi %s startAction %s endAction %s duration %s",ppsRulesArray[i+1], startAction, endAction, duration);
					if(0 > atoi(duration))
						memset(duration, '\0', sizeof(duration));
					bs_SetReaction(ppsRulesArray[i+1], startAction, duration, endAction, &errorString);
				}
			}
		}
                else
                {
                        APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
                        /*free database buffer*/
                        WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
                }
        }
        else
        {
                APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
        }

exit_fn:
        /*free database buffer*/
        if(ppsRulesArray)
        {
                APP_LOG("DEVICE:rule", LOG_DEBUG, "Freeing DB table");
                WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
                ppsRulesArray = NULL;
        }

}
void enableRuleNotification(int ruleId)
{
    char **deviceIds = NULL;
    int numDevices = 0;
    int ret=0;
    int i=1;
    SD_DeviceID     sDevice;		
    char    value[CAPABILITY_MAX_VALUE_LENGTH] = {0};
    int     manualnotifystatus = 0;
    int     freq = 0;
    char    *targetId = NULL;
    int     isGroup = 0;


    APP_LOG("DEVICE:rule", LOG_DEBUG, "&deviceIds: %x, &numDevices: %x", &deviceIds, &numDevices);

    
    ret = prepareLinkDeviceList(ruleId, &deviceIds, &numDevices);

    if (ret)
    {
        APP_LOG("DEVICE:rule", LOG_ERR, "prepareLinkDeviceList failed!!");
	return FAILURE;
    }
    else
    {
	APP_LOG("DEVICE:rule", LOG_DEBUG, "deviceIds: %p, *deviceIds: %p", deviceIds, *deviceIds);
	for(i = 0;i < numDevices; i++)
	{
		APP_LOG("DEVICE:enableRuleNotification", LOG_DEBUG, "device: %s, i: %d, num: %d", deviceIds[i], i, numDevices);

		if(strstr(deviceIds[i], "Bridge") != NULL)
		{
			APP_LOG("DEVICE:enableRuleNotification", LOG_DEBUG, "--Bridge device--");
		}
		else
		{
			APP_LOG("DEVICE:enableRuleNotification", LOG_DEBUG, "Not Bridge device--");
			continue;
		}

		targetId = strrchr(deviceIds[i], ':');
		if (targetId)
		{
			targetId++;
			APP_LOG("DEVICE:enableRuleNotification", LOG_DEBUG, "target device: %s length %d ", targetId, strlen(targetId));
			if((strlen(targetId) != GROUP_ID_LEN) && (strlen(targetId) != DEVICE_ID_LEN)) {
				APP_LOG("DEVICE:enableRuleNotification", LOG_DEBUG, "Wrong groupID or wrong deviceID");
				continue;
			}
			if(strlen(targetId) == GROUP_ID_LEN)
				isGroup=1;
			else
				isGroup=0;

			APP_LOG("DEVICE:enableRuleNotification", LOG_DEBUG, "target device: %s, isGoup: %d", targetId, isGroup);
                        

			strncpy(sDevice.Data, targetId, sizeof(sDevice.Data));

			if(isGroup)
				sDevice.Type = SD_GROUP_ID;
			else
				sDevice.Type = SD_DEVICE_EUID;

			if (SD_GetCapabilityValue(&sDevice, &sCapabilitymanualnotify, value, SD_CACHE))
			{
				if (sscanf(value, "%d:%d", &manualnotifystatus, &freq))
				{
					APP_LOG("DEVICE:enableRuleNotification", LOG_DEBUG, "manualnotifystatus: %d for device:%s",manualnotifystatus,sDevice.Data);
					if (SENSOR_NOTIFY_STATUS == manualnotifystatus)
					{
						snprintf(value, sizeof(value), "0:%d", freq);
						SD_SetCapabilityValue(&sDevice, &sCapabilitymanualnotify, value, SE_LOCAL + SE_REMOTE);
						APP_LOG("DEVICE:enableRuleNotification", LOG_DEBUG, "Setting manualnotifystatus: %d",manualnotifystatus);
					}
				}
			}
		}
		else
		{
			continue;
		}

	}
    }
    for(i = 0;i < numDevices; i++)
    {
        if(deviceIds[i])
        {
            free(deviceIds[i]);
        }
    }

    if(deviceIds)
    {
        free(deviceIds);
    }

    APP_LOG("DEVICE:enableRuleNotification", LOG_DEBUG, "Exiting function enableRuleNotification");
}
void disableSensornNotifyRule(SD_Device device, int *p_manualNotifyStatus, int* p_freq)
{
    SRuleInfo* psRule=NULL;
    char **deviceIds = NULL;
    int numDevices = 0;
    int ret = FAILURE;
    int i = 1;
    char    *targetId = NULL;
    char    value[CAPABILITY_MAX_VALUE_LENGTH] = {0};
    int     manualnotifystatus = 0;
    int     freq = 0;

    APP_LOG("DEVICE:disableSensornNotifyRule", LOG_DEBUG, "Entering fn:disableSensornNotifyRule");
	
    if(0 != strcmp (device->ModelCode, SENSOR_MODELCODE_PIR))
    {
       APP_LOG("DEVICE:disableSensornNotifyRule", LOG_DEBUG, "Exiting:Sensor Notifications are disabled");
       return;
    }
    psRule = gpsRuleList;	
	
    if (gRuleHandle[e_NOTIFICATION_RULE].ruleCnt)
    {
        while(psRule != NULL)
	{
            if ((e_NOTIFICATION_RULE == psRule->ruleType) && (psRule->isActive))
	    {
                APP_LOG("DEVICE:disableSensornNotifyRule", LOG_DEBUG, "Rule ID %d is Active, Execute it!", psRule->ruleId);
                ret = prepareLinkDeviceList(psRule->ruleId, &deviceIds, &numDevices);
                
                if (ret)
                {
                    APP_LOG("DEVICE:disableSensornNotifyRule", LOG_ERR, "prepareLinkDeviceList failed!!");
                    return;
                }
                else
                {
                    APP_LOG("DEVICE:disableSensornNotifyRule", LOG_DEBUG, "deviceIds: %p, *deviceIds: %p", deviceIds, *deviceIds);
                    for(i = 0; i < numDevices; i++)
                    {
                        APP_LOG("DEVICE:disableSensornNotifyRule", LOG_DEBUG, "device: %s, i: %d, num: %d", deviceIds[i], i, numDevices);

                        if(strstr(deviceIds[i], "Bridge") != NULL)
                        {
                            APP_LOG("DEVICE:disableSensornNotifyRule", LOG_DEBUG, "--Bridge device--");
                        }
                        else
                        {
                            APP_LOG("DEVICE:disableSensornNotifyRule", LOG_DEBUG, "Not Bridge device--");
                            continue;
                        }

                        targetId = strrchr(deviceIds[i], ':');
                        if(targetId)
                        {
                            targetId++;
                            APP_LOG("DEVICE:disableSensornNotifyRule", LOG_DEBUG, "target device: %s length %d ", targetId, strlen(targetId));

                            if((strlen(targetId) != GROUP_ID_LEN) && (strlen(targetId) != DEVICE_ID_LEN))
                            {
                                 APP_LOG("DEVICE:disableSensornNotifyRule", LOG_DEBUG, "Wrong groupID or wrong deviceID");
                                 continue;
                            }

                            if (0 == strcmp(targetId, device->EUID.Data))
                            {
				    if (SD_GetCapabilityValue(&(device->EUID), &sCapabilitymanualnotify, value, SD_CACHE))
				    {
					    if (sscanf(value, "%d:%d", &manualnotifystatus, &freq))
					    {
					      if (SD_SENSOR_ENABLE == manualnotifystatus)
					      {
						   *p_manualNotifyStatus = SENSOR_NOTIFY_STATUS;
                                                   *p_freq = freq;
						   psRule->isActive = 0;
                                                   psRule->enablestatus = 0;
						   APP_LOG("DEVICE:disableSensornNotifyRule", LOG_DEBUG, "Rule:%d is deactivated:%d for device:%s",psRule->ruleId,psRule->isActive,targetId);
						   break;
					      }
					    }
				    }
			    }
			    else
			    {
				    continue;
			    }	
			}
		    }
		}
	    }
	    psRule = psRule->psNext;
	}
    }

    APP_LOG("DEVICE:rule", LOG_DEBUG, "Freeing device list");

    for(i = 0;i < numDevices; i++)
    {
        if(deviceIds[i])
        {
            free(deviceIds[i]);
        }
    }

    if(deviceIds)
    {
        free(deviceIds);
    }

    APP_LOG("DEVICE:disableSensornNotifyRule", LOG_DEBUG, "Exiting fn:disableSensornNotifyRule");
    return;
}
#endif

void runSensorRule(int *sensorRuleId,int count,char *sensorName,int capabilityId,int value)
{
		SRuleInfo *psRule = gpsRuleList;
#ifndef PRODUCT_WeMo_LEDLight
		char startAction[SIZE_8B],duration[SIZE_8B],endAction[SIZE_8B];
		char dummyArr[SIZE_100B];
        	memset(dummyArr,0x00,sizeof(dummyArr));
#endif
	int i=0;
	while(psRule != NULL)
		{
			for(i=0;i<count;i++) 	
				if(((e_SENSOR_RULE == psRule->ruleType) || (e_MAKERSENSOR_RULE == psRule->ruleType)) && (psRule->isActive) && (psRule->ruleId == sensorRuleId[i]))
				{
					if(isRuleIdApplicableForZb(psRule->ruleId,sensorName,capabilityId,value))
					{
#ifdef PRODUCT_WeMo_LEDLight
						executeSensorRuleForLed(psRule->ruleId,capabilityId);
#else
						memset(startAction, 0, sizeof(startAction));
                                                memset(duration, 0, sizeof(duration));
                                                memset(endAction, 0, sizeof(endAction));
						/*get sensor duration*/
						if(SUCCESS != getSensorRuleData(psRule->ruleId, g_szUDN_1, startAction, endAction, duration))
						{
							APP_LOG("DEVICE:rule", LOG_ERR, "Fetching sensor rule data failed");
							return;

						}
						APP_LOG("DEVICE:rule", LOG_ERR, "Sensor rule data found g_szUDN_1 %s startAction %s endAction %s duration %s",g_szUDN_1, startAction, endAction, duration);
						changeDeviceState(atoi(startAction),atoi(endAction),atoi(duration),dummyArr,dummyArr);
#endif
					}
				}
		psRule = psRule->psNext;	
		}
}

void getZbProductName(int* ruleNo, char** sensorName)
{
	int rowsRules=0,colsRules=0;
        char **ppsRulesArray=NULL;
        char query[SIZE_256B];
	memset(query, 0, sizeof(query));
	/*
	Sql query to fetch productName from RULEDEVICES table. Ruleid passed in this query is retrieved from DEVICECOMBINATION, 
	Using the SENSOR UDN(current notification is received from) and Device own UDN.
	*/
	snprintf(query, sizeof(query), "SELECT DISTINCT productName from RULEDEVICES where RuleID=\"%d\" AND DayID=-1",ruleNo[0]);
	APP_LOG("DEVICE:rule", LOG_DEBUG, "query:%s", query);

	/*execute database query*/
	if(!WeMoDBGetTableData(&g_RulesDB, query, &ppsRulesArray,&rowsRules,&colsRules))
	{
		/*check if we got the data*/
		if(rowsRules && colsRules)
		{
			*sensorName=strdup(ppsRulesArray[1]);
		}
		else
		{
			APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
			/*free database buffer*/
			WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
		}
	}
	else
	{
		APP_LOG("DEVICE:rule", LOG_ERR, "No target devices");
		goto exit_fn;
	}
exit_fn:
        /*free database buffer*/
        if(ppsRulesArray)
        {
                APP_LOG("DEVICE:rule", LOG_DEBUG, "Freeing DB table");
                WeMoDBTableFreeResult(&ppsRulesArray,&rowsRules,&colsRules);
		APP_LOG("DEVICE:rule", LOG_DEBUG, "Done");
                ppsRulesArray = NULL;
        }

}
/*      
* Check and run sensor/motion rule on given UDN is configured.
*/
void checkAndExecuteSensorRule(char* device_id, int capability_id, const char* value)
{
	int *ruleID;
	int count=0;
	long capVal=0;
	char *sensorName =NULL;
#ifdef PRODUCT_WeMo_LEDLight
	char sensorUDN[SIZE_256B];
	if(strstr(device_id,"Sensor") || strstr(device_id,"Maker"))
		snprintf(sensorUDN,SIZE_256B-1,"%s",device_id);
	else
		snprintf(sensorUDN,SIZE_256B-1,"%s:%s",g_szUDN_1,device_id);
	if(isSensorRuleForZBActive(sensorUDN,&ruleID,&count))
#else
	if(isSensorRuleForZBActive(device_id,&ruleID,&count))
#endif
	{
		/*If value is -1 set sensorName as sensor to handle backward compatibility for wemo motion*/ 
		if(atoi(value) < 0)
		{
			char *tmpStr = "Sensor";
			sensorName = MALLOC(strlen(tmpStr)+1);
			strncpy(sensorName,tmpStr,strlen(tmpStr));
		}
		else if(strstr(device_id,"Maker"))
		{
			sensorName = MALLOC(strlen(device_id)+1);
			strncpy(sensorName,device_id,strlen(device_id));
		}
		else
			getZbProductName(ruleID,&sensorName);
		if(sensorName)
		{
			/*zbCapabilityValue is a hexadecimal string for capabilityID 10500, Fetching last bit of this hex string to know sensor status(MOTION/NOMOTION)
                        */
			if(capability_id == SENSORCAPABILITY)
			{
				capVal = strtol(value, NULL, 16);
				capVal = (capVal & 0x1);
			}
			else
				capVal=atoi(value);
			
			APP_LOG("UPnPCtrPt", LOG_ERR, "Got sensor notification from %s UDN %s with capability value %lu",sensorName,device_id,capVal);
			runSensorRule(ruleID,count,sensorName,capability_id,capVal);
			free(sensorName);
		}
		if(ruleID)
			free(ruleID);
	}	
}
