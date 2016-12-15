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
*/

#ifndef _WASP_EMBEDDED_H_
#define _WASP_EMBEDDED_H_

typedef unsigned char u8;     // i.e. 8 bit unsigned data
typedef unsigned short u16;   // i.e. 16 bit unsigned data

#define BIT_0_7(x)   ((char) (x & 0xff))
#define BIT_8_15(x)  ((char) ((x >> 8) & 0xff))
#define BIT_16_23(x) ((char) ((x >> 16) & 0xff))
#define BIT_24_31(x) ((char) ((x >>24) & 0xff))
#define DESC_U16(x)  BIT_0_7(x),BIT_8_15(x)
#define DESC_U32(x)  BIT_0_7(x),BIT_8_15(x),BIT_16_23(x),BIT_24_31(x)


#define  MAX_CLUSTERS      5  // We'll tune this

#if 0
typedef struct {
   time_t   Timestamp;     // Time when data was taken
   u16      FlowCount;     // Flow counter value
   u16      PresureMeasurements; // number of pressure measures
   u16      Pressure[0];   // Actually PresureMeasurements entries
} DataCluster;

typedef struct {
   int   FormatVersion;    // version of data contained in file
   char  SN[14];           // Device serial number that collected the data
   time_t   UpTime;        // Seconds since last device reset/power cycle

// Device configuration parameters

// Minimum pressure change to trigger data transmission
   u16   MinPressureChange;   

// Maximum time to wait for new data before sending anyway
   u16   MaxIdleTime;         
// ...

   DataCluster Clusters[MAX_CLUSTERS];
} FileHeader;

/* 
   Filename format: <Unique Sequence ID from SN>_<TimeStamp as 8 hex digits>.gz
*/
#endif


/* Global Variables */

/* Set this to the maximum WASP Message size need by this product */
#define RX_BUF_SIZE     32

extern u8  RxBuf[RX_BUF_SIZE];
// Offsets in RxBuf to various items
#define  SEQ_OFFSET     0
#define  CMD_OFFSET     1
#define  ARG_OFFSET     2

extern u8 RxSeqNum;
extern u8 RxMsgLen;

void WaspInit(void);

/* Returns 1 if a new message is available in RxBuf */
u8 ParseWaspData(u8 RxByte);

/* 
   Returns data byte to send
*/
u8 GetWaspTxData(void);

/* 
   Send WASP message.
   Returns 0 if message queued for transmission
*/
u8 SendWaspMsg(u8 *Msg,u8 MsgLen,u8 bAsync);

u8 WaspTxReady(void);

#endif   // _WASP_EMBEDDED_H_

