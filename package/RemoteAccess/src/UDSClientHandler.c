/***************************************************************************
*
*
* UDSClientHandler.c
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
#include "global.h"
#include <stdlib.h>
#include "defines.h"
#include "types.h"
#include "logger.h"
#include "remoteAccess.h"
#include "ipcUDS.h"
#include "natClient.h"
#include "ithread.h"

extern int gNTPTimeSet;
extern int gUDSClientDataFD;
extern int gIceRunning;
extern char g_szSerialNo[SIZE_64B];
extern char g_szWiFiMacAddress[SIZE_64B];
extern char g_szClientType[SIZE_128B];

void restartNatClient(void)
{
	System("killall natClient");
	APP_LOG("UDSClient:", LOG_DEBUG, "natClient Stopped");

	System("/sbin/natClient &>/dev/console &");
	sleep(2);
	APP_LOG("UDSClient:", LOG_DEBUG, "natClient Restarted");
}

void* UDS_MonitorAndStartClientDataPath(void *arg)
{
	APP_LOG("UDSClient:", LOG_DEBUG, "command:IPC_CMD_NAT_START_REMOTE_DATA_PATH");
	int retVal = -1; 
	pthread_t remoteDataThreadId = -1;
	pIPCCommand pcmd = NULL;
	pIPCCommand responseData = NULL;
	pIPCCommandIntData replyData = NULL;

	while(1)
	{
		pcmd = (IPCCommand*)calloc(1, sizeof(IPCCommand));
		if(pcmd == NULL)
		{
			APP_LOG("UDSClient:", LOG_ERR, "calloc failed");
			resetSystem();
		}
		pcmd->cmd = IPC_CMD_NAT_START_REMOTE_DATA_PATH;
		pcmd->arg_length = 0;

		responseData = ProcessClientIPCCommand(pcmd,SOCK_NAT_CLIENT_PATH);
		free(pcmd);pcmd = NULL;
		if(responseData)
		{
			replyData = (pIPCCommandIntData)(responseData->arg);
			if(replyData)
			{
				retVal = replyData->value;
				free(replyData);replyData = NULL;
			}
			free(responseData);responseData = NULL;
		}
		else
		{
			restartNatClient();
			APP_LOG("UDSClient:", LOG_DEBUG, "Nat Client restarted, continue with command:IPC_CMD_NAT_START_REMOTE_DATA_PATH");
			continue;
		}

		if(0 == retVal)
		{
			retVal = pthread_create(&remoteDataThreadId, NULL, UDS_ProcessRemoteData, NULL);
			if (0x00 != retVal)
			{
				APP_LOG("UDSClient:", LOG_DEBUG, "Could not create UDS remote data process thread");
				resetSystem();
			}
			else
			{
				APP_LOG("UDSClient:", LOG_DEBUG, "UDS remote data process thread created, Join Thread");
				pthread_join(remoteDataThreadId, NULL);

				APP_LOG("UDSClient:", LOG_DEBUG, "UDS remote data process thread terminated! Reinit NAT CLIENT and data path");
				restartNatClient();
				
#ifndef _OPENWRT_
				pjUpdateDstTz();
#endif
				UDS_SetNatData();
				UDS_SetRemoteEnableStatus();
				UDS_SetCurrentClientState();
				UDS_SetNTPTimeSyncStatus();
				UDS_remoteAccessInit();
				APP_LOG("UDSClient:", LOG_DEBUG, "Reinit NAT CLIENT done");
			}
		}
	}
	return NULL;
}

void* UDS_ProcessRemoteData(void *arg)
{
	int retVal = -1;
	IPCCommand remoteData;
	char *pktPtr = NULL;
	int pktLen = 0;
	char *transaction_id = NULL;
	char iceRunningStatusStr[5];

	while(1)
	{
		memset(&remoteData, 0, sizeof(IPCCommand));
		APP_LOG("UDSClient:", LOG_DEBUG, "Waiting for UDS remote data on:%d", gUDSClientDataFD);
		retVal = GetIPCResponse(&remoteData, gUDSClientDataFD);
		if(ERROR == retVal)
		{
			APP_LOG("UDSClient:", LOG_DEBUG, "Error in remote data path receive socket");
			break;
		}

		if(IPC_CMD_NAT_SET_ICE_RUNNING_STATUS == remoteData.cmd)
		{
			gIceRunning = atoi(remoteData.arg);
			APP_LOG("UDSClient:", LOG_DEBUG, "got gIceRunning status as:%d", gIceRunning);

			memset(iceRunningStatusStr, 0, sizeof(iceRunningStatusStr));
			snprintf(iceRunningStatusStr,sizeof(iceRunningStatusStr), "%d", gIceRunning);
			UDS_SendClientDataResponse(iceRunningStatusStr, sizeof(iceRunningStatusStr));
		}
		else if(IPC_CMD_NAT_REMOTE_DATA == remoteData.cmd)
		{
			APP_LOG("UDSClient:", LOG_DEBUG, "Process UDS remote data");
			if(gIceRunning)
			{
				APP_LOG("UDSClient:", LOG_DEBUG, "Remote data on ICE");
				transaction_id  = (char *)calloc(TRANSACTION_ID_LEN, sizeof(char));
				memcpy(transaction_id, remoteData.arg, TRANSACTION_ID_LEN-1);
				transaction_id[TRANSACTION_ID_LEN-1] = '\0';
				APP_LOG("REMOTEACCESS", LOG_DEBUG," Transaction id is:%s", transaction_id);

				/*Remove first 30 bytes of transaction ID*/
				pktPtr = (char*)(remoteData.arg) + TRANSACTION_ID_LEN-1;
				pktLen = (remoteData.arg_length) - TRANSACTION_ID_LEN-1;

				remoteAccessXmlHandler(NULL, pktPtr, pktLen, NULL, transaction_id, NULL);

				free(transaction_id);transaction_id = NULL;
				APP_LOG("UDSClient:", LOG_DEBUG, "Remote data on ICE processed");
			}
			else
			{
				APP_LOG("UDSClient:", LOG_DEBUG, "Remote data on RELAY");
				remoteAccessServiceHandler(NULL, remoteData.arg, remoteData.arg_length, NULL, 0, NULL);
				APP_LOG("UDSClient:", LOG_DEBUG, "Remote data on RELAY processed");
			}
		}
		else
		{
			APP_LOG("UDSClient:", LOG_DEBUG, "invalid command received");
		}
		
		free(remoteData.arg);
	}
	
	APP_LOG("UDSClient:", LOG_DEBUG, "close UDS remote data socket:%d", gUDSClientDataFD);
	close(gUDSClientDataFD);
	gUDSClientDataFD = ERROR;
	
	return NULL;
}

int UDS_SendClientDataResponse(char *UDSClientData, int dataLen)
{
	int retVal = -1;
	pIPCCommand remoteData = NULL;

	remoteData = (pIPCCommand)calloc(1, IPC_CMD_HDR_LEN + dataLen);
	if(NULL == remoteData)
	{
		APP_LOG("UDSClient:", LOG_DEBUG, "sending remote data failed");
		return retVal;
	}

	remoteData->cmd = IPC_CMD_NAT_REMOTE_DATA_REPLY;

	remoteData->arg_length = dataLen;
	memcpy((char *)remoteData + IPC_CMD_HDR_LEN, UDSClientData, remoteData->arg_length);

	retVal = SendIPCCommand(remoteData, gUDSClientDataFD);

	free(remoteData);remoteData=NULL;
	return retVal;
}

int UDS_SendRecieveCommandData(IPCCOMMAND cmd, void *data, int dataLen)
{
	int retVal = -1;
	pIPCCommand pCmd = NULL;
	pIPCCommand pResponse = NULL;
	pIPCCommandIntData pReplyData = NULL;

	if(data && dataLen)
	{
		/*allocate memory for header and argument*/
		pCmd = (pIPCCommand)calloc(1, IPC_CMD_HDR_LEN + dataLen);
		if(pCmd == NULL)
		{
			APP_LOG("UDSClient:", LOG_ERR, "calloc failed");
			resetSystem();
		}
		/*fill argument data length*/
		pCmd->arg_length = dataLen;
		/*fill argument data*/
		memcpy((char *)pCmd + IPC_CMD_HDR_LEN, data, dataLen);
	}
	else
	{
		/*allocate memory for header only*/
		pCmd = (IPCCommand*)calloc(1, sizeof(IPCCommand));
		/*fill argument data length*/
		pCmd->arg_length = 0;
	}

	/*fill command*/
	pCmd->cmd = cmd;

	/*process command*/
	pResponse = ProcessClientIPCCommand(pCmd,SOCK_NAT_CLIENT_PATH);
	if (pResponse)
	{
		/*process response*/
		pReplyData = (pIPCCommandIntData)(pResponse->arg);
		if (pReplyData)
		{
			retVal = pReplyData->value;
			free(pReplyData);pReplyData = NULL;
		}
		free(pResponse);pResponse = NULL;
	}
	free(pCmd);pCmd = NULL;

	return retVal;
}

void UDS_SetCurrentClientState()
{
	APP_LOG("UDSClient:", LOG_DEBUG, "command:IPC_CMD_NAT_SET_CLIENT_STATE");
	int stateSet = DEVICE_STATE_DISCONNECTED;
	int curState = getCurrentClientState();

	stateSet = UDS_SendRecieveCommandData(IPC_CMD_NAT_SET_CLIENT_STATE, &curState, sizeof(curState));

	APP_LOG("UDSClient:",LOG_DEBUG,"Client state set is =  %d", stateSet);
	return;
}

int UDS_triggerNat()
{
	APP_LOG("UDSClient:", LOG_DEBUG, "command:IPC_CMD_NAT_INVOKE_DESTROY");
        int cmdStatus = -1;

	cmdStatus = UDS_SendRecieveCommandData(IPC_CMD_NAT_INVOKE_DESTROY, NULL, 0);

	APP_LOG("UDSClient:",LOG_DEBUG,"command status =  %d", cmdStatus);
        return cmdStatus;
}

int UDS_remoteAccessInit()
{
	APP_LOG("UDSClient:", LOG_DEBUG, "command:IPC_CMD_NAT_REMOTE_ACCESS_INIT");
        int cmdStatus = -1;

	cmdStatus = UDS_SendRecieveCommandData(IPC_CMD_NAT_REMOTE_ACCESS_INIT, NULL, 0);

	APP_LOG("UDSClient:",LOG_DEBUG,"command status =  %d", cmdStatus);
        return cmdStatus;
}

int UDS_pluginNatInitialized()
{
	APP_LOG("UDSClient:", LOG_DEBUG, "command:IPC_CMD_NAT_INITIALIZED_STATE");
	int pluginNatInitialized = NATCL_NOT_INITIALIZED; 

	pluginNatInitialized = UDS_SendRecieveCommandData(IPC_CMD_NAT_INITIALIZED_STATE, NULL, 0);

	APP_LOG("UDSClient:",LOG_DEBUG,"Nat Initialized Status =  %d", pluginNatInitialized);
	return pluginNatInitialized;
}

int UDS_invokeNatDestroy()
{
	APP_LOG("UDSClient:", LOG_DEBUG, "command:IPC_CMD_NAT_INVOKE_DESTROY");
        int cmdStatus = -1;

	cmdStatus = UDS_SendRecieveCommandData(IPC_CMD_NAT_INVOKE_DESTROY, NULL, 0);

	APP_LOG("UDSClient:",LOG_DEBUG,"command status =  %d", cmdStatus);
        return cmdStatus;
}

int UDS_getRemoteDataBytes()
{
    APP_LOG("UDSClient:", LOG_DEBUG, "command:IPC_CMD_NAT_GET_REMOTE_DATA_BYTES");
        int bytes=0;
 
    bytes = UDS_SendRecieveCommandData(IPC_CMD_NAT_GET_REMOTE_DATA_BYTES, NULL, 0);
 
    APP_LOG("UDSClient:",LOG_DEBUG,"command status =  %d", bytes);
        return bytes;
}

void UDS_setRemoteDataBytes()
{
    APP_LOG("UDSClient:", LOG_DEBUG, "command:IPC_CMD_NAT_SET_REMOTE_DATA_BYTES");
        int cmdStatus = 0;

    cmdStatus = UDS_SendRecieveCommandData(IPC_CMD_NAT_SET_REMOTE_DATA_BYTES, NULL, 0);

    APP_LOG("UDSClient:",LOG_DEBUG,"command status =  %d", cmdStatus);
        return;
}

int UDS_invokeNatReInit(int initType)
{
	APP_LOG("UDSClient:", LOG_DEBUG, "command:IPC_CMD_NAT_REINIT");
        int NATReinitStatus = NATCL_HEALTH_NOTGOOD;

	NATReinitStatus = UDS_SendRecieveCommandData(IPC_CMD_NAT_REINIT, &initType, sizeof(initType));

        APP_LOG("UDSClient:",LOG_DEBUG,"Nat reinit status =  %d", NATReinitStatus);
        return NATReinitStatus;
}

int UDS_monitorNATCLStatus(void* natClientStatus)
{
	APP_LOG("UDSClient:", LOG_DEBUG, "command:IPC_CMD_MONITOR_NAT_CLIENT_STATUS");
        int NATClientStatus = NATCL_HEALTH_NOTGOOD;

	NATClientStatus = UDS_SendRecieveCommandData(IPC_CMD_MONITOR_NAT_CLIENT_STATUS, NULL, 0);

        APP_LOG("UDSClient:",LOG_DEBUG,"Nat Initialized Status =  %d", NATClientStatus);
        return NATClientStatus;
}

void UDS_SetRemoteEnableStatus()
{
	APP_LOG("UDSClient:", LOG_DEBUG, "command:IPC_CMD_NAT_SET_REMOTE_ENABLE_STATE");
	int stateSet = 0;

	stateSet = UDS_SendRecieveCommandData(IPC_CMD_NAT_SET_REMOTE_ENABLE_STATE, &gpluginRemAccessEnable, sizeof(gpluginRemAccessEnable));

	APP_LOG("UDSClient:",LOG_DEBUG,"remote enable state set is =  %d", stateSet);
	return;
}

void UDS_SetNatData()
{
	APP_LOG("UDSClient:", LOG_DEBUG, "command:IPC_CMD_NAT_SET_NAT_DATA");
	int stateSet = -1;
	IPCNATInitData cmdData;

	memset(&cmdData, 0, sizeof(IPCNATInitData));
	/*fill command data payload*/
	snprintf(cmdData.serialNo,sizeof(cmdData.serialNo), "%s", g_szSerialNo);
	snprintf(cmdData.WiFiMacAddress,sizeof(cmdData.WiFiMacAddress), "%s", g_szWiFiMacAddress);
	snprintf(cmdData.pluginPrivatekey,sizeof(cmdData.pluginPrivatekey), "%s", g_szPluginPrivatekey);
	snprintf(cmdData.restoreState,sizeof(cmdData.restoreState), "%s", g_szRestoreState);
	snprintf(cmdData.clientType,sizeof(cmdData.clientType), "%s", g_szClientType);

	APP_LOG("UDSServer:", LOG_DEBUG, "NAT Data send\nSerialNo:%s\nWiFiMacAddress:%s\nPluginPrivatekey:%s\nRestoreState:%s\nClientType:%s", g_szSerialNo,g_szWiFiMacAddress,g_szPluginPrivatekey,g_szRestoreState,g_szClientType);

	stateSet = UDS_SendRecieveCommandData(IPC_CMD_NAT_SET_NAT_DATA, &cmdData, sizeof(IPCNATInitData));

	APP_LOG("UDSClient:",LOG_DEBUG,"NAT data set status  is =  %d", stateSet);
	return;
}

#ifndef _OPENWRT_
void UDS_pj_dst_data_os(int index, char *ltimezone, int dstEnable)
{
	APP_LOG("UDSClient:", LOG_DEBUG, "command:IPC_CMD_NAT_DST_DATA");
	int stateSet = -1;
	IPCSetDstData cmdData;

	memset(&cmdData, 0, sizeof(IPCSetDstData));
	/*fill command data payload*/
	cmdData.idx = index;
	cmdData.dstEnable = dstEnable;
	snprintf(cmdData.timezone,sizeof(cmdData.timezone), "%s", ltimezone);

	stateSet = UDS_SendRecieveCommandData(IPC_CMD_NAT_DST_DATA, &cmdData, sizeof(IPCSetDstData));

	APP_LOG("UDSClient:",LOG_DEBUG,"DST set status is =  %d", stateSet);
	return;
}
#endif

void UDS_SetTurnServerEnvIPaddr(char *turnServerEnvIPaddr)
{
	APP_LOG("UDSClient:", LOG_DEBUG, "command:IPC_CMD_NAT_DST_DATA");
	int stateSet = -1;

	stateSet = UDS_SendRecieveCommandData(IPC_CMD_NAT_SET_TURN_SERVER_IP_ADDRESS, turnServerEnvIPaddr, strlen(turnServerEnvIPaddr)+1);

	APP_LOG("UDSClient:",LOG_DEBUG,"DST set status is =  %d", stateSet);
	return;
}

void UDS_SetNTPTimeSyncStatus()
{
	APP_LOG("UDSClient:", LOG_DEBUG, "command:IPC_CMD_NAT_SET_NTP_TIME_SYNC");
	int stateSet = 0;

	stateSet = UDS_SendRecieveCommandData(IPC_CMD_NAT_SET_NTP_TIME_SYNC, &gNTPTimeSet, sizeof(gNTPTimeSet));

	APP_LOG("UDSClient:",LOG_DEBUG,"NTP time sync set status = %d", stateSet);
	return;
}
