#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "log.h"

#define TRUE   1
#define FALSE  (!TRUE)

typedef struct {
   int   LinesPerSecLimit;
   char *LogPath;
   char *LogPathOld;
   pthread_mutex_t LogMutex;
   FILE *LogFp;
   int LinesLogged;
   unsigned int MaxLogFileSize;
   int *pDaemon;
   time_t LastLogTime;
   int bLoggingEnabled;
   int bLoggingPaused;
   int bCompleteLine;
   int GoodSecs;
   int Flags;
} LoggerInternalData;

static char *MonthNames[] = {
   "Jan",
   "Feb",
   "Mar",
   "Apr",
   "May",
   "Jun",
   "Jul",
   "Aug",
   "Sep",
   "Oct",
   "Nov",
   "Dec"
};

// The following are reference by the _LOG and LOG_DEBUG macros in
// logger.h, there are not used in this file directly.

void *LogInit(
   char *LogPath,
   int MaxLinesPerSec,
   unsigned int MaxLogFileSize,
   int *bDaemon)
{
   LoggerInternalData *p = malloc(sizeof(LoggerInternalData));

   if(p != NULL) {
      memset(p,0,sizeof(LoggerInternalData));
      p->LinesPerSecLimit = MaxLinesPerSec;
      p->MaxLogFileSize = MaxLogFileSize;
      p->pDaemon = bDaemon;
      p->Flags = LOG_FLAG_MSEC;
      p->bCompleteLine = TRUE;

      if(LogPath != NULL) {
         p->LogPath = strdup(LogPath);
      }

      if(LogPath != NULL && (
            (p->LogPath) == NULL ||
            (p->LogPathOld = (char *) malloc(strlen(p->LogPath) + 5)) == NULL))
      {  // Out of memory
         if(p->LogPathOld != NULL) {
            free(p->LogPathOld);
         }

         if(p->LogPath != NULL) {
            free(p->LogPath);
         }

         free(p);
         p = NULL;
      }
      else {
         pthread_mutexattr_t MutexAttr = {{PTHREAD_MUTEX_RECURSIVE_NP}};

         pthread_mutex_init(&p->LogMutex,&MutexAttr);
         if(p->LogPath != NULL) {
            strcpy(p->LogPathOld,p->LogPath);
            strcat(p->LogPathOld,".old");
         }
         p->bLoggingEnabled = TRUE;
         p->bLoggingPaused = FALSE;
         p->GoodSecs = 0;
      }
   }

   return p;
}

int LogShutdown(void *LogHandle)
{
   LoggerInternalData *p = (LoggerInternalData *) LogHandle;

   if(p != NULL) {
      if(p->LogPathOld != NULL) {
         free(p->LogPathOld);
      }

      if(p->LogPath != NULL) {
         free(p->LogPath);
      }

      if(p->LogFp != NULL) {
         fclose(p->LogFp);
      }

      free(p);
   }
   return 0;
}

void LogIt(void *LogHandle,char *fmt,...)
{
   char  Temp[1024];
   struct timeval ltime;
   struct tm *tm;
   va_list args;
   struct timezone tz;
   int WriteLen;
   LoggerInternalData *p = (LoggerInternalData *) LogHandle;
   
   va_start(args,fmt);
   if(p != NULL) {
      pthread_mutex_lock(&p->LogMutex);

      gettimeofday(&ltime,&tz);
      tm = localtime(&ltime.tv_sec);

      if(ltime.tv_sec != p->LastLogTime) {
         p->LastLogTime = ltime.tv_sec;
         if(p->bLoggingPaused) {
            if(p->LinesLogged < p->LinesPerSecLimit) {
            // Not logging too fast at the moment
               if(p->GoodSecs++ > 3) {
               // 3 seconds of normal log activity since logging was disabled,
               // re-enable logging.
                  p->bLoggingPaused = FALSE;
                  p->LinesLogged = 1;
                  LogIt(p,"Logging resumed.\n");
               }
            }
            else {
               p->GoodSecs = 0;
            }
         }
         p->LinesLogged = 1;
      }
      else if(!p->bLoggingPaused && p->bLoggingEnabled && 
              p->LinesLogged++ > p->LinesPerSecLimit && 
              p->LinesPerSecLimit != 0) 
      {
      // Hmmm ... logging in an infinite loop perhaps?
         p->LinesLogged = 0;
         LogIt(p,"Logged %d lines in the last second, disabling logging!\n",
                 p->LinesPerSecLimit);
         p->bLoggingPaused = TRUE;
         p->GoodSecs = 0;
         if(p->LogFp != NULL) {
            fclose(p->LogFp);
            p->LogFp = NULL;
         }
         p->LinesLogged = p->LinesPerSecLimit + 2;
      }

      if(!p->bLoggingPaused && p->bLoggingEnabled) {
         if(p->LogPath != NULL && p->LogFp == NULL) {
            p->LogFp = fopen(p->LogPath,"a");
         }

         WriteLen = vsnprintf(Temp,sizeof(Temp),fmt,args);

         if(p->LogPath == NULL ||
            ((p->pDaemon == NULL || !*p->pDaemon) && isatty(fileno(stdout))))
         {  // Log to console
            printf("%s",Temp);
         }

         if(p->LogFp != NULL) {
            if(p->bCompleteLine) {
               if(p->Flags & LOG_FLAG_DATE) {
                  fprintf(p->LogFp,"%s %d ",MonthNames[tm->tm_mon],tm->tm_mday);
               }

               if(p->Flags & LOG_FLAG_MSEC) {
                  fprintf(p->LogFp,"%d:%02d:%02d.%03ld ",tm->tm_hour,tm->tm_min,
                          tm->tm_sec,ltime.tv_usec/1000);
               }
               else if(p->Flags & LOG_FLAG_TIME) {
                  fprintf(p->LogFp,"%d:%02d:%02d ",tm->tm_hour,tm->tm_min,
                          tm->tm_sec);
               }
            }
            fprintf(p->LogFp,"%s",Temp);
            fflush(p->LogFp);

            if(p->MaxLogFileSize != 0 && ftell(p->LogFp) > p->MaxLogFileSize) {
               fclose(p->LogFp);
               p->LogFp = NULL;
               unlink(p->LogPathOld);
               rename(p->LogPath,p->LogPathOld);
            }
         }
         va_end(args);
      }

      if(Temp[WriteLen-1] == '\n') {
         p->bCompleteLine = 1;
      }
      else {
         p->bCompleteLine = 0;
      }
      pthread_mutex_unlock(&p->LogMutex);
   }
   else {
   // Log file not open, just log to console
      WriteLen = vsnprintf(Temp,sizeof(Temp),fmt,args);
      printf("%s",Temp);
   }
}

void LogHex(void *LogHandle,void *AdrIn,int Len)
{
   unsigned char Line[80];
   unsigned char *cp;
   unsigned char *Adr = (unsigned char *) AdrIn;
   int i = 0;
   int j;
   LoggerInternalData *p = (LoggerInternalData *) LogHandle;

   if(p != NULL) {
      while(i < Len) {
         if(p != NULL) {
            p->LinesLogged = 0;
         }
         cp = Line;
         for(j = 0; j < 16; j++) {
            if((i + j) == Len) {
               break;
            }
            sprintf((char *) cp,"%02x ",Adr[i+j]);
            cp += 3;
         }

         *cp++ = ' ';
         for(j = 0; j < 16; j++) {
            if((i + j) == Len) {
               break;
            }
            if(isprint(Adr[i+j])) {
               *cp++ = Adr[i+j];
            }
            else {
               *cp++ = '.';
            }
         }
         *cp = 0;
         LogIt(LogHandle,"%s\n",Line);
         i += 16;
      }
   }
}

void LogReset(void *LogHandle)
{
   LoggerInternalData *p = (LoggerInternalData *) LogHandle;

   if(p != NULL) {
      p->LinesLogged = 0;
      p->bLoggingPaused = FALSE;
   }
}

void LogEnable(void *LogHandle,int bEnable)
{
   LoggerInternalData *p = (LoggerInternalData *) LogHandle;

   if(p != NULL) {
      p->bLoggingEnabled = bEnable;
   }
}

