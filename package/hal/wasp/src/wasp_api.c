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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "wasp.h"
#include "wasp_api.h"
#define WASPD_PRIVATE
#include "waspd.h"

#ifdef i386
#define  API_LOG_PATH   "api.log"
#define LOG_LEVEL_PATH  "wasp_debug_level"
#else
#define  API_LOG_PATH   "/tmp/api.log"
#define LOG_LEVEL_PATH  "/tmp/Belkin_settings/wasp_debug_level"
#endif

#define API_LOG if(gWASP_Verbose & LOG_API_CALLS) LOG
#define LOG_RESULT() if(gWASP_Verbose & LOG_API_CALLS) \
                        LogResult(__FUNCTION__,Ret)
#define API_LOGV ((gWASP_Verbose & LOG_API_CALLS) && (gWASP_Verbose & LOG_V))


typedef struct {
   VarReference VarRef; // NB must be first
   WaspVariable *pVar;
} VarReferenceArgs;

typedef struct {
   int Count;
   int bLive;
   WaspVariable **pVarArray;
} VarsListArgs;

typedef struct {
   WaspVariable *pVarArray;
   int Count;
} SetVarsArgs;

void *gWASP_Log = NULL;
int gWASP_LogLevelSet = FALSE;
int gWASP_Verbose = 0;

// public globals
int WASP_VarType2Len[] = {
   -1,   // not used
   1,    // WASP_VARTYPE_ENUM
   2,    // WASP_VARTYPE_PERCENT
   2,    // WASP_VARTYPE_TEMP
   4,    // WASP_VARTYPE_TIME32
   2,    // WASP_VARTYPE_TIME16
   3,    // WASP_VARTYPE_TIMEBCD
   1,    // WASP_VARTYPE_BOOL
   4,    // WASP_VARTYPE_BCD_DATE
   7,    // WASP_VARTYPE_DATETIME
   -1,   // WASP_VARTYPE_STRING
   -1,   // WASP_VARTYPE_BLOB
   1,    // WASP_VARTYPE_UINT8
   1,    // WASP_VARTYPE_INT8
   2,    // WASP_VARTYPE_UINT16
   2,    // WASP_VARTYPE_INT16
   4,    // WASP_VARTYPE_UINT32
   4,    // WASP_VARTYPE_INT32
   2     // WASP_VARTYPE_TIME_M16
};

const char *WASP_VarTypes[] = {
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

const char *WASP_ValStates[] = {
   "Not set",        // VAR_VALUE_UNKNOWN
   "Cached value",   // VAR_VALUE_CACHED:
   "Live value",     // VAR_VALUE_LIVE
   "Set Value",      // VAR_VALUE_SET
   "Value being Set" // VAR_VALUE_SETTING
};


// Internal static globals
static __thread int gSocket = -1;
static AdrUnion gHisAdr;

static const char *ErrStr[] = {
   "WASP_ERR_INVALID_RESPONSE_TYPE",
   "WASP_ERR_DAEMON_TIMEOUT",
   "WASP_ERR_COMM_ERR",
   "WASP_ERR_NOT_READY",
   "WASP_ERR_INVALID_VARIABLE",
   "WASP_ERR_READONLY",
   "WASP_ERR_INVALID_VALUE",
   "WASP_ERR_MORE_DATA",
   "WASP_ERR_NO_DATA",
   "WASP_ERR_NOT_SUPPORTED",
   "WASP_ERR_WASPD_INTERNAL",
   "WASP_ERR_WRONG_TYPE",
   "WASP_ERR_INVALID_TYPE",
   "WASP_ERR_INVALID_TYPE",
   "WASP_ERR_WAIT_BUSY"
};

static const char *WaspErrStr[] = {
   "No Error",
   "WASP_ERR_CMD",
   "WASP_ERR_ARG",
   "WASP_ERR_PERM",
   "WASP_ERR_COMM",
   "WASP_ERR_MORE",
   "WASP_ERR_OVFL",
   "WASP_ERR_INTERNAL",
   "WASP_ERR_DATA_NOTRDY"
};

DebugMaskVal DebugMasks[] = {
   {LOG_V,"old -v"},
   {LOG_V1,"old -v -v"},
   {LOG_V2,"old -v -v -v"},
   {LOG_SERIAL,"WASP messages"},
   {LOG_IPC,"IPC"},
   {LOG_API_CALLS,"Api calls"},
   {LOG_PSEUDO,"Pseudo variables"},
   {LOG_STATE,"waspd internal state changes"},
   {LOG_VAR_CHANGE,"Changed variable reporting"},

   {LOG_RAW_SERIAL,"Raw serial data"},
   {0},  // end of table
};


// Internal functions
static int UdpSend(int fd,AdrUnion *pToAdr,WaspApiMsg *pMsg,int MsgLen);
static int GetDataFromDaemon(int Type,void *p);
static int PurgeUdp(void);
static void LogResult(const char *Func,int Ret);

#ifdef DEBUG
void DebugBreak(const char *Function,int Line)
{
   printf("%s called from %s#%d\n",__FUNCTION__,Function,Line);
}
#endif

static int UdpSend(int fd,AdrUnion *pToAdr,WaspApiMsg *pMsg,int MsgLen)
{
   int Ret = 0;   // assume the best
   int BytesSent;

   BytesSent = sendto(fd,pMsg,MsgLen,0,&pToAdr->s,sizeof(pToAdr->s));
   if(BytesSent != MsgLen) {
      LOG("%s: sendto failed - %s\n",__FUNCTION__,strerror(errno));
      Ret = -errno;
   }

   return Ret;
}

static int GetDataFromDaemon(int Type,void *p)
{
   int Ret = WASP_OK;
   int Err;
   int BytesRead;
   int ReplySize; 
   int ReturnedData;
   int DataLen = 0;
   fd_set ReadFdSet;
   struct timeval Timeout;
   WaspApiMsg Request;
   WaspApiMsg Reply;
   socklen_t Len = sizeof(gHisAdr);
   WaspApiMsg *pReply = &Reply;
   WaspApiMsg *pRequest = &Request;
   int MsgLen = sizeof(Request.Hdr);
   static __thread int Recurse = 0;

   if(Recurse++ != 0) {
      LOG("%s: Recursion error!  Recurse: %d\n",__FUNCTION__,Recurse);
   }

   Timeout.tv_sec = 5;
   Timeout.tv_usec = 0;

   do {
      if(p == NULL) {
         // Make sure we were passed a return data pointer
         Ret = EINVAL;
         LOG("%s: ERROR - passed null pointer\n",__FUNCTION__);
         break;
      }

      memset(&Request,0,MsgLen);

      if(gSocket < 0) {
         if((Ret = WASP_Init()) != 0) {
            LOG("%s: WASP_Init failed: %s\n",__FUNCTION__,WASP_strerror(Ret));
            break;
         }
         LOG("%s: Called WASP_Init\n",__FUNCTION__);
      }

      switch(Type) {
         case CMD_SET_VAR:
         case CMD_GET_VAR: {
            VarReferenceArgs *pVarRefArgs = (VarReferenceArgs *) p;
            Request.u.VarRef = pVarRefArgs->VarRef;
            DataLen = sizeof(Request.u.VarRef);
            break;
         }

         case CMD_GET_VARS: {
            int i;
            VarsListArgs *pArgs = (VarsListArgs*) p;
            int Count = pArgs->Count;
            MsgLen += sizeof(pRequest->u.GetVars) + Count;

            if(MsgLen > sizeof(*pRequest)) {
               if((pRequest = (WaspApiMsg *) malloc(MsgLen)) == NULL) {
                  LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
                  Ret = ENOMEM;
                  break;
               }
            }
            pRequest->u.GetVars.Count = Count;
            pRequest->u.GetVars.bLive = (u8) pArgs->bLive;

         // Copy the requested IDs into the request
            for(i = 0; i < Count; i++) {
               pRequest->u.GetVars.IDs[i] = pArgs->pVarArray[i]->ID;
            }
            break;
         }

         case CMD_SET_LEDSCRIPT: {
            SetLedScriptArgs *pArgs = (SetLedScriptArgs *) p;
            int Len;

            DataLen = sizeof(*pArgs) + (pArgs->Count * sizeof(LedState));
            Len = sizeof(MsgHeader) + DataLen;

            if((pRequest = (WaspApiMsg *) malloc(Len)) == NULL) {
               LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
               Ret = ENOMEM;
               break;
            }
            memcpy(&pRequest->u,pArgs,DataLen);
            break;
         }
#ifdef OLD_API
         case CMD_SET_COOKTIME:
            Request.u.IntData = *((int *) p);
            DataLen = sizeof(Request.u.VarRef);
            break;
#endif

         case CMD_SET_VARS: {
            SetVarsArgs *pArgs = (SetVarsArgs *) p;
            int i;
            void *vp;
            int Len;
            int Partial;

            MsgLen += sizeof(pRequest->u.IntData);
            for(i = 0; i < pArgs->Count; i++) {
               MsgLen += WASP_GetVarLen(&pArgs->pVarArray[i]);
            }

            if(MsgLen > sizeof(*pRequest)) {
               if((pRequest = (WaspApiMsg *) malloc(MsgLen)) == NULL) {
                  LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
                  Ret = ENOMEM;
                  break;
               }
            }
            pRequest->u.IntData = pArgs->Count;

            vp = (void *) &pRequest->u.IntData + sizeof(pRequest->u.IntData);

         // Copy the variables to set into the request
            for(i = 0; i < pArgs->Count; i++) {
               Len = WASP_GetVarLen(&pArgs->pVarArray[i]);
               switch(pArgs->pVarArray[i].Type) {
                  case WASP_VARTYPE_STRING: {
                     Partial = sizeof(WaspVariable) - sizeof(WaspValUnion);
                     memcpy(vp,&pArgs->pVarArray[i],Partial);
                     vp += Partial;
                     Len -= Partial;
                     memcpy(vp,&pArgs->pVarArray[i].Val.String,Len);
                     break;
                  }

                  case WASP_VARTYPE_BLOB:
                     Partial = sizeof(WaspVariable) - sizeof(WaspValUnion) +
                               sizeof(int);
                     memcpy(vp,&pArgs->pVarArray[i],Partial);
                     vp += Partial;
                     Len -= Partial;
                     memcpy(vp,&pArgs->pVarArray[i].Val.Blob.Data,Len);
                     break;

                  default:
                     memcpy(vp,&pArgs->pVarArray[i],Len);
                     break;
               }
               vp += Len;
            }
            break;
         }

         case CMD_SET_ASYNC_Q_LEN:
            Request.u.IntData = *((int *) p);
            DataLen = sizeof(Request.u.VarRef);
            break;

         case CMD_GET_CHANGED_VAR:
            Timeout.tv_sec = 0x7fffffff;  // wait forever (more or less)
            break;

         default:
            break;
      }

      if(Ret != WASP_OK) {
         break;
      }

      pRequest->Hdr.Cmd = Type;
      if(DataLen != 0) {
         MsgLen = offsetof(WaspApiMsg,u) + DataLen;
         memcpy(&Request.u,p,DataLen);
      }

      while(PurgeUdp());

      if((Ret = UdpSend(gSocket,&gHisAdr,pRequest,MsgLen)) != 0) {
         break;
      }

// Wait for data then read it
      FD_ZERO(&ReadFdSet);
      FD_SET(gSocket,&ReadFdSet);
   // Fix this !!! If we timeout prematurely the next request
   // will get old data.  We need to add a sequence number to the
   // request and echo it in the reply and then toss any
   // replies that are from earlier requests that timed out.
   // For now ... just make the timeout "large"

      if((Err = select(gSocket+1,&ReadFdSet,NULL,NULL,&Timeout)) < 0) {
         LOG("%s: select failed - %s\n",__FUNCTION__,strerror(errno));
         Ret = -errno;
         break;
      }

      if(Err == 0) {
         LOG("%s: timeout waiting for data\n",__FUNCTION__);
         Ret = WASP_ERR_DAEMON_TIMEOUT;
         break;
      }

      if((Err = ioctl(gSocket,FIONREAD,&ReplySize)) < 0) {
         LOG("%s: ioctl failed - %s\n",__FUNCTION__,strerror(errno));
         Ret = errno;
         break;
      }

      if(ReplySize > sizeof(Reply)) {
         // Response is bigger than standard reply object, allocate
         // a temporary buffer to hold the data
         if((pReply = (WaspApiMsg *) malloc(ReplySize)) == NULL) {
            LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
            Ret = ENOMEM;
            break;
         }
      }
      BytesRead = recvfrom(gSocket,pReply,ReplySize,0,&gHisAdr.s,&Len);

      if(BytesRead < 0) {
         LOG("%s: recvfrom failed - %s\n",__FUNCTION__,strerror(errno));
         break;
      }
      else if(BytesRead == 0) {
         LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
         break;
      }
      else if(BytesRead != ReplySize) {
         LOG("%s: short read, requested %d, got %d\n",
             __FUNCTION__,ReplySize,BytesRead);
      }

      if(pReply->Hdr.Cmd != (Type | CMD_RESPONSE)) {
         // Opps
         LOG("%s: Invalid response, Type: 0x%x:\n",__FUNCTION__,Type);
         DumpHex(pReply,sizeof(*pReply));
         Ret = WASP_ERR_INVALID_RESPONSE_TYPE;
         break;
      }
      ReturnedData = BytesRead-offsetof(WaspApiMsg,u);
      if((Ret = pReply->Hdr.Err) != 0) {
         break;
      }

      switch(Type) {
         case CMD_GET_CHANGED_VAR:
         case CMD_GET_VAR: {
            // Handle blobs and strings
            VarReferenceArgs *pVarRefArgs = (VarReferenceArgs *) p;
            WaspVariable *pVar = pVarRefArgs->pVar;
            VarType VarType = pReply->u.WaspVar.Type;

            if(Type == CMD_GET_VAR) {
               if(pVar->ID != pReply->u.WaspVar.ID) {
                  LOG("%s: Error, requested variable ID 0x%x, got 0x%x\n",
                      __FUNCTION__,pVar->ID,pReply->u.WaspVar.ID);
               }
            }
            else {
            // CMD_GET_CHANGED_VAR
               pVar->ID = pReply->u.WaspVar.ID;
            }

            pVar->Type = VarType;
            pVar->State = pReply->u.WaspVar.State;
            if(VarType == WASP_VARTYPE_STRING) {
               pVar->Pseudo = pReply->u.WaspVar.Pseudo;
               if(pVar->Val.String != NULL) {
                  free(pVar->Val.String);
               }
               pVar->Val.String = strdup((char *) &pReply->u.WaspVar.Val);
               if(pVar->Val.String == NULL) {
                  LOG("%s#%d: strdup failed\n",__FUNCTION__,__LINE__);
                  Ret = ENOMEM;
                  break;
               }
            }
            else if(VarType == WASP_VARTYPE_BLOB) {
               int Len = pReply->u.WaspVar.Val.Blob.Len;

               pVar->Pseudo = pReply->u.WaspVar.Pseudo;
               pVar->Val.Blob.Len = Len;
               if(pVar->Val.Blob.Data != NULL) {
                  free(pVar->Val.Blob.Data);
               }
               if((pVar->Val.Blob.Data = malloc(Len)) == NULL) {
                  LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
                  Ret = ENOMEM;
                  break;
               }
               memcpy(pVar->Val.Blob.Data,
                      &pReply->u.WaspVar.Val.Blob.Data,Len);
            }
            else {
               if(ReturnedData > sizeof(*pVar)) {
                  LOG("%s#%d: Internal error: truncating data "
                      "from %d to %d bytes\n",__FUNCTION__,__LINE__,
                      ReturnedData,sizeof(*pVar));
                  ReturnedData = sizeof(*pVar);
               }
               memcpy(pVar,&pReply->u.WaspVar,ReturnedData);
            }
            ReturnedData = 0; // Data handled
            break;
         }

         case CMD_GET_VARS: {
            int i;
            VarsListArgs *pArgs = (VarsListArgs*) p;
            int Count = pArgs->Count;
            WaspVariable **pVarArray = pArgs->pVarArray;
            unsigned char *cp;
            int ValueLen;
            VarType Type;
            WaspVariable *pVar;
            ReturnedData = 0; // Data will be handled

         // Copy the returned values into the variable array
            cp = &pReply->u.U8;
            for(i = 0; i < Count; i++) {
               pVar = (WaspVariable *) cp;
               if(pVarArray[i]->ID != pVar->ID) {
                  LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
                  break;
               }
               Type = pVar->Type;
               if(Type == 0 || Type > NUM_VAR_TYPES) {
                  LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
                  break;
               }
               pVarArray[i]->Type = Type;
               pVarArray[i]->State = pVar->State;

               ValueLen = WASP_VarType2Len[Type];
               if(ValueLen > 0) {
                  memcpy(&pVarArray[i]->Val,&pVar->Val,ValueLen);
               }
               else switch(Type) {
               // Handle special types
                  case WASP_VARTYPE_STRING:
                     if(pVarArray[i]->Val.String != NULL) {
                        free(pVarArray[i]->Val.String);
                     }
                     pVarArray[i]->Val.String = strdup((char *) &pVar->Val);
                     if(pVarArray[i]->Val.String == NULL) {
                        LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
                        Ret = ENOMEM;
                     }
                     ValueLen = strlen((char *) &pVar->Val) + 1;
                     break;

                  case WASP_VARTYPE_BLOB: {
                     int Len = pVar->Val.Blob.Len;
                     if(pVarArray[i]->Val.Blob.Data != NULL) {
                        free(pVarArray[i]->Val.Blob.Data);
                     }
                     pVarArray[i]->Val.Blob.Len = Len;
                     if((pVarArray[i]->Val.Blob.Data = malloc(Len)) != NULL) {
                        memcpy(pVarArray[i]->Val.Blob.Data,
                               &pVar->Val.Blob.Data,Len);
                     }
                     else {
                        LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
                        Ret = ENOMEM;
                     }
                     ValueLen = sizeof(pVar->Val.Blob.Len) + Len;
                     break;
                  }

                  default:
                     LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
                     break;
               }

               if(Ret != WASP_OK) {
                  break;
               }
               cp += offsetof(WaspVariable,Val) + ValueLen;
            }
            break;
         }

         case CMD_GET_VAR_LIST: {
            VarListArgs *pReq = (VarListArgs *) p;
            int Count = pReply->u.IntData;
            char *rd;
            char *wr;
            int i,j;
            WaspVarDesc **pVarDescs;
            WaspVarDesc *pDesc;
            int MallocSize = ReturnedData - sizeof(int) +
                             (Count * sizeof(WaspVarDesc *));

            *pReq->Count = Count;
            pDesc = (WaspVarDesc *) ((char *) &pReply->u + sizeof(int));

            // Add space for enum name pointers
            for(i = 0; i < Count; i++) {
               MallocSize += pDesc->Enum.Count * sizeof(char *);
               pDesc++;
            }

            if((pVarDescs = (WaspVarDesc **) malloc(MallocSize)) == NULL) {
               LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
               Ret = ENOMEM;
               break;
            }
            *pReq->pVarDescArray = pVarDescs;

            // Copy the data
            rd = (char *) &pReply->u.IntData;
            wr = (char *) pVarDescs + (Count * sizeof(WaspVarDesc *));
            rd += sizeof(int);

            // Fill in the pointers
            for(i = 0; i < Count; i++) {
               pVarDescs[i] = (WaspVarDesc *) wr;
               pDesc = (WaspVarDesc *) rd;
               memcpy(wr,rd,sizeof(WaspVarDesc));
               rd += sizeof(WaspVarDesc);
               wr += sizeof(WaspVarDesc) + 
                     (pDesc->Enum.Count * sizeof(char *));
            }

            // Copy the names
            Len = ReturnedData - (Count * sizeof(WaspVarDesc)) - sizeof(int);
            memcpy(wr,rd,Len);

            // Setup pointers to names
            for(i = 0; i < Count; i++) {
               for(j = 0; j < pVarDescs[i]->Enum.Count; j++) {
                  pVarDescs[i]->Enum.Name[j] = wr;
                  // Skip to the next name
                  wr += strlen(wr) + 1;
               }
            }
            ReturnedData = 0; // Data handled
            break;
         }

         case CMD_GET_BUTTON: {
            *((u8 *) p) = pReply->u.U8;
            break;
         }

         case CMD_GET_SELFTEST: {
            SelfTestResults *pResults;
            int MallocLen = sizeof(SelfTestResults) + ReturnedData -1;

            pResults = (SelfTestResults *) malloc(MallocLen);
            if(pResults == NULL) {
               LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
               Ret = ENOMEM;
               break;
            }
            *((SelfTestResults **) p) = pResults;
            pResults->Len = ReturnedData - 1;
            memcpy(&pResults->Result,&pReply->u,ReturnedData);
            ReturnedData = 0; // Data handled
            break;
         }
#ifdef OLD_API
         case CMD_GET_COOKTIME:
            break;
#endif

         default:
            break;
      }

      if(ReturnedData > 0) {
         if(ReturnedData > sizeof(Reply.u)) {
            LOG("%s: Error: Unable to handle %d bytes data for "
                "request type %d\n",__FUNCTION__,ReturnedData,Type);
            Ret = WASP_ERR_INTERNAL;
            break;
         }
         // Copy returned data
         memcpy(p,&pReply->u.IntData,ReturnedData);
      }
   } while(FALSE);

   if(pReply != &Reply && pReply != NULL) {
      free(pReply);
   }

   if(pRequest != &Request && pRequest != NULL) {
      free(pRequest);
   }

   if(Recurse-- != 1) {
      LOG("%s: Recursion error!  Recurse: %d\n",__FUNCTION__,Recurse);
   }

   return Ret;
}

// Read and toss any old replies
static int PurgeUdp()
{
   int Err;
   int ReplySize; 
   int BytesRead;
   socklen_t Len = sizeof(gHisAdr);
   WaspApiMsg *pReply = NULL;

   do {
      if((Err = ioctl(gSocket,FIONREAD,&ReplySize)) < 0) {
         LOG("%s: ioctl failed - %s\n",__FUNCTION__,strerror(errno));
         break;
      }

      if(ReplySize == 0) {
      // Nothing to read
         break;
      }

      if((pReply = (WaspApiMsg *) malloc(ReplySize)) == NULL) {
         LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
         break;
      }
      BytesRead = recvfrom(gSocket,pReply,ReplySize,0,&gHisAdr.s,&Len);

      if(BytesRead < 0) {
         LOG("%s: recvfrom failed - %s\n",__FUNCTION__,strerror(errno));
         break;
      }

      if(BytesRead == 0) {
         LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
         break;
      }

      if(BytesRead != ReplySize) {
         LOG("%s: short read, requested %d, got %d\n",__FUNCTION__,
             ReplySize,BytesRead);
         break;
      }
   } while(FALSE);

   if(pReply != NULL) {
      LOG("%s: tossing %d byte reply:\n",__FUNCTION__,ReplySize);
      DumpHex(pReply,ReplySize);
      free(pReply);
   }

   return ReplySize;
}


// Public functions

// Initialize Hardware Abstraction Layer for use
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - HAL error code (see WASP_ERR_*)
int WASP_Init()
{
   int Ret = 0;

   if(!gWASP_LogLevelSet) {
   // Try to get log level from file
      WASP_ReadLogLevel();
   }

   if((gWASP_Verbose & LOG_API_CALLS) && gWASP_Log == NULL) {
      gWASP_Log = LogInit(API_LOG_PATH,200,200000,NULL);
      LOG("libWasp compiled "__DATE__ " "__TIME__"\n");
      WASP_LogLogLevel(gWASP_Verbose);
   }

   if((gSocket = socket(AF_INET,SOCK_DGRAM,0)) == -1) {
      Ret = -errno;
      LOG("%s: socket() failed, %s\n",__FUNCTION__,strerror(Ret));
   }

   gHisAdr.i.sin_family = AF_INET;
   gHisAdr.PORT = htons(WASPD_PORT);
   gHisAdr.ADDR = inet_addr("127.0.0.1");   // localhost

   LOG_RESULT();
   return Ret;
}

// Return interface card firmware version number.
// 
// pBuf:
// Pointer to character pointer to receive the value.
// Returned value is dynamically allocated via malloc(), 
// it the responsibility of the caller to free the memory.
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetFWVersionNumber(char **pBuf)
{
   int Ret = 0;   // assume the best
   static char Version[MAX_VERSION_LEN];

   if(pBuf == NULL) {
      Ret = EINVAL;
   }
   else {
      int Err = GetDataFromDaemon(CMD_GET_FW_VERSION,&Version);
      if(Err == 0) {
         *pBuf = strdup(Version);
      }
      else {
         Ret = Err;
      }
   }

   if(Ret == 0 && (gWASP_Verbose & LOG_V)) {
      API_LOG("%s: returning %s\n",__FUNCTION__,*pBuf);
   }
   else {
      LOG_RESULT();
   }
   return Ret;
}

// Shutdown the HAL deamon. 
// 
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - HAL error code (see WASP_ERR_*)
int WASP_Shutdown()
{
   MsgHeader Dummy;
   int Ret = GetDataFromDaemon(CMD_EXIT,&Dummy);

   API_LOG("%s: returning %d\n",__FUNCTION__,Ret);
   return Ret;
}

// Free and release resources used by HAL
// 
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - HAL error code (see WASP_ERR_*)
int WASP_Cleanup()
{
   int Ret = 0;
   API_LOG("%s: called\n",__FUNCTION__);

   if(gSocket >= 0) {
      close(gSocket);
      gSocket = -1;
   }

   LOG_RESULT();
   return Ret;
}

const char *WASP_strerror(int Err)
{
   const char *Ret = NULL;
   int i = (-Err) + WASP_ERR_BASE - 1;
   int j = -Err;

   if(i >= 0 && i < (sizeof(ErrStr) / sizeof(const char*))) {
      Ret = ErrStr[i];
   }
   else if(j >= 0 && j < (sizeof(WaspErrStr) / sizeof(const char*))) {
      Ret = WaspErrStr[j];
   }
   else {
      Ret = strerror(j);
   }

   return Ret;
}

// Get list of supported variables
// 
// int WASP_GetVarList(int *Count,WaspVarDesc (*pVarDescArray)[])
// 
// Count: pointer to an integer to receive the number of variable
// definitions returned in pVarDescList.
// 
// pVarDescArray: pointer to a pointer to an array containing the
// returned variable descriptions.  The returned array in dynamically 
// allocated memory that the caller is responsible for freeing.
// EnumVarNames are allocated within pVarDescArray and do not need
// to be freed separately.
//
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetVarList(int *Count,WaspVarDesc ***pVarDescArray)
{
   int Ret = 0;   // assume the best
   VarListArgs ReqArgs;

   API_LOG("%s: called\n",__FUNCTION__);
   if(pVarDescArray == NULL || Count == NULL) {
      Ret = -EINVAL;
   }
   else {
      ReqArgs.Count = Count;
      ReqArgs.pVarDescArray = pVarDescArray;

      Ret = GetDataFromDaemon(CMD_GET_VAR_LIST,&ReqArgs);
   }
   LOG_RESULT();
   return Ret;
}


// Read specified variable.
//
// int WASP_GetVariable(WaspVariable *pVar);
// 
// pVar: pointer to buffer to receive the value.
// 
// Filled by caller:
// pVar->ID is set by the caller to requested Variable's ID.
// 
// pVar->bLive:
//        true  - get value from interface board
//        false - returned cached value when possible
// 
// Returned:
// pVar->bLive:
//        true  - value from interface board returned
//        false - cached value returned
// 
// If the variable value is returned via a pointer in pVar->Val
// (string, blob) then the pointer is to dynamically allocated 
// memory that the caller is responsible for freeing.
// 
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetVariable(WaspVariable *pVar)
{
   int Ret = 0;   // assume the best

   if(pVar == NULL) {
      Ret = -EINVAL;
   }
   else {
      VarReferenceArgs Args;

      API_LOG("%s: called for ID 0x%x\n",__FUNCTION__,pVar->ID);
      Args.pVar = pVar;
      Args.VarRef.WaspVar = *pVar;
      Args.VarRef.Id = pVar->ID;
      Args.VarRef.Name[0] = 0;

      Ret = GetDataFromDaemon(CMD_GET_VAR,&Args);
   }

   LOG_RESULT();

   if(Ret == 0 && API_LOGV) {
      WASP_DumpVar(pVar);
   }

   return Ret;
}

// Read specified variable.
//
// int WASP_GetVarByName(const char *Name,WaspValUnion *pVar)
// 
// Name: Name of the Variable to get
//
// bLive: true  - get value from interface board
//        false - returned cached value
//
// pVar: pointer to buffer to receive the value.
// If the variable value is returned via a pointer in
// WaspValUnion (string, blob) then the pointer is to dynamically 
// allocated memory that the caller is responsible for freeing.
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetVarByName(const char *Name,WaspVariable *pVar)
{
   int Ret = 0;   // assume the best
   if(pVar == NULL || Name == NULL || strlen(Name) > MAX_VAR_NAME_LEN) {
      Ret = -EINVAL;
   }
   else {
      VarReferenceArgs Args;

      Args.pVar = pVar;
      Args.VarRef.WaspVar = *pVar;
      Args.VarRef.Id = pVar->ID;
      strcpy(Args.VarRef.Name,Name);
      Ret = GetDataFromDaemon(CMD_GET_VAR,&Args);
   }
   LOG_RESULT();
   return Ret;
}

// Read the current value of 2 or more variables.
//
// int WASP_GetVariables(int Count,int bLive,WaspVariable **pVarArray);
//
// Filled by caller:
//    pVarArray:
//       Pointer to an array of pointers to WaspVariables with the ID field
//       filled in.  If the variable value is returned via a pointer in
//       WaspVariable (string, blob) then the pointer is to dynamically 
//       allocated memory that the caller is responsible for freeing.
//       A maximum of one Blob value may be read and if present it must be the 
//       last variable in the array.
// 
//    bLive:
//     0 - returned cached values when possible
//  != 0 - get values from interface board
// 
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetVariables(int Count,int bLive,WaspVariable **pVarArray)
{
   int Ret = WASP_OK;
   VarsListArgs Args;
   Args.Count = Count;
   Args.bLive = bLive;
   Args.pVarArray = pVarArray;

   do {
      if(pVarArray == NULL || Count < 2 || Count > WASP_MAX_DATA_LEN - 1) {
         Ret = -EINVAL;
         break;
      }
      API_LOG("%s: called\n",__FUNCTION__);
      Ret = GetDataFromDaemon(CMD_GET_VARS,&Args);
   } while(FALSE);

   LOG_RESULT();
   return Ret;
}

// Read chunk of specified variable.
//
// int WASP_GetChunk(VarID Id,unsigned int Offset,unsigned int Len,
//                   unsigned char *pData);
// 
// Id: Variable Id of variable to get
//
// Offset: Offset from first byte of Variable to get.
// 
// Len: Number of bytes to get.
//
// pData: pointer to buffer to receive the data.
// 
// This function will return WASP_ERR_MORE_DATA if the last byte of the 
// variable wasn't read by this command.  In this case this not an error,
// it is only an indication that more data is available.
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetChunk(VarID Id,unsigned int Offset,unsigned int Len,
                  unsigned char *pData)
{
   int Ret = WASP_ERR_NOT_SUPPORTED;

   LOG_RESULT();
   return Ret;
}

// Read chunk of specified variable.
//
// int WASP_GetChunkByName(const char *Name,unsigned int Offset,
//                         unsigned int Len,unsigned char *pData);
// 
// Name: Name of the Variable to get
//
// Offset: Offset from first byte of Variable to get.
// 
// Len: Number of bytes to get.
//
// pData: pointer to buffer to receive the data.
// 
// This function will return WASP_ERR_MORE_DATA if the last byte of the 
// variable wasn't read by this command.  In this case this not an error,
// it is only an indication that more data is available.
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetChunkByName(const char *Name,unsigned int Offset,
                        unsigned int Len,unsigned char *pData)
{
   int Ret = WASP_ERR_NOT_SUPPORTED;

   LOG_RESULT();
   return Ret;
}

// Set specified variable.
// 
// int WASP_SetVariable(WaspVariable *pVar)
// 
// pVar: pointer to buffer to with the value to set.
// The ID, VarType and Val must be filled in appropriately
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_SetVariable(WaspVariable *pVar)
{
   int Ret = 0;   // assume the best
   if(pVar == NULL) {
      Ret = -EINVAL;
   }
   else {
      VarReferenceArgs Args;

      API_LOG("%s: called for ID 0x%x\n",__FUNCTION__,pVar->ID);
      if(API_LOGV) {
         WASP_DumpVar(pVar);
      }
      switch(pVar->Type) {
      case WASP_VARTYPE_BLOB:
      case WASP_VARTYPE_STRING:
      // Not supported yet
         Ret = WASP_ERR_NOT_SUPPORTED;
         break;

      default:
         Args.pVar = pVar;
         Args.VarRef.WaspVar = *pVar;
         Args.VarRef.Id = pVar->ID;
         Args.VarRef.Name[0] = 0;

         Ret = GetDataFromDaemon(CMD_SET_VAR,&Args);
      }
   }
   LOG_RESULT();
   return Ret;
}

// Set the specified variable.
//
// int WASP_SetVarByName(const char *Name,WaspVariable *pVar);
// 
// Name: Name of the Variable to set
// 
// pVar: pointer to buffer to with the value to set.
// The VarType and Val fields must be filled in appropriately
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_SetVarByName(const char *Name,WaspVariable *pVar)
{
   int Ret = 0;   // assume the best
   if(pVar == NULL || Name == NULL || strlen(Name) > MAX_VAR_NAME_LEN) {
      Ret = -EINVAL;
   }
   else {
      VarReferenceArgs Args;

      API_LOG("%s: called\n",__FUNCTION__);
      switch(pVar->Type) {
         case WASP_VARTYPE_BLOB:
         case WASP_VARTYPE_STRING:
         // Not supported yet
            Ret = WASP_ERR_NOT_SUPPORTED;
            break;

         default:
            Args.pVar = pVar;
            Args.VarRef.WaspVar = *pVar;
            Args.VarRef.Id = pVar->ID;
            strcpy(Args.VarRef.Name,Name);
            Ret = GetDataFromDaemon(CMD_SET_VAR,&Args);
      }
   }
   LOG_RESULT();
   return Ret;
}

// Set specified variable.
// 
// int WASP_SetVariables(int Count, WaspVariable **pVarArray);
//
// pVarArray: pointer to an array of WaspVariables with the values to set.
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_SetVariables(int Count, WaspVariable *pVarArray)
{
   int Ret = 0;   // assume the best
   if(pVarArray == NULL || Count <= 0) {
      Ret = -EINVAL;
   }
   else {
      SetVarsArgs Args;
      Args.pVarArray = pVarArray;
      Args.Count = Count;

      API_LOG("%s: called\n",__FUNCTION__);
      Ret = GetDataFromDaemon(CMD_SET_VARS,&Args);
   }
   LOG_RESULT();
   return Ret;
}


// Set a chunk of specified variable.
//
// int WASP_SetChunk(VarID Id,unsigned int Offset,unsigned int Len,
//                   int bLast,unsigned char *pData);
// 
// Id: Variable Id of variable to set
//
// Offset: Offset from first byte of Variable to set.
// 
// Len: Number of bytes to set.
// 
// bLast: FALSE - More data to come
//        TRUE - this write complete a series of writes to this variable.
// It is up to the device to decide if it's appropriate to delay the 
// setting of variable until bLast is set or not.  
// 
// pData: pointer to buffer to with the data.
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_SetChunk(VarID Id,unsigned int Offset,unsigned int Len,
                  int bLast,unsigned char *pData)
{
   int Ret = WASP_ERR_NOT_SUPPORTED;

   LOG_RESULT();
   return Ret;
}

// Set a chunk of specified variable by name.
//
// int WASP_SetChunkByName(const char *Name,unsigned int Offset,
//                         unsigned int Len,int bLast,unsigned char *pData);
// 
// Name: Name of the Variable to set
//
// Offset: Offset from first byte of Variable to set.
// 
// Len: Number of bytes to set.
// 
// bLast: FALSE - More data to come
//        TRUE - this write complete a series of writes to this variable.
// It is up to the device to decide if it's appropriate to delay the 
// setting of variable until bLast is set or not.  
// 
// pData: pointer to buffer to with the data.
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_SetChunkByName(const char *Name,unsigned int Offset,
                        unsigned int Len,int bLast,unsigned char *pData)
{
   int Ret = WASP_ERR_NOT_SUPPORTED;

   LOG_RESULT();
   return Ret;
}


// Get button presses.
// 
// pButton: pointer to byte to receive button ID.
// Value will be 0 if no buttons have been pressed or released
// since the last call.
// 
// Returns:
//      0 - No Buttons have been Pressed or released since last call
//     >0 - Button ID & Press/released bit
//    < 0 - WASP error code (see WASP_ERR_*)
//
// WASP_ERR_NO_DATA - No buttons have changed state since last call.
int WASP_GetButton(unsigned char *pButtonID)
{
   int Ret = EINVAL; // assume the worse

   API_LOG("%s: called\n",__FUNCTION__);
   if(pButtonID != NULL) {
      Ret = GetDataFromDaemon(CMD_GET_BUTTON,pButtonID);
   }
   LOG_RESULT();

   return Ret;
}


// Get Interface board selftest results
// int WASP_GetSelfTestResults(SelfTestResults **pResults);
//
// pResults: pointer to pointer to returned selftest results.
// Returned value is dynamically allocated via malloc(), 
// it the responsibility of the caller to free the memory.
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetSelfTestResults(SelfTestResults **pResults)
{
   int Ret = EINVAL; // assume the worse

   API_LOG("%s: called\n",__FUNCTION__);
   if(pResults != NULL) {
      Ret = GetDataFromDaemon(CMD_GET_SELFTEST,pResults);
   }
   LOG_RESULT();
   return Ret;
}

// 
// int WASP_SetLEDScript(LedID Id,LedState *pLedStates);
// 
// Id: The ID of the LED to be controlled.
// 
// pLedStates: pointer to an array of LED states defining the desired
// LED behavour.
// 
// The WASP daemon drives the devices LEDs by following the list
// of LED states with the specified durations until a state with 
// a duration of zero or LED_DURATION_INFINITE is found.  If the
// last state has a duration zero then the sequence starts over a
// the first state.  This allows complex blink pattern to be displayed
// easily.
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_SetLEDScript(LedID Id,LedState *pLedStates)
{
   SetLedScriptArgs *pArgs = NULL;
   LedState *p = pLedStates;
   int Ret = WASP_OK;
   int MaxCount;
   int Count = 1;
   int MallocLen;

   API_LOG("%s: called\n",__FUNCTION__);
   do {
      if(pLedStates == NULL) {
         Ret = -EINVAL;
         break;
      }
   // Calculate the maximum script size (it must fit in a WASP message)
      MaxCount = (WASP_MAX_MSG_LEN - 2) / sizeof(LedState);

   // Count the number of states
      while(p->Duration != 0 && p->Duration != LED_DURATION_INFINITE) {
         Count++;
         p++;
         if(Count > MaxCount) {
            Ret = WASP_ERR_OVFL;
            break;
         }
      }

      if(Ret != WASP_OK) {
         break;
      }

      MallocLen = sizeof(SetLedScriptArgs) + (Count * sizeof(LedState));
      if((pArgs = (SetLedScriptArgs *) malloc(MallocLen)) == NULL) {
         LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
         Ret = ENOMEM;
         break;
      }

      pArgs->Count = Count;
      pArgs->Id = Id;
      memcpy(pArgs->LedStates,pLedStates,Count*sizeof(LedState));
      Ret = GetDataFromDaemon(CMD_SET_LEDSCRIPT,pArgs);
   } while(FALSE);

   if(pArgs != NULL) {
      free(pArgs);
   }
   LOG_RESULT();
   return Ret;
}

// int WASP_GetVarLen(WaspVariable *pVar)
// 
// Returns:
//      Size of variable in bytes
//    < 0 - Error
int WASP_GetVarLen(WaspVariable *pVar)
{
   VarType Type;
   int Len = sizeof(WaspVariable) - sizeof(WaspValUnion);

   do {
      if(pVar == NULL) {
         Len = -EINVAL;
         break;
      }

      Type = pVar->Type;

      if(Type > NUM_VAR_TYPES) {
         Len = -EINVAL;
         break;
      }

      switch((Type = pVar->Type)) {
         case WASP_VARTYPE_STRING:
         // Assume that the string follows the variable and include
         // it in the size
            Len += strlen(pVar->Val.String) + 1;
            break;

         case WASP_VARTYPE_BLOB:
         // Assume that the blob data follows the variable and include
         // it in the size
            Len += sizeof(BlobVar) + pVar->Val.Blob.Len;
            break;

         default:
            Len += WASP_VarType2Len[Type];
            break;
      }
   } while(FALSE);

   return Len;
}

// Set the maximum number asynchronous data messages to queue in WASP Daemon.  
// Asynchronous data reporting is enabled when QueueLen > 0.  Asynchronous 
// data reporting is disabled by default.
// 
// int WASP_SetAsyncDataQueueDepth(int QueueLen);
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_SetAsyncDataQueueDepth(int QueueLen)
{
   API_LOG("%s: called\n",__FUNCTION__);
   int Ret = GetDataFromDaemon(CMD_SET_ASYNC_Q_LEN,&QueueLen);

   LOG_RESULT();
   return Ret;
}

// Free any malloc'ed storaged used by WaspVariable
// 
// int WASP_FreeValue(WaspVariable *pVar);
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_FreeValue(WaspVariable *pVar)
{
   int Ret = 0;
   
   if(pVar->Type == 0 || pVar->Type > NUM_VAR_TYPES) {
      Ret = WASP_ERR_INVALID_TYPE;
   }
   else {
      switch(pVar->Type) {
         case WASP_VARTYPE_STRING:
            pVar->State = VAR_VALUE_UNKNOWN;
            if(pVar->Val.String != NULL) {
               free(pVar->Val.String);
               pVar->Val.String = NULL;
            }
            break;

         case WASP_VARTYPE_BLOB:
            pVar->State = VAR_VALUE_UNKNOWN;
            if(pVar->Val.Blob.Data != NULL) {
               free(pVar->Val.Blob.Data);
               pVar->Val.Blob.Data = NULL;
            }
            break;
      }
   }

   return Ret;
}


// Compare two WASP variables
// 
// int WASP_Compare(WaspVariable *pVar1,WaspVariable *pVar2)
// 
// Returns:
//      0 - pVar1 == pVar2
//      1 - pVar1 > pVar2
//      2 - pVar1 < pVar2
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_Compare(WaspVariable *pVar1,WaspVariable *pVar2)
{
   int Ret = WASP_ERR_NOT_SUPPORTED;   // assume the worse
   int Diff;

   if(pVar1->Type != pVar2->Type) {
      Ret = WASP_ERR_INVALID_TYPE;
   }
   else {
      switch(pVar1->Type) {
         case WASP_VARTYPE_BOOL:
         case WASP_VARTYPE_ENUM:
         case WASP_VARTYPE_UINT8:
            Diff = pVar1->Val.U8 - pVar2->Val.U8;
            Ret = WASP_OK;
            break;


         case WASP_VARTYPE_INT16:
         case WASP_VARTYPE_TEMP:
            Diff = pVar1->Val.I16 - pVar2->Val.I16;
            Ret = WASP_OK;
            break;

         case WASP_VARTYPE_INT8:
            Diff = pVar1->Val.I8 - pVar2->Val.I8;
            Ret = WASP_OK;
            break;

         case WASP_VARTYPE_INT32:
         case WASP_VARTYPE_TIME32:
            Diff = pVar1->Val.I32 - pVar2->Val.I32;
            Ret = WASP_OK;
            break;

         case WASP_VARTYPE_PERCENT:
         case WASP_VARTYPE_TIME16:
         case WASP_VARTYPE_TIME_M16:
         case WASP_VARTYPE_UINT16:
            Diff = pVar1->Val.U16 - pVar2->Val.U16;
            Ret = WASP_OK;
            break;

         case WASP_VARTYPE_UINT32:
            Diff = pVar1->Val.U32 - pVar2->Val.U32;
            Ret = WASP_OK;
            break;

         case WASP_VARTYPE_BLOB:
         case WASP_VARTYPE_STRING:
            break;

         case WASP_VARTYPE_TIMEBCD:
            break;
         case WASP_VARTYPE_BCD_DATE:
            break;
         case WASP_VARTYPE_DATETIME:
            break;

         default:
            Ret = WASP_ERR_INVALID_TYPE;
      }
   }

   if(Ret == WASP_OK) {
      if(Diff > 0) {
         Ret = 1;
      }
      else if(Diff < 0) {
         Ret = 2;
      }
   }

   return Ret;
}

// Make a copy of a wasp variable.
//
// int WASP_CopyVar(WaspVariable *pTo,WaspVariable *pFrom);
// 
// pTo: pointer to variable to copy to 
// pFrom: pointer to variable to copy from
// 
// If the variable value is returned via a pointer in pVar->Val
// (string, blob) then the pointer is to dynamically allocated 
// memory that the caller is responsible for freeing.
// 
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_CopyVar(WaspVariable *pVar1,WaspVariable *pVar2)
{
   int Ret = WASP_ERR_INVALID_VARIABLE;   // assume the worse

   if(pVar1 != NULL && pVar2 != NULL) {
      VarType Type = pVar2->Type;
   // Copy everything other than the value
      memcpy(pVar1,pVar2,sizeof(WaspVariable) - sizeof(WaspValUnion));
      if(Type == WASP_VARTYPE_BLOB) {
         if((pVar1->Val.Blob.Data = malloc(pVar1->Val.Blob.Len)) == NULL) {
            Ret = ENOMEM;
         }
         else {
            pVar1->Val.Blob.Len = pVar2->Val.Blob.Len;
            memcpy(pVar1->Val.Blob.Data,pVar2->Val.Blob.Data,pVar2->Val.Blob.Len);
         }
      }
      else if(Type == WASP_VARTYPE_STRING) {
         if(pVar2->Val.String == NULL) {
            pVar1->Val.String = NULL;
         }
         else {
            pVar1->Val.String = strdup(pVar2->Val.String);
            if(pVar1->Val.String == NULL) {
               Ret = ENOMEM;
            }
         }
      }
      else if(Type > 0 && Type <= NUM_VAR_TYPES) {
         int CopyLen = WASP_VarType2Len[Type];
         if(CopyLen < 1) {
            Ret = WASP_ERR_WASPD_INTERNAL;
         }
         else {
            memcpy(&pVar1->Val,&pVar2->Val,CopyLen);
         }
      }
      else {
         Ret = WASP_ERR_INVALID_TYPE;
      }
   } while(FALSE);

   return Ret;
}


// Return variable that has changed since the time it was read.
//
// int WASP_GetChangedVar(WaspVariable *pVar);
// 
// pVar: pointer to buffer to receive the value.
// 
// Filled by caller:
// 
// pVar->State:
//     VAR_VALUE_LIVE - get value from interface board
//     VAR_VALUE_CACHED - returned cached value when possible
// 
// Returned:
// pVar->ID Variable's ID.
//
// pVar->State:
//     VAR_VALUE_LIVE   - value from interface board
// 
// If the variable value is returned via a pointer in pVar->Val
// (string, blob) then the pointer is to dynamically allocated 
// memory that the caller is responsible for freeing.
// 
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetChangedVar(WaspVariable *pVar)
{
   int Ret = 0;   // assume the best

   API_LOG("%s: called\n",__FUNCTION__);
   if(pVar == NULL) {
      Ret = -EINVAL;
   }
   else {
      VarReferenceArgs Args;

      Args.pVar = pVar;
      Ret = GetDataFromDaemon(CMD_GET_CHANGED_VAR,&Args);
   }

   LOG_RESULT();

   if(Ret == 0 && API_LOGV) {
      WASP_DumpVar(pVar);
   }
   return Ret;
}


void WASP_LogLogLevel(int LevelMask)
{
   int i;
   LOG("Logging mask 0x%x:\n",LevelMask);
   for(i = 0; DebugMasks[i].Mask != 0; i++) {
      if(LevelMask & DebugMasks[i].Mask) {
         LOG("  %08x - %s\n",DebugMasks[i].Mask,DebugMasks[i].Desc);
      }
   }
}

void WASP_ReadLogLevel()
{
   FILE *fp = fopen(LOG_LEVEL_PATH,"r");

   if(fp != NULL) {
      if(fscanf(fp,"%x",&gWASP_Verbose) == 1) {
         gWASP_LogLevelSet = TRUE;
      }
      fclose(fp);
   }
}

void WASP_DumpVar(WaspVariable *p)
{
   int Integer;
   int Fraction;
   VarType VarType = p->Type;

   do {
      LOG("    VarID: %d (0x%02x)\n",p->ID,p->ID);
      if(VarType == 0 || VarType > NUM_VAR_TYPES) {
         LOG("%s: Error invalid VarType 0x%x\n",__FUNCTION__,VarType);
         break;
      }
      LOG("  VarType: %s (0x%x)\n",WASP_VarTypes[VarType],VarType);

      if(p->State > NUM_VAL_STATES) {
         LOG("%s: Error invalid State 0x%x\n",__FUNCTION__,p->State);
         break;
      }
      LOG("    State: %s (0x%x)\n",WASP_ValStates[p->State],p->State);

      if(p->Pseudo.bPseudoVar) {
      // Display Pseudo variable information
         LOG("Timestamp: ");

         if(p->Pseudo.TimeStamp == 0) {
            LOG("not set\n");
         }
         else {
            struct tm *pTm;
            pTm = gmtime(&p->Pseudo.TimeStamp);
            LOG("%d/%02d/%04d %d:%02d:%02d (UTC)\n",pTm->tm_mon + 1,
                   pTm->tm_mday,pTm->tm_year + 1900,pTm->tm_hour,
                   pTm->tm_min,pTm->tm_sec);
         }
         LOG("    Count: %d\n",p->Pseudo.Count);
         LOG(" bInRange: %s\n",p->Pseudo.bInRange ? "yes" : "no");
         LOG("bRestored: %s\n",p->Pseudo.bRestored ? "yes" : "no");
      }
      LOG("      Val: ");
      switch(VarType) {
         default:

         case WASP_VARTYPE_BOOL:
         case WASP_VARTYPE_ENUM:
         case WASP_VARTYPE_UINT8:
            LOG("%u (0x%02x)\n",p->Val.Enum,p->Val.Enum);
            break;

         case WASP_VARTYPE_PERCENT:
            LOG("%d.%d%%\n",p->Val.Percent/10,p->Val.Percent%10);
            break;

         case WASP_VARTYPE_TEMP:
            Integer = p->Val.Temperature/10;
            Fraction = p->Val.Temperature%10;
            if(Fraction < 0) {
               Fraction = -Fraction;
            }
            LOG("%d.%d degrees F\n",Integer,Fraction);
            break;

         case WASP_VARTYPE_TIME32:
            Integer = p->Val.TimeTenths/10;
            Fraction = p->Val.TimeTenths%10;
            if(Fraction < 0) {
               Fraction = -Fraction;
            }
            LOG("%d.%d secs\n",Integer,Fraction);
            break;

         case WASP_VARTYPE_TIME16:
            LOG("%d secs\n",p->Val.TimeSecs);
            break;

         case WASP_VARTYPE_TIMEBCD:
            LOG("%02x:%02x:%02x\n",p->Val.BcdTime[2],p->Val.BcdTime[1],
                   p->Val.BcdTime[0]);
            break;

         case WASP_VARTYPE_BCD_DATE:
            LOG("%02x/%02x/%02x%02x\n",p->Val.BcdDate[1],p->Val.BcdDate[0],
                   p->Val.BcdDate[3],p->Val.BcdDate[2]);
            break;

         case WASP_VARTYPE_DATETIME:
            LOG("%02x/%02x/%02x%02x %02x:%02x:%02x\n",
                   p->Val.BcdDateTime[4],p->Val.BcdDateTime[3],
                   p->Val.BcdDateTime[6],p->Val.BcdDateTime[5],
                   p->Val.BcdDateTime[2],p->Val.BcdDateTime[1],
                   p->Val.BcdDateTime[0]);
            break;

         case WASP_VARTYPE_STRING:
            LOG("'%s'\n",p->Val.String);
            break;

         case WASP_VARTYPE_BLOB:
            LOG("Blob %d bytes:\n",p->Val.Blob.Len);
            DumpHex(p->Val.Blob.Data,p->Val.Blob.Len);
            break;

         case WASP_VARTYPE_UINT16:
            LOG("%u (0x%x)\n",p->Val.U16,p->Val.U16);
            break;

         case WASP_VARTYPE_UINT32:
            LOG("%u (0x%x)\n",p->Val.U32,p->Val.U32);
            break;

         case WASP_VARTYPE_INT8:
            LOG("%d (0x%x)\n",p->Val.I8,p->Val.I8);
            break;

         case WASP_VARTYPE_INT16:
            LOG("%d (0x%x)\n",p->Val.I16,p->Val.I16);
            break;

         case WASP_VARTYPE_INT32:
            LOG("%d (0x%x)\n",p->Val.I32,p->Val.I32);
            break;

         case WASP_VARTYPE_TIME_M16:
            LOG("%d minutes\n",p->Val.TimeMins);
            break;
      }
   } while(FALSE);
}

static void LogResult(const char *Func,int Ret)
{
   if(Ret == 0) {
      API_LOG("%s: returning %d\n",Func,Ret);
   }
   else {
      API_LOG("%s: returning %s (%d)\n",Func,WASP_strerror(Ret),Ret);
   }
}

#ifdef OLD_API
// Set number of minutes to cook time at currently selected temperature.
// The current mode must be MODE_LOW or MODE_HIGH to set a cook time. 
//  
// Note: requested value will be rounded up to nearest 30 minute increment
// (CrockPot hardware limitations) 
//    < 0 - WASP error code (see ERR_WASP_*)
int WASP_SetCookTime(int Minutes)
{
   int Ret = GetDataFromDaemon(CMD_SET_COOKTIME,&Minutes);

   LOG("%s: returning %d\n",__FUNCTION__,Ret);
   return Ret;
}

// Return number of minutes of cook time remaining from product LED display. 
// A value of 0 indicates that the cockpot is not in a cooking mode
//    < 0 - WASP error code (see ERR_WASP_*)
int WASP_GetCookTime()
{
   int Minutes;
   int Ret = GetDataFromDaemon(CMD_GET_COOKTIME,&Minutes);

   if(Ret == WASP_OK) {
      Ret = Minutes;
   }

   LOG("%s: returning %d\n",__FUNCTION__,Ret);
   return Ret;
}

#endif
