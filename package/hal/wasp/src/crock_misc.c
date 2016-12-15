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
#if defined(CROCKPOT_HW) || defined(OLD_API)

#include <errno.h>
#include <crockpot.h>
#include "wasp_crockpot.h"

CROCKPOT_MODE _WaspMode2OldMode(int WaspMode)
{
   CROCKPOT_MODE Mode = MODE_UNKNOWN;

   switch(WaspMode) {
     case CROCKPOT_MODE_OFF:
        Mode = MODE_OFF;
        break;

     case CROCKPOT_MODE_WARM:
        Mode = MODE_WARM;
        break;

     case CROCKPOT_MODE_LOW:
        Mode = MODE_LOW;
        break;

     case CROCKPOT_MODE_HIGH:
        Mode = MODE_HIGH;
        break;

     default:
//        LOG("%s: called with invalid mode %d.\n",__FUNCTION__,WaspMode);
        break;
   }

   return Mode;
}

int _OldMode2WaspMode(CROCKPOT_MODE Mode)
{
   int Ret = CROCKPOT_MODE_OFF;

   switch(Mode) {
      case MODE_OFF:
         Ret = CROCKPOT_MODE_OFF;
         break;

      case MODE_WARM:
         Ret = CROCKPOT_MODE_WARM;
         break;

      case MODE_LOW:
         Ret = CROCKPOT_MODE_LOW;
         break;

      case MODE_HIGH:
         Ret = CROCKPOT_MODE_HIGH;
         break;

      default:
         Ret = -EINVAL;
         break;
   }

   return Ret;
}

#endif

