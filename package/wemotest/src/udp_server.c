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

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>


#include "udp_server.h"
#include "instanceIF.h"

/**
 * start_server(const char* szIp, int port)
 *      To start the UDP server for test
 *
 *
 * @param szIp
 * @param port
 * @return
 ***************************************************************/
static sockfd = 0x00;
static struct sockaddr_in cliaddr;
int SEQUENCE_NUMBER = 0x00;

int start_server(const char* szIp, int port)
{
   int n;
   struct sockaddr_in servaddr;
   socklen_t len;
   char mesg[MAX_UDP_BUFFER];


   sockfd=socket(AF_INET, SOCK_DGRAM, 0x00);
   setPort2Reuse(sockfd);       //- Even failure, still move ahead

   bzero(&servaddr,sizeof(servaddr));
   servaddr.sin_family = AF_INET;

   if (0x00 == szIp)
   {
       servaddr.sin_addr.s_addr = INADDR_ANY;
   }
   else
   {
       servaddr.sin_addr.s_addr = inet_addr(szIp);
   }

   servaddr.sin_port=htons(port);
   int rect = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

   if (-1 == rect)
   {
         printf("server bind failure\n");
         perror("test");

         return UDP_FAILURE;
   }
   else
   {
       printf("server binding success: %s\n", inet_ntoa(servaddr.sin_addr));
   }

   printf("listen command\n");
   len = sizeof(cliaddr);

   int isFirst = 0x00;

   for (;;)
   {
       memset(mesg, 0x00, MAX_UDP_BUFFER);
       n = recvfrom(sockfd, mesg, MAX_UDP_BUFFER, 0, (struct sockaddr *)&cliaddr, &len);
       //- Here how to make sure all data received and then can be processed, try three more times to make sure
       printf("%d bytes received:", n);
       printHex(mesg, n);
       processRequest(mesg, n);
   }


    return UDP_SUCCESS;
}

/***
 * stop_server: Stop the UDP server
 *
 *
 *
 * @return
 *
 *
 ***************************************************************************/
int stop_server()
{
  close(sockfd);

  return UDP_SUCCESS;
}


/*************************************************************
 *
 *
 *
 *
 * @param src
 * @param size
 *
 *
 */
void    executeResponse(char* src, int size)
{
    printHex(src, size);
    int nSent = sendto(sockfd, src, size, 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
    if (-1 == nSent)
    {
        printf("UDP sent error\n");
    }
    else
    {
        printf("UDP sent success: %d bytes to: %s\n", size, inet_ntoa(cliaddr.sin_addr));
    }
}

/**
 *
 *
 *
 * @param src
 * @param size
 *
 * This is the router for protocol instance entry
 */
/**
Field   Size    Description
Preamble        1byte   0xFE, the start of a command frame
Command ID      4 bytes Identifier of request command or response result
Result Code     4 bytes Identifier of request result, 0x0000 indicating success, 0xFFFF indicating invalid command
Sequence Number 4 bytes Increment by one for each command, wrap to zero when it reaches the maximum value.
Data Size       4 bytes The effective data size of request response, the value indicating the data size of "Data Content" in bytes
Data Content    0-n bytes       Data content
CheckSum        4 bytes CheckSum of entire data frame through "Preamble" to "Data Content"
*********************************************************************************************/
void processRequest(char* src, int size)
{
  //- Check the data and consistency of the data
  if (0x00 == src || 0x00 == size || size < MIN_UDP_COMMAND_SIZE)
  {
      printf("Invalid command data\n");
      return;
  }

  if (0xFE != ((0xFF) & src[0x00]))
  {
      printf("Unexpected command data\n");
      return;
  }

  //- Get command ID
  unsigned int commandID = 0x00;
  commandID = (0x00FFFFFF) & ((src[1] << 24) + (src[2] << 16) + (src[3] << 8) + (0xFF & src[4]));

  //- Get seqeunce number: 5 - 8 bytes
  SEQUENCE_NUMBER = (0xFFFFFFFF) & ((src[9] << 24) + (src[10] << 16) + (src[11] << 8) + (0xFF & src[12]));

  int payload = (0xFFFFFFFF) & ((src[13] << 24) + (src[14] << 16) + (src[15] << 8) + (0xFF & src[16]));

  //commandID = (0xFF & src[4]);
  printf("Command:%08X, sequence: %d, size: %d\n", commandID, SEQUENCE_NUMBER, payload);
  routeInstance(commandID, src + 17, payload);
}

/**
 *      set port to reused
 *
 * @param socket
 * @return
 */
int setPort2Reuse(int socket)
{
    int isReused = 1;

    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &isReused,sizeof(int)) == -1)
    {
        printf("setsockopt: can not set port to reuse\n");
        return UDP_FAILURE;
    }

    return UDP_SUCCESS;
}
