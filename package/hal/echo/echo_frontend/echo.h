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

#ifndef _ECHO_H_
#define _ECHO_H_

#include "wasp.h"
#include "echo_data.h"

// Device variables
#define WASP_VAR_PRESSURE     0x80
#define WASP_VAR_PRESSURE_MIN 0x81
#define WASP_VAR_PRESSURE_MAX 0x82
#define WASP_VAR_FLOW         0x83
#define WASP_VAR_ECHO_DATA    0x84
#define WASP_VAR_ECHO_DEBUG   0x85

typedef struct {
   u8    Hdr[3];     //Cmd, Rcode, VarId or <not used>, Cmd, Rcode
   EchoData Data;
} GCC_PACKED EchoDataMsg;

// Structure of WASP_VAR_ECHO_DEBUG blob
// NB: msp430 requires even address alignment for 16 bit values,
// the following structure was hand crafted to ensure that
// this constraint is met.
typedef struct {
   u8    DataOvfl;   // number of pressure sample lost
   volatile u16 MaxPresure;
   volatile u16 MinPresure;
   u8    TxBusyErr;  // Lost messages
   u8    RespOvfl;   // Responses lost
} GCC_PACKED EchoDebug;

typedef struct {
   u8    Seq;
   u8    Cmd;
   u8    Rcode;
   EchoDebug   Data;
} GCC_PACKED DebugMsg;



#endif   // _ECHO_H_

