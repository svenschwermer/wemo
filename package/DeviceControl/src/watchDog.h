/***************************************************************************
*
*
* watchDog.h
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
#ifndef _WATCHDOG_H
#define _WATCHDOG_H

#include <stdio.h>
#include "global.h"
#include "logger.h"

#define SEC_PER_MINUTE    60
#define WD_DEFAULT_TRIG_TIME    (5*SEC_PER_MINUTE)
#define WD_DEFAULT_DIFF_TIME    (1*SEC_PER_MINUTE)
#define WD_SLEEP_DURATION       ((WD_DEFAULT_TRIG_TIME - WD_DEFAULT_DIFF_TIME)/SEC_PER_MINUTE)

#define REBOOT_UNKNOWN  0 // abnormal power off, kernel panic.etc.
#define REBOOT_NORMAL   1 // User reboot
#define REBOOT_SOFTDOG  2 // Kernel softdog
#define REBOOT_MEMORY   3 // Less free memory
#define REBOOT_LOADING  4 // CPU overloading

typedef struct _CPU_INFO_ {
  unsigned long long userCpu;
  unsigned long long niceCpu;
  unsigned long long systemCpu;
  unsigned long long idleCpu;
  unsigned long long totalCpu;
} cpuInfo, *pCpuInfo;

extern char *pWDLogFile;
void watchDogTask(void) ;

int initWatchDog (void);
void resetSystem();
int detectUptime (int *hr, int *min, int *sec);

#endif
