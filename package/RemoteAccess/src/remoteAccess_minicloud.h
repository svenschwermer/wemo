/***************************************************************************
*
*
* remoteAccess.h
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
#ifndef __REMOTEACCESS_H
#define __REMOTEACCESS_H
#ifdef PRODUCT_WeMo_LEDLight
#include "mxml.h"
#include "sigGen.h"
#include "defines.h"

#define CLOUD_PERM_HANDLING 0

// Must TODO read for enabled TURN_DNS_RESOLVE and CLOUD_DNS_NAME
// Point your router DNS server configuration to 173.196.160.174 for CI and
// 173.196.160.170 for QA; If no DNS setting in router is done then by default
// it will point to production platform
#define TRANSACTION_ID_LEN            30+1
#define TURN_SERVER_IP   "nat.test.jardon.xbcs.net"
#define TURN_LB_IP   "nat.test.jardon.xbcs.net"

#define DLR_URL "https://api.test.jardon.xbcs.net:8443/apis/http/dlr"
#define DLR_URL_WOS "http://api.test.jardon.xbcs.net:8080/apis/http/dlr/"
#define PLUGIN_REGISTRATION_URL "https://api.test.jardon.xbcs.net:8443/apis/http/plugin/"
#define PLUGIN_REGISTRATION_URL_WOS "http://api.test.jardon.xbcs.net:8080/apis/http/plugin/"

#if defined(PRODUCT_WeMo_Baby) || defined(PRODUCT_WeMo_Streaming)
#define POST_WD_UPLOAD_URL "https://api.test.jardon.xbcs.net:6080/WemoTool/toolService/uploadLogFile/"
#define POST_WD_UPLOAD_URL_WOS "http://api.test.jardon.xbcs.net:6080/WemoTool/toolService/uploadLogFile/"
#endif

#define TURN_SERVER_PORT 3478
#define TURN_PUBSERVER_PORT 3475
#define REALM "belkin.org"
#define USE_TCP 1
#define USE_UDP 0

#define AUTH_PORT 49150


#define OP_PLUGIN_DEV_REG 1

#define SMARTDEVXML "/tmp/Belkin_settings/smartDev.txt"

#define REMOTE_STATUS_SEND_RETRY_TIMEOUT    (30*1000000)

#define MAX_STATUS_BUF 2048

#define ICE_PEER_LB_NAME "ice.xbcs.net"    //ice lite peer

typedef struct smartDeviceInfo
{
	unsigned long smartDeviceId;
	char smartDeviceDescription[MAX_DESC_LEN];
	char  smartUniqueId[MAX_PKEY_LEN];
	char privateKey[MAX_SKEY_LEN];
#ifdef WeMo_SMART_SETUP
	char  reunionKey[MAX_PKEY_LEN];
#endif
}smartDeviceInfo;


typedef struct pluginDeviceInfo
{
	unsigned long pluginId;
	char macAddress[MAX_MAC_LEN];
	char routerMacAddress[MAX_MAC_LEN];
	char oldRouterMacAddress[MAX_MAC_LEN];
	unsigned long homeId;
	char serialNumber[MAX_MAC_LEN];
	char pluginUniqueId[MAX_PKEY_LEN];
	char modelCode[SIZE_32B];
	char routerEssid[MAX_ESSID_SPLCHAR_LEN];
	char status[MAX_MIN_LEN];
	char pad1;
	unsigned long pluginDeviceTS;	//status not in http request ??check TODO
	char privateKey[MAX_SKEY_LEN];
	unsigned long lastPluginTimestamp;
	char description[MAX_SKEY_LEN];
}pluginDeviceInfo;

typedef struct pluginRegisterInfo
{
	int  bRemoteAccessEnable;
	int  bRemoteAccessAuthCheck;
	char statusCode[MAX_RES_LEN];
	char resultCode[MAX_DVAL_LEN];
	char description[MAX_DVAL_LEN];
	char seed[MAX_PKEY_LEN];
	char key[MAX_PKEY_LEN];
	char message[MAX_LVALUE_LEN];
	char pad[SIZE_2B];
	pluginDeviceInfo pluginDevInf;
	smartDeviceInfo smartDevInf;
}pluginRegisterInfo;

int remoteAccessServiceHandler(void *relay,
		void *pkt,
		unsigned pkt_len,
		const void* peer_addr,
		unsigned addr_len,
		void *data_sock);

typedef struct pluginDeviceStatus {
	unsigned long pluginId;
	char macAddress[MAX_MAC_LEN];
	char friendlyName[MAX_DESC_LEN];
	unsigned short status;
	int statusTS;
}pluginDeviceStatus;

typedef struct pluginUpgradeFwStatus{
	unsigned long pluginId;
	char macAddress[MAX_MAC_LEN];
	char fwVersion[SIZE_64B];
	int fwUpgradeStatus;
}pluginUpgradeFwStatus;

extern char g_szHomeId[SIZE_20B];
extern char g_szPluginPrivatekey[MAX_PKEY_LEN];
extern char g_szRestoreState[MAX_RES_LEN];
extern unsigned short gpluginRemAccessEnable;
extern char gFirmwareVersion[SIZE_64B];
extern char g_serverEnvIPaddr[SIZE_32B];
extern unsigned int gBackOffInterval;
extern unsigned int gNatMonBypassCnt;

#ifdef WeMo_SMART_SETUP
extern char gszSetupSmartDevUniqueId[];
extern char gszSetupReunionKey[];
extern pluginRegisterInfo *pgSetupRegResp;
#endif

extern int gIceRunning;

#if defined(PRODUCT_WeMo_Insight)
extern int g_EventEnable;
extern int g_SendEvent;
extern int g_EventDuration;
#endif

#if 0
void* remoteAccessInitThdAg(void *args);
#endif

void* remoteAccessInitThd(void *args);
int remotePostDeRegister(void *dereg, int flag);
int isNotificationEnabled (void);
int getPluginStatusTS(void);
int UploadIcon(char *UploadURL,char *fsFileUrl ,char *cookie);
sduauthSign* createSDUAuthSignature(char* udid, char* key);
int PluginAppStartPhAuth(const char* pdeviceId, const char* phomeId, const char* pdeviceName);
int remoteAccessResetNameRulesData(void*hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void *remoteInfo,char *transaction_id);

#if defined(PRODUCT_WeMo_Insight) || defined(PRODUCT_WeMo_SNS) || defined(PRODUCT_WeMo_NetCam)
int remoteAccessEditPNRuleMessage(void *hndl,char *xmlBuf, unsigned int buflen,void *data_sock, void *remoteInfo,char* transaction_id);
#endif

typedef struct smartDevDets {
	char smartId[SIZE_64B];
	char smartKey[MAX_PKEY_LEN];
	char smartName[SIZE_128B];
}smartDevDets;

smartDevDets* readSmartXml(int *count);

#define BL_DOMAIN_NM "api.test.jardon.xbcs.net"
#define BL_DOMAIN_NM_WD "devices.xbcs.net"

#define BL_DOMAIN_PORT_WD   "8899"
#define BL_DOMAIN_URL_WD    "ToolService/log/uploadWdLogFile"
#define BL_DOMAIN_URL_PLUGIN	"ToolService/log/uploadPluginLogFile"

void remoteCheckAttributes();
int remoteTSDomainResolve(char * turn_server_ip);
int remoteLBDomainResolve(char * turn_lb_ip);

void* remoteInitNatClient_info(void *args);
int remoteInitNatClient_thds (void *args);
void* sendRemoteAccessUpdateStatusThread (void *arg);
void *sendRemAccessUpdStatusThdMonitor(void *arg);
int loadSmartDevXml();
int createSmartXmlIfno(void *smartDev);
int checkSmartEntry(void *smartDev);
int updateSmartXml(void *smartDev);
int addSmartXml(void *smartDev);
int numSmartDevInTree(void);
void remoteAccessUpdateStatusTSParams(int status);
void setPluginStatusTS();
int RemoteAccessRegister(void);
int setEventPush (void* hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void *remoteInfo,char *transaction_id);
void* uploadLogFileThread_6Hours(void *arg);
void* uploadLogFileThread(void *arg);
int setNotificationStatus (int state);
void initRemoteUpdSync(void);
void initRemoteStatusTS(void);
int clearMemPartitions(void);
int resetToDefaults(void);
int setRemoteRestoreParam (int rstval);
int remoteAccessUpgradeFwVersion(char *xmlBuf, unsigned int buflen, void *data_sock);
int remoteAccessChangeFriendlyName(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void * remoteInfo,char *transaction_id);

extern char g_routerSsid[MAX_ESSID_LEN];
extern char g_routerMac[MAX_MAC_LEN];
extern char g_szRestoreState[MAX_RES_LEN];

//Remote access enabled or not flag
#define REMOTE_ACCESS_NOT_ENABLED 0
#define REMOTE_ACCESS_ENABLED 1

//Nat initialization status
#define NATCL_NOT_INITIALIZED 0
#define NATCL_INIT_INPROGRESS 2
#define NATCL_INITIALIZED 1
#define NATCL_INIT_TRIGGER	3

//Nat Initialization wait status
#define NATCL_INIT_NOWAIT	2
#define NATCL_INIT_INWAIT	1
#define NATCL_INIT_OUTWAIT	0

//PJNATH turn on state flags
#define NAT_ONSTATE_IDLE 0
#define NAT_ONSTATE_INPROGRESS 1

//NAT Re-init in progreaa states
#define NAT_REINIT_IDLE 0
#define NAT_REINIT_INPROGRESS 1

//NAT Re-init 24 hourly states
#define NAT_REINIT_24HOURLY_IDLE 0
#define NAT_REINIT_24HOURLY_INPROGRESS 1

//NAT Re-init counts
#define MIN_NATREINIT_COUNT 0
#define MAX_NATREINIT_COUNT 3

//Miscllaneous
#define MIN_DLR_RETRY_COUNT 1
#define MAX_DLR_RETRY_COUNT 3

#define MIN_RETRY_COUNT MIN_DLR_RETRY_COUNT
#define MAX_RETRY_COUNT MAX_DLR_RETRY_COUNT
#define MAX_LOG_ACOUNT 200

//Sleep multiples
#define NATCL_SLEEP_ONE 1
#define MIN_NATCL_SLEEP 2
#define NATCL_CUSTOM_METHOD_ID	10
#define MIN_NATCL_RNDSLEEP 30
#define MIN_NATCL_INITSLEEP 140
#define MAX_NATCL_BACKOFF 60
#define MAX_NATCL_RELAYCHK_CNT 10

//NAT REINIT Configuration
/* Upper limit for randomizing periodic reinit interval (in seconds) */
#define CONFIG_NAT_REINIT_INTERVAL	(6*60)
/* Upper limit for back-off algorithm (in minutes) */
#define CONFIG_NAT_REINIT_TIMEOUT	30

//Auth Retries variables
#define MIN_NATAUTH_SLEEP MIN_NATCL_SLEEP*15
#define AUTH_1ST_ITER 5
#define SUBS_AUTH_INTERVAL 30
#define MAX_AUTH_RETRY	12

#define NATCL_DISCON_SLEEP_INTERVAL	60

#define NATCL_TURE	1
#define NATCL_FALSE	0

#define NATCL_HEALTH_NOTGOOD	-1
#define NATCL_HEALTH_PARTIALGOOD	-2
#define NATCL_HEALTH_GOOD	0

#ifndef __ALLOC_NO_UNAUTH__
#define REMOTE_ALLOC_AUTH_ERROR 111
#endif

/* BUGSENSE Related */
#define BUGSENSE_API_KEY "581d8de1"
#define BUGSENSE_UID_LEN	40
#define BUGSENSE_MAX_EVENT_LEN	200

/* XML Special Characters Related */
#define XML_SPL_CHARACTER_1			'&'
#define XML_SPL_CHARACTER_2			'<'
#define XML_SPL_CHARACTER_3			'>'
#define XML_SPL_CHARACTER_4			'\"'
#define XML_SPL_CHARACTER_5			'\''

#define XML_SPL_CHARACTER_RPL_1		"&#38;" 	//or &amp;
#define XML_SPL_CHARACTER_RPL_2		"&#60;"  	//or &lt;
#define XML_SPL_CHARACTER_RPL_3		"&#62;"  	//or &gt;
#define XML_SPL_CHARACTER_RPL_4		"&#34;" 	// or &quot;
#define XML_SPL_CHARACTER_RPL_5		"&#39;"		// or &apos;

void core_init_late(int forceEnableCtrlPoint);
void core_init_early(void);

void* remoteRegThread(void *args);
int monitorNATCLStatus(void*);
void invokeNatReInit(int flag);
char *getTSPublicIP(char *turnserver_ip);
char *getTSPublicIP_N(char *turnlb_ip);
char *getTSUdpPublicIP(char *turnlb_ip);
int resetFlashRemoteInfo(pluginRegisterInfo *pPlgnRegInf);
void resetRestoreParam();
void* sendConfigChangeStatusThread(void *arg);
void* rcvSendstatusRspThread(void *arg);
void CheckUpdateRemoteFailureCount(void);
void reRegisterDevice(void);
void DespatchSendNotificationResp(void *sendNfResp);
void initCommandQueueLock();
#ifdef __NFTOCLOUD__
int  sendNATStatus (int natStatus);
#endif
void invokeNatDestroy();
extern void initdevConfiglock(void);
void enqueueBugsense(char *s);
void resetSystem(void);
int uploadLogFileDevStatus(const char *filename, const char *uploadURL);
void trigger_nat(void);
int remoteSendEncryptPkt(int dsock, void *pkt, unsigned pkt_len);
void sendHomeIdListToCloud(char *homeidlist, char *signaturelist);
int initNATCore(int flag);

#define ICE_GETNODE_MAX_COUNT 5
#define MAX_MSG_CNT   4
#define ICE_PEER_LB_NAME "ice.xbcs.net"    //ice lite peer name for getNode
#define MAX_IP_LEN                    16
#define REMOTE_PEER_MAC   "PL17Aug4002" //To be removed
#define PLG_SIGNATURE_EXP 3600
#define PLG_PEERIP "12.2.2.2"
#define PLG_PEER_PORT 23432

#define CHECK_PREV_TSX_ID(index,transaction_id,retVal) {\
    if((gIceRunning)&&(!strncmp(gIcePrevTransactionId[index],transaction_id,TRANSACTION_ID_LEN))) \
    { \
		retVal=PLUGIN_FAILURE; \
	APP_LOG("REMOTEACCESS", LOG_DEBUG, "discarding transaction_id ===%s", transaction_id); \
	goto handler_exit1;\
    }\
}

#define NAT_REINIT 0
#define NAT_FORCE_REINIT 1

int SendNatPkt(void *hndl,char* statusResp,void* remoteInfo,char*transaction_id,void*data_sock,int index);
int SendIcePkt(void *hndl,char* statusResp,void *remoteInfo,char *transaction_id);
int SendTurnPkt(int dsock,char * statusResp);
int remoteAccessXmlHandler(void *hndl, void *pkt, unsigned pkt_len,void* remoteInfo, char* transaction_id,void*data_sock);
int remoteAccessInitNatICE();
int remoteAccessInitNatTURN();
int remoteAccessInitIce();


int DownloadIcon(char *url,char* cookie);
int GetDevStatusHandle(void *hndl, char *xmlBuf, unsigned int buflen, void* data_sock , void* remoteInfo,char* transaction_id);
int SetDevStatusHandle(void *hndl, char *xmlBuf, unsigned int buflen, void *data_sock, void* remoteInfo,char* transaction_id);
int UpgradeFwVersion(void *hndl, char *xmlBuf, unsigned int buflen,void *data_sock, void* remoteInfo,char* transaction_id );
int GetRulesHandle(void *hndl, char *xmlBuf, unsigned int buflen,void*data_sock,  void* remoteInfo,char* transaction_id);
int GetLogHandle(void *hndl, char *xmlBuf, unsigned int buflen,void *data_sock,  void* remoteInfo,char* transaction_id);
int SetRulesHandle(void *hndl, char *xmlBuf, unsigned int buflen, void*data_sock, void* remoteInfo,char* transaction_id);
int RemoteSetRulesDBVersion(void *hndl, char *xmlBuf, unsigned int buflen, void*data_sock, void* remoteInfo,char* transaction_id);
int RemoteGetRulesDBVersion(void *hndl, char *xmlBuf, unsigned int buflen, void *data_sock, void* remoteInfo,char* transaction_id);
int sendDbFileHandle(void *hndl, char *xmlBuf, unsigned int buflen, void *data_sock, void* remoteInfo,char* transaction_id);
int getDbFileHandle(void *hndl, char *xmlBuf, unsigned int buflen, void *data_sock, void* remoteInfo,char* transaction_id);
int RemoteAccessDevConfig(void *hndl, char *xmlBuf, unsigned int buflen,void *data_sock, void* remoteInfo,char* transaction_id );
int remoteAccessSetIcon(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void*remoteInfo,char* transaction_id);
int remoteAccessGetIcon(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void *remoteInfo,char* transaction_id);
int remoteGetDeRegStatus(int flag);
int resetFNGlobalsToDefaults(int type);
#ifdef WeMo_SMART_SETUP_V2
void sendCustomizedStateNotification(void);
#endif
#endif //#ifdef PRODUCT_WeMo_LEDLight
#endif
