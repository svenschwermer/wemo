/***************************************************************************
*
*
* gpioApi.c 
*
* Created by Belkin International, Software Engineering on Apr 06, 2012
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
#include <sys/time.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include "gpioApi.h"
#include "fcntl.h"
#include "upnp.h"
#include "utils.h"
#include "unistd.h"
#include "stdlib.h"
#include <string.h>
#include "logger.h"
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

pthread_t gpioMonitorThread = -1;
int callBackCounter = 0;
gpioEventCb *eventCb; 

int setDirGPIOIp(int set_mode, int gpioid){
	int err, fd;

	if( (fd = open("/dev/gpio", O_RDWR)) < 0 )
	{
		APP_LOG("NetworkControl",LOG_DEBUG, "Open /dev/gpio failed");
		return -1;
	}

	err = ioctl(fd, RALINK_GPIO_SET_DIR_IN, (void *) &set_mode);
	if( err < 0 ){
		APP_LOG("NetworkControl",LOG_DEBUG, "Set input direction GPIO failed");
	}
	close(fd);
	return err;
}

int setDirGPIOOp(int set_mode, int gpioid){
	int err, fd;

	if( (fd = open("/dev/gpio", O_RDWR)) < 0 )
	{
		APP_LOG("NetworkControl",LOG_DEBUG, "Open /dev/gpio failed");
		return -1;
	}

	err = ioctl(fd, RALINK_GPIO_SET_DIR_OUT, (void *) &set_mode);
	if( err < 0 ){
		APP_LOG("NetworkControl",LOG_DEBUG, "Set Output direction GPIO failed");
	}
	close(fd);
	return err;
}

int GPIOReadByte(int *byteRead, int gpioid){

	int err, fd;

	if( (fd = open("/dev/gpio", O_RDWR)) < 0 )
	{
		APP_LOG("NetworkControl",LOG_DEBUG, "Open /dev/gpio failed");
		return -1;
	}
	if (gpioid < 22) {
		err = ioctl(fd, RALINK_GPIO_READ, (void *)byteRead);
		if( err < 0 ){
			APP_LOG("NetworkControl",LOG_DEBUG, "GPIO Read byte failed ");
		}
	}else {
		err = ioctl(fd, RALINK_GPIO5140_READ, (void *)byteRead);
		if( err < 0 ){
			APP_LOG("NetworkControl",LOG_DEBUG, "GPIO Read byte failed ");
		}
	}
	close(fd);
	return err;
}

int GPIOWriteByte(int *byteWrite, int gpioid){

	int err, fd;

	if( (fd = open("/dev/gpio", O_RDWR)) < 0 )
	{
		APP_LOG("NetworkControl",LOG_DEBUG, "Open /dev/gpio failed");
		return -1;
	}

	if (gpioid < 22) {
		err = ioctl(fd, RALINK_GPIO_WRITE, (void *)byteWrite);
		if( err < 0 ){
			APP_LOG("NetworkControl",LOG_DEBUG, "GPIO Write byte failed ");
		}
	}else {
		err = ioctl(fd, RALINK_GPIO5140_WRITE, (void *)byteWrite);
		if( err < 0 ){
			APP_LOG("NetworkControl",LOG_DEBUG, "GPIO Write byte failed ");
		}
	}
	close(fd);
	return err;

}

int GPIOWSetDir(int *setByte, int gpioid){

	int err, fd;

	if( (fd = open("/dev/gpio", O_RDWR)) < 0 )
	{
		APP_LOG("NetworkControl",LOG_DEBUG, "Open /dev/gpio failed");
		return -1;
	}

	err = ioctl(fd, RALINK_GPIO_SET_DIR, (void *) setByte);
	if( err < 0 ){
		APP_LOG("NetworkControl",LOG_DEBUG, "GPIO Write byte failed ");
	}
	close(fd);
	return err;

}

int set_GPIO_status (const int gpioId, const int state){

	int byteRead, value, retVal;
	int adjusted = 0;
	retVal = GPIOReadByte( &byteRead, gpioId);
	if(retVal){
		APP_LOG("NetworkControl",LOG_DEBUG, "GPIO Read byte failed ");
		return retVal;
	}
	if (gpioId < 22) {
		value = ((1 & state));
		if(value)
			byteRead |= 1L << gpioId;
		else 
			byteRead &= ~(1L << gpioId);
	}else {
		value = ((1 & state));
		adjusted = gpioId-22;
		if(value)
			byteRead |= 1L << adjusted;
		else 
			byteRead &= ~(1L << adjusted);
	}

	retVal = GPIOWriteByte( &byteRead, gpioId);	
	if(retVal){
		APP_LOG("NetworkControl",LOG_DEBUG, "GPIO Write byte failed ");
		return retVal;
	}
	return retVal;
}

int get_GPIO_status (const int gpioId){

	int byteRead, value, retVal;
	int adjusted = 0;
	retVal = GPIOReadByte(&byteRead, gpioId);				
	if(retVal){
		APP_LOG("NetworkControl",LOG_DEBUG, "GPIO Read byte failed ");
		return retVal;
	}
	if (gpioId < 22) {
		value = (byteRead >> gpioId) & 1;	
	}else {
		adjusted = gpioId-22;
		value = (byteRead >> adjusted) & 1;	
	}
	return value;

}

void *gpioLedMonitorTask(void *args){
	int i, value, retVal, byteRead;
    
	tu_set_my_thread_name( __FUNCTION__ );

	while (1) {
		retVal = GPIOReadByte(&byteRead, 0);				
		if(retVal){
			APP_LOG("NetworkControl",LOG_DEBUG, "GPIO Read byte failed ");
			sleep(1);
			continue;
		}

		value = (byteRead >> GPIO0) & 1;	
		for(i=0; i< MAX_GPIO_MONITORED_EVENTS; i++){
			if((value != (eventCb+i)->state) && ((eventCb+i)->pinId == GPIO0)){
				((eventCb+i)->cb)();
				(eventCb+i)->state = value;
			}	
		}
#ifndef PRODUCT_WeMo_LEDLight        
		value = (byteRead >> GPIO13) & 1;	
		for(i=0; i< MAX_GPIO_MONITORED_EVENTS; i++){
			if((value != (eventCb+i)->state) && ((eventCb+i)->pinId == GPIO13)){
				((eventCb+i)->cb)();
				(eventCb+i)->state = value;
			}	
		}

		value = (byteRead >> GPIO11) & 1;	
		for(i=0; i< MAX_GPIO_MONITORED_EVENTS; i++){
			if((value != (eventCb+i)->state) && ((eventCb+i)->pinId == GPIO11)){
				((eventCb+i)->cb)();
				(eventCb+i)->state = value;
			}			
		}
#endif
		pluginUsleep(100*1000);
	}
	return NULL;
}

int gpioCbRegInit(){
	int i;

	eventCb = (gpioEventCb*)ZALLOC(sizeof(gpioEventCb)*MAX_GPIO_MONITORED_EVENTS);
	
	for(i=0; i< MAX_GPIO_MONITORED_EVENTS; i++){
		(eventCb+i)->state = -1;
		(eventCb+i)->pinId = -1;
	}
	return 0x0;
}

int gpioCallbackRegister( gpioCbFunc cbFunction, const int gpioId){

	int retVal=0;

	if(!callBackCounter){
		gpioCbRegInit();	
	}	

	(eventCb+callBackCounter)->cb = cbFunction; 
	(eventCb+callBackCounter)->pinId = gpioId; 

	if(!callBackCounter){
		pthread_create(&gpioMonitorThread, NULL, gpioLedMonitorTask, NULL);
	}
	callBackCounter++;

	return retVal;
}

int SetWiFiGPIOLED(int value) {
	int err, fd;
	if( (fd = open("/dev/gpio", O_RDWR)) < 0 )
	{
		APP_LOG("NetworkControl",LOG_DEBUG, "Open /dev/gpio failed");
		return -1;
	}

	err = ioctl(fd, RALINK_GPIO_SET_CUSTOM_LED, (void *)&value);
	if( err < 0 ){
		APP_LOG("NetworkControl",LOG_DEBUG, "LED GPIO Write byte failed ");
	}
	close(fd);
	return err;
}
