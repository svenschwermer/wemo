/***************************************************************************
*
* Created by Belkin International, Software Engineering on Nov 21, 2013
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
#ifdef PRODUCT_WeMo_LEDLight

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <pthread.h>

#include "types.h"
#include "defines.h"
#include "mxml.h"
#include "sigGen.h"
#include "logger.h"
#include "belkin_api.h"
#include "utils.h"
#include "aes_inc.h"
#include "httpsWrapper.h"
#include "controlledevice.h"
#include "natClient.h"
#include "remoteAccess.h"
#include "subdevice.h"
#include "rule.h"

#include "bridge_remote_rule.h"
#include "bridge_away_rule.h"
#include "logger.h"

extern char g_szFriendlyName[SIZE_256B];
extern char g_szPluginCloudId[SIZE_16B];
extern char g_szWiFiMacAddress[SIZE_64B];

extern char gIcePrevTransactionId[PLUGIN_MAX_REQUESTS][TRANSACTION_ID_LEN];
extern int gIceRunning;
extern int gDataInProgress;

//-----------------------------------------------------------------------------
int BR_RemoteUpdateCalenarList(void *hndl, char *xmlBuf, unsigned int buflen, void* data_sock, void* remoteInfo, char* transaction_id)
{
    char    szRespData[MAX_RESP_LEN];
    int     retVal = PLUGIN_FAILURE;
    bool    result=0;

    if (!xmlBuf)
        return retVal;

    if (buflen)
        CHECK_PREV_TSX_ID(E_UPDATEWEEKLYCALENDAR, transaction_id, retVal);

    memset(szRespData, 0x00, sizeof(szRespData));
    snprintf(szRespData, sizeof(szRespData),
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                "<plugin><recipientId>%s</recipientId><macAddress>%s</macAddress><status>%d</status><friendlyName>%s</friendlyName></plugin>\n",
                g_szPluginCloudId, g_szWiFiMacAddress, result, g_szFriendlyName);


    retVal = SendNatPkt(hndl, szRespData, remoteInfo, transaction_id, data_sock, E_UPDATEWEEKLYCALENDAR);

    /* Schedule Rules engine reload */
    gRestartRuleEngine = RULE_ENGINE_SCHEDULED;
handler_exit1:

    return retVal;
}
//-----------------------------------------------------------------------------
int BR_RemoteUpdateSimulatedRuleData(void *hndl, char *xmlBuf, unsigned int buflen, void* data_sock, void* remoteInfo, char* transaction_id)
{
    int     retVal = PLUGIN_FAILURE;
    char    szRespData[MAX_RESP_LEN];
    bool    result;
    if (!xmlBuf)
        return retVal;

    if (buflen)
        CHECK_PREV_TSX_ID(E_SETSIMULATEDRULEDATA, transaction_id, retVal);

    memset(szRespData, 0x00, sizeof(szRespData));
    result = BA_RemoteUpdateSimulatedRuleData(xmlBuf);
	// SNS based call
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "RemoteUpdateSimulatedRuleData()result [%d]", result);

    snprintf(szRespData, sizeof(szRespData),
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                "<plugin><recipientId>%s</recipientId><macAddress>%s</macAddress><status>%d</status><friendlyName>%s</friendlyName></plugin>\n",
                g_szPluginCloudId, g_szWiFiMacAddress, result, g_szFriendlyName);

    retVal = SendNatPkt(hndl, szRespData, remoteInfo, transaction_id, data_sock, E_SETSIMULATEDRULEDATA);

    /* Schedule Rules engine reload */
    gRestartRuleEngine = RULE_ENGINE_SCHEDULED;

handler_exit1:

    return retVal;
}
//-----------------------------------------------------------------------------
#endif


