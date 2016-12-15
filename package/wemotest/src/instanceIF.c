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
 * instanceIF.c
 *
 *  Created on: Oct 3, 2012
 *      Author: simon.xiang
 */
#include "instanceIF.h"
#include "instance.h"

static pInstanceIF instanceTable[UDP_COMMAND_MAX] = {0x00};
/*
 *  UDP_PROGRAMMING_STATE_REQ     = 0x0001,
    UDP_BOOTLOADER_VERSION_REQ    = 0x0002,
    UDP_FIRMWARE_VERSION_REQ      = 0x0003,
    UDP_WIFI_DRIVER_REQ           = 0x0004,
    UDP_SOUND_CARD_VERSION_REQ    = 0x0005,
    UDP_AUDIO_CARD_VERSION_REQ    = 0x0006,
    UDP_SERIAL_NUMBER_REQ         = 0x0007,
    UDP_WIFI_RA0_MAC_REQ          = 0x0008,
    UDP_WIFI_APCLI0_MAC_REQ       = 0x0009,
    UDP_WIFI_COUNTRY_CODE_REQ     = 0x000A,
    UDP_WIFI_REGION_CODE_REQ      = 0x000B,
    UDP_DEVICE_FRIENDLY_NAME_REQ
    UDP_COMMAND_MAX

 */

/**
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
 */
void initInstanceTable()
{
  //- Assign directly
  instanceTable[UDP_PROGRAMMING_STATE_REQ]      = executeProgrammingStatusReq;
  instanceTable[UDP_BOOTLOADER_VERSION_REQ]     = executeBootloaderReq;
  instanceTable[UDP_FIRMWARE_VERSION_REQ]       = executeFirmwareVersionReq;
  instanceTable[UDP_WIFI_DRIVER_REQ]            = executeWiFiVersionReq;
  instanceTable[UDP_SOUND_CARD_VERSION_REQ]     = executeSoundCardVersionReq;
  instanceTable[UDP_VIDEO_CARD_VERSION_REQ]     = executeVideoCardVersionReq;
  instanceTable[UDP_SERIAL_NUMBER_REQ]          = executeSerialNumberReq;
  instanceTable[UDP_WIFI_RA0_MAC_REQ]           = executeRA0MacReq;
  instanceTable[UDP_WIFI_COUNTRY_CODE_REQ]      = executeCoutryCodeReq;
  instanceTable[UDP_WIFI_REGION_CODE_REQ]       = executeRegionCodeReq;
  instanceTable[UDP_WIFI_RA0_IP_REQ]            = executeRa0IPAddressReq;
  instanceTable[UDP_WIFI_SSID_REQ]              = executeSSIDReq;
  instanceTable[UDP_SERIAL_NUMBER_REQ]          = executeSerialNumberReq;
  instanceTable[UDP_WIFI_WRITE_SERIAL_REQ]      = executeWriteSerialReq;
  instanceTable[UDP_WIFI_RA0_MAC_CHANGE_REQ]    = executeWriteMaclReq;
  instanceTable[UDP_AUDIO_RECORDING_REQ]        = executeAudioRecordingReq;
  instanceTable[UDP_GPIO_WRITE_REQ]             = executeGPIOWriteReq;
  instanceTable[UDP_GPIO_READ_REQ]              = executeGPIOReadReq;
  instanceTable[UDP_WFI_CONNECT_WIFI_REQ]       = ConnectWiFi;
  instanceTable[UDP_DEVICE_FRIENDLY_NAME_REQ]   = executeGetFriendlyName;
}


/**
 * routeInstance
 *      To route the instance to peoper entry
 *
 * @param commandID
 * @param data
 * @param size
 *
 *
 */
void routeInstance(unsigned int commandID, char* data, int size)
{
   if (commandID >= UDP_COMMAND_MAX)
   {
       printf("ERROR: Command ID overflow\n");
       return;
   }

   if (0x00 != instanceTable[commandID])
   {
       instanceTable[commandID](data, size);
   }
   else
   {
       printf("Error: instance entry not assigned yet");
   }
}
