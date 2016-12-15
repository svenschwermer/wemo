/***************************************************************************
* Created by Belkin International, Software Engineering on XX/XX/XX.
* Copyright (c) 2012-2013 Belkin International, Inc. and/or its affiliates.
* All rights reserved.
*
* Belkin International, Inc. retains all right, title and interest (including
* all intellectual property rights) in and to this computer program, which is
* protected by applicable intellectual property laws.  Unless you have obtained
* a separate written license from Belkin International, Inc., you are not
* authorized to utilize all or a part of this computer program for any purpose
* (including reproduction, distribution, modification, and compilation into
* object code) and you must immediately destroy or return to Belkin
* International, Inc all copies of this computer program.  If you are
* licensed by Belkin International, Inc., your rights to utilize this
* computer program are limited by the terms of that license.
*
* To obtain a license, please contact Belkin International, Inc.
*
* This computer program contains trade secrets owned by Belkin International,
* Inc. and, unless unauthorized by Belkin International, Inc. in writing,
* you agree to maintain the confidentiality of this computer program and
* related information and to not disclose this computer program and related
* information to any other person or entity.
*
* THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND
* BELKIN INTERNATIONAL, INC. EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR
* IMPLIED, INCLUDING THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A
* PARTICULAR PURPOSE, TITLE, AND NON-INFRINGEMENT.
***************************************************************************/

#ifndef _WASP_H_
#define _WASP_H_

#define WASP_DEFAULT_BAUDRATE 9600

// Maximum length of a WASP message not include transport overhead
#define WASP_MAX_MSG_LEN      255

// Maximum length of data field in a WASP message 
// (WASP_MAX_MSG_LEN - header size - padding for future header expansion)
#define WASP_MAX_DATA_LEN     240

#define WASP_OK               0  
#define WASP_ERR_CMD          1  
#define WASP_ERR_ARG          2  
#define WASP_ERR_PERM         3  
#define WASP_ERR_COMM         4  
#define WASP_ERR_MORE         5  
#define WASP_ERR_OVFL         6  
#define WASP_ERR_INTERNAL     7  
#define WASP_ERR_DATA_NOTRDY  8

// command/response bit, 0x00 - command, 0x80 - response
#define WASP_CMD_RESP         0x80  

// Commands
#define WASP_CMD_NOP                0
#define WASP_CMD_GET_VALUE          1
#define WASP_CMD_GET_VALUE_CHUNK    2
#define WASP_CMD_GET_VALUES         3
#define WASP_CMD_SET_VALUE          4
#define WASP_CMD_SET_VALUE_CHUNK    5
#define WASP_CMD_SET_VALUES         6
#define WASP_CMD_SET_LED            7
#define WASP_CMD_GET_BUTTONS        8
#define WASP_CMD_GET_PRESSES        9
#define WASP_CMD_SELFTEST_STATUS    0x0a
#define WASP_CMD_GET_VAR_ATTRIBS    0x0b
#define WASP_CMD_GET_ENUM_ATTRIBS   0x0c
#define WASP_CMD_GET_BAUDRATES      0x0d
#define WASP_CMD_SET_BAUDRATE       0x0e
#define WASP_CMD_ASYNC_ENABLE       0x0f
#define WASP_CMD_ASYNC_DATA         0x10


// Standard Variables

#define WASP_VAR_DEVID        1
#define WASP_VAR_FW_VER       2
#define WASP_VAR_DEV_DESC     3
#define WASP_VAR_WEMO_STATUS  4
#define WASP_VAR_WEMO_STATE   5
#define WASP_VAR_WASP_VER     6
#define WASP_VAR_CLOCK_TIME   7

#define NUM_STANDARD_VARS     7

#define WASP_VAR_DEVICE       0x80     // first device specific variable


// WASP_VAR_WEMO_STATE values:
#define WASP_STATE_OFF        1
#define WASP_STATE_STANDBY    2
#define WASP_STATE_ON         3

// WASP_CMD_SELFTEST_STATUS results
#define WASP_SELF_TEST_PASSED          0
#define WASP_TEST_TEST_MINOR_ISSUE     1
#define WASP_TEST_TEST_FAILED          2

#define MAX_VAR_NAME_LEN               32

#define WASP_BUTTON_PRESSED            0x80

// Pseudo buttons
#define WASP_BUTTON_FACTORY_RESET      0x70
#define WASP_BUTTON_SETTINGS_CHANGED   0x71

#ifndef GCC_PACKED
#if defined(__GNUC__)
#define GCC_PACKED __attribute__ ((packed))
#else
#define GCC_PACKED
#endif
#endif

typedef struct {
   unsigned char Min;
   unsigned char Max;
   char Name[MAX_VAR_NAME_LEN];
} GCC_PACKED MinMaxU8;

typedef struct {
   signed char Min;
   signed char Max;
   char Name[MAX_VAR_NAME_LEN];
} GCC_PACKED MinMaxI8;

typedef struct {
   unsigned short Min;
   unsigned short Max;
   char Name[MAX_VAR_NAME_LEN];
} GCC_PACKED MinMaxU16;

typedef struct {
   short Min;
   short Max;
   char Name[MAX_VAR_NAME_LEN];
} GCC_PACKED MinMaxI16;

typedef struct {
   unsigned int Min;
   unsigned int Max;
   char Name[MAX_VAR_NAME_LEN];
} GCC_PACKED MinMaxU32;

typedef struct {
   int Min;
   int Max;
   char Name[MAX_VAR_NAME_LEN];
} GCC_PACKED MinMaxInt;

typedef struct {
   unsigned char Min[3];
   unsigned char Max[3];
   char Name[MAX_VAR_NAME_LEN];
} GCC_PACKED MinMaxBCD3;

typedef struct {
   unsigned char Min[4];
   unsigned char Max[4];
   char Name[MAX_VAR_NAME_LEN];
} GCC_PACKED MinMaxBCD4;

typedef struct {
   unsigned char Min[7];
   unsigned char Max[7];
   char Name[MAX_VAR_NAME_LEN];
} GCC_PACKED MinMaxBCD7;

typedef union {
   unsigned short Pollrate;
   struct {
      unsigned char TrackedVarId;
      unsigned char TrackingFlags;
         #define TRACK_FLAG_PUSH    0x80
         #define TRACK_FLAG_SAVE    0x40
   } p;
} PollPseudoUnion;

typedef struct {
   unsigned char Type;
   unsigned char Usage;
   PollPseudoUnion u;
   #define PollRate  u.Pollrate  // for backwards compability
   union {
      MinMaxU8 U8;
      MinMaxI8 I8;
      MinMaxU16 U16;
      MinMaxI16 I16;
      MinMaxU32 U32;
      MinMaxInt Int;
      MinMaxBCD3 BCD3;
      MinMaxBCD4 BCD4;
      MinMaxBCD7 BCD7;
   } MinMax;
} GCC_PACKED VariableAttribute;

#define WASP_VARTYPE_ENUM        1
#define WASP_VARTYPE_PERCENT     2
#define WASP_VARTYPE_TEMP        3
#define WASP_VARTYPE_TIME32      4
#define WASP_VARTYPE_TIME16      5
#define WASP_VARTYPE_TIMEBCD     6
#define WASP_VARTYPE_BOOL        7
#define WASP_VARTYPE_BCD_DATE    8
#define WASP_VARTYPE_DATETIME    9
#define WASP_VARTYPE_STRING      0x0a
#define WASP_VARTYPE_BLOB        0x0b
#define WASP_VARTYPE_UINT8       0x0c
#define WASP_VARTYPE_INT8        0x0d
#define WASP_VARTYPE_UINT16      0x0e
#define WASP_VARTYPE_INT16       0x0f
#define WASP_VARTYPE_UINT32      0x10
#define WASP_VARTYPE_INT32       0x11
#define WASP_VARTYPE_TIME_M16    0x12

#define NUM_VAR_TYPES            WASP_VARTYPE_TIME_M16


// Variable usage types
#define WASP_USAGE_FIXED         1
#define WASP_USAGE_MONITORED     2
#define WASP_USAGE_DESIRED       3
#define WASP_USAGE_CONTROLLED    4
// Pseudo variables
#define WASP_USAGE_PSEUDO        0x80
#define WASP_USAGE_MINIMUM       0x80
#define WASP_USAGE_MAXIMUM       0x81
#define WASP_USAGE_RANGE_ENTER   0x82
#define WASP_USAGE_RANGE_EXIT    0x83

// Special values for PollRate
#define WASP_POLL_NEVER          0
#define WASP_ASYNC_UPDATE        0xfffe
#define WASP_POLL_SETTINGS       0xffff


#endif  // _WASP_H_
