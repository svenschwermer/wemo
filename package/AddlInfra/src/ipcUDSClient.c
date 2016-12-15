/***************************************************************************
*
*
* ipcUDSClient.c
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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <semaphore.h>
#include "types.h"
#include "global.h"
#include "logger.h"

#include "ipcUDS.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

int gUDSClientDataFD = -1;
int gWebIconVersion = 0;

#ifdef PRODUCT_WeMo_Baby
int GetNetworkState()
{
	int NetworkState = 0; 
	pIPCCommand response = NULL;
	pIPCNetworkStateResp status = NULL;

	response = ProcessClientIPCCommand(IPC_CMD_GET_NETWORK_STATE,SOCK_BABY_PATH);
	if (response)
	{
		status = (pIPCNetworkStateResp)(response->arg);
		if (status)
		{
			NetworkState = status->value;
			free(status);status = NULL;
		}
		free(response);response = NULL;
	}

	APP_LOG("UDSClient:", LOG_DEBUG, "*****NetworkState = %d\n", NetworkState);
	return NetworkState;
}

int GetServerEnv()
{
        int ServerEnv = 0;
        pIPCCommand response = NULL;
        pIPCServerEnvResp status = NULL;

        response = ProcessClientIPCCommand(IPC_CMD_GET_SERVER_ENVIRONMENT,SOCK_BABY_PATH);
        if (response)
        {
                status = (pIPCServerEnvResp)(response->arg);
                if (status)
                {
                        ServerEnv = status->value;
			free(status);status = NULL;
                }
                free(response);response = NULL;
        }

        APP_LOG("UDSClient:", LOG_DEBUG, "*****ServerEnv = %d\n", ServerEnv);
	return ServerEnv;
}

int GetSignalStrength()
{
        int SignalStrength = 0;
        pIPCCommand response = NULL;
        pIPCSignalStrengthResp status = NULL;

        response = ProcessClientIPCCommand(IPC_CMD_GET_SIGNAL_STRENGTH,SOCK_BABY_PATH);
        if (response)
        {
                status = (pIPCSignalStrengthResp)(response->arg);
                if (status)
                {
                        SignalStrength = status->value;
                        free(status);status = NULL;
                }
                free(response);response = NULL;
        }

        APP_LOG("UDSClient:", LOG_DEBUG, "*****SignalStrength = %d\n", SignalStrength);
        return SignalStrength;
}

int PunchAppWatchDogState()
{
        int WatchdogStatus = 0;
        pIPCCommand response = NULL;
        pIPCSignalStrengthResp status = NULL;

        response = ProcessClientIPCCommand(IPC_CMD_PUNCH_APP_WATCHDOG_STATE,SOCK_BABY_PATH);
        if (response)
        {
                status = (pIPCSignalStrengthResp)(response->arg);
                if (status)
                {
                        WatchdogStatus = status->value;
                        free(status);status = NULL;
                }
		free(response);response = NULL;
	}
	if (WatchdogStatus)
	{
	}
	else
	{
		APP_LOG("UDSClient:", LOG_DEBUG, "*****WatchdogStatus punch Fail  = %d", WatchdogStatus);
	}
        return WatchdogStatus;
}
#endif /* #ifdef PRODUCT_WeMo_Baby */

pIPCCommand ProcessClientIPCCommand(pIPCCommand pCmd, char *UDSSocketPath)
{
	pIPCCommand pgetResp = NULL;
	int retVal = ERROR,  clientFd = ERROR;

	clientFd = StartIPCClient(UDSSocketPath);
	if(ERROR == clientFd)
	{
		APP_LOG("UDSClient:", LOG_ERR, "Failed to start IPC client");
		return NULL;
		}

	if(ERROR == SendIPCCommand(pCmd, clientFd))
	{
		APP_LOG("UDSClient:", LOG_ERR, "Failed to to send IPC command");
		return NULL;
	}

	pgetResp = (pIPCCommand)CALLOC(1, sizeof(IPCCommand));
	if(pgetResp == NULL)
	{
		APP_LOG("UDSClient:", LOG_ERR, "calloc failed\n");
		resetSystem();
	}

	retVal = GetIPCResponse(pgetResp, clientFd);
	if(ERROR != retVal)
	{
		/*close UDS client socket*/
		if(IPC_CMD_NAT_START_REMOTE_DATA_PATH == pCmd->cmd)
		{
			gUDSClientDataFD = clientFd;
			APP_LOG("UDSClient:",LOG_DEBUG, "UDS Client data path Socket success on socket:%d", gUDSClientDataFD);
		}
		else
		{
			close(clientFd);
			APP_LOG("UDSClient:",LOG_DEBUG, "Closed UDS Client Socket:%d", clientFd);
		}
	}
	else
	{
		free(pgetResp->arg);pgetResp->arg = NULL;
		free(pgetResp);pgetResp = NULL;
		APP_LOG("UDSClient:", LOG_ERR, "ipcUDS Get Response failed");
	}

	return pgetResp;
}

int StartIPCClient(char *UDSSocketPath)
{
	int clientSock, len;
	struct sockaddr_un remote;

	if ((clientSock = socket(AF_UNIX, SOCK_STREAM, 0)) == ERROR) 
	{
		APP_LOG("UDSClient:", LOG_ERR, "client socket creation error!, errno:%d", errno);
		return ERROR;
	}

	remote.sun_family = AF_UNIX;
	strncpy(remote.sun_path, UDSSocketPath, sizeof(remote.sun_path)-1);
	len = strlen(remote.sun_path) + sizeof(remote.sun_family);
	if (connect(clientSock, (struct sockaddr *)&remote, len) == ERROR) 
	{
		APP_LOG("UDSClient:", LOG_ERR, "Connected to IPC server failed, errno:%d", errno);
		close(clientSock);
		return ERROR;
	}
	APP_LOG("UDSClient",LOG_DEBUG,"IPC Client started on Socket:%d", clientSock);

	return clientSock;
}

int SendIPCCommand(pIPCCommand psCmd, int aClientFd)
{
	unsigned int totalBytesToSend = IPC_CMD_HDR_LEN + psCmd->arg_length;
	unsigned int byteSend = 0;
	char *ptr = (char*)psCmd;

	while(totalBytesToSend)
	{
		if(totalBytesToSend <= MAX_SEND_BUF_LEN)
			byteSend = totalBytesToSend;
		else
			byteSend = MAX_SEND_BUF_LEN;

		if(send(aClientFd, ptr, byteSend, 0) < 0)
		{
			APP_LOG("UDSClient:",LOG_ERR, "Socket Sent error, errno:%d", errno);
			return FAILURE;
		}
		else
		{
			ptr += byteSend;
			totalBytesToSend -= byteSend;
			APP_LOG("UDSClient:",LOG_ERR, "IPC client Socket Sent success:%d, remaining:%d", byteSend, totalBytesToSend);
		}

	}
	return SUCCESS;
}

int GetIPCResponse(pIPCCommand pRespbuff, int aClientFd)
{
	char *payload_buffer = NULL;
	int nbytes;

	nbytes = recv(aClientFd, (char*)pRespbuff, IPC_CMD_HDR_LEN, 0x0);
	if(nbytes <= 0)
	{
		APP_LOG("UDSClient:", LOG_ERR, "header read error, nbytes:%d, errno:%d", nbytes,errno);
		return ERROR;
	}

	payload_buffer = NULL;
	if(pRespbuff->arg_length > 0x0)
	{
		nbytes = 0;
		payload_buffer = (char*)CALLOC(1, pRespbuff->arg_length);
		if(payload_buffer == NULL )
		{
			APP_LOG("UDSClient:", LOG_ERR, " payload buffer calloc failed");
			resetSystem();
		}

		nbytes = recv(aClientFd, payload_buffer, pRespbuff->arg_length, 0x0);
		if(nbytes <= 0)
		{
			APP_LOG("UDSClient:", LOG_ERR, "payload read error, nbytes:%d, errno:%d", nbytes,errno);
			free(payload_buffer);
			payload_buffer = NULL;
			return ERROR;
		}
	}

	pRespbuff->arg = (void *)payload_buffer;

	return SUCCESS;
}
