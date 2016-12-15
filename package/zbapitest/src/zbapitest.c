/***************************************************************************
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>
#include "zigbee_api.h"

#define DBG
#ifdef DBG
/*#define DBGPRINT(fmt, args...)      printf(fmt, ## args)*/
#define DBGPRINT(fmt, args...)      fprintf(stderr, fmt, ## args)
#else
#define DBGPRINT(fmt, args...)
#endif

TNODEID g_NodeID = 0xabcd;
/*const TEUID_64 g_NodeEUI = {0x6D,0x8F,0x29,0x9F,0xC9,0x62,0xC0,0x28};*/
#if 1
TEUID_64 g_NodeEUI = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
#else
TEUID_64 g_NodeEUI = {0x00,0x15,0x8d,0x00,0x00,0x31,0x79,0xd6};
#endif

const char *Help_Str = {
	"\n"
	"usage : zbapitest <args>\n"
	"<args>:\n"
	"        scanjoin\n"
	"        clear\n"
	"        form\n"
	"        leave\n"
	"        info\n"
	"        nodeid </tmp/Belkin_settings/zbid.xxxx>\n"
	"        MTORR \n"
	"        permit <sec 8u>\n"
	"        channel <channel 11~26>\n"
	"        identify <node id> <sec 16u>\n"
	"        on-off <node id> <0:off 1:on 2:toggle>\n"
	"        on-off-get <node id> <0:sync 1:async>\n"
	"        level <node id> <level 8u> <time 16u> <with_onoff 0:1>\n"
	"        level-move <node id> <0:up 1:down> <rate 8u>\n"
	"        level-stop <node id>\n"
	"        level-get <node id> <0:sync 1:async>\n"
	"        color <node id> <colorX 16u> <colorY 16u> <time 16u>\n"
	"        color-get <node id>\n"
	"        colortemp <node id> <color temperature> <time 16u>\n"
	"        colortemp-get <node id> \n"
	"        endpoint <node id>\n"
	"        clusters <node id>\n"
	"        leavereq <node id> <0:normal, 1:QA mode keep sending>\n"
	"        zclversion <node id>\n"
	"        appversion <node id>\n"
	"        stackversion <node id>\n"
	"        hwversion <node id>\n"
	"        modelcode <node id> <0:sync 1:async>\n"
	"        datecode <node id>\n"
	"        mfgname <node id>\n"
	"        powersource <node id>\n"
	"        group-add <node id> <group id 16u>\n"
	"        group-rem <node id> <group id 16u>\n"
	"        ota-notify <node id> <payloadtype> <policy>\n"
	"        ota-version <node id>\n"
	"        zonestatus <node id>\n"
	"        testmode <node id> <0:off 1..255:on>\n"
	"        \n"
	"example: ./zbapitest level 0xABCD 255 50\n"
	"\n"
};

void usage()
{
	fprintf(stderr, Help_Str);
}

static void SetZBID(TZBID *pzbID, TNODEID nodeID, TEUID_64 nodeEUI)
{
	if (!pzbID)
		return;

	pzbID->node_id = nodeID;

	if (nodeEUI)
	{
		memcpy(pzbID->node_eui, nodeEUI, 8);
	}
}

static int TestModeFunction(int paramEnable)
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_SET_TESTMODE;
	zb_param.param1 = paramEnable;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("TestModeFunction zb_param.device_type:%d\n", zb_param.device_type );
	}

	return ret;
}

static int TestPermitJoin(int durationSec)
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_NET_PERMITJOIN;
	zb_param.param1 = durationSec;

	DBGPRINT("TestPermitJoin time:%d\n", durationSec );

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("TestPermitJoin zb_param.param2:%d\n", zb_param.device_type );
	}

	return ret;
}

static int TestGroupAddRemove(int groupAdd, int groupId)
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_SET_GROUP;
	zb_param.param1 = groupAdd;
	zb_param.param2 = groupId;

	DBGPRINT("TestGroupAddRemove groupAdd:%d, groupId:0x%02X\n", groupAdd, groupId);

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success\n");
	}

	return ret;
}

static int TestIdentify(int time)
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_SET_IDENTIFY;
	zb_param.param1 = time;

	DBGPRINT("TestIdentify time:%d\n", time );

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success\n");
	}

	return ret;
}

static int TestChangeChannel(int channel)
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_NET_CHANGE_CHANNEL;
	zb_param.param1 = channel;

	DBGPRINT("TestChangeChannel channel:%d\n", channel );

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success\n");
	}

	return ret;
}

static int TestLeaveRequest(int bkeepSend)
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_NET_LEAVE_REQUEST;
	zb_param.param1  = 0;
	zb_param.param2  = bkeepSend;

	DBGPRINT("TestLeaveRequest bkeepSend:%d \n", bkeepSend );

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success zb_param.ret:%d\n", zb_param.ret );
	}

	return ret;
}

static int TestMTORR()
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_NET_MTORR;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success zb_param.ret:%d\n", zb_param.ret );
	}

	return ret;
}

static int TestGetMFGName()
{
	int ret;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_MFGNAME;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("MFG Name:%s len:%d\n", zb_param.str, zb_param.param1);
	}

	return ret;
}

static int TestGetModelCode(int isAsync)
{
	int ret;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_MODELCODE;
	zb_param.param1 = isAsync;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Model Code:%s len:%d\n", zb_param.str, zb_param.param1);
	}

	return ret;
}

static int TestOnOff(int paramOnOff)
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	if (paramOnOff > ZB_POWER_TOGGLE || paramOnOff < 0)
	{
		DBGPRINT("Invalid param:%d\n", paramOnOff);
		return -1;
	}

	SetZBID(&zbID, g_NodeID, g_NodeEUI);

	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_SET_ONOFF;
	zb_param.param1  = paramOnOff;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success TestOnOff zb_param.ret:%d\n", zb_param.ret );
	}

	return ret;
}

static int TestGetLevel(int isAsync)
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_LEVEL;
	zb_param.param1 = isAsync;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success TestGetLevel Level:%d, ret:%d\n", zb_param.param1, zb_param.ret );
	}

	return ret;
}

static int TestGetOnOff(int isAsync)
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_ONOFF;
	zb_param.param1 = isAsync;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success TestGetOnOff OnOff:%d, ret:%d\n", zb_param.param1, zb_param.ret );
	}

	return ret;
}

static int TestGetZCLVersion()
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_ZCLVERSION;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		DBGPRINT("Success ZB_CMD_GET_ZCLVERSION:0x%X, ret:%d\n", zb_param.param1, zb_param.ret );
	}

	return ret;
}

static int TestGetStackVersion()
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_STACKVERSION;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		DBGPRINT("Success ZB_CMD_GET_STACKVERSION :0x%X, ret:%d\n", zb_param.param1, zb_param.ret );
	}

	return ret;
}

static int TestGetOTAVersion()
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_OTA_VERSION;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		DBGPRINT("Success ZB_CMD_GET_OTA_VERSION :0x%X, ret:%d\n", zb_param.param1, zb_param.ret );
	}

	return ret;
}

static int TestGetHWVersion()
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_HWVERSION;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		DBGPRINT("Success ZB_CMD_GET_HWVERSION :0x%X, ret:%d\n", zb_param.param1, zb_param.ret );
	}

	return ret;
}

static int TestGetPowerSource()
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_POWERSOURCE;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		DBGPRINT("Success ZB_CMD_GET_POWERSOURCE :0x%X, ret:%d\n", zb_param.param1, zb_param.ret );
	}

	return ret;
}

static int TestGetDateCode()
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_DATE;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Date code:%s len:%d\n", zb_param.str, zb_param.param1);
	}

	return ret;
}

static int TestGetAppVersion()
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_APPVERSION;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		DBGPRINT("Success ZB_CMD_GET_APPVERSION:0x%X, ret:%d\n", zb_param.param1, zb_param.ret );
	}

	return ret;
}

static int TestLevelMove(int updown, int rate)
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_SET_LEVEL_MOVE;
	zb_param.param1  = updown;
	zb_param.param2  = rate;

	DBGPRINT("updown:%d rate:0x%x \n", updown, rate);

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success TestLevelMove zb_param.ret:%d\n", zb_param.ret );
	}

	return ret;
}

static int TestLevelStop()
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_SET_LEVEL_STOP;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success TestLevelStop zb_param.ret:%d\n", zb_param.ret );
	}

	return ret;
}

static int TestLevelControl(int level, int time, int withOnOff)
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_SET_LEVEL;
	zb_param.param1  = level;
	zb_param.param2  = time;
	zb_param.param3  = withOnOff;

	DBGPRINT("level:%d time:%d withOnOff:%d \n", level, time, withOnOff);

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success TestLevelControl zb_param.ret:%d\n", zb_param.ret );
	}

	return ret;
}

static int TestGetColorTemperature()
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_COLOR_TEMP;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success ColorTemperature:0x%02X, ret:%d\n", zb_param.param1, zb_param.ret );
	}

	return ret;
}

static int TestColorTemperatureControl(int colorTemp, int time)
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_SET_COLOR_TEMP;
	zb_param.param1  = colorTemp;
	zb_param.param2  = time;

	DBGPRINT("ColorTemperature:0x%02X, time:%d \n", colorTemp, time);

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success zb_param.ret:%d\n", zb_param.ret );
	}

	return ret;
}

static int TestColorControl(int colorX, int colorY, int time)
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_SET_COLOR;
	zb_param.param1  = colorX;
	zb_param.param2  = colorY;
	zb_param.param3  = time;

	DBGPRINT("colorX:0x%02X, colorY:0x%02X time:%d \n", colorX, colorY, time);

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success TestColorControl zb_param.ret:%d\n", zb_param.ret );
	}

	return ret;
}

static int TestGetColorControl()
{
	int ret;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 3;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_COLOR;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	DBGPRINT("Success ColorX:0x%02X ColorY:0x%02X \n", zb_param.param1, zb_param.param2);

	return ret;
}

#define N_ENDPOINTS 10
static int TestGetEndpoint()
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;
	int endpoint_list[N_ENDPOINTS];

	memset(endpoint_list, 0, sizeof(endpoint_list));

	SetZBID(&zbID, g_NodeID, g_NodeEUI);

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_ENDPOINT;
	zb_param.param1  = (int) endpoint_list;
	zb_param.param2  = N_ENDPOINTS;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		int i;
		int n_endpoints = zb_param.param1;

		for (i = 0; i < N_ENDPOINTS; i++)
		{
			if ( i == n_endpoints )
			{
				DBGPRINT("Reached n_endpoints:%d\n", n_endpoints);
				break;
			}
			DBGPRINT("i:%d, endpoint:%d\n", i, endpoint_list[i]);
		}

		DBGPRINT("Success zb_param.param1:%d\n", zb_param.param1 );
	}

	return ret;
}

#define N_CLUSTERS 20
static int TestGetClusters()
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;
	int cluster_list[N_CLUSTERS];

	memset(cluster_list, 0, sizeof(cluster_list));

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_CLUSTERS;
	zb_param.param1  = (int) cluster_list;
	zb_param.param2  = N_CLUSTERS;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		int i;
		int n_clusters = zb_param.param1;

		for (i = 0; i < N_CLUSTERS; i++)
		{
			if ( i == n_clusters )
			{
				DBGPRINT("Reached n_clusters:%d\n", n_clusters);
				break;
			}
			DBGPRINT("Cluster ID:0x%04X\n", cluster_list[i]);
		}

		DBGPRINT("Success device_type:0x%04X, param1:%d\n", zb_param.device_type, zb_param.param1 );
	}

	return ret;
}

static int TestScanJoinableNetwork()
{
	int ret;
	TZBParam zb_param;
	TZBID zbID;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_NET_SCANJOIN;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	DBGPRINT("Success TestScanJoinableNetwork \n");

	return ret;
}

static int TestInfoNetwork()
{
	int ret;
	TZBParam zb_param;
	TZBID zbID;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_NET_INFO;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	DBGPRINT("Success Channel:%d Power:%d\n", zb_param.param1, zb_param.param2);

	return ret;
}

static zbid2EUI(const char *strEUI, TEUID_64 nodeEUI)
{
	unsigned char *ptr_eui = nodeEUI;

	DBGPRINT("strEUI:%s\n", strEUI);

	sscanf(strEUI, "/tmp/Belkin_settings/zbid.%2hhX%2hhX%2hhX%2hhX%2hhX%2hhX%2hhX%2hhX", &(ptr_eui[7]), &(ptr_eui[6]), &(ptr_eui[5]), &(ptr_eui[4]), &(ptr_eui[3]), &(ptr_eui[2]), &(ptr_eui[1]), &(ptr_eui[0]) );

	int i;
	for(i=0; i<=7; i++)
	{
		DBGPRINT("%02X", nodeEUI[7-i]);
	}
	DBGPRINT("\n");

	return;
}

static int TestGetNodeID()
{
	int ret;
	TZBParam zb_param;
	TZBID zbID;

	memset(&zb_param, 0, sizeof(zb_param));

	SetZBID(&zbID, 0, g_NodeEUI);
	zbDebugPrintZBID(&zbID);

	zb_param.command = ZB_CMD_GET_NODEID;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success NodeID:0x%X, ret:%d\n", zb_param.param1, zb_param.ret );
	}

	return ret;
}

static int TestFormNetwork()
{
	int ret;
	TZBParam zb_param;
	TZBID zbID;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_NET_FORM;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	return ret;
}

static int TestNetworkLeave()
{
	int ret;
	TZBParam zb_param;
	TZBID zbID;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_NET_LEAVE;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	return ret;
}

static int TestOTANotify(int payloadType, int policy)
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_SET_OTA_NOTIFY;
	zb_param.param1 = payloadType;
	zb_param.param2 = policy;

	DBGPRINT("TestOTANotify payloadType:%d, policy:%d\n", payloadType, policy);

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success\n");
	}

	return ret;
}

static int TestGetIasZoneStatus()
{
	int ret = 0;
	TZBParam zb_param;
	TZBID zbID;

	SetZBID(&zbID, g_NodeID, g_NodeEUI);
	zbID.endpoint = 1;

	memset(&zb_param, 0, sizeof(zb_param));
	zb_param.command = ZB_CMD_GET_IAS_ZONE_STATUS;

	ret = zbSendCommand(&zbID, &zb_param);
	if (ret == ZB_RET_FAIL)
	{
		DBGPRINT("Error zbSendCommand:%d\n", ret );
		return -1;
	}

	if (zb_param.ret == ZB_RET_SUCCESS)
	{
		// It's a success!
		DBGPRINT("Success TestGetIasZoneStatus\n");
	}

	return ret;
}

int main(int argc, char *argv[])
{
	int ret = -1;
	char *str, *endptr;
	TZBID zbID;

	zbInit();

	do
	{
		if (argc < 2) break;

		if (0 == strcmp(argv[1], "on-off"))
		{
			int onoff;
			if (argc < 4) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x\n", g_NodeID);
			str = argv[3];
			onoff = strtol(str, &endptr, 0);
			DBGPRINT("onoff:%d\n", onoff);
			ret = TestOnOff(onoff);
			return ret;
		}
		else if (0 == strcmp(argv[1], "on-off-get"))
		{
			int is_async;
			if (argc < 4) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			str = argv[3];
			is_async = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x is_async:%d \n", g_NodeID, is_async);
			return TestGetOnOff(is_async);
		}
		else if (0 == strcmp(argv[1], "level"))
		{
			int level, time, with_onoff;
			if (argc < 6) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			str = argv[3];
			level = atoi(str);
			str = argv[4];
			time = atoi(str);
			str = argv[5];
			with_onoff= atoi(str);
			return TestLevelControl(level, time, with_onoff);
		}
		else if (0 == strcmp(argv[1], "level-move"))
		{
			int updown, rate;
			if (argc < 5) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			str = argv[3];
			updown = strtol(str, &endptr, 0);
			str = argv[4];
			rate = strtol(str, &endptr, 0);
			return TestLevelMove(updown, rate);
		}
		else if (0 == strcmp(argv[1], "level-stop"))
		{
			if (argc < 3) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			return TestLevelStop();
		}
		else if (0 == strcmp(argv[1], "level-get"))
		{
			int is_async;
			if (argc < 4) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			str = argv[3];
			is_async = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x is_async:%d \n", g_NodeID, is_async);
			return TestGetLevel(is_async);
		}
		else if (0 == strcmp(argv[1], "colortemp"))
		{
			int color_temp, time;
			if (argc < 5) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			str = argv[3];
			color_temp = strtol(str, &endptr, 0);
			str = argv[4];
			time = atoi(str);
			return TestColorTemperatureControl(color_temp, time);
		}
		else if (0 == strcmp(argv[1], "colortemp-get"))
		{
			if (argc < 3) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x\n", g_NodeID);
			return TestGetColorTemperature();
		}
		else if (0 == strcmp(argv[1], "color"))
		{
			int color_x, color_y, time;
			if (argc < 6) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			str = argv[3];
			color_x = strtol(str, &endptr, 0);
			str = argv[4];
			color_y = strtol(str, &endptr, 0);
			str = argv[5];
			time = atoi(str);
			return TestColorControl(color_x, color_y, time);
		}
		else if (0 == strcmp(argv[1], "color-get"))
		{
			if (argc < 3) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x\n", g_NodeID);
			return TestGetColorControl();
		}
		else if (0 == strcmp(argv[1], "endpoint"))
		{
			if (argc < 3) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x\n", g_NodeID);
			return TestGetEndpoint();
		}
		else if (0 == strcmp(argv[1], "clusters"))
		{
			if (argc < 3) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x\n", g_NodeID);
			return TestGetClusters();
		}
		else if (0 == strcmp(argv[1], "leavereq"))
		{
			int b_keep_send;
			if (argc < 4) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);

			str = argv[3];
			b_keep_send = strtol(str, &endptr, 0);

			DBGPRINT("node_id:0x%x\n", g_NodeID);
			return TestLeaveRequest(b_keep_send);
		}
		else if (0 == strcmp(argv[1], "permit"))
		{
			int time;
			if (argc < 3) break;
			str = argv[2];
			time = strtol(str, &endptr, 0);
			DBGPRINT("permitjoin sec:%d\n", time);
			return TestPermitJoin(time);
		}
		else if (0 == strcmp(argv[1], "group-add"))
		{
			int group_id;
			if (argc < 4) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			str = argv[3];
			group_id = strtol(str, &endptr, 0);
			DBGPRINT("group_id:%d\n", group_id);
			return TestGroupAddRemove(ZB_GROUP_ADD, group_id);
		}
		else if (0 == strcmp(argv[1], "group-rem"))
		{
			int group_id;
			if (argc < 4) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			str = argv[3];
			group_id = strtol(str, &endptr, 0);
			DBGPRINT("group_id:%d\n", group_id);
			return TestGroupAddRemove(ZB_GROUP_REMOVE, group_id);
		}
		else if (0 == strcmp(argv[1], "identify"))
		{
			int time;
			if (argc < 4) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			str = argv[3];
			time = strtol(str, &endptr, 0);

			DBGPRINT("identify sec:%d\n", time);
			return TestIdentify(time);
		}
		else if (0 == strcmp(argv[1], "channel"))
		{
			int channel;
			if (argc < 3) break;
			str = argv[2];
			channel = strtol(str, &endptr, 0);
			DBGPRINT("channel:%d\n", channel);
			return TestChangeChannel(channel);
		}
		else if (0 == strcmp(argv[1], "modelcode"))
		{
			int is_async;
			if (argc < 4) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			str = argv[3];
			is_async = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x is_async:%d \n", g_NodeID, is_async);
			return TestGetModelCode(is_async);
		}
		else if (0 == strcmp(argv[1], "zclversion"))
		{
			if (argc < 3) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x\n", g_NodeID);
			return TestGetZCLVersion();
		}
		else if (0 == strcmp(argv[1], "appversion"))
		{
			if (argc < 3) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x\n", g_NodeID);
			return TestGetAppVersion();
		}
		else if (0 == strcmp(argv[1], "stackversion"))
		{
			if (argc < 3) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x\n", g_NodeID);
			return TestGetStackVersion();
		}
		else if (0 == strcmp(argv[1], "ota-version"))
		{
			if (argc < 3) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x\n", g_NodeID);
			return TestGetOTAVersion();
		}
		else if (0 == strcmp(argv[1], "hwversion"))
		{
			if (argc < 3) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x\n", g_NodeID);
			return TestGetHWVersion();
		}
		else if (0 == strcmp(argv[1], "datecode"))
		{
			if (argc < 3) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x\n", g_NodeID);
			return TestGetDateCode();
		}
		else if (0 == strcmp(argv[1], "powersource"))
		{
			if (argc < 3) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x\n", g_NodeID);
			return TestGetPowerSource();
		}
		else if (0 == strcmp(argv[1], "mfgname"))
		{
			if (argc < 3) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x\n", g_NodeID);
			return TestGetMFGName();
		}
		else if (0 == strcmp(argv[1], "scanjoin"))
		{
			return TestScanJoinableNetwork();
		}
		else if (0 == strcmp(argv[1], "info"))
		{
			return TestInfoNetwork();
		}
		else if (0 == strcmp(argv[1], "form"))
		{
			return TestFormNetwork();
		}
		else if (0 == strcmp(argv[1], "leave"))
		{
			return TestNetworkLeave();
		}
		else if (0 == strcmp(argv[1], "MTORR"))
		{
			return TestMTORR();
		}
		else if (0 == strcmp(argv[1], "clear"))
		{
			system("rm /tmp/Belkin_settings/zbdjoinlist.lst");
			system("rm /tmp/Belkin_settings/zbid.*");
			return 0;
		}
		else if (0 == strcmp(argv[1], "nodeid"))
		{
			if (argc < 3) break;
			str = argv[2];
			zbid2EUI(str, g_NodeEUI);
			return TestGetNodeID();
		}
		else if (0 == strcmp(argv[1], "ota-notify"))
		{
			int payload_type;
			if (argc < 5) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			str = argv[3];
			payload_type = strtol(str, &endptr, 0);
			str = argv[4];
			int policy = strtol(str, &endptr, 0);
			DBGPRINT("payload_type:%d, policy:%d\n", payload_type, policy);
			return TestOTANotify(payload_type, policy);
		}
		else if (0 == strcmp(argv[1], "zonestatus"))
		{
			if (argc < 3) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			DBGPRINT("node_id:0x%x\n", g_NodeID);
			return TestGetIasZoneStatus();
		}
		else if (0 == strcmp(argv[1], "testmode"))
		{
			int enable;
			if (argc < 4) break;
			str = argv[2];
			g_NodeID = strtol(str, &endptr, 0);
			str = argv[3];
			enable = strtol(str, &endptr, 0);

			DBGPRINT("testmode enable:%d\n", enable);
			return TestModeFunction(enable);
		}
	}
	while(0);

	usage();
	return 0;
}

