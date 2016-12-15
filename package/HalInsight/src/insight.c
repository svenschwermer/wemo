/***************************************************************************
*
*
*
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
*
*
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

#include "insight.h"

#define	INSIGHTD_PRIVATE
#include "insightd.h"

static int gSocket = -1;
static AdrUnion gHisAdr;

#ifdef DEBUG
#define LOG(format, ...) printf(format, ## __VA_ARGS__)
#else
#define LOG(format, ...) syslog(LOG_ERR,format, ## __VA_ARGS__)
#endif

static int UdpSend(int fd,AdrUnion *pToAdr,InsightdMsg *pMsg,int MsgLen)
{
	int Ret = 0;	// assume the best
	int BytesSent;

	BytesSent = sendto(fd,pMsg,MsgLen,0,&pToAdr->s,sizeof(pToAdr->s));
	if(BytesSent != MsgLen) {
		LOG("%s: sendto failed - %s\n",__FUNCTION__,strerror(errno));
		Ret = -errno;
	}

	return Ret;
}

int HAL_Init()
{
   int Ret = 0;

	if((gSocket = socket(AF_INET,SOCK_DGRAM,0)) == -1) {
		Ret = -errno;
		LOG("%s: socket() failed, %s\n",__FUNCTION__,strerror(Ret));
	}

	gHisAdr.i.sin_family = AF_INET;
	gHisAdr.PORT = htons(INSIGHTD_PORT);
	gHisAdr.ADDR = inet_addr("127.0.0.1");   // localhost

	//openlog("libinsight.so",0,LOG_USER);

   return Ret;
}

static int GetDataFromDaemon(int Type,void *p)
{
	int Ret = 0;	// assume the best
	int Err;
	int BytesRead;
	int MsgLen;
	int DataLen = 0;
	fd_set ReadFdSet;
	struct timeval Timeout;
	InsightdMsg Request;
	InsightdMsg Reply;
	socklen_t Len = sizeof(gHisAdr);

	do {
		if(p == NULL) {
		// Make sure we were passed a return data pointer
			Ret = EINVAL;
			LOG("%s: ERROR - passed null pointer\n",__FUNCTION__);
			break;
		}

		if(gSocket < 0) {
			if((Ret = HAL_Init()) != 0) {
				break;
			}
		}

		MsgLen = sizeof(Request.Hdr);
		memset(&Request,0,MsgLen);
		Request.Hdr.Cmd = Type;

		switch(Type) {
			case CMD_READ_REG:
			case CMD_WRITE_REG:
				DataLen = sizeof(Request.u.RegData);
				break;
			case CMD_SET_INSIGHTVAR:
				DataLen = sizeof(Request.u.InsightValues);
				break;
		}

		if(DataLen != 0) {
			MsgLen = offsetof(InsightdMsg,u) + DataLen;
			memcpy(&Request.u,p,DataLen);
		}

		if((Ret = UdpSend(gSocket,&gHisAdr,&Request,MsgLen)) != 0) {
         break;
		}

// Wait for data then read it
      FD_ZERO(&ReadFdSet);
      FD_SET(gSocket,&ReadFdSet);
      Timeout.tv_sec = 1;
      Timeout.tv_usec = 0;

      if((Err = select(gSocket+1,&ReadFdSet,NULL,NULL,&Timeout)) < 0) {
         LOG("%s: select failed - %s\n",__FUNCTION__,strerror(errno));
			Ret = -errno;
         break;
      }

		if(Err == 0) {
			LOG("%s: timeout waiting for data\n",__FUNCTION__);
			Ret = ERR_HAL_DAEMON_TIMEOUT;
			break;
      }

		BytesRead = recvfrom(gSocket,&Reply,sizeof(Reply),0,&gHisAdr.s,&Len);

		if(BytesRead > 0) {
			if(Reply.Hdr.Cmd != (Type | CMD_RESPONSE)) {
			// Opps
				Ret = ERR_HAL_INVALID_RESPONSE_TYPE;
			}
			else {
				memcpy(p,&Reply.u.Data,BytesRead-offsetof(InsightdMsg,u));
			}
		}
	} while (FALSE);

	return Ret;
}

int HAL_GetCurrentReadings(DataValues *p)
{
	return GetDataFromDaemon(CMD_GET_CURRENT_DATA,p);
}

int HAL_GetAverageReadings(DataValues *p)
{
	return GetDataFromDaemon(CMD_GET_AVERAGE_DATA,p);
}

int HAL_GetTotalPower(TotalData *p)
{
	return GetDataFromDaemon(CMD_GET_TOTAL,p);
}

int HAL_GetInsightVariables(InsightVars *p)
{
	return GetDataFromDaemon(CMD_GET_INSIGHTVAR,p);
}

int HAL_SetInsightVariables(InsightVars *p)
{
	return GetDataFromDaemon(CMD_SET_INSIGHTVAR,p);
}

int HAL_GetAndClearTotalPower(TotalData *p)
{
	return GetDataFromDaemon(CMD_GET_TOTAL_CLR,p);
}

int _HalGetRegister(RegisterData *p)
{
	return GetDataFromDaemon(CMD_READ_REG,p);
}

int _HalSetRegister(RegisterData *p)
{
	return GetDataFromDaemon(CMD_WRITE_REG,p);
}

int _HalReadCalRegisters(unsigned int *p)
{
	return GetDataFromDaemon(CMD_READ_CAL_REGS,p);
}

int HAL_Shutdown()
{
	MsgHeader Dummy;
	return GetDataFromDaemon(CMD_EXIT,&Dummy);
}

int HAL_Cleanup()
{
	if(gSocket >= 0) {
		close(gSocket);
		gSocket = -1;
	}

	return 0;
}

