/***************************************************************************
*
*
* rule.h
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
#ifndef		__RULE__H__
#define		__RULE__H__


typedef enum {
	WEEK_MON_E = 0x00, 
	WEEK_THUE_E, 
	WEEK_WEN_E, 
	WEEK_THURS_E, 
	WEEK_FRI_E, 
	WEEK_SAT_E, 
	WEEK_SUN_E,
	WEEK_DAYS_NO
} DAY_OF_WEEK_INDEX;

typedef enum {
	FOB_LEAVES = 0x00,
	FOB_ARRIVES,
	FOB_ARRIVES_OR_LEAVES,
	FOB_BUTTON_PRESS
} FOB_EVENT;

typedef enum {
	NO_MOTION = 0x00,
	MOTION
} PIR_EVENT;

typedef enum {
	OPENS = 0x00,
	CLOSES,
	OPENS_OR_CLOSES
} DW_EVENT;
#define SENSORFOB "Fob"
#define SENSORPIR "PIR"
#define SENSORDW "DWSensor" 
#define MOTIONSENSOR "Sensor"
#define MAKERSENSOR "Maker" 
#define SENSORCAPABILITY 10500
#define FOBKEYPRESS 20502


#ifdef PRODUCT_WeMo_LEDLight
#include "mxml.h"

#define  _SD_MODULE_
#include "subdevice.h"
#include "sd_configuration.h"

extern const char* szWeekDayShortName[WEEK_DAYS_NO];

#define DW_OPENS 0
#define DW_CLOSES 1
#define DW_OPENS_OR_CLOSES 2
#define SENSOR_NOTIFICATION_DISABLED "2" 
#define SENSOR_NOTIFY_STATUS 2

#define LED_START_CAPABILITIES 5
#define LED_END_CAPABILITIES 10
#define LED_OVERNIGHT_ACTION 15
#define GROUP_ID_LEN 10
#define DEVICE_ID_LEN 16
int isSensorRuleForZBActive(const char* , int **, int *);
int RunLinkSimpleTimerRule(int, unsigned int, char*, int);
bool makerulenode(mxml_node_t *pNode, SD_Device device, int status,int ruleId);
int doesNotificationRuleExist(SD_Device deviceID,int status, int *ruleId);
int isNotifyTrue (SD_Device device, int status,int level);
extern void GetCacheDeviceOnOff(SD_DeviceID deviceID, char *capability_id, char *capability_value);
extern int bs_SetReaction(char* udn, char* start, char* duration, char* end_action, const char** error_string);
void disableSensornNotifyRule(SD_Device device, int *p_manualNotifyStatus, int* p_freq);
void enableRuleNotification(int ruleId);
#endif
#include <ithread.h>
#include "defines.h"

#define INVALID_THREAD_HANDLE 		-1

#define UNITS_DIGIT_DET     		10
#define RULE_DB_URL             	"/tmp/rules/temppluginRules.db"
#define RULE_DB_FILE_PATH       	"/tmp/Belkin_settings/rules.db"
#define RULE_EXTRACT_DIR        	"/tmp/rules/"

#define RULE_TASK_FREQUENCY		10
#define SET_RULE_FLAG       1
#define RESET_RULE_FLAG     0

extern unsigned int g_SendRuleID;
	
#ifdef PRODUCT_WeMo_Insight
#define MAX_APNS_SUPPORTED    210 //(7*MAX_TIMER60)/2
#define INSIGHT_TASK_POLL_FREQ	10

typedef enum {
	E_EQUAL = 0x00,
	E_LARGER,
	E_LESS,
	E_EQUAL_OR_LARGER,
	E_EQUAL_OR_LESS,
	E_WRONG_OPCODE
	} CONDITION_OPCODE;

typedef enum {
	E_COST = 0x00,
	E_ON_DURATION,
	E_OFF_DURATION,
	E_SBY_DURATION,
	E_STATE,
	E_POWER,
	E_INVALID
	} INSIGHT_RULE_PARAMS;

struct __InsightCondition
{
    int        ParamCode;      //Prameter Code
    int        OPCode;           // Operation Code
    int        OPVal;            //Operation Value
    int        isTriggered;      //isTrigger Flag when condition is true
    int        SendApnsFlag;      //Send APNS Flag for that rule
    int        isActive;      //Check if rule is active
};

typedef struct __InsightCondition InsightCondition;
struct __InsightActiveState
{
    unsigned int         RuleId;      //Rule Id
    char		 isActive;       // is rule active flag
};
typedef struct __InsightActiveState InsightActiveState;
InsightActiveState	InsightActive[MAX_APNS_SUPPORTED];
#endif

#if defined (PRODUCT_WeMo_Smart)
extern int g_SmartAttributeID;
#endif

typedef enum ruleType
{
	e_SIMPLE_RULE = 0,
	e_TIMER_RULE,
	e_SENSOR_RULE,
	e_INSIGHT_RULE,
	e_AWAY_RULE,
	e_NOTIFICATION_RULE,
	e_COUNTDOWN_RULE,
	e_CROCKPOT_RULE,
	e_MAKERSENSOR_RULE,
	e_MAX_RULE
}ERuleType;


#define THREAD_HANDLE_INVALID		((pthread_t)-1)

#define RULE_ENGINE_DEFAULT		0
#define RULE_ENGINE_SCHEDULED		1
#define RULE_ENGINE_RELOAD		2
#define RULE_ENGINE_RUNNING		3

typedef struct ruleInfo
{
	struct ruleInfo	*psNext;
	ERuleType	ruleType;
	unsigned int 	ruleId;
	unsigned int	startTime;
	unsigned int 	ruleDuration;
	unsigned int 	startAction;
	unsigned int 	endAction;
	unsigned int	endTime;
	unsigned char	activeDays;
	unsigned char	isActive;
	unsigned char	isOvernight;
	unsigned char	isSunriseSunset;
	unsigned char	isDayChange;
        unsigned char   enablestatus;
	unsigned char	pad[2];
} SRuleInfo;

typedef struct timerList
{
	unsigned int time;
	unsigned int action;
#ifdef PRODUCT_WeMo_LEDLight	
	unsigned int ruleIdTimer;
#endif	
	struct timerList *nextTimer;
} STimerList;

extern STimerList *gpsTimerList;
typedef void* (*fpRuleCallback)(void *) ;

typedef struct ruleHandle
{
	pthread_t 	ruleThreadId;
	unsigned int 	ruleCnt;
}SRuleHandle;

typedef struct rulesQueue
{
	unsigned int ruleID;
	unsigned int notifyRuleID;
	unsigned int ruleFreq;
	unsigned long int ruleTS;
	char *ruleMSG;
	struct rulesQueue *next;
}SRulesQueue;

extern ithread_t g_handlerSchedulerTask;
extern volatile int gRestartRuleEngine;
extern volatile int gCountdownRuleInLastMinute;
extern unsigned long gCountdownEndTime;

void *RulesTask(void *args);
void *TimerTask(void *args);
void *InsightTask(void *args);
void *AwayTask(void *args);
void *CrockpotTask(void *args);
void *MakersensorTask(void *args);

extern fpRuleCallback gfpRuleThreadFn[e_MAX_RULE];
extern SRuleHandle gRuleHandle[e_MAX_RULE]; /* Includes thread handle for Scheduler at index 0, simple rules are executed by scheduler */

void GetCalendarDayInfo(int* dayIndex, int* monthIndex, int* year, int* seconds);

int IsNtpUpdate();

void *RulesNtpTimeCheckTask(void *args);

#if defined(PRODUCT_WeMo_Insight) || defined(PRODUCT_WeMo_SNS) || defined(PRODUCT_WeMo_NetCam)
int GetRuleIDFlag();
void SetRuleIDFlag(int FlagState);
#endif

void initRule();

void lockRule();
void unlockRule();

int ActivateRuleEngine();
int daySeconds(void);

void RuleToggleLed(int curState);
void resetSystem(void);
void executeSensorRule(void);
void executeNotifyRule(void);
void executeCountdownRule(int deviceState);
#ifndef PRODUCT_WeMo_LEDLight
void stopCountdownTimer(void);
int checkAndExecuteCountdownTimer(int deviceState);

#endif
void enqueueRuleQ(SRulesQueue *qNode);
SRulesQueue *dequeueRuleQ(void);
void destroyRuleQueue(void);

int SetRuleAction(unsigned int ruleAction, char *actuation);
#endif
