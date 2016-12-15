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

#ifndef _ECHO_DATA_H_
#define _ECHO_DATA_H_

#ifndef GCC_PACKED
#if defined(__GNUC__)
#define GCC_PACKED __attribute__ ((packed))
#else
#define GCC_PACKED
#endif
#endif // GCC_PACKED

// Structure of WASP_VAR_ECHO_DATA blob
typedef struct {
   u16   RecNum;     // Increments each time to detect data loss, resets
   u16   FlowCount;  // Pulse count from flow meter
#define NUM_PRESSURE_READINGS 30
   u16   Pressure[NUM_PRESSURE_READINGS]; // full scale = 150 millivolts
} GCC_PACKED EchoData;

typedef struct {
   #define DATAFILE_SIGNATURE    "EchoData"
   char  Signature[8];     // "EchoData" in ASCII
   #define DATA_FILE_VERSION  1
   u16   FormatVersion;    // version of data contained in this file
   char  SN[16];           // Device serial number that collected the data
   time_t   TimeStamp;     // Approximate UTC time first pressure sample was taken
   u16   NumDataBlocks;    // Number of EchoData blocks following header

// Device configuration parameters

// Minimum pressure change to trigger data transmission
   u16   MinPressureChange;   

// Maximum time to wait for new data before sending anyway
   u16   MaxIdleTime;         
// ...

//   DataCluster Clusters[MAX_CLUSTERS];
} FileHeader;



#endif // _ECHO_DATA_H_

