/***************************************************************************
*
*
* coreInit.c
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
#include <ithread.h>
#include "types.h"
#include "upnp.h"
#include "controlledevice.h"
#include "plugin_ctrlpoint.h"
#include "wifiHndlr.h"
#include "fwDl.h"
#include "utils.h"
#include "utlist.h"
#include "osUtils.h"
#include "httpsWrapper.h"
#include "logger.h"
#include "itc.h"
#include "gpio.h"
#include "remoteAccess.h"
#include "getRemoteMac.h"
#include "WemoDB.h"
#include "ipcUDS.h"
#include "pmortem.h"
#include "UDSClientHandler.h"

#ifdef PRODUCT_WeMo_LEDLight
#include "remote_event.h"
#include "ledLightRemoteAccess.h"
#include "subdevice.h"
#endif

#include "rule.h"
#include "watchDog.h"
#ifdef SIMULATED_OCCUPANCY
#include "simulatedOccupancy.h"
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include "thready_utils.h"
#include <sys/syscall.h>
#include "thready_utils.h"
#include <sys/syscall.h>
#include "natClient.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

#define WEMO_VERSION_GEMTEK_PROD   "WeMo_version"
#define BELKIN_DAEMON_SUCCESS "Belkin_daemon_success"
#define LAN_IPADDR "lan_ipaddr"
#define MY_FW_VERSION "my_fw_version"

#define SIGNED_PUBKEY_FILE_NAME  "WeMoPubKey.asc"
#define IMPORT_KEY_PARAM_NAME "import_pkey_name"
extern char gIcePrevTransactionId[PLUGIN_MAX_REQUESTS][TRANSACTION_ID_LEN];

void disconnectFromRouter();


#ifndef _OPENWRT_
extern pthread_mutex_t lookupLock;
#else
extern char g_szBootArgs[SIZE_128B];
extern int g_PowerStatus;
extern void libNvramInit();
pthread_attr_t sysRestore_attr;

#define RALINK_GENERAL_READ             0x50

#endif

extern struct Command *front, *rear;
extern unsigned int g_queue_counter;
extern pthread_mutex_t g_queue_mutex;
extern pthread_attr_t wdLog_attr;
ithread_t sendremoteupdstatus_thread = -1;

#ifdef __WIRED_ETH__
int g_bWiredEthernet;
extern void StartInetMonitorThread();
#endif

char g_szHomeId[SIZE_20B];
char g_szPluginPrivatekey[MAX_PKEY_LEN];
char g_routerMac[MAX_MAC_LEN];
char g_routerSsid[MAX_ESSID_LEN];
char g_szSmartDeviceId[SIZE_256B];
char g_szSmartPrivateKey[MAX_PKEY_LEN];
char g_szPluginCloudId[SIZE_16B];
int gDstSupported;
int ghwVersion=1;
char g_NotificationStatus[MAX_RES_LEN];
extern SERVERENV g_ServerEnvType;
char g_serverEnvIPaddr[SIZE_32B];
char g_turnServerEnvIPaddr[SIZE_32B];

char g_szRestoreState[MAX_RES_LEN];

pthread_t logFile_thread = -1;
pthread_t dbFile_thread = -1;

extern void* UDS_MonitorAndStartClientDataPath(void *arg);

/*
 * Here's where we used to just have the thread exit.  Which sometimes
 * just leaves the application in a semi-functional state.  Better to
 * just shut down and let wemoApp get restarted.  That way, any leaked
 * resources get freed and we have a chance to recover.
 */
static void handleSigSegv(int signum,siginfo_t *pInfo,void *pVoid)
{
    uint32_t *up = (uint32_t *)&signum;
    unsigned int sp = 0;
#if defined (__mips__)
	 static int Recursed = 0;

	 if(Recursed++ == 0) {
		 APP_LOG("WiFiApp",LOG_ALERT,"[%d:%s] SIGNAL SIGSEGV [%d] RECEIVED, bad address: %08x, fault address: %08x, ra: %08x",
		(int)syscall(SYS_gettid), tu_get_my_thread_name(), signum, pInfo->si_addr, up[46], up[110]);

		 pmortem_connect_and_send((uint32_t *) &signum, 4 * 1024);
	 }
	 else {
	 // Just wait for the grim reaper
		 for( ; ; ) {
			 sleep(100000);
		 }
	 }
#else
    APP_LOG("WiFiApp",LOG_ALERT,"[%d:%s] SIGNAL SIGSEGV [%d] RECEIVED, aborting...",
   (int)syscall(SYS_gettid), tu_get_my_thread_name(), signum);
#endif
    //pthread_exit(NULL);

    /* exit application on encountering SIGSEGV as per Tina's mail */
    abort();
}
static void handleSigAbrt(int signum)
{
    APP_LOG("WiFiApp",LOG_ERR,"[%d:%s] SIGNAL SIGABRT [%d] RECEIVED, Aborting.", (int)syscall(SYS_gettid), tu_get_my_thread_name(), signum);
    // It appears the authors' intention was to have the application
    // exit at this point.  That is the documented behavior of
    // exit().  Unfortunately, real-world testing shows taht only the
    // thread exits.  This is a surprise.  Fortunately, merely
    // returning from the abort signal handler also causes the process
    // to exit and in this case it actually does.
    //exit(0);
}
/* signal handler for wemoApp */
static void handleRtMinSignal(int signum)
{
   if(signum == SIGRTMIN)
   {
      APP_LOG("WiFiApp",LOG_ALERT,"SIGNAL [%d] RECEIVED ..",signum);
      pthread_exit(NULL);
   }
   else
   {
      APP_LOG("WiFiApp",LOG_ALERT,"UNEXPECTED SIGNAL RECEIVED [%d] ..",signum);
   }
}
static void handleSigPipe(int signum)
{
    APP_LOG("WiFiApp",LOG_ALERT,"SIGNAL SIGPIPE [%d] RECEIVED ..",signum);
    return;
}
void setSignalHandlers(void)
{
   struct sigaction act, oldact;
   act.sa_flags = (SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART);
   act.sa_handler = handleRtMinSignal;

   if(sigaction(SIGRTMIN, &act, &oldact))
   {
   APP_LOG("WiFiApp",LOG_ERR,
      "sigaction failed... errno: %d", errno);
   }
   else
   {
   if(oldact.sa_handler == SIG_IGN)
      APP_LOG("WiFiApp",LOG_DEBUG,"oldact RTMIN: SIGIGN");

        if(oldact.sa_handler == SIG_DFL)
      APP_LOG("WiFiApp",LOG_DEBUG,"oldact RTMIN: SIGDFL");
   }

   act.sa_flags = (SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART | SA_SIGINFO);
   act.sa_sigaction = handleSigSegv;
   if(sigaction(SIGSEGV, &act, &oldact))
   {
   APP_LOG("WiFiApp",LOG_ERR,
      "sigaction failed... errno: %d", errno);
   }
   else
   {
   if(oldact.sa_handler == SIG_IGN)
      APP_LOG("WiFiApp",LOG_DEBUG,"oldact RTMIN: SIGIGN");

        if(oldact.sa_handler == SIG_DFL)
      APP_LOG("WiFiApp",LOG_DEBUG,"oldact RTMIN: SIGDFL");
   }
   act.sa_flags = (SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART);
   act.sa_handler = handleSigAbrt;
   if(sigaction(SIGABRT, &act, &oldact))
   {
   APP_LOG("WiFiApp",LOG_ERR,
      "sigaction failed... errno: %d", errno);
   }
   else
   {
   if(oldact.sa_handler == SIG_IGN)
      APP_LOG("WiFiApp",LOG_DEBUG,"oldact RTMIN: SIGIGN");

        if(oldact.sa_handler == SIG_DFL)
      APP_LOG("WiFiApp",LOG_DEBUG,"oldact RTMIN: SIGDFL");
   }
      act.sa_flags = (SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART);
   act.sa_handler = handleSigPipe;
   if(sigaction(SIGPIPE, &act, &oldact))
   {
   APP_LOG("WiFiApp",LOG_ERR,
      "sigaction failed... errno: %d", errno);
   }
   else
   {
   if(oldact.sa_handler == SIG_IGN)
      APP_LOG("WiFiApp",LOG_DEBUG,"oldact RTMIN: SIGIGN");

        if(oldact.sa_handler == SIG_DFL)
      APP_LOG("WiFiApp",LOG_DEBUG,"oldact RTMIN: SIGDFL");
   }
}

#define MAX_CHANNELS 11
int selectQuietestApChannel()
{
   PMY_SITE_SURVEY pAvlAPList;
   int count=0,i,min,chan;
   int ApCnt[MAX_CHANNELS+1];


   memset(ApCnt, 0, sizeof(ApCnt));

	pAvlAPList = (PMY_SITE_SURVEY) ZALLOC(sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE);
	if(!pAvlAPList) {
		APP_LOG("UPNP", LOG_DEBUG,"Malloc Failed...");
		return -1;
	}

   APP_LOG("UPNP", LOG_DEBUG,"Get List...");
   getCurrentAPList(pAvlAPList, &count);
   APP_LOG("UPNP", LOG_DEBUG,"List Size <%d>...",count);

   for (i=0;i<count;i++) {
      ApCnt[atoi((char *)pAvlAPList[i].channel)]++;
   }

   min = ApCnt[1];
   chan = 1;

   /* find out the quietest channel */
   for(i=1;i<=MAX_CHANNELS;i++)
   {
      if(min > ApCnt[i])
      {
         min = ApCnt[i];
         chan = i;
      }
   }

   APP_LOG("UPNP", LOG_DEBUG,"Selected channel: <%d>, ApCnt: %d g_channelAp: <%d>...",chan, ApCnt[chan], g_channelAp);

   if(g_channelAp != chan) {
      char pCommand[SIZE_64B],chBuf[SIZE_16B];
      int ret;

      memset(pCommand, 0, SIZE_64B);
      memset(chBuf, 0, SIZE_16B);
      strncpy(pCommand, "Channel=",sizeof(pCommand)-1);
      snprintf(chBuf,sizeof(chBuf),"%d",chan);
      strncat(pCommand,chBuf,sizeof(pCommand) - strlen(pCommand) - 1);
      ret = wifiSetCommand (pCommand,INTERFACE_AP);
      if(ret < 0)
      {
         APP_LOG("NetworkControl", LOG_ERR, "%s - failed", pCommand);
         free (pAvlAPList);
         return FAILURE;
      }
   }

   g_channelAp = chan;

   free (pAvlAPList);
   return 0;
}


#ifdef _OPENWRT_
#define FILE_BOOT_ARGS "/tmp/bootArgs"
/*
 * Function: parseBootArgs
 *  This parses the bootArgs which are written to the file by wemo.init
 *  Fetches the current state of the switch
 *
 * Done for the Story: 2187, To restore the state of the switch before power failure
 *
 */
static int parseBootArgs()
{
    FILE *fp;
    char line[SIZE_128B] = {'\0'};
    char *p = NULL, *name, *value;
    int i=0;
    char ch;
	 char *strtok_r_temp;

    memset(g_szBootArgs, '\0', SIZE_128B);

    fp = fopen(FILE_BOOT_ARGS, "rb");
    if (!fp)
    {
   perror("File opening error");
   return 0;
    }

    while (fgets(line, SIZE_128B, fp))
   if ((p = strstr(line, "boot_A_args")))
       break;

    fclose(fp);

    name = strtok_r(line,"=",&strtok_r_temp);
    if(name)
    {
   value = line + strlen(name) + 1;

   if(value[strlen(value)-1] == '\n')
       value[strlen(value)-1] = '\0';

   if((p = strstr(value, "switchStatus")))
   {
       p = p + strlen("switchStatus") + 1;
       ch = p[0];
       g_PowerStatus = ch - '0';

       for(i=strlen(value)-1; i>0; i--)
       {
      if(value[i] == ' ')
      {
          value[i] = '\0';
          break;
      }
       }
   }

   if(strstr(value, "root"))
       strncpy(g_szBootArgs, value, sizeof(g_szBootArgs)-1);
    }
    return 0;
}
#endif

extern void initdevConfiglock();

void restoreRelayState()
{
   FILE * pRelayFile = 0x00;
#if defined(PRODUCT_WeMo_Insight)
   char* szRelayPath = "/proc/RELAY_LED";
#else
   char* szRelayPath = "/proc/GPIO9";
#endif
   char szflag[4];
   int command = 0;
   char* pResult = NULL;

   pRelayFile = fopen(szRelayPath, "r");
   if (pRelayFile == 0x00)
   {
      APP_LOG("InitializePowerLEDState:", LOG_DEBUG, "Error on Open file for read: %s ", szRelayPath);
      return;
   }

   memset(szflag, 0x00, sizeof(szflag));
   pResult = fgets(szflag, sizeof(szflag), pRelayFile);
   if (pResult)
   {
      fclose(pRelayFile);
      command = atoi(szflag);
      LockLED();
      setPower(!command);
      SetCurBinaryState(!command);
                UnlockLED();
      APP_LOG("InitializePowerLEDState", LOG_DEBUG, "BOOT TIME BINARY STATE: %d , power set : %d", command, !command);
   }
   else
   {
      APP_LOG("InitializePowerLEDState", LOG_DEBUG, "READ GPIO RETURNED NULL");
      fclose(pRelayFile);
   }
}

int startCtrlPoint()
{
   if(DEVICE_SENSOR == g_eDeviceType)
   {
      APP_LOG("CtrlPt", LOG_DEBUG, "Sensor Start Plugin Control Point");
      return TRUE;
   }
   else if (DEVICE_SOCKET == g_eDeviceType)
   {
      if ((0x00 == atoi(g_szRestoreState)) && (0x00 == strlen(g_szHomeId) ) && (0x00 == strlen(g_szPluginPrivatekey)))
      {
         APP_LOG("CtrlPt", LOG_DEBUG, "Socket Start Plugin Control Point");
         return TRUE;
      }
   }
   else if ( (DEVICE_CROCKPOT == g_eDeviceType) || (DEVICE_SBIRON == g_eDeviceType) ||
              (DEVICE_MRCOFFEE == g_eDeviceType) || (DEVICE_PETFEEDER == g_eDeviceType) ||
           (DEVICE_SMART == g_eDeviceType)
         )
    {
      if ((0x00 == atoi(g_szRestoreState)) && (0x00 == strlen(g_szHomeId) ) && (0x00 == strlen(g_szPluginPrivatekey)))
      {
         APP_LOG("CtrlPt", LOG_DEBUG, "Socket Start Plugin Control Point");
         return TRUE;
      }
   }
   return FAILURE;
}

int startRemoteAccessReRegister()
{
   char routerMac[MAX_MAC_LEN];
   char routerssid[SIZE_32B];

   memset(routerMac, 0, MAX_MAC_LEN);
   memset(routerssid, 0, SIZE_32B);

   getRouterEssidMac (routerssid, routerMac, INTERFACE_CLIENT);
   if(strlen(gGatewayMAC) > 0x0)
   {
      memset(routerMac, 0, sizeof(routerMac));
      strncpy(routerMac, gGatewayMAC, sizeof(routerMac)-1);
   }


   if(0x01 == atoi(g_szRestoreState))
   {
      return TRUE;
   }

   else if(0x00 == atoi(g_szRestoreState))
   {
      if((strlen(g_szHomeId) == 0x0) && (strlen(g_szPluginPrivatekey) == 0x0))
      {
         APP_LOG("ReReg", LOG_DEBUG, "main: Remote Access is not Enabled...");
         return FALSE;
      }
      else
      {
         if( (strcmp(g_routerMac, routerMac) != 0) && (g_routerSsid!=NULL && strlen (g_routerSsid) > 0) )
         {
            APP_LOG("ReReg", LOG_DEBUG, "Remote Access is not Enabled...");
            return TRUE;
         }
      }
   }

   return FALSE;
}

void HandleHardRestoreAndCtrlPoint(int forceStart, char* ip_address)
{
   if(forceStart || startCtrlPoint())
   {
      APP_LOG("CtrlPt", LOG_DEBUG, "Starting Plugin Ctrl point, forceStart: %d", forceStart);
      int ret=StartPluginCtrlPoint(ip_address, 0x00);
      if(UPNP_E_INIT_FAILED==ret)
      {
          APP_LOG("UPNP", LOG_DEBUG,"UPNP on error: %d", ret);
	  APP_LOG("UPNP", LOG_DEBUG,"################### Wemo App going to be reset ###################");
	  resetSystem();
      }
      EnableContrlPointRediscover(TRUE);
   }

   if(startRemoteAccessReRegister())
   {
      APP_LOG("ReReg", LOG_DEBUG, "Starting Remote Re-register");
      createAutoRegThread();
   }
}


void startUPnP(int forceEnableCtrlPoint)
{
   char* ip_address = NULL;
   unsigned int port = 0;
   int ret=-1;
   //-How to make sure it connects to home network?
   if(getCurrentClientState())
   {
      ip_address = wifiGetIP(INTERFACE_CLIENT);
      APP_LOG("UPNP", LOG_DEBUG, "################### UPNP on router: %s ###################", ip_address);
      ret=ControlleeDeviceStart(ip_address, port, desc_doc_name, web_dir_path);
      if(( ret != UPNP_E_SUCCESS ) && ( ret != UPNP_E_INIT ) )
      {
          APP_LOG("UPNP", LOG_DEBUG,"UPNP on error: %d", ret);
          APP_LOG("UPNP", LOG_DEBUG,"################### Wemo App going to be reset ###################");
          resetSystem();
      }
      UpdateUPnPNetworkMode(UPNP_INTERNET_MODE);
      HandleHardRestoreAndCtrlPoint(forceEnableCtrlPoint, ip_address);
   }
   else
   {
      ip_address = wifiGetIP(INTERFACE_AP);
      APP_LOG("UPNP", LOG_DEBUG, "################### UPNP on local:%s ###################", ip_address);
      ret=ControlleeDeviceStart(ip_address, port, desc_doc_name, web_dir_path);
      if(( ret != UPNP_E_SUCCESS ) && ( ret != UPNP_E_INIT ) )
      {
          APP_LOG("UPNP", LOG_DEBUG,"UPNP on error: %d", ret);
          APP_LOG("UPNP", LOG_DEBUG,"################### Wemo App going to be reset ###################");
          resetSystem();
      }
      UpdateUPnPNetworkMode(UPNP_LOCAL_MODE);
      EnableSiteSurvey();
   }
}

#ifdef PRODUCT_WeMo_LEDLight
int WemoLinkRestore(int restoreValue)
{
    int nRet;
    int nStatusCode = 0, retry_cnt = 0;
    int i = 0, nCount = 0;

    char *pErrorCode = NULL;
    char *pClientSSID = NULL;

    if( restoreValue == 1 )
    {
        pClientSSID = GetBelkinParameter(WIFI_CLIENT_SSID);

        if( pClientSSID && pClientSSID[0] )
        {
            while( FAILURE == (nRet = checkInetStatus(4)) )
            {
                usleep(500000);
                retry_cnt++;
                if( retry_cnt >= 10 )
                {
                    break;
                }
            }

            if( nRet == SUCCESS )
            {
                APP_LOG("SystemRestore", LOG_DEBUG, "Internet connection is ready...");

                for( retry_cnt = 0; retry_cnt < 3; retry_cnt++ )
                {
                  if( 0 == RunNTP() )
                  {
                     APP_LOG("SystemRestore", LOG_DEBUG, "Success RunNTP()...")
                     break;
                  }
                  else
                  {
                     APP_LOG("SystemRestore", LOG_DEBUG, "Failure RunNTP() and try again...")
                  }
                }

                //Delete group if group exists
                {
                  SD_GroupList group_list;
                  nCount = SD_GetGroupList(&group_list);
                  for( i = 0; i < nCount; i++ )
                  {
                      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "DEL_GROUPDEVICE(%s) is triggered...", group_list[i]->ID.Data);
                      trigger_remote_event(WORK_EVENT, DELETE_GROUPDEVICE, "group", "delete",
                                            &(group_list[i]->ID), sizeof(SD_GroupID));
                  }
                }

                //Delete all subdevices if subdevices exists
                {
                  SD_DeviceList device_list;

                  nCount = SD_GetDeviceList(&device_list);
                  if( nCount )
                  {
                    trigger_remote_event(WORK_EVENT, DEL_SUBDEVICES, "subdevice", "deleteall", NULL, 0);
                    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "DEL_SUBDEVICES is triggered...");
                  }
                }

                #if 0
                char *pHomeId = GetBelkinParameter(DEFAULT_HOME_ID);
                char *pPluginId = GetBelkinParameter(DEFAULT_PLUGIN_CLOUD_ID);

                if( pHomeId && pHomeId[0] )
                {
                  SD_GroupList group_list;

                  nCount = SD_GetGroupList(&group_list);

                  for( i = 0; i < nCount; i++ )
                  {
                     pErrorCode = NULL;
                     nStatusCode = DeleteGroups(pHomeId, (char*)group_list[i]->ID.Data, &pErrorCode);
                     APP_LOG("SystemRestore", LOG_DEBUG, "DeleteGroups(pHomeId = %s, groupId = %s, nStatusCode = %d)",
                              pHomeId, group_list[i]->ID.Data, nStatusCode);
                     if( (200 != nStatusCode) && (NULL != pErrorCode) )
                     {
                        APP_LOG("SystemRestore", LOG_DEBUG, "Failure: DeleteGroups(ErrorCode = %s)", pErrorCode);
                     }
                  }
                }

                if( pPluginId && pPluginId[0] )
                {
                  SD_DeviceList device_list;

                  nCount = SD_GetDeviceList(&device_list);

                  if( nCount )
                  {
                     pErrorCode = NULL;
                     nStatusCode = DeleteSubDevices(pPluginId, &pErrorCode);
                     APP_LOG("SystemRestore", LOG_DEBUG, "DeleteSubDevices(pPluginId = %s, nStatusCode = %d)",
                              pPluginId, nStatusCode);
                     if( (200 != nStatusCode) && (NULL != pErrorCode) )
                     {
                        APP_LOG("SystemRestore", LOG_DEBUG, "Failure: DeleteSubDevices(ErrorCode = %s)", pErrorCode);
                     }
                  }
                }
                else
                {
                  //If it is failed to delete the all sub devices, please set the flag to the nvram variable and then
                  //when system rebooting is finished, it sends an event to the ledRemoteAccessThread routin to delete all sub devices...
                }
                #endif
            }
            else
            {
               APP_LOG("SystemRestore", LOG_DEBUG, "Internet connection is NOT ready...");
               //TODO : Jeff Yoon
               //If it is failed to delete the all sub devices, please set the flag to the nvram variable and then
               //when system rebooting is finished, it sends an event to the ledRemoteAccessThread routin to delete all sub devices...
            }
        }

        pluginUsleep(1000000);

        SD_ClearAllStoredList();

        system("zbapitest clear > /dev/console");
        system("rm -f /tmp/Belkin_settings/sd_network_forming");

        system("rm -f /tmp/Belkin_settings/flag_setup_done");

        system("rm -f /tmp/Belkin_settings/rule*");
        system("rm -f /tmp/Belkin_settings/icon*");
        system("rm -f /tmp/Belkin_settings/zb*");

        system("rm -rf /tmp/Belkin_settings/sd_*");
        system("rm -rf /tmp/Belkin_settings/data_stores");

        SD_SetZBMode(WEMO_LINK_PRISTINE_ZC);
	
	/*deInit Device preset*/
	deInitDevicePreset(true);

        correctUbootParams();
        resetNetworkParams();
        clearMemPartitions();
        ClearRuleFromFlash();
        resetToDefaults();
        usleep(500000);
        system("nvram restore");

        pluginUsleep(2000000);

        char *homeId = GetBelkinParameter(DEFAULT_HOME_ID);
        char *pluginKey = GetBelkinParameter(DEFAULT_PLUGIN_PRIVATE_KEY);
        if ((homeId && pluginKey) && (0x00 == strlen(homeId) ) && (0x00 == strlen(pluginKey)))
          setRemoteRestoreParam(0x0);
        else
          setRemoteRestoreParam(0x1);
        APP_LOG("UPNP", LOG_DEBUG, "System rebooting........");
        system("reboot -f");

        return 1;
    }

    return 0;
}
#endif

#ifdef _OPENWRT_
void *systemRestore(void *args)
{
    int restoreValue = 0;
    int err = 0, fd = 0;

    tu_set_my_thread_name( __FUNCTION__ );

    if( (fd = open("/dev/gpio", O_RDWR)) < 0 )
    {
        APP_LOG("WiFiApp", LOG_DEBUG, "Open /dev/gpio failed");
        return (void *)-1;
    }

    while( 1 )
    {
        err = ioctl(fd, RALINK_GENERAL_READ, (void *)&restoreValue);
        if( err < 0 )
        {
            APP_LOG("WiFiApp", LOG_DEBUG, "Ralink Read byte failed ");
            close(fd);
            return (void *)err;
        }

        usleep(500000);

#ifdef PRODUCT_WeMo_LEDLight
        if( WemoLinkRestore(restoreValue) )
        {
          break;
        }
#else
        if( restoreValue == 1 )
        {
            correctUbootParams();
            resetNetworkParams();
            clearMemPartitions();
            ClearRuleFromFlash();
            resetToDefaults();
            usleep(500000);
            system("nvram restore");

            pluginUsleep(2000000);

            char *homeId = GetBelkinParameter(DEFAULT_HOME_ID);
            char *pluginKey = GetBelkinParameter(DEFAULT_PLUGIN_PRIVATE_KEY);
            if ((homeId && pluginKey) && (0x00 == strlen(homeId) ) && (0x00 == strlen(pluginKey)))
              setRemoteRestoreParam(0x0);
            else
              setRemoteRestoreParam(0x1);
            APP_LOG("UPNP", LOG_DEBUG, "System rebooting........");
            system("reboot -f");
            break;
        }
#endif
    }

    pthread_exit(NULL);
}
#endif

void initCoreThreads()
{
   //Check remote access and initialize NAT
   char *fwUpURLStr = NULL;
   FirmwareUpdateInfo fwUpdInf;
   char *pUploadEnable = NULL;

   fwUpURLStr = GetBelkinParameter("FirmwareUpURL");
   if(fwUpURLStr && strlen(fwUpURLStr)!=0) {
      memset(&fwUpdInf, 0x00, sizeof(fwUpdInf));
      strncpy(fwUpdInf.firmwareURL, fwUpURLStr, sizeof(fwUpdInf.firmwareURL)-1);
      StartFirmwareUpdate(fwUpdInf);
   }

   if ((0x00 != strlen(g_szHomeId) ) && (0x00 != strlen(g_szPluginPrivatekey)) && (atoi(g_szRestoreState) == 0x0)) {
      gpluginRemAccessEnable = 1;
		UDS_SetRemoteEnableStatus();
		UDS_SetNatData();
#if !defined(PRODUCT_WeMo_Baby)
      remoteAccessUpdateStatusTSParams (0xFF);

      APP_LOG("UPNP",LOG_DEBUG, "UDS call for remoteAccessInit");
      UDS_remoteAccessInit();
#endif

   }
#if !defined(PRODUCT_WeMo_Baby)
#ifdef PRODUCT_WeMo_LEDLight
   //create ledremoteaccess thread for handling a remote api in accordance with a specific event.
   createRemoteThread();
#else
   ithread_create(&sendremoteupdstatus_thread, NULL, sendRemoteAccessUpdateStatusThread, NULL);
   ithread_detach (sendremoteupdstatus_thread);

   /* create send notification monitor thread */
   ithread_t mon_sendremoteupdstatus_thread;
   ithread_create(&mon_sendremoteupdstatus_thread, NULL, sendRemAccessUpdStatusThdMonitor, NULL);
   ithread_detach (mon_sendremoteupdstatus_thread);
#endif

   ithread_t sendConfigChange_thread;
   ithread_create(&sendConfigChange_thread, NULL, sendConfigChangeStatusThread, NULL);
   ithread_detach (sendConfigChange_thread);

   ithread_t rcvSendstatusRsp_thread;
   ithread_create(&rcvSendstatusRsp_thread, NULL, rcvSendstatusRspThread, NULL);
   ithread_detach (rcvSendstatusRsp_thread);
#endif

#ifdef PRODUCT_WeMo_Baby
   remoteAccessUpdateStatusTSParams (0xFF);

   ithread_create(&sendremoteupdstatus_thread, NULL, sendRemoteAccessUpdateStatusThread, NULL);
   ithread_detach (sendremoteupdstatus_thread);

   /* create send notification monitor thread */
   ithread_t mon_sendremoteupdstatus_thread;
   ithread_create(&mon_sendremoteupdstatus_thread, NULL, sendRemAccessUpdStatusThdMonitor, NULL);
   ithread_detach (mon_sendremoteupdstatus_thread);

#endif
   pUploadEnable = GetBelkinParameter(LOG_UPLOAD_ENABLE);
   if( (NULL != pUploadEnable) && (0x00 != strlen(pUploadEnable)) && (atoi(pUploadEnable) == 1))
   {
       APP_LOG("UPNP",LOG_DEBUG, "***************LOG Thread created***************\n");
       pthread_attr_init(&wdLog_attr);
       pthread_attr_setdetachstate(&wdLog_attr, PTHREAD_CREATE_DETACHED);
       pthread_create(&logFile_thread, &wdLog_attr, uploadLogFileThread, NULL);
   }
   else
   {
      APP_LOG("WiFiApp",LOG_DEBUG, "Uploading log files not enabled");
   }

#ifdef _OPENWRT_
    ithread_t systemRestore_thread;
    pthread_attr_init(&sysRestore_attr);
    pthread_attr_setdetachstate(&sysRestore_attr, PTHREAD_CREATE_DETACHED);
    ithread_create(&systemRestore_thread, &sysRestore_attr, systemRestore, NULL);

    parseBootArgs();
#endif
    /*create UDS Client DataPath and monitor thread */
    ithread_t uds_ClientDataPath_thread;
    ithread_create(&uds_ClientDataPath_thread, NULL, UDS_MonitorAndStartClientDataPath, NULL);
}


void core_init_late(int forceEnableCtrlPoint)
{
  // setSignalHandlers();

#if !defined(_OPENWRT_) && !defined(PRODUCT_WeMo_NetCam)
   char lanIp[SIZE_64B];
   char *lanIp1 = GetBelkinParameter("lan_ipaddr");
   int indx = strlen(WAN_IP_ADR);

   memset (lanIp,0,SIZE_64B);

   memcpy (lanIp, lanIp1,indx);
   lanIp[indx] = '\0';
   APP_LOG("UPNP",LOG_DEBUG, "ip: %s\n",lanIp);
   if(strcmp(WAN_IP_ADR,lanIp) != 0) {
      APP_LOG("UPNP",LOG_DEBUG, "#########CHANGING THE WEMO WAN IP...SYSTEM WILL REBOOT##############\n" );
      SetBelkinParameter(LAN_IPADDR, WAN_IP_ADR);
      sleep(40);
      APP_LOG("UPNP", LOG_ALERT, "CHANGING THE WEMO WAN IP");
      system ("reboot");
      //NOT REACHED
      return;
   }
#endif

   initIPC();

#ifdef PRODUCT_WeMo_LEDLight
   init_upnp_lock();
   init_upnp_event_lock();
   init_remote_lock();
#endif

   g_szBuiltFirmwareVersion = FW_REV;
   g_szBuiltTime         = BUILD_TIME;

   {
      char buf[SIZE_256B];
      char *ver=NULL;

      memset (buf,0,SIZE_256B);

      APP_LOG("UPNP",LOG_DEBUG, "Update NVRAM with FW Details..." );
      SetBelkinParameter(WEMO_VERSION_GEMTEK_PROD, FW_REV1);
      strncpy (buf,"\"",sizeof(buf)-1);

#ifndef _OPENWRT_
      if((ver = GetBelkinParameter("my_fw_version")))
         strncat (buf,ver, sizeof(buf));
      else
         strncat (buf, "MY_FW_VER_NULL", sizeof(buf));
      strncat (buf," ", sizeof(buf) - strlen(buf)-1);
      strncat (buf, FW_REV1, sizeof(buf)- strlen(buf)-1);
      strncat (buf,"\"", sizeof(buf)- strlen(buf)-1);
      SetBelkinParameter(MY_FW_VERSION, buf);
#endif
   }

#if defined(PRODUCT_WeMo_Baby)
        BMSetAppSSID();
#endif

   initDeviceUPnP();

   createPasswordKeyData();

#ifndef PRODUCT_WeMo_NetCam
   if (DEVICE_SOCKET == g_eDeviceType)
   {
#ifndef PRODUCT_WeMo_LEDLight
      initLED();
#if defined(LONG_PRESS_SUPPORTED)
      initLongPressLock();
#endif
      ithread_create(&power_thread, NULL, PowerButtonTask, NULL);
      ithread_create(&relay_thread, NULL, RelayControlTask, NULL);
      ithread_detach (power_thread);
      ithread_detach (relay_thread);
      ithread_create(&ButtonTaskMonitor_thread, NULL, ButtonTaskMonitorThread, NULL);
      ithread_detach (ButtonTaskMonitor_thread);
#endif
   }

   wifiGetChannel(INTERFACE_AP, &g_channelAp);
   selectQuietestApChannel();
   APP_LOG("UPNP", LOG_DEBUG, "################### ra0 is on Channel: %d ###################", g_channelAp);

#ifdef __WIRED_ETH__
   if(g_bWiredEthernet) {
      StartInetMonitorThread();
   }
   else
#endif
   {
   initWiFiHandler ();
   startUPnP(forceEnableCtrlPoint);
   }
    APP_LOG("RemoteAccess",LOG_DEBUG, "***********initRule()***********\n");
   initRule();

   if (DEVICE_SENSOR== g_eDeviceType)
   {
      StartSensorTask();
      ithread_create(&SensorTaskMonitor_thread, NULL, SensorTaskMonitorThread, NULL);
      ithread_detach (SensorTaskMonitor_thread);
   }
   #ifdef PRODUCT_WeMo_Jarden
   else if ( (DEVICE_CROCKPOT == g_eDeviceType) || (DEVICE_SBIRON == g_eDeviceType) ||
              (DEVICE_MRCOFFEE == g_eDeviceType) || (DEVICE_PETFEEDER == g_eDeviceType) ||
           (DEVICE_SMART == g_eDeviceType)
         )
    {
      StartJardenTask();
   }
   #endif /*#ifdef PRODUCT_WeMo_Jarden*/
   #ifdef PRODUCT_WeMo_Smart
   else if ( (DEVICE_CROCKPOT == g_eDeviceType) || (DEVICE_SBIRON == g_eDeviceType) ||
              (DEVICE_MRCOFFEE == g_eDeviceType) || (DEVICE_PETFEEDER == g_eDeviceType) ||
           (DEVICE_SMART == g_eDeviceType)
         )
    {
      StartSmartTask();
   }
   #endif /* #ifdef PRODUCT_WeMo_Smart */
   else
   {
      APP_LOG("UPNP", LOG_DEBUG, "################### No Device type found ###################");
   }

   core_init_threads(0);
#elif defined(PRODUCT_WeMo_NetCam)
   APP_LOG("RemoteAccess",LOG_DEBUG, "***********initRule()***********\n");
   initRule();
#endif
}

void core_init_threads(int flag) {
  initCoreThreads();
  char *pszRespXML = CreateManufactureData();
  if(pszRespXML)
   free (pszRespXML);
}

#ifndef _OPENWRT_
void pjUpdateDstTz()
{
   char ltimezone[MAX_RVAL_LEN];
   int index = 0;

   memset(ltimezone, 0x0, sizeof(ltimezone));
   snprintf(ltimezone, sizeof(ltimezone),"%f", g_lastTimeZone);
   index = getTimeZoneIndexFromFlash();
   APP_LOG("UPNP", LOG_DEBUG,"set pj dst data index: %d, lTimeZone:%s gDstEnable: %d success", index, ltimezone, gDstEnable);
   UDS_pj_dst_data_os(index, ltimezone, gDstEnable);
}
#endif

void core_init_early()
{
#ifdef _OPENWRT_
   char szBuff[MAX_DBVAL_LEN];
   char serBuff[MAX_DBVAL_LEN];
   char* DFT_SERIAL_NO = "0123456789";
#endif

   setSignalHandlers();

#ifdef __WIRED_ETH__
#ifdef _OPENWRT_
   if(WiredEthernetUp(0) == -1) {
      g_bWiredEthernet = 1;
      APP_LOG("REMOTEACCESS",LOG_DEBUG,"Wired Ethernet mode");
   }
#else
// sh: read wan_ether_name from nvram.  If it's "vlan1" then we're
// running in wired Ethernet mode, otherwise assume normal WiFi mode
    {
       char *WanEtherName = GetBelkinParameter("wan_ether_name");
       g_bWiredEthernet = 0;

       if(WanEtherName != NULL) {
          APP_LOG("REMOTEACCESS",LOG_DEBUG,"wan_ether_name: %s",WanEtherName);
          if(strcmp(WanEtherName,"vlan1") == 0) {
             g_bWiredEthernet = 1;
             APP_LOG("REMOTEACCESS",LOG_DEBUG,"Wired Ethernet mode");
          }
       }
    }
#endif
#endif

   initFWUpdateStateLock();
   initCommandQueueLock();
   initdevConfiglock();
   initRemoteAccessLock();
   initHomeIdListLock();
   setCurrFWUpdateState(FM_STATUS_DEFAULT);    //setting to default
   webAppInit(0); //call it when no other thread exists in wemoApp

#ifndef PRODUCT_WeMo_NetCam
   SetAppSSID();
   SetAppSSIDCommand();
#endif
   //pluginOpenLog (PLUGIN_LOGS_FILE, CONSOLE_LOGS_SIZE);
#ifdef _OPENWRT_
   APP_LOG("WiFiApp",LOG_DEBUG, "\n #########Removing /tmp/*.starting files\n");
   system("rm -f /tmp/*.starting");

   memset(serBuff, 0x0, MAX_DBVAL_LEN);
   getNvramParameter(serBuff);

   if((0x00 != strlen(serBuff)) && (0x00 == strcmp(serBuff, DFT_SERIAL_NO))) {
      char* szSerialNo = GetSerialNumber();
      if ((0x00 != szSerialNo) && (0x00 != strlen(szSerialNo))) {
         memset(szBuff, 0x0, MAX_DBVAL_LEN);

         snprintf(szBuff, MAX_DBVAL_LEN, "fw_setenv SerialNumber %s", szSerialNo);
         system(szBuff);
      }
   }
#else
    SetBelkinParameter(BELKIN_DAEMON_SUCCESS, "1");
#endif

#if defined(PRODUCT_WeMo_Baby)
   StartIpcUdsServer();
#endif

   SetBelkinParameter(IMPORT_KEY_PARAM_NAME, SIGNED_PUBKEY_FILE_NAME);

   initWatchDog();

#ifndef PRODUCT_WeMo_NetCam
   //disconnect from router to start afresh in case of both reboot and restart of wemoApp
   disconnectFromRouter();
#endif

   //- reset possible
   SetBelkinParameter(SETTIME_SEC, "");
   remoteCheckAttributes();

#if defined(SIMULATED_OCCUPANCY) && defined(PRODUCT_WeMo_LEDLight)
  simulatedOccupancyInit();
#endif
#if !defined(PRODUCT_WeMo_Baby)
#ifndef _OPENWRT_
	// This is only needed on Gemtek builds, on OpenWRT the system
	// clock is set correctly so the local modifications to
	// pjlib's os_time_unix.c to compensate for the system time being
	// set to local time are not longer needed or applied.

   /* pj_dst_data_os is to propagate DST information to PJ lib, but this call to pj_dst_data_os may not require here later , will review and update */
   pjUpdateDstTz();
#endif
   initRemoteUpdSync();
   initRemoteStatusTS();
   setNotificationStatus (1);
#endif

#ifdef PRODUCT_WeMo_Baby
   initRemoteStatusTS();
   setNotificationStatus (1);
#endif

#ifndef _OPENWRT_
   osUtilsCreateLock(&lookupLock);
#endif
}

int setRemoteRestoreParam (int rstval)
{
		char szRstPrm[MAX_RES_LEN];
		memset (szRstPrm, 0, MAX_RES_LEN);
		snprintf(szRstPrm, sizeof(szRstPrm), "%d", rstval);
		SetBelkinParameter(RESTORE_PARAM, szRstPrm);
		SaveSetting();
		memset(g_szRestoreState, 0x0, sizeof(g_szRestoreState));
		strncpy(g_szRestoreState, szRstPrm, sizeof(g_szRestoreState)-1);
		return PLUGIN_SUCCESS;
}

void remoteCheckAttributes() {
		memset(gIcePrevTransactionId,0,sizeof(gIcePrevTransactionId));
		memset(g_szHomeId, 0x00, sizeof(g_szHomeId));
		memset(g_szSmartDeviceId, 0x00, sizeof(g_szSmartDeviceId));
		memset(g_szSmartPrivateKey, 0x00, sizeof(g_szSmartPrivateKey));
		memset(g_szPluginPrivatekey, 0x00, sizeof(g_szPluginPrivatekey));
		memset(g_szPluginCloudId, 0x00, sizeof(g_szPluginCloudId));
		memset(g_szRestoreState, 0x0, sizeof(g_szRestoreState));
		memset(g_routerMac, 0x0, sizeof(g_routerMac));
		memset(g_routerSsid, 0x0, sizeof(g_routerSsid));
		memset(g_NotificationStatus, 0x0, sizeof(g_NotificationStatus));
		memset(g_turnServerEnvIPaddr, 0x00, sizeof(g_turnServerEnvIPaddr));
		memset(g_serverEnvIPaddr, 0x00, sizeof(g_serverEnvIPaddr));
#ifdef WeMo_INSTACONNECT
		memset(gBridgeNodeList, 0x00, sizeof(gBridgeNodeList));
#endif
		g_ServerEnvType = E_SERVERENV_PROD;

		char *homeId = GetBelkinParameter (DEFAULT_HOME_ID);
		if (0x00 != homeId && (0x00 != strlen(homeId))){
				strncpy(g_szHomeId, homeId, sizeof(g_szHomeId)-1);
		}

		char *smartId = GetBelkinParameter (DEFAULT_SMART_DEVICE_ID);
		if (0x00 != smartId && (0x00 != strlen(smartId))){
				strncpy(g_szSmartDeviceId, smartId, sizeof(g_szSmartDeviceId)-1);
		}

		char *smartKey = GetBelkinParameter (DEFAULT_SMART_PRIVATE_KEY);
		if (0x00 != smartKey && (0x00 != strlen(smartKey))){
				strncpy(g_szSmartPrivateKey, smartKey, sizeof(g_szSmartPrivateKey)-1);
		}

		char *pluginKey =	GetBelkinParameter (DEFAULT_PLUGIN_PRIVATE_KEY);
		if (0x00 != pluginKey && (0x00 != strlen(pluginKey))){
				strncpy(g_szPluginPrivatekey, pluginKey, sizeof(g_szPluginPrivatekey)-1);
		}

		char *pluginId = GetBelkinParameter (DEFAULT_PLUGIN_CLOUD_ID);
		if (0x00 != pluginId && (0x00 != strlen(pluginId))){
				strncpy(g_szPluginCloudId, pluginId, sizeof(g_szPluginCloudId)-1);
		}

		char *pRouterMac = GetBelkinParameter (WIFI_ROUTER_MAC);
		if (0x00 != pRouterMac && (0x00 != strlen(pRouterMac))){
				strncpy(g_routerMac, pRouterMac, sizeof(g_routerMac)-1);
		}

		char *pRouterSSID = GetBelkinParameter (WIFI_ROUTER_SSID);
		if (0x00 != pRouterSSID && (0x00 != strlen(pRouterSSID))){
				strncpy(g_routerSsid, pRouterSSID, sizeof(g_routerSsid)-1);
		}

		char *restoreStat = GetBelkinParameter(RESTORE_PARAM);
		if((restoreStat != NULL) && (0x0 != strlen(restoreStat))){
				strncpy(g_szRestoreState, restoreStat, sizeof(g_szRestoreState)-1);
				APP_LOG("UPNP",LOG_DEBUG, "Not Setting remote restore param now......" );
		}else{
				APP_LOG("UPNP",LOG_DEBUG, "Setting remote restore param to 0......" );
				setRemoteRestoreParam (0x0);
		}
		char *lasttimezone = GetBelkinParameter(SYNCTIME_LASTTIMEZONE);
		if (0x00 != lasttimezone && (0x00 != strlen(lasttimezone))){
				g_lastTimeZone = atof(lasttimezone);
				APP_LOG("UPNP", LOG_DEBUG,"setting g_lastTimeZone:%f from flash...success", g_lastTimeZone);
		}else{
				APP_LOG("UPNP", LOG_DEBUG,"Not setting g_lastTimeZone from flash... failure");
		}

		char *lastdstenable = GetBelkinParameter(LASTDSTENABLE);
		if (0x00 != lastdstenable && (0x00 != strlen(lastdstenable))){
				gDstEnable = atoi(lastdstenable);
				APP_LOG("UPNP", LOG_DEBUG,"setting gDstEnable:%d from flash... success", gDstEnable);
		}else{
				APP_LOG("UPNP", LOG_DEBUG,"Not setting gDstEnable from flash... failure");
		}

		char *dstSupported = GetBelkinParameter(SYNCTIME_DSTSUPPORT);
		if (0x00 != dstSupported && (0x00 != strlen(dstSupported))){
				gDstSupported = atoi(dstSupported);
				APP_LOG("UPNP", LOG_DEBUG,"setting gDstSupported:%d from flash... success", gDstSupported);
		}else{
				APP_LOG("UPNP", LOG_DEBUG,"Not setting gDstSupported from flash... failure");
		}
		//NOTIFICATION STATUS
		char *pNotificationStatus = GetBelkinParameter(NOTIFICATION_VALUE);
		if(pNotificationStatus) {
				if((0x0 == strlen(pNotificationStatus))){
						APP_LOG("UPNP",LOG_DEBUG, "Setting Event Notification Status to 0......" );
						setNotificationStatus (0x0);
				}else{
						strncpy(g_NotificationStatus, pNotificationStatus, sizeof(g_NotificationStatus)-1);
				}
		}

		char *serverenv = GetBelkinParameter (CLOUD_SERVER_ENVIRONMENT);
		if ( (0x00 != serverenv) && (0x00 != strlen(serverenv)) )
		{
				strncpy(g_serverEnvIPaddr, serverenv, sizeof(g_serverEnvIPaddr)-1);
		}

		char *turnserverenv = GetBelkinParameter (CLOUD_TURNSERVER_ENVIRONMENT);
		if ( 0x00 !=(turnserverenv)&& (0x00 != strlen(turnserverenv)) ){
			strncpy(g_turnServerEnvIPaddr, turnserverenv, sizeof(g_turnServerEnvIPaddr)-1);
			UDS_SetTurnServerEnvIPaddr(g_turnServerEnvIPaddr);
		}

		char *serverenvtype = GetBelkinParameter (CLOUD_SERVER_ENVIRONMENT_TYPE);
		if (0x00 != serverenvtype && (0x00 != strlen(serverenvtype)) ){
				g_ServerEnvType = (SERVERENV)atoi(serverenvtype);
		}

#ifdef WeMo_INSTACONNECT
		char *pBrList = GetBelkinParameter (WIFI_BRIDGE_LIST);
		if (0x00 != pBrList && (0x00 != strlen(pBrList))){
				strncpy(gBridgeNodeList, pBrList, sizeof(gBridgeNodeList)-1);
		}
#endif
#ifdef WeMo_SMART_SETUP_V2
		char *customizedState = GetBelkinParameter(CUSTOMIZED_STATE);
		if((customizedState!= NULL) && (0x0 != strlen(customizedState)))
		    g_customizedState = atoi(customizedState);
		else
		    setCustomizedState(DEVICE_UNCUSTOMIZED);
		APP_LOG("UPNP",LOG_DEBUG, "Customized state saved is: %d", g_customizedState);
#endif
		char *hw_version = GetBelkinParameter("hwVersion");
		if(hw_version && strlen(hw_version) != 0)
		{
		    ghwVersion = atoi(hw_version);
		}

#if defined(DEBUG_ENABLE)
                char *pUploadEnable = GetBelkinParameter(LOG_UPLOAD_ENABLE);
                if((NULL != pUploadEnable) && (((0x00 != strlen(pUploadEnable)) && (atoi(pUploadEnable) == 0)) || (0x00 == strlen(pUploadEnable))))
                {
                        APP_LOG("UPNP", LOG_DEBUG,"Setting log upload to cloud variable on flash");
                        SetBelkinParameter(LOG_UPLOAD_ENABLE, "1");
                }
                else
                        APP_LOG("UPNP", LOG_DEBUG,"Log upload to cloud variable already set on flash");
#endif
		char * pDbUploadEnable = GetBelkinParameter(DB_UPLOAD_ENABLE);
                if((NULL != pDbUploadEnable) && (((0x00 != strlen(pDbUploadEnable)) && (atoi(pDbUploadEnable) == 0)) || (0x00 == strlen(pDbUploadEnable))))
                {
                        APP_LOG("UPNP", LOG_DEBUG,"Setting db upload to cloud variable on flash");
                        SetBelkinParameter(DB_UPLOAD_ENABLE, "1");
                }
                else
                        APP_LOG("UPNP", LOG_DEBUG,"DB upload to cloud variable already set on flash");
		return;
}

