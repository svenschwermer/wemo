/***************************************************************************
*
*
* aes_inc.h
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
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#define PASSWORD_KEY_LEN	128

char* base64Encode(unsigned char *str, int len);
int aes128_init(unsigned char *key_data, int key_data_len, unsigned char *salt, int saltlen, unsigned char *iv, EVP_CIPHER_CTX *e_ctx, 
		EVP_CIPHER_CTX *d_ctx);

unsigned char *aes128_decrypt(EVP_CIPHER_CTX *e, unsigned char *ciphertext, int *len);
unsigned char *pluginAES128Encrypt(unsigned char *key_data, int key_data_len, unsigned char *salt, int saltlen, unsigned char *iv, int ivlen, char *input, int *iolen);

char *pluginPasswordDecrypt(unsigned char *key_data, int key_data_len, unsigned char *salt, int saltlen, 
				unsigned char *iv, int ivlen, unsigned char *cipherinput, int *iolen);
unsigned char *aes_encrypt(EVP_CIPHER_CTX *e, unsigned char *plaintext, int *len);
char *pluginAESDecrypt(unsigned char *key_data, int key_data_len, unsigned char *salt, int saltlen, unsigned char *iv, int ivlen, unsigned char *cipherinput, int *iolen);
unsigned char *pluginAESEncrypt(unsigned char *key_data, int key_data_len, unsigned char *salt, int saltlen, unsigned char *iv, int ivlen, char *input, int *iolen);
char *pluginSignatureDecrypt(unsigned char *key_data, int key_data_len, unsigned char *salt, int saltlen, 
				unsigned char *iv, int ivlen, unsigned char *cipherinput, int *iolen);
