/***************************************************************************
*
*
* logger.c
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
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <malloc.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "types.h"
#include "defines.h"
#include "logger.h"
#include "osUtils.h"
#include "global.h"
#include "fw_rev.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "ulog.h"

#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

extern char *program_invocation_short_name;

#ifndef QA_BUILD

#ifdef PRODUCT_WeMo_LEDLight
int gloggerOptions = 33;
int gloggerLevel = 9;
#else
int gloggerOptions = 33;
int gloggerLevel = -1;
#endif
#else
#ifdef PRODUCT_WeMo_LEDLight
int gloggerOptions = 0;
int gloggerLevel = 6;
#else
int gloggerOptions = 0;
int gloggerLevel = -1;
#endif
#endif

#define PVT_LOG_ENABLE	"PVT_LOG_ENABLE"

#ifdef __HLOGS__
#define HIDDEN_LOGS	"CustLogLevel"
#define HIDDEN_LOGS_VAL	"OodNeBswQ"
char *gpHiddenLogs = NULL;
#endif

char g_buffTimeZoneOffset[SIZE_128B] = {0};
int gTimeZoneUpdated = 0;

char *gpLogEnable=NULL;
int lenIndex;
int logCounter;

//Always log in background
int gAppDaemon = 1;
void appLog(char *buff);

/*UPnP Log disabled by default*/
int gUPnPLogEnable = 0;

int loggerSetLogLevel (int lvl, int option) {
  gloggerLevel = lvl;
  gloggerOptions = option;
  return PLUGIN_SUCCESS;
}

int loggerGetLogLevel () {
	return gloggerLevel;
}

int get_file_size (const char * file_name)
{
		struct stat sb;
		if (stat (file_name, &sb) != 0) {
				APP_LOG("WATCHDOG", LOG_ERR, "stat failed for %s: %s.\n", file_name, strerror (errno));
				return PLUGIN_FAILURE;
		}
		return sb.st_size;
}

/*
 *  Function to set g_buffTimeZoneOffset value
 *
 ******************************************/

int setTimeZoneOffset (void)
{

	int DstVal=-2;
	float localTZ=0.0;

	char bufTemp[SIZE_32B];
	char bufTime[SIZE_32B];
	char *pch = NULL;
	int tz = 0;
	int tz1 = 0;

	char *LocalTimeZone = GetBelkinParameter(SYNCTIME_LASTTIMEZONE);
	if((LocalTimeZone != NULL) && (strlen(LocalTimeZone) != 0))
		localTZ = atof(LocalTimeZone);

	char *LastDstValue = GetBelkinParameter(LASTDSTENABLE);
	if((LastDstValue != NULL) && (strlen(LastDstValue) != 0))
		DstVal = atoi(LastDstValue);

	if(DstVal == 0)
		localTZ = localTZ + 1.0;

	gTimeZoneUpdated = 0;

	memset(g_buffTimeZoneOffset, 0x00, sizeof(g_buffTimeZoneOffset));
	memset(bufTemp, 0x0, SIZE_32B);
	memset(bufTime, 0x0, SIZE_32B);

	snprintf(bufTemp, sizeof(bufTemp), "%f|", localTZ);

	tz = atoi(bufTemp);
	pch = strstr (bufTemp,".");
	strncpy (bufTime,pch+1,2);
	tz1 = atoi(bufTime)*60/100;

	if (bufTemp[0x00] == '-')
		snprintf(g_buffTimeZoneOffset, sizeof(g_buffTimeZoneOffset), "%03d%02d",tz,tz1);
	else
		snprintf(g_buffTimeZoneOffset, sizeof(g_buffTimeZoneOffset), "+%02d%02d",tz,tz1);

	APP_LOG("WATCHDOG",LOG_DEBUG," ----g_buffTimeZoneOffset:%s\n",g_buffTimeZoneOffset);

	return SUCCESS;
}


void openFile( int flag)
{
	int DstVal=-2;
	float localTZ=0.0;
	time_t now_t;
	struct tm now;

	time(&now_t);
	localtime_r((time_t*)&now_t, &now);

	char *LocalTimeZone = GetBelkinParameter(SYNCTIME_LASTTIMEZONE);
	if((LocalTimeZone != NULL) && (strlen(LocalTimeZone) != 0))
	{
		localTZ = atof(LocalTimeZone);
	}

	char *LastDstValue = GetBelkinParameter(LASTDSTENABLE);
	if((LastDstValue != NULL) && (strlen(LastDstValue) != 0))
	{
		DstVal = atoi(LastDstValue);
	}
	APP_LOG("WATCHDOG",LOG_DEBUG," ---localTZ: %f,LocalTimeZone:%s, DSTVal: %d\n",localTZ,LocalTimeZone,!DstVal);
	if(DstVal == 0)
		localTZ = localTZ + 1.0;

	if(gTimeZoneUpdated || (strlen(g_buffTimeZoneOffset) == 0x00))
	{
		setTimeZoneOffset();
	}

	if(flag)
	{
		if (DstVal < 0) {
			syslog(LOG_DEBUG,"WDLOG: %04d-%02d-%02dT%02d:%02d:%02d%s|%d|TimeZone: %f,DST: NA,Ver: %s\n",now.tm_year+1900,now.tm_mon+1,now.tm_mday, now.tm_hour,now.tm_min,now.tm_sec,g_buffTimeZoneOffset,(int)syscall(SYS_gettid),localTZ,FW_REV);
		}else {
			syslog(LOG_DEBUG,"WDLOG: %04d-%02d-%02dT%02d:%02d:%02d%s|%d|TimeZone: %f,DST: %d,Ver: %s\n",now.tm_year+1900,now.tm_mon+1,now.tm_mday, now.tm_hour,now.tm_min,now.tm_sec,g_buffTimeZoneOffset,(int)syscall(SYS_gettid),localTZ,!DstVal,FW_REV);
		}
	}
	APP_LOG("WATCHDOG",LOG_DEBUG,"*************** TimeZone: %f, DST Enable Value: %d ***************\n",localTZ,!DstVal);
}

/************************************************************************
 * Function: writeLogs
 *	Write Logs to the RAM buffer or Flash file (wdLogFile.txt)
 * Parameters:
 *	severityLevel - Decides whether to write to the RAM/File
 *	eventType - The logging event type (BOOT, RT, WD)
 *	buff - String to be logged
 *
 *  rollOverBuffer - 2KB RAM buffer
 *
 *  NORMAL severity logs will be buffered in the RAM and will be flushed to flash once every 6 hours.
 *  The RAM buffer will be flushed also when something bad happens in the system viz. wemoApp exits or system reboots
 *
 *  CRITICAL severity logs will be flushed to flash immediately
 *
 **********************************************************************/
void writeLogs(severityLevel severity, char *eventType, char *buff)
{
    osUtilsGetLock(&logMutex);

    char buf[2*MAX_BUF_LEN];
    memset(buf, 0x0, 2*MAX_BUF_LEN);

    time_t now_t;
    struct tm now;

    time(&now_t);
    localtime_r((time_t*)&now_t, &now);

    if(gTimeZoneUpdated || (strlen(g_buffTimeZoneOffset) == 0x00))
    {
	    setTimeZoneOffset();
    }

    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d%s|",now.tm_year+1900,now.tm_mon+1,now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec, g_buffTimeZoneOffset);

    if(eventType)
	strncat(buf, eventType, sizeof(buf)-strlen(buf)-1);

    strncat(buf,"|", sizeof(buf)-strlen(buf)-1);

    if(buff)
	strncat(buf, buff, sizeof(buf)-strlen(buf)-1);
    strncat(buf, "\n", sizeof(buf)-strlen(buf)-1);

    syslog(LOG_DEBUG, "WDLOG: %s", buf);
    osUtilsReleaseLock(&logMutex);
}

/************************************************************************
 * Function: pluginLog
 *     Interface to be used by disfferent modules to write logs to syslog.
 *  Parameters:
 *     logIdentifier - mainly module name.
 *     logLevel - severity
 *     pLogStr - string to written to syslog
 *  Return:
 *     None
 ************************************************************************/
void pluginLog(char* logIdentifier, int logLevel, const char* pLogStr, ...) {
	char	buff[MAX_DATA_LEN];
	char	nbuff[MAX_DATA_LEN+SIZE_32B];
	char	nebuff[MAX_DATA_LEN+MAX_FILE_LINE];
	va_list	args;
	UINT8 cTime[SIZE_32B] = {};
	struct timeval tv;
	struct tm *now;

	memset(buff, 0x0, MAX_DATA_LEN);
	memset(nbuff, 0x0, MAX_DATA_LEN+SIZE_32B);

#ifdef __HLOGS__
	int hiddenSet = 0;
	if (logLevel == LOG_HIDE) {
  	gpHiddenLogs = GetBelkinParameter(HIDDEN_LOGS);
  	if ((0x00 != gpHiddenLogs) && (0x00 != strlen(gpHiddenLogs))) {
			if (!strncmp(gpHiddenLogs, HIDDEN_LOGS_VAL, strlen(HIDDEN_LOGS_VAL))) {
				hiddenSet = 1;
			}
		}

		if (hiddenSet) {
			logLevel = LOG_DEBUG;
		}
	}
#else
  if (logLevel == LOG_HIDE) logLevel = LOG_DEBUG;
#endif

	va_start(args, pLogStr);
	vsnprintf(buff, sizeof (buff), pLogStr, args);
	va_end(args);

	gettimeofday(&tv,NULL);
	now = localtime(&tv.tv_sec);

	snprintf((char*)cTime,sizeof(cTime),"[%04d-%02d-%02d][%02d:%02d:%02d.%06d]",now->tm_year+1900,now->tm_mon+1,now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec, (int)tv.tv_usec);
	strncpy(nbuff, (char*)cTime, sizeof(nbuff)-1);
	strncat(nbuff, (char*)buff, sizeof(nbuff)-strlen(nbuff)-1);
	if ((gloggerOptions == 0) && (gloggerLevel >= logLevel)) {
		
#ifdef PRODUCT_WeMo_LEDLight
        snprintf(nebuff, sizeof(nebuff), "%s[%d]:%s", logIdentifier, (int)syscall(SYS_gettid), nbuff);
//		DEBUG_LOG(ULOG_APP, UL_DEBUG, "%s", nebuff);
#else
        snprintf(nebuff, sizeof(nebuff), "%s[%d]:%s", logIdentifier, (int)syscall(SYS_gettid), nbuff);
		appLog(nebuff);
#endif
	}
	else {
		if (gloggerLevel >= logLevel) {
#ifdef PRODUCT_WeMo_LEDLight
			DEBUG_LOG(ULOG_APP, UL_DEBUG, "%s[%d:{%s}]:%s", program_invocation_short_name, (int)syscall(SYS_gettid),
								tu_get_my_thread_name(), nbuff);
#else
			syslog(logLevel, "%s[%d:{%s}]:%s\n", program_invocation_short_name, (int)syscall(SYS_gettid),tu_get_my_thread_name(),nbuff);
#endif
		}
	}
	va_end(args);
	return;
}

/**********************************************************************************
 * Function: wdLog
 *     Interface to be used by different modules to write logs to watchdog log file.
 *  Parameters:
 *     logLevel - severity
 *     pLogStr - string to written to wdLog
 *  Return:
 *     None
 *********************************************************************************/
void wdLog(int logLevel, const char* pLogStr, ...)
{
	char	buff[MAX_DATA_LEN];
	char	idBuff[SIZE_128B];
	va_list		args;

	memset(buff, 0x0, MAX_DATA_LEN);
	memset(idBuff, 0x0, SIZE_128B);

#ifdef __HLOGS__
	int hiddenSet = 0;
	if (logLevel >= LOG_HIDE) {
  gpHiddenLogs = GetBelkinParameter(HIDDEN_LOGS);
  if ((0x00 != gpHiddenLogs) && (0x00 != strlen(gpHiddenLogs))) {
		if (!strncmp(gpHiddenLogs, HIDDEN_LOGS_VAL, strlen(HIDDEN_LOGS_VAL))) {
				hiddenSet = 1;
		}
	}

	if (hiddenSet) logLevel -= LOG_HIDE;
	else return;
	}
#else
  if (logLevel >= LOG_HIDE) logLevel -= LOG_HIDE;
#endif

	va_start(args, pLogStr);
	vsnprintf(buff, sizeof (buff), pLogStr, args);
	va_end(args);

	snprintf(idBuff, sizeof(idBuff), "%s[%d:{%s}]:", program_invocation_short_name, (int)syscall(SYS_gettid),tu_get_my_thread_name());

	writeLogs(logLevel, (char *)idBuff,buff);

	va_end(args);
	return;
}

static FILE *gAppLogFp = NULL;
int gAppLogSizeMax = 0;
static char *gAppLogPath = NULL;
int gFileSizeFlag = 0;

int pluginOpenLog(char *BaseLogName,int MaxLogSize)
{
	int Ret = PLUGIN_SUCCESS;

	if(gAppLogPath != NULL) {
		free(gAppLogPath);
	}
	gAppLogPath = strdup(BaseLogName);
	gAppLogSizeMax = MaxLogSize;

	if(gAppLogFp != NULL)
	{
		fclose(gAppLogFp);
		gAppLogFp = NULL;
	}
	gFileSizeFlag = 0;

	return Ret;
}

void appLog(char *buff)
{
    char  Temp[SIZE_1024B];
    if(gAppLogFp != NULL) {
	if(ftell(gAppLogFp) > gAppLogSizeMax) {
	//Stop logging into the file if size exceeds the MAX size
	    gFileSizeFlag = 1;
	    return;
	}
    }

    if(gAppLogFp == NULL && gAppLogPath != NULL) {
	snprintf(Temp,sizeof(Temp),"%s",gAppLogPath);
	gAppLogFp = fopen(Temp,"w");
    }

    if(!gAppDaemon && isatty(fileno(stdin))) {
	printf("%s\n",buff);
    }

    if(gAppLogFp != NULL) {
	fprintf(gAppLogFp,"%s\n",buff);
	fflush(gAppLogFp);
    }

    return;
}

void pluginPrintBuf(char *title, char *buf, int len)
{
	int i;
	char *p, s[SIZE_512B];

	p = s;
	snprintf(s,sizeof(s),"%s ", title);
	p = s + strlen(s);
	for (i=0;i<len && i < 256;i++)
	{
		snprintf(p,(sizeof(s)-strlen(s)),"%02x ",((*buf++) & 0xff));
		p+=3;
	}
	APP_LOG("PLUGINApp", LOG_DEBUG, "%s\n",s);
}

void setLogLevel(void)
{
	char *loggerlevel = GetBelkinParameter(SYSLOGLEVEL);
	if (0x00 != loggerlevel && (0x00 != strlen(loggerlevel)))
	{
		gloggerLevel = atoi(loggerlevel);
		APP_LOG("UPNP", LOG_ALERT,"setting gloggerLevel to: %d from flash... success", gloggerLevel);
	}
	else
	{
		gloggerLevel = DEFAULT_LOG_LEVEL;
		char lvl[MAX_RES_LEN];
		memset(lvl, 0, sizeof(lvl));
		snprintf(lvl, sizeof(lvl), "%d", gloggerLevel);
		SetBelkinParameter(SYSLOGLEVEL, lvl);
		APP_LOG("UPNP", LOG_ALERT,"setting default gloggerLevel: %d", gloggerLevel);
		SaveSetting();
	}
	setlogmask(LOG_UPTO(gloggerLevel));
}

void initLogger(void)
{
	char *pUPnPLogEnable = NULL;
#ifdef DEBUG_ENABLE
	/*Open syslog for logging*/
	openlog(NULL, LOG_NDELAY|LOG_PERROR, LOG_USER);
#else   /* BOARD_TYPE: PVT */
	gpLogEnable = GetBelkinParameter (PVT_LOG_ENABLE);
	if ((0x00 != gpLogEnable) && (0x00 != strlen(gpLogEnable)))
	{
	    /*Open syslog for logging*/
	    openlog(NULL, LOG_NDELAY|LOG_PERROR, LOG_USER);
	}
#endif

	setLogLevel();
	/*check if UPnP prints need to be enabled*/
	pUPnPLogEnable = GetBelkinParameter("UPnPLogEnable");
	if(pUPnPLogEnable && strlen(pUPnPLogEnable))
	{
		gUPnPLogEnable = atoi(pUPnPLogEnable);
		APP_LOG("PLUGINApp", LOG_DEBUG, "UPnP Log Enabled:%d", gUPnPLogEnable);
	}
	else
		APP_LOG("PLUGINApp", LOG_DEBUG, "UPnP Log Disabled:%d", gUPnPLogEnable);
}

void deInitLogger(void)
{
	/*Close syslog*/
	closelog();
}
