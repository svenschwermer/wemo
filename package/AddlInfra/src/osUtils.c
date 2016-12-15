/***************************************************************************
*
*
* osUtils.c
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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/syscall.h>
#include "defines.h"
#include "osUtils.h"
#include "logger.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

#ifndef PRODUCT_WeMo_NetCam
// All products except Netcam support robust mutexs
// remove this if/when the toolchain used by netcam is updated
#define ROBUST_MUTEX    1
#endif

int ghndlrsFd = 0;
int gMutexCnt=0;

/************************************************************************
 * Function: osUtilsCreateSocket
 *     creates socket and return socket fd if successfull or some error code
 *     if failed
 *  Parameters:
 *     connMode - connection mode.
 *     connType - connection Type
 *     fdType - socket type GLOBAL or OTHER
 *  Return:
 *     Retuns socket fd if success else failure (< PLUGIN_SUCCESS)
************************************************************************/
int osUtilsCreateSocket(int connMode, int connType, int fdType)
{
	int retVal = PLUGIN_SUCCESS;
	
	if(connType == TCP_CONN) {
		/* Not supported at this moment, Rightnow we only dealing with UDP socket */
					
	} else {
		if(connMode == BROADCAST){
			retVal = socket(AF_INET, SOCK_DGRAM, 0);
    } else{
			retVal = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		}
	}
	if(retVal < PLUGIN_SUCCESS)
		perror("Error in creating UDP socket ");
	if (fdType == GLOBAL) ghndlrsFd = retVal;
	return retVal;
				
}
/************************************************************************
 * Function: osUtilsBindSocket
 *      Socket binds with the provided ipAddr and port
 *  Parameters:
 *     sockFd - socket created
 *     connMode - connection mode.
 *     ipAddr - Local IP Address
 *     port - Local port
 *  Return:
 *     Retuns the return value from bind()
 ************************************************************************/
int osUtilsBindSocket(int sockFd, int connMode, char* ipAddr, int ipPort)
{
    int retVal = PLUGIN_SUCCESS, loop = 0;
    struct sockaddr_in localAddr;

    localAddr.sin_family = AF_INET;
    if(ipAddr ==  NULL) {
	localAddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    } else
	localAddr.sin_addr.s_addr = inet_addr(ipAddr);
    localAddr.sin_port = htons(ipPort);
    if(connMode == BROADCAST) {
	retVal = setsockopt(sockFd , SOL_SOCKET , SO_BROADCAST , (const char *)&loop ,sizeof(int));
	if(retVal < 0) {
	    perror("Unable to set broadcast option");
	    return(retVal);
	}
    }
    retVal = bind(sockFd, (struct sockaddr*)&(localAddr), sizeof(struct sockaddr));
    if (retVal < 0) {
	perror("ERROR in binding UDP socket");
    }
    return (retVal);
}
/************************************************************************
 * Function: osUtilsReadSocket
 *     Read from the created and bind socket
 *  Parameters:
 *     sockFd - socket created
 *     readBuffer - Read Buffer.
 *     timeout - Timeout value if nothing received on socket
 *  Return:
 *     Retuns the length of data read from socket
************************************************************************/
int	osUtilsReadSocket(int sockFd, char* readBuffer, int timeout)
{
	int nRet, sRet=0, selFd;
	char szBuf[MAX_BUF_LEN];
	struct timeval TimeVal;
	struct sockaddr_in tempAddr;
	socklen_t nLen;
	fd_set readFd;

	nLen = sizeof(struct sockaddr);
	selFd = sockFd;
	memset(szBuf, 0, sizeof(szBuf));
	FD_ZERO(&readFd);
	FD_SET(selFd, &readFd);
	TimeVal.tv_sec = timeout;
	TimeVal.tv_usec= 0;
	sRet = select(selFd+1, &readFd , NULL , NULL ,&TimeVal);
	if(sRet < 0) {
		perror("ERROR in select");
		return (sRet);
	} else if(sRet ==0) {
		return (sRet);
	} else {
	    nRet = recvfrom(sockFd,				// Bound socket
		    szBuf,					// Receive buffer
		    sizeof(szBuf),			// Size of buffer in bytes
		    /*MSG_NOSIGNAL | MSG_DONTWAIT,						// Flags*/
		    0,
		    (struct sockaddr*)&(tempAddr),	// Buffer to receive client address 
		    &nLen);					// Length of client address buffer
	    if(nRet > 0) memcpy(readBuffer,szBuf, nRet);
	}
	return nRet;
}
/************************************************************************
 * Function: osUtilsWriteSocket
 *     write to the remote socket
 *  Parameters:
 *     sockFd - socket created
 *     connMode - connection mode.
 *     remoteIP - Remote IP Address
 *     remotePort - Remote port
 *     writeBuffer - write Buffer.
 *     len - Length of Buffer
 *  Return:
 *     Retuns the length of data written to remote socket
 ************************************************************************/
int osUtilsWriteSocket(int sockFd, int connMode, char* remoteIP, int remotePort, char* writeBuffer, int len)
{
    int nRet = 0;
    struct sockaddr_in remoteAddr;
    remoteAddr.sin_family = AF_INET;
    remoteAddr.sin_port = htons(remotePort);
    if(connMode == BROADCAST)
	remoteAddr.sin_addr.s_addr = INADDR_BROADCAST;
    else
	remoteAddr.sin_addr.s_addr = inet_addr(remoteIP);
    nRet = sendto(sockFd,				// Socket
	    writeBuffer,					// Data buffer
	    len,						// Length of data
	    /*MSG_NOSIGNAL | MSG_DONTWAIT,						// Flags*/
	    0,
	    (struct sockaddr*)&(remoteAddr),	// Server address
	    sizeof(struct sockaddr)); // Length of address
    if (nRet  < 0) { perror("ERROR in sending UDP message"); return nRet;}
    return nRet;
}
/************************************************************************
 * Function: osUtilsCreateLock
 *     Create Lock
 *  Parameters:
 *     theLock - Mutex lock variable
 *  Return:
 *     Retuns errorcode returned from pthread_mutex_init
************************************************************************/
int osUtilsCreateLock(pthread_mutex_t *theLock)
{
	int errorCode;
#ifdef ROBUST_MUTEX
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	if((errorCode = pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST)))
	{
	    APP_LOG("UTIL",LOG_DEBUG, "\npthread_mutexattr_setrobust failed [%d]!!!\n", errorCode);
	    return errorCode;
	}
	// PTHREAD_MUTEX_INITIALIZER;
	errorCode=pthread_mutex_init(theLock, &attr);
#else
	errorCode=pthread_mutex_init(theLock, NULL);
#endif
	return errorCode;
}
/************************************************************************
 * Function: osUtilsDestroyLock
 *     Destroy Lock
 *  Parameters:
 *     theLock - Mutex lock variable
 *  Return:
 *     Retuns errorcode returned from pthread_mutex_init
************************************************************************/
int osUtilsDestroyLock(pthread_mutex_t *theLock)
{
	int errorCode;
	
	errorCode=pthread_mutex_destroy(theLock);
	return errorCode;
}
/************************************************************************
 * Function: osUtilsGetLock
 *     Acquire Lock
 *  Parameters:
 *     theLock - Mutex lock variable
 *  Return:
 *     Retuns errorcode returned from pthread_mutex_init
************************************************************************/
int osUtilsGetLock(pthread_mutex_t *theLock)
{
	int errorCode;
	errorCode=pthread_mutex_lock(theLock);
#ifdef ROBUST_MUTEX
	if(EOWNERDEAD == errorCode)
	{
	    APP_LOG("UTIL",LOG_DEBUG, "\nEOWNERDEAD, making mutex consistent\n");
	    if((errorCode = pthread_mutex_consistent(theLock)))
	    {
		APP_LOG("UTIL",LOG_DEBUG, "\npthread_mutexattr_consistent failed [%d]!!!\n", errorCode);
		return errorCode;
	    }
	}
#endif
	return errorCode;
}
/************************************************************************
 * Function: osUtilsTryLock
 *     Conditionally Acquire Lock
 *  Parameters:
 *     theLock - Mutex lock variable
 *  Return:
 *     Retuns errorcode returned from pthread_mutex_trylock
+************************************************************************/
int osUtilsTryLock(pthread_mutex_t *theLock)
{
	int errorCode;
	errorCode=pthread_mutex_trylock(theLock);
#ifdef ROBUST_MUTEX
	if(EOWNERDEAD == errorCode)
	{
	    APP_LOG("UTIL",LOG_DEBUG, "\nTryLock: EOWNERDEAD, making mutex consistent\n");
	    if((errorCode = pthread_mutex_consistent(theLock)))
	    {
		APP_LOG("UTIL",LOG_DEBUG, "\npthread_mutexattr_consistent failed [%d]!!!\n", errorCode);
		return errorCode;
	    }
	}
#endif
	return errorCode;
}
/************************************************************************
 * Function: osUtilsReleaseLock
 *     Release Lock
 *  Parameters:
 *     theLock - Mutex lock variable
 *  Return:
 *     Retuns errorcode returned from pthread_mutex_init
************************************************************************/
int osUtilsReleaseLock(pthread_mutex_t *theLock)
{
	int errorCode;
	errorCode=pthread_mutex_unlock(theLock);
	return errorCode;
}
