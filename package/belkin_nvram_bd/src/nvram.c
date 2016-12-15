/***************************************************************************
*
* Created by Belkin International, Software Engineering on XX/XX/XX.
* Copyright (c) 2012-2015 Belkin International, Inc. and/or its affiliates. All rights reserved.
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
#include <getopt.h>
#include <syslog.h>
#include <sys/syscall.h>
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

#include "libnvram_internal.h"
#include "libnvram.h"

#define OPTION_STRING         "hnv:V:"
#define DAEMON_OPTION_STRING  "hfv:V:"

#define HELP_CMD  "help"

SharedMem *g_pShared = NULL;

int NvramGetCmd(int argc, char *argv[]);
int NvramSetCmd(int argc, char *argv[]);
int NvramCommand(int argc, char *argv[]);
int NvramShowCmd(int argc, char *argv[]);
int NvramRestoreCmd(int argc, char *argv[]);
int NvramResetCmd(int argc, char *argv[]);
int NvramFactoryCmd(int argc, char *argv[]);
int NvramCommitCmd(int argc, char *argv[]);
int NvramHelpCmd(int argc, char *argv[]);
int NvramStatusCmd(int argc, char *argv[]);
int NvramStartDaemonCmd(int argc, char *argv[]);
int NvramShutdownCmd(int argc, char *argv[]);
int NvramUnsetCmd(int argc, char *argv[]);
#ifdef __MIPSEL__
int NvramWriteCmd(int argc, char *argv[]);
#endif

void Usage(void);
void DaemonUsage(void);

typedef int (*NvramCmd)(int argc, char *argv[]);

typedef struct {
   char  *Cmd;
   NvramCmd Funct;
} CmdTbl;

CmdTbl Cmd1ArgTbl[] = {
   {"get",NvramGetCmd},
   {"firstread",NvramGetCmd},
   {"set",NvramSetCmd},
   {"unset",NvramUnsetCmd},
#ifdef __MIPSEL__
   {"write",NvramWriteCmd},
#endif
   {NULL}   // End of table
};

CmdTbl CmdNoArgTbl[] = {
   {"commit",NvramCommitCmd},
   {"factory",NvramFactoryCmd},
   {"reset",NvramResetCmd},
   {"restore",NvramRestoreCmd},
   {"show",NvramShowCmd},
   {"shutdown",NvramShutdownCmd},
   {"status",NvramStatusCmd},
   {"updatecrc",NvramCommitCmd},
   {HELP_CMD,NvramHelpCmd},
   {NULL}   // End of table
};

CmdTbl AliasTbl[] = {
   {"nvram_set", NvramSetCmd},
   {"nvram_get", NvramGetCmd},
   {"nvram", NvramCommand},
   {"nvramd", NvramStartDaemonCmd},
   {NULL}   // End of table
};

bool g_bCommit = true;   // default to true for backwards compatibility

int NvramCommand(int argc, char *argv[])
{
   int i;
   int Ret = 0;   // assume the best
   CmdTbl *pCmds = NULL;
   bool bExit = false;
   bool bLogLevelSet = false;
   int Option;
   int Verbose;

   while((Option = getopt(argc, argv,OPTION_STRING)) != -1) {
      switch (Option) {
         case '?':
         case 'h':   // Help
            Usage();
            bExit = true;
            break;

         case 'n':   // don't automatically commit on set
            g_bCommit = false;
            break;

         case 'V':
         case 'v':
            if(*optarg == '?') {
               int i;
               printf("Verbose logging levels available:\n");
               for(i = 0; NvramDebugMasks[i].Mask != 0; i++) {
                  printf("%08x - %s\n",NvramDebugMasks[i].Mask,NvramDebugMasks[i].Desc);
               }
               bExit = true;
            }
            else if(sscanf(optarg,"%x",&Verbose) != 1) {
               Ret = EINVAL;
            }
            else {
               if((Ret = NvramSetLogMask(Verbose,Option == 'V')) == 0) {
                  bLogLevelSet = true;
               }
            }
            break;

         default:
           Ret = EINVAL;
           break;
      }
   }

// Remove command line options
   if(optind > 1) {
      for(i = 1; i <= optind; i++) {
         argv[i] = argv[optind+i-1];
      }
      argc -= optind - 1;
   }

   if(Ret == 0 && !bExit) {
      switch(argc-1) {
         default:
            fprintf(stderr,"Error - invalid number of arguments (%d)\n",argc - optind);
            Ret = EINVAL;
         // Intentional fall though to next case
         case 0:  // No arguments
            if(!bLogLevelSet) {
               Usage();
            }
            break;

         case 1:
            pCmds = CmdNoArgTbl;
            break;

         case 2:
            pCmds = Cmd1ArgTbl;
            break;
      }

      if(pCmds != NULL) {
         for(i = 0; pCmds[i].Cmd != NULL; i++) {
            if(strcmp(argv[1],pCmds[i].Cmd) == 0) {
               if(strcmp(argv[1],"firstread") == 0 || 
                  strcmp(argv[1],"reset") == 0) 
               {
                  SYSLOG(LOG_DEBUG,"nvram %s called",argv[1]);
                  Ret = NvramInit(true,"nvram");
               }
               else if(strcmp(argv[1],HELP_CMD) != 0) {
                  Ret = NvramInit(false,"nvram");
               }

               if(Ret != 0) {
                  fprintf(stderr,"NvramInit failed - %s\n",strerror(Ret));
               }
               else {
                  Ret = pCmds[i].Funct(argc,argv);
               }
               break;
            }
         }
         if(Ret == EINVAL) {
            fprintf(stderr,"Invalid argument\n");
            Usage();
         }

         if(pCmds[i].Cmd == NULL) {
            Ret = EINVAL;
            fprintf(stderr,"Error: invalid command '%s'\n",argv[1]);
            Usage();
         }
      }
   }

   return Ret;
}

int main(int argc, char *argv[])
{
   int i;
   const char *ProgramName = basename(argv[0]);
   int Ret = EINVAL; // assume the worse

   /* Initialize Belkin diagnostics, if enabled */

#if 0
// The stock init_diagnostics() macro defined in belkin_diag.h uses 
// GetBelkinParameter which we can't use while debugging the nvram library 
// so we setup dmalloc manually here.

   dmalloc_debug_setup(DEFAULT_DM_CONFIG);
#endif

// Look for a alias entry
   for(i = 0; AliasTbl[i].Cmd != NULL; i++) {
      if(strcmp(ProgramName,AliasTbl[i].Cmd) == 0) {
         Ret = AliasTbl[i].Funct(argc,argv);
         break;
      }
   }

   if(AliasTbl[i].Cmd == NULL) {
      fprintf(stderr,"Identity crisis, who is %s?\n",ProgramName);
   }

   return Ret;
}

int NvramGetCmd(int argc, char *argv[])
{
   char *cp;
   int VariableArg = 2;
   const char *ProgramName = basename(argv[0]);

   if(strcmp(ProgramName,"nvram_get") == 0) {
      libNvramInit();
      VariableArg = 1;
   }

   cp = NvramGet(argv[VariableArg]);

   if(strlen(cp) > 0) {
      printf("%s\n",cp);
   }

   return 0;
}

// Lots of possibilities:
// nvram set <variable>=<value>
// nvram set <variable>=            (unset)
// nvram_set <variable> <value>
// nvram_set <variable>             (unset)
// return 0 on success, 1 on failure
int NvramSetCmd(int argc, char *argv[])
{
   char *cp;
   int VariableArg = 2;
   char *Value = NULL;
   const char *ProgramName = basename(argv[0]);
   int Ret = 1;   // Assume the worse

   do {
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
         SYSLOG(LOG_DEBUG,"Set %s=%s",argv[VariableArg],Value);
         Ret = NvramSet(argv[VariableArg],Value,g_bCommit);
      }
      else {
      // Unset command
         SYSLOG(LOG_DEBUG,"Unset %s",argv[VariableArg]);
         Ret = NvramUnset(argv[VariableArg],g_bCommit);
      }
   } while(false);

   if(Ret != 0 && VariableArg != 2) {
   // Error and not nvram_set command, display usage
      printf("Missing arguments\n");
      Usage();
   }

   return Ret;
}

int NvramShowCmd(int argc, char *argv[])
{
   const char *Variable;
   int Next = -1;

   for( ; ; ) {
      Next = NvramShow(Next,&Variable);
      if(Variable == NULL) {
         break;
      }
      printf("%s\n",Variable);
   }

   return 0;
}

int NvramRestoreCmd(int argc, char *argv[])
{
   SYSLOG(LOG_DEBUG,"Called");
   return NvramRestore(RESTORE_TYPE_RESTORE);
}


int NvramResetCmd(int argc, char *argv[])
{
   SYSLOG(LOG_DEBUG,"Called");
   return NvramRestore(RESTORE_TYPE_ERASE);
}

int NvramFactoryCmd(int argc, char *argv[])
{
   SYSLOG(LOG_DEBUG,"Called");
#ifdef __MIPSEL__
   restoreFactory();
#endif
   return 0;
}

// updatecrc and commit commands
// 
// Updatecrc is provided for backwards compatibility, it is exactly the same
// as commit
int NvramCommitCmd(int argc, char *argv[])
{
   SYSLOG(LOG_DEBUG,"Called");
   return NvramCommit();
}

int NvramStatusCmd(int argc, char *argv[])
{
   return NvramStatus();
}

int NvramStartDaemonCmd(int argc, char *argv[])
{
   int Ret = 0;   // assume the best
   bool bForeground = false;
   int Option;
   int Verbose;
   bool bExit = false;
   
#ifdef DMALLOC
// The stock init_diagnostics() macro defined in belkin_diag.h uses 
// GetBelkinParameter which we can't use while debugging the nvram library 
// so we setup dmalloc manually here.

   dmalloc_debug_setup("debug=0x4e4ed03,inter=10,log=/var/log/dmalloc_nvramd.log");
#endif
      
   while((Option = getopt(argc,argv,DAEMON_OPTION_STRING)) != -1) {
      switch(Option) {
         case '?':
         case 'h':   // Help
            DaemonUsage();
            bExit = true;
            break;

         case 'f':
            bForeground = true;
            break;

         case 'v':
         case 'V':
            if(*optarg == '?') {
               int i;
               printf("Verbose logging levels available:\n");
               for(i = 0; NvramDebugMasks[i].Mask != 0; i++) {
                  printf("%08x - %s\n",NvramDebugMasks[i].Mask,NvramDebugMasks[i].Desc);
               }
               bExit = true;
            }
            else if(sscanf(optarg,"%x",&Verbose) != 1) {
               Ret = EINVAL;
            }
            else {
               Ret = NvramSetLogMask(Verbose,Option == 'V');
            }
            break;
      }
   }
      
   if(argc == 2 && strcmp(argv[1],HELP_CMD) == 0) {
      DaemonUsage();
      bExit = true;
   }
         
   if(Ret == 0 && !bExit) {
      if((Ret = NvramInit(false,"nvramd")) == 0) {
         if(!bForeground){
            fprintf(stderr,"NVRAM daemon starting\n");
         }
         if((Ret = NvramDaemon(bForeground)) == EEXIST) {
            fprintf(stderr,"Can't start daemon, it's already running\n");
         }
      }
   }

   return Ret;
}

int NvramShutdownCmd(int argc, char *argv[])
{
   return NvramShutdown();
}

int NvramUnsetCmd(int argc, char *argv[])
{
   return NvramUnset(argv[2],g_bCommit) == 0 ? 0 : ENOENT;
}

#ifdef __MIPSEL__

// nvram write <filename>
int NvramWriteCmd(int argc, char *argv[])
{
   return NvramWriteFile(argv[2]);
}
#endif

void Usage()
{
   fprintf(stderr,"nvram compiled "__DATE__ " "__TIME__"\n");
   fprintf(stderr,"usage: nvram [options] <command> [arguments]\n");
   fprintf(stderr,"  Command:\n");
   fprintf(stderr,"    help   - show nvram usage help\n");
   fprintf(stderr,"    commit - Write NVRAM cache to flash\n");
   fprintf(stderr,"    show   - dump all NVRAM variables and values\n");
   fprintf(stderr,"    get <variable>\n");
   fprintf(stderr,"    set <variable>=<value>\n");
   fprintf(stderr,"    unset <variable>\n");
   fprintf(stderr,"    restore - erase NVRAM preserving select values\n");
   fprintf(stderr,"    reset   - erase all NVRAM contents\n");
   fprintf(stderr,"    factory - Reset pseudo EEPROM (i.e. MAC addresses)\n");
   fprintf(stderr,"    status  - Display NVRAM runtime status and statistics\n");
   fprintf(stderr,"    shutdown - stop NVRAM deamon, delete shared memory\n");
   fprintf(stderr,"    firstread - first read without crc check\n");
   fprintf(stderr,"    updatecrc - update crc value for openwrt\n");
   fprintf(stderr,"    write <file> - write file to flash\n");
   fprintf(stderr,"  Options:\n");
   fprintf(stderr,"    -h      - Show nvram usage help\n");
   fprintf(stderr,"    -n      - Don't commit (set and unset commands)\n");
   fprintf(stderr,"    -v<hex> - Set verbose output mask\n");
   fprintf(stderr,"    -V<hex> - Set and save verbose output mask\n");
   fprintf(stderr,"    -v?     - Display verbose output mask levels\n");
}

void DaemonUsage()
{
   fprintf(stderr,"nvramd compiled "__DATE__ " "__TIME__"\n");
   fprintf(stderr,"usage: nvramd [options]\n");
   fprintf(stderr,"  -h      - Show nvram usage help\n");
   fprintf(stderr,"  -f      - run in foreground\n");
   fprintf(stderr,"  -v<hex> - Set verbose output mask\n");
   fprintf(stderr,"  -V<hex> - Set and save verbose output mask\n");
   fprintf(stderr,"  -v?     - Display verbose output mask levels\n");
}

int NvramHelpCmd(int argc, char *argv[])
{
   Usage();
   return 0;
}

