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
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <string.h>
#include <syslog.h>

#include "wasp.h"
#include "wasp_api.h"
#define  WASPD_PRIVATE
#include "waspd.h"
#include "wasp_async.h"

#define MRCOFFEE_VAR_MODE              0x80
#define MRCOFFEE_VAR_MODETIME          0x81
#define MRCOFFEE_VAR_TIME_REMAINING    0x82
#define MRCOFFEE_VAR_WATER_LEVEL       0x83
#define MRCOFFEE_VAR_CLEAN_ADVISE      0x84
#define MRCOFFEE_VAR_FILTER_ADVISE     0x85
#define MRCOFFEE_VAR_BREWING_TIME      0x86
#define MRCOFFEE_VAR_BREWED_TIME       0x87
#define MRCOFFEE_VAR_CLEANING_TIME     0x88
#define MRCOFFEE_VAR_LAST_CLEANED_TIME 0x89


#define MRCOFFEE_MODE_REFILL           0
#define MRCOFFEE_MODE_PLACE_CARAFE     1
#define MRCOFFEE_MODE_REFILL_WATER     2
#define MRCOFFEE_MODE_READY            3
#define MRCOFFEE_MODE_BREWING          4
#define MRCOFFEE_MODE_BREWED           5
#define MRCOFFEE_MODE_CLEANING_BREWING 6
#define MRCOFFEE_MODE_CLEANING_SOAKING 7
#define MRCOFFEE_MODE_CARAFE_REMOVED   8

#define SUPPORT_BUTTONS                0
#define SUPPORTED_WASP_VERSION   "1.06"

u8 gWemoState = WASP_STATE_OFF;
u8 gMode = MRCOFFEE_MODE_REFILL;
u8 g_bWaterLevelReached;
u8 g_bCleanAdvise;
u8 g_bFilterAdvise;
u16 gCleaningTimeRemaing;
time_t gLastModeChange;


VariableAttribute MyVariables[] = {
   // MRCOFFEE_VAR_MODE, 0x80
   {  WASP_VARTYPE_ENUM,
      WASP_USAGE_CONTROLLED,
      .PollRate = 1000, // 1 / second
      .MinMax.U8 = { MRCOFFEE_MODE_REFILL, MRCOFFEE_MODE_CARAFE_REMOVED,"Mode"}
   },
   // MRCOFFEE_VAR_MODETIME, 0x81
   {  WASP_VARTYPE_TIME_M16,
      WASP_USAGE_MONITORED,
      .PollRate = 1000, // 1 / second
      .MinMax.U16 = { 0, 0xffff,"Mode Time"}
   },
   // MRCOFFEE_VAR_TIME_REMAINING, 0x82
   {  WASP_VARTYPE_TIME_M16,
      WASP_USAGE_MONITORED,
      .PollRate = 1000, // 1 / second
      .MinMax.U16 = { 0, 0xffff,"Time Remaining"}
   },
   // MRCOFFEE_VAR_WATER_LEVEL, 0x83
   {  WASP_VARTYPE_BOOL,
      WASP_USAGE_MONITORED,
      .PollRate = 1000, // 1 / second
      .MinMax.U8 = { 0, 1,"Water level reached"}
   },
   // MRCOFFEE_VAR_CLEAN_ADVISE, 0x84
   {  WASP_VARTYPE_BOOL,
      WASP_USAGE_MONITORED,
      .PollRate = 1000, // 1 / second
      .MinMax.U8 = { 0, 1,"Clean advise"}
   },
   // MRCOFFEE_VAR_FILTER_ADVISE, 0x85
   {  WASP_VARTYPE_BOOL,
      WASP_USAGE_MONITORED,
      .PollRate = 1000, // 1 / second
      .MinMax.U8 = { 0, 1,"Filter advise"}
   },
   // MRCOFFEE_VAR_BREWING_TIME, 0x86
   {  WASP_VARTYPE_ENUM,
      WASP_USAGE_RANGE_ENTER,
      .u.p.TrackedVarId = MRCOFFEE_VAR_MODE,
      .u.p.TrackingFlags = TRACK_FLAG_PUSH | TRACK_FLAG_SAVE,
      .MinMax.U8 = { MRCOFFEE_MODE_BREWING, MRCOFFEE_MODE_BREWING,"Brewing"}
   },
   // MRCOFFEE_VAR_BREWED_TIME, 0x87
   {  WASP_VARTYPE_ENUM,
      WASP_USAGE_RANGE_ENTER,
      .u.p.TrackedVarId = MRCOFFEE_VAR_MODE,
      .u.p.TrackingFlags = TRACK_FLAG_PUSH | TRACK_FLAG_SAVE,
      .MinMax.U8 = { MRCOFFEE_MODE_BREWED, MRCOFFEE_MODE_BREWED,"Brewed"}
   },
   // MRCOFFEE_VAR_CLEANING_TIME, 0x88
   {  WASP_VARTYPE_ENUM,
      WASP_USAGE_RANGE_ENTER,
      .u.p.TrackedVarId = MRCOFFEE_VAR_MODE,
      .u.p.TrackingFlags = TRACK_FLAG_PUSH | TRACK_FLAG_SAVE,
      .MinMax.U8 = { MRCOFFEE_MODE_CLEANING_BREWING, MRCOFFEE_MODE_CLEANING_SOAKING,"Cleaning"}
   },
   // MRCOFFEE_VAR_LAST_CLEANED_TIME, 0x89
   {  WASP_VARTYPE_ENUM,
      WASP_USAGE_RANGE_EXIT,
      .u.p.TrackedVarId = MRCOFFEE_VAR_MODE,
      .u.p.TrackingFlags = TRACK_FLAG_PUSH | TRACK_FLAG_SAVE,
      .MinMax.U8 = { MRCOFFEE_MODE_CLEANING_BREWING, MRCOFFEE_MODE_CLEANING_SOAKING,"Last Cleaned"}
   },
   {0}   // End of table
};

const char *ModeEnumNames[] = {
   "Refill",
   "Place Carafe",
   "Refill water",
   "Ready",
   "Brewing",
   "Brewed",
   "Cleaning brewing",
   "Cleaning soaking",
   "Brew failed, carafe removed"
};


int CmdGetValue(HandlerArgs *p);
int CmdGetValues(HandlerArgs *p);
int CmdGetVarAttribs(HandlerArgs *p);
int CmdGetEnumAttribs(HandlerArgs *p);
int CmdSetValue(HandlerArgs *p);
int CmdSetLed(HandlerArgs *p);
int CmdGetPresses(HandlerArgs *p);
int CmdGetSelftest(HandlerArgs *p);
int CmdGetButtons(HandlerArgs *p);

CmdTableEntry CmdTable[] = {
   {WASP_CMD_NOP,CmdNopHandler},
   {WASP_CMD_GET_VALUE,CmdGetValue},
   {WASP_CMD_GET_VALUES,CmdGetValues},
   {WASP_CMD_GET_VAR_ATTRIBS,CmdGetVarAttribs},
   {WASP_CMD_GET_ENUM_ATTRIBS,CmdGetEnumAttribs},
   {WASP_CMD_SET_VALUE,CmdSetValue},
   {WASP_CMD_SET_VALUES,CmdSetValue},
   {WASP_CMD_SET_LED,CmdSetLed},
   {WASP_CMD_SELFTEST_STATUS,CmdGetSelftest},
#if SUPPORT_BUTTONS
   {WASP_CMD_GET_PRESSES,CmdGetPresses},
   {WASP_CMD_GET_BUTTONS,CmdGetButtons},
#endif
   {0,NULL} // end of table marker
};

int CmdGetValue(HandlerArgs *p)
{
   int Ret = WASP_ERR_ARG; // assume the worse
   int RetBytes = 0;
   int MsgLen = p->MsgLen;
   u8 *Msg = p->Msg;
   union {
      u8 RetU8;
      u16 RetU16;
      u32 RetU32;
      short RetShort;
      int RetInt;
   } u;
   const char *RetString = NULL;
   u8 Resp[10];

   do {
      if(MsgLen < 2) {
         break;
      }

      if(gWASP_Verbose & LOG_V) {
         LOG("%s: Read request for variable 0x%02x.\n",__FUNCTION__,Msg[1]);
      }

      switch(Msg[1]) {
   // Standard Variables
         case WASP_VAR_DEVID:
            u.RetInt = 0x010001;
            RetBytes = sizeof(u.RetInt);
            break;

         case WASP_VAR_FW_VER:
            RetString = "V 0.01";
            break;

         case WASP_VAR_DEV_DESC:
            RetString = "Jarden Crock-Pot";
            break;

         case WASP_VAR_WEMO_STATUS:
         // This is a write only variable
            break;

         case WASP_VAR_WEMO_STATE:
            u.RetU8 = gWemoState;
            RetBytes = sizeof(u.RetU8);
            break;

         case WASP_VAR_WASP_VER:
            RetString = SUPPORTED_WASP_VERSION;
         // init gLastModeChange
            time(&gLastModeChange);
            break;

         // Device variables
         case MRCOFFEE_VAR_MODE:
            u.RetU8 = gMode;
            RetBytes = sizeof(u.RetU8);
            break;

         case MRCOFFEE_VAR_MODETIME:
            u.RetU16 = (u16) ((time(NULL) - gLastModeChange)/60);
            RetBytes = sizeof(u.RetU16);
            break;

         case MRCOFFEE_VAR_TIME_REMAINING:
            u.RetU16 = gCleaningTimeRemaing;
            RetBytes = sizeof(u.RetU16);
            break;

         case MRCOFFEE_VAR_WATER_LEVEL:
            u.RetU8 = g_bWaterLevelReached;
            RetBytes = sizeof(u.RetU8);
            break;

         case MRCOFFEE_VAR_CLEAN_ADVISE:
            u.RetU8 = g_bCleanAdvise;
            RetBytes = sizeof(u.RetU8);
            break;

         case MRCOFFEE_VAR_FILTER_ADVISE:
            u.RetU8 = g_bFilterAdvise;
            RetBytes = sizeof(u.RetU8);
            break;
      }

      if(RetString != NULL) {
         int Strlen = strlen(RetString);
         u8 *pResp = malloc(Strlen + 2);
         if(pResp != NULL) {
            Ret = -1;   // don't send default response
            pResp[0] = *Msg | WASP_CMD_RESP;
            pResp[1] = WASP_OK;
            memcpy(&pResp[2],RetString,Strlen);
            WaspAsyncSendResp(p->p,pResp,Strlen+2);
            free(pResp);
         }
         else {
            Ret = WASP_ERR_INTERNAL;
         }
      }
      else if(RetBytes > 0) {
         Ret = WASP_OK;
         Resp[0] = *Msg | WASP_CMD_RESP;
         Resp[1] = WASP_OK;
         memcpy(&Resp[2],&u,RetBytes);
         WaspAsyncSendResp(p->p,Resp,RetBytes+2);
         Ret = -1;   // don't send default response
      }
   } while(FALSE);

   return Ret;
}

int CmdGetValues(HandlerArgs *p)
{
   int Ret = -1;  // assume the best
   int RetBytes = 0;
   int MsgLen = p->MsgLen;
   u8 *Msg = p->Msg;
   u8 *ID = &p->Msg[1];
   u8 *pEnd = &p->Msg[p->MsgLen];
   int RetMsgLen = 2;
   union {
      u8 RetU8;
      u16 RetU16;
      u32 RetU32;
      short RetShort;
      int RetInt;
   } u;
   const char *RetString;
   u8 Resp[WASP_MAX_MSG_LEN];

   Resp[0] = *Msg | WASP_CMD_RESP;
   Resp[1] = WASP_OK;   // assume the best

   do {
      if(MsgLen < 2) {
         break;
      }

      while(ID < pEnd) {
         RetString = NULL;
         if(gWASP_Verbose & LOG_V) {
            LOG("%s: Read request for variable 0x%02x.\n",__FUNCTION__,*ID);
         }

         switch(*ID) {
      // Standard Variables
            case WASP_VAR_DEVID:
               u.RetInt = 0x10003;
               RetBytes = sizeof(u.RetInt);
               break;

            case WASP_VAR_FW_VER:
               RetString = "V 0.01";
               break;

            case WASP_VAR_DEV_DESC:
               RetString = "Mr Coffee";
               break;

            case WASP_VAR_WEMO_STATUS:
            // This is a write only variable
               break;

            case WASP_VAR_WEMO_STATE:
               u.RetU8 = gWemoState;
               RetBytes = sizeof(u.RetU8);
               break;

            case WASP_VAR_WASP_VER:
               RetString = SUPPORTED_WASP_VERSION;
               break;

         // Device variables
            case MRCOFFEE_VAR_MODE:
               u.RetU8 = gMode;
               RetBytes = sizeof(u.RetU8);
               break;

            case MRCOFFEE_VAR_MODETIME:
               u.RetU16 = (u16) ((time(NULL) - gLastModeChange)/60);
               RetBytes = sizeof(u.RetU16);
               break;

            case MRCOFFEE_VAR_TIME_REMAINING:
               u.RetU16 = gCleaningTimeRemaing;
               RetBytes = sizeof(u.RetU16);
               break;

            case MRCOFFEE_VAR_WATER_LEVEL:
               u.RetU8 = g_bWaterLevelReached;
               RetBytes = sizeof(u.RetU8);
               break;

            case MRCOFFEE_VAR_CLEAN_ADVISE:
               u.RetU8 = g_bCleanAdvise;
               RetBytes = sizeof(u.RetU8);
               break;

            case MRCOFFEE_VAR_FILTER_ADVISE:
               u.RetU8 = g_bFilterAdvise;
               RetBytes = sizeof(u.RetU8);
               break;

            default:
               LOG("%s: Error: unable to handle variable 0x%02x.\n",__FUNCTION__,*ID);
               Ret = WASP_ERR_ARG;
               break;
         }
         ID++;

         if(Ret > 0) {
            break;
         }

         if(RetString != NULL) {
            RetBytes = strlen(RetString) + 1;
         }

         if(RetBytes > 0) {
            if((RetMsgLen + RetBytes) > WASP_MAX_DATA_LEN) {
               Ret = WASP_ERR_OVFL;
               break;
            }
            if(RetString == NULL) {
               memcpy(&Resp[RetMsgLen],&u,RetBytes);
            }
            else {
               memcpy(&Resp[RetMsgLen],RetString,RetBytes);
            }
            RetMsgLen += RetBytes;
         }
      }
   } while(FALSE);

   if(Ret < 0) {
      WaspAsyncSendResp(p->p,Resp,RetMsgLen);
   }

   return Ret;
}


int CmdSetValue(HandlerArgs *p)
{
   int Ret = WASP_OK; // assume the best
   int i = 1;
   int MsgLen = p->MsgLen;
   u8 *Msg = p->Msg;

   do {
      if(MsgLen < 2) {
         break;
      }

      while(i < MsgLen) {
         if(gWASP_Verbose & LOG_V) {
            LOG("%s: Write request for variable 0x%02x.\n",__FUNCTION__,
                Msg[i]);
         }

         switch(Msg[i]) {
         // Standard Variables
            case WASP_VAR_DEVID:
            case WASP_VAR_FW_VER:
            case WASP_VAR_DEV_DESC:
            case WASP_VAR_WASP_VER:
            // Read only variables
               Ret = WASP_ERR_PERM;
               break;

            case WASP_VAR_WEMO_STATUS:
               i += 2;
               break;

            case WASP_VAR_WEMO_STATE:
               gWemoState = Msg[i+1];
               i += 2;
               break;

            case WASP_VAR_CLOCK_TIME:
               LOG("%s: WASP_VAR_CLOCK_TIME set to %x/%02x/%02x%02x %x:%02x:%02x\n",
                   __FUNCTION__,Msg[6],Msg[5],Msg[8],Msg[7],
                   Msg[4],Msg[3],Msg[2]);
               DumpHex(Msg,MsgLen);
               i += 8;
               break;

      //    Crock-pot device variables
            case MRCOFFEE_VAR_MODE:
               gMode = Msg[i+1];
               i += 2;
               break;

            default:
               LOG("%s: Variable 0x%x undefined or not supported\n",
                  __FUNCTION__,Msg[i]);
               Ret = WASP_ERR_ARG;
               i = MsgLen;
               break;
         }
      }
   } while(FALSE);

   return Ret;
}

int CmdGetVarAttribs(HandlerArgs *p)
{
   int Ret = WASP_ERR_ARG; // assume the worse
   int RetBytes = 0;
   int Entry;
   int MsgLen = p->MsgLen;
   u8 *Msg = p->Msg;

   do {
      if(MsgLen < 2) {
         break;
      }
      Entry = Msg[1] - 0x80;

      if(gWASP_Verbose & LOG_V) {
         LOG("%s: Variable attributes requested for variable 0x%02x.\n",
             __FUNCTION__,Msg[1]);
      }

      switch(Msg[1]) {
   //    Crock-pot device variables
         case MRCOFFEE_VAR_MODE:
         case MRCOFFEE_VAR_WATER_LEVEL:
         case MRCOFFEE_VAR_CLEAN_ADVISE:
         case MRCOFFEE_VAR_FILTER_ADVISE:
         case MRCOFFEE_VAR_BREWING_TIME:
         case MRCOFFEE_VAR_BREWED_TIME:
         case MRCOFFEE_VAR_CLEANING_TIME:
         case MRCOFFEE_VAR_LAST_CLEANED_TIME:
            RetBytes = offsetof(VariableAttribute,MinMax.U8.Name);
            RetBytes += strlen(MyVariables[Entry].MinMax.U8.Name);
            break;

         case MRCOFFEE_VAR_MODETIME:
         case MRCOFFEE_VAR_TIME_REMAINING:
            RetBytes = offsetof(VariableAttribute,MinMax.U16.Name);
            RetBytes += strlen(MyVariables[Entry].MinMax.U16.Name);
            break;
      }

      if(RetBytes > 0) {
         u8 *pResp = malloc(RetBytes + 2);
         if(pResp != NULL) {
            Ret = WASP_OK;
            pResp[0] = *Msg | WASP_CMD_RESP;
            pResp[1] = WASP_OK;
            memcpy(&pResp[2],&MyVariables[Entry],RetBytes);
            WaspAsyncSendResp(p->p,pResp,RetBytes+2);
            free(pResp);
            Ret = -1;   // don't send default response
         }
         else {
            Ret = WASP_ERR_INTERNAL;
         }
      }
   } while(FALSE);

   return Ret;
}

int CmdGetEnumAttribs(HandlerArgs *p)
{
   int Ret = WASP_ERR_ARG; // assume the worse
   const char *RetString = NULL;
   int MsgLen = p->MsgLen;
   u8 *Msg = p->Msg;

   do {
      if(MsgLen < 3) {
         break;
      }

      if(gWASP_Verbose & LOG_V) {
         LOG("%s: Enum variable name requested for variable 0x%02x, State 0x%x.\n",
             __FUNCTION__,Msg[1],Msg[2]);
      }

      switch(Msg[1]) {
   // Crock-pot device variables
         case MRCOFFEE_VAR_MODE:
            if(Msg[2] <= MRCOFFEE_MODE_CARAFE_REMOVED) {
               RetString = ModeEnumNames[Msg[2]];
            }
            break;
      }

      if(RetString != NULL) {
         int Strlen = strlen(RetString);
         u8 *pResp = malloc(Strlen + 2);
         if(pResp != NULL) {
            Ret = WASP_OK;
            pResp[0] = *Msg | WASP_CMD_RESP;
            pResp[1] = WASP_OK;
            memcpy(&pResp[2],RetString,Strlen);
            WaspAsyncSendResp(p->p,pResp,Strlen+2);
            free(pResp);
            Ret = -1;   // don't send default response
         }
      }
   } while(FALSE);

   return Ret;
}


int CmdSetLed(HandlerArgs *p)
{
   LedState *pState = (LedState *) &p->Msg[2];
   int i = 1;
   u8 *End = &p->Msg[p->MsgLen];
   u8 *Current = NULL;
   unsigned short LastDuration;


   if(gWASP_Verbose & LOG_V) {
      LOG("%s: Received Set LED script command LED 0x%02x:\n",__FUNCTION__,
          p->Msg[1]);
      if(gWASP_Verbose & LOG_V1) {
         DumpHex(p->Msg,p->MsgLen);
      }
   }
   
   for( ; ; ) {
      LOG("\nStep %d:\n",i++);
      if((LastDuration = pState->Duration) == LED_DURATION_INFINITE) {
         LOG("  Duration: Infinite\n");
      }
      else if(pState->Duration == 0) {
         LOG("  Goto step 1\n");
         break;
      }
      else {
         LOG("  Duration: %u milliseconds\n",pState->Duration);
      }

      LOG("  RedIntensity: 0x%x\n",pState->RedIntensity);
      LOG("  GreenIntensity: 0x%x\n",pState->GreenIntensity);
      LOG("  BlueIntensity: 0x%x\n",pState->BlueIntensity);


      pState++;
      Current = (u8 *) pState;

      if(LastDuration == LED_DURATION_INFINITE) {
         break;
      }

      if(Current >= End) {
         LOG("%s: Error, last step not found before end of message\n",
             __FUNCTION__);
         break;
      }
   }

   if(Current != End) {
      LOG("%s: Garbage at end of message\n",__FUNCTION__);
   }

   return WASP_OK;
}

static ButtonRing gRing = {
   0,4,{0x71,0x81,0x1,0xe0}
};

int CmdGetPresses(HandlerArgs *p)
{
   u8 Resp[3];

   Resp[2] = 0;
   if(gRing.Rd != gRing.Wr) {
      Resp[2] = gRing.Buf[gRing.Rd++];
      if(gRing.Rd >= BUTTON_BUF_LEN) {
         gRing.Rd = 0;
      }
   }

   if(gWASP_Verbose & LOG_V1) {
      LOG("%s: Button read request received\n",__FUNCTION__);
   }

   Resp[0] = *p->Msg | WASP_CMD_RESP;
   Resp[1] = WASP_OK;
   WaspAsyncSendResp(p->p,Resp,Resp[2] == 0 ? 2 : 3);

   return -1;   // don't send default response
}

int CmdGetSelftest(HandlerArgs *p)
{
   u8 Resp[] = {
      WASP_CMD_SELFTEST_STATUS | WASP_CMD_RESP,
      WASP_OK,
      WASP_TEST_TEST_MINOR_ISSUE,
      'T','h','i','s',' ','i','s',' ','M','i','n','o','r','!',0
   };
   WaspAsyncSendResp(p->p,Resp,sizeof(Resp));

   return -1;   // don't send default response
}

int CmdGetButtons(HandlerArgs *p)
{
   u8 Resp[] = {
      WASP_CMD_GET_BUTTONS | WASP_CMD_RESP,
      WASP_OK,
// Simulate a device with 4 buttons where the last button is pressed on startup
      0x00,0x01,0x02,0x83,
   };
   WaspAsyncSendResp(p->p,Resp,sizeof(Resp));

   return -1;   // don't send default response
}

