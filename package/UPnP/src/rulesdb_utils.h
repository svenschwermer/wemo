/***************************************************************************
 *
 *
 * rulesdb_utils.h
 *
 * Created by Belkin International, Software Engineering on May 27, 2011
 * Copyright (c) 2012-2014 Belkin International, Inc. and/or its affiliates.
 * All rights reserved.
 *
 * Belkin International, Inc. retains all right, title and interest (including
 * all intellectual property rights) in and to this computer program, which
 * is protected by applicable intellectual property laws.  Unless you have
 * obtained a separate written license from Belkin International, Inc., you are
 * not authorized to utilize all or a part of this computer program for any
 * purpose (including reproduction, distribution, modification, and compilation
 * into object code) and you must immediately destroy or return to Belkin
 * International, Inc all copies of this computer program.  If you are licensed
 * by Belkin International, Inc., your rights to utilize this computer program
 * are limited by the terms of that license.
 *
 * To obtain a license, please contact Belkin International, Inc.
 *
 * This computer program contains trade secrets owned by Belkin International,
 * Inc. and, unless unauthorized by Belkin International, Inc. in writing, you
 * agree to maintain the confidentiality of this computer program and related
 * information and to not disclose this computer program and related information
 * to any other person or entity.
 *
 * THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND BELKIN
 * INTERNATIONAL, INC. EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * INCLUDING THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, TITLE, AND NON-INFRINGEMENT.
 *
 *
 ***************************************************************************/
#ifndef __RULESDB_UTILS__H__
#define __RULESDB_UTILS__H__

#include <sys/types.h>

#define  RULESDB_UTILS_DBG

/** @brief Utility debugging macro.
 * When RULESDB_UTILS_DBG is defined it proxies its' arguments to
 * APP_LOG.  When not, the statement is not compiled.
 */
#ifdef RULESDB_UTILS_DBG
#  define UTL_LOG( source, level, format, ... ) \
          APP_LOG( source, level, format, ##__VA_ARGS__ )
#else
#  define UTL_LOG( source, level, format, ... ) 
#endif

#ifndef TRUE
#define  TRUE                         (1)
#define  FALSE                        (0)
#endif

#define CDATA_PATTERN                 "![CDATA["
#define CDATA_PATTERN_PREFIX_LEN      (8)
#define CDATA_PATTERN_SUFFIX_LEN      (2)

/**
 * @brief  DecodeRuleDbData: Decodes the XML received as part of the StoreRules
 *                           action and carries out the following:
 *                           a) Saves the rules db to its relevant location.
 *                           b) Sets the rule db version.
 *                           c) Processes the db based on the processDB field.
 * @param  sRuleDbData [IN] - RuleDbData XML string.
 *
 * @return retVal: 0: On Success. 
 *                -1: On Failure
 *
 * @author Christopher A F
 */
int DecodeRuleDbData (char *sRuleDbData);

#endif /* ifndef __RULESDB_UTILS__H__ */
