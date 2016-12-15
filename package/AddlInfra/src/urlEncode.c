/***************************************************************************
*
*
* urlEncode.c
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
#include <stdlib.h>
#include <string.h>
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

static const char * const urlCodeMap[256] = {
   NULL, "%01", "%02", "%03", "%04", "%05", "%06", "%07", "%08", "%09",
   "%0A", "%0B", "%0C", "%0D", "%0E", "%0F", "%10", "%11", "%12", "%13",
   "%14", "%15", "%16", "%17", "%18", "%19", "%1A", "%1B", "%1C", "%1D",
   "%1E", "%1F", "+",   "%21", "%22", "%23", "%24", "%25", "%26", "%27",
   "%28", "%29", NULL,  "%2B", "%2C", NULL,  NULL,  "%2F", NULL,  NULL,
   NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  "%3A", "%3B",
   "%3C", "%3D", "%3E", "%3F", NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
   NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
   NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
   NULL,  "%5B", "%5C", "%5D", "%5E", NULL,  "%60", NULL,  NULL,  NULL,
   NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
   NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,  NULL,
   NULL,  NULL,  NULL,  "%7B", "%7C", "%7D", "%7E", "%7F", "%80", "%81",
   "%82", "%83", "%84", "%85", "%86", "%87", "%88", "%89", "%8A", "%8B",
   "%8C", "%8D", "%8E", "%8F", "%90", "%91", "%92", "%93", "%94", "%95",
   "%96", "%97", "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
   "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7", "%A8", "%A9",
   "%AA", "%AB", "%AC", "%AD", "%AE", "%AF", "%B0", "%B1", "%B2", "%B3",
   "%B4", "%B5", "%B6", "%B7", "%B8", "%B9", "%BA", "%BB", "%BC", "%BD",
   "%BE", "%BF", "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
   "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF", "%D0", "%D1",
   "%D2", "%D3", "%D4", "%D5", "%D6", "%D7", "%D8", "%D9", "%DA", "%DB",
   "%DC", "%DD", "%DE", "%DF", "%E0", "%E1", "%E2", "%E3", "%E4", "%E5",
   "%E6", "%E7", "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
   "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7", "%F8", "%F9",
   "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

/*********************************************************************
 *
 * Function    :  urlEncode
 *
 * Description :  Encodes a string so it can be used in a URL
 *                query string.  Replaces special characters with
 *                the appropriate %xx codes.
 *
 * Parameters  :
 *          1  :  s = String to encode.  Null-terminated.
 *
 * Returns     :  Encoded string, newly allocated on the heap.
 *                If s is NULL, or on out-of memory, returns NULL.
 *********************************************************************/
char * urlEncode(const char *s)
{
   char * buf;

   if (s == NULL)
   {
      return NULL;
   }

   /* each input char can expand to at most 3 chars */
   buf = (char *) MALLOC((strlen(s) * 3) + 1);

   if (buf)
   {
      char c;
      char * p = buf;
      while( (c = *s++) != '\0')
      {
         const char * replaceWith = urlCodeMap[(unsigned char) c];
         if (replaceWith != NULL)
         {
            strncpy(p, replaceWith, (strlen(s) * 3));
            p += strlen(replaceWith);
         }
         else
         {
            *p++ = c;
         }
      }

      *p = '\0';

   }

   return(buf);
}
