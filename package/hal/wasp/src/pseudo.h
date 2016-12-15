/***************************************************************************
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
***************************************************************************/
#ifndef _PSEUDO_H_
#define _PSEUDO_H_

#define PSEUDO_FILE_VER    1

#ifdef GEMTEK_SDK
#define PSEUDO_SAVE_DIR "/tmp/Belkin_settings"
#else
#define PSEUDO_SAVE_DIR "./"
#endif
#define PSEUDO_TMP_PATH PSEUDO_SAVE_DIR "pseudo_vars.tmp"
#define PSEUDO_SAVE_PATH PSEUDO_SAVE_DIR "pseudo_vars.bin"
#define PSEUDO_OLD_PATH PSEUDO_SAVE_DIR "pseudo_vars.old.bin"


// Minimum number of seconds to wait between saving Pseudo variable
// state (to prevent premature Flash wear out when Pseudo variables change
// frequently).
#define  MIN_PSEUDO_SAVE_DELAY   60

void UpdatePseudoVars(WaspVariable *pVar);
int SavePseudoVars(void);
int RestorePseudoVars(void);
int PseudoCommand(char *Arg);

extern int gPseudoVarChanged;
extern time_t gPseudoVarsLastSaved;

#endif   // _PSEUDO_H_


