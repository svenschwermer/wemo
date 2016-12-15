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
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <zlib.h>
#include <time.h>
#include <sys/resource.h>
#include <semaphore.h>
#include <libgen.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <mtd/mtd-user.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <inttypes.h>
#include <sys/syscall.h>

#include "libnvram_internal.h"
#include "libnvram.h"

#define CONFIG_FILE     "/etc/nvram.config"


#define LOG(format,...) SYSLOG(LOG_DEBUG,format, ## __VA_ARGS__)
#define ELOG(format,...) SYSLOG(LOG_ERR,format, ## __VA_ARGS__)

#define ALOG(format,...) \
   if(g_pShared != NULL && (g_pShared->Verbose & LOG_API_CALLS) != 0) \
      LOG(format, ## __VA_ARGS__)

#define DLOG(format,...) \
   if(g_pShared != NULL && (g_pShared->Verbose & LOG_NVRAM_DAEMON) != 0) \
      LOG(format, ## __VA_ARGS__)

#define WLOG(format,...) \
   if(g_pShared != NULL && (g_pShared->Verbose & LOG_WRITE) != 0) \
      LOG(format, ## __VA_ARGS__)

#define SLOG(format,...) \
   if(g_pShared != NULL && (g_pShared->Verbose & LOG_SEMAPHORE) != 0) \
      LOG(format, ## __VA_ARGS__)

#define FLOG(format,...) \
   if(g_pShared != NULL && (g_pShared->Verbose & LOG_FIND) != 0) \
      LOG(format, ## __VA_ARGS__)

#define VLOG(format,...) \
   if(g_pShared != NULL && (g_pShared->Verbose & LOG_V) != 0) \
      LOG(format, ## __VA_ARGS__)


// Time to wait to commit after first change in millseconds
#define COMMIT_TO  3000

#ifdef __MIPSEL__
#define START_DAEMON_CMD   "nvramd"
#else
#define START_DAEMON_CMD   "./nvramd"
#define NVRAM_FILENAME     "nvram.bin"
#endif

typedef struct env_image_single {
   uint32_t crc;  /* CRC32 over data bytes    */
   uint8_t     data[];
} env_image_single;

typedef struct {
   char     signature[4];
   uint32_t crc;  /* CRC32 over data bytes    */
   uint32_t count;
   uint32_t eod;
   uint8_t     data[];
} env_image_gemtek;

static SharedMem *g_pShared = NULL;
static int64_t gUpTime;  // in milliseconds
static int gSharedMemLen = 0;

static bool g_bIgnoreCRC = false;

// Legacy variables
char sem_fn[] = "/nvram"; /* ref: man 7 sem_overview */
sem_t *semdes;

typedef struct 
{
	const char *Name;
	const char *Value;
} NameValuePair;

static NameValuePair Defaults[] = {
/* These NVRAM variable are set by the Gemtek "nvram reset"
 * command.  They are referenced somewhere in existing code.
 * But their usefulness is in doubt.  In particular it seems to
 * be the wrong place to define a firmware version.
 */
	{ "bootstate", "0" },
	{ "lan_ipaddr", "10.22.22.1" },
	{ "model_name", "basic" },
	{ "my_fw_version", "0.00.10 20111115" },
	{ "ntp_server1", "192.43.244.18" },
	{ "timezone_index", "005" },
	{ "wan_ipaddr", "0.0.0.0" },
	{ "wan_netmask", "0.0.0.0" },
/* These NVRAM variables were preserved by the original Belkin
 * NVRAM reset code.  They didn't seem to be part of the Gemtek
 * implementation.
 */
	{ "home_id", NULL },
	{ "SmartDeviceId", NULL },
	{ "SmartPrivatekey", NULL },
	{ "plugin_key", NULL },
	{ "PluginCloudId", NULL },
	{ "SerialNumber", NULL },
	{ "wl0_currentChannel", NULL },
   { VAR_COMMIT_COUNT, NULL},
   { VAR_RESET_TIME, NULL},
   { VAR_RESET_TYPE, NULL},
   { VAR_LOG_MASK, NULL},
   {NULL}   // end of table
};

MaskName FlagDescs[] = {
   {FLAG_GEMTEK,"Gemtek"},
   {FLAG_DIRTY,"Dirty"},
   {FLAG_DAEMON_STARTING,"Daemon_Starting"},
   {0}   // End of table
};

MaskName NvramDebugMasks[] = {
   {LOG_V,"Verbose messages"},
   {LOG_READ,"Reads"},
   {LOG_WRITE,"Writes"},
   {LOG_API_CALLS,"Api calls"},
   {LOG_NVRAM_DAEMON,"Nvram Daemon related"},
   {LOG_SEMAPHORE,"Semaphore accesses"},
   {LOG_FIND,"Variable lookup related"},
   {0},  // end of table
};


static int TriggerCommit(SharedMem *p,bool bNow);
static int InitSharedMem(NvramInfo *pNvram);
static void GetSemaphore(SharedMem *p);
static void ReleaseSemaphore(SharedMem *p);
static void BuildIndex(SharedMem *p);
static void GetUpTime(void);
static int GetNvramInfo(NvramInfo *p);
static int NvramCommitInternal(void);

#ifdef __MIPSEL__
#ifdef CONFIG_FILE
static int get_config(NvramInfo *p);
#endif
static int ParseProc(NvramInfo *p);
static int FlashWrite(SharedMem *p,char *Buffer);
static int FlashRead(SharedMem *p);
#endif

static void GetUpTime()
{
   struct timespec TimeSpec;
   if(clock_gettime(CLOCK_MONOTONIC,&TimeSpec) != 0) {
      ELOG("clock_gettime failed - %s",strerror(errno));
   }

   gUpTime = (TimeSpec.tv_nsec/1000000LL) + (TimeSpec.tv_sec * 1000LL);
}

// NB: Caller must own the shared memory semaphore when calling this function
static int TriggerCommit(SharedMem *p,bool bNow)
{
   bool bSendSignal = false;
   int Ret;

   DLOG("Called");
   if(bNow) {
      GetUpTime();
      p->CommitTimer = gUpTime;
      bSendSignal = true;
   }
   else {
   // Start the CommitTimer if it's not running already
      if(p->CommitTimer == 0) {
         GetUpTime();
         p->CommitTimer = gUpTime + COMMIT_TO;
         bSendSignal = true;
         DLOG("Set CommitTimer to %" PRId64 "",p->CommitTimer);
      }
   }
   DLOG("Calling NvramStartDaemon(%d)",bSendSignal);
   Ret = NvramStartDaemon(bSendSignal);
   DLOG("Returning %d",Ret);

   return Ret;
}

// Return:
// <0 - Internal error
//  0 - successfully initialization of shared memory library
// >0 - Standard Linux errno
int libNvramInit()
{
   int OFlags;
   int Fd = -1;
   bool bInitialize = false;
   struct stat Stat;
   int i;
   int Ret = -2;     // assume the worse
   NvramInfo Nvram = {0};

   do {
      ALOG("Called");
      if(g_pShared != NULL) {
      // Nothing to do, libNvramInit has been called before
         Ret = 0;
         break;
      }
   // Try to open an existing shared memory
      OFlags = O_RDWR;
      if((Fd = shm_open(SHARED_MEM_NAME,OFlags,S_IRWXU)) == -1) {
         if(errno != ENOENT) {
            LOG("shm_open failed - %s",strerror(errno));
            break;
         }
         LOG("libbelkin_nvram compiled "__DATE__ " "__TIME__"");
         LOG("shared memory doesn't exist, creating it.");

      // NVRAM shared memory hasn't be created yet, try to create it
         OFlags |= O_CREAT | O_EXCL;
         if((Fd = shm_open("nvram",OFlags,S_IRWXU)) == -1) {
            if(errno != EEXIST) {
               LOG("shm_open failed - %s",strerror(errno));
               break;
            }
            else {
            // Couldn't create it because someone beat use to it, try one 
            // final time to open existing shared memory
               OFlags = O_RDWR;
               if((Fd = shm_open("nvram",OFlags,S_IRWXU)) == -1) {
                  LOG("shm_open failed - %s",strerror(errno));
                  break;
               }
            }
         }
         else {
         // We've create a new shared memory, initialize it
            bInitialize = true;
            LOG("initializing shared memory");
            if((Ret = GetNvramInfo(&Nvram)) != 0) {
               break;
            }
            gSharedMemLen = sizeof(SharedMem) - 1 + Nvram.NvramLen;

            if(ftruncate(Fd,gSharedMemLen) == -1) {
               LOG("ftruncate failed - %s",strerror(errno));
            // This isn't good, delete the shared memory region
               close(Fd);
               Fd = -1;
               if(shm_unlink(SHARED_MEM_NAME) != 0) {
                  ELOG("shm_unlink failed - %s",strerror(errno));
                  Ret = errno;
               }
               break;
            }
         }
      }

      for(i = 0; i < 2; i++) {
   // Find size of shared memory
         if(fstat(Fd,&Stat) != 0) {
            LOG("stat failed - %s",strerror(errno));
            break;
         }
         if(Stat.st_size > 0) {
            gSharedMemLen = (int) Stat.st_size;
            break;
         }
      // possible collision on initial shared memory creation, sleep once
      // and try again
         LOG("shared memory size is zero, sleeping for retry.");
         sleep(1);
      }

      if(Stat.st_size == 0) {
         ELOG("Error - shared memory size still zero after retry.");
         Ret = -1;
         break;
      }

      g_pShared = (SharedMem *) mmap(NULL,gSharedMemLen,
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED,Fd,0);
      if(g_pShared == MAP_FAILED) {
         LOG("mmap failed - %s",strerror(errno));
         break;
      }

      if(bInitialize) {
         Ret = InitSharedMem(&Nvram);
      }
      else {
         for(i = 0; i < 100; i++) {
            if(g_pShared->Signature == SIGNATURE_VALUE) {
               break;
            }
            usleep(100000);   // .1 second
         }
         if(g_pShared->Signature != SIGNATURE_VALUE) {
            ELOG("Error - Incorrect signature (%d)",g_pShared->Signature);
            Ret = -1;
            break;
         }
         Ret = 0;
      }
   } while(false);

   if(Fd != -1) {
      close(Fd);
   }

   if(Ret == -2 && errno != 0) {
      Ret = errno;
   }

   ALOG("Returning %d",Ret);
   return Ret;
}

int NvramInit(int bIgnoreCRC,const char *SyslogName)
{
   g_bIgnoreCRC = bIgnoreCRC;
   if(SyslogName != NULL) {
      openlog(SyslogName,LOG_NDELAY,LOG_DAEMON);
   }
   return libNvramInit();
}

static int GetNvramInfo(NvramInfo *p)
{
   int Ret = 0;   // Assume the best

#ifdef __MIPSEL__
   do {
#if defined(CONFIG_FILE)
      if((Ret = get_config(p)) != 0) {
         ELOG("get_config failed, trying /proc");
      }
      else {
         break;
      }
#endif
      Ret = ParseProc(p);
   } while(false);

#else
   struct stat Stat;

   if(stat(NVRAM_FILENAME,&Stat) != 0) {
      LOG("stat failed - %s",strerror(errno));
      Ret = errno;
   }
   else {
      p->NvramLen = (int) Stat.st_size;
   }
#endif

   return Ret;
}

#ifdef BTREE_SEARCH
int NameCompare(const void *p1,const void *p2);

int NameCompare(const void *p1,const void *p2)
{
   SharedMem *p = g_pShared;
   const char *Name1 = &p->Nvram[*((int *) p1)];
   const char *Name2 = &p->Nvram[*((int *) p2)];

   return strcmp(Name1,Name2);
}

static void BuildIndex(SharedMem *p)
{
// Initialize index
   int i;
   char *cp = &p->Nvram[p->DataOffset];
   int Offset = p->DataOffset;
   int StrLen;

   for(i = 0; i < MAX_TBL_ENTRIES; i++) {
      if((StrLen = strlen(cp)) == 0) {
         break;
      }
      p->Index[i] = Offset;
      Offset += StrLen + 1;
      cp += StrLen + 1;
   }

   if(i >= MAX_TBL_ENTRIES) {
      LOG("Error - Index overflow");
   }
   else {
      p->EndOfData = Offset;
      p->NumEntries = i;
      LOG("%d variables defined in NVRAM",i);
      if(p->Flags & FLAG_GEMTEK) {
         int *pCount = (int *) &p->Nvram[p->CountOffset];
         if(i != *pCount) {
            LOG("count error, calculated %d, stored %d",i,*pCount);
         }
      }
   // Sort the index table
      qsort(p->Index,p->NumEntries,sizeof(int),NameCompare);
   }
}
// Find variable <Variable> in nvram array, save pointer to value
// string in Value.
// 
// Returns variable index in Index or -1 if not found,
int NvramFind(const char *Variable,char **Value)
{
   SharedMem *p = g_pShared;
   int Len = strlen(Variable);
   int Ret = -1;  // Assume the worse
   int Bottom = 0;
   int Top = p->NumEntries - 1;
   int Result;
   const char *cp;

   FLOG("looking for '%s'",Variable);
   if(Top >= 0) {
      for( ; ; ) {
      // Test middle of the possible range

         Ret = (Top + Bottom) / 2;
         FLOG("top: %d, bottom: %d, Ret: %d",Top,Bottom,Ret);
         cp = &p->Nvram[p->Index[Ret]];
         if((Result = strncmp(Variable,cp,Len)) == 0 && cp[Len] == '=') {
         // Found it
            *Value = &p->Nvram[p->Index[Ret] + Len + 1];
            FLOG("found '%s' @ offset %d, index %d",
                 Variable,p->Index[Ret],Ret);
            break;
         }

         FLOG("Result: %d",Result);
         if(Top == Bottom) {
         // Variable not found
            Ret = -1;
            break;
         }

         if(Result > 0) {
         // Variable is above the tested entry

            Bottom = Ret + 1;
         }
         else {
         // Variable is below the tested entry
            Top = Ret - 1;
         }
         if(Top < Bottom) {
         // Variable not found
            Ret = -1;
            break;
         }
      }
   }

   FLOG("Returning %d",Ret);
   return Ret;
}

int NvramShow(int Next,const char **Variable)
{
   SharedMem *p = g_pShared;
   int Ret = Next;

   if(Ret == -1) {
      Ret = 0;
   }
   GetSemaphore(p);
   if(Next < p->NumEntries) {
      *Variable = &p->Nvram[p->Index[Ret++]];
   }
   else {
      *Variable = NULL;
      Ret = -1;
   }
   ReleaseSemaphore(p);

   ALOG("Returning %d",Ret);
   return Ret;
}

// Remove <Variable> value from NVRAM
// bCommit: 0 - Don't schedule a commit
// bCommit: 1 - Schedule a commit
int NvramUnset(char *Variable,int bCommit)
{
   SharedMem *p = g_pShared;
   char *cp;
   int Entries2Copy;
   int Ret = 0;   // assume the best

   GetSemaphore(p);

   if((Ret = NvramFind(Variable,&cp)) != -1) {
   // Remove entry from index
      cp = (char *) &p->Index[Ret];
      p->NumEntries--;
      Entries2Copy = p->NumEntries-Ret;
      if(Entries2Copy > 0) {
         memmove(cp,cp+sizeof(int),Entries2Copy * sizeof(int));
      }
      Ret = 0;
      p->UnSets++;
      p->Flags |= FLAG_DIRTY;
      if(bCommit) {
         TriggerCommit(p,false);
      }
   }
   ReleaseSemaphore(p);

   ALOG("Returning %d",Ret);
   return Ret;
}


/* Reset / Restore NVRAM
   RESTORE_TYPE_ERASE   - Erase all NVRAM contents.

   RESTORE_TYPE_RESTORE - Erase all NVRAM contents except certain values listed 
   in the internal defaults table, preserve those if possible.

   RESTORE_TYPE_CORRUPTED - Corrupted NVRAM detected on initialization, 
   erase all NVRAM contents.
*/
int NvramRestore(int Type)
{
   int Ret = -2;   // Assume the worse
   SharedMem *p = g_pShared;
   int DataLen = p->NvramLen - offsetof(env_image_gemtek,data);
   char *pNew = (char *) calloc(p->DataLen,1);
   env_image_gemtek *pHdr = (env_image_gemtek *) pNew;
   char *cp = (char *) pHdr->data;
   char *cp1;
   char *Eod = (char *) &pHdr->data[DataLen];
   int Len;
   int TimeStamp = NvramGetTimeStamp();

   do {
      GetSemaphore(p);
      if(pNew == NULL) {
         LOG("calloc failed");
         Ret = ENOMEM;
         break;
      }
   // Reset to Gemtek format
      memcpy(&pHdr->signature,GEMTEK_NVRAM_SIG,strlen(GEMTEK_NVRAM_SIG));
      pHdr->count = 0;

      if(Type == RESTORE_TYPE_RESTORE) {
         NameValuePair *pVar = Defaults;
         while(pVar->Name != NULL) {
            Len = strlen(pVar->Name);
            if(cp + Len + 3 >= Eod) {  
            // Internal error no room for <string>,'=',<NULL>,<NULL>
               break;
            }
            if(NvramFind(pVar->Name,&cp1) == -1) {
            // We don't have a value, use the default
               cp1 = (char *) pVar->Value;
            }

            if(cp1 != NULL) {
            // We have a value
               memcpy(cp,pVar->Name,Len);
               cp += Len;
               *cp++ = '=';
               Len = strlen(cp1);
               if(cp + Len + 2 > Eod) {
               // Internal error no room for <string>,<NULL>,<NULL>
                  break;
               }
               memcpy(cp,cp1,Len);
               cp += Len;
               *cp++ = 0;
               pHdr->count++;
            }
            pVar++;
         }
         *cp++ = 0;  // final NULL at end of all variables
      }
      pHdr->eod = cp - (char*) pHdr;
      memcpy(p->Nvram,pNew,p->NvramLen);

  // Reset to Gemtek format
      p->Flags |= (FLAG_GEMTEK | FLAG_DIRTY);
      p->DataLen = p->NvramLen - offsetof(env_image_gemtek,data);
      p->CrcOffset = offsetof(env_image_gemtek,crc);
      p->DataOffset = offsetof(env_image_gemtek,data);
      p->Offset2EOD = offsetof(env_image_gemtek,eod);
      p->CountOffset = offsetof(env_image_gemtek,count);
      p->ResetType = Type;
      p->ResetTime = TimeStamp;
      BuildIndex(p);
   // Save timestamp of when and why this occured
      NvramSetInt(VAR_RESET_TIME,TimeStamp,false);
      NvramSetInt(VAR_RESET_TYPE,Type,false);
      GetUpTime();
      p->CommitTimer = gUpTime;
      Ret = 0;
   } while(false);

   ReleaseSemaphore(p);

   LOG("Returning %d",Ret);

   if(Ret == 0) {
      NvramStartDaemon(true);
   }
   else if(Ret == -2 && errno != 0) {
      Ret = errno;
   }

   ALOG("Returning %d",Ret);
   return Ret;
}

#endif   // BTREE_SEARCH

#ifdef MRU_SEARCH
static void BuildIndex(SharedMem *p)
{
// Initialize MRU table
   int i;
   char *cp = &p->Nvram[p->DataOffset];
   int Offset = p->DataOffset;
   int StrLen;

   for(i = 0; i < MAX_TBL_ENTRIES; i++) {
      if((StrLen = strlen(cp)) == 0) {
         break;
      }
      p->MruArray[i].Offset = Offset;
      p->MruArray[i].Next = i + 1;
      Offset += StrLen + 1;
      cp += StrLen + 1;
   }

   if(i == MAX_TBL_ENTRIES) {
      LOG("Error - MruArray overflow");
   }
   else {
      p->EndOfData = Offset;
      p->NumEntries = i;
      p->MruArray[i-1].Next = -1;   // End of table
      LOG("%d variables defined in NVRAM",i);
      if(p->Flags & FLAG_GEMTEK) {
         int *pCount = (int *) &p->Nvram[p->CountOffset];
         if(i != *pCount) {
            LOG("count error, calculated %d, stored %d",i,*pCount);
         }
      }
   // Clear the rest of the table
      for( ; i < MAX_TBL_ENTRIES; i++) {
         p->MruArray[i].Next = -1;
         p->MruArray[i].Offset = 0;
      }
   }
}

// Find variable <Variable> in nvram array, save pointer to value
// string in Value.
// 
// Returns variable index in MruArray or -1 if not found,
int NvramFind(const char *Variable,char **Value)
{
   SharedMem *p = g_pShared;
   int NameLen = strlen(Variable);
   int Last = -1;
   char *cp;
   int Ret = p->MruArrayHead;

   FLOG("%s: looking for '%s'",Variable);
   for( ; ; ) {
      cp = &p->Nvram[p->MruArray[Ret].Offset];
      FLOG("testing %d: %s",Ret,cp);
      if(strncmp(cp,Variable,NameLen) == 0 && cp[NameLen] == '=') {
      // Update the MRU array
         if(Last != -1) {
            p->MruArray[Last].Next = p->MruArray[Ret].Next;
            p->MruArray[Ret].Next = p->MruArrayHead; 
            p->MruArrayHead = Ret;
         }
         *Value = cp + NameLen + 1;
         FLOG("found '%s' @ offset %d, index %d",Variable,
              p->MruArray[Ret].Offset,Ret);
         break;
      }
      Last = Ret;
      Ret = p->MruArray[Ret].Next;
      if(Ret < 0) {
         break;
      }
   }

   return Ret;
}

int NvramUnset(char *Variable,int bCommit)
{
   SharedMem *p = g_pShared;
   int NameLen = strlen(Variable);
   int Last = -1;
   char *cp;
   int i;
   int Ret = 0;   // assume the best

   GetSemaphore(p);

   i = p->MruArrayHead;
   for( ; ; ) {
      cp = &p->Nvram[p->MruArray[i].Offset];
      if(strncmp(cp,Variable,NameLen) == 0 && cp[NameLen] == '=') {
      // Remove from the MRU array
         if(Last == -1) {
            p->MruArrayHead = p->MruArray[i].Next;
         }
         else {
            p->MruArray[Last].Next = p->MruArray[i].Next;
         }
         p->UnSets++;
         p->Flags |= FLAG_DIRTY;
         if(bCommit) {
            TriggerCommit(p,false);
         }
         break;
      }

      if((i = p->MruArray[i].Next) < 0) {
      // Variable not found
         Ret = -1;
         break;
      }
   }
   ReleaseSemaphore(p);

   ALOG("Returning %d",Ret);
   return Ret;
}
#endif


// Return:
// <0 - Internal error
//  0 - successfully initialization of shared memory library
// >0 - Standard Linux errno
static int InitSharedMem(NvramInfo *pNvram)
{
   int Ret = -2;   // Assume the worse
   uLong CalclatedCRC;
   uint8_t *pData;
   SharedMem *p = g_pShared;
   uint32_t *pCrc = (uint32_t *) &p->Nvram[p->CrcOffset];

   do {
      LOG("Called");
   // Init Semaphore in "taken state" 
      if(sem_init(&p->Semaphore,1,0) == -1) {
         ELOG("sem_init failed - %s",strerror(errno));
         break;
      }
      p->SemOwnerTID = (pid_t) syscall(SYS_gettid);
      p->SemCount = 1;

   // Init RecoverySemaphore in available state
      if(sem_init(&p->RecoverySemaphore,1,1) == -1) {
         ELOG("sem_init of RecoverySemaphore failed - %s",strerror(errno));
         break;
      }

   // NB: The signature must be set after sem_init call above to avoid races
      p->Signature = SIGNATURE_VALUE;
      p->NvramLen = pNvram->NvramLen;
      p->DaemonPID = -1;

#ifdef __MIPSEL__
      strcpy(p->MtdName,pNvram->MtdName);
      p->EraseSize = pNvram->EraseSize;
      /* read environment from FLASH to local buffer */
      if((Ret = FlashRead(p)) != 0) {
         break;
      }
#else
// Desktop, read NVRAM data from file
      {
         int Read;
         FILE *fp;

         if((fp = fopen(NVRAM_FILENAME,"r")) == NULL) {
            LOG("fopen failed - %s",strerror(errno));
            break;
         }

         Read = fread(p->Nvram,pNvram->NvramLen,1,fp);
         fclose(fp);

         if(Read != 1) {
            LOG("fread failed - %s",strerror(errno));
            break;
         }
      }
#endif
      if(memcmp(p->Nvram,GEMTEK_NVRAM_SIG,strlen(GEMTEK_NVRAM_SIG)) == 0) {
      // NVRAM is in Gemtek format, set the flag
         LOG("NVRAM partition is in Gemtek format");
         p->Flags |= FLAG_GEMTEK;
         p->DataLen = p->NvramLen - offsetof(env_image_gemtek,data);
         p->CrcOffset = offsetof(env_image_gemtek,crc);
         p->DataOffset = offsetof(env_image_gemtek,data);
         p->Offset2EOD = offsetof(env_image_gemtek,eod);
         p->CountOffset = offsetof(env_image_gemtek,count);
      }
      else {
         p->DataLen = p->NvramLen - offsetof(env_image_single,data);
         p->CrcOffset = offsetof(env_image_single,crc);
         p->DataOffset = offsetof(env_image_single,data);
      }

      pData = (uint8_t *) &p->Nvram[p->DataOffset];

      CalclatedCRC = crc32(0,pData,p->DataLen);
      pCrc = (uint32_t *) &p->Nvram[p->CrcOffset];

      if(CalclatedCRC != *pCrc) {
         if(g_bIgnoreCRC) {
            LOG("Ignoring CRC error (calculated 0x%lx, stored 0x%x)",
                CalclatedCRC,*pCrc);
         }
         else {
            ELOG("CRC error, calculated 0x%lx, stored 0x%x",
                 CalclatedCRC,*pCrc);
            ELOG("Reinitializaing NVRAM");
            if((Ret = NvramRestore(RESTORE_TYPE_CORRUPTED)) != 0) {
               ELOG("NvramRestore failed");
               break;
            }
         }
      }
      BuildIndex(p);

      if(NvramGetInt(VAR_LOG_MASK,&p->Verbose) == 0) {
         ELOG("Nvram logging mask set to 0x%x",p->Verbose);
      }

      NvramGetInt(VAR_COMMIT_COUNT,&p->TotalCommits);
      NvramGetInt(VAR_RESET_TIME,&p->ResetTime);
      NvramGetInt(VAR_RESET_TYPE,&p->ResetType);
   // Remove above gets from statistics
      p->Gets = 0;
      p->NotFound = 0;

      ReleaseSemaphore(p);
      Ret = 0;
   } while(false);

   if(Ret == -2 && errno != 0) {
      Ret = errno;
   }

   LOG("Returning %d",Ret);
   return Ret;
}

char *NvramGet(char *Variable)
{
   char *Ret = "";
   SharedMem *p = g_pShared;

   if(p != NULL) do {
      if(Variable == NULL) {
         ELOG("Error - argument is NULL");
         break;
      }
      GetSemaphore(p);
      if(NvramFind(Variable,&Ret) == -1) {
         p->NotFound++;
      }
      else {
         p->Gets++;
      }
      ReleaseSemaphore(p);

      if((p->Verbose & (LOG_API_CALLS | LOG_READ)) != 0) {
         if(strlen(Ret) == 0) {
            LOG("'%s' not found",Variable);
         }
         else {
            LOG("Returning '%s'='%s'",Variable,Ret);
         }
      }
   } while(false);

   return Ret;
}

// Get integer value from NVRAM, store it at pInt
// Return 0 value found and converted successfully
int NvramGetInt(char *Variable,int *pInt)
{
   int Ret = -1;  // Assume the worse

   ALOG("Called");
   if(pInt == NULL) {
      ELOG("Error - 2'nd agument is NULL");
      Ret = EINVAL;
   }
   else {
      char *Value = NvramGet(Variable);
      if(sscanf(Value,"%d",pInt) == 1) {
         Ret = 0;
      }
   }

   ALOG("Returning %d",Ret);
   return Ret;
}

int NvramSet(char *Variable,char *Value,int bCommit)
{
   int Ret = 0;   // Assume the best
   SharedMem *p = g_pShared;
   char *Current;
   int VarLen;
   bool bStartCommitTimer = false;
   char *cp;
   int i;

   if(p != NULL) {
      WLOG("%s='%s', %scommit",Variable,Value,bCommit ? "" : "don't ");
      do {
         GetSemaphore(p);
         if((i = NvramFind(Variable,&Current)) != -1) {
         // Variable exists
            VLOG("Variable exists");
            if(strcmp(Value,Current) == 0) {
            // Value is already set, nothing more to do
               WLOG("Value is unchanged");
               p->IgnoredSets++;
               break;
            }
            if(strlen(Current) >= strlen(Value)) {
            // Value fits in place
               VLOG("New value fits in place");
               p->Sets++;
               p->Flags |= FLAG_DIRTY;
               strcpy(Current,Value);
               bStartCommitTimer = bCommit;
               break;
            }
            else {
               VLOG("New value doesn't fit in place");
            }
         }
         else {
         // New variable
            VLOG("New variable");
            if(p->NumEntries > MAX_TBL_ENTRIES-1) {
               LOG("error - table full");
               Ret = -1;
               break;
            }
         }

         VLOG("Adding variable value at end of data.");
      // Try to add variable at end
         VarLen = strlen(Variable) + strlen(Value) + 2;
         VLOG("VarLen: %d, p->DataLen: %d,  p->EndOfData: %d",
             VarLen,p->DataLen,p->EndOfData);
         if(p->EndOfData + VarLen + 1 > p->DataLen) {
            ELOG("error - NVRAM full");
            Ret = -1;
            break;
         }

      // Variable will fit, add it to the end
         p->Sets++;
         cp = &p->Nvram[p->EndOfData];
#ifdef BTREE_SEARCH
         if(i == -1) {
         // Add new entry to index 
            WLOG("Adding new variable at offset %d, NumEntries: %d",
                   p->EndOfData,p->NumEntries+1);
            p->Index[p->NumEntries++] = p->EndOfData;
         }
         else {
            p->Index[i] = p->EndOfData;
         }
#endif

#ifdef MRU_SEARCH
         if(i == -1) {
         // Add new entry to MRU table
            p->MruArray[p->NumEntries].Offset = p->EndOfData;
            p->MruArray[p->NumEntries].Next = p->MruArrayHead;
            p->MruArrayHead = p->NumEntries++;
         }
         else {
         // Update MRU table
            LOG("p->MruArray[i].Offset %d -> %d",
                p->MruArray[i].Offset,p->EndOfData);

            p->MruArray[i].Offset = p->EndOfData;
         }
#endif

         strcpy(cp,Variable);
         cp += strlen(Variable);
         *cp++ = '=';
         strcpy(cp,Value);
         cp += strlen(Value) + 1;
         p->EndOfData = cp - p->Nvram;
         VLOG("New EndOfData: %d",p->EndOfData);
#ifdef BTREE_SEARCH
         // Resort the index
         if(i == -1) {
            qsort(p->Index,p->NumEntries,sizeof(int),NameCompare);
         }
#endif
         p->Flags |= FLAG_DIRTY;
         bStartCommitTimer = bCommit;
      } while(false);

      if(bStartCommitTimer) {
         WLOG("Calling TriggerCommit");
         TriggerCommit(p,false);
      }
      ReleaseSemaphore(p);

      ALOG("Returning %d",Ret);
   }

   return Ret;
}

int NvramSetInt(char *Variable,int Value,int bCommit)
{
   char String[16];

   snprintf(String,sizeof(String),"%d",Value);
   return NvramSet(Variable,String,bCommit);
}

static int NvramCommitInternal()
{
   int Ret = 0;   // Assume the best
   SharedMem *p = g_pShared;
   char *cp;
   char *cp1;
   int i;
   char *pNew = (char *) calloc(p->DataLen,1);
   int CopyLen;
   uLong CalclatedCRC;
   int Commits = 0;
   uint32_t *pCrc = (uint32_t *) &p->Nvram[p->CrcOffset];
   uint8_t *pData = (uint8_t *) &p->Nvram[p->DataOffset];
   int TimeStamp = -1;

   do {
      VLOG("Called");
      GetSemaphore(p);
      if(!(p->Flags & FLAG_DIRTY)) {
      // Nothing to do
         LOG("Cache is not dirty");
         break;
      }

      if(pNew == NULL) {
         ELOG("calloc failed");
         Ret = ENOMEM;
         break;
      }
   // Increment the commit count in NVRAM
      NvramGetInt(VAR_COMMIT_COUNT,&Commits);
      Commits++;
      NvramSetInt(VAR_COMMIT_COUNT,Commits,false);
      NvramGetInt(VAR_RESET_TIME,&TimeStamp);
      if(TimeStamp == 0) {
      // Reset timestamp has been set, but it's zero because the system
      // clock wasn't set when the reset occured (probably at power up)
      // Set it now ... that's the best we can do...
         TimeStamp = NvramGetTimeStamp();
         if(TimeStamp > 0) {
            VLOG(VAR_RESET_TIME " is zero, setting it to %d\n",TimeStamp);
            NvramSetInt(VAR_RESET_TIME,TimeStamp,false);
         }
      }

#ifdef MRU_SEARCH
      i = p->MruArrayHead;
      cp1 = pNew;
      p->NumEntries = 0;

      for( ; ; ) {
         cp = &p->Nvram[p->MruArray[i].Offset];
         CopyLen = strlen(cp) + 1;
         memcpy(cp1,cp,CopyLen);
         cp1 += CopyLen;
         i = p->MruArray[i].Next;
         if(i < 0) {
            break;
         }
         p->NumEntries++;
      }
#endif

#ifdef BTREE_SEARCH
      cp1 = pNew;

      for(i = 0; i < p->NumEntries; i++) {
         cp = &p->Nvram[p->Index[i]];
         CopyLen = strlen(cp) + 1;
         memcpy(cp1,cp,CopyLen);
         cp1 += CopyLen;
      }
#endif

      memcpy(&p->Nvram[p->DataOffset],pNew,p->DataLen);
      free(pNew);
      pNew = NULL;

      CalclatedCRC = crc32(0,pData,p->DataLen);
      *pCrc = (unsigned int) CalclatedCRC;

      if(p->Flags & FLAG_GEMTEK) {
      // Update Gemtek values
         env_image_gemtek *pGemtek = (env_image_gemtek *) p->Nvram;
         pGemtek->count = p->NumEntries;
         pGemtek->eod = p->EndOfData;
      }
   // write back to flash

#ifdef __MIPSEL__
      LOG("Flushing cache to flash");
      if((Ret = FlashWrite(p,p->Nvram)) != 0) {
         break;
      }
      LOG("Flash write complete");
#else
// Desktop, write NVRAM data from file
      {
         int Wrote;
         FILE *fp;

         VLOG("Write cache to file");
         if((fp = fopen(NVRAM_FILENAME,"w")) == NULL) {
            LOG("fopen failed - %s",strerror(errno));
            break;
         }

         Wrote = fwrite(p->Nvram,p->NvramLen,1,fp);
         fclose(fp);

         if(Wrote != 1) {
            LOG("fwrite failed - %s",strerror(errno));
            break;
         }
      }
#endif
      BuildIndex(p);
      p->Commits++;
      p->TotalCommits++;
   } while(false);

   if(Ret == 0) {
      p->Flags &= ~FLAG_DIRTY;
   }
   else {
      p->Errors++;
   }
   ReleaseSemaphore(p);

   if(pNew != NULL) {
      free(pNew);
   }

   VLOG("Returning %d",Ret);

   return Ret;
}

// Start NVRAM daemon if not already running.
// If it is already running and bSendSignal is true then send it a SIGHUP
// to wake it up.
int NvramStartDaemon(int bSendSignal)
{
   SharedMem *p = g_pShared;
   int Ret = 0;   // assume the best

   LOG("Called");

   GetSemaphore(p);
   if(p->DaemonPID != -1) {
   // The daemon should be running, verify that it is.
      if(kill(p->DaemonPID,0) == -1) {
         ELOG("NVRAM daemon pid %d is no longer running",p->DaemonPID);
         p->DaemonPID = -1;
      }
   }

   if(p->DaemonPID == -1) {
      LOG("Starting daemon");
      p->Flags |= FLAG_DAEMON_STARTING;
      p->bRunDaemon = true;
      if((Ret = system(START_DAEMON_CMD)) != 0) {
         p->Flags &= ~FLAG_DAEMON_STARTING;
         ELOG("Error - system() returned %d",Ret);
      }
      else {
         DLOG("system() returned %d",Ret);
      }
   }
   else if(bSendSignal) {
      LOG("Sending SIGHUP to nvramd (%d)",p->DaemonPID);
      if(kill(p->DaemonPID,SIGHUP) == -1) {
         ELOG("kill failed - %s",strerror(errno));
      }
   }
   ReleaseSemaphore(p);

   return Ret;
}

static void SigHupHandler(int Signal)
{
   SharedMem *p = g_pShared;

   if(p != NULL && Signal == SIGTERM) {
      p->bRunDaemon = false;
   }
}

int NvramDaemon(bool bForground)
{
   SharedMem *p = g_pShared;
   struct sigaction NewAction;
   useconds_t WaitTime;
   int Ret = -2;   // assume the worse

   NewAction.sa_flags = 0;
   NewAction.sa_handler = SigHupHandler;

   ALOG("Called");
   do {
      if(p->DaemonPID != -1) {
         if(kill(p->DaemonPID,0) != -1) {
            ELOG("Can't start daemon, it's already running (pid: %d)",
                 p->DaemonPID);
            Ret = EEXIST;
            break;
         }
         p->DaemonPID = -1;
      }

      Ret = 0;
      if(bForground) {
      // Running in debug mode, set bRunDaemon which is normally set by
      // NvramStartDaemon()
         p->bRunDaemon = true;
      }
      else {
         if(daemon(true,0) == -1) {
            ELOG("daemon failed - %s",strerror(errno));
            Ret = errno;
            break;
         }

      }
#ifndef __MIPSEL__
      if(!bForground && gLogfp == NULL) {
         gLogfp = fopen("nvramd.log","w");
      }
#endif

      p->DaemonPID = getpid();
      ELOG("NVRAM daemon starting - pid %d",p->DaemonPID);
      GetSemaphore(p);
      p->Flags &= ~FLAG_DAEMON_STARTING;
      p->DaemonStarts++;

      sigemptyset(&NewAction.sa_mask);
      if(sigaction(SIGHUP,&NewAction,NULL)) {
         ELOG("sigaction failed - %s",strerror(errno));
         break;
      }

      if(sigaction(SIGTERM,&NewAction,NULL)) {
         ELOG("sigaction failed - %s",strerror(errno));
         break;
      }

      for( ; ; ) {
      // Check the commit timers
         GetUpTime();
         if(p->CommitTimer != 0 && p->CommitTimer < gUpTime) {
            p->CommitTimer = 0;
            DLOG("CommitTimer timeout, calling NvramCommitInternal");
            NvramCommitInternal();
         }
         else {
            if(p->CommitTimer != 0) {
               DLOG("gUpTime: %" PRId64 ", CommitTimer: %" PRId64 "",
                    gUpTime,p->CommitTimer);
               WaitTime = (useconds_t)(p->CommitTimer - gUpTime);
               WaitTime *= 1000; // convert to microseconds
               ReleaseSemaphore(p);
               DLOG("calling usleep for %d microseconds",WaitTime);
               usleep(WaitTime);
               GetSemaphore(p);
            }
            else {
               ReleaseSemaphore(p);
               DLOG("calling pause");
               pause();
               DLOG("pause returned");
               GetSemaphore(p);
            }

            if(!p->bRunDaemon) {
               DLOG("Shutting down");
               NvramCommitInternal();
               p->DaemonPID = -1;
               p->DaemonExits++;
               ReleaseSemaphore(p);
               break;
            }
         }
      }
   } while(false);

   if(Ret == -2 && errno != 0) {
      Ret = errno;
   }

   ALOG("Returning %d",Ret);

   return Ret;
}

static void GetSemaphore(SharedMem *p)
{
   struct timespec TimeOut;
   bool bLoggedTimeout = false;
   pid_t OwnerTID = 0;
   pid_t MyTID = (pid_t) syscall(SYS_gettid);

   for( ; ; ) {
      if(p->SemOwnerTID == MyTID) {
      // already own it, just bump the count
         break;
      }
      if(clock_gettime(CLOCK_REALTIME,&TimeOut) != 0) {
         ELOG("clock_gettime failed - %s",strerror(errno));
         break;
      }
      TimeOut.tv_nsec += 100000;    // wait 100 milliseconds max
      if(TimeOut.tv_nsec >= 1000000000) {
         TimeOut.tv_sec++;
         TimeOut.tv_nsec %= 1000000000;
      }

      if(!bLoggedTimeout){
         SLOG("Calling sem_timedwait");
      }
      if(sem_timedwait(&p->Semaphore,&TimeOut) == 0) {
      // Got it
         p->SemOwnerTID = MyTID;
         break;
      }
      if(errno != ETIMEDOUT) {
      // NB: do NOT exit the wait loop on error return from sem_timedwait.
      // For example it might have been EINTR caused by a SIGCHILD or some 
      // such.  If we exit the loop we'll think we own the semaphore when 
      // we do not.  Just log the error and keep waiting
         ELOG("sem_timedwait failed - %s",strerror(errno));
      }

   // Check and make sure the owner is still alive
      if(sem_wait(&p->RecoverySemaphore) != 0) {
      // Didn't get it
      // NB: do NOT exit the wait loop on error return from sem_wait.
         ELOG("sem_wait on RecoverySemaphore failed - %s",strerror(errno));
         sleep(1);
         continue;
      }

      OwnerTID = p->SemOwnerTID;
      if(kill(OwnerTID,0) == -1) {
      // Release semaphore on behalf of prior owner
         ELOG("NVRAM Semaphore owner tid %d died.",OwnerTID);
         p->SemOwnerTID = -1;
         p->SemCount = 0;
         if(sem_post(&p->Semaphore) == -1) {
            ELOG("sem_post failed - %s",strerror(errno));
         }
         else {
            ELOG("Reset Semaphore state");
         }
      }
      else if(!bLoggedTimeout){
         bLoggedTimeout = true;   // Once is enough
         ELOG("Timeout waiting NVRAM Semaphore owned by tid %d.",
              p->SemOwnerTID);
      }
      for( ; ; ) {
         if(sem_post(&p->RecoverySemaphore) == 0) {
            break;
         }
      // NB: do NOT exit the wait loop on error return from sem_post,
      // Wait a second and try it again (see additional comments above)
         ELOG("sem_post on RecoverySemaphore failed - %s",strerror(errno));
         sleep(1);
      }
   }
   p->SemCount++;
   if(bLoggedTimeout) {
      LOG("Got semaphore previously owned by pid %d.",OwnerTID);
   }
   if(p->SemCount > 1) {
      SLOG("Returning, SemCount: %d",p->SemCount);
   }
   else {
      SLOG("Returning");
   }
}

static void ReleaseSemaphore(SharedMem *p)
{
   pid_t MyTID = (pid_t) syscall(SYS_gettid);

   SLOG("Called");
   if(p->SemOwnerTID == -1) {
      ELOG("Error - Semaphore is not owned");
   }
   else if(p->SemOwnerTID != MyTID) {
      ELOG("Error - I'm not the owner, the owner is tid: %d",p->SemOwnerTID);
   }
   else {
      if(--p->SemCount == 0) {
         p->SemOwnerTID = -1;
         if(sem_post(&p->Semaphore) == -1) {
            ELOG("sem_post failed - %s",strerror(errno));
         }
      }
      else {
         SLOG("%d release calls still pending",p->SemCount);
      }
   }
   SLOG("Returning");
}

// This is a function needed for unit testing, it is not called 
// during normal operation
int NvramShutdown()
{
   int Ret = 0;
   SharedMem *p = g_pShared;

   LOG("Called");
   if(p != NULL) do {
   // TODO check for waiters
      GetSemaphore(p);
      if(p->DaemonPID != -1) {
      // Shutdown Daemon
         if(kill(p->DaemonPID,SIGTERM) == -1) {
            ELOG("kill failed sending SIGTERM to pid %d - %s)",
                 p->DaemonPID,strerror(errno));
         }
      }
      if(sem_destroy(&p->Semaphore) == -1) {
         ELOG("sem_destroy failed - %s",strerror(errno));
         Ret = errno;
         break;
      }
      if(munmap(p,gSharedMemLen) != 0) {
         ELOG("munmap failed - %s",strerror(errno));
         Ret = errno;
         break;
      }

      if(shm_unlink(SHARED_MEM_NAME) != 0) {
         ELOG("shm_unlink failed - %s",strerror(errno));
         Ret = errno;
         break;
      }
      g_pShared = NULL;
   } while(false);

   VLOG("Returning %d",Ret);

   return Ret;
}

// Legacy call from Belkin_api
// argv[1] = variable name
// if buffer == NULL then printf value
int nvramget(int argc, char *argv[], char *buffer)
{
   int Ret = EINVAL;    // Assume the worse
   char *cp;

   do {
      ALOG("Called");
      if(libNvramInit() != 0) {
         break;
      }
      if(argc == 2) {
      // get
         cp = NvramGet(argv[1]);
         if(buffer != NULL) {
            strcpy(buffer,cp);
         }
         else {
            printf("%s\n",cp);
         }
         Ret = 0;
      }
      else {
         ELOG("Error: Invalid argument count %d",argc);
      }
   } while(false);

   return Ret;
}

// Legacy call from Belkin_api
// argv[1] = variable name
// argv[2] = value

// Lots of possibilities:
// nvram set <variable>=<value>
// nvram set <variable>=            (unset)
// nvram_set <variable> <value>
// nvram_set <variable>             (unset)
// return 0 on success, 1 on failure
int nvramset(int argc, char *argv[])
{
   char *cp;
   int VariableArg = 2;
   char *Value = NULL;
   const char *ProgramName = basename(argv[0]);
   int Ret = 1;   // Assume the worse

   do {
      ALOG("Called");
      if(strcmp(ProgramName,"nvram_set") == 0) {
         VariableArg = 1;
         if(libNvramInit() != 0) {
            break;
         }
      }

      if((argc - VariableArg) == 1) {
      // There's a single argument, if it has a '=' in it then it's a set,
      // Otherwise it's an unset
         if((cp = strchr(argv[VariableArg],'=')) != NULL) {
            *cp++ = 0;
            if(strlen(cp) > 0) {
               Value = cp;
            }
         }
      }
      else if((argc - VariableArg) == 2) {
      // There are two arguments, the second one is the value
         Value = argv[VariableArg + 1];
      }
      else {
         break;
      }

      if(Value != NULL) {
      // Set command
         WLOG("Calling NvramSet('%s','%s',true)",argv[VariableArg],Value);
         Ret = NvramSet(argv[VariableArg],Value,true);
      }
      else {
      // Unset command
         WLOG("Calling NvramUnset('%s',true)",argv[VariableArg]);
         Ret = NvramUnset(argv[VariableArg],true);
      }
   } while(false);


   ALOG("Returning %d",Ret);
   return Ret;
}

int NvramStatus()
{
   SharedMem *p = g_pShared;
   bool bNeedComma = false;
   MaskName *pFlagDesc = FlagDescs;

   printf("Flags: ");
   if(p->Flags == 0) {
      printf("none");
   }
   else while(pFlagDesc->Mask != 0){
      if(p->Flags & pFlagDesc->Mask) {
         printf("%s%s",bNeedComma ? ", " : "",pFlagDesc->Desc);
         bNeedComma = true;
      }
      pFlagDesc++;
   }
   printf("\n");

   if(p->SemOwnerTID != -1) {
      if(kill(p->SemOwnerTID,0) == -1) {
         printf("NVRAM Semaphore owner tid %d died.\n",p->SemOwnerTID);
      }
      else {
         printf("NVRAM Semaphore owned by tid %d.\n",p->SemOwnerTID);
         printf("NVRAM Semaphore SemCount %d.\n",p->SemCount);
      }
   }
   else {
      printf("NVRAM Semaphore is available.\n");
   }

   if(p->DaemonPID != -1) {
      if(kill(p->DaemonPID,0) == -1) {
         printf("NVRAM daemon is no longer running (pid %d).\n",p->DaemonPID);
      }
      else {
         printf("NVRAM daemon is running (pid %d).\n",p->DaemonPID);
      }
   }
   else {
      printf("NVRAM daemon is not running.\n");
   }
   printf("\n");
   printf("   Variables: %d\n",p->NumEntries);
   printf("        Sets: %d\n",p->Sets);
   printf(" IgnoredSets: %d\n",p->IgnoredSets);
   printf("      UnSets: %d\n",p->UnSets);
   printf("        Gets: %d\n",p->Gets);
   printf("    NotFound: %d\n",p->NotFound);
   printf("     Commits: %d\n",p->Commits);
   printf("TotalCommits: %d\n",p->TotalCommits);
   printf("      Errors: %d\n",p->Errors);
   printf("DaemonStarts: %d\n",p->DaemonStarts);
   printf(" DaemonExits: %d\n",p->DaemonExits);
   printf("InternalErrs: %d\n",p->InternalErrs);

   if(p->ResetTime != 0 || p->ResetType != 0) {
      printf("\n");
      printf("Last NVRAM Reset:\n");
      printf("  Cause: ");
      switch(p->ResetType) {
         case RESTORE_TYPE_ERASE:
            printf("Nvram utility reset command\n");
            break;

         case RESTORE_TYPE_RESTORE:
            printf("Nvram utility restore command\n");
            break;
            
         case RESTORE_TYPE_CORRUPTED:
            printf("Automatic - nvram corruption detected\n");
            break;

         default:
            printf("Unknown (0x%x)\n",p->ResetType);
            break;
      }

      printf("   Time: ");
      if(p->ResetTime != 0) {
         struct tm Time;
         time_t ResetTime = (time_t) p->ResetTime;

         localtime_r(&ResetTime,&Time);
         printf("%d/%02d/%d %2d:%02d:%02d (local)\n",
                Time.tm_mon + 1,Time.tm_mday,Time.tm_year + 1900,
                Time.tm_hour,Time.tm_min,Time.tm_sec);
      }
      else {
         printf("unknown\n");
      }
   }

   if(p->Verbose != 0) {
      int i;

      printf("\n");
      printf("Logging levels enabled: 0x%x:\n",p->Verbose);
      for(i = 0; NvramDebugMasks[i].Mask != 0; i++) {
         if(p->Verbose & NvramDebugMasks[i].Mask) {
            printf("  %08x - %s\n",NvramDebugMasks[i].Mask,NvramDebugMasks[i].Desc);
         }
      }
   }


#ifdef __MIPSEL__
   printf("\n");
   printf("  MTD Device: %s\n",p->MtdName);
   printf("  Erase size: 0x%x (%d)\n",p->EraseSize,p->EraseSize);
   printf("  NVRAM size: 0x%x (%d)\n",p->NvramLen,p->NvramLen);
#endif
   return 0;
}

// Set NVRAM subsystem logging level.  
//    bPersist: 0 - Don't save level in NVRAM
//    bPersist: 1 - Save level in NVRAM variable 'NvramLogMask'
// The NVRAM subsystem logging level is restored from 'NvramLogMask' if it
// exists on library initialization.
int NvramSetLogMask(int Mask,int bPersist)
{
   int Ret = 0;
   do {
      if(g_pShared == NULL) {
         if((Ret = NvramInit(false,NULL)) != 0) {
            break;
         }
      }
      g_pShared->Verbose = Mask;
      if(bPersist) {
         NvramSetInt(VAR_LOG_MASK,Mask,true);
      }
   } while(false);

   return Ret;
}


// Return integer number of seconds since Unix epoc, or 0 if the
// system time hasn't bee set yet
int NvramGetTimeStamp()
{
   time_t TimeT;
   struct tm Time;

   time(&TimeT);
   localtime_r(&TimeT,&Time);
   if((Time.tm_year + 1900) < 2015) {
   // Time isn't set, store a zero for now
      TimeT = 0;
   }

   return (int) TimeT;
}

int NvramCommit()
{
   int Ret = 0;   // assume the best
   SharedMem *p = g_pShared;

   GetSemaphore(p);
   if(p->Flags & FLAG_DIRTY) {
   // Only bother if there's actually something to commit
      Ret = TriggerCommit(p,true);
   }
   ReleaseSemaphore(p);

   return Ret;
}

#ifdef __MIPSEL__

#ifdef CONFIG_FILE

static int get_config(NvramInfo *p)
{
   FILE *fp;
   char Line[128];
   int Ret = -1;  // assume the worse
   int Converted;

   do {
      if((fp = fopen(CONFIG_FILE,"r")) == NULL) {
         ELOG("fopen(" CONFIG_FILE "failed - %s",strerror(errno));
         Ret = errno;
         break;
      }

      while(fgets(Line,sizeof(Line),fp) != NULL) {
         if (Line[0] == '#') {
         // Ignore comments
            continue;
         }

         Converted = sscanf(Line,"%16s %*x %x %x",p->MtdName,&p->NvramLen,
                            &p->EraseSize);
         if(Converted == 3) {
            Ret = 0;
            break;
         }
      }
      fclose(fp);
   } while(false);

   return Ret;
}
#endif

static int ParseProc(NvramInfo *p)
{
	FILE *fp = NULL;
	int Ret = -1;	// assume the worse
	char Line[80];
	unsigned int Size;
	unsigned int EraseSize;
	char *cp;

	do {
		if((fp = fopen("/proc/mtd","r")) == NULL) {
			fprintf(stderr,"fopen(/proc/mtd) failed: %s\n", strerror (errno));
			break;
		}
		while(fgets(Line,sizeof(Line),fp) != NULL) {
			if((cp = strchr(Line,'"')) != NULL) {
				if(strncasecmp(cp+1,"nvram",5) == 0) {
				// Found the NVRAM partition
					if((cp = strchr(Line,':')) != NULL) {
						*cp++ = 0;
						strcpy(p->MtdName,"/dev/");
						strcat(p->MtdName,Line);

						if(sscanf(cp," %x %x",&Size,&EraseSize) == 2) {
							p->NvramLen = Size;
							p->EraseSize = EraseSize;
							Ret = 0;
						}
					}
					break;
				}
			}
		}
	} while(false);

	if(fp != NULL) {
		fclose(fp);
	}

	return Ret;
}

// Write nvram image to flash
static int FlashWrite(SharedMem *p,char *Buffer)
{
	struct erase_info_user Erase;
	int Ret = -2;
   int fd = -1;
   int Wrote;
   int ByteWritten;
   bool bUnLocked = false;
   int Line = 0;

   do {
      VLOG("Called");
      if(strlen(p->MtdName) == 0) {
         Line = __LINE__;
         break;
      }

      if(p->EraseSize == 0) {
         Line = __LINE__;
         break;
      }

      if((fd = open(p->MtdName,O_RDWR)) == -1) {
         ELOG("open(%s,O_RDWR) failed - %s",p->MtdName,strerror(errno));
         break;
      }

      Erase.start = 0;
      Erase.length = p->EraseSize;
      if(ioctl(fd,MEMUNLOCK,&Erase) < 0) {
      //  Not a fatal error, perhaps locking/unlocking isn't supported
         ELOG("ioctl(MEMUNLOCK) failed - %s",strerror(errno));
      }
      else {
         bUnLocked = true;
      }

      if(ioctl(fd,MEMERASE,&Erase) < 0) {
         ELOG("ioctl(MEMERASE) failed - %s",strerror(errno));
         break;
      }

      for(Wrote = 0; Wrote < p->NvramLen; Wrote += p->EraseSize) {
         ByteWritten = write(fd,&Buffer[Wrote],p->EraseSize);
         if(ByteWritten != p->EraseSize) {
            ELOG("write failed, requested %d, wrote %d",
                 p->EraseSize,ByteWritten);
            break;
         }
      }

      if(Wrote < p->NvramLen) {
         ELOG("write failed - %s",strerror(errno));
         break;
      }
      Ret = 0; // success
   } while(false);

   if(bUnLocked) {
      if(ioctl(fd,MEMLOCK,&Erase) < 0) {
      // This is an error since we *were* able to unlock it...
         Ret = -2;
         ELOG("ioctl(MEMLOCK) failed - %s",strerror(errno));
      }
   }

   if(fd != -1) {
      close(fd);
   }

   if(Line != 0) {
   // Internal error
      ELOG("Internal error line %d",Line);
      p->InternalErrs++;
      Ret = -1;
   }
   else if(Ret == -2 && errno != 0) {
      Ret = errno;
   }

   VLOG("Returning %d",Ret);

   return Ret;
}

// Write contents of Path to flash
int NvramWriteFile(char *Path)
{
   SharedMem *p = g_pShared;
   int Ret = 0;
   FILE *fp = NULL;
   char *Buffer = NULL;
   struct stat Stat;

   do {
      if(stat(Path,&Stat) != 0) {
         ELOG("stat('%s') failed - %s",Path,strerror(errno));
         Ret = errno;
         break;
      }

      if(Stat.st_size != p->NvramLen) {
         ELOG("file size (%d) does not match flash size (%d)",
              (int) Stat.st_size,p->NvramLen);
         Ret = -1;
         break;
      }

      if((Buffer = malloc(p->NvramLen)) == NULL) {
         ELOG("Malloc failed");
         Ret = ENOMEM;
         break;
      }

      if((fp = fopen(Path,"r")) == NULL) {
         ELOG("Can't open '%s' - %s",Path,strerror(errno));
         Ret = errno;
         break;
      }

      if(fread(Buffer,p->NvramLen,1,fp) != 1) {
         ELOG("fread filed ' - %s",strerror(errno));
         Ret = errno;
         break;
      }
      GetSemaphore(p);
      Ret = FlashWrite(p,Buffer);
   // Prevent the daemon from flushing the cache on shutdown.
      p->Flags &= ~FLAG_DIRTY;
   // Shutdown flash to force a reload
      NvramShutdown();
   // NB: Don't release the Semaphore, NvramShutdown() has deleted it!
   } while(false);

   if(fp != NULL) {
      fclose(fp);
   }

   if(Buffer != NULL) {
      free(Buffer);
   }

   return Ret;
}

// Read nvram image from flash
static int FlashRead(SharedMem *p)
{
	int Ret = -2;
   int fd = -1;
   int Line = 0;

   do {
      if(strlen(p->MtdName) == 0) {
         Line = __LINE__;
         break;
      }

      if(p->NvramLen == 0) {
         Line = __LINE__;
         break;
      }

      if((fd = open(p->MtdName,O_RDONLY)) == -1) {
         ELOG("open(%s,O_RDONLY) failed - %s",p->MtdName,strerror(errno));
         break;
      }

      if(read(fd,p->Nvram,p->NvramLen) != p->NvramLen) {
         ELOG("read failed - %s",strerror(errno));
         break;
      }
      Ret = 0; // success
   } while(false);

   if(fd != -1) {
      close(fd);
   }

   if(Line != 0) {
   // Internal error
      ELOG("Internal error line %d",Line);
      p->InternalErrs++;
      Ret = -1;
   }
   else if(Ret == -2 && errno != 0) {
      Ret = errno;
   }

   return Ret;
}
#endif


#ifndef __MIPSEL__
void logger(const char *fmt,...)
{
   char Line[120];
   va_list args;
   va_start(args,fmt);

   vsnprintf(Line,sizeof(Line),fmt,args);
   if(gLogfp != NULL) {
      fprintf(gLogfp,"%s",Line);
      fflush(gLogfp);
   }
   else {
      fprintf(stderr,"%s",Line);
   }
   va_end(args);
}
#endif   // __MIPSEL__

