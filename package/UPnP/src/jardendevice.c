/***************************************************************************
*
*
* jardendevice.c
*
* Created by Belkin International, Software Engineering on Feb 6, 2013
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
#ifdef PRODUCT_WeMo_Jarden
#include <ithread.h>
#include <upnp.h>
#include <sys/time.h>
#include <math.h>


#include "global.h"
#include "defines.h"
#include "fw_rev.h"
#include "logger.h"
#include "wifiHndlr.h"
#include "controlledevice.h"
#include "gpio.h"
#include "wasp_api.h"
#include "wasp.h"

#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "itc.h"
#include "fwDl.h"
#include "remoteAccess.h"
#include "pktStructs.h"
#include "rule.h"
#include "plugin_ctrlpoint.h"
#include "utils.h"
#include "mxml.h"
#include "sigGen.h"
#include "watchDog.h"
#include "upnpCommon.h"
#include "osUtils.h"
#include "gpioApi.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */


extern void LocalBinaryStateNotify(int curState);
extern UpnpDevice_Handle device_handle;
extern int g_phoneFlag;
extern int g_timerFlag;
extern unsigned long int g_poweronStartTime;

void LocalJardenStateNotify(int jMode, int jDelayTime, unsigned short int jcookedTime);

/*
 * Set the Jarden Device Status as requested by the remote cloud server.
 */
int SetJardenStatusRemote(int mode, int delaytime)
{
	int ret=0,newMode;
	int allowSetTime=FALSE;
	int Err;
	WaspVariable WaspMode, WaspCookTime;
	int CookTime = delaytime;


	/* Mode value varies between 0x0 & 0x1
	 */
	APP_LOG("REMOTE",LOG_DEBUG, "In SetJardenStatusRemote Function: mode:%d, time:%d\n", mode, delaytime);

	if (DEVICE_CROCKPOT == g_eDeviceType)
	{
		if( (MODE_IDLE != mode) ||
				(MODE_UNKNOWN != mode) )
		{
			if(APP_MODE_ERROR != mode) {
				memset(&WaspMode,0,sizeof(WaspVariable));
				WaspMode.ID = CROCKPOT_VAR_MODE;
				WaspMode.Type = WASP_ENUM;
				WaspMode.Val.Enum = convertModeValue(mode,TO_CROCKPOT);

				if((Err = WASP_SetVariable(&WaspMode)) != WASP_OK) {
					if(Err >= 0) {
						Err = -Err;
					}
					ret = Err;
				}
				//ret = HAL_SetMode(mode);
				if(ret >= 0) {
					APP_LOG("REMOTE",LOG_DEBUG, "Crockpot Mode set to: %d\n", mode);
					g_phoneFlag = 1;
					
					if((mode == APP_MODE_HIGH) || (mode == APP_MODE_LOW))
					{
						 g_SetStatefromPhone = 1;
					}
					else
					{
						 g_SetStatefromPhone = 0;
					}
				}
				else {
					APP_LOG("REMOTE",LOG_DEBUG, "Unable to Set Crockpot Mode to: %d, ERROR:%d\n", mode,ret);
				}
			} /* if(APP_MODE_ERROR... */
		} /* end of if(tmpMode== ... */
	} /* end of if(tmpMode== ... */
	else if ( (g_eDeviceType == DEVICE_SBIRON) ||
			(g_eDeviceType == DEVICE_MRCOFFEE) ||
			(g_eDeviceType == DEVICE_PETFEEDER) 
		)
	{
		if(MODE_UNKNOWN != mode)
		{  
			memset(&WaspMode,0,sizeof(WaspVariable));
			WaspMode.ID = CROCKPOT_VAR_MODE;
			WaspMode.Type = WASP_ENUM;
			WaspMode.Val.Enum = convertModeValue(mode,TO_CROCKPOT);

			if((Err = WASP_SetVariable(&WaspMode)) != WASP_OK) {
				if(Err >= 0) {
					Err = -Err;
				}
				ret = Err;
			}
			//ret = HAL_SetMode(mode);
			if(ret >= 0) {        
				APP_LOG("REMOTE",LOG_DEBUG, "Jarden Device Mode set to: %d\n", mode); 
			}
			else {
				APP_LOG("REMOTE",LOG_DEBUG, "Unable to Set Jarden Device Mode to: %d, ERROR:%d\n", mode,ret);
			}
		} /* end of if( mode== ... */
	}
	else
	{
		/* WeMo Smart module, do nothing*/
	}

	/* Wait for a few msec before set the timer */
	//usleep(100*1000);

	if (DEVICE_CROCKPOT == g_eDeviceType)
	{
		if(delaytime != MAX_COOK_TIME) {
			allowSetTime = TRUE;
			g_timerFlag = 1;
		}
		else 
		{
			memset(&WaspMode,0,sizeof(WaspVariable));
			WaspMode.ID = CROCKPOT_VAR_MODE;
			WaspMode.State = VAR_VALUE_LIVE;

			if((Err = WASP_GetVariable(&WaspMode)) != WASP_OK) 
			{
				if(Err >= 0) {
					Err = -Err;
				}
				newMode = Err;
			}
			else 
			{
				if(WaspMode.ID != CROCKPOT_VAR_MODE) 
				{
					APP_LOG("Crockpot Error", LOG_DEBUG, "requested variable ID %d got %d",CROCKPOT_VAR_MODE,WaspMode.ID);
					/* LOG("%s: Error, requested variable ID 0x%x, got 0x%x\n",
					   __FUNCTION__,CROCKPOT_VAR_MODE,WaspMode.ID); */
				}
				newMode = convertModeValue(WaspMode.Val.Enum, FROM_CROCKPOT);
			}

			//newMode = HAL_GetMode();
			if(delaytime != MAX_COOK_TIME) {
				allowSetTime = TRUE;
				g_timerFlag = 1;
			}
			else {
				allowSetTime = FALSE;
				g_timerFlag = 0;
			}
		}
		if((APP_TIME_ERROR != delaytime) && (TRUE==allowSetTime)) 
		{
			ret = 0;
			APP_LOG("REMOTE", LOG_DEBUG, "setting cooking time to %d minutes, mode: %d.\n",delaytime,newMode);
			/* LOG("%s: setting cooking time to %d minutes, mode: %d.\n",__FUNCTION__,
			   delaytime,newMode); */
			do 
			{
				if(delaytime < 0 || delaytime > MAX_COOK_TIME) {
					// Argment out of range
					ret = -EINVAL;
					break;
				}

				if(newMode != MODE_LOW && newMode != MODE_HIGH) {
					ret = ERR_HAL_WRONG_MODE;
					break;
				}
				memset(&WaspCookTime,0,sizeof(WaspVariable));
				WaspCookTime.ID = CROCKPOT_VAR_COOKTIME;
				WaspCookTime.Type = WASP_VARTYPE_TIME_M16;
				WaspCookTime.Val.U16 = CookTime;

				if((ret = WASP_SetVariable(&WaspCookTime)) != WASP_OK) {
					if(ret >= 0) {
						ret = -ret;
					}
				}
			} while(FALSE);

			//ret = HAL_SetCookTime(delaytime);
			if(ret >= 0) {
				APP_LOG("REMOTE",LOG_DEBUG, "Crockpot time set to: %d\n", delaytime);
			}
			else {
				APP_LOG("REMOTE",LOG_DEBUG, "Unable to Set Crockpot Cooktime to: %d, ERROR: %d\n", delaytime,ret);
			}
		} /* end of if(APP_TIME_ERROR... */
	}
	else if ( (g_eDeviceType == DEVICE_SBIRON) ||
			(g_eDeviceType == DEVICE_MRCOFFEE) ||
			(g_eDeviceType == DEVICE_PETFEEDER) 
		)
	{
		/* Delay setting is allowed only if device is powered on? */
		if((mode == MODE_ON) || (mode == MODE_OFF)) 
		{
			allowSetTime = TRUE;
			g_timerFlag = 1;
		}
		else 
		{
			memset(&WaspMode,0,sizeof(WaspVariable));
			WaspMode.ID = CROCKPOT_VAR_MODE;
			WaspMode.State = VAR_VALUE_LIVE;

			if((Err = WASP_GetVariable(&WaspMode)) != WASP_OK) 
			{
				if(Err >= 0) {
					Err = -Err;
				}
				newMode = Err;
			}
			else 
			{
				if(WaspMode.ID != CROCKPOT_VAR_MODE) 
				{
					APP_LOG("Crockpot Error", LOG_DEBUG, "requested variable ID %d got %d",CROCKPOT_VAR_MODE,WaspMode.ID);
					/* LOG("%s: Error, requested variable ID 0x%x, got 0x%x\n",
					   __FUNCTION__,CROCKPOT_VAR_MODE,WaspMode.ID); */
				}
				newMode = convertModeValue(WaspMode.Val.Enum, FROM_CROCKPOT);
			}
			//newMode = HAL_GetMode();
			if((newMode == MODE_ON) || (newMode == MODE_OFF)) 
			{
				allowSetTime = TRUE;
				g_timerFlag = 1;
			}
			else 
			{
				allowSetTime = FALSE;
				g_timerFlag = 0;
			}
		}

		if((0 < delaytime) && (MAX_DELAY_TIME >= delaytime) && (TRUE==allowSetTime)) 
		{
			/* Set the time only if desired, otherwise don't do set */
			APP_LOG("REMOTE", LOG_DEBUG, "setting cooking time to %d minutes, mode: %d.\n",delaytime, newMode);
			/*LOG("%s: setting cooking time to %d minutes, mode: %d.\n",__FUNCTION__,
			  delaytime,newMode);*/
			ret = 0;
			do 
			{
				if(delaytime < 0 || delaytime > MAX_DELAY_TIME) 
				{
					// Argment out of range
					ret = -EINVAL;
					break;
				}
				if(newMode != MODE_LOW && newMode != MODE_HIGH)
				{
					ret = ERR_HAL_WRONG_MODE;
					break;
				}

				memset(&WaspCookTime,0,sizeof(WaspVariable));
				WaspCookTime.ID = CROCKPOT_VAR_COOKTIME;
				WaspCookTime.Type = WASP_VARTYPE_TIME_M16; //WASP_INT32;
				WaspCookTime.Val.U16 = delaytime;

				if((ret = WASP_SetVariable(&WaspCookTime)) != WASP_OK) {
					if(ret >= 0) {
						ret = -ret;
					}
				} 
			} while(FALSE);
			//ret = HAL_SetDelayTime(delaytime);  /* Currently using Cooktime logic of WASP as Delay time requirements are not there */
			if(ret >= 0) {
				APP_LOG("REMOTE",LOG_DEBUG, "Jarden device delay set to: %d\n", delaytime); 
			}
			else {
				APP_LOG("REMOTE",LOG_DEBUG, "Unable to Set Jarden Device delaytime to: %d, ERROR: %d\n", delaytime,ret);
			}
		} /* end of if(APP_TIME_ERROR... */ 
	}
	return 0;
}

/*
 * Get the current Jarden device Status as requested the remote cloud server.
 */
int GetJardenStatusRemote(int *mode, int *delaytime, unsigned short int *cookedTime)
{
    int tempMode, Err;
	WaspVariable WaspMode, WaspCookTime, WaspCookedTime;
	
    if(DEVICE_CROCKPOT == g_eDeviceType)
    {
        memset(&WaspMode,0,sizeof(WaspVariable));
		WaspMode.ID = CROCKPOT_VAR_MODE;
		WaspMode.State = VAR_VALUE_LIVE;
		
		if((Err = WASP_GetVariable(&WaspMode)) != WASP_OK) 
		{
			if(Err >= 0) {
				Err = -Err;
			}
			*mode = Err; 
		}
		else 
		{
			if(WaspMode.ID != CROCKPOT_VAR_MODE) 
			{
				APP_LOG("Crockpot Error", LOG_DEBUG, "requested variable ID %d got %d",CROCKPOT_VAR_MODE,WaspMode.ID);
				/* LOG("%s: Error, requested variable ID 0x%x, got 0x%x\n",
					__FUNCTION__,CROCKPOT_VAR_MODE,WaspMode.ID); */
			}
			*mode = convertModeValue(WaspMode.Val.Enum, FROM_CROCKPOT);
		}
		APP_LOG("RemoteJardenStatus",LOG_DEBUG, "mode  being returned is %d",*mode);
		
		/*mode = HAL_GetMode();
		APP_LOG("JardenStatus",LOG_DEBUG, "GetJardenStatusRemote mode being returned is %d",*mode);        
		tempMode = *mode;
		APP_LOG("JardenStatus",LOG_DEBUG, "GetJardenStatusRemote tempMode  is %d",tempMode); 
        *mode = convertModeValue(tempMode,FROM_CROCKPOT);
		APP_LOG("JardenStatus",LOG_DEBUG, "convertModeValue mode being returned is %d",*mode); */
		memset(&WaspCookTime,0,sizeof(WaspVariable));
		WaspCookTime.ID = CROCKPOT_VAR_COOKTIME;
		WaspCookTime.Type = WASP_VARTYPE_TIME_M16;

		if((Err = WASP_GetVariable(&WaspCookTime)) != WASP_OK) {
			if(Err >= 0) {
				Err = -Err;
			}
			*delaytime = Err; 
		}
		else {
			if(WaspCookTime.ID != CROCKPOT_VAR_COOKTIME) {
				APP_LOG("Crockpot Error", LOG_DEBUG, "requested variable ID %d got %d",CROCKPOT_VAR_COOKTIME,WaspCookTime.ID);
				/* LOG("%s: Error, requested variable ID 0x%x, got 0x%x\n",
					__FUNCTION__,CROCKPOT_VAR_COOKTIME,WaspCookTime.ID); */
			}
			*delaytime = WaspCookTime.Val.U16;
		}

		//*delaytime = HAL_GetCookTime();
		//*delaytime = 0;
		//cookedTime
		
		memset(&WaspCookedTime,0,sizeof(WaspVariable));
		WaspCookedTime.ID = CROCKPOT_VAR_COOKEDTIME;
		WaspCookedTime.Type = WASP_VARTYPE_TIME_M16;

		if((Err = WASP_GetVariable(&WaspCookedTime)) != WASP_OK) {
			if(Err >= 0) {
				Err = -Err;
			}
			*cookedTime = Err; 
		}
		else {
			if(WaspCookedTime.ID != CROCKPOT_VAR_COOKEDTIME) {
				APP_LOG("Crockpot Error", LOG_DEBUG, "requested variable ID %d got %d",CROCKPOT_VAR_COOKTIME,WaspCookTime.ID);
				/* LOG("%s: Error, requested variable ID 0x%x, got 0x%x\n",
					__FUNCTION__,CROCKPOT_VAR_COOKTIME,WaspCookTime.ID); */
			}
			*cookedTime = WaspCookedTime.Val.U16;
		}
		
		APP_LOG("JardenStatus",LOG_DEBUG, "cookedTime being returned is %hd minutes",*cookedTime);
    }
    else if ( (g_eDeviceType == DEVICE_SBIRON) ||
	          (g_eDeviceType == DEVICE_MRCOFFEE) ||
			  (g_eDeviceType == DEVICE_PETFEEDER) 
			)
    {
		/* Mode value varies between 0 &  1
		* Max Delay can be 24hrs
		*/
     
		memset(&WaspMode,0,sizeof(WaspVariable));
		WaspMode.ID = CROCKPOT_VAR_MODE;
		WaspMode.State = VAR_VALUE_LIVE;
		
		if((Err = WASP_GetVariable(&WaspMode)) != WASP_OK) 
		{
			if(Err >= 0) {
				Err = -Err;
			}
			*mode = Err;
		}
		else 
		{
			if(WaspMode.ID != CROCKPOT_VAR_MODE) 
			{
				APP_LOG("Crockpot Error", LOG_DEBUG, "requested variable ID %d got %d",CROCKPOT_VAR_MODE,WaspMode.ID);
				/* LOG("%s: Error, requested variable ID 0x%x, got 0x%x\n",
					__FUNCTION__,CROCKPOT_VAR_MODE,WaspMode.ID); */
			}
			*mode = convertModeValue(WaspMode.Val.Enum, FROM_CROCKPOT);
		}
		
		memset(&WaspCookTime,0,sizeof(WaspVariable));
		WaspCookTime.ID = CROCKPOT_VAR_COOKTIME;
		WaspCookTime.Type = WASP_VARTYPE_TIME_M16; //WASP_INT32;

		if((Err = WASP_GetVariable(&WaspCookTime)) != WASP_OK) {
			if(Err >= 0) {
				Err = -Err;
			}
			*delaytime = Err;
		}
		else {
			if(WaspCookTime.ID != CROCKPOT_VAR_COOKTIME) {
				APP_LOG("Crockpot Error", LOG_DEBUG, "requested variable ID %d got %d",CROCKPOT_VAR_COOKTIME,WaspCookTime.ID);
				/* LOG("%s: Error, requested variable ID 0x%x, got 0x%x\n",
					__FUNCTION__,CROCKPOT_VAR_COOKTIME,WaspCookTime.ID); */
			}
			*delaytime = WaspCookTime.Val.I32;
		}

		/**mode = HAL_GetMode();     
		*delaytime = HAL_GetDelayTime();*/ /* Currently using Cooktime logic of WASP as Delay time requirements are not there */
    }
	else
	{
		/* WeMo Smart, do nothing */
	}
   
    return 0;
}

int convertModeValue(int mode, int flag)
{
    int retMode;
    if(TO_CROCKPOT == flag) {
        switch(mode) {
            case 0:
                retMode = CROCKPOT_MODE_OFF;
                break;
            case 1:
                retMode = CROCKPOT_MODE_OFF;  //Mode Idle
                break;
            case 50:
                retMode = CROCKPOT_MODE_WARM;
                break;
            case 51:
                retMode = CROCKPOT_MODE_LOW;
                break;
            case 52:
                retMode = CROCKPOT_MODE_HIGH;
                break;
            default:
                retMode = MODE_UNKNOWN;
				break;
        }
    }
    else { /*FROM_CROCKPOT*/
        switch(mode) {
            case CROCKPOT_MODE_OFF:
					retMode = APP_MODE_OFF;
					break;
            /*case MODE_IDLE:
                retMode = APP_MODE_IDLE;
                break;*/
            case CROCKPOT_MODE_WARM:
					retMode = APP_MODE_WARM;
					break;
            case CROCKPOT_MODE_LOW:
					retMode = APP_MODE_LOW;
					break;
            case CROCKPOT_MODE_HIGH:
					retMode = APP_MODE_HIGH;
					break;
            default:
					retMode = APP_MODE_ERROR;
					break;
        }
    }
    return retMode;
}
/* SetJardenStatus function is to change the Jarden Device settings 
 * This function make use of HAL_SetMode() & HAL_SetDelaytime() functions from HAL layer
 * pActionRequest - Handle to request
 *
 */
int SetJardenStatus(pUPnPActionRequest pActionRequest, 
                     IXML_Document *request, 
                     IXML_Document **out, 
                     const char **errorString)
{
	int jardenMode;
	int tempmode, curMode;
	int delayTime;
	int retstatus=0,allowSetTime=FALSE;
	char szCurState[16];
	WaspVariable WaspMode, WaspCookTime;
	int Err;

	memset(szCurState, 0x00, sizeof(szCurState));

	if (NULL == pActionRequest || NULL == request)
	{
		APP_LOG("UPNPDevice", LOG_DEBUG, "SetJardenStatus: command paramter invalid");
		return SET_PARAMS_NULL;
	}
	/*else {
	  APP_LOG("UPNPDevice", LOG_DEBUG, "SetJardenStatus: Begining of set operation");
	  }*/

	char* parammode = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "mode");
	APP_LOG("UPNPDevice", LOG_DEBUG, "parammode is %s", parammode);

	if (0x00 == parammode || 0x00 == strlen(parammode))
	{
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = SET_PARAM_ERROR;
		/*UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetJardenStatus",
		  CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "mode", "Parameter Error");*/

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetCrockpotState",
				CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "mode", "Parameter Error");

		APP_LOG("UPNPDevice", LOG_DEBUG, "SetCrockpotState: mode Parameter Error");

		return UPNP_E_SUCCESS;
	}
	char* paramtime = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "time");
	APP_LOG("UPNPDevice", LOG_DEBUG, "paramtime is %s", paramtime);

	//if (0x00 == paramtime || 0x00 == strlen(paramtime))
	if (0x00 == strlen(paramtime))
	{
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = SET_PARAM_ERROR;
		/* UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetJardenStatus",
		   CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "time", "Parameter Error");*/
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetCrockpotState",
				CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "time", "Parameter Error");

		APP_LOG("UPNPDevice", LOG_DEBUG, "SetCrockpotState: time Parameter Error");
		FreeXmlSource(parammode);
		return UPNP_E_SUCCESS;
	}

	/*APP_LOG("UPNPDevice", LOG_DEBUG, "SetJardenStatus: First Doc item completed");*/

	jardenMode = atoi(parammode);
	delayTime = atoi(paramtime);
	APP_LOG("UPNPDevice", LOG_ERR, "SETJARDENSTATUS: jarden Mode received is: %d and delayTime received is %d ", jardenMode, delayTime);


	sprintf(szCurState, "%d", jardenMode);

	/*APP_LOG("UPNPDevice", LOG_DEBUG, "SetJardenStatus: Setting the Crockpot to %d mode with %d time", jardenMode,delayTime);  */

	if (DEVICE_CROCKPOT == g_eDeviceType)
	{
		/* Change the mode to crockpot values from 0, 1, 50, 51, 52 to maintain
		 * cosistency between local & remote mode
		 */
		tempmode = convertModeValue(jardenMode,TO_CROCKPOT);

		APP_LOG("UPNPDevice", LOG_ERR, "SETJARDENSTATUS: temp Mode received is: %d ", tempmode);

		if(MODE_UNKNOWN != tempmode) {
			APP_LOG("UPNPDevice", LOG_ERR, "Inside if(MODE_IDLE != tempmode)");

			if(APP_MODE_ERROR != tempmode) {

				APP_LOG("UPNPDevice", LOG_ERR, "Inside if(APP_MODE_ERROR != tempmode)");
				/* Call HAL API's to set mode and set time for a crockpot */
				retstatus=0;
				memset(&WaspMode,0,sizeof(WaspVariable));
				WaspMode.ID = CROCKPOT_VAR_MODE;
				WaspMode.Type = WASP_ENUM;
				WaspMode.Val.Enum = tempmode;

				if((Err = WASP_SetVariable(&WaspMode)) != WASP_OK) {
					if(Err >= 0) {
						Err = -Err;
					}
					APP_LOG("UPNPDevice", LOG_ERR, "SETJARDENSTATUS: WASP_SetVariable returns : %d ", Err);
					retstatus = Err;
				}

				//retstatus = HAL_SetMode(tempmode);
				APP_LOG("UPNPDevice", LOG_ERR, "SETJARDENSTATUS: retstatus = HAL_SetMode(tempmode) is : %d ", retstatus);
				if (0x00 == retstatus) {
					/* Is this internal broadcast is required? */
					//UPnPInternalToggleUpdate(crockpot_mode);
					pActionRequest->ActionResult = NULL;
					pActionRequest->ErrCode = 0;
					/* UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetJardenStatus",
					   CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "mode", szCurState);*/


					UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetCrockpotState",
							CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "mode", szCurState);

					APP_LOG("UPNPDevice", LOG_ERR, "SETJARDENSTATUS: Mode changed to: %d a.k.a %d mode", jardenMode, tempmode);
					g_phoneFlag = 1;
					
					if((tempmode == CROCKPOT_MODE_LOW) || (tempmode == CROCKPOT_MODE_HIGH))
					{
						g_SetStatefromPhone = 1;
					}
					else
					{
						g_SetStatefromPhone = 0;
					}
				}
				else {
					pActionRequest->ActionResult = NULL;
					pActionRequest->ErrCode = SET_CPMODE_FAILURE;

					/* UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetJardenStatus",
					   CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "mode", "Error");*/


					UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetCrockpotState",
							CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "mode", "Error");

					APP_LOG("UPNPDevice", LOG_ERR, "SETJARDENSTATUS: Mode not changed");
				}
			} /* if(APP_MODE_ERROR... */
		} /* end of if(MODE_IDLE... */
		else { /* Set to IDLE mode is not allowed */
			APP_LOG("Jarden", LOG_ERR, "SETJARDENSTATUS: Invalid mode value");
		}

		/* Wait for a sec before setting the time */
		//usleep(1000*1000); /* 1 sec */

		sprintf(szCurState, "%d", delayTime);

		/* As per new requirements, time set can be performed even in Warm state and shoudnt send time value
		 * to WASP if we receive 0 
		 */
		if(MAX_COOK_TIME != delayTime) 
		{
			APP_LOG("UPNPDevice", LOG_ERR, "Inside if((tempmode == MODE_LOW) || (tempmode == MODE_HIGH))");
			allowSetTime = TRUE;
			//g_timerFlag = 1;	
		}
		else 
		{
			allowSetTime = FALSE;
			//g_timerFlag = 0;	
		}

		if((APP_TIME_ERROR != delayTime) && (TRUE == allowSetTime)) 
		{
			retstatus=0;
			do 
			{
				int CookTime = delayTime;
				APP_LOG("UPNPDevice", LOG_ERR, "CookTime is : %d ", CookTime);
				memset(&WaspCookTime,0,sizeof(WaspVariable));
				WaspCookTime.ID = CROCKPOT_VAR_COOKTIME;
				WaspCookTime.Type = WASP_VARTYPE_TIME_M16;
				WaspCookTime.Val.U16 = CookTime;

				if((retstatus = WASP_SetVariable(&WaspCookTime)) != WASP_OK) {
					if(retstatus >= 0) {
						retstatus = -retstatus;
					}
				}
			} while(FALSE);

			APP_LOG("UPNPDevice", LOG_ERR, "SETJARDENSTATUS: Return status: %d", retstatus);


			//retstatus = HAL_SetCookTime(delayTime);
			if (0x00 == retstatus) {
				/* Is internal broadcast is required? */
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x00;
				/* UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetJardenStatus",
				   CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "time", szCurState); */

				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetCrockpotState",
						CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "time", szCurState);

				APP_LOG("UPNPDevice", LOG_ERR, "SETJARDENSTATUS: Time changed to: %d minutes", delayTime);
				g_timerFlag = 1;

			}
			else {
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = SET_CPTIME_FAILURE;
				/* UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetJardenStatus",
				   CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "time", "Error");*/

				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetCrockpotState",
						CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "time", "Error");

				APP_LOG("UPNPDevice", LOG_ERR, "SETJARDENSTATUS: Time not changed\n");
			}
		} /* end of if(APP_TIME_ERROR... */
		else {
			APP_LOG("Jarden", LOG_ERR, "SETJARDENSTATUS: Time set not performed\n");
		}

		if(g_phoneFlag == 1 && g_timerFlag == 1)
		{
			DataUsageTableUpdate(2); 
		}
		else if (g_phoneFlag == 1 && g_timerFlag != 1)
		{
			DataUsageTableUpdate(1); 
		}
	}
	else if ( (g_eDeviceType == DEVICE_SBIRON) ||
			(g_eDeviceType == DEVICE_MRCOFFEE) ||
			(g_eDeviceType == DEVICE_PETFEEDER) 
		)
	{
		retstatus =0;
		if(MODE_UNKNOWN != jardenMode) {

			/* Call HAL API's to set mode and set time for a crockpot */
			memset(&WaspMode,0,sizeof(WaspVariable));
			WaspMode.ID = CROCKPOT_VAR_MODE;
			WaspMode.Type = WASP_ENUM;
			WaspMode.Val.Enum = convertModeValue(jardenMode,TO_CROCKPOT);

			if((Err = WASP_SetVariable(&WaspMode)) != WASP_OK) {
				if(Err >= 0) {
					Err = -Err;
				}
				retstatus = Err;
			}
			//retstatus = HAL_SetMode(jardenMode);
			if (0x00 == retstatus) {
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0;
				/* UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetJardenStatus",
				   CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "mode", szCurState);*/

				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetCrockpotState",
						CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "mode", szCurState);

				APP_LOG("UPNPDevice", LOG_ERR, "SetJardenStatus: Mode changed to: %d mode", jardenMode);
				g_phoneFlag = 1;
			}
			else {
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0;
				/* UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetJardenStatus",
				   CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "mode", "Error"); */

				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetCrockpotState",
						CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "mode", "Error");

				APP_LOG("UPNPDevice", LOG_ERR, "SetJardenStatus: Mode not changed");
			}
		} /* end of if(MODE_IDLE... */
		else { /* Set to IDLE mode is not allowed */
			APP_LOG("Jarden", LOG_ERR, "SetJardenStatus: Invalid mode value");
		}

		/* Wait for a sec before setting the time */
		//usleep(1000*1000); /* 1 sec */

		sprintf(szCurState, "%d", delayTime);

		/* Allow setting of time only in power on mode */
		if((jardenMode == MODE_ON) || (jardenMode == MODE_OFF)) {
			allowSetTime = TRUE;
			//g_timerFlag = 1;
		}
		else {
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
					/* LOG("%s: Error, requested variable ID 0x%x, got 0x%x\n",
					   __FUNCTION__,CROCKPOT_VAR_MODE,WaspMode.ID); */
				}
				curMode = convertModeValue(WaspMode.Val.Enum, FROM_CROCKPOT);
			}
			//curMode = HAL_GetMode();
			if((curMode == APP_MODE_ON) || (curMode == APP_MODE_OFF)) {
				allowSetTime = TRUE;
				//g_timerFlag = 1;
			}
			else {
				allowSetTime = FALSE;
				//g_timerFlag = 0;
			}
		}

		if((0 < delayTime) && (MAX_DELAY_TIME >= delayTime) && (TRUE == allowSetTime)) {
			retstatus =0;
			do 
			{
				if(delayTime < 0 || delayTime > MAX_DELAY_TIME) 
				{
					// Argment out of range
					retstatus = -EINVAL;
					break;
				}
				if(curMode != MODE_LOW && curMode != MODE_HIGH)
				{
					retstatus = ERR_HAL_WRONG_MODE;
					break;
				}

				memset(&WaspCookTime,0,sizeof(WaspVariable));
				WaspCookTime.ID = CROCKPOT_VAR_COOKTIME;
				WaspCookTime.Type = WASP_VARTYPE_TIME_M16; //WASP_INT32;
				WaspCookTime.Val.U16 = delayTime;

				APP_LOG("Jarden", LOG_ERR, "delayTime is %d", delayTime);

				if((retstatus = WASP_SetVariable(&WaspCookTime)) != WASP_OK) {
					if(retstatus >= 0) {
						retstatus = -retstatus;
					}
				} 
			} while(FALSE);

			//retstatus = HAL_SetDelayTime(delayTime);
			if (0x00 == retstatus) {
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x00;
				/* UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetJardenStatus",
				   CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "time", szCurState); */

				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetCrockpotState",
						CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "time", szCurState);

				APP_LOG("UPNPDevice", LOG_ERR, "SetJardenStatus: Time changed to: %d minutes", delayTime); 
                                g_timerFlag = 1;

			}
			else {
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = SET_CPTIME_FAILURE;
				/* UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetJardenStatus",
				   CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "time", "Error"); */

				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetCrockpotState",
						CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "time", "Error");

				APP_LOG("UPNPDevice", LOG_ERR, "SetJardenStatus: Time not changed\n");  
			}
		} /* end of if(0 < delayTime... */
		else {
			APP_LOG("Jarden", LOG_ERR, "SetJardenStatus: Time set not performed\n"); 
		}
	}
	else
	{
		/* WeMo Smart, Do Nothing */	
	}

	FreeXmlSource(parammode);
	FreeXmlSource(paramtime);

	/*APP_LOG("UPNPDevice", LOG_DEBUG, "SetJardenStatus: End of set operation");*/
	return UPNP_E_SUCCESS;
}

/* GetJardenStatus function is to change the Jarden Device settings 
 * This function make use of HAL_GetMode() & HAL_GetCooktime() functions from HAL layer
 * pActionRequest - Handle to request
 *
 */
int GetJardenStatus(pUPnPActionRequest pActionRequest, 
                     IXML_Document *request, 
                     IXML_Document **out, 
                     const char **errorString)
{
    int jardenMode=0;
    int tempmode=0, delayTime=0;
	unsigned short int cookedTime = 0;
    char getStatusRespMode[16];
    char getStatusRespTime[16];
    char getCookedTime[32];
    int ret;
	WaspVariable WaspMode, WaspCookTime, WaspCookedTime;
	int Err;

    
    memset(getStatusRespMode, 0x00, sizeof(getStatusRespMode));
    memset(getStatusRespTime, 0x00, sizeof(getStatusRespTime));
    memset (getCookedTime, 0x00, sizeof(getCookedTime));
    
    if (NULL == pActionRequest || NULL == request)
    {
        APP_LOG("UPNPDevice", LOG_DEBUG, "GetJardenStatus: command paramter invalid");
        return GET_PARAMS_NULL;
    }

    /* Get the current mode setting */
		memset(&WaspMode,0,sizeof(WaspVariable));
		WaspMode.ID = CROCKPOT_VAR_MODE;
		WaspMode.State = VAR_VALUE_LIVE;
		
		if((Err = WASP_GetVariable(&WaspMode)) != WASP_OK) 
		{
			if(Err >= 0) {
				Err = -Err;
			}
			jardenMode = 0x00; //Err; //if WASP values are negative i.e. not responding, so that App can display Off
		}
		else 
		{
			if(WaspMode.ID != CROCKPOT_VAR_MODE) 
			{
				APP_LOG("Crockpot Error", LOG_DEBUG, "requested variable ID %d got %d",CROCKPOT_VAR_MODE,WaspMode.ID);
				/*LOG("%s: Error, requested variable ID 0x%x, got 0x%x\n",
					__FUNCTION__,CROCKPOT_VAR_MODE,WaspMode.ID);*/
			}
			jardenMode = convertModeValue(WaspMode.Val.Enum, FROM_CROCKPOT);
		}
	//jardenMode = HAL_GetMode();
    
    APP_LOG("UPNP: Device", LOG_DEBUG, "GetJardenStatus:jardenMode %d", jardenMode);
    if (0x00 > jardenMode)
    {
        pActionRequest->ActionResult = NULL;
        pActionRequest->ErrCode = GET_CPMODE_FAILURE;
        /* UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetJardenStatus",
                                CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "mode", "Error"); */

        UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetCrockpotState",
                                CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "mode", "Error");

        APP_LOG("UPNPDevice", LOG_ERR, "GetJardenStatus: Unable to get the mode: ");  
    }
        
    if (DEVICE_CROCKPOT == g_eDeviceType)
    {
        /* Get the time information */
        memset(&WaspCookTime,0,sizeof(WaspVariable));
		WaspCookTime.ID = CROCKPOT_VAR_COOKTIME;
		WaspCookTime.Type = WASP_VARTYPE_TIME_M16;

		if((Err = WASP_GetVariable(&WaspCookTime)) != WASP_OK) {
			if(Err >= 0) {
				Err = -Err;
			}
			delayTime = 0x00; //Err; //if WASP values are negative i.e. not responding, so that App can display Off
		}
		else {
			if(WaspCookTime.ID != CROCKPOT_VAR_COOKTIME) {
				APP_LOG("Crockpot Error", LOG_DEBUG, "requested variable ID %d got %d",CROCKPOT_VAR_COOKTIME,WaspCookTime.ID);
				/* LOG("%s: Error, requested variable ID 0x%x, got 0x%x\n",
					__FUNCTION__,CROCKPOT_VAR_COOKTIME,WaspCookTime.ID); */
			}
			delayTime = WaspCookTime.Val.U16;
		}
		//delayTime = HAL_GetCookTime();

        tempmode = jardenMode; //convertModeValue(jardenMode,FROM_CROCKPOT);
		//delayTime = 0;
		APP_LOG("UPNP: Device", LOG_DEBUG, "GetJardenStatus:delaytime is  %d and tempmode is %d", delayTime,tempmode);
        sprintf(getStatusRespMode, "%d",tempmode);
    }
    else if ( (g_eDeviceType == DEVICE_SBIRON) ||
	          (g_eDeviceType == DEVICE_MRCOFFEE) ||
			  (g_eDeviceType == DEVICE_PETFEEDER) 
			)
    {
        /* Get the time information */
        memset(&WaspCookTime,0,sizeof(WaspVariable));
		WaspCookTime.ID = CROCKPOT_VAR_COOKTIME;
		WaspCookTime.Type = WASP_VARTYPE_TIME_M16; //WASP_INT32;

		if((Err = WASP_GetVariable(&WaspCookTime)) != WASP_OK) {
			if(Err >= 0) {
				Err = -Err;
			}
			delayTime = Err;
		}
		else {
			if(WaspCookTime.ID != CROCKPOT_VAR_COOKTIME) {
				APP_LOG("Crockpot Error", LOG_DEBUG, "requested variable ID %d got %d",CROCKPOT_VAR_COOKTIME,WaspCookTime.ID);
				/* LOG("%s: Error, requested variable ID 0x%x, got 0x%x\n",
					__FUNCTION__,CROCKPOT_VAR_COOKTIME,WaspCookTime.ID); */
			}
			delayTime = WaspCookTime.Val.I32;
		}
		delayTime = 0;
		//delayTime = HAL_GetDelayTime();  /* Currently using Cooktime logic of WASP as Delay time requirements are not there */
        sprintf(getStatusRespMode, "%d",jardenMode);
    }
	else
	{
		/* WeMo Smart, Do nothing */
	}

    if (0x00 > delayTime)
    {
        pActionRequest->ActionResult = NULL;
        pActionRequest->ErrCode = GET_CPTIME_FAILURE;
        /* UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetJardenStatus",
                                CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "time", "Error"); */

        UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetCrockpotState",
                                CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "time", "Error");

        APP_LOG("UPNPDevice", LOG_ERR, "GetJardenStatus: Time not changed, current time: ");
    }

    /* ret = UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetJardenStatus",
                                  CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "mode", getStatusRespMode); */


    ret = UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetCrockpotState",
                                  CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "mode", getStatusRespMode);

    APP_LOG("UPNP: Device", LOG_DEBUG, "GetJardenStatus:Mode %s", getStatusRespMode);

    sprintf(getStatusRespTime, "%d",delayTime);

    /* ret = UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetJardenStatus",
                                  CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "time", getStatusRespTime); */

    ret = UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetCrockpotState",
                                  CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "time", getStatusRespTime);

    APP_LOG("UPNP: Device", LOG_DEBUG, "GetJardenStatus:Time %s minutes", getStatusRespTime);
    
	/* Get the cooked time information */
	memset(&WaspCookedTime,0,sizeof(WaspVariable));
	WaspCookedTime.ID = CROCKPOT_VAR_COOKEDTIME;
	WaspCookedTime.Type = WASP_VARTYPE_TIME_M16; //WASP_INT32;

	if((Err = WASP_GetVariable(&WaspCookedTime)) != WASP_OK) {
		if(Err >= 0) {
			Err = -Err;
		}
		cookedTime = Err;
	}
	else {
		if(WaspCookedTime.ID != CROCKPOT_VAR_COOKEDTIME) {
			APP_LOG("Crockpot Error", LOG_DEBUG, "requested variable ID %d got %d",CROCKPOT_VAR_COOKEDTIME,WaspCookedTime.ID);
			/* LOG("%s: Error, requested variable ID 0x%x, got 0x%x\n",
				__FUNCTION__,CROCKPOT_VAR_COOKEDTIME,WaspCookedTime.ID); */
		}
		cookedTime = WaspCookedTime.Val.U16;
	}
	
	sprintf(getCookedTime, "%hd",cookedTime);
	
    ret = UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetCrockpotState",
                                  CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "cookedTime", getCookedTime);

    APP_LOG("UPNP: Device", LOG_DEBUG, "GetJardenStatus:cookedTime %s minutes", getCookedTime);


    return UPNP_E_SUCCESS;
}

void jardenStatusChange(int mode, int time, unsigned short int cookedTime)
{

    if((DEVICE_CROCKPOT == g_eDeviceType)){
       
       APP_LOG("UPNP", LOG_DEBUG, "Inside if((DEVICE_CROCKPOT == g_eDeviceType of jardenstatuschange mode is %d, time is %d, cooked time is %hd",mode,time, cookedTime);
       LocalJardenStateNotify(mode, time, cookedTime);
    }
    else if ( (g_eDeviceType == DEVICE_SBIRON) ||
	          (g_eDeviceType == DEVICE_MRCOFFEE) ||
			  (g_eDeviceType == DEVICE_PETFEEDER) 
			)
	{
   
        APP_LOG("UPNP", LOG_DEBUG, "Inside else((DEVICE_CROCKPOT == g_eDeviceType of jardenstatuschange");
		LocalBinaryStateNotify(mode);
    }
	else
	{
		/* WeMo Smart, Do nothing */
	}

    //LocalJardenStateNotify(mode, time);
    LocalBinaryStateNotify(mode);
    return;
}

/*---------- JArden Device Status change notify -------*/
void LocalJardenStateNotify(int jMode, int jDelayTime, unsigned short int jcookedTime)
{
    APP_LOG("UPNP", LOG_DEBUG, "Inside LocalJardenStateNotify mode received is %d and delay/cooktime is %d and cooked time is %hd",jMode,jDelayTime,jcookedTime );
    int ret=-1;
    int tempMode=0;
    char* szCurState[3];
    char* paramters[3];
    
    if (!IsUPnPNetworkMode())
    {
        //- Not report since not on router or internet
        APP_LOG("UPNP", LOG_DEBUG, "Notification:Jarden Device Status: Not in home network, ignored");
        return;
    }
    
    szCurState[0] = (char*)ZALLOC(sizeof(jMode));
    szCurState[1] = (char*)ZALLOC(sizeof(jDelayTime));
    szCurState[2] = (char*)ZALLOC(sizeof(jcookedTime));
    
    paramters[0] = (char*)ZALLOC(20);
    paramters[1] = (char*)ZALLOC(20);
    paramters[2] = (char*)ZALLOC(20);	
    
	
    if(DEVICE_CROCKPOT == g_eDeviceType)
    {
		APP_LOG("UPNP", LOG_DEBUG, "Inside LocalJardenStateNotify if(DEVICE_CROCKPOT == g_eDeviceType)");
        tempMode = jMode;
        sprintf(szCurState[0], "%d", tempMode);
		APP_LOG("UPNP", LOG_DEBUG, "LocalJardenStateNotify tempMode being set is %s",szCurState[0]);
        sprintf(szCurState[1], "%d", jDelayTime);
		APP_LOG("UPNP", LOG_DEBUG, "LocalJardenStateNotify jDelayTime being set is %s",szCurState[1]);
        sprintf(szCurState[2], "%hd", jcookedTime);
		APP_LOG("UPNP", LOG_DEBUG, "LocalJardenStateNotify jcookedTime being set is %s",szCurState[2]);
		
        sprintf(paramters[0], "%s", "mode");
		APP_LOG("UPNP", LOG_DEBUG, "LocalJardenStateNotify mode being set is %s",paramters[0]);
        sprintf(paramters[1], "%s", "time");
		APP_LOG("UPNP", LOG_DEBUG, "LocalJardenStateNotify time being set is %s",paramters[1]);
        sprintf(paramters[2], "%s", "cookedTime");
		APP_LOG("UPNP", LOG_DEBUG, "LocalJardenStateNotify cookedTime being set is %s",paramters[2]);
	}
    else if ( (g_eDeviceType == DEVICE_SBIRON) ||
	          (g_eDeviceType == DEVICE_MRCOFFEE) ||
			  (g_eDeviceType == DEVICE_PETFEEDER) 
			)
    {
		sprintf(szCurState[0], "%d", jMode);
		APP_LOG("UPNP", LOG_DEBUG, "LocalJardenStateNotify jMode being set is %s",szCurState[0]);
		sprintf(szCurState[1], "%d", jDelayTime);
		APP_LOG("UPNP", LOG_DEBUG, "LocalJardenStateNotify jDelayTime being set is %s",szCurState[1]);

        sprintf(paramters[0], "%s", "BinaryState");
		APP_LOG("UPNP", LOG_DEBUG, "LocalJardenStateNotify BinaryState being set is %s",paramters[0]);
        sprintf(paramters[1], "%s", "time");
		APP_LOG("UPNP", LOG_DEBUG, "LocalJardenStateNotify time being set is %s",paramters[1]);
    }
	else
	{
		/* WeMo smart, Do nothing */
	}

    APP_LOG("UPNP", LOG_DEBUG, "LocalJardenStateNotify before UpnpNotify");

    ret = UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].UDN, 
                     SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].ServiceId, (const char **)paramters, (const char **)szCurState, 0x03);
    if(UPNP_E_SUCCESS != ret)
    {
        APP_LOG("UPNP", LOG_DEBUG, "########## UPnP Notification ERROR #########");
    }
    else {
        APP_LOG("UPNP", LOG_DEBUG, "Notification:- mode is:: %s, ##### time is:: %s", szCurState[0],szCurState[1]);
    }

    free(szCurState[0]);
    free(szCurState[1]);
	free(szCurState[2]);
    free(paramters[0]);
    free(paramters[1]);
	free(paramters[2]);

    APP_LOG("UPNP", LOG_DEBUG, "LocalJardenStateNotify After freeing szcurstate and parameters");
    /* Push to the cloud */
    {
        remoteAccessUpdateStatusTSParams(1);
	APP_LOG("UPNP", LOG_DEBUG, "LocalJardenStateNotify after remoteAccessUpdateStatusTSParams");
    }
    return;
}
#endif /* #ifdef PRODUCT_WeMo_Jarden */
