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
* 
***************************************************************************/

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <signal.h>

#include "wasp_api.h"

typedef unsigned char u8;     // i.e. 8 bit unsigned data
typedef unsigned short u16;   // i.e. 16 bit unsigned data
typedef unsigned int u32;     // i.e. 32 bit unsigned data

#include "echo.h"

#define LOG(format, ...) printf(format,## __VA_ARGS__ )
#define DBG(format, ...) if(gDebug) printf(format,## __VA_ARGS__ )
#define VDBG(format, ...) if(gVerbose && gDebug) printf(format,## __VA_ARGS__ )

#ifndef FALSE
#define FALSE     0
#endif

#ifndef TRUE
#define TRUE      (!FALSE)
#endif

// 244.14 samples/sec * 60 sec/minute / 30 samples/block = 
// 488.28125  blocks / minute
#define DATA_BLOCKS_PER_MINUTE         488
#define DATA_MINUTES_PER_FILE          5  // Default
#define DATA_BLOCKS_PER_FILE_DEFAULT   (DATA_MINUTES_PER_FILE * DATA_BLOCKS_PER_MINUTE)

#define QUEUE_LEN 5

#define UPLOAD_SCRIPT   "echo \"Uploading\" >> e"
#define COMPRESS_SCRIPT "echo \"Compressing\" >> e"
#define DATA_DIR        "/tmp/raw_data"
#define TEMP_FILE       "/tmp/echo.temp"

typedef union {
   u8 Byte[4];
   u16 U16;
   u32 U32;
} ValUnion;

ValUnion SwapTmp,SwapTmp1;

#define BYTE_SWAP_SHORT(x)             \
   SwapTmp.U16 = x;                    \
   SwapTmp1.Byte[0] = SwapTmp.Byte[1]; \
   SwapTmp1.Byte[1] = SwapTmp.Byte[0]; \
   x = SwapTmp1.U16

#define BYTE_SWAP_LONG(x)              \
   SwapTmp.U32 = x;                    \
   SwapTmp1.Byte[0] = SwapTmp.Byte[3]; \
   SwapTmp1.Byte[1] = SwapTmp.Byte[2]; \
   SwapTmp1.Byte[2] = SwapTmp.Byte[1]; \
   SwapTmp1.Byte[3] = SwapTmp.Byte[0]; \
   x = SwapTmp1.U32


#define OPTION_STRING  "b:cdD:fmMps:v"

time_t StartTime = 0;
struct timeval gTimeNow;

// Number of seconds since program start.
// It might wrap, but it sure as hell won't go backwards until it does.
// All timers should be based on this, not the absolute time of day which
// may jump when the time of day is updated from the Internet.
time_t UpTime = 0;

int gVerbose = 0;
int gDebug = FALSE;

int gDisplayFlow = FALSE;
int gDisplayPressure = FALSE;
int gDisplayMinPressure = FALSE;
int gDisplayMaxPressure = FALSE;
int gIdleDetection = TRUE;
int gMaxDataBlocks = DATA_BLOCKS_PER_FILE_DEFAULT;
char *gSN = NULL;

int MainLoop(void);
void SignalHandler(int Signal);
void GetTimeNow(void);
int DisplayVar(int VarID,const char *Label);
pid_t RunSystemCommand(const char *Cmd);
void DecodeFile(char *Filename);
void Usage(void);

int main(int argc, char *argv[])
{
   int Option;
   int Ret = 0;
   struct sigaction Act;
   struct sigaction Old;
   char *DecodeFilename = NULL;


   Act.sa_flags = (SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART);
   Act.sa_handler = SignalHandler;

   if(sigaction(SIGTERM,&Act,&Old) || sigaction(SIGSEGV,&Act,&Old)) {
      LOG("%s: sigaction failed - %s\n",__FUNCTION__,strerror(errno));
   }

   while(Ret == 0 && (Option = getopt(argc, argv,OPTION_STRING)) != -1) {
      switch (Option) {
         case 'b':
            if(sscanf(optarg,"%d",&gMaxDataBlocks) != 1 || 
               gMaxDataBlocks < 1 || gMaxDataBlocks > 0xffff)
            {
               printf("Error: invalid data blocks per file, must be 1 to 65535\n");
               Ret = EINVAL;
            }
            break;

         case 'c':
            gIdleDetection = FALSE;
            break;

         case 'd':
            gDebug = 1;
            break;

         case 'D':
            if(DecodeFilename != NULL) {
               free(DecodeFilename);
            }
            DecodeFilename = strdup(optarg);
            break;

         case 'f':   // Display current flow count
            gDisplayFlow = TRUE;
            break;

         case 'm':
            gDisplayMinPressure = TRUE;
            break;

         case 'M':
            gDisplayMaxPressure = TRUE;
            break;

         case 'p':   // Display current pressure
            gDisplayPressure = TRUE;
            break;

         case 's':   // Set device SN
            if(gSN != NULL) {
               free(gSN);
            }
            gSN = strdup(optarg);
            break;

         case 'v':
            gVerbose++;
            break;

         default:
           Ret = EINVAL;
           break;
      }
   }

   do {
      if(Ret != 0) {
         Usage();
         break;
      }

      if(gDebug) {
         printf("echod compiled "__DATE__ " "__TIME__"\n");
      }

      if((Ret = WASP_Init()) != 0) {
         LOG("WASP_Init failed - %s\n",WASP_strerror(Ret));
         break;
      }

      if(gDisplayFlow) {
         if((Ret = DisplayVar(WASP_VAR_FLOW,"Flow counter")) != 0) {
            break;
         }
      }

      if(gDisplayMinPressure) {
         if((Ret = DisplayVar(WASP_VAR_PRESSURE_MIN,"Minimum pressure")) != 0) {
            break;
         }
      }

      if(gDisplayPressure) {
         if((Ret = DisplayVar(WASP_VAR_PRESSURE,"Current pressure")) != 0) {
            break;
         }
      }

      if(gDisplayMaxPressure) {
         if((Ret = DisplayVar(WASP_VAR_PRESSURE_MAX,"Maximum pressure")) != 0) {
            break;
         }
      }

      if(DecodeFilename != NULL) {
         DecodeFile(DecodeFilename);
      }

      if(gSN != NULL) do {
      // Serial number set, enter mainloop
         if(!gDebug) {
            pid_t pid;

            LOG("echod compiled "__DATE__ " "__TIME__" starting\n");
            printf("Becoming a Daemon ...\n");

            // Fork daemon
            if((pid = fork()) < 0) {
               LOG("%s: fork() failed: %s\n",__FUNCTION__,strerror(errno));
               Ret = errno;
               break;
            }
            else if(pid != 0) {
            // parent
               exit(0);
            }

         // must be the child
            if(setsid() == -1) {
               LOG("%s: setsid() failed: %s\n",__FUNCTION__,strerror(errno));
               Ret = errno;
               break;
            }

            if(daemon(1, 0) < 0) {
               LOG("%s: daemon(1,0) failed: %s\n",__FUNCTION__,strerror(errno));
               Ret = errno;
               break;
            }
         } while(FALSE);

         Ret = MainLoop();
      } while(FALSE);
   } while(FALSE);

   WASP_Cleanup();

   if(gSN != NULL) {
      free(gSN);
   }

   if(DecodeFilename != NULL) {
      free(DecodeFilename);
   }

   if(Ret < 0) {
      LOG("Returning EIO for error code %d as exit status.\n",Ret);
      Ret = EIO;  // Catch all for WASP errors
   }

   return Ret;
}

int MainLoop()
{
   int Ret = 0;
   FILE *fp = NULL;
   char *FirmwareVer = NULL;
   pid_t CompressPid = -1;
   pid_t UploadPid = -1;
   int bRunCompress = FALSE;
   int bRunUpload = FALSE;
   int Blocks = 0;
   int ExpectedRecNum = -1;
   FileHeader Hdr;

   memset(&Hdr,0,sizeof(Hdr));
   strncpy(Hdr.Signature,DATAFILE_SIGNATURE,sizeof(Hdr.Signature));
   Hdr.FormatVersion = htons(DATA_FILE_VERSION);
   strncpy(Hdr.SN,gSN,sizeof(Hdr.SN)-1);

   while(Ret == 0) {
   // Make sure our data directory exists
      mkdir(DATA_DIR,0777);

      for( ;  ; ) {
         if((Ret = WASP_GetFWVersionNumber(&FirmwareVer)) == 0 ||
            Ret != WASP_ERR_NOT_READY) 
         {
         // only retry not ready errors
            break;
         }
         sleep(1);   // Give the daemon time to become ready
      }

      if(Ret != 0) {
         break;
      }

      LOG("Frontend processor firmware version: %s\n",FirmwareVer);

      if((Ret = WASP_SetAsyncDataQueueDepth(QUEUE_LEN)) != 0) {
         LOG("WASP_SetAsyncDataQueueDepth failed - %s\n",WASP_strerror(Ret));
         break;
      }

      LOG("Data files will have a maximum of %d data blocks\n",gMaxDataBlocks);

   // Run "forever loop" until error or exit
      while(Ret == 0) {
         WaspVariable Var;
         EchoData *pData;
         int i;
         u16 RecNum;

         if(CompressPid >= 0) {
            int ExitStatus;

         // Check to see if the compress script has finished
            if(waitpid(CompressPid,&ExitStatus,WNOHANG)) {
               LOG("Compress script exited (%d)\n",WEXITSTATUS(ExitStatus));
               CompressPid = -1;
               bRunUpload = TRUE;   // compress the new files
            }
         }

         if(UploadPid >= 0) {
            int ExitStatus;

         // Check to see if the upload script has finished
            if(waitpid(UploadPid,&ExitStatus,WNOHANG)) {
               LOG("Upload script exited (%d)\n",WEXITSTATUS(ExitStatus));
               UploadPid = -1;
            }
         }

         if(bRunCompress && CompressPid < 0) {
         // Compress file(s) that have just been created
            bRunCompress = FALSE;
            CompressPid = RunSystemCommand(COMPRESS_SCRIPT);
         }

         if(bRunUpload && UploadPid < 0) {
         // Upload files that have just been compressed
            bRunUpload = FALSE;
            UploadPid = RunSystemCommand(UPLOAD_SCRIPT);
         }

         Var.Type = WASP_VARTYPE_BLOB;
         Var.ID = WASP_VAR_ECHO_DATA;
         Var.Val.Blob.Data = NULL;

         if((Ret = WASP_GetVariable(&Var)) != 0) {
            if(Ret == WASP_ERR_NO_DATA) {
            // No data yet, wait for a bit
               Ret = 0;
               usleep(1000);
            }
            else {
               break;
            }
         }
         else do {
            if(Var.Val.Blob.Len != sizeof(EchoData)) {
               LOG("Ignoring data with invalid length (%d != %d)\n",
                   Var.Val.Blob.Len,sizeof(EchoData));
               break;
            }

            if(Var.Type != WASP_VARTYPE_BLOB) {
               LOG("%s#%d: Internal error (%d != %d)\n",__FUNCTION__,__LINE__,
                   Var.Type,WASP_VARTYPE_BLOB);
               break;
            }

            if(Var.ID != WASP_VAR_ECHO_DATA) {
               LOG("%s#%d: Internal error (%d != %d)\n",__FUNCTION__,__LINE__,
                   Var.ID,WASP_VAR_ECHO_DATA);
               break;
            }

            pData = (EchoData *) Var.Val.Blob.Data;

         // The Echo front end board is little endian, convert the 
         // data into Network order

            BYTE_SWAP_SHORT(pData->RecNum);
            BYTE_SWAP_SHORT(pData->FlowCount);

            for(i = 0; i < NUM_PRESSURE_READINGS; i++) {
               BYTE_SWAP_SHORT(pData->Pressure[i]);
            }

            pData->FlowCount = BYTE_SWAP_SHORT(pData->FlowCount);

            RecNum = ntohs(pData->RecNum);

            VDBG("Got data block %d\n",RecNum);
            if(ExpectedRecNum != -1 && RecNum != ExpectedRecNum) {
               LOG("Unexpected record number (%d != %d)\n",
                   RecNum,ExpectedRecNum);
            }
            ExpectedRecNum = ++RecNum;

         // Don't failure to write data to RAM disk a fatal error.  
         // The RAM disk may be full if the Internet connection went down.  
         // If it comes back up the upload script should eventually empty it.

            if(fp == NULL) {

            // Open a temp file for the data
               Blocks = 0;
               GetTimeNow();
               DBG("Opening new data file\n");

               if((fp = fopen(TEMP_FILE,"w")) == NULL) {
                  LOG("fopen of '" TEMP_FILE "' failed - %s\n",strerror(errno));
               }
               else {
                  Hdr.TimeStamp = htonl(gTimeNow.tv_sec);
               // Skip the file header for now
                  if(fseek(fp,sizeof(Hdr),SEEK_SET) != 0) {
                     LOG("fseek failed - %s\n",strerror(errno));
                     fclose(fp);
                     fp = NULL;
                     unlink(TEMP_FILE);
                  }
               }
            }

            if(fp != NULL) {
            // Write the new data block
               if(fwrite(pData,sizeof(EchoData),1,fp) != 1) {
                  LOG("fwrite failed - %s\n",strerror(errno));
                  fclose(fp);
                  fp = NULL;
                  unlink(TEMP_FILE);
               }
               else if(++Blocks >= gMaxDataBlocks) {
               // File is complete, write the file header and close the file
                  char Filename[128];

                  rewind(fp);
               // Write the file header
                  Hdr.NumDataBlocks = htons(Blocks)   ;
                  if(fwrite(&Hdr,sizeof(Hdr),1,fp) != 1) {
                     LOG("fwrite failed - %s\n",strerror(errno));
                     fclose(fp);
                     fp = NULL;
                     unlink(TEMP_FILE);
                  }
                  else {
                     fclose(fp);
                     fp = NULL;

                     bRunCompress = TRUE;
                  // Move the completed file to the compress queue
                  // Filename: <SN>_<TimeStamp as 8 hex digits>
                     snprintf(Filename,sizeof(Filename),DATA_DIR "/%s_%08X",
                              gSN,(unsigned int) gTimeNow.tv_sec);
                     if(rename(TEMP_FILE,Filename) != 0) {
                        LOG("rename('" TEMP_FILE "','%s' failed - %s\n",Filename,
                            strerror(errno));
                     }
                  }
               }
            }
         } while(FALSE);
         WASP_FreeValue(&Var);
      }
   }

   if(fp != NULL) {
      fclose(fp);
   }

   if(FirmwareVer != NULL) {
      free(FirmwareVer);
   }

   return Ret;
}

void SignalHandler(int Signal)
{
   switch(Signal) {
      case SIGTERM:
         LOG("%s: Received signal SIGTERM (%d)\n",__FUNCTION__,Signal);
         break;

      case SIGSEGV:
         LOG("%s: Received signal SIGSEGV (%d)\n",__FUNCTION__,Signal);
         break;

      default:
         LOG("%s: Received unknown signal %d\n",__FUNCTION__,Signal);
         break;
   }
   exit(Signal);
}

void GetTimeNow()
{
   struct timezone tz;
   gettimeofday(&gTimeNow,&tz);
}

int DisplayVar(int VarID,const char *Label)
{
   int Tries;
   int Err;
   WaspVariable Var;
   Var.Type = WASP_VARTYPE_UINT16;
   Var.ID = VarID;
   Var.State = VAR_VALUE_LIVE;

   for(Tries = 0; Tries < 5; Tries++) {
      if((Err = WASP_GetVariable(&Var)) != 0) {
         if(Err != WASP_ERR_NOT_READY) {
         // only retry not ready errors
            break;
         }
         sleep(1);   // Give the daemon time to become ready
      }
      else {
         printf("%s: %u\n",Label,Var.Val.U16);
         break;
      }
   }

   if(Err != 0) {
      LOG("WASP_GetVariable failed - %s\n",WASP_strerror(Err));
   }

   return Err;
}

pid_t RunSystemCommand(const char *Cmd)
{
   pid_t pid;

   do {
      if((pid = fork()) < 0) {
         LOG("%s: fork() failed - %s\n",__FUNCTION__,strerror(errno));
         break;
      }

      if(pid != 0) {
      // parent
         break;
      }
   // must be the child
      exit(WEXITSTATUS(system(Cmd)));
   } while(FALSE);

   return pid;
}


void Usage()
{
   printf("echod compiled "__DATE__ " "__TIME__"\n");
   printf("Usage: echod [options]\n");
   printf("\t  -b<blocks> - set maximum blocks of data per file\n");
   printf("\t  -c - send data continuously (disable idle data detection)\n");
   printf("\t  -d - debug\n");
   printf("\t  -D<file> - Decode Dump specified file\n");
   printf("\t  -f - display current flow counter reading\n");
   printf("\t  -m - display minimum pressure reading\n");
   printf("\t  -M - display maximum pressure reading\n");
   printf("\t  -p - display current pressure reading\n");
   printf("\t  -s - device Serial number (also enables data file generation)\n");
   printf("\t  -v - verbose output\n");
}


