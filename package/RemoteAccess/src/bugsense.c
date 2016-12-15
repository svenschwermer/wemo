/***************************************************************************
*
*
* bugsense.c
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
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "types.h"
#include "defines.h"
#include "osUtils.h"
#include "logger.h"
#include "global.h"
#include "utils.h"
#include "curl/curl.h"
#include "httpsWrapper.h"
#include "controlledevice.h"
#include "remoteAccess.h"
#include "wifiSetup.h"
#include "fw_rev.h"
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

typedef struct node
{
 char str[BUGSENSE_MAX_EVENT_LEN];
 struct node *link;
}SBugsenseNode;

static SBugsenseNode *header=NULL, *tail=NULL;
static pthread_mutex_t g_bugsense_mutex;
static UserAppSessionData *pUsrSsnData = NULL;
static UserAppData *pUserAppData = NULL;

void enqueueBugsense(char *s)
{
	return;

	SBugsenseNode *p = NULL;

	if(!s)
	{
		APP_LOG("BUGSENSE",LOG_ERR,"Invalid Event");
		return;
	}

	p=(SBugsenseNode *)CALLOC(1, sizeof(SBugsenseNode));

	if(!p)
	{
		APP_LOG("BUGSENSE",LOG_ERR,"Memory allocation failed");
		return;
	}

	strncpy(p->str, s, BUGSENSE_MAX_EVENT_LEN-1);
	p->link=NULL;

        osUtilsGetLock(&g_bugsense_mutex);
	if(header==NULL)
	{
		APP_LOG("BUGSENSE",LOG_DEBUG,"First node");
		header=p;
		tail=p;
	}
	else
	{
		tail->link = p;
		tail = p;
	}
    	osUtilsReleaseLock(&g_bugsense_mutex);
	APP_LOG("BUGSENSE",LOG_DEBUG,"Event: %s enqueue successful", s);
}

int dequeueBugsense(char *string)
{
	SBugsenseNode *p = NULL;

	if(!string)
	{
		APP_LOG("BUGSENSE",LOG_ERR,"Invalid Param");
		return FAILURE;
	}

        osUtilsGetLock(&g_bugsense_mutex);
	if(header==NULL)
	{
		//APP_LOG("BUGSENSE",LOG_DEBUG,"Queue empty");
		tail = NULL;
    		osUtilsReleaseLock(&g_bugsense_mutex);
		return FAILURE;
	}
	else
	{
		p=header;
		header=p->link;

		strncpy(string, p->str, BUGSENSE_MAX_EVENT_LEN-1);
		free(p);
    		osUtilsReleaseLock(&g_bugsense_mutex);
		return SUCCESS;
	}
}


int sendBugsense(char *str)
{
	int retVal = PLUGIN_FAILURE;
	char tmp_url[MAX_FW_URL_LEN];
	char uid[BUGSENSE_UID_LEN];
	char flat_line[MAX_FILE_LINE];
	char appVersion[5];

	pUserAppData = (UserAppData *)ZALLOC(sizeof(UserAppData));
	if (!pUserAppData) 
	{
		APP_LOG("REMOTEACCESS", LOG_ERR, "Malloc Failed for user App data");
		return FAILURE;
	}

	pUsrSsnData = webAppCreateSession(0);
	if(!pUsrSsnData)
	{
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "Websession Creation failed");
		free(pUserAppData);
		pUserAppData=NULL;
		return FAILURE;
	}
	else
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "Websession Created");

	/* Form the uid with device default friendly name  - MAC Address - Serial number */
    	memset(uid,0,sizeof(uid));
	snprintf(uid, BUGSENSE_UID_LEN, "WeMo-%s-%s", g_szWiFiMacAddress, g_szSerialNo);
	//APP_LOG("REMOTEACCESS", LOG_DEBUG, "uid: %s, len: %d", uid, strlen(uid));

    	memset(tmp_url,0,sizeof(tmp_url));
    	snprintf(tmp_url, sizeof(tmp_url), "https://ticks2.bugsense.com/api/ticks/%s/%s", BUGSENSE_API_KEY, uid);

	strncpy(pUserAppData->url, tmp_url, sizeof(pUserAppData->url)-1);
	strncpy( pUserAppData->keyVal[0].key, "Content-Type", sizeof(pUserAppData->keyVal[0].key)-1);   
	strncpy( pUserAppData->keyVal[0].value, "application/xml", sizeof(pUserAppData->keyVal[0].value)-1);   
	strncpy( pUserAppData->keyVal[1].key, "Accept", sizeof(pUserAppData->keyVal[1].key)-1);   
	strncpy( pUserAppData->keyVal[1].value, "application/xml", sizeof(pUserAppData->keyVal[1].value)-1);
	strncpy( pUserAppData->keyVal[2].key, "X-BugSense-Api-Key", sizeof(pUserAppData->keyVal[2].key)-1);   
	strncpy( pUserAppData->keyVal[2].value, BUGSENSE_API_KEY, sizeof(pUserAppData->keyVal[2].value)-1);
	
	pUserAppData->keyValLen = 3;

	/* prepare the data to be sent */
	/* data format: 1.0:eventTag:model:manufacturer:osVersion:appVersion:locale:utcTS  = has to be < 256 bytes */
	memset(appVersion, 0, sizeof(appVersion));
	strncpy(appVersion, FW_REV1+strlen(FW_REV1)-4, 4); 
	//APP_LOG("REMOTEACCESS", LOG_DEBUG, "App Version: %s", appVersion);

	memset(flat_line, 0, sizeof(flat_line));
	snprintf(flat_line, sizeof(flat_line)-1, "data=1.0:%s:%s:Belkin:1:%s:en-US:%u", str, getDefaultFriendlyName(), appVersion, (unsigned int)GetUTCTime());
	APP_LOG("REMOTEACCESS", LOG_HIDE, "Bugsense Data: %s, len: %d", flat_line, strlen(flat_line));

	//memset(pUserAppData->inData, 0, sizeof(pUserAppData->inData));
	strncpy( pUserAppData->inData, flat_line, sizeof(pUserAppData->inData)-1);
	pUserAppData->inDataLength = strlen(flat_line);

	pUserAppData->httpsFlag = 1;
	pUserAppData->disableFlag = 1;   
 
	APP_LOG("REMOTEACCESS", LOG_DEBUG, " **********Sending Bugsense Data");

	retVal = webAppSendData( pUsrSsnData, pUserAppData, 1);  
	if (retVal)
	{
		APP_LOG("REMOTEACCESS", LOG_ERR, "Some error encountered in send status to bugsense, errorCode %d", retVal);
		retVal = PLUGIN_FAILURE;
		goto on_return;
	}

	if( (strstr(pUserAppData->outHeader, "500")) || (strstr(pUserAppData->outHeader, "503")) ){
			APP_LOG("BUGSENSE", LOG_DEBUG, "Some error encountered: Cloud is not reachable");
			retVal = PLUGIN_FAILURE;
		}else if(strstr(pUserAppData->outHeader, "200")){
			retVal = SUCCESS;
			APP_LOG("BUGSENSE", LOG_DEBUG, "send status to cloud success");
		}else if(strstr(pUserAppData->outHeader, "403")){
			    APP_LOG("BUGSENSE", LOG_DEBUG, "########AUTH FAILURE (403): Not sending this Event ########");
		}else{
			APP_LOG("BUGSENSE", LOG_DEBUG, "Some error encountered: Error response from cloud");
		}
on_return:

	if (pUsrSsnData) {webAppDestroySession ( pUsrSsnData ); pUsrSsnData = NULL;}
	if (pUserAppData) {
		if (pUserAppData->outData) {free(pUserAppData->outData); pUserAppData->outData = NULL;}
		free(pUserAppData); pUserAppData = NULL;
	}

	return retVal;
}

void* dequeueBugsenseEvent(void *arg)
{
	char event[BUGSENSE_MAX_EVENT_LEN];
	int retVal; 

    	tu_set_my_thread_name( __FUNCTION__ );

	APP_LOG("BUGSENSE", LOG_DEBUG, "Bugsense Dequeue Thread Executing");
	while(1)
	{
		while(1)
		{
			if(getCurrentClientState() == STATE_CONNECTED)
			{
				break;
			}
			else
			{	
				pluginUsleep(10000000); //10 seconds
			}
		}

		memset(event, 0, BUGSENSE_MAX_EVENT_LEN);
		if(FAILURE == dequeueBugsense(event))
		{
			pluginUsleep(10000000); //10 seconds
			continue;
		}
		else
		{
			if(strlen(event))
			{
				APP_LOG("BUGSENSE", LOG_DEBUG, "Internet connected, going to post event: %s", event);
				retVal = sendBugsense(event);
				/* sleep a little before sending new event */
				pluginUsleep(1000000); //1 second
			}
		}
	}

	return NULL;
}
	
int createBugsenseDequeueThread()
{
	pthread_attr_t tmp_attr;
	pthread_t tmpthread;
	int retVal = FAILURE;

	pthread_attr_init(&tmp_attr);
	pthread_attr_setdetachstate(&tmp_attr,PTHREAD_CREATE_DETACHED);
	retVal = pthread_create(&tmpthread, &tmp_attr,
			(void*)dequeueBugsenseEvent, NULL);

	if(retVal < SUCCESS) 
	{
		APP_LOG("BUGSENSE", LOG_ERR, "Bugsense Dequeue Thread not created");
		return FAILURE;
	} 
	else
	{
		APP_LOG("BUGSENSE", LOG_DEBUG, "Bugsense Dequeue Thread created");
		return SUCCESS;
	}
}

void initBugsense(void)
{
    osUtilsCreateLock(&g_bugsense_mutex);
    createBugsenseDequeueThread();
}

