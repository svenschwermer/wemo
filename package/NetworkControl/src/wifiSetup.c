/***************************************************************************
*
*
* wifiSetup.c
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
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/types.h>                /* for "caddr_t" et al          */
#include <linux/socket.h>               /* for "struct sockaddr" et al  */
#ifdef PRODUCT_WeMo_NetCam
#include <linux/if.h>                   /* for IFNAMSIZ and co... */
#else
#include <net/if.h>                   /* for IFNAMSIZ and co... */
#endif
#include <linux/wireless.h>
#include <math.h>
#include "global.h"
#ifndef PRODUCT_WeMo_NetCam
#include "controlledevice.h"
#endif
#include "osUtils.h"
#include "utils.h"
#include "logger.h"
#include "wifiHndlr.h"
#include "wifiSetup.h"
#include "watchDog.h"
#include "ipcUDS.h"
#include "getRemoteMac.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

#define RT_PRIV_IOCTL	(SIOCIWFIRSTPRIV + 0x01)

char *pCommandList[] = {
	"ApCliAuthMode",
	"ApCliEncrypType",
	"ApCliKey1",
	"ApCliSsid",
	"ApCliWPAPSK",
};


char 	g_szSerialNo[SIZE_64B];
char g_szApSSID[MAX_APSSID_LEN];
char g_szProdVarType[SIZE_16B];
int gRa0DownFlag=0;
int gDhcpcStarted=0;
extern int gReconnectFlag;
WIFI_PAIR_PARAMS gWiFiParams;
WIFI_PAIR_PARAMS gWifiSettings;
//globalLock
pthread_mutex_t gWifiSettingsLock;
pthread_mutex_t   s_client_state_mutex;
int gWiFiClientCurrState=0;
SERVERENV g_ServerEnvType = E_SERVERENV_PROD;
int gSignalStrength = 0;
extern int gWatchDogStatus;
int g_channelAp=0;
#define _CHECK_IP_FOR_CONNECTION_ 1
#ifdef WeMo_INSTACONNECT
extern int g_usualSetup;
#define INSTA_MAX_UDHCPC_TIMEOUT    210	//secs
#endif


/************************************************************************
 * Function: WifiInit
 *     Initialize settings configuration source.
 *  Parameters:
 *    None.
 *  Return:
 *     SUCCESS/FAILURE
 ************************************************************************/
int WifiInit()
{
    int retVal = SUCCESS;
    char command[SIZE_64B];

    retVal = osUtilsCreateLock(&gWifiSettingsLock);

    memset(command, '\0', SIZE_64B);
    strncpy(command, "SiteSurvey=1", sizeof(command)-1);
    retVal = wifiSetCommand (command,"apcli0");

    return retVal;
}

/************************************************************************
 * Function: wifiSetCommand
 *     send command to device.
 *  Parameters:
 *    pCommand - command to be sent to device.
 *  Return:
 *     SUCCESS if success else < SUCCESS
 ************************************************************************/
int wifiSetCommand (char *pCommand, char *pInterface)
{
    struct iwreq wrq;
    char data[IW_SCAN_MAX_DATA];
    int ret = 0, sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0) {
        APP_LOG("NetworkControl", LOG_ERR, "Socket Error %s", strerror(errno));
        return FAILURE;
    }

    if(!pCommand || !pInterface) {
        APP_LOG("NetworkControl", LOG_ERR, "Parameter Error");
        close (sock);
        return FAILURE;
    }

    memset(data, 0x00, IW_SCAN_MAX_DATA);

    strncpy(wrq.ifr_ifrn.ifrn_name, pInterface, IFNAMSIZ);
    strncpy(data, pCommand, sizeof(data)-1);
    wrq.u.data.length = strlen(data)+1;
    APP_LOG("NetworkControl", LOG_HIDE, "[Command] %s[len = %d]",data,wrq.u.data.length);
    wrq.u.data.pointer = data;
    wrq.u.data.flags = 0;

    ret = ioctl(sock, RTPRIV_IOCTL_SET, &wrq);

    if(ret < 0)
    {
         close(sock);
         APP_LOG("NetworkControl", LOG_ERR, "IOCTL Error %s", strerror(errno));
         return -1;
    }
    APP_LOG("NetworkControl", LOG_HIDE, "RTPRIV_IOCTL_SET %s passed\n",data);

    close(sock);
    return 0;
}

/************************************************************************
 * Function: wifiGetStatus
 *     send command to get paired essid.
 *  Parameters:
 *    essid - name of connected device.
 *  Return:
 *     SUCCESS if success else < SUCCESS
 ************************************************************************/
int wifiGetStatus (char *pEssid, char *pApmac, char *pInterface)
{
    struct iwreq wrq;
    char data[IW_ESSID_MAX_SIZE];
    int ret = 0, sock = socket(AF_INET, SOCK_DGRAM, 0);
    char macp[MAX_MAC_LEN];

    if (sock < 0) {
    	APP_LOG("NetworkControl", LOG_ERR, "Socket Error %s\n", strerror(errno));
    	return FAILURE;
    }

    if(!pEssid) {
    	APP_LOG("NetworkControl", LOG_ERR, "Param Error %s\n", strerror(errno));
        close (sock);
        return FAILURE;
    }
    if(!pApmac) {
    	APP_LOG("NetworkControl", LOG_ERR, "Param Error %s\n", strerror(errno));
        close (sock);
        return FAILURE;
    }
    memset(data, 0x00, IW_ESSID_MAX_SIZE);

    strncpy(wrq.ifr_ifrn.ifrn_name, pInterface, IFNAMSIZ);
    wrq.u.data.length = IW_ESSID_MAX_SIZE;
    wrq.u.data.pointer = data;
    wrq.u.data.flags = 0;

    ret = ioctl(sock, SIOCGIWESSID, &wrq);
    if(ret < 0)
    {
    	APP_LOG("NetworkControl", LOG_ERR, "IOCTL Error %s", strerror(errno));
        close (sock);
    	return FAILURE;
    }
    if (wrq.u.data.length < 1)
    {
        close (sock);
    	return FAILURE;
    }
    APP_LOG("NetworkControl", LOG_DEBUG, "SIOCGIWESSID = %s length %d\n",data, wrq.u.essid.length);

    memcpy(pEssid, wrq.u.essid.pointer, wrq.u.essid.length);
    pEssid[wrq.u.essid.length] = '\0';

    // AP Mac address
    if ((ioctl(sock, SIOCGIWAP, &wrq))>=0)
    {
	memset(macp, '\0', MAX_MAC_LEN);
	snprintf(macp, sizeof(macp), "%02x%02x%02x%02x%02x%02x", (unsigned char)wrq.u.ap_addr.sa_data[0], (unsigned char)wrq.u.ap_addr.sa_data[1], (unsigned char)wrq.u.ap_addr.sa_data[2], (unsigned char)wrq.u.ap_addr.sa_data[3], (unsigned char)wrq.u.ap_addr.sa_data[4], (unsigned char)wrq.u.ap_addr.sa_data[5]);
	memcpy(pApmac, macp, strlen(macp));
    }
    else
    {
    	APP_LOG("NetworkControl", LOG_ERR, "IOCTL Error %s", strerror(errno));
        close (sock);
    	return FAILURE;
    }

    close (sock);
    return SUCCESS;
}

void update_resolv_config()
{
  struct stat sb;
  struct stat sb1;

  APP_LOG("WeMoApp",LOG_DEBUG, "update_resolv_config()...");

  if( (0 == stat("/tmp/resolv.conf", &sb)) && (0 == stat("/tmp/resolv.conf.auto", &sb1)) )
  {
    APP_LOG("WeMoApp",LOG_DEBUG, "resolve.conf and resolv.conf.auto is existed...");
    system("cp /tmp/resolv.conf.auto /tmp/resolv.conf");
  }
}

void StopDhcpRequest()
{
	char cmdBuf[SIZE_256B];
	gDhcpcStarted = 0x00;
	if(gRa0DownFlag)
	{
      system("ifconfig ra0 down");//Story: 1727 "ra0 UP called in RUNdhcp to assign Valid IP"
	    APP_LOG("NetworkControl", LOG_DEBUG, "RA0 DOWN");
	    gRa0DownFlag =0;
	}

// Keep udhcpc demon to send DHCPREQUEST when half time of DHCP lease is coming
	memset (cmdBuf,0,SIZE_256B);
	strncpy(cmdBuf, "killall -9 udhcpc", sizeof(cmdBuf)-1);
	system (cmdBuf);

	update_resolv_config();
}

extern void NotifyInternetConnected();
/*
 * RunDhcpRequest:
 *  1. Stop dhcp client,if any
 *  2. Start new dhcp client
 */
int RunDhcpRequest(int restart_upnp_server)
{
    char cmdBuf[SIZE_256B];
    char pCommand[SIZE_64B];
    int ret = 0;
    memset(pCommand, '\0', SIZE_64B);

    gDhcpcStarted = 1;

    system("ifconfig ra0 up");//Story: 1727 required as without ra0 UP the dhcp request will give IP: 0.0.0.0
    if(gRa0DownFlag)
    {
        strncpy(pCommand, "HideSSID=1", sizeof(pCommand)-1);
        ret = wifiSetCommand (pCommand,"ra0");
        if(ret < 0)
        {
            APP_LOG("WiFiApp", LOG_ERR, "%s - failed", pCommand);
        }
        SetAppSSID();
    }
    memset (cmdBuf,0,SIZE_256B);
    strncpy(cmdBuf, "killall -9 udhcpc", sizeof(cmdBuf)-1);
    system (cmdBuf);

    /* setting wan_netmask & wan_ipaddr is required to get a new IP */
    SetBelkinParameter(WAN_NETMASK, "0.0.0.0");
    SetBelkinParameter(WAN_IPADDR, "0.0.0.0");

    memset (cmdBuf, 0, SIZE_256B);
    strncpy (cmdBuf, UDHCPC_CMD, sizeof(cmdBuf)-1);
    APP_LOG("NetworkControl", LOG_DEBUG, "udhcp client started, requesting IP address");

    System (cmdBuf);
    if( restart_upnp_server )
    {
      NotifyInternetConnected();
    }
    return 0;
}

int ReRunDhcpClient()
{
    char cmdBuf[SIZE_256B];

    gDhcpcStarted = 1;

    memset (cmdBuf,0,SIZE_256B);
    strncpy(cmdBuf, "killall -9 udhcpc", sizeof(cmdBuf)-1);
    system (cmdBuf);

    memset (cmdBuf, 0, SIZE_256B);
    strncpy (cmdBuf, UDHCPC_CMD, sizeof(cmdBuf)-1);
    APP_LOG("NetworkControl", LOG_DEBUG, "Restarted dhcp client");

    System (cmdBuf);
    return 0;
}

void disconnectFromRouter()
{
    char pCommand[SIZE_64B];

    memset(pCommand, '\0', SIZE_64B);
    strncpy(pCommand, "ApCliSsid= \"\"", sizeof(pCommand)-1);
    wifiSetCommand(pCommand,"apcli0");

    system("ifconfig apcli0 0.0.0.0");

    APP_LOG("NetworkControl", LOG_DEBUG,"Disconnected from router");
}

/************************************************************************
 * Function: wifiPair
 *     get settings information and pair with AP.
 *  Parameters:
 *    Pairing parameters, interface.
 *  Return:
 *     SUCCESS if success else < SUCCESS
 ************************************************************************/
#define PCMD_LEN SIZE_128B
int wifiPair (PWIFI_PAIR_PARAMS pWifiParams,char *pInterface)
{
  char buffer[SIZE_256B];
  int ret = 0;
  char pCommand[PCMD_LEN],chBuf[SIZE_16B];
  char ssid[WIFI_MAXSTR_LEN+1];
  int flag_retried = FAILURE;

  if(!pWifiParams)
  {
  	APP_LOG("NetworkControl", LOG_ERR,"Error Params");
  	return FAILURE;
  }
  APP_LOG("NetworkControl", LOG_DEBUG, "WiFi Pairing Commands start...");

pair_again:
  memset(ssid, '\0', WIFI_MAXSTR_LEN+1);
  memset(pCommand, '\0', PCMD_LEN);
  strncpy(pCommand, "ApCliEnable=1", sizeof(pCommand)-1);

  ret = wifiSetCommand (pCommand,pInterface);

  if(ret < 0)
  {
  	APP_LOG("NetworkControl", LOG_ERR, "%s - failed", pCommand);
  	return FAILURE;
  }
  else
  {
    FILE *pipe;

    strncpy(pCommand, "ifconfig | grep apcli0", sizeof(pCommand)-1);
    pipe = popen(pCommand, "r");
    if (pipe == NULL)
    {
    	APP_LOG("NetworkControl", LOG_ERR, "Popen Error %s", strerror(errno));
	resetSystem();
    }
    if (fgets(buffer, SIZE_256B, pipe) == NULL)
    {
    	APP_LOG("NetworkControl", LOG_DEBUG, "NOT FOUND apcli0: again");
    	system("ifconfig apcli0 up");
    }
    pclose(pipe);
  }

  if (IsValidSSID(pWifiParams->SSID) == 0)
  {
    APP_LOG("NetworkControl", LOG_ERR, "SiteSurvey again, SSID = %s", pWifiParams->SSID);
    EnableSiteSurvey();
    pluginUsleep(50000);
  }

	/* update the channel info with the channel number we just latched on to */
	wifiGetChannel(pInterface, &g_channelAp);
	APP_LOG("NetworkControl", LOG_DEBUG, "Updated g_channelAp to [%d] interface: %s", g_channelAp, pInterface);

	if(g_channelAp != pWifiParams->channel) {
		memset(pCommand, '\0', PCMD_LEN);
		strncpy(pCommand, "Channel=", sizeof(pCommand)-1);
		snprintf(chBuf, sizeof(chBuf), "%d",pWifiParams->channel);
		strncat(pCommand,chBuf, sizeof(pCommand)-strlen(pCommand)-1);
		ret = wifiSetCommand (pCommand,INTERFACE_AP);
		if(ret < 0)
		{
			APP_LOG("NetworkControl", LOG_ERR, "%s - failed", pCommand);
			return FAILURE;
		}
	} else {
		APP_LOG("NetworkControl", LOG_DEBUG, "Not setting channel [%d:%d]", g_channelAp,pWifiParams->channel);
	}
	strncpy(buffer,pWifiParams->AuthMode, sizeof(buffer)-1);
	if(!strcmp (buffer,"WPA1PSKWPA2PSK") || (!strcmp(buffer,"WPAPSKWPA2PSK"))) {
		//Disallow TKIP Option - For Cisco E3000 Router
		if (IsNetworkMixedMode(pWifiParams->SSID))
	        {
		    memset(pCommand, '\0', PCMD_LEN);
		    strncpy(pCommand, "WirelessMode=0", sizeof(pCommand)-1);
		    ret = wifiSetCommand (pCommand,pInterface);
		    if(ret < 0)
		    {
			APP_LOG("NetworkControl", LOG_ERR, "%s - failed", pCommand);
			return FAILURE;
		    }
		    APP_LOG("NetworkControl", LOG_DEBUG, "**********Command Set: %s",pCommand);
	            APP_LOG("NetworkControl", LOG_DEBUG, "iwpriv apcli0 set WirelessMode=0 command executed");
	        }
		memset(pCommand, '\0', PCMD_LEN);
		strncpy(pCommand, "HtDisallowTKIP=1", sizeof(pCommand)-1);
		ret = wifiSetCommand (pCommand,pInterface);
		if(ret < 0)
		{
			APP_LOG("NetworkControl", LOG_ERR, "%s - failed", pCommand);
			return FAILURE;
		}
		APP_LOG("NetworkControl", LOG_DEBUG, "**********Command Set: %s\n",pCommand);
	}

	/************************************************************************
	 *
	 *  Story: 2571
	 *  Driver returns the SSID with having non printable characters in hex string
	 *  format. Chinese SSID is also returned in Hex format.
	 *  This "convertSSID" will convert the hex string in raw bytes format,
	 *  which is used in pairing with the router
	 *
	 ************************************************************************/

	strncpy(ssid, pWifiParams->SSID, sizeof(ssid)-1);
	strncpy(ssid, convertSSID(ssid), sizeof(ssid)-1);

	memset(pCommand, '\0', PCMD_LEN);
	strncpy(pCommand, "ApCliAuthMode=", sizeof(pCommand)-1);
	if(!strcmp (buffer,"WPA1PSKWPA2PSK") || (!strcmp(buffer,"WPAPSKWPA2PSK"))) {
		strncat(pCommand, "WPAPSKWPA2PSK", sizeof(pCommand)-strlen(pCommand)-1);
	} else if (!strcmp(buffer,"WEP")) {
		strncat(pCommand, "WEPAUTO", sizeof(pCommand)-strlen(pCommand)-1);
		strncpy(pWifiParams->EncrypType,"WEP", sizeof(pWifiParams->EncrypType)-1);
	} else {
		strncat(pCommand, buffer, sizeof(pCommand)-strlen(pCommand)-1);
	}
	ret = wifiSetCommand(pCommand,pInterface);
	if(ret < 0)
	{
		APP_LOG("NetworkControl", LOG_ERR, "%s - failed", pCommand);
		return FAILURE;
	}
	memset(pCommand, '\0', PCMD_LEN);
	strncpy(pCommand, "ApCliEncrypType=", sizeof(pCommand)-1);
	strncat(pCommand, pWifiParams->EncrypType, sizeof(pCommand)-strlen(pCommand)-1);

	ret = wifiSetCommand(pCommand,pInterface);
	if(ret < 0)
	{
		APP_LOG("NetworkControl", LOG_ERR, "%s - failed", pCommand);
		return FAILURE;
	}

	if ((strcmp(pWifiParams->EncrypType, "WEP")) == 0)
	{
		// set default key id
		memset(pCommand, '\0', PCMD_LEN);
		strncpy(pCommand, "ApCliDefaultKeyID=1", sizeof(pCommand)-1);
		ret = wifiSetCommand(pCommand,pInterface);
		if(ret < 0)
		{
			APP_LOG("NetworkControl", LOG_ERR,"%s - failed", pCommand);
			return FAILURE;
		}
		memset(pCommand, '\0', PCMD_LEN);
		strncpy(pCommand, "ApCliKey1=", sizeof(pCommand)-1);
		strncat(pCommand, pWifiParams->Key, sizeof(pCommand)-strlen(pCommand)-1);
		ret = wifiSetCommand(pCommand,pInterface);
		if(ret < 0) {
			APP_LOG("NetworkControl", LOG_ERR,"%s - failed", pCommand);
			return FAILURE;
		}
		memset(pCommand, '\0', PCMD_LEN);
		strncpy(pCommand, "ApCliSsid=", sizeof(pCommand)-1);
		strncat(pCommand, ssid, sizeof(pCommand)-strlen(pCommand)-1);
		ret = wifiSetCommand(pCommand,pInterface);
		if(ret < 0) {
			APP_LOG("NetworkControl", LOG_ERR,"%s - failed", pCommand);
			return FAILURE;
		}
	} else if ((strcmp(pWifiParams->EncrypType, "TKIP")) == 0)	{
		memset(pCommand, '\0', PCMD_LEN);
		strncpy(pCommand, "ApCliSsid=", sizeof(pCommand)-1);
		strncat(pCommand, ssid, sizeof(pCommand)-strlen(pCommand)-1);
		ret = wifiSetCommand(pCommand,pInterface);
		if(ret < 0) {
			APP_LOG("NetworkControl", LOG_ERR,"%s - failed", pCommand);
			return FAILURE;
		}
		memset(pCommand, '\0', PCMD_LEN);
		strncpy(pCommand, "ApCliWPAPSK=", sizeof(pCommand)-1);
		strncat(pCommand, pWifiParams->Key, sizeof(pCommand)-strlen(pCommand)-1);
		ret = wifiSetCommand(pCommand,pInterface);
		if(ret < 0) {
			APP_LOG("NetworkControl", LOG_ERR,"%s - failed", pCommand);
			return FAILURE;
		}
	} else if ((strcmp(pWifiParams->EncrypType, "AES")) == 0) {
		memset(pCommand, '\0', PCMD_LEN);
		strncpy(pCommand, "ApCliSsid=", sizeof(pCommand)-1);
		strncat(pCommand, ssid, sizeof(pCommand)-strlen(pCommand)-1);
		ret = wifiSetCommand(pCommand,pInterface);
		if(ret < 0)
		{
			APP_LOG("NetworkControl", LOG_ERR, "%s - failed", pCommand);
			return FAILURE;
		}
		memset(pCommand, '\0', PCMD_LEN);
		strncpy(pCommand, "ApCliWPAPSK=", sizeof(pCommand)-1);
		strncat(pCommand, pWifiParams->Key, sizeof(pCommand)-strlen(pCommand)-1);
		ret = wifiSetCommand(pCommand, pInterface);
		if(ret < 0)
		{
			APP_LOG("NetworkControl", LOG_ERR,"%s - failed", pCommand);
			return FAILURE;
		}
		memset(pCommand, '\0', PCMD_LEN);
		strncpy(pCommand, "ApCliSsid=", sizeof(pCommand)-1);
		strncat(pCommand, ssid, sizeof(pCommand)-strlen(pCommand)-1);
		ret = wifiSetCommand(pCommand, pInterface);

		if(ret < 0)
		{
			APP_LOG("NetworkControl", LOG_ERR, "%s - failed", pCommand);
			return FAILURE;
		}
	}else if ((strcmp(pWifiParams->EncrypType, "TKIPAES")) == 0)	{
		memset(pCommand, '\0', PCMD_LEN);
		strncpy(pCommand, "ApCliSsid=", sizeof(pCommand)-1);
		strncat(pCommand, ssid, sizeof(pCommand)-strlen(pCommand)-1);
		ret = wifiSetCommand(pCommand, pInterface);
		if(ret < 0)
		{
			APP_LOG("NetworkControl", LOG_ERR, "%s - failed", pCommand);
			return FAILURE;
		}
		memset(pCommand, '\0', PCMD_LEN);
		strncpy(pCommand, "ApCliWPAPSK=", sizeof(pCommand)-1);
		strncat(pCommand, pWifiParams->Key, sizeof(pCommand)-strlen(pCommand)-1);
		ret = wifiSetCommand(pCommand,pInterface);
		if(ret < 0)
		{
			APP_LOG("NetworkControl", LOG_ERR, "%s - failed", pCommand);
			return FAILURE;
		}
		memset(pCommand, '\0', PCMD_LEN);
		strncpy(pCommand, "ApCliSsid=", sizeof(pCommand)-1);
		strncat(pCommand, ssid, sizeof(pCommand)-strlen(pCommand)-1);
		ret = wifiSetCommand(pCommand,pInterface);
		if(ret < 0)
		{
			APP_LOG("NetworkControl", LOG_ERR,"%s - failed", pCommand);
			return FAILURE;
		}
	}else if ((strcmp(pWifiParams->EncrypType, "NONE")) == 0)	{
		memset(pCommand, '\0', PCMD_LEN);
		strncpy(pCommand, "ApCliSsid=", sizeof(pCommand)-1);
		strncat(pCommand, ssid, sizeof(pCommand)-strlen(pCommand)-1);
		ret = wifiSetCommand(pCommand,pInterface);
		if(ret < 0)
		{
			APP_LOG("NetworkControl", LOG_ERR,"%s - failed", pCommand);
			return FAILURE;
		}
	}

	ret = isAPConnected(ssid);
	APP_LOG("NetworkControl", LOG_DEBUG, "Pairing IOCTL's:%d", ret);

	if(ret == SUCCESS)
	{
		/* set a flag which would indicate the checkInetConn thread to wait for sm time */
		gDhcpcStarted = 1;
        RunDhcpRequest(0);
	}
	if(flag_retried==SUCCESS && ret==SUCCESS)
	{
    APP_LOG("NetworkControl", LOG_DEBUG, "flag_retried and ret is SUCCESS...");
		char *pTemp = GetBelkinParameter (WIFI_CLIENT_ENCRYP);
		if(pTemp!= NULL && (strlen(pTemp) > 0))
		{
			APP_LOG("NetworkControl", LOG_DEBUG, "encryption Type is: %s", pTemp);
			if(memcmp(pWifiParams->EncrypType,pTemp,strlen(pWifiParams->EncrypType)))
			{
				SetBelkinParameter (WIFI_CLIENT_ENCRYP,pWifiParams->EncrypType);
			}
		}
		pTemp = GetBelkinParameter (WIFI_CLIENT_AUTH);
		if(pTemp!= NULL && (strlen(pTemp) > 0))
		{
			APP_LOG("NetworkControl", LOG_DEBUG, "AuthCode is: %s", pTemp);
			if(memcmp(pWifiParams->AuthMode,pTemp,strlen(pWifiParams->AuthMode)))
			{
				SetBelkinParameter (WIFI_CLIENT_AUTH,pWifiParams->AuthMode);
			}
		}
	}

	if(ret < SUCCESS)
	{
		FILE *pipe;

		APP_LOG("WiFiApp", LOG_DEBUG, "trying to connect Router Again ");
		strncpy(pCommand,"ifconfig | grep apcli0", sizeof(pCommand)-1);
		pipe = popen(pCommand,"r");
		if (pipe == NULL)
		{
			APP_LOG("NetworkControl", LOG_ERR, "Popen Error %s", strerror(errno));
			resetSystem();
		}
		if (fgets(buffer, SIZE_256B, pipe) == NULL)
		{
			APP_LOG("NetworkControl", LOG_DEBUG, "NOT FOUND apcli0: again");
			system("ifconfig apcli0 up");
		}
    pclose(pipe);

		flag_retried = findEncryptionForSsid(pWifiParams->SSID, pWifiParams->EncrypType, pWifiParams->AuthMode);

    if( flag_retried == SUCCESS )
    {
			goto pair_again;
    }
	}

	return ret;
}

int isAPConnected(char *pSSID)
{
	FILE *pipe;
	char buf[SIZE_256B] = {0,};
	char command1[SIZE_256B];
	int success=FAILURE,i=0,ret=0;
	char *start_ptr;

	for (i = 0; i < 20 ; i++)
	{
		ret = 0x00;
		success = FAILURE;

		strncpy(command1,"iwconfig apcli0", sizeof(command1)-1);
		pipe = popen(command1,"r");
		if (pipe == NULL)
		{
			APP_LOG("NetworkControl", LOG_ERR, "Popen Error %s", strerror(errno));
			resetSystem();
		}

		while (fgets(buf, SIZE_256B, pipe) != NULL)
		{
			if ((start_ptr = strstr(buf, "ESSID:")) != NULL)
			{
				char *p;

				if(pSSID)
				{
					p = strstr(buf, pSSID);
					if (p)
					{
						APP_LOG("NetworkControl", LOG_DEBUG, "# AP physically connected: pSSID:%s", pSSID);
					}
				}
				else
				{
					APP_LOG("NetworkControl", LOG_DEBUG, "# AP physically not connected: pSSID:%s", pSSID);
					ret = pclose(pipe);
					return success;
				}

				if(p)
				{
					success = SUCCESS;
					ret = pclose(pipe);
					return success;
				}
			}
		}

		ret = pclose(pipe);
		pluginUsleep(500000);
	}

	APP_LOG("NetworkControl", LOG_DEBUG, "Ap physically not connected: %s", pSSID);

	return success;
}

/**
 *  Check the AP ssid is existed or NOT
 */
int IsValidSSID(char* szApSSID)
{
    int rect = 0x00;
    int cntApCount = 0x00;
    PMY_SITE_SURVEY pCurApList = 0x00;

    if ((0x00 == szApSSID) || (0x00 == strlen(szApSSID)))
    {
        APP_LOG("NetworkControl",LOG_INFO, "IsValidSSID : wrong parameter");
        return rect;
    }

    pCurApList = (PMY_SITE_SURVEY)MALLOC(sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE);
    if(!pCurApList)
    {
        APP_LOG("NetworkControl",LOG_DEBUG, "Malloc Failed");
        return rect;
    }

    //- Get the list again
    APP_LOG("NetworkControl",LOG_DEBUG, "Reading cached AP list");
    wifiGetNetworkList(pCurApList, "apcli0", &cntApCount);
    if ((cntApCount > 0x00) && (0x00 != pCurApList))
    {
        //-Check the AP list and get the network type here
        int index = 0x00;
        APP_LOG("NetworkControl",LOG_DEBUG, "Cached AP list:%d", cntApCount);
        for (; index < cntApCount; index++)
        {
            //- compare the ssid
            if (0x00 == strlen(pCurApList[index].ssid))
                continue;

            if (0x00 == strcmp(pCurApList[index].ssid, szApSSID))
            {
                // founded
                rect = 0x01;
                break;
            }
        }
    }

    free(pCurApList);

    APP_LOG("NetworkControl",LOG_INFO, "ssid:%s %s", szApSSID, rect ? "FOUND" : "NOT FOUND");
    return rect;
}

int isValidIp()
{
    char *pIp = NULL,*pGateway = NULL;
    char *pSavedSSID = NULL;
    int count = 0;
    int maxUdhcpTimeOut = 0;
    char ssid[WIFI_MAXSTR_LEN+1];
    char rSSID[WIFI_MAXSTR_LEN+1];
    char rMAC[MAX_DVAL_LEN];

    memset(rSSID, 0x0, WIFI_MAXSTR_LEN+1);
    memset(rMAC, 0x0, MAX_DVAL_LEN);

#ifdef WeMo_INSTACONNECT
    if(!g_usualSetup)
	maxUdhcpTimeOut = INSTA_MAX_UDHCPC_TIMEOUT;
    else
	maxUdhcpTimeOut = 50;
#else
	maxUdhcpTimeOut = 50;
#endif
    while(count < maxUdhcpTimeOut)
    {
       count++;
       pIp = GetWanIPAddress ();
       APP_LOG("NetworkControl", LOG_DEBUG,"IP:%s",pIp);
       if( pIp && (0 != strcmp(pIp, "0.0.0.0")) )
       {
			/* we were anyways not checking what the gateway IP is */
            // pGateway = GetWanDefaultGateway();
            pGateway = UpdateWanDefaultGateway();
			while(0x00 == strcmp(pGateway, "0.0.0.0"))
			{
				sleep(1);
                // pGateway = GetWanDefaultGateway();
                pGateway = UpdateWanDefaultGateway();
			}

			wifiGetStatus(rSSID, rMAC, INTERFACE_CLIENT);

			APP_LOG("NetworkControl", LOG_CRIT, "IP: %s, Gateway: %s, BSSID: %s", pIp, pGateway, rMAC);
			APP_LOG("NetworkControl", LOG_DEBUG,"Update gGatewayMAC");
			if(SUCCESS != getRouterMAC(pGateway))
			{
				APP_LOG("NetworkControl", LOG_CRIT,"Reboot on getRouterMAC failed");
				exit(0);
			}

			update_resolv_config();
#ifdef _OPENWRT_
			// We have a valid IP address, start ntpclient
			createNTPUpdateThread();
#endif
		    return STATE_INTERNET_NOT_CONNECTED;
       }
	   else
	   {
	       /* Give two tries: one for 30 seconds and other for 20 seconds in usual setup, and in every 30 sec in case of inta*/
		   if(!(count%30))
		   {
			   pSavedSSID = gWiFiParams.SSID;

			   memset(ssid, '\0', WIFI_MAXSTR_LEN+1);
			   /************************************************************************
			    *  Story: 2571
			    *  This "convertSSID" will convert the hex string in raw bytes format,
			    *  which is used in pairing with the router
			    ************************************************************************/
			   strncpy(ssid, pSavedSSID, sizeof(ssid)-1);
			   strncpy(ssid, convertSSID(ssid), sizeof(ssid)-1);
			   if(isAPConnected(ssid) == SUCCESS)
			   {
				   APP_LOG("NetworkControl", LOG_DEBUG, "#To issue dhcpc again");
                   RunDhcpRequest(1);
			   }
			   else
			   {
				   APP_LOG("NetworkControl", LOG_DEBUG, "AP association lost!!!!");
				   StopDhcpRequest();

				   return STATE_DISCONNECTED;
			   }
		   }
	   }
       pluginUsleep(1000000);
    }

    StopDhcpRequest();

    return STATE_DISCONNECTED;
}

/************************************************************************
 * Function: wifiTestConnection
 *     test the internet connection
 *  Parameters:
 *    interface.
 *  Return:
 *     SUCCESS if success else < SUCCESS
 ************************************************************************/
int wifiTestConnection (char *pInterface, int count,int dhcp)
{
	int ret = 0,i=0;
	int  success = 0;
	char command1[SIZE_256B],command2[SIZE_256B],cmdBuf[SIZE_256B];
	char pingcmd[SIZE_256B];
	char *pIp,*pGateway;

	memset(command1,0,sizeof(command1));
	strncpy (command1,"ifconfig ", sizeof(command1)-1);
	strncat (command1, pInterface, sizeof(command1)-strlen(command1)-1);

	memset(command2,0,sizeof(command2));
	strncpy (command2,"iwpriv ", sizeof(command2)-1);
	strncat (command2,pInterface, sizeof(command2)-strlen(command2)-1);
	strncat (command2," show connStatus", sizeof(command2)-strlen(command2)-1);
	if(0)
	{
		strncpy(cmdBuf, "killall -9 psmon wan_connect ledctrl udhcpc", sizeof(cmdBuf)-1);
		system (cmdBuf);
		memset (cmdBuf,0,SIZE_256B);
		strncpy (cmdBuf, "udhcpc -i apcli0 -s /bin/udhcpc.sh", sizeof(cmdBuf)-1);
		System (cmdBuf);
	}
	else
	{
		pIp = GetWanIPAddress ();
		APP_LOG("NetworkControl", LOG_DEBUG,"#IP:%s", pIp);

		if(pIp && strcmp(pIp, "0.0.0.0"))
		{
            // pGateway = GetWanDefaultGateway();
            pGateway = UpdateWanDefaultGateway();
			if (pGateway && (strlen (pGateway) > 1) && (strcmp(pGateway, "0.0.0.0")) )
			{
				APP_LOG("NetworkControl", LOG_DEBUG, "#IP:%s, Gateway:%s", pIp, pGateway);
				if(checkDataBytesFromRouter())
				{
				    APP_LOG("NetworkControl", LOG_DEBUG, "Router connectivity already checked..");
				    return 1;
				}
				else
				{
				if( PACKET_LOSE_100_PERCENT != checkPingStatus(3, pGateway) )
				{
					return 1;
				}
				else
				{
					APP_LOG("NetworkControl", LOG_CRIT, "#can not ping gateway: %s", pGateway);
					gDhcpcStarted = 1;

					memset (cmdBuf,0,SIZE_256B);
					strncpy(cmdBuf, "killall -9 udhcpc", sizeof(cmdBuf)-1);
					system (cmdBuf);

					SetBelkinParameter(WAN_NETMASK, "0.0.0.0");
					SetBelkinParameter(WAN_IPADDR, "0.0.0.0");

					pluginUsleep(5000000);
					APP_LOG("NetworkControl", LOG_DEBUG, "#Request ip address again");

					memset (cmdBuf,0,SIZE_256B);
					strncpy (cmdBuf, UDHCPC_CMD, sizeof(cmdBuf)-1);
					APP_LOG("NetworkControl", LOG_DEBUG, "#Started dhcp again: %s", cmdBuf);
					system (cmdBuf);
				}
			}
			}
			else
			{
				APP_LOG("NetworkControl", LOG_CRIT, "***NO GATEWAY NOT CONNECTED***\n");
			}
		}
	}


	for(i=0;i < count;i++)
	{
		//Start the DHCP client
#ifdef _CHECK_IP_FOR_CONNECTION_

		pIp = GetWanIPAddress ();
		APP_LOG("NetworkControl", LOG_DEBUG,"IP:%s", pIp);

        // pGateway = GetWanDefaultGateway();
        pGateway = UpdateWanDefaultGateway();
		APP_LOG("NetworkControl", LOG_DEBUG,"Gateway IP:%s", pGateway);

		if(pIp)
		{
			if(strcmp(pIp,"0.0.0.0") && strcmp(pGateway, "0.0.0.0"))
			{
			    if(checkDataBytesFromRouter())
			    {
				APP_LOG("NetworkControl", LOG_DEBUG, "Router connectivity already checked..");
				success = 1;
			    }
			    else
			    {
				if( PACKET_LOSE_100_PERCENT != checkPingStatus(3, pGateway) )
					success = 1;
			}
		}
		}


		if (success == 1)
		{
			APP_LOG("NetworkControl", LOG_CRIT, "IP: %s, Gateway: %s connection tested", pIp, pGateway);
			break;
		}
#endif //_CHECK_IP_FOR_CONNECTION_
		pluginUsleep (2000000);
	}
	return success;
}

/************************************************************************
 * Function: wifiGetNetworkList
 *     get available wifi connections list.
 *  Parameters:
 *    getList - list coming from wifi.
 *  Return:
 *     SUCCESS if success else < SUCCESS
 ************************************************************************/
int wifiGetNetworkList(PMY_SITE_SURVEY SiteSurvey,
	char *pInterface, int *pListCount)
{
    struct iwreq wrq;
    int retVal = SUCCESS, apCount = 0, sock = socket(AF_INET, SOCK_DGRAM, 0);
    int i = 0;
    int TotalLen = 0;
    char *data = NULL;


    if (sock < 0)
    {
	perror("socket failed ");
	return -1;
    }

    /* buffer allocated is as per the WIFI driver. Refer: cmm_info.c*/
    /* Memory allocated for 64 entries */
    TotalLen = (MAX_LEN_OF_BSS_TABLE*sizeof(MY_SITE_SURVEY)) + SITE_SURVEY_HDR_LEN;
    data = (char *)ZALLOC(TotalLen);
    if(data == NULL)
    {
	perror("Memory allocation failed");
	return -1;
    }


    strncpy (data,"", TotalLen-1);
    strncpy(wrq.ifr_name, pInterface, sizeof(wrq.ifr_name)-1);
    wrq.u.data.length = TotalLen;
    wrq.u.data.pointer = data;
    wrq.u.data.flags = 0;

    retVal = ioctl(sock, RTPRIV_IOCTL_GSITESURVEY, &wrq);
    if(retVal < 0)
    {
	APP_LOG("NetworkControl", LOG_ERR, "IOCTL Error %s", strerror(errno));
	free(data);
	close(sock);
	return FAILURE;
    }

    APP_LOG("NetworkControl", LOG_DEBUG, "RTPRIV_IOCTL_SET passed \n");

    if (wrq.u.data.length > 0)
    {
	int len = wrq.u.data.length,j=0;
	char *ptr = data;
	char *ptr1;

	ptr = ptr + SITE_SURVEY_HDR_LEN - 1;
	while((apCount*(sizeof(MY_SITE_SURVEY)+1)) < len)
	{
	    strncpy(SiteSurvey[i].Hidden, ptr+HIDDEN_LOC, HIDDEN_LEN);
	    SiteSurvey[i].Hidden[HIDDEN_LEN-1] = '\0';

	    if(atoi(SiteSurvey[i].Hidden))
	    {
		ptr = ptr + sizeof(MY_SITE_SURVEY)+1;
		++apCount;
		continue;
	    }

#ifdef WeMo_INSTACONNECT
	    strncpy((char *)SiteSurvey[i].HopCount, ptr+HOP_COUNT_LOC, HOP_COUNT_LEN);
            SiteSurvey[i].HopCount[HOP_COUNT_LEN-1] = '\0';

            strncpy((char *)SiteSurvey[i].Erssi, ptr+ERSSI_LOC, ERSSI_LEN);
            SiteSurvey[i].Erssi[ERSSI_LEN-1] = '\0';

            strncpy((char *)SiteSurvey[i].Load, ptr+LOAD_LOC, LOAD_LEN);
            SiteSurvey[i].Load[LOAD_LEN-1] = '\0';

            strncpy((char *)SiteSurvey[i].ApBssid, ptr+AP_BSSID_LOC, AP_BSSID_LEN);
            SiteSurvey[i].ApBssid[AP_BSSID_LEN-1] = '\0';
#endif

	    strncpy((char *)SiteSurvey[i].channel, ptr, CHANNEL_LEN);
	    SiteSurvey[i].channel[CHANNEL_LEN-1] = '\0';

	    memset (&SiteSurvey[i].ssid, '\0', SSID_LEN);
	    ptr1 = ptr+SSID_LOC;
	    for (j=0; j<SSID_LEN; j++)
	    {
		if(ptr1[j] <= ' ' && ptr1[j+1] <= ' ')
		    break;
		SiteSurvey[i].ssid[j] = ptr1[j];
	    }

	    strncpy((char *)SiteSurvey[i].bssid, ptr+BSSID_LOC, BSSID_LEN);
	    SiteSurvey[i].bssid[BSSID_LEN-1] = '\0';

	    memset (&SiteSurvey[i].security, '\0', SECURITY_LEN);
	    ptr1 = ptr+SECURITY_LOC;
	    for (j=0; j<SECURITY_LEN; j++)
	    {
		if(ptr1[j] <= ' ')
		    break;
		SiteSurvey[i].security[j] = ptr1[j];
	    }
	    SiteSurvey[i].security[SECURITY_LEN-1] = '\0';
	    if(!strcmp(SiteSurvey[i].security,"NONE"))
		strncpy(SiteSurvey[i].security,"OPEN/NONE", strlen("OPEN/NONE"));

	    strncpy(SiteSurvey[i].signal, ptr+SIGNAL_LOC, SIGNAL_LEN);
	    SiteSurvey[i].signal[SIGNAL_LEN-1] = '\0';

	    memset (&SiteSurvey[i].WMode, '\0', WMODE_LEN);
	    ptr1 = ptr+WMODE_LOC;
	    for (j=0; j<WMODE_LEN; j++)
	    {
		if(ptr1[j] <= ' ')
		    break;
		SiteSurvey[i].WMode[j] = ptr1[j];
	    }
	    SiteSurvey[i].WMode[WMODE_LEN-1] = '\0';

	    strncpy((char *)SiteSurvey[i].ExtCH, ptr+EXTCH_LOC, EXTCH_LEN);
	    SiteSurvey[i].ExtCH[EXTCH_LEN-1] = '\0';

	    ptr = ptr + sizeof(MY_SITE_SURVEY)+1;
	    i++, apCount++;
	}
	*pListCount = i-2;
	APP_LOG("NetworkControl", LOG_INFO,"Count:<%d>:<%d>\n",*pListCount,apCount);
    }
    free(data);
    close(sock);
    return SUCCESS;
}

int getCompleteAPList(PMY_SITE_SURVEY SiteSurvey,
	int *pListCount)
{
    struct iwreq wrq;
    int retVal = SUCCESS, apCount = 0, sock = socket(AF_INET, SOCK_DGRAM, 0);
    int i = 0;
    int TotalLen = 0;
    char *data = NULL;


    if (sock < 0)
    {
	perror("socket failed ");
	return -1;
    }

    /* buffer allocated is as per the WIFI driver. Refer: cmm_info.c*/
    /* Memory allocated for 64 entries */
    TotalLen = (MAX_LEN_OF_BSS_TABLE*sizeof(MY_SITE_SURVEY)) + SITE_SURVEY_HDR_LEN;
    data = (char *)ZALLOC(TotalLen);
    if(data == NULL)
    {
	perror("Memory allocation failed");
	return -1;
    }


    strncpy (data,"", TotalLen-1);
    strncpy(wrq.ifr_name, "apcli0", sizeof(wrq.ifr_name)-1);
    wrq.u.data.length = TotalLen;
    wrq.u.data.pointer = data;
    wrq.u.data.flags = 0;

    retVal = ioctl(sock, RTPRIV_IOCTL_GSITESURVEY, &wrq);
    if(retVal < 0)
    {
	APP_LOG("NetworkControl", LOG_ERR, "IOCTL Error %s", strerror(errno));
	free(data);
	close(sock);
	return FAILURE;
    }

    APP_LOG("NetworkControl", LOG_DEBUG, "RTPRIV_IOCTL_SET passed \n");

    if (wrq.u.data.length > 0)
    {
	int len = wrq.u.data.length,j=0;
	char *ptr = data;
	char *ptr1;

	ptr = ptr + SITE_SURVEY_HDR_LEN - 1;
	while((i*(sizeof(MY_SITE_SURVEY)+1)) < len)
	{
	    strncpy((char *)SiteSurvey[i].channel, ptr, CHANNEL_LEN);
	    SiteSurvey[i].channel[CHANNEL_LEN-1] = '\0';

	    memset (&SiteSurvey[i].ssid, '\0', SSID_LEN);
	    ptr1 = ptr+SSID_LOC;
	    for (j=0; j<SSID_LEN; j++)
	    {
		if(ptr1[j] <= ' ' && ptr1[j+1] <= ' ')
		    break;
		SiteSurvey[i].ssid[j] = ptr1[j];
	    }

#ifdef WeMo_INSTACONNECT
	    strncpy((char *)SiteSurvey[i].HopCount, ptr+HOP_COUNT_LOC, HOP_COUNT_LEN);
            SiteSurvey[i].HopCount[HOP_COUNT_LEN-1] = '\0';

            strncpy((char *)SiteSurvey[i].Erssi, ptr+ERSSI_LOC, ERSSI_LEN);
            SiteSurvey[i].Erssi[ERSSI_LEN-1] = '\0';

            strncpy((char *)SiteSurvey[i].Load, ptr+LOAD_LOC, LOAD_LEN);
            SiteSurvey[i].Load[LOAD_LEN-1] = '\0';

            strncpy((char *)SiteSurvey[i].ApBssid, ptr+AP_BSSID_LOC, AP_BSSID_LEN);
            SiteSurvey[i].ApBssid[AP_BSSID_LEN-1] = '\0';
#endif

	    strncpy((char *)SiteSurvey[i].bssid, ptr+BSSID_LOC, BSSID_LEN);
	    SiteSurvey[i].bssid[BSSID_LEN-1] = '\0';

	    memset (&SiteSurvey[i].security, '\0', SECURITY_LEN);
	    ptr1 = ptr+SECURITY_LOC;
	    for (j=0; j<SECURITY_LEN; j++)
	    {
		if(ptr1[j] <= ' ')
		    break;
		SiteSurvey[i].security[j] = ptr1[j];
	    }
	    SiteSurvey[i].security[SECURITY_LEN-1] = '\0';
	    if(!strcmp(SiteSurvey[i].security,"NONE"))
		strncpy(SiteSurvey[i].security,"OPEN/NONE", sizeof(SiteSurvey[i].security)-1);

	    strncpy(SiteSurvey[i].signal, ptr+SIGNAL_LOC, SIGNAL_LEN);
	    SiteSurvey[i].signal[SIGNAL_LEN-1] = '\0';

	    strncpy(SiteSurvey[i].WMode, ptr+WMODE_LOC, WMODE_LEN);
	    SiteSurvey[i].WMode[WMODE_LEN-1] = '\0';

	    strncpy((char *)SiteSurvey[i].ExtCH, ptr+EXTCH_LOC, EXTCH_LEN);
	    SiteSurvey[i].ExtCH[EXTCH_LEN-1] = '\0';

	    strncpy(SiteSurvey[i].Hidden, ptr+HIDDEN_LOC, HIDDEN_LEN);
	    SiteSurvey[i].Hidden[HIDDEN_LEN-1] = '\0';

	    ptr = ptr + sizeof(MY_SITE_SURVEY)+1;
	    i++, apCount++;
	}
	*pListCount = i-2;
	APP_LOG("NetworkControl", LOG_INFO,"Count:<%d>:<%d>\n",*pListCount,apCount);
    }
    free(data);
    close(sock);
    return SUCCESS;
}

double wifi_iw_freq2float(struct	iw_freq*    in)
{
#ifdef WE_NOLIBM
  int           i;
  double        res = (double) in->m;
  for(i = 0; i < in->e; i++)
    res *= 10;
  return(res);
#else
  return ((double) in->m) * pow(10,in->e);
#endif
}


/************************************************************************
 * Function: wifiGetChanInfo
 *     get wireless interface channel information from driver
 *  Parameters:
 *    pInterface - interface
 *    info - wireless config struct pointer
 *  Return:
 *     SUCCESS if success else < SUCCESS
 ************************************************************************/
int wifiGetChanInfo(char *pInterface, struct wireless_info  *info)
{
    struct iwreq wrq;
    char data[SIZE_128B];
    int ret = 0, sock = socket(AF_INET, SOCK_DGRAM, 0);

    memset((char *) info, 0, sizeof(struct wireless_info));

    if (sock < 0) {
        APP_LOG("NetworkControl", LOG_ERR, "Socket Error %s", strerror(errno));
        return FAILURE;
    }

    if(!pInterface) {
        APP_LOG("NetworkControl", LOG_ERR, "Parameter Error");
        close (sock);
        return FAILURE;
    }

    memset(data, 0x00, sizeof(data));
    strncpy(data, "", sizeof(data)-1);

    strncpy(wrq.ifr_ifrn.ifrn_name, pInterface, IFNAMSIZ);

    wrq.u.data.length = 0;
    APP_LOG("NetworkControl", LOG_INFO, "[Command] %s[len = %d]",data,wrq.u.data.length);
    wrq.u.data.pointer = data;
    wrq.u.data.flags = 0;

    ret = ioctl(sock, SIOCGIWFREQ, &wrq);

    if(ret < 0)
    {
         close(sock);
         APP_LOG("NetworkControl", LOG_ERR, "IOCTL Error %s", strerror(errno));
         return -1;
    }
    APP_LOG("NetworkControl", LOG_DEBUG, "SIOCGIWFREQ passed\n");
    {
	info->b.has_freq = 1;
	info->b.freq = wifi_iw_freq2float(&(wrq.u.freq));
	info->b.freq_flags = 1;
    }

    close(sock);
    return 0;
}

void wifi_iw_print_freq_value(char *      buffer,
                    int         buflen,
                    double      freq)
{
  if(freq < KILO)
    snprintf(buffer, buflen, "%g", freq);
  else
    {
      char      scale;
      int       divisor;

      if(freq >= GIGA)
        {
          scale = 'G';
          divisor = GIGA;
        }
	else
        {
          if(freq >= MEGA)
            {
              scale = 'M';
              divisor = MEGA;
            }
          else
            {
              scale = 'k';
              divisor = KILO;
            }
        }
      snprintf(buffer, buflen, "%g %cHz", freq / divisor, scale);
    }
}

void wifi_iwPrintFreq(char *      buffer,
              int       buflen,
              double    freq,
              int       channel,
              int       freq_flags)
{
  char  vbuf[SIZE_16B];

  /* Print the frequency/channel value */
  wifi_iw_print_freq_value(vbuf, sizeof(vbuf), freq);

  /* Check if channel only */
  if(freq < KILO)
    snprintf(buffer, buflen, "%s", vbuf);
  else
    snprintf(buffer, buflen, "%s", vbuf);
}

/************************************************************************
 * Function: wifiGetChannel
 *     get wireless interface channel
 *  Parameters:
 *    pInterface - interface
 *    pChannel - channel
 *  Return:
 *     SUCCESS if success else < SUCCESS
 ************************************************************************/
int wifiGetChannel(char *pInterface, int *pChannel)
{
    struct wireless_info  info;
    char buffer[SIZE_128B];    /* Temporary buffer */
    char buff[SIZE_256B] = {'\0'};

    wifiGetChanInfo(pInterface, &info);

    if(info.b.has_freq)
    {
        double  freq = info.b.freq;    /* Frequency/channel */

        wifi_iwPrintFreq(buffer, sizeof(buffer), freq, -1, info.b.freq_flags);
        strncpy(buff, buffer, sizeof(buff)-1);
	if(strlen(buff))
	{
	    APP_LOG("NetworkControl", LOG_DEBUG, "Channel buff: %s", buff);
	    *pChannel = atoi(buff);
	    APP_LOG("NetworkControl", LOG_DEBUG, "Channel : %d", *pChannel);
	}
	else
	{
	    APP_LOG("NetworkControl", LOG_ERR, "Channel buff is null");
	}
    }

    return 0;
}

/************************************************************************
 * Function: wifiGetStats
 *     get WiFi Statistics Counters from driver
 *  Parameters:
 *    pInterface - interface
 *  Return:
 *     SUCCESS if success else < SUCCESS
 ************************************************************************/
int wifiGetStats (char *pInterface, PWIFI_STAT_COUNTERS pWiFiStats)
{
    struct iwreq wrq;
    char data[SIZE_2048B];
    int ret = 0, sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0) {
        APP_LOG("NetworkControl", LOG_ERR, "Socket Error %s", strerror(errno));
        return FAILURE;
    }

    if(!pInterface) {
        APP_LOG("NetworkControl", LOG_ERR, "Parameter Error");
        close (sock);
        return FAILURE;
    }

    memset(data, 0x00, SIZE_2048B);
    strncpy(data, "", sizeof(data)-1);

    strncpy(wrq.ifr_ifrn.ifrn_name, pInterface, IFNAMSIZ);

    wrq.u.data.length = 0;
    APP_LOG("NetworkControl", LOG_INFO, "[Command] %s[len = %d]",data,wrq.u.data.length);
    wrq.u.data.pointer = data;
    wrq.u.data.flags = 0;

    ret = ioctl(sock, RTPRIV_IOCTL_STATISTICS, &wrq);

    if(ret < 0)
    {
         close(sock);
         APP_LOG("NetworkControl", LOG_ERR, "IOCTL Error %s", strerror(errno));
         return -1;
    }
    APP_LOG("NetworkControl", LOG_DEBUG, "RTPRIV_IOCTL_STATISTICS, passed\n");
    {
        int i;
        char *pSP = wrq.u.data.pointer;
        unsigned long *pCounter = (unsigned long *) pWiFiStats;

        for (i=0;i<13;i++) {
            pSP = strstr (pSP,"=");
            pSP = pSP+2;
            sscanf(pSP, "%ul", (unsigned int *)&pCounter[i]);
        }

        //Print Some data Here
	APP_LOG("NetworkControl", LOG_INFO,"TxSuccess:<%d>\n",pWiFiStats->TxSuccessTotal);
	APP_LOG("NetworkControl", LOG_INFO,"RSSIA:<%d>\n",pWiFiStats->RssiA);
	APP_LOG("NetworkControl", LOG_INFO,"RSSIB:<%d>\n",pWiFiStats->RssiB);


    }

    close(sock);
    return 0;
}

/************************************************************************
 * Function: wifiGetAPIfState
 *     get off/On AP mode
 *  Parameters:
 *     State- NONE
 *  Return:
 *     SUCCESS if success else < SUCCESS
 ************************************************************************/
int wifiGetAPIfState () {
    return GetEnableAP();
}

/************************************************************************
 * Function: wifiChangeAPIfState
 *     to
 *     swith off/On AP mode
 *  Parameters:
 *     State- ON/OFF
 *  Return:
 *     SUCCESS if success else < SUCCESS
 ************************************************************************/
int wifiChangeAPIfState (int state) {
	int retVal=SUCCESS;
    char cmd[SIZE_256B];

    memset (cmd,0x0,SIZE_256B);
    strncpy (cmd,"ifconfig ", sizeof(cmd)-1);
    strncat (cmd,INTERFACE_AP,sizeof(cmd)-strlen(cmd)-1);
    strncat (cmd," ",sizeof(cmd)-strlen(cmd)-1);
    switch (state) {
        case TURN_ON:
        {
			strncat (cmd,"up",sizeof(cmd)-strlen(cmd)-1);
        }
        break;
        case TURN_OFF:
        {
			strncat (cmd,"down",sizeof(cmd)-strlen(cmd)-1);
        }
        break;
    }

	retVal = system (cmd);
    return SUCCESS;
}

/************************************************************************
 * Function: wifiChangeClientIfState
 *     to
 *     Enable/Disable Client mode
 *  Parameters:
 *     State- ON/OFF
 *  Return:
 *     SUCCESS if success else < SUCCESS
 ************************************************************************/
int wifiChangeClientIfState (int state) {
    char cmd[SIZE_256B];
    int retVal=SUCCESS;

    memset (cmd,0x0,SIZE_256B);
    strncpy(cmd,"ApCliEnable=",sizeof(cmd)-1);
    switch (state) {
        case TURN_ON:
        {
            strncat (cmd,"1",sizeof(cmd)-strlen(cmd)-1);
        }
        break;
        case TURN_OFF:
        {
            strncat (cmd,"0",sizeof(cmd)-strlen(cmd)-1);

        }
        break;
    }
    retVal = wifiSetCommand (cmd,INTERFACE_CLIENT);
    //Enable SiteSurvey Here itself.
    if(SUCCESS == retVal) {
        memset (cmd,0x0,SIZE_256B);
        strncat (cmd,"SiteSurvey=1",sizeof(cmd)-strlen(cmd)-1);
        strncpy(cmd,"SiteSurvey=1",sizeof(cmd)-1);
        retVal = wifiSetCommand (cmd,INTERFACE_CLIENT);
    }
    return retVal;
}

/************************************************************************
 * Function: wifisetSSIDOfAP ()
 *     to
 *     change the SSID of AP.
 *  Parameters:
 *     New Name
 *  Return:
 *     SUCCESS if success else < SUCCESS
 ************************************************************************/
int wifisetSSIDOfAP (char *pSSID) {
    char cmd[SIZE_64B];

    if(!pSSID) {
        APP_LOG("NetworkControl", LOG_ERR, "Param ER \n");
        return FAILURE;
    }
    else
    {
        FILE *pipe;
        char buffer[SIZE_256B];

        strncpy(cmd, "ifconfig | grep ra0", sizeof(cmd)-1);
        pipe = popen( cmd, "r");
        if (pipe == NULL)
        {
        	APP_LOG("NetworkControl", LOG_ERR, "Popen Error %s", strerror(errno));
		resetSystem();
        }
        if (fgets(buffer, SIZE_256B, pipe) == NULL)
        {
        	APP_LOG("NetworkControl", LOG_DEBUG, "NOT FOUND ra0");
        	system("ifconfig ra0 up");
        }
        pclose(pipe);
    }
   memset(cmd, '\0',sizeof(cmd));
    strncpy(cmd, "SSID=", sizeof(cmd)-1);
    strncat(cmd, pSSID,sizeof(cmd)-strlen(cmd)-1);
    return wifiSetCommand (cmd,INTERFACE_AP);
}

/************************************************************************
 * Function: wifisetChannelOfAP ()
 *     to
 *     change the channel of AP.
 *  Parameters:
 *     channel value
 *  Return:
 *     SUCCESS if success else < SUCCESS
 ************************************************************************/
int wifisetChannelOfAP (char *nChannel) {
    char cmd[SIZE_64B];

    if(!nChannel) {
        APP_LOG("NetworkControl", LOG_ERR, "Param ER \n");
        return FAILURE;
    }
    memset(cmd, '\0', SIZE_64B);
    strncpy(cmd, "Channel=", sizeof(cmd)-1);
    strncat(cmd, nChannel,sizeof(cmd)-strlen(cmd)-1);
    return wifiSetCommand (cmd,INTERFACE_AP);
}
/**
 *  Check the AP ssid is mixed mode or NOT
 *
 *
 *
 */
int IsNetworkMixedMode(char* szApSSID)
{

    int rect = 0x00;
    int cntApCount = 0x00;
    PMY_SITE_SURVEY pCurApList = 0x00;
    if ((0x00 == szApSSID) || (0x00 == strlen(szApSSID)))
    {
        return rect;
    }
    else
    {
        APP_LOG("UPNPDevice",LOG_INFO, "To match ssid:%s", szApSSID);
    }


    pCurApList = (PMY_SITE_SURVEY)ZALLOC(sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE);
    if(!pCurApList)
    {
	    APP_LOG("UPNPDevice",LOG_DEBUG, "Malloc Failed");
	    return rect;
    }

    //- Get the list again
    APP_LOG("UPNPDevice",LOG_DEBUG, "To get cached AP list");
    wifiGetNetworkList(pCurApList, "apcli0", &cntApCount);

    if ((cntApCount > 0x00) && (0x00 != pCurApList))
    {
        //-Check the AP list and get the network type here
        int index = 0x00;
        APP_LOG("UPNPDevice",LOG_DEBUG, "Cached AP list:%d", cntApCount);
        for (; index < cntApCount; index++)
        {
            //- compare the ssid
            if (0x00 == strlen(pCurApList[index].ssid))
                continue;

            if (0x00 == strcmp(pCurApList[index].ssid, szApSSID))
            {
                //- Get the target
                APP_LOG("UPNPDevice", LOG_INFO, "found target SSID:%s:%s", szApSSID, pCurApList[index].WMode);

                if (0x00 == strcmp(AP_MIXED_MODE_STR, pCurApList[index].WMode))
                {
                    //- Mixed mode
                    rect = 0x01;
                    APP_LOG("UPNPDevice", LOG_DEBUG, "Network mode: mixed mode, network mode adjusted");
                }
                else
                {
                    APP_LOG("UPNPDevice", LOG_DEBUG, "Network mode: normal mode, network mode adjusted:%s",
                        pCurApList[index].WMode);
                }

                break;
            }
        }
    }

    free(pCurApList);

    return rect;
}

#ifdef WeMo_INSTACONNECT
char *GetBr0InterfaceIPAddr()
{
	int sock;
	struct ifreq ifr;
	int ret;
	char *pIfName = INTERFACE_BR;
	static char OutIpAddress[SIZE_16B];

	/*Copy default ip address*/
	memset(OutIpAddress, 0, sizeof(OutIpAddress));
	strncpy(OutIpAddress, "0.0.0.0", sizeof(OutIpAddress)-1);

	/*open an IP socket*/
	if (0 > (sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)))
	{
		APP_LOG("UPnP Device", LOG_DEBUG, "Cannot open socket! Interface %s IpAddress %s", pIfName, "0.0.0.0");
		return OutIpAddress;
	}

	/*ioctl to get interface ip address*/
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, pIfName, IFNAMSIZ-1);
	if (ioctl(sock, SIOCGIFADDR, &ifr) < 0)
	{
		ret = close(sock);
		if(ret < 0)
			APP_LOG("UPnP Device", LOG_DEBUG, "Error in closing socket! Interface %s IpAddress %s", pIfName, "0.0.0.0");
		return OutIpAddress;
	}

	/*get ipaddress in dot format*/
	memset(OutIpAddress, 0, sizeof(OutIpAddress));
	strncpy(OutIpAddress, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), sizeof(OutIpAddress)-1);
	APP_LOG("UPnP Device", LOG_DEBUG, "pIfName:%s, InterfaceIPAddress:%s", pIfName,OutIpAddress);

	/*close socket*/
	ret = close(sock);
	if(ret < 0)
		APP_LOG("UPnP Device", LOG_DEBUG, "Error in closing socket");

	return OutIpAddress;
}
#endif

int getCurrentClientState(void)
{
        int state;
        osUtilsGetLock(&s_client_state_mutex);
        state = gWiFiClientCurrState;
        osUtilsReleaseLock(&s_client_state_mutex);
        return state;
}

int wifiGetRSSI(char *pInterface)
{
	struct iwreq wrq;
	int rssiData = 0;
	unsigned int Rssi_Quality = 0;
	int ret = 0, sock = -1;
#ifdef PRODUCT_WeMo_NetCam
	char *pData;

#define WIFI_STAT_BUF_SIZE 2048
	pData = ZALLOC(WIFI_STAT_BUF_SIZE);
	if (pData == NULL) {
	  APP_LOG("NetworkControl", LOG_ERR, "No more memory available.");
	  return Rssi_Quality;
	}

#endif


	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		APP_LOG("NetworkControl", LOG_DEBUG, "Socket Error");
		return Rssi_Quality;
	}

	if(!pInterface)
	{
		APP_LOG("NetworkControl", LOG_DEBUG, "Parameter Error");
		close (sock);
		return Rssi_Quality;
	}

	strncpy(wrq.ifr_name, pInterface, sizeof(wrq.ifr_name)-1);

#ifdef PRODUCT_WeMo_NetCam
	wrq.u.data.length = 0;
	wrq.u.data.pointer = pData;
	wrq.u.data.flags = 0x0;
	ret = ioctl(sock, RTPRIV_IOCTL_STATISTICS, &wrq);
#else
	wrq.u.data.length = sizeof(rssiData);
	wrq.u.data.pointer = (void*)(&rssiData);
	wrq.u.data.flags = RT_OID_APCLI_RSSI;

	ret = ioctl(sock,RT_PRIV_IOCTL,&wrq);
#endif
	if(ret < 0)
	{
		close(sock);
		APP_LOG("NetworkControl", LOG_DEBUG,"IOCTL Error %d",ret);
		return Rssi_Quality;
	}

	if (wrq.u.data.length > 0) {
#ifdef PRODUCT_WeMo_NetCam
	  char *sp;

#if defined(NETCAM_HD_V2) || defined(NETCAM_LS)
	  int rssi0, rssi1, rssi2;

	  sp = strstr(pData, "RSSI");
	  if (sp == NULL) {
	    APP_LOG("NetworkControl", LOG_ERR, "No information related to RSSI.");
	    close(sock);
	    free(pData);
	    return Rssi_Quality;
	  }

	  sp = strstr(sp, "=");
	  sp += 2;
	  sscanf(sp, "%d %d %d", &rssi0, &rssi1, &rssi2);

	  if (rssi2)
	    rssiData = (rssi0+rssi1+rssi2)/3;
	  else
	    rssiData = (rssi0+rssi1)/2;
#endif
#if defined (NETCAM_HD_V1) || defined (NETCAM_NetCam)
	  int rssi0;

	  sp = strstr(pData, "RSSI-A");
	  if (sp == NULL) {
	    APP_LOG("NetworkControl", LOG_ERR, "No information related to RSSI-A.");
            close(sock);
            free(pData);
            return Rssi_Quality;
          }

	  sp = strstr(sp, "=");
	  sp += 2;
          sscanf(sp, "%d", &rssi0);

	  rssiData = rssi0;
#endif /* NETCAM_HDV1, NETCAM SD */

#else
	  APP_LOG("NetworkControl", LOG_DEBUG,"rssiData:%d",rssiData);
#endif
		if(rssiData >= -50)
			Rssi_Quality = 100;
		else if(rssiData >= -80)    /* between -50 ~ -80dbm*/
			Rssi_Quality = (unsigned int)(24 + ((rssiData + 80) * 26)/10);

		else if(rssiData >= -90)   /* between -80 ~ -90dbm*/
			Rssi_Quality = (unsigned int)(((rssiData + 90) * 26)/10);
		else    /* < -84 dbm*/
			Rssi_Quality = 0;
		APP_LOG("NetworkControl", LOG_DEBUG,"Rssi quality in percent:%u%%", Rssi_Quality);
	}
	close(sock);
 Rssi_out:
#ifdef PRODUCT_WeMo_NetCam
	free(pData);
#endif
	return Rssi_Quality;
}

#if defined(PRODUCT_WeMo_Baby) || defined(PRODUCT_WeMo_Streaming)
void BMSetAppSSID()
{
	char szBuff[SIZE_32B];
	int index = 0;
	char* ProductPrefix = "WeMo_Baby";
	char* BM_DEFAULT_SERIAL_NO = "0123456789";
	int   BM_DEFAULT_SERIAL_TAILER_SIZE = 3;

	memset(szBuff, 0x00, sizeof(szBuff));
	memset(g_szProdVarType, 0x00, sizeof(g_szProdVarType));

	char *pProdVar = GetBelkinParameter (PRODUCT_VARIANT_NAME);
	if(pProdVar!= NULL && (strlen(pProdVar) > 0))
	{
	    APP_LOG("UPNP", LOG_DEBUG, "Product Variant name: %s", pProdVar);
	    snprintf(szBuff, sizeof(szBuff), "%sBaby.", pProdVar);
	    for (index = 0; szBuff[index]; index++)
		g_szProdVarType[index] = tolower(szBuff[index]);
	    g_szProdVarType[index - 1] = '\0';
	    APP_LOG("UPNP", LOG_DEBUG, "Product Variant type: %s", g_szProdVarType);
	}
	else
	    snprintf(szBuff, sizeof(szBuff), "%s.", ProductPrefix);

	char* szSerialNo = GetSerialNumber();
	if ((0x00 == szSerialNo) || (0x00 == strlen(szSerialNo)))
	{
		//-User default one
		szSerialNo = BM_DEFAULT_SERIAL_NO;
	}

	strncpy(g_szSerialNo, szSerialNo, sizeof(g_szSerialNo)-1);
	strncat(szBuff, szSerialNo + strlen(szSerialNo) - BM_DEFAULT_SERIAL_TAILER_SIZE, sizeof(szBuff)-strlen(szBuff)-1);

        APP_LOG("UPNP: Device", LOG_DEBUG, "APSSID: %s", szBuff);
        APP_LOG("STARTUP: Device", LOG_DEBUG, "serial: %s", g_szSerialNo);

	wifisetSSIDOfAP(szBuff);

	memset(g_szApSSID, 0x00, sizeof(g_szApSSID));
        strncpy(g_szApSSID, szBuff, sizeof(g_szApSSID)-1);
}


double GetDeviceTimeZone() {
	char *lasttimezone = GetBelkinParameter(SYNCTIME_LASTTIMEZONE);
	double flastTimeZone = 0.;
	if (0x00 != lasttimezone && (0x00 != strlen(lasttimezone))){
		flastTimeZone = atof(lasttimezone);
		APP_LOG("NetworkControl", LOG_INFO, "Getting g_lastTimeZone: %f ...success", flastTimeZone);
	}else{
		APP_LOG("NetworkControl", LOG_ERR, "Getting g_lastTimeZone ... failure");
  }
	return flastTimeZone;
}

int SetCurrentClientState(int curState)
{
	char cstate[SIZE_4B];
	memset(cstate, 0x0, SIZE_4B);
	osUtilsGetLock(&s_client_state_mutex);
	gWiFiClientCurrState = curState;
	snprintf(cstate, sizeof(cstate), "%d", curState);
	osUtilsReleaseLock(&s_client_state_mutex);

	UDS_SetCurrentClientState();
	return 0x00;
}

int getCurrentServerEnv (void)
{
        int ServerEnv;
        osUtilsGetLock(&s_client_state_mutex);
        ServerEnv = (int)g_ServerEnvType;
        osUtilsReleaseLock(&s_client_state_mutex);
        return ServerEnv;
}


int getCurrentSignalStrength (void)
{
        int SignalStrength;
        osUtilsGetLock(&s_client_state_mutex);
        SignalStrength = gSignalStrength;
        osUtilsReleaseLock(&s_client_state_mutex);
        return SignalStrength;
}

int getDevNetworkState (void) {
        return GetNetworkState();
}

int SyncAppWatchDogState(void) {
	return	PunchAppWatchDogState();
}

int DisableAppWatchDogState(void) {
        gWatchDogStatus = 3;
	return 0x00;
}

int GetAppWatchDogState(void) {
        return gWatchDogStatus;
}

int ResetAppWatchDogState(void) {
        gWatchDogStatus = 0;
	return 0x00;
}

#endif
