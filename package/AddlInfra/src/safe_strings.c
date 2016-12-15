/***************************************************************************
*
* safe_strings.c
*
* Created by Belkin International, Software Engineering on 1/16/2015.
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
#include <stddef.h>
#include <string.h>
#include <pthread.h>

#include "defines.h"
#include "logger.h"

static const char *GetFileFromPath(const char *Path);

static const char *GetFileFromPath(const char *Path)
{
   const char *Ret = strrchr(Path,'/');

   if(Ret == NULL) {
      Ret = Path;
   }
   else {
      Ret++;
   }

   return Ret;
}

#if !defined(BOARD_PVT) || defined(DEBUG)
void resetSystem(void);

char *safe_strcpy(char *Dst,const char *Src,int DstLen,const char *Path,int Line)
{
   char *Ret = NULL;

   do {
      if(Dst == NULL || Src == NULL || DstLen == sizeof(char *)) {
         const char *File = GetFileFromPath(Path);

         if(Dst == NULL) {
            syslog(LOG_ERR,"Error - %s:%d called SAFE_STRCPY with a "
                   "NULL destination pointer\n",File,Line);

         }
         else if(DstLen == sizeof(char *)) {
            syslog(LOG_WARNING,"Warning - %s:%d might have called SAFE_STRCPY "
						 "with a bare character pointer\n",File,Line);
         }

         if(Src == NULL) {
            syslog(LOG_ERR,"Error - %s:%d called SAFE_STRCPY with a "
                   "NULL source pointer\n",File,Line);
            break;
         }
      }

      if(Dst == NULL || Src == NULL) {
         break;
      }

      Ret = strncpy(Dst,Src,DstLen);

      if(Dst[DstLen-1] != 0) {
         const char *File = GetFileFromPath(Path);

         Dst[DstLen-1] = 0;
         syslog(LOG_WARNING,"Warning - SAFE_STRCPY called from %s:%d "
					 "truncated string to '%s'\n",File,Line,Dst);
      }
   } while(0);

   return Ret;
}

char *safe_strcat(char *Dst,const char *Src,int DstLen,const char *Path,int Line)
{
   char *Ret = NULL;
   int DstStrLen;
   int SrcStrLen;

   do {
      if(Dst == NULL || Src == NULL || DstLen == sizeof(char *)) {
         const char *File = GetFileFromPath(Path);

         if(Dst == NULL) {
            syslog(LOG_ERR,"Error - %s:%d called SAFE_STRCAT with a "
                   "NULL destination\n",File,Line);
         }
         else if(DstLen == sizeof(char *)) {
            syslog(LOG_WARNING,"Warning - %s:%d might have called SAFE_STRCAT "
						 "with a bare character pointer\n",File,Line);
         }

         if(Src == NULL) {
            syslog(LOG_ERR,"Error - %s:%d called SAFE_STRCAT with a "
                   "NULL source\n",File,Line);
         }
         break;
      }

      if(Dst == NULL || Src == NULL) {
         break;
      }

      DstStrLen = strlen(Dst);
      SrcStrLen = strlen(Src);

      if(DstStrLen >= DstLen) {
         const char *File = GetFileFromPath(Path);

         syslog(LOG_ERR,"Error - %s:%d appears to have called SAFE_STRCAT "
					 "with a uninitialized destination pointer\n",File,Line);
         break;
      }

      Ret = strncat(Dst,Src,DstLen-DstStrLen-1);

      if(SrcStrLen > DstLen-DstStrLen-1) {
         const char *File = GetFileFromPath(Path);

         syslog(LOG_ERR,"Warning - SAFE_STRCAT called from %s:%d truncated "
                "string to '%s'\n",File,Line,Dst);
      }
   } while(0);

   return Ret;
}

#endif	// if !defined(BOARD_PVT) || defined(DEBUG)

// warn on allocations bigger than this
#define LARGE_ALLOC_THRESHOLD		(1024*128)	
void *CheckAlloc(void *ptr, size_t size, const char *Path, int Line)
{
	if(ptr == NULL || size == 0) {
		const char *File = GetFileFromPath(Path);
		if(size == 0) {
			syslog(LOG_ERR,
					 "Error: %s#%d attempted to allocate zero bytes\n",File,Line);
		}
		else if(ptr == NULL) {
			syslog(LOG_ERR,
					 "Error: %s#%d: allocation of %u bytes failed, exiting.\n",
					 File,Line,size);
			resetSystem();
		}
	}
	else {
	// Frequently MALLOC'ed buffers are used with snprintf with a max len
	// of the MALLOC'ed buffer minus one.  Zero the last byte of the buffer
	// to ensure truncated strings are null terminated.
		((char *) ptr)[size-1] = 0;

#if !defined(BOARD_PVT) || defined(DEBUG)
		if(size > LARGE_ALLOC_THRESHOLD) {
		// Warn if an allocation is suspiciously large
         const char *File = GetFileFromPath(Path);
			syslog(LOG_WARNING,
					 "Warning: %s#%d allocated a %u byte buffer\n",File,Line,size);
		}
#endif
	}

	return ptr;
}

void *CheckAllocAndZero(void *ptr, size_t size, const char *File, int Line)
{
	CheckAlloc(ptr,size,File,Line);
	if(ptr != NULL) {
		memset(ptr,0,size);
	}

	return ptr;
}

char *CheckedStrdup(const char *String,const char *Path, int Line)
{
	char *Ret = NULL;
	if(String == NULL || (Ret = strdup(String)) == NULL) {
		const char *File = GetFileFromPath(Path);
		if(String == NULL) {
			syslog(LOG_ERR,
					 "Error: %s#%d: called strdup with a NULL pointer\n",File,Line);
		}
		else {
			syslog(LOG_ERR,
					 "Error: %s#%d: strdup of %u byte string failed, exiting\n",
					 File,Line,strlen(String));
			resetSystem();
		}
	}

	return Ret;
}
