/***************************************************************************
*
*
* gpio.h
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
#ifndef __GPIO_H__
#define __GPIO_H__

#include <stdio.h>
#include <ithread.h>
#include "global.h"
#include "logger.h"
#include "itc.h"

#define DEFAULT_SENSOR_DELAY 			1
#define DEFAULT_SENSOR_SENSITIVITY 		10

extern ithread_t power_thread;
extern ithread_t sensor_thread;
extern ithread_t relay_thread;
extern int gButtonHealthPunch;
extern ithread_t ButtonTaskMonitor_thread;
extern ithread_t SensorTaskMonitor_thread;



typedef int LED_ID;
typedef unsigned char BYTE;


#define         POWER_ON        1
#define         POWER_OFF       0
#ifdef PRODUCT_WeMo_Insight
#define         POWER_SBY       8
#endif


#define DEVICE_UNKNOWN	    0x00
#define DEVICE_SOCKET 	    0x01
#define DEVICE_SENSOR 	    0x02
#define DEVICE_BABYMON 		0x03
#define DEVICE_STREAMING	0x04
#define DEVICE_BRIDGE       0x05
#define DEVICE_INSIGHT      0x06

/* Device Types for Jarden Products */
#define DEVICE_CROCKPOT  	0x07	/* Device type for CrockPot */
#define DEVICE_SBIRON       0x0A
#define DEVICE_MRCOFFEE	  	0x0B	/* Device type for Coffee */
#define DEVICE_PETFEEDER    0x0C
#define DEVICE_SMART    	0x0D

#define DEVICE_LIGHTSWITCH  	0x08
#define DEVICE_NETCAM		0x09
//#define FAKE_WNC_TYPE_AS_NETCAM
#if defined(FAKE_WNC_TYPE_AS_NETCAM)
#  define DEVICE_LINKSYS_WNC_CAM DEVICE_NETCAM
#else
#  define DEVICE_LINKSYS_WNC_CAM 10
#endif
#define LINKSYSWNC_NAME "LinksysWNCSensor"

#define DEVICE_SBIRON       	0x0A
#define DEVICE_MRCOFFEE         0x0B    /* Device type for Coffee */
#define DEVICE_PETFEEDER    	0x0C
#define DEVICE_SMART            0x0D
#define DEVICE_MAKER            0x0E
#define DEVICE_ECHO            	0x0F

#define STATUS_TS "StatusTS"
extern int g_PowerStatus;

#ifdef PRODUCT_WeMo_Jarden

#define ONE_MSEC								1000
#define MINIMUM_DELAY							250
#define FIVE_HUNDRED_MSEC                       (500*1000)
#define ONE_SECOND                              (FIVE_HUNDRED_MSEC * 2)
/* 500 msec once loop, so 2 times is 1 sec, 60*15 is 15min duration */
#define PUSH_INFO_TIME                          (2*60*15)

/* Global variable associated to Jarden */

extern int g_modeStatus; /* initialized to 0, this may be diff from actual mode value */
extern int g_timeCookTime; /* Time is initialized to 0 min */
extern int g_delayTime; /* Time is initialized to 0 min */
extern int g_remoteSetStatus;
extern int g_updateInprogress;
extern int g_remoteNotify;
extern int g_remoteAPNSNotify;
extern int g_setStatusInProgress; /* To indicate set status is in progress */
extern int g_OffTimer;	/* To switch off automatically after a specifid time */
extern short int g_SetStatefromPhone;
#endif /*#ifdef PRODUCT_WeMo_Jarden*/

#ifdef PRODUCT_WeMo_Smart
extern int g_remoteNotify;
extern int g_remoteAPNSNotify;
#endif
//---- Sensor ------
void *PowerButtonTask(void *args);
extern void *ButtonTaskMonitorThread(void *arg);

void *sensorGPIOTask(void *args);
extern void *SensorTaskMonitorThread(void *arg);

//- LED
void *LedAutoToggleLoop(void *args);


int setPower(int command);

#ifdef PRODUCT_WeMo_Light
void *ResetButtonTask(void *args);
int ChangeNightLight(int type);
int u32DimVal;
#endif

//------ LED -------------
#ifndef PRODUCT_WeMo_LEDLight
void initLED();
#endif
void *WiFiLedTask(void *args);

/**
 * LED_ID: ID of the LED
 *
 * OnDuration: on interval, 0xFF solid
 *
 * OffDuration: off interval, 0xFF OFF
 *
 * counter: 0x00, forever until next change request coming
 * ***/
int setLED(LED_ID id, BYTE OnDuration, BYTE OffDuration, int counter);

/************************************************************
 * SaveDeviceConfig:
 * 	Call file system API to save key
 * 
 * 
 * 
 * 
 * 
 * **********************************************************/
int SaveDeviceConfig(const char*  szKey, const char* szValue);

/**************************************************************
 * GetDeviceConfig:
 * 	Call file system API to get value of key
 * 
 * 
 * 
 * 
 * ************************************************************/
char* GetDeviceConfig(const char*  szKey);


int GetCurBinaryState();

void SetCurBinaryState(int toState);

void StartSensorTask();
void StopSensorTask();

#ifdef PRODUCT_WeMo_Jarden
void LockJarden();
void UnlockJarden();
void LockJardenRemote();
void UnlockJardenRemote();
void StartJardenTask();
void StopJardenTask();
#endif /*#ifdef PRODUCT_WeMo_Jarden*/

#ifdef PRODUCT_WeMo_Smart
void LockSmart();
void UnlockSmart();
void LockSmartRemote();
void UnlockSmartRemote();
void StartSmartTask();
void StopSmartTask();
#endif /*#ifdef PRODUCT_WeMo_Smart*/

/**
 *
 *	To change relay state, uniform the interface through local button,
 *  mobile app, and cloud
 *
 *
 *
 *
 ******************************************/
int ChangeBinaryState(int newState);


void LockLED();
void UnlockLED();

void LockSensor();
void UnlockSensor();

void togglePower();


#define SENSORING_ON	0x01
#define SENSORING_OFF	0x00


void initSensor();

extern int g_cntSensorDelay;
extern int g_cntSensitivity;

void SetSensorConfig(int  delay, int sensitivity);


extern int g_isInsightRuleActivated;

void InternalToggleUpdate(int curState);
void ToggleUpdate(int curState);

void *RelayControlTask(void *args);
int ProcessRelayEvent(pNode node);

void ResetSensor2Default();

int GetSensorState(void);

void SetLastUserActionOnState(int state);

int IsLastUserActionOn();

#ifdef PRODUCT_WeMo_NetCam
void SetSensorStatus(int state);
#endif
#endif /* GPIO_H_ */
