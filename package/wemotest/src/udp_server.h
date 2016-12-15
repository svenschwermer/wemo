/***************************************************************************
*
*
*
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
/**********************************************************************************
 * udp_server.c
 *
 *  Created on: Oct 3, 2012
 *      Author: simon.xiang
 *
 *  @ Description:
 *      This file contains all UDP server related APIs
 *********************************************************************************/

#ifndef __UDP_SERVER_H__
#define __UDP_SERVER_H__


#define         UDP_SUCCESS             0x00
#define         UDP_FAILURE             0x01
#define         MAX_UDP_BUFFER          1024     //- 1K bytes
#define         MIN_UDP_COMMAND_SIZE    1


int     start_server(const char* szIp, int port);
int     stop_server();
void    processRequest(char* src, int size);
void    executeResponse(char* src, int size);

extern int SEQUENCE_NUMBER;

int setPort2Reuse(int socket);

#endif
