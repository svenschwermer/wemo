/***************************************************************************
* Created by Belkin International, Software Engineering on XX/XX/XX.
* Copyright (c) 2012-2013 Belkin International, Inc. and/or its affiliates.
* All rights reserved.
*
* Belkin International, Inc. retains all right, title and interest (including
* all intellectual property rights) in and to this computer program, which is
* protected by applicable intellectual property laws.  Unless you have obtained
* a separate written license from Belkin International, Inc., you are not
* authorized to utilize all or a part of this computer program for any purpose
* (including reproduction, distribution, modification, and compilation into
* object code) and you must immediately destroy or return to Belkin
* International, Inc all copies of this computer program.  If you are
* licensed by Belkin International, Inc., your rights to utilize this
* computer program are limited by the terms of that license.
*
* To obtain a license, please contact Belkin International, Inc.
*
* This computer program contains trade secrets owned by Belkin International,
* Inc. and, unless unauthorized by Belkin International, Inc. in writing,
* you agree to maintain the confidentiality of this computer program and
* related information and to not disclose this computer program and related
* information to any other person or entity.
*
* THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND
* BELKIN INTERNATIONAL, INC. EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR
* IMPLIED, INCLUDING THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A
* PARTICULAR PURPOSE, TITLE, AND NON-INFRINGEMENT. 
* 
*   WASP ASYNC protocol handler 
***************************************************************************/

#include "wasp_embedded.h"
#include "wasp.h"

#define  NULL  (0)

#define DLE                   0x10
#define SOH                   0x01
#define STX                   0x02
#define ETX                   0x03

enum RxStates {
   GET_SOH,       // waiting for <dle><soh> start new frame
   GET_DATA,      // have <dle><soh>, getting data
   GET_CHECK1,    // waiting for first Checksum byte
   GET_CHECK2,    // waiting for second Checksum byte
   DLE_SOH,       // Got DLE in GET_SOH state
   DLE_DATA,      // Got DLE in GET_DATA state
   DLE_CHECK1,    // Got DLE in GET_CHECK1 state
   DLE_CHECK2     // Got DLE in GET_CHECK2 state
};

enum TxStates {
   SEND_DLE_SOH,
   SEND_SOH,
   SEND_SEQ,
   SEND_DLE_DLE,
   SEND_DATA,
   SEND_DLE_ETX,
   SEND_ETX,
   SEND_CHECK1,
   SEND_DLE_CHECK1,
   SEND_CHECK2,
   SEND_DLE_CHECK2,
   SEND_READY
};

/* Global variables */
u8 RxBuf[RX_BUF_SIZE];
u8 RxMsgLen;
static u8 CheckErrMsg[2];

/* Local variables */
static u8 RxState;
static u8 TxState;
static u8 TxMsgLen;
static u8 *TxData;
static u16 TxChecksum;
static u8 g_bAsync;
static u8 RxSeqNum;

void WaspInit()
{
   RxState = GET_SOH;
   TxState = SEND_READY;
   TxData = NULL;
}

/* Returns 1 if a new message is available in RxBuf */
u8 ParseWaspData(u8 RxByte)
{
   static u16 RxChecksum;

   if(RxMsgLen >= RX_BUF_SIZE) {
   // this message is too big, toss it!
      RxState = GET_SOH;
   }

   if(RxState < DLE_SOH && RxByte == DLE) {
   // process <DLE> escape sequence
      RxState += DLE_SOH;
      return 0;
   }
   else if(RxState >= DLE_SOH) {
   // The last character was an DLE
      switch(RxByte) {
         case SOH:
         // start of a new frame
            RxChecksum = 0;
            RxMsgLen = 0;
            RxState = GET_DATA;
            return 0;

         case ETX:
         // End of frame
            RxState = GET_CHECK1;
            return 0;

         case DLE:
         // Data DLE, // Return to prior state
            RxState -= DLE_SOH;
            break;

         default:
            // illegal, go back to looking for header
            RxState = GET_SOH;
            return 0;
      }
   }

   switch(RxState) {
      case GET_SOH:
      // Just keep waiting
         break;

      case GET_DATA:
         RxBuf[RxMsgLen++] = RxByte;
         RxChecksum += RxByte;
         break;

      case GET_CHECK1:
         RxChecksum -= RxByte;
         RxState = GET_CHECK2;
         break;

      case GET_CHECK2:
         RxState = GET_SOH;
         RxChecksum -= RxByte << 8;
         if(RxChecksum == 0) {
         // Message is complete and the checksum is correct
            if(RxBuf[SEQ_OFFSET] == 0 || RxBuf[SEQ_OFFSET] != RxSeqNum) {
            // Master restarted, or not a duplicate command
            // Accept the command
               RxSeqNum = RxBuf[SEQ_OFFSET];
               return 1;
            }
            else if(RxBuf[SEQ_OFFSET] == RxSeqNum && TxData != NULL) {
            // Duplicate command, resend the last response
               SendWaspMsg(TxData,TxMsgLen,g_bAsync);
            }
         }
         else {
         // Checksum error
         // NB: bypass SendWaspMsg so we can send the previous response 
         // again if needed
            if(TxState == SEND_READY) {
               TxData = CheckErrMsg;
               CheckErrMsg[0] = RxBuf[CMD_OFFSET] | WASP_CMD_RESP;
               CheckErrMsg[1] = WASP_ERR_COMM;
               TxMsgLen = 2;
               TxState = SEND_DLE_SOH;
               TxChecksum = 0;
               g_bAsync = 0;
               break;
            }
         }
         break;

      default:
      // illegal, go back to looking for header
         RxState = GET_SOH;
         break;
   }

   return 0;
}

// Returns data byte to send
u8 GetWaspTxData()
{
   static u8 TxCount;
   u8 Ret;

   switch(TxState++) {
      case SEND_DLE_SOH:
      case SEND_DLE_DLE:
      case SEND_DLE_ETX:
      case SEND_DLE_CHECK1:
      case SEND_DLE_CHECK2:
         Ret = DLE;
         break;

      case SEND_SOH:
         Ret = SOH;
         TxCount = 0;
         break;

      case SEND_SEQ:
         if(g_bAsync) {
            Ret = 0;
         }
         else {
            Ret = RxBuf[SEQ_OFFSET];
         }
         TxChecksum += Ret;
         if(Ret != DLE) {
            TxState = SEND_DATA;
         }
         break;

      case SEND_DATA:
         Ret = TxData[TxCount++];
         TxChecksum += Ret;

         if(Ret == DLE) {
            TxState = SEND_DLE_DLE;
         }
         else {
         // Stay SEND_DATA state until TxCount reaches TxMsgLen
            TxState = SEND_DATA;
         }
         break;

      case SEND_ETX:
         Ret = ETX;
         break;

      case SEND_CHECK1:
         Ret = (TxChecksum &  0xff);
         if(Ret != DLE) {
            TxState = SEND_CHECK2;
         }
         break;

      case SEND_CHECK2:
         Ret = ((TxChecksum >> 8) &  0xff);
         if(Ret != DLE) {
            TxState = SEND_READY;
         }
         break;

      default:
      // This shouldn't happen!  Timeout the watchdog timer
         for( ; ; ) {
            Ret++;
         }
   }

   if(TxState > SEND_READY) {
      // This shouldn't happen!  Timeout the watchdog timer
         for( ; ; ) {
            Ret++;
         }
   }

   if(TxState == SEND_DATA && TxCount == TxMsgLen) {
   // Nothing more to send
      TxState = SEND_DLE_ETX;
   }

   return Ret;
}

// Send WASP message.
// Returns 0 if message queued for transmission
u8 SendWaspMsg(u8 *Msg,u8 MsgLen,u8 bAsync)
{
   if(TxState == SEND_READY) {
      TxMsgLen = MsgLen;
      TxData = Msg;
      TxState = SEND_DLE_SOH;
      TxChecksum = 0;
      g_bAsync = bAsync;
      return 0;
   }
// Already transmitting 
   return 1;
}

u8 WaspTxReady()
{
   return TxState == SEND_READY ? 0 : 1;
}

