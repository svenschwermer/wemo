/***************************************************************************
*
*
* cloudIF.c
*
* Created by Belkin International, Software Engineering on Aug 8, 2011
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
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "cloudIF.h"
#include "gpio.h"
#include "controlledevice.h"
#include "rule.h"

#ifdef PRODUCT_WeMo_Smart
#include "SmartRemoteAccess.h"
#endif

#ifdef SIMULATED_OCCUPANCY
#include "simulatedOccupancy.h"
#endif
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */


char* GetOverridenStatusIF(char* szOutBuff);
extern char g_NotificationStatus[MAX_RES_LEN];

#ifdef PRODUCT_WeMo_Light
/**
 * - CloudSetNightLightStatus
 *
 * - Set LS night light status
 *
 * - newState: HIGH(2), LOW(1), OFF(0)
 *
 * - return:
 *      0x00: success
 *      0x01: unsuccess
 *************************************************/
int CloudSetNightLightStatus(char *dimValue)
{
    return changeNightLightStatus(dimValue);
}
#endif


/**
 * - CloudSetBinaryState
 *
 * - Set binary state
 *
 * - newState: ON(0x01), OFF(0x00)
 *
 * - return:
 *      0x00: success
 *      0x01: unsuccess
 *************************************************/
int CloudSetBinaryState (int newState)
{
    int ret = -1;

//#ifndef PRODUCT_WeMo_LEDLight
    setRemote ("1");
    setActuation (ACTUATION_MANUAL_APP);
//#endif

    /* Get Countdown timer last minute running state*/
    int countdownRuleLastMinStatus =  gCountdownRuleInLastMinute;

#ifdef PRODUCT_WeMo_Insight
    int tempInsightState = newState;
    if (POWER_ON == tempInsightState)
    {
        tempInsightState = POWER_SBY;
        APP_LOG ("REMOTE", LOG_DEBUG, "tempInsightState =  %d",
                 tempInsightState);
    }
    executeCountdownRule (tempInsightState);
#else
    /* This API will start/restart/stop Countdown timer depending upon if
     * Countdown timer is not_running/ runing_in_last_minute/
     * running_not_in_last_ minute */
#ifndef PRODUCT_WeMo_LEDLight
    executeCountdownRule (newState);
#endif
#endif

    /* Check if it was running in last minute, if yes do not toggle relay,
     * restart Countdown timer*/
    if (gRuleHandle[e_COUNTDOWN_RULE].ruleCnt && countdownRuleLastMinStatus)
    {
        APP_LOG ("REMOTE", LOG_DEBUG, "Countdown timer was in last minute."
                 " Not toggling the RELAY!");
    }
    else
    {
        ret = ChangeBinaryState (newState);
    }

    SetLastUserActionOnState (newState);
    if (0x00 == ret)
    {
        //- Notify phone
        /* To avoid duplicate local notification, being send from
         * InternalLocalUpdate function */
#ifdef PRODUCT_WeMo_Insight
        if (0x01 == newState)
            newState = 0x08;
#endif

        //- Notify sensor
        UPnPInternalToggleUpdate(newState);
    }

    return ret;
}

/**
 * - CloudUpgradeFwVersion
 *
 * - To Upgrade Firmware Version
 *
 * - dwldStartTime:
 *
 * - return:
 *      0x00: success
 *      0x01: unsuccess
 *************************************************/
int     CloudUpgradeFwVersion(int dwldStartTime, char *FirmwareURL, int fwUnSignMode)
{
    int retVal = 0x00, state = -1;
    FirmwareUpdateInfo fwUpdInf;

    state = getCurrFWUpdateState();
    if( (state == FM_STATUS_DOWNLOADING) || (state == FM_STATUS_DOWNLOAD_SUCCESS) ||
        (state == FM_STATUS_UPDATE_STARTING) )
    {
	APP_LOG("REMOTE",LOG_ERR, "************Firmware Update Already in Progress...");
	return 0x00;
    }

    fwUpdInf.dwldStartTime = dwldStartTime;
		fwUpdInf.withUnsignedImage = fwUnSignMode;
    memset(fwUpdInf.firmwareURL, 0x00, sizeof(fwUpdInf.firmwareURL));
    strncpy(fwUpdInf.firmwareURL, FirmwareURL, sizeof(fwUpdInf.firmwareURL)-1);

    retVal = StartFirmwareUpdate(fwUpdInf);
    return retVal;
}

/**
 * - CloudGetBinaryState
 *
 * - Get binary state
 *
 * - newState: ON(0x01, motion detected), OFF(0x00, motion not detected)
 *
 * - return:
 *      0x00: success
 *      0x01: unsuccess
 *************************************************/
int     CloudGetBinaryState()
{

    int ret = GetDeviceStateIF();

    return ret;
}

#ifdef PRODUCT_WeMo_Jarden
/**

 * - CloudSetJardenStatus
 *
 * - Set crockpot status
 *
 * - mode: OFF(0), WARM(0x2), LOW(0x3), HIGH(0x4)
 * - delaytime: in Minutes (0 to 1440)
 *
 * - return:
 *      0x00: success
 *      0x01: unsuccess
 *************************************************/

int CloudSetJardenStatus(int mode, int delaytime)

{
    int tempMode;

    /*if(DEVICE_CROCKPOT == g_eDeviceType)
    {
        tempMode = convertModeValue(mode,TO_CROCKPOT);
    }
    else
    {
        tempMode = mode;
    }*/

    /* At this location using the mutex is relevant? Since this is not in a thread (I am not sure)*/
    LockJarden();
    g_modeStatus = mode; /*Remove the upper nibble value */
    if(DEVICE_CROCKPOT == g_eDeviceType)
    {
        g_timeCookTime = delaytime;
        APP_LOG("REMOTE",LOG_DEBUG, "Set Crockpot status done: %d mode, %d time\n",g_modeStatus,g_timeCookTime);
    }
    else if ( (g_eDeviceType == DEVICE_SBIRON) ||
	          (g_eDeviceType == DEVICE_MRCOFFEE) ||
			  (g_eDeviceType == DEVICE_PETFEEDER)
			)
    {
        g_delayTime = delaytime;
        APP_LOG("REMOTE",LOG_DEBUG, "Set Jarden device status done: %d mode, %d time\n",g_modeStatus,g_delayTime);
    }
	else
	{
		/* WeMo Smart, Do Nothing */
	}
    UnlockJarden();
    g_remoteSetStatus = 1;

    /* Cloud set jarden device status function is invoked from a remote access function
     * remote access function is a handler for the rx/tx data handler for the n/w operations
     * I am not sure rx/tx data hanlder will run as thread or not.
     * If it is not a thread invoking set functions here may create problems like only few instructions
     * may be executed and remaing may never get executed - like only set mode & set time will never
     * Best way is to set the global parameters (use lock) and do the actual set operation in a thread
     */
    return 0;
}

/**
 * - CloudGetJardenStatus
 *
 * - Get jarden device status
 *
 * - mode: OFF(0x0/0x30), IDLE(0x1/0x31), WARM(0x2/0x32), LOW(0x3/0x33), HIGH(0x4/0x34)
 * - cooktime: in minutes (0 to 1200)
 * - delaytime: in minutes (0 to 1440)
 * - cookedTime: in minutes
 *
 * - return:
 *      0x00: success
 *      0x01: unsuccess
 *************************************************/
int CloudGetJardenStatus(int *mode, int *delaytime, unsigned short int *cookedTime)
{
	int ret = GetJardenStatusRemote(mode, delaytime, cookedTime);

	return ret;
}
#endif /* #ifdef PRODUCT_WeMo_Jarden */

#ifdef PRODUCT_WeMo_Smart
/**

 * - CloudSetAttributes
 *
 * - Sets Smart Attributes
 *
 * - return:
 *      0x00: success
 *      0x01: unsuccess
 *************************************************/

int CloudSetAttributes(SmartParameters *SmartParams)

{
	int ret = SetSmartAttributesRemote(SmartParams);

	return ret;
}

/**
 * - CloudGetAttributes
 *
 * - Gets Smart device status
 *
 * - return:
 *      0x00: success
 *      0x01: unsuccess
 *************************************************/
int CloudGetAttributes(SmartParameters *SmartParams, int i)
{
	int ret = GetSmartAttributesRemote(SmartParams, i);

	return ret;
}
#endif /* #ifdef PRODUCT_WeMo_Smart */

char* CloudGetRuleDBVersion(char* buf)
{
    return GetRuleDBVersionIF(buf);
}

/**
 * - CloudSetRuleDBVersion
 *
 * - Set Rule DB version
 *
 *************************************************/
void CloudSetRuleDBVersion(char* buf)
{
    SetRuleDBVersionIF(buf);
}


/**
 * - BinaryStatusNotify
 *
 * - Cloud client needs to implement the sync and data protection
 *
 * - Called by local embedded when any status change
 *
 *
 */
int     DeviceBinaryStatusNotify(int newState)
{
    return 0x00;
}

int     DeviceRuleNotify(char** WeeklyRuleList)
{
    return 0x00;
}


/**
 * @Function:
 GetOverriddenStatus
 * @Description:
 Called to get override status remotely
 *
 * @Parameters:
 *  szOutBuff
 *	buffer from caller, allocated and dellocated by caller, the size should be 512 bytes
 *
 */
char* GetOverriddenStatus(char* szOutBuff)
{

    char* ret = 0x00;

    if (0x00 == szOutBuff)
    {
	return ret;
    }

    ret = GetOverridenStatusIF(szOutBuff);


    return ret;
}
