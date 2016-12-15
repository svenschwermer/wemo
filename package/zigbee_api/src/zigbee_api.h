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
#ifndef __WEMO_ZIGBEE_H__
#define __WEMO_ZIGBEE_H__

#include "cluster-id.h"

/* Belkin Custom Cluster */

#define ZCL_BELKIN_MFG_SPECIFIC_CLUSTER_ID           0xFF00


enum {
	ZB_DEV_UNKNOWN,
	ZB_DEV_HOME_GATEWAY	= 0x0050,
	ZB_DEV_LIGHT_ONOFF	= 0x0100,
	ZB_DEV_LIGHT_DIMMABLE	= 0x0101,
	ZB_DEV_LIGHT_COLOR	= 0x0102,
	ZB_DEV_IAS_ZONE		= 0x0402,
};

enum {
	ZB_ZONE_FOB        = 0x0115,
	ZB_ZONE_MOTION     = 0x000d,
	ZB_ZONE_DOOR       = 0x0015,
	ZB_ZONE_FIRE_ALARM = 0x0028,
};

enum {
	ZB_CMD_NULL,
	ZB_CMD_NET_FORM,
	ZB_CMD_NET_CHANGE_CHANNEL,
	ZB_CMD_NET_INFO,
	ZB_CMD_NET_PERMITJOIN,
	ZB_CMD_NET_PERMITJOIN_CLOSE,
	ZB_CMD_NET_LEAVE_REQUEST,
	ZB_CMD_GET_ENDPOINT,
	ZB_CMD_GET_CLUSTERS,
	ZB_CMD_SET_ONOFF,
	ZB_CMD_GET_ONOFF,
	ZB_CMD_SET_LEVEL,
	ZB_CMD_GET_LEVEL,
	ZB_CMD_SET_COLOR,
	ZB_CMD_GET_COLOR,
	ZB_CMD_GET_MODELCODE,
	ZB_CMD_GET_APPVERSION,
	ZB_CMD_SET_TESTMODE,
	ZB_CMD_GET_ZCLVERSION,
	ZB_CMD_GET_STACKVERSION,
	ZB_CMD_GET_HWVERSION,
	ZB_CMD_GET_MFGNAME,
	ZB_CMD_GET_DATE,
	ZB_CMD_GET_POWERSOURCE,
	ZB_CMD_GET_RADIOPARAMS,
	ZB_CMD_GET_NODEID,
	ZB_CMD_NET_MTORR,
	ZB_CMD_SET_IDENTIFY,
	ZB_CMD_SET_COLOR_TEMP,
	ZB_CMD_GET_COLOR_TEMP,
	ZB_CMD_SET_GROUP,
	ZB_CMD_SET_LEVEL_MOVE,
	ZB_CMD_SET_LEVEL_STOP,
	ZB_CMD_SET_OTA_NOTIFY,
	ZB_CMD_NET_SCANJOIN,
	ZB_CMD_GET_OTA_VERSION,
	ZB_CMD_NET_LEAVE,
	ZB_CMD_GET_IAS_ZONE_STATUS,
	ZB_CMD_TERMINATOR, // Add New commands above this
};

enum {
	ZB_LEVEL_UP = 0,
	ZB_LEVEL_DOWN = 1,
};

enum {
	ZB_POWER_OFF = 0,
	ZB_POWER_ON = 1,
	ZB_POWER_TOGGLE,
};

enum {
	ZB_GROUP_ADD = 1,
	ZB_GROUP_REMOVE = 2,
};

enum {
	ZB_RET_MAX_NODE = -7,
	ZB_RET_MAX_MSG = -6,
	ZB_RET_OUT_OF_ORDER = -5,
	ZB_RET_NO_RESPONSE = -4,
	ZB_RET_DELIVERY_FAILED = -3,
	ZB_RET_BUSY = -2,
	ZB_RET_FAIL = -1,
	ZB_RET_SUCCESS = 0,
	ZB_RET_CMD
};

// OTA Upgrade Policy
enum {
	ZB_UPGRADE_IF_SERVER_HAS_NEWER = 0,
	ZB_DOWNGRADE_IF_SERVER_HAS_OLDER = 1,
	ZB_REINSTALL_IF_SERVER_HAS_SAME = 2,
	ZB_NO_NEXT_VERSION = 3,
};

#define ZB_NO_MSG_BYTES     	80
#define ZB_NO_MSG_U16 		(ZB_NO_MSG_BYTES / 2)
#define ZB_NO_MSG_U32 		(ZB_NO_MSG_U16 / 2)

#define ZB_JOIN_DIR 	    	"/tmp/Belkin_settings"
#define ZB_JOIN_LIST 	    	ZB_JOIN_DIR "/zbdjoinlist.lst"
#define ZB_MODE_ROUTER 	    	ZB_JOIN_DIR "/zigbee.mode.router"

#define ZB_BITMASK_MULTICAST	0x00FF0000

// LED
#define ZBD_EVENT_OTA_STATUS 	"zb_ota_status"
#define ZBD_EVENT_OTA_PERCENT 	"zb_ota_percent"
#define ZBD_EVENT_ERROR_STATUS 	"zb_error_status"
// HomeSense
#define ZBD_EVENT_ZONE_STATUS 	"zb_zone_status"
#define ZBD_EVENT_NODE_PRESENT 	"zb_node_present"
#define ZBD_EVENT_NODE_VOLTAGE 	"zb_node_voltage"
#define ZBD_EVENT_NODE_LQI 	"zb_node_lqi"
#define ZBD_EVENT_NODE_RSSI 	"zb_node_rssi"
#define ZBD_EVENT_SCANJOIN_STATUS 	"zb_scanjoin_status"

typedef unsigned char BYTE;

typedef BYTE TEUID_64[8];
typedef unsigned int TNODEID;

typedef struct tagZBIDParam {
	TNODEID node_id;
	TEUID_64 node_eui;
	int endpoint;
} TZBID;

typedef struct tagZBParam {
	int device_type;
	int command;
	int ret;
	int param1;
	int param2;
	int param3;
	int param4;
	union
	{
		BYTE str[ZB_NO_MSG_BYTES];
		BYTE u8_data[ZB_NO_MSG_BYTES];
		unsigned short u16_data[ZB_NO_MSG_U16];
		unsigned int u32_data[ZB_NO_MSG_U32];
	};
} TZBParam;


/*
 * 	ZigBee API Commands & Return Values
 *
 *
 H1. ZigBee Network commands

	 # Form ZigBee network, find unused PAN ID and Form network
	 This function needs
		 command     = ZB_CMD_NET_FORM;
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Leave ZigBee network
	 This function needs
		 command     = ZB_CMD_NET_LEAVE;
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Get Channel and Power info
	 This function needs
		 command     = ZB_CMD_NET_INFO;
	 The the function will return
		 param1      = <channel>;
		 param2      = <power>;
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Change Channel
	 This function needs
		 command     = ZB_CMD_NET_CHANGE_CHANNEL;
		 param1      = <channel>; // 11 ~ 26
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Permit Joining
	 This function needs
		 command     = ZB_CMD_NET_PERMITJOIN;
		 param1      = duration in secs;
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;
		 And ZB_JOIN_LIST will contain the join list.
		 <node_id> <reversed bytes node_eui64> done
		 e.g.
		 afb7 0022a3000000afa0 done
		 afb8 0022a3000000afa1 done

	 # Close Permit Joining
	 This function needs
		 command     = ZB_CMD_NET_PERMITJOIN_CLOSE;
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Request to leave our network
	 This function needs
		 <node_id>
		 <node_eui>
		 command     = ZB_CMD_NET_LEAVE_REQUEST;
		 param1      = <leave flag >;0 remove itself, 0x40 also remove children, 0x80 rejoin
		 param2      = 0 default, 1 keep requesting every 5s until left upto 1 min 30 sec
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Get endpoint
	 This function needs
		 <node_id>
		 command     = ZB_CMD_GET_ENDPOINT;
		 param1      = <int*>; // array of int
		 param2      = <size of array>; // 10 should be sufficient normally, 240 max
	 The the function will return
		 param1      = <actual number of end points>
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Get nodeid
	 This function needs
		 <node_eui>
		 command     = ZB_CMD_GET_NODEID;
	 The the function will return
		 param1      = <node id>
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # MTORR
	 This function needs
		 command     = ZB_CMD_NET_MTORR;
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Scan for Joinable network, and join
	 This function needs
		 command     = ZB_CMD_NET_SCANJOIN;
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

 H1. ZigBee device common

	 # Getting ZigBee device type, and cluster IDs
	 This function needs
		 <node_id>
		 <endpoint>
		 command     = ZB_CMD_GET_CLUSTERS;
		 param1      = <int*>; // array of int
		 param2      = <size of array>; // 20 should be sufficient normally
	 The the function will return
	 	device type and
	 	copy the cluster list result to the param1 ptr as param[i]=<cluster id>
		 device_type = <ZB_DEV_LIGHT_DIMMABLE>;
		 param1      = <actual number of cluster ids>;
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

 H1. Basic Cluster Commands

	 # Get Model Code
	 This function needs
		 <node_id>
		 <endpoint>
		 command     = ZB_CMD_GET_MODELCODE;
		 param1	     = 0:Synchronous return, 1: Asynchronous will fetch app version, mfg name, and model code
	 The the function will return
		 param1      = <length of char string in msg>;
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;
		 str         = <model code as char string>

	 # Get ZCL version
	 This function needs
		 <node_id>
		 <endpoint>
		 command     = ZB_CMD_GET_ZCLVERSION;
	 The the function will return
		 param1      = <version>;
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Get Application version
	 This function needs
		 <node_id>
		 <endpoint>
		 command     = ZB_CMD_GET_APPVERSION;
	 The the function will return
		 param1      = <version>;
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Get Stack version
	 This function needs
		 <node_id>
		 <endpoint>
		 command     = ZB_CMD_GET_STACKVERSION;
	 The the function will return
		 param1      = <version>;
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Get HW version
	 This function needs
		 <node_id>
		 <endpoint>
		 command     = ZB_CMD_GET_HWVERSION;
	 The the function will return
		 param1      = <version>;
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Get Manufacturer Name
	 This function needs
		 <node_id>
		 <endpoint>
		 command     = ZB_CMD_GET_MFGNAME;
	 The the function will return
		 param1      = <length of char string in msg>;
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;
		 str         = <manufacturer as char string>

	 # Get Date Code
	 This function needs
		 <node_id>
		 <endpoint>
		 command     = ZB_CMD_GET_DATE;
	 The the function will return
		 param1      = <length of char string in msg>;
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;
		 str         = <date code as char string>

	 # Get Power Source
	 This function needs
		 <node_id>
		 <endpoint>
		 command     = ZB_CMD_GET_POWERSOURCE;
	 The the function will return
		 param1      = <power source>;
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;


 H1. Identify Cluster Commands

	 # Setting level of the LED light
		 command     = ZB_CMD_SET_IDENTIFY;
		 param1      = <unsigned 16bit value: remaining time in seconds>;
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

 H1. On/Off Cluster Commands

	 ALL commands in this cluster need
		 <node_id>
		 <endpoint>

	 # Setting On, Off controll
		 command     = ZB_CMD_SET_ONOFF;
		 param1      = ZB_POWER_OFF, ZB_POWER_ON, POWER_TOGGLE;
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Getting the status of On, Off
		 command     = ZB_CMD_GET_ONOFF;
		 param1	     = 0:Synchronous return, 1: Asynchronous return;
	 The synchronous function will return
		 param1      = ZB_POWER_OFF, ZB_POWER_ON;
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;
	 The asynchronous value will return by sysevent
		zb_onoff_status <eui64> <1,0>


 H1. Level Control Cluster Commands

	 ALL commands in this cluster need
		 <node_id>
		 <endpoint>

	 # Setting level of the LED light
		 command     = ZB_CMD_SET_LEVEL;
		 param1      = 0-255; the level
		 param2      = 0-0xffff; time in tenth of a second to reach the level
		 param3      = 0: With On/Off 1: level only
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Getting the status of level
		 command     = ZB_CMD_GET_LEVEL;
		 param1	     = 0:Synchronous return, 1: Asynchronous return;
	 The the function will return
		 param1      = 0-255;
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;
	 The asynchronous value will return by sysevent
		zb_level_status <eui64> <0-255>


	 # Move from the current dim level in an up or down direction
		 command     = ZB_CMD_SET_LEVEL_MOVE;
		 param1      = move mode: 0 up, 1 down
		 param2      = 0-0xff; rate in units per second, 0xff is as fast as possible.
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Stop level movement
		 command     = ZB_CMD_SET_LEVEL_STOP;
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;


 H1. Color Control Cluster Commands

	 ALL commands in this cluster need
		 <node_id>
		 <endpoint>

	 # Move to Color Command
		 command     = ZB_CMD_SET_COLOR;
		 param1      = <unsigned 16bit value of CurrentX> "CIE 1931 x" = CurrentX / 65536
		 param2      = <unsigned 16bit value of CurrentY> "CIE 1931 y" = CurrentY / 65536
		 param3      = <unsigned 16bit value of transition time in tenth of a second to reach the color>
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Getting color
		 command     = ZB_CMD_GET_COLOR;
	 The the function will return
		 param1      = <unsigned 16bit value of CurrentX> "CIE 1931 x" = CurrentX / 65536
		 param2      = <unsigned 16bit value of CurrentY> "CIE 1931 y" = CurrentY / 65536
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Move to ColorTemperature Command
		 command     = ZB_CMD_SET_COLOR_TEMP;
		 param1      = <unsigned 16bit value of ColorTemperature> "Color Temperature" = 1,000,000 / ColorTemperature in the range 1 to 65279
		 param2      = <unsigned 16bit value of transition time in tenth of a second to reach the color>
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Getting ColorTemperature
		 command     = ZB_CMD_GET_COLOR_TEMP;
	 The the function will return
		 param1      = <unsigned 16bit value of ColorTemperature> "Color Temperature" = 1,000,000 / ColorTemperature in the range 1 to 65279
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

 H1. Group Cluster Commands

	 ALL commands in this cluster need
		 <node_id>
		 <endpoint>

	 # Adding a device to a group
		 command     = ZB_CMD_SET_GROUP;
		 param1      = ZB_GROUP_ADD;
		 param2      = 0-0xffff; Arbitrary group id
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

	 # Removing a device to a group
		 command     = ZB_CMD_SET_GROUP;
		 param1      = ZB_GROUP_REMOVE;
		 param2      = 0-0xffff; Arbitrary group id
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;


 H1. OTA Cluster Commands

	 ALL commands in this cluster need
		 <node_id>
		 <endpoint>

	 # Trigger OTA upgrade
		 command     = ZB_CMD_SET_OTA_NOTIFY;
		 param1      = Payload type;
// Payload Type:
//   	0 = jitter value only
//   	1 = jitter and manufacuter id
//   	2 = jitter, manufacuter id, and device id
//   	3 = jitter, manufacuter id, device id, and firmware version
//
		 param2      = policy;
// Policy:
//	0 = UPGRADE_IF_SERVER_HAS_NEWER
//	1 = DOWNGRADE_IF_SERVER_HAS_OLDER
//	2 = REINSTALL_IF_SERVER_HAS_SAME
//	3 = NO_NEXT_VERSION

	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;


	 # Getting the version from OTA cluster
		 command     = ZB_CMD_GET_OTA_VERSION;
	 The the function will return
		 param1      = <version>;
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;

 H1. IAS Zone Cluster Commands

	 ALL commands in this cluster need
		 <node_id>
		 <endpoint>

	 # Getting the zone status
		 command     = ZB_CMD_GET_IAS_ZONE_STATUS;
	 The the function will return
		 ret         = ZB_RET_SUCCESS, ZB_RET_FAIL;
	 Zone Status will be notified by the sysevent
		zb_zone_status <eui64> <zone status> <zone id>

 *
 *
 *
 *
 *   	e.g.
 *
 *              TZBID zbID;
 *              TZBParam zb_param;
 *
 *		// How to turn on the ZigBee LED LIGHT
 *
 *              // .......
 *              // Assuming you have set node_id, endpoint
 *              // .......
 *
 *              zb_param.command	= ZB_CMD_SET_ONOFF;
 *              zb_param.param1		= ZB_POWER_ON;
 *
 *              zbSendCommand(&zbID, &zb_param);
 *
 * 		if (zb_param.ret == ZB_RET_SUCCESS)
 * 		{
 * 			// It's a success!
 *			// The light should be on!
 * 		}
 *
 */


int zbInit(void);
int zbSendCommand(TZBID *pzbID, TZBParam *pzbParam);

void zbDebugPrintZBID(TZBID *pzbID);
void zbCopyZBID(TZBID *pdestpzbID, TZBID *psrczbID);
#endif /* WEMO_ZIGBEE_H_ */

