#ifdef NFC_CONFIG
/***************************************************************************
* NfcConfig.c
*
* Created by Belkin International, Software Engineering on 5/20/2013.
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

#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#include "i2c_drv.h"
#include "upnp/LinkedList.h"
#include "timer.h"
#include "wifiHndlr.h"
#include "logger.h"
#include "wifiSetup.h"
#include "global.h"
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

#ifndef FALSE
#define FALSE	0
#endif

#ifndef TRUE
#define TRUE (!FALSE)
#endif

// Capability container
#define CC0_MAGIC    0xe1

// External global data
extern int gAppCalledCloseAp;
extern int gBootReconnect; 
extern char gUserKey[];
extern ithread_t CloseApWaiting_thread;

// External functions
extern void *CloseApWaitingThread(void *args);
void encryptPassword(char *input,char *finalstr);

// Public functions
void StartNfcThread(void);

// Data extracted from URIs in the format of 
// "wifi://[network ssid]/[wep|wpa|open]/[network key]"
static char *gNfcSSID = NULL;
static char *gNfcAuth = NULL;
static char *gNfcKey = NULL;
static ithread_t gNfcMainLoop_thread;

// Internal functions
static void *NfcMainLoop(void *args);
static int NfcTryConnect(void);
static int NfcConnect(int,char *,char *,char *,char *);
static int ReadNfcContents(void);
static int ReadByte(int fd,int Adr);

void StartNfcThread(void)
{
// Start a thread to handle NFC based configuration
	ithread_create(&gNfcMainLoop_thread,NULL,NfcMainLoop,NULL);
}


static void *NfcMainLoop(void *args)
{
	int bRun = TRUE;
	int bNfcRequestedConnect = FALSE;
	int ClientState;

    	tu_set_my_thread_name( __FUNCTION__ );

	APP_LOG("WiFiApp",LOG_DEBUG,"NfcMainLoop running...");
	while(bRun) {
		if(!bNfcRequestedConnect) {
			if(ReadNfcContents() > 0 && NfcTryConnect() == SUCCESS) {
				bNfcRequestedConnect = TRUE;
			}
		}

		switch((ClientState = getCurrentClientState())) {
			case STATE_PAIRING_FAILURE_IND:
			case STATE_IPADDR_NEGOTIATION_FAILED:
				if(bNfcRequestedConnect) {
					APP_LOG("WiFiApp",LOG_DEBUG,"Pairing failed (%d)",ClientState);
					bNfcRequestedConnect = FALSE;
				}
				break;

			case STATE_CONNECTED:
				if(bNfcRequestedConnect) {
					ithread_create(&CloseApWaiting_thread,NULL,CloseApWaitingThread,NULL);
					APP_LOG("WiFiApp",LOG_DEBUG,"Pairing successful, closing AP");
				}
				bRun = FALSE;
				break;
		}
		pluginUsleep(1000000);	// sleep for 1 second
	}

	APP_LOG("WiFiApp",LOG_DEBUG,"Exiting");

	return NULL;	
}


// return SUCCESS if we were able to read WiFi configuration from
// the NFC chip and started to try to connect using the data, 
// otherwise FAILURE.
static int NfcTryConnect()
{
	PMY_SITE_SURVEY p = NULL;
	int i=0, count=0;
	int Ret = FAILURE;	// assume the worse
	char *auth = "";
	char *encrypt = "";
	char *cp;
	int Chan;

	do {
	// gNfcSSID, gNfcAuth and gNfcKey should be available now
		if(gNfcSSID == NULL || strlen(gNfcSSID) == 0 ||
		   gNfcAuth == NULL || strlen(gNfcAuth) == 0 ||
			gNfcKey == NULL)
		{
		// Internal error
			break;
		}

		APP_LOG("WiFiApp",LOG_DEBUG,"gNfcSSID:%s",gNfcSSID);
		APP_LOG("WiFiApp",LOG_DEBUG,"gNfcAuth:%s",gNfcAuth);
		APP_LOG("WiFiApp",LOG_DEBUG,"gNfcKey:%s",gNfcKey);

	// The encryption type and channel number aren't stored via NFC so
	// we need to get them from a scan

		p = (PMY_SITE_SURVEY) malloc (sizeof(MY_SITE_SURVEY)*100);

		EnableSiteSurvey();
		pluginUsleep(50000);
		getCompleteAPList(p,&count);

		for(i=0; i < count; i++) {
			if(!strcmp(p[i].ssid,gNfcSSID)) {
			// Found it, save the parameters
				auth = p[i].security;
				if((cp = strchr(auth,'/')) != NULL) {
					*cp++ = 0;
					encrypt = cp;
				}
				APP_LOG("WiFiApp",LOG_DEBUG,
						  "Channel determined from SITE SURVEY: %s",
						  p[i].channel);
				APP_LOG("WiFiApp",LOG_DEBUG,
						  "auth determined from SITE SURVEY: %s",auth);
				APP_LOG("WiFiApp",LOG_DEBUG,
						  "encrypt determined from SITE SURVEY: %s",encrypt);

				APP_LOG("WiFiApp",LOG_DEBUG,
						  "hidden from SITE SURVEY: %s",p[i].Hidden);

				if(atoi(p[i].Hidden)) {
				// Set special hidden network pseudo channel number
					Chan = CHAN_HIDDEN_NW;
				}
				else {
					Chan = atoi((char *) p[i].channel);
				}
				Ret = SUCCESS;
				break;
			}
		}
		if(Ret == SUCCESS) {
			Ret = NfcConnect(Chan,gNfcSSID,auth,encrypt,gNfcKey);
		}
		else {
			APP_LOG("WiFiApp",LOG_ERR,
					  "SSID: %s not found in the available network list",gNfcSSID);
		}
		free(p);
	} while(FALSE);

	return Ret;
}


static 
int NfcConnect(int chan,char *ssid,char *auth,char *encrypt,char *password)
{
	int Ret = FAILURE;

	do {
		APP_LOG("UPNPDevice",LOG_CRIT,"%s",__FUNCTION__); 
		if(isSetupRequested()) {
			APP_LOG("UPNPDevice",LOG_ERR,"#### Setup request already executed ######");
			break;
		}

		APP_LOG("UPNPDevice",LOG_DEBUG,
			     "Attempting to connect home network: %s, channel: %d, auth:%s,"
			     "encrypt:%s, password:%s",ssid,chan,auth,encrypt,password);

		gAppCalledCloseAp = 0;
		gBootReconnect = 0; 
		setSetupRequested(1);
	// Save the password in a global variable - to be used later by WifiConn thread
		strncpy(gUserKey,password,PASSWORD_MAX_LEN);
		if(chan == CHAN_HIDDEN_NW) {
			PWIFI_PAIR_PARAMS p = ZMALLOC(sizeof(WIFI_PAIR_PARAMS));

			strncpy(p->SSID,ssid,sizeof(p->SSID)-1);
			strncpy(p->AuthMode,auth,sizeof(p->AuthMode)-1);
			strncpy(p->EncrypType,encrypt,sizeof(p->EncrypType)-1);
			strncpy(p->Key,password,sizeof(p->Key)-1);
			p->channel = CHAN_HIDDEN_NW;
			Ret = threadConnectHiddenNetwork(p);
		}
		else {
			Ret = threadConnectHomeNetwork(chan,ssid,auth,encrypt,password);
		}
	} while(FALSE);

	return Ret;
}

// The app processes URIs in the format of 
// "wifi://[network ssid]/[wep|wpa|open]/[network key]"
// Return:
//     < 0 on error,
//    == 0 if data hasn't changed since last read
//    > 0 if new data has been read
int ParseURI(char *Uri)
{
   char *cp;
   char *ssid;
   char *encryption;
   char *key;
	char EncryptedPass[PASSWORD_MAX_LEN];
   int Ret = 0;

   do {
      if (strncmp(Uri,"wifi://",7) != 0) {
         // Not the droid we're looking for
			APP_LOG("WiFiApp",LOG_DEBUG,
					  "Ignoring URI that does not begin with \"wifi://\"");
         Ret = -EINVAL;
         break;
      }

      ssid = &Uri[7];
      if ((cp = strchr(ssid,'/')) == NULL) {
			APP_LOG("WiFiApp",LOG_DEBUG,
				     "Error ssid termination character not found\n");
         Ret = -EINVAL;
         break;
      }

      *cp++ = 0;
      encryption = cp;
      if ((cp = strchr(cp,'/')) == NULL) {
			APP_LOG("WiFiApp",LOG_DEBUG,
				     "Error ssid encryption character not found");
         Ret = -EINVAL;
         break;
      }
      *cp++ = 0;
      key = cp;

		APP_LOG("WiFiApp",LOG_DEBUG,
				  "ssid: %s, auth: %s, key: %s",ssid,encryption,key);

      if(gNfcSSID == NULL || strcmp(gNfcSSID,ssid) != 0) {
      // SSID has changed
         Ret = 1;
         if(gNfcSSID != NULL) {
            free(gNfcSSID);
         }
         gNfcSSID = strdup(ssid);
      }

      if(gNfcAuth == NULL || strcmp(gNfcAuth,encryption) != 0) {
      // The authentication method has changed
         Ret = 1;
         if(gNfcAuth != NULL) {
            free(gNfcAuth);
         }
         gNfcAuth = strdup(encryption);
      }
		encryptPassword(key,EncryptedPass);

		if(gNfcKey == NULL || strcmp(gNfcKey,EncryptedPass) != 0) {
      // Key has changed
         if(gNfcKey != NULL) {
            free(gNfcKey);
         }
         gNfcKey = strdup(EncryptedPass);
         Ret = 1;
      }
   } while (0);

   return Ret;
}

// Return:
//     < 0 on error,
//    == 0 if data hasn't changed since last read
//    > 0 if new data has been read
static int ReadNfcContents()
{
   unsigned long Len;
   char *Temp = NULL;
   int i;
   int x;
   int StartAdr;
   int Ret = -1;	// assume the worse
	int fd = -1;
   
   do {
		if((fd = open("/dev/" I2C_DEV_NAME,O_RDONLY)) < 0) {
			APP_LOG("WiFiApp",LOG_DEBUG,"open() failed - %s",strerror(errno));
			Ret = -errno;
			break;
		}

      ioctl(fd,RT2880_I2C_SET_ADDR,0x53);
      if((x = ReadByte(fd,0)) < 0) {
         break;
      }

      if(x != CC0_MAGIC) {
			APP_LOG("WiFiApp",LOG_DEBUG,
					   "CC0_MAGIC not found (is 0x%x, should be 0x%x)\n",
                  x,CC0_MAGIC);
         break;
      }

   // Get payload length
      if((Len = ReadByte(fd,8)) < 0) {
         break;
      }

      if((Temp = malloc(Len)) == NULL) {
			APP_LOG("WiFiApp",LOG_ERR,"malloc failed");
			Ret = -ENOMEM;
         break;
      }

		APP_LOG("WiFiApp",LOG_DEBUG,"read %d bytes payload",Len);

   // Get record type
      if((x = ReadByte(fd,9)) < 0) {
         break;
      }

      if(x != 'U') {
      // URI record
			APP_LOG("WiFiApp",LOG_DEBUG,"Error - Unsupported record type 0x%x\n",x);
			break;
		}

		Len -= 1;
		StartAdr = 0xb;
      for(i = 0; i < Len; i++) {
      // Read payload
         if((x = ReadByte(fd,StartAdr + i)) < 0) {
            break;
         }
         Temp[i] = (char) x;
      }
		if(x < 0) {
			break;
		}
      Temp[i] = 0;
		Ret = ParseURI(Temp);
   } while (FALSE);

   if(Temp != NULL) {
      free(Temp);
   }

	if(fd >= 0) {
		close(fd);
	}

   return Ret;
}

static int ReadByte(int fd,int Adr)
{
   unsigned long x = (unsigned long) Adr;
   int Ret;

   if(ioctl(fd,RT2880_I2C_READ,&x) != 0) {
      Ret = -errno;
   }
   else {
      Ret = (int) x;
   }

   return Ret;
}

#endif 	// NFC_CONFIG

