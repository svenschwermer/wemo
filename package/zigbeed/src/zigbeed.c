/***************************************************************************
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
#include <stdio.h>
#include <string.h>
#include <errno.h>  
#include <unistd.h>  
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>
#include "zigbeed.h"
#include "zbIPC.h"

#ifdef DBG
/*#define DBGPRINT(fmt, args...)      printf(fmt, ## args)*/
#define DBGPRINT(fmt, args...)      fprintf(stderr, fmt, ## args)
#else
#define DBGPRINT(fmt, args...)
#endif

void usage()
{
	fprintf(stderr, "usage: zigbeed \n");
	fprintf(stderr, "example: zigbeed \n");
}

int main(int argc, char *argv[])
{
	usage();

	StartIpcUdsServer();

	while(1)
	{
		sleep(1);
	}

	return 0;
}

