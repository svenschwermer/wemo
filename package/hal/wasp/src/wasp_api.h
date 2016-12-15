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
#ifndef _WASP_API_H_
#define _WASP_API_H_

#ifndef _WASP_H_
#include "wasp.h"
#endif

// WASP layer specific errors
#ifndef WASP_ERR_BASE
#define WASP_ERR_BASE                        -10200
#endif

// Internal WASP error: WASP Demon returned an unexpected response
#define WASP_ERR_INVALID_RESPONSE_TYPE    (WASP_ERR_BASE - 1)

// Communications with WASP Demon failed.
#define WASP_ERR_DAEMON_TIMEOUT           (WASP_ERR_BASE - 2)

// WASP Demon Communications with interface board failed.
#define WASP_ERR_COMM_ERR                 (WASP_ERR_BASE - 3)

// WASP is not ready to process the requested function
// 
// It takes time for WASP to establish communications with the
// device interface and to discover the variables supported by the
// device.  This error code means that a WASP API function was
// called before WASP is ready to handle the request. 
// When this error is returned it means that the caller should
// delay some time (1 second or so) and then retry the call.
#define WASP_ERR_NOT_READY                (WASP_ERR_BASE - 4)

// The requested variable does not exist
// or a NULL was passed for a variable pointer
#define WASP_ERR_INVALID_VARIABLE         (WASP_ERR_BASE - 5)

// An attempt was made to write to a read only variable
// For example a variable with a usage type of WASP_VALUE_FIXED
// or WASP_VALUE_MONITORED.
#define WASP_ERR_READONLY                 (WASP_ERR_BASE - 6)

// The variable value is outside of the defined minimum and maximum range.
// If this error is returned to a set call the value is not set.
// If this error is returned to a get call the value is returned anyway.
#define WASP_ERR_INVALID_VALUE            (WASP_ERR_BASE - 7)

// The data returned by WASP_GetChunk() or WASP_GetChunkByName() does
// not include the last byte of data.
#define WASP_ERR_MORE_DATA                (WASP_ERR_BASE - 8)

// No data to return 
#define WASP_ERR_NO_DATA                  (WASP_ERR_BASE - 9)

// The requested function/data is not supported by the
// device interface card.
#define WASP_ERR_NOT_SUPPORTED            (WASP_ERR_BASE - 10)

// An internal error has occured in waspd
#define WASP_ERR_WASPD_INTERNAL           (WASP_ERR_BASE - 11)

// The WaspVariable VarType is not correct for the specified VarID
#define WASP_ERR_WRONG_TYPE               (WASP_ERR_BASE - 12)

// The WaspVariable VarType is invalid
#define WASP_ERR_INVALID_TYPE             (WASP_ERR_BASE - 13)

// No response from device
#define WASP_ERR_NO_RESPONSE              (WASP_ERR_BASE - 14)

// Another task/thread is waiting in WASP_GetChangedVar()
#define WASP_ERR_WAIT_BUSY                (WASP_ERR_BASE - 15)

// Variable types
#define WASP_ENUM                0x01
#define WASP_PERCENT             0x02
#define WASP_TEMPERATURE         0x03
#define WASP_TIME_TENS           0x04
#define WASP_TIME_SECS           0x05
#define WASP_BCD_TIME            0x06
#define WASP_BOOL                0x07
#define WASP_BCD_DATA            0x08
#define WASP_BCD_DATA_TIME       0x09
#define WASP_STRING              0x0a
#define WASP_BLOB                0x0b
#define WASP_UINT8               0x0c
#define WASP_INT8                0x0d
#define WASP_UINT16              0x0e
#define WASP_INT16               0x0f
#define WASP_UINT32              0x10
#define WASP_INT32               0x11
#define WASP_TIME_MINS           0x12


// Button presses: ID in bottom 7 bits,
// MSB: 1 - Button pressed,
//      0 - Button released
#define BUTTON_ID_MASK           0x7f
#define BUTTON_PRESSED           0x80

#ifndef GCC_PACKED
#if defined(__GNUC__)
#define GCC_PACKED __attribute__ ((packed))
#else
#define GCC_PACKED
#endif
#endif // GCC_PACKED

typedef unsigned char VarID;
typedef unsigned char VarType;
typedef unsigned char VarUsage;
typedef unsigned char LedID;

typedef struct {
   int Len;                // length in bytes of blob data
   unsigned char *Data;    // pointer to blob data
} BlobVar;

typedef struct {
   unsigned short Count;   // Number of Enum names in name array  
   char *Name[];           //
} GCC_PACKED EnumVarNames;

typedef union {
   unsigned char Enum;           // type 1
   unsigned short Percent;       // type 2 - % of full 0.0 to 100.0
   short Temperature;            // type 3 - -3278.8 F to +3276.7 F
   int TimeTenths;               // type 4 - time in +/- .1 sec steps
   unsigned short TimeSecs;      // type 5 - time in +/- 1 sec steps
   unsigned char BcdTime[3];     // type 6 - SS:MM:HH in BCD
   unsigned char Boolean;        // type 7 - 0 or 1 / On or Off
   unsigned char BcdDate[4];     // type 8 - dd/mm/yyyy in BCD
   unsigned char BcdDateTime[7]; // type 9 - ss:mm:hh dd/mm/yy in BCD
   char *String;                 // type 0xa - pointer to a String value
   BlobVar Blob;                 // type 0xb - Blob
   unsigned char U8;             // type 0x0c - WASP_VARTYPE_UINT8
   signed char I8;               // type 0x0d - WASP_VARTYPE_INT8
   unsigned short U16;           // type 0x0e - WASP_VARTYPE_UINT16
   signed short I16;             // type 0x0f - WASP_VARTYPE_INT16
   unsigned int U32;             // type 0x10 - WASP_VARTYPE_UINT32
   int I32;                      // type 0x11 - WASP_VARTYPE_INT32
   unsigned short TimeMins;      // type 0x12 - time in 1 minute steps
} WaspValUnion;

typedef enum {
   VAR_VALUE_UNKNOWN,   // variable has not been accessed
   VAR_VALUE_CACHED,
   VAR_VALUE_LIVE,      // variable has just been read from device
   VAR_VALUE_SET,       // variable sent to the device
   VAR_VALUE_SETTING    // variable is in the process of being sent to device
} VarState;
#define NUM_VAL_STATES VAR_VALUE_SETTING

typedef struct {
   int   Count;         // number of times pseudo variable has been met
   time_t   TimeStamp;  // UTC time when Pseudo variable condition was last met
   int   bPseudoVar:1;  // This variable is a Pseudo variable
   int   bRestored:1;   // Previous value has restored from nvram, db, etc.
   int   bInited:1;     // Tracked value has been read once
   int   bInRange:1;    // Last value was in range
   int   bChanged:1;    // value changed since last read
} PseudoInfo; 

typedef struct {
   VarID ID;
   VarType Type;
   VarState State;
   PseudoInfo Pseudo;
   WaspValUnion Val; // NB must be last
} WaspVariable;

typedef struct {
   VarID ID;
   VarType Type;
   VarUsage Usage;
   PollPseudoUnion u;
   WaspValUnion MinValue;
   WaspValUnion MaxValue;
   char Name[MAX_VAR_NAME_LEN+1];
   EnumVarNames Enum;
} GCC_PACKED WaspVarDesc;

typedef struct {
   int Len;    // size of pAdditionData when present
   unsigned char Result;
   unsigned char AdditionData[];
} SelfTestResults;

typedef struct {
   #define LED_DURATION_INFINITE 0xffff
   unsigned short Duration;      // duration of this state in milliseconds
   unsigned char RedIntensity;   // 0 = off, 0xff = full on
   unsigned char GreenIntensity; // 0 = off, 0xff = full on
   unsigned char BlueIntensity;  // 0 = off, 0xff = full on
} GCC_PACKED LedState;


// Initialize WASP Layer for use
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_Init();

// Return a string describing error number.
// The returned string is statically allocated.
const char *WASP_strerror(int Err);

// Return interface card firmware version number.
// 
// pBuf:
// Pointer to character pointer to receive the value.
// Returned value is dynamically allocated via malloc(), 
// it the responsibility of the caller to free the memory.
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetFWVersionNumber(char **pBuf);

// Get list of supported variables
//
// int WASP_GetVarList(int *Count,WaspVarDesc ***pVarDescArray);
// 
// Count: pointer to an integer to receive the number of variable
// definitions returned in pVarDescList.
// 
// pVarDescArray: pointer to a pointer to an array containing the
// returned variable descriptions.  The returned array in dynamically 
// allocated memory that the caller is responsible for freeing.
// EnumVarNames are allocated within pVarDescArray and do not need
// to be freed separately.
//
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetVarList(int *Count,WaspVarDesc ***pVarDescArray);

// Read specified variable.
//
// int WASP_GetVariable(WaspVariable *pVar);
// 
// pVar: pointer to buffer to receive the value.
// 
// Filled by caller:
// pVar->ID is set by the caller to requested Variable's ID.
// 
// pVar->State:
//     VAR_VALUE_LIVE - get value from interface board
//     VAR_VALUE_CACHED - returned cached value when possible
// 
// Returned:
// pVar->State:
//     VAR_VALUE_LIVE   - get value from interface board
//     VAR_VALUE_CACHED - returned cached value when possible
//     VAR_VALUE_SET      variable sent to the device
//     VAR_VALUE_SETTING - variable is in the process of being sent to device
// 
// If the variable value is returned via a pointer in pVar->Val
// (string, blob) then the pointer is to dynamically allocated 
// memory that the caller is responsible for freeing.
// 
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetVariable(WaspVariable *pVar);

// Read specified variable.
//
// int WASP_GetVarByName(const char *Name,WaspValUnion *pVar)
// 
// Name: Name of the Variable to get
// Filled by caller:
// pVar->ID is set by the caller to requested Variable's ID.
// 
// pVar->State:
//     VAR_VALUE_LIVE - get value from interface board
//     VAR_VALUE_CACHED - returned cached value when possible
// 
// Returned:
// pVar->State:
//     VAR_VALUE_LIVE   - get value from interface board
//     VAR_VALUE_CACHED - returned cached value when possible
//     VAR_VALUE_SET      variable sent to the device
//     VAR_VALUE_SETTING - variable is in the process of being sent to device
// 
// If the variable value is returned via a pointer in pVar->Val
// (string, blob) then the pointer is to dynamically allocated 
// memory that the caller is responsible for freeing.
// 
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetVarByName(const char *Name,WaspVariable *pVar);

// Read the current value of 2 or more variables.
//
// int WASP_GetVariables(int Count,int bLive,WaspVariable **pVarArray);
//
// Filled by caller:
//    pVarArray:
//       Pointer to an array of pointers to WaspVariables with the ID field
//       filled in.  If the variable value is returned via a pointer in
//       WaspVariable (string, blob) then the pointer is to dynamically 
//       allocated memory that the caller is responsible for freeing.
//       A maximum of one Blob value may be read and if present it must be the 
//       last variable in the array.
// 
//    bLive:
//     0 - returned cached values when possible
//  != 0 - get values from interface board
// 
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetVariables(int Count,int bLive,WaspVariable **pVarArray);

// Read chunk of specified variable.
//
// int WASP_GetChunk(VarID Id,unsigned int Offset,unsigned int Len,
//                   unsigned char *pData);
// 
// Id: Variable Id of variable to get
//
// Offset: Offset from first byte of Variable to get.
// 
// Len: Number of bytes to get.
//
// pData: pointer to buffer to receive the data.
// 
// This function will return WASP_ERR_MORE_DATA if the last byte of the 
// variable wasn't read by this command.  In this case this not an error,
// it is only an indication that more data is available.
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetChunk(VarID Id,unsigned int Offset,unsigned int Len,
                  unsigned char *pData);

// Read chunk of specified variable.
//
// int WASP_GetChunkByName(const char *Name,unsigned int Offset,
//                         unsigned int Len,unsigned char *pData);
// 
// Name: Name of the Variable to get
//
// Offset: Offset from first byte of Variable to get.
// 
// Len: Number of bytes to get.
//
// pData: pointer to buffer to receive the data.
// 
// This function will return WASP_ERR_MORE_DATA if the last byte of the 
// variable wasn't read by this command.  In this case this not an error,
// it is only an indication that more data is available.
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetChunkByName(const char *Name,unsigned int Offset,
                        unsigned int Len,unsigned char *pData);

// Set specified variable.
// 
// int WASP_SetVariable(WaspVariable *pVar)
// 
// pVar: pointer to buffer to with the value to set.
// The ID, VarType and Val fields must be filled in appropriately
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_SetVariable(WaspVariable *pVar);

// Set the specified variable.
//
// int WASP_SetVarByName(const char *Name,WaspVariable *pVar);
// 
// Name: Name of the Variable to set
// 
// pVar: pointer to buffer to with the value to set.
// The VarType and Val fields must be filled in appropriately
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_SetVarByName(const char *Name,WaspVariable *pVar);

// Set specified variable.
// 
// int WASP_SetVariables(int Count, WaspVariable **pVarArray);
//
// pVarArray: pointer to an array of WaspVariables with the values to set.
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_SetVariables(int Count, WaspVariable *pVarArray);


// Set a chunk of specified variable.
//
// int WASP_SetChunk(VarID Id,unsigned int Offset,unsigned int Len,
//                   int bLast,unsigned char *pData);
// 
// Id: Variable Id of variable to set
//
// Offset: Offset from first byte of Variable to set.
// 
// Len: Number of bytes to set.
// 
// bLast: FALSE - More data to come
//        TRUE - this write complete a series of writes to this variable.
// It is up to the device to decide if it's appropriate to delay the 
// setting of variable until bLast is set or not.  
// 
// pData: pointer to buffer to with the data.
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_SetChunk(VarID Id,unsigned int Offset,unsigned int Len,
                  int bLast,unsigned char *pData);

// Set a chunk of specified variable by name.
//
// int WASP_SetChunkByName(const char *Name,unsigned int Offset,
//                         unsigned int Len,int bLast,unsigned char *pData);
// 
// Name: Name of the Variable to set
//
// Offset: Offset from first byte of Variable to set.
// 
// Len: Number of bytes to set.
// 
// bLast: FALSE - More data to come
//        TRUE - this write complete a series of writes to this variable.
// It is up to the device to decide if it's appropriate to delay the 
// setting of variable until bLast is set or not.  
// 
// pData: pointer to buffer to with the data.
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_SetChunkByName(const char *Name,unsigned int Offset,
                        unsigned int Len,int bLast,unsigned char *pData);

// Get button presses.
// 
// pButton: pointer to byte to receive button ID.
// Value will be 0 if no buttons have been pressed or released
// since the last call.
// 
// Returns:
//      0 - No Buttons have been Pressed or released since last call
//     >0 - Button ID & Press/released bit
//    < 0 - WASP error code (see WASP_ERR_*)
//
// WASP_ERR_NO_DATA - No buttons have changed state since last call.
int WASP_GetButton(unsigned char *pButtonID);

// Get Interface board selftest results
// int WASP_GetSelfTestResults(SelfTestResults **pResults);
//
// pResults: pointer to pointer to returned selftest results.
// Returned value is dynamically allocated via malloc(), 
// it the responsibility of the caller to free the memory.
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetSelfTestResults(SelfTestResults **pResults);

// 
// int WASP_SetLEDScript(LedID Id,LedState *pLedStates);
// 
// Id: The ID of the LED to be controlled.
// 
// pLedStates: pointer to an array of LED states defining the desired
// LED behavour.
// 
// The WASP daemon drives the devices LEDs by following the list
// of LED states with the specified durations until a state with 
// a duration of zero or LED_DURATION_INFINITE is found.  If the
// last state has a duration zero then the sequence starts over a
// the first state.  This allows complex blink pattern to be displayed
// easily.
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_SetLEDScript(LedID Id,LedState *pLedStates);

// int WASP_GetVarLen(WaspVariable *pVar)
// 
// Returns:
//      Size of variable in bytes
//    < 0 - Error
int WASP_GetVarLen(WaspVariable *pVar);


// Shutdown the WASP daemon. 
// int WASP_Shutdown();
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_Shutdown();


// Free and release resources used by WASP
// int WASP_Cleanup();
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_Cleanup();

// Set the maximum number asynchronous data messages to queue in WASP Daemon.  
// Asynchronous data reporting is enabled when QueueLen > 0.  Asynchronous 
// data reporting is disabled by default.
// 
// int WASP_SetAsyncDataQueueDepth(int QueueLen);
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_SetAsyncDataQueueDepth(int QueueLen);

// Free any malloc'ed storaged used by WaspVariable
// 
// int WASP_FreeValue(WaspVariable *pVar);
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_FreeValue(WaspVariable *pVar);

// Compare two WASP variables
// 
// int WASP_Compare(WaspVariable *pVar1,WaspVariable *pVar2)
// 
// Returns:
//      0 - pVar1 == pVar2
//      1 - pVar1 > pVar2
//      2 - pVar1 < pVar2
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_Compare(WaspVariable *pVar1,WaspVariable *pVar2);

// Return variable that has changed since the time it was read.
//
// int WASP_GetChangedVar(WaspVariable *pVar);
// 
// pVar: pointer to Variable to receive the value.
// 
// Filled by caller:
// 
// pVar->State:
//     VAR_VALUE_LIVE - get value from interface board
//     VAR_VALUE_CACHED - returned cached value when possible
// 
// Returned:
// pVar->ID Variable's ID.
//
// pVar->State:
//     VAR_VALUE_LIVE   - value from interface board
// 
// If the variable value is returned via a pointer in pVar->Val
// (string, blob) then the pointer is to dynamically allocated 
// memory that the caller is responsible for freeing.
// 
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_GetChangedVar(WaspVariable *pVar);

// Make a copy of a wasp variable.
//
// int WASP_CopyVar(WaspVariable *pTo,WaspVariable *pFrom);
// 
// pTo: pointer to variable to copy to 
// pFrom: pointer to variable to copy from
// 
// If the variable value is returned via a pointer in pVar->Val
// (string, blob) then the pointer is to dynamically allocated 
// memory that the caller is responsible for freeing.
// 
// 
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - WASP error code (see WASP_ERR_*)
int WASP_CopyVar(WaspVariable *pTo,WaspVariable *pFrom);

#ifdef OLD_API
// Set number of minutes to cook time at currently selected temperature.
// The current mode must be MODE_LOW or MODE_HIGH to set a cook time. 
//  
// Note: requested value will be rounded up to nearest 30 minute increment
// (CrockPot hardware limitations) 
//    < 0 - HAL error code (see WASP_ERR_*)
int WASP_SetCookTime(int Minutes);

// Return number of minutes of cook time remaining from product LED display. 
// A value of 0 indicates that the cockpot is not in a cooking mode
//    < 0 - WASP error code (see ERR_WASP_*)
int WASP_GetCookTime(void);
#endif

// Public data


// Lookup table by Wasp variable type, returns
// the length of the data portion in bytes
extern int WASP_VarType2Len[];

#endif  // _WASP_H_
