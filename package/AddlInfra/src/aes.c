/***************************************************************************
*
*
* aes.c
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "aes_inc.h"
#include "logger.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

/**
 * Create an 128 bit key and IV using the supplied key_data. salt can be added for taste.
 * Fills in the encryption and decryption ctx objects and returns 0 on success
 **/
char* base64Encode(unsigned char *str, int len)
{
        BIO *bio, *b64;
        BUF_MEM *buf;
        unsigned char *encStr;

        b64 = BIO_new(BIO_f_base64());
        bio = BIO_new(BIO_s_mem());
        b64 = BIO_push(b64, bio);
        BIO_write(b64, str, len);
        BIO_flush(b64);

        BIO_get_mem_ptr(b64, &buf);

        encStr = (unsigned char*)CALLOC(1, buf->length + 1);
        strncpy((char*)encStr, buf->data, buf->length);
        APP_LOG("AES",LOG_DEBUG, "%s\n",encStr);
        BIO_free_all(b64);
        return (char *)encStr;
}



int aes128_init(unsigned char *key_data, int key_data_len, unsigned char *salt, int saltlen, unsigned char *iv, EVP_CIPHER_CTX *e_ctx, EVP_CIPHER_CTX *d_ctx)
{
	int i = 0, nrounds = 1;
	unsigned char key[PASSWORD_KEY_LEN];
	unsigned char geniv[PASSWORD_KEY_LEN];

	/*
	 * Gen key & IV for AES 128 CBC mode. A MD5 digest is used to hash the supplied key material.
	 * nrounds is the number of times the we hash the material. More rounds are more secure but
	 * slower.
	 */

	memset(key, 0x0, PASSWORD_KEY_LEN);
        i = EVP_BytesToKey(EVP_aes_128_cbc(), EVP_md5(), salt, key_data, key_data_len, nrounds, key, geniv);
        if (i != 16) {
		APP_LOG("AES",LOG_DEBUG, "EVP_BytesToKey returned %d\n", i);
		return -1;
	}
	EVP_CIPHER_CTX_init(e_ctx);
	EVP_EncryptInit_ex(e_ctx, EVP_aes_128_cbc(), NULL, key, iv);
	EVP_CIPHER_CTX_init(d_ctx);
	EVP_DecryptInit_ex(d_ctx, EVP_aes_128_cbc(), NULL, key, iv);

	return 0;
}

/*
 * Decrypt *len bytes of ciphertext
 */
unsigned char *aes128_decrypt(EVP_CIPHER_CTX *e, unsigned char *ciphertext, int *len)
{
	/* plaintext will always be equal to or lesser than length of ciphertext*/
	int c_len = *len, p_len=0,f_len = 0;

	unsigned char *plaintext = NULL;
        plaintext = (unsigned char *)ZALLOC(c_len);

	if(NULL == plaintext) {
		APP_LOG("AES",LOG_DEBUG, "aes128_decrypt - malloc failed for %d bytes\n",c_len);
		*len = 0;
		return NULL;
	}

	if(!EVP_DecryptInit_ex(e, NULL, NULL, NULL, NULL))
	{
	    APP_LOG("AES",LOG_DEBUG, "ERROR in EVP_DecryptInit_ex \n");
	    *len = 0;
	    free(plaintext);
	    plaintext = NULL;
	    return NULL;
	}
	if(!EVP_DecryptUpdate(e, plaintext, &p_len, ciphertext, c_len))
	{
	    APP_LOG("AES",LOG_DEBUG, "ERROR in EVP_DecryptUpdate\n");
	    *len = 0;
	    free(plaintext);
	    plaintext = NULL;
	    return NULL;
	}


	/* This function verifies the padding and then discards it.  It will
	 return an error if the padding isn't what it expects, which means that
	 the data was malformed or you are decrypting it with the wrong key */
	if(!EVP_DecryptFinal_ex(e, plaintext+p_len, &f_len))
	{
	    APP_LOG("AES",LOG_DEBUG, "ERROR in EVP_DecryptFinal_ex\n");
	    *len = 0;
	    free(plaintext);
	    plaintext = NULL;
	    return NULL;
	}

	*len = p_len + f_len;
	return plaintext;
}

char *pluginSignatureDecrypt(unsigned char *key_data, int key_data_len, unsigned char *salt, int saltlen, 
				unsigned char *iv, int ivlen, unsigned char *cipherinput, int *iolen) 
{
	/* "opaque" encryption, decryption ctx structures that libcrypto uses to record
		 status of enc/dec operations */
	EVP_CIPHER_CTX en, de;
	char *plaintext;
	int len;

	/* gen key and iv. init the cipher ctx object */
	if (aes128_init(key_data, key_data_len, (unsigned char *)salt, saltlen, iv, &en, &de)) {
		APP_LOG("AES",LOG_DEBUG, "Couldn't initialize AES cipher\n");
		return NULL;
	}
	/* The enc/dec functions deal with binary data and not C strings. strlen() will 
		 return length of the string without counting the '\0' string marker. We always
		 pass in the marker byte to the encrypt/decrypt functions so that after decryption 
		 we end up with a legal C string 
	*/
	len = *iolen;
	plaintext = (char*)aes128_decrypt(&de, cipherinput, &len);
	*iolen = len;
	EVP_CIPHER_CTX_cleanup(&en);
	EVP_CIPHER_CTX_cleanup(&de);
	return plaintext;
}

char *pluginPasswordDecrypt(unsigned char *key_data, int key_data_len, unsigned char *salt, int saltlen, 
				unsigned char *iv, int ivlen, unsigned char *cipherinput, int *iolen) 
{
	/* "opaque" encryption, decryption ctx structures that libcrypto uses to record
		 status of enc/dec operations */
	EVP_CIPHER_CTX en, de;
	char *plaintext;
	int len;

	/* gen key and iv. init the cipher ctx object */
	if (aes128_init(key_data, key_data_len, (unsigned char *)salt, saltlen, iv, &en, &de)) {
		APP_LOG("AES",LOG_DEBUG, "Couldn't initialize AES cipher\n");
		return NULL;
	}
	/* The enc/dec functions deal with binary data and not C strings. strlen() will 
		 return length of the string without counting the '\0' string marker. We always
		 pass in the marker byte to the encrypt/decrypt functions so that after decryption 
		 we end up with a legal C string 
	*/
	len = *iolen;
	plaintext = (char*)aes128_decrypt(&de, cipherinput, &len);
	*iolen = len;
	EVP_CIPHER_CTX_cleanup(&en);
	EVP_CIPHER_CTX_cleanup(&de);
	return plaintext;
}

unsigned char *pluginAES128Encrypt(unsigned char *key_data, int key_data_len, unsigned char *salt, int saltlen, unsigned char *iv, int ivlen, char *input, int *iolen) {
	/* "opaque" encryption, decryption ctx structures that libcrypto uses to record
		 status of enc/dec operations */
	EVP_CIPHER_CTX en, de;
	unsigned char *ciphertext;
	int len;

	/* gen key and iv. init the cipher ctx object */
	if (aes128_init(key_data, key_data_len, (unsigned char *)salt, saltlen, iv, &en, &de)) {
		return NULL;
	}
	/* The enc/dec functions deal with binary data and not C strings. strlen() will 
		 return length of the string without counting the '\0' string marker. We always
		 pass in the marker byte to the encrypt/decrypt functions so that after decryption 
		 we end up with a legal C string 
	*/
	len = *iolen;
	ciphertext = aes_encrypt(&en, (unsigned char *)input, &len);
	*iolen = len;
	EVP_CIPHER_CTX_cleanup(&en);
	EVP_CIPHER_CTX_cleanup(&de);
	return ciphertext;
}

