/***************************************************************************
 *
 *
 * simulatedOccupancy.h
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
#ifdef SIMULATED_OCCUPANCY
#include "rule.h"
#include "osUtils.h"
#include "global.h"
#include "logger.h"
#include <time.h>
#include <sys/time.h>
#include "upnpCommon.h"
#include "gpio.h"
#include "mxml.h"
#include "controlledevice.h"
#include "plugin_ctrlpoint.h"
#include "sunriset.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif


/* Default values: SIMULATED_MIN_ON_TIME: 30 mins, SIMULATED_MAX_ON_TIME: 180 mins, SIMULATED_FIRST_ON_TIME: 15 mins*/
#define 	SIMULATED_MIN_ON_TIME			(30*60)
#define 	SIMULATED_MAX_ON_TIME			(3*60*60)
#define		SIMULATED_FIRST_ON_TIME			(15*60)
#define		SIMULATED_RULE_FILE_PATH		"/tmp/Belkin_settings/simulatedRule.txt"
#define		SIMULATED_MAX_ON_ALLOWED		3
#define		SIMULATED_DISCOVERY_RETRIES		2
#define		SIMULATED_DISCOVERY_SLEEP		5	
#define		SIMULATED_UPNP_RESP_WAIT		5	
#define		SIMULATED_DURATION_ADDLTIME		5
#define		SIMULATED_TIME_NEAR_SHORTRULE_END_TIME	(60)	
#define		SIMULATED_TIME_NEAR_LARGERULE_END_TIME	(5*60)	
#define		SIMULATED_TIME_NEAR_TESTRULE_END_TIME	60
#define		SIMULATED_RULE_DURATION			(10*60)
#define		SIMULATED_MAX_TIMER_COUNT		2
#define		SIM_FIRST_ON_TIME			"FirstOnTime"
#define		SIM_MIN_ON_TIME				"MinimumOnTime"
#define		SIM_MAX_ON_TIME				"MaximumOnTime"
#define		SIM_DEVICE_COUNT			"SimulatedDeviceCount"
#define		SIM_MANUAL_TRIGGER			"SimulatedManualTrigger"
#define		SIM_MANUAL_TRIGGER_DATE			"SimulatedManualTriggerDate"
#define		SIMULATED_RANDOM_TIME(X,Y)		((X) + (rand()%(Y-X)))

typedef struct simulatedDevInfo
{
    unsigned int 	devIndex;
    char 		UDN[SIZE_256B];	
} SimulatedDevInfo;


typedef struct simulatedDevData
{
    int 			remTimeToToggle;	
    int 			binaryState;
    SimulatedDevInfo	sDevInfo;
    struct simulatedDevData *next;
} SimulatedDevData;

typedef struct simulatedDeviceRule
{
    int totalCount;
    int selfIndex;
    int upnpRespTotalCount;
    int onTS;
    int randomTimeToToggle;
    int firstOnTime;
    int minOnTime;
    int maxOnTime;
    int	ruleEndTime;
    char manualTriggerDate[SIZE_32B];
    SimulatedDevInfo *pDevInfo;
    SimulatedDevData *pUpnpRespInfo;
} SimulatedDevice;

extern SimulatedDevice *gpSimulatedDevice;
int adjustFirstTimer(int startTimer, int endTimer);
extern pCtrlPluginDeviceNode g_pGlobalPluginDeviceList;

int parseSimulatedRule(void);
int setSimulatedRuleFile(void *deviceList);
int evaluateNextActions(int startTimer, int endTimer);
void notifyManualToggle(void);
void unsetSimulatedData(void);
void cleanupAwayRule(int);
void setSimulatedStatus (char *status);
void inline updateManualToggleStatus(char *httpStatus);
void LockSimulatedOccupancy(void);
void UnlockSimulatedOccupancy(void);
void initSimulatedOccupancyLock(void);
void simulatedStartControlPoint(void);
void simulatedOccupancyInit(void);
void saveManualTriggerData(void);
void todaysDate(char *date);
int checkLastManualToggleState(void);

#endif
