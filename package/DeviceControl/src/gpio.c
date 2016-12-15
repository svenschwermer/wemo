/***************************************************************************
*
*
* gpio.c
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
#include <stdlib.h>
#include <sys/time.h>
#include "fcntl.h"

#include "gpioApi.h"
#include "utils.h"
#include "gpio.h"
#include "rule.h"
#include "itc.h"
#include "controlledevice.h"
#include "remoteAccess.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#include "gemtek_api_LED.h"
#endif

#ifdef PRODUCT_WeMo_Jarden
#include "wasp_api.h"
#include "wasp.h"
#endif

#ifdef PRODUCT_WeMo_Smart
#include "wasp_api.h"
#include "wasp.h"
#endif

#ifdef SIMULATED_OCCUPANCY
#include "simulatedOccupancy.h"
#endif

#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

#ifdef SIMULATED_OCCUPANCY
extern unsigned int gSimDevSelfOnTS;
extern int gSimulatedRuleRunning;
extern int gSimManualTrigger;
#endif

extern SRuleHandle gRuleHandle[e_MAX_RULE];

#if defined(PRODUCT_WeMo_Insight)
	const char *szButtonPath = "/proc/POWER_BUTTON";
	char *szNewSensorPath   = "/proc/MOTION_SENSOR_STATUS"; 
	#define	 MOTION_SENSORED 0x01
#elif defined NEW_BOARD

const char* szButtonPath = "/proc/GPIO12";
char* szNewSensorPath   = "/proc/MOTION_SENSOR_STATUS"; 
#define		MOTION_SENSORED 0x01

#else
const char* szNewSensorPath = "/proc/GPIO13";

const char* szButtonPath = "/proc/GPIO13";

#define		MOTION_SENSORED 	0x00


#endif

//thread will sleep for 50000 microseconds, a loop of 100 will produce delay for 5 seconds
#if defined(PRODUCT_WeMo_Light) || defined(PRODUCT_WeMo_SNS)
const char* szResetBottonPath = "/proc/GPIO10";
#define RESET_NETWORK_LOOP_CNT	100
#elif defined(PRODUCT_WeMo_LEDLight)
const char* szResetBottonPath = "/proc/GPIO0";
#define RESET_NETWORK_LOOP_CNT	100
#endif 
#if defined(PRODUCT_WeMo_Light)
int u32DimVal=0;
#endif
int g_PowerStatus  = POWER_OFF;
 
int g_SensorStatus = 0x00;

#define MIN_HUMAN_BEING_ACTION  3 //- seconds
#define IVALID_THREAD_HANDLE -1

static  ithread_t led_thread = IVALID_THREAD_HANDLE; //-Led thread if OnDuration and OffDuration normal (not 0xFF)

int g_cntSensorDelay = DEFAULT_SENSOR_DELAY;
int g_cntSensitivity = DEFAULT_SENSOR_SENSITIVITY;
int gButtonHealthPunch = 0;
int gSensorHealthPunch = 0;

static int g_isButtonPressed = 0x00;

/**! track the time stamp of last message to prevent message traffic within UPnP and etc */
#define	MAX_SENSOR_MSG_TIMEOUT	DELAY_20SEC	// 20 seconds
static unsigned long	s_lLastSensoringUpdateTime 		= 0x00;
static unsigned long	s_lLastNoSensoringUpdateTime 	= 0x00;

#if defined(PRODUCT_WeMo_Insight)
	#define RELAY_GPIO	"/proc/POWER_RELAY"
	#define RELAY_ON		"1"
	#define RELAY_OFF		"0"
#elif defined NEW_BOARD
	#define		RELAY_GPIO			"/proc/GPIO13"
	#define		RELAY_COMMAND_ON	"echo 1 > /proc/GPIO13 &"
	#define		RELAY_COMMAND_OFF	"echo 0 > /proc/GPIO13 &"
	#define		RELAY_ON			"1"
	#define		RELAY_OFF			"0"
#else
	#define		RELAY_COMMAND_ON	"echo 0 > /proc/GPIO25 &"
	#define		RELAY_COMMAND_OFF	"echo 1 > /proc/GPIO25 &"
	#define		RELAY_GPIO			"/proc/GPIO25"
	#define		RELAY_ON			"0"
	#define		RELAY_OFF			"1"
	
#endif


#if defined(PRODUCT_WeMo_Insight)
	#define		LED_GPIO					"/proc/RELAY_LED"
	#define		RELAY_LED_ON			"0"
	#define		RELAY_LED_OFF			"1"
#elif defined(PRODUCT_WeMo_LEDLight)
	#define		INDICATOR_LED_COMMAND_ON	"echo 0 > /proc/GPIO14 &"
	#define		INDICATOR_LED_COMMAND_OFF	"echo 1 > /proc/GPIO14 &"
	#define		LED_GPIO		            "/proc/GPIO14"
	#define		RELAY_LED_ON			    "0"
	#define		RELAY_LED_OFF			    "1"
#elif defined NEW_BOARD
	#define		INDICATOR_LED_COMMAND_ON	"echo 0 > /proc/GPIO9 &"
	#define		INDICATOR_LED_COMMAND_OFF	"echo 1 > /proc/GPIO9 &"
	#define		LED_GPIO		"/proc/GPIO9"
	#define		RELAY_LED_ON			"0"
	#define		RELAY_LED_OFF			"1"
#else
	//-EVB, port different
	#define		INDICATOR_LED_COMMAND_ON	"echo 0 > /proc/GPIO26 &"
	#define		INDICATOR_LED_COMMAND_OFF	"echo 1 > /proc/GPIO26 &"
	#define 	LED_GPIO		"/proc/GPIO26"
	#define 	RELAY_LED_ON			"0"
	#define 	RELAY_LED_OFF 			"1"
	
#endif

#if defined(PRODUCT_WeMo_Light) 
	#define 	NIGHTLIGHT_LED_ON			"0"
	#define 	NIGHTLIGHT_LED_OFF 			"1"
	#define		NIGHT_LED_GPIO				"/proc/GPIO14"
#endif
#define BUTTON_RELEASED 0x01
#define BUTTON_PRESSED	0x00



extern int g_eDeviceType;
extern int g_phoneFlag;
extern int g_timerFlag;
#ifdef PRODUCT_WeMo_Jarden
extern unsigned long int g_poweronStartTime;
#endif /*#ifdef PRODUCT_WeMo_Jarden*/
ithread_t power_thread = -1;
ithread_t relay_thread = -1;
ithread_t ButtonTaskMonitor_thread = -1;
ithread_t SensorTaskMonitor_thread = -1;

ithread_t sensor_thread = -1;

#ifdef PRODUCT_WeMo_Jarden

#define WARM_COOK_TIME_IN_MINUTES  (6*60)   //6hours default cook time for WARM once state changed from Phone

/* Global variable associated to Crockpot*/

int g_modeStatus = 0; /* initialized to 0, this may be diff from actual mode value */
int g_delayTime = 0; /* Time is initialized to 0 min */
int g_timeCookTime = 0; /* Time is initialized to 0 min */
int g_timeCookedTime = 0; /* Cooked Time is initialized to 0 min */
int g_remoteSetStatus = 0; /* by default set the set status flag to 0*/
int g_updateInprogress = 0;
int g_remoteNotify=0;    /* To send the remote notification along with local notification */
int g_remoteAPNSNotify=0;    /* To send the remote APNS notification when cooking is completed */
int g_setStatusInProgress=0; /* To indicate set status is in progress */
int g_OffTimer=-1;	/* To switch off automatically after a specifid time */
short int g_SetStatefromPhone = 0; /* To indicate flag whether set from App or manually */

int initTheadFlag=0;  /* To indicate init thread is done */
int notificationFlag = 0; /* Data usage Flag */ 

ithread_t jarden_thread = -1;
ithread_t jarden_RemoteThread = -1;

static pthread_mutex_t   s_jarden_mutex;
static pthread_mutex_t   s_jarden_remote_mutex;

#endif /*PRODUCT_WeMo_Jarden*/

#ifdef PRODUCT_WeMo_Smart

#if 0
#define WARM_COOK_TIME_IN_MINUTES  (6*60)   //6hours default cook time for WARM once state changed from Phone

/* Global variable associated to Crockpot*/

int g_modeStatus = 0; /* initialized to 0, this may be diff from actual mode value */
int g_delayTime = 0; /* Time is initialized to 0 min */
int g_timeCookTime = 0; /* Time is initialized to 0 min */
int g_remoteSetStatus = 0; /* by default set the set status flag to 0*/
int g_updateInprogress = 0;
int g_remoteNotify=0;    /* To send the remote notification along with local notification */
int g_remoteAPNSNotify=0;    /* To send the remote APNS notification when cooking is completed */
int g_setStatusInProgress=0; /* To indicate set status is in progress */
int g_OffTimer=-1;	/* To switch off automatically after a specifid time */
short int g_SetStatefromPhone = 0; /* To indicate flag whether set from App or manually */
#endif
int initTheadFlag=0;  /* To indicate init thread is done */
int notificationFlag = 0; /* Data usage Flag */ 
int g_remoteNotify=0;    /* To send the remote notification along with local notification */
int g_remoteAPNSNotify=0;    /* To send the remote APNS notification when cooking is completed */

extern int g_notificationFlag; //notifcation check flag
extern int g_attributeFlag[100];

ithread_t smart_thread = -1;
ithread_t smart_RemoteThread = -1;

static pthread_mutex_t   s_smart_mutex;
static pthread_mutex_t   s_smart_remote_mutex;

extern WaspVarDesc **g_attrVarDescArray;
extern int g_attrCount;

#endif /*PRODUCT_WeMo_Smart*/

static pthread_mutex_t   s_led_mutex;
static pthread_mutex_t   s_sensor_mutex;



int g_isInsightRuleActivated = 0x00;


int g_IsLastUserActionOn = 0x00;

#define BUTTON_MONITOR_TIMEOUT    5	//secs
#define SENSOR_MONITOR_TIMEOUT    5	//secs

#if defined(LONG_PRESS_SUPPORTED)
#define LONG_PRESS_LOOP_CNT	40  //2secs
int gLongPressEnabled = 0x01;  //enabled by default
int gButtonLongPressed = 0x00;
int gLongPressTriggered = 0x00;
pthread_mutex_t gLongPressLock;

void initLongPressLock()
{
    osUtilsCreateLock(&gLongPressLock);
}

void LockLongPress()
{
    osUtilsGetLock(&gLongPressLock);
}

void UnlockLongPress()
{
    osUtilsReleaseLock(&gLongPressLock);
}
#endif

#ifndef PRODUCT_WeMo_LEDLight
void initLED()
{
	ithread_mutexattr_t attr;
    ithread_mutexattr_init(&attr);
    ithread_mutexattr_setkind_np( &attr, ITHREAD_MUTEX_RECURSIVE_NP );
    ithread_mutex_init(&s_led_mutex, &attr);
    ithread_mutexattr_destroy(&attr);
}
#endif //#ifndef PRODUCT_WeMo_LEDLight

void LockLED()
{
	ithread_mutex_lock(&s_led_mutex);

}

void UnlockLED()
{
	ithread_mutex_unlock(&s_led_mutex);
}


void LockSensor()
{
	ithread_mutex_lock(&s_sensor_mutex);

}

void UnlockSensor()
{
	ithread_mutex_unlock(&s_sensor_mutex);
}

void ToggleUpdate(int curState)
{
	LocalBinaryStateNotify(curState);

	//- Does not know myself has a sensor rule or not
#ifdef PRODUCT_WeMo_Insight
	if(curState == 0x08)
	    curState=0x01;
#endif
	LocalUserActionNotify(curState + 2);
	
}

void InternalLocalUpdate(int curState)
{
	pMessage msg = 0x00;
	
	if (0x00 == curState)
	{
		msg = createMessage(LOCAL_MESSAGE_OFF_IND, 0x00, 0x00);
	}
	else if (0x01 == curState)
	{
		msg = createMessage(LOCAL_MESSAGE_ON_IND, 0x00, 0x00);
	}

	SendMessage(PLUGIN_E_RELAY_THREAD, msg);
}

void InternalToggleUpdate(int curState)
{
	pMessage msg = 0x00;
	
	if (0x00 == curState)
	{
		msg = createMessage(BTN_MESSAGE_OFF_IND, 0x00, 0x00);
	}
	else if (0x01 == curState)
	{
		msg = createMessage(BTN_MESSAGE_ON_IND, 0x00, 0x00);
	}
#ifdef PRODUCT_WeMo_Insight
	else if (0x08 == curState)
	{
		msg = createMessage(BTN_MESSAGE_SBY_IND, 0x00, 0x00);
	}
#endif

	SendMessage(PLUGIN_E_RELAY_THREAD, msg);
}

extern void StopPowerMonitorTimer();
void togglePower()
{
	int curState = 0x00;
	APP_LOG ("Button", LOG_DEBUG, "######### g_PowerStatus ON/OFF VALUE : %d FROM BUTTON TASK", g_PowerStatus);
#if !defined(PRODUCT_WeMo_Insight) && !defined(PRODUCT_WeMo_LEDLight)
	/*check if it was running in last minute, if yes do not toggle relay, countdown timer restarted*/
	if(checkAndExecuteCountdownTimer(!g_PowerStatus))
		return;
#endif

	LockLED();
#ifdef PRODUCT_WeMo_Insight
		g_APNSLastState = g_PowerStatus;//used to restrict sending APNS in case of OFF->SBY and SBY->OFF
#endif
		g_PowerStatus = !g_PowerStatus;
		curState = g_PowerStatus;
		SetLastUserActionOnState(curState);
	UnlockLED();
#ifndef PRODUCT_WeMo_Insight
	setPower(curState);
#endif
#ifdef PRODUCT_WeMo_Insight
    int tempInsightState = curState;
	if(g_StateLog)
	   APP_LOG("Button", LOG_ALERT, "######### RELAY ON/OFF VALUE : %d FROM BUTTON TASK",curState);
	if(curState == POWER_ON){
	LockLED();
	    curState=POWER_SBY;
	    g_PowerStatus = POWER_SBY;
	UnlockLED();
	}

    /* check if it was running in last minute, if yes do not toggle relay, countdown timer restarted*/
    if (checkAndExecuteCountdownTimer (g_PowerStatus)) {}
    else
    {
        APP_LOG ("Button", LOG_DEBUG, "&&&&& tempInsightState = [%d]",
                 tempInsightState);
        setPower (tempInsightState);
        InternalToggleUpdate (tempInsightState);
    }
#endif
#ifndef PRODUCT_WeMo_Insight
	InternalToggleUpdate(curState);
#endif
}


void *LedAutoToggleLoop(void *args)
{
    static int counter = 0x00;

    while (1)
    {
	sleep(DELAY_5SEC);
	APP_LOG("Button",LOG_DEBUG, "Automatically toggle power");
	counter++;
	togglePower();

	APP_LOG("Button", LOG_DEBUG, "########## Try %d ON/OFF ############", counter);

	if (counter > 5000)
	    return NULL;
    }
}

void *PowerButtonTask(void *args)
{
    tu_set_my_thread_name( __FUNCTION__ );

    APP_LOG("Button", LOG_DEBUG, "##### button task running: %s ##############", szButtonPath);
#if defined(LONG_PRESS_SUPPORTED)
    int CounterForLongPress = 0;
#endif

    while (1)
    {
	FILE * pButtonFile = 0x00;
	int isToggled = 0x00;
	gButtonHealthPunch++;
	pButtonFile = fopen(szButtonPath, "r");
	if (pButtonFile == 0x00)
	{
	    APP_LOG("Socket Button:", LOG_DEBUG, "####### open sensor: %s error", szButtonPath);
	    return 0x00;
	}

	char szflag[SIZE_4B];
	memset(szflag, 0x00, sizeof(szflag));

	char* pResult = fgets(szflag, sizeof(szflag), pButtonFile);

	if (pResult != 0x00)
	{  
	    int command = BUTTON_RELEASED;

	    if (0x0 != strlen(szflag))
		command = atoi(szflag);

	    if (BUTTON_PRESSED == command)
	    {
#ifdef PRODUCT_WeMo_Insight
		if (g_isButtonPressed)
		{
		    //released
		    g_isButtonPressed = 0x00;
		    isToggled = 0x01;
		}
#else
		g_isButtonPressed = 0x01;
#if defined(LONG_PRESS_SUPPORTED)
		CounterForLongPress += 1;
		if (gLongPressEnabled && (LONG_PRESS_LOOP_CNT == CounterForLongPress)) //Long Press for 2.0 seconds detected
		{
		    APP_LOG("Button",LOG_DEBUG,"Long Press for 2.0 seconds detected... CounterForLongPress: %d", CounterForLongPress);
		    APP_LOG("APP", LOG_DEBUG, "-------->>>>> POWER_ON: ACTIVITY LED STATE 0 -------->>>>>");
#ifndef PRODUCT_WeMo_LEDLight			
		    SetActivityLED(0x00);
#endif			
		    LockLongPress();
		    gButtonLongPressed = 0x01;
		    gLongPressTriggered = 0x01;
		    UnlockLongPress();
		    APP_LOG("Button",LOG_DEBUG,"set gButtonLongPressed: %d and gLongPressTriggered: %d", gButtonLongPressed, gLongPressTriggered);
		    remoteAccessUpdateStatusTSParams (0xFF);	//notify to cloud
		}
#endif
#endif
	    }
	    else
	    {
#ifdef PRODUCT_WeMo_Insight
		g_isButtonPressed = 0x01;
#else
		if (g_isButtonPressed)
		{
#if defined(LONG_PRESS_SUPPORTED)
		    CounterForLongPress = 0;
		    if(gLongPressEnabled && gButtonLongPressed)
		    {
			//released
			g_isButtonPressed = 0x00;
			APP_LOG("APP", LOG_DEBUG, "-------->>>>> POWER_ON: ACTIVITY LED STATE 1 -------->>>>>");
#ifndef PRODUCT_WeMo_LEDLight						
			SetActivityLED(0x01);
#endif			
			if(g_PowerStatus)
			{
			    usleep(50000 * 26);	//1.3 secs
			    system("echo 0 > /proc/GPIO9"); //to keep LED ON, if relay state is ON
			}
			LockLongPress();
			gButtonLongPressed = 0x00;
			UnlockLongPress();
			APP_LOG("Button",LOG_DEBUG,"set gButtonLongPressed to: %d", gButtonLongPressed);
		    }
		    else
		    {
			//released
			g_isButtonPressed = 0x00;
			isToggled = 0x01;
		    }
#else
		    //released
		    g_isButtonPressed = 0x00;
		    isToggled = 0x01;
#endif
		}
#endif
	    }
	}

	fclose(pButtonFile);
	if (isToggled)
	{
	    setActuation(ACTUATION_MANUAL_DEVICE);
	    togglePower();
	}

	usleep(50000);
	isToggled = 0x00;
    }

}

void *ButtonTaskMonitorThread(void *arg)
{
    	tu_set_my_thread_name( __FUNCTION__ );

	APP_LOG("ButtonMonitor",LOG_CRIT,"ButtonTaskMonitorThread  started...");

	while(1)
	{
		pluginUsleep(BUTTON_MONITOR_TIMEOUT * 1000000);
		if(gButtonHealthPunch == 0)
		{	
			APP_LOG("ButtonMonitor",LOG_CRIT,"ButtonTaskMonitorThread detected bad health ...");
			if(power_thread)
				pthread_kill(power_thread, SIGRTMIN);
			power_thread = -1;

			APP_LOG("ButtonMonitor",LOG_DEBUG,"Removed PowerButtonTask thread...");
		
			//Again create a PowerButtonTask thread
			ithread_create(&power_thread, NULL, PowerButtonTask, NULL);
			ithread_detach (power_thread);

			APP_LOG("ButtonMonitor",LOG_CRIT,"PowerButtonTask thread created again...");

		}
		else
		{
			//APP_LOG("ButtonMonitor",LOG_DEBUG,"Button Task thread health OK [%d]...", gButtonHealthPunch);
			gButtonHealthPunch = 0;
		}
	}
	return NULL;	
}

void *SensorTaskMonitorThread(void *arg)
{
    	tu_set_my_thread_name( __FUNCTION__ );

	APP_LOG("SensorMonitor",LOG_CRIT,"SensorTaskMonitorThread  started...");

	while(1)
	{
		pluginUsleep(SENSOR_MONITOR_TIMEOUT * 1000000);
		if(gSensorHealthPunch == 0)
		{	
			APP_LOG("SensorMonitor",LOG_CRIT,"SensorGPIOTaskMonitorThread detected bad health ...");
			if(sensor_thread)
				pthread_kill(sensor_thread, SIGRTMIN);
			sensor_thread = -1;

			APP_LOG("SensorMonitor",LOG_DEBUG,"Removed SensorGPIOTask thread...");
		
			//Again create a PowerSensorTask thread
			ithread_create(&sensor_thread, NULL, sensorGPIOTask, NULL);
			ithread_detach (sensor_thread);

			APP_LOG("SensorMonitor",LOG_CRIT,"SensorGPIOTask thread created again...");

		}
		else
		{
			//APP_LOG("SensorMonitor",LOG_DEBUG,"Sensor Task thread health OK [%d]...", gSensorHealthPunch);
			gSensorHealthPunch = 0;
		}
	}
	return NULL;	
}



void AyncUPnPNotify(int msgID)
{
    pMessage msg = 0x00;

    msg = createMessage(msgID, 0x00, 0x00);

    SendMessage2App(msg);
}

int ProcessRelayEvent(pNode node)
{
    if (0x00 == node)
	return 0x01;

    if (0x00 == node->message)
	return 0x01;

    switch(node->message->ID)
    {

	case BTN_MESSAGE_ON_IND:
	    APP_LOG("ITC:LED", LOG_DEBUG, "BTN_MESSAGE_ON_IND");
	    ToggleUpdate(0x01);
#ifdef SIMULATED_OCCUPANCY
	    if( (gRuleHandle[e_AWAY_RULE].ruleCnt && (gpSimulatedDevice && gpSimulatedDevice->ruleEndTime))
		    && (DEVICE_INSIGHT != g_eDeviceTypeTemp) ) 
	    {
		APP_LOG("ITC:LED", LOG_DEBUG, "simulated rule: manual toggle, BTN_MESSAGE_ON_IND");
		notifyManualToggle();
	    }
#endif  
	    break;
	case BTN_MESSAGE_OFF_IND:
	    APP_LOG("ITC:LED", LOG_DEBUG, "BTN_MESSAGE_OFF_IND");
	    ToggleUpdate(0x00);
#ifdef SIMULATED_OCCUPANCY
	    if(gRuleHandle[e_AWAY_RULE].ruleCnt && (gpSimulatedDevice && gpSimulatedDevice->ruleEndTime))
	    {
		APP_LOG("ITC:LED", LOG_DEBUG, "simulated rule: manual toggle, BTN_MESSAGE_OFF_IND");
		notifyManualToggle();
	    }
#endif 
#ifdef PRODUCT_WeMo_Insight
	    if(POWER_SBY != g_APNSLastState)
		    processInsightNotification(E_STATE, 0x00);
#endif
	    break;
	case UPNP_MESSAGE_ON_IND:
	    AyncUPnPNotify(UPNP_MESSAGE_ON_IND);
	    APP_LOG("ITC:LED", LOG_DEBUG, "UPNP_MESSAGE_ON_IND");
	    break;
	case UPNP_MESSAGE_OFF_IND:
	    AyncUPnPNotify(UPNP_MESSAGE_OFF_IND);
	    APP_LOG("ITC:LED", LOG_DEBUG, "UPNP_MESSAGE_OFF_IND");
	    break;
	case RULE_MESSAGE_OFF_IND:
	    APP_LOG("ITC:LED", LOG_DEBUG, "RULE_MESSAGE_OFF_IND");
	    LocalBinaryStateNotify(0x00);
#ifdef PRODUCT_WeMo_Insight
	    if(POWER_SBY != g_APNSLastState)
		    processInsightNotification(E_STATE, 0x00);
#endif
	    break;
	case RULE_MESSAGE_ON_IND:
	    APP_LOG("ITC:LED", LOG_DEBUG, "RULE_MESSAGE_ON_IND");	
	    LocalBinaryStateNotify(0x01);
	    break;
	case META_FULL_RESET:
	    APP_LOG("ITC:LED", LOG_DEBUG, "META_FULL_RESET");	
	    ControlleeDeviceStop();
	    break;
#ifdef PRODUCT_WeMo_Insight
	case UPNP_MESSAGE_SBY_IND:
	    AyncUPnPNotify(UPNP_MESSAGE_SBY_IND);
	    APP_LOG("ITC: LED", LOG_DEBUG, "UPNP_MESSAGE_SBY_IND");
	    break;
	case BTN_MESSAGE_SBY_IND:
	    APP_LOG("ITC:LED", LOG_DEBUG, "BTN_MESSAGE_SBY_IND");
	    ToggleUpdate(0x08);
#ifdef SIMULATED_OCCUPANCY
	    if(gRuleHandle[e_AWAY_RULE].ruleCnt && (gpSimulatedDevice && gpSimulatedDevice->ruleEndTime))
	    {
		APP_LOG("ITC:LED", LOG_DEBUG, "simulated rule: manual toggle, BTN_MESSAGE_SBY_IND");
		notifyManualToggle();
	    }
#endif  
	    break;
#endif
	default:
	    break;
    }

    if (0x00 != node->message->message)
	free(node->message->message);
    free(node->message);
    free(node);
    return 0;
}

void *RelayControlTask(void *args)
{
    pNode node = 0x00;
    tu_set_my_thread_name( __FUNCTION__ );

    APP_LOG("Relay", LOG_CRIT, "####### relay task running #######");

    while(1)
    {
	node = readMessage(PLUGIN_E_RELAY_THREAD);
	ProcessRelayEvent(node);	
    }
    return NULL;
}



void *sensorGPIOTask(void *args)
{   
    tu_set_my_thread_name( __FUNCTION__ );

    APP_LOG("Sensor", LOG_CRIT, "Sensor task running");

    while (1)
    {
	FILE * pSensorFile = 0x00;
	gSensorHealthPunch++;

	pSensorFile = fopen(szNewSensorPath, "r");

	if (pSensorFile == 0x00)
	{
	    APP_LOG("Sensor", LOG_CRIT, "Open sensor: %s error", szNewSensorPath);

	    return 0x00;
	}

	char szflag[SIZE_128B];
	char* pResult = fgets(szflag, sizeof(szflag), pSensorFile);

	if (pResult != 0x00)
	{
	    int command = atoi(szflag);

	    //-Get now time
	    struct timeval tv;
	    time_t curTime;
	    gettimeofday(&tv, NULL); 
	    curTime = tv.tv_sec;

	    if (MOTION_SENSORED == command)
	    {	
		if (curTime - s_lLastSensoringUpdateTime >= MAX_SENSOR_MSG_TIMEOUT)
		{
		    //- Add one more trace here indicating system status
		    APP_LOG("Sensor", LOG_DEBUG, "Sensor status changed, to notify");
		    MotionSensorInd();
		    s_lLastSensoringUpdateTime 		= curTime;
		    s_lLastNoSensoringUpdateTime 	= 0x00;
		    LockSensor();
		    g_SensorStatus = 0x01;
		    UnlockSensor();
		}
	    }
	    else
	    {
		if (curTime - s_lLastNoSensoringUpdateTime >= MAX_SENSOR_MSG_TIMEOUT)
		{
		    NoMotionSensorInd();	
		    s_lLastNoSensoringUpdateTime = curTime;
		    s_lLastSensoringUpdateTime	 = 0x00;
		    LockSensor();
		    g_SensorStatus = 0x00;
		    UnlockSensor();
		}
	    }
	}

	fclose(pSensorFile);

	sleep(DELAY_1SEC);
    }

    return NULL;
}


int setPower(int command)
{
	FILE* pfRelay = 0x00;
	FILE* pfLed   = 0x00;
#if defined(PRODUCT_WeMo_Light) 
	FILE *pfNLed = 0x00;
#endif
	pfRelay = fopen(RELAY_GPIO, "w");
	if (0x00 == pfRelay)
	{
		APP_LOG("GPIO", LOG_ERR, "######## relay open failure #######");
		return 0x01;
	}

	pfLed   = fopen(LED_GPIO, "w");
	if (0x00 == pfLed)
	{
		APP_LOG("GPIO", LOG_ERR, "######## led open failure #######");
		fclose(pfRelay);
		return 0x01;
	}
#if defined(PRODUCT_WeMo_Light) 
	pfNLed   = fopen(NIGHT_LED_GPIO, "w");
	if (0x00 == pfNLed)
	{
		APP_LOG("GPIO", LOG_ERR, "######## Night led open failure #######");
		fclose(pfRelay);
		fclose(pfLed);
		return 0x01;
	}
#endif

    if (POWER_ON == command)
    {
		//- Set to ON
		fputs(RELAY_ON, pfRelay);
		fputs(RELAY_LED_ON, pfLed);
#if defined(PRODUCT_WeMo_Light) 
	if((u32DimVal == 0) || (u32DimVal == 2)){
		fputs(NIGHTLIGHT_LED_OFF, pfNLed);
		APP_LOG("GPIO", LOG_ERR, "######## Night led OFF #######");
	}
#endif
#ifdef SIMULATED_OCCUPANCY
	if(gRuleHandle[e_AWAY_RULE].ruleCnt && (gpSimulatedDevice && 
		    gpSimulatedDevice->ruleEndTime))
	{
	    time_t curTime;
	    curTime = (int) GetUTCTime();
	    gpSimulatedDevice->onTS = curTime;
	    APP_LOG("GPIO", LOG_DEBUG, "######## Simulated device self ON TS: %d #######", gpSimulatedDevice->onTS);
	}
#endif
    }
    else if (POWER_OFF == command)
    {
		//- Set to OFF
    	fputs(RELAY_OFF, pfRelay);
	fputs(RELAY_LED_OFF, pfLed);
#if defined(PRODUCT_WeMo_Light) 
	if(u32DimVal == 0){
	  APP_LOG("GPIO", LOG_ERR, "######## Night led ON #######");
	  fputs(NIGHTLIGHT_LED_ON, pfNLed);
	}
	else if(u32DimVal == 2){
		fputs(NIGHTLIGHT_LED_OFF, pfNLed);
		APP_LOG("GPIO", LOG_ERR, "######## Night led OFF #######");
	}
#endif
    }

	
	//- Close handle
	fclose(pfRelay);
	fclose(pfLed);
#if defined(PRODUCT_WeMo_Light) 
	fclose(pfNLed);
#endif
    return 0x00;
}

int setLED(LED_ID id, BYTE OnDuration, BYTE OffDuration, int counter)
{
    if ( IVALID_THREAD_HANDLE != led_thread)
    {
        int rect = ithread_cancel(led_thread);
        led_thread = IVALID_THREAD_HANDLE;
        if (0x00 != rect)
        {
            APP_LOG("DeviceControl",LOG_ERR, "LED thread cancellation failure\n");
        }
    }

    return 0x00;
}


int SaveDeviceConfig(const char*  szKey, const char* szValue)
{
  SetBelkinParameter((char *)szKey, (char *)szValue);
  SaveSetting();  
  return 0x00;
}

/**************************************************************
 * GetDeviceConfig:
 * 	Call file system API to get value of key
 * 
 * 
 * 
 * 
 * ************************************************************/
char* GetDeviceConfig(const char*  szKey)
{
  return GetBelkinParameter((char *)szKey);
}

int GetCurBinaryState()
{
  return g_PowerStatus;
}


void SetCurBinaryState(int toState)
{
  g_PowerStatus = toState;
  SetLastUserActionOnState(g_PowerStatus);
}


void SetSensorConfig(int  delay, int sensitivity)
{
	#ifdef NEW_BOARD
		g_cntSensorDelay = delay;
		g_cntSensitivity = sensitivity;
		SetMotionSensorDelay(g_cntSensorDelay, g_cntSensitivity);
	#endif
}




void StartSensorTask()
{
	//-Double check
	initSensor();
	
	if (-1 != sensor_thread)
	{
		ithread_cancel(sensor_thread);
		sensor_thread = -1;
	}

	#ifdef NEW_BOARD
	EnableMotionSensorDetect(0x01);
	SetMotionSensorDelay(g_cntSensorDelay, g_cntSensitivity);
	#endif
	
	ithread_create(&sensor_thread, NULL, sensorGPIOTask, NULL);

	if (-1 == sensor_thread)
	{
		//- If can not be created, it is lik DEAD for sensor, so reboot again
		//- Though this is almost "impossible", but to ensure
		APP_LOG("Sensor",LOG_ERR, "@@@@@@@@@@@@@@@@@@@ Sensor task can not be created, this should not happen @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
		APP_LOG("Sensor",LOG_CRIT, "Sensor task can not be created, this should not happen");
	}
	else
	{
		//- success
		ithread_detach(sensor_thread);
	}
}

void StopSensorTask()
{
	#ifdef NEW_BOARD
		EnableMotionSensorDetect(0x00);
	#endif
    if (-1 != sensor_thread)
    {
		ithread_cancel(sensor_thread);
		sensor_thread = -1;
    }

}



int ChangeBinaryState(int newState)
{
	int ret = 0x01;
	LockLED();
	int curState = 0x00;
	curState = g_PowerStatus;

	APP_LOG ("DeviceControl", LOG_DEBUG, "In ChangeBinaryState");

	if ((0x00 == newState) || (0x01 == newState))
	{	
	    APP_LOG("DeviceControl", LOG_DEBUG, "------------->>>>>>curState:%d newState:%d",curState, newState);
	    if (curState != newState)
	    {
#ifdef PRODUCT_WeMo_Insight
		if(newState == POWER_ON){
		    g_PowerStatus = POWER_SBY;
		    g_APNSLastState = curState;//used to restrict sending APNS in case of OFF->SBY and SBY->OFF
		}
		else{
		    g_PowerStatus = POWER_OFF;
		    g_APNSLastState = curState;//used to restrict sending APNS in case of OFF->SBY and SBY->OFF
		}
#else
		    g_PowerStatus = newState;
#endif
		ret = 0x00;		
	    }
	}
	else
	{
		APP_LOG("DeviceControl", LOG_DEBUG, "state request incorrect:%d", newState);
	}

	UnlockLED();
	if (0x00 == ret)
		setPower(newState);
	
	APP_LOG ("DeviceControl", LOG_DEBUG, "Exiting ChangeBinaryState");

	return ret;
}


void initSensor()
{
	ithread_mutexattr_t attr;
    ithread_mutexattr_init(&attr);
    ithread_mutexattr_setkind_np( &attr, ITHREAD_MUTEX_RECURSIVE_NP );
    ithread_mutex_init(&s_sensor_mutex, &attr);
    ithread_mutexattr_destroy(&attr);

	APP_LOG("DeviceControl", LOG_CRIT, "sensor resource initialized with success");

}


int GetSensorState()
{
	return g_SensorStatus;
}

/**
 *	Reset sensor configuration to default
 *
 *
 *
 */
void ResetSensor2Default()
{
	if (DEVICE_SENSOR == g_eDeviceType)
	{
		//- If sensor, applied
		g_cntSensitivity = DEFAULT_SENSOR_SENSITIVITY;
		g_cntSensorDelay = DEFAULT_SENSOR_DELAY;
		SetSensorConfig(g_cntSensorDelay, g_cntSensitivity);
		APP_LOG("Sensor", LOG_DEBUG, "delay:%d, densitivity:%d", g_cntSensorDelay, g_cntSensitivity);
	}
}

#ifdef PRODUCT_WeMo_Jarden
/*
 * jarden specific code
 */

void initJarden()
{
    if(initTheadFlag == 0) {
        ithread_mutexattr_t attr;
        ithread_mutexattr_init(&attr);
        ithread_mutexattr_setkind_np( &attr, ITHREAD_MUTEX_RECURSIVE_NP );
        ithread_mutex_init(&s_jarden_mutex, &attr);
        ithread_mutexattr_destroy(&attr);
        initTheadFlag = 1;
    }
}

void LockJarden()
{
	ithread_mutex_lock(&s_jarden_mutex);
}

void UnlockJarden()
{
	ithread_mutex_unlock(&s_jarden_mutex);
}

void LockJardenRemote()
{
	ithread_mutex_lock(&s_jarden_remote_mutex);
}

void UnlockJardenRemote()
{
	ithread_mutex_unlock(&s_jarden_remote_mutex);
}

/* Task to identify the status change in jarden device */
void *jardenPollTask(void *args)
{
	//struct timeval tv;
	int timeCnt=0;
	int curTime=0,prevTime=0,diffTime=0;
	int curMode=0, prevMode=0,newMode;
	unsigned short int cookedTime = 0;
	int statusChange = 0;
	int init=0,allowSetTime=FALSE;
	int TimeChanged_APNSNotify=0;
	int ModeChanged_APNSNotify=0;

	int setStatusFlag=0;
    int tmpMode=0,tmpCookTime=0,tmpDelayTime=0;
    int retStatus, setInProgress=0;
	int tmpWait=0;
	WaspVariable WaspMode, WaspCookTime, WaspCookedTime;
	int Err;
	char buff[SIZE_64B];

    tu_set_my_thread_name( __FUNCTION__ );

	APP_LOG("Jarden Device", LOG_DEBUG, "Jarden device task running");

	while (1)
	{
		if(DEVICE_CROCKPOT == g_eDeviceType)
		{
			//if(g_updateInprogress == 0)
			{
				//-Get the current time
				prevTime = curTime;
				memset(&WaspCookTime,0,sizeof(WaspVariable));
				WaspCookTime.ID = CROCKPOT_VAR_COOKTIME;
				WaspCookTime.Type = WASP_VARTYPE_TIME_M16;
				if((Err = WASP_GetVariable(&WaspCookTime)) != WASP_OK) 
				{
					if(Err >= 0) {
						Err = -Err;
					}
					curTime = Err;
				}
				else 
				{
					if(WaspCookTime.ID != CROCKPOT_VAR_COOKTIME) {
						APP_LOG("Crockpot Error", LOG_DEBUG, "requested variable ID %d got %d",CROCKPOT_VAR_COOKTIME,WaspCookTime.ID);
					}
					curTime = WaspCookTime.Val.U16;
				}

				//APP_LOG("Crockpot", LOG_DEBUG, "Received WASP Time %d,  Crockpot's time changed to %d",WaspCookTime.Val.I32, curTime);
				//curTime = HAL_GetCookTime();

				/* If the time change is beyond 1min, means it is a manual time change, not the timer decrement
				 * Timer decrement is in steps of 1min, hence even manually 1min is changed, that will not considered
				 * as status change.
				 */
				if(curTime > prevTime){
					diffTime = curTime-prevTime;
				}
				else {
					diffTime = prevTime-curTime;
				}
				if(diffTime > 1) {
					statusChange = 1;
					APP_LOG("Crockpot", LOG_DEBUG, "Crockpot time changed by user to %d", curTime);
				}

				if((curTime == WARM_COOK_TIME_IN_MINUTES) && 
				   (curTime != prevTime))
				{
					TimeChanged_APNSNotify = 1;  
				}
				
				//-Get the current cooked time
				memset(&WaspCookedTime,0,sizeof(WaspVariable));
				WaspCookedTime.ID = CROCKPOT_VAR_COOKEDTIME;
				WaspCookedTime.Type = WASP_VARTYPE_TIME_M16;
				if((Err = WASP_GetVariable(&WaspCookedTime)) != WASP_OK) 
				{
					if(Err >= 0) {
						Err = -Err;
					}
					cookedTime = Err;
				}
				else 
				{
					if(WaspCookedTime.ID != CROCKPOT_VAR_COOKEDTIME) {
						APP_LOG("Crockpot Error", LOG_DEBUG, "requested variable ID %d got %d",CROCKPOT_VAR_COOKEDTIME,WaspCookedTime.ID);
					}
					cookedTime = WaspCookedTime.Val.U16;
				}
				
				prevMode = curMode;
				memset(&WaspMode,0,sizeof(WaspVariable));
				WaspMode.ID = CROCKPOT_VAR_MODE;
				WaspMode.State = VAR_VALUE_LIVE;
				if((Err = WASP_GetVariable(&WaspMode)) != WASP_OK) 
				{
					if(Err >= 0) {
						Err = -Err;
					}
					curMode = Err;
				}
				else 
				{
					if(WaspMode.ID != CROCKPOT_VAR_MODE) 
					{
						APP_LOG("Crockpot Error", LOG_DEBUG, "requested variable ID %d got %d",CROCKPOT_VAR_MODE,WaspMode.ID);
					}
					curMode = convertModeValue(WaspMode.Val.Enum, FROM_CROCKPOT);
				}
				//APP_LOG("Crockpot", LOG_DEBUG, "WASP mode is %d, Crockpot mode changed to %d",WaspMode.Val.Enum, curMode);
				//curMode = HAL_GetMode();

				if((curMode == APP_MODE_WARM) && 
				   (curMode != prevMode))
				{
					ModeChanged_APNSNotify = 1;  
				}
				
				/* to Send APNS Push Notification, some times time change and mode change are not coming synchronously
				 * so changed condition to meet the criteria.				
				*/
				if((1 == ModeChanged_APNSNotify) && 
				   (1 == TimeChanged_APNSNotify) &&
				   (1 == g_SetStatefromPhone)
				  )
				{		
					TimeChanged_APNSNotify = 0;
					ModeChanged_APNSNotify = 0;
					g_SetStatefromPhone = 0;
					APP_LOG("Jarden Device", LOG_DEBUG, "Inside Jarden Push Notification");
					LockJardenRemote();
					g_remoteAPNSNotify = 1;
					notificationFlag = 1; //For Data Usage
					UnlockJardenRemote();
					//remoteAccessUpdateStatusTSParams (0xFF);
				}

				/* Any change in the mode is a status change
				 */
				if(curMode != prevMode) 
				{
					statusChange = 1;
					APP_LOG("Crockpot", LOG_DEBUG, "Crockpot mode changed to %d",WaspMode.Val.Enum);
					
					//if (getCurrentClientState() == STATE_CONNECTED)
					if(GetUTCTime() > 1300000000)		
					{
						if(prevMode == APP_MODE_OFF)
						{
							LockJarden();
							g_poweronStartTime = GetUTCTime();
							UnlockJarden();
							// Store Start Time Value to NVRAM to retain if firmware reboots by any chance, so that app will not get 0
							memset(buff, 0x0, SIZE_64B);
							snprintf(buff, SIZE_64B, "%lu", g_poweronStartTime);
							SetBelkinParameter(SLOW_COOKER_START_TIME, buff);
							SaveSetting();
							// APP_LOG("Crockpot", LOG_DEBUG, "Crockpot prev mode after change is %d, current mode is %d",prevMode, curMode);	

						}

						/* To ensure that DataUsageTableUpdate(0) is called only when there is manual mode change. 
						   If mode changed by phone, DataUsageTableUpdate(1) is  called right after wasp_setvariable is called. 
						   This check is put here to prevent double entries of data when mode set by phone */
						if(g_phoneFlag != 1 && g_timerFlag == 0)  
						{
							APP_LOG("Crockpot", LOG_DEBUG, "Inside GPIO DataUsageTableUpdate(0)");
							DataUsageTableUpdate(0); //Added for data usage implementation. define enums
						}
						else if(g_phoneFlag != 1 && g_timerFlag == 1 && notificationFlag == 1)  
						{
							APP_LOG("Crockpot", LOG_DEBUG, "Inside GPIO DataUsageTableUpdate(1))");
							DataUsageTableUpdate(2); //Added for data usage implementation for phone mode entry  
							g_timerFlag = 0; 
							notificationFlag =0;	
						}
						else
						{
							g_phoneFlag = 0; 
						}
					}
				}
				
				//APP_LOG("Crockpot", LOG_DEBUG, "Crockpot prev mode is %d, current mode is %d",prevMode, curMode);	
				/* If the status is changed, notify the app through UPnP push notification */
				if(1 == statusChange)
				{

					LockJarden();
					setInProgress = g_setStatusInProgress;
					UnlockJarden();
					if(setInProgress == 0) {
						APP_LOG("Crockpot", LOG_DEBUG, "Crockpot notify: status changed");
						jardenStatusChange(curMode, curTime, cookedTime);
						statusChange = 0; /* Once notified, reset the status change flag */
						timeCnt = 0; /* If any change is notified, reset the 15min timer as well */
						init = 0;
						LockJardenRemote();
						if(g_remoteNotify == 0) {
							g_remoteNotify = 1;
						}
						UnlockJardenRemote();

						LockJarden();
						g_modeStatus = curMode;
						g_timeCookTime = curTime;
						g_timeCookedTime = cookedTime;
						UnlockJarden();
					}
				}
			}
		}
		else if ( (g_eDeviceType == DEVICE_SBIRON) ||
				(g_eDeviceType == DEVICE_MRCOFFEE) ||
				(g_eDeviceType == DEVICE_PETFEEDER) 
			)
		{
			if(g_OffTimer == 0) {
				setStatusFlag = ChangeBinaryState(0); //HAL_SetMode(MODE_OFF);
				if(setStatusFlag < 0) {
					APP_LOG("Jarden Device", LOG_DEBUG, "Jarden Device mode change failed");
				}
				else {
					APP_LOG("Jarden Device", LOG_DEBUG, "Jarden Device mode changed to OFF");
				}
				g_OffTimer = -1;
			}
			/*else {
			  if(g_OffTimer >= 0)
			  APP_LOG("Jarden Device", LOG_DEBUG, "Delay timer value: %d\n",g_OffTimer);
			  }*/

			//if(g_updateInprogress == 0)
			{
				prevMode = curMode;
				//curMode = HAL_GetMode();
				LockLED();
				curMode = GetCurBinaryState();
				UnlockLED();

				//APP_LOG("Jarden Device", LOG_DEBUG, "Current mode curMode is %d", curMode);

				//curTime = HAL_GetDelayTime();
				/* Any change in the mode is a status change */
				if(curMode != prevMode) {
					statusChange = 1;
					APP_LOG("Jarden Device", LOG_DEBUG, "Jarden Device mode changed to %d",curMode);
				}

				/* If the status is changed, notify the app through UPnP push notification */
				if(1 == statusChange)
				{
					//APP_LOG("Jarden Device", LOG_DEBUG, "Jarden Device notify: status changed");
					//if(MODE_ON == curMode) {
					if(1 == curMode) {
						APP_LOG("Jarden Device", LOG_DEBUG, "############# Jarden Device status changed to ON ###########");
						if(g_OffTimer <= 0) {
							if(DEVICE_PETFEEDER == g_eDeviceType) {
								g_OffTimer = (ONE_SECOND * 10); /* 10 seconds */
							}
							else {
								g_OffTimer = -1;
							}
						}
					}

					APP_LOG("Jarden Device", LOG_DEBUG, "Jarden Device notify: status changed");
					jardenStatusChange(curMode, curTime, cookedTime);
					statusChange = 0; /* Once notified, reset the status change flag */
					timeCnt = 0; /* If any change is notified, reset the 15min timer as well */
					init = 0;
					LockJardenRemote();
					if(g_remoteNotify == 0) {
						g_remoteNotify = 1;
					}
					UnlockJardenRemote();

					LockJarden();
					g_modeStatus = curMode;
					g_delayTime = curTime;
					g_timeCookedTime = cookedTime;
					UnlockJarden();
				}
				}
			} 
			  else
			  {
				  /* WeMo Smart, Do Nothing */
			  }
			  /* end of g_updateInprogress if condition */

			  /* Loop is running every 500msec(0.5 sec) once, hence timeCnt=1800 means 15Min once
			   * Notify the app with the current mode & time. Time is important for app sync
			   */
			  if(PUSH_INFO_TIME == timeCnt) {
				  APP_LOG("Jarden Device", LOG_DEBUG, "Jarden Device notify: regular");
				  jardenStatusChange(curMode,curTime, cookedTime);
				  timeCnt = 0; /* Once it is notfied reset the timer */
				  init = 0;    /* To update the global status - used for cloud update */
				  LockJardenRemote();
				  if(g_remoteNotify == 0) {
					  g_remoteNotify = 1;
				  }
				  UnlockJardenRemote();
			  }
#if 1
			  {
				  //APP_LOG("Jarden Device", LOG_DEBUG, "JARDEN DEVICE THREAD: Mode = %d, Time = %d,set?=%d",tmpMode,tmpCookTime,setStatusFlag);
				  if(DEVICE_CROCKPOT == g_eDeviceType)
				  {
					  setStatusFlag = g_remoteSetStatus;

					  LockJarden();
					  tmpMode = g_modeStatus;
					  tmpCookTime = g_timeCookTime;
					  UnlockJarden();

					  if(setStatusFlag == 1) 
					  {
						  //retStatus = SetCrockpotStatus(tmpMode,tmpCookTime);

						  APP_LOG("REMOTE",LOG_DEBUG, "Requested Crockpot Mode to: %d\n", tmpMode);
						  /* You can't set to IDLE state */
						  if(MODE_UNKNOWN != tmpMode) {
							  if(APP_MODE_ERROR != tmpMode) {
								  retStatus =0;
								  memset(&WaspMode,0,sizeof(WaspVariable));
								  WaspMode.ID = CROCKPOT_VAR_MODE;
								  WaspMode.Type = WASP_ENUM;
								  WaspMode.Val.Enum = convertModeValue(tmpMode,TO_CROCKPOT);
								  if((Err = WASP_SetVariable(&WaspMode)) != WASP_OK) {
									  if(Err >= 0) {
										  Err = -Err;
									  }
									  retStatus = Err;
								  }
								  //retStatus = HAL_SetMode(tmpMode);
								  if(retStatus >= 0) {
									  APP_LOG("REMOTE",LOG_DEBUG, "Crockpot Mode set to: %d\n", tmpMode);
									  g_phoneFlag = 1;
									  
									  if((tmpMode == APP_MODE_HIGH) || (tmpMode == APP_MODE_LOW))
									  {
										 g_SetStatefromPhone = 1;
									  }
									  else
									  {
										 g_SetStatefromPhone = 0;
									  }
									  //DataUsageTableUpdate(1);   Piyush Check
								  }
								  else {
									  APP_LOG("REMOTE",LOG_DEBUG, "Unable to Set Crockpot Mode to: %d, ERROR:%d\n", tmpMode,retStatus);
								  }
							  } /* if(APP_MODE_ERROR... */
						  } /* end of if(tmpMode== ... */

						  /* Wait for a sec before setting the time */
						  //usleep(FIVE_HUNDRED_MSEC*2);

						  /* As per new requirements, time set can be performed even in Warm state and shoudnt send time value
						   * to WASP if we receive 0 
						   */
						  if(tmpCookTime != MAX_COOK_TIME) {
							  allowSetTime = TRUE;
							  //g_timerFlag = 1;
						  }
						  else 
						  {
							  allowSetTime = FALSE;
							  //g_timerFlag = 0;
						  }


						  if((APP_TIME_ERROR != tmpCookTime) && (TRUE==allowSetTime)) {
							  /* Set the time only if desired, otherwise don't do set */
							  APP_LOG("REMOTE",LOG_DEBUG, "Requested Crockpot time to: %d\n", tmpCookTime);
							  retStatus = 0;
							  do 
							  {
								  if(tmpCookTime < 0 || tmpCookTime > MAX_COOK_TIME) {
									  retStatus = -EINVAL;
									  break;
								  }

								  int CookTime = tmpCookTime;
								  memset(&WaspCookTime,0,sizeof(WaspVariable));
								  WaspCookTime.ID = CROCKPOT_VAR_COOKTIME;
								  WaspCookTime.Type = WASP_VARTYPE_TIME_M16;
								  WaspCookTime.Val.U16 = CookTime;
								  if((retStatus = WASP_SetVariable(&WaspCookTime)) != WASP_OK) {
									  if(retStatus >= 0) {
										  retStatus = -retStatus;
									  }
								  }
							  } while(FALSE);
							  //retStatus = HAL_SetCookTime(tmpCookTime);
							  if(retStatus >= 0) {
								  APP_LOG("REMOTE",LOG_DEBUG, "Crockpot time set to: %d\n", tmpCookTime);
                                                                  g_timerFlag = 1;
							  }
							  else {
								  APP_LOG("REMOTE",LOG_DEBUG, "Unable to Set Crockpot Cooktime to: %d, ERROR: %d\n", tmpCookTime,retStatus);
							  }
						  } /* end of if(APP_TIME_ERROR... */
                          
						  if(g_phoneFlag == 1 && g_timerFlag == 1)
						  {
							DataUsageTableUpdate(2); 
						  }
						  else if (g_phoneFlag == 1 && g_timerFlag != 1)
						  {
						        DataUsageTableUpdate(1); 
						  }

						  APP_LOG("Crockpot", LOG_DEBUG, "CROCKPOT THREAD: Crockpot remote status update done");
						  //LockCrockpot();
						  g_remoteSetStatus = 0;
						  //UnlockCrockpot();
					  } /* if(setStatusFlag... */
				  }  /* if(DEVICE_CROCKPOT == g_eDeviceType) */
				  else
				  {
					  setStatusFlag = g_remoteSetStatus;

					  LockJarden();
					  tmpMode = g_modeStatus;
					  tmpDelayTime = g_delayTime;
					  UnlockJarden();

					  if(setStatusFlag == 1) {
						  //retStatus = SetJardenStatus(tmpMode,tmpCookTime);

						  /* Mode has to be valid to set */
						  if(MODE_UNKNOWN != tmpMode) 
						  {
							  //retStatus = HAL_SetMode(tmpMode);
							  retStatus = ChangeBinaryState(tmpMode);
							  if(retStatus >= 0) {
								  APP_LOG("REMOTE",LOG_DEBUG, "Jarden Device Mode set to: %d\n", tmpMode);
							  }
							  else {
								  APP_LOG("REMOTE",LOG_DEBUG, "Unable to Set Jarden Device Mode to: %d, ERROR:%d\n", tmpMode,retStatus);
							  }
						  } /* end of if(tmpMode== ... */

						  /* Wait for a sec before setting the time */
						  //usleep(FIVE_HUNDRED_MSEC*2);

						  if(tmpDelayTime > 0 && tmpDelayTime <= MAX_DELAY_TIME) {
							  /* Set the time only if desired, otherwise don't do set */
							  /* retStatus = HAL_SetDelayTime(tmpDelayTime);
							     if(retStatus >= 0) {
							     APP_LOG("REMOTE",LOG_DEBUG, "Jarden device delay set to: %d\n", tmpDelayTime);
							     }
							     else {
							     APP_LOG("REMOTE",LOG_DEBUG, "Unable to Set Jaden device delay to: %d, ERROR: %d\n", tmpDelayTime,retStatus);
							     }*/
						  } /* end of if(tmpDelayTime... */

						  APP_LOG("Jarden Device", LOG_DEBUG, "JARDEN DEVICE THREAD: Jarden device remote status update done");
						  //LockJarden();
						  g_remoteSetStatus = 0;
						  //UnlockJarden();
					  } /* if(setStatusFlag... */
				  }
			  }
#endif /* #if 1 */

			  LockJarden();
			  setInProgress = g_setStatusInProgress;
			  UnlockJarden();

			  if(setInProgress) {
				  if(DEVICE_CROCKPOT == g_eDeviceType)
				  {
					  /* If set status is in progress wait for more time */
					  tmpWait = setInProgress * ONE_MSEC;
					  usleep(tmpWait+FIVE_HUNDRED_MSEC);
					  timeCnt = ((tmpWait/FIVE_HUNDRED_MSEC) + 1);
				  }
				  else if ( (g_eDeviceType == DEVICE_SBIRON) ||
						  (g_eDeviceType == DEVICE_MRCOFFEE) ||
						  (g_eDeviceType == DEVICE_PETFEEDER) 
					  )
				  {
					  /* If set status is in progress wait for more time
					     tmpWait = setInProgress * ONE_MSEC;
					     usleep(tmpWait+FIVE_HUNDRED_MSEC);
					     timeCnt = ((tmpWait/FIVE_HUNDRED_MSEC) + 1);*/
					  /* Reset the status & delay */
					  /* To make sure we have delay, this is to be removed */
					  usleep(FIVE_HUNDRED_MSEC); /* 500 msec once */
					  timeCnt++; /* Min timer :: timeCnt = 2 = 1sec */
				  }
				  else
				  {
					  /* WeMo Smart, Do Nothing */
				  }
				  LockJarden();
				  g_setStatusInProgress = 0;
				  UnlockJarden();
			  }
			  else {
				  /* Check the status every 500msec once and send the notification every 15min once */
				  usleep(FIVE_HUNDRED_MSEC); /* 500 msec once */
				  timeCnt++; /* Min timer :: timeCnt = 2 = 1sec */
			  }
			  if(DEVICE_CROCKPOT != g_eDeviceType)
			  {
				  if(g_OffTimer > 0) {
					  g_OffTimer = (g_OffTimer - FIVE_HUNDRED_MSEC);
					  //APP_LOG("Jarden Device", LOG_DEBUG, "############# Jarden Device %d  ###########", g_OffTimer);
				  }
			  }
		} /* end of while */

		return;
	}

void StartJardenTask()
{
	initJarden();

	if (-1 != jarden_thread)
	{
		ithread_cancel(jarden_thread);
		jarden_thread = -1;
	}

	ithread_create(&jarden_thread, NULL, jardenPollTask, NULL);

	if (-1 == jarden_thread) {
		//- If can not be created, it is like DEAD for crockpot, so reboot again
		//- Though this is almost "impossible", but to ensure
		APP_LOG("Jarden Device",LOG_ERR, "@@@@@@@@@@@@@@@@@@@ Jarden device task can not be created, this should not happen @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	}
	else {
		//- success
		ithread_detach(jarden_thread);
	}
}

void StopJardenTask()
{
    if (-1 != jarden_thread)
    {
		ithread_cancel(jarden_thread);
		jarden_thread = -1;
    }
}
#endif /*#ifdef PRODUCT_WeMo_Jarden*/

#ifdef PRODUCT_WeMo_Smart
/*
 * jarden specific code
 */

void initSmart()
{
    APP_LOG("UPNP", LOG_DEBUG, "################### Inside Init Smart Function ###################");
	if(initTheadFlag == 0) {
        ithread_mutexattr_t attr;
        ithread_mutexattr_init(&attr);
        ithread_mutexattr_setkind_np( &attr, ITHREAD_MUTEX_RECURSIVE_NP );
        ithread_mutex_init(&s_smart_mutex, &attr);
        ithread_mutexattr_destroy(&attr);
        initTheadFlag = 1;
    }
}

void LockSmart()
{
	ithread_mutex_lock(&s_smart_mutex);
}

void UnlockSmart()
{
	ithread_mutex_unlock(&s_smart_mutex);
}

void LockSmartRemote()
{
	ithread_mutex_lock(&s_smart_remote_mutex);
}

void UnlockSmartRemote()
{
	ithread_mutex_unlock(&s_smart_remote_mutex);
}

#if 0
/* Task to identify the status change in smart device */
void *smartPollTask(void *args)
{
	WaspVariable WaspAttr;
	int statusChange = 0;
	int i=0, j =0, Err;
	
	char* PrevValueStr[]={0};
	char* CurValueStr[]={0};
	
    APP_LOG("Smart Device", LOG_DEBUG, "Smart device gpio task running");
	
	/* Allocate memory of 32bytes for each field and initializing */
	for (j=0; j< g_attrCount; j++)
	{
		PrevValueStr[j] = (char*)allocateMemory(SIZE_32B);
		memset(PrevValueStr[j], 0, SIZE_32B);
		
		CurValueStr[j] = (char*)allocateMemory(SIZE_32B);
		memset(CurValueStr[j], 0, SIZE_32B);
	}
	
	while (1)
	{
	    if (i < g_attrCount)
		{
			if(g_attrVarDescArray[i]->Usage == 4)
			{
				/* APP_LOG("GPIOPollTask", LOG_DEBUG, "Polling for: i is %d, g_attrVarDescArray[i]->Name is: %s g_attrVarDescArray[i]->Usage is: %d", 
										i, g_attrVarDescArray[i]->Name, g_attrVarDescArray[i]->Usage); */
				
				/* Get the current mode setting */
				memset(&WaspAttr,0,sizeof(WaspVariable));
				WaspAttr.ID = g_attrVarDescArray[i]->ID;
				WaspAttr.Type = g_attrVarDescArray[i]->Type;
		
				if((Err = WASP_GetVariable(&WaspAttr)) != WASP_OK) 
				{
					if(Err >= 0) {
						Err = -Err;
					}	
				}
				else
				{
					strncpy (PrevValueStr[i], CurValueStr[i], sizeof(PrevValueStr[i])-1);
					
					//convertSmartValues(g_attrVarDescArray[i]->Type, &WaspAttr, FROM_SMART, CurValueStr[i]);
					switch(g_attrVarDescArray[i]->Type)
					{
						//APP_LOG("UPNPDevice", LOG_ERR, "Inside GetAttribute switch(g_attrVarDescArray[i]->Type)");
						case WASP_VARTYPE_ENUM:
							sprintf(CurValueStr[i], "%d",WaspAttr.Val.Enum);
						break;
                
						case WASP_VARTYPE_PERCENT:
							sprintf(CurValueStr[i], "%hu",WaspAttr.Val.Percent);
						break;
					
						case WASP_VARTYPE_TEMP:
							sprintf(CurValueStr[i], "%hd",WaspAttr.Val.Temperature);
						break;
                
						case WASP_VARTYPE_TIME32:
							sprintf(CurValueStr[i], "%d",WaspAttr.Val.TimeTenths);
						break;
						
						case WASP_VARTYPE_TIME16:
							sprintf(CurValueStr[i], "%hu",WaspAttr.Val.TimeSecs);
						break;
						
						case WASP_VARTYPE_BOOL:
							sprintf(CurValueStr[i], "%d",WaspAttr.Val.Boolean);
						break;
						
						case WASP_VARTYPE_STRING:
							sprintf(CurValueStr[i], "%s",WaspAttr.Val.String);
						break;
						
						case WASP_VARTYPE_BLOB:
							sprintf(CurValueStr[i], "%s",WaspAttr.Val.Blob.Data);
						break;
						
						case WASP_VARTYPE_UINT8:
							sprintf(CurValueStr[i], "%d",WaspAttr.Val.U8);
						break;
						
						case WASP_VARTYPE_INT8:
							sprintf(CurValueStr[i], "%d",WaspAttr.Val.I8);
						break;
						
						case WASP_VARTYPE_UINT16:
							sprintf(CurValueStr[i], "%d",WaspAttr.Val.U16);
						break;
						
						case WASP_VARTYPE_INT16:
							sprintf(CurValueStr[i], "%d",WaspAttr.Val.I16);
						break;
						
						case WASP_VARTYPE_UINT32:
							sprintf(CurValueStr[i], "%d",WaspAttr.Val.U32);
						break;
						
						case WASP_VARTYPE_INT32:
							sprintf(CurValueStr[i], "%d",WaspAttr.Val.I32);
						break;
						
						case WASP_VARTYPE_TIME_M16:
							sprintf(CurValueStr[i], "%d",WaspAttr.Val.TimeMins);
						break;
						
						default:
							APP_LOG("UPNPDevice", LOG_ERR, "Invalid attribute type");
							break;
					}	
            
					if(strncmp(PrevValueStr[i], CurValueStr[i], strlen(CurValueStr[i])))
					{
						/*APP_LOG("GPIOPollTask", LOG_DEBUG, "Previous Value String is %s and Current Value String is %s ", 
																PrevValueStr[i],CurValueStr[i]);
						*/										
						//SmartStateChangeNotify (g_attrVarDescArray[i]->Name, PrevValueStr[i], CurValueStr[i]);
					}
					else
					{
						/* both values are same, no need to send notification */
					}
				}
	
			}
			else
			{
				//APP_LOG("GPIOPollTask", LOG_DEBUG, "Not Controllee Variable, not required any polling going to next Variable");
			}
			/* go to next variable */
			i++;
		}
		else
		{
			/* Reached max. count, so start from beginning */
			i = 0;
		}
	}
	
	for (j=0; j< g_attrCount; j++)
	{
		free(PrevValueStr[j]);
		free(CurValueStr[j]);
	}
	return;
}
#else
int WaspVar2String(WaspVariable *pVar,char *String,int MaxLen);

int WaspVar2String(WaspVariable *pVar,char *String,int MaxLen)
{
	int Ret;
	switch(pVar->Type) {
		case WASP_VARTYPE_ENUM:
			Ret = snprintf(String,MaxLen,"%d",pVar->Val.Enum);
			break;

		case WASP_VARTYPE_PERCENT:
			Ret = snprintf(String,MaxLen,"%hu",pVar->Val.Percent);
			break;

		case WASP_VARTYPE_TEMP:
			Ret = snprintf(String,MaxLen,"%hd",pVar->Val.Temperature);
			break;

		case WASP_VARTYPE_TIME32:
			Ret = snprintf(String,MaxLen,"%d",pVar->Val.TimeTenths);
			break;

		case WASP_VARTYPE_TIME16:
			Ret = snprintf(String,MaxLen,"%hu",pVar->Val.TimeSecs);
			break;

		case WASP_VARTYPE_BOOL:
			Ret = snprintf(String,MaxLen,"%d",pVar->Val.Boolean);
			break;

		case WASP_VARTYPE_STRING:
			Ret = snprintf(String,MaxLen,"%s",pVar->Val.String);
			break;

		case WASP_VARTYPE_BLOB:
			Ret = snprintf(String,MaxLen,"%s",pVar->Val.Blob.Data);
			break;

		case WASP_VARTYPE_UINT8:
			Ret = snprintf(String,MaxLen,"%d",pVar->Val.U8);
			break;

		case WASP_VARTYPE_INT8:
			Ret = snprintf(String,MaxLen,"%d",pVar->Val.I8);
			break;

		case WASP_VARTYPE_UINT16:
			Ret = snprintf(String,MaxLen,"%d",pVar->Val.U16);
			break;

		case WASP_VARTYPE_INT16:
			Ret = snprintf(String,MaxLen,"%d",pVar->Val.I16);
			break;

		case WASP_VARTYPE_UINT32:
			Ret = snprintf(String,MaxLen,"%d",pVar->Val.U32);
			break;

		case WASP_VARTYPE_INT32:
			Ret = snprintf(String,MaxLen,"%d",pVar->Val.I32);
			break;

		case WASP_VARTYPE_TIME_M16:
			Ret = snprintf(String,MaxLen,"%d",pVar->Val.TimeMins);
			break;

		default:
			APP_LOG("UPNPDevice", LOG_ERR, "Invalid attribute type 0x%x",pVar->Type);
			break;
	}  

	return Ret;
}

void *smartPollTask(void *args)
{
	WaspVariable Var;
	WaspVariable *pVar;
	int i;
	int Err;
    int attrChange = 0;
	#define VALUE_LEN	SIZE_32B
	char PrevValueStr[VALUE_LEN];
	char CurValueStr[VALUE_LEN];

    tu_set_my_thread_name( __FUNCTION__ );

    #if 0
	LockSmart();    
    g_attributeFlag[0] = 1; //added for notification
    UnlockSmart();	
	#endif
     

	WaspVariable *CurValue = NULL;

	APP_LOG("Smart Device",LOG_DEBUG,"Smart device gpio task running");

	if(g_attrCount == 0) {
		APP_LOG("Smart Device",LOG_DEBUG,"Waiting for g_attrCount to be set");
		while(g_attrCount == 0) {
			sleep(1);
		}
	}
	APP_LOG("Smart Device",LOG_DEBUG,"g_attrCount: %d",g_attrCount);

	CurValue = ZALLOC(sizeof(WaspVariable)*g_attrCount);
	

// Init IDs
	for(i = 0; i < g_attrCount; i++) {
		pVar = &CurValue[i];
		pVar->ID = g_attrVarDescArray[i]->ID;
		pVar->State = VAR_VALUE_UNKNOWN;
	}

	for( ; ; ) {
	// Wait for a variable to change value
		if((Err = WASP_GetChangedVar(&Var)) != WASP_OK) {
			APP_LOG("GPIOPollTask",LOG_ERR,"WASP_GetChangedVar failed: %s",
					  WASP_strerror(Err));
		// This shouldn't happen, but if it does don't hard spin, maybe
		// it will "fix" itself
			sleep(1);
			continue;
		}

	// Find the variable that changed
		for(i = 0; i < g_attrCount; i++) {
			if(CurValue[i].ID == Var.ID) {
				break;
			}
		}
		if(i == g_attrCount) {
			APP_LOG("GPIOPollTask",
					  LOG_ERR,"Ignoring changed VarID 0x%x not found",Var.ID);
			continue;
		}

		pVar = &CurValue[i];
		if(pVar->State == VAR_VALUE_UNKNOWN) {
		// Value hasn't been read before
			memcpy(pVar,&Var,sizeof(*pVar));
			WaspVar2String(pVar,CurValueStr,VALUE_LEN);
			printf("Variable 0x%x initial value: %s\n",pVar->ID,CurValueStr);
		}
		else {
			if(WASP_Compare(&Var,pVar) > 0) {
				WaspVar2String(pVar,PrevValueStr,VALUE_LEN);
				WaspVar2String(&Var,CurValueStr,VALUE_LEN);
				WASP_FreeValue(pVar);
				memcpy(&pVar->Val,&Var.Val,sizeof(pVar->Val));
				APP_LOG("GPIOPollTask",LOG_DEBUG,
						  "VarID 0x%x, Previous Value: %s, Current Value: %s ", 
						  pVar->ID,PrevValueStr,CurValueStr);

                #if 0 //Commneted because of some threading issue
				UpdateNotifStructure(g_attrVarDescArray[i]->Name, PrevValueStr, CurValueStr);

                LockSmart();
                g_attributeFlag[80+i] = 1;
                APP_LOG("GPIOPollTask", LOG_DEBUG, "g_attributeFlag[80+%d] is %d",i,g_attributeFlag[80+i]);
                g_notificationFlag = 1;
                APP_LOG("GPIOPollTask", LOG_DEBUG, "g_notificationFlag is %d",g_notificationFlag);
                UnlockSmart();
				#endif
				
			}
		}
	}

	free(CurValue);
	return NULL;
}
#endif

void StartSmartTask()
{
	APP_LOG("UPNP", LOG_DEBUG, "################### Inside Smart Task ###################");
	initSmart();

	if (-1 != smart_thread)
	{
		ithread_cancel(smart_thread);
		smart_thread = -1;
	}

	ithread_create(&smart_thread, NULL, smartPollTask, NULL);

	if (-1 == smart_thread) {
		//- If can not be created, it is like DEAD for crockpot, so reboot again
		//- Though this is almost "impossible", but to ensure
		APP_LOG("Smart Device",LOG_ERR, "@@@@@@@@@@@@@@@@@@@ Smart device task can not be created, this should not happen @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	}
	else {
		//- success
		ithread_detach(smart_thread);
	}
}

void StopSmartTask()
{
    if (-1 != smart_thread)
    {
		ithread_cancel(smart_thread);
		smart_thread = -1;
    }
}
#endif /*#ifdef PRODUCT_WeMo_Smart*/

/*!
 *	\function
 *		SetLastUserActionOnState
 *
 *	\brief
 *		This is to track the user action so that sensor rule will not override the user action as a basic requirement
 *		Note, once any user action OFF. the flag should be reset
 *	\param
 *		state: the last user action ON/OFF: 0x01/0x00
 *					
 *	\return
 *		void
 */
 extern void StopPowerMonitorTimer();
void SetLastUserActionOnState(int state)
{
	//- Add thread protection here
	g_IsLastUserActionOn = state;
	StopPowerMonitorTimer();
}

/*!
 *	\function
 *		IsLastUserActionOn
 *
 *	\brief
 *		return the last user action
 *	\param
 *		
 *					
 *	\return
 *		int
 */
int IsLastUserActionOn()
{
	return g_IsLastUserActionOn;
}


#ifdef _OPENWRT_
int toggleBootState()
{
    FILE    *f = NULL;  
    char    cmd[SIZE_32B] = "fw_printenv -n bootstate\0";
    int     ok = 0, bootstate = 0;

	APP_LOG("ResetAction", LOG_DEBUG, "##### Reset button task Action ##############");

    f = popen(cmd, "r");
    if ( f != NULL ) 
    {
        memset(cmd, 0, sizeof(cmd));
        ok = fread(cmd, 1, sizeof(cmd), f);
        fclose(f);
    }
    if (ok) 
    {
        bootstate = atoi(cmd); 
		APP_LOG("ResetAction", LOG_DEBUG, "Current bootstate: %d", bootstate);
    }
    else
    {
        APP_LOG("ResetAction", LOG_ERR, "Could not fetch bootstate from fw_printenv");
		return FAILURE;
    }

	memset(cmd, 0, sizeof(cmd));
	if (bootstate == 0)
		snprintf(cmd, SIZE_32B, "fw_setenv bootstate 2");
	else if( bootstate == 2)
		snprintf(cmd, SIZE_32B, "fw_setenv bootstate 0");
	else
	{
		APP_LOG("ResetAction", LOG_ERR, "Invalid bootstate [%d]", bootstate);
		return FAILURE;
	}

	system(cmd);	
	APP_LOG("ResetAction", LOG_DEBUG, "Executed command: %s..", cmd);
	system("reboot");
	return SUCCESS;
}
#elif defined(PRODUCT_WeMo_SNS)
int toggleBootState()
{
	char *bootstate = NULL;
	char cmd[SIZE_32B];

	APP_LOG("ResetAction", LOG_DEBUG, "##### Reset button task Action ##############");
	
	bootstate = GetBelkinParameter("bootstate");
	if(!bootstate)
	{
		APP_LOG("ResetAction", LOG_ERR, "Could not fetch bootstate from nvram");
		return FAILURE;
	}
	else
		APP_LOG("ResetAction", LOG_DEBUG, "Current bootstate: %s", bootstate);

	memset(cmd, 0, sizeof(cmd));

	if(!strcmp(bootstate, "0"))
		snprintf(cmd, SIZE_32B, "uboot_env setenv bootstate 2");
	else if(!strcmp(bootstate, "2"))
		snprintf(cmd, SIZE_32B, "uboot_env setenv bootstate 0");
	else
	{
		APP_LOG("ResetAction", LOG_ERR, "Invalid bootstate [%s]", bootstate);
		return FAILURE;
	}

	system(cmd);	
	APP_LOG("ResetAction", LOG_DEBUG, "Executed command: %s..", cmd);
	system("reboot");
	return SUCCESS;
}
#endif

#if defined(PRODUCT_WeMo_Light)
void resetWiFiSettings()
{
	int CurState = POWER_OFF;

	//checking if device is on or not, if on switch off device.
	LockLED();
	CurState = GetCurBinaryState();
	UnlockLED();
	if (POWER_ON== CurState )
		ChangeBinaryState(POWER_OFF);
	/* Remove saved IP from flash */
	UnSetBelkinParameter ("wemo_ipaddr");
	ControlleeDeviceStop();
	setRemoteRestoreParam(0x1);
	UnSetBelkinParameter(WIFI_CLIENT_SSID);
	APP_LOG("ResetButtonTask", LOG_DEBUG, "Going To Self Reboot...");
	system("reboot");
}

int ChangeNightLight(int type)
{
    int err, fd;
    int reg1_gpio_dir=0;
    int reg1_gpio_data=0;
    if( (fd = open("/dev/gpio", O_RDWR)) < 0 )
    {
	APP_LOG("ChangeNightLight", LOG_DEBUG, "Open /dev/gpio failed");
	return 1;
    }
    u32DimVal = type;
    if(type == 1)
    {
	
	APP_LOG("ChangeNightLight", LOG_DEBUG, "*****  DIMING THE NIGHT LIGHT LED *****");
	reg1_gpio_dir = 0xFFFFABFF;
	err = ioctl(fd,RALINK_GPIO_SET_DIR_IN , (void *)&reg1_gpio_dir);
	if( err < 0 ){
	    APP_LOG("ChangeNightLight", LOG_DEBUG, "Ralink RALINK_GPIO_SET_DIR_IN failed");
	    close(fd);
	    return err;
	}
	reg1_gpio_dir = 0x00002A80;
	err = ioctl(fd,RALINK_GPIO_SET_DIR_OUT , (void *)&reg1_gpio_dir);
	if( err < 0 ){
	    APP_LOG("ChangeNightLight", LOG_DEBUG, "Ralink RALINK_GPIO_SET_DIR_OUT failed");
	    close(fd);
	    return err;
	}
	err = ioctl(fd,RALINK_GPIO_READ , (void *)&reg1_gpio_data);
	if( err < 0 ){
	    APP_LOG("ChangeNightLight", LOG_DEBUG, "Ralink RALINK_GPIO_READ failed");
	    close(fd);
	    return err;
	}
	APP_LOG("ChangeNightLight", LOG_DEBUG, "Read RALINK_REG_PIODATA :[0x%08X]",reg1_gpio_data);
    }
    else if ((type == 0) || (type == 2))
    {
	APP_LOG("ChangeNightLight", LOG_DEBUG, "***** Removing DIMING OF NIGHT LIGHT LED *****");
	reg1_gpio_dir = 0xFFFFEBFF;
	err = ioctl(fd,RALINK_GPIO_SET_DIR_IN , (void *)&reg1_gpio_dir);
	if( err < 0 ){
	    APP_LOG("ChangeNightLight", LOG_DEBUG, "Ralink RALINK_GPIO_SET_DIR_IN failed");
	    close(fd);
	    return err;
	}
	reg1_gpio_dir = 0x00006A80;
	err = ioctl(fd,RALINK_GPIO_SET_DIR_OUT , (void *)&reg1_gpio_dir);
	if( err < 0 ){
	    APP_LOG("ChangeNightLight", LOG_DEBUG, "Ralink RALINK_GPIO_SET_DIR_OUT failed");
	    close(fd);
	    return err;
	}
	err = ioctl(fd,RALINK_GPIO_READ , (void *)&reg1_gpio_data);
	if( err < 0 ){
	    APP_LOG("ChangeNightLight", LOG_DEBUG, "Ralink RALINK_GPIO_READ failed");
	    close(fd);
	    return err;
	}
	APP_LOG("ChangeNightLight", LOG_DEBUG, "Read RALINK_REG_PIODATA :[0x%08X]",reg1_gpio_data);
	setPower(g_PowerStatus);
    }
    else{
	APP_LOG("ChangeNightLight", LOG_DEBUG, "Invalid type value : %d",type);
	return 1;
    }
    
    return 0;
    
}
#endif

#if defined(PRODUCT_WeMo_Light) || defined(PRODUCT_WeMo_LEDLight)
void *ResetButtonTask(void *args)
{
	unsigned int CounterForButtorPressed = 0;
	FILE * pButtonFile = 0x00;
	int isResetOccurred = 0x00;
	char szflag[SIZE_4B];
	char* pResult = 0x00;
	int command = BUTTON_RELEASED;
	int InsideResetFlag = 1;

	APP_LOG("ResetButtonTask", LOG_DEBUG, "##### Reset button task running: %s ##############", szResetBottonPath);

	pButtonFile = fopen(szResetBottonPath, "r");
	if (pButtonFile == 0x00)
	{
		APP_LOG("ResetButtonTask:", LOG_DEBUG, "####### open reset button: %s error", szResetBottonPath);
		return 0x00;
	}

	//checking if thread has entered with button already pressed, if so wait for the button to get released
	while (1)
	{
		memset(szflag, 0x00, sizeof(szflag));
		fseek(pButtonFile, 0, SEEK_SET);
		pResult = fgets(szflag, sizeof(szflag), pButtonFile);
		if (pResult != 0x00)
		{
			if (0x0 != strlen(szflag))
				command = atoi(szflag);

			if (BUTTON_PRESSED == command)
			{
				APP_LOG("ResetButtonTask:", LOG_DEBUG, "button prsd on entry: %s error", szResetBottonPath);
				continue;
			}
			else
			{
				APP_LOG("ResetButtonTask:", LOG_DEBUG, "starting RB Task : %s", szResetBottonPath);
				break;
			}
		}
		usleep(50000);
	}
	//waiting for 5 second button pressed
	while (1)
	{
		memset(szflag, 0x00, sizeof(szflag));
		fseek(pButtonFile, 0, SEEK_SET);
		pResult = fgets(szflag, sizeof(szflag), pButtonFile);
		if (pResult != 0x00)
		{
			if (0x0 != strlen(szflag))
				command = atoi(szflag);

			if (BUTTON_PRESSED == command)
			{
				if (InsideResetFlag)
				{
					InsideResetFlag = 0;
					APP_LOG("ResetButtonTask:", LOG_DEBUG, "Inside Reset");
				}
				CounterForButtorPressed += 1;
				if (RESET_NETWORK_LOOP_CNT == CounterForButtorPressed)
				{
					isResetOccurred = 0x01;
				}
			}
			else
			{
				InsideResetFlag = 1;
				CounterForButtorPressed = 0;
			}
		}
		if (isResetOccurred)
		{
			isResetOccurred = 0x0;
			break;
		}
		usleep(50000);
	}

	APP_LOG("ResetButtonTask:", LOG_DEBUG, "####### Reset Occurred");
	fclose(pButtonFile);
#ifdef PRODUCT_WeMo_Light
	resetWiFiSettings();
#else
	toggleBootState();
#endif
  return NULL;
}
#endif

#ifdef PRODUCT_WeMo_NetCam
/*!
 * 	\@fucntion	SetSensorStatus
 *
 *	\@brief 	To set the sensor status flag in case the subscription occurred after motioned detection
 *
 *
 *
 *	\@return	void
 */
void SetSensorStatus(int state)
{
	LockSensor();
	g_SensorStatus = state;
	UnlockSensor();
}
#endif
