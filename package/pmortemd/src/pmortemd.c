/***************************************************************************
*
*
* pmortemd.c 
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
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include "pmortem-internal.h"

#define MAX_STACK_DUMP  (8 * 1024)

/* Daemon to capture another process's stack
 */

static void dump(void *data, size_t size, ptrdiff_t offset)
{
	uint32_t *up = data;
	syslog(LOG_ERR, "Stack dump follows\n");

	while (size > 32) {
		syslog(LOG_ERR, "%08tx: %08x %08x %08x %08x   %08x %08x %08x %08x\n",
			offset,
			up[0], up[1], up[2], up[3], up[4], up[5], up[6], up[7]);
		up += 8;
		size -= 32;
		offset += 32;
	}
	if (size) {
		char buffer[128];
		int i = 0, n, len = 0;

		n = snprintf(buffer + len, sizeof(buffer) - len,
			     "%08tx: ", offset);
		if (n > 0)
			len += n;
		while (size) {
			n = snprintf(buffer + len, sizeof(buffer) - len,
				     "%08x ", up[0]);
			if (n > 0)
				len += n;
			++up;
			size -= 4;
			++i;
			if (i == 4) {
				n = snprintf(buffer + len, sizeof(buffer) - len,
					     "  ");
			if (n > 0)
				len += n;
			}
		}
		strncpy(buffer + len, "\n", sizeof(buffer) - len);
		syslog(LOG_ERR, buffer);
	}
}

static void dump_map(pid_t pid)
{
	char name[64];
	char buffer[256];
	FILE *fp;

	snprintf(name, sizeof(name), "/proc/%d/maps", pid);
	fp = fopen(name, "r");
	if (!fp) {
		syslog(LOG_ERR, "Unable to open %s: %s\n",
			name, strerror(errno));
		return;
	}
	while (fgets(buffer, sizeof(buffer), fp))
		syslog(LOG_ERR, "%s", buffer);
	fclose(fp);
}

static void process_incoming(int socket)
{
	pid_t pid = 0;
	void  *vp = NULL;
	char  *buffer;
	ssize_t n;

	buffer = malloc(MAX_STACK_DUMP);
	if (!buffer)
		LOG_ERROR("Unable to allocate receive buffer: %s\n",
			  strerror(errno));
	n = recv(socket, &pid, sizeof(pid), 0);
	if (n != sizeof(pid))
		LOG_ERROR("recv pid failed");
	n = recv(socket, &vp, sizeof(vp), 0);
	if (n < 0)
		LOG_ERROR("recv vp failed");

	syslog(LOG_ERR, "Stack address at crash time: %p", vp);
	dump_map(pid);

	//printf("\npmortemd: before recv\n");
	while ((n = recv(socket, buffer, MAX_STACK_DUMP, 0)) > 0) {
		if (n < 0)
			LOG_ERROR("recv stack data");
		else {
			dump(buffer, n, (ptrdiff_t) vp);
			vp += n;
			//printf("\nReceived %d bytes\n", n);
		}

		/* Acknowlege: let the client finish */
		send(socket, buffer, 1, 0);
	}
	//printf("\npmortemd: after recv\n");
	free(buffer);
}

static void *listener(void)
{
	int fd;

	unlink(PMORTEMD_UDS_PATH);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		LOG_ERROR("socket failed\n");
	else {
		struct sockaddr_un address;

		memset(&address, 0, sizeof(address));
		address.sun_family = AF_UNIX;
		strncpy(address.sun_path, PMORTEMD_UDS_PATH,
			sizeof(address.sun_path));
		if (bind(fd, (struct sockaddr *) &address, sizeof(address)) != 0)
			LOG_ERROR("bind failed");
		else if (listen(fd, 1) < 0)
			LOG_ERROR("listen failed");
		else {
			struct sockaddr_un caddr;
			int connection;
			socklen_t sock_len = 0;

			for (;;) {
				memset(&caddr, 0, sizeof(caddr));
				connection = accept(fd, (struct sockaddr *) &caddr,
						    &sock_len);
				if (connection < 0)
					LOG_ERROR("accept failed");
				process_incoming(connection);
				close(connection);
			}
		}
	}
	return NULL;
}

struct option opts[] = {
        { "help",  0, NULL, 'h' },
        { NULL,    0, NULL, 0 }
};

void usage(const char *mesg)
{
	if (mesg)
		fputs(mesg, stderr);
	syslog(LOG_ERR, "%s [options]\n"
		"  options:\n"
		"    -h|--help             Show this usage message\n",
		*__environ);
}

int main(int argc, char *argv[])
{
	int c;

	openlog("pmortemd", LOG_CONS | LOG_PERROR, LOG_DAEMON);
	while ((c = getopt_long(argc, argv, "h", opts,
				NULL)) != -1) {
		switch (c) {
		case 'h':
			usage(NULL);
			exit(0);
		default:
			usage(NULL);
			exit(1);
		}
	}

	if (optind < argc)
		syslog(LOG_ERR, "Extra arguments %s... ignored\n",
			argv[optind]);

	freopen("/dev/console", "w", stdout);
	freopen("/dev/console", "w", stderr);
	chdir("/tmp");
	daemon(1, 1);
	listener();

	return 0;
}
