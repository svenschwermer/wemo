/***************************************************************************
*
*
* pmtest.c 
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
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include "pmortem.h"

#if !defined(min)
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

int nop(uint32_t *arg);

/* Utility to test SIGSEGV traceback on WeMo hardware.
 */

static inline void fatal(const char *x, ...)
{
        va_list ap;

        va_start(ap, x);
        vfprintf(stderr, x, ap);
        va_end(ap);
        exit(EXIT_FAILURE);
}

static void sig_segv(int signum)
{
	uint32_t *up = (uint32_t *) &signum;

	pmortem_connect_and_send(up, 8 * 1024);
	fprintf(stderr, "SIGSEGV [%d].  Best guess fault address: %08x, ra: %08x, sig return: %p\n",
		signum, up[8], up[72], __builtin_return_address(0));

	_exit(1);
}

void level3(uint32_t arg)
{
	uint32_t value = arg + 1;

	*(uint32_t *) arg = nop(&value);
}

void level2(uint32_t arg)
{
	char buffer[56];
	uint32_t value = arg + 1;

	memset(buffer, 0, sizeof(buffer));
	nop((uint32_t *) buffer);
	level3(nop(&value));
}

void level1(uint32_t arg)
{
	uint32_t value = arg + 1;

	level2(nop(&value));
}

void level0(uint32_t arg)
{
	uint32_t value = arg + 1;

	level1(nop(&value));
}

void start_crash(uint32_t arg)
{
	uint32_t value = arg + 1;

	level0(nop(&value));
}

void clear_some_stack(void)
{
	char buffer[2048];

	memset(buffer, 0, sizeof(buffer));
	printf("stack: %p-%p\n", buffer, buffer + sizeof(buffer));
	nop((void *) buffer);
}

struct option opts[] = {
        { "help",  0, NULL, 'h' },
        { NULL,    0, NULL, 0 }
};

void usage(const char *mesg)
{
	if (mesg)
		fputs(mesg, stderr);
	fprintf(stderr, "%s [options]\n"
		"  options:\n"
		"    -h|--help             Show this usage message\n",
		*__environ);
}

int main(int argc, char *argv[])
{
	struct sigaction action;
	int c;

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
		fprintf(stderr, "Extra arguments %s... ignored\n",
			argv[optind]);

	memset(&action, 0, sizeof(action));
	action.sa_flags = (SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART);
	action.sa_handler = sig_segv;
	if (sigaction(SIGSEGV, &action, NULL))
		fatal("sigaction failed: %s", strerror(errno));

	clear_some_stack();
	start_crash(0x1234);

	return 0;
}
