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
#include "natClient.h"
#include "getRemoteMac.h"
#include "WemoDB.h"

#include "rule.h"
#include "watchDog.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */


int gIceRunning = NATCL_NOT_INITIALIZED;

int g_IsDebugMode = 0x00;
int g_IsAutoTest  = 0x00;

int g_IsAutoSensor  = 0x00;

extern int default_advr_expire;
extern UpnpDevice_Handle device_handle;

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

		pWiFiStats = (PWIFI_STAT_COUNTERS) MALLOC(sizeof(WIFI_STAT_COUNTERS));
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

int main(int argc, char **argv )
{
    unsigned int portTemp = 0;
    char* ip_address = NULL;
    char* desc_doc_name = NULL;
    char* web_dir_path = NULL;

    //ithread_t reset_thread;	

    int i = 0;
    tu_set_my_thread_name( __FUNCTION__ );

    startup_banner();

	/* 
	* libNvramInit initialize the semaphore in OpenWRT NVRAM.
	* This should be called before any APP_LOG call, otherwise OpenWRT PVT build will break
	*/
	libNvramInit();

    /*start logger*/
    initLogger();

    /* Initialize Belkin diagnostics, if enabled */
    init_diagnostics();

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
    /* 
     ** core initialization - part 2: Init WiFi, start UPnP and start required threads 
     ** argument (1/0) tells whether force start of Control point is desired or not respectively
     */
    core_init_late(0);

    //creating Network Reset thread, for monitoring 5 second reset button press
    //ithread_create(&reset_thread, NULL, ResetButtonTask, NULL);

    while(1)
    {
	pNode node = readMessage(PLUGIN_E_MAIN_THREAD);
	ProcessItcEvent(node);
    }

    /*close syslog logging*/
    deInitLogger();

    return EXIT_SUCCESS;
}
