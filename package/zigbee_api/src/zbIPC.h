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
#ifndef __ZIGBEE_IPC__
#define __ZIGBEE_IPC__

#include "types.h"
#include "global.h"

#include "zigbee_api.h"

#define DBG
#ifdef DBG
//#define DBGPRINT(fmt, args...)      printf(fmt, ## args)
#define DBGPRINT(fmt, args...)      fprintf(stderr, fmt, ## args)
#else
#define DBGPRINT(fmt, args...)
#endif

#define SUCCESS      0
#define ERROR        -1

#define SOCK_ZB_PATH "/tmp/ipczigbeed1.sock"

/** Enumerated type of end node command type*/
typedef enum {
	IPC_CMD_INVALID = 0,
	IPC_CMD_SET_TESTMODE,
	IPC_CMD_GET_NETWORK_STATE = 100,
	IPC_CMD_SET_NET_FORM,
	IPC_CMD_SET_NET_LEAVE,
	IPC_CMD_GET_NET_INFO,
	IPC_CMD_SET_NET_CHANNEL,
	IPC_CMD_SET_PERMITJOIN,
	IPC_CMD_SET_LEAVE_REQUEST,
	IPC_CMD_GET_DEVTYPE,
	IPC_CMD_GET_ENDPOINT,
	IPC_CMD_GET_CLUSTERS,
	IPC_CMD_SET_ONOFF,
	IPC_CMD_GET_ONOFF,
	IPC_CMD_SET_LEVEL,
	IPC_CMD_GET_LEVEL,
	IPC_CMD_SET_COLOR,
	IPC_CMD_GET_MODELCODE,
	IPC_CMD_GET_APPVERSION,
	IPC_CMD_GET_ZCLVERSION,
	IPC_CMD_GET_STACKVERSION,
	IPC_CMD_GET_HWVERSION,
	IPC_CMD_GET_MFGNAME,
	IPC_CMD_GET_DATE,
	IPC_CMD_GET_POWERSOURCE,
	IPC_CMD_GET_NODEID,
	IPC_CMD_SET_MTORR,
	IPC_CMD_SET_IDENTIFY,
	IPC_CMD_SET_COLOR_TEMP,
	IPC_CMD_GET_COLOR_TEMP,
	IPC_CMD_SET_GROUP,
	IPC_CMD_SET_LEVEL_MOVE,
	IPC_CMD_SET_LEVEL_STOP,
	IPC_CMD_SET_OTA_NOTIFY,
	IPC_CMD_SET_NET_SCANJOIN,
	IPC_CMD_GET_OTA_VERSION,
	IPC_CMD_GET_IAS_ZONE_STATUS,
	IPC_CMD_GET_COLOR,
}TIPC_CMD;

typedef struct IPC_Header {
	TIPC_CMD cmd;					/* command name*/
	UINT32 arg_length;				/* length of payload*/
	TZBID zbnode;
	int param1;
	int param2;
	int param3;
	int param4;
} IPCHeader, *pIPCHeader;

// GENERIC DATA STRUCTURE
#define IPC_CMD_HDR_LEN     sizeof(IPCHeader)

/** Structure representing IPC action command request */
typedef struct IPC_command {
	IPCHeader hdr;
	PVOID arg;					/* Pointer to the actual data of command */
}TIPCMsg, *pTIPCMsg;

#define IPC_MSG_LEN     sizeof(TIPCMsg)

/** Structure representing IPC response for Network state */
typedef struct Network_State_response {
	INT32 value;
} IPCNetworkStateResp, *pIPCNetworkStateResp;

/** Structure representing IPC response for General use */
typedef struct General_response {
	INT32 value;
} IPCGeneralResp, *pIPCGeneralResp;

typedef struct Array_response {
	INT32 device_type;
	INT32 n_member;
	INT32 byte_per_member;
	char data[0];
} IPCArrayResp, *pIPCArrayResp;

/*IPC UDS Server API's*/
int ProcessServerIpcCmd(TIPCMsg* s_cmd, int aChildFD);
int CreateIpcUdsServer(void *arg);
void ChildFDTask(void *pClientFD);
int StartIpcUdsServer(void);
int IpcUdsSend(pTIPCMsg sa_resp, int aChildFD);

/*IPC UDS Client API's*/
pTIPCMsg clientProcessCMD(TIPC_CMD cmd);
pTIPCMsg clientProcessIPCMsg(TIPCMsg *pIPCMsg);
int clientSendIPCMsg(pTIPCMsg psCmd, int aClientFD);
int clientGetIPCMsgResponse(pTIPCMsg buffer, int aClientFD);

int clientGetNetworkState(void);
int clientNetworkPermitJoin(TZBID *pzbID, int timeSec);
int clientSetOnOff(TZBID *pzbID, int argOnOff);
int clientGeneralCommand(TZBID *pzbID, TZBParam *pzbParam, TIPC_CMD cmd, int *prespValue);
int clientGetStringCommand(TZBID *pzbID, TZBParam *pzbParam, TIPC_CMD cmd);
int clientGetArrayCommand(TZBID *pzbID, TZBParam *pzbParam, TIPC_CMD cmd);

#endif /*__ZIGBEE_IPC__*/



