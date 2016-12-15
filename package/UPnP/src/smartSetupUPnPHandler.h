/***************************************************************************
*
*
* smartSetupUPnPHandler.h
*
* Created by Belkin International, Software Engineering on May 27, 2011
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



#ifdef WeMo_SMART_SETUP
#ifndef SMART_SETUP_UPNP_HANDLER_H_
#define SMART_SETUP_UPNP_HANDLER_H_

struct pair_reg_info
{
	WIFI_PAIR_PARAMS pairInfo;
	RemoteAccessInfo regInfo;
};
typedef struct pair_reg_info PairRegInfo;

typedef struct PAIR_AND_REG_INFO_
{
	char* pPairingData;
	char* pRegistrationData;
}PAIR_AND_REG_INFO,*PPAIR_AND_REG_INFO;

#ifdef WeMo_SMART_SETUP_V2
extern int g_customizedState;
extern int g_isAppConnected;
#endif
int parseXMLData(char *apXMLData, char **apParameterToParse, char** apOutData, int aParameterCount);
void* StartPairAndRegisterThread(void* arg);

int PairAndRegister(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString); 
int GetRegistrationData(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString); 
int GetRegistrationStatus(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString); 
int parseXMLData(char *apXMLData, char **apParameterToParse, char** apOutData, int aParameterCount);
#ifdef WeMo_SMART_SETUP_V2
void setCustomizedState(int state);
int SetCustomizedState(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetCustomizedState(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
#endif

#endif//SMART_SETUP_UPNP_HANDLER_H_
#endif//WeMo_SMART_SETUP
