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
/*
 * instance.h
 *
 *  Created on: Oct 5, 2012
 *      Author: wemo
 *
 *      This file contains all test cases protocol decode and response
 */

#ifndef INSTANCE_H_
#define INSTANCE_H_

void executeProgrammingStatusReq(char* src, int size);
void executeBootloaderReq(char* src, int size);
void executeFirmwareVersionReq(char* src, int size);
void executeWiFiVersionReq(char* src, int size);
void executeSoundCardVersionReq(char* src, int size);
void executeVideoCardVersionReq(char* src, int size);
void executeSerialNumberReq(char* src, int size);
void executeRA0MacReq(char* src, int size);
void executeAPCLI0MacReq(char* src, int size);
void executeCoutryCodeReq(char* src, int size);
void executeRegionCodeReq(char* src, int size);
void executeRa0IPAddressReq(char* src, int size);
void executeSSIDReq(char* src, int size);
void executeSerialNumberReq(char* src, int size);
void executeWriteSerialReq(char* src, int size);
void executeWriteMaclReq(char* src, int size);
void executeAudioRecordingReq(char* src, int size);
void executeGPIOWriteReq(char* src, int size);
void executeGPIOReadReq(char* src, int size);
void ConnectWiFi(char* src, int size);


void executeGetFriendlyName(char* src, int size);

#define         SEQ_LEN         4
#define         RESULT_LEN      4
#define         COMMAND_LEN     4
#define         FIX_LEN         1
#define         PAYLOAD_LEN     4

#define         COMMAND_INDEX           1
#define         RESP_CODE_INDEX         5
#define         SEQ_INDEX               9
#define         PAYLOAD_INDEX           13
#define         PLAYLOAD_BODY_INDEX     17

int calculateDataSize(int palyloadlen);

void responseError(int commandID, int responseCode, int SEQ);

#endif /* INSTANCE_H_ */
