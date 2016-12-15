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
/*
   WASP ASYNC protocol handler 
 
*/
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <termios.h>
#include <fcntl.h>
#include <syslog.h>

#include "wasp.h"
#define WASPD_PRIVATE
#include "wasp_api.h"
#include "waspd.h"
#include "wasp_async.h"

#define DLE                   0x10
#define SOH                   0x01
#define STX                   0x02
#define ETX                   0x03

// Max time to wait for a response before resending command in milliseconds
#define RESPONSE_TIMEOUT      1000

#ifdef DEBUG
#define RECEIVE_ERR RecieveErr(__FUNCTION__,__LINE__)

void RecieveErr(const char *Function,int Line);

void RecieveErr(const char *Function,int Line)
{
   LOG("%s#%d: Receive error.\n",Function,Line);
}

#else
#define RECEIVE_ERR
#endif

void ParseSerialData(WaspAsyncCtrl *pCtrl,u8 RxByte);
int sio_async_init(const char *Device,int Baudrate);
void QueueMsg(WaspAsyncCtrl *p,WaspMsg *pResponse);
void DumpTermios(struct termios *p);
void RegisterAsyncHandler(WaspAsyncCtrl *p,WaspMsgHandler *pAsyncHandler);

#ifdef SIMULATE_JARDEN
void JardenRx(u8 RxByte);
#endif

void ParseSerialData(WaspAsyncCtrl *p,u8 RxByte)
{
   if(p->RxState == DLE_ESCAPE) {
      // The last character was an DLE
      switch(RxByte) {
         case SOH:
            // start of a new frame
            if(p->RxCount > 1) {
               // unexpected header, restart everything
               // count everything except the DLE as garbage
               p->GarbageCharCount += p->RxCount - 1;
               RECEIVE_ERR;
            }
            p->RxCount = 0;
            p->Checksum = 0;
            p->RxBuf[p->RxCount++] = DLE;
            p->RxState = GET_DATA;
            break;

         case ETX:
            p->RxState = GET_CHECK1;
            break;

         case DLE:
            switch(p->LastState) {
               case GET_DATA:
                  p->Checksum += RxByte;
               case DLE_WAIT:
                  p->RxState = p->LastState;
                  break;
         
               case GET_CHECK1:
                  p->RxState = GET_CHECK2;
                  break;
         
               case GET_CHECK2:
                  p->RxState = CHECK_MSG;
                  break;

               default:
                  // illegal, go back to looking for header
                  p->IllegalFrame++;
                  RECEIVE_ERR;
                  p->RxState = DLE_WAIT;
                  p->RxCount = 0;
                  break;
            }
            break;

         default:
            // illegal, go back to looking for header
            p->IllegalFrame++;
            RECEIVE_ERR;
            p->RxState = DLE_WAIT;
            p->RxCount = 0;
            break;
      }
      if(RxByte != DLE) {
         p->RxBuf[p->RxCount++] = RxByte;
      }
   }
   else {
   // The last character wasn't an DLE
      p->RxBuf[p->RxCount++] = RxByte;

      if(RxByte == DLE) {
      // It's a Data Link Escape
         p->LastState = p->RxState;
         p->RxState = DLE_ESCAPE;
      }
      else {
         switch(p->RxState) {
            case DLE_WAIT:
               p->RxCount = 0;
               p->GarbageCharCount++;
               break;
      
            case GET_DATA:
               p->Checksum += RxByte;
               break;
      
            case GET_CHECK1:
               p->RxState = GET_CHECK2;
               break;
      
            case GET_CHECK2:
               p->RxState = CHECK_MSG;
               break;

            default:
               // illegal, go back to looking for header
               p->IllegalFrame++;
               RECEIVE_ERR;
               p->RxState = DLE_WAIT;
               p->RxCount = 0;
               break;
         }
      }
   }

   if(p->RxState == CHECK_MSG) do {
   // Message is complete
      int MsgLen = p->RxCount-ASYNC_OVERHEAD;
      u8 CmdByte = p->RxBuf[BUF_OFFSET_CMD];
      u8 Err = p->RxBuf[BUF_OFFSET_ERR];

      u8 bResp = (CmdByte & WASP_CMD_RESP) == 0 ? FALSE : TRUE;
      u16 SenderCheckSum = (p->RxBuf[p->RxCount-1] << 8) |
                           p->RxBuf[p->RxCount-2];

      if(gWASP_Verbose & LOG_RAW_SERIAL) {
         LOG("%s: received complete %d byte frame:\n",__FUNCTION__,p->RxCount);
         DumpHex(p->RxBuf,p->RxCount);
      }
      p->RxState = DLE_WAIT;
      p->RxCount = 0;

      if(SenderCheckSum != p->Checksum) {
         p->RxBadChecksum++;
         RECEIVE_ERR;
         LOG("%s: Ignoring message with checksum error (0x%04x != 0x%04x).\n",
            __FUNCTION__,SenderCheckSum,p->Checksum);
         break;
      }
   // The checksum is good
      p->RxMsgCount++;
      if(MsgLen == 0 || ((bResp) && MsgLen < 2)) {
         p->TooSmall++;
         LOG("%s: Ignoring runt message.\n",__FUNCTION__);
         RECEIVE_ERR;
         break;
      }
   // The message length is good
      if(bResp && (CmdByte & ~WASP_CMD_RESP) == WASP_CMD_ASYNC_DATA) {
      // Async data
         if(p->pAsyncHandler != NULL) {
            p->Args.MsgLen = MsgLen;
            p->pAsyncHandler(&p->Args);
         }
         else {
            LOG("%s: Ignoring async data, no handler.\n",__FUNCTION__);
            RECEIVE_ERR;
         }
         break;
      }

      if(bResp && 
         (p->LastCommand == NULL || 
          (CmdByte & ~WASP_CMD_RESP) != p->LastCommand->CmdByte))
      { 
         p->UnexpectedResponse++;
         if(p->LastCommand == NULL) {
            LOG("%s: Ignoring unexpected response 0x%x, no LastCommand.\n",
                __FUNCTION__,CmdByte);
            RECEIVE_ERR;
         }
         else {
            LOG("%s: Ignoring unexpected response 0x%x, expecting 0x%x.\n",
                __FUNCTION__,CmdByte,
                p->LastCommand->CmdByte | WASP_CMD_RESP);

            LOG("%s: CmdByte: 0x%x, p->LastCommand->CmdByte 0x%x.\n",
                __FUNCTION__,CmdByte,p->LastCommand->CmdByte);

            RECEIVE_ERR;
         }
         break;
      }

      if(bResp && Err == WASP_ERR_COMM) {
      // The other end encountered a checksum error
         p->TxBadChecksum++;
         LOG("%s: Communications error, resending last command\n",
            __FUNCTION__);
         RECEIVE_ERR;
         p->RespTimer.tv_sec = 1;   // force immediate timeout
         break;
      }

      do {
         HandlerArgs *pArgs = &p->Args;
         pArgs->MsgLen = MsgLen;
         pArgs->Err = 0;

         if(bResp) {
         // A response, call the handler
            WaspMsg *pMsg = p->LastCommand;

            p->LastCommand = NULL;
            p->Retries = 0;
            p->RxRespCount++;
            p->RespTimer.tv_sec = 0;

            if(pMsg->pHandler != NULL) {
               pArgs->Data = pMsg->pHandlerData;
               CallResponseHandler(pMsg->pHandler,pArgs);
            }
            else {
               LOG("%s#%d: Internal error, no handler defined\n",
                   __FUNCTION__,__LINE__);
               RECEIVE_ERR;
            }
            free(pMsg);
         // Send the next message on the queue if there is one
            if((pMsg = p->CmdHead) != NULL) {
               p->CmdHead = pMsg->Link;
               pMsg->Link = NULL;
               if(p->TxHead == NULL) {
                  p->TxHead = pMsg;
               }
               else {
                  p->TxTail->Link = pMsg;
               }
               p->TxTail = pMsg;
            }
            WaspAsyncTransmit(p);
         }
         else {
         // A command
            u8 SeqNum = p->RxBuf[BUF_OFFSET_SEQ];

            if(SeqNum != 0 && SeqNum == p->RxSeqNum) {
            // Command retransmission, just send the last response, don't
            // execute the command again
               p->DupRxSeqCount++;
               RECEIVE_ERR;
               WaspMsg *pResponse = p->LastResponse;

               p->LastResponse = NULL;
               QueueMsg(p,pResponse);
               break;
            }
            p->RxCmdCount++;
            WaspDispatchCommand(pArgs);
         }
      } while (FALSE);
   } while(FALSE);

   if(p->RxCount == RX_BUF_SIZE) {
      // this message is too big, toss it!
      p->TooBig++;
      RECEIVE_ERR;
      p->RxState = DLE_WAIT;
   }
}

// Called when data is ready to be read from fs
int WaspAsyncReceive(WaspAsyncCtrl *pCtrl)
{
   int BytesRead;
   int Ret = 0;   // Assume no error, no new data
   int i;
   char RxBuf[RX_BUF_SIZE];

   do {
      BytesRead = read(pCtrl->fd,RxBuf,sizeof(RxBuf));
      if(BytesRead < 0) {
         if(errno != EAGAIN) {
            Ret = errno;
            LOG("%s: read failed - %s\n",__FUNCTION__,strerror(errno));
         }
         break;
      }

#if 0
      if(gWASP_Verbose > 2) {
         LOG("%s: read %d bytes\n",__FUNCTION__,BytesRead);
         DumpHex(RxBuf,BytesRead);
      }
#endif

      for(i = 0; i < BytesRead; i++) {
         if(gJardenMode) {
#ifdef SIMULATE_JARDEN
            JardenRx(RxBuf[i]);
#endif
         }
         else {
            ParseSerialData(pCtrl,RxBuf[i]);
         }
      }
   } while(FALSE);

   return Ret;
}

WaspMsg *WaspAsyncEncode(u8 *Data, int MsgLen,int SeqNum)
{
   u8 *cp;
   u8 c;
   WaspMsg *pMsg = NULL;
   int i;
   int OverHead = ASYNC_OVERHEAD + 2;  // pad for possible checksum DLE escapes
   u16 Checksum = 0;

   do {
   // Count the number of DLE data bytes we'll need to escape
      for(i = 0; i < MsgLen ; i++) {
         if(Data[i] == DLE) {
            OverHead++;
         }
      }
      if(SeqNum == DLE) {
         OverHead++;
      }

      if((pMsg = malloc(sizeof(WaspMsg) + MsgLen + OverHead)) == NULL) {
         LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
         break;
      }

      pMsg->pBody = ((u8 *) pMsg) + sizeof(WaspMsg);

      pMsg->Link = NULL;
      pMsg->CmdByte = *Data;
      cp = pMsg->pBody; 
      *cp++ = DLE;
      *cp++ = SOH;
      *cp++ = (u8) SeqNum;
      Checksum += (u8) SeqNum;
      if(SeqNum == DLE) {
      // Escape DLE, do not include in checksum
         *cp++ = DLE;
      }

      for(i = 0; i < MsgLen ; i++) {
         c = Data[i];
         *cp++ = c;
         Checksum += c;
         if(c == DLE) {
         // Escape DLE data, do not include in checksum
            *cp++ = c;
         }
      }
      *cp++ = DLE;
      *cp++ = ETX;

      c = (u8) (Checksum & 0xff);
      *cp++ = c;
      if(c == DLE) {
      // Escape DLE data, do not include in checksum
         *cp++ = c;
      }
      c = (u8) ((Checksum >> 8) & 0xff);
      *cp++ = c;
      if(c == DLE) {
      // Escape DLE data, do not include in checksum
         *cp++ = c;
      }
   // Save total size of encoded Async message
      pMsg->DataSize = cp - pMsg->pBody;
   } while(FALSE);

   return pMsg;
}

// Called when FD is ready for data
// Return:
//  > 0 - Error
int WaspAsyncTransmit(WaspAsyncCtrl *pCtrl)
{
   int Ret = 0;
   int Bytes2Send;
   int BytesSent;
   WaspMsg *p;

   while((p = pCtrl->TxHead) != NULL) {
   // Something more to send
      Bytes2Send = p->DataSize - pCtrl->TxCount;
      BytesSent = write(pCtrl->fd,&p->pBody[pCtrl->TxCount],Bytes2Send);

      if(BytesSent < 0) {
      // Error
         if(errno == EAGAIN) {
         // Output buffer is just full
            pCtrl->bAsyncTxActive = TRUE;
            break;
         }
         LOG("%s: write failed - %s\n",__FUNCTION__,strerror(errno));
         Ret = errno;
         break;
      }
      else if(BytesSent == Bytes2Send) {
      // Finished with this one
         pCtrl->bAsyncTxActive = FALSE;
         WaspMsg *pMsg = pCtrl->TxHead;

         if(gWASP_Verbose & LOG_RAW_SERIAL) {
            LOG("%s: Sent:\n",__FUNCTION__);
            DumpHex(pMsg->pBody,pMsg->DataSize);
         }

         pCtrl->TxHead = pMsg->Link;
         pCtrl->TxMsgCount++;
         pCtrl->TxCount = 0;

         if(pMsg->CmdByte == 0xff) {
         // Raw frame, toss it
            free(pMsg);
         }
         else if(pMsg->CmdByte & WASP_CMD_RESP) {
         // This is a response, save it in case we need to send it again
            if(pCtrl->LastResponse != NULL) {
               free(pCtrl->LastResponse);
            }
            pCtrl->LastResponse = pMsg;
         }
         else {
         // This is a command, save it in case we need to send it again
            if(pCtrl->LastCommand != NULL) {
               free(pCtrl->LastCommand);
            }
            pCtrl->LastCommand = pMsg;
         // Start the response timeout timer
            SetTimeout(&pCtrl->RespTimer,RESPONSE_TIMEOUT);
         }
      }
      else {
      // Sent some, but not all of the data
         pCtrl->TxCount += BytesSent;
         pCtrl->bAsyncTxActive = TRUE;
         break;
      }
   }

   return Ret;
}

// Called periodically to check for timeouts
int WaspAsyncPoll(WaspAsyncCtrl *p)
{
   if(p->RespTimer.tv_sec != 0 && TimeLapse(&p->RespTimer) >= 0) {
   // Time to try sending the last command again
      WaspMsg *pMsg = p->LastCommand;
      p->RespTimer.tv_sec = 0;
      if(pMsg == NULL) {
         LOG("%s#%d: Internal error, no command to resend\n",
             __FUNCTION__,__LINE__);
      }
      else {
         p->LastCommand = NULL;
         if(pMsg->pBody[BUF_OFFSET_SEQ] == 0) {
         // Initial command, just keep sending it until we get a response
            if(gWASP_Verbose & LOG_SERIAL) {
               LOG("%s: Reply timeout, resending initial command.\n",
                   __FUNCTION__);
            }
            QueueMsg(p,pMsg);
         }
         else if(++p->Retries < MAX_RETRIES) {
            p->TotalRetries++;
            if(gWASP_Verbose & LOG_SERIAL) {
               LOG("%s: Reply timeout, resending command.\n",__FUNCTION__);
            }
            QueueMsg(p,pMsg);
         }
         else {
         // Retry count exceeded
            HandlerArgs *pArgs = &p->Args;

            p->Retries = 0;
            LOG("%s: Error retry count exceeded.\n",__FUNCTION__);
            pArgs->Data = pMsg->pHandlerData;
            pArgs->Err = WASP_ERR_NO_RESPONSE;
            CallResponseHandler(pMsg->pHandler,pArgs);
            free(pMsg);
         }
      }
   }
   return 0;
}


void WaspAsyncShutdown(WaspAsyncCtrl *p)
{
   WaspMsg *pMsg;

   if(p != NULL) {
      if(p->fd >= 0) {
         close(p->fd);
      }

   // Free messages
      while((pMsg = p->TxHead) != NULL) {
         p->TxHead = pMsg->Link;
         free(pMsg);
      }

      if(p->LastCommand != NULL) {
         free(p->LastCommand);
      }

      if(p->LastResponse != NULL) {
         free(p->LastResponse);
      }

      if(p->Device != NULL) {
         free(p->Device);
      }

      free(p);
   }
}


// Returns serial fd or < 0 on error
#define SERIAL_OPEN_FLAGS O_FSYNC | O_APPEND | O_RDWR | O_NOCTTY | O_NDELAY
int WaspAsyncInit(const char *DevicePath,WaspAsyncCtrl **pRetCtrl)
{
   int Ret = 0;
   int Err;
   WaspAsyncCtrl *pCtrl = malloc(sizeof(WaspAsyncCtrl));

   do {
      if(pCtrl == NULL) {
         Ret = -ENOMEM;
         break;
      }
      memset(pCtrl,0,sizeof(WaspAsyncCtrl));
      pCtrl->Device = strdup(DevicePath);
      pCtrl->RxState = DLE_WAIT;
      pCtrl->Args.p = pCtrl;
      pCtrl->Args.Msg = &pCtrl->RxBuf[BUF_OFFSET_CMD];

      if((pCtrl->fd = open(DevicePath,SERIAL_OPEN_FLAGS)) < 0) {
         LOG("Couldn't open \"%s\": %s\n",DevicePath,strerror(errno));
         Ret = -errno;
         break;
      }
      Ret = pCtrl->fd;

      if((Err = WaspAsyncSetBaudRate(pCtrl,WASP_DEFAULT_BAUDRATE)) != 0) {
         LOG("%s: WaspAsyncSetBaudRate failed\n",__FUNCTION__);
         Ret = -Err;
         break;
      }
      *pRetCtrl = pCtrl;
   } while(FALSE);

   if(Ret < 0 && pCtrl != NULL) {
      WaspAsyncShutdown(pCtrl);
   }

   return Ret;
}



int WaspAsyncSendRaw(WaspAsyncCtrl *pCtrl,u8 *Data,int MsgLen)
{
   int Ret = 0;
   WaspMsg *pMsg;

   do {
      if((pMsg = malloc(sizeof(WaspMsg) + MsgLen)) == NULL) {
         LOG("%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
         break;
      }
      pMsg->pBody = ((u8 *) pMsg) + sizeof(WaspMsg);
      memcpy(pMsg->pBody,Data,MsgLen);
      pMsg->DataSize = MsgLen;
      pMsg->CmdByte = 0xff;
      pMsg->Link = NULL;
      pMsg->pHandler = NULL;
      pMsg->pHandlerData = NULL;

      if(pCtrl->TxHead == NULL) {
         pCtrl->TxHead = pMsg;
      }
      else {
         pCtrl->TxTail->Link = pMsg;
      }
      pCtrl->TxTail = pMsg;

      if(!pCtrl->bAsyncTxActive) {
      // Start sending the message now
         WaspAsyncTransmit(pCtrl);
      }
   } while(FALSE);

   return Ret;
}

int WaspAsyncSendCmd(
   WaspAsyncCtrl *pCtrl,
   u8 *Data,
   int MsgLen,
   WaspMsgHandler *pHandler,
   void *pHandlerData)
{
   int Ret = 0;
   int bResp = *Data & WASP_CMD_RESP;

   do {
      WaspMsg *pMsg = WaspAsyncEncode(Data,MsgLen,pCtrl->TxSeqNum);
      if(pMsg == NULL) {
         break;
      }
      pMsg->pHandler = pHandler;
      pMsg->pHandlerData = pHandlerData;

      if(++pCtrl->TxSeqNum == 0) {
      // Skip sequence number 0 to allow other end to detect a restart
         pCtrl->TxSeqNum = 1;
      }

      if(!bResp) {
      // this is a command, only send it if we don't already have a
      // outstanding command
         if(pCtrl->LastCommand != NULL) {
         // Can't send it now, queue it for later
            if(pCtrl->CmdHead == NULL) {
               pCtrl->CmdHead= pMsg;
            }
            else {
               pCtrl->CmdTail->Link = pMsg;
            }
            pCtrl->CmdTail= pMsg;
            break;
         }
      }

      if(pCtrl->TxHead == NULL) {
         pCtrl->TxHead = pMsg;
      }
      else {
         pCtrl->TxTail->Link = pMsg;
      }
      pCtrl->TxTail = pMsg;

      if(!pCtrl->bAsyncTxActive) {
      // Start sending the message now
         WaspAsyncTransmit(pCtrl);
      }
   } while(FALSE);

   return Ret;
}

int WaspAsyncSendResp(WaspAsyncCtrl *pCtrl,u8 *Data,int MsgLen)
{
   return WaspAsyncSendCmd(pCtrl,Data,MsgLen,NULL,NULL);
}

int WaspAsyncSetBaudRate(WaspAsyncCtrl *pCtrl,u32 BaudRate)
{
   struct termios options;
   int BaudrateConstant;
   int Err = 0;   // assume the best
   int fd = pCtrl->fd;

   do {
      switch(BaudRate) {
         case 1200:
            BaudrateConstant = B1200;
            break;

         case 2400:
            BaudrateConstant = B2400;
            break;

         case 4800:
            BaudrateConstant = B4800;
            break;

         case 9600:
            BaudrateConstant = B9600;
            break;

         case 19200:
            BaudrateConstant = B19200;
            break;

         case 38400:
            BaudrateConstant = B38400;
            break;

         case 57600:
            BaudrateConstant = B57600;
            break;

        default:
           Err = EINVAL;
           break;
      }

      if(Err == EINVAL) {
         LOG("Error: Invalid or unsupported baud rate %d\n",BaudRate);
         break;
      }


     if(tcgetattr(fd,&options) < 0) {
       LOG("Unable to read port configuration: %s\n",strerror(errno));
       Err = errno;
       break;
     }

     /* There's a bug in the FTDI FT232 driver that requires
        hardware handshaking to be toggled before data is
        transfered properly.  If you don't do this (or run minicom manually)
        received data will be all zeros and nothing will be transmitted.
      
        Although it shouldn't matter we'll only toggle hardware handshaking
        for USB connected serial ports.
     */

     if(gWASP_Verbose & LOG_V2) {
        LOG("Termios before modification:\n");
        DumpTermios(&options);
     }

     if(strstr(pCtrl->Device,"USB") != 0) {
        options.c_cflag |= CRTSCTS;       /* enable hardware flow control */
        if(gWASP_Verbose & LOG_V2) {
           LOG("Attempting to bypass FTDI FT232 driver bug.\n");
           LOG("Termios with hardware flow control enabled:\n");
           DumpTermios(&options);
        }
        if(tcsetattr(fd,TCSANOW,&options) < 0) {
           LOG("tcsetattr() failed: %s\n",strerror(errno));
           Err = -errno;
           break;
        }
        options.c_cflag &= ~CRTSCTS;      /* disable hardware flow control */
        if(gWASP_Verbose & LOG_V2) {
           LOG("Termios with hardware flow control disabled:\n");
           DumpTermios(&options);
        }
        if(tcsetattr(fd,TCSANOW,&options) < 0) {
           LOG("tcsetattr() failed: %s\n",strerror(errno));
           Err = -errno;
           break;
        }
     }

     options.c_oflag &= ~OPOST;
     options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  // raw mode
     options.c_iflag &= ~(IXON | IXOFF | IXANY);  /* disable software flow control */
     options.c_iflag |=  IGNBRK;            /* ignore break */
     options.c_iflag &= ~ISTRIP;            /* do not strip high bit */
     options.c_iflag &= ~(INLCR | ICRNL);   /* do not modify CR/NL   */
     options.c_cflag |=  (CLOCAL | CREAD);  /* enable the receiver and set local mode */
     options.c_cflag &= ~CRTSCTS;           /* disable hardware flow control */

     options.c_cflag &= ~CSIZE;             /* Mask the character size bits */
     options.c_cflag |= CS8;                /* 8 data bits */
     options.c_cflag &= ~PARENB;            /* no parity */
     options.c_cflag &= ~CSTOPB;            /* 1 stop bit */

     if(cfsetspeed(&options,BaudrateConstant) == -1) {
        LOG("cfsetspeed() failed: %s\n",strerror(errno));
        Err = errno;
        break;
     }

     if(gWASP_Verbose & LOG_V2) {
        LOG("Termios after modification:\n");
        DumpTermios(&options);
     }

     if(tcsetattr(fd,TCSANOW,&options) < 0) {
        LOG("tcsetattr() failed: %s\n",strerror(errno));
        Err = errno;
        break;
     }
  } while(FALSE);

  if(Err == 0) {
     pCtrl->Baudrate = BaudRate;
  }
  return Err;
}

void QueueMsg(WaspAsyncCtrl *p,WaspMsg *pMsg)
{
   if(p->bAsyncTxActive) {
   // Queue behind current message
      pMsg->Link = p->TxHead->Link;
      p->TxHead->Link = pMsg;
   }
   else {
   // put response on head of queue
      pMsg->Link = p->TxHead;
      p->TxHead = pMsg;
      WaspAsyncTransmit(p);
   }
}

void RegisterAsyncHandler(WaspAsyncCtrl *p,WaspMsgHandler *pAsyncHandler)
{
   if(pAsyncHandler == NULL) {
      LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
   }
   else {
      p->pAsyncHandler = pAsyncHandler;
   }
}


void DumpTermios(struct termios *p)
{
   LOG("c_iflag: 0x%x\n",p->c_iflag);     /* input mode flags */
   LOG("c_oflag: 0x%x\n",p->c_oflag);     /* output mode flags */
   LOG("c_cflag: 0x%x\n",p->c_cflag);     /* control mode flags */
   LOG("c_lflag: 0x%x\n",p->c_lflag);     /* local mode flags */
   LOG("c_line: 0x%x\n",p->c_line);       /* line discipline */
   LOG("cc_cc:\n");
   DumpHex(p->c_cc,sizeof(p->c_cc));
}

