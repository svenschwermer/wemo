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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "wasp.h"
#include "wasp_api.h"
#define  WASPD_PRIVATE
#include "waspd.h"
#include "pseudo.h"

int WASP_SetValue(WaspVariable *pVar1,WaspVariable *pVar2);
int LimitCompare(WaspValUnion *pLimit,WaspVariable *pVar);
int UpdateRangeVar(DeviceVar *p,WaspVariable *pVar);
int SavePseudoVar(VarID ID,FILE *fp);
int SavePseudoVarsCmd(char *Filename);
int RestorePseudoVarsCmd(char *Filename,int bDumpOnly);

int gPseudoVarChanged = FALSE;
time_t gPseudoVarsLastSaved;

// set pVar1->Val = pVar2->Val
int WASP_SetValue(WaspVariable *pVar1,WaspVariable *pVar2)
{
   int Ret = WASP_OK;
   int CopyLen = 0;

   if(pVar1->Type != pVar2->Type) {
      Ret = WASP_ERR_INVALID_TYPE;
   }
   else {
      switch(pVar1->Type) {
         case WASP_VARTYPE_BLOB:
         case WASP_VARTYPE_STRING:
            Ret = WASP_ERR_NOT_SUPPORTED;
            break;

         case WASP_VARTYPE_BOOL:
         case WASP_VARTYPE_ENUM:
         case WASP_VARTYPE_UINT8:
            CopyLen = sizeof(pVar1->Val.U8);
            break;

         case WASP_VARTYPE_INT16:
         case WASP_VARTYPE_TEMP:
            CopyLen = sizeof(pVar1->Val.I16);
            break;

         case WASP_VARTYPE_INT8:
            CopyLen = sizeof(pVar1->Val.I8);
            break;

         case WASP_VARTYPE_INT32:
         case WASP_VARTYPE_TIME32:
            CopyLen = sizeof(pVar1->Val.I32);
            break;

         case WASP_VARTYPE_PERCENT:
         case WASP_VARTYPE_TIME16:
         case WASP_VARTYPE_TIME_M16:
         case WASP_VARTYPE_UINT16:
            CopyLen = sizeof(pVar1->Val.U16);
            Ret = WASP_OK;
            break;

         case WASP_VARTYPE_UINT32:
            CopyLen = sizeof(pVar1->Val.U32);
            Ret = WASP_OK;
            break;

         case WASP_VARTYPE_TIMEBCD:
            CopyLen = sizeof(pVar1->Val.BcdTime);
            break;

         case WASP_VARTYPE_BCD_DATE:
            CopyLen = sizeof(pVar1->Val.BcdDate);
            break;

         case WASP_VARTYPE_DATETIME:
            CopyLen = sizeof(pVar1->Val.BcdDateTime);
            break;

         default:
            Ret = WASP_ERR_INVALID_TYPE;
      }

      if(CopyLen > 0) {
         memcpy(&pVar1->Val,&pVar2->Val,CopyLen);
      }
   }

   return Ret;
}

// Compare WASP variable to Value union (typically a MinValue or MaxValue)
// 
// int LimitCompare(WaspValUnion *pLimit,WaspVariable *pVar)
// 
// Returns:
//      0 - pVar == Limit 
//      1 - pVar > Limit 
//      2 - pVar < Limit
//    < 0 - WASP error code (see WASP_ERR_*)
int LimitCompare(WaspValUnion *pLimit,WaspVariable *pVar)
{
   int Ret = WASP_OK;
   int Diff = 0;

   switch(pVar->Type) {
      case WASP_VARTYPE_BOOL:
      case WASP_VARTYPE_ENUM:
      case WASP_VARTYPE_UINT8:
         Diff = pLimit->U8 - pVar->Val.U8;
         break;

      case WASP_VARTYPE_INT16:
      case WASP_VARTYPE_TEMP:
         Diff = pLimit->I16 - pVar->Val.I16;
         break;

      case WASP_VARTYPE_INT8:
         Diff = pLimit->I8 - pVar->Val.I8;
         break;

      case WASP_VARTYPE_INT32:
      case WASP_VARTYPE_TIME32:
         Diff = pLimit->I32 - pVar->Val.I32;
         break;

      case WASP_VARTYPE_PERCENT:
      case WASP_VARTYPE_TIME16:
      case WASP_VARTYPE_TIME_M16:
      case WASP_VARTYPE_UINT16:
         Diff = pLimit->U16 - pVar->Val.U16;
         break;

      case WASP_VARTYPE_UINT32:
         Diff = pLimit->U32 - pVar->Val.U32;
         break;

      case WASP_VARTYPE_BLOB:
      case WASP_VARTYPE_STRING:
         break;

      case WASP_VARTYPE_TIMEBCD:
         break;
      case WASP_VARTYPE_BCD_DATE:
         break;
      case WASP_VARTYPE_DATETIME:
         break;

      default:
         Ret = WASP_ERR_INVALID_TYPE;
   }

   if(Ret == WASP_OK) {
      if(Diff > 0) {
         Ret = 1;
      }
      else if(Diff < 0) {
         Ret = 2;
      }
      else {
         Ret = 0;
      }
   }

   return Ret;
}

// Return 1 if range is met
int UpdateRangeVar(DeviceVar *p,WaspVariable *pVar)
{
   int Ret = 0;
   WaspVariable *pPseudoVar = p->pVars->pVar;
   int MinDiff;
   int MaxDiff;
   int bInRange = FALSE;

   do {
      if(pPseudoVar->Type != pVar->Type) {
         Ret = WASP_ERR_INVALID_TYPE;
         break;
      }

      if((MinDiff = LimitCompare(&p->Desc.MinValue,pVar)) < 0) {
         break;
      }

      if((MaxDiff = LimitCompare(&p->Desc.MaxValue,pVar)) < 0) {
         break;
      }

      if((MinDiff == 0 || MinDiff == 1) &&
         (MinDiff == 2 || MinDiff == 0))
      {  // pVarMin <= pVar <= pVarMax, i.e. Value in range
         bInRange = TRUE;
      }

      if((gWASP_Verbose & LOG_PSEUDO) && (gWASP_Verbose & LOG_V1)) {
         LOG("%s: VarID 0x%x is %swithin range defined by VarID 0x%x\n",
             __FUNCTION__,pVar->ID,bInRange ? "" : "not ",pPseudoVar->ID);
      }

      if(bInRange) {
         if(pPseudoVar->Pseudo.bRestored || pPseudoVar->Pseudo.bInited) {
         // There is a previous history so bInRange has been updated
            if(!pPseudoVar->Pseudo.bInRange && 
               p->Desc.Usage == WASP_USAGE_RANGE_ENTER)
            {  // Tracked variable just came in range
               Ret = TRUE;
            }
         }
      }
      else {
      // pVar not in range
         if(pPseudoVar->Pseudo.bRestored || pPseudoVar->Pseudo.bInited) {
         // There is a previous history so bInRange has been updated
            if(pPseudoVar->Pseudo.bInRange && 
               p->Desc.Usage == WASP_USAGE_RANGE_EXIT)
            {  // Tracked variable just when out of range
               Ret = TRUE;
            }
         }
      }
      pPseudoVar->Pseudo.bInRange = bInRange;
      pPseudoVar->Pseudo.bInited = TRUE;
   } while(FALSE);

   return Ret;
}


void UpdatePseudoVars(WaspVariable *pVar)
{
   int i = 0;
   DeviceVar *p;
   int bSetVar = FALSE;
   WaspVariable *pPseudoVar = NULL;

   for( ; ; ) {
      if((p = Variables[i++]) == NULL) {
      // No more variables to check
         break;
      }

      if((p->Desc.Usage & WASP_USAGE_PSEUDO) &&
         p->Desc.u.p.TrackedVarId == pVar->ID)
      {  // Found a pseudo variable tracking this variable
         bSetVar = FALSE;
         pPseudoVar = p->pVars->pVar;
         switch(p->Desc.Usage) {
            case WASP_USAGE_MINIMUM:
               if(WASP_Compare(pPseudoVar,pVar) == 1) {
                  bSetVar = TRUE;
               }
               break;

            case WASP_USAGE_MAXIMUM:
               if(WASP_Compare(pPseudoVar,pVar) == 2) {
                  bSetVar = TRUE;
               }
               break;

            case WASP_USAGE_RANGE_ENTER:
            case WASP_USAGE_RANGE_EXIT:
               if(UpdateRangeVar(p,pVar) == 1) {
                  bSetVar = TRUE;
               }
               break;

            default:
               break;
         }
      }

      if(bSetVar) {
         gPseudoVarChanged = TRUE;
         WASP_SetValue(pPseudoVar,pVar);
         pPseudoVar->Pseudo.Count++;
         if(g_bTimeSynced) {
            pPseudoVar->Pseudo.TimeStamp = gTimeNow.tv_sec;
         }
         else {
            pPseudoVar->Pseudo.TimeStamp = 0;
            LOG("%s: Error - system clock not set\n",__FUNCTION__);
         }
         if(gWASP_Verbose & LOG_PSEUDO) {
            LOG("%s: Set Pseudo variable 0x%x, count: %d\n",
                __FUNCTION__,p->Desc.ID,pPseudoVar->Pseudo.Count);
         }
      }
   }
}

int SavePseudoVar(VarID ID,FILE *fp)
{
   int Ret = WASP_OK;
   WaspVariable Var;
   Var.ID = ID;
   Var.State = VAR_VALUE_CACHED;

   do {
      if((Ret = WASP_GetVariable(&Var)) != WASP_OK) {
         if(Ret != WASP_ERR_NO_DATA) {
            LOG("%s: WASP_GetVariable failed: %s\n",__FUNCTION__,
                WASP_strerror(Ret));
         }
         break;
      }

      if(Ret != WASP_OK) {
         LOG("%s: WASP_GetVariable failed: %s\n",__FUNCTION__,
             WASP_strerror(Ret));
      }

      if((gWASP_Verbose & LOG_PSEUDO) && fp != NULL) {
         WASP_DumpVar(&Var);
         LOG("\n");
      }

      if(Var.Type == WASP_VARTYPE_STRING ||
         Var.Type == WASP_VARTYPE_BLOB)
      {
         LOG("%s: Error, VarType %d is not supported\n",__FUNCTION__,Var.Type);
         WASP_FreeValue(&Var);
         Ret = WASP_ERR_NOT_SUPPORTED;
         break;
      }

      if(fp != NULL) {
      // Save the variable
         if(fwrite(&Var,sizeof(Var),1,fp) != 1) {
            Ret = errno;
            LOG("%s: fwrite failed - %s\n",__FUNCTION__,strerror(errno));
         }
      }
   } while(FALSE);

   return Ret;
}


int SavePseudoVarsCmd(char *Filename)
{
   int Ret;
   FILE *fp = NULL;
   int Count;
   WaspVarDesc **pVarDescArray = NULL;
   int i;
   int PseudoVars = 0;
   int SetVars = 0;

   do {
      if((Ret = WASP_Init()) != 0) {
         LOG("%s: WASP_Init() failed, %s\n",__FUNCTION__,WASP_strerror(Ret));
         break;
      }

      if((Ret = WASP_GetVarList(&Count,&pVarDescArray)) != 0) {
         LOG("%s: WASP_GetVarList() failed: %s\n",__FUNCTION__,
             WASP_strerror(Ret));
         break;
      }

      for(i = 0; i < Count; i++) {
         if(pVarDescArray[i]->Usage & WASP_USAGE_PSEUDO) {
            PseudoVars++;
            if(SavePseudoVar(pVarDescArray[i]->ID,NULL) == WASP_OK) {
               SetVars++;
            }
         }
      }

      if(PseudoVars == 0) {
         LOG("No pseudo variables defined\n");
         break;
      }

      if(SetVars == 0) {
         LOG("None of the %d pseudo variables have set yet\n",PseudoVars);
         break;
      }
      LOG("Saving %d pseudo variables\n",SetVars);

      if((fp = fopen(Filename,"w")) == NULL) {
         Ret = errno;
         LOG("%s: fopen('%s') failed - %s\n",
             __FUNCTION__,Filename,strerror(errno));
         break;
      }

   // Write file version
      i = PSEUDO_FILE_VER;
      if(fwrite(&i,sizeof(i),1,fp) != 1) {
         Ret = errno;
         LOG("%s: fwrite failed - %s\n",__FUNCTION__,strerror(errno));
         break;
      }

   // Write size of WASP variable
      i = sizeof(WaspVariable);
      if(fwrite(&i,sizeof(i),1,fp) != 1) {
         Ret = errno;
         LOG("%s: fwrite failed - %s\n",__FUNCTION__,strerror(errno));
         break;
      }

   // Write number of variables
      if(fwrite(&SetVars,sizeof(SetVars),1,fp) != 1) {
         Ret = errno;
         LOG("%s: fwrite failed - %s\n",__FUNCTION__,strerror(errno));
         break;
      }

   // Write the variables
      for(i = 0; i < Count && Ret == WASP_OK; i++) {
         if(pVarDescArray[i]->Usage & WASP_USAGE_PSEUDO) {
            Ret = SavePseudoVar(pVarDescArray[i]->ID,fp);
            if(Ret == WASP_ERR_NO_DATA) {
            // Pseudo variable hasn't been set yet, nothing to save
               Ret = WASP_OK;
            }
         }
      }
   } while(FALSE);

   if(fp != NULL) {
      fclose(fp);
   }

   if(pVarDescArray != NULL) {
      free(pVarDescArray);
   }

   return Ret;
}


int RestorePseudoVarsCmd(char *Filename,int bDumpOnly)
{
   int Ret = WASP_OK;
   FILE *fp = NULL;
   int i;
   int Count;

   do {
      if((Ret = WASP_Init()) != 0) {
         LOG("%s: WASP_Init() failed, %s\n",__FUNCTION__,WASP_strerror(Ret));
         break;
      }

      if((fp = fopen(Filename,"r")) == NULL) {
         Ret = errno;
         LOG("%s: fopen failed('%s') - %s\n",
             __FUNCTION__,Filename,strerror(errno));
         break;
      }

      if(fread(&i,sizeof(i),1,fp) != 1) {
         Ret = errno;
         LOG("%s#%d: fread failed - %s\n",__FUNCTION__,__LINE__,strerror(errno));
         break;
      }

      if(i != PSEUDO_FILE_VER) {
         LOG("%s: Error unknown file format version %d\n",__FUNCTION__,i);
         break;
      }

      if(fread(&i,sizeof(i),1,fp) != 1) {
         Ret = errno;
         LOG("%s#%d: fread failed - %s\n",__FUNCTION__,__LINE__,strerror(errno));
         break;
      }

      if(i != sizeof(WaspVariable)) {
         LOG("%s: Error incorrect WaspVariable size %d\n",__FUNCTION__,i);
         break;
      }

      if(fread(&Count,sizeof(Count),1,fp) != 1) {
         Ret = errno;
         LOG("%s#%d: fread failed - %s\n",__FUNCTION__,__LINE__,strerror(errno));
         break;
      }

      if(!bDumpOnly) {
         LOG("Restoring %d pseudo variables\n",Count);
      }

      for(i = 0; i < Count; i++) {
         WaspVariable Var;

         if(fread(&Var,sizeof(Var),1,fp) != 1) {
            Ret = errno;
            LOG("%s: fread failed - %s\n",__FUNCTION__,strerror(errno));
            break;
         }

         if(bDumpOnly || (gWASP_Verbose & LOG_PSEUDO)) {
            WASP_DumpVar(&Var);
            printf("\n");
         }

         if(!bDumpOnly) {
            if((Ret = WASP_SetVariable(&Var)) != 0) {
               LOG("%s: WASP_SetVariable() failed: %s\n",__FUNCTION__,
                   WASP_strerror(Ret));
               break;
            }
         }
      }
   } while(FALSE);


   if(fp != NULL) {
      fclose(fp);
   }

   return Ret;
}

/*
   -ps<file> - Save pesudo variables to <file>
   -pr<file> - restore pesudo variables from <file>
*/ 
int PseudoCommand(char *Arg)
{
   int Ret = EINVAL; // assume the worse

   switch(*Arg) {
      case 's':
         Ret = SavePseudoVarsCmd(&Arg[1]);
         break;

      case 'r':
         Ret = RestorePseudoVarsCmd(&Arg[1],FALSE);
         break;

      case 'd':
         Ret = RestorePseudoVarsCmd(&Arg[1],TRUE);
         break;

   }

   return Ret;
}

int SavePseudoVars()
{
   int Ret = WASP_OK;
   FILE *fp = NULL;
   int i;
   int PseudoVars = 0;
   int SetVars = 0;
   VarType Type;
   DeviceVar *p;

   do {
      for(i = NUM_STANDARD_VARS; Variables[i] != NULL; i++) {
         if(Variables[i]->Desc.Usage & WASP_USAGE_PSEUDO) {
            p = Variables[i];
            Type = p->Desc.Type;
            if(Type == WASP_VARTYPE_STRING || Type == WASP_VARTYPE_BLOB) {
               LOG("%s: Ignoring ID 0x%x, type %d not supported\n",
                   __FUNCTION__,p->Desc.ID,Type);
            }
            else {
               PseudoVars++;
               if(p->pVars->pVar->Pseudo.bInited) {
                  SetVars++;
               }
            }
         }
      }

      if(PseudoVars == 0) {
         LOG("%s: Error, no pseudo variables to save\n",__FUNCTION__);
         break;
      }

      if(SetVars == 0) {
         LOG("%s: Error, no pseudo variables set\n",__FUNCTION__);
         break;
      }

      if(gWASP_Verbose & LOG_PSEUDO) {
         LOG("Saving %d pseudo variables\n",SetVars);
      }

      if((fp = fopen(PSEUDO_TMP_PATH,"w")) == NULL) {
         Ret = errno;
         LOG("%s: fopen failed - %s\n",__FUNCTION__,strerror(errno));
         break;
      }

   // Write file version
      i = PSEUDO_FILE_VER;
      if(fwrite(&i,sizeof(i),1,fp) != 1) {
         Ret = errno;
         LOG("%s: fwrite failed - %s\n",__FUNCTION__,strerror(errno));
         break;
      }

   // Write size of WASP variable
      i = sizeof(WaspVariable);
      if(fwrite(&i,sizeof(i),1,fp) != 1) {
         Ret = errno;
         LOG("%s: fwrite failed - %s\n",__FUNCTION__,strerror(errno));
         break;
      }

   // Write number of variables
      if(fwrite(&SetVars,sizeof(SetVars),1,fp) != 1) {
         Ret = errno;
         LOG("%s: fwrite failed - %s\n",__FUNCTION__,strerror(errno));
         break;
      }

   // Write the variables
      for(i = NUM_STANDARD_VARS; Variables[i] != NULL; i++) {
         if(Variables[i]->Desc.Usage & WASP_USAGE_PSEUDO) {
            p = Variables[i];
            Type = p->Desc.Type;
            if(Type == WASP_VARTYPE_STRING ||Type == WASP_VARTYPE_BLOB) {
               continue;
            }

            if(fwrite(p->pVars->pVar,sizeof(WaspVariable),1,fp) != 1) {
               Ret = errno;
               LOG("%s: fwrite failed - %s\n",__FUNCTION__,strerror(errno));
               break;
            }
         }
      }
   } while(FALSE);

   if(fp != NULL) {
      fclose(fp);
      unlink(PSEUDO_OLD_PATH);
   // pseudo_vars.bin -> pseudo_vars.old.bin,
   // pseudo_vars.tmp -> pseudo_vars.bin
      rename(PSEUDO_SAVE_PATH,PSEUDO_OLD_PATH);
      rename(PSEUDO_TMP_PATH,PSEUDO_SAVE_PATH);
   }

   if(Ret == WASP_OK) {
      gPseudoVarsLastSaved = gTimeNow.tv_sec;
   }

   return Ret;
}

int RestorePseudoVars()
{
   int Ret = WASP_OK;
   FILE *fp = NULL;
   int i;
   int Count = 0;

   do {
      if((fp = fopen(PSEUDO_SAVE_PATH,"r")) == NULL) {
         Ret = errno;
         LOG("%s: fopen failed - %s\n",__FUNCTION__,strerror(errno));
         break;
      }

      if(fread(&i,sizeof(i),1,fp) != 1) {
         Ret = errno;
         LOG("%s#%d: fread failed - %s\n",__FUNCTION__,__LINE__,strerror(errno));
         break;
      }

      if(i != PSEUDO_FILE_VER) {
         LOG("%s: Error unknown file format version %d\n",__FUNCTION__,i);
         break;
      }

      if(fread(&i,sizeof(i),1,fp) != 1) {
         Ret = errno;
         LOG("%s#%d: fread failed - %s\n",__FUNCTION__,__LINE__,strerror(errno));
         break;
      }

      if(i != sizeof(WaspVariable)) {
         LOG("%s: Error incorrect WaspVariable size %d\n",__FUNCTION__,i);
         break;
      }

      if(fread(&Count,sizeof(Count),1,fp) != 1) {
         Ret = errno;
         LOG("%s#%d: fread failed - %s\n",__FUNCTION__,__LINE__,strerror(errno));
         break;
      }

      LOG("Restoring %d pseudo variables\n",Count);

   // Read the variables
      for(i = 0; i < Count; i++) {
         WaspVariable Var;
         WaspVariable *pVar;

         if(fread(&Var,sizeof(Var),1,fp) != 1) {
            Ret = errno;
            LOG("%s: fread failed - %s\n",__FUNCTION__,strerror(errno));
            break;
         }

         if(Var.Type == WASP_VARTYPE_STRING || Var.Type == WASP_VARTYPE_BLOB) {
            LOG("%s: Ignoring ID 0x%x, type %d not supported\n",
                __FUNCTION__,Var.ID,Var.Type);
            continue;
         }


         if((pVar = FindVariable(Var.ID)) == NULL) {
            LOG("%s: Error - VarID 0x%x not found\n",__FUNCTION__,Var.ID);
         }
         else {
            *pVar = Var;
            pVar->Pseudo.bRestored = TRUE;
            pVar->Pseudo.bInited = TRUE;
         }

         if(gWASP_Verbose & LOG_PSEUDO) {
            WASP_DumpVar(pVar);
         }
      }
   } while(FALSE);

   if(fp != NULL) {
      fclose(fp);
   }

   return Ret;
}


