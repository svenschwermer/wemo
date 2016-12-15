/***************************************************************************
*
*
* pmortem-internal.h
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

#if !defined(__PMORTEM_INTERNAL_H__)
#define __PMORTEM_INTERNAL_H__

typedef size_t ptrdiff_t;	/* The WeMo toolchain doesn't provide a
				 * ptrdiff_t, substitute size_t until building
				 * with a recent toolchain.
				 */

#define PMORTEMD_UDS_PATH  "/tmp/pmortemd.socket"
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#define LOG_ERROR(...) log_error(__FUNCTION__, __LINE__, __VA_ARGS__)

static inline void log_error(const char *f, int line, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	fprintf(stderr, "%s:%d ", f, line);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, ": %s\n", strerror(errno));
	va_end(args);
        exit(EXIT_FAILURE);
}

#endif /* __PMORTEM_INTERNAL_H__ */
