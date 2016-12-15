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
#ifndef _INSIGHTD_H_
#define _INSIGHTD_H_

#ifndef INSIGHTD_PRIVATE
#error "Information in this header is private to the HAL layer, application code should include it"
#endif

#ifndef FALSE
#define FALSE     0
#endif

#ifndef TRUE
#define TRUE      (!FALSE)
#endif

#define INSIGHTD_PORT	2666

#define NUM_CAL_REGISTERS	10

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef struct {
   u16      magicNumber;
   u16      versionNum;
   u16      Cmd;
   u16      pktSeqNum;
   u16      pktSource;
   u16      pktDestination;
   u16      pktLength;
} MsgHeader;


#define CMD_RESPONSE				0x8000
#define CMD_GET_CURRENT_DATA	1
#define CMD_CURRENT_DATA		(CMD_GET_CURRENT_DATA | CMD_RESPONSE)

#define CMD_GET_AVERAGE_DATA	2
#define CMD_AVERAGE_DATA		(CMD_GET_AVERAGE_DATA | CMD_RESPONSE)

#define CMD_GET_TOTAL			3
#define CMD_TOTAL					(CMD_GET_TOTAL | CMD_RESPONSE)

#define CMD_GET_TOTAL_CLR		4
#define CMD_TOTAL_CLR			(CMD_GET_TOTAL_CLR | CMD_RESPONSE)

#define CMD_EXIT					5
#define CMD_EXITING				(CMD_EXIT | CMD_RESPONSE)

#define CMD_READ_REG				6
#define CMD_REG_DATA				(CMD_READ_REG | CMD_RESPONSE)

#define CMD_WRITE_REG			7
#define CMD_WROTE_REG			(CMD_WRITE_REG | CMD_RESPONSE)

#define CMD_READ_CAL_REGS		8
#define CMD_CAL_REGS				(CMD_READ_CAL_REGS | CMD_RESPONSE)

#define CMD_GET_INSIGHTVAR		9
#define CMD_GOT_INSIGHTVAR				(CMD_GET_INSIGHTVAR | CMD_RESPONSE)

#define CMD_SET_INSIGHTVAR		10
#define CMD_SETDONE_INSIGHTVAR				(CMD_SET_INSIGHTVAR | CMD_RESPONSE)


typedef struct {
	unsigned int Adr;
	unsigned int Data;
} RegisterData;

typedef struct {
	MsgHeader Hdr;
	union {
		DataValues Values;
		TotalData  Totals;
		RegisterData RegData;
		u32 CalRegValues[NUM_CAL_REGISTERS];
		char Data;
		InsightVars InsightValues;
		
	} u;
} InsightdMsg;

// a type to make sockaddr/sockaddr_in translations a bit easier
typedef union {
   struct sockaddr s;
   struct sockaddr_in i;
   #define ADDR   i.sin_addr.s_addr
   #define PORT   i.sin_port
} AdrUnion;

// Read 78m6610 register
// NOTE: The application should not call this function,
// it is provided for hardware validation only.
// 
// Parameter:
// 	p - pointer to structure to receive data 
// 	p->Adr - Register address to read
// 
// Returns:
// 	  0 - SUCCESS
// 	< 0 - standard Linux errno error code
//		> 0 - HAL error code (see ERR_HAL_*)
int _HalGetRegister(RegisterData *p);

// Write 78m6610 register
// NOTE: The application should not call this function,
// it is provided for hardware validation only.
// 
// Parameter:
// 	p - pointer to structure to with data to write
// 	p->Adr  - Register address to write
// 	p->Data - Data to write
// 
// Returns:
// 	  0 - SUCCESS
// 	< 0 - standard Linux errno error code
//		> 0 - HAL error code (see ERR_HAL_*)
int _HalSetRegister(RegisterData *p);

// Read 78m6610 calibration registers
// NOTE: The application should not call this function,
// it is provided for hardware validation only.
// 
// Parameter:
// 	p - pointer to array to receive NUM_CAL_REGISTERS register values
// 
// Returns:
// 	  0 - SUCCESS
// 	< 0 - standard Linux errno error code
//		> 0 - HAL error code (see ERR_HAL_*)
int _HalReadCalRegisters(unsigned int *p);


#endif	// _INSIGHTD_H_

