/***************************************************************************
*
*
* sigGen.c
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
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <sys/time.h>
#include <syslog.h>
#include <pthread.h>

#include "sigGen.h"
#include "logger.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

int baseEncode(unsigned char *str, unsigned char *encStr)
{
	BIO *bio, *b64;
	BUF_MEM *buf;

	b64 = BIO_new(BIO_f_base64());
	bio = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bio);
	BIO_write(b64, str, SIZE_20B);
	BIO_flush(b64);

	BIO_get_mem_ptr(b64, &buf);

	strncpy((char*)encStr, buf->data, buf->length);
	BIO_free_all(b64);
	return 0;
}

int calculate_hmac(const char* buf, size_t len, const char* key, size_t key_len, unsigned char* integrity)
{
	HMAC_CTX ctx;
	unsigned int md_len = 20;

	/* MESSAGE-INTEGRITY uses HMAC-SHA1 */
	HMAC_CTX_init(&ctx);
	HMAC_Init(&ctx, key, key_len, EVP_sha1());
	HMAC_Update(&ctx, (const unsigned char *)buf, len);
	HMAC_Final(&ctx, integrity, &md_len); /* HMAC-SHA1 is 20 bytes length */


	HMAC_CTX_cleanup(&ctx);

	return 0;
}

authSign* createAuthSignature(char* mac, char* serial, char* key)
{
	char spice[SIZE_256B], encStr[SIZE_256B], buf[SIZE_64B];
	int expires;
	char buffer[SIZE_256B];
	unsigned char integrity[SIZE_256B] = {'\0',};
	unsigned char nintegrity[SIZE_256B] = {'\0',};
	authSign *asign = NULL;
	
	memset(integrity, 0, SIZE_256B);
	memset(nintegrity, 0, SIZE_256B);

	asign = (authSign*)ZALLOC(sizeof(authSign));
	
	strncpy(asign->mac, mac, sizeof(asign->mac)-1);
	strncpy(asign->serial, serial, sizeof(asign->serial)-1);

	memset(spice, 0x0, SIZE_256B);
	strncpy(spice, mac, sizeof(spice)-1);
	strncat(spice, "\n", sizeof(spice)-strlen(spice)-1);
	strncat(spice, serial, sizeof(spice)-strlen(spice)-1);
	strncat(spice, "\n", sizeof(spice)-strlen(spice)-1);

	expires = (int)GetUTCTime();
	APP_LOG("INFRAS", LOG_DEBUG, "Expires before %d", expires);
	expires += 500;
	APP_LOG("INFRAS", LOG_DEBUG, "Expires after 500 %d", expires);
	memset(buf, 0, SIZE_64B);
	snprintf(buf, sizeof(buf), "%d", expires );
	strncat(spice, buf, sizeof(spice)-strlen(spice)-1); 
	
	asign->expiry = expires;
 	calculate_hmac(spice, strlen(spice), key, strlen(key), integrity);
	
	baseEncode(integrity, nintegrity); 
	memset(buffer, 0, SIZE_256B);
	strncpy((char*)buffer, (char*)nintegrity, sizeof(buffer)-1);
	buffer[strlen(buffer) -1] = '\0';

	strncpy((char*)asign->ssign, (char*)buffer, sizeof(asign->ssign)-1);

	memset(encStr, 0, SIZE_256B);
	strncpy(encStr, mac, sizeof(encStr)-1); 
	strncat(encStr, ":", sizeof(encStr)-strlen(encStr)-1); 
	strncat(encStr, buffer, sizeof(encStr)-strlen(encStr)-1); 
	strncat(encStr, ":", sizeof(encStr)-strlen(encStr)-1); 
	snprintf(buf, sizeof(buf), "%d", expires );
	strncat(encStr, buf, sizeof(encStr)-strlen(encStr)-1); 

	strncpy((char*)asign->signature, (char*)encStr, sizeof(asign->signature)-1);
	return asign;
}

sduauthSign* createSDUAuthSignature(char* udid, char* key)
{
	char spice[SIZE_256B], encStr[SIZE_256B], buf[SIZE_64B];
	int expires;
	char buffer[SIZE_256B];
	unsigned char integrity[SIZE_256B] = {'\0',};
	unsigned char nintegrity[SIZE_256B] = {'\0',};
	sduauthSign *asign = NULL;

	asign = (sduauthSign*)ZALLOC(sizeof(sduauthSign));
	
	strncpy(asign->udid, udid, sizeof(asign->udid)-1);

	memset(spice, 0x0, SIZE_256B);
	strncpy(spice, udid, sizeof(spice)-1);
	strncat(spice, "\n", sizeof(spice)-strlen(spice)-1);
	strncat(spice, "\n", sizeof(spice)-strlen(spice)-1);

	expires = (int)GetUTCTime();
	APP_LOG("INFRAS", LOG_DEBUG, "Expires before %d", expires);
	expires += 500;
	APP_LOG("INFRAS", LOG_DEBUG, "Expires after 500 %d", expires);
	memset(buf, 0, SIZE_64B);
	snprintf(buf, sizeof(buf), "%d", expires );
	strncat(spice, buf, sizeof(spice)-strlen(spice)-1); 
	
	asign->expiry = expires;
 	calculate_hmac(spice, strlen(spice), key, strlen(key), integrity);
	
	baseEncode(integrity, nintegrity); 
	memset(buffer, 0, SIZE_256B);
	strncpy((char*)buffer, (char*)nintegrity, sizeof(buffer)-1);
	buffer[strlen(buffer) -1] = '\0';

	strncpy((char*)asign->ssign, (char*)buffer, sizeof(asign->ssign)-1);

	memset(encStr, 0, sizeof(encStr));
	strncpy(encStr, "SDU ", sizeof(encStr)-1); 
	strncat(encStr, udid, sizeof(encStr)-strlen(encStr)-1); 
	strncat(encStr, ":", sizeof(encStr)-strlen(encStr)-1); 
	strncat(encStr, buffer, sizeof(encStr)-strlen(encStr)-1); 
	strncat(encStr, ":", sizeof(encStr)-strlen(encStr)-1); 
	snprintf(buf, sizeof(buf), "%d", expires );
	strncat(encStr, buf, sizeof(encStr)-strlen(encStr)-1); 

	strncpy((char*)asign->signature, (char*)encStr, sizeof(asign->signature)-1);
	return asign;
}

char* encryptStringHmacSha(char* str, char* key){
	
	unsigned char integrity[SIZE_256B] = {'\0',};
	unsigned char nintegrity[SIZE_256B] = {'\0',};
	char *encStr;

 	calculate_hmac(str, strlen(str), key, strlen(key), integrity);
	baseEncode(integrity, nintegrity); 

  	encStr = STRDUP(nintegrity);

	return encStr;
}

authSign* createAuthSignatureNoExp(char* mac, char* serial, char* key)
{
	char spice[SIZE_256B], encStr[SIZE_256B], buf[SIZE_64B];
	char buffer[SIZE_256B];
	unsigned char integrity[SIZE_256B] = {'\0',};
	unsigned char nintegrity[SIZE_256B] = {'\0',};
	authSign *asign = NULL;

	asign = (authSign*)MALLOC(sizeof(authSign));
	
	strncpy(asign->mac, mac, sizeof(asign->mac)-1);
	strncpy(asign->serial, serial, sizeof(asign->serial)-1);

	memset(spice, 0x0, SIZE_256B);
	strncpy(spice, mac, sizeof(spice)-1);
	strncat(spice, "\n", sizeof(spice)-strlen(spice)-1);
	strncat(spice, serial, sizeof(spice)-strlen(spice)-1);
	
	memset(buf, 0, SIZE_64B);
 	calculate_hmac(spice, strlen(spice), key, strlen(key), integrity);
	
	baseEncode(integrity, nintegrity); 
	memset(buffer, 0, SIZE_256B);
	strncpy((char*)buffer, (char*)nintegrity, sizeof(buffer)-1);
	buffer[strlen(buffer) -1] = '\0';

	strncpy((char*)asign->ssign, (char*)buffer, sizeof(asign->ssign)-1);

	memset(encStr, 0, SIZE_256B);
	strncpy(encStr, mac, sizeof(encStr)-1); 
	strncat(encStr, ":", sizeof(encStr)-strlen(encStr)-1); 
	strncat(encStr, buffer, sizeof(encStr)-strlen(encStr)-1); 
	strncat(encStr, buf, sizeof(encStr)-strlen(encStr)-1); 

	strncpy((char*)asign->signature, (char*)encStr, sizeof(asign->signature)-1);
	return asign;
}
