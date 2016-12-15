#include <stdio.h>
#include <math.h>
#include "jardenhandler.h"
#include "wifiHndlr.h"
#include "global.h"
#include "logger.h"
#include <sys/select.h>
#include "types.h"
#include "defines.h"
#include "remoteAccess.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

int g_uploadDataUsage = 0;
int dataUploadFlag = 0;
int g_remoteEnableFlag = 0;
int dataUsageUpload = 0;

extern int g_remoteSetStatus;
extern int g_phoneFlag;

int g_cMode;
int g_cCookTime;
unsigned short int g_cCookedTime;
unsigned long int g_cStartTime;
unsigned long int g_cEndTime;
unsigned long int g_cDuration;
char g_cMethod[16];
unsigned long int timestamp;
extern char g_szPluginPrivatekey[MAX_PKEY_LEN];
extern char g_szRestoreState[MAX_RES_LEN];
extern unsigned short gpluginRemAccessEnable;
extern char g_szHomeId[SIZE_20B];


/*
<macAddress> EC1A59F21548</macAddress>
<mode>03</mode>
<cookTime>10</cookTime>
<cookedTime>25</cookedTime>
<startTS>1401704700</startTS>
<stopTS>1401704800</stopTS>
<duration>100</duration>
<method>SMARTPHONE</method>
<ts>1401704800</ts>

<macAddress>%s</macAddress><mode>%d</mode><startTS>%lu</startTS><stopTS>%lu</stopTS><duration>%lu</duration><method>%s</method>
*/
char bufCurrent[256] = {0};
char bufnext[256] = {0};
int flagFirstRead = 0;

void GetJardenStat(int phoneMethodSet) {
	int jardenMode = 0;
    int CookTime = 0;
    unsigned short int CookedTime = 0;
    unsigned long int StartTime = 0;
    unsigned long int StopTime =0;
    unsigned long int duration =0;
    char Method[16];
	int n = 0;
	
	memset(Method,0,sizeof(Method));
	APP_LOG("DataUsage", LOG_DEBUG, "GetJardenStat(int)");

	GetJardenStatusRemote(&jardenMode, &CookTime, &CookedTime); // Called to determine the latest state of the cooker
    StartTime = GetUTCTime();
    
    if(phoneMethodSet == 1)
	{ 
        strcpy(Method,"'SMARTPHONE'");
		APP_LOG("DataUsage", LOG_DEBUG, "Mode was set using a smartphone");
		if(g_remoteSetStatus == 1)
		{
			/* APP Remote */ 
			setActuation(ACTUATION_MANUAL_APP);
			setRemote("1");	
		}
		else
		{
			/* APP Local */ 
			setActuation(ACTUATION_MANUAL_APP);
			setRemote("0");	
		}
	}
    else if(phoneMethodSet == 2)
	{ 
        strcpy(Method,"'TIMERSET'");
		if(g_remoteSetStatus == 1)
		{
			/* APP Timer Remote */ 
			setActuation(ACTUATION_TIME_RULE);
			setRemote("1");	
		}
		else
		{
			/* APP Timer Local */ 
			setActuation(ACTUATION_TIME_RULE);
			setRemote("0");	
		}
	}
	else
	{
        strcpy(Method,"'MANUAL'");
		APP_LOG("DataUsage", LOG_DEBUG, "Mode was set manually");
		/* Manual Device */
		setActuation(ACTUATION_MANUAL_DEVICE);
		setRemote("0");
	}

	if(flagFirstRead == 0){
		memset(bufCurrent,0x00,sizeof(bufCurrent));
		n=sprintf (bufCurrent, "%d %d %hd %lu %lu %lu %s",jardenMode,CookTime,CookedTime,StartTime,StopTime,duration,Method);
	
	}
	else{
		memset(bufnext,0x00,sizeof(bufnext));
		n=sprintf (bufnext, "%d %d %hd %lu %lu %lu %s",jardenMode,CookTime,CookedTime,StartTime,StopTime,duration,Method);
	}

}	

void DataUsageTableUpdate(int phoneMethodSet) {

	int jardenModeNext = 0;
    int CookTimeNext = 0;
    unsigned short int CookedTimeNext = 0;
    unsigned long int StartTimeNext = 0;
    unsigned long int StopTimeNext =0;
    unsigned long int durationNext =0;
    char MethodNext[16];
	
	int jardenModeCurrent = 0;
    int CookTimeCurrent = 0;
    unsigned short int CookedTimeCurrent = 0;
    unsigned long int StartTimeCurrent = 0;
    unsigned long int StopTimeCurrent =0;
    unsigned long int durationCurrent =0;
	char MethodCurrent[16];
	
	memset(MethodNext,0,sizeof(MethodNext));
	memset(MethodCurrent,0,sizeof(MethodCurrent));
    
    APP_LOG("DataUsage", LOG_DEBUG, "buffer value: bufCurrent =%s\t bufnext =%s\n",bufCurrent,bufnext);
  
	if (flagFirstRead == 0) { 
		GetJardenStat(phoneMethodSet);
		flagFirstRead = 1;
    }
	else { 
		GetJardenStat(phoneMethodSet);
		
        sscanf (bufnext,"%d %d %hd %lu %lu %lu %s",&jardenModeNext,&CookTimeNext,&CookedTimeNext,&StartTimeNext,&StopTimeNext,&durationNext,MethodNext);
		sscanf (bufCurrent,"%d %d %hd %lu %lu %lu %s",&jardenModeCurrent,&CookTimeCurrent,&CookedTimeCurrent,&StartTimeCurrent\
		,&StopTimeCurrent,&durationCurrent,MethodCurrent);
        
        APP_LOG("DataUsage", LOG_DEBUG,"\n************************************\n");
        APP_LOG("DataUsage", LOG_DEBUG,"jardenModeCurrent: %d  CookTimeCurrent: %d  CookedTimeCurrent: %hd  StartTimeCurrent :%lu  StopTimeCurrent :%lu  durationCurrent :%lu  MethodCurrent: %s\n",jardenModeCurrent,CookTimeCurrent,CookedTimeCurrent,StartTimeCurrent,StopTimeCurrent,durationCurrent,MethodCurrent);
        
        APP_LOG("DataUsage", LOG_DEBUG,"jardenModeNext  : %d   CookTimeNext:   %d   CookedTimeNext:    %hd  StartTimeNext    :%lu  StopTimeNext    :%lu  durationNext    :%lu  MethodNext:   %s\n",jardenModeNext,CookTimeNext,CookedTimeNext,StartTimeNext,StopTimeNext,durationNext,MethodNext);
        APP_LOG("DataUsage", LOG_DEBUG,"\n************************************\n");


		g_cMode = jardenModeCurrent;
		g_cCookTime = CookTimeCurrent;
		g_cCookedTime = CookedTimeCurrent;
		g_cStartTime = StartTimeCurrent;
		g_cEndTime = StartTimeNext; /* Start time of next mode is end time of previous mode*/
		g_cDuration = g_cEndTime - g_cStartTime;
		strcpy(g_cMethod,MethodCurrent);
		
		memset(bufCurrent,0x00,sizeof(bufCurrent));
		strcpy(bufCurrent,bufnext);
		        
		APP_LOG("DataUsage", LOG_DEBUG,"\n jardenMode is %d",g_cMode);
		APP_LOG("DataUsage", LOG_DEBUG,"CookTime is %d",g_cCookTime);
		APP_LOG("DataUsage", LOG_DEBUG,"CookedTime is %hd",g_cCookedTime);
		APP_LOG("DataUsage", LOG_DEBUG,"StartTime is %lu",g_cStartTime);
		APP_LOG("DataUsage", LOG_DEBUG,"StopTime is %lu",g_cEndTime);
		APP_LOG("DataUsage", LOG_DEBUG,"Duration is %lu",g_cDuration);
		APP_LOG("DataUsage", LOG_DEBUG,"Method is %s \n",g_cMethod);
        
        if(jardenModeCurrent != jardenModeNext) {
            LockJardenRemote();	
            dataUsageUpload = 1;
            UnlockJardenRemote();
            /* accelarate remoteNotify,before Analytic notification by making g_remoteNotify = 1 */
            remoteAccessUpdateStatusTSParams (0xFF);
            APP_LOG("DataUsage", LOG_DEBUG,"\n@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");    
        }
        else {
            g_phoneFlag = 0;
        }
	}
}

void* updateAnalyticDataThread(void *arg) 
{
    int localdataUsageUpload = 0;
    while(1)
    {
        LockJardenRemote();	
		localdataUsageUpload = dataUsageUpload;
        UnlockJardenRemote();
        if(localdataUsageUpload == 1) 
        {
	   /* sleeping to let state change notification go */	
            sleep(5);
            sendAnalyticsData();
            LockJardenRemote();
            dataUsageUpload = 0;
            UnlockJardenRemote();
        }
        sleep(10);
    }  
}

void sendAnalyticsData(void)
{
    int localdataUsageUpload = 0;
    LockJardenRemote();	      
    g_uploadDataUsage = 1;  
    UnlockJardenRemote();
    remoteAccessUpdateStatusTSParams (0xFF);	
}
