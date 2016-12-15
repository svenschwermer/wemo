/***************************************************************************
 *
 *
 * DebugFrameWork.c
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
#include <syslog.h>
#include <stdio.h>
#include <ctype.h>
#include "logger.h"
#include "mxml.h"
#include "global.h"
#include "utils.h"
#include "httpsWrapper.h"
#include "wifiSetup.h"
#include "sigGen.h"
#include "natClient.h"
#include "remoteAccess.h"
#include "DebugFrameWork.h"
#include "utils.h"
#include "controlledevice.h"

#if defined(PRODUCT_WeMo_Insight)
#include "insightRemoteAccess.h"
#endif
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

struct Command *front=NULL, *rear=NULL;
unsigned int g_queue_counter=0;
unsigned int g_configChange=0;
pthread_mutex_t g_queue_mutex;
pthread_t consoleLogs_thread = -1;
pthread_attr_t consoleLogs_attr;
int g_cmd_type=CMD_NONE;
static UserAppSessionData *pUsrAppSsnData = NULL;
static UserAppData *pUsrAppData = NULL;
//static authSign *assign = NULL;
static char *sMac = NULL;

extern int gloggerOptions;
extern int gAppLogSizeMax;
extern int gFileSizeFlag;
extern char g_szWiFiMacAddress[SIZE_64B];
extern pthread_attr_t wdLog_attr;
extern pthread_t logFile_thread;

void resetLogLevel(int level)
{
    APP_LOG("REMOTE", LOG_ALERT, "logger level to set and save is: %d", level);
    char loggerlevel[MAX_RES_LEN];

    memset(loggerlevel, 0, sizeof(loggerlevel));
    snprintf(loggerlevel, sizeof(loggerlevel), "%d", level);
    SetBelkinParameter(SYSLOGLEVEL, loggerlevel);
    gloggerLevel = level;
    AsyncSaveData();

    setlogmask(LOG_UPTO(gloggerLevel));
}

void resetLogMask(void)
{
    char *loggerlevel = NULL;

    loggerlevel = GetBelkinParameter(SYSLOGLEVEL);
    if (0x00 != loggerlevel && (0x00 != strlen(loggerlevel)))
    {
	gloggerLevel = atoi(loggerlevel);
	setlogmask(LOG_UPTO(gloggerLevel));
    }
}

int command_to_index(char *p)
{

		if(!p)
				return CMD_NONE;
		if(!memcmp(p,"CMD_ICE",strlen("CMD_ICE")) )
				return CMD_ICE;
		else if(!memcmp(p,"CMD_RELAY",strlen("CMD_RELAY")))
				return CMD_RELAY;
		else if(!memcmp(p,"CMD_REGISTER",strlen("CMD_REGISTER")))
				return CMD_REGISTER;
		else if(!memcmp(p,"CMD_NAT",strlen("CMD_NAT")))
				return CMD_NAT;
		else if(!memcmp(p,"CMD_SYSTEM",strlen("CMD_SYSTEM")))
				return CMD_SYSTEM;
		else if(!memcmp(p,"CMD_CONSOLELOGS",strlen("CMD_CONSOLELOGS")))
				return CMD_CONSOLELOGS;
		else if(!memcmp(p,"CMD_LOGLEVEL",strlen("CMD_LOGLEVEL")))
				return CMD_LOGLEVEL;
		else if(!memcmp(p,"CMD_LOGUPLOAD",strlen("CMD_LOGUPLOAD")))
				return CMD_LOGUPLOAD;

		return CMD_NONE;

}
void DespatchSendNotificationResp(void *sendNfResp)
{
		char *snRespXml = NULL;
		mxml_node_t *tree;
		mxml_node_t *find_node;
		mxml_node_t *child_cmd;
		int counter=0;
		struct Command* temp=NULL;

		snRespXml = (char*)sendNfResp;
		APP_LOG("SNS", LOG_DEBUG, "XML received in response of deviceConfig is %s",snRespXml);
		tree = mxmlLoadString(NULL, snRespXml, MXML_OPAQUE_CALLBACK);
		if (tree){
				find_node = mxmlFindElement(tree, tree, "commandCount", NULL, NULL, MXML_DESCEND);
				if(find_node){
						if (find_node && find_node->child) {
								APP_LOG("SNS", LOG_DEBUG, "The commandCount value is  %s\n", find_node->child->value.opaque);
								counter=atoi(find_node->child->value.opaque);
						}
				}else{
						goto on_return;
				}
				while(counter--)
				{
						APP_LOG("SNS", LOG_DEBUG, "XML String Loaded is %s\n", snRespXml);
						temp=(struct Command*)MALLOC(sizeof(struct Command));

						child_cmd=mxmlFindElement(tree, tree, "command", NULL, NULL, MXML_DESCEND);	
						if(child_cmd){

								find_node = mxmlFindElement(child_cmd, child_cmd, "name", NULL, NULL, MXML_DESCEND);
								if(find_node){
										if (find_node && find_node->child) {
												temp->Info.index=command_to_index(find_node->child->value.opaque);
												APP_LOG("SNS", LOG_DEBUG, "The command value is  %s index %d\n", find_node->child->value.opaque,temp->Info.index);
										}
								}else{
										goto on_return;
								}
								find_node = mxmlFindElement(tree, tree, "value", NULL, NULL, MXML_DESCEND);
								if(find_node){
										if (find_node && find_node->child) {
												APP_LOG("SNS", LOG_DEBUG, "The value  is %s\n", find_node->child->value.opaque);
												strcpy(temp->Info.cmd_value,find_node->child->value.opaque);
										}
								}else{
										goto on_return;
								}
								find_node = mxmlFindElement(tree, tree, "length", NULL, NULL, MXML_DESCEND);
								if(find_node){
										if (find_node && find_node->child) {
												APP_LOG("SNS", LOG_DEBUG, "The length   is %s\n", find_node->child->value.opaque);
												temp->Info.cmd_length=atoi(find_node->child->value.opaque);
										}
								}else{
										goto on_return;
								}
						}
						mxmlDelete(child_cmd);
						enqueue(temp);/* Adding Elements in Queue */
						free(temp);
						temp=NULL;
				}/* End of While */
		}else {
				goto on_return;
		}
		mxmlDelete(tree);
		return;
on_return:
		mxmlDelete(tree);
}

void updateResponseXML(char *httpPostStatus, char *str, int len)
{
		//int timeStamp = (int) GetUTCTime();

		snprintf(httpPostStatus, len, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><logging><macAddress>%s</macAddress><udId>Serial:%s</udId><loggingDetailsTS></loggingDetailsTS><message>CMD_RES:%s</message></logging>", \
						sMac, GetSerialNumber(), str);

}

int processLogUploadCmd(char* cmd)
{
    if(!cmd)
    {
	APP_LOG("DGB", LOG_DEBUG,"command value error");
	return FAILURE;
    }

    int enable = atoi(cmd);

    if(enable)
    {			
	if(logFile_thread == -1)
	{
	    APP_LOG("DGB", LOG_DEBUG,"Start uploading log files from remote");
	    pthread_attr_init(&wdLog_attr);
	    pthread_attr_setdetachstate(&wdLog_attr, PTHREAD_CREATE_DETACHED);
	    pthread_create(&logFile_thread, &wdLog_attr, uploadLogFileThread, NULL);
	    SetBelkinParameter(LOG_UPLOAD_ENABLE, "1");
	    AsyncSaveData();
	}
	else
	    APP_LOG("DGB", LOG_DEBUG,"File upload thread already running");
    }
    else
    {			
	if(logFile_thread != -1)
	{
	    APP_LOG("DGB", LOG_DEBUG,"Stopping messages file uploads from remote");
	    pthread_cancel(logFile_thread);
	    logFile_thread=-1;
	    SetBelkinParameter(LOG_UPLOAD_ENABLE, "0");
	    AsyncSaveData();
	}
	else
	    APP_LOG("DGB", LOG_DEBUG,"messages file uploads not running");
    }

    return SUCCESS;
}

int postCommandResponse(char *resp)
{
		char *httpPostData = NULL;
		int retVal = PLUGIN_SUCCESS;
    static authSign *assign = NULL;

		/* Drop the response in case there is no Internet connection - Rare case */	
		if (getCurrentClientState() != STATE_CONNECTED) {
				free(resp);
				return PLUGIN_FAILURE;
		}

		httpPostData = (char *)MALLOC(strlen(resp) + MAX_FILE_LINE);
		if (!httpPostData) {
				APP_LOG("REMOTEACCESS", LOG_ERR, "\n Allocating data returned NULL\n");
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}

		pUsrAppData = (UserAppData *)MALLOC(sizeof(UserAppData));
		if (!pUsrAppData) {
				APP_LOG("REMOTEACCESS", LOG_ERR, "\n Malloc Failed\n");
				goto on_return;
		}

		sMac = utilsRemDelimitStr(GetMACAddress(), ":");
		assign = createAuthSignature(sMac, GetSerialNumber(), g_szPluginPrivatekey);
		if (!assign) {
				APP_LOG("REMOTEACCESS", LOG_ERR, "\n Signature Structure returned NULL\n");
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}

		memset(httpPostData, 0x0, strlen(resp) + MAX_FILE_LINE);
		updateResponseXML(httpPostData, resp, strlen(resp) + MAX_FILE_LINE);
		free(resp);
		resp=NULL;

		APP_LOG("REMOTEACCESS", LOG_HIDE, "httpPostData=%s len=%d\n",httpPostData, strlen(httpPostData));
		memset( pUsrAppData, 0x0, sizeof(UserAppData));

		pUsrAppSsnData = webAppCreateSession(0);
		if(!pUsrAppSsnData)
		{
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "Websession Creation failed");
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}
		else
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "Websession Created\n");


		//used for testing - strncpy(pUsrAppData->url, "http://10.20.90.160:8080/PluginService8262/plugin/logging", sizeof(pUsrAppData->url)-1);
		snprintf( pUsrAppData->url, sizeof(pUsrAppData->url), "https://%s:8443/apis/http/plugin/logging", BL_DOMAIN_NM);
		strncpy( pUsrAppData->keyVal[0].key, "Content-Type", sizeof(pUsrAppData->keyVal[0].key)-1);   
		strncpy( pUsrAppData->keyVal[0].value, "application/xml", sizeof(pUsrAppData->keyVal[0].value)-1);   
		strncpy( pUsrAppData->keyVal[1].key, "Accept", sizeof(pUsrAppData->keyVal[1].key)-1);   
		strncpy( pUsrAppData->keyVal[1].value, "application/xml", sizeof(pUsrAppData->keyVal[1].value)-1);   
		strncpy( pUsrAppData->keyVal[2].key, "Authorization", sizeof(pUsrAppData->keyVal[2].key)-1);   
		strncpy( pUsrAppData->keyVal[2].value, assign->signature, sizeof(pUsrAppData->keyVal[2].value)-1);   
		strncpy( pUsrAppData->keyVal[3].key, "X-Belkin-Client-Type-Id", sizeof(pUsrAppData->keyVal[3].key)-1);   
		strncpy( pUsrAppData->keyVal[3].value, g_szClientType, sizeof(pUsrAppData->keyVal[3].value)-1);   
		pUsrAppData->keyValLen = 4;

		strncpy( pUsrAppData->inData, httpPostData, sizeof(pUsrAppData->inData)-1);
		pUsrAppData->inDataLength = strlen(httpPostData);
		char *check = strstr(pUsrAppData->url, "https://");
		if (check) {
				pUsrAppData->httpsFlag = 1;
		}
		pUsrAppData->disableFlag = 1;    
		APP_LOG("REMOTEACCESS", LOG_DEBUG, " **********Sending Cloud Response XML\n");

		retVal = webAppSendData( pUsrAppSsnData, pUsrAppData, 1);  
		if (retVal)
		{
				APP_LOG("REMOTEACCESS", LOG_ERR, "\n Some error encountered in send status to cloud  , errorCode %d \n", retVal);
				retVal = PLUGIN_FAILURE;
				goto on_return;
		}

		if( (strstr(pUsrAppData->outHeader, "500")) || (strstr(pUsrAppData->outHeader, "503")) ){
				APP_LOG("REMOTEACCESS", LOG_ERR, "Cloud is not reachable, error: 500 or 503");
				retVal = PLUGIN_FAILURE;
		}else if(strstr(pUsrAppData->outHeader, "200")){
				APP_LOG("REMOTEACESS", LOG_DEBUG, "send status to cloud success");
		}else if(strstr(pUsrAppData->outHeader, "403")){
				APP_LOG("REMOTEACCESS", LOG_ERR, "Cloud error: 403");
		}else{
				APP_LOG("REMOTEACESS", LOG_DEBUG, "Some error encountered: Error response from cloud");
				retVal = PLUGIN_FAILURE;
		}

on_return:
		if (resp) { free(resp); resp = NULL; }
		if (httpPostData) { free(httpPostData); httpPostData= NULL;}
		if (pUsrAppSsnData) {webAppDestroySession ( pUsrAppSsnData ); pUsrAppSsnData = NULL;}
		if (sMac) {free(sMac); sMac = NULL;}
		if (assign) {free(assign); assign = NULL;};
		if (pUsrAppData) {
				if (pUsrAppData->outData) {free(pUsrAppData->outData); pUsrAppData->outData = NULL;}
				free(pUsrAppData); pUsrAppData = NULL;
		}

		return retVal;
}

#define SIZE_4K	(4*SIZE_1024B)

int process_syscmd(char* cmd)
{
		int ret=0;
		char command[SIZE_256B];

		if(cmd)
		{
#if defined(PRODUCT_WeMo_Insight)
				char InsightParams[SIZE_256B];
				if(strstr(cmd,"get insight"))
				{
						FetchInsightParams(InsightParams);
						memset(command, 0, sizeof(command));
						snprintf(command,sizeof(command),"echo \"%s\" > /tmp/cmdres 2>&1",InsightParams);
						ret = system(command);	
						APP_LOG("SNS", LOG_DEBUG,"Processed system command for INSIGHT: %s, retval: %d",command, ret);
				}
				else
#endif
				{
						memset(command, 0, sizeof(command));
						strncpy(command, cmd, SIZE_256B);
						strncat(command, " > /tmp/cmdres 2>&1",sizeof(SIZE_256B) - strlen(command) - 1);
						ret = system(command);	
						APP_LOG("SNS", LOG_DEBUG,"Processed system command: %s, retval: %d",command, ret);
						if(strstr(cmd, SYSLOGLEVEL))
						{
						    APP_LOG("SNS", LOG_ALERT, "reset syslog mask");
						    resetLogMask();
						}
				}
				/* send the response */
				FILE *fp = fopen("/tmp/cmdres", "r");
				if(!fp)
				{
						APP_LOG("SNS", LOG_DEBUG,"No output from system command");
						return FAILURE;
				}
				else
				{
						long fsize = 0;
						char *string = NULL;

						fseek(fp, 0, SEEK_END);
						fsize = ftell(fp);
						APP_LOG("SNS", LOG_DEBUG,"fsize: %d", fsize);

						if(fsize)
						{
								/* send the file only it is sized non-zero */
								fseek(fp, 0, SEEK_SET);

								/* allocate no more than SIZE_4KB */
								if(fsize+1  > SIZE_4K)
										fsize = (SIZE_4K -1);

								string = ZALLOC(fsize + 1);

								if(string)
								{
										fread(string, fsize, 1, fp);
										string[fsize] = 0;
										APP_LOG("SNS", LOG_DEBUG,"Command response[%d bytes]: %s", fsize+1, string);
										postCommandResponse(string);
								}
								else
								{
										APP_LOG("SNS", LOG_ERR,"Could not allocate %d bytes", fsize+1);
								}
						}
						else
								APP_LOG("SNS", LOG_DEBUG,"Not sending command response");

						fclose(fp);
						/* remove the temp file */
						system("rm -f  /tmp/cmdres");
				}
		}
		return SUCCESS;
}


void process_command(struct CommandInfo  *p)
{
		APP_LOG("SNS", LOG_DEBUG,"Cmmand Index is received as %d\n",p->index);
		APP_LOG("SNS", LOG_DEBUG,"Cmmand Value is received as %s\n",p->cmd_value);
		char nat_type[SIZE_2B];
		switch(p->index)
		{
				case CMD_ICE:
						APP_LOG("SNS", LOG_DEBUG,"CMD_ICE Command  triggered\n");
						memset(nat_type,0,sizeof(nat_type));
						snprintf(nat_type,sizeof(nat_type),"%d",CMD_ICE);
						SetBelkinParameter(NAT_TYPE,nat_type);
						AsyncSaveData();
						UDS_triggerNat();
						break;
				case CMD_RELAY:
						APP_LOG("SNS", LOG_DEBUG,"CMD_RELAY  Command triggered \n");
						memset(nat_type,0,sizeof(nat_type));
						snprintf(nat_type,sizeof(nat_type),"%d",CMD_RELAY);
						SetBelkinParameter(NAT_TYPE,nat_type);
						AsyncSaveData();
						UDS_triggerNat();
						break;
				case CMD_REGISTER:
						APP_LOG("SNS", LOG_DEBUG,"CMD_REGISTER Command triggered \n");
						reRegisterDevice();
						break;
				case CMD_NAT:
						APP_LOG("SNS", LOG_DEBUG,"CMD_NAT Command triggered \n");
						UDS_triggerNat();
						break;
				case CMD_SYSTEM:
						APP_LOG("DGB", LOG_DEBUG,"CMD_SYSTEM Command triggered \n");
						process_syscmd(p->cmd_value);
						break;
				case CMD_CONSOLELOGS:
						APP_LOG("DGB", LOG_DEBUG,"CMD_CONSOLELOGS Command triggered \n");
						trigger_logging(p->cmd_value);
						break;
				case CMD_LOGLEVEL:
						APP_LOG("SNS", LOG_DEBUG,"CMD_LOGLEVEL Command triggered \n");
						resetLogLevel(atoi(p->cmd_value));
						break;
				case CMD_LOGUPLOAD:
						APP_LOG("DGB", LOG_DEBUG,"CMD_LOGUPLOAD Command triggered \n");
						processLogUploadCmd(p->cmd_value);
						break;
				default:
						APP_LOG("SNS", LOG_ERR,"No Valid Command \n");
		}
}

struct CommandInfo  *dequeue()
{
		struct Command *p;
		unsigned int flag=0;
		if (front == NULL || rear == NULL)
		{
				//APP_LOG("SNS", LOG_DEBUG," Queue is empty \n");
				sleep(SLEEP_SEC_DEQUEUE);
				return NULL;
		}
		else
		{
				APP_LOG("SNS", LOG_DEBUG,"  element is going to be popped out 4m  Q\n");
				pthread_mutex_lock(&g_queue_mutex);
				if(g_queue_counter==1)
						flag=1;
				g_queue_counter--;
				pthread_mutex_unlock(&g_queue_mutex);
				/* Popping Out the Front Node */
				if(flag) 
				{
						pthread_mutex_lock(&g_queue_mutex);
				}
				p = front;
				front = front->next;
				if( flag )
				{
						pthread_mutex_unlock(&g_queue_mutex);
				}
				return &(p->Info);
		}
}
void* rcvSendstatusRspThread(void *arg)
{
		struct CommandInfo  *p=NULL;
    
		tu_set_my_thread_name( __FUNCTION__ );

		while(1)
		{
				p=dequeue();
				if(!p) continue;
				/*Process Command received */
				process_command(p);
				free(p);
				p=NULL;
				sleep(SLEEP_SEC_DEQUEUE);
		}
		return p;
}

void enqueue(struct Command *tmp)
{
		struct Command *p =NULL;
		unsigned int flag =0;
		p = (struct Command*)MALLOC(sizeof(struct Command));
                APP_LOG("SNS", LOG_DEBUG," enqueue entered \n");
		memcpy(&(p->Info.index),&(tmp->Info.index),sizeof(int));
		memcpy(p->Info.cmd_value,tmp->Info.cmd_value,SIZE_256B*sizeof(char));
		memcpy(&(p->Info.cmd_length),&(tmp->Info.cmd_length),sizeof(int));
		p->next = NULL;

		pthread_mutex_lock(&g_queue_mutex);
		if(g_queue_counter ==1)
		{
				flag=1;
				APP_LOG("SNS", LOG_DEBUG," Only one variable \n");
				/* Set the local variable */
		}
		g_queue_counter++;
		pthread_mutex_unlock(&g_queue_mutex);

		if(flag)	
		{
				pthread_mutex_lock(&g_queue_mutex);
		}
		if (rear == NULL || front == NULL)
				front =rear= p;
		else
		{
				APP_LOG("SNS", LOG_DEBUG,"  element is added in Q\n");
				rear->next = p;
				rear = p;
		}
		if(flag)	
				pthread_mutex_unlock(&g_queue_mutex);
		return ;
}

void initCommandQueueLock()
{
		pthread_mutex_init(&g_queue_mutex, NULL);
}

static int parseCmd(char *cmd, int *time, int *size, int *timed)
{
		int ret = 0;

		if(cmd) {
				APP_LOG("SNS", LOG_DEBUG,"  CMD_CONSOLELOGS value is %s\n", cmd);
				ret = sscanf(cmd, "time=%d,size=%d",time,size);
				APP_LOG("SNS", LOG_DEBUG,"  CMD_CONSOLELOGS value parsed is time %d size %d ret %d from cmd :%s:\n",\
						*time, *size, ret, cmd);

				if (*size == 99999) {
						*time = CONSOLE_LOGS_TIME;
						*size = CONSOLE_LOGS_SIZE;
						*timed = 1;
				}else {
						*timed = 0;
						if(*time <= 0 || *time > CONSOLE_LOGS_TIME)
								*time = CONSOLE_LOGS_TIME;

						if(*size <= 0 || *size > CONSOLE_LOGS_SIZE)
								*size = CONSOLE_LOGS_SIZE;
				}
				ret = 0;
		} else {
				*time = CONSOLE_LOGS_TIME;
				*size = CONSOLE_LOGS_SIZE;
				*timed = 0;
				ret = 1; //Failure
		}   
		return ret;
}

void *consoleLogginginFile(void *args)
{
		ConsoleLogsInfo *pConLogs = NULL;
		int preservePrevLoggerOption = gloggerOptions;
		struct stat fileInfo;
		char buf[SIZE_256B];
		char tmp_url[MAX_FW_URL_LEN];
		int fileExists = 1, ret = 0, i=0;
		char *string = NULL;

    		tu_set_my_thread_name( __FUNCTION__ );

		if(args) {
				pConLogs = (ConsoleLogsInfo *)args;
		}

		pluginOpenLog (PLUGIN_LOGS_FILE, CONSOLE_LOGS_SIZE);
		APP_LOG("SNS", LOG_DEBUG, "Sleep for %d secs", pConLogs->timePeriod * DELAY_60SEC);

		gloggerOptions = 0; //This will start logging in the file
		gAppLogSizeMax = pConLogs->fileSize;

		for(i=0; i < (pConLogs->timePeriod)*3; i++) {
				sleep(DELAY_20SEC);
				//File size exceeds. Stop logging and post the file
				if(gFileSizeFlag)
						break;
		}

		gFileSizeFlag = 0;

		if(pConLogs)
				free(pConLogs);

		gloggerOptions = preservePrevLoggerOption;

		memset(buf, 0x0, SIZE_256B);
		memset(tmp_url, 0x0, MAX_FW_URL_LEN);
		memset(&fileInfo, 0x0, sizeof(struct stat));

		/* Checking for the existence of the console log files.
		 * If the file exists, then upload the contents to cloud
		 */
		if(stat(PLUGIN_LOGS_FILE, &fileInfo) == -1 && errno == ENOENT) {
				fileExists = 0;
				APP_LOG("SNS", LOG_DEBUG, "Plugin Log file doesn't exists");
				consoleLogs_thread = -1;
				pthread_exit(0);
				return NULL;
		}

		snprintf(tmp_url, sizeof(tmp_url), "http://%s:%s/%s/", BL_DOMAIN_NM_WD, BL_DOMAIN_PORT_WD, BL_DOMAIN_URL_PLUGIN);
		strncat(tmp_url, g_szWiFiMacAddress, sizeof(tmp_url)-strlen(tmp_url)-1);

		/* loop until we find Internet connected */
		while(1)
		{
				if (getCurrentClientState() == STATE_CONNECTED)
						break;
				pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);  //30 sec
		}

		if (fileExists) {
				ret = uploadLogFileDevStatus(PLUGIN_LOGS_FILE, tmp_url);
		}

		if(ret < 0) {
				APP_LOG("SNS", LOG_DEBUG, "Upload Log File status fail");
		} else {
				APP_LOG("SNS", LOG_DEBUG, "Upload Log File status success...");
				//Clearing the log files if Uploaded successfully.
				snprintf(buf, sizeof(buf), "rm %s", PLUGIN_LOGS_FILE);
				system(buf);

				//Send command response
				string = ZALLOC(MAX_RESP_LEN);

				if(string)
				{
						snprintf(string, MAX_RESP_LEN, "Plugin Log file posted to URL: %s", tmp_url);
						APP_LOG("SNS", LOG_DEBUG,"Command response[%d bytes]: %s", MAX_RESP_LEN, string);
						postCommandResponse(string);
				}
				else
				{
						APP_LOG("SNS", LOG_ERR,"Could not allocate %d bytes", MAX_RESP_LEN);
				}
		}

		APP_LOG("SNS",LOG_DEBUG, "***************EXITING PLUGIN LOG Thread***************\n");
		consoleLogs_thread = -1;
		pthread_exit(0);
		return NULL;
}

void *consoleLogginginFile_timed(void *args)
{
		ConsoleLogsInfo *pConLogs = NULL;
		int preservePrevLoggerOption = gloggerOptions;
		struct stat fileInfo;
		char buf[SIZE_256B];
		char bufn[SIZE_128B];
		char tmp_url[MAX_FW_URL_LEN];
		int fileExists = 1, ret = 0, i=0;
		int fileCounter = 0;
		char *string = NULL;

    		tu_set_my_thread_name( __FUNCTION__ );

		if(args) {
				pConLogs = (ConsoleLogsInfo *)args;
		}

		APP_LOG("SNS", LOG_DEBUG, "Run for %d secs", pConLogs->timePeriod * DELAY_60SEC);
		memset(bufn, 0x0, SIZE_128B);
		snprintf(bufn, sizeof(bufn), "%s%d", PLUGIN_LOGS_FILE,fileCounter);
		pluginOpenLog (bufn, CONSOLE_LOGS_SIZE);
				
		gloggerOptions = 0; //This will start logging in the file
		gAppLogSizeMax = pConLogs->fileSize;

		memset(tmp_url, 0x0, MAX_FW_URL_LEN);
		snprintf(tmp_url, sizeof(tmp_url), "http://%s:%s/%s/", BL_DOMAIN_NM_WD, BL_DOMAIN_PORT_WD, BL_DOMAIN_URL_PLUGIN);
		strncat(tmp_url, g_szWiFiMacAddress, sizeof(tmp_url)-strlen(tmp_url)-1);

		for(i=0; i < (pConLogs->timePeriod)*20; i++) {
				sleep(DELAY_3SEC);
				/* Checking for the existence of the console log files.
				 * If the file exists, then upload the contents to cloud
				 */
				memset(&fileInfo, 0x0, sizeof(struct stat));
				if((stat(bufn, &fileInfo) == -1) && (errno == ENOENT)) {
						fileExists = 0;
						APP_LOG("SNS", LOG_DEBUG, "Plugin Log file %s doesn't exist", bufn);
				}else {
						fileExists = 1;
				}
				if((gFileSizeFlag) && (fileExists == 1)) {
						APP_LOG("SNS", LOG_DEBUG, "Log reached set limit %d", gFileSizeFlag);
						if (getCurrentClientState() == STATE_CONNECTED) {
								APP_LOG("SNS", LOG_DEBUG, "Internet connected, going to upload file %s to url %s", bufn, tmp_url);
								ret = uploadLogFileDevStatus(bufn, tmp_url);
						}
						
						memset(buf, 0x0, SIZE_256B);
						snprintf(buf, sizeof(buf), "rm %s", bufn);
						system(buf);

						gFileSizeFlag = 0;

						memset(bufn, 0x0, SIZE_128B);
						snprintf(bufn, sizeof(bufn), "%s%d", PLUGIN_LOGS_FILE, ++fileCounter);
						pluginOpenLog (bufn, CONSOLE_LOGS_SIZE);
				}
		}

		gFileSizeFlag = 0;
		gloggerOptions = preservePrevLoggerOption;

		if(pConLogs)
				free(pConLogs);
		
		memset(&fileInfo, 0x0, sizeof(struct stat));
		if((!(stat(bufn, &fileInfo)))) {
				APP_LOG("SNS", LOG_DEBUG, "Remove last plugin Log file %s if exists", bufn);
				memset(buf, 0x0, SIZE_256B);
				snprintf(buf, sizeof(buf), "rm %s", bufn);
				system(buf);
		}
		

		//Send command response
		string = ZALLOC(MAX_RESP_LEN);

				snprintf(string, MAX_RESP_LEN, "Plugin Log file posted to URL: %s", tmp_url);
				APP_LOG("SNS", LOG_DEBUG,"Command response[%d bytes]: %s", MAX_RESP_LEN, string);
				postCommandResponse(string);

		APP_LOG("SNS",LOG_DEBUG, "***************EXITING PLUGIN LOG Thread***************\n");
		consoleLogs_thread = -1;
		pthread_exit(0);
		return NULL;
}

void trigger_logging(char *cmd)
{
		int timeLimit = CONSOLE_LOGS_TIME, sizeLimit = CONSOLE_LOGS_SIZE, timed = 0;
		ConsoleLogsInfo *pConLogs = NULL;

		if (-1 != consoleLogs_thread)
		{    
				APP_LOG("UPNP: Device", LOG_ERR, "############Plugin LOG thread already created################");
				return;
		}

		parseCmd(cmd, &timeLimit, &sizeLimit, &timed);
		APP_LOG("SNS", LOG_DEBUG,"  CMD_CONSOLELOGS value parsed is time %d size %d istimed %d\n",\
						timeLimit, sizeLimit, timed);

		pConLogs = (ConsoleLogsInfo *)MALLOC(sizeof(ConsoleLogsInfo));
		if(!pConLogs) {
				APP_LOG("SNS", LOG_ERR, "pConLogs MALLOC failed");
				return ;
		}
		
                pConLogs->timePeriod = timeLimit;
		pConLogs->fileSize = sizeLimit;
		pConLogs->timed = timed;

		APP_LOG("UPNP: Device", LOG_DEBUG, "CMD_CONSOLELOGS attrs timePeriod %d fileSize %d timed or not %d", \
						pConLogs->timePeriod, pConLogs->fileSize, pConLogs->timed);

		if (pConLogs->timed <= 0) {
				APP_LOG("SNS",LOG_DEBUG, "***************Plugin LOG Thread***************\n");
				pthread_attr_init(&consoleLogs_attr);
				pthread_attr_setdetachstate(&consoleLogs_attr, PTHREAD_CREATE_DETACHED);
				pthread_create(&consoleLogs_thread, &consoleLogs_attr, consoleLogginginFile, (void *)pConLogs);
		} else {
				APP_LOG("SNS",LOG_DEBUG, "***************Plugin LOG Thread***************\n");
				pthread_attr_init(&consoleLogs_attr);
				pthread_attr_setdetachstate(&consoleLogs_attr, PTHREAD_CREATE_DETACHED);
				pthread_create(&consoleLogs_thread, &consoleLogs_attr, consoleLogginginFile_timed, (void *)pConLogs);
		}
}

