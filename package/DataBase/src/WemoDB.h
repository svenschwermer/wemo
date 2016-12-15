/***************************************************************************
*
*
* WemoDB.h
*
* Copyright (c) 2012-2014 Belkin International, Inc. and/or its affiliates.
* All rights reserved.
*
* Permission to use, copy, modify, and/or distribute this software for any 
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* 
*
* THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
* INCIDENTAL, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER 
* RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
* NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH
* THE USE OR PERFORMANCE OF THIS SOFTWARE.
*
*
***************************************************************************/

#ifndef _WEMODB_H_
#define _WEMODB_H_

#include "sqlite3.h"
#include "defines.h"
#define DB_SUCCESS	0
#define DB_ERROR	1
#define DB_NOT_OPEN	2
#define MAX_NAME_SIZE	SIZE_512B		//MAX Char 155, Localization Size, One Chinese = 155*3=465

typedef struct 
{
char *ParamName,*ParamDataType;
} TableDetails;

typedef struct 
{
char *ColName,*ColValue;
} ColDetails;

int InitDB(const char *ps8DBURL,sqlite3 **db);
void CloseDB(sqlite3 *db);
int ExecuteStatement(char *ps8Statement,sqlite3 **db);
void DeleteDB(char *ps8DBURL);
int WeMoDBCreateTable(sqlite3 **DBHandle,char *TableName,TableDetails TableParams[],int PrimaryKeyEnable,int ParametersCount);
int WeMoDBUpdateTable(sqlite3 **DBHandle,char *TableName,ColDetails ColParams[],int ParametersCount,char *Condition);
int WeMoDBInsertInTable(sqlite3 **DBHandle,char *TableName,ColDetails ColParams[],int ParametersCount);
int WeMoDBDropTable(sqlite3 **DBHandle,char *TableName);
int WeMoDBGetTableData(sqlite3 **DBHandle,char *s8Query,char ***s8Result,int *s32NumofRows, int *s32NumofCols);
void WeMoDBTableFreeResult(char ***s8Result,int *s32NumofRows,int *s32NumofCols);
int WeMoDBDeleteEntry(sqlite3 **DBHandle,char *TableName,char *Condition);
int WeMoDBDeleteAllEntries(sqlite3 **DBHandle,char *TableName);
#endif
