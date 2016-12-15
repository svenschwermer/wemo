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
 * utility.c
 *
 *  Created on: Oct 5, 2012
 *      Author: wemo
 */
#include <stdlib.h>
#include <stdio.h>
#include "instance.h"

void printHex(unsigned char* src, int size)
{
  int index = 0x00;
  printf("0X");
  for (; index < size; index++)
  {
      printf("%02X", (0xFF & *(src + index)) );
  }

  printf("\n");
}

/**
 *
 *
 *
 * @param dest
 * @param value
 * @return
 *
 */
char*   createPayload(char* dest, int value)
{
    if (0x00 == dest)
       return 0x00;

    dest[0x00] = 0x00;
    dest[0x01] = 0x00;
    dest[0x02] = (value >> 8) & 0xFF;
    dest[0x03] = value & 0xFF;

    return dest;
}

char* createHeaderPayload(int commandID, int response, int SEQ, int payloadsize, char* dest)
{
    dest[0x00] = 0xFE;
    createPayload(dest + COMMAND_INDEX, commandID);
    createPayload(dest + RESP_CODE_INDEX, response);
    createPayload(dest + SEQ_INDEX, SEQ);
    createPayload(dest + PAYLOAD_INDEX, payloadsize);

    return dest;
}


char* createPayloadStr(char* dest, char* src)
{
    if ((0x00 == dest) || (0x00 == src))
    return 0x00;

    strcpy(dest, src);
    return dest;
}

/**
 * readScriptResult
 *      Read script result from temp file, not use popen yet since some error observed, may change later
 *
 * @param script
 * @param dest
 * @param size
 * @return
 *
 *
 */
char* readScriptResult(char* script, char* dest, int size, int isReadAll/*0x00, NO, just first */)
{

    FILE* pfScript = 0x00;
    pfScript = fopen(script, "r");
    if (0x00 != pfScript)
    {
          //- pipe is there and please read the first line and the store to buff
        if (!isReadAll)
          fgets(dest, size, pfScript);
        else
          fread(dest, 1, size, pfScript);

        int size = strlen(dest);
        if (size > 0x01)
        {
           if (dest[size -1] == '\r' || dest[size -1] == '\n')
                dest[size -1] = 0x00;
        }
    }
    else
    {
        printf("cant not read file:%s", script);
    }

    fclose(pfScript);

    return dest;
}
