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
#include <sys/socket.h>
#include <netinet/in.h>

#include "wasp.h"
#include "wasp_api.h"
#define WASPD_PRIVATE
#include "waspd.h"

#define __interrupt
typedef unsigned char   __BYTE;
__BYTE T00CR0;
__BYTE T00CR1;
__BYTE PDRG_PG1;
__BYTE SSR_TIE = 0;
__BYTE SSR_RDRF = 1;
__BYTE RDR_TDR;
__BYTE SCR_CRE;
__BYTE SSR_TDRE = 1;

#define GET_VAR_DESC_80 0x800b
#define GET_VAR_DESC_81 0x810b
#define GET_VAR_DESC_82 0x820b
#define GET_ENUM_DESC   0x800c
#define SET_VAR_STATUS  0x0404
#define SET_VAR_STATE   0x0504
#define GET_VAR_WASP_VER       0x0601
#define GET_VAR_MODE    0x8001
#define SET_VAR_MODE    0x8004
#define SET_VAR_TIME    0x8104
#define PRINTF(x) printf x

#include "arcfl_fw/Debug/scr/main.c"

void JardenRx(unsigned char RxByte)
{
   RDR_TDR = RxByte;
   int_uart_rx();
}

void SimJardenTxIsr(void);

void SimJardenTxIsr()
{
   static __BYTE TxBuf[128];
   static int TxPtr = 0;

   if(RXD_FLG==1) {
      printf("Complete frame received, calling do_rec()\n");
      do_rec();
   }

   if(SSR_TIE) {
      while(SSR_TIE) {
         int_uart_tx();
         TxBuf[TxPtr++] = RDR_TDR;
         if(TxPtr >= sizeof(TxBuf)) {
            printf("TxBuf overflow\n");
            break;
         }
      }
      printf("Jarden serial out:\n");
      DumpHex(TxBuf,TxPtr);
      if(gJardenMode == 99) {
      // Send live reponse
         JardenTx(TxBuf,TxPtr);
      }
      TxPtr = 0;
   }
}

void SimJarden(int Mode)
{
   __BYTE NopCmd[] = {
      0x10,0x01,0x00,0x00,0x10,0x03,0x00,0x00  
   };

   __BYTE GetVarDesc_80[] = {
      0x10,0x01,0x02,0x0b,0x80,0x10,0x03,0x8d,0x00
   };
   /* expected response 
   10 01 1d 8b 00 01 04 e8 03 00 03 4d 6f 64 65 10  ...........Mode.
   03 20 03  . . 
   */

   int i;
   __BYTE *Data;
   int DataLen;

   switch(Mode) {
      case 0:  // nop
         Data = NopCmd;
         DataLen = sizeof(NopCmd);
         break;

      case 1:  // Get variable description for 0x80
         Data = GetVarDesc_80;
         DataLen = sizeof(GetVarDesc_80);
         break;

      default:
         printf("Invalid argument\n");
         return;
   }

   for(i = 0; i < DataLen; i++) {
      SimJardenTxIsr();
      
      JardenRx(Data[i]);
      if(RXD_FLG==1) {
         printf("Complete frame received, calling do_rec()\n");
         do_rec();
      }
   }
   SimJardenTxIsr();
}

