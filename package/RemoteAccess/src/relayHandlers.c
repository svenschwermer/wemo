/***************************************************************************
*
*
* remoteHandlers.c
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
#include <curl/curl.h>
#include <curl/easy.h>

#include "types.h"
#include "defines.h"
#include "remoteAccess.h"
#include "mxml.h"
#include "logger.h"
#include "rule.h"

#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "utils.h"
#include "aes_inc.h"
#include "controlledevice.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

extern char g_szPluginPrivatekey[MAX_PKEY_LEN];
extern char g_szPluginCloudId[SIZE_16B];
extern char g_szWiFiMacAddress[SIZE_64B];

static int gEncDecEnabled = 1;
extern char *remotePktDecrypt(void *pkt, unsigned pkt_len, int *declen);

#define MAX_BBUF_LEN	1025
#define MAX_BBUF1_LEN	1050
#define BIN_START_LEN 12
#define BIN_END_LEN 10
#define PKT_HDR_SIZE	8

int remoteAccessBinDataHandle(void *pkt, int pkt_len, FILE *handleWrite)// save bin data in file
{
			int len=0;

     /*Writing data to file*/
     len = fwrite(pkt, 1, pkt_len, handleWrite);
	   APP_LOG("REMOTEACCESS", LOG_ERR, "bytes %d written to rule db file successfully", len);
		 return PLUGIN_SUCCESS;
}

int remoteAccessServiceHandlerI(void *relay, void *pkt, unsigned pkt_len, const void *peer_addr,
		unsigned addr_len, void* data_sock)
{
	int retVal = PLUGIN_SUCCESS;
	char *xmlBuf = NULL, *decPkt = NULL, *xmltBuf = NULL;
	mxml_node_t *tree = NULL;
	mxml_node_t *find_node = NULL;
	int declen = 0;

	APP_LOG("REMOTEACCESS", LOG_ERR, "-----Invoked Internal %d", pkt_len);
	if (pkt_len <= 0) {
		APP_LOG("REMOTEACCESS", LOG_ERR, "Packet Length received is %d", pkt_len);
		retVal = PLUGIN_FAILURE;
		goto handler_exit;
	}
	if (!pkt) {
		APP_LOG("REMOTEACCESS", LOG_ERR, "Packet received is NULL");
		retVal = PLUGIN_FAILURE;
		goto handler_exit;
	}
	
	if (gEncDecEnabled) {
		decPkt = remotePktDecrypt((void*)pkt, pkt_len, &declen);
		if (!decPkt) {
			retVal = PLUGIN_FAILURE;
			goto handler_exit;
		}
	}else {
		declen = pkt_len;
		decPkt = (char*)CALLOC(1, declen);
		strncpy(decPkt, (char *)pkt, declen);
		if (!decPkt) {
			retVal = PLUGIN_FAILURE;
			goto handler_exit;
		}
	}
	
	xmlBuf = (char*)CALLOC(1, declen);
	if (!xmlBuf) {
		retVal = PLUGIN_FAILURE;
		goto handler_exit;
	}
	strncpy(xmlBuf, (char*)decPkt, declen);
  APP_LOG("REMOTEACCESS", LOG_DEBUG, "Pkt Received is %s", xmlBuf);


	xmltBuf = (char*)CALLOC(1, (declen+SIZE_64B));
	if (!xmltBuf) {
		retVal = PLUGIN_FAILURE;
		goto handler_exit;
	}
	snprintf(xmltBuf, (declen+SIZE_64B), "%s", xmlBuf);
  APP_LOG("REMOTEACCESS", LOG_DEBUG, "Modified Pkt to be parsed is %s", xmltBuf);
	tree = mxmlLoadString(NULL, xmltBuf, MXML_OPAQUE_CALLBACK);
	if (tree) {
		if ((find_node = mxmlFindElement(tree, tree, "pluginDeviceStatus", NULL, NULL, MXML_DESCEND))) {
			retVal = PLUGIN_SUCCESS;
			mxmlDelete(find_node);
			if (retVal != PLUGIN_SUCCESS) {
				retVal = PLUGIN_FAILURE;
				goto handler_exit;
			}
		}else if ((find_node = mxmlFindElement(tree, tree, "pluginSetDeviceStatus", NULL, NULL, MXML_DESCEND))) {
			retVal = PLUGIN_SUCCESS;
			mxmlDelete(find_node);
			if (retVal != PLUGIN_SUCCESS) {
				retVal = PLUGIN_FAILURE;
				goto handler_exit;
			}

		}else if ((find_node = mxmlFindElement(tree, tree, "PluginGetDBFile", NULL, NULL, MXML_DESCEND))) {
			retVal = PLUGIN_SUCCESS;
			mxmlDelete(find_node);
			if (retVal != PLUGIN_SUCCESS) {
				retVal = PLUGIN_FAILURE;
				goto handler_exit;
			}
		}else if ((find_node = mxmlFindElement(tree, tree, "updateweeklycalendar", NULL, NULL, MXML_DESCEND))) {
  		APP_LOG("REMOTEACCESS", LOG_DEBUG, "node found with updateweeklycalender");
			retVal = PLUGIN_SUCCESS;
			mxmlDelete(find_node);
			if (retVal != PLUGIN_SUCCESS) {
				retVal = PLUGIN_FAILURE;
				goto handler_exit;
			}
		}else if ((find_node = mxmlFindElement(tree, tree, "setDbVersion", NULL, NULL, MXML_DESCEND))) {
  		APP_LOG("REMOTEACCESS", LOG_DEBUG, "node found with set setDbVersion");
			retVal = PLUGIN_SUCCESS;
			mxmlDelete(find_node);
			if (retVal != PLUGIN_SUCCESS) {
				retVal = PLUGIN_FAILURE;
				goto handler_exit;
			}
		}else if ((find_node = mxmlFindElement(tree, tree, "getPluginDetails", NULL, NULL, MXML_DESCEND))) {
  		APP_LOG("REMOTEACCESS", LOG_DEBUG, "node found with getPluginDetails");
			retVal = PLUGIN_SUCCESS;
			mxmlDelete(find_node);
			if (retVal != PLUGIN_SUCCESS) {
				retVal = PLUGIN_FAILURE;
				goto handler_exit;
			}
		}else if ((find_node = mxmlFindElement(tree, tree, "getDbVersion", NULL, NULL, MXML_DESCEND))) {
  		APP_LOG("REMOTEACCESS", LOG_DEBUG, "node found with getDbVersion");
			retVal = PLUGIN_SUCCESS;
			mxmlDelete(find_node);
			if (retVal != PLUGIN_SUCCESS) {
				retVal = PLUGIN_FAILURE;
				goto handler_exit;
			}
		}else{
  		APP_LOG("REMOTEACCESS", LOG_ERR, "No Valid XML request payload received %s", xmlBuf);
			retVal = PLUGIN_FAILURE;
			goto handler_exit;
		}
	}else {
  	APP_LOG("REMOTEACCESS", LOG_ERR, "No Valid XML TREE in request payload received NULL");
		retVal = PLUGIN_FAILURE;
	}
handler_exit:
	if (tree) mxmlDelete(tree);
	if (decPkt) free(decPkt);
	if (xmlBuf) free(xmlBuf);
	if (xmltBuf) free(xmltBuf);
	return retVal;
}

int remoteAccessServiceHandler(void *relay, void *pkt, unsigned pkt_len, const void *peer_addr,
		unsigned addr_len, void* data_sock)
{
	int retVal = PLUGIN_SUCCESS;
	char *pStart = NULL;
	static int bReadBinaryDataInProg = 0;
	static int bReadBinaryDataSock = 0;
	int not_found = 0;
	int status = PLUGIN_SUCCESS;
	static FILE *handleWrite = NULL;
	char writeLogsBuf[MAX_BUF_LEN];

	APP_LOG("REMOTEACCESS", LOG_ERR, "-----Invoked %d", pkt_len);
	if (pkt_len <= 0) {
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "Packet Length received is %d", pkt_len);
		memset(writeLogsBuf, 0x0, MAX_BUF_LEN);
		snprintf(writeLogsBuf, MAX_BUF_LEN, "Packet Length received is %d", pkt_len);
		retVal = PLUGIN_FAILURE;
		goto handler_exit;
	}
	if (!pkt) {
		memset(writeLogsBuf, 0x0, MAX_BUF_LEN);
		snprintf(writeLogsBuf, MAX_BUF_LEN, "Packet received is NULL");
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "Packet received is NULL");
		retVal = PLUGIN_FAILURE;
		goto handler_exit;
	}
	
	int dsock=PLUGIN_SUCCESS;
	char resp_data[SIZE_256B];
	if(NULL != data_sock)
		dsock = *(int *)data_sock;
	if(bReadBinaryDataInProg)
	{
			char *pktc = (char*)pkt + (pkt_len - BIN_END_LEN);
		
			not_found = memcmp(pktc, "binEndData", BIN_END_LEN);
			if(!not_found)
			{
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "binary last chunk of data received");
				bReadBinaryDataInProg = 0;
				bReadBinaryDataSock = 0;
				retVal = remoteAccessBinDataHandle(pkt, pkt_len-BIN_END_LEN, handleWrite);// save bin data in file from start to end - binEndData

				snprintf(resp_data, sizeof(resp_data), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><recipientId>%lu</recipientId><macAddress>%s</macAddress><status>1</status></plugin>\n", strtol(g_szPluginCloudId, NULL, 0), g_szWiFiMacAddress);
				retVal = UDS_SendClientDataResponse(resp_data, strlen(resp_data));
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "response sent to cloud:\n%s", resp_data);

				/*Closing File*/
				fclose(handleWrite);
				handleWrite = NULL;


			}else {
				if (bReadBinaryDataSock != dsock) {
					retVal = remoteAccessServiceHandlerI(relay, pkt, pkt_len, peer_addr, addr_len, data_sock);
					if (retVal != PLUGIN_SUCCESS) {
  					APP_LOG("REMOTEACCESS", LOG_DEBUG, "--more pkts received-- ");
						retVal = remoteAccessBinDataHandle(pkt, pkt_len, handleWrite);// save bin data in file
					}else {
  					APP_LOG("REMOTEACCESS", LOG_DEBUG, "--Looks to be an XML Request pkts received-- ");
  					APP_LOG("REMOTEACCESS", LOG_DEBUG, "--Go ahead and apply xml parsing logic instead of binary logic handling-- ");
						goto xml_flow;
					}
				}else {
  				APP_LOG("REMOTEACCESS", LOG_DEBUG, "--more pkts received-- ");
					retVal = remoteAccessBinDataHandle(pkt, pkt_len, handleWrite);// save bin data in file
				}
			}
	  /* Reload DB if it was scheduled */
	  if(RULE_ENGINE_SCHEDULED == gRestartRuleEngine)
	  {
	    	APP_LOG("REMOTEACCESS", LOG_DEBUG, "Restart rules engine now");
			gRestartRuleEngine = RULE_ENGINE_RELOAD;
		}

		goto handler_exit;
	}
	else
	{
		/*
		 * The change assumes that "binStartData" will be at the start of packet and 
		 * "binEndData" will always be at the end of frame. The assumption holds true
		 * on the basis that Cloud sends rules.db file as byte stream consisting of:
		 * "binStartData" + <rules.db> + "binEndData" without any header/trailer bytes
		 * in this request
		 */
		not_found = memcmp((char *)pkt, "binStartData", BIN_START_LEN);
		if(!not_found)
		{
			/* remove exisiting file and create new file*/
			char cmdbuf[SIZE_64B];
			memset(cmdbuf, 0x0, SIZE_64B);
			snprintf(cmdbuf, sizeof(cmdbuf), "rm -rf %s", RULE_DB_PATH);
			status = system(cmdbuf);
			if(status != PLUGIN_SUCCESS)
				APP_LOG("REMOTEACCESS", LOG_ERR, "failed to remove existing rule db file");

			handleWrite = fopen(RULE_DB_PATH, "ab");
			if(handleWrite == NULL) {
			    memset(writeLogsBuf, 0x0, MAX_BUF_LEN);
			    snprintf(writeLogsBuf, MAX_BUF_LEN, "Failed to open rules file for writing");
			    APP_LOG("REMOTEACCESS", LOG_ERR, "failed to open rules file for writing");
				return PLUGIN_FAILURE;
			}

			pStart = (char *)pkt;

			not_found = memcmp((char *)pkt + (pkt_len - BIN_END_LEN), "binEndData", BIN_END_LEN);
			if (!not_found) {
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "first binary pkt received with end");
				pStart = pStart+BIN_START_LEN;	
				retVal = remoteAccessBinDataHandle(pStart, pkt_len-(BIN_START_LEN+BIN_END_LEN), handleWrite);// save bin data in file
				bReadBinaryDataInProg = 0;
				bReadBinaryDataSock = 0;
				snprintf(resp_data, sizeof(resp_data), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><recipientId>%lu</recipientId><macAddress>%s</macAddress><status>1</status></plugin>\n", strtol(g_szPluginCloudId, NULL, 0), g_szWiFiMacAddress);
				retVal = UDS_SendClientDataResponse(resp_data, strlen(resp_data));
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "response sent to cloud:\n%s", resp_data);

				/*Closing File*/
				fclose(handleWrite);
				handleWrite = NULL;
	
				/* Reload DB if it was scheduled */
				if(RULE_ENGINE_SCHEDULED == gRestartRuleEngine)
				{
					APP_LOG("REMOTEACCESS", LOG_DEBUG, "Restart rules engine now");
					gRestartRuleEngine = RULE_ENGINE_RELOAD;
				}


			}else {
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "first binary pkt received without end");
				pStart = pStart+BIN_START_LEN;	
				retVal = remoteAccessBinDataHandle(pStart, pkt_len-BIN_START_LEN, handleWrite);// save bin data in file

				bReadBinaryDataInProg = 1;
				bReadBinaryDataSock = *(int *)data_sock;
			} 
			goto handler_exit;
		}
	}

xml_flow:
		retVal=remoteAccessXmlHandler( NULL,pkt,(pkt_len),NULL,NULL,data_sock);
handler_exit:
	return retVal;
}


int remoteSendEncryptPkt(int dsock, void *pkt, unsigned pkt_len)
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
	int ivlen = SIZE_16B;

#ifdef PRODUCT_WeMo_LEDLight
	int msg_type = 0xfeed;
	int nMsgType = 0;
	int nDataLen = 0;
	unsigned char *pSendData = NULL;
#endif
	APP_LOG("REMOTEACCESS", LOG_HIDE, "\n Key for plug-in device is %s", g_szPluginPrivatekey);
	memset(publickey, 0x0, MAX_PKEY_LEN);
	strncpy(publickey, g_szPluginPrivatekey, sizeof(publickey)-1);
	
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
	
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "Packet Length Received from remote is %d\n", pkt_len);

	plaintext = (char*)pkt;
	olen = pkt_len;

	ciphertext = pluginAESEncrypt(key_data, key_data_len, (unsigned char *)salt, saltlen, (unsigned char*)iv, ivlen, plaintext, &olen);
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "\n....after encrypt input length is %d\n", olen);
	ciphertext[olen] = '\0';

#ifdef PRODUCT_WeMo_LEDLight
     //Make a header that has a length(4byte) and message type(0xFEED) for handling the large packet in Cloud server side.
     pSendData = (unsigned char*)CALLOC(olen+PKT_HDR_SIZE+1, sizeof(unsigned char));
     if( pSendData )
     {
           nMsgType = htonl(msg_type);
           nDataLen = htonl(olen);
           
           memcpy(pSendData, &nMsgType, 4);
           memcpy(&pSendData[4], &nDataLen, 4);
           memcpy(&pSendData[PKT_HDR_SIZE], ciphertext, olen);

		retVal = UDS_SendClientDataResponse(pSendData, olen+PKT_HDR_SIZE);

		free(pSendData);
		pSendData = NULL;
	}
#else
	retVal = UDS_SendClientDataResponse(ciphertext, olen);
	if(retVal == PLUGIN_FAILURE)
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "\n....Encrypted bytes send failed:%d", olen);
#endif
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "\n....Encrypted bytes sent to remote %d\n", retVal);

	free(ciphertext);
	ciphertext = NULL;
	free(salt);
	salt = NULL;
	free(iv);
	iv = NULL;

	return retVal;
}

int SendTurnPkt(int dsock,char * statusResp)
{
    int retVal;
    if (gEncDecEnabled) {
	retVal = remoteSendEncryptPkt(dsock, statusResp, strlen(statusResp));
    }else {
	    retVal = UDS_SendClientDataResponse(statusResp, strlen(statusResp));
    }
    return retVal;
}
