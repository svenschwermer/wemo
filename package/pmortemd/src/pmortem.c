/***************************************************************************
*
*
* pmortem.c 
*
* Created by Belkin International, Software Engineering on XX/XX/XX.
* Copyright (c) 2012-2015 Belkin International, Inc. and/or its affiliates. All rights reserved.
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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include "pmortem-internal.h"
#include "pmortem.h"

void pmortem_connect_and_send(void *stack, size_t size)
{
	int fd;
	char c = 0;
	long pagesize;
	int size_int = size;

	pagesize = sysconf(_SC_PAGESIZE);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		LOG_ERROR("socket");
	else {
		pid_t pid = getpid();
		struct sockaddr_un address;

		memset(&address, 0, sizeof(address));
		address.sun_family = AF_UNIX;
		strncpy(address.sun_path, PMORTEMD_UDS_PATH,
			sizeof(address.sun_path));
		if (connect(fd, (struct sockaddr *) &address, sizeof(address)) < 0)
		    LOG_ERROR("connect failed");
		else {
			ssize_t n;
			int send_err = 0;

			n = send(fd, &pid, sizeof(pid), 0);
			if (n < 0)
				LOG_ERROR("send pid failed");
			n = send(fd, &stack, sizeof(stack), 0);
			if (n < 0)
				LOG_ERROR("pmortem: send stack address failed");

			//printf("\nstack: %p, pagesize: %d\n", stack, pagesize);

			while ((size_int > 0) && !send_err) {
				//printf("\nsending from %p, %d bytes, size: %d\n", stack, pagesize -((ptrdiff_t) stack & (pagesize - 1)), size_int);

				n = send(fd, stack,
					 pagesize -
					 ((ptrdiff_t) stack & (pagesize - 1)),
					 0);
				if (n < 0) {
						/* We don't log an error here,
						 * it is most likely a bad
						 * address reference.  Rather
						 * than log an error and exit,
						 * just return to let the
						 * caller clean up or abort.
						 */
					send_err = 1;
					printf("\nsend failed for %d bytes\n", pagesize -((ptrdiff_t) stack & (pagesize - 1)));
					break;
				}
				else {
					size_int -= n;
					stack += n;
					//printf("\nstack: %p, size: %d, n: %d\n", stack, size_int, n);
				}

				/* Wait for acknowledgment */
				n = recv(fd, &c, sizeof(c), 0);
			}

		}
		close(fd);
	}
	return;
}
