/***************************************************************************
*
*
* wifiSetup.h
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

#ifndef _WIFI_SETUP_H_
#define _WIFI_SETUP_H_
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "defines.h"

//Constants
#define WIFI_MAXSTR_LEN	    SIZE_64B

#define CHANNEL_LEN	    4
#define SSID_LEN	    WIFI_MAXSTR_LEN + 1
#define BSSID_LEN	    20
#define SECURITY_LEN	    23
#define SIGNAL_LEN	    9
#define WMODE_LEN	    8
#define EXTCH_LEN	    7
#define NT_LEN		    3
#define HIDDEN_LEN	    3
#define WPS_LEN		    4
#define DPID_LEN	    5

#define SSID_LOC	    CHANNEL_LEN
#define BSSID_LOC	    SSID_LOC + SSID_LEN
#define SECURITY_LOC	    BSSID_LOC + BSSID_LEN
#define SIGNAL_LOC	    SECURITY_LOC + SECURITY_LEN
#define WMODE_LOC	    SIGNAL_LOC + SIGNAL_LEN
#define EXTCH_LOC	    WMODE_LOC + WMODE_LEN
#define NT_LOC		    EXTCH_LOC + EXTCH_LEN
#define HIDDEN_LOC	    NT_LOC + NT_LEN

#ifdef WeMo_INSTACONNECT
#define VARLEN              (5+5+5+18)//hop count+Errsi+load+ApBssid

#define HOP_COUNT_LEN	    5
#define ERSSI_LEN	    5
#define LOAD_LEN	    5
#define AP_BSSID_LEN	    18

#define HOP_COUNT_LOC	    HIDDEN_LOC + HIDDEN_LEN
#define ERSSI_LOC	    HOP_COUNT_LOC + HOP_COUNT_LEN
#define LOAD_LOC	    ERSSI_LOC + ERSSI_LEN
#define AP_BSSID_LOC	    LOAD_LOC + LOAD_LEN

#define WPS_LOC		    AP_BSSID_LOC + AP_BSSID_LEN
#define SITE_SURVEY_HDR_LEN	(160 + VARLEN)
#else
#define WPS_LOC                    HIDDEN_LOC + HIDDEN_LEN
#define SITE_SURVEY_HDR_LEN	160
#endif

#define DPID_LOC	    WPS_LOC + WPS_LEN

#define RTPRIV_IOCTL_SET          (SIOCIWFIRSTPRIV + 0x02)
#define RTPRIV_IOCTL_STATISTICS   (SIOCIWFIRSTPRIV + 0x09)
#define RTPRIV_IOCTL_GSITESURVEY  (SIOCIWFIRSTPRIV + 0x0D)
#define RTPRIV_IOCTL_GET_MAC_TABLE (SIOCIWFIRSTPRIV + 0x0F)
#define RT_OID_APCLI_RSSI         0x0680

#define MIN_RSSI			    -50
#define NORMAL_RSSI			    -39
#define SCAN_MAX_DATA			    SIZE_8192B
#define MAX_LEN_OF_BSS_TABLE		    SIZE_64B
#define CONNECTION_IN_PROGRESS		    -2
#define NETWORK_NOT_FOUND		    -1
#define STATE_DISCONNECTED		    0
#define STATE_CONNECTED			    1
#define STATE_PAIRING_FAILURE_IND	    2
#define AP_MIXED_MODE_STR		    "11b/g/n"

#define STATE_INTERNET_NOT_CONNECTED	    3
#define STATE_IPADDR_NEGOTIATION_FAILED	    4
// DEFAULT GW DS
#define RBUFSIZE			    SIZE_8192B

#ifndef IF_NAMESIZE
#  ifdef IFNAMSIZ
#    define IF_NAMESIZE IFNAMSIZ
#  else
#    define IF_NAMESIZE 16
#  endif
#endif

#ifdef _OPENWRT_
#define UDHCPC_CMD  "udhcpc -i apcli0 &"
#else
#define UDHCPC_CMD  "udhcpc -i apcli0 -s /bin/udhcpc.sh &"
#endif
#define KILO    1e3
#define MEGA    1e6
#define GIGA    1e9

#define WAN_NETMASK "wan_netmask"
#define WAN_IPADDR "wan_ipaddr"

#define PRODUCT_VARIANT_NAME "WeMo_Variant"

#define PACKET_LOSE_100_PERCENT		100

typedef struct wireless_config
{
  int           has_nwid;
  int           has_freq;
  double        freq;                   /* Frequency/channel */
  int           freq_flags;
  int           has_key;
  int           key_size;               /* Number of bytes */
  int           key_flags;              /* Various flags */
  int           has_essid;
  int           essid_on;
  int           has_mode;
  int           mode;                   /* Operation mode */
} wireless_config;

typedef struct wireless_info
{
  struct wireless_config        b;      /* Basic information */

  int           has_sens;
  int           has_nickname;
  int           has_ap_addr;
  int           has_bitrate;
  int           has_rts;
  int           has_frag;
  int           has_power;
  int           has_txpower;
  int           has_retry;

  /* Stats */
  int           has_stats;
  int           has_range;

  /* Auth params for WPA/802.1x/802.11i */
  int           auth_key_mgmt;
  int           has_auth_key_mgmt;
  int           auth_cipher_pairwise;
  int           has_auth_cipher_pairwise;
  int           auth_cipher_group;
  int           has_auth_cipher_group;
} wireless_info;

typedef struct _WIFI_COUNTERS {
    unsigned long TxSuccessTotal;;
    unsigned long TxSuccessWithRetry;
    unsigned long TxFailWithRetry;
    unsigned long RtsSuccess;
    unsigned long RtsFail;
    unsigned long RxSuccess;
    unsigned long RxWithCRC;
    unsigned long RxDropNoBuffer;
    unsigned long RxDuplicateFrame;
    unsigned long FalseCCA;
    unsigned long RssiA;
    unsigned long RssiB;
} WIFI_STAT_COUNTERS,*PWIFI_STAT_COUNTERS;

typedef struct _MY_SITE_SURVEY
{
	unsigned char channel[CHANNEL_LEN];
	char ssid[SSID_LEN];
	unsigned char bssid[BSSID_LEN];
	char security[SECURITY_LEN];
	char signal[SIGNAL_LEN];
	char WMode[WMODE_LEN];
	unsigned char ExtCH[EXTCH_LEN];
	char NT[NT_LEN];
	char Hidden[HIDDEN_LEN];
#ifdef WeMo_INSTACONNECT
	char HopCount[HOP_COUNT_LEN];
	char Erssi[ERSSI_LEN];
	char Load[LOAD_LEN];
	unsigned char ApBssid[AP_BSSID_LEN];
#endif
	unsigned char WPS[WPS_LEN];
	unsigned char DPID[DPID_LEN];
} MY_SITE_SURVEY, *PMY_SITE_SURVEY;

extern char *pCommandList[];
//Data Structures

typedef struct WIFI_PAIR_PARAMS_T {
        int channel;
	char SSID[WIFI_MAXSTR_LEN];
	char AuthMode[WIFI_MAXSTR_LEN];
	char EncrypType[WIFI_MAXSTR_LEN];
	char signal [WIFI_MAXSTR_LEN];
	char Key[PASSWORD_MAX_LEN];
}WIFI_PAIR_PARAMS, *PWIFI_PAIR_PARAMS;

//char gateway[SIZE_256B];
typedef struct ROUTE_INFO{
	u_int dstAddr;
	u_int srcAddr;
	u_int gateWay;
	char ifName[IF_NAMESIZE];
}ROUTE_INFO,*PROUTE_INFO;

/** Typedef enum for server environment */
typedef enum {
        E_SERVERENV_PROD = 0x0,
        E_SERVERENV_CI,
        E_SERVERENV_QA,
        E_SERVERENV_DEV,
        E_SERVERENV_STAG,
        E_SERVERENV_ALPHA,
        E_SERVERENV_CUSTOM,
        E_SERVERENV_OTHER
} SERVERENV;

extern int g_channelAp;
extern pthread_mutex_t   s_client_state_mutex;
extern int gWiFiClientCurrState;
extern SERVERENV g_ServerEnvType;
extern int gSignalStrength;

//Prototypes
int WifiInit();
int wifiCheckConfigAvl();
int wifiCheckConfigAvl();
int wifiSetCommand (char *pCommand, char *pInterface);
int wifiGetStatus (char *pEssid, char *pApmac, char *pInterface);
int wifiPair (PWIFI_PAIR_PARAMS pWifiParams, char *pInterface);
int wifiGetNetworkList(PMY_SITE_SURVEY pAvlAPList, char *pInterface, int *pListCount);
int wifiConnectControlPt (PWIFI_PAIR_PARAMS pWifiParams,char *pInterface);
int wifiTestConnection (char *pInterface,int count,int dhcp) ;
int wifiGetStats (char *pInterface, PWIFI_STAT_COUNTERS pWiFiStats);
int wifiChangeClientIfState (int state) ;
int wifiChangeAPIfState (int state) ;
int wifisetSSIDOfAP (char *pSSID) ;
int wifisetChannelOfAP (char *nChannel);
int wifiGetAPIfState () ;
int IsNetworkMixedMode(char* szApSSID);
int wifiGetChannel(char *pInterface, int *pChannel);
int isAPConnected(char *pSSID);
int isValidIp(void);
int IsValidSSID(char* szApSSID);
void StopDhcpRequest(void);


int getCompleteAPList (PMY_SITE_SURVEY,int *);
int getCurrentClientState(void);
int wifiGetRSSI(char *pInterface);

#ifdef WeMo_INSTACONNECT
char *GetBr0InterfaceIPAddr();
#endif

#ifdef PRODUCT_WeMo_Baby
#define SetAppSSID BMSetAppSSID
#endif

#if defined(PRODUCT_WeMo_Baby) || defined(PRODUCT_WeMo_Streaming)
#define SetWiFiLED SetWiFiGPIOLED
void BMSetAppSSID();
int SetCurrentClientState(int curState);
void initClientStatus();
int getDevNetworkState (void);
int SyncAppWatchDogState(void);
int DisableAppWatchDogState(void);
int GetAppWatchDogState(void);
int ResetAppWatchDogState(void);
int getCurrentServerEnv (void) ;
int getCurrentSignalStrength (void) ;
double GetDeviceTimeZone();
#endif /*PRODUCT_WeMo_Baby*/

#endif //_WIFI_SETUP_H_
