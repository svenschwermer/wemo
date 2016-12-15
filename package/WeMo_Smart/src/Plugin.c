/***************************************************************************
*
*
* Plugin.c 
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
#include "global.h"
#include <stdlib.h>
#include "fcntl.h"
#include "defines.h"
#include "fw_rev.h"
#include "ithread.h" 
#include "types.h"
#include "upnp.h"
#include "controlledevice.h"
#include "plugin_ctrlpoint.h"
#include "wifiHndlr.h"
#include "fwDl.h"
#include "utils.h"
#include "osUtils.h"
#include "httpsWrapper.h"
#include "logger.h"
#include "itc.h"
#include "gpio.h"
#include "remoteAccess.h"
#include "getRemoteMac.h"
#include "WemoDB.h"


#include 	"rule.h"
#include "watchDog.h"

#include "wasp_api.h"
#include "wasp.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>

#include <belkin_diag.h>

#define DBURL    "/tmp/Belkin_settings/notification.db"
#define NOTIF_FILE_PATH "/tmp/Belkin_settings/notification.txt"

int gIceRunning = NATCL_NOT_INITIALIZED;
extern pthread_mutex_t gwebAppDataLock;

int g_IsDebugMode = 0x00;
int g_IsAutoTest  = 0x00;
unsigned long int g_SetUpCompleteTS = 0;

int g_IsAutoSensor  = 0x00;
int g_SerialNumber;
int g_serialNo_delete;
int g_phoneFlag = 0;  /*Flag set to 1 whenever mode changed by phone. see gpio.c for more details*/
int g_timerFlag = 0;  /*Flag set to 1 whenever time changed by phone. see gpio.c for more details*/
int g_bootEntryFlag = 0;
int g_notificationFlag = 0;
int g_NotifSerialNumber = 1;
int g_numberofattributes = 0;
int g_attributeFlag[100];
char* g_attrName;
char* g_prevValue;
char* g_curValue;
WaspVarDesc **g_attrVarDescArray = NULL;
int g_attrCount = 0;
char g_remotebuff[1024];

attrDetails notifattrdetails[10];


pthread_attr_t adv_attr;
pthread_attr_t sysRestore_attr;
pthread_t advthread=-1;

extern int default_advr_expire;
extern UpnpDevice_Handle device_handle;

extern unsigned short gpluginRemAccessEnable;

extern ithread_t power_thread;
//------------------

extern void *LedAutoToggleLoop(void *args);
extern void *AutoCtrlPointTestLoop(void * args);

extern void EnableSiteSurvey();

extern char g_routerMac[MAX_MAC_LEN];
extern char g_routerSsid[SIZE_64B];
extern char g_szRestoreState[SIZE_2B];
extern char g_szHomeId[SIZE_20B];
extern char g_szPluginPrivatekey[SIZE_50B];
extern int g_isRouterChange;
extern int  g_hr_respStatus;

sqlite3 *NotificationDB=NULL;
extern void* attr_compare(void* arg);

int socketDeviceProcessCommand(char *cmdline);
struct cmdloop_commands {
	/* the string  */
	const char *str;
	/* the command */
	int cmdnum;
	/* the number of arguments */
	int numargs;
	/* the args */
	const char *args;
} cmdloop_commands;

enum cmdloop_tvcmds {
	PRTHELP = 0,
	SiteSurvey,
	ConnectHome,
	SetSSID,
	SetAPState,
	SetClientState,
	GetWifiClientStatistics,
	GetWifiClientCurrentState,
	ResetWiFiSettings,
	SetChannelAP,
	SetLogs,
	GetIP,
	DwnldFirmware,
	SetFrndlyName,
	GetFrndlyName,
	IconSet,
	IconGet,	
	EXITCMD
};
static struct cmdloop_commands cmdloop_cmdlist[] = {
	{"Help",          PRTHELP,     1, ""},
	{"SiteSurvey",    SiteSurvey, 1, ""},
	{"Connect",       ConnectHome,      6, "<Channel (int)> <SSID> <Auth> <Encryp> <Passwd>"},
	{"SetAPSSID",      SetSSID,     2, "<SSID>"},
	{"SetAPMode",	SetAPState,	2,	"<ON/OFF>"},
	{"SetClientMode",	SetClientState,	2,	"<ON/OFF>"},
	{"GetWifiStats",	GetWifiClientStatistics,	1,	""},
	{"GetWifiCurrentStatus",	GetWifiClientCurrentState,	1,	""},
	{"ResetWiFiSettings",	ResetWiFiSettings,	1,	""},
	{"SetAPChannel",	SetChannelAP,	2,	"<Channel>"},
	{"SetLogLevel",    SetLogs,     3, "<Level> <Logs into file/not>"},
	{"GetIfIP",    GetIP,     2, "<I/F [0-Client,1-AP]>"},
	{"DownloadFirmware",    DwnldFirmware,     3, "<URL> <PATH>"},	
	{"SetFriendlyName",    SetFrndlyName,     2, "<NAME>"},
	{"GetFriendlyName",    GetFrndlyName,     1, ""},
	{"SetIcon",    IconSet,     2, "<IMAGEFILE>"},
	{"GetIcon",    IconGet,     1, ""},
	{"Exit", EXITCMD, 1, ""}
};

void showHelp () {
	printf("Help"); 
        printf("SiteSurvey");
        printf("Connect<Channel> <SSID> <Auth> <Encryp> <Passwd>");
        printf("SetAPSSID<SSID>");
		printf("SetAPMode<ON/OFF>");
		printf("SetClientMode<ON/OFF>");
		printf("GetWifiStats");
		printf("GetWifiCurrentStatus");
		printf("ResetWiFiSettings");
		printf("SetAPChannel<Channel>");
        printf("SetLogLevel <Level> <File/No>");
        printf("GetIfIP <I/F [0-Client,1-AP]>");
		printf("DownloadFirmware <URL> <PATH>");
		printf("SetFriendlyName <NAME>");
		printf("GetFriendlyName");
		printf("SetIcon <IMAGEFILE>");
		printf("GetIcon");
        printf("Exit");

}


void *SocketDeviceCommandLoop(void *args)
{
	char cmdline[100];

	while (1) {
		printf(">> ");
		fgets(cmdline, 100, stdin);
		socketDeviceProcessCommand(cmdline);
	}

	return NULL;
	args = args;
}

void nat_set_log_level();

int socketDeviceProcessCommand(char *cmdline)
{
	char cmd[SIZE_100B];
        char arg3[SIZE_64B],arg4[SIZE_64B],arg5[SIZE_64B],arg6[SIZE_64B],arg7[SIZE_64B];
	int cmdnum = -1;
	int numofcmds = (sizeof cmdloop_cmdlist) / sizeof (cmdloop_commands);
	int cmdfound = 0;
	int i;
	int rc;
	int invalidargs = 0;
	int validargs;

	validargs = sscanf(cmdline, "%s %s %s %s %s %s", cmd,arg3,arg4,arg5,arg6,arg7);
	for (i = 0; i < numofcmds; ++i) {
		if (strcasecmp(cmd, cmdloop_cmdlist[i].str ) == 0) {
			cmdnum = cmdloop_cmdlist[i].cmdnum;
			cmdfound++;
			if (validargs != cmdloop_cmdlist[i].numargs)
				invalidargs++;
			break;
		}
	}
	if (!cmdfound) {
		APP_LOG("UPNP", LOG_DEBUG,"Command not found; try 'Help'");
		return SUCCESS;
	}
	if (invalidargs) {
		APP_LOG("UPNP", LOG_DEBUG,"Invalid arguments; try 'Help'");
		return SUCCESS;
	}
	switch (cmdnum) {
	case PRTHELP:
		showHelp();
		break;
	case SiteSurvey:
  	{
                PMY_SITE_SURVEY pAvlAPList;
                int count=0,i;
            
		pAvlAPList = (PMY_SITE_SURVEY) ZALLOC(sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE);

                if(!pAvlAPList) {
                    APP_LOG("UPNP", LOG_DEBUG,"Malloc Failed...");
                    return -1;
                }

                APP_LOG("UPNP", LOG_DEBUG,"Get List...");
                getCurrentAPList(pAvlAPList, &count);
                APP_LOG("UPNP", LOG_DEBUG,"List Size <%d>...",count);
                for (i=0;i<count;i++) {
                    APP_LOG("UPNP", LOG_DEBUG,"%d-%s-%s-%d",atoi((char *)pAvlAPList[i].channel),pAvlAPList[i].ssid,pAvlAPList[i].security,atoi(pAvlAPList[i].signal)); 
                }
                free (pAvlAPList);
	}		
	break;
	case ConnectHome:
	{
            int ret=0;
            ret = connectHomeNetwork (atoi(arg3),arg4,arg5,arg6,arg7);
	}
	break;
	case SetSSID:
	{
            APP_LOG("UPNP", LOG_DEBUG,"SSID To be set: %s",arg3); 
	    wifisetSSIDOfAP(arg3);
	}
	break;
	case SetAPState:
	{
        int ret=0x00;
	    ret = setAPIfState(arg3);
	}
	break;
	case SetClientState:
	{
        int ret=0x00;
	    ret = setClientIfState(arg3);
	}
	break;
	case GetWifiClientStatistics:
	{
		PWIFI_STAT_COUNTERS pWiFiStats;
		
		pWiFiStats = (PWIFI_STAT_COUNTERS) MALLOC (sizeof(WIFI_STAT_COUNTERS));
		if(pWiFiStats==NULL) {
			APP_LOG("UPNP", LOG_DEBUG,"GetWifiClientStatistics: Malloc Failed...");
			return FAILURE;
		}		
		if((getWiFiStatsCounters (pWiFiStats)) < 0 ){
			APP_LOG("UPNP", LOG_DEBUG,"Failed in getting wifi statistics\n");
			free (pWiFiStats);
			return FAILURE;
		}
		
		APP_LOG("UPNP", LOG_DEBUG,"Listing Wifi Statistics ...");
		APP_LOG("UPNP", LOG_DEBUG,"-------------------------------------------");
		APP_LOG("UPNP", LOG_DEBUG,"TxSuccess:			%ld",pWiFiStats->TxSuccessTotal);
		APP_LOG("UPNP", LOG_DEBUG,"TxSuccessWithRetry:		%ld",pWiFiStats->TxSuccessWithRetry);
		APP_LOG("UPNP", LOG_DEBUG,"TxFailWithRetry:		%ld",pWiFiStats->TxFailWithRetry);
		APP_LOG("UPNP", LOG_DEBUG,"RtsSuccess:			%ld",pWiFiStats->RtsSuccess);
		APP_LOG("UPNP", LOG_DEBUG,"RtsFail:			%ld",pWiFiStats->RtsFail);
		APP_LOG("UPNP", LOG_DEBUG,"RxSuccess:			%ld",pWiFiStats->RxSuccess);
		APP_LOG("UPNP", LOG_DEBUG,"RxWithCRC:			%ld",pWiFiStats->RxWithCRC);
		APP_LOG("UPNP", LOG_DEBUG,"RxDropNoBuffer:			%ld",pWiFiStats->RxDropNoBuffer);
		APP_LOG("UPNP", LOG_DEBUG,"RxDuplicateFrame:		%ld",pWiFiStats->RxDuplicateFrame);
		APP_LOG("UPNP", LOG_DEBUG,"FalseCCA:			%ld",pWiFiStats->FalseCCA);
		APP_LOG("UPNP", LOG_DEBUG,"RSSIA:				%ld",pWiFiStats->RssiA);
		APP_LOG("UPNP", LOG_DEBUG,"RSSIB:				%ld",pWiFiStats->RssiB);
		APP_LOG("UPNP", LOG_DEBUG,"--------------------------------------------");
		
		free (pWiFiStats);
		
	}
	break;
	case GetWifiClientCurrentState:
	{
		int WiFiClientCurrState;
		APP_LOG("UPNP", LOG_DEBUG,"============================================");
		switch((WiFiClientCurrState=getCurrentClientState ())){
			case STATE_DISCONNECTED:
				APP_LOG("UPNP", LOG_DEBUG,"WiFi Client disconnected");
				break;
			case STATE_INTERNET_NOT_CONNECTED:
				APP_LOG("UPNP", LOG_DEBUG,"WiFi Client internet not connected");
				break;
			default:
				break;
		}
		APP_LOG("UPNP", LOG_DEBUG,"============================================");
	}
	break;
        case ResetWiFiSettings:
        {
            setClientIfState("OFF");
            SetBelkinParameter (WIFI_CLIENT_SSID,"");
            SaveSetting();
            gWiFiConfigured=0;
        }
        break;
	case SetChannelAP:
	{
		int ret;
		ret = setAPChnl(arg3);
	}
	break;
	case SetLogs:
	{
        int level	=	atoi(arg3);
        int option	=	atoi(arg4);

        APP_LOG("UPNP", LOG_DEBUG,"set Log setting: %d %d", level, option); 
	    loggerSetLogLevel(level,option);
			nat_set_log_level();
	}
	break;
	case GetIP:
	{
            int interface=atoi(arg3);
            
            if(interface){
                APP_LOG("UPNP", LOG_DEBUG,"Get IP: %s",wifiGetIP(INTERFACE_CLIENT));
	    }
            else{
                APP_LOG("UPNP", LOG_DEBUG,"Get IP: %s",wifiGetIP(INTERFACE_AP));
	    }
	}
	break;
	case DwnldFirmware:
	{
		int ret;		
		ret = downloadFirmware(arg3, arg4);		
	}
	break;
	case SetFrndlyName:
	{
		int ret;
		APP_LOG("UPNP", LOG_DEBUG,"Set friendly name is: %s", arg3);
		ret = SaveDeviceConfig("FriendlyName",arg3);	
	}
	break;
	case GetFrndlyName:
	{		
		 char *szFName = GetDeviceConfig("FriendlyName");
		 APP_LOG("UPNP", LOG_DEBUG,"=======================================");
		 APP_LOG("UPNP", LOG_DEBUG,"Get friendly name is: %s", szFName);
		 APP_LOG("UPNP", LOG_DEBUG,"=======================================");
	}
	break;
	case IconSet:
	{
	}
	break;
	case IconGet:
	{
		char szIURL[SIZE_128B];
		memset(szIURL, 0x00, sizeof(szIURL));
		
		snprintf(szIURL, sizeof(szIURL), "http://%s:%d/icon.jpg", g_server_ip, g_server_port);
		APP_LOG("UPNP", LOG_DEBUG,"GetIcon is: %s", szIURL);
	}
	break;
	case EXITCMD:
	{
		rc = 0;
	}
	APP_LOG("UPNP", LOG_ALERT, "Exit command");
	exit(rc);
	break;
	default:
		APP_LOG("UPNP", LOG_DEBUG,"Command not implemented; see 'Help'");
		break;
	}
	if(invalidargs)
		APP_LOG("UPNP", LOG_DEBUG,"Invalid args in command; see 'Help'");

	return SUCCESS;
}

void* devAdvMainLoop(void *arg)
{
	int ret;
    	struct timespec ts;

	APP_LOG("UPNP",LOG_DEBUG, "Device advertisement thread running...\n");

	while(1)
	{

		if( ( ret = UpnpSendAdvertisement( device_handle, default_advr_expire ) )
				!= UPNP_E_SUCCESS ) 
		{
			APP_LOG("UPNP",LOG_DEBUG, "Error sending advertisements : %d\n", ret );
			UpnpFinish();
			return (void *)ret;
		}
		else
			APP_LOG("UPNP",LOG_DEBUG, "Device advertisement sent...\n");

        	ts.tv_sec = 1;
	        ts.tv_nsec = 0;
		nanosleep (&ts,NULL);
	}


}

void *ResetButtonTask(void *args);


/* Gets Device ID from WASP module */
unsigned int GetDevID(void)
{
	WaspVariable DevID;
	int Err;
	unsigned int Ret = 0;

	memset(&DevID,0,sizeof(WaspVariable));
	DevID.ID = WASP_VAR_DEVID;
	DevID.State = VAR_VALUE_CACHED;

	if((Err = WASP_GetVariable(&DevID)) != WASP_OK) {
		// Report error
	}
	else {
		Ret = DevID.Val.U32;
	}

	return Ret;
}

/* Gets Device Name from WASP module */
char* GetDevName(void)
{
	WaspVariable DevName;
	int Err;
	

	memset(&DevName,0,sizeof(WaspVariable));
	DevName.ID = WASP_VAR_DEV_DESC;
	DevName.State = VAR_VALUE_CACHED;

	if((Err = WASP_GetVariable(&DevName)) != WASP_OK) {
        		// Report error
	}
	else {
	  memset(g_DevName, 0, sizeof(g_DevName));
	  strcpy(g_DevName, DevName.Val.String);
	  if(DevName.Val.String != NULL) {
	    free(DevName.Val.String);
	  }
	  APP_LOG("Plugin", LOG_DEBUG, "g_DevName is %s", g_DevName);
	}

	return g_DevName;
}

/* Gets Device Brand Name from WASP module */
char* getBrandName(void)
{
	WaspVariable brandName;
	int Err;

	memset(&brandName,0,sizeof(WaspVariable));
	brandName.ID = WASP_VAR_BRAND_NAME;
	brandName.State = VAR_VALUE_CACHED;

	if((Err = WASP_GetVariable(&brandName)) != WASP_OK) {
		// Report error
		APP_LOG("Plugin", LOG_DEBUG, "Error! while fetching brand name %d", Err);
	}
	else {
                strcpy(g_brandName, brandName.Val.String);
		printf("g_brandName is %s\n", g_brandName);
		if (brandName.Val.String != NULL) {
		  free(brandName.Val.String);
		}
		APP_LOG("Plugin", LOG_DEBUG, "g_brandName is %s", g_brandName);
	}
	return g_brandName;
}

const char *VarTypes[] = {
   "Invalid!",
   "ENUM",
   "PERCENT",
   "TEMP",
   "TIME32",
   "TIME16",
   "TIMEBCD",
   "BOOL",
   "BCD_DATE",
   "DATETIME",
   "STRING",
   "BLOB",
   "UINT8",
   "INT8",
   "UINT16",
   "INT16",
   "UINT32",
   "INT32",
   "TIME_M16"
};

const char *Usages[] = {
   "Invalid!",
   "FIXED",
   "MONITORED",
   "DESIRED",
   "CONTROLLED"
};

const char *ValStates[] = {
   "Not set",        // VAR_VALUE_UNKNOWN
   "Cached value",   // VAR_VALUE_CACHED:
   "Live value",     // VAR_VALUE_LIVE
   "Set Value",      // VAR_VALUE_SET
   "Value being Set" // VAR_VALUE_SETTING
};

void attrPrint(WaspVarDesc *p)
{
   APP_LOG("Plugin", LOG_DEBUG, "Inside attrPrint");
   printf("VarType:\t%s\n",VarTypes[p->Type]);
   int Integer;
   int Fraction;
      
      if(p->Type == 0 || p->Type > NUM_VAR_TYPES) {
         printf("%s: Error invalid VarType 0x%x\n",__FUNCTION__,p->Type);
      }
      printf("Variable '%s' (0x%02x):\n",p->Name,p->ID);
      printf("  VarType:\t%s\n",VarTypes[p->Type]);
      if(p->Usage == 0 || p->Usage > WASP_USAGE_CONTROLLED) {
         printf("%s: Error invalid Usage 0x%x\n",__FUNCTION__,p->Usage);
      }
      printf("  Usage:\t%s\n",Usages[p->Usage]);
      printf("  PollRate:\t%d.%03d secs\n",p->PollRate/1000,p->PollRate%1000);

      switch(p->Type) {
         case WASP_VARTYPE_ENUM:
            // MinMax.U8
         printf("  Minimum:\t%d\n",p->MinValue.U8);
         printf("  Maximum:\t%d\n",p->MaxValue.U8);
         break;

         case WASP_VARTYPE_TIMEBCD:
         // MinMax.BCD3
         // The first byte is the number of seconds, 
         // the second byte is the number of hours and the 
         // third byte is the number of minutes.  
            printf("  Minimum:\t%02x:%02x:%02x\n",p->MinValue.BcdTime[2],
                   p->MinValue.BcdTime[1],p->MinValue.BcdTime[0]);
            printf("  Maximum:\t%02x:%02x:%02x\n",p->MaxValue.BcdTime[2],
                   p->MaxValue.BcdTime[1],p->MaxValue.BcdTime[0]);
            break;

         case WASP_VARTYPE_BCD_DATE:
         // MinMax.BCD4 dd/mm/yyyy in BCD
         // The first byte is the day of the month, the second byte is the month,
         // the third byte is the least significant 2 digits of the year and 
         // the forth byte is the most significant 2 digits of the year.
            printf("  Minimum:\t%02x/%02x/%02x%02x\n",p->MinValue.BcdDate[1],
                   p->MinValue.BcdDate[0],p->MinValue.BcdDate[3],
                   p->MinValue.BcdDate[2]);
            printf("  Maximum:\t%02x/%02x/%02x%02x\n",p->MaxValue.BcdDate[1],
                   p->MaxValue.BcdDate[0],p->MaxValue.BcdDate[3],
                   p->MaxValue.BcdDate[2]);
            break;

         case WASP_VARTYPE_DATETIME:
            // MinMax.BCD7
            printf("  Minimum:\t%02x:%02x:%02x %02x/%02x/%02x%02x\n",
                   p->MinValue.BcdDateTime[2],
                   p->MinValue.BcdDateTime[1],p->MinValue.BcdTime[0],
                   p->MinValue.BcdDateTime[4],p->MinValue.BcdDateTime[3],
                   p->MinValue.BcdDateTime[6],p->MinValue.BcdDateTime[5]);

            printf("  Maximum:\t%02x:%02x:%02x %02x/%02x/%02x%02x\n",
                   p->MaxValue.BcdDateTime[2],
                   p->MaxValue.BcdDateTime[1],p->MaxValue.BcdTime[0],
                   p->MaxValue.BcdDateTime[4],p->MaxValue.BcdDateTime[3],
                   p->MaxValue.BcdDateTime[6],p->MaxValue.BcdDateTime[5]);
            break;

         case WASP_VARTYPE_TEMP:
         // MinMax.I16
            Integer = p->MinValue.Temperature / 10;
            Fraction = p->MinValue.Temperature % 10;
            if(Fraction < 0) {
               Fraction = -Fraction;
            }
            printf("  Minimum:\t%d.%d degrees F\n",Integer,Fraction);
            Integer = p->MaxValue.Temperature / 10;
            Fraction = p->MaxValue.Temperature % 10;
            if(Fraction < 0) {
               Fraction = -Fraction;
            }
            printf("  Maximum:\t%d.%d degrees F\n",Integer,Fraction);
            break;

         case WASP_VARTYPE_TIME32:
         // MinMax.Int Number of seconds in .1 second steps. 
            printf("  Minimum:\t%d.%d secs\n",
                   p->MinValue.TimeTenths/10,p->MinValue.TimeTenths%10);
            printf("  Maximum:\t%d.%d secs\n",
                   p->MinValue.TimeTenths/10,p->MaxValue.TimeTenths%10);
            break;

         case WASP_VARTYPE_TIME16:
            // MinMax.U16 Number of seconds in 1 second steps. 
            printf("  Minimum:\t%u secs\n",p->MinValue.TimeSecs);
            printf("  Maximum:\t%u secs\n",p->MaxValue.TimeSecs);
            break;

         case WASP_VARTYPE_BLOB:
         case WASP_VARTYPE_STRING:
         case WASP_VARTYPE_UINT32:
            // MinMax.U32
            printf("  Minimum:\t%u\n",p->MinValue.U32);
            printf("  Maximum:\t%u\n",p->MaxValue.U32);
            break;

         case WASP_VARTYPE_PERCENT:
            // MinMax.U16
            printf("  Minimum:\t%u.%u%%\n",p->MinValue.U16/10,p->MinValue.U16%10);
            printf("  Maximum:\t%u.%u%%\n",p->MaxValue.U16/10,p->MaxValue.U16%10);
            break;

         case WASP_VARTYPE_TIME_M16:
            // MinMax.U16 Number of minute in 1 minute steps. 
            printf("  Minimum:\t%u minutes\n",p->MinValue.TimeMins);
            printf("  Maximum:\t%u minutes\n",p->MaxValue.TimeMins);
            break;

         default:
            break;
      }

      if(p->Type == WASP_VARTYPE_ENUM) {
      // Dump the enum names
         int i;
         for(i = 0; i < p->Enum.Count; i++) {
            printf("  Enum %d:\t%s\n",i,p->Enum.Name[i]);
         }
      }
}

void initNotificationDB(void)
{
    FILE *fp = fopen(DBURL, "rb");
    
	if(fp)
	{
        fclose(fp);
        if(InitDB(DBURL,&NotificationDB) == DB_SUCCESS)
		{
			APP_LOG("APP", LOG_DEBUG, "^^^^^^^^^^^^ NotificationDB Init done! ^^^^^^^^^^^^^^^"  );
        }
        else
		{
			APP_LOG("APP", LOG_DEBUG, "^^^^^^^^^^^^ NotificationDB  Init Failed ^^^^^^^^^^^^^^^"  );
		}
    }
    else{
        if(InitDB(DBURL,&NotificationDB) == DB_SUCCESS)
		{
            TableDetails notificationData[]={{"SERIALNO","int"},{"Attr1","int"},{"Attr2","int"},{"Attr3","int"},{"Attr4","int"},{"Attr5","int"},{"dstURL","TEXT"},{"dstType","int"}};
  
                WeMoDBCreateTable(&NotificationDB, "notificationData", notificationData, 0, 8);
        }
        else
		{
			APP_LOG("APP", LOG_DEBUG, "^^^^^^^^^^^^ NotificationDB  Init Failed ^^^^^^^^^^^^^^^"  );
		}
    }
}

#if 0
int readNotificationtxt()
{

    if( access(NOTIF_FILE_PATH, F_OK ) != -1 ) {
        FILE* pfReadStream = fopen(NOTIF_FILE_PATH,"r");
    } else {
        
        APP_LOG("APP", LOG_DEBUG, "Notification.txt does not exist");
        return -1;        
    }
    
    
    //FILE* pfReadStream = fopen(NOTIF_FILE_PATH,"r");
    const char szBuff[SIZE_256B];
    char* attrlist = 0x00;
    char* dstURL, dstType, buff ;
    int i, attribute[5];
    char    pipedelims[] = "|";
    char    commadelims[] = ",";

    memset(szBuff, 0x00, sizeof(szBuff));

    while (fgets(szBuff, SIZE_256B, pfReadStream)!= NULL)
    {

        APP_LOG("APP", LOG_DEBUG, "szBuff is %s ", szBuff);
        
        attrlist = strtok_r(szBuff, pipedelims,&strtok_r_temp);
        APP_LOG("APP", LOG_DEBUG, "attrlist is %s", attrlist);

        dstURL = strtok_r(NULL, pipedelims,&strtok_r_temp);
        APP_LOG("APP", LOG_DEBUG, "dstURL is %s", dstURL);
        dstType = atoi(strtok_r(NULL,pipedelims,&strtok_r_temp));
        APP_LOG("APP", LOG_DEBUG, "dstType is %d", dstType);

        attribute[0] = atoi(strtok_r(attrlist, commadelims,&strtok_r_temp));
        APP_LOG("APP", LOG_DEBUG, "attribute[0] is %d", attribute[0]);
        attribute[1] = atoi(strtok_r(NULL, commadelims,&strtok_r_temp));
        APP_LOG("APP", LOG_DEBUG, "attribute[1] is %d", attribute[1]);
        attribute[2] = atoi(strtok_r(NULL, commadelims,&strtok_r_temp));
        APP_LOG("APP", LOG_DEBUG, "attribute[2] is %d", attribute[2]);
        attribute[3] = atoi(strtok_r(NULL, commadelims,&strtok_r_temp));
        APP_LOG("APP", LOG_DEBUG, "attribute[3] is %d", attribute[3]);
        attribute[4] = atoi(strtok_r(NULL, commadelims,&strtok_r_temp));
        APP_LOG("APP", LOG_DEBUG, "attribute[4] is %d", attribute[4]);
        APP_LOG("APP", LOG_DEBUG, "Before setnotif");
        SetNotif(&attribute, dstURL, dstType);
        APP_LOG("APP", LOG_DEBUG, "End of readnotification");
        memset(szBuff, 0x00, sizeof(szBuff));
    }
    
    return 0;
}
#endif

void UpdateSmartDevParams(void)
{
	char *DeviceID = NULL;
	unsigned int intDeviceID = 0;
	unsigned int wait_duration = 10;  //Wait for 5secs to initilize
	int Ret = WASP_OK;
	int count=0;
	char buff[SIZE_32B];
	char buff1[SIZE_20B];
	
	DeviceID = GetBelkinParameter(DEFAULT_SMART_DEV_ID);
	APP_LOG("Plugin", LOG_DEBUG, "Device ID from NVRAM is: %s", DeviceID);
	
	memset(g_DevName, 0, sizeof(g_DevName));
	strcpy(g_DevName, GetBelkinParameter(DEFAULT_SMART_DEV_NAME));
	APP_LOG("Plugin", LOG_DEBUG, "Device Name from NVRAM is: %s", g_DevName);
	if(strlen(g_DevName) == 0) {
	  strcpy(g_DevName, "smart");
	}
    
    if(DeviceID != NULL)
    {
        intDeviceID = atoi(DeviceID);
        APP_LOG("Plugin", LOG_DEBUG, "Device ID from NVRAM is not null and the device id after conversion is %d:", intDeviceID);
    }
    
	while (((Ret = WASP_Init()) != 0) && count < wait_duration) 
	{
		/* 0.5s */
		usleep(500u * 1000u);
		count++;
	}
	
	APP_LOG("Plugin", LOG_DEBUG, "WASP Init return value is: %d", Ret);

	if( Ret == WASP_OK)
	{
		unsigned int Device_ID;
		Device_ID =  GetDevID();
		
		if (0x00 == intDeviceID || 0x00 == strlen(DeviceID))
		{
			char* Device_Name;
		
			Device_Name = GetDevName();

			APP_LOG("Plugin", LOG_DEBUG, "Device ID from WASP is: %d", Device_ID); 

			szDeviceID = Device_ID;
			intDeviceID = Device_ID;

			memset(buff, 0x0, SIZE_32B);
			snprintf(buff, SIZE_32B, "%d", Device_ID);
			SetBelkinParameter(DEFAULT_SMART_DEV_ID, buff);
			
			memset(buff1, 0x0, SIZE_20B);
			snprintf(buff1, SIZE_20B, "%s", g_DevName);
			SetBelkinParameter(DEFAULT_SMART_DEV_NAME, buff1);
			SaveSetting();
		} 
		else
		{
			szDeviceID = atoi(DeviceID);
			APP_LOG("Plugin", LOG_DEBUG, "Device ID from NVRAM after Conversion is: %d", szDeviceID);
		}
		
		/* Update Device Attributes */
		if ((0x00 != intDeviceID) && (intDeviceID == Device_ID))
		{
			// Check to read attributes from WASP
            int attrRet=0;
            //WaspVarDesc **attrVarDescArray = NULL;

            if((attrRet = WASP_GetVarList(&g_attrCount,&g_attrVarDescArray)) != 0) {
                APP_LOG("Plugin", LOG_DEBUG, "WASP_GetVarList failed and returbed value is : %d", attrRet);
            }
            else 
			{
                APP_LOG("Plugin", LOG_DEBUG, " g_attrCount: %d", g_attrCount);
                int attri;
                for(attri = 0; attri < g_attrCount; attri++) 
				{
                    attrPrint(g_attrVarDescArray[attri]);
                }
            }
		}
	}
	else
	{
		APP_LOG("Plugin", LOG_DEBUG, "WASP Init Error so setting device type to defaults i.e. Smart");
		
		szDeviceID = 0x00;
			
		memset(buff, 0x0, SIZE_32B);
		snprintf(buff, SIZE_32B, "%d", szDeviceID);
		SetBelkinParameter(DEFAULT_SMART_DEV_ID, buff);
		
		memset(buff1, 0x0, SIZE_20B);
		snprintf(buff1, SIZE_20B, "%s", g_DevName);
		SetBelkinParameter(DEFAULT_SMART_DEV_NAME, buff1);
		SaveSetting();
	}
}

int main(int argc, char **argv )
{
	unsigned int portTemp = 0;
	char* ip_address = NULL;
	char* desc_doc_name = NULL;
	char* web_dir_path = NULL;

	ithread_t reset_thread;	

	int i = 0;


    // The following code snippet is a dangerous test of
    // GetBelkinParameter().  When compiled for non-production use,
    // GetBelkinParameter() tests its' parameter with assert().  This
    // will abort this process.  When compiled for production use, it
    // should return an empty string ("") when passed a NULL pointer.
    // Only enable the following code if you need to test this
    // particular behavior.  On a production system it will print a
    // harmless message but non-production images WILL ABORT.
    // You have been warned.
#if 0
    printf( ">>> Note: About to call GetBelkinParameter( NULL )...\n" );
    char *null_nvram_test_result = GetBelkinParameter( NULL );
    printf( ">>> Note: GetBelkinParameter( NULL ) returned 0x%lx (%s)\n", 
            null_nvram_test_result,
            null_nvram_test_result ? null_nvram_test_result : "NULL" );
#endif

    /* Initialize Belkin diagnostics, if enabled */
    init_diagnostics();

    // Uncomment following to compile code that will intentionally
    // trigger a fatal error for dmalloc to report.  For extra safety,
    // an nvram variable must also be set (see below).
//#define IRRITATE_DMALLOC
#if defined(DMALLOC) && defined(IRRITATE_DMALLOC)
    // Now (just for testing purposes) do something that should annoy
    // dmalloc.  A double-free should do it.
    const char *irritate_dmalloc = GetBelkinParameter( "dm_irritate" );
    if( irritate_dmalloc && strlen( irritate_dmalloc )) {
      char *foo = NULL;
      if( (foo = MALLOC( 20 )) ) {
        printf( ">>>: Note: Freeing foo (0x%lx)\n", foo );
        free( foo );
        printf( ">>>: Note: Freeing foo (0x%lx) AGAIN.\n", foo );
        free( foo );
        printf( ">>>: Note: Done messing around with dmalloc.\n" );
        foo = NULL;
      } else
        printf( ">>> Note: test MALLOC failed.\n" );
    }
#endif  /* defined(DMALLOC) && defined(IRRITATE_DMALLOC) */

	// Parse options
	for( i = 1; i < argc; i++ )
	{
		if( strcmp( argv[i], "-ip" ) == 0 )
		{
			ip_address = argv[++i];
		}
		else if( strcmp( argv[i], "-port" ) == 0 )
		{
			sscanf( argv[++i], "%u", &portTemp );
		}
		else if( strcmp( argv[i], "-desc" ) == 0 ) {
			desc_doc_name = argv[++i];
		}
		else if( strcmp( argv[i], "-webdir" ) == 0 )
		{
			web_dir_path = argv[++i];
		}
		else if( strcmp( argv[i], "-Erase" ) == 0 )
		{
			resetNetworkParams();
			clearMemPartitions();
			ClearRuleFromFlash();
			resetToDefaults ();
			usleep(500000);
			system("nvram restore");
			APP_LOG("UPNP", LOG_DEBUG,"System rebooting........");
			pluginUsleep(2000000);

			APP_LOG("UPNP", LOG_ALERT, "wemoApp -Erase");
			system("reboot");
		}
		else if( strcmp( argv[i], "-Restore" ) == 0 )
		{
			char *homeId = GetBelkinParameter(DEFAULT_HOME_ID);
			char *pluginKey = GetBelkinParameter(DEFAULT_PLUGIN_PRIVATE_KEY);
			if ((homeId && pluginKey) && (0x00 == strlen(homeId) ) && (0x00 == strlen(pluginKey))) 
			{
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "\n Remote Access is not Enabled..not trying\n");
				setRemoteRestoreParam (0x0);	//Ideally it should already be zero. Forcing anyway.
			} else {
				setRemoteRestoreParam (0x1);	//setting de-register flag
			}
			APP_LOG("UPNP", LOG_ALERT, "wemoApp -Restore");
			system("reboot");
		}
		else if( strcmp( argv[i], "-Debug" ) == 0 )
		{
			g_IsDebugMode = 1;
		}
		else if( strcmp( argv[i], "-Sensitivity" ) == 0 )
		{
			int delay, sensitivity;
			sscanf( argv[++i], "%d", &delay);
			sscanf( argv[++i], "%d", &sensitivity);
			APP_LOG("Sensor", LOG_DEBUG, "Setting: delay:%d, sensitivity:%d", delay, sensitivity);
			if (delay != 0x00)
				g_cntSensorDelay = delay;

			if (sensitivity != 0x00)
				g_cntSensitivity = sensitivity;
		}

		else if (0x00 == strcmp(argv[i], "-Auto"))
		{
			//-Enable Auto test
			APP_LOG("UPNP", LOG_DEBUG, "################### Enable automation test ###################" );
			g_IsAutoTest = 0x01;
		}

		else if (0x00 == strcmp(argv[i], "-Sensoring"))
		{
			APP_LOG("UPNP", LOG_DEBUG, "################ Start sensoring immediately #######################");
			g_IsAutoSensor = 0x01;
		}
		else if( strcmp( argv[i], "-Serial" ) == 0 )
		{
			char* szTmp = argv[++i];
			if (!szTmp || 0x00 == strlen(szTmp))
			{
				APP_LOG("UPNP", LOG_DEBUG,"Please input correct Serial No");
				exit(0x01);
			}
			else
			{
				SetSerialNumber(szTmp);
				APP_LOG("UPNP", LOG_DEBUG,"Serial Number: %s written and saved, system rebooting ......", szTmp);
				system("nvram set_factory_from_nvram");
				pluginUsleep(3000000);
				APP_LOG("UPNP", LOG_ALERT, "wemoApp -Serial");
				system("reboot");
			}
		}
		else if( strcmp( argv[i], "-Mac" ) == 0 )
		{
			char* szTmp = argv[++i];
			if (!szTmp || 0x00 == strlen(szTmp))
			{
				APP_LOG("UPNP", LOG_DEBUG,"Please input correct MAC");
				exit(0x01);
			}
			else
			{
				SetMACAddress (szTmp);
				APP_LOG("UPNP", LOG_DEBUG,"MAC: %s written and saved, system rebooting ......", szTmp);
				system("nvram set_factory_from_nvram");
				pluginUsleep(3000000);
				APP_LOG("UPNP", LOG_ALERT, "wemoApp -Mac");
				system("reboot");
			}
		}
		else if( strcmp( argv[i], "-malloc" ) == 0 )
		{
			char *cp = MALLOC(0);
			cp = STRDUP(NULL);
			cp = CALLOC(1,0x7fffffff);
		}
		else if( strcmp( argv[i], "-help" ) == 0 )
		{
			APP_LOG("UPNP",LOG_DEBUG, "Usage: %s -ip ipaddress -port port"
					" -desc desc_doc_name -webdir web_dir_path"
					" -help (this message)\n", argv[0] );
			APP_LOG("UPNP",LOG_DEBUG, "\tipaddress:     IP address of the device"
					" (must match desc. doc)\n" );
			APP_LOG("UPNP",LOG_DEBUG, "\t\te.g.: 192.168.0.4\n" );
			APP_LOG("UPNP",LOG_DEBUG, "\tport:          Port number to use for "
					"receiving UPnP messages (must match desc. doc)\n" );
			APP_LOG("UPNP",LOG_DEBUG, "\t\te.g.: 5431\n" );
			APP_LOG("UPNP",LOG_DEBUG, "\tdesc_doc_name: name of device description document\n" );
			APP_LOG("UPNP",LOG_DEBUG, "\t\te.g.: tvdevicedesc.xml\n" );
			APP_LOG("UPNP",LOG_DEBUG, "\tweb_dir_path: Filesystem path where web files "
					"related to the device are stored\n" );
			APP_LOG("UPNP",LOG_DEBUG, "\t\te.g.: /upnp/sample/tvdevice/web\n" );
			return 1;
		}
	}

    core_init_early();
    restoreRelayState();
	UpdateSmartDevParams();	
	
	/* 
	 ** core initialization - part 2: Init WiFi, start UPnP and start required threads 
	 ** argument (1/0) tells whether force start of Control point is desired or not respectively
	 */
	core_init_late(0);
    
    initNotificationDB();
    APP_LOG("Plugin", LOG_DEBUG, " After initnotificationdb");
    #if 0
	if(readNotificationtxt() != 0)
    {
        APP_LOG("Plugin", LOG_DEBUG, " readNotificationtxt failed");
    
    }
    else{
    
        APP_LOG("Plugin", LOG_DEBUG, " readNotificationtxt successful");
    
    }
	#endif
    
    #if 0  //Commented because of some locking issue
    pthread_t comparison_thread;
	pthread_create(&comparison_thread, NULL, attr_compare, NULL);
	#endif 

	while(1)
	{
		pNode node = readMessage(PLUGIN_E_MAIN_THREAD);
		ProcessItcEvent(node);
	}
	return EXIT_SUCCESS;
}
