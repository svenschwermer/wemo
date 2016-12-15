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
#ifndef _WASP_ASYNC_H_
#define _WASP_ASYNC_H_

#define MAX_MESSAGE_SIZE   255
#define ASYNC_OVERHEAD     7  // <del><soh><seq#>...<dle><etx><chklsb><chkmsb>
#define RX_BUF_SIZE        (MAX_MESSAGE_SIZE + ASYNC_OVERHEAD)

enum RxStates {
   DLE_WAIT = 1,     // waiting for DLE to start new frame
   GET_DATA,         // have <dle><soh>, getting data
   GET_CHECK1,       // waiting for first Checksum byte
   GET_CHECK2,       // waiting for second Checksum byte
   DLE_ESCAPE,       // Last byte was an <dle>, waiting for next byte
   CHECK_MSG,        // we have the complete frame, check it
};

struct WaspMsg_TAG;

typedef struct WaspMsg_TAG {
   struct WaspMsg_TAG *Link;  // NB: must be first
   int   DataSize;
   WaspMsgHandler *pHandler;
   void *pHandlerData;
   u8 *pBody;
   u8 CmdByte;
} WaspMsg;

typedef struct WaspAsyncCtrl_TAG {
   WaspMsg *TxHead;
   WaspMsg *TxTail;
   WaspMsg *CmdHead;
   WaspMsg *CmdTail;
   WaspMsg *LastCommand;
   WaspMsg *LastResponse;
   WaspMsgHandler *pAsyncHandler;

   char *Device;
   enum RxStates RxState;
   enum RxStates LastState;
   int RxCount;
   int TxCount;
   int bAsyncTxActive;
   #define MAX_RETRIES     3
   int Retries;
   struct timeval RespTimer;
   u32 Baudrate;
   int fd;
   HandlerArgs Args;

   u16 Checksum;
   u8  TxSeqNum;
   u8  RxSeqNum;
   u8  LastCmd;
   u8  RxBuf[RX_BUF_SIZE];
   #define BUF_OFFSET_SEQ   2
   #define BUF_OFFSET_CMD   3
   #define BUF_OFFSET_ERR   4
   
// Statistic counters
   u32 TxMsgCount;
   u32 RxMsgCount;
   u32 RxCmdCount;
   u32 RxRespCount;
   u32 RxBadChecksum;
   u32 TxBadChecksum;
   u32 GarbageCharCount;
   u32 IllegalFrame;
   u32 TooBig;
   u32 TooSmall;
   u32 NoMemErrs;
   u32 UnexpectedResponse;
   u32 TotalRetries;
   u32 DupRxSeqCount;
} WaspAsyncCtrl;


//
int WaspAsyncSetBaudRate(WaspAsyncCtrl *pCtrl,u32 BaudRate);
void WaspAsyncShutdown(WaspAsyncCtrl *p);
int WaspAsyncInit(const char *DevicePath,WaspAsyncCtrl **pRetCtrl);
int WaspAsyncPoll(WaspAsyncCtrl *p);
int WaspAsyncSendRaw(WaspAsyncCtrl *pCtrl,u8 *Data,int MsgLen);
int WaspAsyncSendCmd(WaspAsyncCtrl *pCtrl,u8 *Data,int MsgLen,
                     WaspMsgHandler *pHandler,void *pHandlerData);
int WaspAsyncSendResp(WaspAsyncCtrl *pCtrl,u8 *Data,int MsgLen);
int WaspAsyncReceive(WaspAsyncCtrl *pCtrl);
int WaspAsyncTransmit(WaspAsyncCtrl *pCtrl);
void RegisterAsyncHandler(WaspAsyncCtrl *p,WaspMsgHandler *pAsyncHandler);
#endif // _WASP_ASYNC_H_
