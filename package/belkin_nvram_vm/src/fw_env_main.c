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
/*
 ============================================================================
 Name        : fw_env_main.c
 Author      : Abhishek.Agarwal
 Version     : 15 October' 2012
 Copyright   :
 Description : 
 ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "fw_env.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

#define	CMD_PRINTNVRAM	"nvram_get"
#define CMD_SETNVRAM	"nvram_set"
#define CMD_RESTORE	"restore"
#define CMD_RESET	"reset"
#define CMD	    "nvram"

int
main(int argc, char *argv[])
{
    char *p;
    char *cmdname = *argv;

    if ((p = strrchr (cmdname, '/')) != NULL) {
	cmdname = p + 1;
    }

    if (strcmp(cmdname, CMD_PRINTNVRAM) == 0) {

	if (nvramget (argc, argv, NULL) != 0)
	    return (EXIT_FAILURE);

	return (EXIT_SUCCESS);

    } else if (strcmp(cmdname, CMD_SETNVRAM) == 0) {

	if (nvramset (argc, argv) != 0)
	    return (EXIT_FAILURE);

	return (EXIT_SUCCESS);

    } else if (strcmp(cmdname, CMD) == 0) {
	if (strcmp(argv[1], CMD_RESTORE) == 0) {
	    eraseNvram();
	    return (EXIT_SUCCESS);
	}

	if (strcmp(argv[1], CMD_RESET) == 0) {
	    resetNvram();
	    return (EXIT_SUCCESS);
	}

	if(argc != 3) {
	    printf("Incorrect set of arguments\n");
	    return (EXIT_FAILURE);
	}

	int len=0, i=0, j=0;
	int argCount=0;
	char **argValue;
	char dumPtr[20] = {'\0'};

	argCount = argc;

	if(strcmp(argv[1], "set") == 0) {
	    strcpy(dumPtr, "nvram_set");
	} else if (strcmp(argv[1], "get") == 0) {
	    strcpy(dumPtr, "nvram_get");
	} else {
	    printf("Invalid Command\n");
	    return (EXIT_FAILURE);
	}

	argValue = (char **)malloc(argCount * sizeof(char *));

	len = strlen(dumPtr);
	j=0;
	argValue[j] = (char *)malloc((len+1) * sizeof(char));
	strcpy(argValue[j], dumPtr);
	j++;

	char buff[256] = {'\0'};
	strcpy(buff, argv[2]);
	char *ptr = buff;
	char field[32];
	int n, count=0, status=0;
	while ( (sscanf(ptr, "%31[^=]%n", field, &n) == 1) && count != 2)
	{
	    len = strlen(field);
	    argValue[j] = (char *)malloc((len+1) * sizeof(char));
	    strcpy(argValue[j], field);
	    count++; j++;

	    ptr += n; /* advance the pointer by the number of characters read */
	    if ( *ptr != '=' )
	    {
		break; /* didn't find an expected delimiter, done? */
	    }
	    ++ptr; /* skip the delimiter */
	}

	if(count == 1)
	    --argCount;

	if(strcmp(argValue[0], "nvram_set") == 0) {
	    status = nvramset (argCount, argValue);

	    /* Freeing the allocated memory */
	    if(count == 1) argCount++;
	    for(i=0; i<argCount; i++)
		free(argValue[i]);
	    free(argValue);

	    if (status != 0)
		return (EXIT_FAILURE);
	} else {
	    status = nvramget (argCount, argValue, NULL);

	    /* Freeing the allocated memory */
	    if(count == 1) argCount++;
	    for(i=0; i<argCount; i++)
		free(argValue[i]);
	    free(argValue);

	    if (status != 0)
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
    }

    fprintf (stderr,
	    "Identity crisis - may be called as `" CMD_PRINTNVRAM
	    "' or as `" CMD_SETNVRAM "' or as `" CMD "' but not as `%s'\n",
	    cmdname);
    return (EXIT_FAILURE);
}
