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
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termios.h>
#include <ctype.h>
#include <syslog.h>
#include <linux/serial.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "defines.h"
#include "insight.h"
#define  INSIGHTD_PRIVATE
#include "insightd.h"

#define LOG(format, ...) \
   if(gDebug) \
      printf(format, ## __VA_ARGS__); \
   else \
      syslog(LOG_ERR,format, ## __VA_ARGS__)

#define OPTION_STRING      "1aA:b:cCdD:em:M:r:s:tTvw:"

// Define this if the hardware support sending data to the Maxim chip
// as well as receiving it.  The Insight hardware design doesn't support
// sending data to the chip to save cost
//#define TX_DATA_SUPPORTED   1

#define ASCII_TAB 0x09
#define ASCII_CR  0x0d
#define ASCII_LF  0x0a
#define NO_VAL 0xffffffff

#define SSI_ACK_DATA    0xaa
#define SSI_AUTO_RPT    0xae
#define SSI_ACK         0xad
#define SSI_NAK         0xb0
#define SSI_BC          0xbc
#define SSI_CHKSUM_ERR  0xbd
#define SSI_BUF_OVFL    0xbf

#define SSI_CMD_SEL_DEV 0xc0  // plus device in lower nibble
#define SSI_CMD_SET_ADR 0xa3
#define SSI_CMD_READ_3  0xe3
#define SSI_CMD_READ_N  0xe0
#define SSI_CMD_WRITE_3 0xd3

// Calibration values
#define SSI_ADR_S1_GAIN 0xc
#define SSI_ADR_S0_GAIN 0xd
#define SSI_ADR_S3_GAIN 0xe
#define SSI_ADR_S2_GAIN 0xf

#define SSI_ADR_S1_OFFS 0x10
#define SSI_ADR_S0_OFFS 0x11
#define SSI_ADR_S3_OFFS 0x12
#define SSI_ADR_S2_OFFS 0x13

#define SSI_ADR_T_GAIN  0x14
#define SSI_ADR_T_OFFS  0x15

#define SSI_MAX_ADR     0x168

#define NUM_VALUES   5
int g_bAutoReportingEnabled = TRUE;

// Number of seconds since program start.
// It might wrap, but it sure as hell won't go backwards until it does.
// All timers should be based on this, not the absolute time of day which
// may jump when the time of day is updated from the Internet.
time_t UpTime = 0;
time_t NextTimeOut = 0;

// Undefine the following to save some code space.
// To actually simulate data the -D switch (device path) must NOT be given
// on startup.
#define SIMULATE    
#ifdef SIMULATE    
char * buffer;
char * pch;
char * prev;
char  next[10];
int arrayindex=0;
#endif

u8 TxBuf[256];
u8 SsiMsg[256];
int gVerbose = 0;
int gDebug = 0;
int gIpcSocket = -1;
int gAverageSeconds = 10;

DataValues gCurrent;
DataValues gAverage;
InsightdMsg gReply;
InsightVars gInsightValues;

u32 gAccumvRMS;
u32 gAccumiRMS;
u32 gAccumWatts;
u32 gAccumPF;
u32 gAccumFreq;
u32 gAccumIntTemp;
u32 gAccumExtTemp;
int ValuesAveraged;
u32 gRegisterAdr;
u32 gRegisterData;

u32 gWattMinuteTotal;
u32 gAccumWattMinute;
u32 gTotalMinutes;
int WattMinuteAveraged;

time_t AverageTimer;
time_t WattMinuteTimer;

char *DevicePath = NULL;
int gRunMainLoop = TRUE;
int gSerialFd = -1;
AdrUnion gHisAdr;
int gResponseCmd = 0;
int gFractionBits = 0;
char *gSaveFilePath = NULL;
char *gLoadFilePath = NULL;
int g_bAutoRptEnabled = FALSE;
int g_bSanityCheck = FALSE;
float gMinimumSanePower = -1.0;
float gMaximumSanePower = -1.0;
u32 gChecksumErrors = 0;
u32 gGoodFrames = 0;
int gBaudrate = 9600;
int gBaudrateConstant = B9600;

const char *CalRegisterNames[] = {
   "S1_GAIN",
   "S0_GAIN",
   "S3_GAIN",
   "S2_GAIN",
   "S1_OFFS",
   "S0_OFFS",
   "S3_OFFS",
   "S2_OFFS",
   "T_GAIN ",
   "T_OFFS "
};

struct AutoRptParse {

};

void Usage(void);
int MainLoop(void);
int sio_async_init(char *Device,int BaudrateConstant);
int ProcessRxData(int fd);
int UdpInit(u16 Port);
void IpcRead(int fd);
int UdpSend(int fd,AdrUnion *pToAdr,InsightdMsg *pMsg,int MsgLen);
int IPCTestDebug(int Type);
int IPCTest(int Type);
void PrintValues(DataValues *p);
void PrintTotals(TotalData *p);
int SendSiiCmd(u8 Cmd,u8 *Data,int DataLen);
int ReadRegister(u32 Adr);
int WriteRegister(u32 Adr,u32 Data);
int ReadCalRegisters(void);
int SelectTarget(u8 Device);
int AutoRpt(int bEnable);
void ProcessAutoData(u8 *Msg);
void SanityCheck(u8 *Msg);
void DumpHex(void *data, int len);

void Usage()
{
   printf("insightd $Revision: 1.2 $ compiled "__DATE__ " "__TIME__"\n");
   printf("Usage: insightd [options]\n");
   printf("\t  -1 - Don't start auto reporting.\n");
   printf("\t  -a - Get and display average reading from deamon\n");
   printf("\t  -b <baud rate> - set baud rate\n");
   printf("\t  -A <secs> - Set averaging time\n");
   printf("\t  -c - Get and display current reading from deamon\n");
   printf("\t  -C - Get and display 78M6610 calibration values\n");
   printf("\t  -d - debug\n");
   printf("\t  -D - set serial device path, i.e. /dev/ttyS0\n");
#ifdef SIMULATE    
   printf("\t       (data is simulated if the device path isn't specified)\n");
#endif
   printf("\t  -i <path> - Load Initialization register values.\n");
   printf("\t  -e - Send command to daemon to exit.\n");
   printf("\t  -r <reg>  - Read 78M6610 register value and exit.\n");
   printf("\t  -m <watts> - Set minimum wattage level for insanity logging\n");
   printf("\t  -M <watts> - Set Maximum wattage level for insanity logging\n");
   printf("\t  -s <frac> - Set fraction bits register for display.\n");
   printf("\t  -S <path> - Save Initialization register values.\n");
   printf("\t  -t - Get and display totals from deamon\n");
   printf("\t  -T - Get, display and clear totals from deamon\n");
   printf("\t  -v - verbose output\n");
   printf("\t  -w <reg>:<value> - Write 78M6610 register with value and exit.\n");
}


#define SUCCESS 0
#define FAILURE -1
int main(int argc, char *argv[])
{
   int Option;
   int Ret = 0;
   int bReadCurrent = FALSE;
   int bReadAverage = FALSE;
   int bReadTotal = FALSE;
   int bClearTotals = FALSE;
   int bSendQuit = FALSE;
   int bTestCommand = FALSE;
   int bReadCalValues = FALSE;
   int RegCmd = 0;

   while ((Option = getopt(argc, argv,OPTION_STRING)) != -1) {
      switch (Option) {
         case '1':
            g_bAutoReportingEnabled = FALSE;
            break;

         case 'a':   // Read current Values
            bReadAverage = TRUE;
            bTestCommand = TRUE;
            break;

         case 'A':
            if(sscanf(optarg,"%d",&gAverageSeconds) != 1 ||
               gAverageSeconds < 0)
            {
               Ret = EINVAL;
               printf("Error: invalid argument '%s'\n",optarg);
               Usage();
            }
            else {
               bTestCommand = TRUE;
               printf("Averaging time set to %d seconds\n",gAverageSeconds);
            }
            break;

         case 'b': {
            if(sscanf(optarg,"%d",&gBaudrate) != 1)  {
               Ret = EINVAL;
            }
            else switch(gBaudrate) {
               case 1200:
                  gBaudrateConstant = B1200;
                  break;

               case 2400:
                  gBaudrateConstant = B2400;
                  break;

               case 4800:
                  gBaudrateConstant = B4800;
                  break;

               case 9600:
                  gBaudrateConstant = B9600;
                  break;

               case 19200:
                  gBaudrateConstant = B19200;
                  break;

               case 38400:
                  gBaudrateConstant = B38400;
                  break;

              default:
                 Ret = EINVAL;
                 break;
            }

            if(Ret != 0) {
               printf("Error: invalid or unsupported baudrate '%s'\n",optarg);
               Usage();
            }
            break;
         }

         case 'c':   // Read current Values
            bTestCommand = TRUE;
            bReadCurrent = TRUE;
            break;

         case 'C':
            bTestCommand = TRUE;
            bReadCalValues = TRUE;
            break;

         case 'd':
            gDebug = 1;
            break;

         case 'D':
            if(DevicePath != NULL) {
               free(DevicePath);
            }
            DevicePath = strdup(optarg);
            break;
         
         case 'e':
            bSendQuit = TRUE;
            bTestCommand = TRUE;
            break;

         case 'i':
            bTestCommand = TRUE;
            if(gLoadFilePath != NULL) {
               free(gLoadFilePath);
            }
            gLoadFilePath = strdup(optarg);
            break;

         case 'm': {
            float x;
            if(sscanf(optarg,"%f",&x) != 1 || x <= 0.0)  {
               Ret = EINVAL;
               printf("Error: invalid argument '%s'\n",optarg);
               Usage();
            }
            else {
               g_bSanityCheck = TRUE;
               if(Option == 'm') {
                  gMinimumSanePower = x;
               }
               else {
                  gMaximumSanePower = x;
               }
            }
            break;
         }

         case 'M':
            g_bSanityCheck = TRUE;
            break;


         case 'r':
            if(sscanf(optarg,"%x",&gRegisterAdr) != 1 ||
               gRegisterAdr > SSI_MAX_ADR)
            {
               Ret = EINVAL;
               printf("Error: invalid argument '%s'\n",optarg);
               Usage();
            }
            else {
               bTestCommand = TRUE;
               RegCmd = CMD_READ_REG;
            }
            break;

         case 's':
            if(sscanf(optarg,"%d",&gFractionBits) != 1 ||
               gFractionBits < 10 || gFractionBits > 23)
            {
               Ret = EINVAL;
               printf("Error: invalid argument '%s'\n",optarg);
               Usage();
            }
            break;

         case 'S':
            bTestCommand = TRUE;
            if(gSaveFilePath != NULL) {
               free(gSaveFilePath);
            }
            gSaveFilePath = strdup(optarg);
            break;

         case 'T':
            bClearTotals = TRUE;
         case 't':
            bTestCommand = TRUE;
            bReadTotal = TRUE;
            break;

         case 'v':
            gVerbose++;
            break;

         case 'w':
            if(sscanf(optarg,"%x:%x",&gRegisterAdr,&gRegisterData) != 2 ||
               gRegisterAdr > SSI_MAX_ADR || gRegisterData > 0xffffff)
            {
               Ret = EINVAL;
               printf("Error: invalid argument '%s'\n",optarg);
               Usage();
            }
            else {
               bTestCommand = TRUE;
               RegCmd = CMD_WRITE_REG;
            }
            break;

         default:
           Usage();
           break;
      }
   }

   if(Ret == 0) do {
      if(gDebug) {
         printf("insightd $Revision: 1.2 $ compiled "__DATE__ " "__TIME__"\n");
      }

      if(bTestCommand) {
         if(RegCmd != 0) {
            if((Ret = IPCTest(RegCmd)) != 0) {
               break;
            }
         }

         if(bReadCurrent) {
            if((Ret = IPCTest(CMD_GET_CURRENT_DATA)) != 0) {
               break;
            }
         }

         if(bReadAverage) {
            if((Ret = IPCTest(CMD_GET_AVERAGE_DATA)) != 0) {
               break;
            }
         }

         if(bReadTotal) {
            Ret = IPCTest(bClearTotals ? CMD_GET_TOTAL_CLR : CMD_GET_TOTAL);
            if(Ret != 0) {
               break;
            }
         }

         if(bReadCalValues) {
            if((Ret = IPCTest(CMD_READ_CAL_REGS)) != 0) {
               break;
            }
         }

         if(bSendQuit) {
            Ret = IPCTest(CMD_EXIT);
         }
      }
      else {
         openlog("insightd",0,LOG_DAEMON);
         Ret = MainLoop();
      }
   } while(FALSE);

   if(gSaveFilePath != NULL) {
      free(gSaveFilePath);
   }

   if(gLoadFilePath != NULL) {
      free(gLoadFilePath);
   }
   return Ret;
}

int MainLoop()
{
   int Ret = 0;
   int Err;
   time_t TimeNow;
   time_t LastTime = 0;
   time_t DeltaT;
#ifdef SIMULATE 
    FILE * pFile;
    long lSize;
    size_t result;

    if(DevicePath == NULL) {
       pFile = fopen ("/tmp/Belkin_settings/InstantPower.txt" , "r");
      if (pFile==NULL) {
        fputs("!!!!!!!!!! File InstantPower.txt Does not exists !!!!!!!!!!!!",stderr );
        system("cp -f /sbin/InstantPower.txt /tmp/Belkin_settings/");
        pFile = fopen ("/tmp/Belkin_settings/InstantPower.txt" , "r");
        if (pFile==NULL) {
           fputs("@@@@@@@@@@ File InstantPower.txt is not in /sbin !!!!!!!!!!!!",stderr );
           exit(1);
        }
        else{
            fputs("-------->>>>> File InstantPower.txt Copied",stderr );
        }
      }

      // obtain file size:
      fseek (pFile , 0 , SEEK_END);
      lSize = ftell (pFile);
      rewind (pFile);

      // allocate memory to contain the whole file:
      buffer = (char*) malloc (sizeof(char)*lSize);
      if (buffer == NULL) {fputs ("Memory error",stderr); exit (2);}

      // copy the file into the buffer:
      result = fread (buffer,1,lSize,pFile);
      if (result != lSize) {fputs ("Reading error",stderr); exit (3);}

      /* the whole file is now loaded in the memory buffer. */

      // terminate
      fclose (pFile);
      prev = buffer;
      pch=strchr(prev,',');
      arrayindex = pch-prev;
    }
#endif

   do {
      if(DevicePath == NULL) {
         LOG("%s: Generating SIMULATED data\n",__FUNCTION__);
      }
      else {
         if((gSerialFd = sio_async_init(DevicePath,gBaudrateConstant)) < 0) {
            LOG("%s: sio_async_init() failed\n",__FUNCTION__);
            Ret = -gSerialFd;
            break;
         }
         else if(gVerbose) {
            printf("\"%s\" opened successfully @ %d baud\n",
                   DevicePath,gBaudrate);
         }
      }

      if(((gIpcSocket = UdpInit(INSIGHTD_PORT))) < 0) {
         LOG("%s: UdpInit() failed (%d)\n",__FUNCTION__,gIpcSocket);
      }
   } while(FALSE);

   if(Ret == 0 && !gDebug) do {
      pid_t pid;

      printf("Becoming a Daemon ...\n");

      // Fork daemon
      if((pid = fork()) < 0) {
         perror("Forking");
         Ret = errno;
         break;
      }
      else if(pid != 0) {
      // parent
         exit(0);
         break;
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
      }
   } while(FALSE);


   if(Ret == 0) {
      LOG("insightd compiled "__DATE__ " "__TIME__" starting\n");
      SelectTarget(2);
   }
      int retVal=0;
   
// Run "forever loop" until error or exit
   while(gRunMainLoop && Ret == 0) {
      fd_set ReadFdSet;
      struct timeval Timeout;
      int MaxFd = gIpcSocket;

      FD_ZERO(&ReadFdSet);
      FD_SET(gIpcSocket,&ReadFdSet);

      if(gSerialFd != -1) {
         FD_SET(gSerialFd,&ReadFdSet);
         if(gSerialFd > MaxFd) {
            MaxFd = gSerialFd;
         }
      }

      Timeout.tv_sec = 0;
      Timeout.tv_usec = 500000;  // 1/2 second
      if((Err = select(MaxFd+1,&ReadFdSet,NULL,NULL,&Timeout)) < 0) {
         LOG("%s: select failed - %s\n",__FUNCTION__,strerror(errno));
         Ret = errno;
         break;
      }

      time(&TimeNow);
      if(LastTime != 0) {
         DeltaT = TimeNow - LastTime;
      }
      else {
         DeltaT = 0;
      }
      LastTime = TimeNow;

      if(DeltaT > 3 || DeltaT < 0) {
      // We should wake up more than once a second.  If the time delta
      // greater than 3 seconds or less than zero seconds assume that what has
      // really happened is that we've connected to the Internet and run
      // ntpdate which has caused a time warp.  Ignore this time interval
         LOG("%s: time jump of %d seconds detected, UpTime: %u.\n",
              __FUNCTION__,(int) DeltaT,(unsigned int) UpTime);
         DeltaT = 0;
      }
      UpTime += DeltaT;

      if(gSerialFd != -1 && FD_ISSET(gSerialFd,&ReadFdSet)){
         if((Err = ProcessRxData(gSerialFd)) > 0) {
            if(SsiMsg[0] == SSI_ACK_DATA) {
            // Data we requested
               int Expected = 0;
               switch(gResponseCmd) {
               case CMD_REG_DATA:
               // Expecting 3 bytes of register data (6 bytes total)
                  if(SsiMsg[1] == 6) {
                  // Send response
                     memcpy(&gReply.u.RegData.Data,&SsiMsg[2],3);
                     UdpSend(gIpcSocket,&gHisAdr,&gReply,
                             offsetof(InsightdMsg,u)+sizeof(gReply.u.RegData));
                  }
                  else {
                     Expected = 6;
                     LOG("%s: Received %d byte response, was expecting 6.\n",
                         __FUNCTION__,SsiMsg[1]);
                  }
                  break;

               case CMD_CAL_REGS:
               // Expecting 30 bytes of register data (33 bytes total)
                  if(SsiMsg[1] == 33) {
                     // Send response
                     int i;
                     u32 *p32 = gReply.u.CalRegValues;
                     u8 *p8 = &SsiMsg[2];

                     for(i = 0; i < NUM_CAL_REGISTERS; i++) {
                        *p32 = 0;
                        memcpy(p32++,p8,3);
                        p8 += 3;
                     }
                     UdpSend(gIpcSocket,&gHisAdr,&gReply,
                             offsetof(InsightdMsg,u) + 
                             sizeof(gReply.u.CalRegValues));
                  }
                  else {
                     Expected = 6;
                     LOG("%s: Received %d byte response, was expecting 33.\n",
                         __FUNCTION__,SsiMsg[1]);
                  }
                  break;

               default:
                  LOG("%s: Ignoring unexpected %d byte response.\n",
                      __FUNCTION__,SsiMsg[1]);
               }
               gResponseCmd = 0;

               if(Expected != 0) {
                  LOG("%s: Error: received %d byte response, was expecting %d.\n",
                      __FUNCTION__,SsiMsg[1],Expected);
               }
            }
            else if(SsiMsg[0] == SSI_AUTO_RPT) {
            // Auto reported data
               ProcessAutoData(SsiMsg);
            }
         }

         else if(Err < 0) {
            Ret = -Err;
         }
      }

      if(FD_ISSET(gIpcSocket,&ReadFdSet)) {
         IpcRead(gIpcSocket);
      }
   }

   if(gSerialFd >= 0) {
      close(gSerialFd);
   }

   if(gIpcSocket >= 0) {
      close(gIpcSocket);
   }

   if(DevicePath != NULL) {
      free(DevicePath);
   }

#ifdef SIMULATE
free(buffer);
#endif
   return Ret;
}


#define SERIAL_OPEN_FLAGS O_FSYNC | O_APPEND | O_RDWR | O_NOCTTY | O_NDELAY
// return fd or <0 if on error
int sio_async_init(char *Device,int BaudrateConstant)
{
   int fd = -1;
   int Err = -1;  // assume the worse
  struct termios options;
  struct serial_struct ser;

  do {
     // open the port, set the stuff
     if((fd = open(Device,SERIAL_OPEN_FLAGS)) < 0) {
        printf("Couldn't open \"%s\": %s\n",Device,strerror(errno));
        Err = -errno;
        break;
     }

     if(tcgetattr(fd,&options) < 0) {
       printf("Unable to read port configuration: %s\n",strerror(errno));
       Err = -errno;
       break;
     }

     options.c_oflag &= ~OPOST;
     options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  // raw mode
     options.c_iflag &= ~(IXON | IXOFF | IXANY);  /* disable software flow control */
     options.c_iflag |=  IGNBRK;            /* ignore break */
     options.c_iflag &= ~ISTRIP;            /* do not strip high bit */
     options.c_iflag &= ~(INLCR | ICRNL);   /* do not modify CR/NL   */
     options.c_cflag |=  (CLOCAL | CREAD);  /* enable the receiver and set local mode */
     options.c_cflag &= ~CRTSCTS;           /* disable hardware flow control */

     options.c_cflag &= ~CSIZE;             /* Mask the character size bits */
     options.c_cflag |= CS8;                /* 8 data bits */
     options.c_cflag &= ~PARENB;            /* no parity */
     options.c_cflag &= ~CSTOPB;            /* 1 stop bit */


     if(cfsetspeed(&options,BaudrateConstant) == -1) {
        printf("cfsetspeed() failed: %s\n",strerror(errno));
        Err = -errno;
        break;
     }

     if(tcsetattr(fd,TCSANOW,&options) < 0) {
        printf("tcsetattr() failed: %s\n",strerror(errno));
        Err = -errno;
        break;
     }

     if(ioctl(fd, TIOCGSERIAL, &ser) != 0) {
        printf("ioctl(TIOCGSERIAL) failed: %s\n",strerror(errno));
        Err = -errno;
        break;
     }

     ser.flags &= ~ASYNC_SPD_MASK;
     ser.flags |= ASYNC_SPD_CUST;
     ser.custom_divisor= ser.baud_base / 38400;
     if(ioctl(fd, TIOCSSERIAL, &ser) != 0) {
        printf("ioctl(TIOCSSERIAL) failed: %s\n",strerror(errno));
        Err = -errno;
        break;
     }

     Err = 0;
  } while(FALSE);

  if(Err == 0) {
     Err = fd;
  }
  else if(fd >= 0) {
     close(fd);
  }

  return Err;
}


/*
   Get a message with consisting of:
 
   return:
       1 - new message read
       0 - no new data
      <0 - error
 
   Typical exchanges:
   TX:0xAA 04 C2 90           // select target 2
   RX:0xAD                    // ack
   TX:0xAA 07 A3 03 00 E3 C6  // set register pointer adr to 0x0003 (FW version)
   RX:0xAA 06 04 C7 00 85     // version 0x00c704 (7/4/12)
   TX:0xAA 07 A3 3B 01 E3 8D  // set register pointer adr to 0x013b (TEMPC)
   RX:0xAA 06 EB 77 00 EE     // Tempc 0x0077eb (30699 / 10^10) = 29.9794921875
   TX:0xAA 07 A3 47 01 E3 81  // Set register pointer adr to 0x147 (FREQ)
   RX:0xAA 06 E2 7A 69 8B     // Freq 0x697ae2 (6912738/2^16) = 105.48
 
*/
int ProcessRxData(int fd)
{
   int bFrameFound = FALSE;
   int BytesRead;
   int i;
   int Ret = 0;   // Assume no error, no new data

   typedef enum {
      STATE_GET_START,
      STATE_GET_LEN,
      STATE_GET_DATA,
   } RxState;

   static DataValues TempValues;
   static RxState State = STATE_GET_START;
   static int RxCount = 0;
   static int ByteCount = 4;
   static u8 CheckSum = 0;
   static u8 Msg[256];
   static  int HealthCount=0;
   u8 RxBuf[256];

   do {
      BytesRead = read(fd,RxBuf,sizeof(RxBuf));
      if(BytesRead < 0) {
         if(errno != EAGAIN) {
            Ret = errno;
            LOG("%s: read failed - %s\n",__FUNCTION__,strerror(errno));
         }
         break;
      }

	if(3600 == HealthCount)
	{
	    HealthCount=0;
	    system("date >/dev/console");
	    system("echo --Insight Daemon Health OK!! > /dev/console");
	}
	HealthCount++;
      if(gVerbose > 2) {
         printf("%s: read %d bytes\n",__FUNCTION__,BytesRead);
         DumpHex(RxBuf,BytesRead);
      }

      for(i = 0; i < BytesRead; i++) {
         u8 Byte = RxBuf[i];
         CheckSum += Byte;
         Msg[RxCount++] = Byte;

         switch(State) {
         case STATE_GET_LEN:
            if(Byte > 3) {
               if(gVerbose > 2) {
                  printf("Byte count: %d\n",Byte);
               }
               ByteCount = Byte;
                     State = STATE_GET_DATA;
               break;
            }
         // Intentional fall-thought to STATE_GET_START on bogus Byte count

         case STATE_GET_START:
            RxCount = 0;
#ifdef TX_DATA_SUPPORTED   
            if(Byte == SSI_ACK_DATA || Byte == SSI_AUTO_RPT) {
               CheckSum = Byte;  // reset checksum
               if(gVerbose > 2) {
                  printf("Got Start byte 0x%x\n",Byte);
               }
               RxCount = 1;
               State = STATE_GET_LEN;
            }
            else if(Byte == SSI_ACK) {
               if(gVerbose > 2) {
                  LOG("%s: Received ACK\n",__FUNCTION__);
               }
               if(!g_bAutoRptEnabled) {
                  g_bAutoRptEnabled = TRUE;
                  AutoRpt(g_bAutoReportingEnabled);
               }
            }
            else if(Byte == SSI_NAK) {
               LOG("%s: Received NAK\n",__FUNCTION__);
            }
            else if(Byte == SSI_BC) {
               LOG("%s: Received bad command error\n",__FUNCTION__);
            }
            else if(Byte == SSI_CHKSUM_ERR) {
               LOG("%s: Received checksum error\n",__FUNCTION__);
            }
            else if(Byte == SSI_BUF_OVFL) {
               LOG("%s: Received buffer overflow\n",__FUNCTION__);
            }
#else
            if(Byte == SSI_AUTO_RPT) {
               CheckSum = Byte;  // reset checksum
               if(gVerbose > 2) {
                  printf("Got Start byte 0x%x\n",Byte);
               }
               RxCount = 1;
               State = STATE_GET_LEN;
            }
#endif
            break;

         case STATE_GET_DATA:
            if(RxCount == ByteCount) {
            // We have a complete frame
               if(CheckSum == 0) {
               // and it's good
                  bFrameFound = TRUE;
                  memcpy(SsiMsg,Msg,Msg[1]);
                  gGoodFrames++;
                  if(gVerbose > 2) {
                     printf("Got a good frame:\n");
                     DumpHex(SsiMsg,Msg[1]);
                  }
               }
               else {
                  gChecksumErrors++;
                  LOG("%s: checksum error (0x%x), good: %u, checksum errors: %u\n",
                     __FUNCTION__,CheckSum,gGoodFrames,gChecksumErrors);
                  if(gDebug) {
                     DumpHex(SsiMsg,Msg[1]);
                  }
               }
               RxCount = 0;
               State = STATE_GET_START;
            }
            break;

         default:
            LOG("Invalid state, resetting\n");
            State = STATE_GET_START;
            break;
         }
      }
   } while(FALSE);

   if(bFrameFound && Ret == 0) {
      Ret = bFrameFound;
   }

   return Ret;
}

// Initialize communications on UDP port <port>.  
// Enables messages to be received on the specified port from other
// applications. This function must be called once and only once for
// each port the application wishes to receive message on.
// return == 0: Ok, otherwise error code
int UdpInit(u16 Port)
{
   int Ret = 0;   // assume the best
   AdrUnion MyAdr;
   int On = 1;
   int Socket = -1;

   do {
      if((Socket = socket(AF_INET,SOCK_DGRAM,0)) == -1) {
         Ret = -errno;
         LOG("%s: socket() failed, %s\n",__FUNCTION__,strerror(Ret));
         break;
      }

      if(setsockopt(Socket,SOL_SOCKET,SO_REUSEADDR,(char *) &On,
                    sizeof(On)) == -1) 
      {
         LOG("%s: Warning - setsockopt() failed: %s\n",__FUNCTION__,
                 strerror(errno));
      }

      MyAdr.i.sin_family = AF_INET;
      MyAdr.PORT = htons(Port);
      MyAdr.ADDR = inet_addr("127.0.0.1");   // localhost

      if(bind(Socket,&MyAdr.s,sizeof(MyAdr)) == -1) {
         Ret = -errno;
         LOG("%s: bind() failed for %s:%d %s\n",__FUNCTION__,
                 inet_ntoa(MyAdr.i.sin_addr),Port,strerror(errno));
         break;
      }
   } while(FALSE);

   if(Ret == 0) {
      Ret = Socket;
   }
   else {
      if(Socket != -1) {
         close(Socket);
      }
   }

   return Ret;
}

#ifdef SIMULATE 
void CalculateSimulatedValues()
{
    memset(&next,0,sizeof(next));
    strncpy(next,prev,arrayindex);
    next[arrayindex+1]='\0';
    gCurrent.Watts = atoi(next);
    prev = pch+1;
    pch=strchr(prev,',');
    if(pch != NULL)
    {
        arrayindex = pch-prev;
    }
    else
    {
   prev = buffer;
   pch=strchr(prev,',');
   arrayindex = pch-prev;
    }
   // gCurrent.Watts = gCurrent.Watts + 1;
   // if(gCurrent.Watts > 20)
// gCurrent.Watts=0;

}
#endif

void PrintInsightVariable(InsightVars *Cur)
{
	printf("\n*************  INSIGHT VARIABLES ************\n");
	printf("		 TodayONTime:	%d\n",Cur->TodayONTime);
	printf("		 TodayONTimeTS:	%d\n",Cur->TodayONTimeTS);
	printf("		 TodaySBYTime:	%d\n",Cur->TodaySBYTime);
	printf("		 TodaySBYTimeTS:	%d\n",Cur->TodaySBYTimeTS);
	printf("		 AccumulatedWattMinute:	%d\n",Cur->AccumulatedWattMinute);
	printf("		 TodayKWHON:	%d\n",Cur->TodayKWHON);
	printf("		 TodayKWHSBY:	%d\n",Cur->TodayKWHSBY);
	printf("		 TotalKWHSinceIsInsightUP:  %f\n",Cur->TotalKWHSinceIsInsightUP);
	printf("		 Last30MinKWH:	%d\n",Cur->Last30MinKWH);
	printf("		 ONForTS:	%d\n",Cur->ONForTS);
	printf("		 StateChangeTS:	%d\n",Cur->StateChangeTS);
	printf("		 KWHONFor:	%f\n",Cur->KWHONFor);
	printf("		 ONForCalc:	%f\n",Cur->ONForCalc);
	printf("		 LastTimeStamp:	%ul\n",Cur->LastTimeStamp);
	printf("		 LastRuntime:	%d\n",Cur->LastRuntime);
	printf("		 LastKWHconsumed:	%f\n",Cur->LastKWHconsumed);
	printf("		 LastDeviceState:	%d\n",Cur->LastDeviceState);
	printf("		 ONFor:	    	%d\n",Cur->ONFor);
	printf("		 LastFlashWrite: 	%d\n",Cur->LastFlashWrite);
	printf("		 LastRelayState: 	%d\n",Cur->LastRelayState);
	printf("\n*********************************************\n");
    
}

void setInsightVariables(InsightVars *CurVal)
{
 gInsightValues.TodayONTime   = CurVal->TodayONTime;   
 gInsightValues.TodayONTimeTS = CurVal->TodayONTimeTS;   
 gInsightValues.TodaySBYTime = CurVal->TodaySBYTime;   
 gInsightValues.TodaySBYTimeTS = CurVal->TodaySBYTimeTS;   
 gInsightValues.AccumulatedWattMinute = CurVal->AccumulatedWattMinute;   
 gInsightValues.TodayKWHON = CurVal->TodayKWHON;   
 gInsightValues.TodayKWHSBY = CurVal->TodayKWHSBY;   
 gInsightValues.TotalKWHSinceIsInsightUP = CurVal->TotalKWHSinceIsInsightUP;   
 gInsightValues.Last30MinKWH = CurVal->Last30MinKWH;   
 gInsightValues.ONForTS = CurVal->ONForTS;   
 gInsightValues.StateChangeTS = CurVal->StateChangeTS;   
 gInsightValues.KWHONFor = CurVal->KWHONFor;   
 gInsightValues.ONForCalc = CurVal->ONForCalc;   
 gInsightValues.LastTimeStamp = CurVal->LastTimeStamp;   
 gInsightValues.LastRuntime = CurVal->LastRuntime;   
 gInsightValues.LastKWHconsumed = CurVal->LastKWHconsumed;   
 gInsightValues.LastDeviceState = CurVal->LastDeviceState;   
 gInsightValues.ONFor = CurVal->ONFor;   
 gInsightValues.LastFlashWrite = CurVal->LastFlashWrite;   
 gInsightValues.LastRelayState = CurVal->LastRelayState;   
}

int ParseRestoreVars(char *RestoreValue)
{
    
    int     varListLoop     = 0x00;
    char    delims[]        = "|";
    char    *result         = 0x00;
    char    szValues[SIZE_20B][SIZE_20B];
	 char *strtok_r_temp;
    
    result = strtok_r(RestoreValue, delims,&strtok_r_temp);
    while((result != NULL))
    {
        memset(szValues[varListLoop], 0x00, SIZE_20B);
        strcpy(szValues[varListLoop], result);
        varListLoop++;
        result = strtok_r(NULL, delims,&strtok_r_temp);
    }
    if(varListLoop == 20){
	gInsightValues.LastRelayState = atoi(szValues[0]);   
	gInsightValues.TodayONTime   = atoi(szValues[1]);   
	gInsightValues.TodayONTimeTS = atoi(szValues[2]);  
	gInsightValues.TodaySBYTime = atoi(szValues[3]);   
	gInsightValues.TodaySBYTimeTS = atoi(szValues[4]);   
	gInsightValues.AccumulatedWattMinute = atoi(szValues[5]);   
	gInsightValues.TodayKWHON = atoi(szValues[6]);   
	gInsightValues.TodayKWHSBY = atoi(szValues[7]);   
	gInsightValues.TotalKWHSinceIsInsightUP = atof(szValues[8]);   
	gInsightValues.Last30MinKWH = atoi(szValues[9]);   
	gInsightValues.ONForTS = atoi(szValues[10]);   
	gInsightValues.StateChangeTS = atoi(szValues[11]);   
	gInsightValues.KWHONFor = atof(szValues[12]);   
	gInsightValues.ONForCalc = atof(szValues[13]);   
	gInsightValues.LastTimeStamp = strtoul(szValues[14],NULL,0);   
	gInsightValues.LastRuntime = atoi(szValues[15]);   
	gInsightValues.LastKWHconsumed = atof(szValues[16]);   
	gInsightValues.LastDeviceState = atoi(szValues[17]);   
	gInsightValues.ONFor = atoi(szValues[18]);   
	gInsightValues.LastFlashWrite = strtoul(szValues[19],NULL,0);   
    }
    else{
	return 1;
    }
    return 0;
}

void IpcRead(int fd)
{
   InsightdMsg Request;
   int BytesRead;
   socklen_t Len = sizeof(gHisAdr);
   int DataLen = -1;

   memset(&gReply,0,sizeof(gReply));
#ifdef SIMULATE 
   if(DevicePath == NULL) {
      CalculateSimulatedValues();
   }
#endif
   BytesRead = recvfrom(fd,&Request,sizeof(Request),0,&gHisAdr.s,&Len);
   if(BytesRead <= 0) {
      LOG("%s: read failed: %d (%s)\n",__FUNCTION__,errno,strerror(errno));
   }
   else {
   // We read something
      if(gVerbose) {
         printf("%s: Got request:\n",__FUNCTION__);
         DumpHex(&Request,BytesRead);
      }
      gReply.Hdr.Cmd = Request.Hdr.Cmd | CMD_RESPONSE;
      switch(Request.Hdr.Cmd) {
         case CMD_GET_CURRENT_DATA:
            gReply.u.Values = gCurrent;
            DataLen = sizeof(gReply.u.Values);
            if(gVerbose > 1) {
               printf("%s: Sending current values:\n",__FUNCTION__);
               PrintValues(&gReply.u.Values);
            }
            break;

         case CMD_GET_AVERAGE_DATA:
            gReply.u.Values = gAverage;
            DataLen = sizeof(gReply.u.Values);
            if(gVerbose > 1) {
               printf("%s: Sending averaged values:\n",__FUNCTION__);
               PrintValues(&gReply.u.Values);
            }
            break;

         case CMD_GET_TOTAL_CLR:
         case CMD_GET_TOTAL:
            DataLen = sizeof(gReply.u.Totals);
         // convert from milliwatt minutes to watt minutes
            gReply.u.Totals.WattMinutes = gWattMinuteTotal / 1000;
            gReply.u.Totals.TotalMinutes = gTotalMinutes;
            gReply.u.Totals.RunTimeSeconds = (u32) UpTime;

            if(gVerbose > 1) {
               printf("%s: Sending total values:\n",__FUNCTION__);
               PrintTotals(&gReply.u.Totals);
            }

            if(Request.Hdr.Cmd == CMD_GET_TOTAL_CLR) {
               WattMinuteTimer = UpTime + 60;
               gWattMinuteTotal = 0;
               gTotalMinutes = 0;
               gAccumWattMinute = 0;
               WattMinuteAveraged = 0;
            }
            break;

         case CMD_EXIT:
            gRunMainLoop = FALSE;
            DataLen = 0;
            LOG("%s: Received CMD_EXIT, exiting...\n",__FUNCTION__);
            break;

         case CMD_READ_REG:
            gReply.u.RegData.Adr = Request.u.RegData.Adr;
            ReadRegister(Request.u.RegData.Adr);
            break;

         case CMD_WRITE_REG:
            WriteRegister(Request.u.RegData.Adr,Request.u.RegData.Data);
            break;

         case CMD_READ_CAL_REGS:
            ReadCalRegisters();
            break;
         
	case CMD_GET_INSIGHTVAR:
            gReply.u.InsightValues = gInsightValues;
            DataLen = sizeof(gReply.u.InsightValues);
	    if(gDebug){
		printf("\n*************  GET INSIGHT VARIABLES ************\n");
		PrintInsightVariable(&gReply.u.InsightValues);
	    }
            break;
         
	case CMD_SET_INSIGHTVAR:
	    if(gDebug){
		printf("\n******  SET INSIGHT VARIABLES\n");
		PrintInsightVariable(&Request.u.InsightValues);
	    }
	    setInsightVariables(&Request.u.InsightValues);
            break;

         default:
            LOG("%s: Invalid request 0x%x ignored.\n",__FUNCTION__,
                Request.Hdr.Cmd);
            break;
      }

      if(DataLen >= 0) {
         DataLen += offsetof(InsightdMsg,u);
         UdpSend(fd,&gHisAdr,&gReply,sizeof(gReply.Hdr)+DataLen);
      }
   }
}

int UdpSend(int fd,AdrUnion *pToAdr,InsightdMsg *pMsg,int MsgLen)
{
   int Ret = 0;   // assume the best
   int BytesSent;

   if(gVerbose > 1) {
      printf("%s: Sending %d bytes:\n",__FUNCTION__,MsgLen);
      DumpHex(pMsg,MsgLen);
   }
   BytesSent = sendto(fd,pMsg,MsgLen,0,&pToAdr->s,sizeof(pToAdr->s));
   if(BytesSent != MsgLen) {
      LOG("%s: sendto failed - %s\n",__FUNCTION__,strerror(errno));
      Ret = -errno;
   }

   return Ret;
}


int IPCTestDebug(int Type)
{
   int Ret = 0;
   DataValues Values;
   TotalData Power;
   RegisterData Reg;
   int bHalInitialized = FALSE;

   do {
      if((Ret = HAL_Init()) != 0) {
         LOG("%s: HAL_Init() failed, %s\n",__FUNCTION__,strerror(Ret));
         break;
      }
      bHalInitialized = TRUE;

      switch(Type) {
         case CMD_GET_CURRENT_DATA:
            if((Ret = HAL_GetCurrentReadings(&Values)) != 0) {
               LOG("%s: HAL_GetCurrentReadings() failed, %s\n",__FUNCTION__,
                   strerror(Ret));
               break;
            }
            printf("\nCurrent values:\n");
            PrintValues(&Values);
            break;

         case CMD_GET_AVERAGE_DATA:
            if((Ret = HAL_GetAverageReadings(&Values)) != 0) {
               LOG("%s: HAL_GetAverageReadings() failed, %s\n",__FUNCTION__,
                   strerror(Ret));
               break;
            }
            printf("\nAverage values:\n");
            PrintValues(&Values);
            break;

         case CMD_GET_TOTAL:
            if((Ret = HAL_GetTotalPower(&Power)) != 0) {
               LOG("%s: HAL_GetTotalPower() failed, %s\n",__FUNCTION__,
                   strerror(Ret));
               break;
            }
            printf("\n");
            PrintTotals(&Power);
            break;

         case CMD_GET_TOTAL_CLR:
            if((Ret = HAL_GetAndClearTotalPower(&Power)) != 0) {
               LOG("%s: HAL_GetAndClearTotalPower() failed, %s\n",__FUNCTION__,
                   strerror(Ret));
               break;
            }
            printf("\n");
            PrintTotals(&Power);
            break;

         case CMD_READ_REG:
            Reg.Adr = gRegisterAdr;
            if((Ret = _HalGetRegister(&Reg)) != 0) {
               LOG("%s: _HalGetRegister() failed, %s\n",__FUNCTION__,
                   strerror(Ret));
            }
            else {
               if(gFractionBits == 0) {
                  printf("0x%x: %d (0x%x)\n",Reg.Adr,Reg.Data,Reg.Data);
               }
               else {
                  float x;
                  x = (int) Reg.Data;
                  x /= powf(2,gFractionBits);
                  printf("0x%x: %f (0x%x)\n",Reg.Adr,x,Reg.Data);
               }
            }
            break;

         case CMD_WRITE_REG:
            Reg.Adr = gRegisterAdr;
            Reg.Data = gRegisterData;
            if((Ret = _HalSetRegister(&Reg)) != 0) {
               LOG("%s: _HalSetRegister() failed, %s\n",__FUNCTION__,
                   strerror(Ret));
            }
            else {
            }
            break;

      case CMD_READ_CAL_REGS: {
         u32 CalRegValues[NUM_CAL_REGISTERS];
         if((Ret = _HalReadCalRegisters(CalRegValues)) != 0) {
            LOG("%s: _HalReadCalRegisters() failed, %s\n",__FUNCTION__,
                strerror(Ret));
         }
         else {
            int i;
            for(i = 0; i < NUM_CAL_REGISTERS; i++) {
               printf("%s (0x%02x):%d\t(0x%x)\n",
                      CalRegisterNames[i],i+SSI_ADR_S1_GAIN,
                      CalRegValues[i],CalRegValues[i]);
            }
         }
         break;
      }

         case CMD_EXIT:
            if((Ret = HAL_Shutdown()) != 0) {
               LOG("%s: HAL_Shutdown() failed, %s\n",__FUNCTION__,
                   strerror(Ret));
               break;
            }
            printf("\nDaemon Exiting\n");
            break;

         default:
            LOG("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
            break;
      }
   } while(FALSE);

   if(bHalInitialized && (Ret = HAL_Cleanup()) != 0) {
      LOG("%s: HAL_Cleanup() failed, %s\n",__FUNCTION__,
          strerror(Ret));
   }

   return Ret;
}

int IPCTest(int Type)
{
   int Ret = 0;
   DataValues Values;
   TotalData Power;
   RegisterData Reg;
   int bHalInitialized = FALSE;

   if(gDebug) {
      return IPCTestDebug(Type);
   }

   do {
      if((Ret = HAL_Init()) != 0) {
         printf("failed\n");
         break;
      }
      bHalInitialized = TRUE;

      switch(Type) {
         case CMD_GET_CURRENT_DATA:
            if((Ret = HAL_GetCurrentReadings(&Values)) != 0) {
               printf("failed\n");
               break;
            }
            PrintValues(&Values);
            break;

         case CMD_GET_AVERAGE_DATA:
            if((Ret = HAL_GetAverageReadings(&Values)) != 0) {
               printf("failed\n");
               break;
            }
            PrintValues(&Values);
            break;

         case CMD_GET_TOTAL:
            if((Ret = HAL_GetTotalPower(&Power)) != 0) {
               printf("failed\n");
               break;
            }
            PrintTotals(&Power);
            break;

         case CMD_GET_TOTAL_CLR:
            if((Ret = HAL_GetAndClearTotalPower(&Power)) != 0) {
               printf("failed\n");
               break;
            }
            printf("cleared\n");
            break;

         case CMD_EXIT:
            if((Ret = HAL_Shutdown()) != 0) {
               printf("failed\n");
               break;
            }
            break;

         case CMD_READ_REG:
            Reg.Adr = gRegisterAdr;
            if((Ret = _HalGetRegister(&Reg)) != 0) {
               printf("failed\n");
            }
            else if(gFractionBits == 0) {
               printf("%d|%X\n",Reg.Data,Reg.Data);
            }
            else {
               float x;
               x = (int) Reg.Data;
               x /= powf(2,gFractionBits);
               printf("%f|%X\n",x,Reg.Data);
            }
            break;

         case CMD_WRITE_REG:
            Reg.Adr = gRegisterAdr;
            Reg.Data = gRegisterData;
            if((Ret = _HalSetRegister(&Reg)) != 0) {
               printf("failed\n");
            }
            else {
               printf("written\n");
            }
            break;

      case CMD_READ_CAL_REGS:
         break;

         default:
            printf("failed\n");
            break;
      }
   } while(FALSE);

   if(bHalInitialized && (Ret = HAL_Cleanup()) != 0) {
      printf("failed\n");
   }

   return Ret;
}


void PrintValues(DataValues *p)
{
   if(gDebug) {
      printf("   vRMS: %d.%03d\n",p->vRMS/1000,p->vRMS%1000);
      printf("   iRMS: %d.%03d\n",p->iRMS/1000,p->iRMS%1000);
      printf("  Watts: %d.%03d\n",p->Watts/1000,p->Watts%1000);
      printf("     PF: %d.%02d\n",p->PF/100,p->PF%100);
      printf("   Freq: %d.%02d\n",p->Freq/100,p->Freq%100);
      printf("IntTemp: %d.%02d\n",p->InternalTemp/100,p->InternalTemp%100);
      printf("ExtTemp: %d.%02d\n",p->ExternalTemp/100,p->ExternalTemp%100);
   }
   else {
      printf("%d.%03d|",p->Watts/1000,p->Watts%1000);
      printf("%d.%03d|",p->vRMS/1000,p->vRMS%1000);
      printf("%d.%03d|",p->iRMS/1000,p->iRMS%1000);
      printf("%d.%02d|",p->PF/100,p->PF%100);
      printf("%d.%02d|",p->Freq/100,p->Freq%100);
      printf("%d.%02d|",p->InternalTemp/100,p->InternalTemp%100);
      printf("%d.%02d\n",p->ExternalTemp/100,p->ExternalTemp%100);
   }
}

void PrintTotals(TotalData *p)
{
   u32 x = p->WattMinutes * 100 / 60;

   if(gDebug) {
      printf("   Total Watt Minutes: %u\n",p->WattMinutes);
      printf("     Total Watt hours: %u.%02u\n",x/100, x % 100);
      printf(" Measurement duration: %u minutes\n",p->TotalMinutes);
      printf("Total Daemon run time: %u seconds\n",p->RunTimeSeconds);
   }
   else {
      printf("%u.%02u|",x/100, x % 100);
      printf("%u|",p->TotalMinutes);
      printf("%u\n",p->RunTimeSeconds);
   }
}

#define ADD2CMD(x) Buf[j++] = x; CheckSum += x
int SendSiiCmd(u8 Cmd,u8 *Data,int DataLen)
{
   int Ret = 0;
   char CheckSum = 0;
   int i;
   int j = 0;
   u8 Buf[256];

   DataLen += 4;  // add overhead
   ADD2CMD(SSI_ACK_DATA);  // Start of frame
   ADD2CMD((u8) DataLen);  // Frame length
   ADD2CMD(Cmd);

   for(i = 0; i < DataLen - 4; i++) {
      ADD2CMD(Data[i]);
   }
   Buf[j++] = -CheckSum;

   if(gVerbose > 1) {
      printf("%s: sending:\n",__FUNCTION__);
      DumpHex(Buf,DataLen);
   }

   if(gSerialFd != -1) {
      if(write(gSerialFd,Buf,DataLen) != DataLen) {
         LOG("%s: write failed - %s\n",__FUNCTION__,strerror(errno));
         Ret = errno;
      }
   }

   return Ret;
}

int ReadRegister(u32 Adr)
{
   int Ret = 0;
   u8 Data[3];

   Data[0] = (u8) (Adr & 0xff);        // Address LSB
   Data[1] = (u8) ((Adr >> 8) & 0xff); // Address MSB
   Data[2] = SSI_CMD_READ_3;

   if((Ret = SendSiiCmd(SSI_CMD_SET_ADR,Data,3)) == 0) {
      gResponseCmd = CMD_REG_DATA;
   }

   return Ret;
}

int WriteRegister(u32 Adr,u32 Value)
{
   int Ret;
   u8 Data[6];

   Data[0] = (u8) (Adr & 0xff);        // Address LSB
   Data[1] = (u8) ((Adr >> 8) & 0xff); // Address MSB
   Data[2] = SSI_CMD_WRITE_3;
   Data[3] = (u8) (Value & 0xff);            // Data LSB
   Data[4] = (u8) ((Value >> 8) & 0xff);
   Data[5] = (u8) ((Value >> 16) & 0xff);    // Data MSB

   if((Ret = SendSiiCmd(SSI_CMD_SET_ADR,Data,6)) == 0) {
      gResponseCmd = CMD_WROTE_REG;
   }

   return Ret;
}

int ReadCalRegisters()
{
   int Ret = 0;
   u8 Data[4];
   u32 Adr = SSI_ADR_S1_GAIN;

   Data[0] = (u8) (Adr & 0xff);        // Address LSB
   Data[1] = (u8) ((Adr >> 8) & 0xff); // Address MSB
   Data[2] = SSI_CMD_READ_N;
   Data[3] = 30;  // 30 bytes (10 cal values)

   if((Ret = SendSiiCmd(SSI_CMD_SET_ADR,Data,4)) == 0) {
      gResponseCmd = CMD_CAL_REGS;
   }

   return Ret;
}


int SelectTarget(u8 Device)
{
   u8 Cmd = (u8) (SSI_CMD_SEL_DEV | Device);

   return SendSiiCmd(Cmd,NULL,0);
}

int AutoRpt(int bEnable)
{
   return WriteRegister(0,bEnable ? 0x38 : 0x30);
}


/* 
Typical data:
AE - header
1E - byte count
 
-- values 
7A 5C 00 - Temperature 23.674,   (/1000)
09 00 00 - External Temperature 0.009, (/1000)
AA CD 01 - Line voltage 118.186,  (/1000)
34 58 00 - IRMS 0.17640625, (/128000)
1C 0A 00 - Active power (?) 12.94,  (/200)
0E 0A 00 - Average power (?) 12.87, (/200)
6C 02 00 - Power factor 0.62, (/1000)
93 EA 00 - Line Frequency 60.051,   (/1000)
00 00 00 - Active energy ?
-- 
28 - checksum
*/

void ProcessAutoData(u8 *Msg)
{
   int Values;
   int i;
   union {
      int x;
      u8 U8[4];
   } Value;
   int Int;
   unsigned int UInt;
   #define NUM_AUTORPT_VALUES 9

   do {
      Values = Msg[1] - 3;
      if((Values % 3) != 0) {
      // Non integer number of values
         break;
      }
      Values /= 3;

      if(Values != NUM_AUTORPT_VALUES) {
      // Not the expected number of values
         break;
      }

      for(i = 0; i < Values; i++) {
         memcpy(Value.U8,&Msg[2+(i*3)],3);
      // sign extend to 32 bits
         if(Value.U8[2] & 0x80) {
         // Negative value.  
            Value.U8[3] = 0xff;
            Int = Value.x;
            UInt = 0;
         }
         else {
            Value.U8[3] = 0;
            Int = Value.x;
            UInt = Value.x;
         }
         switch (i) {
/*
7A 5C 00 - Temperature 23.674,   (/1000)
09 00 00 - External Temperature 0.009, (/1000)
AA CD 01 - Line voltage 118.186,  (/1000)
34 58 00 - IRMS 0.17640625, (/128000)
1C 0A 00 - Active power (?) 12.94,  (/200)
0E 0A 00 - Average power (?) 12.87, (/200)
6C 02 00 - Power factor 0.62, (/1000)
93 EA 00 - Line Frequency 60.051,   (/1000)
00 00 00 - Active energy ?

*/
         case 0:  // Temperature
            gCurrent.InternalTemp = UInt / 10;
            break;

         case 1:  // External Temperature
            gCurrent.ExternalTemp = UInt / 10;
            break;

         case 4:  // Active power (?)
         case 8:  // Active energy ?
            break;

         case 2:  // Line voltage
            gCurrent.vRMS = UInt;
            break;

         case 3:  // IRMS
            gCurrent.iRMS = UInt / 128;
            break;

         case 5:  // Average power (?)
         // Scale to milliwatts
            gCurrent.Watts = UInt * 5;
            if(g_bSanityCheck) {
               SanityCheck(Msg);
            }
            break;

         case 6:  // Power factor
            gCurrent.PF = UInt / 10;
            break;

         case 7:  // Line Frequency
            gCurrent.Freq = UInt / 10;
         }
      }
   // Update averages

      if(AverageTimer == 0) {
         AverageTimer = UpTime + gAverageSeconds;
      }

      if(WattMinuteTimer == 0) {
         WattMinuteTimer = UpTime + 60;
      }

      ValuesAveraged++;
      gAccumvRMS += gCurrent.vRMS;
      gAccumiRMS += gCurrent.iRMS;
      gAccumWatts += gCurrent.Watts;
      gAccumPF += gCurrent.PF;
      gAccumFreq += gCurrent.Freq;
      gAccumIntTemp += gCurrent.InternalTemp;
      gAccumExtTemp += gCurrent.ExternalTemp;

      if(UpTime >= AverageTimer) {
      // Time to update the average
         if(gVerbose) {
            printf("AverageTimer timeout, ValuesAveraged: %d\n",
                   ValuesAveraged);
         }
         gAverage.vRMS = gAccumvRMS / ValuesAveraged;
         gAverage.Watts = gAccumWatts / ValuesAveraged;
         gAverage.PF = gAccumPF / ValuesAveraged;
         gAverage.Freq = gAccumFreq / ValuesAveraged;
         gAverage.iRMS = gAccumiRMS / ValuesAveraged;
         gAverage.InternalTemp =  gAccumIntTemp / ValuesAveraged;
         gAverage.ExternalTemp = gAccumExtTemp / ValuesAveraged;

         AverageTimer += gAverageSeconds;
         ValuesAveraged =
         gAccumvRMS = 
         gAccumiRMS =
         gAccumWatts = 
         gAccumPF = 
         gAccumFreq = 
         gAccumIntTemp =
         gAccumExtTemp = 0;
      }

      gAccumWattMinute += gCurrent.Watts;
      WattMinuteAveraged++;

      if(UpTime >= WattMinuteTimer) {
      // Another minute has elapsed, update the total watt / minutes
         gTotalMinutes++;
         gWattMinuteTotal += (gAccumWattMinute / WattMinuteAveraged);
         gAccumWattMinute = 0;
         WattMinuteTimer += 60;
         WattMinuteAveraged = 0;
      }
   } while(FALSE);
}

void SanityCheck(u8 *Msg)
{
   float Watts = ((float) gCurrent.Watts) / 1000.0;

   if((gMinimumSanePower != -1.0 && Watts < gMinimumSanePower) ||
       (gMaximumSanePower!= -1.0 && Watts > gMaximumSanePower))
   {
      LOG("%s: Sanity check failed, gCurrent.Watts: %d.%03d:\n",__FUNCTION__,
         gCurrent.Watts / 1000, gCurrent.Watts % 1000);
      DumpHex(Msg,Msg[1]);
   }
}

void DumpHex(void *AdrIn,int Len)
{
   char Line[80];
   unsigned char *cp;
   unsigned char *Adr = (unsigned char *) AdrIn;
   int i = 0;
   int j;

   while(i < Len) {
      cp = Line;
      for(j = 0; j < 16; j++) {
         if((i + j) == Len) {
            break;
         }
         sprintf(cp,"%02x ",Adr[i+j]);
         cp += 3;
      }

      *cp++ = ' ';
      for(j = 0; j < 16; j++) {
         if((i + j) == Len) {
            break;
         }
         if(isprint(Adr[i+j])) {
            *cp++ = Adr[i+j];
         }
         else {
            *cp++ = '.';
         }
      }
      *cp = 0;
      LOG("%s\n",Line);
      i += 16;
   }
}

