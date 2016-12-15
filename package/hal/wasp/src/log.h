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
#ifndef _LOGGER_H_
#define _LOGGER_H_

#define LOG_FLAG_DATE  1
#define LOG_FLAG_TIME  2
#define LOG_FLAG_MSEC  4

/*
   Initialize logging routines.
   
   LogPath        = Full path to a log file.
   MaxLinesPerSec = Maximum number of lines/sec to log.  A value of zero
                    means no limit (use with caution)
   MaxLogFileSize = Maximum log file size in bytes.  A value of zero means
                    no limit (use with caution).
   bDaemon        = Pointer to an integer that indiates if the application
                    running is running as daemon or not.  If the application
                    is not deamonized lines logged will also be printf'ed to
                    stdout.  Provide a pointer to a integer zero if this
                    is not desired.  Can be a NULL in which case logger assumes
                    printf outputs are desired.
                                 
   return:  NULL on error or a pointer to a handle which is used as an 
            argument with all other logging functions.
*/
void *LogInit(char *LogPath,int MaxLinesPerSec,unsigned int MaxLogFileSize, int *bDaemon);

/* 
   Close down logging routines, free allocated memory, close log files.
   
   Do not use LogHandle any way after this call.
*/
int LogShutdown(void *LogHandle);

/* 
   Log a message
*/
void LogIt(void *LogHandle,char *fmt,...) __attribute__ ((format (printf,2,3)));

/*
   Log a hunk of memory in hex
*/
void LogHex(void *LogHandle,void *AdrIn,int Len);

/*
   Reset the log's lines logged in last second count and re-enable logging
   if disabled due to excessive logging.  Use with caution !
*/
void LogReset(void *LogHandle);

/*
   Enable/disable logging without closing file.  

   Note: Logging automatically disabled by tripping the MaxLinesPerSec limit
   is not effected by this call.  Use LogReset to re-enable logging in such
   cases.
*/
void LogEnable(void *LogHandle,int bEnable);

#endif   /* _LOGGER_H_ */

