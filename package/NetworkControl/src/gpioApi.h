/***************************************************************************
*
*
* gpioApi.h
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
#ifndef __GPIOAPI_H__
#define __GPIOAPI_H__

#define RALINK_GPIO_SET_DIR_IN   0x11
#define RALINK_GPIO_SET_DIR_OUT  0x12
#define RALINK_GPIO_WRITE_BYTE   0x07
#define RALINK_GPIO_WRITE	 0x03
#define RALINK_GPIO_READ         0x02
#define RALINK_GPIO_SET_DIR      0x01
#define RALINK_GPIO5140_READ		 0x62
#define RALINK_GPIO5140_WRITE		 0x63
#define RALINK_GPIO_SET_CUSTOM_LED 0x99

//output gpio
#define GPIO14 14
#define GPIO12 12
#define GPIO25 25
#define GPIO26 26
#define GPIO24 24
#define GPIO23 23

//input GPIO
#define GPIO13 13
#define GPIO0  0
#define GPIO11 11

#define MAX_GPIO_MONITORED_EVENTS 5

int setDirGPIOIp(int set_mode, int gpioid);
int setDirGPIOOp(int set_mode, int gpioid);
int GPIOReadByte(int *byteRead, int gpioid);
int GPIOWriteByte(int *byteWrite, int gpioid);
int GPIOWSetDir(int *setByte, int gpioid);
int set_GPIO_status(const int gpioId, const int state);
int get_GPIO_status(const int gpioId);

typedef void (*gpioCbFunc)();
typedef struct eventCb {

	gpioCbFunc cb;
	int pinId;
	int state;
}gpioEventCb;

int gpioCallbackRegister( gpioCbFunc cbFunction, const int gpioId);
int SetWiFiGPIOLED(int value);

#endif
