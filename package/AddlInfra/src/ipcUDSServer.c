/***************************************************************************
*
*
* ipcUDSServer.c
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
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/un.h>
#include <sys/msg.h>
#include "types.h"
#include "global.h"
#include "logger.h"
#include "ipcUDS.h"
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

#define MAX_LISTEN_QUEUE_LENGTH		5

int gWatchDogStatus = -1;
int gUDSServerDataFD = -1;

int IpcUdsSend(pIPCCommand sa_resp, int aChildFD)
{
	if(sa_resp == NULL)
	{
		APP_LOG("IpcUdsSend:",LOG_DEBUG, "response structure is empty\n");
		return ERROR;
	}

	if (send(aChildFD, (IPCCommand*)sa_resp, (IPC_CMD_HDR_LEN + sa_resp->arg_length), 0) < 0) 
	{
		APP_LOG("IpcUdsSend:",LOG_ERR, "Socket Send error, errno:%d", errno);
	}

	if(sa_resp!=NULL)
	{
		free(sa_resp);
		sa_resp=NULL;
	}

	return SUCCESS;
}

int StartIpcUdsServer(SserverTaskData *serverTaskData)
{
	APP_LOG("StartUdServer:",LOG_ERR, "start StartIpcUdsServer...");
	SChildFDTaskData *childFDTaskData = NULL;
	int server_sock, pClientFD, len;
	socklen_t t_len;
 	struct sockaddr_un local, remote;
        int retVal = 0;
	pthread_attr_t IpcUdsChildth_attr;
	pthread_t IpcUdsChild_thread;
	
	if(NULL == serverTaskData)
	{
		APP_LOG("StartIpcUdsServer:", LOG_ERR, "NULL argument");
		goto ON_ERROR;
	}
	
	tu_set_my_thread_name( __FUNCTION__ );

	if ((server_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == ERROR)
	{
		APP_LOG("StartIpcUdsServer:", LOG_ERR, "Server Socket Creation Failed, errno:%d", errno);
		goto ON_ERROR;
	}

	local.sun_family = AF_UNIX;
	strncpy(local.sun_path, serverTaskData->UDSSockPath, sizeof(local.sun_path)-1);
	unlink(local.sun_path);
	len = strlen(local.sun_path) + sizeof(local.sun_family);

	if (bind(server_sock, (struct sockaddr *)&local, len) == ERROR) 
	{
		APP_LOG("StartIpcUdsServer:", LOG_ERR, "bind failed, errno:%d", errno);
		goto ON_ERROR;
	}

	if (listen(server_sock, MAX_LISTEN_QUEUE_LENGTH) == ERROR)
	{
		APP_LOG("StartIpcUdsServer:", LOG_ERR, "Listen Failed, errno:%d", errno);
		goto ON_ERROR;
	}

	t_len = sizeof(remote);
	for(;;)
	{
		childFDTaskData = (SChildFDTaskData*)CALLOC(1, sizeof(SChildFDTaskData));
		if(childFDTaskData == NULL )
		{
			APP_LOG("StartIpcUdsServer:", LOG_ERR, "FD buffer calloc failed");
			resetSystem();
		}	
		pClientFD = -1;
		APP_LOG("StartIpcUdsServer:", LOG_ERR, "Wait on Accept.");
		if ((pClientFD = accept(server_sock, (struct sockaddr *)&remote, &t_len)) == -1)
		{
			APP_LOG("StartIpcUdsServer:", LOG_ERR, "Accept Failed, errno:%d", errno);
			if (NULL != childFDTaskData){free(childFDTaskData);childFDTaskData = NULL;}
			resetSystem();
		}
		else
		{
			childFDTaskData->funptr = serverTaskData->funptr;
			childFDTaskData->fd = pClientFD;

			pthread_attr_init(&IpcUdsChildth_attr);
			pthread_attr_setdetachstate(&IpcUdsChildth_attr, PTHREAD_CREATE_DETACHED);
			retVal = pthread_create(&IpcUdsChild_thread, &IpcUdsChildth_attr, ChildFDTask, (void*)childFDTaskData);
			if(retVal < 0)
			{
				APP_LOG("StartIpcUdsServer:",LOG_ERR, "Could not create unix domain child server thread... ");
				if (NULL != childFDTaskData){free(childFDTaskData);childFDTaskData = NULL;}
				resetSystem();
			}
			else
			{
				APP_LOG("StartIpcUdsServer:",LOG_DEBUG, "Created unix domain child server thread... ");
			}
		}

	}

ON_ERROR:
	if(serverTaskData){free(serverTaskData);serverTaskData=NULL;}

	APP_LOG("StartUdServer:",LOG_ERR, "return from StartIpcUdsServer...");
	return SUCCESS;
}

void* ChildFDTask(void *pChildFDTaskData)
{
	SChildFDTaskData *childFDTaskData = NULL;
	entryFunPtr ipcUDSCommandHandler;
	IPCCommand buffer;
	int nbytes;
	int ClientFD = -1;
	int retVal = 0;

	tu_set_my_thread_name( __FUNCTION__ );

	childFDTaskData = (SChildFDTaskData*)pChildFDTaskData;
	if (NULL == childFDTaskData)
	{
		APP_LOG("ChildFDTask:", LOG_ERR, "NULL FD Argumnet, exit thread!");
		return NULL;
	}

	ClientFD = childFDTaskData->fd;
	ipcUDSCommandHandler = childFDTaskData->funptr;

	APP_LOG("ChildFDTask:",LOG_DEBUG, "In Child FD Task:%d", ClientFD);

	memset(&buffer, 0, sizeof(IPCCommand));
	nbytes = recv(ClientFD, &buffer, IPC_CMD_HDR_LEN, 0x0);
	if(nbytes <= 0)
	{
		APP_LOG("ChildFDTask:", LOG_ERR, "header read error, errno:%d", errno);
		retVal = -1;
		goto ON_ERROR;
	}

	if(buffer.arg_length > 0x0)
	{
		nbytes = 0;
		buffer.arg = (char*)MALLOC(buffer.arg_length);
		if(buffer.arg == NULL )
		{
			APP_LOG("ChildFDTask:", LOG_ERR, "payload buffer calloc failed");
			resetSystem();
		}
		nbytes = recv(ClientFD, buffer.arg, buffer.arg_length, 0x0);
		if(nbytes <= 0)
		{
			APP_LOG("ChildFDTask:", LOG_ERR, "payload read error, errno:%d", errno);
			retVal = -1;
			goto ON_ERROR;
		}
	}

	ipcUDSCommandHandler(&buffer, ClientFD);	

ON_ERROR:
	if((IPC_CMD_NAT_START_REMOTE_DATA_PATH == buffer.cmd) && (retVal != -1)) 
	{
		gUDSServerDataFD = ClientFD;
		APP_LOG("ChildFDTask:", LOG_DEBUG, "UDS Server Data path started on FD:%d", gUDSServerDataFD);
	}
	else
	{
		APP_LOG("ChildFDTask:",LOG_DEBUG, "Closing Server Child FD Task:%d", ClientFD);
		close(ClientFD);
	}

	if(buffer.arg)free(buffer.arg); 
	if(childFDTaskData)free(childFDTaskData);

	APP_LOG("ChildFDTask:",LOG_DEBUG, "Exit child FD Task");

	return NULL;
}

#ifdef PRODUCT_WeMo_Baby
int ProcessServerIpcCmd(IPCCommand* s_cmd, int aClientFD)
{
	if (NULL == s_cmd)
		return FAILURE;

	switch(s_cmd->cmd)
	{
		case IPC_CMD_GET_NETWORK_STATE:
			SendNetworkState(s_cmd, aClientFD);
			break;
		case IPC_CMD_GET_SERVER_ENVIRONMENT:
			SendServerEnvironmnet(s_cmd, aClientFD);
			break;
		case IPC_CMD_GET_SIGNAL_STRENGTH:
			SendSignalStrength(s_cmd, aClientFD);
			break;
		case IPC_CMD_PUNCH_APP_WATCHDOG_STATE:
			SendWachDogStatus(s_cmd, aClientFD);
			break;

		default:
			APP_LOG("ProcessServerIpcCmd:", LOG_DEBUG, "\nUnknown command: %d", s_cmd->cmd);
			break;
	}

	return SUCCESS;
}

int SendNetworkState(IPCCommand *sa_cmd, int aClientFD)
{
	pIPCResp sa_resp = NULL;
	IPCNetworkStateResp getNetworkState;

	sa_resp = (pIPCResp)CALLOC((IPC_CMD_HDR_LEN + sizeof(IPCNetworkStateResp)), 1);
	if(sa_resp == NULL)
	{
		APP_LOG("SendNetworkState:", LOG_DEBUG, "sa_resp calloc failed");
		resetSystem();
	}

	//fill response header
	sa_resp->cmd = sa_cmd->cmd;
	sa_resp->arg_length = sizeof(IPCNetworkStateResp);

	//fill response payload
	getNetworkState.value = getCurrentClientState();
	memcpy((char *)sa_resp + IPC_CMD_HDR_LEN, &getNetworkState, sizeof(IPCNetworkStateResp));

	//send it on socket
	IpcUdsSend((pIPCCommand)sa_resp, aClientFD);

	return SUCCESS; 
}

int SendServerEnvironmnet(IPCCommand *sa_cmd, int aClientFD)
{
        pIPCResp sa_resp = NULL;
        IPCServerEnvResp getServerEnv;

        sa_resp = (pIPCResp)CALLOC((IPC_CMD_HDR_LEN + sizeof(IPCServerEnvResp)), 1);
        if(sa_resp == NULL)
        {
                APP_LOG("SendNetworkState:", LOG_DEBUG, "sa_resp calloc failed");
		resetSystem();
        }

        //fill response header
        sa_resp->cmd = sa_cmd->cmd;
        sa_resp->arg_length = sizeof(IPCServerEnvResp);

//        getServerEnv.value = getCurrentServerEnv();
        memcpy((char *)sa_resp + IPC_CMD_HDR_LEN, &getServerEnv, sizeof(IPCServerEnvResp));

        //send it on socket
        IpcUdsSend((pIPCCommand)sa_resp, aClientFD);

        return SUCCESS;
}

int SendSignalStrength(IPCCommand *sa_cmd, int aClientFD)
{
        pIPCResp sa_resp = NULL;
        IPCSignalStrengthResp getSignalStrength;

        sa_resp = (pIPCResp)CALLOC((IPC_CMD_HDR_LEN + sizeof(IPCSignalStrengthResp)), 1);
        if(sa_resp == NULL)
        {
                APP_LOG("SendNetworkState:", LOG_DEBUG, "sa_resp calloc failed");
		resetSystem();
        }

        //fill response header
        sa_resp->cmd = sa_cmd->cmd;
        sa_resp->arg_length = sizeof(IPCSignalStrengthResp);

        getSignalStrength.value = getCurrentSignalStrength();
        memcpy((char *)sa_resp + IPC_CMD_HDR_LEN, &getSignalStrength, sizeof(IPCSignalStrengthResp));

        //send it on socket
        IpcUdsSend((pIPCCommand)sa_resp, aClientFD);

        return SUCCESS;
}

int SendWachDogStatus(IPCCommand *sa_cmd, int aClientFD)
{
        pIPCResp sa_resp = NULL;
        IPCWachDogStatusResp getWachDogStatus;

        sa_resp = (pIPCResp)CALLOC(1, IPC_CMD_HDR_LEN + sizeof(IPCWachDogStatusResp));
        if(sa_resp == NULL)
        {
                APP_LOG("SendWachDogStatus:", LOG_DEBUG, "sa_resp calloc failed");
		resetSystem();
        }

        //fill response header
        sa_resp->cmd = sa_cmd->cmd;
        sa_resp->arg_length = sizeof(IPCWachDogStatusResp);
	
	//set WatchDog Status
	gWatchDogStatus = 1;
        APP_LOG("SendWachDogStatus:", LOG_DEBUG, "Updated watchdog gWatchDogStatus = %d", gWatchDogStatus);
        getWachDogStatus.value = gWatchDogStatus;
        memcpy((char *)sa_resp + IPC_CMD_HDR_LEN, &getWachDogStatus, sizeof(IPCWachDogStatusResp));

        //send it on socket
        IpcUdsSend((pIPCCommand)sa_resp, aClientFD);

        return SUCCESS;
}
#endif /* #ifdef PRODUCT_WeMo_Baby */
