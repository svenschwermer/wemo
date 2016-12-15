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
 * instanceIF.h
 *
 *  Created on: Oct 5, 2012
 *      Author: wemo
 */

#ifndef INSTANCEIF_H_
#define INSTANCEIF_H_

//----- Request commands here --------------
typedef enum {
  UDP_PROGRAMMING_STATE_REQ     = 0x0001,
  UDP_BOOTLOADER_VERSION_REQ    = 0x0002,
  UDP_FIRMWARE_VERSION_REQ      = 0x0003,
  UDP_WIFI_DRIVER_REQ           = 0x0004,
  UDP_SOUND_CARD_VERSION_REQ    = 0x0005,
  UDP_VIDEO_CARD_VERSION_REQ    = 0x0006,
  UDP_SERIAL_NUMBER_REQ         = 0x0007,
  UDP_WIFI_RA0_MAC_REQ          = 0x0008,
  UDP_WIFI_APCLI0_MAC_REQ       = 0x0009,
  UDP_WIFI_COUNTRY_CODE_REQ     = 0x000A,
  UDP_WIFI_REGION_CODE_REQ      = 0x000B,
  UDP_WIFI_RA0_IP_REQ           = 0x000C,
  UDP_WIFI_SSID_REQ             = 0x000D,
  UDP_WIFI_WRITE_SERIAL_REQ     = 0x000E,
  UDP_WIFI_RA0_MAC_CHANGE_REQ   = 0x000F,
  UDP_GPIO_WRITE_REQ            = 0x0013,
  UDP_GPIO_READ_REQ             = 0x0014,
  UDP_AUDIO_RECORDING_REQ       = 0x0018,
  UDP_WFI_CONNECT_WIFI_REQ      = 0x0020,
  UDP_DEVICE_FRIENDLY_NAME_REQ  = 0x0031,
  UDP_COMMAND_MAX
} UPD_COMMAND_REQ_E;

typedef enum {
  UDP_PROGRAMMING_STATE_RESP     = 0x1001,
  UDP_BOOTLOADER_VERSION_RESP    = 0x1002,
  UDP_FIRMWARE_VERSION_RESP      = 0x1003,
  UDP_WIFI_DRIVER_RESP           = 0x1004,
  UDP_SOUND_CARD_VERSION_RESP    = 0x1005,
  UDP_VIDEO_CARD_VERSION_RESP    = 0x1006,
  UDP_SERIAL_NUMBER_RESP         = 0x1007,
  UDP_WIFI_RA0_MAC_RESP          = 0x1008,
  UDP_WIFI_APCLI0_MAC_RESP       = 0x1009,
  UDP_WIFI_COUNTRY_CODE_RESP     = 0x100A,
  UDP_WIFI_REGION_CODE_RESP      = 0x100B,
  UDP_WIFI_RA0_IP_RESP           = 0x100C,
  UDP_WIFI_SSID_RESP             = 0x100D,
  UDP_WIFI_WRITE_SERIAL_RESP     = 0x100E,
  UDP_WIFI_RA0_MAC_CHANGE_RESP   = 0x100F,
  UDP_GPIO_WRITE_RESP            = 0x0013,
  UDP_GPIO_READ_RESP             = 0x0014,
  UDP_AUDIO_RECORDING_RESP       = 0x1018,
  UDP_WFI_CONNECT_WIFI_RESP      = 0x1020,
  UDP_DEVICE_FRIENDLY_NAME_RESP  = 0x1031,
  UDP_COMMAND_RESP_MAX
} UPD_COMMAND_RESP_E;


void routeInstance(unsigned int commandID, char* data, int size);

typedef void (*pInstanceIF)(char*, int);
void initInstanceTable();


//------ Resposne commands here ---------------------


#endif /* INSTANCEIF_H_ */
