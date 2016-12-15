/***************************************************************************
*
*
* wifiSetup.c
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
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include "global.h"
#include "defines.h"
#include "gpioApi.h"
#include "wifiSetup.h"
#include "logger.h"
#include "wifiHndlr.h"
#include "aes_inc.h"
#include "itc.h"
#include <ithread.h>
#include "watchDog.h"
#include "osUtils.h"
#include "utils.h"
#include "getRemoteMac.h"
#include "controlledevice.h"
#include "utlist.h"

#include "natClient.h"
#include "UDSClientHandler.h"
#include "remoteAccess.h"
#ifdef WeMo_INSTACONNECT
#include "instaConnectUPnPHandler.h"
#endif
#ifdef PRODUCT_WeMo_Insight
#include "insightUPnPHandler.h"
#endif
#ifdef WeMo_SMART_SETUP
#include "smartSetupUPnPHandler.h"
#endif
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

extern int gRa0DownFlag;
static int inetConnLostCount;
static int inetConnCount;
static int routerConnLostCount;
static int routerConnCount;

extern int UDS_getRemoteDataBytes();
extern void UDS_setRemoteDataBytes();

int gStartWiFiTest = 0;
int gDoDhcp=1;
int gBootReconnect=0;
char gPasswordKeyData[SIZE_64B];
int gWiFiConfigured;
extern int gDhcpcStarted;
extern int gSetupRequested;
extern char gUserKey[];
extern int gAppCalledCloseAp;
extern char 	g_szWiFiMacAddress[];
extern char 	g_szSerialNo[];
extern ithread_t CloseApWaiting_thread;
extern int g_channelAp;
extern int gDstSupported;
extern int gDstEnable;
extern float g_lastTimeZone;
extern int g_ra0DownFlag;
extern WIFI_PAIR_PARAMS gWiFiParams;
void reconnectHome (void) ;
pthread_attr_t reconn_attr,inet_attr,con_attr,hid_attr,closeap_attr,mon_attr, inet_mon_attr,ntp_attr;
pthread_t nwThd=0,reconnthread=0,closeApThId=0,ntpthread;
#ifdef _AP_SITE_SURVEY_
pthread_attr_t siteSur_attr;
#endif //_AP_SITE_SURVEY_
#ifdef WeMo_INSTACONNECT
pthread_t connInsta_thread = -1;
extern char gRouterSSID[SSID_LEN];
extern char gBridgeNodeList[];
extern char gProfileList[];
extern char gRouterProfileMAC[MAX_MAC_LEN];
extern int g_usualSetup;
extern int g_connectInstaStatus;
#define BR_IP_RANGE 253
#endif
int connthread=-1;
int ledstatus=-1;
int gNTPTimeSet=0;
int gistimerinit = 0;
int gReconnectFlag=0;
int gIsHiddenChan=0;
int gInetHealthPunch=0;
int gExitReconnectThread=0;
int gExitInetMonitorThread = 0;
extern int gSignalStrength;

#ifdef _OPENWRT_
int gNTPThreadRunning = 0;
#endif


#ifdef __WIRED_ETH__
extern int g_bWiredEthernet;
#endif

pthread_t WiFi_Connect_thread = -1, Hidden_Connect_thread=-1,WiFi_Monitor_thread=-1,Inet_Monitor_thread=0;
;
#ifdef USE_PING_DEFAULT
char bufAdr[SIZE_256B];
#endif

#define ISITE "4.2.2.2"
#define GOOGLE_DNS_1 "8.8.8.8"
#define GOOGLE_DNS_2 "8.8.4.4"
#define OPENDNS_HOME "208.67.222.222"
#define BELKIN_DOMAIN_NAME "www.belkin.com"
#define ROOTSERVER_DOMAIN_NAME "A.root-servers.net"
#define LED_TIME_OFF_INTERVAL 30
#define CLOSE_AP_TIMEOUT  600
#define SMART_AP_TIMEOUT  300
#define PASSWORD_SALT_LEN	(8+1)
#define PASSWORD_IV_LEN		(16+1)
#define PASSWORD_KEYDATA_LEN	SIZE_256B
#define CHECK_INET_TIMEVAL	1800
#define SUBNET_MASK_24		"255.255.255.0"
#define SUBNET_MASK_8		"255.0.0.0"

#define IS_FIRMWARE_FLASHING (getCurrFWUpdateState() == FM_STATUS_UPDATE_STARTING)
int gInetSleepInterval = CHECK_INET_TIMEVAL; //default value 30 minutes
static pthread_mutex_t   s_setup_req_mutex;
extern void *CloseApWaitingThread(void *args);
void *InetMonitorTask(void *arg);



#define CHECK_AND_INVOKE_NAT_REINIT(initType)	{\
	if(gpluginRemAccessEnable && UDS_pluginNatInitialized())\
	{\
		if(initType){\
			UDS_invokeNatReInit(NAT_REINIT);}\
		else{\
			UDS_invokeNatDestroy();}\
	}\
}

int gPassPlainTextLen =0;
unsigned int gLocalDataBytes = 0;

void StopInetTask(void)
{
    if(Inet_Monitor_thread)
    {
       APP_LOG("WiFiApp", LOG_CRIT,"Killing Inet_Monitor_Thread");
       pthread_kill(Inet_Monitor_thread, SIGRTMIN);
    }
    pluginUsleep(500000);
    if(nwThd)
    {
       APP_LOG("WiFiApp", LOG_CRIT,"Killing checkInetConnect thread");
       pthread_kill(nwThd, SIGRTMIN);
    }
    return;
}

void ExitCheckInetConnectThread(void)
{
	gExitReconnectThread = 1;
}

void ExitInetMonitorThread(void)
{
	gExitInetMonitorThread = 1;
}

#if !defined(PRODUCT_WeMo_Baby) && !defined(PRODUCT_WeMo_Streaming)

int SetCurrentClientState(int curState)
{
	osUtilsGetLock(&s_client_state_mutex);
	gWiFiClientCurrState = curState;
	osUtilsReleaseLock(&s_client_state_mutex);

	UDS_SetCurrentClientState();
	return 0x00;
}

#endif

void checkScanFailCount (int count, int *scanFailCnt, int maxFailCnt)
{
    if(count == 0)
    {
  (*scanFailCnt)++;
  APP_LOG("checkScanFailCount", LOG_CRIT,"No networks found in SiteScan: %d...", *scanFailCnt);
  system("ifconfig apcli0 up");
    }
    else
  *scanFailCnt=0;

#ifdef DRACONIAN_WIFI_SCAN
    if(*scanFailCnt == maxFailCnt)
    {
  APP_LOG("checkScanFailCount", LOG_ALERT,"No networks found in SiteScan for consecutive %d times..., rebooting!!!", *scanFailCnt);
  if (gpluginRemAccessEnable && UDS_pluginNatInitialized()) {
      UDS_invokeNatDestroy();
  }
  system("reboot");
    }
#endif
}

void initClientStatus()
{
    osUtilsCreateLock(&s_client_state_mutex);
}


void initSetupMutex()
{
    osUtilsCreateLock(&s_setup_req_mutex);
}

int setSetupRequested(int state)
{
	osUtilsGetLock(&s_setup_req_mutex);
	gSetupRequested=state;
	osUtilsReleaseLock(&s_setup_req_mutex);

	return 0x00;
}

int isSetupRequested(void)
{
	int state;
	osUtilsGetLock(&s_setup_req_mutex);
	state = gSetupRequested;
	osUtilsReleaseLock(&s_setup_req_mutex);

	return state;
}

#ifdef WeMo_INSTACONNECT

void inline handleInstaConnectFailure(void)
{
	g_connectInstaStatus = APCLI_UNCONFIGURED;
	memset(gRouterSSID, 0, sizeof(gRouterSSID));
	g_usualSetup = 0x00;
	gAppCalledCloseAp = 0x00;
}

/*API getDestinationIP will return destination ip in pOutIpDest buffer*/
void getDestinationIP(char *pIp, char *pNetmask, char *pOutIpDest)
{
	char *ipDest = NULL;
	int intValueOfIP = -1,intValueOfNetmask = -1,intVlaueOfDestIP = -1;
	struct in_addr addr;

	/*convert ip address in decimal*/
	intValueOfIP = htonl(inet_network(pIp));

	/*convert ip mask in decimal*/
	intValueOfNetmask = htonl(inet_network(pNetmask));

	/*arithmatic add to get destination ip in decimal*/
	intVlaueOfDestIP = intValueOfIP & intValueOfNetmask;

	/*get destination address in dot form*/
	addr.s_addr = intVlaueOfDestIP;
	ipDest = inet_ntoa(addr);

	/*copy to out destip buffer */
	strncpy(pOutIpDest, ipDest, SIZE_16B);

	APP_LOG("WiFiApp", LOG_ERR, "bridgeIfaceRouteConfig::\npIp:%s\npNetmask:%s\nintValueOfIP:%d\nintValueOfNetmask:%d\nintVlaueOfDestIP:%d\npOutIpDest:%s",pIp,pNetmask,intValueOfIP,intValueOfNetmask,intVlaueOfDestIP,pOutIpDest);
}

void delBridgeCliIfaceAndRoute(void)
{
	char subnet[SIZE_32B];
	char cmdBuf[SIZE_256B];
	char *pNet = NULL;

	/* remove apcli0 from bridge, to get ip address from router*/
	memset (cmdBuf,0x0,sizeof(cmdBuf));
	snprintf(cmdBuf, SIZE_32B, "brctl delif %s %s",INTERFACE_BR, INTERFACE_CLIENT);
	APP_LOG("WiFiApp", LOG_DEBUG, "command: %s", cmdBuf);
	system (cmdBuf);

	/* remove ra2 from bridge, to get ip address from router*/
	memset (cmdBuf,0x0,sizeof(cmdBuf));
	snprintf(cmdBuf, SIZE_32B, "brctl delif %s %s",INTERFACE_BR, INTERFACE_BRIDGE);
	APP_LOG("WiFiApp", LOG_DEBUG, "command: %s", cmdBuf);
	system (cmdBuf);

	/* remove br0 subnet route entry to ping router*/
	memset (subnet,0,sizeof(subnet));
	memset (cmdBuf,0,sizeof(cmdBuf));
	strncpy(subnet, GetWanDefaultGateway(), sizeof(subnet)-1);
	pNet = strrchr(subnet, '.');
	subnet[(pNet++)-subnet+1] = '0';
	subnet[pNet-subnet+1] = '\0';
	//subnet[strlen(subnet)-1] = '0';
	//snprintf(cmdBuf, sizeof(cmdBuf), "route del -net %s netmask %s dev %s", subnet, GetWanSubnetMask(), INTERFACE_BR);
	snprintf(cmdBuf, sizeof(cmdBuf), "route del -net %s netmask %s dev %s", subnet, SUBNET_MASK_24, INTERFACE_BR);
	APP_LOG("WiFiApp", LOG_DEBUG, "command: %s", cmdBuf);
	system(cmdBuf);

	memset (cmdBuf,0,sizeof(cmdBuf));
	snprintf(cmdBuf, sizeof(cmdBuf), "route del -net %s netmask %s dev %s", subnet, SUBNET_MASK_8, INTERFACE_BR);
	APP_LOG("WiFiApp", LOG_DEBUG, "command: %s", cmdBuf);
	system (cmdBuf);

}

void connectToInstaApThread(void)
{
	int retVal;
	pthread_attr_t connInsta_attr;
	APP_LOG("UPNP",LOG_DEBUG, "***************create Connect to Insta Ap Thread***************");

	pthread_attr_init(&connInsta_attr);
	pthread_attr_setdetachstate(&connInsta_attr, PTHREAD_CREATE_DETACHED);
	retVal = pthread_create(&connInsta_thread, &connInsta_attr, (void*)&connectToInstaApTask, NULL);
	if(retVal < SUCCESS)
	{
	    APP_LOG("WiFiApp",LOG_ALERT, "Connect to Insta AP Thread not created, reset app");
	    resetSystem();
	}
	else
	{
	    APP_LOG("WiFiApp",LOG_DEBUG, "Connect to Insta AP Thread created successfully..");
	}
}

int connectToInstaAP(int chan)
{
    WIFI_PAIR_PARAMS WiFi;
    int i=0,ret=0,retVal=0;
    char cmdBuf[SIZE_32B];
    char param[SIZE_20B] = {0};
    param[i++] = 'p',param[i++] ='r',param[i++] ='o',param[i++] ='j',param[i++] ='e',param[i++] ='c',param[i++] ='t',param[i++] ='I',param[i++] ='n',param[i++] ='s',param[i++] ='t',param[i++] ='a';


    APP_LOG("WiFiApp", LOG_DEBUG,"insta AP channel: %d", chan);

    memset(&WiFi, 0, sizeof(WIFI_PAIR_PARAMS));

    strncpy(WiFi.SSID,INSTA_AP_SSID, sizeof(WiFi.SSID)-1);
    strncpy(WiFi.AuthMode,INSTA_AP_AUTH, sizeof(WiFi.AuthMode)-1);
    strncpy(WiFi.EncrypType,INSTA_AP_ENC, sizeof(WiFi.EncrypType)-1);
    strncpy(WiFi.Key,param, sizeof(WiFi.Key)-1);
    WiFi.channel = chan;

    ret = wifiPair(&WiFi, INTERFACE_CLIENT);
    if (ret >= SUCCESS)
    {
	    APP_LOG("WiFiApp", LOG_DEBUG,"Pairing Successful");
	    APP_LOG("WiFiApp", LOG_DEBUG, "WiFi physical connection established and dhcpc started");
	    if(STATE_INTERNET_NOT_CONNECTED == (ret = isValidIp()))
		  ret = STATE_CONNECTED;

	    /* router connected condition: delete multicast route, bring ra0 down, set r0 down flag to let site survey thread to exit and start upnp on client interface */
	    if(STATE_CONNECTED == ret)
	    {
		    APP_LOG("WiFiApp", LOG_DEBUG,"State: Connection OK");
		    retVal = system("route del -net 239.0.0.0 netmask 255.0.0.0");  //TODO: need to check if required or not
		    memset(cmdBuf, 0x0, SIZE_32B);
		    snprintf(cmdBuf, SIZE_32B, "ifconfig %s down", INTERFACE_AP);
		    system(cmdBuf);
		    //g_ra0DownFlag = 1;	//to stop site survey thread: may not required to stop site survey till device is configured
		    pluginUsleep(1000000);
		    NotifyInternetConnected();
	    }
	    else
	    {
		    APP_LOG("NetworkControl", LOG_ERR, "########## Pairing failure-Connect ***********************");
		    StopDhcpRequest();
		    ret = INVALID_PARAMS;
	    }
    }
    else
    {
	APP_LOG("WiFiApp", LOG_DEBUG,"Pairing failure");
        ret = INVALID_PARAMS;
    }

    return ret;
}

void setBridgeControlEx(const char *pAuth, const char *pEnc, const char *pPass)
{
    char *pIp = NULL,*pGateway = NULL, *pNetmask = NULL;
    char cmdBuf[SIZE_1024B];
    char tmpcmdBuf[SIZE_128B];
    char subnet[SIZE_32B];
    char pCommand[SIZE_64B];

    memset (cmdBuf,0,sizeof(cmdBuf));
    memset (tmpcmdBuf,0,sizeof(tmpcmdBuf));
    memset (subnet,0,sizeof(subnet));

    /* get network info */
    pIp = GetWanIPAddress ();
    pGateway = GetWanDefaultGateway();
    pNetmask = GetWanSubnetMask();
    strncpy(subnet, pGateway, sizeof(subnet)-1);
    subnet[strlen(subnet)-1] = '\0';

    /* client interface commands */
    snprintf(cmdBuf, sizeof(cmdBuf), "brctl addif %s %s;",INTERFACE_BR, INTERFACE_CLIENT);
    snprintf(tmpcmdBuf, sizeof(tmpcmdBuf), "ifconfig %s %s;", INTERFACE_BR, pIp);
    strncat(cmdBuf, tmpcmdBuf, sizeof(cmdBuf)-strlen(cmdBuf)-1);

    /* route commands */
    memset (tmpcmdBuf,0,sizeof(tmpcmdBuf));
    strncpy(tmpcmdBuf, "route del default;", sizeof(tmpcmdBuf)-1);
    strncat(cmdBuf, tmpcmdBuf, sizeof(cmdBuf)-strlen(cmdBuf)-1);
    memset (tmpcmdBuf,0,sizeof(tmpcmdBuf));
    snprintf(tmpcmdBuf, sizeof(tmpcmdBuf), "route del -net %s netmask %s dev %s;", subnet, pNetmask, INTERFACE_CLIENT);
    strncat(cmdBuf, tmpcmdBuf, sizeof(cmdBuf)-strlen(cmdBuf)-1);
    memset (tmpcmdBuf,0,sizeof(tmpcmdBuf));
    snprintf(tmpcmdBuf, sizeof(tmpcmdBuf), "route add default gw %s %s;", pGateway, INTERFACE_BR);
    strncat(cmdBuf, tmpcmdBuf, sizeof(cmdBuf)-strlen(cmdBuf)-1);
    memset (tmpcmdBuf,0,sizeof(tmpcmdBuf));
    snprintf(tmpcmdBuf, sizeof(tmpcmdBuf), "route add -net %s netmask %s dev %s;", subnet, pNetmask, INTERFACE_BR);
    strncat(cmdBuf, tmpcmdBuf, sizeof(cmdBuf)-strlen(cmdBuf)-1);

    /* dhcp server command */
    memset (tmpcmdBuf,0,sizeof(tmpcmdBuf));
    strncpy(tmpcmdBuf, "killall -9 udhcpd;", sizeof(tmpcmdBuf)-1);
    strncat(cmdBuf, tmpcmdBuf, sizeof(cmdBuf)-strlen(cmdBuf)-1);

    /* bridge AP commands */
    memset (tmpcmdBuf,0,sizeof(tmpcmdBuf));
    snprintf(tmpcmdBuf, sizeof(tmpcmdBuf), "ifconfig %s up;", INTERFACE_BRIDGE);
    strncat(cmdBuf, tmpcmdBuf, sizeof(cmdBuf)-strlen(cmdBuf)-1);
    memset (tmpcmdBuf,0,sizeof(tmpcmdBuf));
    snprintf(tmpcmdBuf, sizeof(tmpcmdBuf), "brctl addif %s %s;", INTERFACE_BR, INTERFACE_BRIDGE);
    strncat(cmdBuf, tmpcmdBuf, sizeof(cmdBuf)-strlen(cmdBuf)-1);

    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);

    /* execute commands */
    system (cmdBuf);

    /* bridge AP iwpriv commands */
    memset(pCommand, '0', sizeof(pCommand));
    strncpy(pCommand, "HideSSID=1", sizeof(pCommand)-1);
    wifiSetCommand (pCommand, INTERFACE_BRIDGE);
    memset(pCommand, '0', sizeof(pCommand));
    snprintf(pCommand, sizeof(pCommand), "AuthMode=%s", pAuth);
    wifiSetCommand (pCommand, INTERFACE_BRIDGE);
    memset(pCommand, '0', sizeof(pCommand));
    snprintf(pCommand, sizeof(pCommand), "EncrypType=%s", pEnc);
    wifiSetCommand (pCommand, INTERFACE_BRIDGE);
    if(!strcmp(pEnc, "WEP"))
    {
	memset(pCommand, '0', sizeof(pCommand));
	snprintf(pCommand, sizeof(pCommand), "Key1=%s", pPass);
	wifiSetCommand (pCommand, INTERFACE_BRIDGE);
    }
    else
    {
	memset(pCommand, '0', sizeof(pCommand));
	snprintf(pCommand, sizeof(pCommand), "WPAPSK=%s", pPass);
	wifiSetCommand (pCommand, INTERFACE_BRIDGE);
    }

}

/* undo Insta Ap settings */
void undoInstaApSettings(void)
{
    char cmdBuf[SIZE_32B];

    memset(cmdBuf, 0x0, SIZE_32B);
    snprintf(cmdBuf, SIZE_32B, "ifconfig %s up", INTERFACE_AP);
    system(cmdBuf);

    memset(cmdBuf, 0x0, SIZE_32B);
    snprintf(cmdBuf, SIZE_32B, "ifconfig %s 0.0.0.0", INTERFACE_CLIENT);
    system(cmdBuf);
}

void bridgeIfaceRouteConfig(void)
{
    char *pIp = NULL,*pGateway = NULL, *pNetmask = NULL;
    char cmdBuf[SIZE_512B];
    char subnet[SIZE_32B];
    char apSubnet[SIZE_32B];
    char *pNet, *pApNet, *pIpaddr = NULL;
    int ipLastOct = 0;

    memset (cmdBuf,0,sizeof(cmdBuf));
    memset (subnet,0,sizeof(subnet));
    memset (apSubnet,0,sizeof(apSubnet));

    /* network info */
    pIp = GetWanIPAddress ();
    pNetmask = GetWanSubnetMask();
    pGateway = GetWanDefaultGateway();

    strncpy(subnet, pGateway, sizeof(subnet)-1);
    pNet = strrchr(subnet, '.');
    subnet[(pNet++)-subnet+1] = '0';
    subnet[pNet-subnet+1] = '\0';
    //subnet[strlen(subnet)-1] = '0';

    strncpy(apSubnet, WAN_IP_ADR, sizeof(apSubnet)-1);
    pApNet = strrchr(apSubnet, '.');
    apSubnet[(pApNet++)-apSubnet+1] = '0';
    apSubnet[pApNet-apSubnet+1] = '\0';
    //apSubnet[strlen(apSubnet)-1] = '0';

    /* delete route commands */
    memset (cmdBuf,0,sizeof(cmdBuf));
    strncpy(cmdBuf, "route del default", sizeof(cmdBuf)-1);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    system (cmdBuf);
    memset (cmdBuf,0,sizeof(cmdBuf));
    snprintf(cmdBuf, sizeof(cmdBuf), "route del -net %s netmask %s dev %s", subnet, pNetmask, INTERFACE_CLIENT);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    system (cmdBuf);
    memset (cmdBuf,0,sizeof(cmdBuf));
    snprintf(cmdBuf, sizeof(cmdBuf), "route del -net %s netmask %s dev %s", apSubnet, pNetmask, INTERFACE_BR);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    system (cmdBuf);
    memset (cmdBuf,0,sizeof(cmdBuf));
    strncpy(cmdBuf, "route del -net 127.0.0.0 netmask 255.0.0.0 dev lo", sizeof(cmdBuf)-1);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    system (cmdBuf);

    /* bridge commands */
    memset (cmdBuf,0,sizeof(cmdBuf));
    snprintf(cmdBuf, sizeof(cmdBuf), "brctl addif %s %s", INTERFACE_BR, INTERFACE_CLIENT);
    system (cmdBuf);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    memset (cmdBuf,0,sizeof(cmdBuf));
    pIpaddr = strrchr(pIp, '.');
    ipLastOct = atoi(++pIpaddr);
    ipLastOct = BR_IP_RANGE - ipLastOct;
    sprintf(pIp+(pIpaddr-pIp), "%d", ipLastOct);
    snprintf(cmdBuf, sizeof(cmdBuf), "ifconfig %s %s", INTERFACE_BR, pIp);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    system (cmdBuf);

    /* delete route commands */
    memset (cmdBuf,0,sizeof(cmdBuf));
    snprintf(cmdBuf, sizeof(cmdBuf), "route del -net %s netmask %s dev %s", subnet, SUBNET_MASK_24, INTERFACE_BR);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    system (cmdBuf);

    memset (cmdBuf,0,sizeof(cmdBuf));
    snprintf(cmdBuf, sizeof(cmdBuf), "route del -net %s netmask %s dev %s", subnet, SUBNET_MASK_8, INTERFACE_BR);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    system (cmdBuf);

#if 0
    memset (cmdBuf,0,sizeof(cmdBuf));
    snprintf(cmdBuf, sizeof(cmdBuf), "route del -net %s netmask %s dev %s", subnet, pNetmask, INTERFACE_BR);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    system (cmdBuf);
#endif

    /* add route commands */
    memset (cmdBuf,0,sizeof(cmdBuf));
    snprintf(cmdBuf, sizeof(cmdBuf), "route add -net %s netmask %s dev %s", subnet, pNetmask, INTERFACE_BR);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    system (cmdBuf);

    memset (cmdBuf,0,sizeof(cmdBuf));
    snprintf(cmdBuf, sizeof(cmdBuf), "route add default gw %s %s", pGateway, INTERFACE_BR);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    system (cmdBuf);

    /* bridge AP iwpriv commands */
    memset (cmdBuf,0,sizeof(cmdBuf));
    snprintf(cmdBuf, sizeof(cmdBuf), "ifconfig %s down;", INTERFACE_BRIDGE);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    system (cmdBuf);

#if 0
    memset (cmdBuf,0,sizeof(cmdBuf));
    snprintf(cmdBuf, sizeof(cmdBuf), "route add -net %s netmask %s dev %s", subnet, pNetmask, INTERFACE_BR);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    system (cmdBuf);
#endif

#if 0
    /* bridge AP iwpriv commands */
    memset (cmdBuf,0,sizeof(cmdBuf));
    snprintf(cmdBuf, sizeof(cmdBuf), "ifconfig %s down;", INTERFACE_BRIDGE);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    system (cmdBuf);
#endif

}

void bridgeIfaceSecurityConfig(const char *pSsid, const char *pAuth, const char *pEnc, const char *pPass, int channel)
{
    char cmdBuf[SIZE_128B];
    char pCommand[SIZE_64B];
    int ret = -1;
    char szBuff[MAX_APSSID_LEN];
    char bufferAuth[SIZE_64B];
    char bufferEnc[SIZE_64B];
    char bufferSSID[SIZE_64B+1];

    memset (cmdBuf,0,sizeof(cmdBuf));
    memset(szBuff, 0, sizeof(szBuff));
    memset(bufferAuth, 0, sizeof(bufferAuth));
    memset(bufferEnc, 0, sizeof(bufferEnc));
    memset(bufferSSID, 0, sizeof(bufferSSID));

    strncpy(bufferAuth,pAuth,sizeof(bufferAuth)-1);
    strncpy(bufferEnc,pEnc,sizeof(bufferEnc)-1);
    strncpy(bufferSSID,pSsid,sizeof(bufferSSID)-1);

    GetTrailSerial(szBuff);

    memset(pCommand, 0, sizeof(pCommand));
    snprintf(pCommand, sizeof(pCommand), "Channel=%d", channel);
    ret = wifiSetCommand (pCommand,INTERFACE_BRIDGE);
    if(ret < 0)
    {
	    APP_LOG("NetworkControl", LOG_ERR, "%s - failed", pCommand);
    }
    APP_LOG("NetworkControl", LOG_DEBUG, "**********Command Set: %s",pCommand);

    if(!strcmp (bufferAuth,"WPA1PSKWPA2PSK") || (!strcmp(bufferAuth,"WPAPSKWPA2PSK")))
    {
	    //Disallow TKIP Option - For Cisco E3000 Router
	    if (IsNetworkMixedMode(bufferSSID))
	    {
		memset(pCommand, 0, sizeof(pCommand));
		strncpy(pCommand, "WirelessMode=0", sizeof(pCommand)-1);
		ret = wifiSetCommand (pCommand,INTERFACE_BRIDGE);
		if(ret < 0)
		{
		    APP_LOG("NetworkControl", LOG_ERR, "%s - failed", pCommand);
		}
		APP_LOG("NetworkControl", LOG_DEBUG, "**********Command Set: %s",pCommand);
		APP_LOG("NetworkControl", LOG_DEBUG, "iwpriv apcli0 set WirelessMode=0 command executed");
	    }

	    memset(pCommand, 0, sizeof(pCommand));
	    strncpy(pCommand, "HtDisallowTKIP=1", sizeof(pCommand)-1);
	    ret = wifiSetCommand (pCommand, INTERFACE_BRIDGE);
	    if(ret < 0)
	    {
		    APP_LOG("WiFiApp", LOG_ERR, "%s - failed", pCommand);
	    }
	    APP_LOG("WiFiApp", LOG_DEBUG, "**********Command Set: %s\n",pCommand);

	    memset(bufferAuth, 0, sizeof(bufferAuth));
	    strncpy(bufferAuth,"WPAPSKWPA2PSK",sizeof(bufferAuth)-1);
    }
    else if (!strcmp(bufferAuth,"WEP"))
    {
	    strncpy(bufferAuth, "WEPAUTO", sizeof(bufferAuth)-1);
	    strncpy(bufferEnc,"WEP", sizeof(bufferEnc)-1);
    }

    memset(pCommand, 0, sizeof(pCommand));
    snprintf(pCommand, sizeof(pCommand), "AuthMode=%s", bufferAuth);
    ret = wifiSetCommand (pCommand, INTERFACE_BRIDGE);
    if(ret < 0)
    {
	    APP_LOG("WiFiApp", LOG_ERR, "%s - failed", pCommand);
    }
    APP_LOG("WiFiApp", LOG_DEBUG, "**********Command Set: %s",pCommand);
    ret = -1;
    memset(pCommand, 0, sizeof(pCommand));
    snprintf(pCommand, sizeof(pCommand), "EncrypType=%s", bufferEnc);
    ret = wifiSetCommand (pCommand, INTERFACE_BRIDGE);
    if(ret < 0)
    {
	    APP_LOG("WiFiApp", LOG_ERR, "%s - failed", pCommand);
    }
    APP_LOG("WiFiApp", LOG_DEBUG, "**********Command Set: %s",pCommand);

    if (strcmp(bufferEnc, "WEP") != 0)	//check added
    {
	ret = -1;
	memset(pCommand, 0, sizeof(pCommand));
	snprintf(pCommand, sizeof(pCommand), "SSID=WeMo.Bridge.%s", szBuff);
	ret = wifiSetCommand (pCommand, INTERFACE_BRIDGE);
	if(ret < 0)
	{
		APP_LOG("WiFiApp", LOG_ERR, "%s - failed", pCommand);
	}
	APP_LOG("WiFiApp", LOG_DEBUG, "**********Command Set: %s",pCommand);
    }

    if (strcmp(bufferEnc, "NONE") != 0x0)   //check added
    {
	ret = -1;
	if(!strcmp(bufferEnc, "WEP"))
	{
	    // set default key id
	    memset(pCommand, 0, sizeof(pCommand));
	    strncpy(pCommand, "DefaultKeyID=1", sizeof(pCommand)-1);
	    ret = wifiSetCommand(pCommand, INTERFACE_BRIDGE);
	    if(ret < 0)
	    {
		    APP_LOG("NetworkControl", LOG_ERR,"%s - failed", pCommand);
	    }
	    APP_LOG("WiFiApp", LOG_DEBUG, "**********Command Set: %s",pCommand);
	    memset(pCommand, 0, sizeof(pCommand));
	    snprintf(pCommand, sizeof(pCommand), "Key1=%s", pPass);
	    ret = wifiSetCommand (pCommand, INTERFACE_BRIDGE);
	    if(ret < 0)
	    {
		    APP_LOG("WiFiApp", LOG_ERR, "%s - failed", pCommand);
	    }
	    APP_LOG("WiFiApp", LOG_DEBUG, "**********Command Set: %s",pCommand);
	}
	else
	{
	    memset(pCommand, 0, sizeof(pCommand));
	    snprintf(pCommand, sizeof(pCommand), "WPAPSK=%s", pPass);
	    ret = wifiSetCommand (pCommand, INTERFACE_BRIDGE);
	    if(ret < 0)
	    {
		    APP_LOG("WiFiApp", LOG_ERR, "%s - failed", pCommand);
	    }
	    APP_LOG("WiFiApp", LOG_DEBUG, "**********Command Set: %s",pCommand);
	}

	if (strcmp(bufferEnc, "TKIP") != 0x0)	//additional check added
	{
	    ret = -1;
	    memset(pCommand, 0, sizeof(pCommand));
	    snprintf(pCommand, sizeof(pCommand), "SSID=WeMo.Bridge.%s", szBuff);
	    ret = wifiSetCommand (pCommand, INTERFACE_BRIDGE);
	    if(ret < 0)
	    {
		    APP_LOG("WiFiApp", LOG_ERR, "%s - failed", pCommand);
	    }
	    APP_LOG("WiFiApp", LOG_DEBUG, "**********Command Set: %s",pCommand);
	}
    }

    pluginUsleep(500000);   //500 ms

    /* bridge AP commands */
    memset(pCommand, 0, sizeof(pCommand));
    strncpy(pCommand, "HideSSID=1", sizeof(pCommand)-1);
    ret = wifiSetCommand (pCommand, INTERFACE_BRIDGE);
    if(ret < 0)
    {
	    APP_LOG("WiFiApp", LOG_ERR, "%s - failed", pCommand);
    }
    APP_LOG("WiFiApp", LOG_DEBUG, "**********Command Set: %s",pCommand);

    memset (cmdBuf,0,sizeof(cmdBuf));
    snprintf(cmdBuf, sizeof(cmdBuf), "ifconfig %s up;", INTERFACE_BRIDGE);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    system (cmdBuf);
    memset (cmdBuf,0,sizeof(cmdBuf));
    snprintf(cmdBuf, sizeof(cmdBuf), "brctl addif %s %s;", INTERFACE_BR, INTERFACE_BRIDGE);
    APP_LOG("WiFiApp", LOG_ERR, "Bridge control command: %s", cmdBuf);
    system (cmdBuf);

}

/* configure Bridge Ap, commands could fail but method will continue in that case too */
void configureBridgeAp(const char *pSsid, const char *pAuth, const char *pEnc, const char *pPass, int channel)
{
    char cmdBuf[SIZE_64B];

    bridgeIfaceRouteConfig();
    bridgeIfaceSecurityConfig(pSsid, pAuth, pEnc, pPass, channel);

    /* stop dhcp server command */
    memset (cmdBuf,0,sizeof(cmdBuf));
    strncpy(cmdBuf, "killall -9 udhcpd", sizeof(cmdBuf)-1);
    APP_LOG("WiFiApp", LOG_ERR, "DHCP command: %s", cmdBuf);
    system (cmdBuf);

#ifdef WeMo_INSTACONNECT
    /*set bridge Beacon IE*/
    APP_LOG("WiFiApp", LOG_ERR, "Seting ApSpecBeaconIE");
    setApSpecBeaconIE();
#endif
}

#endif //end of WeMo_INSTACONNECT

/************************************************************************
 * Function: initWiFiHandler
 *     Initialize settings configuration source.
 *  Parameters:
 *    None.
 *  Return:
 *     SUCCESS/FAILURE
 ************************************************************************/
int initWiFiHandler ()
{
    int retVal;

    memset (&gWiFiParams,0,sizeof(WIFI_PAIR_PARAMS));
#ifdef USE_PING_DEFAULT
    memset(bufAdr,0,SIZE_256B);
#endif

    initClientStatus();
    initSetupMutex();
    initSiteSurveyStateLock();

    retVal = wifiCheckConfigAvl();

#ifdef NFC_CONFIG
    if(FAILURE == retVal) {
	// We didn't find WiFi parameters in EEPROM, try to read
   // them from the NFC chip.
		 void StartNfcThread(void);
		 StartNfcThread();
	 }
#endif

    if(FAILURE == retVal) {
	APP_LOG("WiFiApp", LOG_ERR, "No Data in Flash, State 5: Setup mode");
	SetWiFiLED(0x05);
	gBootReconnect=0;
        APP_LOG("WiFiApp",LOG_ERR, "gBootReconnect=0");

        //Create a timer task to monitor if Installation begins-TODO
        //Create a thread to look for connection happens.-TODO.
#ifdef _AP_SITE_SURVEY_
  ithread_t siteSur_thread;
#endif //_AP_SITE_SURVEY_
	g_ra0DownFlag = 0; //RA0 interface is UP
#ifdef _AP_SITE_SURVEY_
	APP_LOG("UPNP",LOG_DEBUG, "***************SiteSurvey Thread created***************\n");
	pthread_attr_init(&siteSur_attr);
	pthread_attr_setdetachstate(&siteSur_attr, PTHREAD_CREATE_DETACHED);
	ithread_create(&siteSur_thread, &siteSur_attr, siteSurveyPeriodic, NULL);
#endif
#ifdef WeMo_INSTACONNECT
	connectToInstaApThread();
#endif

    } else {
	    setAPIfState("OFF");
	    g_ra0DownFlag = 1; //RA0 interface is Down
       //Get Parameters from Flash and switch to Client Mode.-TODO
    }


    return retVal;
}



/**
 * @brief  base64Decode: Decodes the string that is base64 encoded
 * @param  sEnc   [IN]  - Encoded string.
 * @param  sDec   [OUT] - Decoded string.
 * @param  decLen [OUT] - Decoded string length.
 *
 * @return retVal: 0: On Success.
 *                -1: On Failure
 *
 * @author Christopher A F
 */
char* base64Decode (char *sEnc,  unsigned int *decLen)
{
    int              retVal    = SUCCESS;
    char *sDec = NULL;
    unsigned int     len       = 0;
    unsigned int     decBufLen = 0;

    BIO             *bio       = NULL;
    BIO             *b64       = NULL;

    APP_LOG ("WiFiApp", LOG_DEBUG, "***** Entered *****");

    /* Input parameter validation */
    if ((NULL == sEnc) || (NULL == decLen))
    {
        APP_LOG ("WiFiApp", LOG_DEBUG, "%d: Invalid arguments", __LINE__);
        retVal = INVALID_PARAMS;
        goto CLEAN_RETURN;
    }

    decBufLen = *decLen;

    sDec = (char *) MALLOC (decBufLen+1);

    b64 = BIO_new (BIO_f_base64());
    bio = BIO_new_mem_buf (sEnc, strlen (sEnc));

    if(!b64 || !bio)
    {
    	APP_LOG ("WiFiApp", LOG_DEBUG, "Either b64 or bio is null");
	if(bio)
          BIO_free_all (bio);
        retVal = FAILURE;
        goto CLEAN_RETURN;
    }

    BIO_set_flags (b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push (b64, bio);
    len = BIO_read (bio, sDec, decBufLen);

    APP_LOG ("WiFiApp", LOG_DEBUG, "Base64 Decoded Message len = %d", len);

    if (len > decBufLen)
    {
        BIO_free_all (bio);
        retVal = FAILURE;
        goto CLEAN_RETURN;
    }

    *decLen = len;

    BIO_free_all (bio);

CLEAN_RETURN:

    if(retVal)
    {
	if(sDec)
	{
	  free(sDec);
	  sDec = NULL;
	}
    }

    return sDec;
}


void createMacKeyData(char *KeyData)
{
    	char routerMac[MAX_MAC_LEN];
	int i = 0;

        memset(routerMac, 0, sizeof(routerMac));
        memset(KeyData,0,sizeof(KeyData));

	if(strlen(gGatewayMAC) > 0x0)
	{
	    strncpy(routerMac, gGatewayMAC, sizeof(routerMac)-1);
	    for(i = 0; routerMac[i]; i++){
		routerMac[i] = tolower(routerMac[i]);
	    }
	}
        /* copy first 4 bytes of the router MAC address */
        memcpy(KeyData, routerMac, 4);
        /* Append last 4 bytes of MAC */
        strncat(KeyData, routerMac+8, 4);
        /* Now copy middle 4 bytes of the router MAC address */
        strncat(KeyData, routerMac+4, 4);

        APP_LOG("WiFiApp", LOG_HIDE, "key data: %s", KeyData);
}

void createSignatureKeyData(char *SignKeyData)
{
    	char routerMac[MAX_MAC_LEN];
    	char routerssid[MAX_ESSID_LEN];
	int i = 0;

        memset(routerMac, 0, sizeof(routerMac));
        memset(routerssid, 0, sizeof(routerssid));
        memset(SignKeyData,0,sizeof(SignKeyData));

        getRouterEssidMac (routerssid, routerMac, INTERFACE_CLIENT);
#ifdef WeMo_INSTACONNECT
	/* in case of insta connect mode, replace Essid with router ssid */
	if(!g_usualSetup)
	{
	    APP_LOG("WiFiApp", LOG_DEBUG,"Insta connect setup mode...");
	    if (strlen(gRouterSSID) > 0x0)
	    {
		APP_LOG("WiFiApp", LOG_DEBUG,"replace essid with router ssid for insta");
		memset(routerssid, 0, sizeof(routerssid));
		strncpy(routerssid, gRouterSSID, sizeof(routerssid)-1);
	    }
	}
#endif
	if(strlen(gGatewayMAC) > 0x0)
	{
	    memset(routerMac, 0, sizeof(routerMac));
	    strncpy(routerMac, gGatewayMAC, sizeof(routerMac)-1);
	    for(i = 0; routerMac[i]; i++){
		routerMac[i] = tolower(routerMac[i]);
	    }
	}
        /* copy 3 MSB of the router MAC address */
        memcpy(SignKeyData, routerMac, 6);
        /* Append the  router ssid */
        strcat(SignKeyData, routerssid);
        /* Now copy 3 LSB of the router MAC address */
        strcat(SignKeyData, routerMac+6);

        APP_LOG("WiFiApp", LOG_HIDE, "Signature key data: %s",SignKeyData);
}

void createPasswordKeyData()
{
        memset(gPasswordKeyData,0,sizeof(gPasswordKeyData));
        /* copy 3 MSB of the MAC address */
        memcpy(gPasswordKeyData, g_szWiFiMacAddress, 6);
        /* Append the  serial number */
        strncat(gPasswordKeyData,g_szSerialNo, sizeof(gPasswordKeyData)-strlen(gPasswordKeyData)-1);
        /* Now copy 3 LSB of the MAC address */
        strncat(gPasswordKeyData,g_szWiFiMacAddress+6, sizeof(gPasswordKeyData)-strlen(gPasswordKeyData)-1);

        APP_LOG("WiFiApp", LOG_HIDE, "Password key data: %s",gPasswordKeyData);
}

#ifdef WeMo_INSTACONNECT
void createRouterInfoKeyData(char *KeyData)
{
    char routerMac[MAX_MAC_LEN];
    char routerssid[MAX_ESSID_LEN];
    char *addSalt = "b3{8t;80dIN{ra83eC1s?M70?683@2Yf"; //32 bytes
    int i;

    memset(routerMac, 0, sizeof(routerMac));
    memset(routerssid, 0, sizeof(routerssid));
    memset(KeyData,0,sizeof(KeyData));

    if(strlen(gRouterProfileMAC))
    {
        APP_LOG("WiFiApp", LOG_DEBUG, "use profile mac");
  strncpy(routerMac, gRouterProfileMAC, sizeof(routerMac)-1);
    }
    else
    {
        APP_LOG("WiFiApp", LOG_DEBUG, "get route mac");
  getRouterEssidMac (routerssid, routerMac, INTERFACE_CLIENT);
  for(i = 0; routerMac[i]; i++)
      routerMac[i] = toupper(routerMac[i]);
    }

    APP_LOG("WiFiApp", LOG_DEBUG, "router mac: %s", routerMac);

    memcpy(KeyData, routerMac+SIZE_4B, SIZE_4B);
    strncat(KeyData, routerMac+SIZE_2B, SIZE_2B);
    strncat(KeyData, addSalt, SIZE_32B);
    strncat(KeyData, routerMac+SIZE_4B, SIZE_4B);
    strncat(KeyData, routerMac+SIZE_1B, SIZE_1B);
    strncat(KeyData, routerMac, SIZE_1B);

    APP_LOG("WiFiApp", LOG_DEBUG, "key data: %s",KeyData);
}

void encryptWithRouterInfoKeydata(char *input, char *finalstr)
{
    unsigned char key_data[PASSWORD_KEYDATA_LEN];
    unsigned char salt[PASSWORD_SALT_LEN];
    unsigned char iv[PASSWORD_IV_LEN];
    int key_data_len, salt_len, iv_len;
    unsigned char *ciphertext = NULL;
    int len;
    char* encStr;
    char lenstr[SIZE_4B];
    int cipher_len=0;
    char KeyData[SIZE_128B];

    memset(KeyData,0,sizeof(KeyData));
    memset(key_data, 0, sizeof(key_data));
    memset(salt, 0, sizeof(salt));
    memset(iv, 0, sizeof(iv));
    //memset(finalstr, 0, sizeof(finalstr));
    memset(lenstr, 0, sizeof(lenstr));

    createRouterInfoKeyData(KeyData);

    len = strlen(input);
    strncpy((char *)key_data, KeyData, sizeof(key_data)-1);
    key_data_len = strlen((char *)key_data);
    memcpy(salt, KeyData, PASSWORD_SALT_LEN-1);
    memcpy(iv, KeyData, PASSWORD_IV_LEN-1);
    salt_len = strlen((char *)salt);
    iv_len = strlen((char *)iv);

    APP_LOG("WiFiApp", LOG_DEBUG,"input: %s, input len: %d KeyData: %s KeyData len: %d salt: %s salt len: %d iv: %s iv len: %d", input, len, key_data, key_data_len,

      salt, salt_len, iv, iv_len);

    ciphertext = pluginAES128Encrypt(key_data, key_data_len, salt, salt_len, iv, iv_len, input, &len);
    if(!ciphertext)
    {
  APP_LOG("WiFiApp",LOG_ERR, "pluginAESEncrypt failed!!!");
  return;
    }
    ciphertext[len] = '\0';

    encStr = base64Encode(ciphertext, len);
    len = strlen(encStr);
    cipher_len = len;

    strcpy(finalstr,encStr);

    snprintf(lenstr, sizeof(lenstr), "%02X", cipher_len);

    memset(lenstr, 0, sizeof(lenstr));
    snprintf(lenstr, sizeof(lenstr), "%02X", strlen(input));

    free(ciphertext);
    free(encStr);

}

int decryptWithRouterInfoKeydata(char *input, char *output)
{
    unsigned char salt[PASSWORD_SALT_LEN];
    unsigned char iv[PASSWORD_IV_LEN];
    char ciphertext[PASSWORD_MAX_LEN];
    char *decodedString = NULL;
    unsigned char *decData = NULL;
    int ciphertext_len=0;
    char KeyData[SIZE_128B];

    memset(salt, 0, sizeof(salt));
    memset(iv, 0, sizeof(iv));
    memset(ciphertext, 0, sizeof(ciphertext));
    memset(KeyData,0,sizeof(KeyData));

    createRouterInfoKeyData(KeyData);
    APP_LOG("WiFiApp", LOG_DEBUG, "key data: %s", KeyData);

    /* update the password string */
    strncpy(ciphertext, input, sizeof(ciphertext)-1);
    ciphertext_len=strlen(ciphertext);

    if(ciphertext[ciphertext_len- 1] != '\n')
	    ciphertext[ciphertext_len-4] = '\0';
    else
	    ciphertext[ciphertext_len] = '\0';

    ciphertext_len = strlen(ciphertext);
    APP_LOG("WiFiApp", LOG_DEBUG,"Updated ciphertext: %s, len: %d, KeyData: %s", ciphertext, ciphertext_len, KeyData);


    memcpy(salt, KeyData, PASSWORD_SALT_LEN-1);
    memcpy(iv, KeyData, PASSWORD_IV_LEN-1);

    if(NULL == (decodedString=base64Decode(ciphertext, &ciphertext_len)))
    {
	    APP_LOG("WiFiApp", LOG_ERR,"Base decoding failed...");
	    return FAILURE;
    }
    else
    {
	    APP_LOG("WiFiApp", LOG_DEBUG,"decoding success");
    }

    decData = (unsigned char *)pluginSignatureDecrypt((unsigned char *)KeyData, strlen(KeyData),
      salt, PASSWORD_SALT_LEN-1,
      iv, PASSWORD_IV_LEN-1,
      (unsigned char *)decodedString, &ciphertext_len);
    if(!decData)
    {
  APP_LOG("WiFiApp",LOG_ERR, "pluginDecrypt failed!!!");
  return FAILURE;
    }
    decData[ciphertext_len] = '\0';

    APP_LOG("WiFiApp", LOG_DEBUG,"Decrypted plaintext: %s, len - %d", decData, strlen((char *)decData));

    if(output)
  strcpy(output, (char *)decData);

    if(decodedString)
	free(decodedString);

    free(decData);
    return SUCCESS;
}
#endif

void encryptWithMacKeydata(char *input, char *finalstr)
{
   unsigned char key_data[PASSWORD_KEYDATA_LEN];
   unsigned char salt[PASSWORD_SALT_LEN];
   unsigned char iv[PASSWORD_IV_LEN];
   int key_data_len, salt_len, iv_len;
   unsigned char *ciphertext = NULL;
   int len;
   char* encStr;
   char lenstr[SIZE_4B];
   int cipher_len=0;
  char KeyData[SIZE_128B];

  memset(KeyData,0,sizeof(KeyData));
  memset(key_data, 0, sizeof(key_data));
  memset(salt, 0, sizeof(salt));
  memset(iv, 0, sizeof(iv));
  //memset(finalstr, 0, sizeof(finalstr));
  memset(lenstr, 0, sizeof(lenstr));

  createMacKeyData(KeyData);
  APP_LOG("WiFiApp", LOG_HIDE, "key data: %s", KeyData);

  len = strlen(input);
  strncpy((char *)key_data, KeyData, sizeof(key_data)-1);
  key_data_len = strlen((char *)key_data);
  memcpy(salt, KeyData, PASSWORD_SALT_LEN-1);
  memcpy(iv, KeyData, PASSWORD_IV_LEN-1);
  salt_len = strlen((char *)salt);
  iv_len = strlen((char *)iv);

  APP_LOG("WiFiApp", LOG_HIDE,"input: %s, input len: %d KeyData: %s KeyData len: %d salt: %s salt len: %d iv: %s iv len: %d", input, len, key_data, key_data_len, salt, salt_len, iv, iv_len);

  ciphertext = pluginAES128Encrypt(key_data, key_data_len, salt, salt_len, iv, iv_len, input, &len);
  if(!ciphertext)
  {
        APP_LOG("WiFiApp",LOG_ERR, "pluginAESEncrypt failed!!!");
	return;
  }
  ciphertext[len] = '\0';

  encStr = base64Encode(ciphertext, len);
  len = strlen(encStr);
  cipher_len = len;

  strcpy(finalstr,encStr);

  snprintf(lenstr, sizeof(lenstr), "%02X", cipher_len);

  memset(lenstr, 0, sizeof(lenstr));
  snprintf(lenstr, sizeof(lenstr), "%02X", strlen(input));

  free(ciphertext);
  free(encStr);
}

int decryptWithMacKeydata(char *input, char *output)
{
  unsigned char salt[PASSWORD_SALT_LEN];
  unsigned char iv[PASSWORD_IV_LEN];
  char ciphertext[PASSWORD_MAX_LEN];
  char *decodedString = NULL;
  unsigned char *decData = NULL;
  int ciphertext_len=0;
  char KeyData[SIZE_128B];

  memset(salt, 0, sizeof(salt));
  memset(iv, 0, sizeof(iv));
  memset(ciphertext, 0, sizeof(ciphertext));
  memset(KeyData,0,sizeof(KeyData));

  createMacKeyData(KeyData);
  APP_LOG("WiFiApp", LOG_HIDE, "key data: %s", KeyData);

  /* update the password string */
  strncpy(ciphertext, input, sizeof(ciphertext)-1);
  ciphertext_len = strlen(ciphertext);

  if(ciphertext[ciphertext_len - 1] != '\n')
  {
      ciphertext[ciphertext_len -4] = '\0';
  }
  else
  {
      ciphertext[ciphertext_len] = '\0';
      /**Below code added to remove /n from encoded string
      as the BIO_FLAGS_BASE64_NO_NL flag set into decoding
      mechanism which will make buf flush if \n found. */
      char LocalCip[PASSWORD_MAX_LEN]=" ";
      int i=0,j=0;
      while( (ciphertext[i]) && (i< (PASSWORD_MAX_LEN-1)) )
      {
          if(ciphertext[i] != '\n')
          {
              LocalCip[j++]=ciphertext[i];
          }
          i++;
      }
      LocalCip[j]='\0';
      memset(ciphertext, 0, sizeof(ciphertext));
      strncpy(ciphertext,LocalCip, sizeof(ciphertext)-1);
  }

  ciphertext_len = strlen(ciphertext);
  APP_LOG("WiFiApp", LOG_HIDE,"Updated ciphertext: %s, len: %d, KeyData: %s", ciphertext, ciphertext_len, KeyData);

  memcpy(salt, KeyData, PASSWORD_SALT_LEN-1);
  memcpy(iv, KeyData, PASSWORD_IV_LEN-1);

  if(NULL == (decodedString = base64Decode(ciphertext, &ciphertext_len)))
  {
	  APP_LOG("WiFiApp", LOG_ERR,"Base decoding failed...");
	  return FAILURE;
  }
  else
  {
  	  APP_LOG("WiFiApp", LOG_DEBUG,"decoding success");
  }

   decData = (unsigned char *)pluginSignatureDecrypt((unsigned char *)KeyData, strlen(KeyData),
			salt, PASSWORD_SALT_LEN-1,
			iv, PASSWORD_IV_LEN-1,
			(unsigned char *)decodedString, &ciphertext_len);
  if(!decData)
  {
        APP_LOG("WiFiApp",LOG_ERR, "pluginDecrypt failed!!!");
	return FAILURE;
  }
  decData[ciphertext_len] = '\0';

  APP_LOG("WiFiApp", LOG_HIDE,"Decrypted plaintext: %s, len - %d", decData, strlen((char *)decData));

  if(output)
	strcpy(output, (char *)decData);

  if(decodedString)
	  free(decodedString);

  free(decData);
  return SUCCESS;
}

void encryptSignature(char *input, char *finalstr)
{
   unsigned char key_data[PASSWORD_KEYDATA_LEN];
   unsigned char salt[PASSWORD_SALT_LEN];
   unsigned char iv[PASSWORD_IV_LEN];
   int key_data_len, salt_len, iv_len;
   unsigned char *ciphertext = (unsigned char *)input;
   int len;
   char basePassword[SIZE_256B];
   char* encStr;
   char lenstr[SIZE_4B];
   int cipher_len=0;
  char SignatureKeyData[SIZE_128B];
  memset(SignatureKeyData,0,sizeof(SignatureKeyData));

  createSignatureKeyData(SignatureKeyData);
  APP_LOG("WiFiApp", LOG_HIDE, "Signature key data: %s",SignatureKeyData);

  memset(key_data, 0, sizeof(key_data));
  memset(salt, 0, sizeof(salt));
  memset(iv, 0, sizeof(iv));
  memset(finalstr, 0, sizeof(finalstr));
  memset(basePassword, 0, sizeof(basePassword));
  memset(lenstr, 0, sizeof(lenstr));

  len = strlen(input);
  strncpy((char *)key_data, SignatureKeyData, sizeof(key_data)-1);
  key_data_len = strlen((char *)key_data);
  memcpy(salt, SignatureKeyData, PASSWORD_SALT_LEN-1);
  memcpy(iv, SignatureKeyData, PASSWORD_IV_LEN-1);
  salt_len = strlen((char *)salt);
  iv_len = strlen((char *)iv);

  APP_LOG("WiFiApp", LOG_HIDE,"input: %s, input len: %d KeyData: %s KeyData len: %d salt: %s salt len: %d iv: %s iv len: %d", input, len, key_data, key_data_len, salt, salt_len, iv, iv_len);

  ciphertext = pluginAES128Encrypt(key_data, key_data_len, salt, salt_len, iv, iv_len, input, &len);
  if(!ciphertext)
  {
        APP_LOG("WiFiApp",LOG_ERR, "pluginAESEncrypt failed!!!");
	return;
  }
  ciphertext[len] = '\0';

  encStr = base64Encode(ciphertext, len);
  len = strlen(encStr);
  cipher_len = len;

  strncpy(finalstr,encStr, SIGNATURE_LEN);

  snprintf(lenstr, sizeof(lenstr), "%02X", cipher_len);

  memset(lenstr, 0, sizeof(lenstr));
  snprintf(lenstr, sizeof(lenstr), "%02X", strlen(input));

  free(ciphertext);
  free(encStr);
}

int decryptSignature(char *input, char *output)
{
  unsigned char salt[PASSWORD_SALT_LEN];
  unsigned char iv[PASSWORD_IV_LEN];
  char ciphertext[PASSWORD_MAX_LEN];
  char *decodedString=NULL;
  unsigned char *dsignature = NULL;
  int ciphertext_len=0;
  char SignatureKeyData[SIZE_128B];
  memset(SignatureKeyData,0,sizeof(SignatureKeyData));
  createSignatureKeyData(SignatureKeyData);
  APP_LOG("WiFiApp", LOG_HIDE, "Signature key data: %s", SignatureKeyData);


  memset(salt, 0, sizeof(salt));
  memset(iv, 0, sizeof(iv));
  memset(ciphertext, 0, sizeof(ciphertext));

  /* update the password string */
  strncpy(ciphertext, input, sizeof(ciphertext)-1);
  ciphertext_len = strlen(ciphertext);

  if(ciphertext[ciphertext_len - 1] != '\n')
  {
      ciphertext[ciphertext_len -4] = '\0';
  }
  else
  {
      ciphertext[ciphertext_len] = '\0';
      /**Below code added to remove /n from encoded string
      as the BIO_FLAGS_BASE64_NO_NL flag set into decoding
      mechanism which will make buf flush if \n found. */
      char LocalCip[PASSWORD_MAX_LEN]=" ";
      int i=0,j=0;
      while( (ciphertext[i]) && (i< (PASSWORD_MAX_LEN-1)) )
      {
          if(ciphertext[i] != '\n')
          {
              LocalCip[j++]=ciphertext[i];
          }
          i++;
      }
      LocalCip[j]='\0';
      memset(ciphertext, 0, sizeof(ciphertext));
      strncpy(ciphertext,LocalCip, sizeof(ciphertext)-1);
  }

  ciphertext_len = strlen(ciphertext);
  APP_LOG("WiFiApp", LOG_HIDE,"Updated ciphertext: %s, len: %d, KeyData: %s", ciphertext, ciphertext_len,SignatureKeyData);

  memcpy(salt, SignatureKeyData, PASSWORD_SALT_LEN-1);
  memcpy(iv, SignatureKeyData, PASSWORD_IV_LEN-1);

  if(NULL == (decodedString = base64Decode(ciphertext, &ciphertext_len)))
  {
	  APP_LOG("WiFiApp", LOG_ERR,"Base decoding failed...");
	  return FAILURE;
  }
  else
  {
  	  APP_LOG("WiFiApp", LOG_DEBUG,"decoded passwd success");
  }

   dsignature = (unsigned char *)pluginSignatureDecrypt((unsigned char *)SignatureKeyData, strlen(SignatureKeyData),
			salt, PASSWORD_SALT_LEN-1,
			iv, PASSWORD_IV_LEN-1,
			(unsigned char *)decodedString, &ciphertext_len);
  if(!dsignature)
  {
        APP_LOG("WiFiApp",LOG_ERR, "pluginSignatureDecrypt failed!!!");
	return FAILURE;
  }
  dsignature[ciphertext_len] = '\0';

  APP_LOG("WiFiApp", LOG_HIDE,"Decrypted signature: %s, signature len - %d", dsignature, strlen((char *)dsignature));

  if(output)
	strncpy(output, (char *)dsignature, SIGNATURE_LEN);

  if(decodedString)
	  free(decodedString);

  free(dsignature);
  return SUCCESS;
}

void encryptPassword(char *input, char *finalstr)
{
   unsigned char key_data[PASSWORD_KEYDATA_LEN];
   unsigned char salt[PASSWORD_SALT_LEN];
   unsigned char iv[PASSWORD_IV_LEN];
   int key_data_len, salt_len, iv_len;
   unsigned char *ciphertext = (unsigned char *)input;
   int len;
   char basePassword[SIZE_256B];
   char* encStr;
   char lenstr[SIZE_4B];
   int cipher_len=0;

  memset(key_data, 0, sizeof(key_data));
  memset(salt, 0, sizeof(salt));
  memset(iv, 0, sizeof(iv));
  memset(finalstr, 0, sizeof(finalstr));
  memset(basePassword, 0, sizeof(basePassword));
  memset(lenstr, 0, sizeof(lenstr));

  len = strlen(input);
  strncpy((char *)key_data, gPasswordKeyData, sizeof(key_data)-1);
  key_data_len = strlen((char *)key_data);
  memcpy(salt, gPasswordKeyData, PASSWORD_SALT_LEN-1);
  memcpy(iv, gPasswordKeyData, PASSWORD_IV_LEN-1);
  salt_len = strlen((char *)salt);
  iv_len = strlen((char *)iv);

  APP_LOG("WiFiApp", LOG_HIDE,"input: %s, input len: %d KeyData: %s KeyData len: %d salt: %s salt len: %d iv: %s iv len: %d", input, len, key_data, key_data_len, salt, salt_len, iv, iv_len);

  ciphertext = pluginAES128Encrypt(key_data, key_data_len, salt, salt_len, iv, iv_len, input, &len);
  if(!ciphertext)
  {
        APP_LOG("WiFiApp",LOG_ERR, "pluginAESEncrypt failed!!!");
	return;
  }
  ciphertext[len] = '\0';

  APP_LOG("WiFiApp", LOG_HIDE,"Encrypted ciphertext password: %s, ciphertext password len: %d", ciphertext, strlen((char *)ciphertext));
  encStr = base64Encode(ciphertext, len);
  len = strlen(encStr);
  APP_LOG("WiFiApp", LOG_HIDE,"Base 64 encoded ciphertext len: %d",len);
  cipher_len = len;

  strncpy(finalstr,encStr,PASSWORD_MAX_LEN);

  snprintf(lenstr, sizeof(lenstr), "%02X", cipher_len);
  strcat(finalstr,lenstr);

  memset(lenstr, 0, sizeof(lenstr));
  snprintf(lenstr, sizeof(lenstr), "%02X", strlen(input));
  strcat(finalstr,lenstr);

  APP_LOG("WiFiApp", LOG_HIDE,"Final String to be sent: %s",finalstr);

  free(ciphertext);
  free(encStr);

}

int decryptPassword(char *input, char *output)
{
  unsigned char salt[PASSWORD_SALT_LEN];
  unsigned char iv[PASSWORD_IV_LEN];
  char ciphertext[PASSWORD_MAX_LEN];
  char* decodedString;
  unsigned char *password;
  int ciphertext_len=0;
  memset(salt, 0, sizeof(salt));
  memset(iv, 0, sizeof(iv));
  memset(ciphertext, 0, sizeof(ciphertext));

  APP_LOG("WiFiApp", LOG_HIDE,"input: %s", input);

  /* update the password string */
  strncpy(ciphertext, input, sizeof(ciphertext)-1);
  ciphertext_len = strlen(ciphertext);

  if(ciphertext[ciphertext_len - 1] != '\n')
      ciphertext[ciphertext_len -4] = '\0';
  else
      ciphertext[ciphertext_len] = '\0';

  ciphertext_len = strlen(ciphertext);
  APP_LOG("WiFiApp", LOG_HIDE,"Updated ciphertext: %s, len: %d, KeyData: %s", ciphertext, ciphertext_len,gPasswordKeyData);

  memcpy(salt, gPasswordKeyData, PASSWORD_SALT_LEN-1);
  memcpy(iv, gPasswordKeyData, PASSWORD_IV_LEN-1);

  if(NULL == (decodedString = base64Decode(ciphertext, &ciphertext_len)))
  {
	  APP_LOG("WiFiApp", LOG_ERR,"Base decoding failed...");
	  return FAILURE;
  }
  else
  {
  	  APP_LOG("WiFiApp", LOG_DEBUG,"decoded passwd success");
  }

  password = (unsigned char *)pluginPasswordDecrypt((unsigned char *)gPasswordKeyData, strlen(gPasswordKeyData),
			    salt, PASSWORD_SALT_LEN-1,
			    iv, PASSWORD_IV_LEN-1,
			    (unsigned char *)decodedString, &ciphertext_len);
  if(!password)
  {
        APP_LOG("WiFiApp",LOG_ERR, "pluginPasswordDecrypt failed!!!");
	return FAILURE;
  }
  password[ciphertext_len] = '\0';

  APP_LOG("WiFiApp", LOG_HIDE,"Decrypted password: %s, password len - %d", password, strlen((char *)password));

  /* store password plain text len */
  gPassPlainTextLen = ciphertext_len;

  if(output)
	strncpy(output, (char *)password, PASSWORD_MAX_LEN);

  if(decodedString)
	  free(decodedString);

  free(password);
  return SUCCESS;
}

int findChannelForSSID(char *ssid)
{
	PMY_SITE_SURVEY pAvlNetwork=NULL;
	int chan=-1;
	int found=0;
	int i=0;
	int count=0;
	int retries=3;

   pAvlNetwork = (PMY_SITE_SURVEY) ZALLOC(sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE);
	EnableSiteSurvey();
	pluginUsleep(50000);
	while(retries > 0)
	{
		getCurrentAPList (pAvlNetwork,&count);
		APP_LOG("WiFiApp", LOG_DEBUG,"Avl network list cnt: %d", count);
		for (i=0;i<count;i++)
		{

			if (!strcmp (pAvlNetwork[i].ssid,ssid))
			{

				APP_LOG("WiFiApp", LOG_DEBUG,
						"pAvlNetwork[%d].channel: %s,pAvlNetwork[%d].ssid: %s",
						i, pAvlNetwork[i].channel, i, pAvlNetwork[i].ssid);
				chan = atoi((const char *)pAvlNetwork[i].channel);
				found = 1;
				APP_LOG("WiFiApp", LOG_DEBUG,
						"Channel determined from SITE SURVEY: <%d>",chan);
				break;
			}
		}
		if(found)
		{
			APP_LOG("WiFiApp", LOG_DEBUG,
				"Channel determined from SITE SURVEY: <%d> on retry#%d",chan,4-retries);
			break;
		}
		else
		{
			retries --;
			pluginUsleep(1000000);
		}
	}

	free(pAvlNetwork);

	if(!found)
	{
		APP_LOG("WiFiApp", LOG_ERR,
				"SSID: %s not found in the available network list",ssid);
		return -1;
	}
	else
	{
		return chan;
	}


}

/************************************************************************
 * Function: wifiCheckConfigAvl
 *     Initialize settings configuration source. Read the Flash area
 *     if network configuration is available. Copy it if avl.
 *  Parameters:
 *    None.
 *  Return:
 *     SUCCESS/FAILURE
 ************************************************************************/
int wifiCheckConfigAvl()
{
    char *pSSID=NULL;
    char *pAuth=NULL;
    char *pEncryp=NULL;
    char *pEncPasswd=NULL;
    int chan=-1, retVal;
    char *pChan=NULL;
#ifndef WeMo_INSTACONNECT
    PMY_SITE_SURVEY pAvlNetwork;
    int i=0, count=0, found=0;
#endif
#ifdef WeMo_INSTACONNECT
    //char brlist[SIZE_512B];
    //char bestNode[SIZE_64B + 1];
#endif

    pSSID = GetBelkinParameter (WIFI_CLIENT_SSID);
    APP_LOG("WiFiApp", LOG_DEBUG, "SSID:%s",pSSID);
    if(pSSID && strlen (pSSID) > 0)
    {
#ifdef WeMo_INSTACONNECT
	    g_connectInstaStatus = APCLI_CONFIGURED;	//wifi settings found, device configured

	    pChan = GetBelkinParameter (WIFI_AP_CHAN);
	    APP_LOG("WiFiApp", LOG_DEBUG, "channel: %s", pChan);
	    if(pChan)
	    {
		chan = atoi (pChan);
		gWiFiParams.channel = chan;
	    }
	    else
		return FAILURE;
#if 0
	    /* intialize best node with router ssid */
	    //TODO: here get best node might not required
	    /* Save the router ssid in a global variable - to be used later in wifi save data */
	    memset(gRouterSSID, 0, sizeof(gRouterSSID));
	    memcpy(gRouterSSID, pSSID, sizeof(gRouterSSID));

	    memset(bestNode, 0, sizeof(bestNode));
	    strncpy(bestNode, pSSID, sizeof(bestNode)-1);
	    //chan = gWiFiParams.channel;

	    /* get best node, if any bridge list */
	    if(strlen(gBridgeNodeList))
	    {
		memset(brlist, 0, sizeof(brlist));
		strncpy(brlist, gBridgeNodeList, sizeof(brlist)-1);
		getBestNode(pSSID, brlist, bestNode, &chan);
		APP_LOG("WiFiApp", LOG_DEBUG, "bestnode: %s and updated channel: %d", bestNode, chan);
	    }
#endif
	    //strncpy(gWiFiParams.SSID, bestNode, sizeof(gWiFiParams.SSID)-1);
	    strncpy(gWiFiParams.SSID,pSSID, sizeof(gWiFiParams.SSID)-1);
	    gWiFiParams.channel = chan;
#else
	    strncpy(gWiFiParams.SSID,pSSID, sizeof(gWiFiParams.SSID)-1);
#endif
	    pEncPasswd = GetBelkinParameter (WIFI_CLIENT_PASS);
	    APP_LOG("WiFiApp", LOG_HIDE, "Encrypted Pass:%s",pEncPasswd);

	    if(pEncPasswd)
	    {
		    strncpy(gWiFiParams.Key,pEncPasswd, sizeof(gWiFiParams.Key)-1);
	    	    memcpy(gUserKey,pEncPasswd,PASSWORD_MAX_LEN);

		    pAuth = GetBelkinParameter (WIFI_CLIENT_AUTH);
		    APP_LOG("WiFiApp", LOG_DEBUG, "Auth:%s",pAuth);
		    if(pAuth && strlen (pAuth))
		    {
			    strncpy(gWiFiParams.AuthMode,pAuth, sizeof(gWiFiParams.AuthMode)-1);
			    pEncryp = GetBelkinParameter (WIFI_CLIENT_ENCRYP);
			    APP_LOG("WiFiApp", LOG_HIDE, "Encryp:%s",pEncryp);
			    if(pEncryp && strlen (pEncryp))
			    {
				    strncpy(gWiFiParams.EncrypType,pEncryp, sizeof(gWiFiParams.EncrypType)-1);
				    gWiFiConfigured=1;

#ifndef WeMo_INSTACONNECT
#if 1
				    pChan = GetBelkinParameter (WIFI_AP_CHAN);
				    if(pChan) {
					    chan = atoi (pChan);
					    gWiFiParams.channel = chan;
				    }else {
					    return FAILURE;
				    }
#endif
				    if(chan != CHAN_HIDDEN_NW)
				    {
					    /* We should discover the channel every-time we come up for the saved SSID, as it might change */

                   pAvlNetwork = (PMY_SITE_SURVEY) ZALLOC(sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE);
                   if(!pAvlNetwork)
                   {
                      APP_LOG("WiFiApp", LOG_ERR, "Malloc Failed....\n");
                      return FAILURE;
                   }

					    EnableSiteSurvey();
					    pluginUsleep(50000);

					    getCompleteAPList (pAvlNetwork,&count);
					    for (i=0;i<count;i++)
					    {

						    if (!strcmp (pAvlNetwork[i].ssid,gWiFiParams.SSID))
						    {

							    APP_LOG("WiFiApp", LOG_DEBUG,
									    "pAvlNetwork[%d].channel: %s, gWiFiParams.channel: %d, \
									    pAvlNetwork[%d].ssid: %s, gWiFiParams.SSID: %s",
									    i, pAvlNetwork[i].channel, gWiFiParams.channel,
									    i, pAvlNetwork[i].ssid,gWiFiParams.SSID);
							    chan = atoi((const char *)pAvlNetwork[i].channel);
					    		    gWiFiParams.channel = chan;
							    found = 1;
							    APP_LOG("WiFiApp", LOG_DEBUG,
									    "Channel determined from SITE SURVEY: <%d>",chan);
							    break;
						    }
					    }

					    if(!found)
					    {
						    APP_LOG("WiFiApp", LOG_ERR,
							    "SSID: %s not found in the available network list",gWiFiParams.SSID);

						    gStartWiFiTest=1;
						    pthread_attr_init(&inet_attr);
						    pthread_attr_setdetachstate(&inet_attr,PTHREAD_CREATE_DETACHED);
						    retVal = pthread_create(&nwThd, &inet_attr, checkInetConnectivity, NULL);
						    if(retVal < SUCCESS) {
							APP_LOG("WiFiApp",LOG_ALERT, "Connection Thread not created, errno: %d", errno);
							system("reboot");
						    }

						    if(!Inet_Monitor_thread)
						    {
							/* create the Inet connectivity monitor thread */
							pthread_attr_init(&inet_mon_attr);
							pthread_attr_setdetachstate(&inet_mon_attr,PTHREAD_CREATE_DETACHED);
							retVal = pthread_create(&Inet_Monitor_thread, &inet_mon_attr, InetMonitorTask, NULL);
							if(retVal != 0)
							{
							    APP_LOG("WiFiApp",LOG_ALERT, "InetMon Thread not created, errno: %d", errno);
							    system("reboot");
							}
						    }

						    APP_LOG("WiFiApp", LOG_DEBUG, "Created checkInetConnectivity Thread...");
						    gBootReconnect=1;
        					    APP_LOG("WiFiApp",LOG_DEBUG, "gBootReconnect=1");

						    free(pAvlNetwork);
						    return SUCCESS;
					    }

					    free (pAvlNetwork);
				    }
#endif
			    }
			    else
			    {
				    return FAILURE;
			    }
		    }
		    else
		    {
			    return FAILURE;
		    }
	   }
	   else
	   {
	    	return FAILURE;
	   }
    } else {
        return FAILURE;
    }

    if(chan != -1)
    {
    	APP_LOG("WiFiApp", LOG_DEBUG, "Connection Data available..Trying to connect : %s",pSSID);

	    gStartWiFiTest=1;
	    pthread_attr_init(&inet_attr);
	    pthread_attr_setdetachstate(&inet_attr,PTHREAD_CREATE_DETACHED);
	    retVal = pthread_create(&nwThd, &inet_attr, checkInetConnectivity, NULL);
	    if(retVal < SUCCESS) {
		APP_LOG("WiFiApp",LOG_ALERT,"Connection Thread not created, errno: %d", errno);
		system("reboot");
	    }

	    if(!Inet_Monitor_thread)
	    {
		/* create the Inet connectivity monitor thread */
		pthread_attr_init(&inet_mon_attr);
		pthread_attr_setdetachstate(&inet_mon_attr,PTHREAD_CREATE_DETACHED);
		retVal = pthread_create(&Inet_Monitor_thread, &inet_mon_attr, InetMonitorTask, NULL);
		if(retVal != 0)
		{
		    APP_LOG("WiFiApp",LOG_ALERT, "InetMon Thread not created, errno: %d", errno);
		    system("reboot");
		}
	    }
	    gBootReconnect=1;
	    APP_LOG("WiFiApp",LOG_DEBUG, "gBootReconnect=1");

    }

    return SUCCESS;
}



void chkConnStatus ()
{
    int currState=STATE_DISCONNECTED;
    int ret = -1;
    //char *pSavedSSID=NULL;
    char pSavedSSID[WIFI_MAXSTR_LEN+1];
    char ssid[WIFI_MAXSTR_LEN+1];

    if (!getCurrentClientState())
    {
        FILE    *pipe;
        char    pCommand[WIFI_MAXSTR_LEN];
        char    buffer[WIFI_MAXSTR_LEN];

        strncpy(pCommand, "ifconfig | grep apcli0", sizeof(pCommand)-1);
        pipe = popen(pCommand, "r");
        if (pipe == NULL)
        {
        	APP_LOG("NetworkControl", LOG_ERR, "Popen Error %s", strerror(errno));
		resetSystem();
        }
        if (fgets(buffer, WIFI_MAXSTR_LEN, pipe) == NULL)
        {
        	APP_LOG("NetworkControl", LOG_DEBUG, "NOT FOUND apcli0: again");
        	system("ifconfig apcli0 up");
        }
        pclose(pipe);
    }

    APP_LOG("WiFiApp", LOG_DEBUG, "gWiFiConfigured: %d, gWiFiClientCurrState: %d",
		gWiFiConfigured, getCurrentClientState());

    gInetHealthPunch++;

    if (gWiFiConfigured) {
	/*
         * We need to test Wifi Connection only when AP is connected
         * In case, router is rebooted, we will loose the AP association as well
         */


	//pSavedSSID = GetBelkinParameter (WIFI_CLIENT_SSID);
	memset(pSavedSSID, 0, sizeof(pSavedSSID));
	strncpy(pSavedSSID, gWiFiParams.SSID, sizeof(pSavedSSID)-1);
	//if ((0x00 != pSavedSSID) && 0x00 != strlen(pSavedSSID))
	if (0x00 != strlen(pSavedSSID))
	{
		memset(ssid, '\0', WIFI_MAXSTR_LEN+1);
		/************************************************************************
		 *  Story: 2571
		 *  This "convertSSID" will convert the hex string in raw bytes format,
		 *  which is used in pairing with the router
		 ************************************************************************/
		strncpy(ssid, pSavedSSID, sizeof(ssid)-1);
		strncpy(ssid, convertSSID(ssid), sizeof(ssid)-1);

		APP_LOG("WiFiApp", LOG_DEBUG, "to checked connection of saved ssid: %s", pSavedSSID);
		ret = isAPConnected(ssid);
	}
	else
	{
		APP_LOG("WiFiApp", LOG_ERR, "#no saved ssid found, connection check stopped and executed later");
		return;
	}

	if(ret == SUCCESS)
	{
		currState = wifiTestConnection(INTERFACE_CLIENT, 1, 0);
		if (currState)
		{
		    if(routerConnCount != 0)
			APP_LOG("WiFiApp", LOG_DEBUG, "#Router connected, state: %d", getCurrentClientState());
		    routerConnLostCount=0;
		    routerConnCount++;
		    if(routerConnCount == 1)
			APP_LOG("WiFiApp", LOG_CRIT, "Router connected, state: %d", getCurrentClientState());
		}
		else
		{
			APP_LOG("WiFiApp", LOG_DEBUG, "################# WTF comes here #####################");
			SetCurrentClientState(STATE_DISCONNECTED);
			SetWiFiLED(0x03);
			reconnectHome();
		}
	}
	else
	{
		APP_LOG("WiFiApp", LOG_ERR, "AP not connected, going for reconnection...");
		SetCurrentClientState(STATE_DISCONNECTED);
		SetWiFiLED(0x03);
		reconnectHome();
	}

    }
    return;

}

int getLookup (char *pDomain) {
    FILE *pipe;
    char command1[SIZE_256B];
    char buf[SIZE_256B] = {0,};
    int retVal=SUCCESS;

    strncpy(command1,"nslookup ", sizeof(command1));

    strncat(command1, pDomain, sizeof(command1)-strlen(command1)-1);

    pipe = popen(command1,"r");
    if (pipe == NULL){
       APP_LOG("NetworkControl", LOG_ERR, "Popen Error %s", strerror(errno));
       resetSystem();
    }

    while (fgets( buf, SIZE_256B, pipe) != NULL)

    {
        if (strstr(buf, "Resolver") != NULL) {
            APP_LOG("NetworkControl", LOG_DEBUG, "FAILURE----buf:%s", buf);
            retVal = FAILURE;
            break;
        } else if (strstr(buf, "Name")){
            APP_LOG("NetworkControl", LOG_DEBUG, "buf:%s", buf);
            retVal = FAILURE;
            break;
        }
    }
    pclose(pipe);
    return retVal;
}

#define CHECK_PING_SCRIPT_FILE  "/sbin/chkping.sh"
int checkPingStatus(int packets, char *pszSite)
{
  int nRet = 0;
  int nPacketLoss = PACKET_LOSE_100_PERCENT;
  char line[80] = {0,};
  char szCommand[128] = {0,};

  APP_LOG("checkPingStatus", LOG_DEBUG, "ping %d %s ", packets, pszSite);

  if( access(CHECK_PING_SCRIPT_FILE, F_OK) == 0 )
  {
    snprintf(szCommand, sizeof(szCommand), "%s %d %s", CHECK_PING_SCRIPT_FILE, packets, pszSite);
  }
  else
  {
    snprintf(szCommand, sizeof(szCommand),
            "ping -c%d %s \x7C grep \"packet loss\" \x7C awk \'{ print $7 }\' > /tmp/ping.txt",
            packets, pszSite);
  }

  nRet = system(szCommand);

  if( 0 == nRet )
  {
    FILE *fp = fopen("/tmp/ping.txt", "rb");
    if( fp )
    {
      memset(line, 0x00, sizeof(line));
      while( fgets(line, sizeof(line), fp) )
      {
        if( line[0] )
        {
          nPacketLoss = atoi(line);
          APP_LOG("checkPingStatus", LOG_DEBUG, "ping %s packet loss = %d", pszSite, atoi(line));
          break;
        }
      }
      fclose(fp);
    }
  }

  if( line[0] == '\0' )
  {
    nPacketLoss = PACKET_LOSE_100_PERCENT;
    APP_LOG("checkPingStatus", LOG_DEBUG, "line is null");
  }

  //[WEMO-32439] - Make sure all pings pass
  /*[WEMO-33430] - instead of returning 0 or -1, nPacketLoss will be returned, which holds the parcentage lose.
    The funtionality of this funtion will remain same. previously it was returning -1(a nonzero value) for any parcentage lose and 0 for no lose.
    Now it will return actual parcentage lose(a nonzero value) and 0 for no lose.
    Use of this return value in caller funtion is now implementation dependent, the caller should take care of this*/

  APP_LOG("checkPingStatus", LOG_DEBUG, "packet loss is %d", nPacketLoss);

  return nPacketLoss;
}

int checkInetStatus(int packets)
{
  char pBufe[MAX_DATA_LEN];
  int retVal = FAILURE;

  memset(pBufe, 0x0, MAX_DATA_LEN);

#ifdef USE_PING_DEFAULT
  snprintf(pBufe, sizeof(pBufe), "ping -count %d %s > /dev/null", packets, ISITE);
  retVal = system(pBufe);
  APP_LOG("checkInetStatus", LOG_DEBUG, "packets = %d, retVal = %d", packets, retVal)
#else
  {
    char ipArr[SIZE_10B][SIZE_64B];
    int num = 0;

    if( 0 == checkPingStatus(packets, ISITE) )
    {
      APP_LOG("checkInetStatus", LOG_DEBUG, "Alive %s", ISITE);
      return SUCCESS;
    }

    if( 0 == checkPingStatus(packets, GOOGLE_DNS_1) )
    {
      APP_LOG("checkInetStatus", LOG_DEBUG, "Alive %s", GOOGLE_DNS_1);
      return SUCCESS;
    }

    //[WEMO-32439] - Replace secondary Google DNS by OpenDNS IP
    if( 0 == checkPingStatus(packets, OPENDNS_HOME) )
    {
      APP_LOG("checkInetStatus", LOG_DEBUG, "Alive %s", OPENDNS_HOME);
      return SUCCESS;
    }

    remoteParseDomainLkup(BELKIN_DOMAIN_NAME,ipArr,&num);
    APP_LOG("checkInetStatus", LOG_DEBUG, "Domain = %s, num = %d", BELKIN_DOMAIN_NAME, num)
    if(num > 0)
      return SUCCESS;
    pluginUsleep(5000000);
    remoteParseDomainLkup(BELKIN_DOMAIN_NAME,ipArr,&num);
    APP_LOG("checkInetStatus", LOG_DEBUG, "Domain = %s, num = %d", BELKIN_DOMAIN_NAME, num)
    if(num > 0)
      return SUCCESS;
    pluginUsleep(5000000);
    remoteParseDomainLkup(ROOTSERVER_DOMAIN_NAME,ipArr,&num);
    APP_LOG("checkInetStatus", LOG_DEBUG, "Domain = %s, num = %d", ROOTSERVER_DOMAIN_NAME, num)
    if(num > 0)
      return SUCCESS;
    pluginUsleep(5000000);
    remoteParseDomainLkup(ROOTSERVER_DOMAIN_NAME,ipArr,&num);
    APP_LOG("checkInetStatus", LOG_DEBUG, "Domain = %s, num = %d", ROOTSERVER_DOMAIN_NAME, num)
    if(num > 0)
      return SUCCESS;
}
#endif
  return retVal;
}

int checkRouterConnectivity()
{
  char *pIp,*pGateway;
  char pingcmd[SIZE_256B];
  int ret;

  pIp = GetWanIPAddress();
  pGateway = GetWanDefaultGateway();

  if( pIp && pGateway )
  {
    if( strcmp(pIp, "0.0.0.0") && strcmp(pGateway, "0.0.0.0") )
    {
      APP_LOG("checkRouterConnectivity", LOG_DEBUG, "pGateway is %s", pGateway)

      if( PACKET_LOSE_100_PERCENT != checkPingStatus(3, pGateway) )
      {
        APP_LOG("checkRouterConnectivity", LOG_DEBUG, "checkPingStatus(%s) is alived...", pGateway);
        return SUCCESS;
      }
      else
      {
        APP_LOG("checkRouterConnectivity", LOG_DEBUG, "checkPingStatus(%s) is dead...", pGateway);
        return FAILURE;
      }
    }
  }
  else
  {
    APP_LOG("checkRouterConnectivity", LOG_DEBUG, "pIp and pGateway is not existed...")
  }
  return FAILURE;
}

void EnableSiteSurvey()
{
	char command[SIZE_64B];
	memset(command, '\0', SIZE_64B);
	strncpy(command, "SiteSurvey=1", sizeof(command)-1);
	wifiSetCommand (command,"apcli0");
}

int getCurrentAPList (PMY_SITE_SURVEY SiteSurvey,int *pListCount)
{
    EnableSiteSurvey();
    return wifiGetNetworkList (SiteSurvey,"apcli0",pListCount);
}

int saveData (int chan,char *pSSID,char *pAuth,char *pEnc,char *pPass) {
    char channel[SIZE_4B];
    int len = 0;
    char lenstr[SIZE_4B];
    memset(lenstr, 0, sizeof(lenstr));

    snprintf (channel, sizeof(channel), "%d",chan);
    len = strlen(pSSID);
    if(isStrPrintAble(pSSID, len))
    {
	SetBelkinParameter (WIFI_CLIENT_SSID,pSSID);
    }
    else
    {
	int idx = 0, len =0;
	char tmp[WIFI_MAXSTR_LEN];

	memset(tmp, 0, WIFI_MAXSTR_LEN);
	len = strlen(pSSID);
	snprintf(tmp, sizeof(tmp), "0x");
	for (idx = 0; idx < len; idx++)
	    snprintf(tmp + 2 + (idx*2), sizeof(tmp), "%02X", (unsigned char)pSSID[idx]);

	SetBelkinParameter (WIFI_CLIENT_SSID,tmp);
    }
    /* copy cipher and plain pass len if not exists */
    if(pPass[strlen(pPass) - 1] == '\n')
    {
      snprintf(lenstr, sizeof(lenstr), "%02X", strlen(pPass));
      strcat(pPass,lenstr);

      memset(lenstr, 0, sizeof(lenstr));
      snprintf(lenstr, sizeof(lenstr), "%02X", gPassPlainTextLen);
      strcat(pPass,lenstr);
    }

    SetBelkinParameter (WIFI_CLIENT_PASS,pPass);
    SetBelkinParameter (WIFI_CLIENT_AUTH,pAuth);
    SetBelkinParameter (WIFI_CLIENT_ENCRYP,pEnc);
    SetBelkinParameter (WIFI_AP_CHAN,channel);
#ifdef WeMo_INSTACONNECT
    SetBelkinParameter (WIFI_BRIDGE_LIST,gBridgeNodeList);
    //SetBelkinParameter (WIFI_PROFILE_LIST,gProfileList);
    setProfileList(gProfileList);
    SetBelkinParameter (WIFI_CLIENT_MAC,gRouterProfileMAC);
#endif
    gWiFiConfigured=1;
    AsyncSaveData();
    return 0;
}

int ledStatusOff (void)
{
     //APP_LOG("WiFiApp", LOG_DEBUG,"State 4: after 30 seconds OFF");
     SetWiFiLED(0x04);
     return SUCCESS;
}

int connectHiddenNetwork(int *channel, char *pSSID,char *pAuth,char *pEnc,char *pPass, int boot)
{

  PMY_SITE_SURVEY pAvlNetwork;
  int count=0;
  int chan=-1, i=0;
  int retVal=-1;
  char auth[SIZE_20B];
  char encrypt[SIZE_16B];

  /* try to connect to the passed pSSID amongst only hidden networks */
  pAvlNetwork = (PMY_SITE_SURVEY) ZALLOC(sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE);
  if(!pAvlNetwork)
  {
      APP_LOG("WiFiApp", LOG_ERR,"Malloc Failed..exiting...");
#ifdef WeMo_INSTACONNECT
   handleInstaConnectFailure();
#endif
      return FAILURE;
  }

  gWiFiParams.channel = CHAN_HIDDEN_NW;
  EnableSiteSurvey();
  pluginUsleep(50000);
  getCompleteAPList (pAvlNetwork,&count);

#ifdef WeMo_INSTACONNECT
  initBestNodeInfo(&g_BestNodeInfo);
#endif

  for (i=0;i<count;i++)
  {
	  if (atoi(pAvlNetwork[i].Hidden))
	  {
#ifdef WeMo_INSTACONNECT
      /*Update Best Node Info*/
      updateBestNodeInfo(&g_BestNodeInfo, &(pAvlNetwork[i]), NULL, 1);
#endif

		  memset(auth, 0, sizeof(auth));
		  memset(encrypt, 0, sizeof(encrypt));

		  sscanf(pAvlNetwork[i].security, "%[^'\\/']/%s",auth,encrypt);

		  APP_LOG("WiFiApp", LOG_HIDE,
				  "Trying connect on (hidden) channel: %s - %s/%s", pAvlNetwork[i].channel, auth, encrypt);

		  chan = atoi((const char *)pAvlNetwork[i].channel);
		  if(boot)
		  {
		  	retVal = bootConnectHomeNetwork(chan,pSSID,auth,encrypt,pPass) ;
			if(retVal == STATE_INTERNET_NOT_CONNECTED)
				retVal = STATE_CONNECTED;
		  }
		  else
		  	retVal = connectHomeNetwork(chan,pSSID,auth,encrypt,pPass) ;

		  /*
		   * continue to loop through all hidden networks in case of un-successful attempt
		   */
		  if(retVal == STATE_CONNECTED)
		  {
			*channel = chan;
#ifdef WeMo_INSTACONNECT
			if(!g_usualSetup)
			{
			    APP_LOG("WiFiApp", LOG_DEBUG,"Insta connect setup mode...");
			    saveData(gWiFiParams.channel,gRouterSSID,auth,encrypt,pPass);
			}
			else
			{
			    APP_LOG("WiFiApp", LOG_DEBUG,"saving data...");
			    saveData(gWiFiParams.channel,pSSID,auth,encrypt,pPass);
			}
#else
			APP_LOG("WiFiApp", LOG_DEBUG,"save data...");
		        saveData(gWiFiParams.channel,pSSID,auth,encrypt,pPass);
#endif
			break;
		  }
		  else
		   APP_LOG("WiFiApp", LOG_CRIT,
				  "Connection attempt on (hidden) channel: %s failed!!", pAvlNetwork[i].channel);
	  }
  }

  if(i>=count) {
	  APP_LOG("WiFiApp", LOG_CRIT,"Traversed through all hidden networks.. couldn't connect...");
	  retVal = NETWORK_NOT_FOUND;
#ifdef WeMo_INSTACONNECT
	  handleInstaConnectFailure();
#endif
  }
  else
  {
	  if(retVal == STATE_CONNECTED)
	  {
		  /* connected to network successfully, exit the thread */
		  APP_LOG("WiFiApp", LOG_CRIT,"Connected to Home Network SSID <%s> !!! \n", pSSID);
		  /* switch off AP mode */
	  }
  }

  /* common exit point, free memory and exit */
  free (pAvlNetwork);
  return retVal;
}

typedef void (*sighandler_t)(int);

int createTimer (int time)
{
  struct itimerval tout_val;

  tout_val.it_interval.tv_sec = 0;
  tout_val.it_interval.tv_usec = 0;
  tout_val.it_value.tv_sec = time;
  tout_val.it_value.tv_usec = 0;
  setitimer(ITIMER_REAL, &tout_val,0);

  if(time) {
   APP_LOG("WiFiApp",LOG_DEBUG,"Started the timer for %d seconds...", time);
  } else {
   APP_LOG("WiFiApp",LOG_DEBUG,"Disarmed the timer...");
  }
  signal(SIGALRM,(sighandler_t)ledStatusOff);

  return 0;
}

void *InetMonitorTask(void *arg)
{
	int retVal;

    	tu_set_my_thread_name( __FUNCTION__ );

	APP_LOG("WiFiApp",LOG_DEBUG,"InetMonitorTask running...");

#ifdef __WIRED_ETH__
	if(g_bWiredEthernet) {
	// Wait for dhcp to complete
		APP_LOG("WiFiApp",LOG_CRIT,"Waiting for IP address in wired mode");
		for( ; ; ) {
			if(isValidIp()) {
				APP_LOG("WiFiApp",LOG_CRIT,"Got IP address in wired mode");
				setAPIfState("OFF");
				g_ra0DownFlag = 1; //RA0 interface is Down
				SetWiFiLED(0x01);
				if(!gistimerinit) {
					createTimer(LED_TIME_OFF_INTERVAL);
				}
				SetCurrentClientState(STATE_CONNECTED);
				NotifyInternetConnected();
#ifdef PRODUCT_WeMo_Insight
				{
					char SetUpCompleteTS[SIZE_32B];
					memset(SetUpCompleteTS, 0, sizeof(SetUpCompleteTS));
					if(!g_SetUpCompleteTS) {
						g_SetUpCompleteTS = GetUTCTime();
						sprintf(SetUpCompleteTS, "%lu", g_SetUpCompleteTS);
						SetBelkinParameter(SETUP_COMPLETE_TS, SetUpCompleteTS);
						AsyncSaveData();
					}
					APP_LOG("ITC: network",LOG_ERR,"UPnP updated on setup complete g_SetUpCompleteTS---%lu, SetUpCompleteTS--------%s:", g_SetUpCompleteTS, SetUpCompleteTS);
				}
#endif
				break;
			}
			sleep(1);
		}
	}
#endif

	while(1)
	{
		if( gExitInetMonitorThread )
		{
			APP_LOG("WiFiApp", LOG_DEBUG,"******* Internet Monitor thread exiting *****");
			pthread_exit(NULL);
		}

		pluginUsleep(120000000);

		if( gInetHealthPunch == 0 )
		{
			APP_LOG("WiFiApp",LOG_CRIT,"InetMonitorTask detected bad health ...");
			if( nwThd )
			{
				ExitCheckInetConnectThread();
				// pthread_kill(nwThd, SIGRTMIN);
			}
			nwThd = 0;

			APP_LOG("WiFiApp",LOG_DEBUG,"Removed Inet connectivity thread...");

			//Create a thread to monitor the client connection.
			gStartWiFiTest = 1;
			pthread_attr_init(&inet_attr);
			pthread_attr_setdetachstate(&inet_attr,PTHREAD_CREATE_DETACHED);
			retVal = pthread_create(&nwThd,&inet_attr, checkInetConnectivity, NULL);
			if(retVal < SUCCESS)
			{
				APP_LOG("WiFiApp",LOG_CRIT,
						"New Connection Thread not created, errno: %d", errno);
			}

		}
		else
		{
			APP_LOG("WiFiApp",LOG_DEBUG,"Inet connectivity thread health OK [%d]...", gInetHealthPunch);
			gInetHealthPunch = 0;
		}
	}

	pthread_exit(NULL);
}


int bootConnectHomeNetwork(int chan,char *pSSID,char *pAuth,char *pEnc,char *pPass)
{
    PWIFI_PAIR_PARAMS pWiFi;
    int ret=0,retVal=0;
    char password[SIZE_256B]; //the decrypted password string
    int hiddenChannel = (gWiFiParams.channel == CHAN_HIDDEN_NW)?1:0;

    APP_LOG("WiFiApp", LOG_CRIT,"%d-%s-%s-%s",chan,pSSID,pAuth,pEnc);
    APP_LOG("WiFiApp", LOG_HIDE,"Password: %s",pPass);

    pWiFi = (PWIFI_PAIR_PARAMS) MALLOC(sizeof(WIFI_PAIR_PARAMS));

    if(!pWiFi) {
        APP_LOG("WiFiApp", LOG_ERR, "Malloc FAILED\n");
        return FAILURE;
    }


#ifdef DECRYPT_PASSWORD
    /* Using base64 encoding/decoding scheme for password encryption */

    memset(password, 0, sizeof(password));
    if(strcmp(pAuth,"OPEN"))
    {
	    if(SUCCESS != decryptPassword(pPass, password))
	    {
		    APP_LOG("WiFiApp", LOG_ERR,"Base decoding failed...");
		    free(pWiFi);
		    return FAILURE;
	    }
	    else {
		    APP_LOG("WiFiApp", LOG_HIDE,"decoded passwd: %s", password);
		  }
    }
    else
	strncpy(password,"NOTHING", sizeof(password)-1);
#else
    strncpy(password, pPass, sizeof(password)-1);
#endif

    strncpy(pWiFi->SSID,pSSID, sizeof(pWiFi->SSID)-1);
    strncpy(pWiFi->AuthMode,pAuth, sizeof(pWiFi->AuthMode)-1);
    strncpy(pWiFi->EncrypType,pEnc, sizeof(pWiFi->EncrypType)-1);
    strncpy(pWiFi->Key,password, sizeof(pWiFi->Key)-1);
    pWiFi->channel = chan;

    ret = wifiPair (pWiFi,"apcli0");
    if (ret >= SUCCESS)
    {
	    APP_LOG("WiFiApp", LOG_DEBUG,"Pairing Successful");

	    pluginUsleep(500000);

	    memcpy (&gWiFiParams,pWiFi,sizeof(WIFI_PAIR_PARAMS));

#ifdef DECRYPT_PASSWORD
	    /* over-write the password as encrypted password string */
	    memset(gWiFiParams.Key,0,WIFI_MAXSTR_LEN);
	    memcpy(gWiFiParams.Key,pPass,WIFI_MAXSTR_LEN);
	    APP_LOG("WiFiApp", LOG_HIDE,"Updated Key: %s, pPass: %s", gWiFiParams.Key, pPass);
#endif
	    ret = isValidIp();

	    if(ret)
	    {
		    SetCurrentClientState(STATE_INTERNET_NOT_CONNECTED);

		    APP_LOG("WiFiApp", LOG_DEBUG, "State :%d", getCurrentClientState());
		    SetWiFiLED(0x01);
		    if(!gistimerinit)
		    {
			createTimer (LED_TIME_OFF_INTERVAL);
		    }

		    SetCurrentClientState(STATE_INTERNET_NOT_CONNECTED);
		    gDoDhcp = 0;
		    retVal = system("route del -net 239.0.0.0 netmask 255.0.0.0");
	    }

	    if(hiddenChannel)
	    {
		    APP_LOG("WiFiApp", LOG_DEBUG,"Saving hidden channel information");
		    gWiFiParams.channel = CHAN_HIDDEN_NW;
	    }

    	    gWiFiConfigured=1;

	    if(!nwThd)
	    {
		    //Create a thread to monitor the client connection.
		    gStartWiFiTest = 1;
		    gBootReconnect=1;
        	    APP_LOG("WiFiApp",LOG_DEBUG, "gBootReconnect=1");
		    pthread_attr_init(&inet_attr);
		    pthread_attr_setdetachstate(&inet_attr,PTHREAD_CREATE_DETACHED);
		    retVal = pthread_create(&nwThd, &inet_attr, checkInetConnectivity, NULL);
		    if(retVal < SUCCESS) {
			APP_LOG("WiFiApp",LOG_CRIT, "Connection Thread not created, errno: %d", errno);
		    }

		    if(!Inet_Monitor_thread)
		    {
			    /* create the Inet connectivity monitor thread */
			    pthread_attr_init(&inet_mon_attr);
			    pthread_attr_setdetachstate(&inet_mon_attr,PTHREAD_CREATE_DETACHED);
			    retVal = pthread_create(&Inet_Monitor_thread, &inet_mon_attr, InetMonitorTask, NULL);
			    if(retVal != 0)
			    {
				APP_LOG("WiFiApp",LOG_CRIT, "InetMon Thread not created, errno: %d", errno);
			    }
		    }

	    }
    }
    else
    {
        ret = INVALID_PARAMS;
    }

    free (pWiFi);
#ifdef DECRYPT_PASSWORD
#endif

    return ret;
}

void checkApCloseStatus()
{
	APP_LOG("WiFiApp",LOG_DEBUG, "In check Close AP Status...");
	if(!gAppCalledCloseAp)
	{
		APP_LOG("WiFiApp",LOG_DEBUG, "App did not close AP...");
		if (-1 != CloseApWaiting_thread)
		{
		    APP_LOG("WiFi", LOG_DEBUG, "close ap thread already created");
		    return;
		}
		/* CloseApWaitingThread itself turns off AP */
		ithread_create(&CloseApWaiting_thread, NULL, CloseApWaitingThread, NULL);
		APP_LOG("UPNP: Device", LOG_DEBUG, "AP closing now .......");
	}
	else
		APP_LOG("WiFiApp",LOG_DEBUG, "App has already closed AP, nothing to do...");
}

void* closeApThread(void *arg)
{
    	tu_set_my_thread_name( __FUNCTION__ );

	APP_LOG("WiFiApp",LOG_DEBUG, "In Close AP Thread...");
	int timeOut = *(int *)arg;
	free(arg);
	pluginUsleep(timeOut*1000000);
	checkApCloseStatus();
	APP_LOG("WiFiApp",LOG_DEBUG, "Close AP Thread exiting...");

	return 0;
}

void createCloseApThread(int timeout)
{
   int retVal;
   int *timeOut = (int*)CALLOC(1, sizeof(int));
   if(!timeOut)
   {
       APP_LOG("WiFi",LOG_DEBUG, "Memory could not be allocated for timeOut");
       resetSystem();
   }
   *timeOut = timeout;
   pthread_attr_init(&closeap_attr);
   pthread_attr_setdetachstate(&closeap_attr,PTHREAD_CREATE_DETACHED);
   retVal = pthread_create(&closeApThId,&closeap_attr,
         (void*)&closeApThread, (void*)timeOut);
   if(retVal < SUCCESS) {
      APP_LOG("WiFiApp",LOG_CRIT,
            "Close AP Thread not created, errno: %d", errno);
   }
   else
      APP_LOG("WiFiApp",LOG_DEBUG,
            "Close AP Thread created successfully");
}

void *ConnectWiFiTask(void *args)
{
	int ret = 0x00,ret1=0x0;
	int retVal = 0x00;
	PWIFI_PAIR_PARAMS pWiFi = (PWIFI_PAIR_PARAMS)args;
	int oldstate,oldtype;
	char buffer[SIZE_64B];


	memset(buffer, 0,  sizeof(buffer));
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE , &oldstate);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldtype);


	APP_LOG("WiFiApp", LOG_DEBUG, "############ WiFi pairing task is running ##############");
wifi_pair:
	ret = wifiPair(pWiFi, "apcli0");
	if (ret >= SUCCESS)
	{
		APP_LOG("WiFiApp", LOG_DEBUG, "WiFi physical connection established and dhcpc started");

		memcpy (&gWiFiParams,pWiFi,sizeof(WIFI_PAIR_PARAMS));

#ifdef DECRYPT_PASSWORD
	    /* over-write the password as encrypted password string */
	    memset(gWiFiParams.Key,0,sizeof(gWiFiParams.Key));
	    memcpy(gWiFiParams.Key,gUserKey,sizeof(gWiFiParams.Key));
	    APP_LOG("WiFiApp", LOG_HIDE,"Updated Key: %s, gUserKey: %s", gWiFiParams.Key, gUserKey);
#endif
		if(gReconnectFlag)
		{
			pluginUsleep(500000);
			gReconnectFlag=0;
			APP_LOG("WiFiApp", LOG_DEBUG,"Going to test connection now");
			ret1 = wifiTestConnection (INTERFACE_CLIENT, 10, 1);
		}
		else
		{
			//- Check IP address
			APP_LOG("WiFiApp", LOG_DEBUG, "################# Waiting IP address to be allocated now #################");
			ret1 = isValidIp();
			if (!ret1)
			{
				SetCurrentClientState(STATE_PAIRING_FAILURE_IND);
			}
		}
		//Just trying to avoid confusion with the pairing ret, changed Name to ret1 i.e. result of testConnection.
		//return value of wifiTestConnection is either 1 or 0. testing for 3 will make this loop never enter.
		//Also extending this loop to make sure all saving also happens once this connection is tested.
		if(STATE_CONNECTED == ret1 || STATE_INTERNET_NOT_CONNECTED == ret1)
		{
			APP_LOG("WiFiApp", LOG_DEBUG,"State: %d", getCurrentClientState());
			SetCurrentClientState(STATE_INTERNET_NOT_CONNECTED);
			SetWiFiLED(0x01);
		    if(!gistimerinit)
		    {
			createTimer (LED_TIME_OFF_INTERVAL);
		    }
			gDoDhcp = 0;
			retVal = system("route del -net 239.0.0.0 netmask 255.0.0.0");

			if(gIsHiddenChan)
			{
				APP_LOG("WiFiApp", LOG_DEBUG,"Saving hidden channel information");
				gWiFiParams.channel = CHAN_HIDDEN_NW;
				gIsHiddenChan=0;
			}
#ifdef WeMo_INSTACONNECT
			g_connectInstaStatus = APCLI_CONFIGURED;	//connectToInstaApTask exit
			if(!g_usualSetup)
			{
			    APP_LOG("WiFiApp", LOG_DEBUG,"Insta connect setup mode...");
			    saveData(gWiFiParams.channel,gRouterSSID,pWiFi->AuthMode,pWiFi->EncrypType,gWiFiParams.Key);
			    APP_LOG("WiFiApp", LOG_HIDE,"Saved router SSID: %s and Key to: %s", gRouterSSID, gWiFiParams.Key);
			    /* configure Bridge Ap*/
			    configureBridgeAp(gRouterSSID, pWiFi->AuthMode, pWiFi->EncrypType, pWiFi->Key, pWiFi->channel);
			    //TODO: alternatively can call closesetup method
			    system("ifconfig ra0 down");
			    g_ra0DownFlag = 1; //RA0 interface is Down
			    pluginUsleep(1000000);
			    NotifyInternetConnected();
		    #ifdef PRODUCT_WeMo_Insight
			    char SetUpCompleteTS[SIZE_32B];
			    memset(SetUpCompleteTS, 0, sizeof(SetUpCompleteTS));
			    if(!g_SetUpCompleteTS)
			    {
				g_SetUpCompleteTS = GetUTCTime();
				sprintf(SetUpCompleteTS, "%lu", g_SetUpCompleteTS);
				SetBelkinParameter(SETUP_COMPLETE_TS, SetUpCompleteTS);
				AsyncSaveData();
				APP_LOG("ITC: network", LOG_ERR,"UPnP  updated on setup complete g_SetUpCompleteTS---%lu, SetUpCompleteTS--------%s:", g_SetUpCompleteTS, SetUpCompleteTS);
			    }
		    #endif
			}
			else
			{
			    saveData(gWiFiParams.channel,pWiFi->SSID,pWiFi->AuthMode,pWiFi->EncrypType,gWiFiParams.Key);
			    APP_LOG("WiFiApp", LOG_HIDE,"Saved Key to: %s", gWiFiParams.Key);
			}
			//g_usualSetup= 0x00;
#else
			saveData(gWiFiParams.channel,pWiFi->SSID,pWiFi->AuthMode,pWiFi->EncrypType,gWiFiParams.Key);
		    	APP_LOG("WiFiApp", LOG_HIDE,"Saved Key to: %s", gWiFiParams.Key);
#endif

			if(!nwThd)
			{
				//Create a thread to monitor the client connection.
				gStartWiFiTest = 1;
				gInetSleepInterval = 1;	//run Internet status check every second
				APP_LOG("WiFiApp", LOG_DEBUG,"Changed gInetSleepInterval to %d", gInetSleepInterval);
				pthread_attr_init(&inet_attr);
				pthread_attr_setdetachstate(&inet_attr,PTHREAD_CREATE_DETACHED);
				retVal = pthread_create(&nwThd, &inet_attr, checkInetConnectivity, NULL);
				if(retVal < SUCCESS) {
					APP_LOG("WiFiApp",LOG_CRIT,
							"Connection Thread not created , retVal = %d", retVal);
				}


				if(!Inet_Monitor_thread)
				{
					/* create the Inet connectivity monitor thread */
					pthread_attr_init(&inet_mon_attr);
					pthread_attr_setdetachstate(&inet_mon_attr,PTHREAD_CREATE_DETACHED);
					retVal = pthread_create(&Inet_Monitor_thread, &inet_mon_attr, InetMonitorTask, NULL);
					if(retVal != 0)
					{
						APP_LOG("WiFiApp",LOG_CRIT,
								"InetMon Thread not created, errno: %d", errno);
					}
				}

			}
		} else {
			//MARK this as Pairing failure???
			APP_LOG("NetworkControl", LOG_CRIT, "########## Pairing failure-Connect ***********************");
			//We should save the configuration even if we couldn't obtain an IP
			//saveData(gWiFiParams.channel,pWiFi->SSID,pWiFi->AuthMode,pWiFi->EncrypType,gWiFiParams.Key);

#ifdef WeMo_INSTACONNECT
			handleInstaConnectFailure();
#endif
			SetCurrentClientState(STATE_IPADDR_NEGOTIATION_FAILED);
			StopDhcpRequest();
			ret = INVALID_PARAMS;
			if(nwThd)
				gExitReconnectThread=1;
		}
	}
	else
	{
		//- Put it as pairing failure
		APP_LOG("NetworkControl", LOG_CRIT, "######################## Pairing failure ***********************");


		// - Jira story 2252 - Apple airport express firmware version 7.6.1 bug
		if( (!strcmp(pWiFi->AuthMode,"WPA2PSK")) )
		{
		    APP_LOG("NetworkControl", LOG_CRIT, "Pairing failure in WPA mode, re-trying once using WEP mode...");
		    strncpy(buffer,pWiFi->AuthMode, sizeof(buffer)-1);
		    memset(pWiFi->AuthMode, 0, sizeof(pWiFi->AuthMode));
		    strncpy(pWiFi->AuthMode, "WEP", sizeof(pWiFi->AuthMode)-1);
		    ret = 0x0;
		    goto wifi_pair;
		}

		if(strlen(buffer) > 0x1)
		{
		    memset(pWiFi->AuthMode, 0, sizeof(pWiFi->AuthMode));
		    strncpy(pWiFi->AuthMode, buffer, sizeof(pWiFi->AuthMode)-1);
		    memset(buffer, 0, sizeof(buffer));
		}

#ifdef WeMo_INSTACONNECT
			handleInstaConnectFailure();
#endif
		SetCurrentClientState(STATE_PAIRING_FAILURE_IND);
		ret = INVALID_PARAMS;
	}

	/* kill the monitoring thread as we are done with the required processing */
	APP_LOG("NetworkControl", LOG_DEBUG, "Killing the monitoring thread...");
	if(-1 != WiFi_Monitor_thread)
		pthread_kill(WiFi_Monitor_thread, SIGRTMIN);

	free (pWiFi);
	setSetupRequested(0);
	WiFi_Connect_thread=-1;
	if (ret >= SUCCESS){
#ifdef WeMo_INSTACONNECT
		if(g_usualSetup)
#endif
		createCloseApThread(CLOSE_AP_TIMEOUT);
	}
	return (void *)ret;
}

int connectHomeNetwork(int chan,char *pSSID,char *pAuth,char *pEnc,char *pPass)
{
    PWIFI_PAIR_PARAMS pWiFi;
    int ret=0,retVal=0;
    char password[SIZE_256B]; //the decrypted password string
    int hiddenChannel = (gWiFiParams.channel == CHAN_HIDDEN_NW)?1:0;

    APP_LOG("WiFiApp", LOG_HIDE,"%d-%s-%s-%s", chan, pSSID, pAuth, pEnc);
    APP_LOG("WiFiApp", LOG_HIDE,"Password: %s",pPass);

	int networkState = getCurrentClientState();

    if((networkState > STATE_DISCONNECTED) && (networkState != STATE_PAIRING_FAILURE_IND)  &&
		(networkState != STATE_IPADDR_NEGOTIATION_FAILED))
	{
        APP_LOG("WiFiApp", LOG_DEBUG,"#### Network already connected to %s #####", gWiFiParams.SSID);
#ifdef WeMo_INSTACONNECT
	g_connectInstaStatus = APCLI_CONFIGURED;
#endif
        return networkState;
    }

    pWiFi = (PWIFI_PAIR_PARAMS) ZALLOC(sizeof(WIFI_PAIR_PARAMS));

    if(!pWiFi)
	{
        APP_LOG("WiFiApp", LOG_ERR,"Malloc FAILED");
#ifdef WeMo_INSTACONNECT
   handleInstaConnectFailure();
#endif
        return FAILURE;
    }



#ifdef DECRYPT_PASSWORD
    /* Using base64 encoding/decoding scheme for password encryption */

    memset(password, 0, sizeof(password));
    if(strcmp(pAuth,"OPEN"))
    {
#ifdef WeMo_INSTACONNECT
    if(strlen(gRouterProfileMAC))
    {
      if(SUCCESS != decryptWithRouterInfoKeydata(pPass, password))
      {
        APP_LOG("WiFiApp", LOG_DEBUG,"Base decoding failed...with router key data");
        free(pWiFi);
        handleInstaConnectFailure();
        return FAILURE;
      }
      else
        APP_LOG("WiFiApp", LOG_DEBUG,"decoded passwd: %s...", password);
    }
    else
#endif
    {
	    if(SUCCESS != decryptPassword(pPass, password))
	    {
		    APP_LOG("WiFiApp", LOG_DEBUG,"Base decoding failed...");
		    free(pWiFi);
#ifdef WeMo_INSTACONNECT
	handleInstaConnectFailure();
#endif
		    return FAILURE;
	    }
	    else {
		    APP_LOG("WiFiApp", LOG_HIDE,"decoded passwd: %s...", password);
		  }
		}
    }
    else
	strncpy(password,"NOTHING", sizeof(password)-1);
#else
    strncpy(password, pPass, sizeof(password)-1);
#endif

    strncpy(pWiFi->SSID,pSSID, sizeof(pWiFi->SSID)-1);
    strncpy(pWiFi->AuthMode,pAuth, sizeof(pWiFi->AuthMode)-1);
    strncpy(pWiFi->EncrypType,pEnc, sizeof(pWiFi->EncrypType)-1);
    strncpy(pWiFi->Key,password, sizeof(pWiFi->Key)-1);
    pWiFi->channel = chan;

    /* JIRA WEMO-435: ra0 to be kept up while setup is in progress */
    if(!isSetupRequested())
    	gRa0DownFlag = 1;

#ifdef WeMo_INSTACONNECT
	g_connectInstaStatus = APCLI_CONFIGURING;	//connectToInstaApTask do nothing
#endif

    ret = wifiPair(pWiFi, "apcli0");
    if (ret >= SUCCESS)
    {
	    APP_LOG("WiFiApp", LOG_DEBUG,"Pairing Successful");
	    memcpy (&gWiFiParams,pWiFi,sizeof(WIFI_PAIR_PARAMS));

//Save the encrypted password
#ifdef DECRYPT_PASSWORD
	    /* over-write the password as encrypted password string */
	    memset(gWiFiParams.Key,0,sizeof(gWiFiParams.Key));
	    memcpy(gWiFiParams.Key,gUserKey,sizeof(gWiFiParams.Key));
	    APP_LOG("WiFiApp", LOG_HIDE,"Updated Key: %s, gUserKey: %s", gWiFiParams.Key, gUserKey);
#endif
	    /* udhcpc has been just started in wifiPair, wait for some time before test connection */
	    if(gReconnectFlag)
	    {
		    gReconnectFlag=0;
		    APP_LOG("WiFiApp", LOG_DEBUG,"Going to test connection now....");

		    ret = isValidIp();
		    if(ret == STATE_INTERNET_NOT_CONNECTED)
			ret = wifiTestConnection (INTERFACE_CLIENT,10,1);
	    }
	    else
	    {
		if(STATE_INTERNET_NOT_CONNECTED == (ret = isValidIp()))
		      ret = STATE_CONNECTED;

	    }

	    if(STATE_CONNECTED == ret)
	    {
		    APP_LOG("WiFiApp", LOG_DEBUG,"State: Connection OK");

		    SetWiFiLED(0x01);
		    if(!gistimerinit)
		    {
			createTimer (LED_TIME_OFF_INTERVAL);
		    }
		    SetCurrentClientState(STATE_INTERNET_NOT_CONNECTED);
		    gDoDhcp = 0;
		    retVal = system("route del -net 239.0.0.0 netmask 255.0.0.0");

#ifdef WeMo_INSTACONNECT
		    g_connectInstaStatus = APCLI_CONFIGURED;    //connectToInstaApTask exit
		    if(!g_usualSetup)
		    {
			APP_LOG("WiFiApp", LOG_DEBUG,"Insta connect mode...");
          saveData(gWiFiParams.channel,gRouterSSID,pWiFi->AuthMode,pWiFi->EncrypType,gWiFiParams.Key);
          APP_LOG("WiFiApp", LOG_DEBUG,"Saved router SSID: %s and Key to: %s", gRouterSSID, gWiFiParams.Key);
			/* configure Bridge Ap*/
			configureBridgeAp(gRouterSSID, pWiFi->AuthMode, pWiFi->EncrypType, pWiFi->Key, pWiFi->channel);
			//TODO: alternatively can call closesetup method
			system("ifconfig ra0 down");
			g_ra0DownFlag = 1; //RA0 interface is Down
			pluginUsleep(1000000);
			NotifyInternetConnected();

		    }
#endif

#if defined(WeMo_INSTACONNECT) || defined(WeMo_SMART_SETUP)
#ifdef PRODUCT_WeMo_Insight
		    if(!gReconnectFlag)
		    {
			    char SetUpCompleteTS[SIZE_32B];
			    memset(SetUpCompleteTS, 0, sizeof(SetUpCompleteTS));
			    if(!g_SetUpCompleteTS)
			    {
				g_SetUpCompleteTS = GetUTCTime();
				sprintf(SetUpCompleteTS, "%lu", g_SetUpCompleteTS);
				SetBelkinParameter(SETUP_COMPLETE_TS, SetUpCompleteTS);
				APP_LOG("ITC: network", LOG_ERR,"UPnP  updated on setup complete g_SetUpCompleteTS---%lu, SetUpCompleteTS--------%s:", g_SetUpCompleteTS, SetUpCompleteTS);
			    }
		    }
#endif
#endif
			if(!nwThd)
			{
				//Create a thread to monitor the client connection.
				gStartWiFiTest = 1;
				pthread_attr_init(&inet_attr);
				pthread_attr_setdetachstate(&inet_attr,PTHREAD_CREATE_DETACHED);
				retVal = pthread_create(&nwThd, &inet_attr, checkInetConnectivity, NULL);
				if(retVal < SUCCESS)
				{
					APP_LOG("WiFiApp",LOG_CRIT,"Connection Thread not created, errno: %d", errno);
				}

			    if(!Inet_Monitor_thread)
			    {
				    /* create the Inet connectivity monitor thread */
				    pthread_attr_init(&inet_mon_attr);
				    pthread_attr_setdetachstate(&inet_mon_attr,PTHREAD_CREATE_DETACHED);
				    retVal = pthread_create(&Inet_Monitor_thread, &inet_mon_attr, InetMonitorTask, NULL);
				    if(retVal != 0)
				    {
					    APP_LOG("WiFiApp",LOG_CRIT,"InetMon Thread not created, errno: %d", errno);
				    }
			    }

		    }
	    }
	    else
	    {
#ifdef WeMo_INSTACONNECT
		//if(gReconnectFlag)
		//    handleInstaConnectFailure();
#endif
	    }

	    if(hiddenChannel)
	    {
		    APP_LOG("WiFiApp", LOG_DEBUG,"Saving hidden channel information");
		    gWiFiParams.channel = CHAN_HIDDEN_NW;
	    }
    	    gWiFiConfigured=1;

    }
    else
    {
#ifdef WeMo_INSTACONNECT
	//if(gReconnectFlag)
	  //  handleInstaConnectFailure();
#endif
        ret = INVALID_PARAMS;
    }

    free (pWiFi);
#ifdef DECRYPT_PASSWORD
#endif

    return ret;
}


void StopWiFiPairingTask()
{
    int errnum;

    if (-1 != WiFi_Connect_thread)
    {
    	    APP_LOG("WiFiApp",LOG_DEBUG, "Sending kill to connectWiFitask...");
	    errnum = pthread_kill(WiFi_Connect_thread, SIGRTMIN);
	    if(errnum)
    	    	APP_LOG("WiFiApp",LOG_DEBUG, "pthread_kill ret: %d...",errnum);

	    WiFi_Connect_thread = -1;
	    setSetupRequested(0);
    }
}


void *ConnectHiddenNwTask(void *args)
{
	PWIFI_PAIR_PARAMS pHiddenNwParams = (PWIFI_PAIR_PARAMS)args;
	int hidChan=-1, retVal=-1;

    	tu_set_my_thread_name( __FUNCTION__ );

	retVal = connectHiddenNetwork(&hidChan, pHiddenNwParams->SSID, pHiddenNwParams->AuthMode, pHiddenNwParams->EncrypType,pHiddenNwParams->Key,0);

	if(retVal == STATE_CONNECTED)
        	APP_LOG("WiFiApp", LOG_CRIT,"CONNECTED TO %s CHAN: %d",pHiddenNwParams->SSID,hidChan);
	free(pHiddenNwParams);
	setSetupRequested(0);
	if(retVal == STATE_CONNECTED)
	{
#ifdef WeMo_INSTACONNECT
		if(g_usualSetup)
#endif
		createCloseApThread(CLOSE_AP_TIMEOUT);
	}
	return NULL;
}

int threadConnectHiddenNetwork(PWIFI_PAIR_PARAMS pWiFi)
{
    int retVal;

    pthread_attr_init(&hid_attr);
    pthread_attr_setdetachstate(&hid_attr,PTHREAD_CREATE_DETACHED);
    retVal = pthread_create(&Hidden_Connect_thread, &hid_attr, ConnectHiddenNwTask, (void *)pWiFi);
    if(retVal != 0)
    {
    	APP_LOG("WiFiApp",LOG_CRIT,
		    "HidCon Thread not created, errno: %d", errno);
    }

    return STATE_DISCONNECTED;

}


void *MonitorWiFiTask (void *args)
{
    	tu_set_my_thread_name( __FUNCTION__ );

    	APP_LOG("WiFiApp",LOG_DEBUG, "Running MonitorWiFiTask...");
	/* sleep for 70 seconds and kill the connectWiFiThread if it exists */
	pluginUsleep(70000000);

    	APP_LOG("WiFiApp",LOG_DEBUG, "Going to kill connectWiFitask if it exists...");
	StopWiFiPairingTask();
	return NULL;
}

int threadConnectHomeNetwork(int chan,char *pSSID,char *pAuth,char *pEnc,char *pPass)
{
    PWIFI_PAIR_PARAMS pWiFi;
    int retVal=0;
    char password[PASSWORD_MAX_LEN]; //the decrypted password string

    gIsHiddenChan = (gWiFiParams.channel == CHAN_HIDDEN_NW)?1:0;

    APP_LOG("WiFiApp", LOG_CRIT, "%d-%s-%s-%s", chan, pSSID, pAuth, pEnc);
    APP_LOG("WiFiApp", LOG_HIDE, "Password: %s", pPass);

	int networkStatus = getCurrentClientState();

	if (networkStatus == STATE_PAIRING_FAILURE_IND || networkStatus == STATE_IPADDR_NEGOTIATION_FAILED)
	{
		//- New connection, reset if previous one is incorrect
		SetCurrentClientState(STATE_DISCONNECTED);
	}

    if((networkStatus > STATE_DISCONNECTED) && (networkStatus != STATE_PAIRING_FAILURE_IND)  &&
		(networkStatus != STATE_IPADDR_NEGOTIATION_FAILED))
	{
        APP_LOG("WiFiApp", LOG_DEBUG,"#### already connected to %s ####", gWiFiParams.SSID);
	setSetupRequested(0);
#ifdef WeMo_INSTACONNECT
	g_connectInstaStatus = APCLI_CONFIGURED;
#endif
        return FAILURE;
    }

    pWiFi = (PWIFI_PAIR_PARAMS) ZALLOC(sizeof(WIFI_PAIR_PARAMS));

    if(!pWiFi)
   {
        APP_LOG("WiFiApp", LOG_ERR,"Malloc FAILED");
   setSetupRequested(0);
#ifdef WeMo_INSTACONNECT
   handleInstaConnectFailure();
#endif
        return FAILURE;
    }

#ifdef DECRYPT_PASSWORD
    /* Using base64 encoding/decoding scheme for password encryption */

    memset(password, 0, sizeof(password));
    if(strcmp(pAuth,"OPEN"))
    {
	    if( (!pPass) || (strlen(pPass) < 0x1) || (strcmp(pPass, "(null)") == 0x0) )
            {
                    APP_LOG("WiFiApp", LOG_CRIT,"Requesting pairing without password, while Auth set... failed...");
		    setSetupRequested(0);
#ifdef WeMo_INSTACONNECT
	handleInstaConnectFailure();
#endif
                    return FAILURE;
            }
#ifdef WeMo_INSTACONNECT
      if(strlen(gRouterProfileMAC))
      {
    if(SUCCESS != decryptWithRouterInfoKeydata(pPass, password))
    {
        APP_LOG("WiFiApp", LOG_DEBUG,"Base decoding failed...with router key data");
        free(pWiFi);
        setSetupRequested(0);
        handleInstaConnectFailure();
        return FAILURE;
    }
    else
        APP_LOG("WiFiApp", LOG_DEBUG,"decoded passwd: %s...", password);
      }
      else
#endif
      {
	    if(SUCCESS != decryptPassword(pPass, password))
	    {
		    APP_LOG("WiFiApp", LOG_DEBUG,"Base decoding failed...");
		    free(pWiFi);
		    setSetupRequested(0);
#ifdef WeMo_INSTACONNECT
	handleInstaConnectFailure();
#endif
		    return FAILURE;
	    }
	    else {
		    APP_LOG("WiFiApp", LOG_HIDE,"decoded passwd: %s...", password);
		  }
				}
    }
    else
	strncpy(password,"NOTHING", sizeof(password)-1);
#else
    strncpy(password, pPass, sizeof(password)-1);
#endif

    strncpy(pWiFi->SSID,pSSID, sizeof(pWiFi->SSID)-1);
    strncpy(pWiFi->AuthMode,pAuth, sizeof(pWiFi->AuthMode)-1);
    strncpy(pWiFi->EncrypType,pEnc, sizeof(pWiFi->EncrypType)-1);
    strncpy(pWiFi->Key,password, sizeof(pWiFi->Key)-1);
    pWiFi->channel = chan;

    StopWiFiPairingTask();

    pthread_attr_init(&con_attr);
    pthread_attr_setdetachstate(&con_attr,PTHREAD_CREATE_DETACHED);
    retVal = pthread_create(&WiFi_Connect_thread, &con_attr, ConnectWiFiTask, (void *)pWiFi);
    if(retVal != 0)
    {
    	APP_LOG("WiFiApp",LOG_CRIT,
		    "WifiCon Thread not created, errno: %d", errno);
    }

    pthread_attr_init(&mon_attr);
    pthread_attr_setdetachstate(&mon_attr,PTHREAD_CREATE_DETACHED);
    retVal = pthread_create(&WiFi_Monitor_thread, &mon_attr, MonitorWiFiTask, NULL);
    if(retVal != 0)
    {
    	APP_LOG("WiFiApp",LOG_CRIT,
		    "WifiMon Thread not created, errno: %d", errno);
    }

    return STATE_DISCONNECTED;
}

#ifdef WeMo_SMART_SETUP

int AttemptPairing(PWIFI_PAIR_PARAMS pWiFi)
{
	int ret = 0, ret1 = 0;
	int retVal=0;
	char buffer[SIZE_64B];
wifi_pair:
	ret = wifiPair(pWiFi, "apcli0");
	if (ret >= SUCCESS)
	{
		APP_LOG("WiFiApp", LOG_DEBUG, "WiFi physical connection established and dhcpc started");

		memcpy (&gWiFiParams,pWiFi,sizeof(WIFI_PAIR_PARAMS));

#ifdef DECRYPT_PASSWORD
	    /* over-write the password as encrypted password string */
	    memset(gWiFiParams.Key,0,sizeof(gWiFiParams.Key));
	    memcpy(gWiFiParams.Key,gUserKey,sizeof(gWiFiParams.Key));
	    APP_LOG("WiFiApp", LOG_HIDE,"Updated Key: %s, gUserKey: %s", gWiFiParams.Key, gUserKey);
#endif
		if(gReconnectFlag)
		{
			pluginUsleep(500000);
			gReconnectFlag=0;
			APP_LOG("WiFiApp", LOG_DEBUG,"Going to test connection now");
			ret1 = wifiTestConnection (INTERFACE_CLIENT, 10, 1);
		}
		else
		{
			//- Check IP address
			APP_LOG("WiFiApp", LOG_DEBUG, "################# Waiting IP address to be allocated now #################");
			ret1 = isValidIp();
			if (!ret1)
			{
				SetCurrentClientState(STATE_PAIRING_FAILURE_IND);
			}
		}
		//Just trying to avoid confusion with the pairing ret, changed Name to ret1 i.e. result of testConnection.
		//return value of wifiTestConnection is either 1 or 0. testing for 3 will make this loop never enter.
		//Also extending this loop to make sure all saving also happens once this connection is tested.
		if(STATE_CONNECTED == ret1 || STATE_INTERNET_NOT_CONNECTED == ret1)
		{
			APP_LOG("WiFiApp", LOG_DEBUG,"State: %d", getCurrentClientState());
			SetCurrentClientState(STATE_INTERNET_NOT_CONNECTED);
			SetWiFiLED(0x01);
		    if(!gistimerinit)
		    {
			createTimer (LED_TIME_OFF_INTERVAL);
		    }
			gDoDhcp = 0;
			retVal = system("route del -net 239.0.0.0 netmask 255.0.0.0");

			if(gIsHiddenChan)
			{
				APP_LOG("WiFiApp", LOG_DEBUG,"Saving hidden channel information");
				gWiFiParams.channel = CHAN_HIDDEN_NW;
				gIsHiddenChan=0;
			}

#ifdef PRODUCT_WeMo_Insight
			char SetUpCompleteTS[SIZE_32B];
			memset(SetUpCompleteTS, 0, sizeof(SetUpCompleteTS));
			if(!g_SetUpCompleteTS)
			{
			    g_SetUpCompleteTS = GetUTCTime();
			    sprintf(SetUpCompleteTS, "%lu", g_SetUpCompleteTS);
			    SetBelkinParameter(SETUP_COMPLETE_TS, SetUpCompleteTS);
			    APP_LOG("ITC: network", LOG_ERR,"UPnP  updated on setup complete g_SetUpCompleteTS---%lu, SetUpCompleteTS--------%s:", g_SetUpCompleteTS, SetUpCompleteTS);
			}
#endif

			saveData(gWiFiParams.channel,pWiFi->SSID,pWiFi->AuthMode,pWiFi->EncrypType,gWiFiParams.Key);
		    	APP_LOG("WiFiApp", LOG_HIDE,"Saved Key to: %s", gWiFiParams.Key);

			if(!nwThd)
			{
				//Create a thread to monitor the client connection.
				gStartWiFiTest = 1;
				gInetSleepInterval = 1;	//run Internet status check every second
				APP_LOG("WiFiApp", LOG_DEBUG,"Changed gInetSleepInterval to %d", gInetSleepInterval);
				pthread_attr_init(&inet_attr);
				pthread_attr_setdetachstate(&inet_attr,PTHREAD_CREATE_DETACHED);
				retVal = pthread_create(&nwThd, &inet_attr, checkInetConnectivity, NULL);
				if(retVal < SUCCESS) {
					APP_LOG("WiFiApp",LOG_ERR,
							"Connection Thread not created, retVal = %d", retVal);
				}


				if(!Inet_Monitor_thread)
				{
					/* create the Inet connectivity monitor thread */
					pthread_attr_init(&inet_mon_attr);
					pthread_attr_setdetachstate(&inet_mon_attr,PTHREAD_CREATE_DETACHED);
					retVal = pthread_create(&Inet_Monitor_thread, &inet_mon_attr, InetMonitorTask, NULL);
					if(retVal != 0)
					{
						APP_LOG("WiFiApp",LOG_ERR,
								"InetMon Thread not created, errno: %d", errno);
					}
				}

			}
		} else {
			//MARK this as Pairing failure???
			APP_LOG("NetworkControl", LOG_ERR, "########## Pairing failure-Connect ***********************");
			//We should save the configuration even if we couldn't obtain an IP
			//saveData(gWiFiParams.channel,pWiFi->SSID,pWiFi->AuthMode,pWiFi->EncrypType,gWiFiParams.Key);

			SetCurrentClientState(STATE_IPADDR_NEGOTIATION_FAILED);
			StopDhcpRequest();
			ret = INVALID_PARAMS;
			if(nwThd)
				gExitReconnectThread=1;
		}
	}
	else
	{
		//- Put it as pairing failure
		APP_LOG("NetworkControl", LOG_ERR, "######################## Pairing failure ***********************");


		// - Jira story 2252 - Apple airport express firmware version 7.6.1 bug
		if( (!strcmp(pWiFi->AuthMode,"WPA2PSK")) )
		{
		    APP_LOG("NetworkControl", LOG_ERR, "Pairing failure in WPA mode, re-trying once using WEP mode...");
		    strncpy(buffer,pWiFi->AuthMode, sizeof(buffer)-1);
		    memset(pWiFi->AuthMode, 0, sizeof(pWiFi->AuthMode));
		    strncpy(pWiFi->AuthMode, "WEP", sizeof(pWiFi->AuthMode)-1);
		    ret = 0x0;
		    goto wifi_pair;
		}

		if(strlen(buffer) > 0x1)
		{
		    memset(pWiFi->AuthMode, 0, sizeof(pWiFi->AuthMode));
		    strncpy(pWiFi->AuthMode, buffer, sizeof(pWiFi->AuthMode)-1);
		    memset(buffer, 0, sizeof(buffer));
		}

		SetCurrentClientState(STATE_PAIRING_FAILURE_IND);
		ret = INVALID_PARAMS;
	}
	setSetupRequested(0);
	return ret1;
}

int syncConnectHomeNetwork(PWIFI_PAIR_PARAMS pWiFi)
{
	char password[PASSWORD_MAX_LEN]; //the decrypted password string

	gIsHiddenChan = (gWiFiParams.channel == CHAN_HIDDEN_NW)?1:0;

	APP_LOG("WiFiApp", LOG_CRIT,"%d-%s-%s-%s", pWiFi->channel, pWiFi->SSID, pWiFi->AuthMode, pWiFi->EncrypType);
	APP_LOG("WiFiApp", LOG_HIDE,"Password: %s", pWiFi->Key);

	int networkStatus = getCurrentClientState();

	if (networkStatus == STATE_PAIRING_FAILURE_IND || networkStatus == STATE_IPADDR_NEGOTIATION_FAILED)
	{
		//- New connection, reset if previous one is incorrect
		SetCurrentClientState(STATE_DISCONNECTED);
	}

	if((networkStatus > STATE_DISCONNECTED) && (networkStatus != STATE_PAIRING_FAILURE_IND)  &&
			(networkStatus != STATE_IPADDR_NEGOTIATION_FAILED))
	{
		APP_LOG("WiFiApp", LOG_DEBUG,"#### already connected to %s ####", gWiFiParams.SSID);
		setSetupRequested(0);
		return FAILURE;
	}

#ifdef DECRYPT_PASSWORD
	/* Using base64 encoding/decoding scheme for password encryption */

	memset(password, 0, sizeof(password));
	if(strcmp(pWiFi->AuthMode,"OPEN"))
	{
		if( (!pWiFi->Key) || (strlen(pWiFi->Key) < 0x1) || (strcmp(pWiFi->Key, "(null)") == 0x0) )
		{
			APP_LOG("WiFiApp", LOG_ERR,"Requesting pairing without password, while Auth set... failed...");
			setSetupRequested(0);
			return FAILURE;
		}
		if(SUCCESS != decryptPassword(pWiFi->Key, password))
		{
			APP_LOG("WiFiApp", LOG_DEBUG,"Base decoding failed...");
			setSetupRequested(0);
			return FAILURE;
		}
		else
			APP_LOG("WiFiApp", LOG_DEBUG,"decoded passwd: %s...", password);
	}
	else
		strncpy(password,"NOTHING", sizeof(password)-1);
#else
	strncpy(password, pPass, sizeof(password)-1);
#endif

	/* over-write key with decrypted password */
	memset(pWiFi->Key, 0, sizeof(pWiFi->Key));
	strncpy(pWiFi->Key,password, sizeof(pWiFi->Key)-1);
	APP_LOG("UPNPDevice",LOG_HIDE,"Pairing with %s",pWiFi->Key);

	if(AttemptPairing(pWiFi))
	{
		/*success*/
		APP_LOG("UPNPDevice",LOG_DEBUG,"Pairing attempt successful");
		/* store back encrypted key in pWiFiParams */
		strncpy(pWiFi->Key, gUserKey, sizeof(pWiFi->Key)-1);
		return SUCCESS;
	}
	else
	{
		/*success*/
		APP_LOG("UPNPDevice",LOG_DEBUG,"Pairing attempt failed");
		/* store back encrypted key in pWiFiParams */
		strncpy(pWiFi->Key, gUserKey, sizeof(pWiFi->Key)-1);
		return FAILURE;
	}
}

int pairToRouter(PWIFI_PAIR_PARAMS pWiFi)
{
	int ret = FAILURE;
	if(CHAN_HIDDEN_NW == pWiFi->channel)
	{
		int hidChan=-1, retVal=-1;
		APP_LOG("UPNPDevice",LOG_DEBUG,"Connect request for hidden network: %s......", pWiFi->SSID);

		retVal = connectHiddenNetwork(&hidChan, pWiFi->SSID, pWiFi->AuthMode, pWiFi->EncrypType,pWiFi->Key,0);

		if(retVal == STATE_CONNECTED)
		{
			APP_LOG("WiFiApp", LOG_CRIT,"CONNECTED TO %s CHAN: %d",pWiFi->SSID,hidChan);
			ret = SUCCESS;
		}
		setSetupRequested(0);
	}
	else
	{
		APP_LOG("UPNPDevice",LOG_DEBUG,"connect to selected network: %s", pWiFi->SSID);
		ret = syncConnectHomeNetwork(pWiFi);
	}

	if(ret == SUCCESS)
	{
#ifdef WeMo_SMART_SETUP_V2
	    if(g_isAppConnected)
	    {
		APP_LOG("UPNPDevice",LOG_DEBUG,"app still conneced to AP !!");
		createCloseApThread(SMART_AP_TIMEOUT);
	    }
	    else
#endif
	    {
		/* Connected to router, turn down AP and switch UPnP */
		system("ifconfig ra0 down");
		g_ra0DownFlag = 1; //RA0 interface is Down
		pluginUsleep(1000000);
		NotifyInternetConnected();
	    }
	}
	return ret;
}
#endif

int setAPIfState(char *mode)
{
	int ret, state;

	APP_LOG("WiFiApp", LOG_DEBUG,"Setting AP mode %s",mode);
	if((strcmp(mode,"ON")==0) || (strcmp(mode,"on")==0)){
		state = 1;
		SetWiFiLED(0x05);
	}
	else{
		state = 0;
	}

	ret = wifiChangeAPIfState(state);
	if(SUCCESS == ret){
		APP_LOG("WiFiApp", LOG_DEBUG,"Successfully set AP mode");
		ret = SUCCESS;
	}
	else{
		APP_LOG("WiFiApp", LOG_CRIT,"Failed in set AP mode");
	}


	return ret;
}

int setClientIfState(char *mode)
{
	int ret, state;

	APP_LOG("WiFiApp", LOG_DEBUG,"Setting Client mode %s",mode);
	if((strcmp(mode,"ON")==0) || (strcmp(mode,"on")==0)){
		state = 1;
	}
	else{
		state = 0;
	}

	ret = wifiChangeClientIfState(state);
	if(SUCCESS == ret){
		APP_LOG("WiFiApp", LOG_DEBUG,"Successfully set Client mode");
	}
	else{
		APP_LOG("WiFiApp", LOG_CRIT,"Failed in set Client mode");
	}

	return ret;
}

int getWiFiStatsCounters (PWIFI_STAT_COUNTERS wifistatcounters)
{
	if((wifiGetStats (INTERFACE_CLIENT, wifistatcounters)) < 0 ){
		return FAILURE;
	}
	return SUCCESS;
}

int setAPChnl(char *channel)
{

	if(!channel) {
        APP_LOG("WiFiApp", LOG_ERR, "Param ER ");
        return FAILURE;
    }
	if((atoi(channel))<=11){
		wifisetChannelOfAP (channel);
		APP_LOG("WiFiApp", LOG_DEBUG,"Successfully set AP channel to %s",channel);
		wifiGetChannel(INTERFACE_CLIENT, &g_channelAp);
		APP_LOG("NetworkControl", LOG_CRIT, "Updated g_channelAp to [%d]", g_channelAp);
		return SUCCESS;
	}
	else{
		APP_LOG("WiFiApp", LOG_CRIT,"Wrong channel value: %s",channel);
		return FAILURE;
	}
}
#define SIGNALTIMEVAL 180
#define ROUTERTIMEVAL 60
#define SIGNALTHRESHOLD 25
#define IPCHANGEDETECTTIMEVAL 60
extern UpnpDevice_Handle device_handle;

#ifdef _OPENWRT_
void *ntpUpdateThread(void *arg)
{
	tu_set_my_thread_name( __FUNCTION__ );

	APP_LOG("WiFiApp", LOG_DEBUG, "NTP update thread Running..");
	while(1) {
		if(RunNTP() == BELKIN_SUCCESS) {
		// Time has been set, exit thread
			break;
		}
	// Time couuld not be set, wait for a while and then retry
		sleep(DELAY_60SEC);
	}
	gNTPTimeSet = 1;
	APP_LOG("WiFiApp", LOG_ALERT, "Date set, thread exiting..");
	gNTPThreadRunning = 0;

	return NULL;
}

int createNTPUpdateThread()
{
   int retVal;

	if(!gNTPThreadRunning) {
		gNTPThreadRunning = 1;
		pthread_attr_init(&ntp_attr);
		pthread_attr_setdetachstate(&ntp_attr,PTHREAD_CREATE_DETACHED);
		retVal = pthread_create(&ntpthread,&ntp_attr,ntpUpdateThread,NULL);
		if(retVal < SUCCESS) {
			APP_LOG("WiFiApp",LOG_CRIT,"NTP Thread not created");
			gNTPThreadRunning = 0;
		}
	}

   return 0;
}

#else
// GemTek version
void* ntpUpdateThread(void *arg)
{
    tu_set_my_thread_name( __FUNCTION__ );

    APP_LOG("WiFiApp", LOG_DEBUG, "NTP update thread Running..");
    while(1)
    {
	sleep(DELAY_60SEC);
	if (IsNtpUpdate())
	{
		APP_LOG("WiFiApp", LOG_ALERT, "NTP updated, thread exiting..");
		return NULL;
	}
	else
	{
	    RunNTP();
	}
    }

}

int createNTPUpdateThread()
{
	int retVal;

	pthread_attr_init(&ntp_attr);
	pthread_attr_setdetachstate(&ntp_attr,PTHREAD_CREATE_DETACHED);
	retVal = pthread_create(&ntpthread,&ntp_attr,
			(void*)&ntpUpdateThread, NULL);
	if(retVal < SUCCESS) {
		APP_LOG("WiFiApp",LOG_CRIT,
				"NTP Thread not created");
	}

	return 0;
}
#endif

int checkRemoteDataBytes(void)
{
    unsigned int remoteDataBytes = UDS_getRemoteDataBytes();
    if(remoteDataBytes)
    {
	APP_LOG("WiFiApp", LOG_DEBUG, "Some data bytes received from Remote: %d", remoteDataBytes);
	UDS_setRemoteDataBytes();
	return 0x01;
    }
    else
    {
	APP_LOG("WiFiApp", LOG_DEBUG, "No Data bytes received from Router..");
	return 0x00;
    }
}

int checkDataBytesFromRouter(void)
{
    unsigned int remoteDataBytes = UDS_getRemoteDataBytes();
    if(gLocalDataBytes || remoteDataBytes)
    {
	APP_LOG("WiFiApp", LOG_DEBUG, "Some data bytes received from Router, Local: %u and Remote: %u",
		gLocalDataBytes, remoteDataBytes);
	gLocalDataBytes = 0;
	UDS_setRemoteDataBytes();
	return 0x01;
    }
    else
    {
	APP_LOG("WiFiApp", LOG_DEBUG, "No Data bytes received from Router..");
	return 0x00;
    }
}


void* checkInetConnectivity(void *args)
{
	int retVal = FAILURE, prevRetVal=1;//let the initial log appear
	int curState=0;
	int signalwait=0;
	int checkInetConn;

	tu_set_my_thread_name( __FUNCTION__ );

	APP_LOG("WiFiApp", LOG_DEBUG,"*******Internet Connection Status thread started *****");

	while (1)
	{
		if(IS_FIRMWARE_FLASHING)
		{
			APP_LOG("WiFiApp", LOG_DEBUG, "******* Internet Connection Status thread terminated *****");
			nwThd = 0;
			gExitReconnectThread=1;
			pthread_exit(NULL);
		}

		if(gExitReconnectThread)
		{
			APP_LOG("WiFiApp", LOG_DEBUG,"******* Internet Connection Status thread exiting *****");
			nwThd = 0;
			gExitReconnectThread=0;
			pthread_exit(NULL);
		}

		gInetHealthPunch++;

		checkInetConn = !(signalwait % gInetSleepInterval);

		if(gStartWiFiTest && !isSetupRequested())
		{
			curState = getCurrentClientState();

			if(curState && checkInetConn)
			{
				APP_LOG("WiFiApp", LOG_DEBUG,"Checking Inet connection status");
                //[WEMO-32439] - Reduce ping times from 4 to 2
				if(checkRemoteDataBytes())
				{
				    APP_LOG("WiFiApp", LOG_DEBUG,"Inet connection already checked");
				    retVal = SUCCESS;
				}
				else
				{
				retVal = checkInetStatus(2);
				}
				APP_LOG("WiFiApp", LOG_DEBUG,"Checked Inet connection status %d", retVal);
			}
			else
			{
				/* check connectivity with router */
				if(!(signalwait % ROUTERTIMEVAL))
				{
					if(checkDataBytesFromRouter())
					{
					    APP_LOG("WiFiApp", LOG_DEBUG, "Router connectivity already checked..");
					    retVal = SUCCESS;
					}
					else
					{
					    retVal = checkRouterConnectivity();
					}

					if(retVal)
					{
						if(routerConnLostCount != 0)
						    APP_LOG("WiFiApp", LOG_ERR,"Connectivity with router lost..");
						gInetSleepInterval = 60;
						APP_LOG("WiFiApp", LOG_DEBUG,"Changed gInetSleepInterval to %d", gInetSleepInterval);
						routerConnLostCount++;
						routerConnCount=0;
						if(routerConnLostCount == 1) {
							APP_LOG("WiFiApp", LOG_CRIT,"Router Connectivity lost");
							CHECK_AND_INVOKE_NAT_REINIT(1);
						}
					}
				}
			}

			if (retVal != 0)
			{
				if(checkInetConn)
				{
					if(prevRetVal != retVal && inetConnLostCount != 0)
						APP_LOG("WiFiApp", LOG_ERR,"*******Internet Connection Lost*****");
#if defined(PRODUCT_WeMo_Baby) || defined(PRODUCT_WeMo_Streaming)
					gInetSleepInterval = 10;
#else
					gInetSleepInterval = 60;
#endif
					APP_LOG("WiFiApp", LOG_DEBUG,"Changed gInetSleepInterval to %d", gInetSleepInterval);
					if(curState == STATE_CONNECTED)
						SetCurrentClientState(STATE_INTERNET_NOT_CONNECTED);

					APP_LOG("WiFiApp", LOG_DEBUG,"State %d: No internet connection", getCurrentClientState());
					inetConnLostCount++;
					inetConnCount=0;
					if(inetConnLostCount == 1) {
						APP_LOG("WiFiApp", LOG_CRIT,"Internet Connection Lost");
						CHECK_AND_INVOKE_NAT_REINIT(1);
					}
				  }
				if(!(signalwait % 30))
					chkConnStatus();
			}
			else
			{
				if(checkInetConn)
				{
					SetCurrentClientState(STATE_CONNECTED);
					APP_LOG("WiFiApp", LOG_DEBUG,"Internet looks connected state %d-%d-%d-%d-%d-%d-%d-%d", getCurrentClientState(),\
						prevRetVal, retVal, inetConnCount, checkInetConn, signalwait, gInetSleepInterval, gStartWiFiTest);
					if(prevRetVal != retVal && inetConnCount != 0)
						APP_LOG("WiFiApp", LOG_DEBUG,"Internet is connected");

					inetConnLostCount=0;
					inetConnCount++;
					if(inetConnCount == 1)
					{
					    enqueueBugsense("INTERNET_CONNECTED");
					    APP_LOG("WiFiApp", LOG_CRIT,"Internet Connected, gNTPTimeSet: %d", gNTPTimeSet);
					}
					/* Internet connected, take the internet check freq back to default */
					gInetSleepInterval = CHECK_INET_TIMEVAL;
					APP_LOG("WiFiApp", LOG_DEBUG,"Changed gInetSleepInterval to %d", gInetSleepInterval);

#ifndef _OPENWRT_
// system time is correctly set to GMT/UTC on OpenWRT so this unneeded
               if(!gNTPTimeSet)
               {
                  RunNTP();
                  //gNTPTimeSet = 1;
                  createNTPUpdateThread();
                  if(gDstSupported)
                  {
                     sleep(10);
                     APP_LOG("WiFiApp", LOG_DEBUG,"DST supported...");
                     computeDstToggleTime(1);
                  }
               }
#endif
            }
         }

#ifdef __WIRED_ETH__
         if(!(signalwait % SIGNALTIMEVAL) && !g_bWiredEthernet)
#else
			if(!(signalwait % SIGNALTIMEVAL))
#endif
			{
				if(chksignalstrength())
				{
					APP_LOG("WiFiApp", LOG_DEBUG,"State 2: Poor connection");
#ifndef PRODUCT_WeMo_Light
					SetWiFiLED(0x02);
#endif
				}
			}

	 if(!(signalwait % IPCHANGEDETECTTIMEVAL))
	 {
#ifdef PRODUCT_WeMo_LEDLight
	     if(g_ra0DownFlag)
#endif	
		     detectIPChange();
	 }
			pluginUsleep(1000000); //loop every 1 sec
			++signalwait;
		}
		else
		{
			APP_LOG("WiFiApp", LOG_DEBUG,"Going to the next iteration...");
			pluginUsleep (5000000);
		}
	}
	pthread_exit(NULL);
}

extern void NotifyInternetConnected();


void reconnectHome()
{
     //Either the Home AP is down
     //or some security change has happened.
     //Do a Site Survey
     PMY_SITE_SURVEY pAvlNetwork;
     int retVal=FAILURE,chan=-1,retry_cnt=0;
     char *pSSID=NULL,*pAuth=NULL,*pEnc=NULL,*pPass=NULL;
     int count=0,i=0;
     int scanFailCnt=0;
#ifndef WeMo_INSTACONNECT
     char auth[20];
     char encrypt[12];
#endif
#ifdef WeMo_INSTACONNECT
  char *brlist = NULL, *profileList = NULL;
  char bestNode[SSID_LEN];
  char profileMac[MAX_MAC_LEN];
  char password[PASSWORD_MAX_LEN];
  char ssid[SSID_LEN];
  char auth[WIFI_MAXSTR_LEN];
  char encrypt[WIFI_MAXSTR_LEN];
  char channel[CHANNEL_LEN];
  int ret = SUCCESS;
  initBestNodeInfo(&g_BestNodeInfo);
#endif


     APP_LOG("Reconnect", LOG_DEBUG,"gWiFiConfigured:%d, gWiFiClientCurrState: %d",gWiFiConfigured,
	 	getCurrentClientState());

     /* To be allocated once */
     pAvlNetwork = (PMY_SITE_SURVEY) ZALLOC(sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE);
     if(!pAvlNetwork)
     {
        APP_LOG("Reconnect", LOG_ERR,"Malloc Failed..exiting Reconnect...");
        return;
     }

#ifdef WeMo_INSTACONNECT
     APP_LOG("WiFiApp", LOG_DEBUG,"removing br subnet route, apcli from br and reset flag...");
     delBridgeCliIfaceAndRoute();
     g_usualSetup = 0x00;    //unset flag
#endif


     while (gWiFiConfigured && !isSetupRequested())
     {
	     int currentState;
	     gReconnectFlag=1;

	     gInetHealthPunch++;

	     currentState = getCurrentClientState();
	     if(STATE_CONNECTED == currentState || STATE_INTERNET_NOT_CONNECTED == currentState)
		 {
		     APP_LOG("Reconnect", LOG_DEBUG,"network connected");
		     free (pAvlNetwork);
		#ifndef WeMo_INSTACONNECT
			 EnableSiteSurvey();
			 NotifyInternetConnected();
		#endif
		     return;
		}

#ifdef WeMo_INSTACONNECT

	    /* Save the router ssid in a global variable - to be used later in wifi save data */
	    memset(gRouterSSID, 0, sizeof(gRouterSSID));
  memcpy(gRouterSSID, gWiFiParams.SSID, sizeof(gRouterSSID));

	    /* intialize best node with router ssid */
	    memset(bestNode, 0, sizeof(bestNode));
  strncpy(bestNode, gWiFiParams.SSID, sizeof(bestNode)-1);
#if 0
	    chan = gWiFiParams.channel;
	    APP_LOG("WiFiApp", LOG_DEBUG, "channel: %d", chan);
#endif

  if(strlen(gBridgeNodeList) && strlen(gProfileList))
  {
    memset(profileMac, 0, sizeof(profileMac));
    brlist = gBridgeNodeList;
    APP_LOG("UPNPDevice", LOG_DEBUG,"get best node...");
    retVal = getBestNode(brlist, bestNode, &chan, profileMac);
    if(FAILURE == retVal)
      goto use_default;
    APP_LOG("UPNPDevice", LOG_DEBUG,"get best node done: channel: %d and profileMac: %s", chan, profileMac);
	    /* get best node, if any bridge list */
    if(chan != -1)
	    {
      APP_LOG("UPNPDevice", LOG_DEBUG,"node matched...");
      /* parse profile list */
      memset(ssid, 0, sizeof(ssid));
      memset(auth, 0, sizeof(auth));
      memset(password, 0, sizeof(password));
      memset(encrypt, 0, sizeof(encrypt));
      memset(channel, 0, sizeof(channel));
      profileList = gProfileList;
      APP_LOG("UPNPDevice", LOG_DEBUG,"parse profile list...");
      ret = parseProfileList(profileList, profileMac, ssid, auth, password, encrypt, channel);
      if(ret < 0)
	    {
        APP_LOG("UPNPDevice", LOG_DEBUG,"parse profile list failed...should not come here...");
        pluginUsleep(5000000);
        continue;
      }

      APP_LOG("UPNPDevice", LOG_DEBUG,"parsed profile list: ssid: %s auth: %s password: %s encrypt: %s channel: %s", ssid, auth, password, encrypt, channel);

      /* in case of hidden ssid */
      if(strlen(bestNode) <= 0 )
		{
        strncpy(bestNode, ssid, sizeof(bestNode)-1);
        APP_LOG("UPNPDevice", LOG_DEBUG,"updated bestnode: %s", bestNode);
		}

      /* update the saved password in a global variable - to be used later by WifiConn thread */
      memset(gUserKey, 0, sizeof(gUserKey));
      memcpy(gUserKey, password, sizeof(gUserKey));
      APP_LOG("UPNPDevice", LOG_DEBUG,"gUserKey: %s", password);

      /* update the router ssid in a global variable - to be used later in wifi save data */
      memset(gRouterSSID, 0, sizeof(gRouterSSID));
      memcpy(gRouterSSID, ssid, sizeof(gRouterSSID));
      APP_LOG("UPNPDevice", LOG_DEBUG,"gRouterSSID: %s", gRouterSSID);

      /* save profile mac */
      memset(gRouterProfileMAC, 0, sizeof(gRouterProfileMAC));
      memcpy(gRouterProfileMAC, profileMac, sizeof(gRouterProfileMAC));
      APP_LOG("UPNPDevice", LOG_DEBUG,"gRouterProfileMAC: %s", gRouterProfileMAC);

      /* updated saved data */
      strncpy(gWiFiParams.AuthMode, auth, sizeof(gWiFiParams.AuthMode)-1);
      strncpy(gWiFiParams.EncrypType, encrypt, sizeof(gWiFiParams.EncrypType)-1);
      strncpy(gWiFiParams.Key, password, sizeof(gWiFiParams.Key)-1);

      strncpy(gWiFiParams.SSID, bestNode, sizeof(gWiFiParams.SSID)-1);
      //gWiFiParams.channel = chan;

      pSSID=gWiFiParams.SSID;
      pAuth=gWiFiParams.AuthMode;
      pEnc=gWiFiParams.EncrypType;
      //chan = gWiFiParams.channel;
      pPass=gWiFiParams.Key;

      APP_LOG("Reconnect", LOG_DEBUG,"Trying to Reconnect to: %d-%s-%s-%s", chan, pSSID, pAuth, pEnc);

      if(gWiFiParams.channel != CHAN_HIDDEN_NW)
        retVal = connectHomeNetwork(chan,pSSID,pAuth,pEnc,pPass) ;
      else
        retVal = connectHiddenNetwork(&chan, pSSID, pAuth, pEnc, pPass, 0);
    }
    else
		{
        APP_LOG("UPNPDevice", LOG_DEBUG,"no node matched...");
        goto use_default;
		    }
		}
  else
  {
use_default:
    EnableSiteSurvey();
    pluginUsleep(50000);
    getCompleteAPList (pAvlNetwork,&count);
    checkScanFailCount(count, &scanFailCnt, MAX_SCAN_FAIL_CNT);

		for (i=0;i<count;i++)
		{
		    if(gWiFiParams.channel != CHAN_HIDDEN_NW)
		    {
			int len = strlen(gWiFiParams.SSID);
			if(!isStrPrintAble(gWiFiParams.SSID, len))
			{
			    int idx = 0;
			    char tmp[WIFI_MAXSTR_LEN];

			    memset(tmp, '\0', WIFI_MAXSTR_LEN);
			    len = strlen(gWiFiParams.SSID);
			    snprintf(tmp, sizeof(tmp), "0x");
			    for (idx = 0; idx < len; idx++)
				snprintf(tmp + 2 + (idx*2), sizeof(tmp), "%02X", (unsigned char)gWiFiParams.SSID[idx]);

			    memset(tmp, '\0', WIFI_MAXSTR_LEN);
			    strncpy(tmp, gWiFiParams.SSID, sizeof(tmp)-1);
			    strncpy(gWiFiParams.SSID, tmp, sizeof(gWiFiParams.SSID)-1);
			}
			if (!strcmp (pAvlNetwork[i].ssid,gWiFiParams.SSID))
			{
			    APP_LOG("Reconnect", LOG_DEBUG,
				    "pAvlNetwork[%d].channel: %s, gWiFiParams.channel: %d, \
				    pAvlNetwork[%d].ssid: %s, gWiFiParams.SSID: %s",
				    i, pAvlNetwork[i].channel, gWiFiParams.channel,
				    i, pAvlNetwork[i].ssid,gWiFiParams.SSID);

			    chan = atoi((const char *)pAvlNetwork[i].channel);

          /*fill best node info*/
          updateBestNodeInfo(&g_BestNodeInfo, &(pAvlNetwork[i]), NULL, 1);

          strncpy(gWiFiParams.SSID, bestNode, sizeof(gWiFiParams.SSID)-1);
          //gWiFiParams.channel = chan;
          pSSID=gWiFiParams.SSID;
          pAuth=gWiFiParams.AuthMode;
          pEnc=gWiFiParams.EncrypType;
          //chan = gWiFiParams.channel;
          pPass=gWiFiParams.Key;

          APP_LOG("Reconnect", LOG_DEBUG,"Trying to Reconnect to: %d-%s-%s-%s", chan, pSSID, pAuth, pEnc);

          /*if the connection was successful, break from the loop */
          retVal = connectHomeNetwork(chan,pSSID,pAuth,pEnc,pPass);
			    break;
			}
		    }
      else
      {
          if (atoi(pAvlNetwork[i].Hidden))
          {
            /*fill best node info*/
            updateBestNodeInfo(&g_BestNodeInfo, &(pAvlNetwork[i]), NULL, 1);

            strncpy(gWiFiParams.SSID, bestNode, sizeof(gWiFiParams.SSID)-1);
            //gWiFiParams.channel = chan;
            pSSID=gWiFiParams.SSID;
            pAuth=gWiFiParams.AuthMode;
            pEnc=gWiFiParams.EncrypType;
            //chan = gWiFiParams.channel;
            pPass=gWiFiParams.Key;

            APP_LOG("Reconnect", LOG_DEBUG,"Trying to Reconnect to: %d-%s-%s-%s", chan, pSSID, pAuth, pEnc);
            retVal = connectHomeNetwork(chan,pSSID,pAuth,pEnc,pPass);
            /*if the connection was successful, break from the loop */
            if(retVal == STATE_CONNECTED)
            {
              break;
		}
	    }
      }
    }
  }
#if 0
	    strncpy(gWiFiParams.SSID, bestNode, sizeof(gWiFiParams.SSID)-1);
	    //gWiFiParams.channel = chan;

	    pSSID=gWiFiParams.SSID;
	    pAuth=gWiFiParams.AuthMode;
	    pEnc=gWiFiParams.EncrypType;
	    //chan = gWiFiParams.channel;
	    pPass=gWiFiParams.Key;
	    APP_LOG("Reconnect", LOG_DEBUG,
		     "Tryin to Reconnect to <%s> .\n", pSSID);

	    APP_LOG("Reconnect", LOG_CRIT,"%d-%s-%s-%s", chan, pSSID, pAuth, pEnc);

	    if(gWiFiParams.channel != CHAN_HIDDEN_NW)
		retVal = connectHomeNetwork(chan,pSSID,pAuth,pEnc,pPass) ;
	    else
		retVal = connectHiddenNetwork(&chan, pSSID, pAuth, pEnc, pPass, 0);
#endif
#if 0
	    else
	    {
		    PWIFI_PAIR_PARAMS pHiddenNwParams=NULL;

		    APP_LOG("UPNPDevice",LOG_DEBUG,"Connect request for hidden network: %s......", pSSID);
		    pHiddenNwParams = allocateMemory(sizeof(WIFI_PAIR_PARAMS));

		    strncpy(pHiddenNwParams->SSID, pSSID, sizeof(pHiddenNwParams->SSID)-1);
		    strncpy(pHiddenNwParams->AuthMode, pAuth, sizeof(pHiddenNwParams->AuthMode)-1);
		    strncpy(pHiddenNwParams->EncrypType, pEnc, sizeof(pHiddenNwParams->EncrypType)-1);
		    strncpy(pHiddenNwParams->Key, pPass, sizeof(pHiddenNwParams->Key)-1);
		    pHiddenNwParams->channel = chan;

		    retVal = threadConnectHiddenNetwork(pHiddenNwParams);
	    }
#endif

#else

		 EnableSiteSurvey();
		 pluginUsleep(50000);
	     getCompleteAPList (pAvlNetwork,&count);
	     if(count == 0)
             {
                    scanFailCnt++;
                    APP_LOG("Reconnect", LOG_CRIT,"No networks found in SiteScan: %d...", scanFailCnt);
             }
             else
                    scanFailCnt=0;

        //[WEMO-31632] - Force to up apcli0 interface again when no network found
        if ((scanFailCnt > 0 ) && (scanFailCnt % (MAX_SCAN_FAIL_CNT + 1)) == 0)
        {
            APP_LOG("Reconnect", LOG_CRIT,"Force to up 'apcli0' again. scanFailCnt=%d", scanFailCnt);
            system("ifconfig apcli0 up");
        }

#ifdef DRACONIAN_WIFI_SCAN
             if(scanFailCnt == MAX_SCAN_FAIL_CNT)
             {
                    APP_LOG("Reconnect", LOG_ALERT,"No networks found in SiteScan for consecutive %d times..., rebooting!!!", scanFailCnt);
		    		CHECK_AND_INVOKE_NAT_REINIT(0);
                    system("reboot");
             }
#endif

	     for (i=0;i<count;i++)
	     {
		  if(gWiFiParams.channel != CHAN_HIDDEN_NW)
		  {
		      int len = strlen(gWiFiParams.SSID);
		      if(!isStrPrintAble(gWiFiParams.SSID, len))
		      {
			  int idx = 0;
			  char tmp[WIFI_MAXSTR_LEN];

			  memset(tmp, '\0', WIFI_MAXSTR_LEN);
			  len = strlen(gWiFiParams.SSID);
			  snprintf(tmp, sizeof(tmp), "0x");
			  for (idx = 0; idx < len; idx++)
			      snprintf(tmp + 2 + (idx*2), sizeof(tmp), "%02X", (unsigned char)gWiFiParams.SSID[idx]);

			  memset(tmp, '\0', WIFI_MAXSTR_LEN);
			  strncpy(tmp, gWiFiParams.SSID, sizeof(tmp)-1);
			  strncpy(gWiFiParams.SSID, tmp, sizeof(gWiFiParams.SSID)-1);
		      }
		     if (!strcmp (pAvlNetwork[i].ssid,gWiFiParams.SSID))
			 {

			     APP_LOG("Reconnect", LOG_DEBUG,
					     "pAvlNetwork[%d].channel: %s, gWiFiParams.channel: %d, \
					     pAvlNetwork[%d].ssid: %s, gWiFiParams.SSID: %s",
					     i, pAvlNetwork[i].channel, gWiFiParams.channel,
					     i, pAvlNetwork[i].ssid,gWiFiParams.SSID);

			     pSSID=gWiFiParams.SSID;
			     pAuth=gWiFiParams.AuthMode;
			     pEnc=gWiFiParams.EncrypType;
			     chan = atoi((const char *)pAvlNetwork[i].channel);
			     pPass=gWiFiParams.Key;
			     //Check for Network parameters before trying-TODO
			     //Case of
			     APP_LOG("Reconnect", LOG_DEBUG,
					     "Tryin to Reconnect to <%s> .\n",pAvlNetwork[i].ssid);

			     APP_LOG("Reconnect", LOG_CRIT,"%d-%s-%s-%s", chan, pSSID, pAuth, pEnc);

			     retVal = connectHomeNetwork(chan,pSSID,pAuth,pEnc,pPass) ;

			     /* In either case break from the loop */
			     break;
		     }
		  }
		  else
		  {
	  	     if (atoi(pAvlNetwork[i].Hidden))
			 {

			     pSSID=gWiFiParams.SSID;
			     pAuth=gWiFiParams.AuthMode;
			     pEnc=gWiFiParams.EncrypType;
			     chan = atoi((const char *)pAvlNetwork[i].channel);
			     pPass=gWiFiParams.Key;

			     memset(auth, 0, sizeof(auth));
			     memset(encrypt, 0, sizeof(encrypt));

			     sscanf(pAvlNetwork[i].security, "%[^'\\/']/%s",auth,encrypt);

			     APP_LOG("Reconnect", LOG_DEBUG,
					     "Trying connection to hidden channel: %d %s-%s",chan,auth,encrypt);

			     retVal = connectHomeNetwork(chan,pSSID,auth,encrypt,pPass) ;

			     /*
			      * if the connection was successful, break from the loop;
			      */
			     if(retVal == STATE_CONNECTED)
			     {
			       break;
			     }

		     }
		  }
	     }

	     if(i>=count)
		 {
		     APP_LOG("Reconnect", LOG_ERR,"Retry no. <%d> : Network SSID <%s> doesn't exist any more..Pls. switch on your Home Wireless Router...\n", retry_cnt, gWiFiParams.SSID);
	     }
	     else
#endif
	     {
		     if(retVal == STATE_CONNECTED)
		     {
			     /* connected to network successfully, exit the thread */
			     APP_LOG("Reconnect", LOG_CRIT,"Re-Connected to Home Network SSID <%s> !!!", gWiFiParams.SSID);
			#ifndef WeMo_INSTACONNECT
			     NotifyInternetConnected();
			#endif

			     /*
				We need to close AP only if this is invoked during bootup or
				App called closeAP some time
			      */
			     if(gBootReconnect || gAppCalledCloseAp)
			     {
        			     APP_LOG("WiFiApp",LOG_DEBUG, "gBootReconnect=%d,gAppCalledCloseAp=%d\n",
								gBootReconnect,gAppCalledCloseAp);
				     /* switch off AP mode */
				     setAPIfState("OFF");
			     }

			     break;
		     }
	     }
	     retry_cnt++;
	     pluginUsleep(5000000);
     }

     /* common exit point, free memory and exit */
     free (pAvlNetwork);

     if(isSetupRequested())
     {
     	APP_LOG("Reconnect", LOG_ERR,"Not trying to connect to Home Network SSID <%s> !!! \n", gWiFiParams.SSID);
	gReconnectFlag=0;
     }

    return;
}

char *wifiGetIP (char *pInterface)
{
     if(!strcmp (pInterface, INTERFACE_CLIENT))
	 {
         return GetWanIPAddress();
     }
	 else if (!strcmp (pInterface, INTERFACE_AP))
	 {
         return GetLanIPAddress ();
     }
     //Invalid Interface
     return NULL;
}

int resetNetworkParams()
{
    setClientIfState("OFF");
    SetBelkinParameter (WIFI_CLIENT_SSID,"");
    //Doing SSID should be sufficient.
    gWiFiConfigured=0;
    SaveSetting();
    return 0;
}

int chksignalstrength (void)
{
	static int ledppoorconn;

	gSignalStrength = wifiGetRSSI(INTERFACE_CLIENT);
	if(gSignalStrength < SIGNALTHRESHOLD)
	{
		APP_LOG("WiFiApp", LOG_ALERT,"Poor Connection strength: %d", gSignalStrength);
		ledppoorconn = 0x01;
	}
	else if((gSignalStrength >= SIGNALTHRESHOLD) && ledppoorconn){
		APP_LOG("WiFiApp", LOG_CRIT,"State 4: Connection strength regained: %d\n", gSignalStrength);
		ledppoorconn = 0x0;
		SetWiFiLED(0x04);
	}
	APP_LOG("WiFiApp", LOG_DEBUG,"Connection signal check: strength is:%d,ledppoorconn:%d",gSignalStrength,ledppoorconn);
	return ledppoorconn;
}
void ledStatusCtrl()
{
    static int sleepcnt;
    static int noconnchk;
    static int poorconnchk;
    static int signalwait;
    PWIFI_STAT_COUNTERS pWiFiStats;
    PMY_SITE_SURVEY pAvlAPList;
    int i=0, count=0, found;
    //char *pSSID = NULL;
    char pSSID[WIFI_MAXSTR_LEN+1];

    pAvlAPList = (PMY_SITE_SURVEY) ZALLOC(sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE);
    if(!pAvlAPList) {
   APP_LOG("WiFiApp:ledStatusCtrl:pAvlAPList", LOG_ERR,"Malloc Failed....");
        return;
    }
    //pSSID = GetBelkinParameter (WIFI_CLIENT_SSID);
    memset(pSSID, 0, sizeof(pSSID));
    strncpy(pSSID, gWiFiParams.SSID, sizeof(pSSID)-1);

    pWiFiStats = (PWIFI_STAT_COUNTERS) MALLOC(sizeof(WIFI_STAT_COUNTERS));
    if(pWiFiStats==NULL) {
       APP_LOG("WiFiApp", LOG_ERR,"Malloc Failed....");
       if(pAvlAPList!= NULL){
          free (pAvlAPList);
          pAvlAPList = NULL;
       }
       return;
    }

    while (1)
	{
	found=0;
        if (getCurrentClientState() == STATE_INTERNET_NOT_CONNECTED) {
		APP_LOG("WiFiApp", LOG_DEBUG,"State 3: No connection");
		noconnchk=1;
		chkConnStatus();
            }else {
		if(noconnchk){
			APP_LOG("WiFiApp", LOG_DEBUG,"State 4: Again Connected");
			SetWiFiLED(0x04);
			noconnchk=0;
		}

		if(signalwait < SIGNALTIMEVAL){
			++signalwait;
		}
		if(signalwait == SIGNALTIMEVAL){
			// check signal strength now
		  while(1){
			getCompleteAPList(pAvlAPList, &count);
			for (i=0;i<count;i++){
				if((strcmp(pAvlAPList[i].ssid,pSSID))== 0){
					APP_LOG("WiFiApp", LOG_DEBUG,"Connection signal check: strength is: %d",(atoi(pAvlAPList[i].signal)));
					if((atoi(pAvlAPList[i].signal)) < SIGNALTHRESHOLD){
						APP_LOG("WiFiApp", LOG_CRIT,"State 2: Poor connection %d", (atoi(pAvlAPList[i].signal)));
						if(!poorconnchk){
#ifndef PRODUCT_WeMo_Light
							SetWiFiLED(0x02);
#endif
							poorconnchk=1;
						}
					}
					else if(((atoi(pAvlAPList[i].signal)) >= SIGNALTHRESHOLD) && poorconnchk){
						APP_LOG("WiFiApp", LOG_DEBUG,"State 4: Connection strength regained %d", (atoi(pAvlAPList[i].signal)));
			                        SetWiFiLED(0x04);
                        			poorconnchk=0;
					}
					found=1;
					break;
				}
			}//end of for
			signalwait=0;
			if(found){
				break;
			}
		  }//end of while
		}
            }
            pluginUsleep(1000000);
	    if(sleepcnt<30){
            	++sleepcnt;
	    }
	    if((sleepcnt==30) && (!noconnchk) && (!poorconnchk)){
		APP_LOG("WiFiApp", LOG_DEBUG,"State 4: after 30 seconds OFF");
		SetWiFiLED(0x04);
            	++sleepcnt;
	    }
    }
    if(pAvlAPList!= NULL){
    	free (pAvlAPList);
	pAvlAPList = NULL;
    }
    if(pWiFiStats!= NULL){
    	free (pWiFiStats);
	pWiFiStats = NULL;
    }
}

void* ledStatusInitThread(void *args){
    while (1){
    	if((getCurrentClientState() == STATE_CONNECTED)){
		break;
	}
	pluginUsleep(2000000);
    }
    ledStatusCtrl ();
    return NULL;
}

void getRouterEssidMac (const char *p_essid, const char *p_macaddr, const char *pInterface)
{
	wifiGetStatus ((char *)p_essid, (char *)p_macaddr, (char *)pInterface);
}

int  findEncryptionForSsid(char *ssid,char* EncryptType,char *AuthMode)
{
	PMY_SITE_SURVEY pAvlNetwork=NULL;
	int found=0;
	int i=0;
	int count=0;
	int retries=3;
	char auth[WIFI_MAXSTR_LEN];
	char encrypt[WIFI_MAXSTR_LEN];
	int ret=FAILURE;

      pAvlNetwork = (PMY_SITE_SURVEY) ZALLOC(sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE);
   if(!pAvlNetwork)
   {
      APP_LOG("WiFiApp", LOG_ERR,"Malloc Failed....");
      return FAILURE;
   }

	EnableSiteSurvey();
	pluginUsleep(50000);
	while(retries > 0)
	{
		memset(pAvlNetwork, 0, (sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE));
		getCurrentAPList (pAvlNetwork,&count);
		APP_LOG("WiFiApp", LOG_DEBUG,"Avl network list cnt: %d", count);
		for (i=0;i<count;i++)
		{

			if (!strcmp (pAvlNetwork[i].ssid,ssid))
			{
				memset(auth, 0, sizeof(auth));
				memset(encrypt, 0, sizeof(encrypt));

				sscanf(pAvlNetwork[i].security, "%[^'\\/']/%s",auth,encrypt);


				APP_LOG("WiFiApp", LOG_DEBUG,
						"Encryption Type  is %s  auth Mode is %s ",encrypt,auth);
				found = 1;
				break;
			}
		}
		if(found)
		{
			APP_LOG("WiFiApp", LOG_DEBUG,
				"Encryption Type determined from SITE SURVEY: <%s> on retry#%d",encrypt,4-retries);
			break;
		}
		else
		{
			retries --;
			pluginUsleep(500000);
		}
	}
	if(pAvlNetwork)
		free(pAvlNetwork);

	if(!found)
	{
		APP_LOG("WiFiApp", LOG_ERR,
				"SSID: %s not found in the available network list",ssid);
		return FAILURE;
	}
	else
	{
		/*check if AuthMode changes from secure to OPEN, if so do not update security info*/
		if((0 != strcmp(AuthMode,"OPEN")) && (0 == strcmp(auth,"OPEN")))
		{
			APP_LOG("WiFiApp", LOG_DEBUG,"Not updating router security info!Router auth mode changed from %s to %s", AuthMode,auth);
			return FAILURE;
		}

		if(memcmp(EncryptType,encrypt,strlen(encrypt)))
		{
			memset(EncryptType,0,WIFI_MAXSTR_LEN);
			memcpy(EncryptType,encrypt,strlen(encrypt));
			APP_LOG("WiFiApp", LOG_DEBUG,"Encryption Type copied ass %s ",EncryptType);
			ret=SUCCESS;
		}
		if(memcmp(AuthMode,auth,strlen(auth)))
		{
			memset(AuthMode,0,WIFI_MAXSTR_LEN);
			memcpy(AuthMode,auth,strlen(auth));
			APP_LOG("WiFiApp", LOG_DEBUG,"authMode  copied ass %s ",AuthMode);
			ret=SUCCESS;
		}
		return ret;
	}


}
void SetAppSSIDCommand(void)
{
	APP_LOG("SetAppSSIDCommand", LOG_DEBUG, "**********setting wifi App ssid commands:");
	int ret = -1;
        char command[SIZE_64B];
        memset(command, '\0', SIZE_64B);
        strncpy(command, "RadioOn=1", sizeof(command)-1);
        ret = wifiSetCommand (command,"apcli0");
        if(ret < 0)
        {
                APP_LOG("SetAppSSIDCommand", LOG_ERR, "%s - failed", command);
        }
	APP_LOG("SetAppSSIDCommand", LOG_DEBUG, "**********Command Set: %s",command);

        memset(command, '\0', SIZE_64B);
        strncpy(command, "ifconfig apcli0 up", sizeof(command)-1);
	system(command);
	APP_LOG("SetAppSSIDCommand", LOG_DEBUG, "**********Command Set: %s",command);

        ret = -1;
        memset(command, '\0', SIZE_64B);
        strncpy(command, "SiteSurvey=1", sizeof(command)-1);
        ret = wifiSetCommand (command,"apcli0");
        if(ret < 0)
        {
                APP_LOG("SetAppSSIDCommand", LOG_ERR, "%s - failed", command);
        }
	APP_LOG("SetAppSSIDCommand", LOG_DEBUG, "**********Command Set: %s",command);

        ret = -1;
        memset(command, '\0', SIZE_64B);
        strncpy(command, "RadioOn=1", sizeof(command)-1);
        ret = wifiSetCommand (command,"ra0");
        if(ret < 0)
        {
                APP_LOG("SetAppSSIDCommand", LOG_ERR, "%s - failed", command);
        }
	APP_LOG("SetAppSSIDCommand", LOG_DEBUG, "**********Command Set: %s",command);

        memset(command, '\0', SIZE_64B);
        strncpy(command, "ifconfig ra0 up", sizeof(command)-1);
	system(command);
	APP_LOG("SetAppSSIDCommand", LOG_DEBUG, "**********Command Set: %s",command);

        ret = -1;
        memset(command, '\0', SIZE_64B);
        strncpy(command, "SiteSurvey=1", sizeof(command)-1);
        ret = wifiSetCommand (command,"ra0");
        if(ret < 0)
        {
                APP_LOG("SetAppSSIDCommand", LOG_ERR, "%s - failed", command);
        }
	APP_LOG("SetAppSSIDCommand", LOG_DEBUG, "**********Command Set: %s",command);

#ifdef _OPENWRT_
        ret = -1;
        memset(command, '\0', SIZE_64B);
        strncpy(command, "HideSSID=0", sizeof(command)-1);
        ret = wifiSetCommand (command,"ra0");
        if(ret < 0)
        {
                APP_LOG("SetAppSSIDCommand", LOG_ERR, "%s - failed", command);
        }
	APP_LOG("SetAppSSIDCommand", LOG_DEBUG, "**********Command Set: %s",command);
#endif
}

#ifdef __WIRED_ETH__
// sh:
void StartInetMonitorThread()
{
	int retVal;

	if(!Inet_Monitor_thread) {
		/* create the Inet connectivity monitor thread */
		pthread_attr_init(&inet_mon_attr);
		pthread_attr_setdetachstate(&inet_mon_attr,PTHREAD_CREATE_DETACHED);
		retVal = pthread_create(&Inet_Monitor_thread, &inet_mon_attr, InetMonitorTask, NULL);
		if(retVal != 0) {
			APP_LOG("WiFiApp",LOG_ERR,
					  "InetMon Thread not created, errno: %d", errno);
		}
		else {
			APP_LOG("WiFiApp",LOG_DEBUG,"InetMon Thread started");
		}
	}
}

#endif
