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
*
***************************************************************************/
#ifndef _LIBNVRAM_H_
#define _LIBNVRAM_H_

#ifndef _BELKIN_API_H_
// Legacy APIs
int nvramget(int argc, char *argv[], char *buffer);
int nvramset(int argc, char *argv[]);
int libNvramInit(void);
#endif


// Set <variable> to string value <value>
// bCommit: 0 - Don't schedule a commit
// bCommit: 1 - Schedule a commit
int NvramSet(char *Variable,char *Value,int bCommit);

// Set <variable> to integer value <value>
// bCommit: 0 - Don't schedule a commit
// bCommit: 1 - Schedule a commit
// 
// Note: values are always stored in NVRAM as srings, this is simply
// a convenience function to performs the conversion for the caller.
int NvramSetInt(char *Variable,int Value,int bCommit);

// Get pointer to value of <Variable> in NVRAM.  If <Variable> doesn't
// exist a pointer to an empty string is returned.
// NB: Variable
char *NvramGet(char *Variable);

// Get integer value from NVRAM, store it at pInt
// Return 0 value found and converted successfully
int NvramGetInt(char *Variable,int *pInt);

// Remove <Variable> value from NVRAM
// bCommit: 0 - Don't schedule a commit
// bCommit: 1 - Schedule a commit
int NvramUnset(char *Variable,int bCommit);

// Initialize lib_nvram_bd
int NvramInit(int bIgnoreCRC,const char *SyslogName);

// defines for bit NVRAM logging mask
#define LOG_V              0x1
#define LOG_READ           0x2
#define LOG_WRITE          0x4
#define LOG_API_CALLS      0x8
#define LOG_NVRAM_DAEMON   0x10
#define LOG_SEMAPHORE      0x20
#define LOG_FIND           0x40

// Set NVRAM subsystem logging level.  
//    bPersist: 0 - Don't save level in NVRAM
//    bPersist: 1 - Save level in NVRAM variable 'NvramLogMask'
// The NVRAM subsystem logging level is restored from 'NvramLogMask' if it
// exists on library initialization.
int NvramSetLogMask(int Mask,int bPersist);

// Return integer number of seconds since Unix epoc, or 0 if the
// system time hasn't bee set yet
int NvramGetTimeStamp(void);

#ifdef __MIPSEL__
void restoreFactory(void);
#endif

#endif   // _LIBNVRAM_H_
