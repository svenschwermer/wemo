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
#ifndef _CROCKPOTD_H_
#define _CROCKPOTD_H_

#ifndef WASPD_PRIVATE
#error "This header is private to the HAL layer, application code should NOT include it"
#endif

#include "log.h"

#ifdef DEBUG
void DebugBreak(const char *Function,int Line);
#define DEBUG_BREAK DebugBreak(__FUNCTION__,__LINE__)
#else
#define DEBUG_BREAK
#endif


#ifndef FALSE
#define FALSE     0
#endif

#ifndef TRUE
#define TRUE      (!FALSE)
#endif

#define WASPD_PORT   2667

#define MAX_DEVICE_VARS       128

#define LOG(format, ...) LogIt(gWASP_Log,format,## __VA_ARGS__ )
#define DumpHex(data,len) LogHex(gWASP_Log,data,len)

#define CMD_RESPONSE          0x8000

#define CMD_EXIT              1
#define CMD_EXITING           (CMD_EXIT | CMD_RESPONSE)

#define CMD_GET_FW_VERSION    2
#define CMD_FW_VERSION        (CMD_GET_FW_VERSION | CMD_RESPONSE)

#define CMD_SEND_NOP          3
#define CMD_NOP               (CMD_SEND_NOP | CMD_RESPONSE)

#define CMD_GET_VAR_LIST      4
#define CMD_VAR_LIST          (CMD_GET_VAR_LIST | CMD_RESPONSE)

#define CMD_GET_VAR           5
#define CMD_VAR               (CMD_GET_VAR | CMD_RESPONSE)

#define CMD_GET_VARS          6
#define CMD_VARS              (CMD_GET_VARS | CMD_RESPONSE)

#define CMD_GET_CHUNK         7
#define CMD_CHUNK             (CMD_GET_CHUNK | CMD_RESPONSE)

#define CMD_SET_VAR           8
#define CMD_VAR_SET           (CMD_SET_VAR | CMD_RESPONSE)

#define CMD_SET_VARS          9
#define CMD_VARS_SET          (CMD_SET_VARS | CMD_RESPONSE)

#define CMD_SET_CHUNK         10
#define CMD_CHUNK_SET         (CMD_SET_CHUNK | CMD_RESPONSE)

#define CMD_GET_BUTTON        11
#define CMD_BUTTON            (CMD_GET_BUTTON | CMD_RESPONSE)

#define CMD_GET_SELFTEST      12
#define CMD_SELFTEST          (CMD_GET_SELFTEST | CMD_RESPONSE)

#define CMD_SET_LEDSCRIPT     13
#define CMD_LEDSCRIPT_SET     (CMD_SET_LEDSCRIPT | CMD_RESPONSE)

#ifdef OLD_API
#define CMD_SET_COOKTIME      14
#define CMD_COOKTIME_SET      (CMD_SET_COOKTIME | CMD_RESPONSE)

#define CMD_GET_COOKTIME      15
#define CMD_COOKTIME          (CMD_GET_COOKTIME | CMD_RESPONSE)

#endif

#define CMD_SET_ASYNC_Q_LEN   16
#define CMD_ASYNC_Q_LEN_SET   (CMD_SET_ASYNC_Q_LEN | CMD_RESPONSE)

#define CMD_GET_CHANGED_VAR   17
#define CMD_CHANGED_VAR       (CMD_GET_CHANGED_VAR | CMD_RESPONSE)



// Internal waspd usage
#define CMD_INTERNAL          64

#define CMD_POLL_VARIABLES    CMD_INTERNAL

#define MAX_VERSION_LEN 16

#define LOG_V              0x00000001  // old -v
#define LOG_V1             0x00000002  // old -v -v
#define LOG_V2             0x00000004  // old -v -v -v
#define LOG_SERIAL         0x00000008
#define LOG_IPC            0x00000010
#define LOG_API_CALLS      0x00000020
#define LOG_PSEUDO         0x00000040
#define LOG_STATE          0x00000080
#define LOG_VAR_CHANGE     0x00000100
#define LOG_RAW_SERIAL     0x80000000

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
struct WaspAsyncCtrl_TAG;

typedef struct {
   struct WaspAsyncCtrl_TAG *p;
   u8 *Msg;
   int MsgLen;
   int Err;    // transport error
   void *Data;
} HandlerArgs;

typedef int (WaspMsgHandler)(HandlerArgs *p);

typedef struct {
   u8 Cmd;
   WaspMsgHandler *pCmdHandler;
} CmdTableEntry;

typedef struct {
   u16      Cmd;
   int      Err;
} GCC_PACKED MsgHeader;

typedef struct {
   WaspVariable WaspVar;      // NB: must be first
   VarID Id;
   char Name[MAX_VAR_NAME_LEN];
} GCC_PACKED VarReference;

typedef struct {
   u8 bLive;
   u8 Count;      // number of entries in Id array
   VarID IDs[];
} GCC_PACKED GetVarsList;

typedef struct {
   MsgHeader Hdr;
   union {
      int IntData;
      char Version[MAX_VERSION_LEN];   // Vx.xx<null>
      WaspVariable WaspVar;
      VarReference VarRef;
      GetVarsList GetVars;
      unsigned char U8;
   } u;
} GCC_PACKED WaspApiMsg;

// a type to make sockaddr/sockaddr_in translations a bit easier
typedef union {
   struct sockaddr s;
   struct sockaddr_in i;
   #define ADDR   i.sin_addr.s_addr
   #define PORT   i.sin_port
} AdrUnion;

typedef struct {
   AdrUnion HisAdr;
   WaspApiMsg *pRequest;
} AsyncReq;

typedef struct {
   unsigned char Count; // number of entries in LedStates
   LedID Id;
   LedState LedStates[];
} GCC_PACKED SetLedScriptArgs;

typedef struct {
   int *Count;
   WaspVarDesc ***pVarDescArray;
} VarListArgs;

#define BUTTON_BUF_LEN  16
typedef struct {
   int Rd;
   int Wr;
   u8 Buf[BUTTON_BUF_LEN];
} ButtonRing;

typedef void (TimerCallback)(void *pData);
typedef struct Timer_TAG {
   struct Timer_TAG *Link; // NB must be first
   struct timeval When;
   TimerCallback *pCallBack;
   void *pData;
} Timer;

typedef struct VarLink_TAG VarLink;
struct VarLink_TAG {
   VarLink *Link;    // NB: must be first
   WaspVariable *pVar;
};

typedef struct DeviceVar_TAG DeviceVar;
struct DeviceVar_TAG {
   int         NumValues;  // number of values pointed to by pVars
   int         QueueOvfl;
   VarLink      *pVars;
   WaspVarDesc  Desc;   // NB: must be last (not really a fixed size!)
};

extern int gWASP_Verbose;
extern int gDebug;
extern int gSimulated;
extern int gWASP_LogLevelSet;
extern void *gWASP_Log;

extern CmdTableEntry CmdTable[];
extern int gJardenMode;

extern DeviceVar *Variables[];
extern struct timeval gTimeNow;
extern int g_bTimeSynced;

typedef struct {
   int   Mask;
   const char *Desc;
} DebugMaskVal;

extern DebugMaskVal DebugMasks[];
extern const char *WASP_VarTypes[];
extern const char *WASP_ValStates[];

struct WaspAsyncCtrl_TAG;

int TimeLapse(struct timeval *p);
void SetTimeout(struct timeval *p,int Timeout);
void WaspDispatchCommand(HandlerArgs *p);
int CmdNopHandler(HandlerArgs *p);
int SetTimerCallback(TimerCallback Callback,int Timeout,void *Data);
int CancelTimerCallback(TimerCallback pCallback,void *pData);
void CallResponseHandler(WaspMsgHandler *pHandler,HandlerArgs *pArgs);
int WriteVarValue(u8 **ppRaw,int DataLen,WaspVariable *pVar);
void WASP_DumpVar(WaspVariable *p);
WaspVariable *FindVariable(u8 ID);

#ifdef OLD_API
// functions in old_api.c
int _GetCookTime(void);
int _SetCookTime(int Minutes);
void _VariableSetHook(WaspVariable *pVar);
void _CheckCookTime(void);
#endif

#if defined(CROCKPOT_HW) || defined(OLD_API)
#include <crockpot.h>
#include "wasp_crockpot.h"
CROCKPOT_MODE _WaspMode2OldMode(int WaspMode);
int _OldMode2WaspMode(CROCKPOT_MODE Mode);
#endif
void JardenTx(unsigned char *Data,int Length);

// Functions in wasp_api.c not intended to be used from outside of waspd
void WASP_LogLogLevel(int LevelMask);
void WASP_ReadLogLevel(void);

#endif   // _CROCKPOTD_H_

