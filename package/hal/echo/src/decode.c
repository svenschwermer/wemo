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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <netinet/in.h>

typedef unsigned char u8;     // i.e. 8 bit unsigned data
typedef unsigned short u16;   // i.e. 16 bit unsigned data
typedef unsigned int u32;     // i.e. 32 bit unsigned data

#include "echo.h"

#ifndef FALSE
#define FALSE     0
#endif

#ifndef TRUE
#define TRUE      (!FALSE)
#endif

static char *MonthNames[] = {
   "Jan",
   "Feb",
   "Mar",
   "Apr",
   "May",
   "Jun",
   "Jul",
   "Aug",
   "Sep",
   "Oct",
   "Nov",
   "Dec"
};

int DecodeFile(char *Filename)
{
   FILE *fp = NULL;
   FileHeader Hdr;
   EchoData Data;
   time_t TimeStamp;
   struct tm *tm;
   int NumDataBlocks;
   int Block = 1;
   int i;
   int Ret = 0;   // assume the best

   do {
      if((fp = fopen(Filename,"r")) == NULL) {
         printf("fopen of '%s' failed - %s\n",Filename,strerror(errno));
         Ret = errno;
         break;
      }

   // Read the file header
      if(fread(&Hdr,sizeof(Hdr),1,fp) != 1) {
         if(ferror(fp)) {
            printf("fread failed - %s\n",strerror(errno));
            Ret = errno;
         }
         else if(feof(fp)) {
            printf("Error: unexpected eof, file truncated?\n");
            Ret = 255;  // Internal error
         }
         else {
            printf("fread failed in an odd way\n");
            Ret = 255;  // Internal error
         }
         break;
      }

      if(strncmp(Hdr.Signature,DATAFILE_SIGNATURE,sizeof(Hdr.Signature)) != 0) {
         Ret = 255;  // Internal error
         printf("Error: file signature test failed.\n");
         break;
      }

      printf("      File format: Version %u\n",ntohs(Hdr.FormatVersion));
      printf("        Device SN: %s\n",Hdr.SN);
      TimeStamp = ntohl(Hdr.TimeStamp);
      tm = gmtime(&TimeStamp);
      printf("   Data collected: %s %d %d:%02d:%02d\n",
             MonthNames[tm->tm_mon],tm->tm_mday,
             tm->tm_hour,tm->tm_min,tm->tm_sec);
      NumDataBlocks = ntohs(Hdr.NumDataBlocks);
      printf("MinPressureChange: %u\n",ntohs(Hdr.MinPressureChange));
      printf("      MaxIdleTime: %u\n",ntohs(Hdr.MaxIdleTime));
      printf("      Data blocks: %u\n",NumDataBlocks);

      while(fread(&Data,sizeof(Data),1,fp) == 1) {
         printf("\nData block: %d\n",Block++);
         printf("Flow count: %u\n",ntohs(Data.FlowCount));
         printf("Record number: %u\n",ntohs(Data.RecNum));
         for(i = 0; i < NUM_PRESSURE_READINGS; i++) {
            printf("%5d",ntohs(Data.Pressure[i]));
            if(i == 29) {
               printf("\n");
            }
            else {
               printf(",%s",((i + 1) % 10) == 0 ? "\n" : "");
            }
         }
      }

      if(!feof(fp)) {
         Ret = errno;
         printf("fread failed - %s\n",strerror(errno));
      }
   } while(FALSE);

   if(fp != NULL) {
      fclose(fp);
   }

   return Ret;
}

