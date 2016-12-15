/***************************************************************************
*
*
* insightRemoteAccess.h
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
#ifdef PRODUCT_WeMo_Insight
#ifndef INSIGHT_REMOTE_HANDLER_H_
#define INSIGHT_REMOTE_HANDLER_H_

#define	LINK_DOMAIN_NM "insight.lswf.net"
int remoteAccessGetPluginDetails(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void* remoteInfo,char* transaction_id);
void* parseInsightNotificationResp(void *sendNfResp, char **errcode);
int remoteAccessSetPowerThreshold(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void* remoteInfo,char* transaction_id); 
int remoteAccessGetPowerThreshold(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void* remoteInfo,char* transaction_id); 
int remoteAccessSetClearDataUsage(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void* remoteInfo,char* transaction_id); 
int remoteAccessGetDataExportInfo(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void* remoteInfo,char* transaction_id); 
int remoteAccessScheduleDataExport(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void* remoteInfo,char* transaction_id);
int remoteAccessSetInsightHomeSettings(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void* remoteInfo,char* transaction_id);
int sendRemoteInsightParams (void);
extern char g_InsightDataSend;
void UploadInsightDataFile(void *arg);
int uploadInsightDataFileStatus(void *arg);
void  GetCurrencyString(int Code,char *Str);
void  GetEventCurrencyString(int Code,char *Str);
void FetchInsightParams(char *InsightParams);
int sendRemoteInsightEventParams (void);

#endif//INSIGHT_REMOTE_HANDLER_H_

#endif//PRODUCT_WeMo_Insight