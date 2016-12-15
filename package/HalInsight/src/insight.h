/***************************************************************************
*
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
/* 
   This header defines the public interface to the Hardware Abstraction
   Layer for the Insight product.
*/ 

#ifndef _INSIGHT_H_
#define _INSIGHT_H_

#ifndef ERR_HAL_BASE
#define ERR_HAL_BASE								10000
#endif

#define ERR_HAL_INVALID_RESPONSE_TYPE		(ERR_HAL_BASE + 1)
#define ERR_HAL_DAEMON_TIMEOUT				(ERR_HAL_BASE + 2)

typedef struct {
	unsigned int vRMS;		// units: millivolts
	unsigned int iRMS;		// units: milliamps
	unsigned int Watts;		// units: milliwatts
	unsigned short PF;		// units: .01 %
	unsigned short Freq;		// Units: .01 Hz
	int InternalTemp;			// units: .01 degrees C
	int ExternalTemp;			// units: .01 degrees C
} DataValues;

typedef struct {
	unsigned int WattMinutes;
	unsigned int TotalMinutes;		// Number of minutes over which WattMinutes has acculated
	unsigned int RunTimeSeconds;	// Daemon run time
} TotalData;

typedef struct {
    unsigned int TodayONTime;// Used in calculating parameters based on this
    unsigned int TodayONTimeTS;// Used to display on App. can't use g_TodayONTime due to sleep delays in computation
    unsigned int TodaySBYTime;// Used for data export
    unsigned int TodaySBYTimeTS;// Used for data export
    unsigned int AccumulatedWattMinute;// This is KWH 
    unsigned int TodayKWHON;// Used in calculation as well as display on App
    unsigned int TodayKWHSBY;// Used for data export
    double       TotalKWHSinceIsInsightUP;
    unsigned int Last30MinKWH;
    unsigned int ONForTS;
    unsigned int StateChangeTS;
    double	 KWHONFor;
    double       ONForCalc;
    unsigned long int LastTimeStamp;
    unsigned int LastRuntime;
    double LastKWHconsumed;
    unsigned int LastDeviceState;
    unsigned int ONFor;
    unsigned long LastFlashWrite;
    unsigned int  LastRelayState;
} InsightVars;

// Initialize Hardware Abstraction Layer for use
// Returns:
// 	  0 - SUCCESS
// 	< 0 - standard Linux errno error code
//		> 0 - HAL error code (see ERR_HAL_*)
int HAL_Init();


// Get current (Instantaneous) values
// Parameter:
// 	p - pointer to structure to receive data
// 
// Returns:
// 	  0 - SUCCESS
// 	< 0 - standard Linux errno error code
//		> 0 - HAL error code (see ERR_HAL_*)
int HAL_GetCurrentReadings(DataValues *p);

// Get values averaged over a number of seconds
// (default is 10, may be changed by daemon command line value)
// 
// Parameter:
// 	p - pointer to structure to receive data
// 
// Returns:
// 	  0 - SUCCESS
// 	< 0 - standard Linux errno error code
//		> 0 - HAL error code (see ERR_HAL_*)
int HAL_GetAverageReadings(DataValues *p);

// Get total power consumed since daemon has been running or
// the total was last cleared.
// 
// Parameter:
// 	p - pointer to structure to receive data
// 
// Returns:
// 	  0 - SUCCESS
// 	< 0 - standard Linux errno error code
//		> 0 - HAL error code (see ERR_HAL_*)
int HAL_GetTotalPower(TotalData *p);

// Get total power consumed since daemon has been running or
// the total was last cleared then clear the total.
// 
// Parameter:
// 	p - pointer to structure to receive data
// 
// Returns:
// 	  0 - SUCCESS
// 	< 0 - standard Linux errno error code
//		> 0 - HAL error code (see ERR_HAL_*)
int HAL_GetAndClearTotalPower(TotalData *p);

// Shutdown the HAL deamon. 
// 
// Returns:
// 	  0 - SUCCESS
// 	< 0 - standard Linux errno error code
//		> 0 - HAL error code (see ERR_HAL_*)
int HAL_Shutdown();


// Free and release resources used by HAL
// 
// Returns:
// 	  0 - SUCCESS
// 	< 0 - standard Linux errno error code
//		> 0 - HAL error code (see ERR_HAL_*)
int HAL_Cleanup();

int HAL_GetInsightVariables(InsightVars *p);

int HAL_SetInsightVariables(InsightVars *p);

#endif	// _INSIGHT_H_

