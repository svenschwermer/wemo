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
#ifndef _LIBNVRAM_INTERNAL_H_
#define _LIBNVRAM_INTERNAL_H_

#ifdef __MIPSEL__
#define SYSLOG(level,format,...) syslog(level,"nvram/%d/%s: " format, (int) syscall(SYS_gettid), __FUNCTION__, ## __VA_ARGS__)
#else
static FILE *gLogfp = NULL;
void logger(const char *fmt,...);
#define SYSLOG(level,format,...) logger("%s: " format "\n", __FUNCTION__, ## __VA_ARGS__)
#endif

// Set one of the following to select the search algorithm
// #define MRU_SEARCH   1
#define BTREE_SEARCH 1

#define SHARED_MEM_NAME    "nvram"
#define GEMTEK_NVRAM_SIG   "NVRM"

// Variables names used internally by libbelkin_nvram
#define VAR_RESET_TIME     "NvramResetTime"
#define VAR_RESET_TYPE     "NvramResetType"
#define VAR_COMMIT_COUNT   "NvramCommits"
#define VAR_LOG_MASK       "NvramLogMask"

#define SIGNATURE_VALUE    449220

typedef struct {
   int   Mask;
   const char *Desc;
} MaskName;

extern MaskName NvramDebugMasks[];

#define NVRAM_NAME_LEN  16

typedef struct {
   int NvramLen;
   char MtdName[NVRAM_NAME_LEN];
   uint32_t EraseSize;
} NvramInfo;
#define MAX_TBL_ENTRIES  1024

typedef struct {
   int Signature;
   sem_t Semaphore;
   sem_t RecoverySemaphore;
   pid_t SemOwnerTID;
   int SemCount;

   int Flags;
      #define FLAG_GEMTEK           1
      #define FLAG_DIRTY            2  // commit pending
      #define FLAG_DAEMON_STARTING  4

   int Verbose;

   int64_t CommitTimer;   // Set only when not running

// NVRAM parameters
#ifdef __MIPSEL__
   char MtdName[NVRAM_NAME_LEN];    // probably /dev/mtd6
   uint32_t EraseSize;
#endif

#ifdef MRU_SEARCH
   struct {
   	int Next;	   // index of next entry in MruArray, 0 = last
   	int Offset;    // offset into Nvram[] data for variable name
   } MruArray[MAX_TBL_ENTRIES];

   int MruArrayHead;
#endif

#ifdef BTREE_SEARCH
   int Index[MAX_TBL_ENTRIES];   // offset into Nvram[] for variable name
#endif

   int NumEntries;

   int DataLen;      // Number of bytes of Nvram[] available for variables
   int CrcOffset;    // Offset to CRC in Nvram[]
   int DataOffset;   // Offset to first variable in Nvram[]
   int EndOfData;    // Offset to first free byte in Nvram[]
   int NvramLen;     // Actual size of Nvram[]
// Gemtek format
   int Offset2EOD;   // Offset to Gemtek eod variable in Nvram[]
   int CountOffset;  // Offset to Gemtek count variable  in Nvram[]

// Statistics
   int Sets;         // Successful NvramSets
   int IgnoredSets;  // NvramSets were value wasn't changed
   int UnSets;       // Successful NvramUnSets
   int Gets;         // Successful NvramGets
   int NotFound;     // Unsuccessful NvramGets
   int Commits;      // Successful NvramCommits since boot
   int TotalCommits; // Successful NvramCommits since last NVRAM erase
   int Errors;       // Physical NVRAM access errors
   int InternalErrs; // Bugs
   int DaemonStarts; // Number of times daemon was started
   int DaemonExits;  // Number of times daemon exited gracefully

   int ResetTime;    // Timestamp of last time NVRAM was reset
   int ResetType;    // Type of last NVRAM reset

   pid_t DaemonPID;
   bool bRunDaemon;
   char Nvram[1];    // actually NvramLen bytes (size of NVRAM partition)
} SharedMem;


/* Reset / Restore NVRAM
   RESTORE_TYPE_ERASE   - Erase all NVRAM contents.

   RESTORE_TYPE_RESTORE - Erase all NVRAM contents except certain values listed 
   in the internal defaults table, preserve those if possible.

   RESTORE_TYPE_CORRUPTED - Corrupted NVRAM detected on initialization, 
   erase all NVRAM contents.
*/
#define RESTORE_TYPE_ERASE       1
#define RESTORE_TYPE_RESTORE     2
#define RESTORE_TYPE_CORRUPTED   3
int NvramRestore(int Type);

int NvramStartDaemon(int bSendSignal);
int NvramDaemon(bool bForground);
int NvramShow(int Next,const char **Variable);
int NvramStatus(void);
int NvramShutdown(void);


// Write contents of NVRAM cached to flash NOW.  Note: this function
// is not for general use, the daemon should normally call NvramCommit 
// when scheduled.
int NvramCommit(void);

// Write contents of Path to flash
int NvramWriteFile(char *Path);

#endif   // _LIBNVRAM_INTERNAL_H_
