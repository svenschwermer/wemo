/***************************************************************************
*
*
* asyncDNS.c
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
#include <ares.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "logger.h"
#include "defines.h"
#include "asyncDNS.h"

static void state_cb(void *data, int s, int read, int write)
{
  APP_LOG("ASYNCDNS", LOG_DEBUG, "Change state fd %d read:%d write:%d", s, read, write);
}


void callback_def(void *arg, int status, int timeouts, struct hostent *host)
{

	if(!host || status != ARES_SUCCESS){
		APP_LOG("ASYNCDNS", LOG_ERR, "Failed to lookup %s", ares_strerror(status));
		return;
	}

  APP_LOG("ASYNCDNS", LOG_DEBUG, "Found address name %s", host->h_name);
	char ip[INET6_ADDRSTRLEN];
	int i = 0;

	for (i = 0; host->h_addr_list[i]; ++i) {
		inet_ntop(host->h_addrtype, host->h_addr_list[i], ip, sizeof(ip));
		APP_LOG("ASYNCDNS", LOG_DEBUG, "Found resolved IP %s", ip);
	}
  APP_LOG("ASYNCDNS", LOG_DEBUG, "Going out of callback");
}

static void wait_ares(ares_channel channel)
{
	for(;;){
		struct timeval *tvp, tv;
		fd_set read_fds, write_fds;
		int nfds;

		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);
		nfds = ares_fds(channel, &read_fds, &write_fds);
		if(nfds == 0){
			break;
		}
		tvp = ares_timeout(channel, NULL, &tv);
		select(nfds, &read_fds, &write_fds, NULL, tvp);
		ares_process(channel, &read_fds, &write_fds);
	}
}

int resolveDNSToIP(char* domain, int flag, void* result, async_dns_callback callback)
{
	ares_channel channel;
	int status;
	struct ares_options options;
	int optmask = 0;
		
  APP_LOG("ASYNCDNS", LOG_DEBUG, "Entry for: %s", domain);

	status = ares_library_init(ARES_LIB_INIT_ALL);
	if (status != ARES_SUCCESS){
		APP_LOG("ASYNCDNS", LOG_ERR, "ares_library_init: %s", ares_strerror(status));
		return PLUGIN_FAILURE;
	}
	//options.sock_state_cb_data;
	options.sock_state_cb = state_cb;
	optmask |= ARES_OPT_SOCK_STATE_CB;

	status = ares_init_options(&channel, &options, optmask);
	if(status != ARES_SUCCESS) {
		APP_LOG("ASYNCDNS", LOG_ERR, "ares_init_options: %s", ares_strerror(status));
		return PLUGIN_FAILURE;
	}

	ares_gethostbyname(channel, domain, AF_INET, callback, NULL);
	wait_ares(channel);
	ares_destroy(channel);
	ares_library_cleanup();
  APP_LOG("ASYNCDNS", LOG_DEBUG, "Exit for: %s", domain);
	return PLUGIN_SUCCESS;
}
