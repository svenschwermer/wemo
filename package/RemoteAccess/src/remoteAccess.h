/***************************************************************************
*
*
* remoteAccess.h
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

#ifndef __REMOTEACCESS_H
#define __REMOTEACCESS_H

#include "mxml.h"
#include "sigGen.h"
#include "defines.h"
#include "UDSClientHandler.h"

#define CLOUD_PERM_HANDLING 0
#define DLR_COMMUNICATION 1

#define DLR_URL "https://api.xbcs.net:8443/apis/http/dlr"
#define DLR_URL_WOS "http://api.xbcs.net:8080/apis/http/dlr/"
#define PLUGIN_REGISTRATION_URL "https://api.xbcs.net:8443/apis/http/plugin/"
#define PLUGIN_REGISTRATION_URL_WOS "http://api.xbcs.net:8080/apis/http/plugin/"

#if defined(PRODUCT_WeMo_Baby) || defined(PRODUCT_WeMo_Streaming)
#define POST_WD_UPLOAD_URL "https://api.xbcs.net:6080/WemoTool/toolService/uploadLogFile/"
#define POST_WD_UPLOAD_URL_WOS "http://api.xbcs.net:6080/WemoTool/toolService/uploadLogFile/"
#endif

#define OP_PLUGIN_DEV_REG 1

#define SMARTDEVXML "/tmp/Belkin_settings/smartDev.txt"

#define REMOTE_STATUS_SEND_RETRY_TIMEOUT    (30*1000000)

#define MAX_STATUS_BUF 2048

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
extern unsigned int gBackOffInterval;
extern unsigned int gNatMonBypassCnt;

#ifdef WeMo_SMART_SETUP
extern char gszSetupSmartDevUniqueId[];
extern char gszSetupReunionKey[];
extern pluginRegisterInfo *pgSetupRegResp;
#endif
#if defined(PRODUCT_WeMo_Insight)
extern unsigned int g_EventEnable;
extern int g_SendEvent;
extern unsigned int g_EventDuration;
#endif

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

#define BL_DOMAIN_NM_WD "devices.xbcs.net"
#define BL_DOMAIN_PORT_WD   "8899"
#define BL_DOMAIN_URL_WD    "ToolService/log/uploadWdLogFile"
#define BL_DOMAIN_URL_PLUGIN	"ToolService/log/uploadPluginLogFile"
#define BL_DOMAIN_URL_DB	"ToolService/tool/uploadRuleFile"

extern pthread_t logFile_thread;

void remoteCheckAttributes();
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
int uploadMessageFile(char *file);
void* uploadLogFileThread(void *arg);
void* uploadDbFileThread(void *arg);
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
extern int gDataInProgress;

//Remote access enabled or not flag
#define REMOTE_ACCESS_NOT_ENABLED 0
#define REMOTE_ACCESS_ENABLED 1

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
void core_init_threads(int flag);

void* remoteRegThread(void *args);
int monitorNATCLStatus(void*);
int resetFlashRemoteInfo(pluginRegisterInfo *pPlgnRegInf);
void resetRestoreParam();
void* sendConfigChangeStatusThread(void *arg);
void* rcvSendstatusRspThread(void *arg);
void reRegisterDevice(void);
void DespatchSendNotificationResp(void *sendNfResp);
void initCommandQueueLock();
#ifdef __NFTOCLOUD__
int  sendNATStatus (int natStatus);
#endif
extern void initdevConfiglock(void);
void enqueueBugsense(char *s);
void resetSystem(void);
int uploadLogFileDevStatus(const char *filename, const char *uploadURL);
int remoteSendEncryptPkt(int dsock, void *pkt, unsigned pkt_len);
void sendHomeIdListToCloud(char *homeidlist, char *signaturelist);

enum E_PLUGIN_REQUSET{
		E_PLUGINDEVICESTATUS,
		E_PLUGINSETDEVICESTATUS,
		E_PLUGINGETDBFILE,
		E_WDLOGFILE,
		E_UPDATEWEEKLYCALENDAR,
		E_SETDBVERSION,
		E_GETDBVERSION,
		E_PLUGINSETNOTIFICATIONTYPE,
		E_SENDPLUGINDBFILE,
		E_GETPLUGINDBFILE,
		E_UPGRADEFWVERSION,
		E_DEVCONFIG,
		E_EDITPNRULEMESSAGE,
		E_SETICON,
		E_GETICON,
		E_RESETNAMERULESDATA,
		E_CHANGEFRIENDLYNAME,
#if defined (PRODUCT_WeMo_Insight)
		E_GETPLUGINDETAILS,
		E_SETPOWERTHRESHOLD,
		E_GETPOWERTHRESHOLD,
		E_SETCLEARDATAUSAGE,
		E_GETDATAEXPORTINFO,
		E_SCHEDULEDATAEXPORT,
		E_SETINSIGHTHOMESETTINGS,
#endif
#ifdef SIMULATED_OCCUPANCY
		E_SETSIMULATEDRULEDATA,
#endif
#ifdef PRODUCT_WeMo_Light
		E_SETNIGHTLIGHTSTATUSVALUE,
		E_GETNIGHTLIGHTSTATUSVALUE,
#endif
		E_PLUGINERRORRSP,	
#ifdef WeMo_SMART_SETUP
		E_RESETCUSTOMIZEDSTATE,
#endif
#if defined (PRODUCT_WeMo_LEDLight)
		E_SETGROUPDETAILS,
		E_CREATEGROUP,
		E_DELETEGROUP,
		E_SETDATASTORE,
		E_GETDATASTORE,
		E_SETDEVICEPRESET,
		E_GETDEVICEPRESET,
		E_DELETEDEVICEPRESET,
		E_DELETEDEVICEALLPRESET,
#endif
		PLUGIN_MAX_REQUESTS
};

#define CHECK_PREV_TSX_ID(index,transaction_id,retVal) {\
    if(gIceRunning && (!strncmp(gIcePrevTransactionId[index],transaction_id,TRANSACTION_ID_LEN))) \
    { \
                retVal=PLUGIN_FAILURE; \
        APP_LOG("REMOTEACCESS", LOG_DEBUG, "discarding transaction_id ===%s", transaction_id); \
        goto handler_exit1;\
    }\
}

int SendNatPkt(void *hndl,char* statusResp,void* remoteInfo,char*transaction_id,void*data_sock,int index);
int SendIcePkt(void *hndl,char* statusResp,void *remoteInfo,char *transaction_id);
int SendTurnPkt(int dsock,char * statusResp);
int remoteAccessXmlHandler(void *hndl, void *pkt, unsigned pkt_len,void* remoteInfo, char* transaction_id,void*data_sock);

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
void restoreRelayState();
void* parseSendNotificationResp(void *sendNfResp, char **errcode);
#ifdef WeMo_SMART_SETUP_V2
void sendCustomizedStateNotification(void);
#endif

#endif
