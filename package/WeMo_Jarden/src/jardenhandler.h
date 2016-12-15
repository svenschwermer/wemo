#ifndef __JARDENHANDLER_H
#define __JARDENHANDLER_H


#include <stdio.h>
#include "WemoDB.h"
#include "controlledevice.h"
#include "global.h"
#include "logger.h"
#include "forward-reference.h"

#define DBURL    "/tmp/Belkin_settings/JardenDataUsage.db"

extern int g_cMode;
extern unsigned long int g_cStartTime;
extern unsigned long int g_cEndTime;
extern unsigned long int g_cDuration;
extern char g_cMethod[16];


int getdatafrompreventry(int SerialNumber, char* field, char* data);
void DataUsageTableUpdate(int phoneModeSet);
void senddatatocloudtask();
void* senddatatocloudtimer(void *arg);
void ClearJardenDataUsage(void);


#endif
