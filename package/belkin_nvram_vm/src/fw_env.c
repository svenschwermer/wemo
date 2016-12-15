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
 Name        : fw_env.c
 Author      : Abhishek.Agarwal
 Version     : 15 October' 2012
 Copyright   :
 Description : 
 ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

#define FILE_NVRAM "/tmp/Belkin_settings/nvram.txt"
#define TMP_FILE "/tmp/tmpnvram.txt"
#define MAX_LINE  256

void libNvramInit()
{
    return;
}

static char *envmatch (char * s1, char * s2)
{
    while (*s1 == *s2++)
	if (*s1++ == '=')
	    return s2;
    if (*s1 == '\0' && *(s2 - 1) == '=')
	return s2;
    return NULL;
}

int nvramset(int argc, char *argv[])
{
    FILE *fp_r, *fp_w;
    char line[MAX_LINE] = {'\0'};
    char line2[MAX_LINE] = {'\0'};
    char tmpline[MAX_LINE] = {'\0'};
    char *p = NULL, *name;
    int found = 0, fileNotFound = 0;
    char *str1=NULL;
	 char *strtok_r_temp;

    if (argc < 2 || argc > 3) {
	perror("Invalid set of arguments");
	return -1;
    }

    name = argv[1];
    memset(line, '\0', MAX_LINE);
    memset(line2, '\0', MAX_LINE);
    memset(tmpline, '\0', MAX_LINE);

    fp_r = fopen(FILE_NVRAM, "rb");
    if (!fp_r) {
	fileNotFound = 1;
    }

    fp_w = fopen(TMP_FILE, "wb");
    if (!fp_w) {
	perror("File opening error");
	return 0;
    }

    if(!fileNotFound) {
	while (fgets(line, MAX_LINE, fp_r))
	{
	    strcpy(tmpline, line);
	    str1 = strtok_r(tmpline,"=",&strtok_r_temp);	
	    if (!found && (strcmp(str1, name) == 0))
	    {
		strcpy(line2, line);
		found = 1;
	    } else {
		fputs(line, fp_w);
	    }
	}
    }

    /* Unset the value. Not writing the contents to the new file */
    if(argc < 3) {
	fclose(fp_w);
	if(!fileNotFound) {
	    fclose(fp_r);
	    memset(line, '\0', MAX_LINE);
	    sprintf(line, "mv %s %s", TMP_FILE, FILE_NVRAM);
	    system(line);
	}
	return 0;
    }


    /* Modify the existing value */
    memset(line2, '\0', MAX_LINE);
    strcpy(line2, name);
    strcat(line2, "=");
    strcat(line2, argv[2]);
    strcat(line2, "\n");
    fputs(line2, fp_w);
    if(!fileNotFound)
	fclose(fp_r);
    fclose(fp_w);
    memset(line, '\0', MAX_LINE);
    sprintf(line, "mv %s %s", TMP_FILE, FILE_NVRAM);
    system(line);
    return 0;
}

int nvramget (int argc, char *argv[], char *buffer)
{
    FILE *fp;
    char line[MAX_LINE] = {'\0'};
    char tmpline[MAX_LINE] = {'\0'};
    char *p = NULL, *name;
    char *oldval = NULL;
    char *str1=NULL;
	 char *strtok_r_temp;

    if (argc > 2) {
	perror("Invalid set of arguments");
        return -1;
    }

    name = argv[1];
    memset(line, '\0', MAX_LINE);
    memset(tmpline, '\0', MAX_LINE);

    fp = fopen(FILE_NVRAM, "rb");
    if (!fp) {
	printf("No NVRAM fields\n");
	return 0;
    }


    if (argc == 1) { /* Print all env variables  */
	while (fgets(line, MAX_LINE, fp))
	{
	    if(line[strlen(line)-1] == '\n')
		line[strlen(line)-1] = '\0';

	    printf("%s\n", line);
	}
	fclose(fp);
	return 0;
    }

    while (fgets(line, MAX_LINE, fp))
    {
	strcpy(tmpline, line);
	str1 = strtok_r(tmpline,"=",&strtok_r_temp);
	if ((strcmp(str1, name)==0))
	{
	    if((strcmp(str1, "ClientPass") == 0)) {
		if(fgets(tmpline, MAX_LINE, fp)) {
		    if(!strstr(tmpline,"="))
			strcat(line,tmpline);
		}
	    }
	    if(line[strlen(line)-1] == '\n')
		line[strlen(line)-1] = '\0';

	    oldval = envmatch (name, line);

	    printf("%s=%s\n", name, oldval);
	    if(buffer)
		strcpy(buffer, oldval);
	    fclose(fp);
	    return 0;
	}
    }

    return -1; //Some error encountered
}

void resetNvram() {
    char line[MAX_LINE];

    printf("Erasing the nvram sector\n");

    memset(line, '\0', MAX_LINE);
    sprintf(line, "rm %s; touch %s", FILE_NVRAM, FILE_NVRAM);
    system(line);

    printf("Erased the nvram sector\n");
}

char variablesName[][20] = {"home_id",
			    "SmartDeviceId",
			    "SmartPrivatekey",
			    "plugin_key",
			    "PluginCloudId",
			    "SerialNumber",
			    "wl0_currentChannel"
			};

/* Erasing the nvram sector */
void eraseNvram() {
    FILE *fp_r, *fp_w;
    char line[MAX_LINE] = {'\0'};
    char *p = NULL;
    int num = sizeof(variablesName)/sizeof(variablesName[0]);
    int i;

    memset(line, '\0', MAX_LINE);

    fp_r = fopen(FILE_NVRAM, "rb");
    if (!fp_r) {
	printf("No NVRAM fields\n");
	return;
    }

    fp_w = fopen(TMP_FILE, "wb");
    if (!fp_w) {
	perror("File opening error");
	return;
    }

    while (fgets(line, MAX_LINE, fp_r))
    {
	for(i=0; i<num; i++) {
	    if (p = strstr(line, variablesName[i]) ) {
		fputs(line, fp_w);
		break;
	    }
	}

    }

    fclose(fp_r);
    fclose(fp_w);
    memset(line, '\0', MAX_LINE);
    sprintf(line, "mv %s %s", TMP_FILE, FILE_NVRAM);
    system(line);
    return;
}
