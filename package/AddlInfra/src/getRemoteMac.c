/***************************************************************************
*
*
* getRemoteMac.c
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
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if_packet.h>  
#include <net/if.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
//#include <net/if.h>
#include "global.h"
#include "logger.h"
#include "utils.h"
#include "getRemoteMac.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

char gGatewayMAC[MAX_MAC_LEN];

pthread_attr_t arpThreadAttr;
pthread_t arpThreadId=-1;



void* resolveArpRequest(void* arg)
{

	struct in_addr src_in_addr,targ_in_addr;
	struct arp_packet pkt, resp;
	int sock;
	socklen_t len;
	int err;
	unsigned char hwaddr[MAX_MAC_LEN]={'\0'};
	char ipaddr[MAX_MAC_LEN]={'\0'};
	struct sockaddr_ll device;
	char* szMac;
	char *pIp = NULL;
	char* remoteIP = (char *)arg;
#ifdef PRODUCT_WeMo_NetCam
	char *wan_if;
	char wanif_mac[MAX_MAC_LEN] = {'\0'};
#endif

	tu_set_my_thread_name( __FUNCTION__ );
	if(remoteIP == NULL)
	{
		APP_LOG("ARP", LOG_ERR,"Invalid remote IP address");
		return (void *)FAILURE;
	}

	sock = socket (PF_PACKET, SOCK_RAW, htons (ETH_P_ARP));
	if(sock<0)
	{
		APP_LOG("ARP", LOG_ERR,"socket: %d", errno);
		free(arg);
		return (void *)FAILURE;
	}

	memset(&gGatewayMAC, 0, sizeof(gGatewayMAC));
	memset(&pkt, 0, sizeof(struct arp_packet));
	memset(&resp, 0, sizeof(struct arp_packet));

	pkt.frame_type = htons(ARP_FRAME_TYPE);
	pkt.hw_type = htons(ETHER_HW_TYPE);
	pkt.prot_type = htons(IP_PROTO_TYPE);
	pkt.hw_addr_size = ETH_HW_ADDR_LEN;
	pkt.prot_addr_size = IP_ADDR_LEN;
	pkt.op=htons(OP_ARP_REQUEST);

	get_hw_addr(pkt.rcpt_hw_addr,(unsigned char *)"00:00:00:00:00:00");
	get_hw_addr(pkt.targ_hw_addr,(unsigned char *)"FF:FF:FF:FF:FF:FF");

#ifdef PRODUCT_WeMo_NetCam
	wan_if = GetBelkinParameter("wan_if");
        if((wan_if != NULL) && (strlen("wan_if") != 0)) {
		printf("WAN interface = %s\n", wan_if);
		GetMACAddress_ext(wan_if, wanif_mac);
		szMac = wanif_mac;
	} else	
#endif
	szMac = GetMACAddress();

	strncpy((char *)hwaddr, szMac, sizeof(hwaddr)-1);

	get_hw_addr(pkt.src_hw_addr,hwaddr);
	get_hw_addr(pkt.sndr_hw_addr,hwaddr);

#if 0
	 pIp = GetWanIPAddress(); 
	strncpy(ipaddr, pIp, sizeof(ipaddr)-1);

	APP_LOG("ARP", LOG_DEBUG,"Device MAC addr: %s, ipaddr: %s", hwaddr, ipaddr);

	if(SUCCESS != get_ip_addr(&src_in_addr,ipaddr))
	{
		APP_LOG("ARP", LOG_ERR,"Failed to parse source IP address");
		free(arg);
		close(sock);
		return (void *)FAILURE;
	}

	if(SUCCESS != get_ip_addr(&targ_in_addr, remoteIP))
	{
		APP_LOG("ARP", LOG_ERR,"Failed to parse target IP address");
		free(arg);
		close(sock);
		return (void *)FAILURE;
	}

	memcpy(pkt.sndr_ip_addr,&src_in_addr,IP_ADDR_LEN);
	memcpy(pkt.rcpt_ip_addr,&targ_in_addr,IP_ADDR_LEN);
#endif
	bzero(pkt.padding,TRAILER_BYTES);

	if ((device.sll_ifindex = if_nametoindex (DEFAULT_DEVICE)) == 0) 
	{
		APP_LOG("ARP", LOG_ERR,"if_nametoindex() failed to obtain interface index");
		free(arg);
		close(sock);
		return (void *)FAILURE;
	}

	APP_LOG("ARP", LOG_DEBUG,"Index for interface %s is %i", DEFAULT_DEVICE, device.sll_ifindex);
	// Fill out sockaddr_ll.
	device.sll_family = AF_PACKET;
	memcpy (device.sll_addr, hwaddr, ETH_HW_ADDR_LEN);
	device.sll_halen = htons (ETH_HW_ADDR_LEN);

	/* Loop around and see for our response */

	while(1)
	{
		pIp = GetWanIPAddress();
                strncpy(ipaddr, pIp, sizeof(ipaddr)-1);

                APP_LOG("ARP", LOG_DEBUG,"Device MAC addr: %s, ipaddr: %s", hwaddr, ipaddr);

		if(!strcmp(ipaddr,"0.0.0.0"))
		{     
		    pluginUsleep(1000000); 
		    continue;
		}

                if(SUCCESS != get_ip_addr(&src_in_addr,ipaddr))
                {
                      APP_LOG("ARP", LOG_ERR,"Failed to parse source IP address");
                      free(arg);
                      close(sock);
                      return (void *)FAILURE;
                }

                remoteIP = GetWanDefaultGateway();

                if(SUCCESS != get_ip_addr(&targ_in_addr, remoteIP))
                {
                      APP_LOG("ARP", LOG_ERR,"Failed to parse target IP address");
                      free(arg);
                      close(sock);
                      return (void *)FAILURE;
                }

                memcpy(pkt.sndr_ip_addr,&src_in_addr,IP_ADDR_LEN);
                memcpy(pkt.rcpt_ip_addr,&targ_in_addr,IP_ADDR_LEN);

	
		/* Send out the ARP REQUEST packet */

		if(sendto(sock,&pkt,sizeof(pkt),0,(struct sockaddr *) &device,sizeof(device)) < 0)
		{
			APP_LOG("ARP", LOG_ERR,"sendto: %d", errno);
			free(arg);
			close(sock);
			return (void *)FAILURE;
		}

		pluginUsleep(100*1000);

		/* Wait for the ARP RESPONSE packet */
		len=sizeof(device);
		if(recvfrom(sock, &resp, sizeof(resp), MSG_DONTWAIT, (struct sockaddr *) &device, &len)<0)
		{
			err = errno;
			APP_LOG("ARP", LOG_ERR,"recvfrom: %d", err);
			if( (err == EAGAIN) || (err == EWOULDBLOCK))
			{
				//APP_LOG("ARP", LOG_DEBUG,"No data on socket, resend and wait");
				pluginUsleep(2000*1000);

#if 0
			   	pIp = GetWanIPAddress(); 
				strncpy(ipaddr, pIp, sizeof(ipaddr)-1);

				APP_LOG("ARP", LOG_DEBUG,"Device MAC addr: %s, ipaddr: %s", hwaddr, ipaddr);

				if(SUCCESS != get_ip_addr(&src_in_addr,ipaddr))
				{
				    APP_LOG("ARP", LOG_ERR,"Failed to parse source IP address");
				    free(arg);
				    close(sock);
				    return (void *)FAILURE;
				}
				
				remoteIP = GetWanDefaultGateway();

				if(SUCCESS != get_ip_addr(&targ_in_addr, remoteIP))
				{
				    APP_LOG("ARP", LOG_ERR,"Failed to parse target IP address");
				    free(arg);
				    close(sock);
				    return (void *)FAILURE;
				}

				memcpy(pkt.sndr_ip_addr,&src_in_addr,IP_ADDR_LEN);
				memcpy(pkt.rcpt_ip_addr,&targ_in_addr,IP_ADDR_LEN); 
#endif
				continue;
				
			}
			free(arg);
			close(sock);
			arpThreadId = -1;
			return (void *)FAILURE;
		}
		else
		{
			/*
			 * Check the following:
			 * OpCode is REPLY
			 * Sender IP address matches the Recipient IP address of our request
			 */
			if((ntohs(resp.op) == OP_ARP_REPLY) && (0 == memcmp(resp.sndr_ip_addr, pkt.rcpt_ip_addr, IP_ADDR_LEN)))
			{
				APP_LOG("ARP", LOG_DEBUG,"Got what we wanted!!");
				free(arg);
				close(sock);
				break;
			}
		}

		pluginUsleep(1000*1000);
	}

	snprintf(gGatewayMAC, sizeof(gGatewayMAC), "%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X", 
			resp.sndr_hw_addr[0], 
			resp.sndr_hw_addr[1], 
			resp.sndr_hw_addr[2], 
			resp.sndr_hw_addr[3], 
			resp.sndr_hw_addr[4], 
			resp.sndr_hw_addr[5]);
	APP_LOG("ARP", LOG_CRIT,"ARP Device MAC: %s, Remote MAC: %s", hwaddr, gGatewayMAC);

	arpThreadId = -1;

	return SUCCESS;
}


int get_ip_addr(struct in_addr* in_addr,char* str)
{
  struct hostent *hostp;

  in_addr->s_addr=inet_addr(str);
  if(in_addr->s_addr == -1){
	if( (hostp = gethostbyname(str)))
	  bcopy(hostp->h_addr,in_addr,hostp->h_length);
	else {
	  APP_LOG("ARP", LOG_ERR,"send_arp: unknown host %s",str);
	  return FAILURE;
	}
  }
  return SUCCESS;
}

int get_hw_addr(unsigned char* buf,unsigned char* str)
{

  int i;
  char c,val;

  for(i=0;i<ETH_HW_ADDR_LEN;i++)
  {
	if( !(c = tolower(*str++))) 
	  die("Invalid hardware address1");
	
	if(isdigit(c)) 
	  val = c-'0';
	else 
	  if(c >= 'a' && c <= 'f') 
		val = c-'a'+10;
	else 
	  die("Invalid hardware address2");

	*buf = val << 4;
	
	if( !(c = tolower(*str++))) 
	  die("Invalid hardware address3");
	if(isdigit(c)) 
	  val = c-'0';
	else if(c >= 'a' && c <= 'f') 
	  val = c-'a'+10;
	else 
	  die("Invalid hardware address4");

	*buf++ |= val;

	if(*str == ':')
	  str++;
  }
  return SUCCESS;
}

int getRouterMAC(char* routerIP)
{
	int retVal = SUCCESS;
	char* remoteIP;

	if(arpThreadId != -1) {
	    /* ARP thread exists */
	    return SUCCESS;
	}

	pthread_attr_init(&arpThreadAttr);
	pthread_attr_setdetachstate(&arpThreadAttr,PTHREAD_CREATE_DETACHED);

	remoteIP = (char *)MALLOC(MAX_MAC_LEN);
	if(NULL == remoteIP)
	{
		APP_LOG("ARP",LOG_ERR, "Unable to allocate memory for IP address");
		return FAILURE;
	}

	strncpy(remoteIP, routerIP, MAX_MAC_LEN-1);

	retVal = pthread_create(&arpThreadId,&arpThreadAttr,(void*)&resolveArpRequest, (void *)remoteIP);

	if(retVal < SUCCESS) 
	{
	    APP_LOG("ARP",LOG_ERR, "Resolve ARP request thread creation failed");
	    return FAILURE;
	}

	return SUCCESS;
}
