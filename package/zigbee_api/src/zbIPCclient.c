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
#include <sys/un.h>
#include <semaphore.h>
#include "types.h"
#include "global.h"
/*#include "logger.h"*/

#include "zbIPC.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

static void SetVarIPCMsg(TIPCMsg *paIPCMsg, TZBID *pzbID, TZBParam *pzbParam, TIPC_CMD paramCmd)
{
	if (paIPCMsg)
	{
		memset(paIPCMsg, 0, sizeof(TIPCMsg));

		zbCopyZBID(&(paIPCMsg->hdr.zbnode), pzbID);
		zbDebugPrintZBID(&(paIPCMsg->hdr.zbnode));

		paIPCMsg->hdr.cmd = paramCmd;
		paIPCMsg->hdr.param1 = pzbParam->param1;
		paIPCMsg->hdr.param2 = pzbParam->param2;
		paIPCMsg->hdr.param3 = pzbParam->param3;
		paIPCMsg->hdr.param4 = pzbParam->param4;
	}
}

static int StartIPCClient()
{
	int clientSock, len;
	struct sockaddr_un remote;

	if ((clientSock = socket(AF_UNIX, SOCK_STREAM, 0)) == ERROR)
	{
		DBGPRINT("StartIPCClient:client socket creation error!\n");
		return ERROR;
	}

	remote.sun_family = AF_UNIX;
	strncpy(remote.sun_path, SOCK_ZB_PATH, sizeof(remote.sun_path)-1);
	len = strlen(remote.sun_path) + sizeof(remote.sun_family);
	if (connect(clientSock, (struct sockaddr *)&remote, len) == ERROR)
	{
		DBGPRINT("StartIPCClient:Connected to IPC server failed\n");
		close(clientSock);
		return ERROR;
	}

	return clientSock;
}

int clientGetNetworkState()
{
	int NetworkState = 0;
	pTIPCMsg response = NULL;
	pIPCNetworkStateResp status = NULL;

	response = clientProcessCMD(IPC_CMD_GET_NETWORK_STATE);
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

	DBGPRINT("clientGetNetworkState:*****NetworkState = %d\n", NetworkState);
	return NetworkState;
}

int clientNetworkPermitJoin(TZBID *pzbID, int timeSec)
{
	int             result     = -1;
	pTIPCMsg        p_resp_msg = NULL;
	pIPCGeneralResp status     = NULL;
	TIPCMsg         ipc_msg;

	memset(&ipc_msg, 0, sizeof(ipc_msg));
	ipc_msg.hdr.cmd = IPC_CMD_SET_PERMITJOIN;
	ipc_msg.hdr.param1 = timeSec;

	p_resp_msg = clientProcessIPCMsg(&ipc_msg);
	if (p_resp_msg)
	{
		result = p_resp_msg->hdr.param1;

		if (result == ZB_RET_SUCCESS)
		{
			status = (pIPCGeneralResp)(p_resp_msg->arg);
			if (status)
			{
				/*result = status->value;*/
				free(status);status = NULL;
			}
		}

		free(p_resp_msg);p_resp_msg = NULL;
	}

	DBGPRINT("clientNetworkPermitJoin:*****result = %d\n", result);
	return result;
}

int clientSetOnOff(TZBID *pzbID, int argOnOff)
{
	int             result     = -1;
	pTIPCMsg        p_resp_msg = NULL;
	pIPCGeneralResp status     = NULL;
	TIPCMsg         ipc_msg;

	DBGPRINT("Entering clientSetOnOff:\n");
	memset(&ipc_msg, 0, sizeof(ipc_msg));

	zbCopyZBID(&ipc_msg.hdr.zbnode, pzbID);
	zbDebugPrintZBID(&ipc_msg.hdr.zbnode);

	ipc_msg.hdr.cmd = IPC_CMD_SET_ONOFF;
	ipc_msg.hdr.param1 = argOnOff;

	p_resp_msg = clientProcessIPCMsg(&ipc_msg);
	if (p_resp_msg)
	{
		result = p_resp_msg->hdr.param1;

		if (result == ZB_RET_SUCCESS)
		{
			status = (pIPCGeneralResp)(p_resp_msg->arg);
			if (status)
			{
				/*result = status->value;*/
				free(status);status = NULL;
			}
		}

		free(p_resp_msg);p_resp_msg = NULL;
	}

	DBGPRINT("Leaving clientSetOnOff:*****result = %d\n", result);
	return result;
}

int clientGeneralCommand(TZBID *pzbID, TZBParam *pzbParam, TIPC_CMD cmd, int *prespValue)
{
	int             result       = -1;
	pTIPCMsg        p_resp_msg   = NULL;
	pIPCGeneralResp general_resp = NULL;
	TIPCMsg         ipc_msg;

	DBGPRINT("Entering clientGeneralCommand:\n");

	SetVarIPCMsg(&ipc_msg, pzbID, pzbParam, cmd);

	p_resp_msg = clientProcessIPCMsg(&ipc_msg);
	if (p_resp_msg)
	{
		result = p_resp_msg->hdr.param1;
		pzbParam->ret = result;

		if (result == ZB_RET_SUCCESS)
		{
			general_resp = (pIPCGeneralResp)(p_resp_msg->arg);
			if (general_resp)
			{
				if (prespValue)
				{
					*prespValue = general_resp->value;
					DBGPRINT("clientGeneralCommand: *prespValue = 0x%X\n", *prespValue);
				}

				free(general_resp);general_resp = NULL;
			}
		}

		free(p_resp_msg);p_resp_msg = NULL;
	}

	DBGPRINT("Leaving clientGeneralCommand:*****result = %d\n", result);
	return result;
}

int clientGetStringCommand(TZBID *pzbID, TZBParam *pzbParam, TIPC_CMD cmd)
{
	int       result   = -1;
	pTIPCMsg  p_resp_msg = NULL;
	TIPCMsg   ipc_msg;

	DBGPRINT("Entering clientGetStringCommand:\n");

	SetVarIPCMsg(&ipc_msg, pzbID, pzbParam, cmd);

	p_resp_msg = clientProcessIPCMsg(&ipc_msg);
	if (p_resp_msg)
	{
		result = p_resp_msg->hdr.param1;
		if (result == ZB_RET_SUCCESS)
		{
			char *str_ret = NULL;
			int   str_len = p_resp_msg->hdr.arg_length;
			int   count;

			pzbParam->param1 = str_len;

			count = MIN(str_len, ZB_NO_MSG_BYTES);

			str_ret = (char *)(p_resp_msg->arg);
			if (str_ret && str_len > 0)
			{
				strncpy(pzbParam->str, str_ret, count);
				pzbParam->str[count] = '\0';
			}

			DBGPRINT("Success str_len:%d, str_ret:[%s]\n", str_len, str_ret);

			if (str_ret)
			{
				free(str_ret);str_ret = NULL;
			}
		}

		free(p_resp_msg);p_resp_msg = NULL;
	}

	pzbParam->ret = result;

	DBGPRINT("Leaving clientGetStringCommand:*****result = %d\n", result);
	return result;
}

static int IntArrayCopy(int *intarrDest, char *arrSrc, int numSrc, int byteperMember)
{
	int i;

	if (intarrDest == NULL || arrSrc == NULL)
	{
		return ERROR;
	}

	switch (byteperMember)
	{
		case 1:
			{
				for (i = 0; i < numSrc; ++i)
				{
					intarrDest[i] = arrSrc[i];
				}
			}
			break;

		case 2:
			{
				unsigned short * ptr_word = (unsigned short *)arrSrc;
				for (i = 0; i < numSrc; ++i)
				{
					intarrDest[i] = ptr_word[i];
				}
			}
			break;

		case 4:
			{
				int * ptr_int = (int *)arrSrc;
				for (i = 0; i < numSrc; ++i)
				{
					intarrDest[i] = ptr_int[i];
				}
			}
			break;

		default:
			return ERROR;
	}

	return SUCCESS;
}

int clientGetArrayCommand(TZBID *pzbID, TZBParam *pzbParam, TIPC_CMD cmd)
{
	int       result     = ZB_RET_FAIL;
	pTIPCMsg  p_resp_msg = NULL;
	TIPCMsg   ipc_msg;
	int      *arr_dest   = pzbParam->param1;
	int       n_dest     = pzbParam->param2;

	DBGPRINT("Entering clientGetArrayCommand:\n");

	if (arr_dest == NULL || n_dest <= 0)
	{
		return result;
	}

	SetVarIPCMsg(&ipc_msg, pzbID, pzbParam, cmd);

	p_resp_msg = clientProcessIPCMsg(&ipc_msg);
	if (p_resp_msg)
	{
		result = p_resp_msg->hdr.param1;
		if (result == ZB_RET_SUCCESS)
		{
			pIPCArrayResp p_arr_resp      = (pIPCArrayResp) p_resp_msg->arg;
			int           byte_per_member = p_arr_resp->byte_per_member;
			int           n_src           = p_arr_resp->n_member;
			int           i, count;

			pzbParam->device_type = p_arr_resp->device_type;
			pzbParam->param1 = n_src;

			count = MIN(n_dest, n_src);

			IntArrayCopy(arr_dest, p_arr_resp->data, count, byte_per_member);

			free(p_arr_resp->data);
		}

		free(p_resp_msg);p_resp_msg = NULL;
	}

	pzbParam->ret = result;

	DBGPRINT("Leaving clientGetArrayCommand:*****result = %d\n", result);
	return result;
}

pTIPCMsg clientProcessIPCMsg(TIPCMsg *pIPCMsg)
{
	pTIPCMsg pgetResp = NULL;
	pTIPCMsg pcmd = pIPCMsg;
	int retVal = ERROR,  clientFd = ERROR;

	if(pcmd == NULL)
	{
		DBGPRINT("clientProcessIPCMsg: pcmd NULL\n");
		return NULL;;
	}

	do
	{
		pgetResp = (TIPCMsg*)calloc(sizeof(TIPCMsg), 1);
		if(pgetResp == NULL)
		{
			DBGPRINT("clientProcessIPCMsg: calloc failed\n");
			return NULL;
		}

		retVal = clientFd = StartIPCClient();
		if(ERROR == retVal)
			break;

		if(retVal == clientSendIPCMsg(pcmd, clientFd))
			break;

		retVal = clientGetIPCMsgResponse(pgetResp, clientFd);
	}while(0);
	if (ERROR == retVal)
	{
		free(pgetResp->arg);pgetResp->arg = NULL;
		free(pgetResp);pgetResp = NULL;
	}
	/*close UDS client socket*/
	close(clientFd);
	clientFd = ERROR;
	return pgetResp;
}

pTIPCMsg clientProcessCMD(TIPC_CMD cmd)
{
	pTIPCMsg pgetResp = NULL;
	pTIPCMsg pcmd = NULL;
	int retVal = ERROR,  clientFd = ERROR;
	do
	{
		pcmd = (TIPCMsg*)calloc(sizeof(TIPCMsg), 1);
		pgetResp = (TIPCMsg*)calloc(sizeof(TIPCMsg), 1);
		if(pcmd == NULL || pgetResp == NULL)
		{
			DBGPRINT("clientProcessCMD: calloc failed\n");
			if(pcmd) free (pgetResp);
			if(pgetResp) free(pcmd);
			return NULL;;
		}
		pcmd->hdr.cmd = cmd;
		pcmd->hdr.arg_length = 0;

		retVal = clientFd = StartIPCClient();
		if(ERROR == retVal)
			break;

		if(retVal == clientSendIPCMsg(pcmd, clientFd))
			break;

		free(pcmd);pcmd = NULL;
		retVal = clientGetIPCMsgResponse(pgetResp, clientFd);
	}while(0);
	if (ERROR == retVal)
	{
		free(pcmd);pcmd = NULL;
		free(pgetResp->arg);pgetResp->arg = NULL;
		free(pgetResp);pgetResp = NULL;
	}
	/*close UDS client socket*/
	close(clientFd);
	clientFd = ERROR;
	return pgetResp;
}

int clientSendIPCMsg(pTIPCMsg psCmd, int aClientFd)
{
	char *buf = NULL;
	int len;

	/* Send the message on client fd */
	if(!(psCmd->hdr.arg_length))
	{
		len = IPC_CMD_HDR_LEN;
	}
	else
	{
		len = IPC_CMD_HDR_LEN + psCmd->hdr.arg_length;
	}

	buf = calloc(len,1);
	if (NULL == buf)
	{
		return ERROR;
	}
	memcpy(buf, &(psCmd->hdr),IPC_CMD_HDR_LEN);
	if(psCmd->hdr.arg_length)
	{
		memcpy(buf + IPC_CMD_HDR_LEN, (char *)psCmd->arg, len - IPC_CMD_HDR_LEN);
	}

	/* buffer now ready to send */
	if (send(aClientFd, buf, len, 0) == ERROR)
	{
		DBGPRINT("clientSendIPCMsg: Data send Fail.\n");
		free(buf);
		return ERROR;
	}
	free(buf);
	return SUCCESS;
}


int clientGetIPCMsgResponse(pTIPCMsg pRespbuff, int aClientFd)
{
	char *payload_buffer = NULL;
	int nbytes;

	nbytes = recv(aClientFd, (char*)&(pRespbuff->hdr), IPC_CMD_HDR_LEN, 0x0);
	if(nbytes < 0)
	{
		DBGPRINT("clientGetIPCMsgResponse:header read error\n");
		return ERROR;
	}

	payload_buffer = NULL;
	if(pRespbuff->hdr.arg_length > 0x0)
	{
		nbytes = 0;
		payload_buffer = (char*)calloc(pRespbuff->hdr.arg_length, 1);

		if(payload_buffer == NULL )
		{
			DBGPRINT("clientGetIPCMsgResponse: payload buffer calloc failed\n");
			return ERROR;
		}
		nbytes = recv(aClientFd, payload_buffer, pRespbuff->hdr.arg_length, 0x0);
		if(nbytes < 0)
		{
			DBGPRINT("clientGetIPCMsgResponse:payload read error\n");
			free(payload_buffer);
			payload_buffer = NULL;
			return ERROR;
		}
	}
	else
	{
	}
	pRespbuff->arg = (void *)payload_buffer;
	return SUCCESS;
}

