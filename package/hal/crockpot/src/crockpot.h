/***************************************************************************
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
***************************************************************************/
#ifndef _CROCKPOT_H_
#define _CROCKPOT_H_

#define MAX_COOK_TIME   (20*60)  

// Hardware abstraction layer specific errors
#ifndef ERR_HAL_BASE
#define ERR_HAL_BASE                      -10100
#endif

// Internal HAL error: HAL Daemon returned an unexpected response
#define ERR_HAL_INVALID_RESPONSE_TYPE     (ERR_HAL_BASE - 1)

// Communications with HAL Daemon failed.
#define ERR_HAL_DAEMON_TIMEOUT            (ERR_HAL_BASE - 2)

// Attempted to set cook time when temperature not set
#define ERR_HAL_WRONG_MODE                (ERR_HAL_BASE - 3)

typedef enum {
   MODE_UNKNOWN = 1,
   MODE_OFF,
   MODE_IDLE,       // just turned on, no temp or time set yet
   MODE_WARM,
   MODE_LOW,
   MODE_HIGH,
} CROCKPOT_MODE;

// Initialize Hardware Abstraction Layer for use
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_Init();

// Return the current mode decoded from the product LEDS.
//    < 0 - HAL error code (see ERR_HAL_*)
CROCKPOT_MODE HAL_GetMode(void);

// Set cooking mode
// Legal values are MODE_OFF, MODE_WARM, MODE_LOW or MODE_HIGH.
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_SetMode(CROCKPOT_MODE Mode);

// Return number of minutes of cook time remaining from product LED display. 
// A value of 0 indicates that the cockpot is not in a cooking mode
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_GetCookTime(void);

// Set number of minutes to cook time at currently selected temperature.
// The current mode must be MODE_LOW or MODE_HIGH to set a cook time. 
//  
// Note: requested value will be rounded up to nearest 30 minute increment
// (CrockPot hardware limitations) 
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_SetCookTime(int Minutes);

// Return interface card firmware version number.
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_GetFWVersionNumber(char **pBuf);

// Shutdown the HAL deamon. 
// 
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_Shutdown();


// Free and release resources used by HAL
// 
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_Cleanup();

// Return return string describing error number
const char *HAL_strerror(int Err);

#endif  // _CROCKPOT_H_
