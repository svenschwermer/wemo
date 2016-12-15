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
/*#include "logger.h"*/
#include "zbIPC.h"

static pthread_attr_t IpcUdsServerth_attr;
static pthread_t IpcUdsServer_thread;
int gWatchDogStatus = -1;

int IpcUdsSend(pTIPCMsg sa_resp, int aChildFD)
{
	if(sa_resp == NULL)
	{
		DBGPRINT("IpcUdsSend:response structure is empty\n");
		return ERROR;
	}

	if (send(aChildFD, (TIPCMsg*)sa_resp, (IPC_CMD_HDR_LEN + sa_resp->hdr.arg_length), 0) < 0) 
	{
		DBGPRINT("IpcUdsSend:Socket Sent error\n");
	}

	if(sa_resp!=NULL)
	{
		free(sa_resp);
		sa_resp=NULL;
	}

	return SUCCESS;
}

int StartIpcUdsServer()
{
	int retVal = 0;

	pthread_attr_init(&IpcUdsServerth_attr);
	pthread_attr_setdetachstate(&IpcUdsServerth_attr,PTHREAD_CREATE_DETACHED);
	retVal = pthread_create(&IpcUdsServer_thread,&IpcUdsServerth_attr,(void*)&CreateIpcUdsServer, NULL);
	if(retVal < 0)
	{
		DBGPRINT("StartUdServer:Could not create unix domain server thread... \n");
	}
	else
	{
		DBGPRINT("StartUdServer:created IPC unix domain server thread... \n");
	}
	return retVal;
}

int CreateIpcUdsServer(void *arg)
{
	int server_sock, *pClientFD, t_len, len;
 	struct sockaddr_un local, remote;
        int retVal = 0;
	pthread_attr_t IpcUdsChildth_attr;
	pthread_t IpcUdsChild_thread;

	DBGPRINT("\nStartingThread CreateIpcUdsServer\n");

	if ((server_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == ERROR)
	{
		DBGPRINT("CreateIpcUdsServer:Server Socket Creation Failed.\n");
		return ERROR;
	}

	local.sun_family = AF_UNIX;
	strncpy(local.sun_path, SOCK_ZB_PATH, sizeof(local.sun_path)-1);
	unlink(local.sun_path);
	len = strlen(local.sun_path) + sizeof(local.sun_family);

	if (bind(server_sock, (struct sockaddr *)&local, len) == ERROR) 
	{
		DBGPRINT("CreateIpcUdsServer:bind failed\n");
		return ERROR;
	}

	if (listen(server_sock, 5) == ERROR)
	{
		DBGPRINT("CreateIpcUdsServer:Listen Failed\n");
		return ERROR;
	}

	t_len = sizeof(remote);
	for(;;)
	{
		pClientFD = (int*)malloc(sizeof(int));
		if(pClientFD == NULL )
		{
			DBGPRINT("CreateIpcUdsServer:FD buffer calloc failed\n");
			continue;
		}	
		*pClientFD = -1;
		if ((*pClientFD = accept(server_sock, (struct sockaddr *)&remote, &t_len)) == -1)
		{
			DBGPRINT("CreateIpcUdsServer:Accept Failed.\n");
			if (NULL != pClientFD){free(pClientFD);pClientFD = NULL;}
			continue;
		}
		else
		{
			pthread_attr_init(&IpcUdsChildth_attr);
			pthread_attr_setdetachstate(&IpcUdsChildth_attr, PTHREAD_CREATE_DETACHED);
			retVal = pthread_create(&IpcUdsChild_thread, &IpcUdsChildth_attr, &ChildFDTask, (void*)pClientFD);
			if(retVal < 0)
			{
				DBGPRINT("CreateIpcUdsServer:Could not create unix domain child server thread... \n");
				if (NULL != pClientFD){free(pClientFD);pClientFD = NULL;}
			}
			else
			{
			}
		}

	}

	DBGPRINT("LeavingThread CreateIpcUdsServer\n");

	return SUCCESS;
}

void ChildFDTask(void *pClientFD)
{
	char *hdr_buffer = NULL;
	char *payload_buffer = NULL;
	TIPCMsg buffer;
	int nbytes;
	pIPCHeader hdr;
	int ClientFD = -1;

	if (NULL == (int*)pClientFD)
	{
		DBGPRINT("ChildFDTask:NULL FD Argumnet, exit thread!\n");
		return;
	}
	ClientFD = *(int *)pClientFD;
	free((int*)pClientFD);
	pClientFD = NULL;

	DBGPRINT("\nStartingThread ChildFDTask:%d\n", ClientFD);

	do
	{
		hdr_buffer= (char*)calloc(IPC_CMD_HDR_LEN, 1);
		if(hdr_buffer == NULL )
		{
			DBGPRINT("ChildFDTask:header buffer calloc failed\n");
			break;
		}
		nbytes = recv(ClientFD, hdr_buffer, IPC_CMD_HDR_LEN, 0x0);
		if(nbytes < 0)
		{
			DBGPRINT("ChildFDTask:header read error\n");
			break;
		}

		hdr = NULL;
		hdr = (IPCHeader*)hdr_buffer;

		memcpy(&(buffer.hdr),hdr_buffer,IPC_CMD_HDR_LEN);

		payload_buffer = NULL;
		if(hdr->arg_length > 0x0)
		{
			nbytes = 0;
			payload_buffer = (char*)calloc(hdr->arg_length, 1);

			if(payload_buffer == NULL )
			{
				DBGPRINT("ChildFDTask:payload buffer calloc failed\n");
				break;		
			}
			nbytes = recv(ClientFD, payload_buffer, hdr->arg_length, 0x0);
			if(nbytes < 0)
			{
				DBGPRINT("ChildFDTask:payload read error\n");
				break;
			}

			buffer.arg = (void *)payload_buffer;
			ProcessServerIpcCmd(&buffer, ClientFD);	
		}
		else
		{
			ProcessServerIpcCmd(&buffer, ClientFD);	
		}

	}
	while(0);

	DBGPRINT("LeavingThread ChildFDTask:%d\n", ClientFD);

	if(hdr_buffer!=NULL){free(hdr_buffer); hdr_buffer=NULL;}
	if(payload_buffer!=NULL){free(payload_buffer); payload_buffer=NULL;}
	close(ClientFD);
        pthread_exit(0);

	return;
}

int SendNetworkState(TIPCMsg *sa_cmd, int aClientFD)
{
	pTIPCMsg sa_resp = NULL;
	IPCNetworkStateResp getNetworkState;

	sa_resp = (pTIPCMsg)calloc((IPC_CMD_HDR_LEN + sizeof(IPCNetworkStateResp)), 1);
	if(sa_resp == NULL)
	{
		DBGPRINT("SendNetworkState:sa_resp calloc failed.\n");
		return ERROR;
	}

	//fill response header
	sa_resp->hdr.cmd = sa_cmd->hdr.cmd;
	sa_resp->hdr.arg_length = sizeof(IPCNetworkStateResp);

	//fill response payload
	getNetworkState.value = 777;
	memcpy((char *)sa_resp + IPC_CMD_HDR_LEN, &getNetworkState, sizeof(IPCNetworkStateResp));

	//send it on socket
	IpcUdsSend((pTIPCMsg)sa_resp, aClientFD);

	return SUCCESS; 
}

int ProcessServerIpcCmd(TIPCMsg* s_cmd, int aClientFD)
{

	if (NULL == s_cmd)
		return FAILURE;

	switch(s_cmd->hdr.cmd)
	{
		case IPC_CMD_GET_NETWORK_STATE:
			DBGPRINT("IPC_CMD_GET_NETWORK_STATE:Unknown command: %d\n", s_cmd->hdr.cmd);
			SendNetworkState(s_cmd, aClientFD);
			break;

		default:
			DBGPRINT("ProcessServerIpcCmd:Unknown command: %d\n", s_cmd->hdr.cmd);
			break;
	}

	return SUCCESS;
}

