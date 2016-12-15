/***************************************************************************
*
*
* WemoDB.c
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
#include <syslog.h>
#include <logger.h>
#include "WemoDB.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

static int DBCallback(void *NotUsed, int argc, char **argv, char **azColName);
int InitDB(const char *ps8DBURL,sqlite3 **db)
{
  int s32ret=0;
  s32ret = sqlite3_open(ps8DBURL, db);
  if( s32ret ){
    APP_LOG("InitDB", LOG_ERR, "Can't open database: %s", sqlite3_errmsg(*db));
    CloseDB(*db);
    return DB_ERROR;
  }
  return DB_SUCCESS;
}

void CloseDB(sqlite3 *db)
{
    int rc = 0;
    rc = sqlite3_close(db);
    if(rc)
    {
	APP_LOG("CloseDB", LOG_ERR, "close DB error: %d", rc);
    }
    else
    	APP_LOG("CloseDB", LOG_DEBUG, "Closed DB");
}

int ExecuteStatement(char *ps8Statement,sqlite3 **db)
{
  int s32DBRet=0;
  char *DBErrMsg = NULL;
  if(*db)
  {
    s32DBRet = sqlite3_exec(*db,ps8Statement , DBCallback, 0, &DBErrMsg);
    APP_LOG("ExecuteStatement", LOG_HIDE, "Executed Statement: %s", ps8Statement);
  }
  else
  {
    APP_LOG("ExecuteStatement", LOG_DEBUG, "DB Not Open");
    return DB_NOT_OPEN;
  }
  if( s32DBRet !=SQLITE_OK ){
    APP_LOG("ExecuteStatement", LOG_ERR, "SQL error: %s", DBErrMsg);
    return DB_ERROR;
  }

  if(DBErrMsg != NULL) {
	  sqlite3_free(DBErrMsg);
  }
  return DB_SUCCESS;
}

static int DBCallback(void *NotUsed, int argc, char **argv, char **azColName){
  return DB_SUCCESS;
}

void DeleteDB(char *ps8DBURL)
{
    char ps8DelCommand[MAX_FW_URL_LEN];
    snprintf(ps8DelCommand, sizeof(ps8DelCommand),"rm -rf %s",ps8DBURL);
    system(ps8DelCommand);
}

int WeMoDBCreateTable(sqlite3 **DBHandle,char *TableName,TableDetails TableParams[],int PrimaryKeyEnable,int ParametersCount)
{
    char s8TempBuffer[SIZE_256B];
    char s8Statement[SIZE_256B];
    int s32cntr=0,rc=0;
    memset(s8TempBuffer,SIZE_256B,0);
    memset(s8Statement,SIZE_256B,0);
    snprintf(s8TempBuffer, sizeof(s8TempBuffer), "CREATE TABLE %s (%s %s,",TableName,TableParams[0].ParamName,TableParams[0].ParamDataType);
    strncpy(s8Statement,s8TempBuffer,sizeof(s8Statement)-1);
    for(s32cntr=1;s32cntr<ParametersCount;s32cntr++)
    {
	if (s32cntr == (ParametersCount -1))
	    snprintf(s8TempBuffer, sizeof(s8TempBuffer), "%s %s",TableParams[s32cntr].ParamName,TableParams[s32cntr].ParamDataType);
	else
	    snprintf(s8TempBuffer, sizeof(s8TempBuffer), "%s %s,",TableParams[s32cntr].ParamName,TableParams[s32cntr].ParamDataType);
	    
	strncat(s8Statement,s8TempBuffer, sizeof(s8Statement)-strlen(s8Statement)-1);
    	memset(s8TempBuffer,SIZE_256B,0);
    }
    strncat(s8Statement,");", sizeof(s8Statement)-strlen(s8Statement)-1);

    rc = ExecuteStatement(s8Statement,DBHandle);
    if( rc!=DB_SUCCESS ){
	APP_LOG("WeMoDBCreateTable", LOG_ERR, "SQL error on executing statement: %s", s8Statement);
	return DB_ERROR;
    }
    return DB_SUCCESS;
}



int WeMoDBInsertInTable(sqlite3 **DBHandle,char *TableName,ColDetails ColParams[],int ParametersCount)
{
    char s8TempBuffer[SIZE_256B];
    char s8Statement[SIZE_256B];
    int s32cntr=0,rc=0;
    memset(s8TempBuffer,SIZE_256B,0);
    memset(s8Statement,SIZE_256B,0);
    snprintf(s8TempBuffer, sizeof(s8TempBuffer), "INSERT INTO %s (%s,",TableName,ColParams[0].ColName);
    strncpy(s8Statement,s8TempBuffer,sizeof(s8Statement)-1);
    for(s32cntr=1;s32cntr<ParametersCount;s32cntr++)
    {
	if (s32cntr == (ParametersCount-1))
	    snprintf(s8TempBuffer, sizeof(s8TempBuffer), "%s",ColParams[s32cntr].ColName);
	else
	    snprintf(s8TempBuffer, sizeof(s8TempBuffer), "%s,",ColParams[s32cntr].ColName);
	    
	strncat(s8Statement,s8TempBuffer,  sizeof(s8Statement)-strlen(s8Statement)-1);
    	memset(s8TempBuffer,SIZE_256B,0);
    }
    strncat(s8Statement,") VALUES(",  sizeof(s8Statement)-strlen(s8Statement)-1);
    for(s32cntr=0;s32cntr<ParametersCount;s32cntr++)
    {
	if (s32cntr == (ParametersCount-1))
	    snprintf(s8TempBuffer, sizeof(s8TempBuffer), "%s",ColParams[s32cntr].ColValue);
	else
	    snprintf(s8TempBuffer, sizeof(s8TempBuffer), "%s,",ColParams[s32cntr].ColValue);
	    
	strncat(s8Statement,s8TempBuffer,  sizeof(s8Statement)-strlen(s8Statement)-1);
    	memset(s8TempBuffer,SIZE_256B,0);
    }
    strncat(s8Statement,");",  sizeof(s8Statement)-strlen(s8Statement)-1);
    APP_LOG("WeMoDBInsertInTable", LOG_ERR, "SQL  statement: %s", s8Statement);
    rc = ExecuteStatement(s8Statement,DBHandle);
    if( rc!=DB_SUCCESS ){
	APP_LOG("WeMoDBInsertInTable", LOG_ERR, "SQL error on executing statement: %s", s8Statement);
	return -1;
    }
    return 0;
}


int WeMoDBUpdateTable(sqlite3 **DBHandle,char *TableName,ColDetails ColParams[],int ParametersCount,char *Condition)
{
    char s8TempBuffer[SIZE_256B];
    char s8Statement[SIZE_256B];
    int s32cntr=0,rc=0;
    memset(s8TempBuffer,SIZE_256B,0);
    memset(s8Statement,SIZE_256B,0);
    if(!ParametersCount)
    {
	snprintf(s8TempBuffer, sizeof(s8TempBuffer), "UPDATE %s SET %s=%s",TableName,ColParams[0].ColName,ColParams[0].ColValue);
    }
    else
    {
	snprintf(s8TempBuffer, sizeof(s8TempBuffer), "UPDATE %s SET %s=%s,",TableName,ColParams[0].ColName,ColParams[0].ColValue);
    }
    strncpy(s8Statement,s8TempBuffer,sizeof(s8Statement)-1);
    for(s32cntr=1;s32cntr<ParametersCount;s32cntr++)
    {
	if (s32cntr == (ParametersCount - 1))
	    snprintf(s8TempBuffer, sizeof(s8TempBuffer), "%s=%s",ColParams[s32cntr].ColName,ColParams[s32cntr].ColValue);
	else
	    snprintf(s8TempBuffer, sizeof(s8TempBuffer), "%s=%s,",ColParams[s32cntr].ColName,ColParams[s32cntr].ColValue);
	    
	strncat(s8Statement,s8TempBuffer,  sizeof(s8Statement)-strlen(s8Statement)-1);
    	memset(s8TempBuffer,SIZE_256B,0);
    }
    if(Condition != NULL)
    {
	strncat(s8Statement," WHERE ",  sizeof(s8Statement)-strlen(s8Statement)-1);
	strncat(s8Statement,Condition,  sizeof(s8Statement)-strlen(s8Statement)-1);
    }
    strncat(s8Statement,";",  sizeof(s8Statement)-strlen(s8Statement)-1);
    APP_LOG("WeMoDBUpdateTable", LOG_ERR, "SQL Update statement: %s", s8Statement);
    rc = ExecuteStatement(s8Statement,DBHandle);
    if( rc!=DB_SUCCESS ){
	APP_LOG("WeMoDBUpdateTable", LOG_ERR, "SQL error on executing statement: %s", s8Statement);
    }
    return 0;
}

int WeMoDBGetTableData(sqlite3 **DBHandle,char *s8Query,char ***s8Result,int *s32NumofRows, int *s32NumofCols)
{
    char **s8result;
    char *s8SQLErr=NULL;
    int RowCount=0,ColCount=0;
    int rc=0,s32RowCntr=0,s32ArraySize=0;
    rc = sqlite3_get_table(*DBHandle, s8Query, &s8result, &RowCount, &ColCount, &s8SQLErr);
    if( rc!=DB_SUCCESS ){
	APP_LOG("WeMoDBGetTableData", LOG_DEBUG, "SQL error on executing statement: %s", s8SQLErr);
	return -1;
    }
    *s32NumofRows = RowCount;
    *s32NumofCols = ColCount;
    s32ArraySize = ((*s32NumofRows) + 1)*(*s32NumofCols);
    APP_LOG("WeMoDBGetTableData", LOG_HIDE, "\ns32NumofRows: %d s32NumofCols: %d Array Size: %d\n",*s32NumofRows,*s32NumofCols,s32ArraySize);
    if(s32ArraySize == 0){
	sqlite3_free_table(s8result);
	return 0;
    }

    *s8Result =(char **) MALLOC(sizeof(char *)*s32ArraySize);

    for(s32RowCntr=0; s32RowCntr < s32ArraySize; s32RowCntr++) {
	(*s8Result)[s32RowCntr] = (char *)ZALLOC(MAX_NAME_SIZE);
	
	//memset((*s8Result)[s32RowCntr],0,MAX_NAME_SIZE);   
    }
    for(s32RowCntr=0; s32RowCntr < s32ArraySize; s32RowCntr++) {
	    if(s8result[s32RowCntr] != NULL)
	    {
		//APP_LOG("WeMoDBGetTableData", LOG_DEBUG, "s8result[%d] = %s",s32RowCntr,s8result[s32RowCntr]);
		strncpy((*s8Result)[s32RowCntr],s8result[s32RowCntr],MAX_NAME_SIZE);
	    }
	    else{
		APP_LOG("WeMoDBGetTableData", LOG_DEBUG, "s8result[%d] = NULL",s32RowCntr);
	    }
    }
    sqlite3_free_table(s8result);
    return 0;
}

void WeMoDBTableFreeResult(char ***s8Result,int *s32NumofRows,int *s32NumofCols)
{
    int s32RowCntr=0;
    int s32ArraySize = ((*s32NumofRows) +1)*(*s32NumofCols);

    for(s32RowCntr=0; s32RowCntr < s32ArraySize; s32RowCntr++) 
    {
	if((*s8Result)[s32RowCntr] != NULL)
	    free((*s8Result)[s32RowCntr]);
    }
    if(*s8Result != NULL)
	free(*s8Result);
}

int WeMoDBDropTable(sqlite3 **DBHandle,char *TableName)
{
    char s8Statement[SIZE_256B];
    int	 rc=0;
    snprintf(s8Statement, sizeof(s8Statement), "DROP TABLE %s",TableName);
    rc = ExecuteStatement(s8Statement,DBHandle);
    if( rc!=DB_SUCCESS ){
	APP_LOG("WEMODB",LOG_DEBUG, "\nSQL error\n");
    }
    return 0;
    
}

int WeMoDBDeleteEntry(sqlite3 **DBHandle,char *TableName,char *Condition)
{
    char s8Statement[SIZE_256B];
    int	 rc=0;
    memset(s8Statement,SIZE_256B,0);
    if(Condition != NULL)
    {
	snprintf(s8Statement, sizeof(s8Statement), "DELETE FROM %s WHERE %s;",TableName,Condition);
	APP_LOG("WeMoDBDeleteEntry", LOG_ERR, "SQL Delete statement: %s", s8Statement);
	rc = ExecuteStatement(s8Statement,DBHandle);
	if( rc!=DB_SUCCESS ){
	    APP_LOG("WeMoDBDeleteEntry", LOG_ERR, "SQL error on executing statement: %s", s8Statement);
	}
	return 0;
    }
    else
    {
	APP_LOG("WeMoDBDeleteEntry", LOG_ERR, "No Condition too create Query ");
	return 1;
    }
}

int WeMoDBDeleteAllEntries(sqlite3 **DBHandle,char *TableName)
{
    char s8Statement[SIZE_256B];
    int	 rc=0;
    memset(s8Statement,SIZE_256B,0);

	snprintf(s8Statement, sizeof(s8Statement), "DELETE FROM %s ;",TableName);
	rc = ExecuteStatement(s8Statement,DBHandle);
	if( rc!=DB_SUCCESS ){
	    APP_LOG("WeMoDBDeleteAllEntries", LOG_ERR, "SQL error on executing statement: %s", s8Statement);
	}
	return 0;
}
