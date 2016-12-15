/***************************************************************************
*
*
* fwDl.c
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
#include "global.h"
#include "logger.h"
#include "httpsWrapper.h"
#if defined(PRODUCT_WeMo_Baby) || defined(PRODUCT_WeMo_Streaming)
#include "gpioApi.h"
#include "wifiSetup.h"
#endif
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "fwDl.h"
#include "defines.h"

typedef int(*f)(char*, char*);

int gFwUploadStatus;

/************************************************************************
 * Function: upgradeFlash

 *    Placeholder will be replaced with the Gemtek API
 *  Parameters:
 *    dlUrl  = download url 
 *    dlPath = download path on the board
 *    pFunc = callback func to set firmware status 
 *  Return:
 *    Void
 ************************************************************************/
void upgradeFlash( char *dlUrl, char *dlPath, CallbackFunc pFunc)
{
    int retVal = 0;

    if (dlUrl) 
    {
	    APP_LOG("Firmware", LOG_DEBUG,"Going to download firmware image from %s\n", dlUrl);
	    retVal = do_download(dlUrl, dlPath);
	    if (retVal != 0) {
		    APP_LOG("Firmware", LOG_CRIT,"Download firmware image  %s failed\n", dlUrl);
	    }else {
		    APP_LOG("Firmware", LOG_CRIT,"Downloaded firmware image from location %s successfully\n", dlUrl);
	    }
    }
    else {
#if defined(PRODUCT_WeMo_Baby) || defined(PRODUCT_WeMo_Streaming)
	SetWiFiGPIOLED(0x04);
#else
	SetWiFiLED(0x04);
#endif
	retVal = FAILURE;
    }

    pFunc( retVal );    

}

/************************************************************************
 * Function: retrieveUploadStatus

 *    Placeholder will be replaced with the Gemtek API
 *  Parameters:
 *    dlUrl  = download url 
 *    dlPath = download path on the board
 *    pFunc = callback func to set firmware status 
 *  Return:
 *    Void
 ************************************************************************/
int retrieveUploadStatus(int status)
{

    APP_LOG("Firmware", LOG_DEBUG,"status of firmware upload is %d\n", status);
    gFwUploadStatus = status;
    return status;
}

/************************************************************************
 * Function: downloadFirmware

 *    Downloads the firmware from url received from plugin
 *  Parameters:
 *    firmware : firmware parameters
 *  Return:
 *    Void
 ************************************************************************/
int downloadFirmware(char *dlUrl, char *dlPath)
{

    APP_LOG("Firmware", LOG_DEBUG,"Download url is %s, dlPath %s \n", dlUrl, dlPath);
    upgradeFlash( dlUrl, dlPath, retrieveUploadStatus);
    return 0;
}
