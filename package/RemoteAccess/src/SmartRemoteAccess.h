/***************************************************************************
*
*
* SmartRemoteAccess.h
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
#ifdef PRODUCT_WeMo_Smart
#ifndef SMART_REMOTE_HANDLER_H_
#define SMART_REMOTE_HANDLER_H_

#if 0
#define VALUE_LEN	SIZE_32B

struct {
	unsigned char AttrID;
	unsigned char AttrType;
	char Name[VALUE_LEN+1];
	char status[VALUE_LEN];
}smart_parameters;

typedef struct smart_parameters SmartParameters;
#endif

//UpdateDeviceCapabilities handler
int SetDeviceStatusHandler(void *hndl, char *xmlBuf, unsigned int buflen, void *data_sock, void* remoteInfo, char* transaction_id);
//GetDevicesDetails handler
int GetDeviceStatusHandler(void *hndl, char *xmlBuf, unsigned int buflen, void* data_sock , void* remoteInfo, char* transaction_id);

//char* CreateNotificationXML();
#endif /* SMART_REMOTE_HANDLER_H_ */

#endif /* PRODUCT_WeMo_Smart */
