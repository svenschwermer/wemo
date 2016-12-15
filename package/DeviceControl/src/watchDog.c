/***************************************************************************
*
*
* watchDog.c
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
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include <sys/stat.h>
#include <fcntl.h>

#include "defines.h"
#include "upnp.h"
#include "watchDog.h"
#include "osUtils.h"
#include "utils.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "fw_rev.h"
#include "ithread.h"
#include "wifiSetup.h"
#include "wifiHndlr.h"
#include "remoteAccess.h"
#include "controlledevice.h"
#include "plugin_ctrlpoint.h"
#include "watchDog.h"
#include "natClient.h"
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

#define THREAD_CNT_FILE   "/tmp/thrCnt"
#define MAX_LINE_COUNT    5
#define MAX_NUM_THREADS   60
#define MAX_THREAD_SLEEP  15*1000000
#define MAX_THREAD_RETRY  8


extern int logCounter;
extern int gMainHealth;
extern int gpluginNatInitialized;
extern int gNatReinitTriedCount;

int statFileFd = 0;
int cpuFileFd = 0;
int loadFileFd = 0;
int upFileFd = 0;
int g_numTestAppThreads = -1;

ithread_t numTestApp_thread;
ithread_t MaxThreadCheck_thread;

pthread_t wdTask = -1;

int detectProcessParams(int procId);

void WriteRebootStatus();
int detectRebootCpuParams(cpuInfo *pCpu);

static void checkRunZigeed()
{
  int ret = 0;
  ret = system("ps | grep zbdmonitor.sh | grep -v grep > /dev/null");
  if( ret )
  {
    system("/sbin/zbdmonitor.sh &");
    APP_LOG("WTD", LOG_DEBUG, "checkRunZigeed : %d, re-run zigbeed...\n", ret);
  }
}

void* MaxThreadMonitor(void *arg)
{
  int MonitorProcID=0;
  int MonitorNumThreads =0;
  int RetryCntr = MAX_THREAD_RETRY;

  tu_set_my_thread_name( __FUNCTION__ );

  if( arg )
  {
    MonitorProcID = *(int *)arg;
  }

  APP_LOG("WTD", LOG_CRIT, "### MAX Threads Monitoring STARTED: %d",MonitorNumThreads);

  while( RetryCntr )
  {
    g_numTestAppThreads = detectProcessParams(MonitorProcID);
    pluginUsleep(1000000); //Sleep for 1 sec
    MonitorNumThreads = g_numTestAppThreads;
    APP_LOG("WTD", LOG_CRIT, " Retry Count:%d Threads count reached: %d",
            (MAX_THREAD_RETRY-RetryCntr),MonitorNumThreads);
    if( MAX_NUM_THREADS < MonitorNumThreads )
    {
      if( RetryCntr == 1 )
      {
        APP_LOG("WTD", LOG_ALERT, "MAX Threads count reached System Will Reset: %d",MonitorNumThreads);
        resetSystem();
      }
      else
      {
        RetryCntr--;
        pluginUsleep(MAX_THREAD_SLEEP);
      }
    }
    else
    {
      APP_LOG("WTD", LOG_CRIT, "### Recovered From Max Thread ###");
      break;
    }
  }

  MaxThreadCheck_thread = -1;
  pthread_exit(0);
}

int detectProcessParams(int procId)
{
  FILE *fp = NULL;
  int numThread = 0;
  char buf[MAX_LVALUE_LEN];
  char junk[MAX_DVAL_LEN], cmd[MAX_DBVAL_LEN];

  tu_set_my_thread_name( __FUNCTION__ );

  memset(junk, 0x00, MAX_DVAL_LEN);
  memset(cmd, 0x00, MAX_DBVAL_LEN);

  sprintf(cmd, "cat /proc/%d/status | grep Threads > %s", procId, THREAD_CNT_FILE);
  system(cmd);

  fp = fopen(THREAD_CNT_FILE, "rb");

  if( NULL == fp )
  {
    APP_LOG("WTD",LOG_ERR,"read Error %s", strerror(errno));
    numThread = 0;
  }
  else
  {
    fscanf(fp, "%s %d", junk, &numThread);
  }

  fclose(fp);

  memset(buf, 0x00, sizeof(buf));
  sprintf(buf, "rm %s", THREAD_CNT_FILE);

  APP_LOG("WTD", LOG_DEBUG, "numThread = <%d> ", numThread);

  return numThread;
}

int detectCpuParams(cpuInfo *pCpu)
{
  const char* b;
  char buff[SIZE_1024B];
  unsigned long long ciow,cxxx,cyyy,czzz;

  if( statFileFd )
  {
    lseek(statFileFd, 0L, SEEK_SET);
  }
  else
  {
    statFileFd = open("/proc/stat", O_RDONLY, 0);
    if( statFileFd == -1 )
    {
      APP_LOG("WTD",LOG_ERR,"/proc/stat file open Error %s", strerror(errno));
      return -1;
    }
  }

  memset(buff, 0x00, sizeof(buff));
  read(statFileFd,buff,SIZE_1024B-1);

  b = strstr(buff, "cpu ");
  if( b )
  {
    sscanf(b, "cpu  %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu",
          &pCpu->userCpu, &pCpu->niceCpu, &pCpu->systemCpu, &pCpu->idleCpu, &ciow, &cxxx, &cyyy, &czzz);
  }

  pCpu->totalCpu = pCpu->userCpu + pCpu->niceCpu + pCpu->systemCpu + pCpu->idleCpu;

  APP_LOG("WTD", LOG_DEBUG, "<%Lu> <%Lu> <%Lu> <%Lu> <%Lu>",
          pCpu->userCpu, pCpu->niceCpu, pCpu->systemCpu, pCpu->idleCpu, pCpu->totalCpu);

  return 1;
}

unsigned long detectParams()
{
  const char* b;
  char buff[SIZE_1024B];
  unsigned long memFree = 0;

  if( cpuFileFd )
  {
    lseek(cpuFileFd, 0L, SEEK_SET);
  }
  else
  {
    cpuFileFd = open("/proc/meminfo", O_RDONLY, 0);
    if( cpuFileFd == -1 )
    {
        APP_LOG("WTD",LOG_ERR,"/proc/stat file open Error %s", strerror(errno));
        return -1;
    }
  }

  memset(buff, 0x00, sizeof(buff));
  read(cpuFileFd, buff, SIZE_1024B-1);

  b = strstr(buff, "MemFree: ");
  if( b )
  {
    sscanf(b, "MemFree:  %lu kB", &memFree);
  }

  APP_LOG("WTD", LOG_DEBUG, "memFree = <%lu> ", memFree);

  return memFree;
}

int detectUptime(int *hr, int *min, int *sec)
{
  char buff[SIZE_1024B];
  double sysUpTime = 0.0, sysIdleTime = 0.0;

  if( upFileFd )
  {
    lseek(upFileFd, 0L, SEEK_SET);
  }
  else
  {
    upFileFd = open("/proc/uptime", O_RDONLY, 0);
    if( upFileFd == -1 )
    {
      APP_LOG("WTD", LOG_ERR, "/proc/uptime file open Error %s", strerror(errno));
      return -1;
    }
  }

  memset(buff, 0x00, sizeof(buff));
  read(upFileFd, buff, SIZE_1024B-1);
  sscanf(buff, "%lf %lf", &sysUpTime, &sysIdleTime);

  sysUpTime += 0.5;
  int nearSec = (int)sysUpTime;

  *hr = nearSec/3600;
  nearSec = nearSec - (*hr)*3600;
  *min = nearSec/60;
  nearSec = nearSec - (*min)*60;
  *sec = nearSec;

  APP_LOG("WTD", LOG_DEBUG, "Uptime: %d:%d:%d", *hr, *min, *sec);

  return 0;
}

void initLogFileWatchdog()
{
  osUtilsCreateLock(&logMutex);
}

int initWatchDog()
{
  int ret = FAILURE;

  openFile(1);
#if defined(PRODUCT_WeMo_Baby) || defined(PRODUCT_WeMo_Streaming)
  ret = DisableAppWatchDogState();
#endif

  initLogFileWatchdog();

  /*Create watchdog task*/
  ret = pthread_create(&wdTask, NULL, (void*)&watchDogTask, NULL);
  if( ret < SUCCESS )
  {
    APP_LOG("WTD",LOG_ALERT,"WatchDog thread not created");
  }
  else
  {
    pthread_detach(wdTask);
    APP_LOG("WTD",LOG_CRIT,"WatchDog thread created Detached");
  }
  return ret;
}

void printRebootStatus()
{
  int rebootStatus = -1, sta;
  unsigned long utcTime;
  char rebootStat[MAX_FILE_LINE];

  memset(rebootStat, '\0', MAX_FILE_LINE);

  sta = GetRebootStatus(&rebootStatus, &utcTime);

  APP_LOG("UPNP", LOG_DEBUG, "Reboot status:%d:%d\n", rebootStatus, sta);

  if( sta >= SUCCESS )
  {
    switch( rebootStatus )
    {
      case REBOOT_UNKNOWN:
        snprintf(rebootStat, sizeof(rebootStat), "Reboot Due to Unknown Reason");
        break;
      case REBOOT_NORMAL:
        snprintf(rebootStat, sizeof(rebootStat), "Normal Reboot");
        break;
      case REBOOT_SOFTDOG:
        {
          snprintf(rebootStat, sizeof(rebootStat), "#####Watchdog triggered Reboot#####");
        }
        break;
    case REBOOT_MEMORY:
        snprintf(rebootStat, sizeof(rebootStat), "Reboot: Memory crunch");
        break;
    case REBOOT_LOADING:
        snprintf(rebootStat, sizeof(rebootStat), "Reboot: CPU overloading");
        break;
    }
  }

  strncat(rebootStat, ", FW Ver: ", sizeof(rebootStat)-strlen(rebootStat)-1);
  strncat(rebootStat, FW_REV, sizeof(rebootStat)-strlen(rebootStat)-1);
  APP_LOG("WTD", LOG_ALERT, "%s", rebootStat);
  WriteRebootStatus();
}

#define MEM_FILE	"/tmp/Belkin_settings/RebootMemUsage.log"
#define LOAD_FILE	"/tmp/Belkin_settings/RebootCPULoad.log"
#define CPU_FILE	"/tmp/Belkin_settings/RebootCPUUsage.log"
#define MAX_LOG_SIZE	256

void WriteRebootStatus()
{
  FILE *fp = NULL;
  cpuInfo cpuUsage;
  char PrintBuffer[MAX_LOG_SIZE];

  memset(&cpuUsage, 0, sizeof(cpuInfo));

  int retVal = detectCpuParams(&cpuUsage);

  fp = fopen(MEM_FILE, "rb");
  if( NULL == fp )
  {
    APP_LOG("WTD", LOG_DEBUG, "No Reboot Mem File");
  }
  else
  {
    memset(PrintBuffer, 0x00, MAX_LOG_SIZE);
    fgets(PrintBuffer, MAX_LOG_SIZE, fp);
    APP_LOG("WTD", LOG_ALERT, "Reboot Mem Info:%s", PrintBuffer);
  	if( fp )
    {
      fclose(fp);
    }
    memset(PrintBuffer, 0x00, MAX_LOG_SIZE);
    snprintf(PrintBuffer, sizeof(PrintBuffer), "rm -f %s", MEM_FILE);
    system(PrintBuffer);
  }

  fp = fopen(LOAD_FILE, "rb");
  if( NULL == fp )
  {
    APP_LOG("WTD", LOG_DEBUG, "No Reboot Load File");
  }
  else
  {
    memset(PrintBuffer, 0x00, MAX_LOG_SIZE);
    fgets(PrintBuffer, MAX_LOG_SIZE, fp);
    APP_LOG("WTD", LOG_ALERT, "Reboot CPU Load Info:%s", PrintBuffer);
  	if( fp )
    {
      fclose(fp);
    }
    memset(PrintBuffer, 0x00, MAX_LOG_SIZE);
    APP_LOG("WTD", LOG_DEBUG, "Deleting CPU Load File");
    snprintf(PrintBuffer, sizeof(PrintBuffer), "rm -f %s", LOAD_FILE);
    system(PrintBuffer);
  }

  //CPU Check
  {
    cpuInfo cpuPeriodic;
    double perI; //Calculate only Idle percentage.

    memset(&cpuPeriodic, 0, sizeof(cpuInfo));
    retVal = detectRebootCpuParams(&cpuPeriodic);
    if( retVal )
    {
      unsigned long long diffTotal,diffU,diffN,diffS,diffI;

      diffTotal = cpuPeriodic.totalCpu - cpuUsage.totalCpu;
      diffU = cpuPeriodic.userCpu - cpuUsage.userCpu;
      diffN = cpuPeriodic.niceCpu - cpuUsage.niceCpu;
      diffS = cpuPeriodic.systemCpu - cpuUsage.systemCpu;
      diffI = cpuPeriodic.idleCpu - cpuUsage.idleCpu;
      if(diffTotal > 0)
        perI = (diffI*100)/diffTotal;
      else
        perI = 100.0;
      perI = 100.0 - perI;
      snprintf(PrintBuffer, sizeof(PrintBuffer), "%f", perI);
      APP_LOG("WTD",LOG_CRIT,"Unknown Reboot");
    }
    else
    {
      APP_LOG("WTD", LOG_DEBUG, "No Reboot CPU Usage File");
    }
  }
}

int detectRebootCpuParams (cpuInfo *pCpu)
{
  const char* b;
  FILE *fp = NULL;
  char buff[SIZE_1024B];
  unsigned long long ciow, cxxx, cyyy, czzz;

  fp = fopen(CPU_FILE, "rb");
  if( NULL == fp )
  {
    APP_LOG("WTD", LOG_DEBUG, "No Reboot CPU File");
    return 0;
  }

  memset(buff, 0x00, sizeof(buff));
  fgets(buff,SIZE_1024B-1,fp);

  b = strstr(buff, "cpu ");
  if(b)
  {
    sscanf(b, "cpu  %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu",
          &pCpu->userCpu, &pCpu->niceCpu, &pCpu->systemCpu, &pCpu->idleCpu, &ciow, &cxxx, &cyyy, &czzz);
  }

  pCpu->totalCpu = pCpu->userCpu + pCpu->niceCpu + pCpu->systemCpu + pCpu->idleCpu;

  APP_LOG("WTD", LOG_DEBUG,"<%Lu> <%Lu> <%Lu> <%Lu> <%Lu>",
          pCpu->userCpu, pCpu->niceCpu, pCpu->systemCpu, pCpu->idleCpu, pCpu->totalCpu);

  memset(buff, 0x00, MAX_LOG_SIZE);
  APP_LOG("WTD",LOG_DEBUG,"Deleting CPU Usage File");

  if( fp )
  {
    fclose(fp);
  }

  snprintf(buff, sizeof(buff), "rm -f %s", CPU_FILE);
  system(buff);
  return 1;
}

void watchDogTask(void)
{
  char logBuf[MAX_BUF_LEN], buf[MAX_DBVAL_LEN];
  cpuInfo cpuUsage;
  unsigned long memFree = 0;
  int health = 0;
  int numThreads = 0, procId = 0;
  pthread_attr_t wemoApp_attr;
  int logSysCnt = 0;
  int state = 0;
  int sleepTime = WD_DEFAULT_TRIG_TIME - WD_DEFAULT_DIFF_TIME, retVal;
  struct stat st;


#if defined(PRODUCT_WeMo_Baby) || defined(PRODUCT_WeMo_Streaming)
  int wdstate = -1;
#endif

#if !defined(PRODUCT_WeMo_Baby) && !defined(PRODUCT_WeMo_Streaming)
  int wait_nat_loop = 0;
  int natStatus = NATCL_HEALTH_GOOD;
#endif

  tu_set_my_thread_name( __FUNCTION__ );

#if !defined(PRODUCT_WeMo_NetCam)
  printRebootStatus();
#endif

  memset(&cpuUsage, 0, sizeof(cpuInfo));

  retVal = detectCpuParams(&cpuUsage);

  procId = getpid();

  while( 1 )
  {
#if defined(PRODUCT_WeMo_Baby) || defined(PRODUCT_WeMo_Streaming)
    wdstate = GetAppWatchDogState();
    if (wdstate == 3)
    {
    }
    else if (wdstate == 1)
    {
      ResetAppWatchDogState();
    }
    else if (wdstate == 0)
    {
      break;
    }
#endif

    if( logCounter > FILE_WRITE_TIMER )
    {
      //Watchdog timer is 4 mins, 4*90= 6hours
      APP_LOG("WTD", LOG_DEBUG, "******6 hours timer expired******");
      writeLogs(CRITICAL, "", "");
    }

    logCounter++;

    memset(logBuf, 0x00, MAX_BUF_LEN);
    memset(buf, 0x00, MAX_DBVAL_LEN);

    g_numTestAppThreads = -1;

//Detect the number of threads running
#if !defined(PRODUCT_WeMo_NetCam)
    g_numTestAppThreads = detectProcessParams(procId);
    numThreads = g_numTestAppThreads;

    pluginUsleep(1000000); //Sleep for 1 sec
#endif
    //CPU Check
    {
      cpuInfo cpuPeriodic;
      double perI; //Calculate only Idle percentage.

      memset(&cpuPeriodic, 0, sizeof(cpuInfo));
      retVal = detectCpuParams(&cpuPeriodic);
      if( retVal )
      {
        unsigned long long diffTotal, diffU, diffN, diffS, diffI;

        diffTotal = cpuPeriodic.totalCpu - cpuUsage.totalCpu;
        diffU = cpuPeriodic.userCpu - cpuUsage.userCpu;
        diffN = cpuPeriodic.niceCpu - cpuUsage.niceCpu;
        diffS = cpuPeriodic.systemCpu - cpuUsage.systemCpu;
        diffI = cpuPeriodic.idleCpu - cpuUsage.idleCpu;
        memcpy (&cpuUsage,&cpuPeriodic,sizeof(cpuInfo));
        if( diffTotal > 0 )
        {
          perI = (diffI*100)/diffTotal;
        }
        else
        {
          perI = 100.0;
        }
        perI = 100.0 - perI;
        snprintf(logBuf, sizeof(logBuf), "%f", perI);
      }
    }

    memFree = detectParams();
    snprintf(buf,sizeof(buf),"  %lu kB",memFree);
    strncat(logBuf,buf, sizeof(logBuf)-strlen(logBuf)-1);

    int hr = 0, min = 0, sec = 0;
    memset(buf, 0x00, MAX_DBVAL_LEN);

    detectUptime(&hr, &min, &sec);
    snprintf(buf, sizeof(buf), " ,Uptime: %d:%d:%d", hr, min, sec);
    strncat(logBuf, buf, sizeof(logBuf)-strlen(logBuf)-1);

#if !defined(PRODUCT_WeMo_NetCam)
    memset(buf, 0x0, MAX_DBVAL_LEN);
    snprintf(buf, sizeof(buf), " ,# of wemoApp threads: %d ", numThreads);
    strncat(logBuf, buf, sizeof(logBuf)-strlen(logBuf)-1);

    //Reset the system when threshold crosses
    if( MAX_NUM_THREADS < numThreads )
    {
      APP_LOG("WTD", LOG_CRIT, "%s", logBuf);
      APP_LOG("WTD", LOG_ALERT, "MAX Threads count reached");

      pthread_attr_init(&wemoApp_attr);
      pthread_attr_setdetachstate(&wemoApp_attr, PTHREAD_CREATE_DETACHED);
      ithread_create(&MaxThreadCheck_thread, &wemoApp_attr, MaxThreadMonitor , (void *)&procId);
    }
#endif

#ifdef PRODUCT_WeMo_LEDLight
    checkRunZigeed();
#endif

    pluginUsleep(sleepTime*1000000);

    state = getCurrFWUpdateState();
    if( state == FM_STATUS_UPDATE_STARTING )
    {
      APP_LOG("WTD", LOG_ALERT, "Firmware Updating...");
      continue;
    }

#ifdef PRODUCT_WeMo_LEDLight
    //check if Link is zigbee router and then if yes, watchdog does not check whether UPnP is alive or not.
    if( 0 == stat("/tmp/Belkin_settings/zb_router", &st) )
    {
      APP_LOG("WTD", LOG_ALERT, "Link is running as Zigbee Router mode...");
      continue;
    }
#endif

    health = UpnpGetMiniServerHealth();
    if( !health )
    {
      APP_LOG("WTD", LOG_CRIT, "%s", logBuf);
      APP_LOG("WTD",LOG_ALERT,"MiniServer Health BAD!!!!!");
      resetSystem();
    }
    else
    {
      APP_LOG("WTD",LOG_DEBUG,"MiniServer Health Okay [%d]", health);
      UpnpResetMiniServerHealth();
    }

    if( !gMainHealth )
    {
      APP_LOG("WTD", LOG_CRIT, "%s", logBuf);
      APP_LOG("WTD",LOG_ALERT,"Main Health BAD!!!!!");
      resetSystem();
    }
    else
    {
      if( logSysCnt % 16 == 0 )
        APP_LOG("WTD",LOG_CRIT,"System Usage: %s", logBuf); //A counter of 1 hour should be used for this.

      logSysCnt++;

      APP_LOG("WTD",LOG_DEBUG,"Main Health Okay [%d]", gMainHealth);
      gMainHealth = 0;
    }

#if !defined(PRODUCT_WeMo_Baby) && !defined(PRODUCT_WeMo_Streaming)
    APP_LOG("WTD", LOG_DEBUG, "remote access status - %d internet connectivity status - %d",
            gpluginRemAccessEnable, getCurrentClientState());

    if( (gpluginRemAccessEnable) && ((getCurrentClientState()) == STATE_CONNECTED) )
    {
      natStatus = UDS_monitorNATCLStatus(NULL);
      int pluginNatInitialized = UDS_pluginNatInitialized();
      if( natStatus == NATCL_HEALTH_NOTGOOD )
      {
        APP_LOG("WTD", LOG_DEBUG, "NAT doesn't look to be in good status %d-%d-%d",
                pluginNatInitialized, gpluginRemAccessEnable, wait_nat_loop);

        ++wait_nat_loop;

        if( wait_nat_loop >= MAX_RETRY_COUNT )
        {
          APP_LOG("WTD", LOG_CRIT, "%s", logBuf);
          APP_LOG("WTD", LOG_ALERT, "NAT doesn't look to be in good status %d-%d-%d",
                  pluginNatInitialized, gpluginRemAccessEnable, wait_nat_loop);
#ifdef __NFTOCLOUD__
          sendNATStatus(1);
#endif
          APP_LOG("WTD", LOG_ERR, "remote access nat is not good after wait_nat_loop %d", wait_nat_loop);
	  UDS_invokeNatReInit(NAT_FORCE_REINIT);
          wait_nat_loop = 0;
          //resetSystem();
        }
        else
        {
          APP_LOG("WTD", LOG_DEBUG, "NAT doesn't look to be in good status, re-init %d-%d-%d",
                  pluginNatInitialized, gpluginRemAccessEnable, wait_nat_loop);
          UDS_invokeNatReInit(NAT_REINIT);
        }
      }
      else
      {
        APP_LOG("WTD", LOG_DEBUG, "remote access nat init inprogress or up %d-%d-%d",
              pluginNatInitialized, gpluginRemAccessEnable, wait_nat_loop);
        wait_nat_loop = 0;
#ifdef __BACKOFFTEST__
        char *inv_nat=GetBelkinParameter("nat_backoff_test");
        if( (0x00 != inv_nat) && 0x00 != (strlen(inv_nat)) )
        {
          APP_LOG("REMOTEACCESS", LOG_DEBUG, "nat_backoff_test is read as %s ",inv_nat);
          int inv_nat_idx=atoi(inv_nat);
          APP_LOG("REMOTEACCESS", LOG_DEBUG, "nat_backoff_test is set as inv_nat_idx %d ",inv_nat_idx);
          if (inv_nat_idx == 1)
          {
            APP_LOG("REMOTEACCESS", LOG_DEBUG, "nat_backoff_test is set as inv_nat_idx %d....invokeNatReInit ",
                  inv_nat_idx);
            UDS_invokeNatReInit(NAT_REINIT);
          }
          if( inv_nat_idx == 3 )
          {
            APP_LOG("REMOTEACCESS", LOG_DEBUG, "nat_backoff_test is set as inv_nat_idx %d....disconnectFromRouter ",
                  inv_nat_idx);
            disconnectFromRouter();
          }
        }
#endif
      }
    }
#endif
  }
}
