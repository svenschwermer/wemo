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

#include "wasp.h"
#include "wasp_api.h"
#define WASPD_PRIVATE
#include "waspd.h"

static CROCKPOT_MODE gMode = MODE_UNKNOWN;


// Initialize Hardware Abstraction Layer for use
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_Init()
{
   int Ret = WASP_Init();
   LOG("%s: returning %d\n",__FUNCTION__,Ret);
   return Ret;
}

// Return the current mode decoded from the product LEDS.
//    < 0 - HAL error code (see ERR_HAL_*)
CROCKPOT_MODE HAL_GetMode(void)
{
   CROCKPOT_MODE Mode = MODE_UNKNOWN;
   WaspVariable WaspMode;
   int Err;

   memset(&WaspMode,0,sizeof(WaspVariable));
   WaspMode.ID = CROCKPOT_VAR_MODE;
   WaspMode.State = VAR_VALUE_LIVE;

   if((Err = WASP_GetVariable(&WaspMode)) != WASP_OK) {
      if(Err >= 0) {
         Err = -Err;
      }
      Mode = (CROCKPOT_MODE) Err;
   }
   else {
      if(WaspMode.ID != CROCKPOT_VAR_MODE) {
         LOG("%s: Error, requested variable ID 0x%x, got 0x%x\n",
             __FUNCTION__,CROCKPOT_VAR_MODE,WaspMode.ID);
      }
      Mode = _WaspMode2OldMode(WaspMode.Val.Enum);
      gMode = Mode;
   }

   LOG("%s: returning %d\n",__FUNCTION__,Mode);
   return Mode;
}

// Set cooking mode
// Legal values are MODE_OFF, MODE_WARM, MODE_LOW or MODE_HIGH.
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_SetMode(CROCKPOT_MODE Mode)
{
   int Ret = 0;   // assume the best
   int Err;
   WaspVariable WaspMode;

   memset(&WaspMode,0,sizeof(WaspVariable));
   WaspMode.ID = CROCKPOT_VAR_MODE;
   WaspMode.Type = WASP_ENUM;
   WaspMode.Val.Enum = _OldMode2WaspMode(Mode);

   if((Err = WASP_SetVariable(&WaspMode)) != WASP_OK) {
      if(Err >= 0) {
         Err = -Err;
      }
      Ret = Err;
   }
   else {
      gMode = Mode;
   }

   LOG("%s: Mode: %d, returning %d\n",__FUNCTION__,Mode,Ret);

   return Ret;
}

// Return number of minutes of cook time remaining from product LED display. 
// A value of 0 indicates that the cockpot is not in a cooking mode
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_GetCookTime()
{
   int Ret = 0;
   int Err;
   WaspVariable WaspCookTime;

   if(gMode == MODE_LOW || gMode == MODE_HIGH) {
      memset(&WaspCookTime,0,sizeof(WaspVariable));
      WaspCookTime.ID = CROCKPOT_VAR_COOKTIME;
      WaspCookTime.Type = WASP_INT32;

      if((Err = WASP_GetVariable(&WaspCookTime)) != WASP_OK) {
         if(Err >= 0) {
            Err = -Err;
         }
         Ret = Err;
      }
      else {
         if(WaspCookTime.ID != CROCKPOT_VAR_COOKTIME) {
            LOG("%s: Error, requested variable ID 0x%x, got 0x%x\n",
                __FUNCTION__,CROCKPOT_VAR_COOKTIME,WaspCookTime.ID);
         }
         Ret = WaspCookTime.Val.I32;
      }
   }


   LOG("%s: returning %d\n",__FUNCTION__,Ret);
   if(Ret > 0) {
      DumpHex(&WaspCookTime,sizeof(WaspCookTime));
   }
   return Ret;
}

// Set number of minutes to cook time at currently selected temperature.
// The current mode must be MODE_LOW or MODE_HIGH to set a cook time. 
//  
// Note: requested value will be rounded up to nearest 30 minute increment
// (CrockPot hardware limitations) 
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_SetCookTime(int Minutes)
{
   int Ret = 0;   // assume the best
   int CookTime = ((Minutes + 29) / 30) * 30;
   WaspVariable WaspCookTime;

   LOG("%s: setting cooking time to %d minutes, mode: %d.\n",__FUNCTION__,
       Minutes,gMode);
   do {
      if(Minutes < 0 || Minutes > MAX_COOK_TIME) {
      // Argment out of range
         Ret = -EINVAL;
         break;
      }

      if(gMode != MODE_LOW && gMode != MODE_HIGH) {
         Ret = ERR_HAL_WRONG_MODE;
         break;
      }

      memset(&WaspCookTime,0,sizeof(WaspVariable));
      WaspCookTime.ID = CROCKPOT_VAR_COOKTIME;
      WaspCookTime.Type = WASP_INT32;
      WaspCookTime.Val.I32 = CookTime;

      if((Ret = WASP_SetVariable(&WaspCookTime)) != WASP_OK) {
         if(Ret >= 0) {
            Ret = -Ret;
         }
      }
   } while(FALSE);

   LOG("%s: Returning %d.\n",__FUNCTION__,Ret);
   return Ret;
}

int HAL_GetFWVersionNumber(char **pBuf)
{
   return WASP_GetFWVersionNumber(pBuf);
}


// Shutdown the HAL deamon. 
// 
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_Shutdown()
{
   LOG("%s: called\n",__FUNCTION__);
   return WASP_Shutdown();
}

// Free and release resources used by HAL
// 
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_Cleanup()
{
   LOG("%s: called\n",__FUNCTION__);
   return WASP_Cleanup();
}

const char *HAL_strerror(int Err)
{
   const char *Ret = NULL;

   LOG("%s: called, Err: %d\n",__FUNCTION__,Err);
   switch(Err) {
      case ERR_HAL_INVALID_RESPONSE_TYPE:
         Ret = "ERR_HAL_INVALID_RESPONSE_TYPE";
         break;

      case ERR_HAL_DAEMON_TIMEOUT:
         Ret = "ERR_HAL_DAEMON_TIMEOUT";
         break;

      case ERR_HAL_WRONG_MODE:
         Ret = "ERR_HAL_WRONG_MODE";
         break;

      default:
         Ret = WASP_strerror(Err);
   }

   return Ret;
}





