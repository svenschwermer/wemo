/***************************************************************************
*
*
* itc.c
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
#include "itc.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/errno.h>
#include "logger.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

ipcIF	ipcIFArray[MAX_THREAD_NUMBER];
int gMainHealth=0;

void initIPC()
{ 
  int loop = 0x00;
  pQueue tmp = NULL;
  
  for (loop = 0x00; loop < MAX_THREAD_NUMBER; loop++)
  {
    pthread_mutex_init(&ipcIFArray[loop].mutex, NULL);
    sem_init(&ipcIFArray[loop].semap, 0, 0);
    tmp = (pQueue)(MALLOC(sizeof(Queue)));
    tmp->header = NULL;
    tmp->tailer = NULL;
    ipcIFArray[loop].recvQueue = tmp;
  }

  APP_LOG("UTIL",LOG_DEBUG, "Threads inter-communicaiton initialization finished\n");
}


void SendMessage(PLUGIN_E_THREAD_INDEX threadIndex, pMessage msg)
{
  if (threadIndex >= MAX_THREAD_NUMBER)
    return;
  
  pthread_mutex_lock(&ipcIFArray[threadIndex].mutex);
  
  if (NULL == ipcIFArray[threadIndex].recvQueue->header)
  {
    //-the first one
    pNode tmpNode = (pNode)MALLOC(sizeof(Node));
    if(!tmpNode) 
    {
	APP_LOG("ITC",LOG_ERR,"tmpNode Malloc Failed...");
	exit(0);
    }
    tmpNode->message = msg;
    tmpNode->next    = NULL;

    ipcIFArray[threadIndex].recvQueue->header = tmpNode;
    //Tailer and header pointing to same

    ipcIFArray[threadIndex].recvQueue->tailer =
                    ipcIFArray[threadIndex].recvQueue->header;
  }
  else
  {
    //Just append to tailer
    pNode tmpNode = (pNode)MALLOC(sizeof(Node));
    if(!tmpNode) 
    {
	APP_LOG("ITC",LOG_ERR,"tmpNode Malloc Failed...");
	exit(0);
    }
    tmpNode->message = msg;
    tmpNode->next = NULL;
    //-Append to tailer
    ipcIFArray[threadIndex].recvQueue->tailer->next = tmpNode;
    //- Chanhge tailler to current node
    ipcIFArray[threadIndex].recvQueue->tailer = tmpNode;
  }
  pthread_mutex_unlock (&ipcIFArray[threadIndex].mutex);
  sem_post(&ipcIFArray[threadIndex].semap);  
  
}


void SendMessage2App(pMessage msg)
{
    Node* node = (pNode)MALLOC(sizeof(Node));

    if (0x00 == node)
    {
         return;
    }

    node->message = msg;
    node->next = NULL;

	pthread_mutex_lock(&ipcIFArray[0x00].mutex);

    if(ipcIFArray[0x00].recvQueue != NULL)
    {
	if (0x00 == ipcIFArray[0x00].recvQueue->header)
	{
		ipcIFArray[0x00].recvQueue->header = node;
		ipcIFArray[0x00].recvQueue->tailer = ipcIFArray[0x00].recvQueue->header;
	}
	else
	{
		ipcIFArray[0x00].recvQueue->tailer->next = node;
		//- Chanhge tailler to current node
		ipcIFArray[0x00].recvQueue->tailer = node;
	}
    }
	pthread_mutex_unlock (&ipcIFArray[0x00].mutex);
	sem_post(&ipcIFArray[0x00].semap);  
  
}

pNode readMessage(PLUGIN_E_THREAD_INDEX threadIndex)
{
    pNode recvNode = 0x00;
    if(threadIndex == PLUGIN_E_MAIN_THREAD)
    {
trywait:
	if(-1 == sem_trywait(&ipcIFArray[threadIndex].semap))
	{
		int err = errno;
		if(err == EAGAIN || err == EINTR)
		{
			//punch health 
			gMainHealth++;
			sleep(1);
			goto trywait;
		}
		else
		{
			APP_LOG("UTIL",LOG_DEBUG, "\nsem_trywait failed with err: %d", err);
			return recvNode;
		}
	}
    }
    else
    	sem_wait(&ipcIFArray[threadIndex].semap);
			gMainHealth++;
    pthread_mutex_lock(&ipcIFArray[threadIndex].mutex);
    if (ipcIFArray[threadIndex].recvQueue->header)
    {
	  recvNode = ipcIFArray[threadIndex].recvQueue->header;
	  ipcIFArray[threadIndex].recvQueue->header =
			  ipcIFArray[threadIndex].recvQueue->header->next;


	  //- Check the tailer if the header is come to the first;
	  if (0x00 == ipcIFArray[threadIndex].recvQueue->header)
	  {
	      ipcIFArray[threadIndex].recvQueue->tailer = 0x00;
	  }
	  else if (0x00 == ipcIFArray[threadIndex].recvQueue->header->next)
	  {
		  ipcIFArray[threadIndex].recvQueue->tailer =
				  ipcIFArray[threadIndex].recvQueue->header;
	  }


    }
    else
    {
	  
    }

    pthread_mutex_unlock(&ipcIFArray[threadIndex].mutex);
    return recvNode;
}


pMessage createMessage(int ID, void* payload, int size)
{
    pMessage msg = 0x00;
    msg = (pMessage)MALLOC(sizeof(Message));
    if(!msg) 
    {
	APP_LOG("ITC",LOG_ERR,"Message Malloc Failed...");
	exit(0);
    }
    msg->ID = ID;
	if ((0x00 != payload) && (size > 0))
	{
		msg->message =(pMessage) MALLOC(size);
		if(!msg->message) 
		{
		    APP_LOG("ITC",LOG_ERR,"msg->Message Malloc Failed...");
		    exit(0);
		}
		memcpy(msg->message, payload, size);
	}
	else
		msg->message = 0x00;
    
    return msg;
}

int openApUpnp(void)
{
	pMessage msg = createMessage(NETWORK_AP_OPEN_UPNP, 0x00, 0x00);
	SendMessage2App(msg);
	return 0;
}
