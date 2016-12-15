/***************************************************************************
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
***************************************************************************/
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termios.h>
#include <ctype.h>
#include <syslog.h>
#include <linux/serial.h>
#include <signal.h>

#include "wasp.h"
#include "wasp_api.h"
#define  WASPD_PRIVATE
#include "waspd.h"
#include "wasp_async.h"
#include "pseudo.h"

#define OPTION_STRING      "bBdD:eg:i:I:j:lLm:np:q:s:Stv:V"

#define KEYBOARD_POLL_TIME 100   // .1 seconds
#define LINK_CHECK_TIME    1000  // 1 second
#define CLOCK_SET_TIME     (1000 * 60 * 60 * 12)   // 12 hours

#define IS_TYPE_ENUM(p) \
   ((p->Usage & WASP_USAGE_PSEUDO) == 0 && p->Type == WASP_VARTYPE_ENUM)

#define DEC2BCD(x) ((u8) ((((x)%10) & 0xf) | (((x)/10) << 4)))

#ifdef i386
#define LOG_PATH  "waspd.log"
#else
#define LOG_PATH  "/tmp/waspd.log"
#endif

typedef struct TestSetVar_TAG TestSetVar;
struct TestSetVar_TAG {
   TestSetVar *Link; // NB: must be first
   char *VarName;
   WaspVariable Var;
};


time_t StartTime = 0;
int gSimulated = FALSE;
int gSendNOP = FALSE;
int gVarId = 0;
int gType;
int gVarVal;
int gGetLiveVal = FALSE;
int gGetChangedVal = FALSE;
char *gVarName = NULL;
char *gVarValPtr = NULL;
Timer *gTimerHead = NULL;
int gPollingEnabled = TRUE;
int gDeviceOnly = FALSE;
int gClientVersion = 0; // 1.04 = 104 decimal
TestSetVar *gSetVars = NULL;
int gNumSetVars = 0;
int gJardenMode = FALSE;
int gSimulatedDeviceID = -1;
char *gSimulatedDeviceDesc = NULL;

WaspApiMsg gReply;
struct timeval gTimeNow;
int g_bTimeSynced = FALSE;
int gDebug = 0;


// Number of seconds since program start.
// It might wrap, but it sure as hell won't go backwards until it does.
// All timers should be based on this, not the absolute time of day which
// may jump when the time of day is updated from the Internet.
time_t UpTime = 0;

int gIpcSocket = -1;

char *DevicePath = NULL;
int gRunMainLoop = TRUE;
int gSerialFd = -1;
AdrUnion gHisAdr;
int gResponseCmd = 0;
WaspAsyncCtrl *gAsyncCtrl = NULL;
HandlerArgs gButtonPollerArgs;
HandlerArgs gGenericArgs;
u32 gBestBaudRate;
int gVarChanged = TRUE;
AsyncReq *gGetChangeRequest = NULL;

#ifndef SIMULATE_DEVICE
// Unless we're simulating a device the only command
// we respond to is NOP.  Even that is undocumented as
// WASP is defined as master/slave currently.
CmdTableEntry CmdTable[] = {
   {WASP_CMD_NOP,CmdNopHandler},
   {0,NULL} // end of table marker
};
#endif

typedef enum {
// Initial startup states
   NEXT_STATE,
   SEND_NOP = 1,
   GET_DEVICE_WASP_VER,
   GET_BAUDRATES, // may branch to GET_DEVICE_VARS
   SET_NEW_BAUDRATE,
   TEST_NEW_BAUDRATE,

   GET_DEVICE_VARS,
   GET_DEVICE_ENUMS,
   GET_DEVICE_BUTTONS,
   READ_ALL_VARS,
   ENABLE_ASYNC,
   READY,

   LINK_RESET,
   RESET_BAUDRATE,   // branches to ENABLE_ASYNC
   NUM_STATES
} WaspState;

const char *State2Text[] = {
   "SEND_NOP",
   "GET_DEVICE_WASP_VER",
   "GET_BAUDRATES",
   "SET_NEW_BAUDRATE",
   "TEST_NEW_BAUDRATE",
   "GET_DEVICE_VARS",
   "GET_DEVICE_ENUMS",
   "GET_DEVICE_BUTTONS",
   "READ_ALL_VARS",
   "ENABLE_ASYNC",
   "READY",
   "LINK_RESET",
   "RESET_BAUDRATE"
};

typedef enum {
   READ_ALL = 1,
   READ_PERIODIC,
   READ_SETTINGS,
   READ_ASYNC
} PollType;

int VarType2NameOffset[] = {
   0, // not used
   offsetof(VariableAttribute,MinMax.U8.Name),  // WASP_VARTYPE_ENUM
   offsetof(VariableAttribute,MinMax.U16.Name), // WASP_VARTYPE_PERCENT
   offsetof(VariableAttribute,MinMax.I16.Name), // WASP_VARTYPE_TEMP
   offsetof(VariableAttribute,MinMax.Int.Name), // WASP_VARTYPE_TIME32
   offsetof(VariableAttribute,MinMax.U16.Name), // WASP_VARTYPE_TIME16
   offsetof(VariableAttribute,MinMax.BCD3.Name),// WASP_VARTYPE_TIMEBCD
   offsetof(VariableAttribute,MinMax.U8.Name),  // WASP_VARTYPE_BOOL
   offsetof(VariableAttribute,MinMax.BCD4.Name),// WASP_VARTYPE_BCD_DATE
   offsetof(VariableAttribute,MinMax.BCD7.Name),// WASP_VARTYPE_DATETIME
   offsetof(VariableAttribute,MinMax.U32.Name), // WASP_VARTYPE_STRING
   offsetof(VariableAttribute,MinMax.U32.Name), // WASP_VARTYPE_BLOB
   offsetof(VariableAttribute,MinMax.U8.Name),  // WASP_VARTYPE_UINT8
   offsetof(VariableAttribute,MinMax.I8.Name),  // WASP_VARTYPE_INT8
   offsetof(VariableAttribute,MinMax.U16.Name), // WASP_VARTYPE_UINT16
   offsetof(VariableAttribute,MinMax.I16.Name), // WASP_VARTYPE_INT16
   offsetof(VariableAttribute,MinMax.U32.Name), // WASP_VARTYPE_UINT32
   offsetof(VariableAttribute,MinMax.Int.Name), // WASP_VARTYPE_INT32
   offsetof(VariableAttribute,MinMax.U16.Name), // WASP_VARTYPE_TIME_M16
};


const char *Usages[] = {
   "Invalid!",
   "FIXED",
   "MONITORED",
   "DESIRED",
   "CONTROLLED"
};

const char *PseudoUsages[] = {
   "Minimum value",
   "Maximum value",
   "Range entry",
   "Range exit"
};


const char *TrackingFlags[] = {
   "Push",  // TRACK_FLAG_PUSH, 0x80
   "Save"   // TRACK_FLAG_SAVE, 0x40
};


const char *PseudoUsage[] = {
   "Minimum value",  // WASP_USAGE_MINIMUM
   "Maximum value",  // WASP_USAGE_MAXIMUM
   "Range entered",  // WASP_USAGE_RANGE_ENTER
   "Range exited",   // WASP_USAGE_RANGE_EXIT
};

u8 gDevVar = WASP_VAR_DEVICE;
u8 gAsyncVars = 0;
u8 gDevVars = 0;
u8 gPseudoVars = 0;
u8 gLastChangedVar;

u8 gEnumVal = 0;
u16 gPollDelay;
int gQueueLen = 0;

WaspApiMsg gChangedRequest;

WaspState gWaspdState;
char *gDeviceWaspVersion = NULL;
int g_bClockTimeSet = 0;

typedef struct StartVarInit_TAG StartVarInit;
struct StartVarInit_TAG {
   VarID ID;
   VarType Type;
   VarUsage Usage;
   char  *Name;
};

StartVarInit StandardVars[] = {
   {WASP_VAR_DEVID,WASP_VARTYPE_UINT32,WASP_USAGE_FIXED,"Device ID"},
   {WASP_VAR_FW_VER,WASP_VARTYPE_STRING,WASP_USAGE_FIXED,"FW_VER"},
   {WASP_VAR_DEV_DESC,WASP_VARTYPE_STRING,WASP_USAGE_FIXED,"Device Desc"},
   {WASP_VAR_WEMO_STATUS,WASP_VARTYPE_ENUM,WASP_USAGE_CONTROLLED,"WeMo Status"},
   {WASP_VAR_WEMO_STATE,WASP_VARTYPE_ENUM,WASP_USAGE_CONTROLLED,"WeMo State"},
   {WASP_VAR_WASP_VER,WASP_VARTYPE_STRING,WASP_USAGE_FIXED,"WASP Version"},
   {WASP_VAR_CLOCK_TIME,WASP_VARTYPE_DATETIME,WASP_USAGE_CONTROLLED,"Clock Time"},
   {0},  // end of table
};

LedState LedTestScript[] = {
   {1,2,3,4},
   {5,6,7,8},
   {LED_DURATION_INFINITE}
};


ButtonRing gButtonRing;

DeviceVar *Variables[NUM_STANDARD_VARS+MAX_DEVICE_VARS];

// In jarden.c
#ifdef SIMULATE_JARDEN
void SimJarden(int mode);
void SimJardenTxIsr(void);
#endif

void Usage(void);
int MainLoop(void);
int UdpInit(u16 Port);
void IpcRead(int fd);
int UdpSend(int fd,AdrUnion *pToAdr,WaspApiMsg *pMsg,int MsgLen);
int APITest(int Type,int Arg);
void SendCmd(char *Cmd);
void GetTimeNow(void);
void ProcessWaspResponse(WaspAsyncCtrl *pCtrl);
int StartNopHandler(HandlerArgs *p);
int StartValHandler(HandlerArgs *p);
int StartVarDescHandler(HandlerArgs *p);
void StartGetNextEnum(WaspAsyncCtrl *p);
int StartEnumDescHandler(HandlerArgs *p);
int StartButtonsHandler(HandlerArgs *p);
int SetBaudRateHandler(HandlerArgs *p);
int StartEnableAsyncHandler(HandlerArgs *p);
void DumpVarAttributes(WaspVarDesc *p);
int AsyncApiRequest(WaspApiMsg *pRequest,int ReqLen);
int AsyncApiHandler(HandlerArgs *p);
int FindVariableByName(const char *Name);
DeviceVar *FindDevVar(u8 VarID);
int Var2Ipc(WaspVariable *pVar,WaspApiMsg **pReply);
int Vars2Ipc(GetVarsList *p,WaspApiMsg **ppReply);
int VarList2Ipc(WaspApiMsg **pReply);
int CopyVarValue(WaspVariable *pVar,void *pBuf,int MaxLen,int *pDataLen,
                 int bNullTerminate);
int SendVar2Dev(WaspAsyncCtrl *p,VarID ID,AsyncReq *pAsync);
WaspVarDesc *GetVarDesc(u8 VarID);
void ButtonPoller(void *pData);
void VariablePoller(void *pData);
int LinkCheckHandler(HandlerArgs *p);
void LinkChecker(void *p);
void QueueButton(u8 Button);
int ButtonPressHandler(HandlerArgs *p);
void CheckTimers(void);
void ReadVariables(PollType Which);
void StartPolling(void);
void SignalHandler(int Signal);
TestSetVar *ParseSetVar(char *arg);
void FreeSetVars(void);
int BcdCheck(u32 U32);
int AsyncDataHandler(HandlerArgs *p);
WaspVariable *CreateWaspVariable(void);
DeviceVar *CreateDeviceVar(int DescLen);
int CreateStdVars(void);
void SetStartupState(WaspAsyncCtrl *p,WaspState NewState);
void FreeDeviceVar(DeviceVar *pDevVar);
void FreeTimers(void);
int ClockTimeSetHandler(HandlerArgs *p);
void SendClockTime(void *p);
void SendChangedVariable(void);

void Usage()
{
   printf("Usage: waspd [options]\n");
   printf("\t  -b - Read button presses\n");
   printf("\t  -B - disable button polling\n");
   printf("\t  -d - debug\n");
   printf("\t  -D - set serial device path, i.e. /dev/ttyS0\n");
   printf("\t  -e - Send EXIT command to daemon to exit.\n");
   printf("\t  -gc<Var> - Get variable <Var> from cache.\n");
   printf("\t  -gC - Wait for a variable to change and get the value.\n");
   printf("\t  -gl<Var> - Get variable <Var> from the device.\n");
   printf("\t  -i<DevId>   - Emulate device with device id <DevId> (hex)\n");
   printf("\t  -I<DevDesc> - Emulate device with device Description <DevDesc>\n");
   printf("\t  -j - Use Jarden device code simuator.\n");
   printf("\t  -l - List variable attributes.\n");
   printf("\t  -L - test set LED script command with fixed data\n");
   printf("\t  -mc - test getting multiple values from cache\n");
   printf("\t  -ml - test getting multiple values from the device\n");
   printf("\t  -ps<file> - Save pesudo variables to <file>.\n");
   printf("\t  -pr<file> - restore pesudo variables from <file>.\n");
   printf("\t  -pd<file> - Dump pesudo variables saved in <file>.\n");
   printf("\t  -q<len> - Set ASYNC the Queue depth.\n");
   printf("\t  -n - Send only a NOP command to device on startup.\n");
   printf("\t  -s<Var>:<type>:<Value> - Set <Var> of <type> to <value>.\n");
#ifdef SIMULATE_DEVICE
   printf("\t  -S - Simulate device only.\n");
#endif
   printf("\t  -t - get selftest results.\n");
   printf("\t  -v<hex> - Set verbose output mask\n");
   printf("\t  -v?     - Display verbose output mask usage\n");
   printf("\t  -V - display device interface firmware version\n");
}

int main(int argc, char *argv[])
{
   int Option;
   int Ret = 0;
   int bSendQuit = FALSE;
   int bGetVar = FALSE;
   int bTestCommand = FALSE;
   int bTestGetMultiple = FALSE;
   int bGetSelfTest = FALSE;
   int bSetLEDScript = FALSE;
   int bListVarAttribs = FALSE;
   int bDaemon = FALSE;
   int bDisplayVersion = FALSE;
   int bReadButtons = FALSE;
   int bExit = FALSE;
   int QueueDepth = -1;
   int i;
   char *PseudoArg = NULL;

   struct sigaction Act;
   struct sigaction Old;

   Act.sa_flags = (SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART);
   Act.sa_handler = SignalHandler;
   sigemptyset(&Act.sa_mask);

   if(sigaction(SIGTERM,&Act,&Old) || sigaction(SIGSEGV,&Act,&Old)) {
      LOG("%s: sigaction failed - %s\n",__FUNCTION__,strerror(errno));
   }

   while ((Option = getopt(argc, argv,OPTION_STRING)) != -1) {
      switch (Option) {
         case 'b':
            bReadButtons = TRUE;
            bTestCommand = TRUE;
            break;

         case 'B':
            gPollingEnabled = FALSE;
            break;

         case 'd':
            gDebug = 1;
            break;

         case 'D':
            if(DevicePath != NULL) {
               free(DevicePath);
            }
            DevicePath = strdup(optarg);
            bDaemon = TRUE;
            break;
         
         case 'e':
            bSendQuit = TRUE;
            bTestCommand = TRUE;
            break;

         case 'g': {
            char *cp = optarg;

            if(*cp == 'l') {
               gGetLiveVal = TRUE;
            }
            else if(*cp == 'C') {
               gGetChangedVal = TRUE;
            }
            else if(*cp != 'c') {
               Ret = EINVAL;
               break;
            }
            bTestCommand = TRUE;
            bGetVar = TRUE;

            if(!gGetChangedVal) {
               cp++;
               if(strncmp(cp,"0x",2) == 0) {
               // Hex argument
                  if(sscanf(&cp[2],"%x",&gVarId) != 1) {
                     bTestCommand = FALSE;
                  }
               }
               else if(isalpha(*cp)) {
               // Get by Name
                  gVarName = strdup(cp);
               }
               else if(sscanf(cp,"%d",&gVarId) != 1 || 
                       gVarId <= 0 || gVarId > 0xff) 
               {
                  bTestCommand = FALSE;
               }

               if(!bTestCommand ||
                  (gVarName == NULL && (gVarId <= 0 || gVarId > 0xff))) 
               {
                  printf("Error: Invalid variable ID '%s'.\n",cp);
                  Ret = EINVAL;
               }
            }
            break;
         }

         case 'i':
            if(sscanf(optarg,"%x",&gSimulatedDeviceID) != 1) {
               Ret = EINVAL;
               break;
            }
            gSimulated = TRUE;
            break;

         case 'I':
            if(gSimulatedDeviceDesc != NULL) {
               free(gSimulatedDeviceDesc);
            }
            gSimulatedDeviceDesc = strdup(optarg);
            gSimulated = TRUE;
            break;

#ifdef SIMULATE_JARDEN
         case 'j':
            if(sscanf(optarg,"%d",&gJardenMode) != 1) {
               printf("Error: Invalid Jarden test '%s'.\n",optarg);
               Ret = EINVAL;
               break;
            }
            if(gJardenMode == 99) {
               gDeviceOnly = TRUE;
            }
            else {
               SimJarden(gJardenMode);
               exit(0);
            }
            break;
#endif

         case 'l':
            bTestCommand = TRUE;
            bListVarAttribs = TRUE;
            break;

         case 'L':
            bTestCommand = TRUE;
            bSetLEDScript = TRUE;
            break;

         case 'm': 
            if(*optarg == 'l') {
               gGetLiveVal = TRUE;
            }
            else if(*optarg != 'c') {
               Ret = EINVAL;
               break;
            }
            bTestCommand = TRUE;
            bTestGetMultiple = TRUE;
            break;

         case 'n':
            gSendNOP = TRUE;
            break;

         case 'p':
            if(PseudoArg != NULL) {
               free(PseudoArg);
            }
            PseudoArg = strdup(optarg);
            bTestCommand = TRUE;
            break;

         case 'q':
            if(sscanf(optarg,"%d",&QueueDepth) != 1 || QueueDepth < 0) {
               printf("Error: Invalid Queue depth '%s'.\n",optarg);
               Ret = EINVAL;
               break;
            }
            bTestCommand = TRUE;
            break;

         case 's': {
            TestSetVar *pTestVal;

            if((pTestVal = ParseSetVar(optarg)) == NULL) {
               Ret = EINVAL;
               break;
            }
            else {
               if(gSetVars == NULL) {
               // First entry
                  gSetVars = pTestVal;
               }
               else {
               // find end of chain
                  TestSetVar *pLast = gSetVars;
                  while(pLast->Link != NULL) {
                     pLast = pLast->Link;
                  }
                  pLast->Link = pTestVal;
               }
               gNumSetVars++;
               bTestCommand = TRUE;
            }
            break;
         }

         case 'S':
            gDeviceOnly = TRUE;
            break;

         case 't':
            bTestCommand = TRUE;
            bGetSelfTest = TRUE;
            break;

         case 'v':
            if(*optarg == '?') {
               int i;
               printf("Verbose logging levels available:\n");
               for(i = 0; DebugMasks[i].Mask != 0; i++) {
                  printf("%08x - %s\n",DebugMasks[i].Mask,DebugMasks[i].Desc);
               }
               bExit = TRUE;
            }
            else if(sscanf(optarg,"%x",&gWASP_Verbose) != 1) {
               Ret = EINVAL;
            }
            else {
               gWASP_LogLevelSet = TRUE;
            }
            break;

         case 'V':
            bTestCommand = TRUE;
            bDisplayVersion = TRUE;
            break;

         default:
           Ret = EINVAL;
           break;
      }
   }

   if(Ret == 0 && !bTestCommand) {
      gWASP_Log = LogInit(LOG_PATH,200,200000,NULL);
   }

   LOG("waspd compiled "__DATE__ " "__TIME__"\n");

   if(Ret != 0 || (!bTestCommand && !bDaemon && !gSimulated && !bExit )) {
      Usage();
   }

   if(!bExit && Ret == 0) do {
      if(!gWASP_LogLevelSet && !bTestCommand) {
      // Try to get log level from file
         WASP_ReadLogLevel();
      }

      if(gWASP_LogLevelSet) {
         WASP_LogLogLevel(gWASP_Verbose);
      }

      if(PseudoArg != NULL) {
         Ret = PseudoCommand(PseudoArg);
      }
      else if(bTestCommand) {
         if(bDisplayVersion) {
            Ret = APITest(CMD_GET_FW_VERSION,0);
         }

         if(gNumSetVars == 1) {
            Ret = APITest(CMD_SET_VAR,0);
         }
         else if(gNumSetVars > 1) {
            Ret = APITest(CMD_SET_VARS,0);
         }
         else if(bGetVar) {
            Ret = APITest(CMD_GET_VAR,0);
         }

         if(bListVarAttribs) {
            Ret = APITest(CMD_GET_VAR_LIST,0);
         }

         if(bSetLEDScript) {
            Ret = APITest(CMD_SET_LEDSCRIPT,0);
         }

         if(bReadButtons) {
            Ret = APITest(CMD_GET_BUTTON,0);
         }

         if(bGetSelfTest) {
            Ret = APITest(CMD_GET_SELFTEST,0);
         }

         if(bTestGetMultiple) {
            Ret = APITest(CMD_GET_VARS,0);
         }

         if(bSendQuit) {
            Ret = APITest(CMD_EXIT,0);
         }

         if(QueueDepth >= 0) {
            Ret = APITest(CMD_SET_ASYNC_Q_LEN,QueueDepth);
         }
      }
      else {
         if(!gSimulated) {
            if((gSerialFd = WaspAsyncInit(DevicePath,&gAsyncCtrl)) < 0) {
               Ret = -gSerialFd;
               break;
            }
            gGenericArgs.p = gAsyncCtrl;
         }
         Ret = MainLoop();
      }
   } while(FALSE);

   FreeSetVars();
   if(gAsyncCtrl != NULL) {
      WaspAsyncShutdown(gAsyncCtrl);
   }

   for(i = 0; i < NUM_STANDARD_VARS+MAX_DEVICE_VARS; i++) {
      if(Variables[i] != NULL) {
         FreeDeviceVar(Variables[i]);
      }
   }
   FreeTimers();

   if(PseudoArg != NULL) {
      free(PseudoArg);
   }

   if(gGetChangeRequest != NULL) {
      free(gGetChangeRequest);
   }

   if(gWASP_Log != NULL) {
      LogShutdown(gWASP_Log);
   }

   return Ret;
}

int MainLoop()
{
   int Ret = 0;
   int Err;

   do {
      if(gSimulated) {
         if(gSimulatedDeviceID != -1) {
            LOG("%s: Simulating DevID 0x%x (Vid:Pid %04x:%04x).\n",__FUNCTION__,
                gSimulatedDeviceID,gSimulatedDeviceID >> 16,
                gSimulatedDeviceID & 0xffff);
            if(gSimulatedDeviceDesc != NULL) {
               LOG("%s: Simulating device description \"%s\".\n",
                   __FUNCTION__,gSimulatedDeviceDesc);
            }
         }
         else {
            LOG("%s: operating in simulated mode.\n",__FUNCTION__);
         }
      }
      else {
      // Register handler for Async data
         RegisterAsyncHandler(gAsyncCtrl,AsyncDataHandler);
      }

      if(((gIpcSocket = UdpInit(WASPD_PORT))) < 0) {
         LOG("%s: UdpInit() failed (%d)\n",__FUNCTION__,gIpcSocket);
         Ret = gIpcSocket;
         break;
      }
   // Create the standard variables
      Ret = CreateStdVars();
   } while(FALSE);

   if(Ret == 0 && !gDebug) do {
      pid_t pid;

      printf("Becoming a Daemon ...\n");

      // Fork daemon
      if((pid = fork()) < 0) {
         perror("Forking");
         Ret = errno;
         break;
      }
      else if(pid != 0) {
      // parent
         exit(0);
         break;
      }

   // must be the child
      if(setsid() == -1) {
         LOG("%s: setsid() failed: %s\n",__FUNCTION__,strerror(errno));
         Ret = errno;
         break;
      }

      if(daemon(1, 0) < 0) {
         LOG("%s: daemon(1,0) failed: %s\n",__FUNCTION__,strerror(errno));
         Ret = errno;
      }
   } while(FALSE);

   if(Ret == 0 && !gDeviceOnly && !gSimulated) {
   // Kick off the startup process
      SetStartupState(gAsyncCtrl,SEND_NOP);
   }

// Run "forever loop" until error or exit
   while(gRunMainLoop && Ret == 0) {
      fd_set ReadFdSet;
      fd_set WriteFdSet;
      struct timeval Timeout;
      int MaxFd = gSerialFd;

      FD_ZERO(&ReadFdSet);
      FD_ZERO(&WriteFdSet);

      if(!gSimulated) {
         FD_SET(gSerialFd,&ReadFdSet);
         if(gAsyncCtrl->bAsyncTxActive) {
            FD_SET(gSerialFd,&WriteFdSet);
         }
      }
      FD_SET(gIpcSocket,&ReadFdSet);

      if(gIpcSocket > gSerialFd) {
         MaxFd = gIpcSocket;
      }

      CheckTimers();

   // Set timeout
      if(gTimerHead != NULL) {
         Timeout.tv_sec = gTimerHead->When.tv_sec - gTimeNow.tv_sec;
         Timeout.tv_usec = gTimerHead->When.tv_usec - gTimeNow.tv_usec;
         if(Timeout.tv_usec < 0) {
            Timeout.tv_usec += 1000000;
            Timeout.tv_sec--;
         }

         if(Timeout.tv_sec < 0 || Timeout.tv_usec < 0) {
            printf("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
            Timeout.tv_usec = Timeout.tv_sec = 0;
         }
      }
      else {
         Timeout.tv_sec = 0;
         Timeout.tv_usec = 500000;  // 1/2 second
      }

      if((Err = select(MaxFd+1,&ReadFdSet,NULL,NULL,&Timeout)) < 0) {
         LOG("%s: select failed - %s\n",__FUNCTION__,strerror(errno));
         printf("Timeout.tv_sec: %d ,Timeout.tv_usec: %d\n",
                (int) Timeout.tv_sec,(int) Timeout.tv_usec);
         Ret = errno;
         break;
      }

      GetTimeNow();

      if(!gSimulated) {
         if(FD_ISSET(gSerialFd,&ReadFdSet)) {
            WaspAsyncReceive(gAsyncCtrl);
         }

         if(gAsyncCtrl->bAsyncTxActive && FD_ISSET(gSerialFd,&WriteFdSet)) {
            WaspAsyncTransmit(gAsyncCtrl);
         }

         if(!g_bTimeSynced) {
         // Check to see if the system clock has been set by NTP yet
            struct tm *pTm = gmtime(&gTimeNow.tv_sec);
            int Year = pTm->tm_year + 1900;

            if(Year >= 2013) {
            // Time has been set
               g_bTimeSynced = TRUE;
            }
         }

         if(g_bTimeSynced && !g_bClockTimeSet && gWaspdState == READY) {
            g_bClockTimeSet = TRUE;
            if(gClientVersion >= 106) {
               SendClockTime(&gGenericArgs);
            }
            else {
               LOG("%s: Not setting WASP_VAR_CLOCK_TIME, "
                   "client version is %d.%02d\n",__FUNCTION__,
                   gClientVersion/100,gClientVersion%100);
            }
         }

         if(FD_ISSET(gIpcSocket,&ReadFdSet)) {
            IpcRead(gIpcSocket);
         }
      // check timers
         CheckTimers();
         WaspAsyncPoll(gAsyncCtrl);
      }
      else {
      // Simulated device, just read IPC
         if(FD_ISSET(gIpcSocket,&ReadFdSet)) {
            IpcRead(gIpcSocket);
         }
      }

#ifdef OLD_API
//      _CheckCookTime();
#endif
#ifdef SIMULATE_JARDEN
      SimJardenTxIsr();
#endif
   }

   if(gSerialFd >= 0) {
      close(gSerialFd);
   }

   if(gIpcSocket >= 0) {
      close(gIpcSocket);
   }

   if(DevicePath != NULL) {
      free(DevicePath);
   }

   if(gDeviceWaspVersion != NULL) {
      free(gDeviceWaspVersion);
   }
   return Ret;
}

// Initialize communications on UDP port <port>.  
// Enables messages to be received on the specified port from other
// applications. This function must be called once and only once for
// each port the application wishes to receive message on.
// return == 0: Ok, otherwise error code
int UdpInit(u16 Port)
{
   int Ret = 0;   // assume the best
   AdrUnion MyAdr;
   int On = 1;
   int Socket = -1;
   int Val;
   socklen_t ValLen = sizeof(Val);

   do {
      if((Socket = socket(AF_INET,SOCK_DGRAM,0)) == -1) {
         Ret = -errno;
         LOG("%s: socket() failed, %s\n",__FUNCTION__,strerror(Ret));
         break;
      }

      if(setsockopt(Socket,SOL_SOCKET,SO_REUSEADDR,(char *) &On,
                    sizeof(On)) == -1) 
      {
         LOG("%s: Warning - setsockopt() failed: %s\n",__FUNCTION__,
                 strerror(errno));
      }

      MyAdr.i.sin_family = AF_INET;
      MyAdr.PORT = htons(Port);
      MyAdr.ADDR = inet_addr("127.0.0.1");   // localhost

      if(bind(Socket,&MyAdr.s,sizeof(MyAdr)) == -1) {
         Ret = -errno;
         LOG("%s: bind() failed for %s:%d %s\n",__FUNCTION__,
                 inet_ntoa(MyAdr.i.sin_addr),Port,strerror(errno));
         break;
      }

      if(getsockopt(Socket,SOL_SOCKET,SO_RCVBUF,&Val,&ValLen) == -1) {
         LOG("%s: Warning - getsockopt() failed: %s\n",__FUNCTION__,
                 strerror(errno));
      }
   } while(FALSE);

   if(Ret == 0) {
      Ret = Socket;
   }
   else {
      if(Socket != -1) {
         close(Socket);
      }
   }

   return Ret;
}

void IpcRead(int fd)
{
   WaspApiMsg Request;
   int BytesRead;
   socklen_t Len = sizeof(gHisAdr);
   int DataLen = -1;
   int Err;
   int BytesAvailable;
   WaspApiMsg *pReply = &gReply;

   memset(&gReply,0,sizeof(gReply));

   if((Err = ioctl(fd,FIONREAD,&BytesAvailable)) < 0) {
      printf("ioctl failed\n");
   }
   else if(gDebug && gWASP_Verbose){
      // printf("%s: %d bytes available\n",__FUNCTION__,BytesAvailable);
   }

   BytesRead = recvfrom(fd,&Request,sizeof(Request),0,&gHisAdr.s,&Len);
   if(BytesRead <= 0) {
      LOG("%s: read failed: %d (%s)\n",__FUNCTION__,errno,strerror(errno));
   }
   else {
   // We read something
      if(gWASP_Verbose & (LOG_API_CALLS | LOG_V1)) {
         LOG("%s: Received API Cmd 0x%x\n",__FUNCTION__,Request.Hdr.Cmd);
      }
      if(gWASP_Verbose & LOG_V1) {
         DumpHex(&Request,BytesRead);
      }
      gReply.Hdr.Err = WASP_OK;

      switch(Request.Hdr.Cmd) {
         case CMD_EXIT:
            if(gPseudoVarChanged) {
               gPseudoVarChanged = FALSE;
               if(gWASP_Verbose & LOG_PSEUDO) {
                  LOG("%s: Calling SavePseudoVars()\n",__FUNCTION__);
               }
               SavePseudoVars();
            }
            gRunMainLoop = FALSE;
            DataLen = 0;
            LOG("%s: Received CMD_EXIT, exiting...\n",__FUNCTION__);
            break;

         case CMD_GET_FW_VERSION:
            if(gWaspdState != READY) {
               DataLen = 0;
               gReply.Hdr.Err = WASP_ERR_NOT_READY;
            }
            else {
               WaspVariable *pVar = FindVariable(WASP_VAR_FW_VER);

               if(pVar != NULL) {
                  memset(gReply.u.Version,0,sizeof(gReply.u.Version));
                  strncpy(gReply.u.Version,pVar->Val.String,
                          sizeof(gReply.u.Version)-1);
                  DataLen = strlen(gReply.u.Version);
               }
               else {
               // Maybe the device refused to supply a version ?
                  DataLen = 0;
                  gReply.Hdr.Err = WASP_ERR_NO_DATA;
               }
            }
            break;

         case CMD_GET_VAR: {
            u8 VarID;
            const char *Name = Request.u.VarRef.Name;
            DeviceVar *pDevVar;

            if(Name[0] != 0) {
               Request.u.VarRef.WaspVar.ID = FindVariableByName(Name);
            }
            VarID = Request.u.VarRef.WaspVar.ID;

            if(gWASP_Verbose & LOG_API_CALLS) {
               LOG("%s: WASP_GetVariable() VarID 0x%x\n",__FUNCTION__,VarID);
            }

            DataLen = 0;   // Assume the worse
         // Check if we're emulating a DeviceId
            if(gSimulatedDeviceID != -1 && 
               Request.u.VarRef.WaspVar.ID == WASP_VAR_DEVID) 
            {
               WaspVariable Dummy;

               Dummy.ID = WASP_VAR_DEVID;
               Dummy.Type = WASP_VARTYPE_UINT32;
               Dummy.State = VAR_VALUE_CACHED;
               Dummy.Val.U32 = gSimulatedDeviceID;
               DataLen = Var2Ipc(&Dummy,&pReply);
            }
            else if(gSimulatedDeviceDesc != NULL && 
               Request.u.VarRef.WaspVar.ID == WASP_VAR_DEV_DESC) 
            {
               WaspVariable Dummy;

               Dummy.ID = WASP_VAR_DEV_DESC;
               Dummy.Type = WASP_VARTYPE_STRING;
               Dummy.State = VAR_VALUE_CACHED;
               Dummy.Val.String = gSimulatedDeviceDesc;
               DataLen = Var2Ipc(&Dummy,&pReply);
            }
            else if(gWaspdState != READY) {
               gReply.Hdr.Err = WASP_ERR_NOT_READY;
            }
            else if((pDevVar = FindDevVar(VarID)) == NULL)  {
            // Unknown variable
               gReply.Hdr.Err = WASP_ERR_INVALID_VARIABLE;
            }
            else if(Request.u.WaspVar.State == VAR_VALUE_LIVE) {
               if(pDevVar->Desc.Usage & WASP_USAGE_PSEUDO) {
               // Can't read a Pseudo variable from the interface !
                  gReply.Hdr.Err = WASP_ERR_NOT_SUPPORTED;
               }
               if(AsyncApiRequest(&Request,BytesRead) == 0) {
                  DataLen = -1;  // Nothing to return yet
               }
            }
            else if(pDevVar->Desc.PollRate == WASP_ASYNC_UPDATE && gQueueLen > 0) 
            {  // Special handling for reading cached ASYNC variables 
               // after we've enabled ASYNC updates.
               VarLink *pVars = pDevVar->pVars;
               WaspVariable *pVar = pVars->pVar;

               if(pVar->State == VAR_VALUE_UNKNOWN) {
                  DataLen = 0;
                  gReply.Hdr.Err = WASP_ERR_NO_DATA;
                  break;
               }
            // Return cached value
               DataLen = Var2Ipc(pVar,&pReply);
            // Consume the data
               pVar->State = VAR_VALUE_UNKNOWN;
               WASP_FreeValue(pVar);

               if(pDevVar->NumValues > 1) {
                  pDevVar->NumValues--;
                  pDevVar->pVars = pVars->Link;
                  free(pVars->pVar);
                  free(pVars);
               }
            }
            else if(pDevVar->pVars->pVar->State == VAR_VALUE_UNKNOWN) {
               DataLen = 0;
               gReply.Hdr.Err = WASP_ERR_NO_DATA;
            }
            else {
            // Return cached value
               DataLen = Var2Ipc(pDevVar->pVars->pVar,&pReply);
            }
            break;
         }

         case CMD_GET_VARS: {
            WaspVariable *pVar;
            int Count = Request.u.GetVars.Count;
            VarID *ID = Request.u.GetVars.IDs;
            int bLive = Request.u.GetVars.bLive;
            int i;

            DataLen = 0;   // Assume the worse
            do {
               if(gWaspdState != READY) {
                  gReply.Hdr.Err = WASP_ERR_NOT_READY;
                  break;
               }
            // Check the variable IDs
               for(i = 0; i < Count; i++) {
                  if((pVar = FindVariable(ID[i])) == NULL)  {
                  // Unknown variable
                     gReply.Hdr.Err = WASP_ERR_INVALID_VARIABLE;
                     break;
                  }
                  if(!bLive && pVar->State == VAR_VALUE_UNKNOWN) {
                  // Must get value from interface
                     DataLen = 0;
                     gReply.Hdr.Err = WASP_ERR_NO_DATA;
                     break;
                  }
               }

               if(gReply.Hdr.Err != WASP_OK) {
                  break;
               }

               if(bLive) {
                  if(AsyncApiRequest(&Request,BytesRead) == 0) {
                     DataLen = -1;  // Nothing to return yet
                  }
               }
               else {
               // Return cached value
                  DataLen = Vars2Ipc(&Request.u.GetVars,&pReply);
               }
            } while(FALSE);
            break;
         }

         case CMD_SET_VAR: {
            u8 VarID;
            const char *Name = Request.u.VarRef.Name;
            WaspVariable *pVar;
            u8 *pVal = &Request.u.WaspVar.Val.U8;

            do {
               if(Name[0] != 0) {
                  Request.u.VarRef.WaspVar.ID = FindVariableByName(Name);
               }
               VarID = Request.u.VarRef.WaspVar.ID;

               DataLen = 0;   // Assume the worse
               if(gWaspdState != READY) {
                  gReply.Hdr.Err = WASP_ERR_NOT_READY;
                  break;
               }

               if((pVar = FindVariable(VarID)) == NULL)  {
               // Unknown variable
                  gReply.Hdr.Err = WASP_ERR_INVALID_VARIABLE;
                  break;
               }

               if(pVar->Type == WASP_VARTYPE_STRING ||
                  pVar->Type == WASP_VARTYPE_BLOB)
               {
                  gReply.Hdr.Err = WASP_ERR_NOT_SUPPORTED;
                  break;
               }

               if(pVar->Type != Request.u.WaspVar.Type)  {
                  gReply.Hdr.Err = WASP_ERR_WRONG_TYPE;
                  break;
               }

               if(!pVar->Pseudo.bPseudoVar) {
                  WriteVarValue(&pVal,0,pVar);
                  pVar->State = VAR_VALUE_SETTING;
                  if(AsyncApiRequest(&Request,BytesRead) == 0) {
                     DataLen = -1;  // Nothing to return yet
                  }
               }
               else {
                  WaspVariable *pRestore = &Request.u.WaspVar;
                  if(!pVar->Pseudo.bInited || pVar->Pseudo.TimeStamp == 0) {
                  // The variable value and timestamp hasn't been set yet
                     WriteVarValue(&pVal,0,pVar);
                     pVar->Pseudo.TimeStamp = pRestore->Pseudo.TimeStamp;
                     pVar->Pseudo.bInited = pRestore->Pseudo.bInRange;
                  }

                  if(!pVar->Pseudo.bRestored) {
                  // The variable hasn't been restore yet
                     pVar->Pseudo.bRestored = TRUE;
                     pVar->Pseudo.Count += pRestore->Pseudo.Count;
                  }
                  pVar->State = VAR_VALUE_CACHED;
               }
            } while(FALSE);
            break;
         }

         case CMD_SET_VARS: {
            int Count = Request.u.IntData;
            void *p = (void *) &Request.u.IntData + sizeof(Request.u.IntData);
            WaspVariable *pVar1 = (WaspVariable *) p;
            WaspVariable *pVar = (WaspVariable *) p;
            int i;
            u8 *cp;

            do {
               DataLen = 0;   // Assume the worse
               if(gWaspdState != READY) {
                  gReply.Hdr.Err = WASP_ERR_NOT_READY;
                  break;
               }

            // Check that all of the variables are valid and supporte
            // and of the correct type
               for(i = 0; i < Count; i++) {
                  if((pVar = FindVariable(pVar1->ID)) == NULL)  {
                  // Unknown variable
                     gReply.Hdr.Err = WASP_ERR_INVALID_VARIABLE;
                     break;
                  }

                  if(pVar->Type != pVar1->Type)  {
                     gReply.Hdr.Err = WASP_ERR_WRONG_TYPE;
                     break;
                  }
                  p += WASP_GetVarLen(pVar1);
                  pVar1 = (WaspVariable *) p;
               }
               if(gReply.Hdr.Err != WASP_OK) {
                  break;
               }

               p = (void *) &Request.u.IntData + sizeof(Request.u.IntData);
               pVar1 = (WaspVariable *) p;

               for(i = 0; i < Count; i++) {
                  pVar = FindVariable(pVar1->ID);
                  switch(pVar->Type) {
                     case WASP_VARTYPE_STRING:
                        cp = &pVar1->Val.U8;
                        WriteVarValue(&cp,strlen((char *) cp),pVar);
                        break;

                     case WASP_VARTYPE_BLOB:
                        cp = (u8 *) &pVar1->Val.Blob.Data;
                        WriteVarValue(&cp,pVar1->Val.Blob.Len,pVar);
                        break;

                     default:
                        cp = &pVar1->Val.U8;
                        WriteVarValue(&cp,0,pVar);
                        break;
                  }
                  pVar->State = VAR_VALUE_SETTING;

                  p += WASP_GetVarLen(pVar);
                  pVar1 = (WaspVariable *) p;
               }

               if(AsyncApiRequest(&Request,BytesRead) == 0) {
                  DataLen = -1;  // Nothing to return yet
               }
            } while(FALSE);
            break;
         }

         case CMD_GET_VAR_LIST:
            DataLen = 0;   // Assume the worse
            if(gWaspdState != READY) {
               gReply.Hdr.Err = WASP_ERR_NOT_READY;
            }
            else {
               DataLen = VarList2Ipc(&pReply);
            }
            break;

         case CMD_SET_ASYNC_Q_LEN:
            if(Request.u.IntData < 0) {
            // Say what?
               gReply.Hdr.Err = WASP_ERR_ARG;
               break;
            }

            if(gQueueLen == Request.u.IntData) {
            // Nothing to do, just return WASP_OK
               DataLen = 0;
               break;
            }
         // Intenional fall though

         case CMD_GET_SELFTEST:
         case CMD_SET_LEDSCRIPT: {
            DataLen = 0;   // Assume the worse
            if(gWaspdState != READY) {
               gReply.Hdr.Err = WASP_ERR_NOT_READY;
            }
            else if(AsyncApiRequest(&Request,BytesRead) == 0) {
               DataLen = -1;  // Nothing to return yet
            }
            break;
         }

         case CMD_GET_BUTTON: {
         // Return data from button ring buffer.
            u8 Button = 0;
            if(gWaspdState != READY) {
               DataLen = 0;
               gReply.Hdr.Err = WASP_ERR_NOT_READY;
            }
            else {
               DataLen = 1;
               if(gButtonRing.Rd != gButtonRing.Wr) {
                  Button = gButtonRing.Buf[gButtonRing.Rd++];
                  if(gButtonRing.Rd >= BUTTON_BUF_LEN) {
                     gButtonRing.Rd = 0;
                  }
               }
               gReply.u.U8 = Button;
            }
            break;
         }

#if 0
         case CMD_SET_COOKTIME: {
            _SetCookTime(Request.u.IntData);
            break;
         }

         case CMD_GET_COOKTIME:
            pReply->u.IntData = _GetCookTime();
            DataLen = sizeof(pReply->u.IntData);
            break;
#endif

         case CMD_GET_CHANGED_VAR:
            DataLen = 0;   // Assume the worse
            if(gWaspdState != READY) {
               gReply.Hdr.Err = WASP_ERR_NOT_READY;
            }
            else {
               AsyncReq *p = gGetChangeRequest;
               WaspApiMsg Msg;

               if(p != NULL) {
               // Only one caller can be waiting for changes at a time!
               // Send an error to the *OLD* waiter.  There's good chance 
               // that the old waiter isn't there anymore because of a 
               // crash or restart.

                  Msg.Hdr.Err = WASP_ERR_WAIT_BUSY;
                  Msg.Hdr.Cmd = CMD_GET_CHANGED_VAR | CMD_RESPONSE;
                  UdpSend(gIpcSocket,&p->HisAdr,&Msg,sizeof(Msg));
                  free(gGetChangeRequest);
                  gGetChangeRequest = NULL;
               }

               if(AsyncApiRequest(&Request,BytesRead) == 0) {
                  DataLen = -1;  // Nothing to return yet
               }
            }
            break;

         default:
            LOG("%s: Invalid request 0x%x ignored.\n",__FUNCTION__,
                Request.Hdr.Cmd);
            break;
      }

      if(DataLen >= 0) {
         DataLen += offsetof(WaspApiMsg,u);
         pReply->Hdr.Cmd = Request.Hdr.Cmd | CMD_RESPONSE;
         UdpSend(fd,&gHisAdr,pReply,DataLen);
      }
   }

   if(pReply != &gReply && pReply != NULL) {
      free(pReply);
   }
}

int UdpSend(int fd,AdrUnion *pToAdr,WaspApiMsg *pMsg,int MsgLen)
{
   int Ret = 0;   // assume the best
   int BytesSent;

   if(gWASP_Verbose & LOG_IPC) {
      LOG("%s: Sending %d bytes:\n",__FUNCTION__,MsgLen);
      DumpHex(pMsg,MsgLen);
   }
   BytesSent = sendto(fd,pMsg,MsgLen,0,&pToAdr->s,sizeof(pToAdr->s));
   if(BytesSent != MsgLen) {
      LOG("%s: sendto failed - %s\n",__FUNCTION__,strerror(errno));
      Ret = -errno;
   }

   return Ret;
}

int APITest(int Type,int Arg)
{
   int Ret = 0;
   int bHalInitialized = FALSE;

   do {
      if((Ret = WASP_Init()) != 0) {
         printf("%s: WASP_Init() failed, %s\n",__FUNCTION__,WASP_strerror(Ret));
         break;
      }
      bHalInitialized = TRUE;

      switch(Type) {
         case CMD_GET_FW_VERSION: {
            char *Version = NULL;
            if((Ret = WASP_GetFWVersionNumber(&Version)) != 0) {
               printf("%s: WASP_GetFWVersionNumber() failed: %s\n",__FUNCTION__,
                      WASP_strerror(Ret));
            }
            else {
               printf("WASP_GetFWVersionNumber() returned '%s'\n",
                      Version);
               free(Version);
            }
            break;
         }

         case CMD_GET_VAR: {
            WaspVariable Var;
            memset(&Var,0,sizeof(Var));
            Var.ID = gVarId;
            Var.State = gGetLiveVal ? VAR_VALUE_LIVE : VAR_VALUE_CACHED;
            const char *ApiFunction = NULL;

            if(gGetChangedVal) {
               Ret = WASP_GetChangedVar(&Var);
               ApiFunction = "WASP_GetChangedVar";
            }
            else if(gVarName != NULL) {
               Ret = WASP_GetVarByName(gVarName,&Var);
               ApiFunction = "WASP_GetVarByName";
            }
            else {
               Ret = WASP_GetVariable(&Var);
               ApiFunction = "WASP_GetVariable";
            }

            if(Ret != 0) {
               printf("%s: %s failed: %s\n",__FUNCTION__,ApiFunction,
                      WASP_strerror(Ret));
            }
            else {
               WASP_DumpVar(&Var);
               WASP_FreeValue(&Var);
            }
            break;
         }

         case CMD_GET_VARS: {
            WaspVariable DevId;
            WaspVariable DevDesc;
            WaspVariable Temp;
            WaspVariable Mode;

            memset(&DevId,0,sizeof(WaspVariable));
            memset(&DevDesc,0,sizeof(WaspVariable));
            memset(&Temp,0,sizeof(WaspVariable));
            memset(&Mode,0,sizeof(WaspVariable));
            WaspVariable *Array[4] = {&DevId,&DevDesc,&Temp,&Mode}; 
            DevId.ID = WASP_VAR_DEVID;
            DevDesc.ID = WASP_VAR_DEV_DESC;
            Temp.ID = 0x81;
            Mode.ID = 0x80;
            Ret = WASP_GetVariables(4,gGetLiveVal,Array);

            if(Ret != 0) {
               printf("%s: WASP_GetVariables failed: %s\n",__FUNCTION__,
                      WASP_strerror(Ret));
            }
            else {
               WASP_DumpVar(&Temp);
               WASP_FreeValue(&Temp);
               printf("\n");
               WASP_DumpVar(&Mode);
               WASP_FreeValue(&Mode);
               printf("\n");
               WASP_DumpVar(&DevId);
               WASP_FreeValue(&DevId);
               printf("\n");
               WASP_DumpVar(&DevDesc);
               WASP_FreeValue(&DevDesc);
            }
            break;
         }

         case CMD_SET_VAR: {
         // Set one variable
            WaspVariable Var = gSetVars->Var;

            if(gSetVars->VarName != NULL) {
               printf("%s: Requesting set for %s\n",__FUNCTION__,
                  gSetVars->VarName);
               if((Ret = WASP_SetVarByName(gSetVars->VarName,&Var)) != 0) {
                  printf("%s: WASP_SetVarByName() failed: %s\n",__FUNCTION__,
                         WASP_strerror(Ret));
               }
            }
            else {
               if((Ret = WASP_SetVariable(&Var)) != 0) {
                  printf("%s: WASP_SetVariable() failed: %s\n",__FUNCTION__,
                         WASP_strerror(Ret));
               }
            }
            break;
         }

         case CMD_SET_VARS: {
         // Set multiple variables
            TestSetVar *p = gSetVars;
            int i;
            WaspVariable *pVarArray = malloc(sizeof(WaspVariable) * gNumSetVars);

            if(pVarArray == NULL) {
               printf("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
               break;
            }

            for(i = 0; i < gNumSetVars; i++) {
               memcpy(&pVarArray[i],&p->Var,sizeof(WaspVariable));
               p = p->Link;
            }

            if((Ret = WASP_SetVariables(gNumSetVars,pVarArray)) != 0) {
               printf("%s: WASP_SetVariables() failed: %s\n",__FUNCTION__,
                      WASP_strerror(Ret));
            }
            free(pVarArray);
            break;
         }


         case CMD_GET_VAR_LIST: {
            int Count;
            WaspVarDesc **pVarDescArray = NULL;

            if((Ret = WASP_GetVarList(&Count,&pVarDescArray)) != 0) {
               printf("%s: WASP_GetVarList() failed: %s\n",__FUNCTION__,
                      WASP_strerror(Ret));
            }
            else {
               int i;
               for(i = 0; i < Count; i++) {
                  DumpVarAttributes(pVarDescArray[i]);
               }
               free(pVarDescArray);
            }
            break;
         }

         case CMD_SET_LEDSCRIPT: {
            if((Ret = WASP_SetLEDScript(0x12,LedTestScript)) != 0) {
               printf("%s: WASP_SetLEDScript() failed: %s\n",__FUNCTION__,
                      WASP_strerror(Ret));
            }
            break;
         }

         case CMD_GET_BUTTON: {
            u8 Button;
            u8 bPressed;

            if((Ret = WASP_GetButton(&Button)) != 0) {
               printf("%s: WASP_GetButton() failed: %s\n",__FUNCTION__,
                      WASP_strerror(Ret));
            }
            else if(Button){
               bPressed = Button & WASP_BUTTON_PRESSED;
               Button &= ~WASP_BUTTON_PRESSED;

               printf("Button ID %d was %s\n",Button,
                      bPressed ? "pressed" : "released");
            }
            else {
               printf("No buttons have been pressed or released\n");
            }
            break;
         }

         case CMD_GET_SELFTEST: {
            SelfTestResults *pResults;

            if((Ret = WASP_GetSelfTestResults(&pResults)) != 0) {
               printf("%s: WASP_GetSelfTestResults() failed: %s\n",
                      __FUNCTION__,WASP_strerror(Ret));
            }
            else {
               printf("Self test result: %d\n",pResults->Result);
               if(pResults->Len > 0) {
                  LOG("%d bytes of additional data:\n",pResults->Len);
                  DumpHex(pResults->AdditionData,pResults->Len);
               }
               free(pResults);
            }
            break;
         }

         case CMD_EXIT:
            if((Ret = WASP_Shutdown()) != 0) {
               printf("%s: WASP_Shutdown() failed: %s\n",__FUNCTION__,
                      WASP_strerror(Ret));
               break;
            }
            printf("\nDaemon Exiting\n");
            break;

         case CMD_SET_ASYNC_Q_LEN:
            if((Ret = WASP_SetAsyncDataQueueDepth(Arg)) != 0) {
               printf("%s: WASP_SetAsyncDataQueueDepth() failed: %s\n",
                      __FUNCTION__,WASP_strerror(Ret));
               break;
            }
            break;

         default:
            printf("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
            Ret = WASP_ERR_WASPD_INTERNAL;
            break;
      }
   } while(FALSE);

   if(bHalInitialized && (Ret = WASP_Cleanup()) != 0) {
      printf("%s: WASP_Cleanup() failed, %s\n",__FUNCTION__,WASP_strerror(Ret));
   }

   return Ret;
}

void GetTimeNow()
{
   struct timezone tz;
#ifdef GEMTEK_SDK
   time_t GmtTime;
   struct timeval TimeNow;

// ARG!!!  When running under the Gemtek SDK the system time
// is set to LOCAL time, so none of the standard POSIX time calls
// work correctly unless you just happen to live in Greenwich !!
// Use gettimeofday() to get the microseconds portion of the time
// and GetUTCTime() to get the seconds portion of the time
   do {
   // Loop until we get the same seconds value twice to
   // avoid inconsistancy between gTimeNow.tv_sec and gTimeNow.tv_usec
      gettimeofday(&TimeNow,&tz);
      GmtTime = (time_t) GetUTCTime();
      gettimeofday(&gTimeNow,&tz);
   } while(gTimeNow.tv_sec != TimeNow.tv_sec);
   gTimeNow.tv_sec = GmtTime;
#else
   gettimeofday(&gTimeNow,&tz);
#endif
}

// Set a timeout for <Timeout> milliseconds from now
void SetTimeout(struct timeval *p,int Timeout)
{
   long microseconds;

   GetTimeNow();
   microseconds = gTimeNow.tv_usec + ((Timeout % 1000) * 1000);

   p->tv_usec = microseconds % 1000000;
   p->tv_sec  = gTimeNow.tv_sec + (Timeout / 1000) + microseconds / 1000000;
}

// Return number of milliseconds of time since p
int TimeLapse(struct timeval *p)
{
   int DeltaSeconds = gTimeNow.tv_sec - p->tv_sec;
   int Ret;

   if(DeltaSeconds > (0x7fffffff / 1000)) {
   // TimeLapse is too big to express in milliseconds, just return 
   // the max we can.
      return 0x7fffffff;
   }
   Ret = (DeltaSeconds * 1000) + 
          ((gTimeNow.tv_usec - p->tv_usec) / 1000);

   return Ret;
}

int SetTimerCallback(TimerCallback Callback,int Timeout,void *Data)
{
   int Ret = 0;   // assume the best
   Timer *pTimer;
   Timer *p = gTimerHead;
   Timer *pLast = (Timer *) &gTimerHead;

   do {
      if((pTimer = (Timer *) malloc(sizeof(Timer))) == NULL) {
         LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
         Ret = ENOMEM;
         break;
      }
      pTimer->Link = 0;
      pTimer->pCallBack = Callback;
      pTimer->pData = Data;
      SetTimeout(&pTimer->When,Timeout);

   // Link new timer into chain
      for( ; ; ) {
         if(p == NULL) {
         // Found our place in line
            pLast->Link = pTimer;
            break;
         }
         if(p->When.tv_sec > pTimer->When.tv_sec ||
            (p->When.tv_sec == pTimer->When.tv_sec &&
             p->When.tv_usec > pTimer->When.tv_usec))
         {
         // Found our place in line
            pTimer->Link = p;
            pLast->Link = pTimer;
            break;
         }
         pLast = p;
         p = p->Link;
      }
   } while(FALSE);

   return Ret;
}

int CancelTimerCallback(TimerCallback pCallback,void *pData)
{
   int Ret = 0;   // assume the best
   Timer *p = gTimerHead;
   Timer *pLast = (Timer *) &gTimerHead;

// Find the timer
   for( ; ; ) {
      if(p->pCallBack == pCallback && p->pData == pData) {
      // This the droid we're looking for
         pLast = p->Link;
         free(p);
         break;
      }
      pLast = p;
      p = p->Link;
      if(p == NULL) {
      // Timer not found
         Ret = 1;
         break;
      }
   }

   return Ret;
}

void WaspDispatchCommand(HandlerArgs *p)
{
   u8 Cmd = *p->Msg;
   int Err = WASP_ERR_CMD;   // Assume the worse
   int i;

   for(i = 0; CmdTable[i].pCmdHandler != NULL; i++) {
      if(Cmd == CmdTable[i].Cmd) {
         Err = CmdTable[i].pCmdHandler(p);
         break;
      }
   }

   if(Err >= 0) {
      u8 Resp[2];
      Resp[0] = Cmd | WASP_CMD_RESP;
      Resp[1] = (u8) Err;
      WaspAsyncSendResp(p->p,Resp,2);
   }
}

int CmdNopHandler(HandlerArgs *p)
{
   return WASP_OK;
}

// We've received a NOP response
int StartNopHandler(HandlerArgs *p)
{
   if(p->Msg[1] != WASP_OK) {
      LOG("%s: device returned 0x%x for WASP_CMD_NOP.\n",
          __FUNCTION__,p->Msg[1]);
   }
   else {
      LOG("%s: NOP response received sucessfully @ %u baud\n",
          __FUNCTION__,p->p->Baudrate);

      if(!gSendNOP) {
         SetStartupState(p->p,NEXT_STATE);
      }
   }

   return 0;
}

int StartValHandler(HandlerArgs *p)
{
   int MsgLen = p->MsgLen;
   int MajorVer;
   int MinorVer;

   if(p->Msg[1] != WASP_OK) {
      LOG("%s: device returned 0x%x when reading WASP_VAR_WASP_VER.\n",
          __FUNCTION__,p->Msg[1]);
   }
   else {
      switch(gWaspdState) {
         case GET_DEVICE_WASP_VER:
         // save variable
            if(gDeviceWaspVersion != NULL) {
               free(gDeviceWaspVersion);
            }
            if((gDeviceWaspVersion = malloc(MsgLen-1)) != NULL) {
               memcpy(gDeviceWaspVersion,&p->Msg[2],MsgLen-2);
               gDeviceWaspVersion[MsgLen-2] = 0;
               if(gWASP_Verbose & LOG_V) {
                  LOG("%s: GET_DEVICE_WASP_VER: '%s'\n",__FUNCTION__,
                      gDeviceWaspVersion);
               }
               if(sscanf(gDeviceWaspVersion,"%d.%d",&MajorVer,&MinorVer) != 2 ||
                  MajorVer <= 0 || MinorVer <= 0 || MinorVer > 99)
               {
                  LOG("%s: Error, unable to parse WASP_VER: '%s'\n",
                     __FUNCTION__,gDeviceWaspVersion);
               }
               else {
                  gClientVersion = MajorVer * 100 + MinorVer;
               }
            }
            SetStartupState(p->p,NEXT_STATE);
            break;

         default:
            break;
      }
   }


   return 0;
}

int StartBaudRateHandler(HandlerArgs *p)
{
   int MsgLen = p->MsgLen;
   u32 *pBaudRate;
   int i;
   WaspState NextState = GET_DEVICE_VARS;

   do {
      if(p->Msg[1] != WASP_OK) {
         LOG("%s: device returned 0x%x for WASP_CMD_GET_BAUDRATES.\n",
             __FUNCTION__,p->Msg[1]);
         break;
      }

      if(MsgLen < 6) {
         LOG("%s: Error, invalid message Len %d, 6 bytes is the minimum.\n",
             __FUNCTION__,MsgLen);
         break;
      }
   // Search for a common baudrate
      for(i = 2; i < MsgLen; i += 4) {
         pBaudRate = (u32 *) &p->Msg[i];
      // For the moment only check for 57600.  Eventually add a list of
      // baudrates supported by the hardware and look for the highest match
         if(gWASP_Verbose & LOG_V) {
            LOG("%s: device supports %u baud rate.\n",__FUNCTION__,*pBaudRate);
         }
         if(*pBaudRate == 57600) {
         // Set the baudrate to 57600.
            gBestBaudRate = *pBaudRate;
            NextState = SET_NEW_BAUDRATE;
         }
      }
   } while(FALSE);

   SetStartupState(p->p,NextState);

   return 0;
}


int StartVarDescHandler(HandlerArgs *p)
{
   u8 Cmd[2];
   u8 *cp;
   WaspVariable *pVar;
   int bNextVar = TRUE;
   int Ret = 0;

   do {
      int MinMaxLen;
      int NameLen;
      int DescLen = sizeof(WaspVarDesc);
      VariableAttribute *pAttrib = (VariableAttribute *) &p->Msg[2];
      WaspVarDesc *pDesc = NULL;
      DeviceVar *pDevVar = NULL;
      int MinMsgLen;
      int i;

      if(p->Msg[1] != WASP_OK) {
         bNextVar = FALSE;
         if(p->Msg[1] != WASP_ERR_ARG) {
            LOG("%s: Device returned 0x%x for WASP_CMD_GET_VAR_ATTRIBS, "
                "variable 0x%x.\n",__FUNCTION__,p->Msg[1],gDevVar);
         }
         break;
      }

   // First cut at Minimum length, we don't the MinMaxLen yet
      MinMsgLen = 2 + sizeof(VariableAttribute) -
                      sizeof(pAttrib->MinMax) +
                      sizeof(pAttrib->MinMax.U8) - MAX_VAR_NAME_LEN + 1;

      if(p->MsgLen < MinMsgLen) {
         LOG("%s: Invalid response length of %d bytes, mininum is %d\n",
             __FUNCTION__,p->MsgLen,MinMsgLen);
         Ret = -EINVAL;
         break;
      }

      if(pAttrib->Type > NUM_VAR_TYPES) {
         LOG("%s: Invalid variable type 0x%x\n",__FUNCTION__,pAttrib->Type);
         Ret = -EINVAL;
         break;
      }

   // Calculate the size of the MinValue/MaxValue value
      MinMaxLen = VarType2NameOffset[pAttrib->Type] - 
                  offsetof(VariableAttribute,MinMax);

      MinMsgLen = 2 + sizeof(VariableAttribute) - 
                  sizeof(pAttrib->MinMax) + MinMaxLen;

      if(p->MsgLen < MinMsgLen) {
         LOG("%s: Invalid response length of %d bytes, should be %d\n",
             __FUNCTION__,p->MsgLen,MinMsgLen);
         Ret = -EINVAL;
         break;
      }

      if(pAttrib->Type > NUM_VAR_TYPES) {
         LOG("%s: Device returned invalid variable type %d for "
             "VarID 0x%x, variable ignored.\n",__FUNCTION__,
            pAttrib->Type,gDevVar);
         break;
      }

      if(IS_TYPE_ENUM(pAttrib)) {
      // Adjust allocation size for EnumNames
         DescLen += (pAttrib->MinMax.U8.Max + 1) * sizeof(char *);
      }
   // Create the variable and description
      if((pDevVar = CreateDeviceVar(DescLen)) == NULL) {
         LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
         Ret = -ENOMEM;
         break;
      }

      pVar = pDevVar->pVars->pVar;
      pDesc = &pDevVar->Desc;
      memset(pDesc,0,DescLen);

      pDesc->ID = gDevVar;
      pDesc->Type = pAttrib->Type;
      pDesc->Usage = pAttrib->Usage;
      if(pDesc->Usage & WASP_USAGE_PSEUDO) {
         gPseudoVars++;
         pVar->Pseudo.bPseudoVar = TRUE;
      // Set state to cached to it can be read before it's created by
      // a matched event
         pVar->State = VAR_VALUE_CACHED;
      }
      pDesc->PollRate = pAttrib->PollRate;
      if(pDesc->PollRate == WASP_ASYNC_UPDATE) {
         gAsyncVars++;
      }

   // Calculate the size of the MinValue/MaxValue value
      MinMaxLen = VarType2NameOffset[pDesc->Type] - 
                  offsetof(VariableAttribute,MinMax);
      MinMaxLen /= 2;
      cp = (u8 *) &pAttrib->MinMax;

   // Copy the MinValue
      memcpy(&pDesc->MinValue,cp,MinMaxLen);
      cp += MinMaxLen;
      memcpy(&pDesc->MaxValue,cp,MinMaxLen);
      cp += MinMaxLen;
   // copy the variable name
      NameLen = &p->Msg[p->MsgLen] - cp;
      memcpy(&pDesc->Name,cp,NameLen);
   // Null terminate the variable name
      pDesc->Name[NameLen] = 0;

      if(gWASP_Verbose & LOG_V) {
         LOG("%s: Got variable description for variable '%s' (0x%x):'\n",
             __FUNCTION__,pDesc->Name,gDevVar);
      }

   // Create the variable
      pVar->ID = gDevVar;
      pVar->Type = pAttrib->Type;
      gDevVars++;

   // Find an empty slot to stash the new variable
      for(i = NUM_STANDARD_VARS; Variables[i] != NULL; i++);
      Variables[i] = pDevVar;

      if(!IS_TYPE_ENUM(pDesc)) {
      // Enum types will be dumped later
         DumpVarAttributes(pDesc);
      }
   } while(FALSE);

   if(bNextVar) {
   // Get the next variable
      Cmd[0] = WASP_CMD_GET_VAR_ATTRIBS;
      Cmd[1] = ++gDevVar;
      WaspAsyncSendCmd(p->p,Cmd,2,StartVarDescHandler,NULL);
   }
   else {
      SetStartupState(p->p,NEXT_STATE);
   }

   return Ret;
}

void StartGetNextEnum(WaspAsyncCtrl *p)
{
   for( ;  ;) {
      WaspVarDesc *pDesc = GetVarDesc(gDevVar);
      if(pDesc == NULL) {
      // We're done
         LOG("%s: device defined %d device variables.\n",__FUNCTION__,gDevVars);
         if(gPseudoVars > 0) {
            LOG("%s: device defined %d device pseudo variables.\n",
                __FUNCTION__,gPseudoVars);
         }
         SetStartupState(p,NEXT_STATE);
         break;
      }

      if(IS_TYPE_ENUM(pDesc)) {
         u8 Cmd[3];
         Cmd[0] = WASP_CMD_GET_ENUM_ATTRIBS;
         Cmd[1] = gDevVar;
         Cmd[2] = gEnumVal;
         WaspAsyncSendCmd(p,Cmd,3,StartEnumDescHandler,NULL);
         break;
      }
   // Try the next variable
      gEnumVal = 0;
      gDevVar++;
   }
}

int StartEnumDescHandler(HandlerArgs *p)
{
   WaspVarDesc *pDesc = GetVarDesc(gDevVar);
   int NameLen = p->MsgLen - 2;
   char *EnumName;
   int Ret = 0;

   do {
      if(pDesc == NULL) {
         printf("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
         Ret = WASP_ERR_WASPD_INTERNAL;
         break;
      }

      if(p->Msg[1] == WASP_OK) {
      // save the enum description
         if((EnumName = malloc(NameLen + 1)) == NULL) {
            LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
            Ret = -ENOMEM;
            break;
         }
         memcpy(EnumName,&p->Msg[2],NameLen);
         EnumName[NameLen] = 0;
         pDesc->Enum.Name[gEnumVal] = EnumName;
         pDesc->Enum.Count++;
         gEnumVal++;

         if(gEnumVal <= pDesc->MaxValue.Enum) {
         // Get the next Enum name
            u8 Cmd[3];

            Cmd[0] = WASP_CMD_GET_ENUM_ATTRIBS;
            Cmd[1] = gDevVar;
            Cmd[2] = gEnumVal;
            WaspAsyncSendCmd(p->p,Cmd,3,StartEnumDescHandler,NULL);
         }
         else {
            DumpVarAttributes(pDesc);
            gEnumVal = 0;
            gDevVar++;
            StartGetNextEnum(p->p);
         }
      }
      else {
         LOG("%s: Device returned 0x%x for WASP_CMD_GET_ENUM_ATTRIBS, "
             "variable 0x%x.\n",__FUNCTION__,p->Msg[1],gDevVar);
         DumpVarAttributes(pDesc);
      }
   } while(FALSE);

   return Ret;
}

void DumpVarAttributes(WaspVarDesc *p)
{
   int Integer;
   int Fraction;
      
   do {
      if(p->Type == 0 || p->Type > NUM_VAR_TYPES) {
         printf("%s: Error invalid VarType 0x%x\n",__FUNCTION__,p->Type);
         break;
      }
      printf("Variable '%s':\n",p->Name);
      printf("  VarID:\t%d (0x%02x)\n",p->ID,p->ID);

      if(p->Usage & WASP_USAGE_PSEUDO) {
         u8 Mask = 0x80;
         int i;
         int bFirst = TRUE;

         printf("  VarType:\t%s pseudo variable\n",
                PseudoUsages[p->Usage-WASP_USAGE_PSEUDO]);
         printf("  Tracked Var:\t%d (0x%x)\n",
                p->u.p.TrackedVarId,p->u.p.TrackedVarId);
         printf("  Flags:\t0x%x",p->u.p.TrackingFlags);
         for(i = 0; i < 8; i++,Mask >>= 1) {
            if(p->u.p.TrackingFlags & Mask) {
               if(bFirst) {
                  printf(" (");
                  bFirst = FALSE;
               }
               else {
                  printf(",");
               }
               printf("%s",TrackingFlags[i]);
            }
         }
         printf("%s\n",bFirst ? "" : ")");

      }
      else {
         printf("  VarType:\t%s (0x%x)\n",WASP_VarTypes[p->Type],p->Type);
         switch(p->Usage) {
            case WASP_USAGE_FIXED:
            case WASP_USAGE_MONITORED:
            case WASP_USAGE_DESIRED:
            case WASP_USAGE_CONTROLLED:
               printf("  Usage:\t%s\n",Usages[p->Usage]);
               break;

            default:
               printf("%s: Error invalid Usage 0x%x\n",__FUNCTION__,p->Usage);
               break;
         }

         printf("  PollRate:\t");
         switch(p->PollRate) {
            case 0:
               printf("Never\n");
               break;

            case WASP_POLL_SETTINGS:
               printf("When notified\n");
               break;

            case WASP_ASYNC_UPDATE:
               printf("ASYNC\n");
               break;

            default:
               printf("%d.%03d secs\n",p->PollRate/1000,p->PollRate%1000);
               break;
         }
      }

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

      if(IS_TYPE_ENUM(p)) {
      // Dump the enum names
         int i;
         for(i = 0; i < p->Enum.Count; i++) {
            printf("  Enum %d:\t%s\n",i,p->Enum.Name[i]);
         }
      }
   } while(FALSE);
}

// Handle requests that must wait for reply from the device
int AsyncApiRequest(WaspApiMsg *pRequest,int ReqLen)
{
   int Ret = 0;
   AsyncReq *pAsync = (AsyncReq *) malloc(sizeof(AsyncReq) + ReqLen);
   u8 *pCmd = NULL;
   u8 *cp;

   if(pAsync == NULL) {
      gReply.Hdr.Err = Ret = ENOMEM;
   }
   else {
      pAsync->HisAdr = gHisAdr;
      cp = (u8 *) pAsync;
      cp += sizeof(AsyncReq);
      memcpy(cp,pRequest,ReqLen);
      pAsync->pRequest = (WaspApiMsg *) cp;

      switch(pRequest->Hdr.Cmd) {
         case CMD_GET_VAR: {
            u8 Cmd[3];
            Cmd[0] = WASP_CMD_GET_VALUE;
            Cmd[1] = pRequest->u.WaspVar.ID;
            WaspAsyncSendCmd(gAsyncCtrl,Cmd,2,AsyncApiHandler,pAsync);
            break;
         }

         case CMD_GET_VARS: {
            int Count = pRequest->u.GetVars.Count;
            if((pCmd = (u8 *) malloc(1 + Count)) == NULL) {
               Ret = ENOMEM;
               break;
            }
            pCmd[0] = WASP_CMD_GET_VALUES;
            memcpy(&pCmd[1],pRequest->u.GetVars.IDs,Count);
            WaspAsyncSendCmd(gAsyncCtrl,pCmd,Count+1,AsyncApiHandler,pAsync);
            break;
         }

         case CMD_SET_VAR: {
            SendVar2Dev(gAsyncCtrl,pRequest->u.WaspVar.ID,pAsync);
            break;
         }

         case CMD_SET_VARS: {
            int Count = pRequest->u.IntData;
            void *p = (void *) &pRequest->u.IntData + sizeof(pRequest->u.IntData);
            WaspVariable *pVar1 = (WaspVariable *) p;
            WaspVariable *pVar;
            int i;
            int j = 0;
            int MsgLen = 1;   // command byte
            int x;

            for(i = 0; i < Count; i++) {
               if((pVar = FindVariable(pVar1->ID)) == NULL)  {
               // Unknown variable
                  LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
                  break;
               }
               CopyVarValue(pVar,NULL,0,&x,TRUE);
               MsgLen += 1 + x;  // Var ID + Var data

               p += WASP_GetVarLen(pVar1);
               pVar1 = (WaspVariable *) p;
            }

            if((pCmd = (u8 *) malloc(MsgLen)) == NULL) {
               LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
               Ret = -ENOMEM;
               break;
            }

            pCmd[j++] = WASP_CMD_SET_VALUE;

            p = (void *) &pRequest->u.IntData + sizeof(pRequest->u.IntData);
            pVar1 = (WaspVariable *) p;

            for(i = 0; i < Count; i++) {
               pCmd[j++] = pVar1->ID;
               if((pVar = FindVariable(pVar1->ID)) == NULL)  {
               // Unknown variable
                  LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
                  break;
               }

               if(CopyVarValue(pVar,&pCmd[j],MsgLen-j,&x,TRUE) != WASP_OK) {
                  LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
                  break;
               }
               j += x;
               pVar->State = VAR_VALUE_SETTING;

               p += WASP_GetVarLen(pVar1);
               pVar1 = (WaspVariable *) p;
            }
            WaspAsyncSendCmd(gAsyncCtrl,pCmd,MsgLen,AsyncApiHandler,pAsync);
            break;
         }

         case CMD_SET_LEDSCRIPT: {
            SetLedScriptArgs *pArgs = (SetLedScriptArgs *) &pRequest->u;
            int Len = 2 + (pArgs->Count * sizeof(LedState));

            if((pCmd = (u8 *) malloc(Len)) == NULL) {
               Ret = ENOMEM;
               break;
            }
            pCmd[0] = WASP_CMD_SET_LED;
            pCmd[1] = pArgs->Id;
            memcpy(&pCmd[2],pArgs->LedStates,pArgs->Count * sizeof(LedState));
            WaspAsyncSendCmd(gAsyncCtrl,pCmd,Len,AsyncApiHandler,pAsync);
            break;
         }

         case CMD_GET_SELFTEST: {
            u8 Cmd = WASP_CMD_SELFTEST_STATUS;
            WaspAsyncSendCmd(gAsyncCtrl,&Cmd,1,AsyncApiHandler,pAsync);
            break;
         }

         case CMD_SET_ASYNC_Q_LEN: {
            u8 Cmd[2];

            gQueueLen = pRequest->u.IntData;
         // Enable or disable Async data from device
            Cmd[0] = WASP_CMD_ASYNC_ENABLE;
            Cmd[1] = pRequest->u.IntData == 0 ? 0 : 1;
            WaspAsyncSendCmd(gAsyncCtrl,Cmd,2,AsyncApiHandler,pAsync);
            break;
         }

         case CMD_GET_CHANGED_VAR:
            gGetChangeRequest = pAsync;
            SendChangedVariable();
            break;

         default:
            break;
      }
   }

   if(pCmd != NULL) {
      free(pCmd);
   }
   return Ret;
}

int AsyncApiHandler(HandlerArgs *p)
{
   WaspApiMsg Reply;
   WaspApiMsg *pReply = &Reply;
   int DataLen = -1;
   AsyncReq *pAsync = (AsyncReq *) p->Data;
   int Ret = 0;

   do {
      if(p->MsgLen < 2) {
         break;
      }
      DataLen = p->MsgLen - 2;

      if(p->Msg[1] != WASP_OK) {
         LOG("%s: Device returned error 0x%x for command 0x%x.\n",
             __FUNCTION__,p->Msg[1],p->Msg[0] & ~WASP_CMD_RESP);
         Ret = -p->Msg[1];
         break;
      }

      switch(pAsync->pRequest->Hdr.Cmd) {
         case CMD_GET_VAR: {
            WaspVariable *pVar;
            u8 *pVal = &p->Msg[2];
            VarID Id = pAsync->pRequest->u.VarRef.Id;

         // Cache the variable
            if((pVar = FindVariable(Id)) == NULL)  {
               LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
               Ret = WASP_ERR_WASPD_INTERNAL;
               break;
            }
            Ret = WriteVarValue(&pVal,p->MsgLen-2,pVar);
            pVar->State = VAR_VALUE_LIVE;
            DataLen = Var2Ipc(pVar,&pReply);
            pVar->State = VAR_VALUE_CACHED;  // Life is short
            if(DataLen < 0) {
               Ret = DataLen;
            }
            break;
         }

         case CMD_GET_VARS:
         case CMD_POLL_VARIABLES: {
            WaspVariable *pVar;
            int Count = pAsync->pRequest->u.GetVars.Count;
            VarID *ID = pAsync->pRequest->u.GetVars.IDs;
            u8 *pVal = &p->Msg[2];
            u8 *pEnd = &p->Msg[p->MsgLen];
            int i;

         // Cache the variables
            for(i = 0; i < Count; i++) {
               if((pVar = FindVariable(ID[i])) == NULL)  {
                  LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
                  break;
               }
               if(pVar->Type == WASP_BLOB) {
               // This MUST be the last variable or we won't can't determine
               // the data length
                  if(i != (Count - 1)) {
                     LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
                     break;
                  }
                  DataLen = pEnd - pVal;
               }
               else {
                  DataLen = 0;
               }

               Ret = WriteVarValue(&pVal,DataLen,pVar);
               pVar->State = VAR_VALUE_LIVE;
            }

            if(pAsync->pRequest->Hdr.Cmd == CMD_GET_VARS)  {
            // Send the reply
               DataLen = Vars2Ipc(&pAsync->pRequest->u.GetVars,&pReply);
               if(DataLen < 0) {
                  Ret = DataLen;
                  break;
               }
            }
            else if(pAsync->pRequest->Hdr.Cmd == CMD_POLL_VARIABLES) {
               DataLen = -1;  // Don't send an API response
               if(pAsync->pRequest->u.GetVars.bLive == READ_PERIODIC) {
               // Poll again
                  SetTimerCallback(VariablePoller,gPollDelay,NULL);
               }
               else if(pAsync->pRequest->u.GetVars.bLive == READ_ALL) {
                  SetStartupState(p->p,NEXT_STATE);
               }
            }

         // Change state of the variables to cached
            for(i = 0; i < Count; i++) {
               if((pVar = FindVariable(ID[i])) == NULL)  {
                  LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
                  break;
               }
               pVar->State = VAR_VALUE_CACHED; 
            }
            if(gPseudoVarChanged && 
               (gTimeNow.tv_sec - gPseudoVarsLastSaved) > MIN_PSEUDO_SAVE_DELAY)
            {
               gPseudoVarChanged = FALSE;
               if(gWASP_Verbose & LOG_PSEUDO) {
                  LOG("%s: Calling SavePseudoVars()\n",__FUNCTION__);
               }
               SavePseudoVars();
            }
            break;
         }

         case CMD_SET_VAR: {
            WaspVariable *pVar;
            VarID Id = pAsync->pRequest->u.VarRef.Id;

            if((pVar = FindVariable(Id)) == NULL)  {
               LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
               Ret = WASP_ERR_WASPD_INTERNAL;
               break;
            }
            else {
               pVar->State = Ret == 0 ? VAR_VALUE_SET : VAR_VALUE_UNKNOWN;
            }
            break;
         }

         case CMD_GET_SELFTEST: {
            if(DataLen > sizeof(pReply->u)) {
            // Need a temporary buffer
               pReply = (WaspApiMsg *) malloc(sizeof(pReply->Hdr) + DataLen);
               if(pReply == NULL) {
                  LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
                  Ret = -ENOMEM;
                  break;
               }
            }
            memcpy(&pReply->u,&p->Msg[2],DataLen);
            break;
         }

         default:
            break;
      }
   } while(FALSE);

   if (pAsync->pRequest->Hdr.Cmd < CMD_INTERNAL) {
      if(Ret != 0) {
         Reply.Hdr.Err = Ret;
         Reply.Hdr.Cmd = pAsync->pRequest->Hdr.Cmd | CMD_RESPONSE;
         UdpSend(gIpcSocket,&pAsync->HisAdr,&Reply,sizeof(Reply.Hdr));
      }
      else if(DataLen >= 0){
         pReply->Hdr.Err = 0;
         pReply->Hdr.Cmd = pAsync->pRequest->Hdr.Cmd | CMD_RESPONSE;
         UdpSend(gIpcSocket,&pAsync->HisAdr,pReply,sizeof(Reply.Hdr) + DataLen);
      }
   }

   if(pAsync != NULL) {
      free(pAsync);
   }

   if(pReply != &Reply && pReply != NULL) {
      free(pReply);
   }

   if(gGetChangeRequest != NULL && gVarChanged) {
      SendChangedVariable();
   }

   return Ret;
}

int FindVariableByName(const char *Name)
{
   int VarID = 0;
   int i;

   if(Name != NULL && Name[0] != 0) {
   // find by name
      for(i = 0; Variables[i] != NULL; i++) {
         if(strcmp(Name,Variables[i]->Desc.Name) == 0) {
            VarID = Variables[i]->Desc.ID;
            break;
         }
      }
   }

   if(gWASP_Verbose & LOG_V1) {
      LOG("%s: Returning VarID 0x%x for '%s'\n",__FUNCTION__,VarID,Name);
   }

   return VarID;
}

DeviceVar *FindDevVar(u8 VarID)
{
   DeviceVar *Ret = NULL;
   int i;

   for(i = 0; (Ret = Variables[i]) != NULL; i++) {
      if(VarID == Ret->Desc.ID) {
         break;
      }
   }

   if(Ret == NULL && (gWASP_Verbose & LOG_V2)) {
      LOG("%s: VarID 0x%x not found\n",__FUNCTION__,VarID);
   }
   return Ret;
}

WaspVariable *FindVariable(u8 VarID)
{
   WaspVariable *Ret = NULL;
   DeviceVar *pDevVar = FindDevVar(VarID);

   if(pDevVar != NULL) {
      Ret = pDevVar->pVars->pVar;
   }
   return Ret;
}

int Var2Ipc(WaspVariable *pVar,WaspApiMsg **pReply)
{
   WaspApiMsg *p = *pReply;
   int MallocSize;
   int DataLen = sizeof(p->u.WaspVar); 
   int ValueLen = 0;
   int Err;

   Err = CopyVarValue(pVar,&p->u.WaspVar.Val,sizeof(p->u.WaspVar.Val),
                      &ValueLen,TRUE);

   if(Err == WASP_ERR_MORE_DATA) {
   // Need a temporary buffer
      MallocSize = offsetof(WaspApiMsg,u) + offsetof(WaspVariable,Val) +
                   ValueLen;
      
      if((p = (WaspApiMsg *) malloc(MallocSize)) == NULL) {
         LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
         DataLen = -ENOMEM;
      }
      else {
         DataLen += (ValueLen - sizeof(p->u.WaspVar.Val));
         p->Hdr.Cmd = (*pReply)->Hdr.Cmd;
         p->Hdr.Err = (*pReply)->Hdr.Err;
         *pReply = p;
         Err = CopyVarValue(pVar,&p->u.WaspVar.Val,ValueLen,
                            &ValueLen,TRUE);
         if(Err != WASP_OK) {
         // This shouldn't happen... we allocated a big enough buffer
            LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
            DataLen = -Err;
         }
      }
   }

   if(p != NULL) {
      p->u.WaspVar.ID = pVar->ID;
      p->u.WaspVar.Type = pVar->Type;
      p->u.WaspVar.State = pVar->State;
      p->u.WaspVar.Pseudo = pVar->Pseudo;
   }
   return DataLen;
}

int Vars2Ipc(GetVarsList *pGetArgs,WaspApiMsg **ppReply)
{
   WaspApiMsg *p = *ppReply;
   int MallocSize;
   int DataLen = 0;
   int BytesLeft;
   int ValueLen = 0;
   int Err = 0;
   int Count = pGetArgs->Count;
   int i;
   WaspVariable *pVar;
   u8 *cp;
   u8 *ID = pGetArgs->IDs;

   p->Hdr.Err = WASP_OK;   // assume the best
   do {
   // Calculate the size of the reply
      DataLen = Count * (sizeof(WaspVariable) - sizeof(WaspValUnion));
      for(i = 0; i < Count; i++) {
         if((pVar = FindVariable(ID[i])) == NULL)  {
         // Unknown variable
            printf("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
            Err = WASP_ERR_WASPD_INTERNAL;
            break;
         }
         if(CopyVarValue(pVar,NULL,0,&ValueLen,TRUE) != WASP_ERR_MORE_DATA) {
            printf("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
            Err = WASP_ERR_WASPD_INTERNAL;
            break;
         }
         DataLen += ValueLen;
      }

      if(Err != WASP_OK) {
         break;
      }

      BytesLeft = DataLen;
      if(DataLen > sizeof(p->u)) {
      // Need a temporary buffer
         MallocSize = offsetof(WaspApiMsg,u) + DataLen;
         
         if((p = (WaspApiMsg *) malloc(MallocSize)) == NULL) {
            LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
            DataLen = -ENOMEM;
            break;
         }
         *ppReply = p;
      }
      cp = &p->u.U8;

      for(i = 0; i < Count; i++) {
         if((pVar = FindVariable(ID[i])) == NULL)  {
         // Unknown variable
            printf("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
            Err = WASP_ERR_WASPD_INTERNAL;
            break;
         }
         memcpy(cp,pVar,offsetof(WaspVariable,Val));
         cp += offsetof(WaspVariable,Val);
         BytesLeft -= offsetof(WaspVariable,Val);
         Err = CopyVarValue(pVar,cp,BytesLeft,&ValueLen,TRUE);
         if(Err != WASP_OK) {
         // This shouldn't happen... we allocated a big enough buffer
            LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
            break;
         }
         BytesLeft -= ValueLen;
         cp += ValueLen;
         if(BytesLeft < 0) {
            LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
            Err = WASP_ERR_WASPD_INTERNAL;
         }
      }
   } while(FALSE);

   return Err == 0 ? DataLen : Err;
}


// Get data for
// WASP_GetVarList(int *Count,WaspVarDesc **pVarDescArray);
// API call
int VarList2Ipc(WaspApiMsg **pReply)
{
   WaspApiMsg *p = *pReply;
   int DataLen;
   WaspVarDesc *pDesc;
   int i;
   int j;
   char *cp;
   char *cp1;
   int Count = 0;

   do {
   // Calculate the message size
      DataLen = sizeof(int) + (sizeof(WaspVarDesc) * gDevVars);
      for(i = NUM_STANDARD_VARS; Variables[i] != NULL; i++) {
         Count++;
         pDesc = &Variables[i]->Desc;
         if(IS_TYPE_ENUM(pDesc)) {
         // Add space to save enum names
            for(j = 0; j < pDesc->Enum.Count; j++) {
               DataLen += strlen(pDesc->Enum.Name[j]) + 1;
            }
         }
      }
      if((p = (WaspApiMsg *) malloc(DataLen+offsetof(WaspApiMsg,u))) == NULL) {
         LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
         DataLen = -ENOMEM;
         break;
      }

      p->Hdr.Cmd = (*pReply)->Hdr.Cmd;
      p->Hdr.Err = (*pReply)->Hdr.Err;
      *pReply = p;
      cp = (char *) &p->u;

   // copy number of descriptions into reply
      memcpy(cp,&Count,sizeof(int));
      cp += sizeof(int);
      cp1 = cp + (sizeof(WaspVarDesc) * gDevVars);

      for(i = NUM_STANDARD_VARS; Variables[i] != NULL; i++) {
         pDesc = &Variables[i]->Desc;
         memcpy(cp,pDesc,sizeof(*pDesc));
         cp += sizeof(*pDesc);
         if(IS_TYPE_ENUM(pDesc)) {
         // Add space to save enum names
            for(j = 0; j < pDesc->Enum.Count; j++) {
               strcpy(cp1,pDesc->Enum.Name[j]);
               cp1 += strlen(pDesc->Enum.Name[j]) + 1;
            }
         }
      }
   } while(FALSE);

   return DataLen;
}


// Write variable value
// (DataLen is only needed for blobs and optionally string types)
int WriteVarValue(u8 **ppRaw,int DataLen,WaspVariable *pVar)
{
   VarType VarType = pVar->Type;
   int Ret = 0;   // Assume the best
   u8 *pRaw = *ppRaw;
   WaspVariable OldValue;

   WASP_CopyVar(&OldValue,pVar);

   if(VarType == WASP_VARTYPE_BLOB || VarType == WASP_VARTYPE_STRING) {
      char **pData;
      if(pVar->Type == WASP_VARTYPE_STRING) {
         if(DataLen == 0) {
         // Use string len
            DataLen = strlen((char *) pRaw) + 1;
         }
         else {
            DataLen++;  // allocate space for NULL terminator
         }
         pData = &pVar->Val.String;
      }
      else {
         pData = (char **) &pVar->Val.Blob.Data;
      }

      if(*pData != NULL) {
         free(*pData);
      }

      if((*pData = malloc(DataLen)) == NULL) {
         LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
         Ret = -ENOMEM;
      }
      else {
         memcpy(*pData,pRaw,DataLen);
         if(VarType == WASP_VARTYPE_STRING) {
         // Terminate the string
            ((char *) *pData)[DataLen-1] = 0;
         }
         else if(VarType == WASP_VARTYPE_BLOB) {
            pVar->Val.Blob.Len = DataLen;
         }
         else {
            printf("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
         }
      }
   }
   else {
   // lookup data length
      DataLen = WASP_VarType2Len[pVar->Type];
      memcpy(&pVar->Val,pRaw,DataLen);
#if 0
      // This is a royal Kludge to support Jarden CrockPot cooking time 
      // This will eventually be handled by the App and r
      _VariableSetHook(pVar);
#endif
   }

   *ppRaw = pRaw + DataLen;

   if(WASP_Compare(&OldValue,pVar) > 0) {
   // Value has changed
      pVar->Pseudo.bChanged = TRUE;
      gVarChanged = TRUE;
   }
   WASP_FreeValue(&OldValue);

   if(gPseudoVars > 0 && !pVar->Pseudo.bPseudoVar) {
      UpdatePseudoVars(pVar);
   }
   return Ret;
}

// Copy variable value to buffer
int CopyVarValue(
   WaspVariable *pVar,
   void *pBuf,
   int MaxLen,
   int *pDataLen,
   int bNullTerminate)
{
   VarType VarType = pVar->Type;
   int Ret = WASP_ERR_MORE_DATA; // Assume data won't fit
   void *pData;
   size_t DataLen;

   switch (VarType) {
      case WASP_VARTYPE_STRING:
         pData = pVar->Val.String;
         DataLen = strlen(pVar->Val.String);
         if(bNullTerminate) {
            DataLen++;
         }
         break;

      case WASP_VARTYPE_BLOB:
         pData = &pVar->Val.Blob;
         DataLen = pVar->Val.Blob.Len + sizeof(int);
         if(DataLen <= MaxLen) {
         // copy the Len
            *((int*) pBuf) = pVar->Val.Blob.Len;
            pBuf = (void *) ((int) pBuf + sizeof(int));
         // Copy the data
            memcpy(pBuf,pVar->Val.Blob.Data,pVar->Val.Blob.Len);
            Ret = WASP_OK;
            MaxLen = 0; // Bypass the copy below
         }
         break;

      default:
         pData = &pVar->Val;
         DataLen = WASP_VarType2Len[VarType];
         break;
   }

   if(DataLen <= 0) {
      LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
      Ret = WASP_ERR_WASPD_INTERNAL;
   }
   else if(DataLen <= MaxLen) {
      memcpy(pBuf,pData,DataLen);
      Ret = WASP_OK;
   }
   *pDataLen = DataLen;

   return Ret;
}


// Send variable to the device
int SendVar2Dev(WaspAsyncCtrl *p,VarID ID,AsyncReq *pAsync)
{
   int Ret = 0;   // Assume the best
   WaspVariable *pVar = FindVariable(ID);
   u8 *Cmd = NULL;
   int DataLen;
   int Dummy;

   do {
      if(pVar == NULL) {
         LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
         break;
      }
   // Find Length of the variable's data
      pVar->State = VAR_VALUE_SETTING;
      CopyVarValue(pVar,NULL,0,&DataLen,FALSE);
      DataLen += 2;
      if((Cmd = (u8 *) malloc(DataLen)) == NULL) {
         LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
         Ret = -ENOMEM;
         break;
      }
      Cmd[0] = WASP_CMD_SET_VALUE;
      Cmd[1] = ID;
      if(CopyVarValue(pVar,&Cmd[2],DataLen-2,&Dummy,FALSE) != WASP_OK) {
         LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
         break;
      }
      WaspAsyncSendCmd(gAsyncCtrl,Cmd,DataLen,AsyncApiHandler,pAsync);
   } while(FALSE);

   if(Cmd != NULL) {
      free(Cmd);
   }

   return Ret;
}


WaspVarDesc *GetVarDesc(u8 VarID)
{
   WaspVarDesc *Ret = NULL;
   DeviceVar *pDevVar = FindDevVar(VarID);

   if(pDevVar != NULL) {
      Ret = &pDevVar->Desc;
   }

   return Ret;
}

void ButtonPoller(void *p)
{
   u8 Cmd = WASP_CMD_GET_PRESSES;
   HandlerArgs *pArgs = (HandlerArgs *) p;

   WaspAsyncSendCmd(pArgs->p,&Cmd,1,ButtonPressHandler,pArgs);
}

void VariablePoller(void *pData)
{
   if(gWASP_Verbose & LOG_V1) {
      LOG("%s: Polling device variables\n",__FUNCTION__);
   }
   ReadVariables(READ_PERIODIC);
}

int LinkCheckHandler(HandlerArgs *p)
{
   int MsgLen = p->MsgLen;

   do {
      if(MsgLen < 2) {
         break;
      }

      if(p->Msg[1] != WASP_OK) {
         LOG("%s: device returned %s (0x%x).\n",__FUNCTION__,
             WASP_strerror(-p->Msg[1]),p->Msg[1]);
         break;
      }
   } while(FALSE);

   SetTimerCallback(LinkChecker,LINK_CHECK_TIME,&gGenericArgs);

   return 0;
}

void LinkChecker(void *p)
{
   u8 Cmd = WASP_CMD_NOP;
   HandlerArgs *pArgs = (HandlerArgs *) p;

   if(pArgs->p->Baudrate != WASP_DEFAULT_BAUDRATE) {
   // Keep pinging the device as long as we're not running at
   // the default baudrate
      WaspAsyncSendCmd(pArgs->p,&Cmd,1,LinkCheckHandler,pArgs);
   }
}


void QueueButton(u8 Button)
{
   gButtonRing.Buf[gButtonRing.Wr++] = Button;

   if(gButtonRing.Wr >= BUTTON_BUF_LEN) {
      gButtonRing.Wr = 0;
   }
   if(gButtonRing.Rd == gButtonRing.Wr) {
      LOG("%s: Error - gButtonRing overflow.\n",__FUNCTION__);
   }
   if(gWASP_Verbose & LOG_V) {
      int bPressed = Button & WASP_BUTTON_PRESSED;
      LOG("%s: Queued button %s for button 0x%x.\n",__FUNCTION__,
          bPressed ? "press" : "release",
          Button & ~WASP_BUTTON_PRESSED);
   }
}

int ButtonPressHandler(HandlerArgs *p)
{
   int MsgLen = p->MsgLen;
   int i;

   do {
      if(MsgLen < 2) {
         break;
      }

      if(p->Msg[1] != WASP_OK) {
         LOG("%s: device returned %s (0x%x).\n",__FUNCTION__,
             WASP_strerror(-p->Msg[1]),p->Msg[1]);
         break;
      }

      for(i = 2; i < MsgLen; i++) {
         u8 Button = p->Msg[i];

         if(Button == WASP_BUTTON_SETTINGS_CHANGED) {
         // pseudo button, poll settings variables
            if(gWASP_Verbose & LOG_V1) {
               LOG("%s: Got pseudo key 0x71, reading user settings.\n",
                   __FUNCTION__);
            }
            ReadVariables(READ_SETTINGS);
         }
         else {
         // Queue it
            QueueButton(Button);
         }
      }
   // Poll again
      SetTimerCallback(ButtonPoller,KEYBOARD_POLL_TIME,&gButtonPollerArgs);
   } while(FALSE);

   return 0;
}

void CheckTimers()
{
   Timer *p = gTimerHead;

   GetTimeNow();
   while(p != NULL) {
      if(TimeLapse(&p->When) < 0) {
      // Timer has not expired, no need to look further since
      // link list is ordered
         break;
      }
   // This timer has expired
      gTimerHead = p->Link;
      p->pCallBack(p->pData);
      free(p);
      p = gTimerHead;
   }
}


void ReadVariables(PollType Which)
{
   int i;
   int j = 0;
   u8 Msg[WASP_MAX_MSG_LEN];

   Msg[j++] = WASP_CMD_GET_VALUES;

   if(Which == READ_ALL) {
   // Read the standard variables that we care about
      Msg[j++] = WASP_VAR_DEVID;
      Msg[j++] = WASP_VAR_FW_VER;
      Msg[j++] = WASP_VAR_DEV_DESC;
   }

   for(i = NUM_STANDARD_VARS; Variables[i] != NULL; i++) {
      VarUsage Usage = Variables[i]->Desc.Usage;
      u16 Pollrate = Variables[i]->Desc.PollRate;

      if(Usage & WASP_USAGE_PSEUDO) {
      // No need to read pseudo values ever
         continue;
      }

      if(Which != READ_ALL) {
         if(Usage == WASP_USAGE_FIXED) {
         // No need to read fixed values
            continue;
         }

         if(Pollrate == WASP_POLL_NEVER) {
         // No need to read this variable
            continue;
         }

         if(Which == READ_SETTINGS && Pollrate != WASP_POLL_SETTINGS) {
         // No need to read this variable, it's not a setting 
            continue;
         }

         if(Which == READ_PERIODIC && Pollrate == WASP_POLL_SETTINGS) {
         // No need to read this variable, it's a setting 
            continue;
         }
      }
      else {
      // Reading ALL variables on startup.  Initialize all variables to changed
         Variables[i]->pVars->pVar->Pseudo.bChanged = TRUE;
      }

      if(Which != READ_ASYNC && Pollrate == WASP_ASYNC_UPDATE) {
      // No need to read this variable, it's Async
         continue;
      }

   // Read the variable 
      Msg[j++] = Variables[i]->Desc.ID;
   }
   j--;  // remove the command byte from the count

   if(j > 0) {
      WaspApiMsg *pReq;
      int MallocLen = sizeof(AsyncReq) + sizeof(WaspApiMsg) + j;
      AsyncReq *pAsync;

      if((pAsync = malloc(MallocLen)) != NULL) {
         memset(pAsync,0,MallocLen);
         pAsync->pRequest = pReq = (WaspApiMsg *) (pAsync + 1);
         pReq->Hdr.Cmd = CMD_POLL_VARIABLES;
         pReq->u.GetVars.Count = j;
      // In the context bLive holds the poll type
         pReq->u.GetVars.bLive = (u8) Which;
         memcpy(pReq->u.GetVars.IDs,&Msg[1],j);
         WaspAsyncSendCmd(gAsyncCtrl,Msg,j+1,AsyncApiHandler,pAsync);
      }
      else {
         LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
      }
   }
}

void StartPolling()
{
// Calculate the monitored variable poll time
   int i;

   gPollDelay = 0xffff;
   for(i = NUM_STANDARD_VARS; Variables[i] != NULL; i++) {
      u16 Pollrate = Variables[i]->Desc.PollRate;

      if(Pollrate != 0 && 
         Pollrate != WASP_POLL_SETTINGS &&
         Pollrate != WASP_ASYNC_UPDATE &&
         gPollDelay > Pollrate)
      {  // Save new minimum poll delay
         gPollDelay = Pollrate;
      }
   }


   if(gPollDelay != 0xffff) {
   // Start monitored variable polling
      if(gWASP_Verbose & LOG_V) {
         LOG("%s: gPollDelay set to %u milliseconds\n",__FUNCTION__,gPollDelay);
      }
      SetTimerCallback(VariablePoller,gPollDelay,NULL);
   }
   else {
      if(gWASP_Verbose & LOG_V) {
         LOG("%s: Not polling any variables\n",__FUNCTION__);
      }
   }

// Start button poller
   gButtonPollerArgs.p = gAsyncCtrl;
   SetTimerCallback(ButtonPoller,KEYBOARD_POLL_TIME,&gButtonPollerArgs);

   if(gAsyncCtrl->Baudrate != WASP_DEFAULT_BAUDRATE) {
   // Monitor the link and fall back to the default baudrate if it fails.
   // If the link fails we're assuming that the interface board has
   // been reset for some reason and is back at the default baudrate
      SetTimerCallback(LinkChecker,LINK_CHECK_TIME,&gGenericArgs);
   }
}

void SignalHandler(int Signal)
{
   switch(Signal) {
      case SIGTERM:
         LOG("%s: Received signal SIGTERM (%d)\n",__FUNCTION__,Signal);
         break;

      case SIGSEGV:
         LOG("%s: Received signal SIGSEGV (%d)\n",__FUNCTION__,Signal);
         break;

      default:
         LOG("%s: Received unknown signal %d\n",__FUNCTION__,Signal);
         break;
   }
   exit(Signal);
}


int StartButtonsHandler(HandlerArgs *p)
{
   int NumButtons = p->MsgLen - 2;
   int Ret = 0;
   int i;

   do {
      if(p->Msg[1] == WASP_OK) {
      // Process the inital button state
         if(gWASP_Verbose & LOG_V) {
            LOG("%s: Device defines %d buttons\n",__FUNCTION__,NumButtons);
         }
         for(i = 0; i < NumButtons; i++) {
            if(p->Msg[i + 2] & WASP_BUTTON_PRESSED) {
            // Button is pressed, queue key press event
               QueueButton(p->Msg[i + 2]);
            }
         }
      }
      else {
         LOG("%s: Device returned 0x%x for WASP_CMD_GET_BUTTONS\n",
            __FUNCTION__,p->Msg[1]);
      }

      SetStartupState(p->p,NEXT_STATE);
   } while(FALSE);

   return Ret;
}

int SetBaudRateHandler(HandlerArgs *p)
{
   WaspState NextState = NEXT_STATE;   // assume next state

   if(p->Msg[1] != WASP_OK) {
      LOG("%s: Device returned 0x%x for WASP_CMD_SET_BAUDRATE\n",
         __FUNCTION__,p->Msg[1]);
      switch(gWaspdState) {
         case SET_NEW_BAUDRATE:
            NextState = GET_DEVICE_VARS;
            break;

         case RESET_BAUDRATE:
            LOG("%s#%d: Theory says this can't happen!\n",__FUNCTION__,__LINE__);
            NextState = ENABLE_ASYNC;
            break;

         default:
            LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
            break;
      }
   }
   else {
      u32 BaudRate = *((u32 *) p->Data);
      LOG("%s: Switching to %u baud\n",__FUNCTION__,BaudRate);
      if(WaspAsyncSetBaudRate(p->p,BaudRate) != 0) {
         LOG("%s: WaspAsyncSetBaudRate failed\n",__FUNCTION__);
      }
      else {
         LOG("%s: Device ack'ed switch to %u.\n",__FUNCTION__,BaudRate);
         switch(gWaspdState) {
            case SET_NEW_BAUDRATE:
               break;

            case RESET_BAUDRATE:
               NextState = ENABLE_ASYNC;
               break;

            default:
               LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
               break;
         }
      }
   }

   SetStartupState(p->p,NextState);

   return 0;
}

int StartEnableAsyncHandler(HandlerArgs *p)
{
   if(p->Msg[1] != WASP_OK) {
      LOG("%s: Device returned 0x%x for WASP_CMD_ASYNC_ENABLE\n",
         __FUNCTION__,p->Msg[1]);
   }

   SetStartupState(p->p,NEXT_STATE);

   return 0;
}


// -s<Var>:<type>:<Value> - Set <Var> of <type> to <value>.\n");
TestSetVar *ParseSetVar(char *arg)
{
   char *cp = arg;
   char *VarName = NULL;
   int Type;
   int i32;
   u32 U32;
   u32 U32a;
   int VarId;
   TestSetVar *pRet = NULL;
   TestSetVar *pTemp = NULL;
   int Len = sizeof(TestSetVar);
   const char *ErrStr = NULL;

   do {
   // Get variable name or ID
      if((cp = strchr(arg,':')) == NULL) {
      // Couldn't find Variable name/ID
         break;
      }
      *cp++ = 0;
      if(isalpha(*arg)) {
      // Set by Name
         VarName = arg;
      }
      else if(sscanf(arg,"%d",&VarId) != 1 || VarId <= 0 || VarId > 0xff) {
         ErrStr = "Error: Invalid variable ID '%s'.\n";
         break;
      }

   // Get type
      arg = cp;
      if((cp = strchr(arg,':')) == NULL) {
      // Couldn't find Variable type
         break;
      }
      *cp++ = 0;

      if(sscanf(arg,"%d",&Type) != 1 || Type <= 0 || Type > NUM_VAR_TYPES) {
         ErrStr = "Error: Invalid variable type '%s'.\n";
         break;
      }

      if(Type == WASP_VARTYPE_BLOB) {
         printf("Error: WASP_VARTYPE_BLOB is not supported.\n");
         break;
      }

      switch(Type) {
         case WASP_VARTYPE_STRING:
            Len += strlen(cp);
            break;

         default:
            Len += WASP_VarType2Len[Type];
            break;
      }

      if((pTemp = (TestSetVar *) malloc(Len)) == NULL) {
         printf("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
         break;
      }

      memset(pTemp,0,sizeof(TestSetVar));
      pTemp->Var.ID = VarId;
      pTemp->VarName = VarName;
      pTemp->Var.Type = Type;
      pTemp->Var.State = VAR_VALUE_SET;

      // Copy the value to set
      arg = cp;

      ErrStr = "Error: Invalid %s variable '%s'.\n";  // asssume the worse
      switch(Type) {
         case WASP_VARTYPE_ENUM:
         case WASP_VARTYPE_UINT8:
            if(sscanf(arg,"%u",&U32) != 1 || U32 > 255) {
               break;
            }
            pRet = pTemp;
            pRet->Var.Val.Enum = (u8) U32;
            break;

         case WASP_VARTYPE_PERCENT: // % of full 0.0 to 100.0
            if(sscanf(arg,"%u",&U32) != 1 || U32 > 1000) {
               ErrStr = "Error: Invalid %s variable '%s'.\n"
                        "12.3%% is entered 123\n";
               break;
            }
            pRet = pTemp;
            pRet->Var.Val.Percent = (u16) U32;
            break;

         case WASP_VARTYPE_TEMP:
            if(sscanf(arg,"%d",&i32) != 1 || i32 < -32786 || i32 > 32767) {
               break;
            }
            pRet = pTemp;
            pRet->Var.Val.Temperature = (short) i32;
            break;

         case WASP_VARTYPE_TIME16:
         case WASP_VARTYPE_UINT16:
         case WASP_VARTYPE_TIME_M16:
            if(sscanf(arg,"%u",&U32) != 1 || U32 > 0xffff) {
               break;
            }
            pRet = pTemp;
            pRet->Var.Val.TimeSecs = (unsigned short) U32;
            break;

         case WASP_VARTYPE_TIME32:
            if(sscanf(arg,"%d",&i32) != 1) {
               break;
            }
            pRet = pTemp;
            pRet->Var.Val.TimeTenths = i32;
            break;

         case WASP_VARTYPE_TIMEBCD: 
         // SS:MM:HH in BCD, entered as hhmmss
            if(sscanf(arg,"%x",&U32) != 1 || 
               BcdCheck(U32) ||
               (U32 & 0xff) > 0x59 ||
               ((U32 >> 8) & 0xff) > 0x59 ||
               ((U32 >> 16) & 0xff) > 0x23)
            {
               ErrStr = "Error: Invalid %s variable '%s'.\n"
                        "       ss:mm:hh is entered as hhmmss.\n";
               break;
            }
            pRet = pTemp;
            memcpy(pRet->Var.Val.BcdTime,&U32,3);
            break;

         case WASP_VARTYPE_BOOL:
            if(sscanf(arg,"%u",&U32) != 1 || U32 > 1) {
               break;
            }
            pRet = pTemp;
            pRet->Var.Val.Boolean = (unsigned char) U32;
            break;

         case WASP_VARTYPE_BCD_DATE:   
         // dd/mm/yyyy in BCD, entered as mmddyyyy
            if(sscanf(arg,"%x",&U32) != 1 || 
               BcdCheck(U32) ||
               ((U32 >> 16) & 0xff) > 0x31 ||
               ((U32 >> 16) & 0xff) == 0x00 ||
               ((U32 >> 24) & 0xff) > 0x12 ||
               ((U32 >> 24) & 0xff) == 0x00)
            {
               ErrStr = "Error: Invalid %s variable '%s'.\n"
                        "       dd/mm/yyyy is entered as mmddyyyy.\n";
               break;
            }
            pRet = pTemp;
         // Day of month
            pRet->Var.Val.BcdDate[0] = (U32 >> 16) & 0xff;
         // Month
            pRet->Var.Val.BcdDate[1] = (U32 >> 24) & 0xff;
         // Year LSB
            pRet->Var.Val.BcdDate[2] = U32 & 0xff;
         // Year MSB
            pRet->Var.Val.BcdDate[3] = (U32 >> 8) & 0xff;
            break;

         case WASP_VARTYPE_DATETIME:
         // ss:mm:hh dd/mm/yyyyy in BCD, entered as hhmmss_mmddyyyy
            if(sscanf(arg,"%x_%x",&U32,&U32a) != 2 || 
               BcdCheck(U32) ||
               BcdCheck(U32a) ||
               (U32 & 0xff) > 0x59 ||
               ((U32 >> 8) & 0xff) > 0x59 ||
               ((U32 >> 16) & 0xff) > 0x23 ||

               ((U32a >> 16) & 0xff) > 0x31 ||
               ((U32a >> 16) & 0xff) == 0x00 ||
               ((U32a >> 24) & 0xff) > 0x12 ||
               ((U32a >> 24) & 0xff) == 0x00
               )
            {
               ErrStr = "Error: Invalid %s variable '%s'.\n"
                        "       ss:mm:hh dd/mm/yyyyy is entered"
                        " as hhmmss_mmddyyyy.\n";
               break;
            }
            pRet = pTemp;
            memcpy(pRet->Var.Val.BcdDateTime,&U32,3);

         // Day of month
            pRet->Var.Val.BcdDateTime[3] = (U32a >> 16) & 0xff;
         // Month
            pRet->Var.Val.BcdDateTime[4] = (U32a >> 24) & 0xff;
         // Year LSB
            pRet->Var.Val.BcdDateTime[5] = U32a & 0xff;
         // Year MSB
            pRet->Var.Val.BcdDateTime[6] = (U32a >> 8) & 0xff;
            break;

         case WASP_VARTYPE_STRING:
            pRet = pTemp;
            pRet->Var.Val.String = strdup(cp);
            break;

         case WASP_VARTYPE_UINT32:
            printf("Arg: %s\n",arg);
            if(sscanf(arg,"%u",&U32) != 1) {
               break;
            }
            pRet = pTemp;
            pRet->Var.Val.U32 = U32;
            break;

         case WASP_VARTYPE_INT8:
            if(sscanf(arg,"%d",&i32) != 1 || i32 > 127 || i32 < -128) {
               break;
            }
            pRet = pTemp;
            pRet->Var.Val.I8 = (signed char) i32;
            break;

         case WASP_VARTYPE_INT16:
            if(sscanf(arg,"%d",&i32) != 1 || i32 > 32767 || i32 < -32768) {
               break;
            }
            pRet = pTemp;
            pRet->Var.Val.I16 = (signed short) i32;
            break;

         case WASP_VARTYPE_INT32:
            if(sscanf(arg,"%d",&i32) != 1) {
               break;
            }
            pRet = pTemp;
            pRet->Var.Val.I32 = i32;
            break;

         default:
            printf("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
            break;
      }
   } while(FALSE);

   if(pRet == NULL) {
      if(ErrStr != NULL) {
         printf(ErrStr,WASP_VarTypes[Type],arg);
      }
      if(pTemp != NULL) {
         free(pTemp);
      }
   }

   return pRet;
}

void FreeSetVars()
{
   TestSetVar *p = gSetVars;

   while(p != NULL) {
      gSetVars = p->Link;
      free(p);
      p = gSetVars;
   }
}

int BcdCheck(u32 U32)
{
   int i;
   int Ret = 0;   // assume the best

   for(i = 0; i < 8; i++) {
      if(((U32 >> (i*4)) & 0xf) > 9) {
         printf("Returning fail, i: %d, U32: 0x%x",i,U32);
         Ret = 1;
         break;
      }
   }

   return Ret;
}

void JardenTx(unsigned char *Data,int Length)
{
   WaspAsyncSendRaw(gAsyncCtrl,Data,Length);
}

// Async data from device
// Cmd, Rcode, VarID, data...
int AsyncDataHandler(HandlerArgs *p)
{
   int Ret = 0;
   int DataLen = -1;
   WaspVariable *pVar = NULL;
   u8 VarID;
   DeviceVar *pDevVar;
   VarLink *pVarLink = NULL;
   VarLink *pNew = NULL;
   VarLink *pLastVarLink = NULL;
   u8 *pVal = &p->Msg[3];

   do {
      if(p->MsgLen < 4) {
         LOG("%s: Ignoring %d byte runt async message from device.\n",
             __FUNCTION__,DataLen);
         break;
      }

      if(p->Msg[1] != WASP_OK) {
         LOG("%s: Device returned error 0x%x with %d bytes of data.\n",
             __FUNCTION__,p->Msg[1],DataLen);
         Ret = -p->Msg[1];
         break;
      }
      DataLen = p->MsgLen - 3;
      VarID = p->Msg[2];

      if((pDevVar = FindDevVar(VarID)) == NULL) {
         LOG("%s: Ignoring unknown variable 0x%x.\n",__FUNCTION__,VarID);
         break;
      }

      if(pDevVar->Desc.PollRate != WASP_ASYNC_UPDATE) {
         LOG("%s: Error variable 0x%x is not Async.\n",__FUNCTION__,VarID);
         break;
      }

      if(gQueueLen == 0 && (gWASP_Verbose & LOG_V1)) {
         LOG("%s: Tossing data, gQueueLen == 0.\n",__FUNCTION__);
         break;
      }

      if(pDevVar->pVars->pVar->State == VAR_VALUE_UNKNOWN) {
      // Set the value of the variable at the head of the queue
         Ret = WriteVarValue(&pVal,p->MsgLen-3,pDevVar->pVars->pVar);
         if(Ret == 0) {
            pDevVar->pVars->pVar->State = VAR_VALUE_CACHED;
         }
         break;
      }

      if(pDevVar->NumValues >= gQueueLen && (gWASP_Verbose & LOG_V1)) {
         LOG("%s: Tossing data, queue full with %d entries.\n",
             __FUNCTION__,pDevVar->NumValues);
         pDevVar->QueueOvfl++;
         break;
      }

   // put new data at end of queue
      if((pVar = CreateWaspVariable()) == NULL) {
         LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
         Ret = ENOMEM;
         break;
      }

      memset(pVar,0,sizeof(*pVar));
      pVar->ID = VarID;
      pVar->Type = pDevVar->Desc.Type;

      if((Ret = WriteVarValue(&pVal,p->MsgLen-3,pVar)) != 0) {
      // Something failed
         break;
      }
      pVar->State = VAR_VALUE_CACHED;

      if((pNew = (VarLink *) malloc(sizeof(VarLink))) == NULL) {
         LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
         Ret = ENOMEM;
         break;
      }
      pNew->pVar = pVar;
      pNew->Link = NULL;
      pVarLink = pDevVar->pVars;

   // Find the end of the list
      while(pVarLink != NULL) {
         pLastVarLink = pVarLink;
         pVarLink = pVarLink->Link;
      }
      pLastVarLink->Link = pNew;
      pDevVar->NumValues++;
   } while(FALSE);

   if(Ret != 0) {
   // Error, clean up
      if(pNew != NULL) {
         free(pNew);
      }
      if(pVar != NULL) {
         free(pVar);
      }
      if(pVarLink != NULL) {
         free(pVarLink);
      }
   }
   return Ret;
}


WaspVariable *CreateWaspVariable(void)
{
   WaspVariable *pVar = (WaspVariable *) malloc(sizeof(WaspVariable));

   if(pVar != NULL) {
      memset(pVar,0,sizeof(*pVar));
      pVar->State = VAR_VALUE_UNKNOWN;
   }

   return pVar;
}

DeviceVar *CreateDeviceVar(int DescLen)
{
   WaspVariable *pVar = CreateWaspVariable();
   VarLink *pVars = (VarLink *) malloc(sizeof(VarLink));
   int DeviceVarLen = sizeof(DeviceVar) - sizeof(WaspVarDesc) + DescLen;
   DeviceVar *pDevVar = (DeviceVar *) malloc(DeviceVarLen);

   if(pVar == NULL || pDevVar == NULL || pVars == NULL) {
      LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
      if(pVar != NULL) {
         free(pVar);
      }
      if(pDevVar != NULL) {
         free(pDevVar);
      }
      if(pDevVar != NULL) {
         free(pDevVar);
         pDevVar = NULL;
      }
   }
   else {
      memset(pDevVar,0,sizeof(*pDevVar));
      pVars->pVar = pVar;
      pVars->Link = NULL;
      pDevVar->pVars = pVars;
      pDevVar->NumValues = 1;
      pDevVar->QueueOvfl = 0;
   }

   return pDevVar;
}

int CreateStdVars()
{
   int Ret = 0;   // assume the best
   DeviceVar *pDevVar;
   WaspVariable *pVar;
   int i;

   for(i = 0; StandardVars[i].ID != 0; i++) {
      if((pDevVar = CreateDeviceVar(sizeof(WaspVarDesc))) == NULL) {
         LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
         Ret = -ENOMEM;
         break;
      }
      pVar = pDevVar->pVars->pVar;
      pVar->ID = pDevVar->Desc.ID = StandardVars[i].ID;
      pVar->Type = pDevVar->Desc.Type = StandardVars[i].Type;
      pDevVar->Desc.Usage = StandardVars[i].Usage;
      strcpy(pDevVar->Desc.Name,StandardVars[i].Name);
      Variables[i] = pDevVar;
   }
   return Ret;
}

void SetStartupState(WaspAsyncCtrl *p,WaspState NewState)
{
   u8 Cmd[5];

   if(NewState == 0) {
      gWaspdState++;
   }
   else {
      gWaspdState = NewState;
   }

   if(gWaspdState == 0 || gWaspdState >= NUM_STATES) {
      LOG("%s: Error invalid gWaspdState (%d)\n",__FUNCTION__,gWaspdState);
      gWaspdState = LINK_RESET;
   }

   if(gWASP_Verbose & LOG_STATE) {
      LOG("%s: set gWaspdState to %s (%d)\n",__FUNCTION__,
          State2Text[gWaspdState-1],gWaspdState);
   }

   switch(gWaspdState) {
      case LINK_RESET:
         g_bClockTimeSet = FALSE;   // Assume client is starting or has reset
      // Intentional fall thru to SEND_NOP case
      case SEND_NOP:
         p->TxSeqNum = 0;
      // Intentional fall thru to TEST_NEW_BAUDRATE case
      case TEST_NEW_BAUDRATE:
         Cmd[0] = WASP_CMD_NOP;
         WaspAsyncSendCmd(p,Cmd,1,StartNopHandler,NULL);
         break;

      case GET_DEVICE_WASP_VER:
         Cmd[0] = WASP_CMD_GET_VALUE;
         Cmd[1] = WASP_VAR_WASP_VER;
         WaspAsyncSendCmd(p,Cmd,2,StartValHandler,NULL);
         break;

      case GET_BAUDRATES:
      // WASP_CMD_GET_BAUDRATES was added in WASP 1.06
         if(gClientVersion >= 106) {
            Cmd[0] = WASP_CMD_GET_BAUDRATES;
            WaspAsyncSendCmd(p,Cmd,1,StartBaudRateHandler,NULL);
            break;
         }
         else {
         // Skip to GET_DEVICE_VARS
            if(gWASP_Verbose & LOG_V) {
               LOG("%s: Skipping baudrate negotiation, client is compatible "
                   "with WASP %s.\n",__FUNCTION__,gDeviceWaspVersion);
            }
            SetStartupState(p,GET_DEVICE_VARS);
         }
         break;

      case GET_DEVICE_VARS:
         Cmd[0] = WASP_CMD_GET_VAR_ATTRIBS;
         Cmd[1] = gDevVar;
         WaspAsyncSendCmd(p,Cmd,2,StartVarDescHandler,NULL);
         break;

      case GET_DEVICE_ENUMS:
      // Look for enum variables
         gDevVar = WASP_VAR_DEVICE;
         gWaspdState++;
         StartGetNextEnum(p);
         break;

      case GET_DEVICE_BUTTONS:
         Cmd[0] = WASP_CMD_GET_BUTTONS;
         WaspAsyncSendCmd(p,Cmd,1,StartButtonsHandler,NULL);
         break;

      case READ_ALL_VARS:
      // Read all of the variables defined 
         if(gWASP_Verbose & LOG_V1) {
            LOG("%s: Reading all variables.\n",__FUNCTION__);
         }
         if(gPseudoVars > 0) {
            if(gWASP_Verbose & LOG_PSEUDO) {
               LOG("%s: Calling RestorePseudoVars()\n",__FUNCTION__);
            }
            RestorePseudoVars();
         }
         ReadVariables(READ_ALL);
         break;

      case ENABLE_ASYNC:
         if(gAsyncVars > 0 && gQueueLen > 0) {
            if(gWASP_Verbose & LOG_V1) {
               LOG("%s: Enabling transmission of ASYNC variables.\n",
                   __FUNCTION__);
            }
            Cmd[0] = WASP_CMD_ASYNC_ENABLE;
            Cmd[1] = 1;
            WaspAsyncSendCmd(p,Cmd,2,StartEnableAsyncHandler,NULL);
         }
         else {
         // No ASYNC variables, or not ready to handle them yet
            if(gWASP_Verbose & LOG_V1) {
               LOG("%s: Transmission of ASYNC variables NOT enabled, "
                   "QueueLen = 0\n",__FUNCTION__);
            }
            SetStartupState(p,NEXT_STATE);
         }
         break;

      case READY:
         if(gPollingEnabled) {
            StartPolling();
         }
         break;

      case SET_NEW_BAUDRATE:
      case RESET_BAUDRATE:
         Cmd[0] = WASP_CMD_SET_BAUDRATE;
         memcpy(&Cmd[1],&gBestBaudRate,sizeof(gBestBaudRate));
         WaspAsyncSendCmd(p,Cmd,5,SetBaudRateHandler,&gBestBaudRate);
         break;

      default:
         LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
         gWaspdState = READY;
         break;
   }
}

void CallResponseHandler(WaspMsgHandler *pHandler,HandlerArgs *pArgs)
{
   WaspAsyncCtrl *p = pArgs->p;

   if(pArgs->Err == WASP_ERR_NO_RESPONSE) {
      LOG("%s: Error retry count exceeded.\n",__FUNCTION__);
      if(p->Baudrate != WASP_DEFAULT_BAUDRATE) {
      // restart the WASP baudrate back to default
         if(WaspAsyncSetBaudRate(p,WASP_DEFAULT_BAUDRATE) != 0) {
            LOG("%s: WaspAsyncSetBaudRate failed\n",__FUNCTION__);
         }
      }
      SetStartupState(gAsyncCtrl,LINK_RESET);
   }
   else {
      pHandler(pArgs);
   }
}

void FreeDeviceVar(DeviceVar *pDevVar)
{
   VarLink *pVars = pDevVar->pVars;
   VarLink *pNext;
   int i;

   while(pVars != NULL) {
      WASP_FreeValue(pVars->pVar);
      free(pVars->pVar);
      pNext = pVars->Link;
      free(pVars);
      pVars = pNext;
   }

   if((pDevVar->Desc.ID & WASP_VAR_DEVICE) && IS_TYPE_ENUM((&pDevVar->Desc))) {
   // Free the enum names
      for(i = 0; i <= pDevVar->Desc.MaxValue.U8; i++) {
         free(pDevVar->Desc.Enum.Name[i]);
      }
   }

   free(pDevVar);
}

void FreeTimers()
{
   Timer *p = gTimerHead;
   Timer *pNext;

   while(p != NULL) {
      pNext = p->Link;
      free(p);
      p = pNext;
   }
}

int ClockTimeSetHandler(HandlerArgs *p)
{
   int MsgLen = p->MsgLen;

   do {
      if(MsgLen < 2) {
         break;
      }

      if(p->Msg[1] == WASP_OK) {
      // Set a timer to send another update in a while
         SetTimerCallback(SendClockTime,CLOCK_SET_TIME,p);
      }
      else {
         LOG("%s: device returned %s (0x%x).\n",__FUNCTION__,
             WASP_strerror(-p->Msg[1]),p->Msg[1]);
      }
   } while(FALSE);

   return 0;
}

void SendClockTime(void *p)
{
   HandlerArgs *pArgs = (HandlerArgs *) p;
   u8 Cmd[9] = {WASP_CMD_SET_VALUE,WASP_VAR_CLOCK_TIME};
   struct tm *pTm = localtime(&gTimeNow.tv_sec);
   int Year = pTm->tm_year + 1900;

   LOG("%s: Setting WASP_VAR_CLOCK_TIME to %d/%02d/%d %d:%02d:%02d\n",
       __FUNCTION__,pTm->tm_mon+1,pTm->tm_mday,Year,
       pTm->tm_hour,pTm->tm_min,pTm->tm_sec);

   Cmd[2] = DEC2BCD(pTm->tm_sec);
   Cmd[3] = DEC2BCD(pTm->tm_min);
   Cmd[4] = DEC2BCD(pTm->tm_hour);

   Cmd[5] = DEC2BCD(pTm->tm_mday);
   Cmd[6] = DEC2BCD(pTm->tm_mon+1);
   Cmd[7] = DEC2BCD(Year % 100);
   Cmd[8] = DEC2BCD(Year / 100);

   WaspAsyncSendCmd(pArgs->p,Cmd,9,ClockTimeSetHandler,pArgs);
}

void SendChangedVariable()
{
   int i;
   int j = gLastChangedVar;
   int DataLen;
   DeviceVar *pDevVar;
   WaspApiMsg Reply;
   WaspApiMsg *pReply = &Reply;

#ifdef DEBUG
// Keep Valgrind happy
   memset(&Reply,0,sizeof(Reply));
#endif

   if(j < NUM_STANDARD_VARS) {
      j = NUM_STANDARD_VARS;
   }
   for(i = 0; i < gDevVars; i++) {
      if(Variables[j] == NULL) {
         j = NUM_STANDARD_VARS;
      }
      pDevVar = Variables[j];

      if(pDevVar->pVars->pVar->Pseudo.bChanged) {
         // Return cached value
         if(gWASP_Verbose & LOG_VAR_CHANGE) {
            LOG("%s: Variable 0x%x changed value\n",__FUNCTION__,
                pDevVar->Desc.ID);
            if(gWASP_Verbose & LOG_V) {
               WASP_DumpVar(pDevVar->pVars->pVar);
            }
         }
         pDevVar->pVars->pVar->Pseudo.bChanged = FALSE;
         AsyncReq *pAsync = gGetChangeRequest;
         gGetChangeRequest = NULL;
         DataLen = Var2Ipc(pDevVar->pVars->pVar,&pReply);
         gLastChangedVar = j;
         pReply->Hdr.Err = 0;
         pReply->Hdr.Cmd = pAsync->pRequest->Hdr.Cmd | CMD_RESPONSE;
         UdpSend(gIpcSocket,&pAsync->HisAdr,pReply,sizeof(Reply.Hdr) + DataLen);
         if(pAsync != NULL) {
            free(pAsync);
         }
         if(pReply != &Reply && pReply != NULL) {
            free(pReply);
         }
         break;
      }
      j++;
   }
}

