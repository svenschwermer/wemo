/***************************************************************************
*
*
* httpsWrapper.c
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
/******************************************************************************/
/*                                                                            */
/* Title: httpsWrapper.c                                                      */
/* Author: Amit                                                               */
/*                                                                            */
/* This file provides library implementation for REST based http client       */
/* 									      */
/******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include <upnp/LinkedList.h>
#include <httpsWrapper.h>
#include <osUtils.h>
#include "utils.h"
#include "logger.h"
#include "ulog.h"

// ### Multi part URI params Implementation to be moved to cloud, done for POC ###
#include <sigGen.h>
#include <urlEncode.h>
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

#define amzURL "http://s3.amazonaws.com" 
#define amzContentType "application/xml" 
#define amzKey "Ppx0WTdb8vFKZNBG9k+tfD3Fu9Z5S+Ysxi5kC6xu" // Pran Acc
#define amzAccessId "AKIAJVCEABXXHF3EKLPA" // Pran Acc
#define amzBucket "multipartbucket1"
#define amzResourcePrefix "MyData"
#define amzAlgorithm "HmacSHA1"
#define	MAC_ADDRESS "944452c80e10"
//#define  CURLDEBUG  1/**Add  for Curl Buffer Debugging*/
// Bucket Counter 
int gCounter = 1;
int gLastAuthVal = 0;
int gStopDownloadFW = 0;
// ### end Multipart impementation ###

// List to maintain all web sessions 
LinkedList  gWebSessionList;
// default start session id
int gSessionId = 32456; 
//Lock for web App external exposed functions
pthread_mutex_t gwebAppDataLock;
//Lock for Parallel Upload  
pthread_mutex_t gwebAppUploadLock;

static int webAppAgainDoPut( UserAppSessionData *pSsnData, UserAppData *pUsrAppData, char *redir_url);
static int webAppSetSSLOptions(CURL *curl);
char uploadId[SIZE_128B], eTag[SIZE_128B], encSign[SIZE_64B], eTagPart[100][SIZE_128B];
int nParts = 0, gThreadCount = 0, gUploadError;


int webAppPostTransactionInfoPutOnly( WebSessionListNode *pWebSsnListNode );
int webAppSendDataMultiPartAmazonUpload( UserAppSessionData *pSsnData, UserAppData *pUsrAppData);
int webAppSendDataMultiPartAmazonComplete1Step( UserAppSessionData *pSsnData, UserAppData *pUsrAppData, char* fileUri, char* uploadId, UserAppData *pAppData);
int webAppSendDataMultiPartAmazonUpload1Step( UserAppSessionData *pSsnData, UserAppData *pUsrAppData);
/****************************************************************************
 * Function: webAppInit
 *
 *    Initializes the module.
 *      
 *  Parameters:
 *    flag - placeholder
 *  Returns:
 *    success : 0, non-zero : failure.
 *****************************************************************************/
int webAppInit(int flag){
	int retVal = 0;

	retVal += ListInit( &gWebSessionList, NULL, NULL );
	retVal = osUtilsCreateLock(&gwebAppDataLock);
	retVal = osUtilsCreateLock(&gwebAppUploadLock);
	curl_global_init(CURL_GLOBAL_ALL);

	return retVal;
}

// For POC purpose to be removed
/****************************************************************************
 * Function: createUriParams
 *
 *    Creates URI params for multi part upload 
 *      
 *  Parameters:
 *  Returns:
 *    success : 0, non-zero : failure.
 *****************************************************************************/
char* createUriParams(char *httpMethod, char *path, char *date, char *contentType)
{
	char stringToSign[SIZE_256B], buf[SIZE_256B], *url, *baseSign, *encSsign;
	int expires; 

	expires = time(NULL);
	snprintf( stringToSign, sizeof(stringToSign), "%s\n\n%s\n%s\n/%s/%s", httpMethod, contentType, date, amzBucket, path);

	encSsign = encryptStringHmacSha( stringToSign, amzKey );
	strncpy(encSign, encSsign, sizeof(encSign)-1);	
	baseSign = urlEncode(encSsign);

	snprintf( buf, sizeof(buf), "%s/%s/%s",amzURL, amzBucket, path );

	url = STRDUP(buf);

	free(encSsign);
	free(baseSign);
	return url;
}

/****************************************************************************
 *   Function: read_file_callback
 *
 *    Callback function registered with CURLOPT_READFUNCTION member of CURL.
 *      
 *  Parameters:
 *    ptr   - pointer to data used by client to send to server
 *    size  - Size of data.
 *    nmemb - number of members.
 *    userp - Data given by application.
 *  Returns:
 *    size of data read.
 *****************************************************************************/
static size_t read_file_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	size_t retcode;

	if(size*nmemb < 1){
		return 0;
	}

	/*  fread() is what the library does internally as well, try without callback TODO  */
	retcode = fread(ptr, size, nmemb, userp);

	return retcode;

}

/****************************************************************************
 *   Function: read_transaction_callback
 *
 *    Callback function registered with CURLOPT_READFUNCTION member of CURL.
 *      
 *  Parameters:
 *    ptr   - pointer to data used by client to send to server
 *    size  - Size of data.
 *    nmemb - number of members.
 *    userp - Data given by application.
 *  Returns:
 *    size of data read.
 *****************************************************************************/
static size_t read_transaction_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	TransactionInfo *pData = (TransactionInfo *)userp;
	int static count = 0;

	if(size*nmemb < 1){
		return 0;
	}

	if(pData->dataSize) {
		*(char *)ptr = pData->data[count++]; /* copy one single byte */
		pData->dataSize--;                /* less data left */
		return 1;                        /* we return 1 byte at a time! */
	}

	count = 0;
	return 0;                          /* no more data left to deliver */
}

/****************************************************************************
 *   Function: read_callback
 *
 *    Callback function registered with CURLOPT_READFUNCTION member of CURL.
 *      
 *  Parameters:
 *    ptr   - pointer to data used by client to send to server
 *    size  - Size of data.
 *    nmemb - number of members.
 *    userp - Data given by application.
 *  Returns:
 *    size of data read.
 *****************************************************************************/
static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
	UserAppData *pUsrData = (UserAppData *)userp;
#ifdef  CURLDEBUG /**Add  for Curl Buffer Debugging*/
        static char buff[9000]=" ";
#endif
	if(size*nmemb < 1){
		return 0;
	}

	if(pUsrData->inDataLength) {
#ifdef  CURLDEBUG /**Add for Curl Buffer Debugging*/
                buff[pUsrData->inDataCount]=pUsrData->inData[pUsrData->inDataCount];
#endif
                *(char *)ptr = pUsrData->inData[pUsrData->inDataCount++]; /**This is added  thread access problem 400 and 500 bad request*/
		pUsrData->inDataLength--;                /* less data left */
		return 1;                        /* we return 1 byte at a time! */
	}
#ifdef  CURLDEBUG /**Add for Curl Buffer Debugging*/
        buff[pUsrData->inDataCount]='\0';
        printf("\n==========================================\n");
        printf("\nbuff=%s\n",buff);       
        printf("\n==========================================\n");
        memset(buff,0,sizeof(buff));
#endif
	return 0;                          /* no more data left to deliver */
}

/****************************************************************************
 *   Function: write_callback
 *
 *    Callback function registered with CURLOPT_WRITEFUNCTION member of CURL.
 *      
 *  Parameters:
 *    ptr   - pointer to data used by client to send to server
 *    size  - Size of data.
 *    nmemb - number of members.
 *    userp - Data given by application.
 *  Returns:
 *    size of data read.
 *****************************************************************************/
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	ListNode *pNode = NULL;
	WebSessionListNode *pWebSsnListNode = NULL;
	osUtilsGetLock(&gwebAppDataLock);
	pNode = ListHead( &gWebSessionList);
	char *tmp=NULL;
	while( pNode != NULL)
	{
		pWebSsnListNode = (WebSessionListNode *)(pNode->item); 
		if ( pWebSsnListNode->pUsrAppSsnData->sessionId == ((UserAppSessionData*)userdata)->sessionId )
			break;

		pNode = ListNext( &gWebSessionList, pNode);
	}
	osUtilsReleaseLock(&gwebAppDataLock);
	if( pNode == NULL){
		return 0;
	}

	if(*(char*)ptr == '\r' && *(char*)(ptr+1) == '\n'){
		return size * nmemb;
	}
	int len=pWebSsnListNode->pUsrAppData->outDataLength;
	pWebSsnListNode->pUsrAppData->outDataLength += size*nmemb;                /* less data left */
	tmp=REALLOC(pWebSsnListNode->pUsrAppData->outData,(pWebSsnListNode->pUsrAppData->outDataLength+1));
	
   if(!tmp){free(pWebSsnListNode->pUsrAppData->outData);pWebSsnListNode->pUsrAppData->outData=NULL;return 0;}
   pWebSsnListNode->pUsrAppData->outData=tmp;
	
        if (pWebSsnListNode->pUsrAppData->outData) {
		char *oData = pWebSsnListNode->pUsrAppData->outData;
		memcpy(oData+len, (char *)ptr, size*nmemb);
		pWebSsnListNode->pUsrAppData->outData[pWebSsnListNode->pUsrAppData->outDataLength] = '\0';
		return size * nmemb;
	}

	return 0;
}

/****************************************************************************
 *   Function: header_callback
 *
 *    Callback function registered with CURLOPT_HEADERFUNCTION member of CURL.
 *      
 *  Parameters:
 *    ptr   - pointer to data used by client to send to server
 *    size  - Size of data.
 *    nmemb - number of members.
 *    userp - Data given by application.
 *  Returns:
 *    size of data read.
 *****************************************************************************/
static size_t header_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	ListNode *pNode = NULL;
	WebSessionListNode *pWebSsnListNode = NULL;
	osUtilsGetLock(&gwebAppDataLock);
	pNode = ListHead( &gWebSessionList);
	while( pNode != NULL)
	{
		pWebSsnListNode = (WebSessionListNode *)(pNode->item); 
		if ( pWebSsnListNode->pUsrAppSsnData->sessionId == ((UserAppSessionData*)userdata)->sessionId )
			break;

		pNode = ListNext( &gWebSessionList, pNode);
	}
	osUtilsReleaseLock(&gwebAppDataLock);
	if( pNode == NULL){
		return 0;
	}

	if(*(char*)ptr == '\r' && *(char*)(ptr+1) == '\n'){
		return size * nmemb;
	}

	strncat((char*)pWebSsnListNode->pUsrAppData->outHeader, (char *)ptr, sizeof(pWebSsnListNode->pUsrAppData->outHeader)-strlen(pWebSsnListNode->pUsrAppData->outHeader)-1);
	pWebSsnListNode->pUsrAppData->outHeaderLength += size*nmemb;                /* less data left */

	return size * nmemb;
}

/****************************************************************************
 *   Function: parseHeader
 *
 *    parse Header for the string provided and return the corredsponding value
 *      
 *  Parameters:
 *    pListNode - pointer to WebSessionListNode
 *    url - redirect url 
 *  Returns:
 *    1 if redirects else 0 .
 *****************************************************************************/
char* parseHeader( char *pSrch, char *str, char *value)
{
	return NULL;
}

/****************************************************************************
 *   Function: checkRedirect307
 *
 *    check if there is redirect received in the header response by server
 *      
 *  Parameters:
 *    pListNode - pointer to WebSessionListNode
 *    url - redirect url 
 *  Returns:
 *    1 if redirects else 0 .
 *****************************************************************************/
int checkRedirect307( WebSessionListNode *pListNode, char *url)
{
	int j, flag = 0, i = 0;
	char buf[SIZE_256B];

	for (j=0; j< pListNode->pUsrAppData->outHeaderLength; j++){
		if ( pListNode->pUsrAppData->outData[j] == '3' &&  pListNode->pUsrAppData->outData[j+1] == '0' && pListNode->pUsrAppData->outData[j+2] == '7'){
			flag = 1;
			break;
		}
	}

	if(flag){
		for (j=0; pListNode->pUsrAppData->outHeaderLength; j++){
			if ( pListNode->pUsrAppData->outHeader[j] == 'l' && pListNode->pUsrAppData->outHeader[j+1] == 'o' && pListNode->pUsrAppData->outHeader[j+2] == 'c' && pListNode->pUsrAppData->outHeader[j+3] == 'a' && pListNode->pUsrAppData->outHeader[j+4]== 't' && pListNode->pUsrAppData->outHeader[j+5]== 'i' && pListNode->pUsrAppData->outHeader[j+6]== 'o' && pListNode->pUsrAppData->outHeader[j+7]== 'n'){
				for( j=j+10; pListNode->pUsrAppData->outHeader[j] != '\n'; j++){
					buf[i++] = pListNode->pUsrAppData->outHeader[j];
				}
				break; 
			}	
		}
	}

	strncpy(url, buf, strlen(buf)); 

	return flag;
}

/****************************************************************************
 *  Function: webAppErrorHandling 
 *
 *    Function for error handling  
 *      
 *  Parameters:
 *    curl - Curl object
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppErrorHandling_Reg(CURL *curl, CURLcode res, int disableFlag) {
	int numRetries = 3, retrySleep = 1;

	APP_LOG("HTTPSWRAPPER",LOG_DEBUG, "Retry Details retrySleep %d numRetries %d res %d\n", retrySleep, numRetries, res);

	if (res == 0) {
	    APP_LOG("HTTPSWRAPPER",LOG_DEBUG, "Retry Details retrySleep %d numRetries %d res %d\n", retrySleep, numRetries, res);
	    return res;
	}

	do{
	    APP_LOG("HTTPSWRAPPER",LOG_DEBUG, "Retry Details retrySleep %d numRetries %d res %d\n", retrySleep, numRetries, res);
	    sleep(retrySleep);
	    numRetries--;
	    if (res != 0) {
		res = curl_easy_perform(curl);
	    }else break;
	    retrySleep *= 2;
	}while(numRetries);

	return res;	
}

int webAppErrorHandling(CURL *curl, CURLcode res, int disableFlag) {
    
    long response;

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
    switch(response) {
	case 403: /* authentication error */
	gLastAuthVal=1;
	break;
	case 200: /* authentication error */
	gLastAuthVal=0;
	break;
    }

  if (disableFlag == 2) {
		res = webAppErrorHandling_Reg(curl, res, disableFlag);
		return res;
	}
	return res;
	if( disableFlag == 1){
		return res; 
	}
	retryCode retry = RETRY_NO;
	int numRetries = 3, retrySleep = 1;


	do{
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);

		switch(response) {
			case 500: /* Internal Server Error */
			case 502: /* Bad Gateway */
			case 503: /* Service Unavailable */
			case 504: /* Gateway Timeout */
				retry = RETRY_HTTP;
				break;
			case 403: /* authentication error */
				retry = RETRY_NO;
				break;
		}

		switch(res){// custom error handling for internal libCurl error
			case CURLE_OPERATION_TIMEDOUT:
				retry = RETRY_TIMEOUT;
				break;
			case CURLE_COULDNT_RESOLVE_HOST:
				retry = RETRY_TIMEOUT;
				break;
			default:
				break;
		}

		if(retry) {
			sleep(retrySleep);
			numRetries--;
			res = curl_easy_perform(curl);

			retrySleep *= 2;
			retry = RETRY_NO;
		}
		else {
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
			return res;
		}

	}while(numRetries);

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
	return res;	
}

/****************************************************************************
 *  Function: webAppCreateSession
 *
 *    Creates a new session requested by the application 
 *      
 *  Parameters:
 *    flag   - Post, Put or Get option 
 *  Returns:
 *    Pointer to new session created under struct UserAppSessionData.
 *****************************************************************************/
UserAppSessionData* webAppCreateSession ( int flag )
{
	WebSessionListNode *pWebSsnListNode = NULL;

	pWebSsnListNode = (WebSessionListNode*)MALLOC(sizeof(WebSessionListNode));

	pWebSsnListNode->pUsrAppSsnData = (UserAppSessionData*)MALLOC(sizeof(UserAppSessionData));
	
	if( !(pWebSsnListNode->pUsrAppSsnData))
	{
		free(pWebSsnListNode);
		return NULL;
	}

	pWebSsnListNode->pUsrAppSsnData->curl = curl_easy_init();
	if( !(pWebSsnListNode->pUsrAppSsnData->curl))
	{
		free(pWebSsnListNode->pUsrAppSsnData);
		free(pWebSsnListNode);
	 	return NULL;
	}

	pWebSsnListNode->pUsrAppSsnData->sessionId = gSessionId++;

	osUtilsGetLock(&gwebAppDataLock);
	ListAddTail( &gWebSessionList, pWebSsnListNode);
	osUtilsReleaseLock(&gwebAppDataLock);

	return pWebSsnListNode->pUsrAppSsnData;
} 

/****************************************************************************
 *  Function: webAppSendDataPost 
 *
 *    Sends data to the server
 *      
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data 
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSendDataPost( UserAppSessionData *pSsnData, UserAppData *pUsrAppData)
{

	CURLcode res = CURLE_OK;
	ListNode *pNode = NULL;
	WebSessionListNode *pWebSsnListNode = NULL;
	int i;
	long response = 0;
	char buf[SIZE_1024B];
	struct curl_slist *slist = NULL;


	osUtilsGetLock(&gwebAppDataLock);
	pNode = ListHead( &gWebSessionList);
	while( pNode != NULL)
	{
		pWebSsnListNode = (WebSessionListNode *)(pNode->item); 
		if ( pWebSsnListNode->pUsrAppSsnData->sessionId == pSsnData->sessionId ){
			break;
		}
		pNode = ListNext( &gWebSessionList, pNode);
	}
	osUtilsReleaseLock(&gwebAppDataLock);

	if( pNode == NULL){
		return ERROR_INVALID_SESSION_ID;
	}
        pUsrAppData->inDataCount=0; /**This is added  for thread access problem 400 and 500 bad request*/
	pWebSsnListNode->pUsrAppData = pUsrAppData; 

	/* set the URL that is about to receive our POST. */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_URL, pWebSsnListNode->pUsrAppData->url);

	if( pWebSsnListNode->pUsrAppData->keyValLen){
		for( i=0; i<pWebSsnListNode->pUsrAppData->keyValLen; i++){	
			snprintf(buf, sizeof(buf), "%s: %s", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value);

			slist = curl_slist_append(slist, buf);
		}
	}
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HTTPHEADER, slist);

	/* specify we want to POST data */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_POST, 1L);

	/* Write and header callback */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEHEADER, pWebSsnListNode->pUsrAppSsnData);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEDATA, pWebSsnListNode->pUsrAppSsnData);
	/* verbose debug output option */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_VERBOSE, 1L);

	if (pWebSsnListNode->pUsrAppData->httpsFlag) {
		webAppSetSSLOptions(pWebSsnListNode->pUsrAppSsnData->curl);
	}

	if (pWebSsnListNode->pUsrAppData->inDataLength <= 0) {
	}else{
		/* use the data provided by user */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_callback);
		/* pointer to pass to our read function */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pWebSsnListNode->pUsrAppData);
		/* Set the expected POST size. */ 
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_POSTFIELDSIZE, (long)(pUsrAppData->inDataLength));
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
		res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
		curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
		pWebSsnListNode->pUsrAppData->outResp = response;
		pUsrAppData->nStatusCode = response;
		res = webAppErrorHandling(pWebSsnListNode->pUsrAppSsnData->curl, res, pUsrAppData->disableFlag); 
	}

	/* cleanup */
	curl_slist_free_all(slist);

	return res;
}

/****************************************************************************
 *  Function: webAppSendDataGet 
 *
 *    Fulfills applications request to get data as per the request provided by application 
 *      
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data 
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSendDataGet( UserAppSessionData *pSsnData, UserAppData *pUsrAppData )
{

	CURLcode res = CURLE_OK;
	ListNode *pNode = NULL;
	WebSessionListNode *pWebSsnListNode = NULL;
	int i;
	long response = 0;
	char buf[SIZE_1024B];

	struct curl_slist *slist = NULL;
	osUtilsGetLock(&gwebAppDataLock);
	pNode = ListHead( &gWebSessionList);
	while( pNode != NULL)
	{
		pWebSsnListNode = (WebSessionListNode *)(pNode->item); 
		if ( pWebSsnListNode->pUsrAppSsnData->sessionId == pSsnData->sessionId ){
			break;
		}

		pNode = ListNext( &gWebSessionList, pNode);
	}
	osUtilsReleaseLock(&gwebAppDataLock);
	if( pNode == NULL){
		return ERROR_INVALID_SESSION_ID;
	}

	pWebSsnListNode->pUsrAppData = pUsrAppData; 

	/* set the URL that is about to receive our POST. */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_URL, pWebSsnListNode->pUsrAppData->url);

	/* specify the CURLOPT_HTTPHEADER data */
	snprintf(buf, sizeof(buf), "GET %s HTTP/1.1\r\n", pWebSsnListNode->pUsrAppData->inData );//check for condition when inData not present
	slist = curl_slist_append(slist, buf);
	if( pWebSsnListNode->pUsrAppData->keyValLen){
		for( i=0; i<pWebSsnListNode->pUsrAppData->keyValLen; i++){	
			snprintf(buf, sizeof(buf), "%s: %s", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value); 
			slist = curl_slist_append(slist, buf);
		}
		snprintf(buf, sizeof(buf), "%s: %s\r\n", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value); 
		slist = curl_slist_append(slist, buf);
	}

	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HTTPHEADER, slist);

	if (pWebSsnListNode->pUsrAppData->httpsFlag) {
		webAppSetSSLOptions(pWebSsnListNode->pUsrAppSsnData->curl);
	}
	/* Write and header callback */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEHEADER, pWebSsnListNode->pUsrAppSsnData);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEDATA, pWebSsnListNode->pUsrAppSsnData);
	/* verbose debug output option */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */

	/* res will get the return code */
	res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
	curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
  pWebSsnListNode->pUsrAppData->outResp = response;
	pUsrAppData->nStatusCode = response;
	res = webAppErrorHandling(pWebSsnListNode->pUsrAppSsnData->curl, res, pUsrAppData->disableFlag); 

	/* cleanup */
	curl_slist_free_all(slist);

	return res;
}

/****************************************************************************
 *  Function: webAppSendDataPut 
 *
 *    Puts data to the server
 *      
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data 
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSendDataPut( UserAppSessionData *pSsnData, UserAppData *pUsrAppData)
{
	CURLcode res = CURLE_OK;
	ListNode *pNode = NULL;
	WebSessionListNode *pWebSsnListNode = NULL;
	char buf[SIZE_1024B], file[SIZE_256B];
	int i, fd;
	long response;
	struct stat fileInfo;
	FILE *pFp;
	int dataLength = 0;

	struct curl_slist *slist = NULL;
	osUtilsGetLock(&gwebAppDataLock);
	pNode = ListHead( &gWebSessionList);
	while( pNode != NULL)
	{
		pWebSsnListNode = (WebSessionListNode *)(pNode->item); 
		if ( pWebSsnListNode->pUsrAppSsnData->sessionId == pSsnData->sessionId ){
			break;
		}
		pNode = ListNext( &gWebSessionList, pNode);
	}
	osUtilsReleaseLock(&gwebAppDataLock);
	if( pNode == NULL){
		return ERROR_INVALID_SESSION_ID;
	}

	pWebSsnListNode->pUsrAppData = pUsrAppData; 

	/* verbose debug output option */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_VERBOSE, 1L);

	/* set the URL that is about to receive our POST. */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_URL, pWebSsnListNode->pUsrAppData->url);

	if( pWebSsnListNode->pUsrAppData->keyValLen){
		for( i=0; i<pWebSsnListNode->pUsrAppData->keyValLen; i++){	
			snprintf(buf, sizeof(buf), "%s: %s", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value);
			slist = curl_slist_append(slist, buf);
		}
	}

	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HTTPHEADER, slist);

	/* specify we want to PUT data */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_PUT, 1L);

	/* enable uploading */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_UPLOAD, 1L);

	/*set follow location*/
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_FOLLOWLOCATION, 0L);
	/* Write and header callback */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEHEADER, pWebSsnListNode->pUsrAppSsnData);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEDATA, pWebSsnListNode->pUsrAppSsnData);

	if (pWebSsnListNode->pUsrAppData->httpsFlag) {
		webAppSetSSLOptions(pWebSsnListNode->pUsrAppSsnData->curl);
	}
	if (pWebSsnListNode->pUsrAppData->inDataLength <= 0) {
		strncpy(file, pWebSsnListNode->pUsrAppData->inData, sizeof(file)-1);
		pFp = fopen(file, "rb");
		if ( pFp == NULL ) {
			return ERROR_INVALID_FILE;	 
		}else {
			/* use the data provided by user */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_file_callback);
			/* pointer to pass to our read function */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pFp);
			/* Set the file size */ 
			fd = open(file, O_RDONLY);
			fstat(fd, &fileInfo );
			close(fd);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)fileInfo.st_size);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_COOKIEFILE, "");/* Starting Cookie Engine */
			/* res will get the return code */
			res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
			curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
		  pWebSsnListNode->pUsrAppData->outResp = response;
      pUsrAppData->nStatusCode = response;

		  struct curl_slist *cookies =NULL;
			curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_COOKIELIST, &cookies);
      if(cookies)
			{
			memset(pUsrAppData->cookie_data,0,sizeof(char)*SIZE_512B);
			strncpy(pUsrAppData->cookie_data,cookies->data,sizeof(pUsrAppData->cookie_data) -1);
	    curl_slist_free_all(cookies);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_COOKIELIST, "SESS");
	}
			res = webAppErrorHandling(pWebSsnListNode->pUsrAppSsnData->curl, res, pUsrAppData->disableFlag); 
			fclose(pFp);
		}
	}else {
		dataLength = pWebSsnListNode->pUsrAppData->inDataLength;
		/* use the data provided by user */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_callback);
		/* pointer to pass to our read function */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pWebSsnListNode->pUsrAppData);
		/* res will get the return code */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)dataLength);
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */

		res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
	  curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
		pWebSsnListNode->pUsrAppData->outResp = response;
    pUsrAppData->nStatusCode = response;

		res = webAppErrorHandling(pWebSsnListNode->pUsrAppSsnData->curl, res, pUsrAppData->disableFlag); 
	}

	/* cleanup */
	curl_slist_free_all(slist);

	long respcode;
	curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &respcode); 
	if (res == CURLE_OK) { 
	} else{
	}
	if (respcode == 307) {
		char *new_url = NULL; 
		curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_REDIRECT_URL, &new_url); 
		if ((res == CURLE_OK) && (new_url)){ 
			pUsrAppData->inDataLength = dataLength;
			res = webAppAgainDoPut( pSsnData, pUsrAppData, new_url);
		} else{
		}
	}
	return res;
}

/****************************************************************************
 *  Function: webAppAgainDoPut 
 *
 *    Puts data to the server with redirect URL
 *      
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data 
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppAgainDoPut( UserAppSessionData *pSsnData, UserAppData *pUsrAppData, char *redir_url)
{
	CURLcode res = CURLE_OK;
	ListNode *pNode = NULL;
	WebSessionListNode *pWebSsnListNode = NULL;
	char buf[SIZE_1024B], file[SIZE_256B];
	int i, fd;
	long response = 0;
	struct stat fileInfo;
	FILE *pFp = NULL;
	int dataLength = 0;

	struct curl_slist *slist = NULL;

	pNode = ListHead( &gWebSessionList);
	while( pNode != NULL)
	{
		pWebSsnListNode = (WebSessionListNode *)(pNode->item); 
		if ( pWebSsnListNode->pUsrAppSsnData->sessionId == pSsnData->sessionId ){
			break;
		}
		pNode = ListNext( &gWebSessionList, pNode);
	}

	if( pNode == NULL){
		return ERROR_INVALID_SESSION_ID;
	}

	pWebSsnListNode->pUsrAppData = pUsrAppData; 

	/* verbose debug output option */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_VERBOSE, 1L);

	/* set the URL that is about to receive our POST. */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_URL, redir_url);

	if(pWebSsnListNode->pUsrAppData->keyValLen){
		for( i=0; i<pWebSsnListNode->pUsrAppData->keyValLen; i++){	
			if (!(strcmp(pWebSsnListNode->pUsrAppData->keyVal[i].key, "Authorization"))) {
				continue;
			}
			snprintf(buf, sizeof(buf), "%s: %s", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value);
			slist = curl_slist_append(slist, buf);
		}
	}

	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HTTPHEADER, slist);

	/* specify we want to PUT data */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_PUT, 1L);

	/* enable uploading */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_UPLOAD, 1L);

	/*set follow location*/
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_FOLLOWLOCATION, 0L);
	/* Write and header callback */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEHEADER, pWebSsnListNode->pUsrAppSsnData);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEDATA, pWebSsnListNode->pUsrAppSsnData);

	if (pWebSsnListNode->pUsrAppData->httpsFlag) {
		webAppSetSSLOptions(pWebSsnListNode->pUsrAppSsnData->curl);
	}
	if (pWebSsnListNode->pUsrAppData->inDataLength <= 0) {
		strncpy(file, pWebSsnListNode->pUsrAppData->inData, sizeof(file)-1);
		pFp = fopen(file, "rb");
		if ( pFp == NULL ) {
			curl_slist_free_all(slist);
			return ERROR_INVALID_FILE;	 
		}else {
			/* use the data provided by user */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_file_callback);
			/* pointer to pass to our read function */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pFp);
			/* Set the file size */ 
			fd = open(file, O_RDONLY);
			fstat(fd, &fileInfo );
			close(fd);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)fileInfo.st_size);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
			/* res will get the return code */
			res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
			curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
      pWebSsnListNode->pUsrAppData->outResp = response;
      pUsrAppData->nStatusCode = response;
			if((response >= 400) || (res == CURLE_OPERATION_TIMEDOUT))// response code returned by server or operation timed out 
				res = webAppErrorHandling(pWebSsnListNode->pUsrAppSsnData->curl, res, pUsrAppData->disableFlag); 
			fclose(pFp);
		}
	}else {
		dataLength = pWebSsnListNode->pUsrAppData->inDataLength;
		/* use the data provided by user */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_callback);
		/* pointer to pass to our read function */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pWebSsnListNode->pUsrAppData);
		/* res will get the return code */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)dataLength);
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
		res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
		pWebSsnListNode->pUsrAppData->inDataLength = dataLength;
		curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
		pWebSsnListNode->pUsrAppData->outResp = response;
		pUsrAppData->nStatusCode = response;
		res = webAppErrorHandling(pWebSsnListNode->pUsrAppSsnData->curl, res, pUsrAppData->disableFlag); 
	}

	curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response); 
  pWebSsnListNode->pUsrAppData->outResp = response;
  pUsrAppData->nStatusCode = response;
	if (res == CURLE_OK) { 
		if ( response == 200)
		{
			webAppPostTransactionInfoPutOnly( pWebSsnListNode);
		}

	} else{
	}
	/* cleanup */
	curl_slist_free_all(slist);
	return res;
}

/****************************************************************************
 *  Function: webAppSendDataDelete
 *
 *    Delete data from the server (HTTP DELETE Method)
 *
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSendDataDelete(UserAppSessionData *pSsnData, UserAppData *pUsrAppData)
{
	int i;
  int dataLength = 0;

	long response = 0;
	char buf[SIZE_1024B];

	CURLcode res = CURLE_OK;
	ListNode *pNode = NULL;
	WebSessionListNode *pWebSsnListNode = NULL;

	struct curl_slist *slist = NULL;

	osUtilsGetLock(&gwebAppDataLock);

	pNode = ListHead(&gWebSessionList);
	while( pNode != NULL )
	{
		pWebSsnListNode = (WebSessionListNode *)(pNode->item);
		if( pWebSsnListNode->pUsrAppSsnData->sessionId == pSsnData->sessionId )
		{
			break;
		}
		pNode = ListNext( &gWebSessionList, pNode);
	}

	osUtilsReleaseLock(&gwebAppDataLock);

	if( pNode == NULL )
	{
		return ERROR_INVALID_SESSION_ID;
	}

	pWebSsnListNode->pUsrAppData = pUsrAppData;

	/* set the URL that is about to receive our DELETE. */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_URL, pWebSsnListNode->pUsrAppData->url);

	if( pWebSsnListNode->pUsrAppData->keyValLen )
	{
		for( i = 0; i < pWebSsnListNode->pUsrAppData->keyValLen; i++ )
		{
			snprintf(buf, sizeof(buf), "%s: %s", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value);
			slist = curl_slist_append(slist, buf);
		}
	}

	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HTTPHEADER, slist);

	/* specify we want to DELETE data */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CUSTOMREQUEST, "DELETE");

	/* Write and header callback */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEHEADER, pWebSsnListNode->pUsrAppSsnData);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEDATA, pWebSsnListNode->pUsrAppSsnData);

	if( pWebSsnListNode->pUsrAppData->httpsFlag )
	{
		webAppSetSSLOptions(pWebSsnListNode->pUsrAppSsnData->curl);
	}

  APP_LOG("webAppSendDataDelete", LOG_DEBUG, "inDataLength = %d", pWebSsnListNode->pUsrAppData->inDataLength);

  if( pWebSsnListNode->pUsrAppData->inDataLength > 0 )
  {
    dataLength = pWebSsnListNode->pUsrAppData->inDataLength;

    curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_POSTFIELDS, pUsrAppData->inData);
    curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_POSTFIELDSIZE, (long)dataLength);
  }

  curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
  curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
  curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */

  res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);

  if( res == CURLE_OK )
  {
    APP_LOG("webAppSendDataDelete", LOG_DEBUG, "webAppSendDataDelete is OK");
  }
  else
  {
    APP_LOG("webAppSendDataDelete", LOG_DEBUG, "webAppSendDataDelete is Error");
  }

  curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
  pWebSsnListNode->pUsrAppData->outResp = response;
  pUsrAppData->nStatusCode = response;

  res = webAppErrorHandling(pWebSsnListNode->pUsrAppSsnData->curl, res, pUsrAppData->disableFlag);

	/* cleanup */
	curl_slist_free_all(slist);

	return res;
}

/****************************************************************************
 *  Function: webAppSplitFiles  ###BUFFER IMPLEMENTATION FOR DEMO AND FUTURE POSSIBLE USE###
 *
 *    Sends split data to Amazon
 *      
 *  Parameters:
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSplitFiles(  UserAppSessionData *pSsnData, UserAppData *pUsrAppData ) 
{

	CURLcode res = CURLE_OK;
	char buf[SIZE_256B];
	unsigned long lBufferSize = 0;
	off_t lSize;
	struct stat st;
	int partNumber, fd, i=0;
	char partFileName[17] = "", c;

	fd = open(pUsrAppData->inData, O_RDONLY);
	fstat(fd, &st);	
	lSize = st.st_size;

	lBufferSize = 0;
	partNumber = 1;

	snprintf(buf, sizeof(buf), "split -b 5242880 %s new", pUsrAppData->inData);
	system( buf);
	nParts = lSize/5242880 + 1;
	for (i=1, c = 97; i<= nParts; i++, c++){
		snprintf(buf, sizeof(buf), "newa%c", c);
		strncpy( partFileName, buf, sizeof(partFileName)-1);  
		strncpy( pUsrAppData->inData, partFileName, sizeof(pUsrAppData->inData)-1);
		pUsrAppData->partNumber = partNumber++;	
		res = webAppSendDataMultiPartAmazonUpload( pSsnData, pUsrAppData); 
		memset(pUsrAppData->outHeader, 0x0, SIZE_1024B);
	}

	close(fd);
	return res;
}

/****************************************************************************
 *  Function: webAppSplitFiles 
 *
 *    Sends split data to Amazon
 *      
 *  Parameters:
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSendDataMultiPartParallelAmazon1Step( UserAppSessionData *pSsnData, UserAppData *pUsrAppData)
{
	UserAppData *pUsrAppDataUpload[4], *pUsrAppDataComplete = NULL;
	UserAppSessionData *pSsnDataUpload[4], *pSsnDataComplete = NULL;
	char buf[SIZE_1024B], str1[SIZE_2B], *ptr, auth[4][SIZE_128B];
  	char cTime[SIZE_128B] = {}, fileUrl[SIZE_128B], uploadId[SIZE_128B], c;
	int i, threadCount = 0, res;
  long respcode = 0;
  pthread_attr_t PthreadAttr;

	unsigned long lBufferSize = 0;
	off_t lSize;
	struct stat st;
	int partNumber =0, fd, partSize;
	char partFileName[17] = "";

	strncpy( pUsrAppData->url, "http://173.196.160.173:8080/DuoService/duo/initiateMultipart", sizeof(pUsrAppData->url)-1);
	webAppSendDataGet( pSsnData, pUsrAppData );
	for (i=0; i<pUsrAppData->outHeaderLength && i<DATA_BUF_LEN; i++) {
	}

	for (i=0; i<pUsrAppData->outDataLength && i<DATA_BUF_LEN; i++) {
	}

	if( !pUsrAppData->outDataLength ){
		return ERROR_CLOUD_RESPONSE;
	}

	curl_easy_getinfo( pSsnData->curl, CURLINFO_RESPONSE_CODE, &respcode); 
	if(respcode >= 500) {
		return ERROR_CLOUD_RESPONSE;
	}

	ptr = strstr( pUsrAppData->outData, "fileUri");
	ptr += 8;
	sscanf(ptr, "%[^'<'] %[<]", fileUrl, str1);

	ptr = strstr( pUsrAppData->outData, "multipartUploadId");
	ptr += 18;
	sscanf(ptr, "%[^'<'] %[<]", uploadId, str1);

	ptr = strstr( pUsrAppData->outData, "expireDateString");
	ptr += 17;
	sscanf(ptr, "%[^'<'] %[<]", cTime, str1);

	ptr = strstr( pUsrAppData->outData, "<authHeader");
	for(i=0; i<4; i++){
		ptr += 12;
		sscanf(ptr, "%[^'<'] %[<]", auth[i], str1);
		ptr = strstr( ptr, "<authHeader");
	}

// ##### File Split ###### // Max 4 parts supported 
	fd = open(pUsrAppData->inData, O_RDONLY);
	fstat(fd, &st);	
	lSize = st.st_size;

	lBufferSize = 0;
	partNumber = 1;
	if (lSize/4 > 5242880)
		partSize = lSize/4 + 1;
	else
		partSize = 5242880;
	snprintf(buf, sizeof(buf), "split -b %d %s new", partSize, pUsrAppData->inData);
	system( buf);
	nParts = lSize/partSize;
	nParts += lSize%partSize>0?1:0; 

	close(fd);
// ##### File Split END ###### 
// ##### Loop through files ###### 
	for ( c = 96+partNumber; partNumber<=nParts; partNumber++, c++){

		snprintf(buf, sizeof(buf), "newa%c", c);
		strncpy( partFileName, buf, sizeof(partFileName)-1);  

		pUsrAppDataUpload[partNumber-1] = (UserAppData*)ZALLOC(sizeof(UserAppData));

		strncpy( pUsrAppDataUpload[partNumber-1]->inData, partFileName, sizeof(pUsrAppDataUpload[partNumber-1]->inData)-1);
		pUsrAppDataUpload[partNumber-1]->partNumber = partNumber;	

	snprintf( buf, sizeof(buf), "%s%s?partNumber=%d&uploadId=%s", amzURL, fileUrl, partNumber, uploadId);
	strncpy( pUsrAppDataUpload[partNumber-1]->url, buf, sizeof(pUsrAppDataUpload[partNumber-1]->url)-1);
	strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[0].key, "Date", sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[0].key)-1);   
	strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[0].value, cTime, sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[0].value)-1);
	strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[1].key, "Content-Type", sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[1].key)-1);   
	strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[1].value, "application/octet-stream", sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[1].value)-1);
	strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[2].key, "Authorization", sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[2].key)-1);   
	strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[2].value, auth[partNumber-1], sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[2].value)-1);
	pUsrAppDataUpload[partNumber-1]->keyValLen = 3;
	osUtilsReleaseLock(&gwebAppDataLock); 
	pSsnDataUpload[partNumber-1] = webAppCreateSession(0);
	osUtilsGetLock(&gwebAppDataLock);

// ##### File Split END ###### 
// ##### Threaded Implementation ###### 
	pthread_t lsthread;
	ListNode *pNode = NULL;
	WebSessionListNode *pWebSsnListNode = NULL;
	osUtilsGetLock(&gwebAppDataLock);
	pNode = ListHead( &gWebSessionList);
	while( pNode != NULL)
	{
		pWebSsnListNode = (WebSessionListNode *)(pNode->item); 
		if ( pWebSsnListNode->pUsrAppSsnData->sessionId == pSsnDataUpload[partNumber-1]->sessionId ){
			break;
		}
		pNode = ListNext( &gWebSessionList, pNode);
	}
	osUtilsReleaseLock(&gwebAppDataLock);
	if( pNode == NULL){
		return ERROR_INVALID_SESSION_ID;
	}
	 pWebSsnListNode->pUsrAppData = pUsrAppDataUpload[partNumber-1];
	 pthread_attr_init(&PthreadAttr);
	 pthread_attr_setdetachstate(&PthreadAttr,PTHREAD_CREATE_DETACHED);

	 pthread_create(&lsthread, &PthreadAttr, (void*)&webAppSendDataMultiPartAmazonUploadParallelThreaded1Step, (void*)pWebSsnListNode);
	}
// ##### Loop through files ENDS  ###### 
	while(threadCount != nParts){ threadCount = gThreadCount; if(threadCount == nParts) APP_LOG("HTTPSWRAPPER",LOG_DEBUG, " reached nParts %d\n", nParts);  }
// ##### Threaded Implementation Ends ###### 
// ##### Multi perform  STARTS ###### 

	/* Free the CURL handles */ //convered in destroy session 

// ##### Multi perform  ENDS ###### 

       sleep(1);
       for ( partNumber=1; partNumber <= nParts; partNumber++) {
		for (i=0; i<pUsrAppDataUpload[partNumber-1]->outHeaderLength && i<DATA_BUF_LEN; i++) {
		}

		for (i=0; i<pUsrAppDataUpload[partNumber-1]->outDataLength && i<DATA_BUF_LEN; i++) {
		}
		strncat(pUsrAppData->outData, pUsrAppDataUpload[partNumber-1]->outData, sizeof(pUsrAppData->outData)-strlen(pUsrAppData->outData)-1);
		strncat(pUsrAppData->outHeader, pUsrAppDataUpload[partNumber-1]->outHeader, sizeof(pUsrAppData->outHeader)-strlen(pUsrAppData->outHeader)-1);
		strncpy(pUsrAppData->eTag[partNumber-1], eTagPart[partNumber-1], sizeof(pUsrAppData->eTag[partNumber-1])-1);
	}
	
       sleep(1);
       osUtilsReleaseLock(&gwebAppDataLock); 
       for (partNumber=1; partNumber<=nParts; partNumber++) {
		webAppDestroySession( pSsnDataUpload[partNumber-1] );
		free(pUsrAppDataUpload[partNumber-1]);//copy contents 
	}
	osUtilsGetLock(&gwebAppDataLock);

	osUtilsReleaseLock(&gwebAppDataLock); 
	pSsnDataComplete = webAppCreateSession(0);
	osUtilsGetLock(&gwebAppDataLock);

	pUsrAppDataComplete = (UserAppData*)MALLOC(sizeof(UserAppData));

	snprintf( buf, sizeof(buf), "%s%s?uploadId=%s", amzURL, fileUrl, uploadId);
	strncpy( pUsrAppDataComplete->url, buf, sizeof(pUsrAppDataComplete->url)-1);

	strncpy( pUsrAppDataComplete->keyVal[0].key, "Authorization", sizeof(pUsrAppDataComplete->keyVal[0].key)-1);   
	strncpy( pUsrAppDataComplete->keyVal[0].value, pUsrAppData->keyVal[0].value, sizeof(pUsrAppDataComplete->keyVal[0].value)-1);
	strncpy( pUsrAppDataComplete->keyVal[1].key, "Content-Type", sizeof(pUsrAppDataComplete->keyVal[1].key)-1);   
	strncpy( pUsrAppDataComplete->keyVal[1].value, "application/xml", sizeof(pUsrAppDataComplete->keyVal[1].value)-1);
	pUsrAppDataComplete->keyValLen = 2;

	strncpy( pUsrAppDataComplete->url, "http://173.196.160.173:8080/DuoService/duo/multpartEventFile", sizeof(pUsrAppDataComplete->url)-1);
 	res = webAppSendDataMultiPartAmazonComplete1Step( pSsnDataComplete, pUsrAppDataComplete, fileUrl, uploadId, pUsrAppData);

	//destroy session upload
	osUtilsReleaseLock(&gwebAppDataLock); 
	webAppDestroySession ( pSsnDataComplete );
	osUtilsGetLock(&gwebAppDataLock);
	free(pUsrAppDataComplete);
	gThreadCount = 0;
	return res;
}

/****************************************************************************
 *  Function: webAppSendDataMultiPartAmazon1Step 
 *
 *    Sends split data to Amazon in 1 step
 *      
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data 
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSendDataMultiPartAmazon1Step( UserAppSessionData *pSsnData, UserAppData *pUsrAppData)
{
	UserAppData *pUsrAppDataUpload[4], *pUsrAppDataComplete = NULL;
	UserAppSessionData *pSsnDataUpload[4], *pSsnDataComplete = NULL;
	char buf[SIZE_1024B], str1[2], *ptr, auth[4][SIZE_128B];
  	char cTime[SIZE_128B] = {}, fileUrl[SIZE_128B], uploadId[SIZE_128B], c;
	int i, res;
  long respcode = 0;

	unsigned long lBufferSize = 0;
	off_t lSize;
	struct stat st;
	int partNumber =0, fd, partSize;
	char partFileName[17] = "";

	strncpy( pUsrAppData->url, "http://173.196.160.173:8080/DuoService/duo/initiateMultipart", sizeof(pUsrAppData->url)-1);
	webAppSendDataGet( pSsnData, pUsrAppData );

	if( !pUsrAppData->outDataLength ){
		return ERROR_CLOUD_RESPONSE;
	}

	curl_easy_getinfo( pSsnData->curl, CURLINFO_RESPONSE_CODE, &respcode); 
	if(respcode >= 500) {
		return ERROR_CLOUD_RESPONSE;
	}

	ptr = strstr( pUsrAppData->outData, "fileUri");
	ptr += 8;
	sscanf(ptr, "%[^'<'] %[<]", fileUrl, str1);

	ptr = strstr( pUsrAppData->outData, "multipartUploadId");
	ptr += 18;
	sscanf(ptr, "%[^'<'] %[<]", uploadId, str1);

	ptr = strstr( pUsrAppData->outData, "expireDateString");
	ptr += 17;
	sscanf(ptr, "%[^'<'] %[<]", cTime, str1);

	ptr = strstr( pUsrAppData->outData, "<authHeader");
	for(i=0; i<4; i++){
		ptr += 12;
		sscanf(ptr, "%[^'<'] %[<]", auth[i], str1);
		ptr = strstr( ptr, "<authHeader");
	}

// ##### File Split ###### // Max 4 files or less as required by cloud  
	fd = open(pUsrAppData->inData, O_RDONLY);
	fstat(fd, &st);	
	lSize = st.st_size;

	lBufferSize = 0;
	partNumber = 1;
	if (lSize/4 > 5242880)
		partSize = lSize/4 + 1;
	else
		partSize = 5242880;
	snprintf(buf, sizeof(buf), "split -b %d %s new", partSize, pUsrAppData->inData);
	system( buf);
	nParts = lSize/partSize;
	nParts += lSize%partSize>0?1:0; 

	close(fd);
// ##### File Split END ###### 

// ##### Loop through files ###### 
	for ( c = 96+partNumber; partNumber<= nParts; partNumber++, c++){
		if( strlen(pUsrAppData->eTag[partNumber-1]) ) { //valid Etag exists
			continue;
		}
		snprintf(buf, sizeof(buf), "newa%c", c);
		strncpy( partFileName, buf, sizeof(partFileName)-1);  

	pUsrAppDataUpload[partNumber-1] = (UserAppData*)MALLOC(sizeof(UserAppData));

		strncpy( pUsrAppDataUpload[partNumber-1]->inData, partFileName, sizeof(pUsrAppDataUpload[partNumber-1]->inData)-1);
		pUsrAppDataUpload[partNumber-1]->partNumber = partNumber;	

	snprintf( buf, sizeof(buf), "%s%s?partNumber=%d&uploadId=%s", amzURL, fileUrl, partNumber, uploadId);
	strncpy( pUsrAppDataUpload[partNumber-1]->url, buf, sizeof(pUsrAppDataUpload[partNumber-1]->url)-1);
	strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[0].key, "Date", sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[0].key)-1);   
	strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[0].value, cTime, sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[0].value)-1);
	strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[1].key, "Content-Type", sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[1].key)-1);   
	strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[1].value, "application/octet-stream", sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[1].value)-1);
	strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[2].key, "Authorization", sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[2].key)-1);   
	strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[2].value, auth[partNumber-1], sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[2].value)-1);
	pUsrAppDataUpload[partNumber-1]->keyValLen = 3;

	osUtilsReleaseLock(&gwebAppDataLock); 
	pSsnDataUpload[partNumber-1] = webAppCreateSession(0);
	osUtilsGetLock(&gwebAppDataLock);

	res = webAppSendDataMultiPartAmazonUpload1Step( pSsnDataUpload[partNumber-1], pUsrAppDataUpload[partNumber-1] );
	curl_easy_getinfo( pSsnDataUpload[partNumber-1]->curl, CURLINFO_RESPONSE_CODE, &respcode); 
 	if (respcode == 403){
		strncpy( pUsrAppData->url, "http://173.196.160.173:8080/DuoService/duo/refreshMultipart", sizeof(pUsrAppData->url)-1);
		webAppSendDataGet( pSsnData, pUsrAppData );
		if( !pUsrAppData->outDataLength ){
			osUtilsReleaseLock(&gwebAppDataLock); 
			webAppDestroySession ( pSsnDataUpload[partNumber-1] );
			osUtilsGetLock(&gwebAppDataLock);
			free(pUsrAppDataUpload[partNumber-1]);//copy contents 
			return ERROR_CLOUD_RESPONSE;
		}
		ptr = strstr( pUsrAppData->outData, "expireDateString");
		ptr += 17;
		sscanf(ptr, "%[^'<'] %[<]", cTime, str1);

		ptr = strstr( pUsrAppData->outData, "<authHeader");
		for(i=partNumber; i<=4; i++){
			ptr += 12;
			sscanf(ptr, "%[^'<'] %[<]", auth[i-1], str1);
			ptr = strstr( ptr, "<authHeader");
		}

		strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[0].key, "Date", sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[0].key)-1);   
		strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[0].value, cTime, sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[0].value)-1);
		strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[1].key, "Content-Type", sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[1].key)-1);   
		strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[1].value, "application/octet-stream", sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[1].value)-1);
		strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[2].key, "Authorization", sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[2].key)-1);   
		strncpy( pUsrAppDataUpload[partNumber-1]->keyVal[2].value, auth[partNumber-1], sizeof(pUsrAppDataUpload[partNumber-1]->keyVal[2].value)-1);
		pUsrAppDataUpload[partNumber-1]->keyValLen = 3;

		res = webAppSendDataMultiPartAmazonUpload1Step( pSsnDataUpload[partNumber-1], pUsrAppDataUpload[partNumber-1] );
	}
	if( res == ERROR_TRANSACTION || res == ERROR_INVALID_FILE)
	{
		pUsrAppData->partNumber = partNumber;
		osUtilsReleaseLock(&gwebAppDataLock); 
		webAppDestroySession ( pSsnDataUpload[partNumber-1] );
		osUtilsGetLock(&gwebAppDataLock);
		free(pUsrAppDataUpload[partNumber-1]);//copy contents 
		return res;
	}
//	## copy uploaded data ##
	strncat(pUsrAppData->outData, pUsrAppDataUpload[partNumber-1]->outData, sizeof(pUsrAppData->outData)-strlen(pUsrAppData->outData)-1);
	strncat(pUsrAppData->outHeader, pUsrAppDataUpload[partNumber-1]->outHeader, sizeof(pUsrAppData->outHeader)-strlen(pUsrAppData->outHeader)-1);
	strncpy(pUsrAppData->eTag[partNumber-1], eTagPart[partNumber-1], sizeof(pUsrAppData->eTag[partNumber-1])-1);
	osUtilsReleaseLock(&gwebAppDataLock); 
	webAppDestroySession ( pSsnDataUpload[partNumber-1] );
	osUtilsGetLock(&gwebAppDataLock);
	free(pUsrAppDataUpload[partNumber-1]);//copy contents 
	}
// ##### Loop through files END ##### 
	osUtilsReleaseLock(&gwebAppDataLock); 
	pSsnDataComplete = webAppCreateSession(0);
	osUtilsGetLock(&gwebAppDataLock);

	pUsrAppDataComplete = (UserAppData*)ZALLOC(sizeof(UserAppData));

	snprintf( buf, sizeof(buf), "%s%s?uploadId=%s", amzURL, fileUrl, uploadId);
	strncpy( pUsrAppDataComplete->url, buf, sizeof(pUsrAppDataComplete->url)-1);

	strncpy( pUsrAppDataComplete->keyVal[0].key, "Authorization", sizeof(pUsrAppDataComplete->keyVal[0].key)-1);   
	strncpy( pUsrAppDataComplete->keyVal[0].value, pUsrAppData->keyVal[0].value, sizeof(pUsrAppDataComplete->keyVal[0].value)-1);
	strncpy( pUsrAppDataComplete->keyVal[1].key, "Content-Type", sizeof(pUsrAppDataComplete->keyVal[1].key)-1);   
	strncpy( pUsrAppDataComplete->keyVal[1].value, "application/xml", sizeof(pUsrAppDataComplete->keyVal[1].value)-1);
	pUsrAppDataComplete->keyValLen = 2;

	strncpy( pUsrAppDataComplete->url, "http://173.196.160.173:8080/DuoService/duo/multpartEventFile", sizeof(pUsrAppDataComplete->url)-1);
 	res = webAppSendDataMultiPartAmazonComplete1Step( pSsnDataComplete, pUsrAppDataComplete, fileUrl, uploadId, pUsrAppData);

	//destroy session
	osUtilsReleaseLock(&gwebAppDataLock); 
	webAppDestroySession ( pSsnDataComplete );
	osUtilsGetLock(&gwebAppDataLock);
	free(pUsrAppDataComplete);
	return res;
}

/****************************************************************************
 *  Function: webAppSendDataMultiPartAmazonInitiate #### FOR POC ####
 *
 *    Sends split data to Amazon
 *      
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data 
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSendDataMultiPartAmazonInitiate( UserAppSessionData *pSsnData, UserAppData *pUsrAppData)
{
	CURLcode res = CURLE_OK;
	ListNode *pNode = NULL;
	WebSessionListNode *pWebSsnListNode = NULL;
	int i;
	long response;	
	char buf[SIZE_1024B], *url, *ptr, str[SIZE_64B], str1[SIZE_2B], buf2[SIZE_1024B];
	struct curl_slist *slist = NULL;

	osUtilsGetLock(&gwebAppDataLock);
	pNode = ListHead( &gWebSessionList);
	while( pNode != NULL)
	{
		pWebSsnListNode = (WebSessionListNode *)(pNode->item); 
		if ( pWebSsnListNode->pUsrAppSsnData->sessionId == pSsnData->sessionId ){
			break;
		}
		pNode = ListNext( &gWebSessionList, pNode);
	}
	osUtilsReleaseLock(&gwebAppDataLock);
	if( pNode == NULL){
		return ERROR_INVALID_SESSION_ID;
	}

	pWebSsnListNode->pUsrAppData = pUsrAppData; 

	/* set the URL that is about to receive our POST. */
	url = createUriParams( "POST", "MyData3?uploads", pWebSsnListNode->pUsrAppData->keyVal[0].value, "application/x-www-form-urlencoded");
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_URL, url);
	if( pWebSsnListNode->pUsrAppData->keyValLen){
		for( i=0; i<pWebSsnListNode->pUsrAppData->keyValLen; i++){	
			if (!(strcmp(pWebSsnListNode->pUsrAppData->keyVal[i].key, "Content-Length"))) {
			}
			snprintf(buf, sizeof(buf), "%s: %s", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value);
			slist = curl_slist_append(slist, buf);
		}
	}
	snprintf(buf, sizeof(buf), "Authorization: AWS %s:%s", amzAccessId, encSign);
	slist = curl_slist_append(slist, buf);
	// for method POST 
	snprintf(buf, sizeof(buf), "Content-Type: application/x-www-form-urlencoded");
	slist = curl_slist_append(slist, buf);
	snprintf(buf, sizeof(buf), "Content-Length: 0");
	slist = curl_slist_append(slist, buf);

	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HTTPHEADER, slist);

	/* specify we want to POST data */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_POST, 1L);
	/* Write and header callback */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEHEADER, pWebSsnListNode->pUsrAppSsnData);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEDATA, pWebSsnListNode->pUsrAppSsnData);
	/* verbose debug output option */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_VERBOSE, 1L);

	if (pWebSsnListNode->pUsrAppData->httpsFlag) {
		webAppSetSSLOptions(pWebSsnListNode->pUsrAppSsnData->curl);
	}
	{
		/* use the data provided by user */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_callback);
		/* pointer to pass to our read function */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pWebSsnListNode->pUsrAppData);
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
		/* Set the expected POST size. */ 

		res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
		curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
    pWebSsnListNode->pUsrAppData->outResp = response;
    pUsrAppData->nStatusCode = response;
		res = webAppErrorHandling(pWebSsnListNode->pUsrAppSsnData->curl, res, pUsrAppData->disableFlag); 
	}

	/* cleanup */
	curl_slist_free_all(slist);

  long respcode = 0;
	curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &respcode); 

  pWebSsnListNode->pUsrAppData->outResp = respcode;
  pUsrAppData->nStatusCode = respcode;

	if (res == CURLE_OK) { 
	} else{
	}
	if (respcode == 200) {
		if ((res == CURLE_OK)){ 
			strncpy(str,"<UploadId", sizeof(str)-1);
			strncpy(buf2, pWebSsnListNode->pUsrAppData->outData, sizeof(buf2)-1);
			ptr = strstr(buf2, str);
			ptr = strstr(ptr, ">");
			ptr += 1;
			if (ptr == NULL){
				return -1;// TO DO free stuff
			}
			sscanf(ptr, "%[^'<'] %[<]", uploadId, str1);
		} else{
			return -1;	
		}
	}
	free(url);
	return res;
}

/****************************************************************************
 *  Function: webAppSendDataMultiPartAmazonUpload #### FOR POC Purpose ####
 *
 *    Sends cloud request for Amazon Upload 
 *      
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data 
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSendDataMultiPartAmazonUpload( UserAppSessionData *pSsnData, UserAppData *pUsrAppData)
{
	CURLcode res = CURLE_OK;
	ListNode *pNode = NULL;
	WebSessionListNode *pWebSsnListNode = NULL;
	int i, fd;
	long response;	
	struct stat fileInfo;
	FILE *pFp;
	char buf[SIZE_1024B],file[SIZE_256B], *url, *ptr, str[SIZE_64B], str1[SIZE_2B], buf2[SIZE_1024B];
	struct curl_slist *slist = NULL;

	osUtilsGetLock(&gwebAppDataLock);
	pNode = ListHead( &gWebSessionList);
	while( pNode != NULL)
	{
		pWebSsnListNode = (WebSessionListNode *)(pNode->item); 
		if ( pWebSsnListNode->pUsrAppSsnData->sessionId == pSsnData->sessionId ){
			break;
		}
		pNode = ListNext( &gWebSessionList, pNode);
	}
	osUtilsReleaseLock(&gwebAppDataLock);
	if( pNode == NULL){
		return ERROR_INVALID_SESSION_ID;
	}

	pWebSsnListNode->pUsrAppData = pUsrAppData; 

	/* set the URL that is about to receive our POST. */
	snprintf(buf, sizeof(buf), "MyData3?partNumber=%d&uploadId=%s", pUsrAppData->partNumber, uploadId);
	url = createUriParams( "PUT", buf, pWebSsnListNode->pUsrAppData->keyVal[0].value, pWebSsnListNode->pUsrAppData->keyVal[1].value);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_URL, url);

	if( pWebSsnListNode->pUsrAppData->keyValLen){
		for( i=0; i<pWebSsnListNode->pUsrAppData->keyValLen; i++){	
			snprintf(buf,sizeof(buf),"%s: %s", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value);
			slist = curl_slist_append(slist, buf);
		}
	}

	snprintf(buf,sizeof(buf),"Authorization: AWS %s:%s", amzAccessId, encSign);
	slist = curl_slist_append(slist, buf);
	slist = curl_slist_append(slist, "Transfer-Encoding");
	// for method POST 
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HTTPHEADER, slist);

	/* specify we want to POST data */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_PUT, 1L);

	/* Write and header callback */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEHEADER, pWebSsnListNode->pUsrAppSsnData);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEDATA, pWebSsnListNode->pUsrAppSsnData);
	/* verbose debug output option */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_VERBOSE, 1L);

	if (pWebSsnListNode->pUsrAppData->httpsFlag) {
		webAppSetSSLOptions(pWebSsnListNode->pUsrAppSsnData->curl);
	}

#if 1
	if (pWebSsnListNode->pUsrAppData->inDataLength <= 0) {
		strncpy(file, pWebSsnListNode->pUsrAppData->inData, sizeof(file)-1);
		pFp = fopen(file, "rb");
		if ( pFp == NULL ){
			curl_slist_free_all(slist);/* cleanup */
			free(url);
			return ERROR_INVALID_FILE; 	
		}else {
			/* use the data provided by user */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_file_callback);
			/* pointer to pass to our read function */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pFp);
			/* Set the file size */ 
			fd = open(file, O_RDONLY);
			fstat(fd, &fileInfo );
			close(fd);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)fileInfo.st_size);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
			res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
		  curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
		  pWebSsnListNode->pUsrAppData->outResp = response;
		  pUsrAppData->nStatusCode = response;
			fclose(pFp);
		}
	}else
#endif
	{
		/* use the data provided by user */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_callback);
		/* pointer to pass to our read function */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pWebSsnListNode->pUsrAppData);
		/* Set the expected POST size. */ 
		pWebSsnListNode->pUsrAppData->inDataLength = 10;
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)(pWebSsnListNode->pUsrAppData->inDataLength));
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
		res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
		curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
    pWebSsnListNode->pUsrAppData->outResp = response;
    pUsrAppData->nStatusCode = response;
		res = webAppErrorHandling(pWebSsnListNode->pUsrAppSsnData->curl, res, pUsrAppData->disableFlag); 
	}

	/* cleanup */
	curl_slist_free_all(slist);

  long respcode = 0;
	curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &respcode); 
  pWebSsnListNode->pUsrAppData->outResp = response;
  pUsrAppData->nStatusCode = respcode;
	if (res == CURLE_OK) { 
	} else{
	}
	if (respcode == 200) {
		if ((res == CURLE_OK) ){ 
			strncpy(str,"ETag", sizeof(str)-1);
			strncpy(buf2, pWebSsnListNode->pUsrAppData->outHeader, sizeof(buf2)-1);
			ptr = strstr(buf2, str);
			ptr = strstr(ptr, ":");
			ptr += 3;
			if (ptr == NULL){
				return -1;// TO DO free stuff
			}
			sscanf(ptr, "%[a-z,A-Z,0-9] %[/\n]", eTag, str1);
			strncpy(eTagPart[pUsrAppData->partNumber-1], eTag, sizeof(eTagPart[pUsrAppData->partNumber-1])-1);
		} else{
		}
	}
	free(url);
	return res;
}

/****************************************************************************
 *  Function: webAppSendDataMultiPartAmazonComplete #### FOR POC Purpose ####
 *
 *    Sends cloud request for Amazon Upload with cloud support
 *      
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data 
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSendDataMultiPartAmazonComplete( UserAppSessionData *pSsnData, UserAppData *pUsrAppData)
{
	CURLcode res = CURLE_OK;
	ListNode *pNode = NULL;
	WebSessionListNode *pWebSsnListNode = NULL;
	int i; 
	long response;	
	char buf[SIZE_1024B], *url;
	struct curl_slist *slist = NULL;

	osUtilsGetLock(&gwebAppDataLock);
	pNode = ListHead( &gWebSessionList);
	while( pNode != NULL)
	{
		pWebSsnListNode = (WebSessionListNode *)(pNode->item); 
		if ( pWebSsnListNode->pUsrAppSsnData->sessionId == pSsnData->sessionId ){
			break;
		}
		pNode = ListNext( &gWebSessionList, pNode);
	}
	osUtilsReleaseLock(&gwebAppDataLock);
	if( pNode == NULL){
		return ERROR_INVALID_SESSION_ID;
	}

	pWebSsnListNode->pUsrAppData = pUsrAppData; 

	/* set the URL that is about to receive our POST. */
	snprintf(buf,sizeof(buf),"MyData3?uploadId=%s", uploadId );
	url = createUriParams( "POST", buf, pWebSsnListNode->pUsrAppData->keyVal[0].value, pWebSsnListNode->pUsrAppData->keyVal[1].value);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_URL, url);

	strncpy(pWebSsnListNode->pUsrAppData->inData, "<CompleteMultipartUpload>\n", sizeof(pWebSsnListNode->pUsrAppData->inData)-1);
	for (i = 1; i <= nParts; i++) {
		strncat( pWebSsnListNode->pUsrAppData->inData, "  <Part>\n", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		strncat( pWebSsnListNode->pUsrAppData->inData,"    <PartNumber>", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		snprintf(buf,sizeof(buf),"%d",i);
		strncat( pWebSsnListNode->pUsrAppData->inData, buf, sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		strncat( pWebSsnListNode->pUsrAppData->inData,"</PartNumber>\n", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		strncat( pWebSsnListNode->pUsrAppData->inData, "    <ETag>", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		strncat( pWebSsnListNode->pUsrAppData->inData, eTagPart[i-1], sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		strncat( pWebSsnListNode->pUsrAppData->inData, "</ETag>\n", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		strncat( pWebSsnListNode->pUsrAppData->inData, "  </Part>\n", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
	}  

	strncat(pWebSsnListNode->pUsrAppData->inData, "</CompleteMultipartUpload>\n", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);

	if( pWebSsnListNode->pUsrAppData->keyValLen){
		for( i=0; i<pWebSsnListNode->pUsrAppData->keyValLen; i++){	
			if (!(strcmp(pWebSsnListNode->pUsrAppData->keyVal[i].key, "Content-Length"))) {
				snprintf(buf,sizeof(buf),"%d ", strlen(pWebSsnListNode->pUsrAppData->inData));				
				strncpy( pWebSsnListNode->pUsrAppData->keyVal[i].value, buf, sizeof(pWebSsnListNode->pUsrAppData->keyVal[i].value)-1);
			}
			snprintf(buf,sizeof(buf),"%s: %s", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value);
			slist = curl_slist_append(slist, buf);
		}
	}
	snprintf(buf,sizeof(buf),"Authorization: AWS %s:%s", amzAccessId, encSign);
	slist = curl_slist_append(slist, buf);
	slist = curl_slist_append(slist, "Transfer-Encoding");
	// for method POST 
	slist = curl_slist_append(slist, "Content-Type");
	slist = curl_slist_append(slist, "Content-Length");
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HTTPHEADER, slist);

	/* specify we want to POST data */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_POST, 1L);

	/* Write and header callback */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEHEADER, pWebSsnListNode->pUsrAppSsnData);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEDATA, pWebSsnListNode->pUsrAppSsnData);
	/* verbose debug output option */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_VERBOSE, 1L);

	if (pWebSsnListNode->pUsrAppData->httpsFlag) {
		webAppSetSSLOptions(pWebSsnListNode->pUsrAppSsnData->curl);
	}

	{
		/* use the data provided by user */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_callback);
		/* pointer to pass to our read function */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pWebSsnListNode->pUsrAppData);
		/* Set the expected POST size. */ 
		pWebSsnListNode->pUsrAppData->inDataLength = strlen(pWebSsnListNode->pUsrAppData->inData);
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_POSTFIELDSIZE, (long)(pWebSsnListNode->pUsrAppData->inDataLength));
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
		res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
		curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
    pWebSsnListNode->pUsrAppData->outResp = response;
    pUsrAppData->nStatusCode = response;
		res = webAppErrorHandling(pWebSsnListNode->pUsrAppSsnData->curl, res, pUsrAppData->disableFlag); 
	}

	/* cleanup */
	curl_slist_free_all(slist);

	free(url);
	return res;
}

/****************************************************************************
 *  Function: webAppSendDataMultiPartAmazonComplete1Step 
 *
 *    Sends cloud request for Amazon Upload with cloud support
 *      
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data 
 *    fileUrl - File Url where to be uploaded 
 *    uploadId - Upload Id 
 *    mac - Mac address 
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSendDataMultiPartAmazonComplete1Step( UserAppSessionData *pSsnData, UserAppData *pUsrAppData, char* fileUri, char* uploadId, UserAppData *pAppData)
{
	CURLcode res = CURLE_OK;
	ListNode *pNode = NULL;
	WebSessionListNode *pWebSsnListNode = NULL;
	int i;
	long response;	
	char buf[SIZE_1024B];
	struct curl_slist *slist = NULL;
	osUtilsGetLock(&gwebAppDataLock);
	pNode = ListHead( &gWebSessionList);
	while( pNode != NULL)
	{
		pWebSsnListNode = (WebSessionListNode *)(pNode->item); 
		if ( pWebSsnListNode->pUsrAppSsnData->sessionId == pSsnData->sessionId ){
			break;
		}
		pNode = ListNext( &gWebSessionList, pNode);
	}
	osUtilsReleaseLock(&gwebAppDataLock);
	if( pNode == NULL){
		return ERROR_INVALID_SESSION_ID;
	}

	pWebSsnListNode->pUsrAppData = pUsrAppData; 

	/* set the URL that is about to receive our POST. */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_URL, pWebSsnListNode->pUsrAppData->url);

	snprintf( buf,sizeof(buf),"<eventFile><path>%s</path><fileUri>%s</fileUri><macAddress>%s</macAddress><multipartUploadId>%s</multipartUploadId><contentType>application/octet-stream</contentType>", fileUri, fileUri, pAppData->mac, uploadId);
	strncpy(pWebSsnListNode->pUsrAppData->inData, buf, sizeof(pWebSsnListNode->pUsrAppData->inData)-1);
	for (i = 1; i <= nParts; i++) {
		strncat( pWebSsnListNode->pUsrAppData->inData,"<fileParts>", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		strncat( pWebSsnListNode->pUsrAppData->inData,"<partNumber>", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		snprintf(buf,sizeof(buf),"%d",i);
		strncat( pWebSsnListNode->pUsrAppData->inData, buf, sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		strncat( pWebSsnListNode->pUsrAppData->inData,"</partNumber>", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		strncat( pWebSsnListNode->pUsrAppData->inData, "<eTag>", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		strncat( pWebSsnListNode->pUsrAppData->inData, pAppData->eTag[i-1], sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		strncat( pWebSsnListNode->pUsrAppData->inData, "</eTag>", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		strncat( pWebSsnListNode->pUsrAppData->inData, "<expireDateString></expireDateString>", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		strncat( pWebSsnListNode->pUsrAppData->inData, "<signature></signature>", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
		strncat( pWebSsnListNode->pUsrAppData->inData,"</fileParts>", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);
	}  
	strncat(pWebSsnListNode->pUsrAppData->inData, "</eventFile>\n", sizeof(pWebSsnListNode->pUsrAppData->inData)-strlen(pWebSsnListNode->pUsrAppData->inData)-1);

	if( pWebSsnListNode->pUsrAppData->keyValLen){
		for( i=0; i<pWebSsnListNode->pUsrAppData->keyValLen; i++){	
			if (!(strcmp(pWebSsnListNode->pUsrAppData->keyVal[i].key, "Content-Length"))) {
				snprintf(buf,sizeof(buf),"%d ", strlen(pWebSsnListNode->pUsrAppData->inData));				
				strncpy( pWebSsnListNode->pUsrAppData->keyVal[i].value, buf, sizeof(pWebSsnListNode->pUsrAppData->keyVal[i].value)-1);
			}
			snprintf(buf,sizeof(buf),"%s: %s", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value);
			slist = curl_slist_append(slist, buf);
		}
	}
	slist = curl_slist_append(slist, "Transfer-Encoding");
	// for method POST 
	slist = curl_slist_append(slist, "Content-Type");
	slist = curl_slist_append(slist, "Content-Length");
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HTTPHEADER, slist);

	/* specify we want to POST data */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_POST, 1L);

	/* Write and header callback */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEHEADER, pWebSsnListNode->pUsrAppSsnData);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEDATA, pWebSsnListNode->pUsrAppSsnData);
	/* verbose debug output option */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_VERBOSE, 1L);

	if (pWebSsnListNode->pUsrAppData->httpsFlag) {
		webAppSetSSLOptions(pWebSsnListNode->pUsrAppSsnData->curl);
	}

	{
		/* use the data provided by user */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_callback);
		/* pointer to pass to our read function */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pWebSsnListNode->pUsrAppData);
		/* Set the expected POST size. */ 
		pWebSsnListNode->pUsrAppData->inDataLength = strlen(pWebSsnListNode->pUsrAppData->inData);
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_POSTFIELDSIZE, (long)(pWebSsnListNode->pUsrAppData->inDataLength));
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
		res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
		curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
    pWebSsnListNode->pUsrAppData->outResp = response;
    pUsrAppData->nStatusCode = response;
		res = webAppErrorHandling(pWebSsnListNode->pUsrAppSsnData->curl, res, pUsrAppData->disableFlag); 
	}

	/* cleanup */
	curl_slist_free_all(slist);

	return res;
}


/****************************************************************************
 *  Function: webAppSendDataMultiPartAmazonUploadParallelThreaded1Step 
 *
 *    Parallel upload threaded implementation
 *      
 *  Parameters:
 *    pWebSsnListNode - Pointer to the WebSessionListNode
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSendDataMultiPartAmazonUploadParallelThreaded1Step( WebSessionListNode *pWebSsnListNode)
{
	CURLcode res = CURLE_OK;
	int i, fd;
  long response = 0;
	struct stat fileInfo;
	FILE *pFp;
	char buf[SIZE_1024B],file[SIZE_256B], *ptr, str[SIZE_64B], str1[SIZE_2B], buf2[SIZE_1024B];
	struct curl_slist *slist = NULL;

	tu_set_my_thread_name( __FUNCTION__ );


	/* set the URL that is about to receive our POST. */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_URL, pWebSsnListNode->pUsrAppData->url);

	if( pWebSsnListNode->pUsrAppData->keyValLen){
		for( i=0; i<pWebSsnListNode->pUsrAppData->keyValLen; i++){	
			snprintf(buf,sizeof(buf),"%s: %s", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value);
			slist = curl_slist_append(slist, buf);
		}
	}
	slist = curl_slist_append(slist, "Transfer-Encoding");
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HTTPHEADER, slist);

	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_PUT, 1L);

	/* Write and header callback */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEHEADER, pWebSsnListNode->pUsrAppSsnData);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEDATA, pWebSsnListNode->pUsrAppSsnData);
	/* verbose debug output option */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_VERBOSE, 1L);

	if (pWebSsnListNode->pUsrAppData->httpsFlag) {
		webAppSetSSLOptions(pWebSsnListNode->pUsrAppSsnData->curl);
	}

	if (pWebSsnListNode->pUsrAppData->inDataLength <= 0) {
		strncpy(file, pWebSsnListNode->pUsrAppData->inData, sizeof(file)-1);
		pFp = fopen(file, "rb");
		if ( pFp == NULL ){
			curl_slist_free_all(slist);/* cleanup */
			return ERROR_INVALID_FILE; 	
		}else {
			/* use the data provided by user */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_file_callback);
			/* pointer to pass to our read function */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pFp);
			/* Set the file size */ 
			fd = open(file, O_RDONLY);
			fstat(fd, &fileInfo );
			close(fd);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)fileInfo.st_size);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
			res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
		  curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
		  pWebSsnListNode->pUsrAppData->outResp = response;
      //pUsrAppData->nStatusCode = response;
			fclose(pFp);
		}
	}else
	{
		/* use the data provided by user */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_callback);
		/* pointer to pass to our read function */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pWebSsnListNode->pUsrAppData);
		/* Set the expected POST size. */ 
		pWebSsnListNode->pUsrAppData->inDataLength = 10;
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)(pWebSsnListNode->pUsrAppData->inDataLength));
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
		res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
		curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
		pWebSsnListNode->pUsrAppData->outResp = response;
    //pUsrAppData->nStatusCode = response;

		res = webAppErrorHandling(pWebSsnListNode->pUsrAppSsnData->curl, res, pWebSsnListNode->pUsrAppData->disableFlag); 
	}

	/* cleanup */
	curl_slist_free_all(slist);

	long respcode;
	curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &respcode); 
	if (res == CURLE_OK) { 
	} else{
	}

	if (respcode == 200) {
		if ((res == CURLE_OK) ){ 
			strncpy(str,"ETag",sizeof(str)-1);
			strncpy(buf2, pWebSsnListNode->pUsrAppData->outHeader, sizeof(buf2)-1);
			ptr = strstr(buf2, str);
			ptr = strstr(ptr, ":");
			ptr += 3;
			if (ptr == NULL){
				return ERROR_TRANSACTION; 
			}
			sscanf(ptr, "%[a-z,A-Z,0-9] %[/\n]", eTag, str1);
			strncpy(eTagPart[pWebSsnListNode->pUsrAppData->partNumber-1], eTag, sizeof(eTagPart[pWebSsnListNode->pUsrAppData->partNumber-1])-1);
		} else{
			return ERROR_TRANSACTION;
		}
	}
	gThreadCount++;
	return res;
}

/****************************************************************************
 *  Function: webAppSendDataMultiPartAmazonUploadParallel1Step ### For multi perform future use ####
 *
 *    Parallel upload with multi perfrom implementation, handle set options 
 *      
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data 
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSendDataMultiPartAmazonUploadParallel1Step( UserAppSessionData *pSsnData, UserAppData *pUsrAppData)
{
	CURLcode res = CURLE_OK;
	ListNode *pNode = NULL;
	WebSessionListNode *pWebSsnListNode = NULL;
	int i, fd;
	long response;	
	struct stat fileInfo;
	FILE *pFp;
	char buf[SIZE_1024B],file[SIZE_256B], *ptr, str[SIZE_64B], str1[SIZE_2B], buf2[SIZE_1024B];
	struct curl_slist *slist = NULL;

	pNode = ListHead( &gWebSessionList);
	while( pNode != NULL)
	{
		pWebSsnListNode = (WebSessionListNode *)(pNode->item); 
		if ( pWebSsnListNode->pUsrAppSsnData->sessionId == pSsnData->sessionId ){
			break;
		}
		pNode = ListNext( &gWebSessionList, pNode);
	}

	if( pNode == NULL){
		return ERROR_INVALID_SESSION_ID;
	}

	pWebSsnListNode->pUsrAppData = pUsrAppData; 

	/* set the URL that is about to receive our POST. */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_URL, pWebSsnListNode->pUsrAppData->url);

	if( pWebSsnListNode->pUsrAppData->keyValLen){
		for( i=0; i<pWebSsnListNode->pUsrAppData->keyValLen; i++){	
			snprintf(buf,sizeof(buf),"%s: %s", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value);
			slist = curl_slist_append(slist, buf);
		}
	}
	slist = curl_slist_append(slist, "Transfer-Encoding");
	// for method POST 
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HTTPHEADER, slist);

	/* specify we want to POST data */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_PUT, 1L);

	/* Write and header callback */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEHEADER, pWebSsnListNode->pUsrAppSsnData);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEDATA, pWebSsnListNode->pUsrAppSsnData);
	/* verbose debug output option */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_VERBOSE, 1L);

	if (pWebSsnListNode->pUsrAppData->httpsFlag) {
		webAppSetSSLOptions(pWebSsnListNode->pUsrAppSsnData->curl);
	}

	if (pWebSsnListNode->pUsrAppData->inDataLength <= 0) {
		strncpy(file, pWebSsnListNode->pUsrAppData->inData, sizeof(file)-1);
		pFp = fopen(file, "rb");
		if ( pFp == NULL ){
			curl_slist_free_all(slist);/* cleanup */
			return ERROR_INVALID_FILE; 	
		}else {
			/* use the data provided by user */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_file_callback);
			/* pointer to pass to our read function */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pFp);
			/* Set the file size */ 
			fd = open(file, O_RDONLY);
			fstat(fd, &fileInfo );
			close(fd);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)fileInfo.st_size);
			fclose(pFp);
		}
	}else
	{
		/* use the data provided by user */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_callback);
		/* pointer to pass to our read function */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pWebSsnListNode->pUsrAppData);
		/* Set the expected POST size. */ 
		pWebSsnListNode->pUsrAppData->inDataLength = 10;
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)(pWebSsnListNode->pUsrAppData->inDataLength));
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
		res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
		curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
    pWebSsnListNode->pUsrAppData->outResp = response;
    pUsrAppData->nStatusCode = response;
		res = webAppErrorHandling(pWebSsnListNode->pUsrAppSsnData->curl, res, pWebSsnListNode->pUsrAppData->disableFlag); 
	}

	/* cleanup */
	curl_slist_free_all(slist);

  long respcode = 0;
	curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &respcode); 
	if (res == CURLE_OK) { 
	} else{
	}
	if (respcode == 200) {
		if ((res == CURLE_OK) ){ 
			strncpy(str,"ETag",sizeof(str)-1);
			strncpy(buf2, pWebSsnListNode->pUsrAppData->outHeader, sizeof(buf2)-1);
			ptr = strstr(buf2, str);
			ptr = strstr(ptr, ":");
			ptr += 3;
			if (ptr == NULL){
				return -1;// TO DO free stuff
			}
			sscanf(ptr, "%[a-z,A-Z,0-9] %[/\n]", eTag, str1);
			strncpy(eTagPart[pUsrAppData->partNumber-1], eTag, sizeof(eTagPart[pUsrAppData->partNumber-1])-1);
		} else{
		}
	}
	return res;
}

/****************************************************************************
 *  Function: webAppSendDataMultiPartAmazonUpload1Step 
 *
 *    Amazon Upload with cloud support
 *      
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data 
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSendDataMultiPartAmazonUpload1Step( UserAppSessionData *pSsnData, UserAppData *pUsrAppData)
{
	CURLcode res = CURLE_OK;
	ListNode *pNode = NULL;
	WebSessionListNode *pWebSsnListNode = NULL;
	int i, fd;
	long response;	
	struct stat fileInfo;
	FILE *pFp;
	char buf[SIZE_1024B],file[SIZE_256B], *ptr, str[SIZE_64B], str1[SIZE_2B], buf2[SIZE_1024B];
	struct curl_slist *slist = NULL;
	osUtilsGetLock(&gwebAppDataLock);
	pNode = ListHead( &gWebSessionList);
	while( pNode != NULL)
	{
		pWebSsnListNode = (WebSessionListNode *)(pNode->item); 
		if ( pWebSsnListNode->pUsrAppSsnData->sessionId == pSsnData->sessionId ){
			break;
		}
		pNode = ListNext( &gWebSessionList, pNode);
	}
	osUtilsReleaseLock(&gwebAppDataLock);
	if( pNode == NULL){
		return ERROR_INVALID_SESSION_ID;
	}

	pWebSsnListNode->pUsrAppData = pUsrAppData; 

	/* set the URL that is about to receive our POST. */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_URL, pWebSsnListNode->pUsrAppData->url);

	if( pWebSsnListNode->pUsrAppData->keyValLen){
		for( i=0; i<pWebSsnListNode->pUsrAppData->keyValLen; i++){	
			snprintf(buf,sizeof(buf),"%s: %s", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value);
			slist = curl_slist_append(slist, buf);
		}
	}
	slist = curl_slist_append(slist, "Transfer-Encoding");
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HTTPHEADER, slist);

	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_PUT, 1L);

	/* Write and header callback */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HEADERFUNCTION, header_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEHEADER, pWebSsnListNode->pUsrAppSsnData);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEDATA, pWebSsnListNode->pUsrAppSsnData);
	/* verbose debug output option */
	curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_VERBOSE, 1L);

	if (pWebSsnListNode->pUsrAppData->httpsFlag) {
		webAppSetSSLOptions(pWebSsnListNode->pUsrAppSsnData->curl);
	}

	if (pWebSsnListNode->pUsrAppData->inDataLength <= 0) {
		strncpy(file, pWebSsnListNode->pUsrAppData->inData, sizeof(file)-1);
		pFp = fopen(file, "rb");
		if ( pFp == NULL ){
			curl_slist_free_all(slist);/* cleanup */
			return ERROR_INVALID_FILE; 	
		}else {
			//file handling not supported by server
			/* use the data provided by user */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_file_callback);
			/* pointer to pass to our read function */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pFp);
			/* Set the file size */ 
			// get fb and fname
			fd = open(file, O_RDONLY);
			fstat(fd, &fileInfo );
			close(fd);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)fileInfo.st_size);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
			curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
			res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
		  curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
		  pWebSsnListNode->pUsrAppData->outResp = response;
      pUsrAppData->nStatusCode = response;
			fclose(pFp);
		}
	}else
	{
		/* use the data provided by user */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READFUNCTION, read_callback);
		/* pointer to pass to our read function */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_READDATA, pWebSsnListNode->pUsrAppData);
		/* Set the expected POST size. */ 
		pWebSsnListNode->pUsrAppData->inDataLength = 10;
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)(pWebSsnListNode->pUsrAppData->inDataLength));
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
		curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
		res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
		curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
    pWebSsnListNode->pUsrAppData->outResp = response;
    pUsrAppData->nStatusCode = response;
		res = webAppErrorHandling(pWebSsnListNode->pUsrAppSsnData->curl, res, pUsrAppData->disableFlag); 
	}

	/* cleanup */
	curl_slist_free_all(slist);

	long respcode;
	curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &respcode); 
  pWebSsnListNode->pUsrAppData->outResp = response;
  pUsrAppData->nStatusCode = respcode;
	if (res == CURLE_OK) { 
	} else{
	}
	if (respcode == 200) {
		if ((res == CURLE_OK) ){ 
			strncpy(str,"ETag",sizeof(str));
			strncpy(buf2, pWebSsnListNode->pUsrAppData->outHeader, sizeof(buf2)-1);
			ptr = strstr(buf2, str);
			ptr = strstr(ptr, ":");
			ptr += 3;
			if (ptr == NULL){
				return ERROR_TRANSACTION; 
			}
			sscanf(ptr, "%[a-z,A-Z,0-9] %[/\n]", eTag, str1);
			strncpy(eTagPart[pUsrAppData->partNumber-1], eTag, sizeof(eTagPart[pUsrAppData->partNumber-1])-1);
		} else{
			return ERROR_TRANSACTION;
		}
	}
	return res;
}

/****************************************************************************
 *  Function: webAppSendDataPutCustom
 *
 *    Puts data to the server
 *
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSendDataPutCustom(UserAppSessionData *pSsnData, UserAppData *pUsrAppData)
{
  CURLcode res = CURLE_OK;
  ListNode *pNode = NULL;
  WebSessionListNode *pWebSsnListNode = NULL;

  char buf[SIZE_1024B];

  int i = 0;
  long response = 0;
  int dataLength = 0;

  struct curl_slist *slist = NULL;

  osUtilsGetLock(&gwebAppDataLock);
  pNode = ListHead(&gWebSessionList);
  while( pNode != NULL )
  {
    pWebSsnListNode = (WebSessionListNode *)(pNode->item);
    if( pWebSsnListNode->pUsrAppSsnData->sessionId == pSsnData->sessionId )
    {
      break;
    }
    pNode = ListNext(&gWebSessionList, pNode);
  }
  osUtilsReleaseLock(&gwebAppDataLock);

  if( pNode == NULL )
  {
    return ERROR_INVALID_SESSION_ID;
  }

  pWebSsnListNode->pUsrAppData = pUsrAppData;

  /* verbose debug output option */
  curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_VERBOSE, 1L);

  /* set the URL that is about to receive our POST. */
  curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_URL, pWebSsnListNode->pUsrAppData->url);

  if( pWebSsnListNode->pUsrAppData->keyValLen )
  {
    for( i = 0; i < pWebSsnListNode->pUsrAppData->keyValLen; i++ )
    {
      memset(buf, 0x00, SIZE_1024B);
      snprintf(buf, sizeof(buf), "%s: %s", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value);
      slist = curl_slist_append(slist, buf);
    }
  }

  curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HTTPHEADER, slist);

  if( pWebSsnListNode->pUsrAppData->httpsFlag )
  {
    webAppSetSSLOptions(pWebSsnListNode->pUsrAppSsnData->curl);
  }

  curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CUSTOMREQUEST, "PUT");

  /*set follow location*/
  curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_FOLLOWLOCATION, 0L);
  /* Write and header callback */
  curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_HEADERFUNCTION, header_callback);
  curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEHEADER, pWebSsnListNode->pUsrAppSsnData);
  curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_WRITEDATA, pWebSsnListNode->pUsrAppSsnData);

  APP_LOG("webAppSendDataPutCustom", LOG_DEBUG, "inDataLength = %d", pWebSsnListNode->pUsrAppData->inDataLength);

  if( pWebSsnListNode->pUsrAppData->inDataLength <= 0 )
  {
    APP_LOG("webAppSendDataPutCustom", LOG_DEBUG, "inDataLength is NOT invalid, Nothing to do");
  }
  else
  {
    dataLength = pWebSsnListNode->pUsrAppData->inDataLength;
                APP_LOG("webAppSendDataPutCustom", LOG_DEBUG, "webAppSendDataPut: dataLength = %d", dataLength);

    curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_POSTFIELDS, pUsrAppData->inData);
    curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_POSTFIELDSIZE, dataLength);
    curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
    curl_easy_setopt(pWebSsnListNode->pUsrAppSsnData->curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
    res = curl_easy_perform(pWebSsnListNode->pUsrAppSsnData->curl);
    if( res == CURLE_OK )
    {
      APP_LOG("webAppSendDataPutCustom", LOG_DEBUG, "webAppSendDataPut is OK");
    }
    else
    {
      APP_LOG("webAppSendDataPutCustom", LOG_DEBUG, "webAppSendDataPut is Error");
    }

    curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_RESPONSE_CODE, &response);
    pWebSsnListNode->pUsrAppData->outResp = response;
    pUsrAppData->nStatusCode = response;

    //res = webAppErrorHandling(pWebSsnListNode->pUsrAppSsnData->curl, res, pUsrAppData->disableFlag);
  }

  /* cleanup */
  curl_slist_free_all(slist);

  return res;
}

/****************************************************************************
 *  Function: webAppSendData 
 *
 *    Fulfills applications request to send data as per the data provided by application 
 *      
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data 
 *    flag - differentiates between GET(0) and POST(1) 
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSendData( UserAppSessionData *pSsnData, UserAppData *pUsrAppData, int flag )
{
	UserAppSessionData *pUsrAppSsnData = NULL;
	CURLcode res = CURLE_OK;

	switch(flag){
		case 0: 
			if(pSsnData){ 
				res = webAppSendDataGet( pSsnData, pUsrAppData); 
				break;
			}
			else{
				pUsrAppSsnData = webAppCreateSession(0);
				res = webAppSendDataGet( pUsrAppSsnData, pUsrAppData); 
				webAppDestroySession ( pUsrAppSsnData ); break;
			}
		case 1: 
			if(pSsnData){ 
				res = webAppSendDataPost( pSsnData, pUsrAppData);
				break;
			}
			else{
				pUsrAppSsnData = webAppCreateSession(0);
				res = webAppSendDataPost( pUsrAppSsnData, pUsrAppData); 
				webAppDestroySession ( pUsrAppSsnData ); break;
			}
		case 2: 
			if(pSsnData){ 
				res = webAppSendDataPut( pSsnData, pUsrAppData); 
				break;
			}
			else{
				pUsrAppSsnData = webAppCreateSession(0);
				res = webAppSendDataPut( pUsrAppSsnData, pUsrAppData); 
				webAppDestroySession ( pUsrAppSsnData ); break; 
			}
		case 3: 
			if(pSsnData){ 
				res = webAppSendDataMultiPartAmazonInitiate( pSsnData, pUsrAppData); 
				break;
			}
			else{
				pUsrAppSsnData = webAppCreateSession(0);
				res = webAppSendDataMultiPartAmazonInitiate( pUsrAppSsnData, pUsrAppData); 
				webAppDestroySession ( pUsrAppSsnData ); break; 
			}
		case 4: 
			if(pSsnData){ 
				res = webAppSplitFiles( pSsnData, pUsrAppData); 
				break;
			}
			else{
				pUsrAppSsnData = webAppCreateSession(0);
				res = webAppSplitFiles( pSsnData, pUsrAppData); 
				webAppDestroySession ( pUsrAppSsnData ); break; 
			}
		case 5: 
			if(pSsnData){ 
				res = webAppSendDataMultiPartAmazonComplete( pSsnData, pUsrAppData); 
				break;
			}
			else{
				pUsrAppSsnData = webAppCreateSession(0);
				res = webAppSendDataMultiPartAmazonComplete( pUsrAppSsnData, pUsrAppData); 
				webAppDestroySession ( pUsrAppSsnData ); break; 
			}
		case 6: 
			if(pSsnData){ 
				res = webAppSendDataMultiPartAmazon1Step( pSsnData, pUsrAppData); 
				break;
			}
			else{
				pUsrAppSsnData = webAppCreateSession(0);
				res = webAppSendDataMultiPartAmazon1Step( pUsrAppSsnData, pUsrAppData); 
				webAppDestroySession ( pUsrAppSsnData ); break; 
			}
		case 7: 
			if(pSsnData){ 
				res = webAppSendDataMultiPartParallelAmazon1Step( pSsnData, pUsrAppData);
				break;
			}
			else{
				pUsrAppSsnData = webAppCreateSession(0);
				res = webAppSendDataMultiPartParallelAmazon1Step( pUsrAppSsnData, pUsrAppData); 
				webAppDestroySession ( pUsrAppSsnData ); break; 
			}
		case 8:	//Added HTTP DELETE Method
			if( pSsnData )
			{
				res = webAppSendDataDelete(pSsnData, pUsrAppData);
				break;
			}
			else
			{
				pUsrAppSsnData = webAppCreateSession(0);
				res = webAppSendDataGet(pUsrAppSsnData, pUsrAppData);
				webAppDestroySession (pUsrAppSsnData);
				break;
			}

                case 18:  
                        if(pSsnData){
                                res = webAppSendDataPutCustom( pSsnData, pUsrAppData);
                                break;
                        }
                        else{
                                pUsrAppSsnData = webAppCreateSession(0);
                                res = webAppSendDataPutCustom( pUsrAppSsnData, pUsrAppData);
                                webAppDestroySession ( pUsrAppSsnData ); break;
                        }
			break;

	}
#if 0
	APP_LOG("HTTPSWRAPPER", LOG_CRIT, "HTTP Request URL %s requested flag %d disableFlag %d httpsFlag %d retVal %d respCode %d", \
		pUsrAppData->url, flag, pUsrAppData->disableFlag, pUsrAppData->httpsFlag, res, pUsrAppData->outResp);
#endif
	APP_LOG("HTTPSWRAPPER", LOG_DEBUG, "HTTP Request URL %s requested flag %d disableFlag %d httpsFlag %d retVal %d respCode %d", \
		pUsrAppData->url, flag, pUsrAppData->disableFlag, pUsrAppData->httpsFlag, res, pUsrAppData->outResp);
	return res;
}

/****************************************************************************
 *  Function: webAppDestroySession 
 *
 *    Destroys the session created 
 *      
 *  Parameters:
 *    pSsnData - Session to be deleted
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int  webAppDestroySession ( UserAppSessionData *pSsnData )
{

	ListNode *pNode = NULL, *pDelNode = NULL;
	WebSessionListNode *pWebSsnListNode = NULL;
	int sessionId = 0;
	osUtilsGetLock(&gwebAppDataLock);
	pNode = ListHead( &gWebSessionList);
	while( pNode != NULL)
	{
		pWebSsnListNode = (WebSessionListNode *)(pNode->item); 
		if ( pWebSsnListNode->pUsrAppSsnData->sessionId == pSsnData->sessionId ){
			sessionId = pSsnData->sessionId;
			pDelNode = pNode;	
			break;
		}

		pNode = ListNext( &gWebSessionList, pNode);
	}
	if( pNode == NULL){
		osUtilsReleaseLock(&gwebAppDataLock);
		return ERROR_INVALID_SESSION_ID;
	}
	curl_easy_cleanup(pWebSsnListNode->pUsrAppSsnData->curl);
	ListDelNode( &gWebSessionList, pDelNode, 0 );
	free(pWebSsnListNode->pUsrAppSsnData);
	pWebSsnListNode->pUsrAppSsnData = NULL;
	free(pWebSsnListNode); 
	pWebSsnListNode = NULL;
	osUtilsReleaseLock(&gwebAppDataLock);
	return 0;
}

/****************************************************************************
 *  Function: webAppSetSSLOptions 
 *
 *    Destroys the session created 
 *      
 *  Parameters:
 *    pSsnData - Session to be deleted
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppSetSSLOptions(CURL *curl) {
	char certfile[64] = "/etc/certs/BuiltinObjectToken-GoDaddyClass2CA.crt";
	char certdir[32] = "/etc/certs";
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
	curl_easy_setopt(curl, CURLOPT_CERTINFO, 0L);
	curl_easy_setopt(curl, CURLOPT_CAINFO, certfile);
	curl_easy_setopt(curl, CURLOPT_CAPATH, certdir);
	return 0;	
}

/****************************************************************************
 *  Function: webAppPostTransactionInfo 
 *
 *    Sends data to the server
 *      
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data 
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppPostTransactionInfo( WebSessionListNode *pWebSsnListNode )
{
	CURL *curl;
	int i, k=0, res = 0, contentSize, fd;
	long response;
	struct curl_slist *slist = NULL;
	char buf[SIZE_1024B], buf2[SIZE_1024B], file[SIZE_256B];
	char eTagValue[SIZE_512B], locationValue[SIZE_256B], contentType[SIZE_64B], *ptr, str[SIZE_64B], str1[SIZE_2B];
	char *new_url = NULL; 
	FILE *pFp;
	struct stat fileInfo;
	TransactionInfo *pData;
	curl = curl_easy_init();

	if(pWebSsnListNode->pUsrAppData->httpsFlag)
		curl_easy_setopt( curl, CURLOPT_URL, "https://173.196.160.173:8843/DuoService/duo/eventFile");
	else
		curl_easy_setopt( curl, CURLOPT_URL, "http://173.196.160.173:8080/DuoService/duo/eventFile");

	if(pWebSsnListNode->pUsrAppData->keyValLen){
		for( i=0; i<pWebSsnListNode->pUsrAppData->keyValLen; i++){	
			snprintf(buf,sizeof(buf),"%s: %s", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value);
			slist = curl_slist_append(slist, buf);
			if( !strcmp( pWebSsnListNode->pUsrAppData->keyVal[i].key, "Content-Type") )	
				strncpy( contentType, pWebSsnListNode->pUsrAppData->keyVal[i].value, sizeof(contentType)-1);
		}
	}

	strncpy(buf2, pWebSsnListNode->pUsrAppData->outHeader, sizeof(buf2)-1);

	memset(eTagValue,0x0, SIZE_256B);
	strncpy(str,"ETag",sizeof(str)-1);
	ptr = strstr(buf2, str);
	ptr = strstr(ptr, ":");
	ptr += 3;
	if (ptr == NULL){
		return -1;
	}
	sscanf(ptr, "%[a-z,A-Z,0-9] %[/\n]", eTagValue, str1);

	memset(locationValue,0x0, SIZE_256B);
	curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_EFFECTIVE_URL, &new_url); 
	for(i=0;new_url[i]!='?';i++){
		if(new_url[i] == 'c' && new_url[i+1] == 'o' && new_url[i+2] == 'm'){
			break;	
		}
	}
	for(i=i+3;new_url[i]!='?';i++)
		locationValue[k++] = new_url[i];

	if (pWebSsnListNode->pUsrAppData->inDataLength <= 0) {
		strncpy(file, pWebSsnListNode->pUsrAppData->inData, sizeof(file)-1);
		pFp = fopen( file, "rb");
		if ( pFp == NULL ) {
			return ERROR_INVALID_FILE;	 
		}else {
			fd = open(file, O_RDONLY);
			fstat(fd, &fileInfo );
			close(fd);
			fclose(pFp);
			contentSize = fileInfo.st_size;
		}
	}
	else{
		contentSize = pWebSsnListNode->pUsrAppData->inDataLength;
	}

	pData = (TransactionInfo*)ZALLOC(sizeof(TransactionInfo));
	snprintf( pData->data,sizeof(pData->data),"<eventFile><id></id><macAddress>%s</macAddress><contentType>%s</contentType><contentSize>%d</contentSize><path>%s</path><eTag>%s</eTag></eventFile>", pWebSsnListNode->pUsrAppData->mac, contentType, contentSize, locationValue, eTagValue );
	pData->dataSize = strlen(pData->data);

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
	/* specify we want to POST data */
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	/* verbose debug output option */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	/* use the data provided by user */
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_transaction_callback);
	/* pointer to pass to our read function */
	curl_easy_setopt(curl, CURLOPT_READDATA, pData);

	if (pWebSsnListNode->pUsrAppData->httpsFlag) {
		webAppSetSSLOptions(curl);
	}
	/* Set the expected POST size. */ 
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)(pData->dataSize));
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL,1L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
	res = curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
  pWebSsnListNode->pUsrAppData->outResp = response;
  pWebSsnListNode->pUsrAppData->nStatusCode = response;
	res = webAppErrorHandling(curl, res, pWebSsnListNode->pUsrAppData->disableFlag); 

	free(pData);
	curl_slist_free_all(slist);
	curl_easy_cleanup(curl);

	return res;
}

/****************************************************************************
 *  Function: webAppPostTransactionInfoPutOnly 
 *
 *    Sends data to the server
 *      
 *  Parameters:
 *    pSsnData - Pointer to the session created by user
 *    pUsrAppData - Pointer to the user application data 
 *  Returns:
 *    Succes:0 failure non-zero
 *****************************************************************************/
int webAppPostTransactionInfoPutOnly( WebSessionListNode *pWebSsnListNode )
{
	CURL *curl;
	int i, k=0, res = 0, contentSize, fd;
	long response;
	struct curl_slist *slist = NULL;
	char buf[SIZE_1024B], buf2[SIZE_1024B], file[SIZE_256B];
	char eTagValue[SIZE_512B], locationValue[SIZE_256B], contentType[SIZE_64B], *ptr, str[SIZE_64B], str1[SIZE_2B];
	char *new_url = NULL; 
	FILE *pFp;
	struct stat fileInfo;
	TransactionInfo *pData;
	curl = curl_easy_init();

	if(pWebSsnListNode->pUsrAppData->httpsFlag)
		curl_easy_setopt( curl, CURLOPT_URL, "https://173.196.160.173:8843/DuoService/duo/eventFile");
	else
		curl_easy_setopt( curl, CURLOPT_URL, "http://173.196.160.173:8080/DuoService/duo/eventFile");

	if(pWebSsnListNode->pUsrAppData->keyValLen){
		for( i=0; i<pWebSsnListNode->pUsrAppData->keyValLen; i++){
			if( !strcmp( pWebSsnListNode->pUsrAppData->keyVal[i].key, "Content-Type") ) {	
				strncpy( contentType, pWebSsnListNode->pUsrAppData->keyVal[i].value, sizeof(contentType)-1);
				strncpy( pWebSsnListNode->pUsrAppData->keyVal[i].value, "application/xml", sizeof(pWebSsnListNode->pUsrAppData->keyVal[i].value)-1);
			}
			snprintf(buf,sizeof(buf),"%s: %s", pWebSsnListNode->pUsrAppData->keyVal[i].key, pWebSsnListNode->pUsrAppData->keyVal[i].value);
			slist = curl_slist_append(slist, buf);
		}
	}

	strncpy(buf2, pWebSsnListNode->pUsrAppData->outHeader, sizeof(buf2)-1);

	memset(eTagValue,0x0, SIZE_256B);
	strncpy(str,"ETag",sizeof(str)-1);
	ptr = strstr(buf2, str);
	ptr = strstr(ptr, ":");
	ptr += 3;
	if (ptr == NULL){
		return -1;
	}
	sscanf(ptr, "%[a-z,A-Z,0-9] %[/\n]", eTagValue, str1);

	memset(locationValue,0x0, SIZE_256B);
	curl_easy_getinfo(pWebSsnListNode->pUsrAppSsnData->curl, CURLINFO_EFFECTIVE_URL, &new_url); 
	for(i=0;new_url[i]!='?';i++){
		if(new_url[i] == 'c' && new_url[i+1] == 'o' && new_url[i+2] == 'm'){
			break;	
		}
	}
	for(i=i+3;new_url[i]!='?';i++)
		locationValue[k++] = new_url[i];

	if (pWebSsnListNode->pUsrAppData->inDataLength <= 0) {
		strncpy(file, pWebSsnListNode->pUsrAppData->inData, sizeof(file)-1);
		pFp = fopen( file, "rb");
		if ( pFp == NULL ) {
			return ERROR_INVALID_FILE;	 
		}else {
			fd = open(file, O_RDONLY);
			fstat(fd, &fileInfo );
			close(fd);
			fclose(pFp);
			contentSize = fileInfo.st_size;
		}
	}
	else{
		contentSize = pWebSsnListNode->pUsrAppData->inDataLength;
	}

	pData = (TransactionInfo*)ZALLOC(sizeof(TransactionInfo));
	snprintf( pData->data,sizeof(pData->data),"<eventFile><id></id><macAddress>%s</macAddress><contentType>%s</contentType><contentSize>%d</contentSize><path>%s</path><eTag>%s</eTag></eventFile>", pWebSsnListNode->pUsrAppData->mac, contentType, contentSize, locationValue, eTagValue );
	pData->dataSize = strlen(pData->data);

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
	/* specify we want to POST data */
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	/* verbose debug output option */
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	/* use the data provided by user */
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_transaction_callback);
	/* pointer to pass to our read function */
	curl_easy_setopt(curl, CURLOPT_READDATA, pData);

	if (pWebSsnListNode->pUsrAppData->httpsFlag) {
		webAppSetSSLOptions(curl);
	}
	/* Set the expected POST size. */ 
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)(pData->dataSize));
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL,1L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60L);/* 60 seconds - timeout when connecting to web server */
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);/* 60 seconds  - read timeout */
	res = curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
  pWebSsnListNode->pUsrAppData->outResp = response;
  pWebSsnListNode->pUsrAppData->nStatusCode = response;
	res = webAppErrorHandling(curl, res, pWebSsnListNode->pUsrAppData->disableFlag); 

	free(pData);
	curl_slist_free_all(slist);
	curl_easy_cleanup(curl);

	return res;
}

void StopDownloadRequest(void)
{
#ifdef USE_WGET
        char cmdBuf[SIZE_256B];
        memset (cmdBuf,0,sizeof(cmdBuf));
        strncpy(cmdBuf, "killall -9 wget", sizeof(cmdBuf)-1);
        system (cmdBuf);
#else
	gStopDownloadFW = 1;
#endif
}

int dl_progress(void *clientp,double dltotal,double dlnow,double ultotal,double ulnow)
{
    if (dlnow && dltotal)
	APP_LOG("HTTPSWRAPPER",LOG_DEBUG, "downloading:%3.0f%%\r",100*dlnow/dltotal);
    fflush(stdout);    
    if(gStopDownloadFW)
	return CURLE_ABORTED_BY_CALLBACK;
    return CURLE_OK;
}

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, (FILE*)stream);
}

int do_download(char *url, char *outfilename)
{
    CURL *curl = NULL;
    FILE *fp = NULL;
    CURLcode res = CURLE_OK;
    CURLcode curl_retval;
    struct stat st={0};
    long http_response;
    double dl_size;
    char *check = NULL;

    stat(outfilename, &st);

    curl = curl_easy_init();
    if (curl)
    {
        fp = fopen(outfilename,"ab");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, dl_progress);
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM,st.st_size);
        check = strstr(url, "https://");
        if(check)
        {
            webAppSetSSLOptions(curl);
        }
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) 
        {
            curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &dl_size);
            curl_retval=curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_response);

            // The given url is not correct, unset FirmwareUpURL to exit endless retry downloading
            // Removed again for keep trying to firmware update even if downloading stoped or failed.
            // if (dl_size == -1)
            //    UnSetBelkinParameter("FirmwareUpURL");
        }
        curl_easy_cleanup(curl);
        fclose(fp);
    }
    else
        res = CURLE_FAILED_INIT;

    return res;
}

int webAppFileDownload(char *url, char *outfilename)
{
    int ret = -1;

#ifdef USE_WGET
    char buf[SIZE_256B] = {'\0'};
    snprintf(buf, sizeof(buf), "wget %s -O %s", url, outfilename);
    ret = system(buf);
    if (ret != 0) {
    }else {
    }
#else
    while ((ret = do_download(url, outfilename)) != CURLE_OK)
    {
        char *fwUpStr = GetBelkinParameter("FirmwareUpURL");

		if(ret == CURLE_ABORTED_BY_CALLBACK) {
	 	   // Reset g_StopDownloadFW, so that we won't stop retry.
		    gStopDownloadFW = 0;
		    break;
		}
        if(fwUpStr && strlen(fwUpStr) != 0) {
		    pluginUsleep(10000);
            continue;
		}
        else 
        {
            APP_LOG("HTTPSWRAPPER", LOG_DEBUG, "webAppFileDownload exit\n");
            break;
        }
    }
#endif

    return ret;
}
