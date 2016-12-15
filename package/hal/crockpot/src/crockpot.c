#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>

#include "crockpot.h"
#define  CROCKPOTD_PRIVATE
#include "crockpotd.h"

static int gSocket = -1;
static AdrUnion gHisAdr;

#ifdef DEBUG
#define LOG(format, ...) printf(format, ## __VA_ARGS__)
#else
#define LOG(format, ...) syslog(LOG_ERR,format, ## __VA_ARGS__)
#endif
static int GetDataFromDaemon(int Type,void *p);

void DumpHex(void *data, int len);

static int UdpSend(int fd,AdrUnion *pToAdr,CrockpotdMsg *pMsg,int MsgLen)
{
   int Ret = 0;   // assume the best
   int BytesSent;

   BytesSent = sendto(fd,pMsg,MsgLen,0,&pToAdr->s,sizeof(pToAdr->s));
   if(BytesSent != MsgLen) {
      LOG("%s: sendto failed - %s\n",__FUNCTION__,strerror(errno));
      Ret = -errno;
   }

   return Ret;
}

// Initialize Hardware Abstraction Layer for use
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_Init()
{
   int Ret = 0;

   if((gSocket = socket(AF_INET,SOCK_DGRAM,0)) == -1) {
      Ret = -errno;
      LOG("%s: socket() failed, %s\n",__FUNCTION__,strerror(Ret));
   }

   gHisAdr.i.sin_family = AF_INET;
   gHisAdr.PORT = htons(CROCKPOTD_PORT);
   gHisAdr.ADDR = inet_addr("127.0.0.1");   // localhost

   openlog("libinsight.so",0,LOG_USER);

   return Ret;
}

// Return the current mode decoded from the product LEDS.
//    < 0 - HAL error code (see ERR_HAL_*)
CROCKPOT_MODE HAL_GetMode(void)
{
   CROCKPOT_MODE Mode;
   int Err = GetDataFromDaemon(CMD_GET_MODE,&Mode);
   if(Err != 0) {
      Mode = (CROCKPOT_MODE) Err;
   }
   return Mode;
}

// Set cooking mode
// Legal values are MODE_OFF, MODE_WARM, MODE_LOW or MODE_HIGH.
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_SetMode(CROCKPOT_MODE Mode)
{
   int Ret = 0;   // assume the best

   switch(Mode) {
      case MODE_OFF:
      case MODE_WARM:
      case MODE_LOW:
      case MODE_HIGH:
         break;

      default:
         Ret = -EINVAL;
         break;
   }

   if(Ret == 0) {
   // Set the new mode
      Ret = GetDataFromDaemon(CMD_SET_MODE,&Mode);
   }

   return Ret;
}

// Return number of minutes of cook time remaining from product LED display. 
// A value of 0 indicates that the cockpot is not in a cooking mode
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_GetCookTime(void)
{
   int Ret;
   int Err = GetDataFromDaemon(CMD_GET_COOK_TIME,&Ret);
   if(Err != 0) {
      Ret = Err;
   }
   return Ret;
}

// Set number of minutes to cook time at currently selected temperature.
// The current mode must be MODE_LOW or MODE_HIGH to set a cook time. 
//  
// Note: requested value will be rounded up to nearest 30 minute increment
// (CrockPot hardware limitations) 
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_SetCookTime(int Minutes)
{
   int Ret = 0;   // assume the best
   CrockpotdMsg Request;

   do {
      if(Minutes < 0 || Minutes > MAX_COOK_TIME) {
      // Argment out of range
         Ret = -EINVAL;
         break;
      }
   // Set cook time
      Ret = GetDataFromDaemon(CMD_SET_COOK_TIME,&Minutes);
   } while(FALSE);

   return Ret;
}

int HAL_GetFWVersionNumber(char **pBuf)
{
   int Ret = 0;   // assume the best
   static char Version[MAX_VERSION_LEN];

   if(pBuf == NULL) {
      Ret = -EINVAL;
   }
   else {
      int Err = GetDataFromDaemon(CMD_GET_FW_VERSION,&Version);
      if(Err == 0) {
         *pBuf = Version;
      }
      else {
         Ret = Err;
      }
   }

   return Ret;
}

static int GetDataFromDaemon(int Type,void *p)
{
   int Ret = 0;   // assume the best
   int Err;
   int BytesRead;
   int MsgLen;
   int DataLen = 0;
   fd_set ReadFdSet;
   struct timeval Timeout;
   CrockpotdMsg Request;
   CrockpotdMsg Reply;
   socklen_t Len = sizeof(gHisAdr);

   do {
      if(p == NULL) {
      // Make sure we were passed a return data pointer
         Ret = EINVAL;
         LOG("%s: ERROR - passed null pointer\n",__FUNCTION__);
         break;
      }

      if(gSocket < 0) {
         if((Ret = HAL_Init()) != 0) {
            break;
         }
      }

      MsgLen = sizeof(Request.Hdr);
      memset(&Request,0,MsgLen);
      Request.Hdr.Cmd = Type;

      switch(Type) {
      case CMD_SET_MODE:
         DataLen = sizeof(Request.u.Mode);
         break;

      case CMD_SET_COOK_TIME:
         DataLen = sizeof(Request.u.IntData);
         break;
      }

      if(DataLen != 0) {
         MsgLen = offsetof(CrockpotdMsg,u) + DataLen;
         memcpy(&Request.u,p,DataLen);
      }

      if((Ret = UdpSend(gSocket,&gHisAdr,&Request,MsgLen)) != 0) {
         break;
      }

// Wait for data then read it
      FD_ZERO(&ReadFdSet);
      FD_SET(gSocket,&ReadFdSet);
      Timeout.tv_sec = 1;
      Timeout.tv_usec = 0;

      if((Err = select(gSocket+1,&ReadFdSet,NULL,NULL,&Timeout)) < 0) {
         LOG("%s: select failed - %s\n",__FUNCTION__,strerror(errno));
         Ret = -errno;
         break;
      }

      if(Err == 0) {
         LOG("%s: timeout waiting for data\n",__FUNCTION__);
         Ret = ERR_HAL_DAEMON_TIMEOUT;
         break;
      }

      BytesRead = recvfrom(gSocket,&Reply,sizeof(Reply),0,&gHisAdr.s,&Len);

      if(BytesRead > 0) {
         if(Reply.Hdr.Cmd != (Type | CMD_RESPONSE)) {
         // Opps
            Ret = ERR_HAL_INVALID_RESPONSE_TYPE;
         }
         else {
            int ReturnedDataLen = BytesRead-offsetof(CrockpotdMsg,u);
            Ret = Reply.Hdr.Err;
            if(ReturnedDataLen > 0) {
            // Copy returned data
               if(Type != CMD_GET_FW_VERSION) {
               // The rest of the requests are for integer data
                  ReturnedDataLen = sizeof(Reply.u.IntData);
               }
#if 0
               printf("%s: Copying %d bytes\n",__FUNCTION__,ReturnedDataLen);
#endif
               memcpy(p,&Reply.u.IntData,ReturnedDataLen);
            }
         }
      }
   } while (FALSE);

   return Ret;
}

// Shutdown the HAL deamon. 
// 
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_Shutdown()
{
   MsgHeader Dummy;
   return GetDataFromDaemon(CMD_EXIT,&Dummy);
}

// Free and release resources used by HAL
// 
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_Cleanup()
{
   if(gSocket >= 0) {
      close(gSocket);
      gSocket = -1;
   }

   return 0;
}

const char *HAL_strerror(int Err)
{
   const char *Ret = NULL;

   switch(Err) {
   case ERR_HAL_INVALID_RESPONSE_TYPE:
      Ret = "ERR_HAL_INVALID_RESPONSE_TYPE";
      break;

   case ERR_HAL_DAEMON_TIMEOUT:
      Ret = "ERR_HAL_DAEMON_TIMEOUT";
      break;

   case ERR_HAL_WRONG_MODE:
      Ret = "ERR_HAL_WRONG_MODE";
      break;

   default:
      Ret = strerror(-Err);
   }

   return Ret;
}

#if 0
#include <time.h>
#include <errno.h>

#include "crockpot.h"

#ifndef FALSE
#define FALSE     0
#endif

#ifndef TRUE
#define TRUE      (!FALSE)
#endif

#ifndef SIMULATE
#error "Only simulated operation is currently supported"

#else
// Simulated functionality

CROCKPOT_MODE gCurrentMode = MODE_IDLE;
CROCKPOT_MODE gPriorMode = MODE_IDLE;
time_t StartTime = 0;
int gCookTime = 0;
int TimeLeft;

void UpdateMode(void);

// Initialize Hardware Abstraction Layer for use
// Returns:
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_Init()
{
   return 0;
}

/*
 * Return the current mode decoded from the product LEDS.
 * < 0 - error
 */
CROCKPOT_MODE HAL_GetMode(void)
{
   UpdateMode();
   return gCurrentMode;
}

/*
 * Set cooking mode
 * Legal values are MODE_OFF, MODE_WARM, MODE_LOW or MODE_HIGH.
 * < 0 - error
 */
int HAL_SetMode(CROCKPOT_MODE Mode)
{
   int Ret = 0;   // assume the best

   switch(Mode) {
      case MODE_OFF:
      // Off clears any cook time
         gCookTime = 0;
         break;

      case MODE_WARM:
      case MODE_LOW:
      case MODE_HIGH:
         break;

      default:
         Ret = -EINVAL;
         break;
   }

   if(Ret == 0) {
   // Set the new mode
      gCurrentMode = Mode;
   }

   return Ret;
}

/*
 * Return number of minutes of cook time remaining from product LED display.
 * < 0 - error
 */
int HAL_GetCookTime(void)
{
   UpdateMode();
   return TimeLeft;
}

/*
 * Set number of minutes to cook time at currently selected temperature.
 * Note: requested value will be rounded to nearest 30 minute increment
 * (CrockPot hardware limitation)
 * < 0 - error
 */
int HAL_SetCookTime(int Minutes)
{
   int Ret = 0;   // assume the best

   do {
      if(Minutes < 0 || Minutes > MAX_COOK_TIME) {
      // Argment out of range
         Ret = -EINVAL;
         break;
      }

      if(gCurrentMode == MODE_LOW || gCurrentMode == MODE_HIGH) {
         time(&StartTime);
         gCookTime = ((Minutes + 29) / 30) * 30;
      }
      else {
         Ret = ERR_HAL_WRONG_MODE;
      }
   } while(FALSE);

   return Ret;
}


void UpdateMode(void)
{
   time_t TimeNow;

   TimeLeft = 0;

   if(gCookTime > 0) {
      if(gCurrentMode == MODE_LOW || gCurrentMode == MODE_HIGH) {
         time(&TimeNow);
         TimeLeft = gCookTime - (TimeNow-StartTime)/60;

         if(TimeLeft <= 0) {
         // Cook time is complete change to warming mode
            TimeLeft = 0;
            gCurrentMode = MODE_WARM;
         }
      }
   }
}

// Shutdown the HAL deamon. 
// 
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_Shutdown()
{
   return 0;
}


// Free and release resources used by HAL
// 
//      0 - SUCCESS
//    > 0 - standard Linux errno error code
//    < 0 - HAL error code (see ERR_HAL_*)
int HAL_Cleanup()
{
   return 0;
}
#endif
#endif
