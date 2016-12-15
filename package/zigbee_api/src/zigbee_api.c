/***************************************************************************
*
* Created by Belkin International, Software Engineering on Jun 14, 2011
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

#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#include "types.h"
#include "global.h"
/*#include "logger.h"*/

#include "zbIPC.h"
#include "zigbee_api.h"

static int g_Last_OnOff=0;

void zbDebugPrintZBID(TZBID *pzbID)
{
	int i;
	DBGPRINT("node_id:0x%04X \n", pzbID->node_id);
	DBGPRINT("node_eui:0x");
	for(i=0; i<=7; i++)
	{
		DBGPRINT("%02X", pzbID->node_eui[7-i]);
	}

	DBGPRINT("\n");
}

void zbCopyZBID(TZBID *pdestpzbID, TZBID *psrczbID)
{
	memcpy(pdestpzbID, psrczbID, sizeof(TZBID));
}

int zbInit()
{
	return ZB_RET_SUCCESS;
}

static int zbTestFunc(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret = ZB_RET_SUCCESS;
	int i;

#if 0
	ret = clientGetNetworkState();
	DBGPRINT("clientGetNetworkState:*****NetworkState = %d\n", ret );
#endif

	int time = pzbParam->param1;

	for (i = 0; i < time; ++i)
	{
		DBGPRINT("Waiting...:%d", time - i);
		sleep(1);
	}

	pzbParam->command     = ZB_RET_CMD;
	pzbParam->ret         = ret;

	return ret;
}

static int zbSetPermitJoin(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret = ZB_RET_SUCCESS;
	int time = pzbParam->param1;
	DBGPRINT("zbSetPermitJoin:%d\n", time);

	if (time >= 0xFF)
	{
		// Do not open forever, 0xFF
		time = 254;
	}

	if (time < 0)
	{
		time = 0;
	}

	ret = clientNetworkPermitJoin(pzbID, time);

	pzbParam->ret         = ret;

	return ret;
}

static int zbSetPermitJoinClose(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret = ZB_RET_SUCCESS;
	DBGPRINT("zbSetPermitJoinClose:\n");

	ret = clientNetworkPermitJoin(pzbID, 0);

	pzbParam->ret         = ret;

	return ret;
}

static int zbSetOnOff(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret   = ZB_RET_SUCCESS;
	int onoff = pzbParam->param1;

	DBGPRINT("zbSetOnOff:%d\n", onoff);
	g_Last_OnOff = onoff;

	ret = clientSetOnOff(pzbID, onoff);

	pzbParam->ret         = ret;

	return ret;
}

static int zbGetOnOff(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret   = ZB_RET_SUCCESS;

	pzbParam->param1      = g_Last_OnOff;
	DBGPRINT("zbGetOnOff:%d\n", pzbParam->param1);
	pzbParam->ret         = ret;

	return ret;
}

static int zbSetOTANotify(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret = ZB_RET_SUCCESS;
	int payload_type = pzbParam->param1;

	DBGPRINT("zbSetOTANotify payloadType:%d\n", payload_type);

	ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_OTA_NOTIFY, NULL);

	pzbParam->ret         = ret;

	return ret;
}

static int zbSetGroup(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret = ZB_RET_SUCCESS;
	int group_add = pzbParam->param1;
	int group_id = pzbParam->param2;
	DBGPRINT("zbSetGroup add:%d id:0x%02X\n", group_add, group_id);

	ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_GROUP, NULL);

	pzbParam->ret         = ret;

	return ret;
}

static int zbSetLevel(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret = ZB_RET_SUCCESS;
	int level = pzbParam->param1;
	int time = pzbParam->param2;
	DBGPRINT("zbSetLevel level:%d time:%d\n", level, time);

	ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_LEVEL, NULL);

	pzbParam->ret         = ret;

	return ret;
}

static int zbGetLevel(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret   = ZB_RET_SUCCESS;

	DBGPRINT("zbGetLevel:%d\n", pzbParam->param1);
	pzbParam->ret         = ret;

	return ret;
}

static int zbGetActiveEndpoint(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret = ZB_RET_SUCCESS;
	int *pInt = (int *) pzbParam->param1;
	DBGPRINT("zbGetActiveEndpoint\n");

	ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_GET_ENDPOINT, NULL);

	if (pInt) pInt[0] = 1;
	pzbParam->param1      = 1;
	pzbParam->ret         = ret;

	return ret;
}

static int TEST_CLUSTERS[] =
{
	ZCL_BASIC_CLUSTER_ID,
	ZCL_IDENTIFY_CLUSTER_ID,
	ZCL_ON_OFF_CLUSTER_ID,
	ZCL_LEVEL_CONTROL_CLUSTER_ID,
	ZCL_SCENES_CLUSTER_ID,
	ZCL_GROUPS_CLUSTER_ID,
};

static int zbGetClusters(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret = ZB_RET_SUCCESS;
	DBGPRINT("zbGetClusters\n");
	int *pInt = (int *) pzbParam->param1;
	int n_size = pzbParam->param2;
	int i;
	int n_clusters = sizeof(TEST_CLUSTERS)/sizeof(int);
	DBGPRINT("zbGetClusters pInt:0x%x param1:0x%x n_size:%d\n", pInt, pzbParam->param1, n_size);

	ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_GET_CLUSTERS, NULL);

	if (!pInt)
	{
		return ZB_RET_FAIL;
	}

	for (i = 0; i < n_size; i++) {
		if (i == n_clusters) break;
		if (pInt) pInt[i] = TEST_CLUSTERS[i];
		/*DBGPRINT("zbGetClusters pInt[i]:0x%x i:%d\n", pInt[i], i);*/
	}

	pzbParam->param1      = n_clusters;
	pzbParam->ret         = ret;

	return ret;
}

static int zbGetColor(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret = ZB_RET_SUCCESS;
	int response;
	/*int resp_data[2];*/
	/*pzbParam->param1 = (int) resp_data;*/
	/*pzbParam->param2 = 2;*/
	/*ret = clientGetArrayCommand(pzbID, pzbParam, IPC_CMD_GET_COLOR);*/
	/*pzbParam->param1 = resp_data[0];*/
	/*pzbParam->param2 = resp_data[1];*/

	ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_GET_COLOR, &(pzbParam->param1));
	response = pzbParam->param1;
	pzbParam->param1 = response & 0xffff;
	pzbParam->param2 = (response >> 16) & 0xffff;

	DBGPRINT("zbGetColor x:0x%x, y:0x%x \n", pzbParam->param1, pzbParam->param2);
	return ret;
}

static int zbSetFormNetwork(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret = ZB_RET_SUCCESS;
	ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_NET_FORM, NULL);
	return ret;
}

static int zbSetNetworkChangeChannel(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret = ZB_RET_SUCCESS;
	int channel = pzbParam->param1;

	DBGPRINT("zbSetNetworkChangeChannel channel:%d \n", channel);

	ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_NET_CHANNEL, NULL);

	pzbParam->ret         = ret;

	return ret;
}

static int zbGetNetworkInfo(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret = ZB_RET_SUCCESS;
	int resp_data[2];

	pzbParam->param1 = (int) resp_data;
	pzbParam->param2 = 2;

	ret = clientGetArrayCommand(pzbID, pzbParam, IPC_CMD_GET_NET_INFO);

	pzbParam->param1 = resp_data[0];
	pzbParam->param2 = resp_data[1];

	return ret;
}

static void zbSyncSleep(int isAsync)
{
	if (isAsync == 0)
	{
		DBGPRINT("zbSyncSleep\n");
		usleep(200000);
	}
}

int zbSendCommand(TZBID *pzbID, TZBParam *pzbParam)
{
	int ret = ZB_RET_FAIL;
	int cmd;
	int isAsync;
	static int s_busy = FALSE;

	if (!pzbParam)
	{
		return ZB_RET_FAIL;
	}

	if (s_busy == TRUE)
	{
		DBGPRINT("zigbee_api: Busy...");
		return ZB_RET_BUSY;
	}

	s_busy = TRUE;

	DBGPRINT("zigbee_api: zbSendCommand:");

	cmd = pzbParam->command;

	switch (cmd)
	{
		case ZB_CMD_NET_FORM:
			DBGPRINT("ZB_CMD_NET_FORM\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_NET_LEAVE, NULL);
			sleep(2);
			ret = zbSetFormNetwork(pzbID, pzbParam);
			sleep(10);
			break;

		case ZB_CMD_NET_LEAVE:
			DBGPRINT("ZB_CMD_NET_LEAVE\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_NET_LEAVE, NULL);
			sleep(2);
			break;

		case ZB_CMD_NET_CHANGE_CHANNEL:
			DBGPRINT("ZB_CMD_NET_CHANGE_CHANNEL\n");
			ret = zbSetNetworkChangeChannel(pzbID, pzbParam);
			break;

		case ZB_CMD_NET_INFO:
			DBGPRINT("ZB_CMD_NET_INFO\n");
			ret = zbGetNetworkInfo(pzbID, pzbParam);
			break;

		case ZB_CMD_NET_SCANJOIN:
			DBGPRINT("ZB_CMD_NET_SCANJOIN\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_NET_SCANJOIN, NULL);
			break;

		case ZB_CMD_NET_PERMITJOIN:
			DBGPRINT("ZB_CMD_NET_PERMITJOIN\n");
			ret = zbSetPermitJoin(pzbID, pzbParam);
			break;

		case ZB_CMD_NET_PERMITJOIN_CLOSE:
			DBGPRINT("ZB_CMD_NET_PERMITJOIN_CLOSE\n");
			ret = zbSetPermitJoinClose(pzbID, pzbParam);
			break;

		case ZB_CMD_NET_LEAVE_REQUEST:
			DBGPRINT("ZB_CMD_NET_LEAVE_REQUEST\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_LEAVE_REQUEST, NULL);
			usleep(400000);
			break;

		case ZB_CMD_GET_NODEID:
			DBGPRINT("ZB_CMD_GET_NODEID\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_GET_NODEID, &(pzbParam->param1));
			sleep(1);
			break;

		case ZB_CMD_NET_MTORR:
			DBGPRINT("ZB_CMD_NET_MTORR\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_MTORR, NULL);
			break;

		case ZB_CMD_GET_ENDPOINT:
			DBGPRINT("ZB_CMD_GET_ENDPOINT\n");
			/*ret = zbGetActiveEndpoint(pzbID, pzbParam);*/
			ret = clientGetArrayCommand(pzbID, pzbParam, IPC_CMD_GET_ENDPOINT);
			usleep(300000);
			break;

		case ZB_CMD_GET_CLUSTERS:
			DBGPRINT("ZB_CMD_GET_CLUSTERS\n");
			/*ret = zbGetClusters(pzbID, pzbParam);*/
			ret = clientGetArrayCommand(pzbID, pzbParam, IPC_CMD_GET_CLUSTERS);
			usleep(300000);
			break;

		case ZB_CMD_SET_ONOFF:
			DBGPRINT("ZB_CMD_SET_ONOFF\n");
			ret = zbSetOnOff(pzbID, pzbParam);
			break;

		case ZB_CMD_GET_ONOFF:
			DBGPRINT("ZB_CMD_GET_ONOFF\n");
			isAsync = pzbParam->param1;
			/*ret = zbGetOnOff(pzbID, pzbParam);*/
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_GET_ONOFF, &(pzbParam->param1));
			zbSyncSleep(isAsync);
			break;

		case ZB_CMD_SET_LEVEL:
			DBGPRINT("ZB_CMD_SET_LEVEL\n");
			ret = zbSetLevel(pzbID, pzbParam);
			break;

		case ZB_CMD_GET_LEVEL:
			DBGPRINT("ZB_CMD_GET_LEVEL\n");
			isAsync = pzbParam->param1;
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_GET_LEVEL, &(pzbParam->param1));
			zbSyncSleep(isAsync);
			break;

		case ZB_CMD_SET_COLOR:
			DBGPRINT("ZB_CMD_SET_COLOR\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_COLOR, NULL);
			break;

		case ZB_CMD_GET_COLOR:
			DBGPRINT("ZB_CMD_GET_COLOR\n");
			ret = zbGetColor(pzbID, pzbParam);
			/*ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_GET_COLOR, &(pzbParam->param1));*/
			zbSyncSleep(pzbParam->param1);
			break;

		case ZB_CMD_SET_COLOR_TEMP:
			DBGPRINT("ZB_CMD_SET_COLOR_TEMP\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_COLOR_TEMP, NULL);
			break;

		case ZB_CMD_GET_COLOR_TEMP:
			DBGPRINT("ZB_CMD_GET_COLOR_TEMP\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_GET_COLOR_TEMP, &(pzbParam->param1));
			zbSyncSleep(pzbParam->param1);
			break;

		case ZB_CMD_GET_MODELCODE:
			DBGPRINT("ZB_CMD_GET_MODELCODE\n");
			ret = clientGetStringCommand(pzbID, pzbParam, IPC_CMD_GET_MODELCODE);
			usleep(200000);
			break;

		case ZB_CMD_GET_MFGNAME:
			DBGPRINT("ZB_CMD_GET_MFGNAME\n");
			ret = clientGetStringCommand(pzbID, pzbParam, IPC_CMD_GET_MFGNAME);
			usleep(200000);
			break;

		case ZB_CMD_GET_DATE:
			DBGPRINT("ZB_CMD_GET_DATE\n");
			ret = clientGetStringCommand(pzbID, pzbParam, IPC_CMD_GET_DATE);
			usleep(200000);
			break;

		case ZB_CMD_GET_ZCLVERSION:
			DBGPRINT("ZB_CMD_GET_ZCLVERSION\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_GET_ZCLVERSION, &(pzbParam->param1));
			usleep(200000);
			break;

		case ZB_CMD_GET_APPVERSION:
			DBGPRINT("ZB_CMD_GET_APPVERSION\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_GET_APPVERSION, &(pzbParam->param1));
			usleep(200000);
			break;

		case ZB_CMD_GET_STACKVERSION:
			DBGPRINT("ZB_CMD_GET_STACKVERSION\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_GET_STACKVERSION, &(pzbParam->param1));
			usleep(200000);
			break;

		case ZB_CMD_GET_OTA_VERSION:
			DBGPRINT("ZB_CMD_GET_OTA_VERSION\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_GET_OTA_VERSION, &(pzbParam->param1));
			usleep(200000);
			break;

		case ZB_CMD_GET_HWVERSION:
			DBGPRINT("ZB_CMD_GET_HWVERSION\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_GET_HWVERSION, &(pzbParam->param1));
			usleep(200000);
			break;

		case ZB_CMD_GET_POWERSOURCE:
			DBGPRINT("ZB_CMD_GET_POWERSOURCE\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_GET_POWERSOURCE, &(pzbParam->param1));
			usleep(200000);
			break;

		case ZB_CMD_SET_IDENTIFY:
			DBGPRINT("ZB_CMD_SET_IDENTIFY\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_IDENTIFY, NULL);
			break;

		case ZB_CMD_SET_TESTMODE:
			DBGPRINT("ZB_CMD_SET_TESTMODE\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_TESTMODE, NULL);
			break;

		case ZB_CMD_SET_GROUP:
			DBGPRINT("ZB_CMD_SET_GROUP\n");
			ret = zbSetGroup(pzbID, pzbParam);
			break;

		case ZB_CMD_SET_LEVEL_MOVE:
			DBGPRINT("ZB_CMD_SET_LEVEL_MOVE\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_LEVEL_MOVE, NULL);
			break;

		case ZB_CMD_SET_LEVEL_STOP:
			DBGPRINT("ZB_CMD_SET_LEVEL_STOP\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_SET_LEVEL_STOP, NULL);
			break;

		case ZB_CMD_SET_OTA_NOTIFY:
			DBGPRINT("ZB_CMD_SET_OTA_NOTIFY\n");
			ret = zbSetOTANotify(pzbID, pzbParam);
			break;

		case ZB_CMD_GET_IAS_ZONE_STATUS:
			DBGPRINT("ZB_CMD_GET_IAS_ZONE_STATUS\n");
			ret = clientGeneralCommand(pzbID, pzbParam, IPC_CMD_GET_IAS_ZONE_STATUS, NULL);
			break;

		default:
			DBGPRINT("Unkown command:%d\n", cmd );
			break;
	}

	s_busy = FALSE;
	return ret;
}

