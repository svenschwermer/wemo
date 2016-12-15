/***************************************************************************
*
*
* iceHandlers.c
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
#include <stdio.h>
#include <sys/stat.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "types.h"
#include "defines.h"
#include "natClient.h"
#include "remoteAccess.h"
#include "mxml.h"
#include "logger.h"

#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "gpio.h"
#include "cloudIF.h"
#include "rule.h"
#include "utils.h"
#include "aes_inc.h"
#include "natClient.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

int gDataInProgress;
int gIceRunning = 0;
extern char g_szPluginPrivatekey[MAX_PKEY_LEN];
char gIcePrevTransactionId[PLUGIN_MAX_REQUESTS][TRANSACTION_ID_LEN];

static int gEncDecEnabled = 1;
int remoteIceSendEncryptPkt(void *hndl, void *pkt, unsigned pkt_len, void *remoteInfo, char *transaction_id);


int remoteIceSendEncryptPkt(void *hndl, void *pkt, unsigned pkt_len, void * remoteInfo,  char *transaction_id) 
{
    char publickey[MAX_PKEY_LEN];
    unsigned char *key_data;
    int key_data_len;
    unsigned char *ciphertext;
    int olen, retVal = PLUGIN_FAILURE;
    char *plaintext;
    char *salt = NULL;
    unsigned char *iv;
    int saltlen = MAX_OCT_LEN;
    int ivlen = MAX_RVAL_LEN;
    char *rData = NULL, *sData=NULL;

    APP_LOG("REMOTEACCESS", LOG_HIDE, "\n Key for plug-in device is %s", g_szPluginPrivatekey);

    memset(publickey, 0x0, MAX_PKEY_LEN);
    strcpy(publickey, g_szPluginPrivatekey);

    key_data = (unsigned char *)publickey;
    key_data_len = strlen((char *)key_data);
    key_data[key_data_len] = '\0';

    salt = (char*)CALLOC(1, saltlen);
    strncpy(salt, (char *)key_data, saltlen);
    salt[saltlen] = '\0';

    iv = (unsigned char*)CALLOC(1, ivlen);
    memcpy(iv, key_data, ivlen);
    iv[ivlen] = '\0';

    APP_LOG("REMOTEACCESS", LOG_HIDE, "Key is %s\n", (char*)key_data);
    APP_LOG("REMOTEACCESS", LOG_HIDE, "Key data length 1 is %d\n", key_data_len);
    APP_LOG("REMOTEACCESS", LOG_HIDE, "Raw Salt is %s - length %d\n", (char*)salt, saltlen);
    APP_LOG("REMOTEACCESS", LOG_HIDE, "Raw IV is %s - length %d\n", (char*)iv, ivlen);

    APP_LOG("REMOTEACCESS", LOG_DEBUG, "Packet Length to be sent to remote is %d\n", pkt_len);

    plaintext = (char*)pkt;
    olen = pkt_len;

    ciphertext = pluginAESEncrypt(key_data, key_data_len, (unsigned char *)salt, saltlen, (unsigned char*)iv, ivlen, plaintext, &olen);
    APP_LOG("REMOTEACCESS", LOG_DEBUG, "\n....after encrypt input length is %d\n", olen);
    ciphertext[olen] = '\0';

    rData = ZALLOC(olen + strlen(transaction_id));
    strncpy(rData, transaction_id, strlen(transaction_id));
    sData = rData;
    rData = rData + (strlen(transaction_id));
    APP_LOG("ICE::", LOG_DEBUG, "\n....3 len tsx_id=%d Encrypted data along with transaction id sent to remote \n",strlen(transaction_id));
    memcpy(rData, ciphertext, olen);
    retVal = UDS_SendClientDataResponse(sData, (olen+strlen(transaction_id)));
    APP_LOG("ICE::", LOG_DEBUG, "\n....1 Encrypted data along with transaction id sent to remote data len with tsx id=%d\n", (olen+strlen(transaction_id)));
    APP_LOG("REMOTEACCESS", LOG_DEBUG, "\n....Encrypted bytes sent to remote %d\n", retVal);

    free(sData);
    free(ciphertext);
    sData=NULL;
    rData = NULL;
    ciphertext = NULL;
    free(salt);
    salt = NULL;
    free(iv);
    iv = NULL;
    return retVal;
}

int SendIcePkt(void *hndl,char* statusResp,void *remoteInfo,char *transaction_id)
{
    int retVal;
    if (gEncDecEnabled) {
	retVal = remoteIceSendEncryptPkt(hndl, statusResp, strlen(statusResp), remoteInfo, transaction_id);
    }else {

	char *rData = NULL, *sData=NULL;
	rData = MALLOC(strlen(statusResp) + strlen(transaction_id)+1);
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "\n....rData sizeof is %d\n", sizeof(rData));
	strncpy(rData, transaction_id, strlen(transaction_id));
	APP_LOG("ICE::", LOG_DEBUG, "\n....001 Encrypted data along with transaction id sent to remote %s\n", rData);
	sData = rData;
	rData = rData + (strlen(transaction_id));
	APP_LOG("ICE::", LOG_DEBUG, "\n....3 len tsx_id=%d Encrypted data along with transaction id sent to remote \n",strlen(transaction_id));
	memcpy(rData, statusResp, strlen(statusResp));
	APP_LOG("ICE::", LOG_DEBUG, "\n....4 plain data along with transaction id sent to remote \n" );
	rData = rData - strlen(transaction_id);
	APP_LOG("ICE::", LOG_DEBUG, "\n....5 plain data without transaction id sent to remote \n" );
	APP_LOG("ICE::", LOG_DEBUG, "\n....1 plain data along with transaction id sent to remote %s\n", rData);
	retVal = UDS_SendClientDataResponse(sData, (strlen(transaction_id)+strlen(statusResp)));
	free(rData);
	rData = NULL;
	sData = NULL;
    }
    return retVal;
}
