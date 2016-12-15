/***************************************************************************
 *
 *
 * controlleedevice.c
 *
 * Created by Belkin International, Software Engineering on May 27, 2011
 * Copyright (c) 2012-2014 Belkin International, Inc. and/or its affiliates. All rights reserved.
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
#include <ithread.h>
#include <upnp.h>
#include <sys/time.h>
#include <math.h>
#include "global.h"
#include "defines.h"
#include "fw_rev.h"
#include "logger.h"
#include "wifiHndlr.h"
#include "controlledevice.h"
#include "gpio.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "new_upgrade.h"
#include "itc.h"
#include "natClient.h"
#include "remoteAccess.h"
#include "pktStructs.h"
#include "rule.h"
#include "plugin_ctrlpoint.h"
#include "utils.h"
#include "utlist.h"
#include "mxml.h"
#include "sigGen.h"
#include "watchDog.h"
#include "upnpCommon.h"
#include "osUtils.h"
#include "gpioApi.h"
#include "httpsWrapper.h"
#ifdef PRODUCT_WeMo_Insight
#include "insight.h"
#endif
#ifdef PRODUCT_WeMo_LEDLight
#include "ledLightUPnPHandler.h"
#include "bridge_away_rule.h"
#include "bridge_sensor_rule.h"
#endif
#include "getRemoteMac.h"

#ifdef WeMo_SMART_SETUP
#include "smartSetupUPnPHandler.h"
#endif

#ifdef SIMULATED_OCCUPANCY
#include "simulatedOccupancy.h"
#endif
#if defined(PRODUCT_WeMo_Jarden) || defined(PRODUCT_WeMo_Smart)
#include "wasp_api.h"
#include "wasp.h"
#include "WemoDB.h"
#endif

#include "thready_utils.h"
#include "rulesdb_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

/* serial number length*/
#define MAX_SERIAL_LEN  14
#define SERIAL_TYPE_INDEX 6 //- The seventh digital indicating device type
#define SUB_DEVICE_TYPE_INDEX 8 //- The ninth digital indicating sub device type
//[WEMO-26944]
#define NUM_SECONDS_IN_HOUR 60*60

#ifdef PRODUCT_WeMo_CrockPot
#include "crockpot.h"
#endif

extern int webAppFileDownload(char *url, char *outfilename);
extern void StopDownloadRequest(void);
#if _OPENWRT_
//extern int pj_dst_data_os(int, char *, int);
#endif
extern void nat_trav_destroy(void *);
extern void initBugsense(void);

#ifdef PRODUCT_WeMo_Insight
extern void Update30MinDataOnFlash();
#endif
int g_isRemoteAccessByApp = 0x00;
int g_OldApp = 0x00;
pthread_mutex_t g_remoteAccess_mutex;
pthread_cond_t g_remoteAccess_cond;
pthread_t remoteRegthread=-1;
pthread_attr_t remoteReg_attr;
pthread_t timesyncThread=-1;

pthread_attr_t timesync_attr;
ProxyRemoteAccessInfo *g_pxRemRegInf = NULL;
extern pluginRegisterInfo *pgPluginReRegInf;

pthread_attr_t updateFw_attr;
pthread_t fwUpMonitorthread=-1;
pthread_attr_t firmwareUp_attr;
pthread_t firmwareUpThread=-1;
pthread_attr_t wdLog_attr;
extern pthread_t logFile_thread;
extern pthread_t dbFile_thread;
int currFWUpdateState=0;
extern int gTimeZoneUpdated;

//Increase this time Download time to 10 minutes for now until new strategy/design is created.
#define MAX_FW_DL_TIME_OUT  10*60
char* ip_address 	= NULL;
char* desc_doc_name 	= NULL;
char* web_dir_path 	= NULL;

int g_isTimeSyncByMobileApp = 0x00;
int gWebIconVersion=0;
extern int ghwVersion;
extern int gLastAuthVal;

int   g_lastDstStatus = 0x00;

char  g_server_ip[SIZE_32B];
unsigned short g_server_port;
char gUserKey[PASSWORD_MAX_LEN];

extern char g_szApSSID[MAX_APSSID_LEN];
extern char g_routerMac[MAX_MAC_LEN];
extern char g_routerSsid[MAX_ESSID_LEN];
extern int gNTPTimeSet;
extern char g_szRestoreState[SIZE_2B];
extern int ctrlpt_handle;
extern int gBootReconnect;
extern int gStopDownloadFW;
extern int gSignalStrength;
extern int gIceRunning;

unsigned int szDeviceID;
extern unsigned long int g_poweronStartTime;

char g_szDeviceUDN[MAX_DEVICE_UDN_SIZE];

static char* DEFAULT_SERIAL_NO = "0123456789";
static int   DEFAULT_SERIAL_TAILER_SIZE = 3;
static pthread_attr_t reset_attr;
static pthread_t reset_thread = -1;

int g_eDeviceType = DEVICE_UNKNOWN;	//- Device type indentifier
int g_eDeviceTypeTemp = DEVICE_UNKNOWN;	//- Device type indentifier
int g_ra0DownFlag = 0;
#if defined(PRODUCT_WeMo_Insight)
char g_SendInsightParams =0;
int g_isDSTApplied = 0;
unsigned int g_StateLog = 0;
#endif
int gSetupRequested = 0;
int gAppCalledCloseAp = 0;

char g_szWiFiMacAddress[SIZE_64B];
char g_szFriendlyName[SIZE_256B];

extern char g_szSerialNo[SIZE_64B];
extern char g_szProdVarType[SIZE_16B];

char g_szUDN[SIZE_128B];
char g_szUDN_1[SIZE_128B];
char g_szFirmwareVersion[SIZE_64B];
char g_szSkuNo[SIZE_64B];


char g_szFirmwareURLSignature[MAX_FW_URL_SIGN_LEN];

static  ithread_t ithPowerSensorMonitorTask = -1;
volatile static  int sPowerDuration 	= 0x00;
volatile static  int sPowerEndAction = -1;
extern int g_IsLastUserActionOn;


char* g_szBuiltFirmwareVersion = 0x00;
char* g_szBuiltTime = 0x00;
pthread_attr_t dst_main_attr;
pthread_t dstmainthread = -1;
int gDstEnable=0;
extern int gDstSupported;


int g_iDstNowTimeStatus	= 0x00;
int gpluginStatusTS = 0;
pthread_mutex_t gFWUpdateStateLock;
pthread_mutex_t gSiteSurveyStateLock;

char g_szBootArgs[SIZE_128B];

extern char g_serverEnvIPaddr[SIZE_32B];
extern char g_turnServerEnvIPaddr[SIZE_32B];
extern SERVERENV g_ServerEnvType;

#ifdef WeMo_SMART_SETUP
extern int gSmartSetup;
#endif

#ifdef PRODUCT_WeMo_Smart
char g_DevName[SIZE_128B];
char g_brandName[SIZE_128B];
extern WaspVarDesc **g_attrVarDescArray;
extern int g_attrCount;
extern sqlite3 *NotificationDB;
extern int g_NotifSerialNumber;
#endif

char g_szActuation[SIZE_128B];
char g_szClientType[SIZE_128B];
char g_szRemote[SIZE_8B];

#define	    CONTROLLEE_DEVICE_STOP_WAIT_TIME	   5*1000000
volatile int gRestartRuleEngine=0;

//--------------- Global Definition ------------ //- WiFi setup callback list
PluginDeviceUpnpAction g_Wifi_Setup_Actions[] = {
		{"GetApList", GetApList},
		{"GetNetworkList", GetNetworkList},
		{"ConnectHomeNetwork", ConnectHomeNetwork},
		{"GetNetworkStatus", GetNetworkStatus},
		{"SetSensorEvent", SetBinaryState},
		{"TimeSync", SyncTime},
		{"CloseSetup", CloseSetup},
		{"StopPair", StopPair},
};

PluginDeviceUpnpAction g_time_sync_Actions[] = {
		{"TimeSync", SyncTime},
		{"GetTime", 0x00}
};

//- Basic event callback list
PluginDeviceUpnpAction g_basic_event_Actions[] = {
#ifdef PRODUCT_WeMo_Insight
		{"SetInsightHomeSettings", SetInsightHomeSettings},
		{"GetInsightHomeSettings", GetInsightHomeSettings},
		{"UpdateInsightHomeSettings", UpdateInsightHomeSettings},
#endif
#ifdef PRODUCT_WeMo_LEDLight
	{"SetBinaryState", BS_SetBinaryState},
#else
	{"SetBinaryState", SetBinaryState},
#endif
	{"SetMultiState", 0x00},
	{"GetBinaryState", GetBinaryState},
#ifdef PRODUCT_WeMo_Jarden
    {"SetCrockpotState", SetJardenStatus}, /* Crockpot specific command */
    {"GetCrockpotState", GetJardenStatus}, /* Crockpot specific command */
    {"SetJardenStatus", SetJardenStatus}, /* Jarden Device specific command */
    {"GetJardenStatus", GetJardenStatus}, /* Jarden Device specific command */
#endif
		{"GetFriendlyName", GetFriendlyName},
		{"ChangeFriendlyName", SetFriendlyName},
		{"GetHomeId", GetHomeId},
		{"GetHomeInfo", GetHomeInfo},
		{"SetHomeId", SetHomeId},
		{"GetDeviceId", GetDeviceId},
		{"GetMacAddr", GetMacAddr},
		{"GetSerialNo", GetSerialNo},
		{"GetPluginUDN", GetPluginUDN},
		{"GetSmartDevInfo", GetSmartDevInfo},
		{"SetSmartDevInfo", SetSmartDevInfo},
		{"ShareHWInfo", GetShareHWInfo},
		{"SetDeviceId", SetDeviceId},
		{"GetIconURL", GetIcon},
		{"ReSetup", ReSetup},
		{"SetLogLevelOption", SetLogLevelOption},
		{"GetLogFileURL", GetLogFilePath},
		{"GetWatchdogFile", GetWatchdogFile},
		{"GetSignalStrength", SignalStrengthGet},
		{"SetServerEnvironment", SetServerEnvironment},
		{"GetServerEnvironment", GetServerEnvironment},
		{"GetIconVersion", GetIconVersion},
#ifdef PRODUCT_WeMo_Light
		{"SetNightLightStatus", SetNightLightStatus},
		{"GetNightLightStatus", GetNightLightStatus},
#endif
#ifdef SIMULATED_OCCUPANCY
		{"GetSimulatedRuleData", GetSimulatedRuleData},
		{"NotifyManualToggle", NotifyManualToggle},
#endif
		{"ControlCloudUpload",   ControlCloudUpload},
                {"ControlCloudDBUpload", ControlCloudDBUpload},

};

#ifdef PRODUCT_WeMo_Smart
PluginDeviceUpnpAction g_device_event_Actions[] = {
	{"SetAttribute", SetAttribute},
	{"GetAttribute", GetAttribute},
};
#endif

PluginDeviceUpnpAction g_Rules_Actions[] = {
#ifdef PRODUCT_WeMo_LEDLight
		{"SimulatedRuleData",    BA_SimulatedRuleData},
#endif
		{"UpdateWeeklyCalendar", UpdateWeeklyCalendar},
		{"EditWeeklycalendar", EditWeeklycalendar},

		{"GetRulesDBPath", GetRulesDBPath},
		{"SetRulesDBVersion", SetRulesDBVersion},
		{"GetRulesDBVersion", GetRulesDBVersion},
		{"FetchRules", FetchRules},
		{"StoreRules", StoreRules},
#if defined(PRODUCT_WeMo_Insight) || defined(PRODUCT_WeMo_SNS) || defined(PRODUCT_WeMo_NetCam) || defined(PRODUCT_WeMo_LEDLight)
		{"SetRuleID", SetRuleID},
		{"DeleteRuleID", DeleteRuleID},
#endif
#ifdef SIMULATED_OCCUPANCY
		{"SimulatedRuleData", SimulatedRuleData},
#endif
};



//- Firmware update callback list

PluginDeviceUpnpAction g_firmware_event_Actions[] = {
		{"UpdateFirmware", UpdateFirmware},
		{"GetFirmwareVersion", GetFirmwareVersion},
};

// - Remote access callback
PluginDeviceUpnpAction g_remote_access_Actions[] = {
		{"RemoteAccess", RemoteAccess},
};

PluginDeviceUpnpAction g_metaInfo_Actions[] = {
		{"GetMetaInfo", GetMetaInfo},
		{"GetExtMetaInfo", GetExtMetaInfo},
};

PluginDeviceUpnpAction g_deviceInfo_Actions[] = {
		{"GetDeviceInformation", GetDeviceInformation},
		{"GetInformation", GetInformation},
#ifdef WeMo_INSTACONNECT
		{"OpenInstaAP", OpenInstaAP},
		{"CloseInstaAP", CloseInstaAP},
		{"GetConfigureState", GetConfigureState},
		{"InstaConnectHomeNetwork", InstaConnectHomeNetwork},
		{"UpdateBridgeList", UpdateBridgeList},
		{"InstaRemoteAccess", InstaRemoteAccess},
		{"GetRouterInformation", GetRouterInformation}
#endif
};

#ifdef PRODUCT_WeMo_Insight
PluginDeviceUpnpAction g_insight_Actions[] = {
		{"GetInsightParams",GetInsightParams},
		{"GetPowerThreshold",GetPowerThreshold},
		{"SetPowerThreshold",SetPowerThreshold},
		{"SetAutoPowerThreshold",SetAutoPowerThreshold},
		{"ResetPowerThreshold",ResetPowerThreshold},
		{"ScheduleDataExport", ScheduleDataExport},
		{"GetDataExportInfo", GetDataExportInfo},
};
#endif

#ifdef PRODUCT_WeMo_LEDLight
PluginDeviceUpnpAction g_bridge_Actions[] = {
		{"OpenNetwork", OpenNetwork},
		{"CloseNetwork", CloseNetwork},
		{"GetEndDevices", GetEndDevices},
		{"AddDevice", AddDevice},
		{"AddDeviceName", AddDeviceName},
		{"RemoveDevice", RemoveDevice},
		{"SetDeviceStatus", SetDeviceStatus},
		{"GetDeviceStatus", GetDeviceStatus},
		{"GetCapabilityProfileIDList", GetCapabilityProfileIDList},
		{"GetCapabilityProfileList", GetCapabilityProfileList},
		{"GetDeviceCapabilities", GetDeviceCapabilities},
		{"CreateGroup", CreateGroup},
		{"DeleteGroup", DeleteGroup},
		{"GetGroups", GetGroups},
		{"SetDeviceName", SetDeviceName},
		{"SetDeviceNames", SetDeviceNames},
		{"IdentifyDevice", IdentifyDevice},
		{"QAControl", QAControl},
		{"UpgradeSubDeviceFirmware", UpgradeSubDeviceFirmware},
		{"SetZigbeeMode", SetZigbeeMode},
		{"ScanZigbeeJoin", ScanZigbeeJoin},
		{"SetDataStores", SetDataStores},
		{"GetDataStores", GetDataStores},
		{"DeleteDataStores", DeleteDataStores},
		{"CloseZigbeeRouterSetup", CloseZigbeeRouterSetup},
		{"setDevicePreset", setDevicePreset},
		{"getDevicePreset", getDevicePreset},
		{"deleteDevicePreset", deleteDevicePreset},
		{"deleteDeviceAllPreset", deleteDeviceAllPreset}
};
#endif

#ifdef WeMo_SMART_SETUP
PluginDeviceUpnpAction g_smart_setup_Actions[] = {
		{"PairAndRegister",PairAndRegister},
		{"GetRegistrationData",GetRegistrationData},
		{"GetRegistrationStatus",GetRegistrationStatus},
#ifdef WeMo_SMART_SETUP_V2
		{"NewPairAndRegister",PairAndRegister},
		{"NewGetRegistrationData",GetRegistrationData},
		{"SetCustomizedState",SetCustomizedState},
		{"GetCustomizedState",GetCustomizedState},
#endif
};
#endif

PluginDeviceUpnpAction g_manufacture_Actions[] = {
	{"GetManufactureData", GetManufactureData},
};



char *CtrleeDeviceServiceType[] = {"urn:Belkin:service:WiFiSetup:1",
	"urn:Belkin:service:timesync:1",
	"urn:Belkin:service:basicevent:1",
#ifdef PRODUCT_WeMo_Smart
    "urn:Belkin:service:deviceevent:1",
#endif
	"urn:Belkin:service:firmwareupdate:1",
	"urn:Belkin:service:rules:1",
	"urn:Belkin:service:metainfo:1",
#ifdef PRODUCT_WeMo_Insight
		"urn:Belkin:service:remoteaccess:1",
		"urn:Belkin:service:insight:1",
#else
		"urn:Belkin:service:remoteaccess:1",
#endif
		"urn:Belkin:service:bridge:1",
		"urn:Belkin:service:deviceinfo:1",
#ifdef WeMo_SMART_SETUP
		"urn:Belkin:service:smartsetup:1",
#endif
		"urn:Belkin:service:manufacture:1"

};

char* szServiceTypeInfo[] = {"PLUGIN_E_SETUP_SERVICE",
	"PLUGIN_E_TIME_SYNC_SERVICE",
	"PLUGIN_E_EVENT_SERVICE",
#ifdef PRODUCT_WeMo_Smart
    "PLUGIN_E_DEVICE_EVENT_SERVICE",
#endif
	"PLUGIN_E_FIRMWARE_SERVICE",
	"PLUGIN_E_RULES_SERVICE",
	"PLUGIN_E_METAINFO_SERVICE",
#ifdef PRODUCT_WeMo_Insight
		"PLUGIN_E_REMOTE_ACCESS_SERVICE",
		"PLUGIN_E_INSIGHT_SERVICE",
#else
		"PLUGIN_E_REMOTE_ACCESS_SERVICE",
#endif
		"PLUGIN_E_BRIDGE_SERVICE",
		"PLUGIN_E_DEVICEINFO_SERVICE",
#ifdef WeMo_SMART_SETUP
		"PLUGIN_E_SMART_SETUP_SERVICE",
#endif
		"PLUGIN_E_MANUFACTURE_SERVICE"

};

PluginDevice SocketDevice = {-1, PLUGIN_MAX_SERVICES};


char* s_szNtpServer = "192.43.244.18";	//in Default, use North America
#define DEFAULT_REGION_INDEX 5
#ifndef PRODUCT_WeMo_NetCam
tTimeZone g_tTimeZoneList[] =
{
		{-12.0, 	1, "(GMT-12:00) Enewetak, Kwajalein"},
		{-11.0, 	2, "(GMT-11:00) Midway Island, Samoa"},
		{-10.0, 	3, "(GMT-10:00) Hawaii"},
		{-9.5, 		4, "GMT-09:30) Marquesas Islands"},
		{-9.0, 		5, "(GMT-09:00) Alaska"},
		{-8.0, 		6, "(GMT-08:00) Pacific Time (US & Canada); Tijuana"},
		{-7.0, 		8, "(GMT-07:00) Mountain Time (US & Canada)"},
		{-6.0, 		9, "(GMT-06:00) Central Time (US & Canada)"},
		{-5.0, 		13, "(GMT-05:00) Eastern Time (US & Canada)"},
		{-4.5, 		15, "(GMT-04:30) Venezuela, Caracas"},
		{-4.0, 		16, "(GMT-04:00) Atlantic Time (Canada)"},
		{-3.5, 		19, "(GMT-03:30) Newfoundland"},
		{-3.0, 		20, "(GMT-03:00) Brasilia"},
		{-2.5, 		21, "(GMT-02:30) St. John's, Canada"},
		{-2.0, 		22, "(GMT-02:00) Mid-Atlantic"},
		{-1.0, 		23, "(GMT-01:00) Azores"},
		{0.0, 		26, "(GMT) Greenwich Mean Time: Lisbon, London,Dublin, Edinburgh"},
		{1.0, 		31, "(GMT+01:00) Paris, Sarajevo, Skopje"},
		{2.0, 		35, "(GMT+02:00) Cairo"},
		{3.0, 		40, "(GMT+03:00) Moscow, St. Petersburg,Volgograd, Kazan"},
		{3.5, 		42, "(GMT+03:30) Iran"},
		{4.0, 		43, "(GMT+04:00) Abu Dhabi, Muscat, Tbilisi"},
		{4.5, 		44, "(GMT+04:30) Kabul, Afghanistan"},
		{5.0, 		46, "(GMT+05:00) Islamabad, Karachi"},
		{5.5, 		47, "(GMT+05:30) India, Sri Lanka"},
		{5.75, 		48, "(GMT+05:45) Nepal"},
		{6.0, 		49, "(GMT+06:00) Almaty, Dhaka"},
		{6.5, 		50, "((GMT+06:30) Cocos Islands, Myanmar"},
		{7.0, 		51, "(GMT+07:00) Bangkok, Jakarta, Hanoi"},
		{8.0, 		52, "(GMT+08:00) Beijing, Chongqing, Urumqi, Hong Kong, Perth, Singapore, Taipei"},
		{9.0, 		54, "(GMT+09:00) Toyko, Osaka, Sapporo"},
		{9.5, 		55, "(GMT+09:30) Northern Territory, South Australia"},
		{10.0, 		56, "(GMT+10:00) Brisbane"},
		{10.5, 		60, "(GMT+10:30) Lord Howe Island"},
		{11.0, 		62, "(GMT+11:00) Magada"},
		{11.5, 		63, "(GMT+11:30) Norfolk Island"},
		{12.0, 		64, "(GMT+12:00) Fiji, Kamchatka, Marshall Is."},
		{12.75, 	66, "(GMT+12:45) Chatham Islands"},
		{13.0, 		67, "(GMT+13:00) Tonga"},
		{14.0, 		68, "(GMT+14:00) Line Islands"}
};
#else
tTimeZone g_tTimeZoneList[] =
{
		{-12.0, 	1, "(GMT-12:00) Enewetak, Kwajalein", "Pacific/Kwajalein"},
		{-11.0, 	2, "(GMT-11:00) Midway Island, Samoa", "Pacific/Midway"},
		{-10.0, 	3, "(GMT-10:00) Hawaii", "US/Hawaii"},
		{-9.5, 		4, "GMT-09:30) Marquesas Islands", "Pacific/Marquesas"},
		{-9.0, 		5, "(GMT-09:00) Alaska", "US/Alaska"},
		{-8.0, 		6, "(GMT-08:00) Pacific Time (US & Canada); Tijuana", "US/Pacific"},
		{-7.0, 		8, "(GMT-07:00) Mountain Time (US & Canada)", "US/Mountain"},
		{-6.0, 		9, "(GMT-06:00) Central Time (US & Canada)", "US/Central"},
		{-5.0, 		13, "(GMT-05:00) Eastern Time (US & Canada)", "US/Eastern"},
		{-4.5, 		15, "(GMT-04:30) Venezuela, Caracas", "America/Caracas"},
		{-4.0, 		16, "(GMT-04:00) Atlantic Time (Canada)", "Canada/Atlantic"},
		{-3.5, 		19, "(GMT-03:30) Newfoundland", "Canada/Newfoundland"},
		{-3.0, 		20, "(GMT-03:00) Brasilia", "Brazil/Brasilia"},
		{-2.0, 		22, "(GMT-02:00) Mid-Atlantic", "Atlantic/South_Georgia"},
		{-1.0, 		23, "(GMT-01:00) Azores", "Atlantic/Azores"},
		{0.0, 		26, "(GMT) Greenwich Mean Time: Lisbon, London,Dublin, Edinburgh", "Europe/London"},
		{1.0, 		31, "(GMT+01:00) Paris, Sarajevo, Skopje", "Europe/Paris"},
		{2.0, 		35, "(GMT+02:00) Cairo", "Africa/Cairo"},
		{3.0, 		40, "(GMT+03:00) Moscow, St. Petersburg,Volgograd, Kazan", "Europe/Moscow"},
		{3.5, 		42, "(GMT+03:30) Iran", "Asia/Tehran", "Asia/Tehran"},
		{4.0, 		43, "(GMT+04:00) Abu Dhabi, Muscat, Tbilisi", "Asia/Muscat"},
		{4.5, 		44, "(GMT+04:30) Kabul, Afghanistan", "Asia/Kabul"},
		{5.0, 		46, "(GMT+05:00) Islamabad, Karachi", "Asia/Karachi"},
		{5.5, 		47, "(GMT+05:30) India, Sri Lanka", "Asia/Calcutta"},
		{5.75, 		48, "(GMT+05:45) Nepal", "Asia/Katmandu"},
		{6.0, 		49, "(GMT+06:00) Almaty, Dhaka", "Asia/Almaty"},
		{6.5, 		50, "((GMT+06:30) Cocos Islands, Myanmar", "Asia/Rangoon"},
		{7.0, 		51, "(GMT+07:00) Bangkok, Jakarta, Hanoi", "Asia/Bangkok"},
		{8.0, 		52, "(GMT+08:00) Beijing, Chongqing, Urumqi, Hong Kong, Perth, Singapore, Taipei", "Asia/Chongqing"},
		{9.0, 		54, "(GMT+09:00) Toyko, Osaka, Sapporo", "Asia/Toyko"},
		{9.5, 		55, "(GMT+09:30) Northern Territory, South Australia", "Australia/South"},
		{10.0, 		56, "(GMT+10:00) Brisbane", "Australia/Brisbane"},
		{10.5, 		60, "(GMT+10:30) Lord Howe Island", "Australia/Lord_Howe"},
		{11.0, 		62, "(GMT+11:00) Magada", "Asia/Magadan"},
		{11.5, 		63, "(GMT+11:30) Norfolk Island", "Pacific/Norfolk"},
		{12.0, 		64, "(GMT+12:00) Fiji, Kamchatka, Marshall Is.", "Pacific/Fiji"},
		{12.75, 	66, "(GMT+12:45) Chatham Islands", "Pacific/Chatham"},
		{13.0, 		67, "(GMT+13:00) Tonga", "Pacific/Tongatapu"},
		{14.0, 		68, "(GMT+14:00) Line Islands", "Pacific/Kiritimati"}
};

#endif

#ifndef _OPENWRT_
/* nvram setting restore related definitions */
#define NVRAM_FILE_NAME "/tmp/Belkin_settings/nvram_settings.sh"
#define NVRAM_SETTING_BUF_SIZE	SIZE_2048B


char* g_apu8NvramVars[] = {
		"timezone_index",
		"restore_state",
		"home_id",
		"SmartDeviceId",
		"SmartPrivatekey",
		"token_id",
		"PluginCloudId",
		"plugin_key",
		"wl0_currentChannel",
		"cwf_serial_number",
		"ClientSSID",
		"ClientPass",
		"ClientAuth",
		"ClientEncryp",
		"APChannel",
		"RouterMac",
		"RouterSsid",
		"LastTimeZone",
		"DstSupportFlag",
		"LastDstEnable",
		"NotificationFlag",
		"FirmwareVersion",
		"SkuNo",
		"DeviceType",
		"StatusTS",
		"PVT_LOG_ENABLE",
		"FirmwareUpURL",
		"FriendlyName",
		"RuleDbVersion",
		"ntp_server1"
};
#endif

char* gDevTypeStringArr[] = {
		"controllee", /* Default - unknown case */
		"controllee", /* DEVICE_SOCKET */
		"sensor", /* DEVICE_SENSOR */
		"wemo_baby", /* DEVICE_BABYMON */
		"stream", /* DEVICE_STREAMING - NOT USED */
		"bridge", /* DEVICE_BRIDGE - NOT USED */
		"insight", /* DEVICE_INSIGHT */
		"wemo_crockpot", /* DEVICE_CROCKPOT */
		"lightswitch", /* DEVICE_LIGHTSWITCH */
		"NetCamSensor", /* DEVICE_NETCAM */
  		LINKSYSWNC_NAME, /* DEVICE_LINKSYS_WNC_CAM */
		"sbiron", /* DEVICE_SBIRON */
		"mrcoffee", /* DEVICE_MRCOFFEE */
		"petfeeder", /* DEVICE_PETFEEDER */
		"smart", /* DEVICE_SMART */
		"maker" /* DEVICE_MAKER */
		"EchoWater", /* DEVICE_ECHO */
};

char* gDevUDNStringArr[] = {
		"Socket", /* Default - unknown case */
		"Socket", /* DEVICE_SOCKET */
		"Sensor", /* DEVICE_SENSOR */
		"wemo_baby", /* DEVICE_BABYMON */
		"stream", /* DEVICE_STREAMING - NOT USED */
		"Bridge", /* DEVICE_BRIDGE - For WeMo-Bridge-LEDLight */
		"Insight", /* DEVICE_INSIGHT */
		"wemo_crockpot", /* DEVICE_CROCKPOT */
		"Lightswitch", /* DEVICE_LIGHTSWITCH */
		"NetCamSensor", /* DEVICE_NETCAM */
  		LINKSYSWNC_NAME, /* DEVICE_LINKSYS_WNC_CAM */
		"Sbiron",       /* DEVICE_SBIRON */
		"Mrcoffee",     /* DEVICE_MRCOFFEE */
		"Petfeeder", /* DEVICE_PETFEEDER */
		"Smart",        /* DEVICE_SMART */
		"Maker", /* DEVICE_MAKER */
		"EchoWater", /* DEVICE_ECHO */
};

char* gDefFriendlyName[] = {
		DEFAULT_SOCKET_FRIENDLY_NAME, /* Default - unknown case */
		DEFAULT_SOCKET_FRIENDLY_NAME, /* DEVICE_SOCKET */
		DEFAULT_SENSOR_FRIENDLY_NAME, /* DEVICE_SENSOR */
		DEFAULT_BABY_FRIENDLY_NAME, /* DEVICE_BABYMON */
		DEFAULT_STREAMING_FRIENDLY_NAME, /* DEVICE_STREAMING - NOT USED */
		DEFAULT_BRIDGE_FRIENDLY_NAME, /* DEVICE_BRIDGE - NOT USED */
		DEFAULT_INSIGHT_FRIENDLY_NAME, /* DEVICE_INSIGHT */
		DEFAULT_CROCKPOT_FRIENDLY_NAME, /* DEVICE_CROCKPOT */
		DEFAULT_LIGHTSWITCH_FRIENDLY_NAME, /* DEVICE_LIGHTSWITCH */
		DEFAULT_NETCAM_FRIENDLY_NAME, /* DEVICE_NETCAM */
  		DEFAULT_LINKSYSWNC_FRIENDLY_NAME, /* DEVICE_LINKSYSWNC */
		DEFAULT_SBIRON_FRIENDLY_NAME, /* DEVICE_SBIRON */
		DEFAULT_MRCOFFEE_FRIENDLY_NAME, /* DEVICE_MRCOFFEE */
		DEFAULT_PETFEEDER_FRIENDLY_NAME,/* DEVICE_PETFEEDER */
		DEFAULT_SMART_FRIENDLY_NAME,/* DEVICE_SMART */
		DEFAULT_MAKER_FRIENDLY_NAME,/* DEVICE_MAKER */
		DEFAULT_ECHO_FRIENDLY_NAME,/* DEVICE_ECHO*/
};

const char *VarTypes[] = {
   "Invalid!",
   "ENUM",
   "PERCENT",
   "TEMP",
   "TIME32",
   "TIME16",
   "TIMEBCD",
   "BOOL",
   "BCD_DATE",
   "DATETIME",
   "STRING",
   "BLOB",
   "UINT8",
   "INT8",
   "UINT16",
   "INT16",
   "UINT32",
   "INT32",
   "TIME_M16"
};

const char *Usages[] = {
   "Invalid!",
   "FIXED",
   "MONITORED",
   "DESIRED",
   "CONTROLLED"
};

const char *ValStates[] = {
   "Not set",        // VAR_VALUE_UNKNOWN
   "Cached value",   // VAR_VALUE_CACHED:
   "Live value",     // VAR_VALUE_LIVE
   "Set Value",      // VAR_VALUE_SET
   "Value being Set" // VAR_VALUE_SETTING
};

char* gDeviceClientType[] = {
		":a3b6-41e8-afb5-a3430cea2dcd", /* Default - unknown case */
		":a3b6-41e8-afb5-a3430cea2dcd", /* DEVICE_SOCKET */
		":1a08-463e-8cbb-e4ea74e427ed", /* DEVICE_SENSOR */
		":", /* DEVICE_BABYMON */
		":", /* DEVICE_STREAMING : NOT USED */
		":52f8-406e-a5a6-5c5a2254868c", /* DEVICE_BRIDGE - NOT USED */
		":5e7e-40a7-8bf4-539d1dc2ce42", /* DEVICE_INSIGHT */
		":b136dbb7-5880-48f1-a347-084347ad22d9", /* DEVICE_CROCKPOT */
		":ccbb-4346-a240-70bcf8f53632", /* DEVICE_LIGHTSWITCH */
		":4f76-477f-a7ca-fd56a8f85390", /* DEVICE_NETCAM */
		":", /* DEVICE_SBIRON */
		":66bf-445d-8404-994bc95055bf", /* DEVICE_MRCOFFEE */
		":", /* DEVICE_PETFEEDER */
		":", /* DEVICE_SMART */
		"3310c367-c39b-4966-8b0e-07cbd3cbdde7", /* DEVICE_MAKER */
		":e19a9d16-1136-4519-a1ce-0732e2b2d848", /* DEVICE_ECHO*/
};


productNameTbl g_Modelcode_Productname[]={
   {"AirPurifier","AirPurifier"},
   {"wemo_baby","BabyMonitor"},
   {"Bridge","Bridge"},
   {"Classic A60 RGBW","FlexBulb"},
   {"Classic A60 TW","TemperatureBulb"},
   {"CoffeeMaker","CoffeeMaker"},
   {"Connected A-19 60W Equivalent","Lighting"},
   {"crockpot","crockpot"},
   {"Flex RGBW","FlexBulb"},
   {"Gardenspot RGB","ColorBulb"},
   {"HeaterA","HeaterA"},
   {"HeaterB","HeaterB"},
   {"Humidifier"," Humidifier"},
   {"Insight","Insight"},
   {"iQBR30","Lighting"},
   {"LCT001","Lighting"},
   {"LGDWL","Lighting"},
   {"LIGHTIFY A19 Tunable White","TemperatureBulb"},
   {"LIGHTIFY Flex RGBW","FlexBulb"},
   {"LIGHTIFY Gardenspot RGB","ColorBulb"},
   {"Lightswitch","Lightswitch"},
   {"LWB004","Lighting"},
   {"Maker","Maker"},
   {"MZ100","Lighting"},
   {"NetCam","NetCam"},
   {"NetCamHDv1","NetCamHDv1"},
   {"NetCamHDv2","NetCamHDv2"},
   {"Sensor","Sensor"},
   {"smart","smart"},
   {"Socket","Socket"},
   {"Surface Light TW","Lighting"},
   {"Water","Water"},
   {"ZLL Light","Lighting"},
   {"F7C038","DWSensor"},
   {"F7C039","Fob"},
   {"F7C040","AlarmSensor"},
   {"F7C041","PIR"},
};

#define TOTAL_PRODUCT_NAME  sizeof(g_Modelcode_Productname) / sizeof(g_Modelcode_Productname[0])
#define DEFAULT_CASE_SESOR  "Sensor"
#define DEFAULT_CASE_LIGHT  "lighting"

//--------------- Local Definition -------------

UpnpDevice_Handle device_handle = -1;
/*
	 The amount of time (in seconds) before advertisements
	 will expire
 */
int default_advr_expire = 86400;//24 * 60 *60;

//- In default, in local network
int g_IsUPnPOnInternet = FALSE;
//- In dedault, not in setup mode
int g_IsDeviceInSetupMode = FALSE;

extern char g_szHomeId[SIZE_20B];
extern char g_szSmartDeviceId[SIZE_256B];
extern char g_szSmartPrivateKey[MAX_PKEY_LEN];
extern char g_szPluginPrivatekey[MAX_PKEY_LEN];
extern char g_szPluginCloudId[SIZE_16B];
static pthread_mutex_t s_upnp_param_mutex;

//- store socket override status
#define	MAX_OVERRIDEN_STATUS_LEN	512
char szOverridenStatus[MAX_OVERRIDEN_STATUS_LEN];

char *GetWemoMacAddress(void)
{
  return g_szWiFiMacAddress;
}

char *GetWemoFriendlyName(void)
{
  return g_szFriendlyName;
}

char *GetWemoFirmwareVersion(void)
{
  return g_szFirmwareVersion;
}

char *GetWemoDeviceUDN(void)
{
  return g_szUDN_1;
}

const char *getProductName(char *modelCode)
{
    int index;
    if( (NULL == modelCode) || (0 == strlen(modelCode)))
    /**Handle NULL Case and Empty string Case of model Code specical case for Home Sensor*/
    {
            APP_LOG("UPNP: DEVICE", LOG_DEBUG,"###########modelCode Empty or NULL##############");
            goto EXIT;
    }
    for( index = 0; index < TOTAL_PRODUCT_NAME; index++ )
    {
        if( !strcmp(modelCode,g_Modelcode_Productname[index].modelCode) )
        {
            return g_Modelcode_Productname[index].productName;
        }
    }
EXIT:
    /**This is added for default case as for wiki page,  it
    *  may be extend further as soon as wiki page edited.
    */
#ifdef PRODUCT_WeMo_LEDLight
    return DEFAULT_CASE_LIGHT;
#else
    return DEFAULT_CASE_SESOR;
#endif
}

void getModelCode(char *modelCode)
{
#ifdef NETCAM_NetCam
    strncpy(modelCode,"NetCam",SIZE_32B-1);
#elif NETCAM_HD_V1
    strncpy(modelCode,"NetCamHDv1",SIZE_32B-1);
#elif NETCAM_HD_V2
    strncpy(modelCode,"NetCamHDv2",SIZE_32B-1);
#elif NETCAM_LS
    strncpy(modelCode,"LinksysWNC",SIZE_32B-1);
#else
    strncpy(modelCode,getDeviceUDNString(),SIZE_32B-1);
#endif
}
void initUPnPThreadParam()
{
		ithread_mutexattr_t attr;
		ithread_mutexattr_init(&attr);
		ithread_mutexattr_setkind_np( &attr, ITHREAD_MUTEX_RECURSIVE_NP );
		ithread_mutex_init(&s_upnp_param_mutex, &attr);
		ithread_mutexattr_destroy(&attr);
}

void initFWUpdateStateLock()
{
		osUtilsCreateLock(&gFWUpdateStateLock);
}

void initSiteSurveyStateLock()
{
		osUtilsCreateLock(&gSiteSurveyStateLock);
}

int	 IsTimeUpdateByMobileApp()
{
	int ret = 0x00;
	ithread_mutex_lock(&s_upnp_param_mutex);
		ret = g_isTimeSyncByMobileApp;
	ithread_mutex_unlock(&s_upnp_param_mutex);

	return ret;
}

void UpdateMobileTimeSync(int newState)
{
	ithread_mutex_lock(&s_upnp_param_mutex);
		g_isTimeSyncByMobileApp = newState;
	ithread_mutex_unlock(&s_upnp_param_mutex);
}

void UPnPTimeSyncStatusNotify()
{
		char* szParamNames[] = {"TimeSyncRequest"};
		char* szParamValues[] = {"0"};
#ifdef PRODUCT_WeMo_LEDLight
		int event_type = PLUGIN_E_BRIDGE_SERVICE;
#else
		int event_type = PLUGIN_E_EVENT_SERVICE;
#endif

		UpnpNotify(device_handle, SocketDevice.service_table[event_type].UDN,
						SocketDevice.service_table[event_type].ServiceId, (const char **)szParamNames, (const char **)szParamValues, 0x01);

		APP_LOG("UPNP: DEVICE", LOG_DEBUG, "###############################Notification: time sync request sent");

		//- add the time zone and dst push notification to sensor, short term solution
		//- Below for NetCam, but should in SNS to support the timezone push notification
		char* tzIndex = GetBelkinParameter("timezone_index");
		if ((0x00 != tzIndex) && (0x00 != strlen(tzIndex)))
		{
				char* szTimeZone[] = {"TimeZoneNotification"};
				char* szTimeZoneValue[1] = {0x00};
				szTimeZoneValue[0] = (char*)ZALLOC(SIZE_4B);
				snprintf(szTimeZoneValue[0], SIZE_4B, "%s", tzIndex);
				UpnpNotify(device_handle, SocketDevice.service_table[event_type].UDN,
								SocketDevice.service_table[event_type].ServiceId, (const char **)szTimeZone, (const char **)szTimeZoneValue, 0x01);

				free(szTimeZoneValue[0]);
		}

}

void UPnPSetHomeDeviceIdNotify()
{
	char* szParamValues[] = {g_szHomeId, g_szSmartDeviceId};
	char* szParamNames[] = {"HomeIdRequest", "DeviceIdRequest"};

	UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].UDN,
					SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].ServiceId, (const char **)szParamNames, (const char **)szParamValues, 0x02);

	APP_LOG("UPNP: DEVICE", LOG_HIDE, "###############################Notification: HomeId - szParamValues[0]: <%s> and DeviceId - szParamValues[1]: <%s> sent", szParamValues[0], szParamValues[1]);

}

void UPnPSendSignatureNotify()
{
		char* szParamValues[1];
		authSign *assign = NULL;
		char sKey[MAX_PKEY_LEN];
		memset(sKey, 0x0, MAX_PKEY_LEN);
		strncpy(sKey, g_szPluginPrivatekey, sizeof(sKey)-1);

		char* szParamNames[] = {"PluginParam"};
		szParamValues[0] = (char*)ZALLOC(SIZE_256B);

		assign = createAuthSignature(g_szWiFiMacAddress, g_szSerialNo, sKey);
		if (!assign) {
				free(szParamValues[0]);
				APP_LOG("UPNP: DEVICE", LOG_ERR, "\n Signature Structure returned NULL\n");
				return;
		}

		APP_LOG("UPNP: DEVICE", LOG_HIDE, "###############################Before encryption: Signature: <%s>", assign->signature);
		encryptSignature(assign->signature, szParamValues[0]);

		UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].UDN,
						SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].ServiceId, (const char **)szParamNames, (const char **)szParamValues, 0x01);

		APP_LOG("UPNP: DEVICE", LOG_HIDE, "###############################Notification: Signature- szParamValues[0]: <%s> sent", szParamValues[0]);

		free(szParamValues[0]);
		if (assign) {free(assign); assign = NULL;};
}

char* getDeviceTypeString(void)
{
		APP_LOG("UPNP: DEVICE", LOG_ALERT, "Device type: %d and temp type: %d", g_eDeviceType, g_eDeviceTypeTemp);
		if(g_eDeviceTypeTemp)
		{
				APP_LOG("UPNP: DEVICE", LOG_ALERT, "Device type string: %s", gDevTypeStringArr[g_eDeviceTypeTemp]);
				return gDevTypeStringArr[g_eDeviceTypeTemp];
		}
		else
		{
				APP_LOG("UPNP: DEVICE", LOG_ALERT, "Device type string: %s", gDevTypeStringArr[g_eDeviceType]);
				return gDevTypeStringArr[g_eDeviceType];
		}

}

char* getDeviceUDNString(void)
{
		APP_LOG("UPNP: DEVICE", LOG_ALERT, "Device type: %d and temp type: %d", g_eDeviceType, g_eDeviceTypeTemp);
		if(g_eDeviceTypeTemp)
		{
				APP_LOG("UPNP: DEVICE", LOG_ALERT, "Device UDN: %s", gDevUDNStringArr[g_eDeviceTypeTemp]);
				return gDevUDNStringArr[g_eDeviceTypeTemp];
		}
		else
		{
				APP_LOG("UPNP: DEVICE", LOG_ALERT, "Device UDN: %s", gDevUDNStringArr[g_eDeviceType]);
				return gDevUDNStringArr[g_eDeviceType];
		}

}

#ifdef PRODUCT_WeMo_NetCam
void updateXmlModelName(char *szBuff)
{
    char modelCode[SIZE_32B]=" ";
    getModelCode(modelCode);
    sprintf(szBuff, "<modelName>%s</modelName>\n", modelCode);
    return;
}
#endif

int getClientType(void)
{
	int index=0;

        if(g_eDeviceTypeTemp)
        {
		index = g_eDeviceTypeTemp;
        }
        else
        {
		index = g_eDeviceType;
        }

	memset(g_szClientType, 0, sizeof(g_szClientType));

	strncpy(g_szClientType, g_szFirmwareVersion,sizeof(g_szClientType) - 1);
        strncat(g_szClientType, gDeviceClientType[index], sizeof(g_szClientType) - strlen(g_szClientType) - 1);

        APP_LOG("UPNP: DEVICE", LOG_DEBUG, "Idx: %d, Client Type: %s, len: %d", index, g_szClientType, strlen(g_szClientType));

	return SUCCESS;
}

void updateXmlDeviceTag(char *szBuff)
{

	char *pDevString;
#ifdef PRODUCT_WeMo_Smart
        pDevString = g_DevName;
#else
    pDevString = getDeviceTypeString();

	if(0 == strcmp(pDevString, "wemo_baby"))
	{
		if(strlen(g_szProdVarType) > 0)
			pDevString = g_szProdVarType;
	}
#endif
	sprintf(szBuff, "<deviceType>urn:Belkin:device:%s:1</deviceType>\n", pDevString);
	APP_LOG("UPNP: DEVICE", LOG_DEBUG, "Device type tag: %s", szBuff);
}


void updateXmlUDNTag(char *szBuff, char *szBuff1)
{

	char *pDevString;
#ifdef PRODUCT_WeMo_Smart
        pDevString = g_DevName;
#else
	pDevString = getDeviceUDNString();

	if(0 == strcmp(pDevString, "wemo_baby"))
	{
		if(strlen(g_szProdVarType) > 0)
			pDevString = g_szProdVarType;
	}
#endif

	sprintf(szBuff1, "uuid:%s-1_0-%s", pDevString, g_szSerialNo);
	sprintf(szBuff, "<UDN>%s</UDN>\n", szBuff1);
	APP_LOG("UPNP: DEVICE", LOG_DEBUG, "Device UDN tag: %s", szBuff);
}


int UpdateXML2Factory()
{
		FILE* pfReadStream  = 0x00;
		FILE* pfWriteStream = 0x00;
		char szBuff[SIZE_256B];
		char szBuff1[SIZE_256B];

		//- Open file to write
		//gautam: update the Insight and LS Makefile to copy Insightsetup.xml and Lightsetup.xml in /sbin/web/ as setup.xml
		pfReadStream 	= fopen("/sbin/web/setup.xml", "r");
		pfWriteStream = fopen("/tmp/Belkin_settings/setup.xml", "w");

		if (0x00 == pfReadStream || 0x00 == pfWriteStream)
		{
				APP_LOG("UPNP: DEVICE", LOG_ERR, "UpdateXML2Factory: open files handles failure");

				if(pfReadStream)
						fclose(pfReadStream);

				if(pfWriteStream)
						fclose(pfWriteStream);

				return PLUGIN_UNSUCCESS;
		}

		while (!feof(pfReadStream))
		{
				memset(szBuff, 0x00, sizeof(szBuff));

				fgets(szBuff, SIZE_256B, pfReadStream);

#ifdef PRODUCT_WeMo_NetCam
				if(strstr(szBuff, "<modelName>")) {
					memset(szBuff, 0x00, sizeof(szBuff));
					updateXmlModelName(szBuff);
				} else
#endif
				if (strstr(szBuff, "<deviceType>"))
				{
						memset(szBuff, 0x00, sizeof(szBuff));
						updateXmlDeviceTag(szBuff);
				}
				else if (strstr(szBuff, "<UDN>"))
				{
						//- reset it again
						memset(szBuff, 0x00, sizeof(szBuff));
						memset(szBuff1, 0x00, sizeof(szBuff1));
						updateXmlUDNTag(szBuff, szBuff1);

						memset(g_szUDN, 0x00, sizeof(g_szUDN));
						strncpy(g_szUDN, szBuff1, sizeof(g_szUDN)-1);
						strncpy(g_szUDN_1, szBuff1, sizeof(g_szUDN_1)-1);

	    APP_LOG("UPNP: DEVICE", LOG_DEBUG, "Device UDN: %s", g_szUDN_1);
	}
	else if (strstr(szBuff, "<serialNumber>"))
	{
	    memset(szBuff, 0x00, sizeof(szBuff));
	    snprintf(szBuff, SIZE_256B, "<serialNumber>%s</serialNumber>\n", g_szSerialNo);
	}
    else if (strstr(szBuff, "modelName"))
	{
#ifdef PRODUCT_WeMo_Smart
            memset(szBuff, 0x00, sizeof(szBuff));
            snprintf(szBuff, SIZE_256B, "<modelName>%s</modelName>\n", g_DevName);
#endif
    }
	else if (strstr(szBuff, "friendlyName"))
	{
#ifdef PRODUCT_WeMo_Smart
            memset(szBuff, 0x00, sizeof(szBuff));
            snprintf(szBuff, SIZE_256B, "<friendlyName>%s</friendlyName>\n", g_DevName);
#else
	    memset(szBuff, 0x00, sizeof(szBuff));
	    snprintf(szBuff, SIZE_256B, "<friendlyName>%s</friendlyName>\n", g_szFriendlyName);
#endif
	}
	else if (strstr(szBuff, "firmwareVersion"))
	{
	    memset(szBuff, 0x00, sizeof(szBuff));
	    snprintf(szBuff, SIZE_256B, "<firmwareVersion>%s</firmwareVersion>\n", g_szFirmwareVersion);
	}
	else if (strstr(szBuff, "macAddress"))
	{
	    memset(szBuff, 0x00, sizeof(szBuff));
	    snprintf(szBuff, SIZE_256B, "<macAddress>%s</macAddress>\n", g_szWiFiMacAddress);
	}
	else if (strstr(szBuff, "iconVersion"))
	{
	    memset(szBuff, 0x00, sizeof(szBuff));
	    int port = 0;

						port = UpnpGetServerPort();
						snprintf(szBuff, SIZE_256B, "<iconVersion>%d|%d</iconVersion>\n", gWebIconVersion, port);
				}
				else if (strstr(szBuff, "binaryState"))
				{
						memset(szBuff, 0x00, sizeof(szBuff));
						int state = 0;

						state = GetCurBinaryState();
						snprintf(szBuff, SIZE_256B, "<binaryState>%d</binaryState>\n", state);
				}

				fwrite(szBuff, 1, strlen(szBuff), pfWriteStream);

		}

		fclose(pfReadStream);
		fclose(pfWriteStream);

    APP_LOG("UPNP", LOG_DEBUG, "Replace set up XML successfully\n");
#ifdef PRODUCT_WeMo_Smart
    createservicexml();
#endif
    APP_LOG("UPNP", LOG_DEBUG, "After create xml");
    return 0x00;

}

#ifdef PRODUCT_WeMo_Smart
int createservicexml()
{

    APP_LOG("UPNP: DEVICE", LOG_ERR, "Inside createservicexml");

    FILE* pfWriteStream = 0x00;
    char szBuff[SIZE_256B];
	char szBuff1[SIZE_256B];
    int i, j;

	//- Open file to write

	pfWriteStream = fopen("/tmp/Belkin_settings/deviceservice.xml", "w");

	if (0x00 == pfWriteStream)
	{
		APP_LOG("UPNP: DEVICE", LOG_ERR, "createservicexml: open files handles failure");

		if(pfWriteStream)
			fclose(pfWriteStream);
		memset(g_szUDN, 0x00, sizeof(g_szUDN));
		strncpy(g_szUDN, szBuff1, sizeof(g_szUDN)-1);
		strncpy(g_szUDN_1, szBuff1, sizeof(g_szUDN_1)-1);

		return PLUGIN_UNSUCCESS;
	}

    APP_LOG("UPNP: DEVICE", LOG_ERR, "pfwritestream open successful");
    memset(szBuff, 0x00, sizeof(szBuff));
    snprintf(szBuff, 177, "<?xml version=\"1.0\"?><scpd xmlns=\"urn:Belkin:service-1-0\"><specVersion><major>1</major><minor>0</minor></specVersion><actionList><action><name>SetAttribute</name><argumentList>");

    fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );

    for(i=0; i < g_attrCount; i++)
    {
        APP_LOG("UPNP: DEVICE", LOG_ERR, "Inside setattribute for loop");
        if(g_attrVarDescArray[i]->Usage == 4 ||g_attrVarDescArray[i]->Usage == 3) //Controlled
        {
            memset(szBuff, 0x00, sizeof(szBuff));
            snprintf(szBuff, SIZE_256B, "<argument><retval  /><name>%s</name><relatedStateVariable>%s</relatedStateVariable><direction>in</direction></argument>", g_attrVarDescArray[i]->Name, VarTypes[g_attrVarDescArray[i]->Type]);
            fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );
        }
    }
    memset(szBuff, 0x00, sizeof(szBuff));
    snprintf(szBuff, 73, "</argumentList></action><action><name>GetAttribute</name><argumentList>");
    fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );

    for(i=0; i < g_attrCount; i++)
    {
        APP_LOG("UPNP: DEVICE", LOG_ERR, "Inside getattribute for loop");
        if(g_attrVarDescArray[i]->Usage != 3)
        {
            memset(szBuff, 0x00, sizeof(szBuff));
            APP_LOG("UPNP: DEVICE", LOG_ERR, "Getattribute usage check");
            snprintf(szBuff, SIZE_256B, "<argument><retval /><name>%s</name><relatedStateVariable>%s</relatedStateVariable><direction>in</direction></argument>",g_attrVarDescArray[i]->Name,VarTypes[g_attrVarDescArray[i]->Type]);
            APP_LOG("UPNP: DEVICE", LOG_ERR, "snprintf of getattribute loop");
            fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );
            APP_LOG("UPNP: DEVICE", LOG_ERR, "fwrite of getattribute loop");
        }
    }

        //notification
//setnotif
    memset(szBuff, 0x00, sizeof(szBuff));
    snprintf(szBuff, SIZE_256B, "</argumentList></action><action><name>SetNotif</name><argumentList>");
    fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );

    for(i=0; i < g_attrCount; i++)
    {
        APP_LOG("UPNP: DEVICE", LOG_ERR, "Inside setattribute for loop");
        if(g_attrVarDescArray[i]->Usage == 4) //Controlled
        {
            memset(szBuff, 0x00, sizeof(szBuff));
            snprintf(szBuff, SIZE_256B, "<argument><retval  /><name>%s</name><relatedStateVariable>Attribute</relatedStateVariable><direction>in</direction></argument>", g_attrVarDescArray[i]->Name);
            fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );
        }
    }

    memset(szBuff, 0x00, sizeof(szBuff));
    snprintf(szBuff, SIZE_256B, "<argument><retval  /><name>dstURL</name><relatedStateVariable>dstURL</relatedStateVariable><direction>in</direction></argument>");
    fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );

    memset(szBuff, 0x00, sizeof(szBuff));
    snprintf(szBuff, SIZE_256B, "<argument><retval  /><name>dstType</name><relatedStateVariable>dstType</relatedStateVariable><direction>in</direction></argument>");
    fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );

//ResetNotif
    memset(szBuff, 0x00, sizeof(szBuff));
    snprintf(szBuff, 73, "</argumentList></action><action><name>ResetNotif</name><argumentList>");
    fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );

    for(i=0; i < g_attrCount; i++)
    {
        APP_LOG("UPNP: DEVICE", LOG_ERR, "Inside setattribute for loop");
        if(g_attrVarDescArray[i]->Usage == 4) //Controlled
        {
            memset(szBuff, 0x00, sizeof(szBuff));
            snprintf(szBuff, SIZE_256B, "<argument><retval  /><name>%s</name><relatedStateVariable>Attribute</relatedStateVariable><direction>in</direction></argument>", g_attrVarDescArray[i]->Name);
            fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );
        }
    }


    memset(szBuff, 0x00, sizeof(szBuff));
    snprintf(szBuff, SIZE_256B, "</argumentList></action></actionList><serviceStateTable>");
    APP_LOG("UPNP: DEVICE", LOG_ERR, "After 5th snprintf");
    fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );
    APP_LOG("UPNP: DEVICE", LOG_ERR, "After 5th fwrite");

    for(i=0; i < g_attrCount; i++)
    {
        memset(szBuff, 0x00, sizeof(szBuff));
        APP_LOG("UPNP: DEVICE", LOG_ERR, "Inside the loop for writing statevariables");
        snprintf(szBuff, SIZE_256B, "<stateVariable sendEvents=\"yes\"><name>%s</name><dataType>%s</dataType><attrType>%d</attrType><attrAccessType>%d</attrAccessType><defaultValue>0</defaultValue>",
					g_attrVarDescArray[i]->Name, VarTypes[g_attrVarDescArray[i]->Type], g_attrVarDescArray[i]->Type, g_attrVarDescArray[i]->Usage );
        APP_LOG("UPNP: DEVICE", LOG_ERR, "After snprintf in statevariable");
        fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );
        APP_LOG("UPNP: DEVICE", LOG_ERR, "After fwrite in statevariable");

        if (g_attrVarDescArray[i]->Type == WASP_VARTYPE_ENUM)
        {
            memset(szBuff, 0x00, sizeof(szBuff));
            APP_LOG("UPNP: DEVICE", LOG_ERR, "Listing enums");
            snprintf(szBuff, SIZE_256B, "<allowedValueList>");
            APP_LOG("UPNP: DEVICE", LOG_ERR, "snprintf of listing enums");
            fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );
            APP_LOG("UPNP: DEVICE", LOG_ERR, "fwrite of listing enums");

            for(j=0; j<g_attrVarDescArray[i]->Enum.Count; j++)
            {
                memset(szBuff, 0x00, sizeof(szBuff));
                APP_LOG("UPNP: DEVICE", LOG_ERR, "Inside for loop of enum count");
                snprintf(szBuff, SIZE_256B, "<allowedValue>%d|%s</allowedValue>",j, g_attrVarDescArray[i]->Enum.Name[j]);
                APP_LOG("UPNP: DEVICE", LOG_ERR, "sprintf in the loop of enum count");
                fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );
                APP_LOG("UPNP: DEVICE", LOG_ERR, "fwrite in the loop of enum count");

            }

            memset(szBuff, 0x00, sizeof(szBuff));
            snprintf(szBuff, SIZE_256B, "</allowedValueList>");
            APP_LOG("UPNP: DEVICE", LOG_ERR, "snprintf after allowedvaluelist");
            fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );
            APP_LOG("UPNP: DEVICE", LOG_ERR, "fwrite after allowed value list");
        }

        memset(szBuff, 0x00, sizeof(szBuff));
        snprintf(szBuff, SIZE_256B, "</stateVariable>");
        APP_LOG("UPNP: DEVICE", LOG_ERR, "snprintf after statevariable");
        fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );
        APP_LOG("UPNP: DEVICE", LOG_ERR, "fwrite after statevariable");

    }

	memset(szBuff, 0x00, sizeof(szBuff));
	snprintf(szBuff, SIZE_256B, "</serviceStateTable></scpd>");
	APP_LOG("UPNP: DEVICE", LOG_ERR, "snprintf after scpd");
	fwrite(szBuff , 1 , strlen(szBuff) , pfWriteStream );
	APP_LOG("UPNP: DEVICE", LOG_ERR, "fwrite after scpd");

    fclose(pfWriteStream);

    APP_LOG("UPNP", LOG_DEBUG, "services XML successfully\n");

    return 0x00;
}
#endif

void GetMacAddress()
{
		char* szMac = utilsRemDelimitStr(GetMACAddress(),":");
		memset(g_szWiFiMacAddress, 0x00, sizeof(g_szWiFiMacAddress));

		strncpy(g_szWiFiMacAddress, szMac, sizeof(g_szWiFiMacAddress)-1);
		free(szMac);
		APP_LOG("STARTUP", LOG_DEBUG, "MAC:%s", g_szWiFiMacAddress);
}


void GetFirmware()
{
		//char* szPreviousVserion = GetBelkinParameter("FirmwareVersion");

		char* szPreviousVserion = g_szBuiltFirmwareVersion;

		memset(g_szFirmwareVersion, 0x00, sizeof(g_szFirmwareVersion));
		if (0x00 == szPreviousVserion || 0x00 == strlen(szPreviousVserion))
		{
				snprintf(g_szFirmwareVersion, sizeof(g_szFirmwareVersion), "%s", DEFAULT_FIRMWARE_VERSION);
		}
		else
		{
				snprintf(g_szFirmwareVersion, sizeof(g_szFirmwareVersion), "%s", szPreviousVserion);
		}

		APP_LOG("Bootup", LOG_DEBUG, "Firmware:%s, built time: %s", g_szFirmwareVersion, g_szBuiltTime?g_szBuiltTime:"Unknown");

}
void GetSkuNo()
{
		char* szPreviousSkuNo   = GetBelkinParameter("SkuNo");
		memset(g_szSkuNo, 0x00, sizeof(g_szSkuNo));
		if (0x00 == szPreviousSkuNo || 0x00 == strlen(szPreviousSkuNo))
		{
				snprintf(g_szSkuNo, sizeof(g_szSkuNo), "%s", DEFAULT_SKU_NO);
		}
		else
		{
				snprintf(g_szSkuNo, sizeof(g_szSkuNo), "%s", szPreviousSkuNo);
		}


		APP_LOG("STARTUP", LOG_DEBUG, "SKU:%s", g_szSkuNo);

}

char* getDefaultFriendlyName()
{
	#ifdef PRODUCT_WeMo_Smart
        return g_DevName;
	#endif
	if(g_eDeviceTypeTemp)
	{
		APP_LOG("UPNP: DEVICE", LOG_DEBUG, "Name : %s", gDefFriendlyName[g_eDeviceTypeTemp]);
		return gDefFriendlyName[g_eDeviceTypeTemp];
	}
	else
	{
		APP_LOG("UPNP: DEVICE", LOG_DEBUG, "Name: %s", gDefFriendlyName[g_eDeviceType]);
		return gDefFriendlyName[g_eDeviceType];
	}


}

void GetDeviceFriendlyName()
{
		char *pszFriendlyName = NULL;

		memset(g_szFriendlyName, 0x00, sizeof(g_szFriendlyName));
		char *szFriendlyName = GetDeviceConfig("FriendlyName");

		if ((0x00 != szFriendlyName) && (0x00 != strlen(szFriendlyName)))
		{
				strncpy(g_szFriendlyName, szFriendlyName, sizeof(g_szFriendlyName)-1);
		}
		else
		{
				pszFriendlyName = getDefaultFriendlyName();

				if(0 == strcmp(pszFriendlyName, DEFAULT_BABY_FRIENDLY_NAME))
				{
						char *pProdVar = GetBelkinParameter (PRODUCT_VARIANT_NAME);

						if(pProdVar!= NULL && (strlen(pProdVar) > 0))
								snprintf(g_szFriendlyName, sizeof(g_szFriendlyName), "%sBaby", pProdVar);
						else
								strncpy(g_szFriendlyName, pszFriendlyName, sizeof(g_szFriendlyName)-1);
				}
				else
						strncpy(g_szFriendlyName, pszFriendlyName, sizeof(g_szFriendlyName)-1);
		}

		APP_LOG("Startup", LOG_DEBUG, "Friendly Name: %s", g_szFriendlyName);
}

/**
 * GetDeviceType: This function identifies the device type
 *                based on the device serial number
 *
 * Following is the description of the serial no schema
 *  Supplier ID | Yr of Mfg | Wk of Mfg | Product | Unique Seq Id
 *            22|12|38|K01|FFFFF
 * K = Relay based products
 *      K01- Switch 1.0US; K11-Switch 1.0 WW; K12-Insight; K13 - Light Switch; K14 - Crockpot
 * L = Sensors
 *      L01 - Motion Sensor 1.0 US; L11 - Motion Sensor 1.0 WW;
 * B = Bridges
 * M = Monitors
 * V = Video
 * S = Smart
 */

void GetDeviceType()
{
    #ifdef PRODUCT_WeMo_Jarden
	unsigned short int Product_ID;
	unsigned short int Vendor_ID;
	unsigned int Device_ID;
	#endif /*#ifdef PRODUCT_WeMo_Jarden*/

    char DevSerial[SIZE_64B]={'\0'};

		APP_LOG("UPNP", LOG_ALERT, "serial no: %s", g_szSerialNo);
		strncpy(DevSerial, g_szSerialNo, sizeof(DevSerial)-1);

		if (0x00 == strlen(g_szSerialNo))
		{
				APP_LOG("UPNP", LOG_ERR, "Device type unknow, please see the manufacture reference");
				g_eDeviceType = DEVICE_SOCKET;
				g_eDeviceTypeTemp = DEVICE_UNKNOWN;
		}
		else
		{
				switch(DevSerial[SERIAL_TYPE_INDEX])
				{
						case 'K':
								{
										//- Switch
										if ('1' == DevSerial[SERIAL_TYPE_INDEX+2])
										{
												APP_LOG("UPNP", LOG_DEBUG, "DEVICE: SOCKET");
												g_eDeviceType = DEVICE_SOCKET;
												g_eDeviceTypeTemp = DEVICE_UNKNOWN;
										}
										else if ('2' == DevSerial[SERIAL_TYPE_INDEX+2])
										{
												APP_LOG("UPNP", LOG_DEBUG, "DEVICE: INSIGHT");
												g_eDeviceType = DEVICE_SOCKET;
												g_eDeviceTypeTemp = DEVICE_INSIGHT;
										}
										else if ('3' == DevSerial[SERIAL_TYPE_INDEX+2])
										{
												APP_LOG("UPNP", LOG_DEBUG, "DEVICE: LIGHTSWITCH");
												g_eDeviceType = DEVICE_SOCKET;
												g_eDeviceTypeTemp = DEVICE_LIGHTSWITCH;
										}
                    #if 0
					#ifdef PRODUCT_WeMo_Jarden
					else if ('4' == DevSerial[SERIAL_TYPE_INDEX+2])
					{
						APP_LOG("UPNP", LOG_DEBUG, "DEVICE: CROCKPOT");
						g_eDeviceType = DEVICE_CROCKPOT;
						g_eDeviceTypeTemp = DEVICE_UNKNOWN;

					}
                	else if ('5' == DevSerial[SERIAL_TYPE_INDEX+2])
                	{
                    	APP_LOG("UPNP", LOG_DEBUG, "DEVICE: SUNBEAM IRON");
                    	g_eDeviceType = DEVICE_SBIRON;
						g_eDeviceTypeTemp = DEVICE_UNKNOWN;
                	}
                	else if ('6' == DevSerial[SERIAL_TYPE_INDEX+2])
                	{
                    	APP_LOG("UPNP", LOG_DEBUG, "DEVICE: MR. COFFEE");
                    	g_eDeviceType = DEVICE_MRCOFFEE;
						g_eDeviceTypeTemp = DEVICE_UNKNOWN;
                	}
                	else if ('7' == DevSerial[SERIAL_TYPE_INDEX+2])
                	{
                    	APP_LOG("UPNP", LOG_DEBUG, "DEVICE: PET FEEDER");
                    	g_eDeviceType = DEVICE_PETFEEDER;
						g_eDeviceTypeTemp = DEVICE_UNKNOWN;
	                }
                	#endif /* #ifdef PRODUCT_WeMo_Jarden */
					#endif
								}
								break;

						case 'L':
								{
										APP_LOG("UPNP", LOG_DEBUG, "DEVICE: SENSOR");
										g_eDeviceType = DEVICE_SENSOR;
										g_eDeviceTypeTemp = DEVICE_UNKNOWN;
								}
								break;

						case 'M':
								{
										APP_LOG("UPNP", LOG_DEBUG, "DEVICE: BABYMON");
										g_eDeviceType = DEVICE_SOCKET;
										g_eDeviceTypeTemp = DEVICE_BABYMON;
								}
								break;

						case 'B':
								{
									APP_LOG("UPNP", LOG_DEBUG, "DEVICE: BRIDGE");
									g_eDeviceType = DEVICE_SOCKET;
									g_eDeviceTypeTemp = DEVICE_BRIDGE;
								}
								break;



					        case 'V':
					          /* Note that Netcam devices are built by partners and do not
					           * have factory generated serial numbers.  The serial number
					           * passed here is generated at run time by GetSerialNumber()
					           * in WeMo_NetCam/belkin_api/belkin_api.c  */
							          switch( DevSerial[SUB_DEVICE_TYPE_INDEX] ) {
							          case '1':
							          case '2':
							          default:
							            /* NetCam sensor, still as a sensor */
								            APP_LOG("UPNP", LOG_DEBUG, "DEVICE: NetCam SENSOR");
								            g_eDeviceType = DEVICE_SENSOR;
								            g_eDeviceTypeTemp = DEVICE_NETCAM;
							            break;
							          case '3':
							            // Linksys camera (aka Linksys WNC)
								            APP_LOG("UPNP", LOG_DEBUG, "DEVICE: Linksys SENSOR");
								            g_eDeviceType = DEVICE_SENSOR;
								            g_eDeviceTypeTemp = DEVICE_LINKSYS_WNC_CAM;
								            break;
							          }
							          // printf( "DEVTYPE: %s@%s:%d g_eDeviceType:%d, g_eDeviceTypeTemp:%d\n",
							          //         __FILE__, __FUNCTION__, __LINE__,
						        	  //         g_eDeviceType, g_eDeviceTypeTemp );
							          break;

			#ifdef PRODUCT_WeMo_Jarden
			case 'S':
				{
					Device_ID = szDeviceID;
					APP_LOG("Init", LOG_DEBUG, "Device ID from WASP is: %d", Device_ID);

					Vendor_ID = (unsigned short int)(Device_ID >> 16);
					if (JARDEN_VENDOR_ID != Vendor_ID)
					{
						APP_LOG("UPNP", LOG_DEBUG, "DEVICE: SMART");
						g_eDeviceType = DEVICE_SMART;
						g_eDeviceTypeTemp = DEVICE_UNKNOWN;
					}
					else
					{
						Product_ID = (unsigned short int)Device_ID;
						APP_LOG("Init", LOG_DEBUG, "Product ID from WASP is: %d", Product_ID)

						//- Smart module
						if (SLOW_COOKER_PRODUCT_ID == Product_ID)
						{
							APP_LOG("UPNP", LOG_DEBUG, "DEVICE: CROCKPOT");
							g_eDeviceType = DEVICE_CROCKPOT;
							g_eDeviceTypeTemp = DEVICE_UNKNOWN;
						}
						else if (SBIRON_PRODUCT_ID == Product_ID)
						{
							APP_LOG("UPNP", LOG_DEBUG, "DEVICE: SUNBEAM IRON");
							g_eDeviceType = DEVICE_SBIRON;
							g_eDeviceTypeTemp = DEVICE_UNKNOWN;
						}
						else if (MRCOFFEE_PRODUCT_ID == Product_ID)
						{
							APP_LOG("UPNP", LOG_DEBUG, "DEVICE: MR. COFFEE");
							g_eDeviceType = DEVICE_MRCOFFEE;
							g_eDeviceTypeTemp = DEVICE_UNKNOWN;
						}
						else if (PETFEEDER_PRODUCT_ID == Product_ID)
						{
							APP_LOG("UPNP", LOG_DEBUG, "DEVICE: PET FEEDER");
							g_eDeviceType = DEVICE_PETFEEDER;
							g_eDeviceTypeTemp = DEVICE_UNKNOWN;
						}
						else
						{
							APP_LOG("UPNP", LOG_DEBUG, "DEVICE: SMART");
							g_eDeviceType = DEVICE_SMART;
							g_eDeviceTypeTemp = DEVICE_UNKNOWN;
						}
					}
				}
				break;
			#endif /* #ifdef PRODUCT_WeMo_Jarden */

			#ifdef PRODUCT_WeMo_Smart
			case 'S':
				{

						APP_LOG("UPNP", LOG_DEBUG, "DEVICE: SMART");
						g_eDeviceType = DEVICE_SMART;
						g_eDeviceTypeTemp = DEVICE_UNKNOWN;
				}
				break;
			#endif /*PRODUCT_WeMo_Smart*/
						default:
								{
										/* Default device type is Socket/Switch */
										APP_LOG("UPNP", LOG_DEBUG, "DEVICE: Default SOCKET");
										g_eDeviceType = DEVICE_SOCKET;
										g_eDeviceTypeTemp = DEVICE_UNKNOWN;
										APP_LOG("UPNP", LOG_ALERT, "Serial No: %s, DEVICE type: %d and temp type: %d", DevSerial, g_eDeviceType, g_eDeviceTypeTemp);
								}
								break;
				}
		}
}

void serverEnvIPaddrInit(void)
{
		memset(g_serverEnvIPaddr, 0x0, sizeof(g_serverEnvIPaddr));
		memset(g_turnServerEnvIPaddr, 0x0, sizeof(g_turnServerEnvIPaddr));
		g_ServerEnvType = E_SERVERENV_PROD;
}

void initDeviceUPnP()
{
		char *iconVer;

		//- Get device type
		/** Move serial request to top since it is the king element*/
		initSerialRequest();
		GetDeviceType();

		GetMacAddress();
		GetSkuNo();
		GetFirmware();
		GetDeviceFriendlyName();

		/* load icon version */
		iconVer = GetBelkinParameter(ICON_VERSION_KEY);
		if(iconVer && strlen(iconVer))
		{
				gWebIconVersion = atoi(iconVer);
		}
		APP_LOG("UPNP", LOG_DEBUG, "saved icon version: %d", gWebIconVersion);


		//Copy all xml file from etc to MTD
		system("rm -rf /tmp/Belkin_settings/*.xml");

		system("cp -f /sbin/web/* /tmp/Belkin_settings");

		//gautam: update the Insight and LS Makefile to copy Insightsetup.xml and Lightsetup.xml in /sbin/web/ as setup.xml

		//Change some tags of setup.xml binding to device related

		//-Check existence of icon file
		FILE* pFileIcon = fopen("/tmp/Belkin_settings/icon.jpg", "r");
		if (0x00 == pFileIcon)
		{
				//-Icon not existing, copy the factory one
				APP_LOG("UPNP", LOG_DEBUG, "icon not found, using the default one");
				if ((DEVICE_SOCKET == g_eDeviceType) || (DEVICE_CROCKPOT == g_eDeviceType))
				{
						//gautam: update the Insight and LS Makefile to copy Insight.png and Light.png in /etc/ as icon.jpg
						system("cp /etc/icon.jpg /tmp/Belkin_settings");
				}
				else
				{
						system("cp /etc/sensor.jpg /tmp/Belkin_settings/icon.jpg");
				}
		}
		else
		{
				fclose(pFileIcon);
				pFileIcon = 0x00;
		}

		UpdateXML2Factory();

		initUPnPThreadParam();
		serverEnvIPaddrInit();

		getClientType();
			/* not the ideal place but to make integration transparent for all Applications */
		//initBugsense();
}


int ControlleeDeviceCallbackEventHandler( Upnp_EventType EventType,
				void *Event,
				void *Cookie)
{

		switch (EventType)
		{

				case UPNP_EVENT_SUBSCRIPTION_REQUEST:
						PluginDeviceHandleSubscriptionRequest( (struct Upnp_Subscription_Request *)Event );
						break;

				case UPNP_CONTROL_GET_VAR_REQUEST:
						break;

				case UPNP_CONTROL_ACTION_REQUEST:
						{
								struct Upnp_Action_Request* pEvent = (struct Upnp_Action_Request*)Event;
								CtrleeDeviceHandleActionRequest(pEvent);
						}
						break;

				case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
				case UPNP_DISCOVERY_SEARCH_RESULT:
				case UPNP_DISCOVERY_SEARCH_TIMEOUT:
				case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
				case UPNP_CONTROL_ACTION_COMPLETE:
				case UPNP_CONTROL_GET_VAR_COMPLETE:
				case UPNP_EVENT_RECEIVED:
				case UPNP_EVENT_RENEWAL_COMPLETE:
				case UPNP_EVENT_SUBSCRIBE_COMPLETE:
				case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
						break;

				default:
						APP_LOG("UPNP",LOG_ERR, "Error in ControlleeDeviceCallbackEventHandler: unknown event type %d\n",
										EventType );
		}

		return (0);
}

void FreeActionRequestSource(struct Upnp_Action_Request* pEvent)
{
		if (0x00 == pEvent)
				return;

		if (pEvent->SoapHeader)
				ixmlDocument_free(pEvent->SoapHeader);
		if (pEvent->ActionRequest)
				ixmlDocument_free(pEvent->ActionRequest);
		if (pEvent->ActionResult)
				ixmlDocument_free(pEvent->ActionResult);

}

#ifndef PRODUCT_WeMo_LEDLight
int CtrleeDeviceHandleActionRequest(struct Upnp_Action_Request *pActionRequest)
{
		int rect = UPNP_E_SUCCESS;
		int loop = 0x00;
		char *errorString = NULL;

		if (0x00 == pActionRequest || !pActionRequest->DevUDN || 0x00 == strlen(pActionRequest->DevUDN))
		{
				APP_LOG("UPNP", LOG_ERR, "Parameters error");
				return 0x01;
		}

		for (loop = 0x00; loop < PLUGIN_MAX_SERVICES; loop++)
		{
				//- to locate service containing this command
				if (0x00 == strcmp(pActionRequest->DevUDN, SocketDevice.service_table[loop].UDN) &&
								0x00 == strcmp(pActionRequest->ServiceID, SocketDevice.service_table[loop].ServiceId))
				{
						break;
				}
		}

		if (PLUGIN_MAX_SERVICES == loop)
		{
				APP_LOG("UPNP",LOG_ERR, "Action service not found: %s", pActionRequest->ServiceID);
				rect = UPNP_E_INVALID_SERVICE;
				return rect;
		}

		//- Service found, and to locate the action name and callback function
		{
				int cntActionNo = SocketDevice.service_table[loop].cntTableSize;
				int index = 0x00;

				if (0x00 == cntActionNo)
				{
						APP_LOG("UPNP",LOG_ERR, "No device action found in actions table");
						return rect;
				}


				for (index = 0x00; index < cntActionNo; index++)
				{
						PluginDeviceUpnpAction* pTable = SocketDevice.service_table[loop].ActionTable;
						if (0x00 != pTable)
						{
								if (0x00 == strcmp(pActionRequest->ActionName, (pTable + index)->actionName))
								{
										APP_LOG("UPNP",LOG_DEBUG, "Action found: %s", pActionRequest->ActionName);
										break;
								}
						}
						else
						{
								APP_LOG("UPNP",LOG_ERR, "Action table not set");
								return rect;
						}
				}

				if (cntActionNo == index)
				{
						APP_LOG("UPNP",LOG_ERR, "Action not found: %s", pActionRequest->ActionName);
						return 0x01;
				}

				{
						PluginDeviceUpnpAction* pAction = SocketDevice.service_table[loop].ActionTable + index;
						if (0x00 == pAction)
						{
								APP_LOG("UPNP",LOG_ERR, "Action entry empty");
								return 0x01;
						}

						if (pAction->pUpnpAction)
								pAction->pUpnpAction(pActionRequest, pActionRequest->ActionRequest, &pActionRequest->ActionResult, (const char **)&errorString);
						else
								APP_LOG("UPNP", LOG_WARNING, "Action name found: %s, but callback entry not set", pActionRequest->ActionName);
				}


		}

		//- Get service type

		return rect;

}
#else
int CtrleeDeviceHandleActionRequest(struct Upnp_Action_Request *pActionRequest)
{
		int loop = 0;
		int action_found = 0;
		int retVal = UPNP_E_SUCCESS;
		char *errorString = NULL;

		if (0x00 == pActionRequest || 0x00 == pActionRequest->ActionRequest || 0x00 == strlen(pActionRequest->DevUDN))
		{
				//It should be returned as SOAP Failure Error...
				APP_LOG("UPNP", LOG_ERR, "Parameters error");
				retVal = UPNP_E_INVALID_SERVICE;
		}

		if( UPNP_E_SUCCESS == retVal )
		{
				for (loop = 0x00; loop < PLUGIN_MAX_SERVICES; loop++)
				{
						//- to locate service containing this command
						if (0 == strcmp(pActionRequest->DevUDN, SocketDevice.service_table[loop].UDN) &&
								0 == strcmp(pActionRequest->ServiceID, SocketDevice.service_table[loop].ServiceId))
						{
								//- Service found, and to locate the action name and callback function
								int index = 0;
								int cntActionNo = SocketDevice.service_table[loop].cntTableSize;

								PluginDeviceUpnpAction* pTable = SocketDevice.service_table[loop].ActionTable;

								if ( pTable )
								{
										for (index = 0; index < cntActionNo; index++)
										{
												if (0 == strcmp(pActionRequest->ActionName, (pTable + index)->actionName))
												{
														APP_LOG("UPNP",LOG_DEBUG, "Action found: %s", pActionRequest->ActionName);

														PluginDeviceUpnpAction* pAction = SocketDevice.service_table[loop].ActionTable + index;

														if (pAction && pAction->pUpnpAction)
														{
																retVal = pAction->pUpnpAction(pActionRequest, pActionRequest->ActionRequest,
																				&pActionRequest->ActionResult, (const char **)&errorString);
																action_found = 1;
														}
														break;
												}
										}

										if( action_found )
												break;
								}
								else
								{
										break;
								}
						}
				}
		}

		if( !action_found )
		{
				//Invalid Action
				pActionRequest->ErrCode = 401;
				strncpy(pActionRequest->ErrStr, "Invalid Action", sizeof(pActionRequest->ErrStr));
		}
		else
		{
				if( retVal == UPNP_E_SUCCESS )
				{
						pActionRequest->ErrCode = UPNP_E_SUCCESS;
				}
				else
				{
						if( errorString )
						{
								//It should be returned as UPnP Error format.
								APP_LOG("UPNP", LOG_ERR, "Action %s: Error = %d, %s", pActionRequest->ActionName, retVal, errorString);
								switch( retVal )
								{
										case UPNP_E_INVALID_PARAM:
												pActionRequest->ErrCode = 402;
												break;
										case UPNP_E_INVALID_ARGUMENT:
												pActionRequest->ErrCode = 600;
												break;
										case UPNP_E_INTERNAL_ERROR:
												pActionRequest->ErrCode = 501;
												break;
										default:
												pActionRequest->ErrCode = retVal;
												break;
								}
								strncpy(pActionRequest->ErrStr, errorString, sizeof(pActionRequest->ErrStr));
						}
						else
						{
								//The below code does not need if action command returns the correct value.
								//But currently many action commands are returning the value with incorrect value.
								pActionRequest->ErrCode = UPNP_E_SUCCESS;
						}
				}
		}

		//- Get service type
		return pActionRequest->ErrCode;
}
#endif


int ControlleeDeviceStop()
{

		int ret=-1;
		if (-1 == device_handle)
			return 0x00;

		ret = UpnpUnRegisterRootDevice(device_handle);
		if( (UPNP_E_SUCCESS != ret) && (UPNP_E_FINISH != ret) )
		{
				APP_LOG("UPNP", LOG_DEBUG,"################### Wemo App going to be reset, ret:%d ###################", ret);
				resetSystem();
		}

		device_handle = -1;
		UpnpFinish();

		APP_LOG("UPNP", LOG_DEBUG, "UPNP is to stop for setup");

		return 0x00;
}

static struct UpnpVirtualDirCallbacks VirtualCallBack;

int ControlleeDeviceStart(char *ip_address,
				unsigned short port,
				char *desc_doc_name,
				char *web_dir_path)
{
		int ret = UPNP_E_SUCCESS;
		char desc_doc_url[MAX_FW_URL_LEN];

		//shutdown the previous instance of UPNP, if any
		ControlleeDeviceStop();


		memset(g_server_ip, 0x00, sizeof(g_server_ip));
		g_server_port = 0x00;

		VirtualCallBack.open 		= (void *)&OpenWebFile;
		VirtualCallBack.get_info 	= &GetFileInfo;
		VirtualCallBack.write	  	= (void *)&PostFile;
		VirtualCallBack.read	 	= 0x00;
		VirtualCallBack.seek	 	= 0x00;
		VirtualCallBack.close	 	= 0x00;

		if(strcmp(ip_address, WAN_IP_ADR) == 0)
				port = 49152;
		else
				port = 49153;

		APP_LOG("UPNP",LOG_DEBUG, "Initializing UPnP Sdk with ipaddress = %s port = %u", ip_address, port);
                ret = UpnpInit( ip_address, port, g_szUDN_1);
		if( ( ret != UPNP_E_SUCCESS ) && ( ret != UPNP_E_INIT ) )
		{
				APP_LOG("UPNP",LOG_CRIT, "Error with UpnpInit -- %d\n", ret );
				UpnpFinish();
				return ret;
		}

		ip_address = UpnpGetServerIpAddress();

		strncpy(g_server_ip, ip_address, sizeof(g_server_ip)-1);

		port = g_server_port = UpnpGetServerPort();

		APP_LOG("UPNP",LOG_CRIT, "UPnP Initialized ipaddress= %s port = %u",
						ip_address, port );

		if( desc_doc_name == NULL ) {
				//gautam: update the Insight and LS Makefile to copy Insightsetup.xml and Lightsetup.xml in /sbin/web/ as setup.xml
				desc_doc_name = "setup.xml";

		}

		if( web_dir_path == NULL ) {
				web_dir_path = DEFAULT_WEB_DIR;
		}

		snprintf( desc_doc_url, MAX_FW_URL_LEN, "http://%s:%d/%s", ip_address, port, desc_doc_name );

		UpdateXML2Factory();
		AsyncSaveData();

		APP_LOG("UPNP",LOG_DEBUG, "Specifying the webserver root directory -- %s\n",
						web_dir_path );
		if( ( ret =
								UpnpSetWebServerRootDir( web_dir_path ) ) != UPNP_E_SUCCESS ) {
				APP_LOG("UPNP",LOG_ERR, "Error specifying webserver root directory -- %s: %d\n",
								web_dir_path, ret );
				UpnpFinish();
				return ret;
		}

		APP_LOG("UPNP",LOG_DEBUG,
						"Registering the RootDevice\n"
						"\t with desc_doc_url: %s\n",
						desc_doc_url );


		UpnpEnableWebserver(TRUE);
		ret = UpnpSetVirtualDirCallbacks(&VirtualCallBack);

		if (ret == UPNP_E_SUCCESS)
		{
				APP_LOG("UPNP", LOG_DEBUG, "UpnpSetVirtualDirCallbacks: success");
		}
		else
		{
				APP_LOG("UPNP", LOG_ERR, "UpnpSetVirtualDirCallbacks failure");
		}

		ret = UpnpAddVirtualDir("./");
		if (UPNP_E_SUCCESS != ret)
		{
				APP_LOG("UPNP", LOG_ERR, "Add virtual directory failure");
		}
		else
		{
				APP_LOG("UPNP", LOG_DEBUG, "Add virtual directory success");
		}


		if( ( ret = UpnpRegisterRootDevice( desc_doc_url,
										ControlleeDeviceCallbackEventHandler,
										&device_handle, &device_handle ) )
						!= UPNP_E_SUCCESS ) {
				APP_LOG("UPNP",LOG_ERR, "Error registering the rootdevice : %d\n", ret );
				UpnpFinish();
				return ret;
		}
		else
		{
                                APP_LOG("UPNP",LOG_DEBUG, "RootDevice Registered with device_handle:%d\nInitializing State Table\n", device_handle);
				ControlleeDeviceStateTableInit(desc_doc_url);
				APP_LOG("UPNP",LOG_DEBUG, "State Table Initialized\n");

				if( ( ret =
										UpnpSendAdvertisement( device_handle, default_advr_expire ) )
								!= UPNP_E_SUCCESS ) {
						APP_LOG("UPNP",LOG_ERR, "Error sending advertisements : %d\n", ret );
						UpnpFinish();
						return ret;
				}

				APP_LOG("UPNP",LOG_DEBUG, "Advertisements Sent\n");
		}

		return UPNP_E_SUCCESS;
}

int ControlleeDeviceStateTableInit(char *DescDocURL)
{
		IXML_Document *DescDoc = NULL;
		int ret = UPNP_E_SUCCESS;
		char *servid = NULL;
		char *evnturl = NULL;
		char *ctrlurl = NULL;
		char *udn = NULL;

		/*Download description document */
		if (UpnpDownloadXmlDoc(DescDocURL, &DescDoc) != UPNP_E_SUCCESS)
		{
				APP_LOG("UPNP",LOG_DEBUG, "Controllee device table initialization -- Error Parsing %s\n",
								DescDocURL);
				ret = UPNP_E_INVALID_DESC;
				ixmlDocument_free(DescDoc);

				return ret;
		}
		else
		{
				APP_LOG("UPNP",LOG_DEBUG, "Down load %s success", DescDocURL);
		}

		udn = Util_GetFirstDocumentItem(DescDoc, "UDN");
		memset(g_szUDN, 0x00, sizeof(g_szUDN));

		if (udn)
		{
				APP_LOG("UPNP",LOG_DEBUG, "UDN: %s\n", udn);
				strncpy(g_szUDN, udn, sizeof(g_szUDN)-1);
		}
		else
		{
				APP_LOG("UPNP",LOG_ERR, "UDN: reading failure");
		}

		//-Add setup service here
		if (!Util_FindAndParseService(DescDoc, DescDocURL, CtrleeDeviceServiceType[PLUGIN_E_SETUP_SERVICE], &servid, &evnturl, &ctrlurl))
		{
				APP_LOG("UPNP",LOG_ERR, "%s -- Error: Could not find Service: %s\n", __FILE__, CtrleeDeviceServiceType[PLUGIN_E_SETUP_SERVICE]);

				ret = UPNP_E_INVALID_DESC;
				goto FreeServiceResource;
		}
		else
		{
				CtrleeDeviceSetServiceTable(PLUGIN_E_SETUP_SERVICE, udn, servid,
								CtrleeDeviceServiceType[PLUGIN_E_SETUP_SERVICE],
								&SocketDevice.service_table[PLUGIN_E_SETUP_SERVICE]);
		}

		FreeXmlSource(servid);
		FreeXmlSource(evnturl);
		FreeXmlSource(ctrlurl);
		servid = NULL;
		evnturl = NULL;
		ctrlurl = NULL;


		//- Add Sync time service
		if (!Util_FindAndParseService(DescDoc, DescDocURL, CtrleeDeviceServiceType[PLUGIN_E_TIME_SYNC_SERVICE], &servid, &evnturl, &ctrlurl))
		{
				APP_LOG("UPNP",LOG_ERR, "%s -- Error: Could not find Service: %s\n", __FILE__, CtrleeDeviceServiceType[PLUGIN_E_TIME_SYNC_SERVICE]);

				ret = UPNP_E_INVALID_DESC;
				goto FreeServiceResource;
		}
		else
		{
				CtrleeDeviceSetServiceTable(PLUGIN_E_TIME_SYNC_SERVICE, udn, servid,
								CtrleeDeviceServiceType[PLUGIN_E_TIME_SYNC_SERVICE],
								&SocketDevice.service_table[PLUGIN_E_TIME_SYNC_SERVICE]);

		}

		FreeXmlSource(servid);
		FreeXmlSource(evnturl);
		FreeXmlSource(ctrlurl);
		servid = NULL;
		evnturl = NULL;
		ctrlurl = NULL;

		//- Add basic event service
		if (!Util_FindAndParseService(DescDoc, DescDocURL, CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], &servid, &evnturl, &ctrlurl))
		{
				APP_LOG("UPNP",LOG_ERR, "%s -- Error: Could not find Service: %s\n", __FILE__, CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE]);

		ret = UPNP_E_INVALID_DESC;
		goto FreeServiceResource;
	}
	else
	{
		CtrleeDeviceSetServiceTable(PLUGIN_E_EVENT_SERVICE, udn, servid,
				CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],
				&SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE]);
	}
	FreeXmlSource(servid);
	FreeXmlSource(evnturl);
	FreeXmlSource(ctrlurl);
#ifdef PRODUCT_WeMo_Smart
	servid = NULL;
	evnturl = NULL;
	ctrlurl = NULL;

   //- Add device event service
	if (!Util_FindAndParseService(DescDoc, DescDocURL, CtrleeDeviceServiceType[PLUGIN_E_DEVICE_EVENT_SERVICE], &servid, &evnturl, &ctrlurl))
	{
		APP_LOG("UPNP",LOG_ERR, "%s -- Error: Could not find Service: %s\n", __FILE__, CtrleeDeviceServiceType[PLUGIN_E_DEVICE_EVENT_SERVICE]);

		ret = UPNP_E_INVALID_DESC;
		goto FreeServiceResource;
	}
	else
	{
		CtrleeDeviceSetServiceTable(PLUGIN_E_DEVICE_EVENT_SERVICE, udn, servid,
				CtrleeDeviceServiceType[PLUGIN_E_DEVICE_EVENT_SERVICE],
				&SocketDevice.service_table[PLUGIN_E_DEVICE_EVENT_SERVICE]);
	}
	FreeXmlSource(servid);
	FreeXmlSource(evnturl);
	FreeXmlSource(ctrlurl);
#endif
	servid = NULL;
	evnturl = NULL;
	ctrlurl = NULL;

		//- Add firmware update service
		if (!Util_FindAndParseService(DescDoc, DescDocURL, CtrleeDeviceServiceType[PLUGIN_E_FIRMWARE_SERVICE], &servid, &evnturl, &ctrlurl))
		{
				APP_LOG("UPNP",LOG_ERR, "%s -- Error: Could not find Service: %s\n", __FILE__, CtrleeDeviceServiceType[PLUGIN_E_FIRMWARE_SERVICE]);

				ret = UPNP_E_INVALID_DESC;
				goto FreeServiceResource;


				return ret;
		}
		else
		{
				CtrleeDeviceSetServiceTable(PLUGIN_E_FIRMWARE_SERVICE, udn, servid,
								CtrleeDeviceServiceType[PLUGIN_E_FIRMWARE_SERVICE],
								&SocketDevice.service_table[PLUGIN_E_FIRMWARE_SERVICE]);
		}
		FreeXmlSource(servid);
		FreeXmlSource(evnturl);
		FreeXmlSource(ctrlurl);

		servid = NULL;
		evnturl = NULL;
		ctrlurl = NULL;
		//- Add rule service
		if (!Util_FindAndParseService(DescDoc, DescDocURL, CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE], &servid, &evnturl, &ctrlurl))
		{
				APP_LOG("UPNP",LOG_ERR, "%s -- Error: Could not find Service: %s\n", __FILE__, CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE]);

				ret = UPNP_E_INVALID_DESC;
				goto FreeServiceResource;


				return ret;
		}
		else
		{
				CtrleeDeviceSetServiceTable(PLUGIN_E_RULES_SERVICE, udn, servid,
								CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE],
								&SocketDevice.service_table[PLUGIN_E_RULES_SERVICE]);
		}

		FreeXmlSource(servid);
		FreeXmlSource(evnturl);
		FreeXmlSource(ctrlurl);

		servid = NULL;
		evnturl = NULL;
		ctrlurl = NULL;

		//-Add remote access service here
		if (!Util_FindAndParseService(DescDoc, DescDocURL, CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE], &servid, &evnturl, &ctrlurl))
		{
				APP_LOG("UPNP",LOG_ERR, "%s -- Error: Could not find Service: %s\n", __FILE__, CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE]);

				ret = UPNP_E_INVALID_DESC;
				goto FreeServiceResource;


				return ret;
		}
		else
		{
				CtrleeDeviceSetServiceTable(PLUGIN_E_REMOTE_ACCESS_SERVICE, udn, servid,
								CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],
								&SocketDevice.service_table[PLUGIN_E_REMOTE_ACCESS_SERVICE]);
		}

		FreeXmlSource(servid);
		FreeXmlSource(evnturl);
		FreeXmlSource(ctrlurl);


		servid = NULL;
		evnturl = NULL;
		ctrlurl = NULL;
		//- Add Meta service here
		if (!Util_FindAndParseService(DescDoc, DescDocURL, CtrleeDeviceServiceType[PLUGIN_E_METAINFO_SERVICE], &servid, &evnturl, &ctrlurl))
		{
				APP_LOG("UPNP",LOG_ERR, "%s -- Error: Could not find Service: %s\n", __FILE__, CtrleeDeviceServiceType[PLUGIN_E_METAINFO_SERVICE]);

				ret = UPNP_E_INVALID_DESC;
				goto FreeServiceResource;


				return ret;
		}
		else
		{
				CtrleeDeviceSetServiceTable(PLUGIN_E_METAINFO_SERVICE, udn, servid,
								CtrleeDeviceServiceType[PLUGIN_E_METAINFO_SERVICE],
								&SocketDevice.service_table[PLUGIN_E_METAINFO_SERVICE]);
		}

		FreeXmlSource(servid);
	    FreeXmlSource(evnturl);
		FreeXmlSource(ctrlurl);
		servid = NULL;
		evnturl = NULL;
		ctrlurl = NULL;

#ifdef PRODUCT_WeMo_Insight
		//- Add Insight service here
		if (!Util_FindAndParseService(DescDoc, DescDocURL, CtrleeDeviceServiceType[PLUGIN_E_INSIGHT_SERVICE], &servid, &evnturl, &ctrlurl))
		{
				APP_LOG("UPNP",LOG_ERR, "%s -- Error: Could not find Service: %s\n", __FILE__, CtrleeDeviceServiceType[PLUGIN_E_INSIGHT_SERVICE]);

				ret = UPNP_E_INVALID_DESC;
				goto FreeServiceResource;


				return ret;
		}
		else
		{
				CtrleeDeviceSetServiceTable(PLUGIN_E_INSIGHT_SERVICE, udn, servid,
								CtrleeDeviceServiceType[PLUGIN_E_INSIGHT_SERVICE],
								&SocketDevice.service_table[PLUGIN_E_INSIGHT_SERVICE]);
		}

		FreeXmlSource(servid);
		FreeXmlSource(evnturl);
		FreeXmlSource(ctrlurl);
		servid = NULL;
		evnturl = NULL;
		ctrlurl = NULL;
#endif

#ifdef PRODUCT_WeMo_LEDLight
		//- Add Bridge device information service here
		if (!Util_FindAndParseService(DescDoc, DescDocURL, CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE], &servid, &evnturl, &ctrlurl))
		{
				APP_LOG("UPNP",LOG_ERR, "%s -- Error: Could not find Service: %s\n", __FILE__, CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE]);

				ret = UPNP_E_INVALID_DESC;
				goto FreeServiceResource;
		}
		else
		{
				CtrleeDeviceSetServiceTable(PLUGIN_E_BRIDGE_SERVICE, udn, servid,
								CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
								&SocketDevice.service_table[PLUGIN_E_BRIDGE_SERVICE]);
		}

		FreeXmlSource(servid);
		FreeXmlSource(evnturl);
		FreeXmlSource(ctrlurl);
		servid = NULL;
		evnturl = NULL;
		ctrlurl = NULL;
#endif

		//- Add Device Information service here
		if (!Util_FindAndParseService(DescDoc, DescDocURL, CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], &servid, &evnturl, &ctrlurl))
		{
				APP_LOG("UPNP",LOG_ERR, "%s -- Error: Could not find Service: %s\n", __FILE__, CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE]);

				ret = UPNP_E_INVALID_DESC;
				goto FreeServiceResource;


				return ret;
		}
		else
		{
				CtrleeDeviceSetServiceTable(PLUGIN_E_DEVICEINFO_SERVICE, udn, servid,
								CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE],
								&SocketDevice.service_table[PLUGIN_E_DEVICEINFO_SERVICE]);
		}

		FreeXmlSource(servid);
		FreeXmlSource(evnturl);
		FreeXmlSource(ctrlurl);
		servid = NULL;
		evnturl = NULL;
		ctrlurl = NULL;

#ifdef WeMo_SMART_SETUP
		//- Add smart setup service here
		if (!Util_FindAndParseService(DescDoc, DescDocURL, CtrleeDeviceServiceType[PLUGIN_E_SMART_SETUP_SERVICE], &servid, &evnturl, &ctrlurl))
		{
				APP_LOG("UPNP",LOG_ERR, "%s -- Error: Could not find Service: %s\n", __FILE__, CtrleeDeviceServiceType[PLUGIN_E_SMART_SETUP_SERVICE]);

				ret = UPNP_E_INVALID_DESC;
				goto FreeServiceResource;


				return ret;
		}
		else
		{
				CtrleeDeviceSetServiceTable(PLUGIN_E_SMART_SETUP_SERVICE, udn, servid,
								CtrleeDeviceServiceType[PLUGIN_E_SMART_SETUP_SERVICE],
								&SocketDevice.service_table[PLUGIN_E_SMART_SETUP_SERVICE]);
		}
#endif

//- Add Manufacture Information service here
  if (!Util_FindAndParseService(DescDoc, DescDocURL, CtrleeDeviceServiceType[PLUGIN_E_MANUFACTURE_SERVICE], &servid, &evnturl, &ctrlurl))
  {
    APP_LOG("UPNP",LOG_ERR, "%s -- Error: Could not find Service: %s\n", __FILE__, CtrleeDeviceServiceType[PLUGIN_E_MANUFACTURE_SERVICE]);

    ret = UPNP_E_INVALID_DESC;
    goto FreeServiceResource;
  }
  else
  {
    CtrleeDeviceSetServiceTable(PLUGIN_E_MANUFACTURE_SERVICE, udn, servid,
        CtrleeDeviceServiceType[PLUGIN_E_MANUFACTURE_SERVICE],
        &SocketDevice.service_table[PLUGIN_E_MANUFACTURE_SERVICE]);
  }

FreeServiceResource:
		{
				ixmlDocument_free(DescDoc);
				FreeXmlSource(servid);
				FreeXmlSource(evnturl);
				FreeXmlSource(ctrlurl);
				FreeXmlSource(udn);

				servid = NULL;
				evnturl = NULL;
				ctrlurl = NULL;
		}


		return ret;
}

int CtrleeDeviceSetServiceTable(int serviceType,
				const char* UDN,
				const char* serviceId,
				const char* szServiceType,
				pPluginService pService)
{
		strncpy(pService->UDN, UDN, sizeof(pService->UDN)-1);
		strncpy(pService->ServiceId, serviceId, sizeof(pService->ServiceId)-1);
		strncpy(pService->ServiceType, szServiceType, sizeof(pService->ServiceType)-1);

		CtrleeDeviceSetActionTable(serviceType, &SocketDevice.service_table[serviceType]);

		return 0x00;
}

int PluginDeviceHandleSubscriptionRequest(IN struct Upnp_Subscription_Request *sr_event)
{
    static const char *binstate_true  = "1";
    static const char *binstate_false = "0";

    if (!sr_event)
    {
        APP_LOG("UPNPDevice", LOG_ERR,"Service subscription: parameter error, request stop");
        return 0x01;
    }

    /* These 2 variables are defined differently depending on the
     * product:
     * - Paramters(sic) gets different initialization data.
     * - values[] has varying length (1 or 2) and different sized item(s)
     */

#define PARAMS_BINARY_STATE {"BinaryState"}
#if defined(PRODUCT_WeMo_Jarden)
#  define PARAMS_INIT {"mode", "time"}
#  define VALUES_NUM_ITEMS (2)
#  define VALUE_1_SIZE (4)
#  define VALUE_2_SIZE (4)
#elif defined (PRODUCT_WeMo_Smart)
#  define PARAMS_INIT {"ParamString"};
#  define VALUES_NUM_ITEMS (1)
#  define VALUE_1_SIZE (4)
#elif defined (PRODUCT_WeMo_Insight)
#  define PARAMS_INIT PARAMS_BINARY_STATE
#  define VALUES_NUM_ITEMS (1)
#  define VALUE_1_SIZE (100)
#else
#  define PARAMS_INIT PARAMS_BINARY_STATE
#  define VALUE_1_SIZE (4)
#  define VALUES_NUM_ITEMS (1)
#endif  // defined(PRODUCT_WeMo_Jarden)

    char* paramters[] = PARAMS_INIT;
    char* values[VALUES_NUM_ITEMS];
    values[0] = CALLOC( 1, VALUE_1_SIZE );
 #  if VALUES_NUM_ITEMS > 1
    values[1] = CALLOC( 1, VALUE_2_SIZE );
 #  endif  // VALUES_NUM_ITEMS > 1

    int curState = 0x00;
 #ifdef PRODUCT_WeMo_Jarden
    int retStatus = 1;
    int mode=0, time=0, tmpMode=0;
    unsigned short int cookedTime=0;
#else
    char* firmwareParamters[] = {"FirmwareUpdateStatus"};
    char* pBridgeEvents[] = {"StatusChange", "SubDeviceFWUpdateStatus"};
    char* pBridgeValues[2];
    pBridgeValues[0] = (char*)MALLOC(SIZE_256B);
    pBridgeValues[1] = (char*)MALLOC(SIZE_2B);
#endif /*#ifdef PRODUCT_WeMo_Jarden */

    if (DEVICE_SOCKET == g_eDeviceType)
    {
        //- Power state
        LockLED();
        curState = GetCurBinaryState();
        UnlockLED();
    }
    #ifdef PRODUCT_WeMo_Jarden
    else if(DEVICE_CROCKPOT == g_eDeviceType)
    {
        curState = GetJardenStatusRemote(&mode,&time, &cookedTime);
        APP_LOG("UPNP",LOG_DEBUG, "curState received is %d",curState);
        APP_LOG("UPNP",LOG_DEBUG, "mode received is %d",mode);
        APP_LOG("UPNP",LOG_DEBUG, "cookTime received is %d",time);

        snprintf(values[0], SIZE_4B, "%d", mode);
        APP_LOG("UPNP",LOG_DEBUG, "mode being set is %s",values[0]);
        snprintf(values[1], SIZE_4B, "%d", time);
        APP_LOG("UPNP",LOG_DEBUG, "time being set is %s",values[1]);
    }
    else if( (DEVICE_SBIRON == g_eDeviceType) ||
             (DEVICE_MRCOFFEE == g_eDeviceType) ||
             (DEVICE_PETFEEDER == g_eDeviceType)
           )
    {
        LockLED();
        curState = GetCurBinaryState();
        UnlockLED();

        /* Requires this when Jarden complete functionality implemented */
        /*curState = GetJardenStatusRemote(&mode,&time);
        sprintf(values[0], "%d", mode);
        sprintf(values[1], "%d", time);*/
    }
    #endif /* #ifdef PRODUCT_WeMo_Jarden */
    #ifdef PRODUCT_WeMo_Smart
    else if (DEVICE_SMART == g_eDeviceType)
    {
       // Send some value temperorily
       curState = 0x01;
       strncpy( values[0], binstate_true, VALUE_1_SIZE );
    }
    #endif /* #ifdef PRODUCT_WeMo_Smart */
    else
    {
        //-Sensor state
        curState = GetSensorState();
    }
#if !defined(PRODUCT_WeMo_Jarden) || !defined(PRODUCT_WeMo_Smart)
    if (0x01 == curState)
    {
       strncpy( values[0], binstate_true, VALUE_1_SIZE );
    }
#ifdef PRODUCT_WeMo_Insight
        snprintf(values[0], VALUE_1_SIZE, "%d|%u|%u|%u|%u|%u|%u|%u|%u|%0.f",
                 curState, g_StateChangeTS, g_ONFor, g_TodayONTimeTS,
                 g_TotalONTime14Days, g_HrsConnected, g_AvgPowerON,
                 g_PowerNow, g_AccumulatedWattMinute, g_KWH14Days);
#else
        strncpy( values[0],
                 curState == 1 ? binstate_true : binstate_false,
                 VALUE_1_SIZE );
#endif
#endif
    #ifdef PRODUCT_WeMo_Jarden
    if (DEVICE_CROCKPOT == g_eDeviceType)
    {
        APP_LOG("UPNP",LOG_DEBUG, "Bfore UpnpAcceptSubscription");
        retStatus = UpnpAcceptSubscription(device_handle,
                                            sr_event->UDN,
                                            sr_event->ServiceId,
                                            (const char **)paramters,
                                            (const char **)values,
                                            0x02,
                                            sr_event->Sid);
        if(UPNP_E_SUCCESS == retStatus){
            APP_LOG("UPNPDevice", LOG_DEBUG,"Service subscription: UDN- %s serID - %s: success", sr_event->UDN, sr_event->ServiceId);
        }
        else {
            APP_LOG("UPNPDevice", LOG_DEBUG,"Service subscription ERROR %d ####",retStatus);
        }
    }
    else
    #endif /* #ifdef PRODUCT_WeMo_Jarden */
#ifdef PRODUCT_WeMo_LEDLight
    {
        int retStatus = 1;

        if( 0 == strcmp(sr_event->ServiceId, "urn:Belkin:serviceId:basicevent1") )
        {
            retStatus = UpnpAcceptSubscription(device_handle,
                sr_event->UDN,
                sr_event->ServiceId,
                (const char **)paramters,
                (const char **)values,
                sizeof(values)/sizeof(char*),
                sr_event->Sid);
        }
        else if( 0 == strcmp(sr_event->ServiceId, "urn:Belkin:serviceId:bridge1") )
        {
            memset(pBridgeValues[0], 0x00, SIZE_256B);
            memset(pBridgeValues[1], 0x00, SIZE_2B);

            retStatus = UpnpAcceptSubscription(device_handle,
                                      sr_event->UDN,
                                      sr_event->ServiceId,
                                      (const char **)pBridgeEvents,
                                      (const char **)pBridgeValues,
                                      sizeof(pBridgeValues)/sizeof(char*),
                                      sr_event->Sid);
        }
        else if( 0 == strcmp(sr_event->ServiceId, "urn:Belkin:serviceId:firmwareupdate1") )
        {
            sprintf(values[0],"%d",getCurrFWUpdateState());
            retStatus = UpnpAcceptSubscription(device_handle,
                                      sr_event->UDN,
                                      sr_event->ServiceId,
                                      (const char **)firmwareParamters,
                                      (const char **)values,
                                      sizeof(values)/sizeof(char*),
                                      sr_event->Sid);
        }

        if( UPNP_E_SUCCESS == retStatus )
        {
            APP_LOG("UPNPDevice", LOG_DEBUG,"Service subscription: UDN- %s serviceID - %s, Sid - %s: success", sr_event->UDN, sr_event->ServiceId, sr_event->Sid);
        }
        else
        {
            APP_LOG("UPNPDevice", LOG_DEBUG,"Service subscription serviceID - %s ERROR %d ####", sr_event->ServiceId, retStatus);
        }
    }
#else
    {
        UpnpAcceptSubscription(device_handle,
                                       sr_event->UDN,
                                       sr_event->ServiceId,
                                       (const char **)paramters,
                                       (const char **)values,
                                       0x01,
                                       sr_event->Sid);
    }
#endif
    #ifdef PRODUCT_WeMo_Jarden
        free(values[0x00]);
        free(values[0x01]);
    #else
        free(values[0x00]);
        free(pBridgeValues[0]);
        free(pBridgeValues[1]);
    #endif

        APP_LOG("UPNPDevice", LOG_DEBUG,"Service subscription: %s: success", sr_event->ServiceId);

#ifdef PRODUCT_WeMo_LEDLight
      if (strstr(sr_event->ServiceId, "bridge")) {
          UPnPTimeSyncStatusNotify();
      }
#else
      if (strstr(sr_event->ServiceId, "basicevent"))
      {
         {
             if(g_OldApp)
             {
                UPnPTimeSyncStatusNotify();
                g_OldApp=0;
             }
         }
#ifdef PRODUCT_WeMo_Insight
         APP_LOG("UPNPDevice", LOG_DEBUG,"INSIGHT HOME SETTINGS NOTIFY...");
         //send notification on get request
         SendHomeSettingChangeMsg();
#endif
      }
#endif
      if (strstr(sr_event->ServiceId, "remoteaccess"))
      {
          APP_LOG("UPNPDevice", LOG_DEBUG,"##################### REMOTEACCESS SUBSCRIBED...");
          {
              if ((0x00 != strlen(g_szHomeId) && 0x00 == atoi(g_szRestoreState))) //notify only if self remote access is enabled
              {
                  APP_LOG("UPNPDevice", LOG_DEBUG,"##################### sending signature notification...");
                  UPnPSendSignatureNotify();
                  pluginUsleep(1000000);
                  APP_LOG("UPNPDevice", LOG_DEBUG,"##################### sending homeid and deviceid notification...");
                  UPnPSetHomeDeviceIdNotify();
              }
              else
              {
                  APP_LOG("UPNPDevice", LOG_ERR,"################## there is no homeid, private key, not sending homeid, deviceid and signature notifications");
                  APP_LOG("UPNPDevice", LOG_CRIT,"There is no homeid, private key, not sending homeid, deviceid and signature notifications");
              }
          }
      }
      return (0x00);
}

int CtrleeDeviceSetActionTable(PLUGIN_SERVICE_TYPE serviceType, pPluginService pOut)
{
	switch (serviceType)
	{
		case PLUGIN_E_SETUP_SERVICE:
			pOut->ActionTable  = g_Wifi_Setup_Actions;
			pOut->cntTableSize = sizeof(g_Wifi_Setup_Actions)/sizeof(PluginDeviceUpnpAction);
			APP_LOG("UPNP", LOG_DEBUG, "PLUGIN_E_SETUP_SERVICE: service: %s Action Table set: %d", pOut->ServiceType, pOut->cntTableSize);
			break;
		case PLUGIN_E_TIME_SYNC_SERVICE:
			pOut->ActionTable  = g_time_sync_Actions;
			pOut->cntTableSize = sizeof(g_time_sync_Actions)/sizeof(PluginDeviceUpnpAction);
			APP_LOG("UPNP", LOG_DEBUG, "PLUGIN_E_TIME_SYNC_SERVICE: service: %s Action Table set: %d", pOut->ServiceType, pOut->cntTableSize);
			break;
		case PLUGIN_E_EVENT_SERVICE:
			pOut->ActionTable  = g_basic_event_Actions;
			pOut->cntTableSize = sizeof(g_basic_event_Actions)/sizeof(PluginDeviceUpnpAction);
			APP_LOG("UPNP", LOG_DEBUG, "PLUGIN_E_EVENT_SERVICE: service: %s Action Table set: %d", pOut->ServiceType, pOut->cntTableSize);
			break;
#ifdef PRODUCT_WeMo_Smart
        case PLUGIN_E_DEVICE_EVENT_SERVICE:
			pOut->ActionTable  = g_device_event_Actions;
			pOut->cntTableSize = sizeof(g_device_event_Actions)/sizeof(PluginDeviceUpnpAction);
			APP_LOG("UPNP", LOG_DEBUG, "PLUGIN_E_DEVICE_EVENT_SERVICE: service: %s Action Table set: %d", pOut->ServiceType, pOut->cntTableSize);
			break;
#endif
		case PLUGIN_E_FIRMWARE_SERVICE:
			pOut->ActionTable  = g_firmware_event_Actions;
			pOut->cntTableSize = sizeof(g_firmware_event_Actions)/sizeof(PluginDeviceUpnpAction);
			APP_LOG("UPNP", LOG_DEBUG, "PLUGIN_E_FIRMWARE_SERVICE: service: %s Action Table set: %d", pOut->ServiceType, pOut->cntTableSize);
			break;


				case PLUGIN_E_RULES_SERVICE:
						pOut->ActionTable  = g_Rules_Actions;
						pOut->cntTableSize = sizeof(g_Rules_Actions)/sizeof(PluginDeviceUpnpAction);
						APP_LOG("UPNP", LOG_DEBUG, "PLUGIN_E_RULES_SERVICE: service: %s Action Table set: %d", pOut->ServiceType, pOut->cntTableSize);

						break;

				case PLUGIN_E_REMOTE_ACCESS_SERVICE:
						pOut->ActionTable  = g_remote_access_Actions;
						pOut->cntTableSize = sizeof(g_remote_access_Actions)/sizeof(PluginDeviceUpnpAction);
						APP_LOG("UPNP", LOG_DEBUG, "PLUGIN_E_REMOTE_ACCESS_SERVICE: service: %s Action Table set: %d", pOut->ServiceType, pOut->cntTableSize);
						break;


				case PLUGIN_E_METAINFO_SERVICE:
						pOut->ActionTable  = g_metaInfo_Actions;
						pOut->cntTableSize = sizeof(g_metaInfo_Actions)/sizeof(PluginDeviceUpnpAction);
						APP_LOG("UPNP", LOG_DEBUG, "PLUGIN_E_METAINFO_SERVICE: service: %s Action Table set: %d", pOut->ServiceType, pOut->cntTableSize);
						break;

				case PLUGIN_E_DEVICEINFO_SERVICE:
						pOut->ActionTable  = g_deviceInfo_Actions;
						pOut->cntTableSize = sizeof(g_deviceInfo_Actions)/sizeof(PluginDeviceUpnpAction);
						APP_LOG("UPNP", LOG_DEBUG, "PLUGIN_E_DEVICEINFO_SERVICE: service: %s Action Table set: %d", pOut->ServiceType, pOut->cntTableSize);
						break;

				 case PLUGIN_E_MANUFACTURE_SERVICE:
    					pOut->ActionTable 	= g_manufacture_Actions;
    					pOut->cntTableSize 	= sizeof(g_manufacture_Actions)/sizeof(PluginDeviceUpnpAction);
      					APP_LOG("UPNP", LOG_DEBUG, "PLUGIN_E_MANUFACTURE_SERVICE: service: %s Action Table set: %d", pOut->ServiceType, pOut->cntTableSize);
      					break;

#ifdef PRODUCT_WeMo_LEDLight
				case PLUGIN_E_BRIDGE_SERVICE:
						pOut->ActionTable   = g_bridge_Actions;
						pOut->cntTableSize  = sizeof(g_bridge_Actions)/sizeof(PluginDeviceUpnpAction);
						APP_LOG("UPNP", LOG_DEBUG, "PLUGIN_E_BRIDGE_SERVICE: service: %s Action Table set: %d", pOut->ServiceType, pOut->cntTableSize);
						break;
#endif

#ifdef PRODUCT_WeMo_Insight
				case PLUGIN_E_INSIGHT_SERVICE:
						pOut->ActionTable  = g_insight_Actions;
						pOut->cntTableSize = sizeof(g_insight_Actions)/sizeof(PluginDeviceUpnpAction);
						APP_LOG("UPNP", LOG_DEBUG, "PLUGIN_E_INSIGHT_SERVICE: service: %s Action Table set: %d", pOut->ServiceType, pOut->cntTableSize);
						break;
#endif

#ifdef WeMo_SMART_SETUP
				case PLUGIN_E_SMART_SETUP_SERVICE:
						pOut->ActionTable  = g_smart_setup_Actions;
						pOut->cntTableSize = sizeof(g_smart_setup_Actions)/sizeof(PluginDeviceUpnpAction);
						APP_LOG("UPNP", LOG_DEBUG, "PLUGIN_E_SMART_SETUP_SERVICE: service: %s Action Table set: %d", pOut->ServiceType, pOut->cntTableSize);
						break;
#endif
				default:
						APP_LOG("UPNP", LOG_ERR, "WRONG service ID");
      					break;
		}

		return UPNP_E_SUCCESS;
}

/**
 *
 *
 *
 *
 *
 *
 *
 * ************************************************************************/
#define 	AP_LIST_BUFF_SIZE	3*SIZE_1024B

#if 0
void* siteSurveyPeriodic(void *args)
{
		int count=0;

		if (pAvlAPList)
				free(pAvlAPList);

		/* buffer allocated is as per the WIFI driver. Refer: cmm_info.c*/
		/* Memory allocated for 64 entries */
		pAvlAPList = (PMY_SITE_SURVEY) MALLOC(sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE);

		if(!pAvlAPList)
		{
				APP_LOG("UPNPDevice",LOG_ERR,"Malloc Failed...");
				resetSystem();
		}

		while(1) {
				if(g_ra0DownFlag == 1)
						break;

				osUtilsGetLock(&gSiteSurveyStateLock);
				memset(pAvlAPList, 0x0, sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE);
				getCurrentAPList (pAvlAPList, &count);
				g_cntApListNumber = count;
				osUtilsReleaseLock(&gSiteSurveyStateLock);

				pluginUsleep(10000000); //10 secs
		}

		if (pAvlAPList)
				free(pAvlAPList);
		pAvlAPList = 0x00;

		APP_LOG("UPNPDevice",LOG_DEBUG,"******* SiteSurvey Thread exiting ************");
		pthread_exit(0);
}
#endif

#ifdef WeMo_INSTACONNECT
extern pthread_t connInsta_thread;
int g_connectInstaStatus = APCLI_UNCONFIGURED;
int g_usualSetup = 0x00;
extern WIFI_PAIR_PARAMS gWiFiParams;
extern char gRouterSSID[SSID_LEN];

void GetBridgeTrailSerial(char* szSerialNo, char *szBuff)
{
		if ((0x00 == szSerialNo) || (0x00 == strlen(szSerialNo)))
		{
				szSerialNo = DEFAULT_SERIAL_NO;
		}

		//strncat(szBuff, szSerialNo + strlen(szSerialNo) - DEFAULT_SERIAL_TAILER_SIZE, MAX_APSSID_LEN);
		strncat(szBuff, szSerialNo + strlen(szSerialNo) - DEFAULT_SERIAL_TAILER_SIZE, DEFAULT_SERIAL_TAILER_SIZE);

		//printf("APSSID: %s\n", szBuff);
}

void NotifyApOpen()
{
		pMessage msg = createMessage(NETWORK_AP_OPEN_UPNP, 0x00, 0x00);
		SendMessage2App(msg);
}
void* connectToInstaApTask(void *args)
{
		int i = 0, count = 0, found = 0;
		PMY_SITE_SURVEY pAvlNetwork=NULL;
		int chan=0;
		int ret=0;
		int connectInstaStatus = 0x00;  //un configured

		pluginUsleep(5000000); //5 secs to defer it from site survey thread
		while(1)
		{
				/* 0x01 - means connecthomenetwork received from app on ra0... so do nothing meanwhile */
				if(g_connectInstaStatus == APCLI_CONFIGURING)
				{
						pluginUsleep(1000000);
						continue;
				}

				/* 0x02 - means connecthomenetwork succeeded... so exit*/
				if(g_connectInstaStatus == APCLI_CONFIGURED)
						break;

				/* connect Insta Status = 0x01 - configuring */
				if(connectInstaStatus == 0x01)
				{
						if(isAPConnected(INSTA_AP_SSID) < SUCCESS)
						{
								APP_LOG("UPNP", LOG_ERR,"Insta AP disconnected...undo settings");
								undoInstaApSettings();
								NotifyApOpen();
								connectInstaStatus = 0x00;
								found = 0;
								pluginUsleep(5000000);	// additional 5 secs to let upnp to switch over to local mode
						}
						pluginUsleep(5000000);  //5 secs to check for insta AP connection status
						continue;
				}

				osUtilsGetLock(&gSiteSurveyStateLock);
				count = g_cntApListNumber;
				pAvlNetwork = pAvlAPList;

				if(!pAvlNetwork)
				{
						APP_LOG("UPNP", LOG_ERR,"No AP List....");
						osUtilsReleaseLock(&gSiteSurveyStateLock);
						pluginUsleep(1000000);
						continue;
				}

				APP_LOG("UPNP", LOG_DEBUG,"Avl network list cnt: %d", count);
				for (i=0;i<count;i++)
				{
						if (!strcmp (pAvlNetwork[i].ssid, INSTA_AP_SSID))
						{

								APP_LOG("UPNP", LOG_DEBUG,
												"pAvlNetwork[%d].channel: %s,pAvlNetwork[%d].ssid: %s",
												i, pAvlNetwork[i].channel, i, pAvlNetwork[i].ssid);
								found = 1;
								APP_LOG("WiFiApp", LOG_DEBUG, "WEMO INSTA AP found from SITE SURVEY");
								chan = atoi((const char *)pAvlNetwork[i].channel);
								break;
						}
				}
				osUtilsReleaseLock(&gSiteSurveyStateLock);
				if(found)
				{
						ret = connectToInstaAP(chan);
						if (ret >= SUCCESS)
						{
								//APP_LOG("WiFiApp", LOG_DEBUG, "Connection to Insta AP successful, break from main loop");
								//break;
								APP_LOG("WiFiApp", LOG_DEBUG, "Connection to Insta AP successful, now check for configuring state");
								connectInstaStatus = 0x01;  //configuring
								continue;
						}
						else
						{
								pluginUsleep(2000000);
								APP_LOG("UPNP", LOG_ERR,"connection to Insta Ap failed....retry");
								found = 0;
								continue;
						}
				}
				else
						pluginUsleep(10000000); //10 secs
		}

		APP_LOG("UPNPDevice",LOG_DEBUG,"******* connect to Insta AP Thread exiting ************");
		connInsta_thread = -1;
		pthread_exit(0);
}

void GetTrailSerial(char *szBuff)
{
		char* szSerialNo = GetSerialNumber();
		if ((0x00 == szSerialNo) || (0x00 == strlen(szSerialNo)))
		{
				szSerialNo = DEFAULT_SERIAL_NO;
		}

		//printf("serialNumber: %s\n", szSerialNo);
		//strncat(szBuff, szSerialNo + strlen(szSerialNo) - DEFAULT_SERIAL_TAILER_SIZE, MAX_APSSID_LEN);
		strncat(szBuff, szSerialNo + strlen(szSerialNo) - DEFAULT_SERIAL_TAILER_SIZE, DEFAULT_SERIAL_TAILER_SIZE);

		//printf("APSSID: %s\n", szBuff);
}
#endif

int GetApList(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char szAplistBuffer[AP_LIST_BUFF_SIZE];
		char szApEntry[MAX_RESP_LEN];
		int i=0, count=0;
		int listCnt=0;
		PMY_SITE_SURVEY pAvlAPList = NULL;

		memset(szAplistBuffer, 0x0, AP_LIST_BUFF_SIZE);

		pAvlAPList = (PMY_SITE_SURVEY) ZALLOC(sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE);
		if(!pAvlAPList)
		{
				APP_LOG("UPNPDevice",LOG_ERR,"Malloc Failed...");
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x01;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetApList", CtrleeDeviceServiceType[PLUGIN_E_SETUP_SERVICE],"ApList", "FAILURE");

				return FAILURE;
		}

		APP_LOG("UPNPDevice",LOG_DEBUG,"Get List...");
		getCurrentAPList (pAvlAPList, &count);

		listCnt = count;

		for (i=0; i < count; i++)
		{
				if((strstr(pAvlAPList[i].ssid, ",") != NULL) || (strstr(pAvlAPList[i].ssid, "|") != NULL))
				{
						listCnt--;
						APP_LOG("UPNP: DEVICE",LOG_DEBUG, "Updated listcnt: %d for SSID: %s", listCnt, pAvlAPList[i].ssid);
				}
		}

		APP_LOG("UPNP: DEVICE",LOG_DEBUG, "count: %d, listcnt: %d\n", count, listCnt);

		snprintf(szAplistBuffer, sizeof(szAplistBuffer), "Page:1/1/%d$\n", listCnt);

		for (i=0; i < count; i++)
		{
				if((strstr(pAvlAPList[i].ssid, ",") == NULL) && (strstr(pAvlAPList[i].ssid, "|") == NULL))
				{
						memset(szApEntry, 0x00, sizeof(szApEntry));

						snprintf(szApEntry, sizeof(szApEntry), "%s|%d|%d|%s,\n",
										pAvlAPList[i].ssid,
										atoi((const char *)pAvlAPList[i].channel),
										atoi((const char *)pAvlAPList[i].signal),
										pAvlAPList[i].security
										);
						strncat(szAplistBuffer, szApEntry, sizeof(szAplistBuffer)-strlen(szAplistBuffer)-1);
				}
				else
						APP_LOG("UPNP: DEVICE",LOG_DEBUG, "Skipping entry %d for SSID: %s", i, pAvlAPList[i].ssid);
		}

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetApList", CtrleeDeviceServiceType[PLUGIN_E_SETUP_SERVICE],"ApList", szAplistBuffer);

		APP_LOG("UPNP: DEVICE",LOG_DEBUG, "%s\n", szAplistBuffer);

		if(pAvlAPList)
		{
				free(pAvlAPList);
				pAvlAPList = NULL;
		}

		return UPNP_E_SUCCESS;
}

int GetNetworkList(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char szAplistBuffer[AP_LIST_BUFF_SIZE];
		char szApEntry[MAX_RESP_LEN];
		int i=0, ssidLen = 0;
		int count=0;
		PMY_SITE_SURVEY pAvlAPList = NULL;

		memset(szAplistBuffer, 0x0, AP_LIST_BUFF_SIZE);

		pAvlAPList = (PMY_SITE_SURVEY) ZALLOC(sizeof(MY_SITE_SURVEY)*MAX_LEN_OF_BSS_TABLE);
		if(!pAvlAPList)
		{
				APP_LOG("UPNPDevice",LOG_ERR,"Malloc Failed...");
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x01;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetNetworkList", CtrleeDeviceServiceType[PLUGIN_E_SETUP_SERVICE],"NetworkList", "FAILURE");

				return FAILURE;
		}
		APP_LOG("UPNPDevice",LOG_DEBUG,"Get Network List...");
		getCurrentAPList (pAvlAPList, &count);

		snprintf(szAplistBuffer, sizeof(szAplistBuffer), "Page:1/1/%d$\n", count);

		for (i=0; i < count; i++)
		{
				memset(szApEntry, 0x00, sizeof(szApEntry));

				ssidLen = strlen(pAvlAPList[i].ssid);
				snprintf(szApEntry, sizeof(szApEntry), "%d|%s|%d|%d|%s|\n",
								ssidLen,
								pAvlAPList[i].ssid,
								atoi((const char *)pAvlAPList[i].channel),
								atoi((const char *)pAvlAPList[i].signal),
								pAvlAPList[i].security
								);
				strncat(szAplistBuffer, szApEntry, sizeof(szAplistBuffer)-strlen(szAplistBuffer)-1);
		}

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetNetworkList", CtrleeDeviceServiceType[PLUGIN_E_SETUP_SERVICE],"NetworkList", szAplistBuffer);

		APP_LOG("UPNP: DEVICE",LOG_DEBUG, "%s\n", szAplistBuffer);

		if(pAvlAPList)
		{
				free(pAvlAPList);
				pAvlAPList = NULL;
		}

		return UPNP_E_SUCCESS;
}



/*
 * Unsets the u-boot env variable 'boot_A_args'.
 *
 * Will work only on the OPENWRT boards.
 * Gemtek boards follow different procedure to handle the scenario.
 *
 * Done for the Story: 2187, To restore the state of the switch before power failure
 */
#ifdef _OPENWRT_
void correctUbootParams() {
		char sysString[SIZE_128B];
		int bootArgsLen = 0;

		memset(sysString, '\0', SIZE_128B);
		bootArgsLen = strlen(g_szBootArgs);

		if(bootArgsLen) {
				snprintf(sysString, sizeof(sysString), "fw_setenv boot_A_args %s", g_szBootArgs);
				system(sysString);
		}
}
#endif

void AsyncControlleeDeviceStop()
{
    pMessage msg = createMessage(META_CONTROLLEE_DEVICE_STOP, 0x00, 0x00);
    SendMessage2App(msg);
}

void* resetThread(void *arg)
{
		int resetType = *(int *)arg;
                tu_set_my_thread_name( __FUNCTION__ );

		free(arg);

		switch(resetType)
		{
				case META_SOFT_RESET:
						APP_LOG("ResetThread", LOG_ALERT, "Processing META_SOFT_RESET");
#ifdef PRODUCT_WeMo_Insight
						ClearUsageData();
#endif
						resetToDefaults();
      APP_LOG("UPNP",LOG_DEBUG, "***resetThread:ClearRuleFromFlash()***\n");
						ClearRuleFromFlash();
						UpdateXML2Factory();
						AsyncSaveData();

			#ifdef PRODUCT_WeMo_Jarden
			ClearJardenDataUsage();
			#endif
			break;

		case META_FULL_RESET:
			APP_LOG("ResetThread", LOG_ALERT, "Processing META_FULL_RESET");
#ifdef _OPENWRT_
						correctUbootParams();
#endif
#ifdef __ORESETUP__
						/* Remove saved IP from flash */
						UnSetBelkinParameter ("wemo_ipaddr");
						/* Unsetting IconVersion */
						UnSetBelkinParameter (ICON_VERSION_KEY);
						gWebIconVersion=0;
						StopInetTask();
						AsyncControlleeDeviceStop();
						setRemoteRestoreParam(0x1);
						remotePostDeRegister(NULL, META_FULL_RESET);
#else
						StopInetTask();
						AsyncControlleeDeviceStop();
						setRemoteRestoreParam(0x1);
						remotePostDeRegister(NULL, META_FULL_RESET);
						/* Remove saved IP from flash */
						UnSetBelkinParameter ("wemo_ipaddr");
						/* Unsetting IconVersion */
						UnSetBelkinParameter (ICON_VERSION_KEY);
						gWebIconVersion=0;

#endif
						pluginUsleep(CONTROLLEE_DEVICE_STOP_WAIT_TIME);
						ResetToFactoryDefault(0);
			#ifdef PRODUCT_WeMo_Jarden
			/* Remove Device ID from NVRAM */
			UnSetBelkinParameter (DEFAULT_SMART_DEV_ID);
			memset(szDeviceID, 0x00, sizeof(szDeviceID));
			UnSetBelkinParameter (SLOW_COOKER_START_TIME);
			memset(g_poweronStartTime, 0x00, sizeof(g_poweronStartTime));

			ClearJardenDataUsage();

			system("echo 1 > /proc/WASP_RESET");  //To Reset Crockpot device using gpio pin
			#endif /* #ifdef PRODUCT_WeMo_Jarden */

			#ifdef PRODUCT_WeMo_Smart
			/* Remove Device ID from NVRAM */
			UnSetBelkinParameter (DEFAULT_SMART_DEV_ID);
			memset(szDeviceID, 0x00, sizeof(szDeviceID));
			UnSetBelkinParameter (DEFAULT_SMART_DEV_NAME);
			memset(g_DevName, 0x00, sizeof(g_DevName));
			system("echo 1 > /proc/WASP_RESET");  //To Reset smart device using gpio pin
			#endif /* #ifdef PRODUCT_WeMo_Smart */
						break;

				case META_REMOTE_RESET:
						APP_LOG("ResetThread", LOG_ALERT, "Processing META_REMOTE_RESET");
						UnSetBelkinParameter (DEFAULT_HOME_ID);
						memset(g_szHomeId, 0x00, sizeof(g_szHomeId));
						UnSetBelkinParameter (DEFAULT_PLUGIN_PRIVATE_KEY);
						memset(g_szPluginPrivatekey, 0x00, sizeof(g_szPluginPrivatekey));
			#ifdef PRODUCT_WeMo_Jarden
			UnSetBelkinParameter (DEFAULT_SMART_DEV_ID);
			memset(szDeviceID, 0x00, sizeof(szDeviceID));
			UnSetBelkinParameter (SLOW_COOKER_START_TIME);
			memset(g_poweronStartTime, 0x00, sizeof(g_poweronStartTime));
			#endif /* #ifdef PRODUCT_WeMo_Jarden */

			#ifdef PRODUCT_WeMo_Smart
			UnSetBelkinParameter (DEFAULT_SMART_DEV_ID);
			memset(szDeviceID, 0x00, sizeof(szDeviceID));
			UnSetBelkinParameter (DEFAULT_SMART_DEV_NAME);
			memset(g_DevName, 0x00, sizeof(g_DevName));
			#endif /* #ifdef PRODUCT_WeMo_Smart */
						UnSetBelkinParameter (RESTORE_PARAM);
						memset(g_szRestoreState, 0x0, sizeof(g_szRestoreState));
						gpluginRemAccessEnable = 0;
						/* server environment settings cleanup and nat client destroy */

						APP_LOG("ResetThread",LOG_DEBUG, "Entry Re-Initialize nat client on ENV change");
						while(1)
						{
								APP_LOG("ResetThread",LOG_DEBUG, "Inside while Re-Initialize nat client on ENV change");
								APP_LOG("ResetThread",LOG_HIDE, "gpluginRemAccessEnable = %d\ng_szHomeId = %s\n", gpluginRemAccessEnable, g_szHomeId);
								sleep(5);
								if (gpluginRemAccessEnable && strlen(g_szHomeId))
								{
										APP_LOG("ResetThread",LOG_DEBUG, "Re-Initialize nat client on ENV change");
										UDS_invokeNatReInit(NAT_REINIT);
										APP_LOG("ResetThread",LOG_DEBUG, "Done! Re-Initialize nat client on ENV change");
										break;
								}
						}
						APP_LOG("ResetThread",LOG_CRIT, "Exit Re-Initialize nat client on ENV change");

						SaveSetting();
						break;
#ifdef PRODUCT_WeMo_Insight
				case META_CLEAR_USAGE:
						APP_LOG("ResetThread", LOG_ALERT, "Processing META_CLEAR_USAGE");
						ClearUsageData();
						break;
#endif
				case META_WIFI_SETTING_RESET:
						APP_LOG("ResetThread", LOG_ALERT, "Processing META_WIFI_SETTING_RESET");
						StopInetTask();
						ResetWiFiSetting();
						break;

		}
		reset_thread = -1;
		pthread_exit(0);
		return 0;
}

void ResetWiFiSetting(void)
{
		APP_LOG("ResetWiFiSetting:", LOG_DEBUG, "Reset WiFi Settings");
		/* Remove saved IP from flash */
		UnSetBelkinParameter ("wemo_ipaddr");
		AsyncControlleeDeviceStop();
		setRemoteRestoreParam(0x1);
		SetBelkinParameter(WIFI_CLIENT_SSID,"");
		APP_LOG("ResetButtonTask:", LOG_DEBUG, "Going To Self Reboot...");
		pluginUsleep(CONTROLLEE_DEVICE_STOP_WAIT_TIME);
		system("reboot");
}

int ExecuteReset(int resetIndex)
{
		int *resetType = (int *)MALLOC(sizeof(int));
		int retVal;

		if(!resetType)
		{
				APP_LOG("UPnP: Device",LOG_ERR, "Memory could not be allocated for resetType");
				return SUCCESS;
		}

		if(0x01 == resetIndex)
				*resetType = META_SOFT_RESET;
		else if(0x02 == resetIndex)
				*resetType = META_FULL_RESET;
		else if(0x03 == resetIndex)
				*resetType = META_REMOTE_RESET;
#ifdef PRODUCT_WeMo_Insight
		else if(0x04 == resetIndex)
				*resetType = META_CLEAR_USAGE;
#endif
		else if(0x05 == resetIndex)
				*resetType = META_WIFI_SETTING_RESET;
		else
				return FAILURE;

		/* first of all remove the reset thread, if running */
		if(reset_thread != -1)
		{
				if((retVal = pthread_kill(reset_thread, SIGRTMIN)) == 0)
				{
						reset_thread = -1;
						APP_LOG("UPnP: Device",LOG_DEBUG,"reset thread removed successfully....");
				}
				else
				{
						APP_LOG("UPnP: Device",LOG_ERR,"reset thread removal failed [%d] ....",retVal);
				}
		}
		else
				APP_LOG("UPnP: Device",LOG_DEBUG,"reset thread doesn't exist. Creating reset thread....");

		pthread_attr_init(&reset_attr);
		pthread_attr_setdetachstate(&reset_attr,PTHREAD_CREATE_DETACHED);
		retVal = pthread_create(&reset_thread,&reset_attr,
						(void*)&resetThread, (void *)resetType);

		if(retVal < SUCCESS) {
				APP_LOG("UPnP: Device",LOG_CRIT, "RESET Thread not created");
				return SUCCESS;
		}

		return SUCCESS;
}

#ifndef __ORESETUP__
pthread_mutex_t g_remoteDeReg_mutex;
pthread_cond_t g_remoteDeReg_cond;
#define WAIT_DREG_TIMEOUT	10
#endif

int ReSetup(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		int retVal = UPNP_E_SUCCESS;

		APP_LOG("UPNPDevice: ReSetup", LOG_DEBUG, "%s", __FUNCTION__);

		if (0x00 == pActionRequest || 0x00 == request)
		{
				return 0x01;
		}
		char* paramValue = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Reset");

		if (paramValue)
				APP_LOG("UPNPDevice", LOG_DEBUG, "trying reset plugin to: %s", paramValue);

		int resettype = atoi(paramValue);

#ifdef __ORESETUP__
		/* Clear Rules info */
		if(resettype == 0x01)
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x00;

				UpnpAddToActionResponse(&pActionRequest->ActionResult, "ReSetup",
								CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "Reset", "success");

				APP_LOG("UPNPDevice", LOG_DEBUG, "Reset Plugin-> (Mem Partitions) Rules:  done: %d", resettype);

				ExecuteReset(0x01);
		}
		/* Clear All info */
		else if(resettype == 0x02)
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = SUCCESS;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "ReSetup",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "Reset", "success");
				ExecuteReset(0x02);
		}
		/* Clear Remote info */
		else if(resettype == 0x03)
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = SUCCESS;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "ReSetup",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "Reset", "success");
				ExecuteReset(0x03);
		}
#ifdef PRODUCT_WeMo_Insight
		else if(resettype == 0x04)
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = SUCCESS;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "ReSetup",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "Reset", "success");
				ExecuteReset(0x04);
		}
#endif
		else if(resettype == 0x05)
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = SUCCESS;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "ReSetup",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "Reset", "success");
				ExecuteReset(0x05);
		}
		else
		{
				APP_LOG("UPNPDevice", LOG_ERR, "Reset Plugin not done: %d", resettype);

				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = PLUGIN_ERROR_E_BASIC_EVENT;

				UpnpAddToActionResponse(&pActionRequest->ActionResult, "ReSetup", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "Reset", "unsuccess");
		}
#else
		retVal = ExecuteReset(resettype);
		if (retVal < SUCCESS) {
				APP_LOG("UPNPDevice", LOG_ERR, "Reset Plugin not done: %d", resettype);
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = PLUGIN_ERROR_E_BASIC_EVENT;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "ReSetup", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "Reset", "unsuccess");
		}else {
				//Wait on posix condition timed wait for 5 seconds max if resettype is 2 only
				struct timeval tmv;
				struct timespec tsv;
				if (resettype == 0x02) {
						APP_LOG("REMOTEACCESS", LOG_DEBUG, "In reset_remote type %d\n", resettype);
						/* Convert from timeval to timespec */
						gettimeofday(&tmv, NULL);
						tsv.tv_sec  = tmv.tv_sec;
						tsv.tv_nsec = tmv.tv_usec * 1000;
						tsv.tv_sec += WAIT_DREG_TIMEOUT;
						/* wait here for response */
						pthread_mutex_lock(&g_remoteDeReg_mutex);
						retVal = pthread_cond_timedwait(&g_remoteDeReg_cond, &g_remoteDeReg_mutex, &tsv);
						pthread_mutex_unlock(&g_remoteDeReg_mutex);
						//check whether de-reg successfully happened
						retVal = remoteGetDeRegStatus(0);
						APP_LOG("REMOTEACCESS", LOG_DEBUG, "The status Code got is %d\n", retVal);
						if (retVal == 2) {
								pActionRequest->ActionResult = NULL;
								pActionRequest->ErrCode = SUCCESS;
								UpnpAddToActionResponse(&pActionRequest->ActionResult, "ReSetup",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "Reset", "reset_remote");
								goto retStatus;
						}
				}
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = SUCCESS;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "ReSetup",CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "Reset", "success");
		}
retStatus:
#endif
		return UPNP_E_SUCCESS;
}

void StopPowerMonitorTimer()
{
		int ret = 0x01;
		if (-1 != ithPowerSensorMonitorTask)
		{
				ret = ithread_cancel(ithPowerSensorMonitorTask);

				if (0x00 != ret)
				{
						APP_LOG("UPNP: Rule", LOG_DEBUG, "######################## ithread_cancel: Can not stop power monitor task thread ##############################");
				}

				ithPowerSensorMonitorTask = -1;

		}
}

void UpdatePowerMonitorTimer(int duration, int endAction)
{
		int ret = 0x01;

  if (0x00 == duration)
    return;
		sPowerDuration  = duration;
		sPowerEndAction = endAction;

		if (-1 == ithPowerSensorMonitorTask)
		{
			ret = ithread_create(&ithPowerSensorMonitorTask, 0x00, PowerSensorMonitorTask, 0x00);
			if (0x00 == ret)
			{
				ret = ithread_detach(ithPowerSensorMonitorTask);
				if (0x00 != ret)
				{
					APP_LOG("UPNP: Rule", LOG_DEBUG, "######################## ithread_detach: Can not detach power monitor task thread ##############################");
				}
			}
			else
			{
				APP_LOG("UPNP: Rule", LOG_DEBUG, "######################## ithread_create: Can not create power monitor task thread ##############################");
				resetSystem();
			}
		}
		else
		{
			APP_LOG("UPNP: sensor rule", LOG_DEBUG, "Sensor event, monitoring thread running until %d seconds:", sPowerDuration);
		}
}

void UPnPActionUpdate(int curState)
{
		pMessage msg = 0x00;

		if (0x00 == curState)
		{
				msg = createMessage(UPNP_ACTION_MESSAGE_OFF_IND, 0x00, 0x00);
		}
		else if (0x01 == curState)
		{
				msg = createMessage(UPNP_ACTION_MESSAGE_ON_IND, 0x00, 0x00);
		}

		SendMessage2App(msg);
}


void UPnPInternalToggleUpdate(int curState)
{
		pMessage msg = 0x00;
		if (0x00 == curState)
		{
				msg = createMessage(UPNP_MESSAGE_OFF_IND, 0x00, 0x00);
		}
		else if (0x01 == curState)
		{
				msg = createMessage(UPNP_MESSAGE_ON_IND, 0x00, 0x00);
		}
#ifdef PRODUCT_WeMo_Insight
		else if (POWER_SBY == curState)
		{
				msg = createMessage(UPNP_MESSAGE_SBY_IND, 0x00, 0x00);
		}
#endif

		//gautam:  Relay thread does nothing but sends it to main thread: unnecessary latency
		SendMessage2App(msg);
}
int changeDeviceState(int startAction, int endAction, int duration ,char *szCurState,char *szCdState)
{
		int toState  = startAction;
		APP_LOG("UPNPDevice", LOG_DEBUG, "to request state change to %d", toState);


#ifndef PRODUCT_WeMo_Smart
		int ret = 0x01;
        	int countdownRuleLastMinStatus = 0;
#endif

		/* sensor can send the same state as current state but App should not be allowed to do so */
		if(((0 <= duration) && (0 <= endAction)) || (toState != g_PowerStatus)) 
		{
			/* Sensor trigger */
			/*get contdown timer last minute running state*/
			countdownRuleLastMinStatus =  gCountdownRuleInLastMinute;
#ifdef PRODUCT_WeMo_Insight
			int tempInsightState = toState;
			if (POWER_ON == tempInsightState)
			{
				tempInsightState = POWER_SBY;
				APP_LOG ("DEVICE:rule", LOG_DEBUG, "tempInsightState =  %d",
						tempInsightState);
			}
			executeCountdownRule (tempInsightState);
#else
#ifndef PRODUCT_WeMo_LEDLight
			/*this API will start/restart/stop countdown timer dependng upon if coundown timer is not_running/runing_in_last_minute/running_not_in_last_ minute*/
			executeCountdownRule(toState);
#endif
#endif
			/*check if it was running in last minute, if yes do not toggle relay, countdown timer restarted*/
			if(gRuleHandle[e_COUNTDOWN_RULE].ruleCnt && countdownRuleLastMinStatus)
			{
				APP_LOG("Button", LOG_DEBUG, "Countdown timer was in last minute, Do not toggle!");
			}
			else
			{
				ret = ChangeBinaryState(toState);
			}
		}

		char* szStateOuput = toState?"ON":"OFF";

#ifdef PRODUCT_WeMo_Insight
		if(toState == 1){
				//g_ONFor=0;
				//g_ONForChangeFlag = 1;
				toState = POWER_SBY;
				APP_LOG("UPNPDevice", LOG_DEBUG, "******Changed ON State To %d", toState);
		}
#endif
		if (0x00 == ret)
		{
			UPnPInternalToggleUpdate(toState);
#ifdef PRODUCT_WeMo_Insight
			if (g_InitialMonthlyEstKWH){
				snprintf(szCurState, SIZE_100B, "%d|%u|%u|%u|%u|%u|%u|%u|%u|%d",
						toState,g_StateChangeTS,g_ONFor,g_TodayONTimeTS ,g_TotalONTime14Days,g_HrsConnected,g_AvgPowerON,g_PowerNow,g_AccumulatedWattMinute,g_InitialMonthlyEstKWH);
			}
			else{
				snprintf(szCurState, SIZE_100B, "%d|%u|%u|%u|%u|%u|%u|%u|%u|%0.f",
						toState,g_StateChangeTS,g_ONFor,g_TodayONTimeTS ,g_TotalONTime14Days,g_HrsConnected,g_AvgPowerON,g_PowerNow,g_AccumulatedWattMinute,g_KWH14Days);
			}
			APP_LOG("UPNP", LOG_DEBUG, "Local Binary State Insight Parameters: %s", szCurState);
#else
			snprintf (szCurState, sizeof (szCurState), "%d", toState);
			APP_LOG ("UPNP", LOG_DEBUG, "Local Binary State Parameters: %s",
					szCurState);
#endif

			APP_LOG ("UPNPDevice", LOG_DEBUG, "State changed, current state: %d", toState);

			/* Countdown Time Notification */
			snprintf (szCdState, sizeof (szCdState), "%lu",
					gCountdownEndTime);
			APP_LOG ("UPNP", LOG_DEBUG, "Countdown Time: %s", szCdState);

		/* Device current time */
                memset (szCurState, 0, sizeof (szCurState));
                snprintf (szCurState, sizeof (szCurState), "%lu",
                          GetUTCTime());

                APP_LOG ("UPNP", LOG_DEBUG, "Countdown Time: %s", szCurState);

		}
		else
		{
#ifdef PRODUCT_WeMo_Insight
				if (g_InitialMonthlyEstKWH){
						snprintf(szCurState, SIZE_100B, "%d|%u|%u|%u|%u|%u|%u|%u|%u|%d",
										toState,g_StateChangeTS,g_ONFor,g_TodayONTimeTS ,g_TotalONTime14Days,g_HrsConnected,g_AvgPowerON,g_PowerNow,g_AccumulatedWattMinute,g_InitialMonthlyEstKWH);
				}
				else{
						snprintf(szCurState, SIZE_100B, "%d|%u|%u|%u|%u|%u|%u|%u|%u|%0.f",
										toState,g_StateChangeTS,g_ONFor,g_TodayONTimeTS ,g_TotalONTime14Days,g_HrsConnected,g_AvgPowerON,g_PowerNow,g_AccumulatedWattMinute,g_KWH14Days);
				}
				APP_LOG("UPNP", LOG_DEBUG, "Local Binary State Insight Parameters on failure: %s", szCurState);
#else
				snprintf(szCurState, sizeof(szCurState), "%s", "Error");
#endif

			/* WEMO-33379: Error response should be sent even when countdown rule is in last minute */
			if (gRuleHandle[e_COUNTDOWN_RULE].ruleCnt &&
					countdownRuleLastMinStatus)
			{
				APP_LOG ("UPNPDevice", LOG_DEBUG,
						"Countdown timer was in last minute");
			}
			APP_LOG("UPNPDevice", LOG_ERR, "State not changed, current state: %s", szStateOuput);
		}
		/*Duration value -1 and 0 are Invalid, For on then immediately off duration is one.
		*/
		if ((0 < duration))
		{
				//- JIRA: WEMO-4605: to check the ON logic from user and 8029
#ifdef PRODUCT_WeMo_Insight
				if ((IsLastUserActionOn()) &&((toState == POWER_ON) ||(toState == POWER_SBY)))
#else
						if((IsLastUserActionOn()) && (toState == POWER_ON))
#endif
						{
								APP_LOG("Rule", LOG_DEBUG, "Last user action to ON, no timer");
								StopPowerMonitorTimer();
								//- do nothing here
						}
						else
						{
							APP_LOG("UPnP: Sensor rule", LOG_DEBUG, "duration: %d, endAction: %d", duration, endAction);

							if(duration)
							{
								APP_LOG("UPNPDevice", LOG_DEBUG, "Sensor event, create management thread");
								UpdatePowerMonitorTimer(duration, endAction);
							}
						}
		}
		else
		{
				//- Action from phone, so notify the sensor
				SetLastUserActionOnState(toState);
				if (0x00 == ret)
				{
						StopPowerMonitorTimer();

						//- I am not sure I have a sensor rule or not
						//LocalUserActionNotify(toState + 2);
#ifdef PRODUCT_WeMo_Insight
						//UPnPActionUpdate(g_PowerStatus); //To-Do in Insight Rules Stories.
#else
						//UPnPActionUpdate(toState);
#endif
				}
		}


}

int SetBinaryState(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{

		int duration = -1, endAction = -1;
		if (0x00 == pActionRequest || 0x00 == request)
		{
				APP_LOG("UPNPDevice", LOG_DEBUG, "SetBinaryState: command paramter invalid");
				return 0x01;
		}


		if (DEVICE_SENSOR == g_eDeviceType)
		{
				APP_LOG("UPNPDevice", LOG_ERR, "Sensor device, command not support");

				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x01;

				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetBinaryState",
								CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "BinaryState", "unsuccess");

				return 0x00;
		}

		char* paramValue = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "BinaryState");
		char* paramDuration = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Duration");
		char* paramEndAction = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "EndAction");


		if (0x00 == paramValue || 0x00 == strlen(paramValue))
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x01;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetBinaryState",
								CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "BinaryState", "Error");

				return 0x00;
		}

		if ((0x00 != paramDuration) && (0x00 != strlen(paramDuration)) && (0x00 != paramEndAction) && (0x00 != strlen(paramEndAction)))
		{
			/* Sensor triggered */
			APP_LOG("UPNPDevice", LOG_DEBUG, "Sensor trigger");
			setActuation(ACTUATION_SENSOR_RULE);
			duration = atoi(paramDuration);
			endAction = atoi(paramEndAction);
		}
		else
		{
			/* App triggered */
			APP_LOG("UPNPDevice", LOG_DEBUG, "App trigger");
			setActuation(ACTUATION_MANUAL_APP);
			setRemote("0");
		}
		
#ifdef PRODUCT_WeMo_Insight
                char szCurState[SIZE_100B];
#else
                char szCurState[SIZE_32B];
#endif
		char szCdState[SIZE_32B];
		char szCurTime[SIZE_32B];
		memset(szCurState, 0x00, sizeof(szCurState));
		memset(szCdState, 0x00, sizeof(szCdState));
		memset(szCurTime, 0x00, sizeof(szCurTime));
		
		changeDeviceState(atoi(paramValue),endAction,duration,szCurState,szCdState);
			
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetBinaryState",
				CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "BinaryState", szCurState);

		if(strlen(szCdState))
			UpnpAddToActionResponse (&pActionRequest->ActionResult,
					"SetBinaryState",
					CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],
					"CountdownEndTime", szCdState);


		snprintf(szCurTime, SIZE_32B, "%lu", GetUTCTime());
                UpnpAddToActionResponse (&pActionRequest->ActionResult,
                                         "SetBinaryState",
                               CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],
                                         "deviceCurrentTime", szCurTime);
		FreeXmlSource(paramValue);
		FreeXmlSource(paramDuration);
		FreeXmlSource(paramEndAction);

		return UPNP_E_SUCCESS;
}


#ifdef PRODUCT_WeMo_Smart
int SetBlobStorage(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	mxml_node_t *tree;
	mxml_node_t *attribute_node;
	mxml_node_t *blobNameNode, *blobValueNode;
	char *ptempBlobXMLData = NULL;
	char *pblobXMLData = NULL;
	char *pValue;
	char *pName;
	char retValStr[SIZE_32B] = {0};
	int retVal = PLUGIN_SUCCESS, blobXMLDataLen = 0;

	if(NULL == pActionRequest || NULL == request)
	{
		APP_LOG("UPNP", LOG_ERR, "SetBlobStorage command paramter invalid");
		return SET_PARAMS_NULL;
	}

	/*get xml data under attributeList tag*/
	ptempBlobXMLData = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "attributeList");
	if((NULL == ptempBlobXMLData) || (0x00 == strlen(ptempBlobXMLData)))
	{
		APP_LOG("UPNP", LOG_ERR, "parameter error: attr data null");
		retVal = PLUGIN_FAILURE;
		goto out;
	}

	APP_LOG("UPNP", LOG_DEBUG, "Blob XML Data received:%s", ptempBlobXMLData);

	/*calculate xml length i.e xml received + xml header lenght*/
	blobXMLDataLen = strlen(ptempBlobXMLData) + SIZE_128B;

	/*allocate memory for new xml*/
	pblobXMLData = (char*)CALLOC(1, blobXMLDataLen);
	if(!pblobXMLData)
	{
		APP_LOG("UPNP", LOG_DEBUG, "memory allocation failed");
		resetSystem();
	}

	/*add header to received xml*/
	snprintf(pblobXMLData, blobXMLDataLen, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>%s", ptempBlobXMLData);
	APP_LOG("UPNP", LOG_DEBUG, "modified Blob XML Data is:%s", pblobXMLData);

	/*load xml tree*/
	tree = mxmlLoadString(NULL, pblobXMLData, MXML_OPAQUE_CALLBACK);
	if(tree)
	{
		/*parse all blob labels present in the xml*/
		for(attribute_node = mxmlFindElement(tree, tree, "attribute", NULL, NULL, MXML_DESCEND);
                                        attribute_node != NULL;
                                        attribute_node = mxmlFindElement(attribute_node, tree, "attribute", NULL, NULL, MXML_DESCEND))
		{
			/*attribute present, get name and value tag pointers*/
			blobNameNode = mxmlFindElement(attribute_node, tree, "name", NULL, NULL, MXML_DESCEND);
			blobValueNode = mxmlFindElement(attribute_node, tree, "value", NULL, NULL, MXML_DESCEND);

			/*check for valid name and value tag pointers*/
			if(blobNameNode && blobValueNode)
			{
				/*get blob label and its value*/
				pName = (char *)mxmlGetOpaque(blobNameNode);
				pValue = (char *)mxmlGetOpaque(blobValueNode);

				/*check for valid blob lable and value pointers and length*/
				if(pName && strlen(pName) && pValue && strlen(pValue))
				{
					APP_LOG("UPNP", LOG_DEBUG, "SetBlobStorage: Label Name is %s, value is %s", pName,pValue);
					/*update this blob storage*/
					updateBlobStorageData(pName, pValue, BLOB_WRITE);
				}
			}
		}
		/*free loaded tree*/
		mxmlDelete(tree);
	}
	else
	{
		APP_LOG("UPNP", LOG_ERR, "Could not load tree");
		retVal = PLUGIN_FAILURE;
	}

out:
	/*return response code it would be -1 on failure or 0 on success*/
	snprintf(retValStr, sizeof(retVal), "%d", retVal);

	/*send UPnP action response*/
	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = retVal;
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetBlobStorage", CtrleeDeviceServiceType[PLUGIN_E_DEVICE_EVENT_SERVICE], "attributeList", retValStr);
	APP_LOG("UPNP", LOG_DEBUG, "SetBlobStorage: send UPnP response:%s", retValStr);

	/*free memory*/
	if(pblobXMLData){free(pblobXMLData); pblobXMLData=NULL;}
	FreeXmlSource(ptempBlobXMLData);
	return retVal;
}

#define CDATA_DELIM "![CDATA["
#define CDATA_DELIM_SLEN 8
#define CDATA_DELIM_ELEN 2
/*pSmartParamName is IN argument, pSmartParamValue is IN/OUT argument, flagReadWrite is 0/1 read/write operation*/
int updateBlobStorageData(char *pSmartParamName, char *pSmartParamValue, int flagReadWrite)
{
	int retVal = PLUGIN_SUCCESS;
	char file_path[SIZE_128B]={0,};
	FILE* s_fpBlobHandle = NULL;

	/*check for valid arguments*/
	if((NULL == pSmartParamName) || (NULL == pSmartParamValue))
	{
		APP_LOG("updateBlobStorageData", LOG_ERR, "Invalid argument received");
		return PLUGIN_FAILURE;
	}

	/*create valid file path*/
	snprintf(file_path,sizeof(file_path),"/tmp/Belkin_settings/%s.txt",pSmartParamName);

	/*open file as per the mode read/write received*/
	if(BLOB_READ == flagReadWrite)
		s_fpBlobHandle = fopen(file_path, "r");
	else
		s_fpBlobHandle = fopen(file_path, "w");

	/*check valid file handle*/
	if (!s_fpBlobHandle)
	{
		APP_LOG("updateBlobStorageData", LOG_ERR, "Blob Storage file open failed in mode:%d, file at path:%s", flagReadWrite,file_path);
		return PLUGIN_FAILURE;
	}

	APP_LOG("updateBlobStorageData", LOG_DEBUG, "File opened in mode:%d, file at Path:%s", flagReadWrite,file_path);

	/*perform file operation read/write as per the mode received*/
	if(BLOB_READ == flagReadWrite)
		fgets(pSmartParamValue, SIZE_256B, s_fpBlobHandle);
	else
		fprintf(s_fpBlobHandle, "%s", pSmartParamValue);

	APP_LOG("updateBlobStorageData", LOG_DEBUG, "Data is: %s", pSmartParamValue);
	/*close file handle*/
	fclose(s_fpBlobHandle);

	return retVal;
}

int GetBlobStorage(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	mxml_node_t *tree;
	mxml_node_t *attribute_node;
	mxml_node_t *blobNameNode;
	char *ptempBlobXMLData = NULL;
	char *pblobXMLData = NULL;
	char blobValue[SIZE_256B];
	char *pName;
	char getBlobRespBuf[SIZE_1024B] = {0};
	char szBuff[SIZE_256B] = {0};
	int retVal = PLUGIN_SUCCESS, blobXMLDataLen = 0;

	if(NULL == pActionRequest || NULL == request)
	{
		APP_LOG("UPNP", LOG_ERR, "SetBlobStorage command paramter invalid");
		return SET_PARAMS_NULL;
	}

	/*get xml data under attributeList tag*/
	ptempBlobXMLData = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "attributeList");
	if((NULL == ptempBlobXMLData) || (0x00 == strlen(ptempBlobXMLData)))
	{
		APP_LOG("UPNP", LOG_ERR, "parameter error: attr data null");
		retVal = PLUGIN_FAILURE;
		goto out;
	}

	APP_LOG("UPNP", LOG_DEBUG, "Blob XML Data received:%s", ptempBlobXMLData);

	/*calculate xml length i.e xml received + xml header lenght*/
	blobXMLDataLen = strlen(ptempBlobXMLData)+SIZE_128B;

	/*allocate memory for new xml*/
	pblobXMLData = (char*)CALLOC(1, blobXMLDataLen);
	if(!pblobXMLData)
	{
		APP_LOG("UPNP", LOG_DEBUG, "memory allocation failed");
		resetSystem();
	}

	/*add header to received xml*/
	snprintf(pblobXMLData, blobXMLDataLen, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>%s", ptempBlobXMLData);
	APP_LOG("UPNP", LOG_DEBUG, "modified Blob XML Data is:%s", pblobXMLData);

	/*load xml tree*/
	tree = mxmlLoadString(NULL, pblobXMLData, MXML_OPAQUE_CALLBACK);
	if(tree)
	{
		/*parse all blob labels present in the xml*/
		for(attribute_node = mxmlFindElement(tree, tree, "attribute", NULL, NULL, MXML_DESCEND);
				attribute_node != NULL;
				attribute_node = mxmlFindElement(attribute_node, tree, "attribute", NULL, NULL, MXML_DESCEND))
		{
			/*attribute present, get name tag pointers*/
			blobNameNode = mxmlFindElement(attribute_node, tree, "name", NULL, NULL, MXML_DESCEND);

			/*check for valid name tag pointers*/
			if(blobNameNode)
			{
				/*get blob label*/
				pName = (char *)mxmlGetOpaque(blobNameNode);

				/*check for valid blob lable pointer and its length*/
				if(pName && strlen(pName))
				{
					APP_LOG("UPNP", LOG_DEBUG, "GetBlobStorage: Label Name is %s", pName);
					/*read this blob storage*/
					if(PLUGIN_SUCCESS == updateBlobStorageData(pName, blobValue, BLOB_READ))
					{
						/*found blob storage, add it to response tag*/
						snprintf(szBuff, SIZE_256B, "<attribute><name>%s</name><value>%s</value></attribute>",pName, blobValue);
						strncat(getBlobRespBuf, szBuff, (sizeof(getBlobRespBuf) - strlen(getBlobRespBuf) - 1));
						memset(szBuff,0,SIZE_256B);
						memset(blobValue,0,SIZE_256B);
					}
					else
						APP_LOG("UPNP", LOG_DEBUG, "Failed to get BlobStorage for label: %s", pName);
				}
			}
		}
		/*free loaded tree*/
		mxmlDelete(tree);
	}
	else
	{
		APP_LOG("UPNP", LOG_ERR, "Could not load tree");
		retVal = PLUGIN_FAILURE;
	}

out:
	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = retVal;

	/*check if blob storage fould*/
	if(strlen(getBlobRespBuf))
	{
		APP_LOG("UPNP", LOG_ERR, "GetBlobStorage - response is %s", getBlobRespBuf);
		char *getAttrEncResp = EscapeXML(getBlobRespBuf);
		char *pBuffer = NULL;
		if(getAttrEncResp)
			pBuffer = getAttrEncResp;
		else
			pBuffer = getBlobRespBuf;

		/*send UPnP action success response*/
		retVal = UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetBlobStorage", CtrleeDeviceServiceType[PLUGIN_E_DEVICE_EVENT_SERVICE], "attributeList", pBuffer);
		if(getAttrEncResp){free(getAttrEncResp);getAttrEncResp = NULL;}
	}
	else
	{
		APP_LOG("UPNP", LOG_DEBUG, "GetBlobStorage Failed");
		/*send UPnP action failure response*/
		retVal = UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetBlobStorage", CtrleeDeviceServiceType[PLUGIN_E_DEVICE_EVENT_SERVICE], "attributeList", "-1");
	}

	/*free memory*/
	if(pblobXMLData){free(pblobXMLData);pblobXMLData=NULL;}
	FreeXmlSource(ptempBlobXMLData);
	return retVal;
}

/* SetAttributes function is to change the Jarden Device settings
 * This function make use of HAL_SetMode() & HAL_SetDelaytime() functions from HAL layer
 * pActionRequest - Handle to request
 */
int SetAttribute(pUPnPActionRequest pActionRequest,
                     IXML_Document *request,
                     IXML_Document **out,
                     const char **errorString)
{

	int retstatus=0;
	char szCurState[16];
	WaspVariable WaspAttr;
	int Err;
    char* attr_x;
    int attr, i;

	memset(szCurState, 0x00, sizeof(szCurState));

	if (NULL == pActionRequest || NULL == request)
	{
		APP_LOG("UPNPDevice", LOG_DEBUG, "SetAttribute: command paramter invalid");
		return SET_PARAMS_NULL;
	}

    for (i =0; i<g_attrCount; i++)
    {
        APP_LOG("UPNPDevice", LOG_DEBUG, "SetAttribute: i is %d, g_attrVarDescArray[i]->Name is %s g_attrVarDescArray[i]->Usage is %d", i, g_attrVarDescArray[i]->Name,g_attrVarDescArray[i]->Usage);

        if(g_attrVarDescArray[i]->Usage == 4 || g_attrVarDescArray[i]->Usage == 3)
        {
            attr_x = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, g_attrVarDescArray[i]->Name);

            APP_LOG("UPNPDevice", LOG_ERR, "Decoded upnp xml attr_x is %s, attribute is %s ", attr_x, g_attrVarDescArray[i]->Name);
            if (0x00 == strlen(attr_x))
            {
                pActionRequest->ActionResult = NULL;
                pActionRequest->ErrCode = SET_PARAM_ERROR;

                UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetAttribute",
                        CtrleeDeviceServiceType[PLUGIN_E_DEVICE_EVENT_SERVICE], g_attrVarDescArray[i]->Name, "Parameter Error");

                APP_LOG("UPNPDevice", LOG_DEBUG, "SetAttribute: attr_x  Error");

                return UPNP_E_SUCCESS;
            }


            retstatus=0;
            memset(&WaspAttr,0,sizeof(WaspVariable));
            WaspAttr.ID = g_attrVarDescArray[i]->ID;
            WaspAttr.Type = g_attrVarDescArray[i]->Type;

    attrDataLen = strlen(setAttrData)+SIZE_64B;
    pSetAttrData = (char*)CALLOC(1, attrDataLen);
    if (!pSetAttrData)
	resetSystem();
    snprintf(pSetAttrData, attrDataLen, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>%s", setAttrData);
    APP_LOG("UPNP", LOG_DEBUG, "modified setAttrData XML is:\n %s\n", pSetAttrData);

            if((Err = WASP_SetVariable(&WaspAttr)) != WASP_OK) {
                if(Err >= 0) {
                    Err = -Err;
                }
                retstatus = Err;
            }

            if (0x00 == retstatus) {

                pActionRequest->ActionResult = NULL;
                pActionRequest->ErrCode = 0;

                APP_LOG("UPNPDevice", LOG_ERR, "SetAttribute: WASP_SetVariable sucess %s, %s ", attr_x, g_attrVarDescArray[i]->Name);

                UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetAttribute",
                        CtrleeDeviceServiceType[PLUGIN_E_DEVICE_EVENT_SERVICE], g_attrVarDescArray[i]->Name, attr_x);
            }
            else {
                pActionRequest->ActionResult = NULL;
                pActionRequest->ErrCode = SET_CPMODE_FAILURE;

                UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetAttribute",
                        CtrleeDeviceServiceType[PLUGIN_E_DEVICE_EVENT_SERVICE], g_attrVarDescArray[i]->Name, "Error");

                APP_LOG("UPNPDevice", LOG_ERR, "SETJARDENSTATUS: g_attrVarDescArray[i]->Name %s not changed", g_attrVarDescArray[i]->Name);
            }
            FreeXmlSource(attr_x);
        }
    }
	return UPNP_E_SUCCESS;

}

/* GetAttribute function is to change the Jarden Device settings
 * This function make use of HAL_GetMode() & HAL_GetCooktime() functions from HAL layer
 * pActionRequest - Handle to request
 *
 */
int GetAttribute(pUPnPActionRequest pActionRequest,
                     IXML_Document *request,
                     IXML_Document **out,
                     const char **errorString)
{
    char getAttrResp[32];
    int ret;
	WaspVariable WaspAttr;
	int Err, i;
    memset(getAttrResp, 0x00, sizeof(getAttrResp));

    if (NULL == pActionRequest || NULL == request)
    {
        APP_LOG("UPNPDevice", LOG_DEBUG, "GetAttribute: command paramter invalid");
        return GET_PARAMS_NULL;
    }

    for (i =0; i<g_attrCount; i++)
    {
        if(g_attrVarDescArray[i]->Usage != 3)
        {
            /* Get the current mode setting */
            memset(&WaspAttr,0,sizeof(WaspVariable));
            WaspAttr.ID = g_attrVarDescArray[i]->ID;
            WaspAttr.Type = g_attrVarDescArray[i]->Type;

            if((Err = WASP_GetVariable(&WaspAttr)) != WASP_OK)
            {
                if(Err >= 0) {
                    Err = -Err;
                }
            }
            else
            {
            switch(g_attrVarDescArray[i]->Type)
            {
                APP_LOG("UPNPDevice", LOG_ERR, "Inside GetAttribute switch(g_attrVarDescArray[i]->Type)");
                case WASP_VARTYPE_ENUM:
                sprintf(getAttrResp, "%d",WaspAttr.Val.Enum);
                break;
                case WASP_VARTYPE_PERCENT:
                sprintf(getAttrResp, "%hu",WaspAttr.Val.Percent);
                break;
                case WASP_VARTYPE_TEMP:
                sprintf(getAttrResp, "%hd",WaspAttr.Val.Temperature);
                break;
                case WASP_VARTYPE_TIME32:
                sprintf(getAttrResp, "%d",WaspAttr.Val.TimeTenths);
                break;
                case WASP_VARTYPE_TIME16:
                sprintf(getAttrResp, "%hu",WaspAttr.Val.TimeSecs);
                break;
                case WASP_VARTYPE_BOOL:
                sprintf(getAttrResp, "%d",WaspAttr.Val.Boolean);
                break;
                case WASP_VARTYPE_STRING:
                sprintf(getAttrResp, "%s",WaspAttr.Val.String);
                break;
                case WASP_VARTYPE_BLOB:
                sprintf(getAttrResp, "%s",WaspAttr.Val.Blob.Data);
                break;
                case WASP_VARTYPE_UINT8:
                sprintf(getAttrResp, "%d",WaspAttr.Val.U8);
                break;
                case WASP_VARTYPE_INT8:
                sprintf(getAttrResp, "%d",WaspAttr.Val.I8);
                break;
                case WASP_VARTYPE_UINT16:
                sprintf(getAttrResp, "%d",WaspAttr.Val.U16);
                break;
                case WASP_VARTYPE_INT16:
                sprintf(getAttrResp, "%d",WaspAttr.Val.I16);
                break;
                case WASP_VARTYPE_UINT32:
                sprintf(getAttrResp, "%d",WaspAttr.Val.U32);
                break;
                case WASP_VARTYPE_INT32:
                sprintf(getAttrResp, "%d",WaspAttr.Val.I32);
                break;
                case WASP_VARTYPE_TIME_M16:
                sprintf(getAttrResp, "%d",WaspAttr.Val.TimeMins);
                break;
                default:
                APP_LOG("UPNPDevice", LOG_ERR, "Invalid attribute type");
            }

            APP_LOG("UPNPDevice", LOG_ERR, "GetAttribute - getAttrResp is %s", getAttrResp);
            ret = UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetAttribute",
                                  CtrleeDeviceServiceType[PLUGIN_E_DEVICE_EVENT_SERVICE], g_attrVarDescArray[i]->Name, getAttrResp);

            memset(getAttrResp, 0x00, sizeof(getAttrResp));

		}
    }
    }

    return UPNP_E_SUCCESS;
}

int SetNotif(int* attribute, char* dstURL, int dstType)
{
    int ret;
    char SerialNumber[10];
    sprintf(SerialNumber, "%d", g_NotifSerialNumber);
    APP_LOG("SetNotif", LOG_DEBUG, "SerialNumber is %s", SerialNumber);

    char Attr1[10];
    sprintf(Attr1, "%d", attribute[0]);
    APP_LOG("SetNotif", LOG_DEBUG, "Attr1 is %s", Attr1);

    char Attr2[10];
    sprintf(Attr2, "%d", attribute[1]);
    APP_LOG("SetNotif", LOG_DEBUG, "Attr1 is %s", Attr2);

    char Attr3[10];
    sprintf(Attr3, "%d", attribute[2]);
    APP_LOG("SetNotif", LOG_DEBUG, "Attr1 is %s", Attr3);

    char Attr4[10];
    sprintf(Attr4, "%d", attribute[3]);
    APP_LOG("SetNotif", LOG_DEBUG, "Attr1 is %s", Attr4);

    char Attr5[10];
    sprintf(Attr5, "%d", attribute[4]);
    APP_LOG("SetNotif", LOG_DEBUG, "Attr1 is %s", Attr5);

    char* notifURL = (char*)ZALLOC(64*sizeof(char));
    sprintf(notifURL, "'%s'",dstURL);
    APP_LOG("SetNotif", LOG_DEBUG, "notifURL is %s", notifURL);

    char tdstType[10];
    sprintf(tdstType, "%d", dstType);
    APP_LOG("SetNotif", LOG_DEBUG, "tdstType is %s", tdstType);

    ColDetails SetNotifyInfo[] = {{"SERIALNO",SerialNumber},{"Attr1",Attr1},{"Attr2",Attr2},{"Attr3",Attr3},{"Attr4",Attr4},{"Attr5",Attr5},{"dstURL",notifURL},{"dstType",tdstType}};

    APP_LOG("SetNotif", LOG_DEBUG, "After ColDetails SetNotifyInfo");

	ret = WeMoDBInsertInTable(&NotificationDB,"notificationData",SetNotifyInfo,8);
    APP_LOG("SetNotif", LOG_DEBUG, "After WeMoDBInsertInTable SetNotifyInfo");

    g_NotifSerialNumber++;
}
#endif


int SetLogLevelOption (pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		int lvl;
		int opt;

		if (0x00 == pActionRequest || 0x00 == request)
		{
				APP_LOG("UPNPDevice", LOG_ERR, "SetLogLevelOption: command paramter invalid");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}
		char* szLevel = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Level");
		if(szLevel != NULL)
		{
				if (0x00 == strlen(szLevel))
				{
						pActionRequest->ActionResult = NULL;
						pActionRequest->ErrCode = PLUGIN_ERROR_E_BASIC_EVENT;
						UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetLogLevelOption", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"Level", "Parameter Error");
						APP_LOG("UPNP: Device", LOG_ERR,"Set Log Level parameter: failure");
						return PLUGIN_ERROR_E_BASIC_EVENT;
				}
		}
		char* szOption = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Option");
		if(szOption != NULL)
		{
				if (0x00 == strlen(szOption))
				{
						pActionRequest->ActionResult = NULL;
						pActionRequest->ErrCode = PLUGIN_ERROR_E_BASIC_EVENT;
						UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetLogLevelOption", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"Option", "Parameter Error");
						APP_LOG("UPNP: Device", LOG_ERR,"Set Log Option parameter: failure");
						return PLUGIN_ERROR_E_BASIC_EVENT;
				}
		}

		lvl = atoi(szLevel);
		opt = atoi(szOption);
		APP_LOG("UPNP: Device", LOG_DEBUG,"Setting Log level: %d and option: %d", lvl, opt);

		loggerSetLogLevel (lvl, opt);
		return UPNP_E_SUCCESS;
}

int GetBinaryState(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		int curState = 0x00;

		if (0x00 == pActionRequest || 0x00 == request)
		{
				APP_LOG("UPNPDevice", LOG_DEBUG, "GetBinaryState: paramters error");
				return 0x01;
		}

	if (DEVICE_SOCKET == g_eDeviceType)
	{
		LockLED();
		curState = GetCurBinaryState();
		if(curState)
		{
			APP_LOG("UPNPDevice", LOG_ERR,"Switch State: ON");
		}
		else
		{
			APP_LOG("UPNPDevice", LOG_ERR,"Switch State: OFF");
		}
		UnlockLED();
	}
	else if (DEVICE_SENSOR == g_eDeviceType)
	{
		LockSensor();
		curState = GetSensorState();
		UnlockSensor();

		if(curState)
		{
			APP_LOG("UPNPDevice", LOG_ERR,"Motion Detected: TRUE");
		}
		else
		{
			APP_LOG("UPNPDevice", LOG_ERR,"Motion Detected: FALSE");
		}
	}
    #ifdef PRODUCT_WeMo_Jarden
	else {
    		LockLED();
        curState = GetCurBinaryState();
        if(curState)
        {
            APP_LOG("UPNPDevice", LOG_ERR,"Switch State: ON");
        }
        else
        {
            APP_LOG("UPNPDevice", LOG_ERR,"Switch State: OFF");
        }
        UnlockLED();
    }
	#endif

		char szCurState[SIZE_4B];
		memset(szCurState, 0x00, sizeof(szCurState));

		snprintf(szCurState, sizeof(szCurState), "%d", curState);
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetBinaryState",
						CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "BinaryState", szCurState);

		APP_LOG("UPNPDevice", LOG_DEBUG, "GetBinaryState: %s", szCurState);

		IsOverriddenStatus();
		return UPNP_E_SUCCESS;
}

int StopPair(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		if (0x00 == pActionRequest)
		{
				APP_LOG("UPNPDevice", LOG_DEBUG, "Parameter error");
				return 0x00;
		}

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "StopPair",
						CtrleeDeviceServiceType[PLUGIN_E_SETUP_SERVICE],"status", "success");
#ifndef PRODUCT_WeMo_LEDLight
		system("ifconfig apcli0 down");
		pluginUsleep(500000);
		system("ifconfig apcli0 up");
#endif
		APP_LOG("UPNP", LOG_DEBUG, "WiFi pairing stopped");
		return 0;
}

int ConnectHomeNetwork(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		int channel;    int rect = 0x00;
		char* paramValue = 0x00;
		paramValue = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "channel");

		gAppCalledCloseAp=0;
		gBootReconnect=0;
#ifdef WeMo_SMART_SETUP
		/* reset smart setup presence */
		gSmartSetup = 0;
#endif
#ifdef WeMo_INSTACONNECT
		g_connectInstaStatus = APCLI_CONFIGURING;	//connectToInstaApTask do nothing
		g_usualSetup = 0x01;
#endif

		APP_LOG("UPNPDevice", LOG_CRIT,"%s", __FUNCTION__);
		if(isSetupRequested())
		{
				APP_LOG("UPNPDevice", LOG_ERR, "#### Setup request already executed ######");
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x01;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "ConnectHomeNetwork", CtrleeDeviceServiceType[PLUGIN_E_SETUP_SERVICE], "status", "unsuccess");
				return 0x01;
		}

		setSetupRequested(1);

		UpdateUPnPNetworkMode(UPNP_LOCAL_MODE);

		channel = atoi(paramValue);

		char* ssid 		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "ssid");
		char* auth 		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "auth");
		char* encrypt	= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "encrypt");
		char* password;

		if(strcmp(auth,"OPEN"))
				password = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "password");
		else
				password = "NOTHING";

		/* Save the password in a global variable - to be used later by WifiConn thread */
		memset(gUserKey, 0, sizeof(gUserKey));
		memcpy(gUserKey, password, sizeof(gUserKey));

		APP_LOG("UPNPDevice",LOG_HIDE,"Attempting to connect home network: %s, channel: %d, auth:%s, encrypt:%s, password:%s",
						ssid, channel, auth, encrypt, password);
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "ConnectHomeNetwork", CtrleeDeviceServiceType[PLUGIN_E_SETUP_SERVICE],"PairingStatus", "Connecting");

		if(CHAN_HIDDEN_NW == channel)
		{
				PWIFI_PAIR_PARAMS pHiddenNwParams=NULL;

				APP_LOG("UPNPDevice",LOG_DEBUG,"Connect request for hidden network: %s......", ssid);
				pHiddenNwParams = ZALLOC(sizeof(WIFI_PAIR_PARAMS));

				strncpy(pHiddenNwParams->SSID, ssid, sizeof(pHiddenNwParams->SSID)-1);
				strncpy(pHiddenNwParams->AuthMode, auth, sizeof(pHiddenNwParams->AuthMode)-1);
				strncpy(pHiddenNwParams->EncrypType, encrypt, sizeof(pHiddenNwParams->EncrypType)-1);
				strncpy(pHiddenNwParams->Key, password, sizeof(pHiddenNwParams->Key)-1);
				pHiddenNwParams->channel = CHAN_HIDDEN_NW;

				rect = threadConnectHiddenNetwork(pHiddenNwParams);
		}
		else
		{
				APP_LOG("UPNPDevice",LOG_DEBUG,"connect to selected network: %s", ssid);
				rect = threadConnectHomeNetwork(channel, ssid, auth, encrypt, password);
		}

		if(strcmp(auth,"OPEN"))
				FreeXmlSource(password);
		FreeXmlSource(paramValue);
		FreeXmlSource(ssid);
		FreeXmlSource(auth);
		FreeXmlSource(encrypt);

		return rect;

}

int GetNetworkStatus(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		int WiFiClientCurrState = 0x00;
		char szStatus[SIZE_16B];
		memset(szStatus, 0x00, sizeof(szStatus));

		if (0x00 == pActionRequest)
		{
				APP_LOG("UPNP: Device", LOG_ERR, "%s: request parameter error", __FUNCTION__);

				return PLUGIN_ERROR_E_NETWORK_ERROR;
		}

		WiFiClientCurrState = getCurrentClientState();

		snprintf(szStatus, sizeof(szStatus), "%d", WiFiClientCurrState);

		APP_LOG("UPNPDevice", LOG_ERR,"NetworkStatus: %d:%s", WiFiClientCurrState, szStatus);

#ifdef WeMo_SMART_SETUP_V2
		/* set app connnected flag to allow device to keep AP open for setup */
		if(!g_isAppConnected)
		{
		    g_isAppConnected = 1;
		    APP_LOG("UPNP", LOG_DEBUG, "is app connected flag set");
		}
#endif
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetNetworkStatus",
						CtrleeDeviceServiceType[PLUGIN_E_SETUP_SERVICE],"NetworkStatus", szStatus);

		return UPNP_E_SUCCESS;
}


int GetDeviceTime(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		return UPNP_E_SUCCESS;
}

int IsApplyTimeSync(int utc, double timeZone, int dst)
{
		int rect = 0x00;

		/* 1. 24 Hours
		 * 2. Time Zone difference.
		 */

		APP_LOG("UPNPDevice", LOG_ERR,"TimeSync: %d:%f:%d:%f:%d", utc,timeZone,g_lastTimeSync,g_lastTimeZone,dst);
		if(timeZone != (g_lastTimeZone+dst)) {
				rect = 0x01;
		}

		if(utc > (g_lastTimeSync+86400)) {
				rect = 0x01;
		}
#ifdef _OPENWRT_
		{
			time_t Now = time(NULL);
			struct tm Tm;

			(void) gmtime_r(&Now,&Tm);
			if(Tm.tm_year + 1900 < 2015) {
			// Time not set yet
				rect = 0x01;
			}
		}
#endif

		return rect;
}

#if defined(PRODUCT_WeMo_NetCam) || !defined(_OPENWRT_)
int updateSysTime(void)
{
		int ret;
		struct timeval curtime;

		int iTimeZoneIndex = -1;
		//[WEMO-26944] - Change method of updating system time when DST ON,
		//to cover boundary cases and Lord Howe Island DST.
		int addTime = 0;
		iTimeZoneIndex = GetTimeZoneIndex(g_lastTimeZone);
		if (gDstEnable)
		{
				//Lord Howe Island time-zone
				float delta = g_lastTimeZone - AUS_TIMEZONE_3;
				if ((delta <= 0.001) && (delta >= -0.001))
				{
						addTime = NUM_SECONDS_IN_HOUR/2;
				}
				else
				{
						addTime = NUM_SECONDS_IN_HOUR;
				}
		}
		APP_LOG("Time",LOG_ERR, "Update system-time, g_lastTimeZone: %f, additional time: %d, gDstEnable: %d",g_lastTimeZone, addTime, gDstEnable);
		ret = SetTime(GetUTCTime() + addTime, iTimeZoneIndex, gDstEnable);

		if (0 == ret)
		{
				APP_LOG("Time", LOG_DEBUG, "set Time success");
		}
		else
		{
				APP_LOG("Time", LOG_ERR, "set Time failed!!");
		}

		sleep(2);
		ret = SetNTP(s_szNtpServer, iTimeZoneIndex, 0x00);


		return ret;
}

static int calculateTimeZoneSpecificInfo(const int dstyr, const int nxtyr, int *sdate, int *smonth, int *shr,
				int *edate, int *emonth, int *ehr, int *snxtdate, int *enxtdate)
{

		int isDSTTimeZone = 0;

		//US TIMEZONE
		{
				*sdate = 14-((int)(floor(1+(dstyr*5/4)))%7);
				*edate = 7-((int)(floor(1+(dstyr*5/4)))%7);

				*snxtdate = 14-((int)(floor(1+(nxtyr*5/4)))%7);

				*smonth = 2; //March
				*emonth = 10; //November
				*shr = 2;
				*ehr = 1; //2:00 AM
				isDSTTimeZone = 1;
		}

		int sval = 0, eval = 0, snxtval, enxtval;
		float delta, delta1, delta2;

		delta = (g_lastTimeZone - UK_TIMEZONE);
		if ((delta <= 0.001) && (delta >= -0.001))
		{
				/* Europe: UK(London) ... (GMT)
				 *
				 * Computation to calculate the last SUNDAY of March and
				 * last SUNDAY of October.
				 *
				 */
				sval = *sdate + 14;
				*sdate = ((sval+7) > 31) ? sval : (sval+7);

				eval = *edate - 7;
				*edate = (eval < 0) ? (31+eval) : 31;

				snxtval = *snxtdate + 14;
				*snxtdate = ((snxtval+7) > 31) ? snxtval : (snxtval+7);

				*smonth = 2; //March
				*emonth = 9; //October
				*shr = 1; //1:00 AM
				*ehr = 1; //2:00 AM
				isDSTTimeZone = 1;
				APP_LOG("calculateTimeZoneSpecificInfo",LOG_DEBUG, "sdate: %d, edate: %d, snxtdate: %d, smonth: %d, emonth: %d, shr: %d, ehr: %d, isDSTTimeZone: %d", *sdate, *edate, *snxtdate, *smonth, *emonth, *shr, *ehr, isDSTTimeZone);
				return isDSTTimeZone;
		}

		delta = (g_lastTimeZone - FRANCE_TIMEZONE);
		if ((delta <= 0.001) && (delta >= -0.001))
		{
				/* Europe: France(Paris), Spain(Madrid), Germany(Berlin) ... (GMT+1:00)
				 *
				 * Computation to calculate the last SUNDAY of March and
				 * last SUNDAY of October.
				 *
				 */
				sval = *sdate + 14;
				*sdate = ((sval+7) > 31) ? sval : (sval+7);

				eval = *edate - 7;
				*edate = (eval < 0) ? (31+eval) : 31;

				snxtval = *snxtdate + 14;
				*snxtdate = ((snxtval+7) > 31) ? snxtval : (snxtval+7);

				*smonth = 2; //March
				*emonth = 9; //October
				*shr = 2; //2:00 AM
				*ehr = 2; //3:00 AM
				isDSTTimeZone = 1;
				APP_LOG("calculateTimeZoneSpecificInfo",LOG_DEBUG, "sdate: %d, edate: %d, snxtdate: %d, smonth: %d, emonth: %d, shr: %d, ehr: %d, isDSTTimeZone: %d", *sdate, *edate, *snxtdate, *smonth, *emonth, *shr, *ehr, isDSTTimeZone);
				return isDSTTimeZone;
		}

		delta = (g_lastTimeZone - CHINA_TIMEZONE);

		if ((delta <= 0.001) && (delta >= -0.001))
		{
				//China ... NO DST
				isDSTTimeZone = 0;
				APP_LOG("calculateTimeZoneSpecificInfo",LOG_DEBUG, "isDSTTimeZone: %d", isDSTTimeZone);
				return isDSTTimeZone;
		}

		delta = (g_lastTimeZone - JAPAN_TIMEZONE);

		if ((delta <= 0.001) && (delta >= -0.001))
		{
				//Japan ... NO DST
				isDSTTimeZone = 0;
				APP_LOG("calculateTimeZoneSpecificInfo",LOG_DEBUG, "isDSTTimeZone: %d", isDSTTimeZone);
				return isDSTTimeZone;
		}

		/*************************************************************************************
		 * (Source: http://wwp.greenwichmeantime.com/time-zone/australia/time-zones/index.htm)
		 * Summer Time (Daylight Saving Time) runs in:
		 * 1) New South Wales
		 * 2) Australian Capital Territory
		 * 3) Victoria
		 * 4) South Australia
		 * 5) Tasmania
		 * from the first Sunday in October through to the first Sunday in April
		 *
		 * DST is not supported in:
		 * 1) Northern Territory
		 * 2) Western Australia
		 * 3) Queensland
		 *************************************************************************************/

		delta = (g_lastTimeZone - AUS_TIMEZONE_1);
		delta1 = (g_lastTimeZone - AUS_TIMEZONE_2);
		delta2 = (g_lastTimeZone - AUS_TIMEZONE_3);
		if (((delta <= 0.001) && (delta >= -0.001)) ||
						((delta1 <= 0.001) && (delta1 >= -0.001)) ||
						((delta2 <= 0.001) && (delta2 >= -0.001)))
		{
				/*
				 * Computation to calculate the first SUNDAY of OCTOBER
				 * and first SUNDAY of April
				 *
				 */

				sval = ((31 + *edate)%7);
				if(sval == 0)
						sval = sval + 7;

				eval = (7 - ((31 - *sdate)%7));

				*sdate = sval; //Start date: first SUNDAY of October
				*edate = eval; //End date: first SUNDAY of April

				*enxtdate = 7-((int)(floor(1+(nxtyr*5/4)))%7);

				snxtval = ((31 + *enxtdate)%7);
				if(snxtval == 0)
						snxtval = snxtval + 7;

				enxtval = (7 - ((31 - *snxtdate)%7));

				*snxtdate = snxtval; //Next Year Start date: first SUNDAY of October
				*enxtdate = enxtval; //Next Year End date: first SUNDAY of April

				*smonth = 9; //October
				*emonth = 3; //April

				*shr = 2; //2:00 AM
				*ehr = 2; //3:00 AM

				isDSTTimeZone = 1;
				APP_LOG("calculateTimeZoneSpecificInfo",LOG_DEBUG, "sdate: %d, edate: %d, snxtdate: %d, enxtdate: %d, smonth: %d, emonth: %d, shr: %d, ehr: %d, isDSTTimeZone: %d", *sdate, *edate, *snxtdate, *enxtdate, *smonth, *emonth, *shr, *ehr, isDSTTimeZone);
				return isDSTTimeZone;
		}

		/*************************************************************************************
		 * (Source: http://wwp.greenwichmeantime.com/time-zone/pacific/new-zealand/
		 * Summer Time (Daylight Saving Time) runs in:
		 * 1)Wellington, Auckland
		 * 2)Waitangi, Chatham Island
		 * from the last Sunday in September through to the first Sunday in April
		 *************************************************************************************/

		delta = (g_lastTimeZone - NZ_TIMEZONE_1);
		delta1 = (g_lastTimeZone - NZ_TIMEZONE_2);
		if (((delta <= 0.001) && (delta >= -0.001)) ||
						((delta1 <= 0.001) && (delta1 >= -0.001)))
		{
				/*
				 * Computation to calculate the last Sunday in September
				 * and first SUNDAY of April
				 *
				 */

				sval = ((31 + *edate)%7);
				if(sval == 0)
						sval = sval + 7;

				eval = (7 - ((31 - *sdate)%7));

				/* for last sunday of sep */
				*sdate = (sval - 7) + 30; //Start date: last SUNDAY of September
				*edate = eval; //End date: first SUNDAY of April

				*enxtdate = 7-((int)(floor(1+(nxtyr*5/4)))%7);

				snxtval = ((31 + *enxtdate)%7);
				if(snxtval == 0)
						snxtval = snxtval + 7;

				enxtval = (7 - ((31 - *snxtdate)%7));

				/* for last sunday of sep */
				*snxtdate = (snxtval - 7) + 30; //Next Year Start date: last SUNDAY of September
				*enxtdate = enxtval; //Next Year End date: first SUNDAY of April

				*smonth = 8; //September
				*emonth = 3; //April

				*shr = 2; //2:00 AM
				*ehr = 2; //3:00 AM

				isDSTTimeZone = 1;
				APP_LOG("calculateTimeZoneSpecificInfo",LOG_DEBUG, "sdate: %d, edate: %d, snxtdate: %d, enxtdate: %d, smonth: %d, emonth: %d, shr: %d, ehr: %d, isDSTTimeZone: %d", *sdate, *edate, *snxtdate, *enxtdate, *smonth, *emonth, *shr, *ehr, isDSTTimeZone);
				return isDSTTimeZone;
		}

		APP_LOG("calculateTimeZoneSpecificInfo",LOG_DEBUG, "sdate: %d, edate: %d, snxtdate: %d, smonth: %d, emonth: %d, shr: %d, ehr: %d, isDSTTimeZone: %d", *sdate, *edate, *snxtdate, *smonth, *emonth, *shr, *ehr, isDSTTimeZone);
		return isDSTTimeZone;
}

/*
 * dstComputationThread:
 *  Compute DST time for different timezones
 */
void* dstComputationThread(void *arg)
{
		int retVal = 0;
		int sdate = 0, edate = 0, dstime = 0, snxtdate=0, enxtdate=0;
		int dstyr = 0, nxtyr=0;
		struct tm *tm_struct;
		struct timeval dtime;
		struct tm mkTm = { 0 };
		unsigned int dsdate = 0, dedate = 0, dnsdate=0, dnedate=0;
		int dstToggleTime=0;
		int dstEnable=0;
		int updateDstime = *(int *)arg;
		float adjustment=0.0;
		int smonth = 0, emonth = 0;
		int shr = 0, ehr = 0;
		int isDSTTimeZone = 0; //0 = NO DST, 1 = DST
		char dstenable[SIZE_16B];
		float delta, delta1, delta2, delta3, delta4;
		char ltimezone[SIZE_16B];
		int index = 0;
		int adjust=0;
                tu_set_my_thread_name( __FUNCTION__ );
		memset(dstenable, 0x0, sizeof(dstenable));

		free(arg);

restart_thread:

		/* sleep a while to let SyncTime finish its processing */
		APP_LOG("WiFiApp",LOG_DEBUG, "sleep a while to let SyncTime finish its processing, updateDstime: %d ..",updateDstime);
		sleep(50);

		while(1)
		{
			/*check if NTP is updated */
			if(IsNtpUpdate())
			{
				APP_LOG("Rule", LOG_DEBUG, "NTP updated, going ahead..");

				break;
			}
			sleep(DELAY_3SEC);
		}


		tm_struct = (struct tm *)ZALLOC(sizeof(struct tm));
		if (!tm_struct) {
				APP_LOG("WiFiApp",LOG_ERR, "Unable to allocate memory for tm struct ..");
				return NULL;
		}

		gettimeofday(&dtime, NULL);
		dstime = dtime.tv_sec;
		APP_LOG("UPNP: DEVICE",LOG_DEBUG, "Current time %u\n", dstime);

		dstime = GetUTCTime();
		APP_LOG("UPNP: DEVICE",LOG_DEBUG, "UTC time: %u\n",dstime);
		gmtime_r((const time_t *) &dstime, tm_struct);

		dstyr = tm_struct->tm_year+1900;
		nxtyr = tm_struct->tm_year+1900+1;

		isDSTTimeZone = calculateTimeZoneSpecificInfo(dstyr, nxtyr, &sdate, &smonth, &shr,
						&edate, &emonth, &ehr, &snxtdate, &enxtdate);

		if(!isDSTTimeZone) {
				free(tm_struct);
				tm_struct = NULL;
				return (void *)retVal;
		}


		//start date DST
		mkTm.tm_year = dstyr-1900;
		mkTm.tm_mday = sdate;
		mkTm.tm_mon = smonth; //2; //March
		mkTm.tm_hour = shr; //2;
		mkTm.tm_min = 0;
		mkTm.tm_sec = 0;
		dsdate = mktime(&mkTm);
		APP_LOG("UPNP: DEVICE",LOG_DEBUG, "DST start day %d %s year %d having act time %u\n", sdate, convertMonth(smonth), dstyr, dsdate);

		//end date DST
		mkTm.tm_year = dstyr-1900;
		mkTm.tm_mday = edate;
		mkTm.tm_mon = emonth; //10; //November
		mkTm.tm_hour = ehr; //1;	//2:00 am
		mkTm.tm_min = 0;
		mkTm.tm_sec = 0;
		dedate = mktime(&mkTm);
		APP_LOG("UPNP: DEVICE",LOG_DEBUG, "DST end day %d %s year %d having act time %u \n", edate, convertMonth(emonth), dstyr, dedate);

		//Next year's start date DST
		mkTm.tm_year = nxtyr-1900;
		mkTm.tm_mday = snxtdate;
		mkTm.tm_mon = smonth; //2; //March
		mkTm.tm_hour = shr; //2;
		mkTm.tm_min = 0;
		mkTm.tm_sec = 0;
		dnsdate = mktime(&mkTm);
		APP_LOG("UPNP: DEVICE",LOG_DEBUG, "DST start day %d %s year %d having act time %u \n", snxtdate, convertMonth(smonth), nxtyr, dnsdate);

		delta = (g_lastTimeZone - AUS_TIMEZONE_1);
		delta1 = (g_lastTimeZone - AUS_TIMEZONE_2);
		delta2 = (g_lastTimeZone - AUS_TIMEZONE_3);
		delta3 = (g_lastTimeZone - NZ_TIMEZONE_1);
		delta4 = (g_lastTimeZone - NZ_TIMEZONE_2);
		if (((delta <= 0.001) && (delta >= -0.001)) ||
						((delta1 <= 0.001) && (delta1 >= -0.001)) ||
						((delta2 <= 0.001) && (delta2 >= -0.001)) ||
						((delta3 <= 0.001) && (delta3 >= -0.001)) ||
						((delta4 <= 0.001) && (delta4 >= -0.001)))
		{
				mkTm.tm_year = nxtyr-1900;
				mkTm.tm_mday = enxtdate;
				mkTm.tm_mon = emonth;
				mkTm.tm_hour = ehr;
				mkTm.tm_min = 0;
				mkTm.tm_sec = 0;
				dnedate = mktime(&mkTm);
				APP_LOG("UPNP: DEVICE",LOG_DEBUG, "DST end day %d %s year %d having act time %u \n", enxtdate, convertMonth(emonth), nxtyr, dnedate);
		}

		free(tm_struct);
		tm_struct = NULL;

		/*
		 * dstime -> current time
		 * dsdate -> DST start time
		 * dedate -> DST end time
		 */

		/* we have to re-compute DST toggle if it was set last by phone app*/
		if(updateDstime)
		{
				APP_LOG("WiFiApp",LOG_DEBUG, "old dstime: %d seconds", dstime);
				adjustment = (g_lastTimeZone*3600);
				dstime = GetUTCTime() + (int)adjustment;
				APP_LOG("WiFiApp",LOG_DEBUG, "updated dstime: %d seconds adjusted %d secs", dstime, (int)adjustment);
		}

		/* Countries which are in Southern Hemisphere follow AUS_TIMEZONE and NZ_TIMEZONE rules */
		delta = (g_lastTimeZone - AUS_TIMEZONE_1);
		delta1 = (g_lastTimeZone - AUS_TIMEZONE_2);
		delta2 = (g_lastTimeZone - AUS_TIMEZONE_3);
		delta3 = (g_lastTimeZone - NZ_TIMEZONE_1);
		delta4 = (g_lastTimeZone - NZ_TIMEZONE_2);
		if (((delta <= 0.001) && (delta >= -0.001)) ||
						((delta1 <= 0.001) && (delta1 >= -0.001)) ||
						((delta2 <= 0.001) && (delta2 >= -0.001)) ||
						((delta3 <= 0.001) && (delta3 >= -0.001)) ||
						((delta4 <= 0.001) && (delta4 >= -0.001)))
		{
				//manupulation is done for the Waitangi, Chatham isl., New Zealand for additional 45 mins i.e GMT+12.45, 2700 comes from 45*60)
				delta = (g_lastTimeZone - NZ_TIMEZONE_2);
				if((delta <= 0.001) && (delta >= -0.001)) {
						dedate = dedate + 2700;
						dsdate = dsdate + 2700;
						dnedate = dnedate + 2700;
						dnsdate = dnsdate + 2700;
				}

				if(dstime <= dedate) /* dstime <= dedate */
				{
						dstToggleTime= (dedate - dstime);
						APP_LOG("WiFiApp",LOG_DEBUG, "Time before DST end: %d seconds\n", dstToggleTime);
						dstEnable = 0; /* DST currenty enabled, will remain enabled another (dedate-dstime) seconds */
				}
				else if(dstime <= dsdate)
				{
						dstToggleTime = (dsdate - dstime);
						APP_LOG("WiFiApp",LOG_DEBUG, "Time before DST start: %d seconds\n", dstToggleTime);
						dstEnable = 1; /* DST currenty disabled, will enable (dstEnable=1) in (dsdate-dstime) seconds */
				}
				else if(dstime <= dnedate) /* dsdate < dstime <= dnedate */
				{
						dstToggleTime= (dnedate - dstime);
						APP_LOG("WiFiApp",LOG_DEBUG, "Time before DST end: %d seconds\n", dstToggleTime);
						dstEnable = 0; /* DST currenty enabled, will remain enabled another (dnedate-dstime) seconds */
				}
				else
				{
						dstToggleTime= (dnsdate - dstime);
						APP_LOG("WiFiApp",LOG_DEBUG, "Time before DST start next year: %d seconds\n", dstToggleTime);
						dstEnable = 1; /* DST currenty disabled, will enable in another (dnsdate-dstime) seconds */
				}
		} else {
				if(dstime <= dsdate)
				{
						dstToggleTime = (dsdate - dstime);
						APP_LOG("WiFiApp",LOG_DEBUG, "Time before DST start: %d seconds\n", dstToggleTime);
						dstEnable = 1; /* DST currenty disabled, will enable (dstEnable=1) in (dsdate-dstime) seconds */
				}
				else if(dstime <= dedate) /* dsdate < dstime <= dedate */
				{
						dstToggleTime= (dedate - dstime);
						APP_LOG("WiFiApp",LOG_DEBUG, "Time before DST end: %d seconds\n", dstToggleTime);
						dstEnable = 0; /* DST currenty enabled, will remain enabled another (dedate-dstime) seconds */
				}
				else
				{
						dstToggleTime= (dnsdate - dstime);
						APP_LOG("WiFiApp",LOG_DEBUG, "Time before DST start next year: %d seconds\n", dstToggleTime);
						dstEnable = 1; /* DST currenty disabled, will enable in another (dnsdate-dstime) seconds */
				}
		}



		/* start a timer for toggleTime seconds */
		gDstEnable = dstEnable;

		snprintf(dstenable, sizeof(dstenable), "%d", gDstEnable);
		SetBelkinParameter (LASTDSTENABLE, dstenable);
		gTimeZoneUpdated = 1;
		APP_LOG("WiFiApp",LOG_DEBUG, "############ wait %d seconds to set dst=%d ###########", dstToggleTime, gDstEnable);
		AsyncSaveData();
		if(dstToggleTime > 50)
		    adjust=1;
		else
		    adjust=0;

		sleep(dstToggleTime - adjust*50);
		updateSysTime();
		//sleep(5);
#if defined(PRODUCT_WeMo_Insight)
		g_isDSTApplied=1;
#endif
		RestartRule4DST();

		/* call is to propagate DST information to PJ lib */
		memset(ltimezone, 0x0, sizeof(ltimezone));
		snprintf(ltimezone, sizeof(ltimezone), "%f", g_lastTimeZone);
		index = getTimeZoneIndexFromFlash();
		APP_LOG("UPNP: Device", LOG_DEBUG,"set pj dst data index: %d, lTimeZone:%s gDstEnable: %d success", index, ltimezone, gDstEnable);
#ifndef _OPENWRT_
		UDS_pj_dst_data_os(index, ltimezone, gDstEnable);
#endif

		//APP_LOG("WiFiApp",LOG_DEBUG, "dstComputation thread exiting...");
		/* toggle dst setting */

		gDstEnable = !gDstEnable;
		memset(dstenable, 0x0, sizeof(dstenable));
		snprintf(dstenable, sizeof(dstenable), "%d", gDstEnable);
		APP_LOG("UPNP: Device", LOG_DEBUG,"Updating gDstEnable to: %d", gDstEnable);
		SetBelkinParameter (LASTDSTENABLE, dstenable);
		gTimeZoneUpdated = 1;
		AsyncSaveData();

		/* wait for next DST switch-over */
		goto restart_thread;

		return (void *)retVal;
}

/*
 * computeDstToggleTime:
 *  Method to remove DST compute thread,if any and create a new DST compute thread
 */
int computeDstToggleTime(int updateDstime)
{
		int retVal = 0;
		int *pUpdateDstime;

		pUpdateDstime = (int *)MALLOC(sizeof(int));
		if(!pUpdateDstime)
		{
				APP_LOG("WiFiApp",LOG_ERR,"Unable to allocate memory for an int....");
				return -1;
		}

		*pUpdateDstime = updateDstime;

		/* first of all remove the dst thread, if running */
		if(dstmainthread != -1)
		{
				if((retVal = pthread_kill(dstmainthread, SIGRTMIN)) == 0)
				{
						dstmainthread=-1;
						APP_LOG("WiFiApp",LOG_DEBUG,"DST thread removed successfully....");
				}
				else
						APP_LOG("WiFiApp",LOG_ERR,"DST thread removal failed [%d] ....",retVal);
		}
		else
				APP_LOG("WiFiApp",LOG_DEBUG,"DST thread doesn't exist....");

		/* create a thread which sleeps for dstToggleTime and then updates system time */
		pthread_attr_init(&dst_main_attr);
		pthread_attr_setdetachstate(&dst_main_attr,PTHREAD_CREATE_DETACHED);
		retVal = pthread_create(&dstmainthread,&dst_main_attr,
						(void*)&dstComputationThread, (void *)pUpdateDstime);

		if(retVal < 0) {
				APP_LOG("WiFiApp",LOG_ERR,
								"DST main Thread not created");
		}
		else
		{
				APP_LOG("WiFiApp",LOG_DEBUG,
								"DST main  Thread created successfully");
		}

		return 0;
}
#endif

/***
 *  Function to get timezone_index value
 *	from flash
 *
 ******************************************/

int getTimeZoneIndexFromFlash(void)
{
		int Index = 0;
		char *timeZIdx = NULL;

		timeZIdx = GetBelkinParameter("timezone_index");
		if(timeZIdx && strlen(timeZIdx) != 0)
		{
				Index = atoi(timeZIdx);
		}

		APP_LOG("UPNP", LOG_DEBUG,"Index: %d", Index);
		return Index;
}

#ifdef PRODUCT_WeMo_NetCam
extern int IsTimeZoneSetupByNetCam();
extern int UpdateNetCamTimeZoneInfo(const char* szRegion);
int GetTimezoneRegionIndex(float iTimeZoneValue)
{
		printf("Time zone: look for table of %f\n", iTimeZoneValue);
		int counter = sizeof(g_tTimeZoneList)/sizeof(tTimeZone);
		int loop = 0x00;
		int index = -1;

		for (; loop < counter; loop++)
		{
				float delta = iTimeZoneValue - g_tTimeZoneList[loop].iTimeZone;

				if ((delta <= 0.001) && (delta >= -0.001))
				{
						index = loop;
						break;
				}
		}

		if (loop == counter)
				printf("Can not find: %f\n", iTimeZoneValue);

		return index;
}
#endif

/***
 *
 *
 *
 *
 *
 *
 ******************************************/
int SyncTime(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	int rect;

#ifdef PRODUCT_WeMo_NetCam
		if (IsTimeZoneSetupByNetCam())
		{
				APP_LOG("TimeZone", LOG_DEBUG, "TimeZone set already from NetCam");
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x00;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SyncTime", CtrleeDeviceServiceType[PLUGIN_E_TIME_SYNC_SERVICE],"status", "TimeZone updated already from NetCam");
				return 0x00;
		}
#endif

		int isLocalDstSupported = LOCAL_DST_SUPPORTED_ON;
#ifndef _OPENWRT_
		int index = 0;
#endif

		if (0x00 == pActionRequest)
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = PLUGIN_ERROR_E_TIME_SYNC;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SyncTime", CtrleeDeviceServiceType[PLUGIN_E_TIME_SYNC_SERVICE],"status", "Parameters Error!");
				return PLUGIN_ERROR_E_TIME_SYNC;
		}

		char* szUtc 		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "UTC");
		char* szTimeZone	= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "TimeZone");
		char* szDst			= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "dst");
		//- The read the local phone, dst is supported now or not,
		char* szIsLocalDst	= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "DstSupported");
		char ltimezone[SIZE_16B];

		int startTime = time(0);

		memset(ltimezone, 0x0, sizeof(ltimezone));

		APP_LOG("UPNP: Device",LOG_DEBUG,"set time: utc: %s, timezone: %s, dst: %s", szUtc, szTimeZone, szDst);

		if (0x00 == szUtc || 0x00 == szTimeZone || 0x00 == szDst)
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = PLUGIN_ERROR_E_TIME_SYNC;

				/* block is entered even when one of the parameters is NULL, free valid memory */
				FreeXmlSource(szUtc);
				FreeXmlSource(szTimeZone);
				FreeXmlSource(szDst);
				FreeXmlSource(szIsLocalDst);

				UpnpAddToActionResponse(&pActionRequest->ActionResult, "TimeSync", CtrleeDeviceServiceType[PLUGIN_E_TIME_SYNC_SERVICE],"status", "failure");
				APP_LOG("UPNP: Device", LOG_DEBUG, "paramters error");

				return 0x01;
		}

		float TimeZone = 0.0;

		int utc 			= atoi(szUtc);

		//- atof not working well under this compiler, so calculated manually
		if (szTimeZone[0x00] == '-')
		{
				TimeZone 	= atoi(szTimeZone);
				if (0x00!= strstr(szTimeZone, ".5"))
				{
						TimeZone -= 0.5;
				}
				//[WEMO-26944] - szTimeZone could be rounded up to ".8"
				else if ((0x00 != strstr(szTimeZone, ".75")) || (0x00 != strstr(szTimeZone, ".8")))
				{
						TimeZone += 0.25;
				}
		}
		else
		{
				TimeZone 	= atoi(szTimeZone);
				if (0x00!= strstr(szTimeZone, ".5"))
				{
						TimeZone += 0.5;
				}
				//[WEMO-26944] - szTimeZone could be rounded up to ".8"
				else if ((0x00 != strstr(szTimeZone, ".75")) || (0x00 != strstr(szTimeZone, ".8")))
				{
						TimeZone += 0.75;
				}
		}


		int dst			= atoi(szDst);
#ifdef PRODUCT_WeMo_NetCam
		if (DST_ON == dst)
		{
				TimeZone = TimeZone - 1.0;
		}
#endif

		if (!IsApplyTimeSync(utc, TimeZone, dst))
		{
				//- UPnP response here with failure
				APP_LOG("UPNP: Device", LOG_DEBUG, ": *****NOT APPLYING TIME SYNC");
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "TimeSync", CtrleeDeviceServiceType[PLUGIN_E_TIME_SYNC_SERVICE],"status", "failure");
				FreeXmlSource(szUtc);
				FreeXmlSource(szTimeZone);
				FreeXmlSource(szDst);
				FreeXmlSource(szIsLocalDst);

				return 0x01;
		}

		//- Get the local Dst supported flag
		if ( (0x00 != szIsLocalDst) &&
						0x00!= strlen(szIsLocalDst) )
		{
				isLocalDstSupported = atoi(szIsLocalDst);
				gDstSupported = isLocalDstSupported;
				SetBelkinParameter(SYNCTIME_DSTSUPPORT, szIsLocalDst);

				if(!gDstSupported)
				{
					UnSetBelkinParameter(LASTDSTENABLE);
				}
		}


#ifdef PRODUCT_WeMo_NetCam
		g_lastTimeZone = TimeZone;
		gNTPTimeSet = 1;
		UDS_SetNTPTimeSyncStatus();
		g_lastTimeSync = utc;

		int iRegionIndex = GetTimezoneRegionIndex(g_lastTimeZone);

		if (-1 == iRegionIndex)
		{
				iRegionIndex = DEFAULT_REGION_INDEX;
		}
		printf("Time zone: region index is %d\n", iRegionIndex);

		UpdateNetCamTimeZoneInfo(g_tTimeZoneList[iRegionIndex].szLinuxRegion);
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "SyncTime", CtrleeDeviceServiceType[PLUGIN_E_TIME_SYNC_SERVICE],"status", "TimeZone updated already from NetCam");
		FreeXmlSource(szUtc);
		FreeXmlSource(szTimeZone);
		FreeXmlSource(szDst);
		FreeXmlSource(szIsLocalDst);

		// system time is correctly set to GMT/UTC on OpenWRT so this unneeded
		if (isLocalDstSupported)
		{
			computeDstToggleTime(1);
		}

		/* call is to propagate DST information to PJ lib */
		char lzFlashTimezone[64] = {0x00};
		sprintf(lzFlashTimezone, "%f", g_lastTimeZone);
		SetBelkinParameter(SYNCTIME_LASTTIMEZONE, lzFlashTimezone);
		if (!isLocalDstSupported)
		{
			gTimeZoneUpdated = 1;
		}

		return 0x00;
#endif

		//[WEMO-26944] - Adjust TimeZone in case Chatham Isl DST ON
		int iTimeZoneIndex = 0;
		int adjTime = 0;
		if (TimeZone == NZ_TIMEZONE_2 + 1)
		{
				//No index for timezone NZ_TIMEZONE_2 + 1, so adjust system-time when calling SetTime()
				iTimeZoneIndex = GetTimeZoneIndex(TimeZone - 1);
				adjTime = NUM_SECONDS_IN_HOUR;
		}
		else
		{
				iTimeZoneIndex = GetTimeZoneIndex(TimeZone);
				adjTime = 0;
		}
		APP_LOG("UPNP: Device",LOG_DEBUG,"set time: utc: %d, timezone: %f, additional time: %d, dst: %d", utc, TimeZone, adjTime, dst);

#ifndef _OPENWRT_
		rect = SetNTP(s_szNtpServer, iTimeZoneIndex, 0x00);

		if (Gemtek_Success == rect)
		{
				APP_LOG("UPNP: Device", LOG_DEBUG, "set NTP server success: %s", s_szNtpServer);
		}
		else
		{
				APP_LOG("UPNP: Device", LOG_DEBUG, "set NTP server unsuccess: %s", s_szNtpServer);
		}
#endif

		pluginUsleep(500000);

    int lapTime = time(0);
    int timeDiff = lapTime-startTime;

    if(timeDiff > 0) {
	    //APP_LOG("UPNP: Device",LOG_DEBUG,"additional Time: %d", timeDiff);
        utc += timeDiff;
    }
		APP_LOG("UPNP: Device",LOG_DEBUG,"setTime Adjusted utc: %d, timezoneIndex: %d, dst: %d", utc, iTimeZoneIndex, dst);

#ifdef _OPENWRT_
	 if(timeDiff > 10) {
	 // Sanity check.  We asked to sleep for .5 seconds above so we MAY
	 // have actually overslept by a little, but more than 10 seconds
	 // isn't believable, what probably happened was that we got two
	 // Timesync requests from the App and the first one set the time
	 // so ... bail
		 APP_LOG("UPNP: DEVICE",LOG_ERR,"timeDiff: %d, bailing",timeDiff);
	 }
	 else {
		 rect = SetTimeAndTZ((time_t) utc,szTimeZone,dst,gDstSupported);
	 }
#else
	 //[WEMO-26944] - Add adjTime to utc to cover wrong time in Chatham Isl time-zone
	rect = SetTime(utc + adjTime, iTimeZoneIndex, dst);
#endif
	if (Gemtek_Success == rect)
	{
		UpdateMobileTimeSync(0x01);		//-Set flag to true
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = UPNP_E_SUCCESS;

				gNTPTimeSet = 1;
				UDS_SetNTPTimeSyncStatus();
				g_lastTimeSync = utc;

				if (DST_ON == dst)
				{
						// calculate the absolute one
						{
								g_lastTimeZone = TimeZone - 1;
						}
				}
				else
				{
						//- Keep unchange
						g_lastTimeZone = TimeZone;
				}
				snprintf(ltimezone, sizeof(ltimezone), "%f", g_lastTimeZone);
#ifndef _OPENWRT_
			// This is only needed on Gemtek builds, on OpenWRT the system
			// clock is set correctly so the local modifications to
			// pjlib's os_time_unix.c to compensate for the system time being
			// set to local time are not longer needed or applied.

				/* call is to propagate DST information to PJ lib */
				index = getTimeZoneIndexFromFlash();
				APP_LOG("UPNP: Device", LOG_DEBUG,"set pj dst data index: %d, lTimeZone:%s gDstEnable: %d success", index, ltimezone, gDstEnable);
				UDS_pj_dst_data_os(index, ltimezone, gDstEnable);
				SetBelkinParameter (SYNCTIME_LASTTIMEZONE, ltimezone);
#endif
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "TimeSync", CtrleeDeviceServiceType[PLUGIN_E_TIME_SYNC_SERVICE],"status", "success");
				APP_LOG("UPNP: Device", LOG_CRIT,"set time: utc: %s, timezone: %s, dst: %s g_lastTimeZone:%f success", szUtc, szTimeZone, szDst,g_lastTimeZone);

				//- only if isLocalDstSupported true, the calculate and toggle?
				if (isLocalDstSupported)
				{
						//- Arizona and Hawii will not?
#ifndef _OPENWRT_
		// system time is correctly set to GMT/UTC on OpenWRT so this unneeded
						computeDstToggleTime(1);
#endif
				}
				else
					gTimeZoneUpdated = 1;

				AsyncSaveData();
#ifdef PRODUCT_WeMo_Insight
				APP_LOG("UPNP: Device", LOG_ERR,"Restarting Data export scheduler on time sync");
				g_ReStartDataExportScheduler = 1;
				ReStartDataExportScheduler();
#endif
		}
		else
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = PLUGIN_ERROR_E_TIME_SYNC;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "TimeSync", CtrleeDeviceServiceType[PLUGIN_E_TIME_SYNC_SERVICE],"status", "failure");
				APP_LOG("UPNP: Device", LOG_ERR,"set time: utc: %s, timezone: %s, dst: %s failure", szUtc, szTimeZone, szDst);
		}

		FreeXmlSource(szUtc);
		FreeXmlSource(szTimeZone);
		FreeXmlSource(szDst);
		FreeXmlSource(szIsLocalDst);

		return  pActionRequest->ErrCode;
}

int DeviceActionResponse(pUPnPActionRequest pActionRequest,
				const char* responseName,
				const char* serviceType,
				const char* variabName,
				const char *variableValue
				)
{
		UpnpAddToActionResponse(&pActionRequest->ActionResult, responseName, serviceType, variabName, variableValue);
		return 0;
}

/**
 * Get Friendly Name:
 * 	Callback to get device friendly name
 *
 *
 * *****************************************************************************************************************/
int GetFriendlyName(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		APP_LOG("UPNP: Device", LOG_DEBUG, "%s, called", __FUNCTION__);
#ifdef PRODUCT_WeMo_NetCam
		AsyncRequestNetCamFriendlyName();
#endif //PRODUCT_WeMo_NetCam
		char *szFriendlyName = GetDeviceConfig("FriendlyName");

		APP_LOG("UPNP: Device", LOG_DEBUG, "Read name from flash: %s", szFriendlyName);

		if (pActionRequest == 0x00 || pActionRequest == 0x00)
		{
				APP_LOG("UPNP: Device", LOG_ERR, "UPNP parameter failure");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetFriendlyName", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"FriendlyName", g_szFriendlyName);


		APP_LOG("UPNP: Device", LOG_DEBUG,"Get friendly name: %s", g_szFriendlyName);
		return UPNP_E_SUCCESS;
}
int SetHomeId(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char* szHomeId = NULL;
		char homeId[SIZE_20B];
		if (pActionRequest == 0x00)
		{
				APP_LOG("UPNP: Device", LOG_ERR, "UPNP parameter failure");
				return UPNP_E_INVALID_PARAM;
		}

		szHomeId = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "HomeId");
		if( (szHomeId!=NULL) && (0x00 != strlen(szHomeId)) && !strcmp(g_szRestoreState, "0"))
		{
				memset(homeId, 0x0, sizeof(homeId));
				if(decryptWithMacKeydata(szHomeId, homeId) != SUCCESS)
				{
						APP_LOG("UPNP: Device", LOG_ERR, "set home id decryption failure");
						FreeXmlSource(szHomeId);
						return FAILURE;
				}
				APP_LOG("UPNP: Device", LOG_HIDE, "decrypted home id: %s", homeId);
				SetBelkinParameter("home_id", homeId);
				AsyncSaveData();
				APP_LOG("UPNP: Device", LOG_HIDE, "home id update successfully: %s", homeId);
				/* update the global variable to contain updated home_id */
				memset(g_szHomeId, 0, sizeof(g_szHomeId));
				strncpy(g_szHomeId, homeId, sizeof(g_szHomeId)-1);
				FreeXmlSource(szHomeId);
		}
		else
				APP_LOG("UPNP: Device", LOG_ERR, "UPNP parameter failure or restored device, g_szRestoreState: %s", g_szRestoreState);

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetHomeId", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "HomeId", "SUCCESS");
		APP_LOG("UPNP: Device", LOG_DEBUG, "set home id response sent");

		return UPNP_E_SUCCESS;
}

int SetDeviceId(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		return UPNP_E_SUCCESS;
}

int GetDeviceId(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		return UPNP_E_SUCCESS;
}

int SetSmartDevInfo(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		APP_LOG("UPNP: Device", LOG_DEBUG, "%s, called", __FUNCTION__);

		if (pActionRequest == 0x00 || pActionRequest == 0x00)
		{
				APP_LOG("UPNP: Device", LOG_DEBUG, "UPNP paramter failure");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}
		char* szSmartDevUrl = NULL;
		int retVal = 0;

		szSmartDevUrl = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "SmartDevURL");
		if(szSmartDevUrl!=NULL){
				if (0x00 == strlen(szSmartDevUrl))
				{
						pActionRequest->ActionResult = NULL;
						pActionRequest->ErrCode = PLUGIN_ERROR_E_BASIC_EVENT;
						UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetSmartDevInfo", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"SmartDevURL", "Parameter Error");
						APP_LOG("UPNP: Device", LOG_ERR,"SmartDevURL parameter failure");
						FreeXmlSource(szSmartDevUrl);
						return PLUGIN_ERROR_E_BASIC_EVENT;
				}
		}

		APP_LOG("UPNP: Device",LOG_DEBUG, "SmartDevURL is: %s", szSmartDevUrl);
		retVal = do_download(szSmartDevUrl, SMARTDEVXML);
		if (retVal != 0) {
				APP_LOG("UPNP: Device", LOG_ERR,"Download smart device info xml %s failed\n", szSmartDevUrl);
		}else {
				APP_LOG("UPNP: Device", LOG_NOTICE,"Downloaded smart device info xml from location %s successfully\n", szSmartDevUrl);
		}
		FreeXmlSource(szSmartDevUrl);

		return UPNP_E_SUCCESS;
}

int GetSmartDevInfo(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		APP_LOG("UPNP: Device", LOG_DEBUG, "%s, called", __FUNCTION__);

		if (pActionRequest == 0x00 || pActionRequest == 0x00)
		{
				APP_LOG("UPNP: Device", LOG_DEBUG, "UPNP paramter failure");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		char szSmartDevInfoURL[MAX_FW_URL_LEN];
		memset(szSmartDevInfoURL, 0x00, sizeof(szSmartDevInfoURL));

		if (strlen(g_server_ip) > 0x00)
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x00;
				snprintf(szSmartDevInfoURL, sizeof(szSmartDevInfoURL), "http://%s:%d/smartDev.txt", g_server_ip, g_server_port);
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetSmartDevInfo", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "SmartDevInfo", szSmartDevInfoURL);
		}
		else
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x01;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetSmartDevInfo", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "SmartDevInfo", "");
		}

		APP_LOG("UPNP: Device", LOG_DEBUG,"GetSmartDevInfoURL: %s", szSmartDevInfoURL);

		return UPNP_E_SUCCESS;
}

int GetShareHWInfo(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		APP_LOG("UPNP: Device", LOG_DEBUG, "%s, called", __FUNCTION__);
		char* szMac = NULL;
		char* szSerial = NULL;
		char* szUdn = NULL;
		char* szRestoreState = NULL;
		char* szHomeId = NULL;
		char* szPvtKey = NULL;


		if (pActionRequest == 0x00)
		{
				APP_LOG("UPNP: Device", LOG_ERR, "UPNP parameter failure");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}
		/* to restrict this request if there is already one in process */
		if(g_pxRemRegInf)
		{
				APP_LOG("UPNP: Device", LOG_ERR, "proxy registration already in process");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		/* allocate proxy registration request structure */
		g_pxRemRegInf = (ProxyRemoteAccessInfo *)CALLOC(1, sizeof(ProxyRemoteAccessInfo));
		if(g_pxRemRegInf == NULL)
		{
				APP_LOG("UPNP",LOG_ERR, "proxy RemRegInf mem allocation FAIL");
				return FAILURE;
		}

		szMac = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Mac");
		if(szMac!=NULL)
		{
				if (0x00 == strlen(szMac))
				{
						pActionRequest->ActionResult = NULL;
						pActionRequest->ErrCode = PLUGIN_ERROR_E_REMOTE_ACCESS;
						UpnpAddToActionResponse(&pActionRequest->ActionResult, "ShareHWInfo", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"Mac", "Parameter Error");
						APP_LOG("UPNP: Device", LOG_ERR,"Proxy Mac parameter failure");
						return PLUGIN_ERROR_E_REMOTE_ACCESS;
				}
		}

		szSerial = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Serial");
		if(szSerial!=NULL)
		{
				if (0x00 == strlen(szSerial))
				{
						pActionRequest->ActionResult = NULL;
						pActionRequest->ErrCode = PLUGIN_ERROR_E_REMOTE_ACCESS;
						UpnpAddToActionResponse(&pActionRequest->ActionResult, "ShareHWInfo", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"Serial", "Parameter Error");
						APP_LOG("UPNP: Device", LOG_ERR,"Proxy Serial number parameter failure");
						FreeXmlSource(szMac);
						return PLUGIN_ERROR_E_REMOTE_ACCESS;
				}
		}

		szUdn = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Udn");
		if(szUdn!=NULL)
		{
				if (0x00 == strlen(szUdn))
				{
						pActionRequest->ActionResult = NULL;
						pActionRequest->ErrCode = PLUGIN_ERROR_E_REMOTE_ACCESS;
						UpnpAddToActionResponse(&pActionRequest->ActionResult, "ShareHWInfo", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"Udn", "Parameter Error");
						APP_LOG("UPNP: Device", LOG_ERR,"Proxy Udn parameter failure");
						FreeXmlSource(szMac);
						FreeXmlSource(szSerial);
						return PLUGIN_ERROR_E_REMOTE_ACCESS;
				}
		}

		szRestoreState = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "RestoreState");
		if(szRestoreState!=NULL)
		{
				if (0x00 == strlen(szRestoreState))
				{
						pActionRequest->ActionResult = NULL;
						pActionRequest->ErrCode = PLUGIN_ERROR_E_REMOTE_ACCESS;
						UpnpAddToActionResponse(&pActionRequest->ActionResult, "ShareHWInfo", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"RestoreState", "Parameter Error");
						APP_LOG("UPNP: Device", LOG_ERR,"Proxy Restore State parameter failure");
						FreeXmlSource(szMac);
						FreeXmlSource(szSerial);
						FreeXmlSource(szUdn);
						return PLUGIN_ERROR_E_REMOTE_ACCESS;
				}
		}

		szHomeId = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "HomeId");
		if(szHomeId!=NULL)
		{
				if (0x00 == strlen(szHomeId))
				{
						APP_LOG("UPNP: Device", LOG_ERR,"Proxy Home Id parameter failure");
						FreeXmlSource(szHomeId);
						szHomeId = 0x00;
				}
		}

		szPvtKey = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "PluginKey");
		if(szPvtKey!=NULL)
		{
				if (0x00 == strlen(szPvtKey))
				{
						APP_LOG("UPNP: Device", LOG_ERR,"Proxy Pvt Key parameter failure");
						FreeXmlSource(szPvtKey);
						szPvtKey = 0x00;
				}
		}

		strncpy(g_pxRemRegInf->proxy_macAddress, szMac, sizeof(g_pxRemRegInf->proxy_macAddress)-1);
		strncpy(g_pxRemRegInf->proxy_serialNumber, szSerial, sizeof(g_pxRemRegInf->proxy_serialNumber)-1);
		strncpy(g_pxRemRegInf->proxy_pluginUniqueId, szUdn, sizeof(g_pxRemRegInf->proxy_pluginUniqueId)-1);
		strncpy(g_pxRemRegInf->proxy_restoreState, szRestoreState, sizeof(g_pxRemRegInf->proxy_restoreState)-1);
		if((szHomeId!= NULL) && (strlen(szHomeId)> 0x00))
				strncpy(g_pxRemRegInf->proxy_homeId, szHomeId, sizeof(g_pxRemRegInf->proxy_homeId)-1);
		if((szPvtKey!= NULL) && (strlen(szPvtKey)> 0x00))
				strncpy(g_pxRemRegInf->proxy_privateKey, szPvtKey, sizeof(g_pxRemRegInf->proxy_privateKey)-1);

		APP_LOG("UPNP: Device", LOG_DEBUG,"Proxy MAC: <%s> SERIAL: <%s> UDN: <%s>", g_pxRemRegInf->proxy_macAddress, g_pxRemRegInf->proxy_serialNumber, g_pxRemRegInf->proxy_pluginUniqueId);

		FreeXmlSource(szMac);
		FreeXmlSource(szSerial);
		FreeXmlSource(szUdn);
		FreeXmlSource(szRestoreState);
		if(szHomeId!=NULL)
				FreeXmlSource(szHomeId);
		if(szPvtKey!=NULL)
				FreeXmlSource(szPvtKey);

		return UPNP_E_SUCCESS;
}

int GetMacAddr(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char szPluginUDN[SIZE_128B];
		APP_LOG("UPNP: Device", LOG_DEBUG, "%s, called", __FUNCTION__);

		if (pActionRequest == 0x00 || pActionRequest == 0x00)
		{
				APP_LOG("UPNP: Device", LOG_ERR, "UPNP parameter failure");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}
		char *szMacAddr = g_szWiFiMacAddress;
		if(szMacAddr != NULL)
		{
				if (0x00 == strlen(szMacAddr))
				{
						pActionRequest->ActionResult = NULL;
						pActionRequest->ErrCode = 0x00;
						APP_LOG("UPNP: Device", LOG_ERR,"Failure Get Mac Addr: %s", "");
						return PLUGIN_ERROR_E_BASIC_EVENT;
				}
		}
		char *szSerialNo = g_szSerialNo;
		if(szSerialNo != NULL)
		{
				if (0x00 == strlen(szSerialNo))
				{
						pActionRequest->ActionResult = NULL;
						pActionRequest->ErrCode = 0x00;
						APP_LOG("UPNP: Device", LOG_ERR,"Failure Get Serial: %s", "");
						return PLUGIN_ERROR_E_BASIC_EVENT;
				}
		}
		memset(szPluginUDN, 0x00, sizeof(szPluginUDN));
		strncpy(szPluginUDN, g_szUDN, sizeof(szPluginUDN)-1);
		if(szPluginUDN != NULL)
		{
				if (0x00 == strlen(szPluginUDN))
				{
						pActionRequest->ActionResult = NULL;
						pActionRequest->ErrCode = 0x00;
						APP_LOG("UPNP: Device", LOG_ERR,"Failure Get UDN: %s", "");
						return PLUGIN_ERROR_E_BASIC_EVENT;
				}
		}

		APP_LOG("UPNP: Device", LOG_DEBUG, "Read  from flash Mac Addr: %s Serial No: %s UDN: %s\n", szMacAddr, szSerialNo, szPluginUDN);
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetMacAddr", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"MacAddr", szMacAddr);
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetSerialNo", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"SerialNo", szSerialNo);
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetPluginUDN", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"PluginUDN", szPluginUDN);

		return UPNP_E_SUCCESS;
}
int GetSerialNo(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		return UPNP_E_SUCCESS;
}
int GetPluginUDN(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		return UPNP_E_SUCCESS;
}

int GetHomeId(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{

		APP_LOG("UPNP: Device", LOG_DEBUG, "%s, called", __FUNCTION__);

		APP_LOG("UPNP: Device", LOG_HIDE, "Read homeid from flash: %s\n", g_szHomeId);

		if (pActionRequest == 0x00 || pActionRequest == 0x00)
		{
				APP_LOG("UPNP: Device", LOG_ERR, "UPNP parameter failure");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		//- Zero name
		if (0x00 == strlen(g_szHomeId))
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x00;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetHomeId", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"HomeId", "failure");
				APP_LOG("UPNP: Device", LOG_ERR,"Failure Get home id: %s", "");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetHomeId", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"HomeId", g_szHomeId);
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetDeviceId", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"DeviceId", g_szSmartDeviceId);


		APP_LOG("UPNP: Device", LOG_HIDE,"Get home id: %s", g_szHomeId);

		return UPNP_E_SUCCESS;
}

int GetHomeInfo(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char szResponse[SIZE_512B];
		authSign *assign = NULL;
		char szSign[SIZE_256B];

		memset(szResponse, 0x00, sizeof(szResponse));
		memset(szSign, 0x00, sizeof(szSign));

		if (pActionRequest == 0x00)
		{
				APP_LOG("UPNP: Device", LOG_ERR, "UPNP parameter failure");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		snprintf(szResponse, sizeof(szResponse), "%s|", g_szRestoreState);

		if (0x00 == strlen(g_szHomeId)  ||  (0 == atoi(g_szHomeId)) || (remoteRegthread != -1))
				strncat(szResponse, "NO_HOME", sizeof(szResponse)-strlen(szResponse)-1);
		else
				strncat(szResponse, g_szHomeId, sizeof(szResponse)-strlen(szResponse)-1);

		/* Create & Append the encrypted Auth header as well */

		if(strlen(g_szPluginPrivatekey))
		{
				assign = createAuthSignature(g_szWiFiMacAddress, g_szSerialNo, g_szPluginPrivatekey);
				if (!assign) {
						APP_LOG("UPNP: DEVICE", LOG_ERR, "\n Signature Structure returned NULL\n");
						goto on_failure;
				}

				APP_LOG("UPNP: DEVICE", LOG_HIDE, "###############################Before encryption: Signature: <%s>", assign->signature);
				encryptWithMacKeydata(assign->signature, szSign);
				if (!strlen(szSign))
				{
				    APP_LOG("UPNP: DEVICE", LOG_ERR, "\n Signature Encrypt returned NULL\n");
				    goto on_failure;
				}
				APP_LOG("UPNP: DEVICE", LOG_HIDE, "###############################After encryption: Signature: <%s>", szSign);
		}
		else
		{
				strncpy(szSign, "NO_SIGNATURE", sizeof(szSign)-1);
				APP_LOG("UPNP: DEVICE", LOG_HIDE, "Signature: <%s>", szSign);
		}
		strncat(szResponse, "|", sizeof(szResponse)-strlen(szResponse)-1);
		strncat(szResponse, szSign, sizeof(szResponse)-strlen(szResponse)-1);
		strncat(szResponse, "|", sizeof(szResponse)-strlen(szResponse)-1);

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetHomeInfo", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"HomeInfo", szResponse);
		APP_LOG("UPNP: Device", LOG_HIDE,"Get home info: %s", szResponse);

		if (assign) {free(assign); assign = NULL;};
		return UPNP_E_SUCCESS;

on_failure:
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = PLUGIN_ERROR_E_BASIC_EVENT;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetHomeInfo", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"HomeInfo", "failure");
		APP_LOG("UPNP: Device", LOG_ERR,"Failure in Get home info");
		if (assign) {free(assign); assign = NULL;};
		return PLUGIN_ERROR_E_BASIC_EVENT;

}

void sendRemoteUpnpSuccessResponse(pUPnPActionRequest pActionRequest, pluginRegisterInfo *pPlgReg)
{
		APP_LOG("UPnP: Device", LOG_ERR, "Sending remote access success upnp response");
		char homeId[MAX_RVAL_LEN];

		memset(homeId, 0, sizeof(homeId));
		snprintf(homeId, sizeof(homeId), "%lu", pPlgReg->pluginDevInf.homeId);

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		if(0x00 != strlen(homeId))
		{
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"homeId",homeId);
		}
		if (0x00 != pPlgReg->pluginDevInf.privateKey && (0x01 < strlen(pPlgReg->pluginDevInf.privateKey)))
		{
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"pluginprivateKey", pPlgReg->pluginDevInf.privateKey);
		}
		if (0x00 != pPlgReg->smartDevInf.privateKey && (0x01 < strlen(pPlgReg->smartDevInf.privateKey)))
		{
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"smartprivateKey", pPlgReg->smartDevInf.privateKey);
		}
		else
		{
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"smartprivateKey", "NOKEY");
		}
		if(pPlgReg->resultCode!=NULL){
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"resultCode", pPlgReg->resultCode);
		}
		if(pPlgReg->description!=NULL){
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"description", pPlgReg->description);
		}
		if(pPlgReg->statusCode!=NULL){
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"statusCode", pPlgReg->statusCode);
		}
		if(pPlgReg->smartDevInf.smartUniqueId!=NULL){
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"smartUniqueId", pPlgReg->smartDevInf.smartUniqueId);
		}


		UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"ArpMac", gGatewayMAC);

		APP_LOG("UPnP: Device", LOG_ERR, "Remote access success upnp response sent");
}

void sendRemoteUpnpErrorResponse(pUPnPActionRequest pActionRequest, const char *pdesc, const char *pcode)
{
		APP_LOG("UPnP: Device", LOG_ERR, "Sending remote access error upnp response");
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = PLUGIN_ERROR_E_REMOTE_ACCESS;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"homeId","FAIL");
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"pluginprivateKey", "FAIL");
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"smartprivateKey", "FAIL");
		if (pdesc) {
				APP_LOG("UPnP: Device", LOG_ERR, "ERROR: Remote response message and re-register message %s\n", pdesc);
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"description", pdesc);
		}else{
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"description", "FAIL");
		}
		if (pcode) {
				APP_LOG("UPnP: Device", LOG_ERR, "ERROR: Remote re-register error code %s\n", pcode);
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"resultCode", pcode);
		}else{
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"resultCode", "FAIL");
		}
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "RemoteAccess", CtrleeDeviceServiceType[PLUGIN_E_REMOTE_ACCESS_SERVICE],"statusCode", "F");
		APP_LOG("UPnP: Device", LOG_ERR, "Remote access error upnp response sent");
}

void freeRemoteXMLRes(char* pDeviceId, char *pHomeId, char *pDevicePrivKey, char *pMacAddress, char *pDeviceName)
{
		FreeXmlSource(pHomeId);
		FreeXmlSource(pMacAddress);
		FreeXmlSource(pDeviceId);
		FreeXmlSource(pDeviceName);
		FreeXmlSource(pDevicePrivKey);
}

int createAutoRegThread(void)
{
		int retVal = SUCCESS;

		if ( (-1 != remoteRegthread) || (-1 != firmwareUpThread) )
		{
				APP_LOG("UPNP: Device", LOG_ERR, "############registration or firmware update thread already running################");
				APP_LOG("UPNP: Device", LOG_CRIT, "Registration or firmware update thread already running");
				retVal = FAILURE;
				return retVal;
		}

		pthread_attr_init(&remoteReg_attr);
		pthread_attr_setdetachstate(&remoteReg_attr,PTHREAD_CREATE_DETACHED);
		retVal = pthread_create(&remoteRegthread,&remoteReg_attr,(void*)&remoteRegThread, NULL);
		if(retVal < SUCCESS) {
				APP_LOG("UPnPApp",LOG_ERR, "************registration thread not Created");
				APP_LOG("UPnPApp",LOG_CRIT, "Registration thread not Created");
				retVal = FAILURE;
				return retVal;
		}

		APP_LOG("UPnPApp",LOG_DEBUG, "************registration thread Created");
		APP_LOG("UPnPApp",LOG_CRIT, "Registration thread Created");
		return retVal;
}

int createRemoteRegThread(RemoteAccessInfo remRegInf)
{
		int retVal = SUCCESS;
		RemoteAccessInfo *premRegInf = NULL;

		if ( (-1 != remoteRegthread) || (-1 != firmwareUpThread) )
		{
				APP_LOG("UPNP: Device", LOG_ERR, "############remote registration or firmware update thread already running################");
				APP_LOG("UPNP: Device", LOG_CRIT, "Remote registration or firmware update thread already running");
				retVal = FAILURE;
				return retVal;
		}

		premRegInf = (RemoteAccessInfo*)CALLOC(1, sizeof(RemoteAccessInfo));
		if(!premRegInf)
		{
				APP_LOG("UPNPApp",LOG_DEBUG,"calloc failed...");
				retVal = FAILURE;
				return retVal;
		}

		//*premRegInf = remRegInf;
		memcpy(premRegInf, (RemoteAccessInfo *)&remRegInf, sizeof(RemoteAccessInfo));

		/* set remote access by mobile App flag */
		g_isRemoteAccessByApp = 0x1;

		pthread_attr_init(&remoteReg_attr);
		pthread_attr_setdetachstate(&remoteReg_attr,PTHREAD_CREATE_DETACHED);
		retVal = pthread_create(&remoteRegthread,&remoteReg_attr,(void*)&remoteRegThread, (void*)premRegInf);
		if(retVal < SUCCESS) {
				APP_LOG("UPnPApp",LOG_ERR, "************remote registration thread not Created");
				APP_LOG("UPnPApp",LOG_CRIT, "Remote registration thread not Created");
				retVal = FAILURE;
				return retVal;
		}

		APP_LOG("UPnPApp",LOG_DEBUG, "************registration thread Created");
		APP_LOG("UPnPApp",LOG_CRIT, "Registration thread Created");
		return retVal;
}

void initRemoteAccessLock(void)
{
		pthread_cond_init (&g_remoteAccess_cond, NULL);
		pthread_mutex_init(&g_remoteAccess_mutex, NULL);
#ifndef __ORESETUP__
		pthread_mutex_init(&g_remoteDeReg_mutex, NULL);
		pthread_cond_init (&g_remoteDeReg_cond, NULL);
#endif
}

/**
 * RemoteAccess:
 * 	Callback to Set Remote Access
 *
 *
 * *****************************************************************************************************************/
int RemoteAccess(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		pluginRegisterInfo *pPlgReg = NULL;
		RemoteAccessInfo remAcInf;
		char* szMacAddress = NULL;
		char* szDevicePrivKey = NULL;
		char* szDeviceId = NULL;
		char* szDeviceName = NULL;
		char* szHomeId = NULL;
		int ret = SUCCESS;

		APP_LOG("UPNPDevice", LOG_ERR,"Cloud Connection: NO REMOTE");

		/* sanitize remote access request parameters */
		if (pActionRequest == 0x00)
		{
				APP_LOG("UPNP: Device", LOG_ERR,"INVALID PARAMETERS");
				sendRemoteUpnpErrorResponse(pActionRequest, "parameter failure",  "FAIL");
				goto on_return;
		}

#ifdef WeMo_SMART_SETUP
		/* Reset smart setup progress to allow responses to be sent back to App */
		gSmartSetup = 0;
#endif
		memset(&remAcInf, 0x00, sizeof(RemoteAccessInfo));

		szDeviceId = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "DeviceId");
		if(0x0 != szDeviceId && 0x00 == strlen(szDeviceId))
		{
				APP_LOG("UPNP: Device", LOG_ERR,"smart device udid param is null");
				sendRemoteUpnpErrorResponse(pActionRequest, "deviceid parameter failure",  "FAIL");
				goto on_return;
		}

		APP_LOG("UPNPDevice", LOG_HIDE,"SMART DEVICE ID: %s received in upnp action request", szDeviceId);
		strncpy(remAcInf.smartDeviceId, szDeviceId, sizeof(remAcInf.smartDeviceId)-1);

		szDeviceName = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "DeviceName");
		if(0x0 != szDeviceName && 0x00 != strlen(szDeviceName))
		{
				APP_LOG("UPNP: Device", LOG_ERR,"smart device name is: %s", szDeviceName);
				strncpy(remAcInf.smartDeviceName, szDeviceName, sizeof(remAcInf.smartDeviceName)-1);
		}
		else
		{
				APP_LOG("UPNP: Device", LOG_ERR,"smart device name param is null");
		}


		szHomeId = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "HomeId");
		if (0x00 != szHomeId && (0x00 != strlen(szHomeId)))
				strncpy(remAcInf.homeId, szHomeId, sizeof(remAcInf.homeId)-1);

		/* proxy registration */
		if(!strcmp(szDeviceName, PLUGIN_REMOTE_REQ))
		{
				szMacAddress = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "MacAddr");
				if(0x0 != szMacAddress && 0x00 == strlen(szMacAddress))
				{
						APP_LOG("UPNP: Device", LOG_ERR,"mac parameter in proxy remote request is null");
						sendRemoteUpnpErrorResponse(pActionRequest, "mac parameter failure",  "FAIL");
						goto on_return;
				}
				/* check for hw info */
				if(!g_pxRemRegInf)
				{
						APP_LOG("UPNP: Device", LOG_DEBUG,"Failed in getting HW info of new plugin......");
						sendRemoteUpnpErrorResponse(pActionRequest, "hw info failure",  "FAIL");
						goto on_return;
				}
				else
				{
						/* check for mac, should be same in remote request and hw info received */
						if(strcmp(szMacAddress, g_pxRemRegInf->proxy_macAddress))
						{
								APP_LOG("UPNP: Device", LOG_DEBUG,"Share hardware info MacAddr and RemoteAccess MacAddr are different, Not sending remote req");
								sendRemoteUpnpErrorResponse(pActionRequest, "mac mismatch failure",  "FAIL");
								goto on_return;
						}
						else
						{
								APP_LOG("UPNP: Device", LOG_DEBUG,"Processing proxy registration request");
						}
				}
		}
		/* smart device key from App */
		else
		{
				szDevicePrivKey = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "smartprivateKey");
				if (0x00 != szDevicePrivKey && (0x00 != strlen(szDevicePrivKey)))
				{
						APP_LOG("UPNP: Device", LOG_HIDE,"smart device private key received in request: %s", szDevicePrivKey);
						strncpy(remAcInf.smartDevicePrivateKey, szDevicePrivKey, sizeof(remAcInf.smartDevicePrivateKey)-1);
				}
				else
				{
						APP_LOG("UPNP: Device", LOG_DEBUG,"smart device private key received in request is null");
				}
		}

		/* defer request for network reachability */
		pluginUsleep(2000000);

		/* create remote registration thread */
		ret = createRemoteRegThread(remAcInf);
		if(ret != SUCCESS)
		{
				sendRemoteUpnpErrorResponse(pActionRequest, "Fw upd or reg under process, mem failure",  "FAIL");
				goto on_return;
		}

		/* wait here for response */
		pthread_mutex_lock(&g_remoteAccess_mutex);
		pthread_cond_wait(&g_remoteAccess_cond,&g_remoteAccess_mutex);
		pthread_mutex_unlock(&g_remoteAccess_mutex);

		if(pgPluginReRegInf != NULL)
		{
				/* success response */
				if(!strcmp(pgPluginReRegInf->statusCode, "S"))
				{
						APP_LOG("UPNPDevice", LOG_ERR,"Cloud Connection: AUTHENTICATED");
						pPlgReg = pgPluginReRegInf;
				}
				/* failure response */
				else
				{
						sendRemoteUpnpErrorResponse(pActionRequest, pgPluginReRegInf->resultCode,  pgPluginReRegInf->description);
						goto on_return;
				}
		}
		/* null response */
		else
		{
				sendRemoteUpnpErrorResponse(pActionRequest, NULL,  NULL);
				goto on_return;
		}

		/* send success response */
		sendRemoteUpnpSuccessResponse(pActionRequest, pPlgReg);


on_return:

		/* free registration response & proxy registration request & xml res */
		if (pgPluginReRegInf) {free(pgPluginReRegInf); pgPluginReRegInf = NULL;}
		if (g_pxRemRegInf) {free(g_pxRemRegInf); g_pxRemRegInf = NULL;}
		freeRemoteXMLRes(szHomeId, szMacAddress, szDeviceId, szDeviceName, szDevicePrivKey);
		/* reset remote request by App flag */
		g_isRemoteAccessByApp = 0;
		return UPNP_E_SUCCESS;
}

/**
 * SetIcon:
 * 	Callback to Get icon URL
 *
 *
 * *****************************************************************************************************************/
int SetFriendlyName(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		if (pActionRequest == 0x00 || pActionRequest == 0x00)
		{
				APP_LOG("UPNP: Device", LOG_DEBUG,"Set friendly name: paramter failure");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		char* szFriendlyName = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "FriendlyName");
		if (0x00 == strlen(szFriendlyName))
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = PLUGIN_ERROR_E_BASIC_EVENT;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetFriendlyName", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"status", "Parameters Error");
				APP_LOG("UPNP: Device", LOG_ERR,"Set friendly name: failure");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetFriendlyName", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "FriendlyName", szFriendlyName);

		strncpy(g_szFriendlyName, szFriendlyName, sizeof(g_szFriendlyName)-1);
		APP_LOG("UPNP: Device", LOG_DEBUG,"Set friendly name: %s", szFriendlyName);
		FreeXmlSource(szFriendlyName);

		SetBelkinParameter("FriendlyName", g_szFriendlyName);
		UpdateXML2Factory();
		/* Issue 2894 */
		AsyncSaveData();
		remoteAccessUpdateStatusTSParams (0xFF);

		return UPNP_E_SUCCESS;

}

/**
 * GetIcon:
 * 	Callback to Get icon URL
 *
 *
 * *****************************************************************************************************************/
int GetIcon(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char szIconURL[MAX_FW_URL_LEN];
		memset(szIconURL, 0x00, sizeof(szIconURL));

		//-Return the icon path of the device
		if (strlen(g_server_ip) > 0x00)
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x00;
				snprintf(szIconURL, sizeof(szIconURL), "http://%s:%d/icon.jpg", g_server_ip, g_server_port);
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetIconURL", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "URL", szIconURL);
		}
		else
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x01;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetIconURL", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "URL", "");
		}

		APP_LOG("UPNP: Device", LOG_DEBUG,"GetIcon: %s", szIconURL);

		return UPNP_E_SUCCESS;
}

/**
 * GetIconVersion:
 * 	Callback to Get Icon Version
 *
 *
 * *****************************************************************************************************************/
int GetIconVersion(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char szIconVersion[SIZE_4B];

		memset(szIconVersion, 0, SIZE_4B);

		if (pActionRequest == 0x00 || pActionRequest == 0x00)
		{
				APP_LOG("UPNP: Device", LOG_DEBUG,"GetIconVersion : parameter failure");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		snprintf(szIconVersion, sizeof(szIconVersion), "%d", gWebIconVersion);
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		APP_LOG("UPNP: Device", LOG_DEBUG, "Icon version:%s", szIconVersion);

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetIconVersion", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "IconVersion", szIconVersion);

		return UPNP_E_SUCCESS;
}


typedef struct time_sync_args
{
	char szUtc[SIZE_16B];
	char szTimeZone[SIZE_8B];
	char szDst[SIZE_4B];
	char szIsLocalDst[SIZE_4B];
} STimeSyncArgs;

int timeSyncThread(void *tmp)
{
	int isLocalDstSupported = LOCAL_DST_SUPPORTED_ON;
	int rect;
#ifndef _OPENWRT_
	int index = 0;
#endif
	float TimeZone = 0.0;
	STimeSyncArgs *args = (STimeSyncArgs *)tmp;
	//- The read the local phone, dst is supported now or not,
	char ltimezone[SIZE_16B];
	int utc = 0;
	int startTime = time(0);
	memset(ltimezone, 0x0, sizeof(ltimezone));

	if(!args)
	{
		APP_LOG("UPNP: Device", LOG_ERR, "Invalid argument");
		timesyncThread = -1;
		return FAILURE;
	}

	char *szTimeZone = args->szTimeZone;
	char *szUtc = args->szUtc;
	char *szDst = args->szDst;
	char *szIsLocalDst = args->szIsLocalDst;

	utc = atoi(szUtc);

	//- atof not working well under this compiler, so calculated manually
	if (szTimeZone[0x00] == '-')
	{
		TimeZone 	= atoi(szTimeZone);
		if (0x00!= strstr(szTimeZone, ".5"))
		{
			TimeZone -= 0.5;
		}
		//[WEMO-26944] - szTimeZone could be rounded up to ".8"
		else if ((0x00 != strstr(szTimeZone, ".75")) || (0x00 != strstr(szTimeZone, ".8")))
		{
			TimeZone += 0.25;
		}
	}
	else
	{
		TimeZone 	= atoi(szTimeZone);
		if (0x00!= strstr(szTimeZone, ".5"))
		{
			TimeZone += 0.5;
		}
		//[WEMO-26944] - szTimeZone could be rounded up to ".8"
		else if ((0x00 != strstr(szTimeZone, ".75")) || (0x00 != strstr(szTimeZone, ".8")))
		{
			TimeZone += 0.75;
		}
	}


	int dst			= atoi(szDst);
#ifdef PRODUCT_WeMo_NetCam
	if (DST_ON == dst)
	{
		TimeZone = TimeZone - 1.0;
	}
#endif

	if (!IsApplyTimeSync(utc, TimeZone, dst))
	{
		//- UPnP response here with failure
		APP_LOG("UPNP: Device", LOG_DEBUG, ": *****NOT APPLYING TIME SYNC");
		free(args);
		timesyncThread = -1;
		return 0x01;
	}

	//- Get the local Dst supported flag
	if ( (0x00 != szIsLocalDst) &&
			0x00!= strlen(szIsLocalDst) )
	{
		isLocalDstSupported = atoi(szIsLocalDst);
		gDstSupported = isLocalDstSupported;
		SetBelkinParameter(SYNCTIME_DSTSUPPORT, szIsLocalDst);

		if(!gDstSupported)
		{
			UnSetBelkinParameter(LASTDSTENABLE);
		}
	}


#ifdef PRODUCT_WeMo_NetCam
	g_lastTimeZone = TimeZone;
	gNTPTimeSet = 1;
	UDS_SetNTPTimeSyncStatus();
	g_lastTimeSync = utc;

	int iRegionIndex = GetTimezoneRegionIndex(g_lastTimeZone);

	if (-1 == iRegionIndex)
	{
		iRegionIndex = DEFAULT_REGION_INDEX;
	}
	printf("Time zone: region index is %d\n", iRegionIndex);

	UpdateNetCamTimeZoneInfo(g_tTimeZoneList[iRegionIndex].szLinuxRegion);
	if (isLocalDstSupported)
	{
		computeDstToggleTime(1);
	}

	/* call is to propagate DST information to PJ lib */
	char lzFlashTimezone[64] = {0x00};
	sprintf(lzFlashTimezone, "%f", g_lastTimeZone);
	SetBelkinParameter(SYNCTIME_LASTTIMEZONE, lzFlashTimezone);
	if (!isLocalDstSupported)
	{
		gTimeZoneUpdated = 1;
	}

	free(args);
	timesyncThread = -1;
	APP_LOG("UPNP: Device", LOG_DEBUG, "Time sync thread exiting");
	return 0x00;
#endif

	//[WEMO-26944] - Adjust TimeZone in case Chatham Isl DST ON
	int iTimeZoneIndex = 0;
	int adjTime = 0;
	if (TimeZone == NZ_TIMEZONE_2 + 1)
	{
		//No index for timezone NZ_TIMEZONE_2 + 1, so adjust system-time when calling SetTime()
		iTimeZoneIndex = GetTimeZoneIndex(TimeZone - 1);
		adjTime = NUM_SECONDS_IN_HOUR;
	}
	else
	{
		iTimeZoneIndex = GetTimeZoneIndex(TimeZone);
		adjTime = 0;
	}
	APP_LOG("UPNP: Device",LOG_DEBUG,"set time: utc: %d, timezone: %f, additional time: %d, dst: %d", utc, TimeZone, adjTime, dst);

#ifndef _OPENWRT_
	int rect = SetNTP(s_szNtpServer, iTimeZoneIndex, 0x00);

	if (Gemtek_Success == rect)
	{
		APP_LOG("UPNP: Device", LOG_DEBUG, "set NTP server success: %s", s_szNtpServer);
	}
	else
	{
		APP_LOG("UPNP: Device", LOG_DEBUG, "set NTP server unsuccess: %s", s_szNtpServer);
	}
#endif

	pluginUsleep(500000);

	int lapTime = time(0);
	int timeDiff = lapTime-startTime;

	if(timeDiff > 0) {
		//APP_LOG("UPNP: Device",LOG_DEBUG,"additional Time: %d", timeDiff);
		utc += timeDiff;
	}
	APP_LOG("UPNP: Device",LOG_DEBUG,"setTime Adjusted utc: %d, timezoneIndex: %d, dst: %d", utc, iTimeZoneIndex, dst);

#ifdef _OPENWRT_
	if(timeDiff > 10) {
	// Sanity check.  We asked to sleep for .5 seconds above so we MAY
	// have actually overslept by a little, but more than 10 seconds
	// isn't believable, what probably happened was that we got two
	// Timesync requests from the App and the first one set the time
	// so ... bail
		APP_LOG("UPNP: DEVICE",LOG_ERR,"timeDiff: %d, bailing",timeDiff);
	}
	else {
		rect = SetTimeAndTZ((time_t) utc,szTimeZone,dst,gDstSupported);
	}
#else
	 //[WEMO-26944] - Add adjTime to utc to cover wrong time in Chatham Isl time-zone
	rect = SetTime(utc + adjTime, iTimeZoneIndex, dst);
#endif
	if (Gemtek_Success == rect)
	{
		UpdateMobileTimeSync(0x01);		//-Set flag to true

		gNTPTimeSet = 1;
		UDS_SetNTPTimeSyncStatus();
		g_lastTimeSync = utc;

		if (DST_ON == dst)
		{
			// calculate the absolute one
			{
				g_lastTimeZone = TimeZone - 1;
			}
		}
		else
		{
			//- Keep unchange
			g_lastTimeZone = TimeZone;
		}

		snprintf(ltimezone, sizeof(ltimezone), "%f", g_lastTimeZone);
#ifndef _OPENWRT_
			// This is only needed on Gemtek builds, on OpenWRT the system
			// clock is set correctly so the local modifications to
			// pjlib's os_time_unix.c to compensate for the system time being
			// set to local time are not longer needed or applied.

				/* call is to propagate DST information to PJ lib */
				index = getTimeZoneIndexFromFlash();
				APP_LOG("UPNP: Device", LOG_DEBUG,"set pj dst data index: %d, lTimeZone:%s gDstEnable: %d success", index, ltimezone, gDstEnable);
				UDS_pj_dst_data_os(index, ltimezone, gDstEnable);
#endif
		SetBelkinParameter (SYNCTIME_LASTTIMEZONE, ltimezone);

		APP_LOG("UPNP: Device", LOG_CRIT,"set time: utc: %s, timezone: %s, dst: %s g_lastTimeZone:%f success", szUtc, szTimeZone, szDst,g_lastTimeZone);

		//- only if isLocalDstSupported true, the calculate and toggle?
		if (isLocalDstSupported)
		{
			//- Arizona and Hawii will not?
#ifndef _OPENWRT_
			computeDstToggleTime(1);
#endif
		}
		else
			gTimeZoneUpdated = 1;

		AsyncSaveData();
#ifdef PRODUCT_WeMo_Insight
		APP_LOG("UPNP: Device", LOG_ERR,"Restarting Data export scheduler on time sync");
		g_ReStartDataExportScheduler = 1;
		ReStartDataExportScheduler();
#endif
	}
	else
	{
		APP_LOG("UPNP: Device", LOG_ERR,"set time: utc: %s, timezone: %s, dst: %s failure", szUtc, szTimeZone, szDst);
	}

	free(args);
	timesyncThread = -1;
	APP_LOG("UPNP: Device", LOG_DEBUG, "Time sync thread exiting");
	return SUCCESS;
}


int handleTimeSync(pUPnPActionRequest pActionRequest, char *func)
{
	int retVal = FAILURE;
#ifdef PRODUCT_WeMo_NetCam
		if (IsTimeZoneSetupByNetCam())
		{
				APP_LOG("TimeZone", LOG_DEBUG, "TimeZone set already from NetCam");
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x00;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, func, CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE],"status", "TimeZone updated already from NetCam");
				return 0x00;
		}
#endif

		if (0x00 == pActionRequest)
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = PLUGIN_ERROR_E_TIME_SYNC;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, func, CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE],"status", "Parameters Error!");
				return 0x01;
		}

		char* szUtc 		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "UTC");
		char* szTimeZone	= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "TimeZone");
		char* szDst			= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "dst");
		char* szIsLocalDst	= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "DstSupported");
		STimeSyncArgs *args=NULL;

		if (0x00 == szUtc || 0x00 == szTimeZone || 0x00 == szDst || 0x00 == szIsLocalDst ||
			0x00 == strlen(szUtc) || 0x00 == strlen(szTimeZone) || 0x00 == strlen(szDst) || 0x00 == strlen(szIsLocalDst))
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = PLUGIN_ERROR_E_TIME_SYNC;

				UpnpAddToActionResponse(&pActionRequest->ActionResult, "TimeSync", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE],"status", "failure");

				if(szUtc)
					FreeXmlSource(szUtc);

				if(szDst)
					FreeXmlSource(szDst);

				if(szTimeZone)
					FreeXmlSource(szTimeZone);

				if(szIsLocalDst)
					FreeXmlSource(szIsLocalDst);

				APP_LOG("UPNP: Device", LOG_DEBUG, "Old app case, send time sync request");
       //   			UPnPTimeSyncStatusNotify();
				g_OldApp=1;

				return PLUGIN_ERROR_E_TIME_SYNC;
		}

		APP_LOG("UPNP: Device",LOG_DEBUG,"set time: utc: %s, timezone: %s, dst: %s, szIsLocalDst: %s", szUtc, szTimeZone, szDst, szIsLocalDst);

		args = (STimeSyncArgs*)ZALLOC(sizeof(STimeSyncArgs));

		strncpy(args->szUtc, szUtc, sizeof(args->szUtc)-1);
		strncpy(args->szTimeZone, szTimeZone, sizeof(args->szTimeZone)-1);
		strncpy(args->szDst, szDst, sizeof(args->szDst)-1);
		strncpy(args->szIsLocalDst, szIsLocalDst, sizeof(args->szIsLocalDst)-1);

		pthread_attr_init(&timesync_attr);
		pthread_attr_setdetachstate(&timesync_attr, PTHREAD_CREATE_DETACHED);

		if(timesyncThread == -1)
			retVal = pthread_create(&timesyncThread, &timesync_attr, (void*)&timeSyncThread, (void *)args);

		if(retVal < SUCCESS) {
				APP_LOG("UPnPApp",LOG_ERR, "****TimeSync Thread not Created: %s****", strerror(errno));
		}

		if(szUtc)
			FreeXmlSource(szUtc);

		if(szDst)
			FreeXmlSource(szDst);

		if(szTimeZone)
			FreeXmlSource(szTimeZone);

		if(szIsLocalDst)
			FreeXmlSource(szIsLocalDst);

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "TimeSync", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE],"status", "success");
		return  SUCCESS;
}

/**
 * GetInformation:
 * 	Callback to Get the device information in XML format
 *
 *
 * *****************************************************************************************************************/
int GetInformation(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
    char szBuff[SIZE_1024B];
    int port = 0, state = 0, customizedState = 0;
    unsigned long countdownEndTime = 0;
    char modelCode[SIZE_32B]=" ";

    handleTimeSync(pActionRequest, "GetInformation");

    memset(szBuff, 0x0, sizeof(szBuff));

    port = UpnpGetServerPort();
    state = GetCurBinaryState();
#ifdef WeMo_SMART_SETUP_V2
    if(g_customizedState)
    {
        customizedState = DEVICE_CUSTOMIZED;
    }
#endif

    /*update with countdown timer end time if it is running*/
    if(gRuleHandle[e_COUNTDOWN_RULE].ruleCnt)
    {
        countdownEndTime = gCountdownEndTime;
    }
    getModelCode(modelCode);

#ifdef WeMo_SMART_SETUP_V2
    snprintf(szBuff, sizeof(szBuff),"<Device><DeviceInformation><firmwareVersion>%s</firmwareVersion><iconVersion>%d</iconVersion><iconPort>%d</iconPort><macAddress>%s</macAddress><FriendlyName>%s</FriendlyName><binaryState>%d</binaryState><CustomizedState>%d</CustomizedState><CountdownEndTime>%lu</CountdownEndTime><deviceCurrentTime>%lu</deviceCurrentTime><productName>%s</productName></DeviceInformation></Device>", g_szFirmwareVersion, gWebIconVersion, port, g_szWiFiMacAddress, g_szFriendlyName, state, customizedState, countdownEndTime, GetUTCTime(), getProductName(modelCode));
#else
    snprintf(szBuff, sizeof(szBuff),"<Device><DeviceInformation><firmwareVersion>%s</firmwareVersion><iconVersion>%d</iconVersion><iconPort>%d</iconPort><macAddress>%s</macAddress><FriendlyName>%s</FriendlyName><binaryState>%d</binaryState><hwVersion>v%d</hwVersion><CountdownEndTime>%lu</CountdownEndTime><deviceCurrentTime>%lu</deviceCurrentTime><productName>%s</productName></DeviceInformation></Device>", g_szFirmwareVersion, gWebIconVersion, port, g_szWiFiMacAddress, g_szFriendlyName, state, ghwVersion, countdownEndTime, GetUTCTime(), getProductName(modelCode));
#endif

    pActionRequest->ActionResult = NULL;
    pActionRequest->ErrCode = 0x00;

    APP_LOG("UPNP: Device", LOG_DEBUG, "Device information: %s", szBuff);

    UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetInformation", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], "Information", szBuff);

    return UPNP_E_SUCCESS;
}

/**
 * GetDeviceInformation:
 * 	Callback to Get the device information
 *
 *
 * *****************************************************************************************************************/
#ifdef PRODUCT_WeMo_NetCam
/* NetCam needs Login inforamtion, but the cloud is too inflexible to add
   a new tag. We have to "pretend" the friendly name contains also the login
   information, and NetCam app will know to parse it out */
extern char g_szNetCamLogName[128];
#endif

int GetDeviceInformation(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char szBuff[MAX_BUF_LEN];
		int port = 0, state = 0;
#ifdef PRODUCT_WeMo_NetCam
		char *alt_loginname;
#endif

		handleTimeSync(pActionRequest, "GetDeviceInformation");

		port = UpnpGetServerPort();

		state = GetCurBinaryState();

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;

		memset(szBuff, 0x0, MAX_BUF_LEN);
#ifdef PRODUCT_WeMo_NetCam
#ifdef NETCAM_LS
		alt_loginname = GetBelkinParameter("SeedonkOwnerMail");
		snprintf(szBuff, MAX_BUF_LEN,"%s|%s|%d|%d|%d|%s;LoginName=%s", g_szWiFiMacAddress, g_szFirmwareVersion, gWebIconVersion, port, state, g_szFriendlyName, alt_loginname);
#else
		snprintf(szBuff, MAX_BUF_LEN,"%s|%s|%d|%d|%d|%s;LoginName=%s", g_szWiFiMacAddress, g_szFirmwareVersion, gWebIconVersion, port, state, g_szFriendlyName, g_szNetCamLogName);
#endif
#else
		snprintf(szBuff, MAX_BUF_LEN,"%s|%s|%d|%d|%d|%s", g_szWiFiMacAddress, g_szFirmwareVersion, gWebIconVersion, port, state, g_szFriendlyName);
#endif

		APP_LOG ("UPNP: Device", LOG_HIDE, "DeviceInformation: %s", szBuff);
		UpnpAddToActionResponse (&pActionRequest->ActionResult, "GetDeviceInformation", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], "DeviceInformation", szBuff);

        /* Adding Countdown Time */
        APP_LOG ("UPNP: Device", LOG_DEBUG, "CountdownTime: %lu",
                 gCountdownEndTime);
        memset (szBuff, 0x0, MAX_BUF_LEN);
        snprintf (szBuff, MAX_BUF_LEN, "%lu", gCountdownEndTime);

        UpnpAddToActionResponse (&pActionRequest->ActionResult,
                                 "GetDeviceInformation",
                           CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE],
                                 "CountdownTime", szBuff);

		return UPNP_E_SUCCESS;
}

/**
 * GetLogFilePath:
 * 	Callback to Get log file URL
 *
 *
 * *****************************************************************************************************************/
int GetLogFilePath(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char szLogFileURL[MAX_FW_URL_LEN];
		memset(szLogFileURL, 0x00, sizeof(szLogFileURL));

		//-Return the log file path of the device
		if (strlen(g_server_ip) > 0x00)
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x00;
				snprintf(szLogFileURL, sizeof(szLogFileURL), "http://%s:%d/PluginLogs.txt", g_server_ip, g_server_port);
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetLogFileURL", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "LOGURL", szLogFileURL);
		}
		else
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x01;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetLogFileURL", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "LOGURL", "");
		}

		APP_LOG("UPNP: Device", LOG_DEBUG,"Log File URL: %s", szLogFileURL);

		return UPNP_E_SUCCESS;
}
ithread_t CloseApWaiting_thread = -1;

/**
 * GetWatchdogFile:
 * 	Callback to Get Watchdog Log file
 *
 *
 * *****************************************************************************************************************/
int GetWatchdogFile(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		if (pActionRequest == NULL)
		{
				APP_LOG("UPNP: Device", LOG_DEBUG, "UPNP parameter failure");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetWatchdogFile", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "WDFile", "Sending");

		APP_LOG("UPNP: Device", LOG_DEBUG,"GetWatchdogFile: Sending the wdLogFile");

		APP_LOG("UPNP: Device",LOG_DEBUG, "***************LOG Thread created***************\n");
		pthread_attr_init(&wdLog_attr);
		pthread_attr_setdetachstate(&wdLog_attr, PTHREAD_CREATE_DETACHED);
		ithread_create(&logFile_thread, &wdLog_attr, uploadLogFileThread, NULL);


		return UPNP_E_SUCCESS;
}

/**
 * Get Signal Strength:
 * 	Callback to get device present signal Strength
 *
 *
 * *****************************************************************************************************************/
int SignalStrengthGet(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char szSignalSt[MAX_FW_URL_LEN];
		memset(szSignalSt, 0x00, sizeof(szSignalSt));

		APP_LOG("UPNP: Device", LOG_DEBUG, "%s, called", __FUNCTION__);

		if (pActionRequest == NULL)
		{
				APP_LOG("UPNP: Device", LOG_DEBUG, "UPNP parameter failure");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		/*Update signal strength*/
		if(!gSignalStrength)
		{
				APP_LOG("UPNP: Device", LOG_DEBUG, "Update signal strength!");
				chksignalstrength();
		}

		snprintf(szSignalSt, sizeof(szSignalSt), "%d", gSignalStrength);
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetSignalStrength", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"SignalStrength", szSignalSt);


		APP_LOG("UPNP: Device", LOG_DEBUG,"Get Signal Strength: %s", szSignalSt);
		return UPNP_E_SUCCESS;
}


/**
 * SetServerEnvironment:
 * 	Callback to Set Server Environment IP
 *
 *
 * *****************************************************************************************************************/

int SetServerEnvironment(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char ServerEnvType[SIZE_8B] = {0};
		if (pActionRequest == 0x00)
		{
				APP_LOG("UPNP: Device", LOG_DEBUG,"Set Server Environment: paramter failure");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		char* szserverEnvIPaddr = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "ServerEnvironment");
		char* szturnserverEnvIPaddr = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "TurnServerEnvironment");
		char* szserverEnvType = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "ServerEnvironmentType");

		if (0x00 == strlen(szserverEnvIPaddr))
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = PLUGIN_ERROR_E_BASIC_EVENT;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetServerEnvironment", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"ServerEnvironment", "Parameter Error");
				APP_LOG("UPNP: Device", LOG_ERR,"Set Server Environment: parameter error");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		if (0x00 == strlen(szturnserverEnvIPaddr))
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = PLUGIN_ERROR_E_BASIC_EVENT;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetServerEnvironment", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"TurnServerEnvironment", "Parameter Error");
				APP_LOG("UPNP: Device", LOG_ERR,"Set Turn Server Environment: parameter error");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		if (0x00 == strlen(szserverEnvType))
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = PLUGIN_ERROR_E_BASIC_EVENT;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetServerEnvironment", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"ServerEnvironmentType", "Parameter Error");
				APP_LOG("UPNP: Device", LOG_ERR,"Set Server Environment: parameter error");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetServerEnvironment", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "ServerEnvironment", "success");
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetServerEnvironment", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "TurnServerEnvironment", "success");
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetServerEnvironment", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "ServerEnvironmentType", "success");

		/* cleanup old environment & remote settings and destroy nat client sesstion*/
		APP_LOG("UPNP: Device actual", LOG_DEBUG,"server Environment not set \n");
		ExecuteReset(0x03);
		serverEnvIPaddrInit();

		strncpy(g_serverEnvIPaddr, szserverEnvIPaddr, sizeof(g_serverEnvIPaddr)-1);
		strncpy(g_turnServerEnvIPaddr, szturnserverEnvIPaddr, sizeof(g_turnServerEnvIPaddr)-1);
		g_ServerEnvType = (SERVERENV)atoi(szserverEnvType);

		FreeXmlSource(szserverEnvIPaddr);
		FreeXmlSource(szserverEnvType);
		FreeXmlSource(szturnserverEnvIPaddr);

		snprintf(ServerEnvType, sizeof(ServerEnvType), "%d", g_ServerEnvType);
		SetBelkinParameter(CLOUD_SERVER_ENVIRONMENT, g_serverEnvIPaddr);
		SetBelkinParameter(CLOUD_SERVER_ENVIRONMENT_TYPE, ServerEnvType);
		SetBelkinParameter(CLOUD_TURNSERVER_ENVIRONMENT, g_turnServerEnvIPaddr);
		AsyncSaveData();
		UDS_SetTurnServerEnvIPaddr(g_turnServerEnvIPaddr);
		APP_LOG("UPNP: Device set ENV", LOG_DEBUG,"Set server environment IP: %s \n Set turn server IP: %s \n Set server environment type: %d \n", g_serverEnvIPaddr, g_turnServerEnvIPaddr, g_ServerEnvType);

		return UPNP_E_SUCCESS;

}

/**
 * GetServerEnvironment:
 * 	Callback to Get Server Environment IP
 *
 *
 * *****************************************************************************************************************/
int GetServerEnvironment(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char ServerEnvType[SIZE_8B] = {0};
		if (pActionRequest == 0x00)
		{
				APP_LOG("UPNP: Device", LOG_DEBUG, "Get Server Environment: paramter failure");
				return PLUGIN_ERROR_E_BASIC_EVENT;
		}

		APP_LOG("UPNP: Device", LOG_HIDE, "Server Environment IP is: %s \n turn server IP is: %s \n server environment type: %d \n", g_serverEnvIPaddr, g_turnServerEnvIPaddr, g_ServerEnvType);

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetServerEnvironment", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"ServerEnvironment", g_serverEnvIPaddr);
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetServerEnvironment", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"TurnServerEnvironment", g_turnServerEnvIPaddr);

		snprintf(ServerEnvType, sizeof(ServerEnvType), "%d", g_ServerEnvType);
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetServerEnvironment", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"ServerEnvironmentType", ServerEnvType);


		return UPNP_E_SUCCESS;
}

/**
 *
 *
 *
 *
 *
 ******************************************/
#define	MAX_AP_CLOSE_WAITING_TIME	10
void *CloseApWaitingThread(void *args)
{
		//- Close Setup here

		int k = 0x00;
		int isSetup = 0x00;
		char routerMac[MAX_MAC_LEN];
		char routerssid[MAX_ESSID_LEN];
                tu_set_my_thread_name( __FUNCTION__ );
#ifdef PRODUCT_WeMo_Insight
		//InitOnSetup();
		char SetUpCompleteTS[SIZE_32B];
		memset(SetUpCompleteTS, 0, sizeof(SetUpCompleteTS));
		if(!g_SetUpCompleteTS)
		{
				g_SetUpCompleteTS = GetUTCTime();
				sprintf(SetUpCompleteTS, "%lu", g_SetUpCompleteTS);
				SetBelkinParameter(SETUP_COMPLETE_TS, SetUpCompleteTS);
				AsyncSaveData();
		}
		APP_LOG("ITC: network", LOG_ERR,"UPnP  updated on setup complete g_SetUpCompleteTS---%lu, SetUpCompleteTS--------%s:", g_SetUpCompleteTS, SetUpCompleteTS);
#endif
		//- Stop WiFi pairing thread if necessary
		StopWiFiPairingTask();
		memset(routerMac, 0, sizeof(routerMac));
		memset(routerssid, 0, sizeof(routerssid));

		while (k++ < MAX_AP_CLOSE_WAITING_TIME)
		{
				ip_address = NULL;
				ip_address = wifiGetIP(INTERFACE_CLIENT);

				if ((0x01 == getCurrentClientState()) || (0x03 == getCurrentClientState()))
				{
						isSetup = 0x01;
						break;
				}
				else
				{
						APP_LOG("UPNP: setup", LOG_DEBUG, "###### Network not connected yet, how this could be ?########");
						APP_LOG("UPNP: setup", LOG_CRIT, "Network not connected yet, how this could be ?");
				}

				pluginUsleep(1000000);
		}
		if (isSetup)
		{
#ifdef WeMo_INSTACONNECT
				char password[PASSWORD_MAX_LEN];
				memset(password, 0, sizeof(password));
				if(strcmp(gWiFiParams.AuthMode,"OPEN"))
				{
						if(SUCCESS != decryptPassword(gWiFiParams.Key, password))
						{
								APP_LOG("WiFiApp", LOG_DEBUG,"decrypt Password failed...not setting up bridge control");
						}
						else
						{
								APP_LOG("WiFiApp", LOG_HIDE,"decrypt passwd: %s success...setting up bridge control", password);
								/* configure Bridge Ap */
								configureBridgeAp(gWiFiParams.SSID, gWiFiParams.AuthMode, gWiFiParams.EncrypType, password, gWiFiParams.channel);
						}
				}
				else
				{
						strncpy(password,"NOTHING", sizeof(password)-1);
						APP_LOG("WiFiApp", LOG_HIDE,"passwd: %s...setting up bridge control", password);
						/* configure Bridge Ap */
						configureBridgeAp(gWiFiParams.SSID, gWiFiParams.AuthMode, gWiFiParams.EncrypType, password, gWiFiParams.channel);
				}
				//g_usualSetup= 0x00;    //unset
#endif
				ControlleeDeviceStop();
#ifdef PRODUCT_WeMo_LEDLight
				StopPluginCtrlPoint();

				pluginUsleep(2000000);
#endif
				system("ifconfig ra0 down");
				g_ra0DownFlag = 1; //RA0 interface is Down
				ip_address = NULL;

				ip_address = wifiGetIP(INTERFACE_CLIENT);

				if (ip_address && (0x00 != strcmp(ip_address, DEFAULT_INVALID_IP_ADDRESS)))
				{
						//-Start new UPnP in client AP new address
						APP_LOG("UPNP: Device", LOG_HIDE,"start new UPnP session on %d: %s", g_eDeviceType, ip_address);
						UpdateUPnPNetworkMode(UPNP_INTERNET_MODE);
						//gautam: update the Insight and LS Makefile to copy Insightsetup.xml and Lightsetup.xml in /sbin/web/ as setup.xml
						int ret=ControlleeDeviceStart(ip_address, 0x00, "setup.xml", "/tmp/Belkin_settings");
                                                if(( ret != UPNP_E_SUCCESS ) && ( ret != UPNP_E_INIT ) )
                                                {
                                                    APP_LOG("UPNP", LOG_DEBUG,"UPNP on error: %d", ret);
                                                    APP_LOG("UPNP", LOG_DEBUG,"################### Wemo App going to be reset ###################");
                                                    resetSystem();
                                                }
						getRouterEssidMac (routerssid, routerMac, INTERFACE_CLIENT);
						if(strlen(gGatewayMAC) > 0x0)
						{
								memset(routerMac, 0, sizeof(routerMac));
								strncpy(routerMac, gGatewayMAC, sizeof(routerMac)-1);
						}

						/* Range Extender fix: Irrespective of device type, start the control point */
                                                ret=StartPluginCtrlPoint(ip_address, 0x00);
                                                if(UPNP_E_INIT_FAILED==ret)
                                                {
                                                    APP_LOG("UPNP", LOG_DEBUG,"UPNP on error: %d", ret);
                                                    APP_LOG("UPNP", LOG_DEBUG,"################### Wemo App going to be reset ###################");
                                                    resetSystem();
                                                }
						EnableContrlPointRediscover(TRUE);

						if (g_eDeviceType == DEVICE_SENSOR)
						{
								if(0x01 == atoi(g_szRestoreState))
								{
#ifdef WeMo_INSTACONNECT
										if(g_connectInstaStatus == APCLI_CONFIGURED)
#endif
												createAutoRegThread();
								}
								else if(0x00 == atoi(g_szRestoreState))
								{
										if((strlen(g_szHomeId) == 0x0) && (strlen(g_szPluginPrivatekey) == 0x0))
										{
												APP_LOG("REMOTEACCESS", LOG_DEBUG, "Remote Access is not Enabled... sensor");
												if(strlen(gGatewayMAC) > 0x0)
												{
														SetBelkinParameter (WIFI_ROUTER_MAC,routerMac);
														memset(g_routerMac, 0x0, sizeof(g_routerMac));
														strncpy(g_routerMac, routerMac, sizeof(g_routerMac)-1);
												}
												SetBelkinParameter (WIFI_ROUTER_SSID,routerssid);
												memset(g_routerSsid, 0x0, sizeof(g_routerSsid));
												strncpy(g_routerSsid, routerssid, sizeof(g_routerSsid)-1);
										}
										else
										{
												if ( (strlen(g_routerMac) == 0x00) && (strlen(g_routerSsid) == 0x00) )
												{
														if(strlen(gGatewayMAC) > 0x0)
														{
																SetBelkinParameter (WIFI_ROUTER_MAC,routerMac);
																memset(g_routerMac, 0x0, sizeof(g_routerMac));
																strncpy(g_routerMac, routerMac, sizeof(g_routerMac)-1);
														}
														SetBelkinParameter (WIFI_ROUTER_SSID,routerssid);
														memset(g_routerSsid, 0x0, sizeof(g_routerSsid));
														strncpy(g_routerSsid, routerssid, sizeof(g_routerSsid)-1);
												}
												else if( (strcmp(g_routerMac, routerMac) != 0) && (g_routerSsid!=NULL && strlen (g_routerSsid) > 0) )
												{
														APP_LOG("REMOTEACCESS", LOG_DEBUG, "router is not same.. sensor");
#ifdef WeMo_INSTACONNECT
														if(g_connectInstaStatus == APCLI_CONFIGURED)
#endif
																createAutoRegThread();
												}
												else
												{
														APP_LOG("REMOTEACCESS", LOG_DEBUG, "Remote Access is Enabled and router is same... sensor\n");
														if(strlen(gGatewayMAC) > 0x0)
														{
																SetBelkinParameter (WIFI_ROUTER_MAC,routerMac);
																memset(g_routerMac, 0x0, sizeof(g_routerMac));
																strncpy(g_routerMac, routerMac, sizeof(g_routerMac)-1);
														}
														SetBelkinParameter (WIFI_ROUTER_SSID,routerssid);
														memset(g_routerSsid, 0x0, sizeof(g_routerSsid));
														strncpy(g_routerSsid, routerssid, sizeof(g_routerSsid)-1);
												}
										}
								}
						}
						if (g_eDeviceType == DEVICE_SOCKET)
						{
								if(0x01 == atoi(g_szRestoreState))
								{
#ifdef WeMo_INSTACONNECT
										if(g_connectInstaStatus == APCLI_CONFIGURED)
#endif
												createAutoRegThread();
								}
								else if(0x00 == atoi(g_szRestoreState))
								{
										if((strlen(g_szHomeId) == 0x0) && (strlen(g_szPluginPrivatekey) == 0x0))
										{
												APP_LOG("REMOTEACCESS", LOG_DEBUG, "Remote Access is not Enabled.. opening socket control point");
												if(strlen(gGatewayMAC) > 0x0)
												{
														SetBelkinParameter (WIFI_ROUTER_MAC,routerMac);
														memset(g_routerMac, 0x0, sizeof(g_routerMac));
														strncpy(g_routerMac, routerMac, sizeof(g_routerMac)-1);
												}
												SetBelkinParameter (WIFI_ROUTER_SSID,routerssid);
												memset(g_routerSsid, 0x0, sizeof(g_routerSsid));
												strncpy(g_routerSsid, routerssid, sizeof(g_routerSsid)-1);
										}
										else
										{
												if ( (strlen(g_routerMac) == 0x00) && (strlen(g_routerSsid) == 0x00) )
												{
														APP_LOG("REMOTEACCESS", LOG_DEBUG, "Remote Access is Enabled, having no router info.. not opening socket control point");
														if(strlen(gGatewayMAC) > 0x0)
														{
																SetBelkinParameter (WIFI_ROUTER_MAC,routerMac);
																memset(g_routerMac, 0x0, sizeof(g_routerMac));
																strncpy(g_routerMac, routerMac, sizeof(g_routerMac)-1);
														}
														SetBelkinParameter (WIFI_ROUTER_SSID,routerssid);
														memset(g_routerSsid, 0x0, sizeof(g_routerSsid));
														strncpy(g_routerSsid, routerssid, sizeof(g_routerSsid)-1);
												}
												else if( (strcmp(g_routerMac, routerMac) != 0) && (g_routerSsid!=NULL && strlen (g_routerSsid) > 0) )
												{
														APP_LOG("REMOTEACCESS", LOG_DEBUG, "Remote Access is Enabled but router is not same, opening socket control point");
#ifdef WeMo_INSTACONNECT
														if(g_connectInstaStatus == APCLI_CONFIGURED)
#endif
																createAutoRegThread();
												}
												else
												{
														APP_LOG("REMOTEACCESS", LOG_DEBUG, "Remote Access is Enabled and router is same.. not opening socket control point");
														if(strlen(gGatewayMAC) > 0x0)
														{
																SetBelkinParameter (WIFI_ROUTER_MAC,routerMac);
																memset(g_routerMac, 0x0, sizeof(g_routerMac));
																strncpy(g_routerMac, routerMac, sizeof(g_routerMac)-1);
														}
														SetBelkinParameter (WIFI_ROUTER_SSID,routerssid);
														memset(g_routerSsid, 0x0, sizeof(g_routerSsid));
														strncpy(g_routerSsid, routerssid, sizeof(g_routerSsid)-1);
												}
										}
								}
						}
				}
				else
				{
						APP_LOG("UPNP: Device", LOG_ERR,"IP address is not correct");
						CloseApWaiting_thread = -1;
						return (void *)0x01;
				}
		}
		else
		{
				APP_LOG("UPNP: Device", LOG_ERR, "Network is not connected, setup not closed");
		}

		AsyncSaveData();
		CloseApWaiting_thread = -1;
		return NULL;
}


/* CloseSetup:
 * 	Close the setup so AP will be dropp and UPnP FINISH and restart again
 *
 *
 *
 *
 *
 *
 *********************************************************************************************************************/
int CloseSetup(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{

		APP_LOG("UPNP: Device", LOG_DEBUG,"%s", __FUNCTION__);
		gAppCalledCloseAp=1;

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "CloseSetup", CtrleeDeviceServiceType[PLUGIN_E_SETUP_SERVICE], "status", "success");

		ithread_create(&CloseApWaiting_thread, NULL, CloseApWaitingThread, NULL);
		APP_LOG("UPNP: Device", LOG_DEBUG, "AP closing in %d seconds .......", MAX_AP_CLOSE_WAITING_TIME);

		return UPNP_E_SUCCESS;
}

void setCurrFWUpdateState(int state)
{
		osUtilsGetLock(&gFWUpdateStateLock);
		currFWUpdateState = state;
		osUtilsReleaseLock(&gFWUpdateStateLock);
		APP_LOG("UPNP",LOG_DEBUG, "currFWUpdateState updated: %d", currFWUpdateState);
}

int getCurrFWUpdateState(void)
{
		int state;
		osUtilsGetLock(&gFWUpdateStateLock);
		state = currFWUpdateState;
		osUtilsReleaseLock(&gFWUpdateStateLock);
		//APP_LOG("UPNP",LOG_DEBUG, "currFWUpdateState updated: %d", state);
		return state;
}



/* UpdateFirmware:
 * 	update firmware notification from APP
 * 	"NewFirmwareVersion"[szVersion]
 *	       "ReleaseDate"[szReleaseDate]
 *                  "URL"[szURL]
 *	       "Signature"[szSignature]
 *
 ********************************************************************************/
int UpdateFirmware(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		int state = -1;
		FirmwareUpdateInfo fwUpdInf;

		APP_LOG("UPNP: Device", LOG_CRIT,"%s", __FUNCTION__);

		//-Read out all paramters from APP
		char* szNewFirmwareVersion = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "NewFirmwareVersion");
		char* szReleaseDate = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "ReleaseDate");
		char* szURL = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "URL");
		char* szSignature = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Signature");
		char* szDownloadStartTime = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "DownloadStartTime");
		char* szWithUnsignedImage = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "WithUnsignedImage");

		if((0x00 == szURL) || (0x00 == strlen(szURL)))
		{
				APP_LOG("Firmware Update",LOG_ERR, "URL empty, command not executed");
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = UPNP_E_INVALID_PARAM;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "UpdateFirmware",
								CtrleeDeviceServiceType[PLUGIN_E_FIRMWARE_SERVICE], "status", "failure");
				goto on_return;
		}

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = UPNP_E_SUCCESS;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "UpdateFirmware",
						CtrleeDeviceServiceType[PLUGIN_E_FIRMWARE_SERVICE], "status", "success");

		state = getCurrFWUpdateState();
		if( (state == FM_STATUS_DOWNLOADING) || (state == FM_STATUS_DOWNLOAD_SUCCESS) ||
			  (state == FM_STATUS_UPDATE_STARTING) )
		{
				APP_LOG("UPnPApp",LOG_ERR, "************Firmware Update Already in Progress...");
				goto on_return;
		}

		memset(&fwUpdInf, 0x00, sizeof(FirmwareUpdateInfo));

		if(szDownloadStartTime)
				fwUpdInf.dwldStartTime = atoi(szDownloadStartTime)*60; //Seconds

		if((0x00 != szWithUnsignedImage) && (0x00 != strlen(szWithUnsignedImage)))
				fwUpdInf.withUnsignedImage = atoi(szWithUnsignedImage); // 1 = using unsigned image

		strncpy(fwUpdInf.firmwareURL, szURL, sizeof(fwUpdInf.firmwareURL)-1);
		StartFirmwareUpdate(fwUpdInf);

on_return:
		FreeXmlSource(szNewFirmwareVersion);
		FreeXmlSource(szReleaseDate);
		FreeXmlSource(szURL);
		FreeXmlSource(szSignature);
		FreeXmlSource(szDownloadStartTime);

		return UPNP_E_SUCCESS;
}

int DownLoadFirmware(const char *FirmwareURL, int deferWdLogging, int withUnsigned)
{
		int state=0;
		char firmwareURL[MAX_FW_URL_LEN];
		int retVal = FAILURE;

		SetWiFiLED(0x00);
		FirmwareUpdateStatusNotify(FM_STATUS_DOWNLOADING);
		setCurrFWUpdateState(FM_STATUS_DOWNLOADING);
		RemoteFirmwareUpdateStatusNotify();
		state = getCurrFWUpdateState();
		APP_LOG("UPNP: Device", LOG_DEBUG,"******** current firmware update state is:%d type:%d", state, withUnsigned);

		memset(firmwareURL, 0x0, sizeof(firmwareURL));
		strncpy(firmwareURL, FirmwareURL, sizeof(firmwareURL)-1);
		strncat(firmwareURL, "?mac=", sizeof(firmwareURL)-strlen(firmwareURL)-1);
		strncat(firmwareURL, g_szWiFiMacAddress, sizeof(firmwareURL)-strlen(firmwareURL)-1);

		if(!deferWdLogging) {//Log the event in the WDLogFile only once
				APP_LOG("UPNP: Device", LOG_CRIT, "Starting firmware download ......[%s]", firmwareURL);
		} else {
				APP_LOG("UPNP: Device", LOG_DEBUG, "Starting firmware download ......[%s]", firmwareURL);
		}

		retVal = webAppFileDownload(firmwareURL, "/tmp/firmware.bin.gpg");
		if (retVal != SUCCESS) {
				APP_LOG("Firmware", LOG_ERR,"Download firmware image failed");
		}else {
				APP_LOG("Firmware", LOG_CRIT,"Downloaded firmware image from location %s successfully\n", firmwareURL);
		}

		return retVal;
}

/* GetFirmwareVersion:
 * 	Firmware version request from APP
 *
 ********************************************************************************/
int GetFirmwareVersion(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char szSkuNumber[SIZE_32B];
		char szResponse[SIZE_128B];
		memset(szSkuNumber, 0x00, sizeof(szSkuNumber));
		memset(szResponse, 0x00, sizeof(szResponse));

		char* szPreviousSkuNo   = GetBelkinParameter("SkuNo");

		if (0x00 == szPreviousSkuNo || 0x00 == strlen(szPreviousSkuNo))
		{
				snprintf(szSkuNumber, sizeof(szSkuNumber), "%s", DEFAULT_SKU_NO);
		}
		else
		{
				snprintf(szSkuNumber, sizeof(szSkuNumber), "%s", szPreviousSkuNo);
		}

		snprintf(szResponse, sizeof(szResponse), "FirmwareVersion:%s|SkuNo:%s", g_szBuiltFirmwareVersion, szSkuNumber);

		APP_LOG("UPNP: Device", LOG_DEBUG, "Firmware:%s", szResponse);

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;


		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetFirmwareVersion", CtrleeDeviceServiceType[PLUGIN_E_FIRMWARE_SERVICE],
						"FirmwareVersion", szResponse);



		return UPNP_E_SUCCESS;
}
//-----------------TODO: tmp here ----------------------------------

void FirmwareUpdate_AsyncUpdateNotify()
{
		pMessage msg = createMessage(META_FIRMWARE_UPDATE, 0x00, 0x00);
		SendMessage2App(msg);
}

void *updateMonitorCheckTh (void *args)
{
		APP_LOG("UPnPApp",LOG_ERR, "************Firmware Update Check Monitor thread Created");
		pluginUsleep(180000000);	// 3 mins
		APP_LOG("UPnPApp",LOG_ALERT, "************Firmware Update Check Monitor thread rebooting system...");
		if (gpluginRemAccessEnable && UDS_pluginNatInitialized()) {
				UDS_invokeNatDestroy();
		}
		system("reboot");
		return NULL;
}

#ifndef _OPENWRT_
int dumpFlashVariables(void)
{
		char *val;
		int num_vars=0, i=0,ret=0;
		FILE* pfWriteStream = 0x00;
		char buff[NVRAM_SETTING_BUF_SIZE];
		char tmp[SIZE_128B];

		memset(buff, 0, NVRAM_SETTING_BUF_SIZE);
		snprintf(buff, sizeof(buff), "nvram reset\n");

		num_vars = sizeof(g_apu8NvramVars)/sizeof(char *);

		for(i=0;i<num_vars;i++)
		{
				val = GetBelkinParameter(g_apu8NvramVars[i]);
				if(val && strlen(val))
				{
						memset(tmp, 0, sizeof(tmp));
						if(!(strcmp(g_apu8NvramVars[i], "cwf_serial_number")))
								snprintf(tmp, sizeof(tmp), "nvram_set %s \"%s\"\n", "SerialNumber", val);
						else if(!(strcmp(g_apu8NvramVars[i], "ntp_server1")))
								snprintf(tmp, sizeof(tmp), "nvram_set %s \"%s\"\n", "NTPServerIP", val);
						else
								snprintf(tmp, sizeof(tmp), "nvram_set %s \"%s\"\n", g_apu8NvramVars[i], val);

						strncat(buff, tmp, sizeof(buff)-strlen(buff)-1);
				}
		}

		if(strlen(buff))
		{
				pfWriteStream = fopen(NVRAM_FILE_NAME, "w");

				if (NULL == pfWriteStream)
				{
						APP_LOG("UPnPApp",LOG_ERR, "Open file %s for writing failed...", NVRAM_FILE_NAME);
						return -1;
				}


				ret = fwrite(buff, 1, strlen(buff), pfWriteStream);
				APP_LOG("UPnPApp",LOG_DEBUG, "Written %d bytes of data from %d bytes input", ret, strlen(buff));

				fclose(pfWriteStream);
		}

		return 0;
}
#endif

void *firmwareUpdateTh(void *args)
{
        int status = FAILURE, state = 0, rect = 0;
        char *fwUpStr = NULL;
        FirmwareUpdateInfo fwUpdInf;
        int deferWdLogging = 0;
        tu_set_my_thread_name( __FUNCTION__ );

        if(args) {
            memcpy(&fwUpdInf, args, sizeof(FirmwareUpdateInfo));
            free(args);
            args = NULL;
        }

        APP_LOG("UPnPApp",LOG_ERR, "**** Firmware Update thread Created with URL: %s ****", fwUpdInf.firmwareURL);

        gStopDownloadFW = 0;	//reset stop FW download flag used by httpwrapper curl
        status  = DownLoadFirmware(fwUpdInf.firmwareURL, deferWdLogging, fwUpdInf.withUnsignedImage);

        if (status == SUCCESS) {
              setCurrFWUpdateState(FM_STATUS_DOWNLOAD_SUCCESS);
              state = getCurrFWUpdateState();
              APP_LOG("UPNP: Device", LOG_DEBUG,"******** current firmware update state is:%d", state);
              FirmwareUpdate_AsyncUpdateNotify();
              RemoteFirmwareUpdateStatusNotify();
              setCurrFWUpdateState(FM_STATUS_UPDATE_STARTING);

              APP_LOG("UPNP: Device", LOG_DEBUG,"Unsetting the firmwareUpdate flag");
              RemoteFirmwareUpdateStatusNotify();
              pluginUsleep(1000000);
              AsyncSaveData();
              /* wait for remote notification thread to send out the notifications for download success & upgrade start */
              pluginUsleep(6*1000000);
              APP_LOG("UPNP: Device", LOG_DEBUG,"firmwareUpdate continuing after sleep");

#if defined(PRODUCT_WeMo_Insight)
              Update30MinDataOnFlash();
#endif

#ifndef _OPENWRT_
              dumpFlashVariables(); /* to use in contrast with gemtek and openwrt */
#endif
              state = getCurrFWUpdateState();
              APP_LOG("UPNP: Device", LOG_DEBUG,"******** current firmware update state is:%d", state);
              if (gpluginRemAccessEnable && UDS_pluginNatInitialized()) {
                    UDS_invokeNatDestroy();
              }
#if defined(PRODUCT_WeMo_Baby)
              EnableWatchDog(0,WD_DEFAULT_TRIG_TIME);
              APP_LOG("UPNP: Device", LOG_DEBUG,"******** disableWatchDog:\n");
              system("killall -9 monitor_launcher");
              APP_LOG("UPNP: Device", LOG_DEBUG,"******** killall -9 monitor_launcher:\n");
              system("killall -9 monitor");
              APP_LOG("UPNP: Device", LOG_DEBUG,"******** killall -9 monitor:\n");
#endif
              UnSetBelkinParameter("FirmwareUpURL");
#ifdef __OLDFWAPI__
              rect = Firmware_Update("/tmp/firmware.bin.gpg");
#else
              APP_LOG("UPNP: Device", LOG_DEBUG,"******** current firmware update state is:%d-%d", state, fwUpdInf.withUnsignedImage);
              if (fwUpdInf.withUnsignedImage) {
                  rect = New_Firmware_Update("/tmp/firmware.bin.gpg", 0);
              }else {
                  rect = New_Firmware_Update("/tmp/firmware.bin.gpg", 1);
              }
#endif

#ifdef PRODUCT_WeMo_LEDLight
              SD_SaveSensorPropertyToFile();
#endif
              if (0x00 != rect) {
                    //- In case Gemtek API called failure, though it would not occur And - If failure since the UPnP already stop, should reboot
                  //SetBelkinParameter("FirmwareUpURL", FirmwareURL);
                  //AsyncSaveData();
                  system("rm -f /tmp/firmware.bin.gpg");
                  system("rm -f /tmp/firmware.img");
                  APP_LOG("UPNP: Device", LOG_ALERT, "Gemtek API Firmware_Update called failure");

                  if (gpluginRemAccessEnable && UDS_pluginNatInitialized()) {
                      UDS_invokeNatDestroy();
                  }
                  pluginUsleep(120000000);	// 2 mins
                  firmwareUpThread = -1;
                  system("reboot");
              }
        }
        else {
              deferWdLogging++;
              setCurrFWUpdateState(FM_STATUS_DOWNLOAD_UNSUCCESS);
              state = getCurrFWUpdateState();
              APP_LOG("UPNP: Device", LOG_DEBUG,"Current firmware update state is:%d", state);
              APP_LOG("UPNP: Device", LOG_ERR, "Firmware download failure");
#if defined(PRODUCT_WeMo_Baby) || defined(PRODUCT_WeMo_Streaming)
              SetWiFiGPIOLED(0x04);
#else
              SetWiFiLED(0x04);
#endif
              FirmwareUpdateStatusNotify(FM_STATUS_DOWNLOAD_UNSUCCESS);
              RemoteFirmwareUpdateStatusNotify();
        }

        fwUpStr = GetBelkinParameter("FirmwareUpURL");
        firmwareUpThread=-1;

        if(fwUpStr && strlen(fwUpStr) != 0) {
              StartFirmwareUpdate(fwUpdInf);
        }

		return NULL;
}

void NoMotionSensorInd()
{
		if (!IsUPnPNetworkMode())
				return;
		LocalBinaryStateNotify(SENSORING_OFF);
}



void MotionSensorInd()
{
	if (!IsUPnPNetworkMode())
		return;

	executeNotifyRule();
	LocalBinaryStateNotify(SENSORING_ON);
}


//---------- Button Status change notify -------
void LocalUserActionNotify(int curState)
{
		return;
		if (device_handle == -1)
		{
				return;
		}

		if (!IsUPnPNetworkMode())
		{
				//- Not report since not on router or internet
				return;
		}

		char* szCurState[1];
		szCurState[0x00] = (char*)MALLOC(SIZE_2B+1);
		snprintf(szCurState[0x00], SIZE_2B, "%d", curState);

		char* paramters[] = {"UserAction"} ;

		UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].UDN,
						SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].ServiceId, (const char **)paramters, (const char **)szCurState, 0x01);

		APP_LOG("UPNP: Device", LOG_DEBUG, "Notification: UserAction: state: %d", curState);

		free(szCurState[0x00]);

}


//---------- Button Status change notify -------
void LocalBinaryStateNotify(int curState)
{
#ifdef PRODUCT_WeMo_LEDLight
		return;
#else
#if _OPENWRT_
		char sysString[SIZE_128B];
		int bootArgsLen = 0;
#endif
		if (device_handle == -1)
		{
				return;
		}

		if (!IsUPnPNetworkMode())
		{
				//- Not report since not on router or internet
				APP_LOG("UPNP", LOG_DEBUG, "Notification:BinaryState: Not in home network, ignored");
				return;
		}

		char* szCurState[1];
#ifdef PRODUCT_WeMo_Insight
		if ((0x00 == curState) || (0x01 == curState) || (0x08 == curState))
		{
				StateChangeTimeStamp(curState);
		}
		szCurState[0x00] = (char*)ZALLOC(SIZE_100B);
		if (g_InitialMonthlyEstKWH){
				snprintf(szCurState[0x00], SIZE_100B, "%d|%u|%u|%u|%u|%u|%u|%u|%u|%d",
								curState,g_StateChangeTS,g_ONFor,g_TodayONTimeTS ,g_TotalONTime14Days,g_HrsConnected,g_AvgPowerON,g_PowerNow,g_AccumulatedWattMinute,g_InitialMonthlyEstKWH);
		}
		else{
				snprintf(szCurState[0x00], SIZE_100B, "%d|%u|%u|%u|%u|%u|%u|%u|%u|%0.f",
								curState,g_StateChangeTS,g_ONFor,g_TodayONTimeTS ,g_TotalONTime14Days,g_HrsConnected,g_AvgPowerON,g_PowerNow,g_AccumulatedWattMinute,g_KWH14Days);
		}
		APP_LOG("UPNP", LOG_DEBUG, "Local Binary State Insight Parameters: %s", szCurState[0x00]);
#else
		szCurState[0x00] = (char*)ZALLOC(SIZE_32B+1);
        if (NULL == szCurState[0x00])
        {
            APP_LOG ("UPNP", LOG_DEBUG, "Malloc Error");
            return;
        }
        snprintf (szCurState[0x00], SIZE_32B, "%d", curState);

        APP_LOG ("UPNP", LOG_DEBUG, "Local Binary State Parameters: %s",
                 szCurState[0x00]);
#endif
		char* paramters[] = {"BinaryState"} ;

		UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].UDN,
						SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].ServiceId, (const char **)paramters, (const char **)szCurState, 0x01);


		APP_LOG("UPNP", LOG_DEBUG, "Notification:BinaryState:state: %d", curState);

		/*
		 * Sets the u-boot env variable 'boot_A_args'. This variable is parsed in the kernel
		 * to fetch the state of the variable and take action accordingly.
		 *
		 * Will work only on the OPENWRT boards.
		 * Gemtek boards follow different procedure to handle the scenario.
		 *
		 * Done for the Story: 2187, To restore the state of the switch before power failure
		 */
#if _OPENWRT_
		memset(sysString, '\0', SIZE_128B);
		bootArgsLen = strlen(g_szBootArgs);
		if(bootArgsLen) {
				if(curState) {
						snprintf(sysString, sizeof(sysString), "fw_setenv boot_A_args %s switchStatus=1", g_szBootArgs);
						system(sysString);
				}
				else {
						snprintf(sysString, sizeof(sysString), "fw_setenv boot_A_args %s switchStatus=0", g_szBootArgs);
						system(sysString);
				}
		}
#endif

		free(szCurState[0x00]);

		if ((0x00 == curState) || (0x01 == curState) || (0x08 == curState))
		{
				//- push to cloud only ON/OFF
				remoteAccessUpdateStatusTSParams (curState);
		}
#endif
}

void LocalCountdownTimerNotify()
{
	if (!IsUPnPNetworkMode() || (device_handle == -1))
	{
		return;
	}

	char value[SIZE_64B] = {0,};
	char curtime[SIZE_64B] = {0,};
	unsigned long currentTime=0;
	char* valueSet[2];
	char* paramters[] = {"CountdownEndTime", "deviceCurrentTime"} ;

	currentTime = GetUTCTime();
	snprintf(value, SIZE_64B, "%lu", gCountdownEndTime);
	snprintf(curtime, SIZE_64B, "%lu", currentTime);
	valueSet[0] = value;
	valueSet[1] = curtime;

	UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].UDN,
			SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].ServiceId, (const char **)paramters, (const char **)valueSet, 0x02);

	APP_LOG("UPNP: Device", LOG_DEBUG, "Notification: countdown End Time: %lu, device current time: %lu", gCountdownEndTime, currentTime);
}

//-------------------------------------------------------- Rule --------------------------------------------
//----------------------------------------- Rules related -------------------------------------------------
/*
 *
 *
 *
 *
 *
 *
 *
 **********************************************************/
int AddRule(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		APP_LOG("UPNP: Device", LOG_DEBUG, "%s called", __FUNCTION__);

		char* szName 		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Name");
		char* szType		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Type");
		char* szIsEnable 	= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "IsEnable");

		char* szMon 		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Mon");
		char* szTues 		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Tues");
		char* szWed 		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Wed");
		char* szThurs 	= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Thurs");
		char* szFri 		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Fri");
		char* szSat 		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Sat");
		char* szSun 		= Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "Sun");

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "AddRule", CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE], "status", "success");

		FreeXmlSource(szName);
		FreeXmlSource(szType);
		FreeXmlSource(szIsEnable);
		FreeXmlSource(szMon);
		FreeXmlSource(szTues);
		FreeXmlSource(szWed);
		FreeXmlSource(szThurs);
		FreeXmlSource(szFri);
		FreeXmlSource(szSat);
		FreeXmlSource(szSun);

		return UPNP_E_SUCCESS;
}

int EditRule(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		APP_LOG("UPNP: Device", LOG_DEBUG, "%s called", __FUNCTION__);
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "EditRule", CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE], "status", "success");
		return UPNP_E_SUCCESS;
}

int RemoveRule(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		APP_LOG("UPNP: Device", LOG_DEBUG, "%s called", __FUNCTION__);
		return UPNP_E_SUCCESS;
}

int EnableRule(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		APP_LOG("UPNP: Device", LOG_DEBUG, "%s called", __FUNCTION__);
		return UPNP_E_SUCCESS;
}
int DisableRule(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		return UPNP_E_SUCCESS;
}
int GetRules(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		APP_LOG("UPNP: Device", LOG_DEBUG, "%s called", __FUNCTION__);
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "EditRule", CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE], "GetRules", "success");
		return UPNP_E_SUCCESS;
}

#if defined(PRODUCT_WeMo_Insight) || defined(PRODUCT_WeMo_SNS) || defined(PRODUCT_WeMo_NetCam) || defined(PRODUCT_WeMo_LEDLight)

int SetRuleID(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	APP_LOG("UPNP: Device", LOG_DEBUG, "Do nothing and return");

	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetRuleID",
			CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"RuleID" , "success");
	return UPNP_E_SUCCESS;
}

int DeleteRuleID(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	APP_LOG("UPNP: Device", LOG_DEBUG, "Do nothing and return");
	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "DeleteRuleID",
			CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"RuleID" , "success");
	return UPNP_E_SUCCESS;
}

#endif

#ifdef PRODUCT_WeMo_Light

int changeNightLightStatus(char *DimValue)
{
		int IntDimVal=0, retVal=SUCCESS;

		SetBelkinParameter(DIMVALUE, DimValue);
		AsyncSaveData();
		IntDimVal = atoi(DimValue);
		APP_LOG("UPNPDevice", LOG_DEBUG, "Changing Night Light With DimValue: %d",IntDimVal);
		retVal = ChangeNightLight(IntDimVal);

		if(!retVal && IntDimVal==1)
		{
				APP_LOG("UPNPDevice", LOG_DEBUG, "DimNightLight: ...........Rebooting the System for Diming the Night Light");
				AsyncRebootSystem();
		}

		return retVal;
}

void AsyncRebootSystem(void)
{
		pMessage msg = createMessage(NIGHTLIGHT_DIMMING_MESSAGE_REBOOT, 0x00, 0x00);
		SendMessage2App(msg);
}

int SetNightLightStatus(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		int retVal=UPNP_E_SUCCESS;
		APP_LOG("UPNP: DimNightLight", LOG_DEBUG, "%s called", __FUNCTION__);
		if (0x00 == pActionRequest || 0x00 == request)
		{
				APP_LOG("UPNPDevice", LOG_DEBUG, "DimNightLight: command paramter invalid");
				return UPNP_E_INVALID_ARGUMENT;
		}
		char* DimValue = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "DimValue");
		if (0x00 == DimValue || 0x00 == strlen(DimValue))
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x01;
				APP_LOG("UPNPDevice", LOG_DEBUG, "DimNightLight: No DimValue");
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "DimNightLight",
								CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "DimValue", "Error");

				retVal=UPNP_E_INVALID_ARGUMENT;
		}
		if(UPNP_E_SUCCESS == retVal)
		{
				retVal = changeNightLightStatus(DimValue);
				if(!retVal){
						pActionRequest->ActionResult = NULL;
						pActionRequest->ErrCode = 0x00;
						UpnpAddToActionResponse(&pActionRequest->ActionResult, "DimNightLight",
										CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],"DimValue" , "success");
				}
				else{
						pActionRequest->ActionResult = NULL;
						pActionRequest->ErrCode = 0x01;
						APP_LOG("UPNPDevice", LOG_DEBUG, "DimNightLight: ChangeNightLight Failed");
						UpnpAddToActionResponse(&pActionRequest->ActionResult, "DimNightLight",
										CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "DimValue", "Error");
				}
		}
		FreeXmlSource(DimValue);
		return retVal;
}

int GetNightLightStatus(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char szResponse[SIZE_256B];
		memset(szResponse, 0x00, sizeof(szResponse));
		APP_LOG("UPNP: Device", LOG_DEBUG, "%s", __FUNCTION__);
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;

		APP_LOG("UPNP: GetDimNightLight", LOG_DEBUG, "%s called", __FUNCTION__);
		char *dimVal = GetBelkinParameter (DIMVALUE);
		APP_LOG("UPNP: GetDimNightLight", LOG_DEBUG, "DimValue in Flash: %s ",dimVal);

		snprintf(szResponse, sizeof(szResponse), "%s", dimVal);
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetDimNightLight", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],
						"DimValue", szResponse);

		return UPNP_E_SUCCESS;
}
#endif

void PopulatePluginParams(int DeviceType)
{
		int hr=0, min=0, sec=0;
		memset(gPluginParms.UpTime, 0x0, SIZE_64B);
		memset(gPluginParms.DeviceInfo, 0x0, SIZE_128B);

		detectUptime(&hr, &min, &sec);
		snprintf(gPluginParms.UpTime, sizeof(gPluginParms.UpTime), "%d:%d:%d", hr, min, sec);
#ifdef PRODUCT_WeMo_Insight
		char *paramVersion = NULL,*paramPerUnitcost = NULL,*paramCurrency = NULL;
#endif
		gPluginParms.Internet = getCurrentClientState();
		if (gIceRunning)
				gPluginParms.CloudVia = gIceRunning;
		else
				gPluginParms.CloudVia = 0;
		gPluginParms.CloudConnectivity = UDS_pluginNatInitialized();
		gPluginParms.LastAuthVal=gLastAuthVal;//setting in API webAppErrorHandling
		gPluginParms.LastFWUpdateStatus=getCurrFWUpdateState();
		gPluginParms.NowTime=(int) GetUTCTime();
		gPluginParms.HomeID=g_szHomeId;
		gPluginParms.DeviceID=g_szWiFiMacAddress;
		gPluginParms.RemoteAccess=gpluginRemAccessEnable;
		switch(DeviceType)
		{
				case DEVICE_SOCKET:
						snprintf(gPluginParms.DeviceInfo, sizeof(gPluginParms.DeviceInfo), "%s","Socket");
						break;
				case DEVICE_SENSOR:
						snprintf(gPluginParms.DeviceInfo, sizeof(gPluginParms.DeviceInfo), "%s","Sensor");
						break;
				case DEVICE_BABYMON:
						snprintf(gPluginParms.DeviceInfo, sizeof(gPluginParms.DeviceInfo), "%s","Baby");
						break;
				case DEVICE_STREAMING:
						snprintf(gPluginParms.DeviceInfo, sizeof(gPluginParms.DeviceInfo), "%s","Streaming");
						break;
				case DEVICE_BRIDGE:
						snprintf(gPluginParms.DeviceInfo, sizeof(gPluginParms.DeviceInfo), "%s","Bridge");
						break;
				case DEVICE_INSIGHT:
#ifdef PRODUCT_WeMo_Insight
						paramVersion = GetBelkinParameter(ENERGYPERUNITCOSTVERSION);
						paramPerUnitcost = GetBelkinParameter(ENERGYPERUNITCOST);
						paramCurrency   = GetBelkinParameter(CURRENCY);
						snprintf(gPluginParms.DeviceInfo, sizeof(gPluginParms.DeviceInfo), "%s|%s|%s|%s|%d|%d|%d","Insight",
										paramVersion,paramPerUnitcost,paramCurrency,g_PowerThreshold,g_EventEnable,g_s32DataExportType);
#endif
	    break;
	    case DEVICE_CROCKPOT:
		snprintf(gPluginParms.DeviceInfo, sizeof(gPluginParms.DeviceInfo), "%s","crockpot");
	    break;
	    case DEVICE_LIGHTSWITCH:
		snprintf(gPluginParms.DeviceInfo, sizeof(gPluginParms.DeviceInfo), "%s","Lightswitch");
	    break;
	    case DEVICE_NETCAM:
		snprintf(gPluginParms.DeviceInfo, sizeof(gPluginParms.DeviceInfo), "%s","Netcam");
	    break;
#ifdef PRODUCT_WeMo_Smart
        case DEVICE_SMART:
            snprintf(gPluginParms.DeviceInfo, sizeof(gPluginParms.DeviceInfo), "%s",g_DevName);
	    break;
#endif /*#ifdef PRODUCT_WeMo_Smart*/
	    default:
	    break;
	}

}
//------------------------------------------ Extended Meta info -----------------
int GetExtMetaInfo(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char szResponse[SIZE_256B];
		memset(szResponse, 0x00, sizeof(szResponse));
		APP_LOG("UPNP: Device", LOG_DEBUG, "%s", __FUNCTION__);
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;


    {
        #ifdef PRODUCT_WeMo_Jarden
        if (DEVICE_CROCKPOT == g_eDeviceType) {

	    PopulatePluginParams(g_eDeviceType);
        }
        else
        #endif
		#ifdef PRODUCT_WeMo_Smart
		if (DEVICE_SMART == g_eDeviceType)
		{
			PopulatePluginParams(g_eDeviceType);
        }
        else
        #endif
        #ifdef PRODUCT_WeMo_Light
        if (DEVICE_LIGHTSWITCH == g_eDeviceTypeTemp) {

								PopulatePluginParams(g_eDeviceTypeTemp);
						}
						else
#endif
#ifdef PRODUCT_WeMo_Insight
								if(DEVICE_INSIGHT == g_eDeviceTypeTemp){

										PopulatePluginParams(g_eDeviceTypeTemp);

								}
								else
#endif
								{
										PopulatePluginParams(g_eDeviceType);
								}
		}


		snprintf(szResponse, sizeof(szResponse), "%d|%d|%d|%d|%s|%d|%d|%s|%d|%s",
						gPluginParms.Internet,gPluginParms.CloudVia,
						gPluginParms.CloudConnectivity,gPluginParms.LastAuthVal,gPluginParms.UpTime,
						gPluginParms.LastFWUpdateStatus,gPluginParms.NowTime,gPluginParms.HomeID,
						gPluginParms.RemoteAccess,gPluginParms.DeviceInfo);
		APP_LOG("UPNP: Device", LOG_DEBUG, "ExtMetaInfo:%s", szResponse);
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetExtMetaInfo", CtrleeDeviceServiceType[PLUGIN_E_METAINFO_SERVICE],
						"ExtMetaInfo", szResponse);

		return UPNP_E_SUCCESS;
}

//------------------------------------------ Meta info -----------------
int GetMetaInfo(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char szResponse[SIZE_256B];
		memset(szResponse, 0x00, sizeof(szResponse));
		APP_LOG("UPNP: Device", LOG_DEBUG, "%s", __FUNCTION__);
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;

    {
        #ifdef PRODUCT_WeMo_Jarden
        if (DEVICE_CROCKPOT == g_eDeviceType) {

            snprintf(szResponse, sizeof(szResponse), "%s|%s|%s|%s|%s|%s",
	            g_szWiFiMacAddress, g_szSerialNo, g_szSkuNo, g_szFirmwareVersion,
	            g_szApSSID, "crockpot");
        }
        else if (DEVICE_SBIRON == g_eDeviceType) {
            snprintf(szResponse, sizeof(szResponse), "%s|%s|%s|%s|%s|%s",
                g_szWiFiMacAddress, g_szSerialNo, g_szSkuNo, g_szFirmwareVersion,
                g_szApSSID, "sbiron");
        }
        else if (DEVICE_MRCOFFEE == g_eDeviceType) {
            snprintf(szResponse, sizeof(szResponse), "%s|%s|%s|%s|%s|%s",
                g_szWiFiMacAddress, g_szSerialNo, g_szSkuNo, g_szFirmwareVersion,
                g_szApSSID, "coffee");
        }
        else if (DEVICE_PETFEEDER == g_eDeviceType) {
            snprintf(szResponse, sizeof(szResponse), "%s|%s|%s|%s|%s|%s",
                g_szWiFiMacAddress, g_szSerialNo, g_szSkuNo, g_szFirmwareVersion,
                g_szApSSID, "petfeder");
        }
		else if (DEVICE_SMART == g_eDeviceType) {
            snprintf(szResponse, sizeof(szResponse), "%s|%s|%s|%s|%s|%s",
                g_szWiFiMacAddress, g_szSerialNo, g_szSkuNo, g_szFirmwareVersion,
                g_szApSSID, "smart");
        }
        else
        #endif /*#ifdef PRODUCT_WeMo_Jarden*/

        #ifdef PRODUCT_WeMo_Smart
        if (DEVICE_SMART == g_eDeviceType)
		{
			if(g_DevName != NULL)
			{
				snprintf(szResponse, sizeof(szResponse), "%s|%s|%s|%s|%s|%s",
					g_szWiFiMacAddress, g_szSerialNo, g_szSkuNo, g_szFirmwareVersion,
					g_szApSSID, g_DevName);
			}
			else
			{
				snprintf(szResponse, sizeof(szResponse), "%s|%s|%s|%s|%s|%s",
					g_szWiFiMacAddress, g_szSerialNo, g_szSkuNo, g_szFirmwareVersion,
					g_szApSSID, "smart");
			}
		}
        else
		#endif
        #ifdef PRODUCT_WeMo_Light
        if (DEVICE_LIGHTSWITCH == g_eDeviceTypeTemp) {

								snprintf(szResponse, sizeof(szResponse), "%s|%s|%s|%s|%s|%s",
												g_szWiFiMacAddress, g_szSerialNo, g_szSkuNo, g_szFirmwareVersion,
												g_szApSSID, "Lightswitch");
						}
						else
#endif
#ifdef PRODUCT_WeMo_Insight
								if(DEVICE_INSIGHT == g_eDeviceTypeTemp){

										snprintf(szResponse, sizeof(szResponse), "%s|%s|%s|%s|%s|%s",
														g_szWiFiMacAddress, g_szSerialNo, g_szSkuNo, g_szFirmwareVersion,
														g_szApSSID, "Insight");

								}
								else
#endif
#ifdef PRODUCT_WeMo_LEDLight
								if( DEVICE_BRIDGE == g_eDeviceTypeTemp )
								{
										snprintf(szResponse, sizeof(szResponse), "%s|%s|%s|%s|%s|%s",
														g_szWiFiMacAddress, g_szSerialNo, g_szSkuNo, g_szFirmwareVersion,
														g_szApSSID, "Bridge");
								}
								else
#endif
								{
										snprintf(szResponse, sizeof(szResponse), "%s|%s|%s|%s|%s|%s",
														g_szWiFiMacAddress, g_szSerialNo, g_szSkuNo, g_szFirmwareVersion,
														g_szApSSID, (g_eDeviceType == DEVICE_SENSOR)? "Sensor":"Socket");
								}
		}

		APP_LOG("UPNP: Device", LOG_DEBUG, "%s", szResponse);

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetMetaInfo", CtrleeDeviceServiceType[PLUGIN_E_METAINFO_SERVICE],
						"MetaInfo", szResponse);

		return UPNP_E_SUCCESS;
}

char* CreateManufactureData()
{

#ifdef PRODUCT_WeMo_LEDLight
  mxml_node_t *pRespXml = NULL;
  mxml_node_t *pNodeRoot = NULL;
  mxml_node_t *pNode = NULL;
  char *pszResp = NULL;
#endif


  char *pRetVal = NULL;
  char *pSerialNumber = NULL;
  FILE *fp = NULL;

#ifdef PRODUCT_WeMo_Insight
  DataValues Values = {0,0,0,0,0};
  int Ret = 0;
#endif

#ifdef PRODUCT_WeMo_LEDLight
  pRespXml = mxmlNewXML("1.0");
  pNodeRoot = mxmlNewElement(pRespXml, "ManufactureData");
#endif
  char *pCountryCode   = GetBelkinParameter ("country_code");
  //char *pFirmwareVer   = GetBelkinParameter ("WeMo_version");
  char *pFirmwareVer   = g_szFirmwareVersion;
  char *pAPMacAddress  = GetBelkinParameter ("wl0_macaddr");
  char *pTargetCountry = GetBelkinParameter ("target_country");
#if 0
  char *pSTAMacAddress = GetBelkinParameter ("apcli0_macaddr");
#else

  char ucaMacAddress[SIZE_20B] = {0};
  struct ifreq  s;
  char *pSTAMacAddress = ucaMacAddress;
  int fd = socket (PF_INET, SOCK_DGRAM, IPPROTO_IP);
  strcpy (s.ifr_name, "apcli0");  // <-----<<< DANGER
  if (0 == ioctl (fd, SIOCGIFHWADDR, &s))
  {
    const unsigned char* mac = (unsigned char *) s.ifr_hwaddr.sa_data;
    snprintf (ucaMacAddress, sizeof (ucaMacAddress),
              "%02X:%02X:%02X:%02X:%02X:%02X",
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }
  else
  {
    strncpy (ucaMacAddress, "00:00:00:00:00:00", sizeof (ucaMacAddress));
  }
  APP_LOG ("UPNP: Device", LOG_DEBUG, "&&**&& apcli0 MacAddress = %s &&**&&", \
           pSTAMacAddress);
#endif
#ifdef PRODUCT_WeMo_Insight
  /*Reading Instantaneous Power from daemon*/
  if((Ret = HAL_GetCurrentReadings(&Values)) != 0) {
	APP_LOG("UPNP: Device", LOG_DEBUG, "\nMetering Daemon Not Responding!!!\n");
  }
  else
  {
	APP_LOG("UPNP: Device", LOG_DEBUG, "\nRead Instantaneous Power values from daemon %d && %f\n",Values.vRMS,(float)(Values.vRMS/1000));

  }
#endif

#ifdef PRODUCT_WeMo_LEDLight
  pSerialNumber = GetBelkinParameter("SerialNumber");
#else
  pSerialNumber = GetBelkinParameter("cwf_serial_number");
  //TODO
  //Get PowerMeter and USB information
#endif

  char *pSSID = g_szApSSID;


#ifndef PRODUCT_WeMo_LEDLight
  if ((pRetVal = MALLOC(MAX_STATUS_BUF)) == NULL)
  return (NULL);
#ifndef PRODUCT_WeMo_Insight
  snprintf(pRetVal, MAX_STATUS_BUF, "<?xml version=\"1.0\" encoding=\"utf-8\"?><ManufactureData><CountryCode>%s</CountryCode><FirmwareVersion>%s</FirmwareVersion><APMacAddress>%s</APMacAddress><STAMacAddress>%s</STAMacAddress><SSID>%s</SSID><TargetCountry>%s</TargetCountry><SerialNumber>%s</SerialNumber></ManufactureData>", \
	  pCountryCode, pFirmwareVer, pAPMacAddress, pSTAMacAddress, pSSID, pTargetCountry, pSerialNumber);
#else
  snprintf(pRetVal, MAX_STATUS_BUF, "<?xml version=\"1.0\" encoding=\"utf-8\"?><ManufactureData><CountryCode>%s</CountryCode><FirmwareVersion>%s</FirmwareVersion><APMacAddress>%s</APMacAddress><STAMacAddress>%s</STAMacAddress><SSID>%s</SSID><TargetCountry>%s</TargetCountry><SerialNumber>%s</SerialNumber><PowerMeter><vRMS>%d.%03d</vRMS><iRMS>%d.%03d</iRMS><Watts>%d.%03d</Watts><PF>%d.%03d</PF><Freq>%d.%03d</Freq><IntTemp>%d.%03d</IntTemp><ExtTemp>%d.%03d</ExtTemp></PowerMeter><USB></USB></ManufactureData>", \
	  pCountryCode, pFirmwareVer, pAPMacAddress, pSTAMacAddress, pSSID, pTargetCountry, pSerialNumber, (Values.vRMS/1000),(Values.vRMS%1000), (Values.iRMS/1000), (Values.iRMS%1000),(Values.Watts/1000),(Values.Watts%1000),(Values.PF/1000),(Values.PF%1000),(Values.Freq/100),(Values.Freq%100),(Values.InternalTemp/1000),(Values.InternalTemp%1000),(Values.ExternalTemp/1000),(Values.ExternalTemp%1000));
#endif


   APP_LOG("UPNP: Device", LOG_DEBUG, "Response: %s", pRetVal);

#else

  pNode = mxmlNewElement(pNodeRoot, "CountryCode");
  if( pCountryCode && pCountryCode[0] )
  	mxmlNewText(pNode, 0, pCountryCode);
  else
  	mxmlNewText(pNode, 0, "");

  pNode = mxmlNewElement(pNodeRoot, "FirmwareVesrion");
  if( pFirmwareVer && pFirmwareVer[0] )
  	mxmlNewText(pNode, 0, pFirmwareVer);
  else
  	mxmlNewText(pNode, 0, "");

  pNode = mxmlNewElement(pNodeRoot, "APMacAddress");
  if( pAPMacAddress && pAPMacAddress[0] )
  	mxmlNewText(pNode, 0, pAPMacAddress);
  else
  	mxmlNewText(pNode, 0, "");

  pNode = mxmlNewElement (pNodeRoot, "STAMacAddress");
  if (pSTAMacAddress && pSTAMacAddress[0])
     mxmlNewText (pNode, 0, pSTAMacAddress);
  else
  	mxmlNewText (pNode, 0, "");

  pNode = mxmlNewElement(pNodeRoot, "SSID");
  if( pSSID && pSSID[0] )
  	mxmlNewText(pNode, 0, pSSID);
  else
  	mxmlNewText(pNode, 0, "");

  pNode = mxmlNewElement (pNodeRoot, "TargetCountry");
  if (pTargetCountry && pTargetCountry[0])
  	mxmlNewText (pNode, 0, pTargetCountry);
  else
  	mxmlNewText (pNode, 0, "");

  pNode = mxmlNewElement (pNodeRoot, "SerialNumber");
  if (pSerialNumber && pSerialNumber[0])
  	mxmlNewText (pNode, 0, pSerialNumber);
  else
  	mxmlNewText (pNode, 0, "");

  pNode = mxmlNewElement(pNodeRoot, "PowerMeter");
  pNode = mxmlNewElement(pNodeRoot, "USB");
#endif

  /* write to the file */
  fp = fopen("/tmp/Belkin_settings/ManufactureData.xml", "w");

#ifdef PRODUCT_WeMo_LEDLight
  if( pRespXml )
  {
  	if(!fp)
  	{
		APP_LOG("UPNP: Device", LOG_ERR, "Could not open file for writing err: %s",strerror(errno));
 	}
	else
	{
      		mxmlSaveFile(pRespXml, fp, MXML_NO_CALLBACK);
      		fclose(fp);
	}
      pszResp = mxmlSaveAllocString(pRespXml, MXML_NO_CALLBACK);
  }

  if( pRespXml )
	  mxmlDelete(pRespXml);

  return pszResp;
#else

  if(!fp)
  {
	  APP_LOG("UPNP: Device", LOG_ERR, "Could not open file for writing err: %s",strerror(errno));
  }
  else
  {
	  fwrite(pRetVal, 1, strlen(pRetVal), fp);
      	  fclose(fp);
  }
  return pRetVal;
#endif
}


//------------------------------------------ Manufacture info -----------------
int GetManufactureData(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  char *pszRespXML = NULL;
  int retVal = UPNP_E_SUCCESS;

  pszRespXML = CreateManufactureData();

  // create a response
  if( UpnpAddToActionResponse( out, "GetManufactureData",
            CtrleeDeviceServiceType[PLUGIN_E_MANUFACTURE_SERVICE],
            "ManufactureData", pszRespXML) != UPNP_E_SUCCESS )
  {
    ( *out ) = NULL;
    ( *errorString ) = "Internal Error";
    APP_LOG("UPNPDevice", LOG_DEBUG, "GetManufactureData: Internal Error");
    retVal = UPNP_E_INTERNAL_ERROR;
  }

  if(pszRespXML)
  free (pszRespXML);

  return retVal;
}


#define MAX_RULE_ENTRY_SIZE 1024

//-- NEW RULE implementation ---------------------

int UpdateWeeklyCalendar(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{

	APP_LOG("Rule", LOG_CRIT,"Restart rule engine, UpdateWeeklyCalendar");

	gRestartRuleEngine = RULE_ENGINE_SCHEDULED;

	pActionRequest->ActionResult = 0x00;
	pActionRequest->ErrCode = 0x00;

	UpnpAddToActionResponse(&pActionRequest->ActionResult, "UpdateWeeklyCalendar", CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE],
			"status", "success");

#ifdef SIMULATED_OCCUPANCY
	unsetSimulatedData();
#endif
	return UPNP_E_SUCCESS;
}

void	SaveDbVersion(char* szVersion)
{
		if (!szVersion)
				return;

		SaveDeviceConfig(RULE_DB_VERSION_KEY, szVersion);
}

/**
 *
 *
 *
 *
 *
 *
 *********************************************************************/
int SetRulesDBVersion(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{

		char* szVersion = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "RulesDBVersion");
		if (szVersion && strlen(szVersion))
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x00;
				APP_LOG("UPNP: Rule", LOG_DEBUG, "New database version %s", szVersion);

				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetRulesDBVersion", CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE],
								"RulesDBVersion", szVersion);

				SetBelkinParameter(RULE_DB_VERSION_KEY, szVersion);
		}
		else
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x01;
				APP_LOG("UPNP: Rule", LOG_ERR, "parameters error: database version empty");
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetRulesDBVersion", CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE],
								"status", "unsuccess");
		}

		return UPNP_E_SUCCESS;
}

int GetRulesDBVersion(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char *szVersion = GetDeviceConfig(RULE_DB_VERSION_KEY);
		if (szVersion && strlen(szVersion))
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x00;
				APP_LOG("UPNP: Rule", LOG_DEBUG, "Database version:%s", szVersion);

				UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetRulesDBVersion", CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE],
								"RulesDBVersion", szVersion);
		}
		else
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = 0x00;
				APP_LOG("UPNP: Rule", LOG_ERR, "database version not available");

				UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetRulesDBVersion", CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE],
								"RulesDBVersion", "0");

		}

		return UPNP_E_SUCCESS;

}

int EditWeeklycalendar(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{

	APP_LOG("Rule", LOG_CRIT,"Stop rule engine on EditWeeklycalendar");

	char* szAction = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "action");

	if ((0x00 == szAction) || (0x00 == strlen(szAction)))
	{

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x01;

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "EditWeeklycalendar", CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE],
				"status", "unsuccess");

		APP_LOG("UPNP: Rule", LOG_ERR, "%s: paramters error", __FUNCTION__);

		return 0x00;
	}

	int action = atoi(szAction);

	APP_LOG("UPNP: Rule", LOG_DEBUG, "Rule stop command request:%d", action);

	if (RULE_ACTION_REMOVE == action)
	{
		gRestartRuleEngine = RULE_ENGINE_DEFAULT;

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "EditWeeklycalendar", CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE],
				"status", "success");

		StopRuleEngine();
#ifndef PRODUCT_WeMo_LEDLight
		/*stop Countdown Timer*/
		stopCountdownTimer();
#endif

#ifdef SIMULATED_OCCUPANCY
		unsetSimulatedData();
		system("rm /tmp/Belkin_settings/simulatedRule.txt");
		UnSetBelkinParameter(SIM_DEVICE_COUNT);
		UnSetBelkinParameter(SIM_MANUAL_TRIGGER_DATE);
		AsyncSaveData();
		if(!gProcessData)
		    StopPluginCtrlPoint();
#endif

		//- Reset to default sensor
		ResetSensor2Default();

		APP_LOG("UPNP: Rule", LOG_DEBUG, "Rule stop command executed");
	}
	else
	{
		APP_LOG("UPNP: Rule", LOG_DEBUG, "Rule stop command not executed: %d", action);
	}


	return UPNP_E_SUCCESS;
}


int GetRulesDBPath(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;

		char szDBURL[MAX_FW_URL_LEN];
		memset(szDBURL, 0x00, sizeof(szDBURL));

		//-Return the icon path of the device
		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;

		snprintf(szDBURL, sizeof(szDBURL), "http://%s:%d/rules.db", g_server_ip, g_server_port);
		APP_LOG("UPNP: Rule", LOG_DEBUG, "DBRule:%s", szDBURL);

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetRulesDBPath", CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE],
						"RulesDBPath", szDBURL);

		return UPNP_E_SUCCESS;
}

#ifdef SIMULATED_OCCUPANCY
int NotifyManualToggle(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		APP_LOG("UPNPDevice", LOG_DEBUG, "Notify Manual Toggle");
		saveManualTriggerData();

		/*stop away task for the day*/
		stopExecutorThread(e_AWAY_RULE);

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "NotifyManualToggle",
						CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "ManualToggle", "success");

		return UPNP_E_SUCCESS;
}

int GetSimulatedRuleData(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		char RuleData[SIZE_256B];
		int selfindex = -1;
		int remtimetotoggle = 0;

		APP_LOG("UPNPDevice", LOG_DEBUG, "GetSimulatedRuleData");
		if (0x00 == pActionRequest || 0x00 == request)
		{
				APP_LOG("UPNPDevice", LOG_DEBUG, "GetSimulatedRuleData: paramters error");
				return 0x01;
		}

		memset(RuleData, 0x00, sizeof(RuleData));

		LockLED();
		int curState = GetCurBinaryState();
		UnlockLED();

		int nowTime = daySeconds();
		LockSimulatedOccupancy();
		if(gpSimulatedDevice)
		{
		    selfindex = gpSimulatedDevice->selfIndex;
		    if(gpSimulatedDevice->randomTimeToToggle)
			remtimetotoggle = gpSimulatedDevice->randomTimeToToggle - nowTime;
		}
		UnlockSimulatedOccupancy();

		snprintf(RuleData, sizeof(RuleData), "%d|%d|%d|%s", selfindex, curState, remtimetotoggle, g_szUDN_1);

		pActionRequest->ActionResult = NULL;
		pActionRequest->ErrCode = 0x00;

		UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetSimulatedRuleData",
						CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "RuleData", RuleData);

		APP_LOG("UPNPDevice", LOG_DEBUG, "GetSimulatedRuleData: %s", RuleData);
		return UPNP_E_SUCCESS;
}

int SimulatedRuleData(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
		int retVal=SUCCESS;
		char* DeviceList = NULL;
		char* DeviceCount = NULL;
		char devCount[SIZE_8B];
		int totalCount = 0;

		APP_LOG("UPNP: Device", LOG_DEBUG, "%s called", __FUNCTION__);
		if (pActionRequest == 0x00)
		{
				APP_LOG("UPNP: Device", LOG_ERR,"INVALID PARAMETERS");
				retVal = FAILURE;
				goto on_return;
		}

		DeviceList = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "DeviceList");
		DeviceCount = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "DeviceCount");
		if (0x00 == DeviceList || 0x00 == strlen(DeviceList))
		{
				APP_LOG("UPNPDevice", LOG_DEBUG, "Simulated Rule Data: No DeviceList");
				retVal = FAILURE;
				goto on_return;
		}
		if (0x00 == DeviceCount|| 0x00 == strlen(DeviceCount))
		{
				APP_LOG("UPNPDevice", LOG_DEBUG, "Simulated Rule Data: No DeviceCount");
				retVal = FAILURE;
				goto on_return;
		}

		pActionRequest->ActionResult = 0x00;
		pActionRequest->ErrCode = 0x00;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "SimulatedRuleData", CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE], "status", "success");
		setSimulatedRuleFile(DeviceList);
		totalCount = atoi(DeviceCount);
		memset(devCount, 0, sizeof(devCount));
		snprintf(devCount, sizeof(devCount), "%d", totalCount);
		SetBelkinParameter(SIM_DEVICE_COUNT, devCount);
		AsyncSaveData();
		APP_LOG("UPNPDevice", LOG_DEBUG, "Simulated Rule Data: SimulatedDeviceCount: %d", totalCount);

on_return:
		FreeXmlSource(DeviceList);
		FreeXmlSource(DeviceCount);
		if(retVal != SUCCESS)
		{
				pActionRequest->ActionResult = NULL;
				pActionRequest->ErrCode = UPNP_E_INVALID_ARGUMENT;
				UpnpAddToActionResponse(&pActionRequest->ActionResult, "SimulatedRuleData", CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE], "status", "failure");
		}
		return retVal;
}
#endif
/**
 *  To get ssid prefix so that can form device AP ssid.
 *  Please note that, prefix table not pre-built since the list will not be long and "hard-coding"
 *
 *  @ szKey
 *  @ szBuffer char* INPUT, the buffer to save and return the results, please allocate 16 bytes for it
 */
char* GetSsidPrefix(char* szSerialNo, char* szBuffer)
{
	#ifdef PRODUCT_WeMo_Jarden
	unsigned short int Product_ID;
	unsigned short int Vendor_ID;
	unsigned int Device_ID;
	#endif /*#ifdef PRODUCT_WeMo_Jarden*/

	#ifdef PRODUCT_WeMo_Smart
    APP_LOG("Init", LOG_DEBUG, "g_DevName is %s",g_DevName );
	#endif
    if ((0x00 == szBuffer) ||
        (0x00 == szSerialNo)
        )
    {
        //- Error
        return 0x00;
    }

		if (MAX_SERIAL_LEN != strlen(szSerialNo))
		{
				strncpy(szBuffer, SSID_PREFIX_ERROR, MAX_APSSID_LEN-1);
				return 0x00;
		}

		APP_LOG("Init", LOG_DEBUG, "Product type(K/L/B/M/V): %C", szSerialNo[SERIAL_TYPE_INDEX]);

    if ('K' == szSerialNo[SERIAL_TYPE_INDEX])
    {
        /* Relay based products */
	    if ('1' == szSerialNo[SERIAL_TYPE_INDEX+2])
	    {
            strncpy(szBuffer, SSID_PREFIX_SWICTH, MAX_APSSID_LEN-1);
	    }
	    else if ('2' == szSerialNo[SERIAL_TYPE_INDEX+2])
	    {
	        strncpy(szBuffer, SSID_PREFIX_INSIGHT, MAX_APSSID_LEN-1);
	    }
	    else if ('3' == szSerialNo[SERIAL_TYPE_INDEX+2])
	    {
	        strncpy(szBuffer, SSID_PREFIX_LIGHT, MAX_APSSID_LEN-1);
	    }
		#if 0
		#ifdef PRODUCT_WeMo_Jarden
	    else if ('4' == szSerialNo[SERIAL_TYPE_INDEX+2])
	    {
	        strncpy(szBuffer, SSID_PREFIX_CROCK, MAX_APSSID_LEN-1);
	    }
	    else if ('5' == szSerialNo[SERIAL_TYPE_INDEX+2])
    	{
        	strncpy(szBuffer, SSID_PREFIX_SBIRON, MAX_APSSID_LEN-1);
	    }
    	else if ('6' == szSerialNo[SERIAL_TYPE_INDEX+2])
	    {
    	    strncpy(szBuffer, SSID_PREFIX_MRCOFFEE, MAX_APSSID_LEN-1);
    	}
    	else if ('7' == szSerialNo[SERIAL_TYPE_INDEX+2])
    	{
        	strncpy(szBuffer, SSID_PREFIX_PETFEEDER, MAX_APSSID_LEN-1);
    	}
    	#endif /*#ifdef PRODUCT_WeMo_Jarden*/
		#endif
		else
		{
	            strncpy(szBuffer, SSID_PREFIX_ERROR, MAX_APSSID_LEN-1);
		}
    }
    else if ('L' == szSerialNo[SERIAL_TYPE_INDEX])
    {
        //- Sensor
        strncpy(szBuffer, SSID_PREFIX_MOTION, MAX_APSSID_LEN-1);
    }
    else if ('M' == szSerialNo[SERIAL_TYPE_INDEX])
    {
        //- Baby, please note now just WeMo_Baby, not WeMo.Baby
        strncpy(szBuffer, SSID_PREFIX_BABY, MAX_APSSID_LEN-1);
    }
				else if ('B' == szSerialNo[SERIAL_TYPE_INDEX])
    {
				//- Bridge
				strncpy(szBuffer, SSID_PREFIX_BRIDGE, MAX_APSSID_LEN-1);
    }
	#ifdef PRODUCT_WeMo_Jarden
	else if ('S' == szSerialNo[SERIAL_TYPE_INDEX])
	{
        //- WeMo Smart modules

		Device_ID = szDeviceID;
		APP_LOG("Init", LOG_DEBUG, "Device ID from WASP is: %d", Device_ID);

		Vendor_ID = (unsigned short int)(Device_ID >> 16);
		if (JARDEN_VENDOR_ID != Vendor_ID)
		{
			strncpy(szBuffer, SSID_PREFIX_SMART, MAX_APSSID_LEN-1);
            /* Blink WASP Error indication LED */
			system("echo 8 > /proc/PLUGIN_LED_GPIO");

		}
		else
		{
			Product_ID = (unsigned short int)Device_ID;
			APP_LOG("Init", LOG_DEBUG, "Product ID from WASP is: %d", Product_ID);

			if (SLOW_COOKER_PRODUCT_ID == Product_ID)
			{
				strncpy(szBuffer, SSID_PREFIX_CROCK, MAX_APSSID_LEN-1);
			}
			else if (SBIRON_PRODUCT_ID == Product_ID)
			{
				strncpy(szBuffer, SSID_PREFIX_SBIRON, MAX_APSSID_LEN-1);
			}
			else if (MRCOFFEE_PRODUCT_ID == Product_ID)
			{
				strncpy(szBuffer, SSID_PREFIX_MRCOFFEE, MAX_APSSID_LEN-1);
			}
			else if (PETFEEDER_PRODUCT_ID == Product_ID)
			{
				strncpy(szBuffer, SSID_PREFIX_PETFEEDER, MAX_APSSID_LEN-1);
			}
			else
			{
				strncpy(szBuffer, SSID_PREFIX_SMART, MAX_APSSID_LEN-1);
                /* Blink WASP Error indication LED */
				system("echo 8 > /proc/PLUGIN_LED_GPIO");
			}
		}
	}
	#endif /*#ifdef PRODUCT_WeMo_Jarden*/

	#ifdef PRODUCT_WeMo_Smart
	else if ('S' == szSerialNo[SERIAL_TYPE_INDEX])
	{
        APP_LOG("Init", LOG_DEBUG, "Inside else if ('S' == szSerialNo[SERIAL %s",g_DevName);

		if(szDeviceID != 0)
		{
			snprintf(szBuffer,MAX_APSSID_LEN-1,"WeMo.%s.",g_DevName);
		}
		else
		{
			strncpy(szBuffer, SSID_PREFIX_SMART, MAX_APSSID_LEN-1);
		}
		APP_LOG("Init", LOG_DEBUG, "szBuffer of SSID is %s",szBuffer);
	}
	#endif /*#ifdef PRODUCT_WeMo_Smart*/
    else
    {
        //- Error SSID
        strncpy(szBuffer, SSID_PREFIX_ERROR, MAX_APSSID_LEN-1);
    }


		return szBuffer;

}

int getNvramParameter(char paramVal[])
{
		FILE *pSrFile = 0x00;
		const char *srFile = "/tmp/serFile";
		char srCmd[SIZE_128B];
		char szflag[SIZE_128B];
		char *var = NULL, *val = NULL;
		int len = 0;
		char buf[SIZE_64B];
		char *strtok_r_temp;

		memset(srCmd, 0x0, sizeof(srCmd));

		snprintf(srCmd, SIZE_128B, "fw_printenv SerialNumber > /tmp/serFile");

		system(srCmd);

		pSrFile = fopen(srFile, "r");
		if (pSrFile == 0x00)
		{
				APP_LOG("UPNP: DEVICE",LOG_DEBUG, "File opening %s error", srFile);
				strcpy(paramVal, DEFAULT_SERIAL_NO);  // <-----<<< DANGER
				return 0;
		}

		memset(szflag, 0x0, sizeof(szflag));
		if(fgets(szflag, sizeof(szflag), pSrFile))
		{
				var = strtok_r(szflag,"=",&strtok_r_temp);
				val = strtok_r(NULL,"=",&strtok_r_temp);
		}
		fclose(pSrFile);

		memset(buf, 0x00, sizeof(buf));
		sprintf(buf, "rm %s", srFile);
		system(buf);

		if(val && (len = strlen(val)) > 2) {
				if(val[len - 1] == '\n') {
						val[len - 1] = '\0';
						len = len -1;
				}

				strcpy(paramVal, val);  // <-----<<< DANGER
		} else {
				strcpy(paramVal, DEFAULT_SERIAL_NO);  // <-----<<< DANGER
		}

		return len;
}

void SetAppSSID()
{
		char szBuff[MAX_APSSID_LEN];

		memset(szBuff, 0x00, sizeof(szBuff));

		char* szSerialNo = GetSerialNumber();
		if ((0x00 == szSerialNo) || (0x00 == strlen(szSerialNo)))
		{
#ifdef _OPENWRT_
				char serBuff[SIZE_128B];
				memset(serBuff, 0x0, SIZE_128B);
				getNvramParameter(serBuff);
				szSerialNo = serBuff;
				SaveDeviceConfig("SerialNumber", serBuff);

#else
				//-User default one
				szSerialNo = DEFAULT_SERIAL_NO;
#endif
		}

		//-Get prefix of the AP SSID
		GetSsidPrefix(szSerialNo, szBuff);

		strncpy(g_szSerialNo, szSerialNo, sizeof(g_szSerialNo)-1);

		strncat(szBuff, szSerialNo + strlen(szSerialNo) - DEFAULT_SERIAL_TAILER_SIZE, sizeof(szBuff)-strlen(szBuff)-1);

		APP_LOG("UPNP: Device", LOG_DEBUG, "APSSID: %s", szBuff);
		APP_LOG("STARTUP: Device", LOG_DEBUG, "serial: %s", g_szSerialNo);


		wifisetSSIDOfAP(szBuff);
		memset(g_szApSSID, 0x00, sizeof(g_szApSSID));
		strncpy(g_szApSSID, szBuff, sizeof(g_szApSSID)-1);
}

//---------------------------- POST FILE -------------------
int PostFile(UpnpWebFileHandle fileHnd, char *buf, int buflen)
{
		APP_LOG("UPNP: DEVICE", LOG_DEBUG, "to write data: %d bytes\n", buflen);
		return 0x00;
}

UpnpWebFileHandle OpenWebFile(char *filename, enum UpnpOpenFileMode Mode)
{
		APP_LOG("UPNP DEVICE", LOG_DEBUG, "File is to open\n");
		return 0x00;
}

int GetFileInfo(const char *filename,    struct File_Info *info)
{
		APP_LOG("UPNP DEVICE", LOG_DEBUG, "Get File inform called\n");
		return 0x00;
}


//------------------------------ Sensor control --------------------
void *PowerSensorMonitorTask(void *args)
{
                tu_set_my_thread_name( __FUNCTION__ );
		APP_LOG("UPNP: sensor rule", LOG_DEBUG, "In Power Sensor Monitor Task");
		while(sPowerDuration > 0)
		{
			sleep(1);
			sPowerDuration--;
		}
#ifndef PRODUCT_WeMo_LEDLight
                /*stop Countdown Timer*/
                stopCountdownTimer();
#endif
		APP_LOG("UPNP: sensor rule", LOG_DEBUG, "Sensor event, monitoring thread stop: to change state tp: %d", sPowerEndAction);

		setActuation(ACTUATION_SENSOR_RULE);
		ChangeBinaryState(sPowerEndAction);
		g_IsLastUserActionOn = sPowerEndAction;
		LocalBinaryStateNotify(sPowerEndAction);
		ithPowerSensorMonitorTask = -1;
		return NULL;
}

void FirmwareUpdateStatusNotify(int status)
{
		char* szCurState[1];
		szCurState[0x00] = (char*)ZALLOC(SIZE_2B);
		snprintf(szCurState[0x00], SIZE_2B, "%d", status);

		char* paramters[] = {"FirmwareUpdateStatus"} ;

		UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_FIRMWARE_SERVICE].UDN,
						SocketDevice.service_table[PLUGIN_E_FIRMWARE_SERVICE].ServiceId, (const char **)paramters, (const char **)szCurState, 0x01);

		free(szCurState[0x00]);
		APP_LOG("UPNP: firmware update", LOG_DEBUG, "current status: %d", status);
}


void RemoteFirmwareUpdateStatusNotify(void)
{
		APP_LOG("UPNP",LOG_DEBUG, "REMOTE Firmware update status NOTIFICATION");
		remoteAccessUpdateStatusTSParams (0xFF);
}

int GetTimeZoneIndex(float iTimeZoneValue)
{

		APP_LOG("UPNP: time sync", LOG_DEBUG, "to lookup time zone: %f", iTimeZoneValue);

		int counter = sizeof(g_tTimeZoneList)/sizeof(tTimeZone);
		int loop = 0x00;
		int index = 0x00;

		for (; loop < counter; loop++)
		{
				float delta = iTimeZoneValue - g_tTimeZoneList[loop].iTimeZone;

				if ((delta <= 0.001) && (delta >= -0.001))
				{
						APP_LOG("UPNP: time sync", LOG_DEBUG, "time zone index found: %f, %f, %s", delta, iTimeZoneValue, g_tTimeZoneList[loop].szDescription);
						index = g_tTimeZoneList[loop].index;
						break;
				}
		}

		if (0x00 == index)
				APP_LOG("UPNP: time sync", LOG_ERR, "time zone index not found: %f", iTimeZoneValue);

		//- Reset NTP server
		if (index <= TIME_ZONE_NORTH_AMERICA_INDEX)
				s_szNtpServer = TIME_ZONE_NORTH_AMERICA_NTP_SERVER;
		else if(index == TIME_ZONE_UK_INDEX || index == TIME_ZONE_FRANCE_INDEX)
				s_szNtpServer = TIME_ZONE_EUROPE_NTP_SERVER;
		else if ((index > TIME_ZONE_NORTH_AMERICA_INDEX) && (index <= TIME_ZONE_ASIA_INDEX))
				s_szNtpServer = TIME_ZONE_ASIA_NTP_SERVER;		//- Use asia for Europ
		else
				s_szNtpServer = TIME_ZONE_ASIA_NTP_SERVER;

		return index;

}


int IsUPnPNetworkMode()
{
		if ((0x00 == strcmp(g_server_ip, "0.0.0.0")) ||
						0x00 == strcmp(g_server_ip, AP_LOCAL_IP))
		{
				return 0x00;
		}

		return 0x01;
}

void UpdateUPnPNetworkMode(int networkMode)
{
		if (UPNP_LOCAL_MODE == networkMode)
		{
				APP_LOG("UPNP: network", LOG_DEBUG, "network mode changed to: UPNP_LOCAL_MODE");
				g_IsUPnPOnInternet	= FALSE;
				g_IsDeviceInSetupMode	= TRUE;
		}
		else if (UPNP_INTERNET_MODE == networkMode)
		{
				APP_LOG("UPNP: network", LOG_DEBUG, "network mode changed to: UPNP_INTERNET_MODE");
				g_IsUPnPOnInternet 	= TRUE;
				g_IsDeviceInSetupMode	= FALSE;
		}
		else
		{
				APP_LOG("UPNP: network", LOG_ERR, "wrong network mode");
		}
}


void		NotifyInternetConnected()
{
		pMessage msg = createMessage(NETWORK_INTERNET_CONNECTED, 0x00, 0x00);
		SendMessage2App(msg);
}


void detectIPChange()
{
    int ret = 0;
    static char prevIP[SIZE_20B];
    char *currentIP = GetWanIPAddress();
    char *upnp_ip = UpnpGetServerIpAddress();

    if(!strcmp(currentIP,DEFAULT_INVALID_IP_ADDRESS))
    {
        APP_LOG("WiFiApp", LOG_DEBUG, "****** invalid ip: %s ******", currentIP);
        return;
    }

    if(!strlen(prevIP))
    {
        strncpy(prevIP, currentIP, SIZE_20B-1);
        APP_LOG("WiFiApp", LOG_DEBUG, "****** prevIP: %s ******", prevIP);
        return;
    }

    /* CASE-1: IP change case */
    if(strcmp(currentIP,prevIP))
    {
        APP_LOG("WiFiApp", LOG_CRIT,"IP change detected !!, currentIP|prevIP: %s|%s, device_handle: %d", currentIP, prevIP, device_handle);
        ret = 1;
    }
    /* CASE-2: IP same case, but upnp not running */
    else if(!strcmp(currentIP,prevIP) && device_handle==-1)
    {
        APP_LOG("WiFiApp", LOG_CRIT,"IP same, upnp not running!!, currentIP|prevIP: %s|%s, device_handle: %d", currentIP, prevIP, device_handle);
        ret = 1;
    }
    /* CASE-3: IP same but UPnP running on default IP address */
    else if(upnp_ip && strcmp(prevIP,upnp_ip))
    {   APP_LOG("WiFiApp", LOG_CRIT,"IP same but Upnp running on default IP address!!, currentIP|prevIP|upnp_ip: %s|%s|%s, device_handle: %d", currentIP, prevIP, upnp_ip, device_handle);
        ret = 1;
    }
    else
    {
        /* empty case */
    }

    if(ret)
    {
        NotifyInternetConnected();
        memset(prevIP, 0, SIZE_20B);
        strncpy(prevIP, currentIP, SIZE_20B-1);
        APP_LOG("WiFiApp", LOG_DEBUG, "******update prevIP: %s******", prevIP);
    }

    return;
}


void UpdateUPnPNetwork(int status)
{
		//- If IP address change and etc
		char* ip_address = wifiGetIP(INTERFACE_CLIENT);
		char routerMac[MAX_MAC_LEN];
		char routerssid[MAX_ESSID_LEN];
                char *upnpIP = UpnpGetServerIpAddress();

		memset(routerMac, 0, sizeof(routerMac));
		memset(routerssid, 0, sizeof(routerssid));

		getRouterEssidMac (routerssid, routerMac, INTERFACE_CLIENT);
#ifdef WeMo_INSTACONNECT
		/* in case of insta connect mode, replace Essid with router ssid */
		if(!g_usualSetup)
		{
				APP_LOG("WiFiApp", LOG_DEBUG,"Insta connect setup mode...");
				if (strlen(gRouterSSID) > 0x0)
				{
						APP_LOG("WiFiApp", LOG_DEBUG,"replace essid with router ssid for insta");
						memset(routerssid, 0, sizeof(routerssid));
						strncpy(routerssid, gRouterSSID, sizeof(routerssid)-1);
				}
		}
#endif
		if(upnpIP && !strcmp(ip_address,upnpIP))
		{
		   APP_LOG("WiFiApp", LOG_CRIT,"currentIP and upnpIP are same..so returning without restarting upnp....");
                   return;
		}
		if(strlen(gGatewayMAC) > 0x0)
		{
				memset(routerMac, 0, sizeof(routerMac));
				strncpy(routerMac, gGatewayMAC, sizeof(routerMac)-1);
		}

		if (status == NETWORK_INTERNET_CONNECTED)
		{
				APP_LOG("ITC: network", LOG_DEBUG, "router connection done, UPnP re-runs again");
				ControlleeDeviceStop();
				StopPluginCtrlPoint();
				pluginUsleep(2000000);

				if (ip_address && (0x00 != strcmp(ip_address, DEFAULT_INVALID_IP_ADDRESS)))
				{
						//-Start new UPnP in client AP new address
						APP_LOG("ITC: network", LOG_CRIT,"start new UPnP session: %s", ip_address);
						//gautam: update the Insight and LS Makefile to copy Insightsetup.xml and Lightsetup.xml in /sbin/web/ as setup.xml
						int ret=ControlleeDeviceStart(ip_address, 0x00, "setup.xml", DEFAULT_WEB_DIR);
                                                if(( ret != UPNP_E_SUCCESS ) && ( ret != UPNP_E_INIT ) )
                                                {
                                                    APP_LOG("UPNP", LOG_DEBUG,"UPNP on error: %d", ret);
                                                    APP_LOG("UPNP", LOG_DEBUG,"################### Wemo App going to be reset ###################");
                                                    resetSystem();
                                                }
                                                ret=StartPluginCtrlPoint(ip_address, 0x00);
                                                if(UPNP_E_INIT_FAILED==ret)
                                                {
                                                    APP_LOG("UPNP", LOG_DEBUG,"UPNP on error: %d", ret);
                                                    APP_LOG("UPNP", LOG_DEBUG,"################### Wemo App going to be reset ###################");
                                                    resetSystem();
                                                }
						EnableContrlPointRediscover(TRUE);
						if (DEVICE_SENSOR == g_eDeviceType)
						{
								if(0x01 == atoi(g_szRestoreState))
								{
#ifdef WeMo_INSTACONNECT
										if(g_connectInstaStatus == APCLI_CONFIGURED)
#endif
												createAutoRegThread();
								}
								else if(0x00 == atoi(g_szRestoreState))
								{
										if((strlen(g_szHomeId) == 0x0) && (strlen(g_szPluginPrivatekey) == 0x0))
										{
												APP_LOG("REMOTEACCESS", LOG_DEBUG, "UpdateUPnPNetwork: Remote Access is not Enabled... sensor\n");
#ifdef PRODUCT_WeMo_NetCam
												createAutoRegThread();
#endif
										}
										else
										{
#ifdef PRODUCT_WeMo_NetCam
										    APP_LOG("REMOTEACCESS", LOG_DEBUG, "%s|%s,%s|%s", g_routerMac,routerMac,g_routerSsid,routerssid);
										    if( (strcmp(g_routerMac, routerMac) != 0) || (strcmp(g_routerSsid, routerssid) != 0) )
#else
												if( (strcmp(g_routerMac, routerMac) != 0) && (g_routerSsid!=NULL && strlen (g_routerSsid) > 0) )
#endif
												{
														APP_LOG("REMOTEACCESS", LOG_DEBUG, "UpdateUPnPNetwork: router is not same.. sensor\n");
#ifdef WeMo_INSTACONNECT
														if(g_connectInstaStatus == APCLI_CONFIGURED)
#endif
																createAutoRegThread();//JIRA WEMO-27169
												}
												else
												{
														APP_LOG("REMOTEACCESS", LOG_DEBUG, "UpdateUPnPNetwork: Remote Access is Enabled.. sensor\n");
												}
										}
								}
						}
						if (DEVICE_SOCKET == g_eDeviceType)
						{
								if(0x01 == atoi(g_szRestoreState))
								{
										APP_LOG("REMOTEACCESS", LOG_DEBUG, "\n Restore state set to 1... Remote Access is not Enabled.. opening socket control point\n");
#ifdef WeMo_INSTACONNECT
										if(g_connectInstaStatus == APCLI_CONFIGURED)
#endif
												createAutoRegThread();
								}
								else if(0x00 == atoi(g_szRestoreState))
								{
										if ((0x00 == strlen(g_szHomeId) ) && (0x00 == strlen(g_szPluginPrivatekey)))
										{
												APP_LOG("REMOTEACCESS", LOG_DEBUG, "\n Remote Access is not Enabled.. opening socket control point\n");
										}
										else
										{
												if( (strcmp(g_routerMac, routerMac) != 0) && (g_routerSsid!=NULL && strlen (g_routerSsid) > 0) )
												{
														APP_LOG("REMOTEACCESS", LOG_DEBUG, "Remote Access is Enabled but router is different, openg socket control point");
#ifdef WeMo_INSTACONNECT
														if(g_connectInstaStatus == APCLI_CONFIGURED)
#endif
																createAutoRegThread();//JIRA WEMO-27169
												}
												else
												{
														APP_LOG("REMOTEACCESS", LOG_DEBUG, "\n Remote Access is Enabled.. not opening socket control point\n");
												}
										}
								}
						}
				}

				UpdateUPnPNetworkMode(UPNP_INTERNET_MODE);
#ifdef WeMo_INSTACONNECT
				system("killall -9 udhcpd");   //to make sure udhpcpd is killed
#endif
		}
		else if (status == NETWORK_AP_OPEN_UPNP)
		{
				//- Get gateway address, check current running status
				char* szAPIp = GetLanIPAddress();
				if ((0x00 == szAPIp) || 0x00 == strlen(szAPIp))
				{
						APP_LOG("ITC: network", LOG_ERR,"AP IP address unknown");
						return;
				}

				if (0x00 == strcmp(szAPIp, g_server_ip))
				{
						APP_LOG("ITC: network", LOG_ERR,"UPnP already running on AP network");
						return;
				}
				else
				{
						APP_LOG("ITC: network", LOG_ERR,"UPnP is switching to AP network:%s", szAPIp);
						ControlleeDeviceStop();
						StopPluginCtrlPoint();
						pluginUsleep(2000000);
						//gautam: update the Insight and LS Makefile to copy Insightsetup.xml and Lightsetup.xml in /sbin/web/ as setup.xml
						int ret=ControlleeDeviceStart(szAPIp, 0x00, "setup.xml", DEFAULT_WEB_DIR);
                                                if(( ret != UPNP_E_SUCCESS ) && ( ret != UPNP_E_INIT ) )
                                                {
                                                    APP_LOG("UPNP", LOG_DEBUG,"UPNP on error: %d", ret);
                                                    APP_LOG("UPNP", LOG_DEBUG,"################### Wemo App going to be reset ###################");
                                                    resetSystem();
                                                }
						UpdateUPnPNetworkMode(UPNP_LOCAL_MODE);
				}
		}
}


int ProcessItcEvent(pNode node)
{
		if (0x00 == node)
				return 0x01;

		if (0x00 == node->message)
				return 0x01;
		switch(node->message->ID)
		{
				case NETWORK_INTERNET_CONNECTED:
						APP_LOG("ITC: network", LOG_DEBUG, "NETWORK_INTERNET_CONNECTED");
						UpdateUPnPNetwork(NETWORK_INTERNET_CONNECTED);
						break;

				case META_SAVE_DATA:
						APP_LOG("ITC: meta", LOG_DEBUG, "META_SAVE_DATA");
						SaveSetting();
						break;

				case META_SOFT_RESET:
						APP_LOG("ITC: meta", LOG_CRIT, "META_SOFT_RESET");
						resetToDefaults();
						ClearRuleFromFlash();

						break;
				case META_FULL_RESET:
						APP_LOG("ITC: meta", LOG_CRIT, "META_FULL_RESET");
						pluginUsleep(1000000);
						setRemoteRestoreParam(0x1);
						remotePostDeRegister(NULL, META_FULL_RESET);
						ResetToFactoryDefault(0);
						break;
				case META_REMOTE_RESET:
						APP_LOG("ITC: meta", LOG_DEBUG, "META_REMOTE_RESET");
						UnSetBelkinParameter (DEFAULT_HOME_ID);
						memset(g_szHomeId, 0x00, sizeof(g_szHomeId));
						UnSetBelkinParameter (DEFAULT_PLUGIN_PRIVATE_KEY);
						memset(g_szPluginPrivatekey, 0x00, sizeof(g_szPluginPrivatekey));
						UnSetBelkinParameter (RESTORE_PARAM);
						memset(g_szRestoreState, 0x0, sizeof(g_szRestoreState));
						/* server environment settings cleanup and nat client destroy */
						serverEnvIPaddrInit();
						UDS_invokeNatDestroy();
						SaveSetting();
						break;
				case NETWORK_AP_OPEN_UPNP:
						APP_LOG("ITC: network", LOG_DEBUG, "NETWORK_AP_OPEN_UPNP");
						UpdateUPnPNetwork(NETWORK_AP_OPEN_UPNP);
						break;

				case BTN_MESSAGE_ON_IND:
						APP_LOG("ITC: button", LOG_DEBUG, "BTN_MESSAGE_ON_IND");
						ToggleUpdate(0x01);
						break;
				case BTN_MESSAGE_OFF_IND:
						APP_LOG("ITC: button", LOG_DEBUG, "BTN_MESSAGE_OFF_IND");
						ToggleUpdate(0x00);
						break;
				case UPNP_MESSAGE_ON_IND:
						APP_LOG("ITC: button", LOG_DEBUG, "UPNP_MESSAGE_ON_IND");
						LocalBinaryStateNotify(0x01);
#ifdef PRODUCT_WeMo_Insight
						processInsightNotification(E_STATE, 0x01);
#endif
						break;
				case UPNP_MESSAGE_OFF_IND:
						APP_LOG("ITC: button", LOG_DEBUG, "UPNP_MESSAGE_OFF_IND");
						LocalBinaryStateNotify(0x00);
#ifdef PRODUCT_WeMo_Insight
						if(POWER_SBY != g_APNSLastState)
							processInsightNotification(E_STATE, 0x00);
#endif
						break;
#ifdef PRODUCT_WeMo_Insight
				case UPNP_MESSAGE_SBY_IND:
						APP_LOG("ITC: button", LOG_DEBUG, "UPNP_MESSAGE_SBY_IND");
						pluginUsleep(500000);
						LocalBinaryStateNotify(POWER_SBY);
						if(POWER_OFF != g_APNSLastState)
							processInsightNotification(E_STATE, 0x00);
						break;
				case UPNP_MESSAGE_PWR_IND:
						//APP_LOG("ITC: Power", LOG_DEBUG, "UPNP_MESSAGE_PWR_IND");
						pluginUsleep(500000);
						if(!g_NoNotificationFlag)
						{
								LocalInsightParamsNotify();
						}
						else
						{
								g_NoNotificationFlag =0;
						}
						break;
				case UPNP_MESSAGE_PWRTHRES_IND:
						APP_LOG("ITC: Power", LOG_DEBUG, "UPNP_MESSAGE_PWRTHRES_IND");
						pluginUsleep(500000);
						LocalPowerThresholdNotify(g_PowerThreshold);
						//Send this response towards cloud synchronously using same data socket
						break;
				case UPNP_MESSAGE_ENERGY_COST_CHANGE_IND:
						APP_LOG("ITC: Power", LOG_DEBUG, "UPNP_MESSAGE_ENERGY_COST_CHANGE_IND");
						pluginUsleep(500000);
						LocalInsightHomeSettingNotify();
						break;
				case UPNP_MESSAGE_DATA_EXPORT:
						APP_LOG("ITC: Power", LOG_DEBUG, "UPNP_MESSAGE_DATA_EXPORT");
						pluginUsleep(500000);
						StartDataExportSendThread((void*)(node->message->message));
						break;
#endif
				case UPNP_ACTION_MESSAGE_ON_IND:
						APP_LOG("ITC: button", LOG_DEBUG, "UPNP_ACTION_MESSAGE_ON_IND");
						pluginUsleep(500000);
						LocalUserActionNotify(0x03);
						break;
				case UPNP_ACTION_MESSAGE_OFF_IND:
						APP_LOG("ITC: button", LOG_DEBUG, "UPNP_ACTION_MESSAGE_OFF_IND");
						pluginUsleep(500000);
						LocalUserActionNotify(0x02);
						break;

				case META_FIRMWARE_UPDATE:
						APP_LOG("ITC: FIRMWARE_UPDATE", LOG_DEBUG, "META_FIRMWARE_UPDATE");
						APP_LOG("UPNP: Device", LOG_DEBUG, "Firmware download done, UPnP stop");
						FirmwareUpdateStatusNotify(FM_STATUS_DOWNLOAD_SUCCESS);
						FirmwareUpdateStatusNotify(FM_STATUS_UPDATE_STARTING);
						pluginUsleep(3000000);
						StopPluginCtrlPoint();
						ControlleeDeviceStop();
						break;
#ifdef PRODUCT_WeMo_LEDLight
    case RULE_MESSAGE_RESTART_REQ:
      APP_LOG("ITC: RULE_MESSAGE_RESTART_REQ", LOG_DEBUG, "RULE_MESSAGE_RESTART_REQ");
      //- DST change to adv again, I am now living in new century
      Advertisement4TimeUpdate();
      //- DST time change, need to reload again
//            BR_ResetRule();
      break;
#endif
#ifdef PRODUCT_WeMo_Light
				case NIGHTLIGHT_DIMMING_MESSAGE_REBOOT:
						APP_LOG("ITC: NIGHTLIGHT_DIMMING_MESSAGE_REBOOT", LOG_DEBUG, "NIGHTLIGHT_DIMMING_MESSAGE_REBOOT");
						pluginUsleep(2000000);
						system("reboot");
#endif
				case META_CONTROLLEE_DEVICE_STOP:
						APP_LOG("ITC: CONTROLLEE_DEVICE_STOP", LOG_DEBUG, "META_CONTROLLEE_DEVICE_STOP");
						APP_LOG("UPNP: Device", LOG_DEBUG, "reset plugin, stop controllee device");
						ControlleeDeviceStop();
						break;

				default:
						break;
		}

		if (0x00 != node->message->message)
				free(node->message->message);
		free(node->message);
		free(node);
		return 0;
}


void	AsyncSaveData()
{
		pMessage msg = createMessage(META_SAVE_DATA, 0x00, 0x00);
		SendMessage2App(msg);
}


void RestoreIcon()
{
		APP_LOG("UPNP", LOG_DEBUG, "To use default icon");

		if (DEVICE_SOCKET == g_eDeviceType)
		{
				system("cp -f /etc/icon.jpg /tmp/Belkin_settings");
		}
		else
		{
				system("cp -f /etc/sensor.jpg /tmp/Belkin_settings/icon.jpg");
		}
}



int GetDeviceStateIF()
{
		int curState = 0x00;

	if (DEVICE_SOCKET == g_eDeviceType)
	{
		LockLED();
		curState = GetCurBinaryState();
		UnlockLED();
	}
	else if (DEVICE_SENSOR == g_eDeviceType)
	{
		LockSensor();
		curState = GetSensorState();
		UnlockSensor();
	}
	#ifdef PRODUCT_WeMo_Jarden
	else {
        LockLED();
        curState = GetCurBinaryState();
        UnlockLED();
    }
	#endif

#ifdef PRODUCT_WeMo_Smart
	else if (DEVICE_SMART == g_eDeviceType)
	{
		char *notifBuf = NULL;
		int attrListLen = 0;
		/* Head attribute tag + Body attributes list tags + Tail attribute tag */
		attrListLen = (SIZE_64B + SIZE_128B + SIZE_32B) * g_attrCount;
		notifBuf = (char*)CALLOC(1, attrListLen);
		if(notifBuf == NULL)
		{
			APP_LOG("REMOTE",LOG_DEBUG, "CALLOC error for notifBuf");
			resetSystem();
			}
			else
			{
			/* Sending mode value of Smart devices here */
			APP_LOG("REMOTE",LOG_DEBUG, "Sending current status...!");

			strncpy(notifBuf, "<attributeLists action=\"notify\">", SIZE_64B-1);
		for (i = 0; i < g_attrCount; i++)
		{
			memset(&SmartParams, 0x00, sizeof(SmartParameters));

			retVal = CloudGetAttributes(&SmartParams,i);

			if(retVal == 0)
			{
				sprintf(tempBuf, "<attribute><name>%s</name><value>%s</value></attribute>", SmartParams.Name, SmartParams.status);
			}
			else if(retVal == -1)
			{
				retVal = 0;
				/* Do nothing, as this condition is pseudo variables without value update*/
			}
			else
			{
				APP_LOG("REMOTEACCESS", LOG_DEBUG, "GetAttributes returned error...!");
				/* Do nothing */
			}
				strncat(notifBuf, tempBuf, SIZE_128B-1);
		}
			strncat(notifBuf, "</attributeLists>", SIZE_32B-1);

			APP_LOG("REMOTE",LOG_HIDE, "Status XML:%s", notifBuf);

		LockSmartRemote();
			g_remotebuff = strdup(notifBuf);
			APP_LOG("SmartDevice",LOG_HIDE,"Created g_remotebuff:%s", g_remotebuff);
			//g_remoteNotify=1;//No need, Doing in remoteAccessUpdateStatusTSParams
		UnlockSmartRemote();

			if(notifBuf != NULL)
			{
				free(notifBuf);
				notifBuf = NULL;
			}
		}
	}
#endif

		return curState;
}

void* FirmwareUpdateStart(void *args)
{
  int retVal = SUCCESS;
  int count=0, state=0, dwldStTime = 0, withUnsignedImage = 0;
  FirmwareUpdateInfo *pfwUpdInf = NULL;
  char FirmwareURL[MAX_FW_URL_LEN];

#ifdef PRODUCT_WeMo_LEDLight
  setCurrFWUpdateState(FM_STATUS_DOWNLOADING);
#endif
  tu_set_my_thread_name( __FUNCTION__ );
  memset(FirmwareURL, 0, sizeof(FirmwareURL));

  if(args)
  {
    pfwUpdInf = (FirmwareUpdateInfo *)args;
    dwldStTime = pfwUpdInf->dwldStartTime;
    withUnsignedImage = pfwUpdInf->withUnsignedImage;
    strncpy(FirmwareURL, pfwUpdInf->firmwareURL, sizeof(FirmwareURL)-1);
  }

  APP_LOG("UPnPApp",LOG_DEBUG, "**** [FIRMWARE UPDATE] Saving firmware update URL: %s ****", FirmwareURL);
  SetBelkinParameter("FirmwareUpURL", FirmwareURL);
  AsyncSaveData();

  if(dwldStTime)
  {
    APP_LOG("UPnPApp", LOG_DEBUG,"******** [FIRMWARE UPDATE] Sleeping for %d mins zzzzzz", dwldStTime/60);
    pluginUsleep(dwldStTime * 1000000); //Staggering the downloading of firmware
  }

#ifdef PRODUCT_WeMo_LEDLight
  while( getCurrentClientState() != STATE_CONNECTED )
  {
    APP_LOG("UPnPApp",LOG_DEBUG, "**** [FIRMWARE UPDATE] Network is not available, waiting for %d min.****", count);
    pluginUsleep(60 * 1000000); // 1 min	count++;

    if(count >= 60)
    {
      APP_LOG("UPnPApp", LOG_ERR,
        "**** [FIRMWARE UPDATE] Failure. Network is not available, TIMEOUT 1 hour.****", count);
      setCurrFWUpdateState(FM_STATUS_DOWNLOAD_UNSUCCESS);
      FirmwareUpdateStatusNotify(FM_STATUS_DOWNLOAD_UNSUCCESS);
      goto on_return;
    }
    count++;
  }
  count = 0;

  while( isSubdeviceFirmwareUpdating() )
  {
    APP_LOG("UPnPApp", LOG_DEBUG ,
      "**** [FIRMWARE UPDATE] Subdevice firmware update is in progress, waiting to finish it. %d Seconds.****",
      5*count);
    count++;
    pluginUsleep(5 * 1000000);
  }
  count = 0;
#endif

  pthread_attr_init(&firmwareUp_attr);
  pthread_attr_setdetachstate(&firmwareUp_attr, PTHREAD_CREATE_DETACHED);
  retVal = pthread_create(&firmwareUpThread, &firmwareUp_attr, (void*)&firmwareUpdateTh, (void *)args);
  if(retVal < SUCCESS) {
    APP_LOG("UPnPApp",LOG_ERR, "**** [FIRMWARE UPDATE] Firmware Update thread not Created****");
    goto on_return;
  }

  pluginUsleep (500000); //500 ms
  APP_LOG("UPnPApp",LOG_ERR, "************ [FIRMWARE UPDATE] Firmware Update Monitor thread Created");
  while(1)
  {
    count = 0;
    while (count < MAX_FW_DL_TIME_OUT)
    {
      pluginUsleep (10000000);
      if( (getCurrFWUpdateState() == FM_STATUS_DOWNLOAD_SUCCESS) ||
          (getCurrFWUpdateState() == FM_STATUS_UPDATE_STARTING) )
      {
        int wait_for_flash = 0;
        APP_LOG("UPNP: Device", LOG_DEBUG,"******** [FIRMWARE UPDATE] Download is Complete");
        while(wait_for_flash < 36) // 3 mins
        {
          if (firmwareUpThread == -1)
            break;
          wait_for_flash ++;
          APP_LOG("UPNP: Device", LOG_DEBUG,"******** [FIRMWARE UPDATE] waiting for REBOOTING %d sec", wait_for_flash * 5);
          pluginUsleep(5000000);
        }
        APP_LOG("UPnPApp",LOG_ALERT, "************ [FIRMWARE UPDATE] Firmware Update Monitor thread rebooting system...");
        if ((gpluginRemAccessEnable) && (UDS_pluginNatInitialized()))
          UDS_invokeNatDestroy();
        firmwareUpThread = -1;

        int rc = system("reboot");
        if (rc != -1)
          return NULL;
      }
      else if( getCurrFWUpdateState() == FM_STATUS_DOWNLOAD_UNSUCCESS )
        break;
      else
        count +=10;
    }

    state = getCurrFWUpdateState();
    APP_LOG("UPNP: Device", LOG_DEBUG,"[FIRMWARE UPDATE] Going to stop downloading...:%d", state);

    //- time out here, thread itself quit and the thread ID set to 0x00 for further probable re-visit
    //firmwareUpThread = -1;

    //Stop download firmware request
    StopDownloadRequest();

    //- Remove the file already created to save more space
    // Do not remove the downloaded file, so that curl can resume downloading file
    // when the download fails due to the slow network connection.
    /*remove partially downloaded file*/
    system("rm /tmp/firmware.bin.gpg");
    //- Reset the LED state back so that not confuse the user
    SetWiFiLED(0x04);
    break;
  }
on_return:
  fwUpMonitorthread = -1;
  return NULL;
}

#if defined(AUTO_FW_UPDATE)

/**
 * Get AutoFwUpdateVar:
 *      Callback to Get "g_AutoFwUpdateVar"
 *
 *
 * *****************************************************************************************************************/
int GetAutoFwUpdateVar(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
        char *AutoFwStatus = NULL;
	AutoFwStatus = GetBelkinParameter(AUTOFWUPDATEVAR);

	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "GetAutoFwUpdateVar", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], "AutoFwUpdateVar", AutoFwStatus);
	APP_LOG("UPNP: Device", LOG_DEBUG, "GetAutoFwUpdateVar response sent");

        return UPNP_E_SUCCESS;

}
/**
 * Set AutoFwUpdateVar:
 *      Callback to Set "g_AutoFwUpdateVar"
 *      To kill AutoFwUpdate Thread if g_AutoFwUpdateVar is set to '0'
 *
 * *****************************************************************************************************************/
int SetAutoFwUpdateVar(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{

	int errnum;
	int AutoFwStatus=0x00;
	if (pActionRequest == 0x00)
        {
                APP_LOG("UPNP: Device", LOG_DEBUG,"Set AutoFwUpdateVar: paramter failure");
                return PLUGIN_ERROR_E_BASIC_EVENT;
        }

        char* szAutoFwUpdate = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "AutoFwUpdateVar");

        if( (szAutoFwUpdate == NULL) && (0x00 == strlen(szAutoFwUpdate)))
        {
                pActionRequest->ActionResult = NULL;
                pActionRequest->ErrCode = PLUGIN_ERROR_E_BASIC_EVENT;
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetAutoFwUpdateVar", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE],"status", "Parameters Error");
		APP_LOG("UPNP: Device", LOG_ERR,"Set AutoFwUpdateVar: failure");
		return PLUGIN_ERROR_E_BASIC_EVENT;
	}


	AutoFwStatus = strtoul(szAutoFwUpdate, NULL, 0);
	if(g_AutoFwUpdateVar != AutoFwStatus)
	{
	    g_AutoFwUpdateVar = AutoFwStatus;
	    APP_LOG("UPNP: Device", LOG_DEBUG,"Set g_AutoFwUpdateVar to value : %u", g_AutoFwUpdateVar);

	    SetBelkinParameter(AUTOFWUPDATEVAR, szAutoFwUpdate);
	    AsyncSaveData();
	    APP_LOG("UPNP: Device", LOG_DEBUG, "AutoFwUpdateVar state change to %s", szAutoFwUpdate);


	    if (g_AutoFwUpdateVar == 0 )
	    {
		if ((-1 != AutofwUpgradethread))
		{
		    APP_LOG("UPNP: Device", LOG_DEBUG, "====Auto Firmware Update thread was created====");
		    errnum = pthread_kill(AutofwUpgradethread, SIGRTMIN);
		    if(errnum){
			APP_LOG("UPNP: Device",LOG_ERR, "pthread_kill ret: %d...",errnum);
			exit(0);
		    }
		    AutofwUpgradethread = -1;
		    if(-1 != firmwareUpThread)
		    {
			APP_LOG("UPNP: Device",LOG_DEBUG, "====Firmware download thread was created====");
			errnum = pthread_kill(firmwareUpThread, SIGRTMIN);
			if(errnum){
			    APP_LOG("UPNP: Device",LOG_ERR, "pthread_kill ret: %d...",errnum);
			    exit(0);
			}
			firmwareUpThread = -1;
		    }
		    if(-1 != fwUpMonitorthread)
		    {
			errnum = pthread_kill(fwUpMonitorthread, SIGRTMIN);
			if(errnum){
			    APP_LOG("UPNP: Device",LOG_ERR, "pthread_kill ret: %d...",errnum);
			    exit(0);
			}
			fwUpMonitorthread = -1;
		    }
		    setCurrFWUpdateState(FM_STATUS_DEFAULT);
		}
	    }
	    else if (g_AutoFwUpdateVar == 1 )
	    {
		if ((-1 == AutofwUpgradethread))
		{
		    if(AutoFWUpdate() < SUCCESS)
		    {
			APP_LOG("UPNP: Device",LOG_CRIT, "======AutoFirmware Upgrade thread not Created On Setting g_AutoFwUpdateVar to 1======");
			exit(0);
		    }
		}
	    }
	}
	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode = 0x00;
	UpnpAddToActionResponse(&pActionRequest->ActionResult, "SetAutoFwUpdateVar", CtrleeDeviceServiceType[PLUGIN_E_DEVICEINFO_SERVICE], "AutoFwUpdateVar", szAutoFwUpdate);
	APP_LOG("UPNP: Device", LOG_DEBUG, "set SetAutoFwUpdateVar response sent");
        FreeXmlSource(szAutoFwUpdate);

        return UPNP_E_SUCCESS;

}


void remove_slash(char *string)
{
    char *read, *write;

    for(read = write = string; *read != '\0'; ++read)
    {
        if(*read != '\\')
        {
            *(write++) = *read;
        }
    }
    *write = '\0';
}


#define MAX_LINK_SIZE 256
//API to parse Eco response coming from cloud to check If Firmware.txt Download link is available for Custom Download
void* parseEchoPostResp(void *RespXML, char *link) {

        char *snRespXml = NULL;
	char *ptr       = NULL;
	int escapeVal   = 0;
	char *result    = 0x00;

        snRespXml = (char*)RespXML;

	APP_LOG("AutoFwUpdate", LOG_DEBUG, "XML String Loaded is %s\n", snRespXml);
	ptr = strstr(snRespXml,"fwUpgradeURL" );
	if(*ptr == NULL)
	    return NULL;
	APP_LOG("AutoFwUpdate", LOG_DEBUG, "XML Parsed is %s\n", ptr);
	result = strtok(ptr, "\"");
	while(result != NULL)
	{
	    APP_LOG("AutoFwUpdate", LOG_DEBUG, "%d . Found: %s\n",escapeVal,result);
	    if(escapeVal == 2)
	    {
		remove_slash(result);
		APP_LOG("AutoFwUpdate", LOG_DEBUG, "%d . After Removing Slash : %s\n",escapeVal,result);
		snprintf(link,MAX_LINK_SIZE,"%s",result);
		break;
	    }
	    result = strtok(NULL, "\"");
	    escapeVal++;
	}

        return NULL;
}



#define FW_FILE_PATH	"/tmp/FwFile.txt"
#define MAX_READ_SIZE	1024
#define	DEVICE		"WeMoSignedEcho"
#define DEVICE_LEN      14
#define FW_VER_LEN      22

void ParseFirmwareTxtResp(void *sendNfResp)
{

    char *fwTxtResp = NULL;
    int counter=0;
    struct Command* temp=NULL;
    FILE *FwFile;
    int FileLen=0;
    char ReadBuffer[MAX_READ_SIZE] = {0,};
    //char FwDownloadUrl[MAX_READ_SIZE] = {0,};
    FirmwareUpdateInfo fwUpdInf;

    fwTxtResp = (char*)sendNfResp;
    FileLen = strlen(fwTxtResp) + 1;
    APP_LOG("AutoFwUpdate", LOG_DEBUG, "Length: %d Response received: %s",FileLen,fwTxtResp);
    FwFile = fopen(FW_FILE_PATH,"w");
    if(FwFile)
    {
	counter = fwrite(fwTxtResp,1,FileLen,FwFile);
	APP_LOG("AutoFwUpdate", LOG_DEBUG, "Total Bytes Written to File: %s is %d",FW_FILE_PATH,counter)
    }
    else{
	APP_LOG("AutoFwUpdate", LOG_ERR, "Cannot Open File: %s",FW_FILE_PATH);
    }
    fclose(FwFile);
    FwFile = fopen(FW_FILE_PATH,"r");
    if(FwFile)
    {
	while(1)
	{
	    memset(ReadBuffer,0,MAX_READ_SIZE);
	    if(fgets(ReadBuffer,MAX_READ_SIZE, FwFile) != NULL)
	    {
		if(strncmp(ReadBuffer,DEVICE,DEVICE_LEN) == 0)
		{
		    APP_LOG("AutoFwUpdate", LOG_DEBUG, "Device Found: %s",DEVICE);
		    memset(ReadBuffer,0,MAX_READ_SIZE);
		    if(fgets(ReadBuffer,MAX_READ_SIZE, FwFile) != NULL)
		    {
			if(strncmp(ReadBuffer,g_szFirmwareVersion,FW_VER_LEN) != 0)
			{
			     APP_LOG("AutoFwUpdate", LOG_DEBUG, "Version Different: %s",g_szFirmwareVersion);
			     if(fgets(ReadBuffer,MAX_READ_SIZE, FwFile) != NULL)
			     {
				 memset(ReadBuffer,0,MAX_READ_SIZE);
				 if(fgets(ReadBuffer,MAX_READ_SIZE, FwFile) != NULL)
				 {
				    SAFE_STRCPY(fwUpdInf.firmwareURL,ReadBuffer);
				    APP_LOG("AutoFwUpdate", LOG_DEBUG, "Download URL: %s",fwUpdInf.firmwareURL);
				    fwUpdInf.withUnsignedImage = 0;
				    fwUpdInf.dwldStartTime = rand() % 3600;
				    StartFirmwareUpdate(fwUpdInf);
				    break;
				 }
			     }
			     else
				break;
			}
			else
			    break;
		    }
		    else
			break;
		}
	    }
	    else
		break;
	}
	fclose(FwFile);
	system("rm /tmp/FwFile.txt");
    }
}




void* UpdateFw(void *args)
{

	char *szHomeId = NULL;
	unsigned int randomtime;
	char *downloadLink = NULL;

	int retVal = PLUGIN_SUCCESS;
	int ret = PLUGIN_SUCCESS;
	int count=0;
	int randSleep=0;
	int errnum;

	authSign *sign = NULL;
	int year = 0x00, monthIndex = 0x00, seconds = 0x00, dayIndex = 0x00;

	pthread_attr_t fwTxt_attr;
	pthread_t fwTxtthread=-1;


	// Now to perform a randomly deffered Firmware Upgrade
	// if Remote Enabled Do a Custom Download of Firmware.txt with http 'post'
	while(1)
	{
	    GetCalendarDayInfo(&dayIndex, &monthIndex, &year, &seconds);
	    if(year != 2000)
	    {
		APP_LOG("DEVICE:rule", LOG_DEBUG, "*************start Firmware UPdate TASK NOW YEAR IS NOT 2000");
		break;
	    }
	    pluginUsleep(10*1000000);  //wait for 10 sec and check again

	}

	while(1)
	{
	    /*calculate sleep interval*/
	    GetCalendarDayInfo(&dayIndex, &monthIndex, &year, &seconds);
	    randomtime = rand() % 3600;
	    APP_LOG("AutoFwUpdate", LOG_DEBUG, "CurTime: %d , randomtime: %d",seconds,randomtime);
	    randSleep = (86400 - seconds) + randomtime;


	    /* loop until we find Internet connected */
	    APP_LOG("AutoFwUpdate", LOG_ERR, "\n Firmware Update Thread Sleeping for %d Seconds\n",randSleep);
	    sleep(randSleep);
	    /* if Remote access enabled */
	    if ((0x00 != strlen(g_szHomeId) ) && (0x00 != strlen(g_szPluginPrivatekey))\
		     && (atoi(g_szRestoreState) == 0x0) &&(gpluginRemAccessEnable == 1))
	    {
		/* if Internet available */
		while(1)
		{
		    if (getCurrentClientState() == STATE_CONNECTED) {
			break;
		    }
		    pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);  //30 sec
		}
		if ((-1 != firmwareUpThread) || (-1 != fwUpMonitorthread))
    		{
			APP_LOG("AutoFwUpdate",LOG_DEBUG, "====Firmware Update thread already created====");
			if(-1 != firmwareUpThread)
			{
			    errnum = pthread_kill(firmwareUpThread, SIGRTMIN);
			    if(errnum){
				APP_LOG("AutoFwUpdate",LOG_ERR, "pthread_kill ret: %d...",errnum);
				exit(0);
			    }

			    firmwareUpThread = -1;
			}
			if(-1 != fwUpMonitorthread)
			{
			    errnum = pthread_kill(fwUpMonitorthread, SIGRTMIN);
			    if(errnum){
				APP_LOG("AutoFwUpdate",LOG_ERR, "pthread_kill ret: %d...",errnum);
				exit(0);
			    }

			    fwUpMonitorthread = -1;
			}
			//Do we need to Set the "currFWUpdateState" also to FM_STATUS_DEFAULT or FM_STATUS_DOWNLOAD_UNSUCCESS
			setCurrFWUpdateState(FM_STATUS_DEFAULT);
    		}

      if(downloadLink)
      {
           free(downloadLink);
           downloadLink = NULL;
      }

		downloadLink  = (char *)CALLOC(1, SIZE_1024B);
		if(!downloadLink)
		{
		    APP_LOG("AutoFwUpdate",LOG_ERR, "Memory allocation failed!!!");
		    exit(0);
		}

		/* fetch FW upgrade URL */

		if(PLUGIN_SUCCESS == fetchFwUrl(downloadLink))
		{
		    /* initiate download and upgrade */
		    // launch the thread do download Firmware.txt with "downloadLink"
		    APP_LOG("AutoFwUpdate",LOG_DEBUG, "====== Download thread Create======");
		    pthread_attr_init(&fwTxt_attr);
		    pthread_attr_setdetachstate(&fwTxt_attr,PTHREAD_CREATE_DETACHED);
		    ret = pthread_create(&fwTxtthread,&fwTxt_attr,(void*)&Downloadtxt, (void*)downloadLink);

		    if(ret < SUCCESS) {
			APP_LOG("AutoFwUpdate",LOG_DEBUG, "======Firmware.txt Download thread not Created======");
			retVal = FAILURE;
		    }
		}else{
		    APP_LOG("AutoFwUpdate",LOG_DEBUG, "====== Fetch firmware URL failed");
		}

	    }

	}


}
#endif
int StartFirmwareUpdate(FirmwareUpdateInfo fwUpdInf)
{
		int retVal = SUCCESS;
		FirmwareUpdateInfo *pfwUpdInf = NULL;

		if (-1 != fwUpMonitorthread)
		{
				APP_LOG("UPNP: Device", LOG_ERR, "############Firmware Update thread already created################");
				retVal = FAILURE;
				return retVal;
		}

		pfwUpdInf = (FirmwareUpdateInfo *)CALLOC(1, sizeof(FirmwareUpdateInfo));
		if(!pfwUpdInf)
		{
				APP_LOG("UPNPApp",LOG_ERR,"pfwUpdInf CALLOC failed...");
				return FAILURE;
		}

		pfwUpdInf->dwldStartTime = fwUpdInf.dwldStartTime;
		pfwUpdInf->withUnsignedImage = fwUpdInf.withUnsignedImage;
		strncpy(pfwUpdInf->firmwareURL, fwUpdInf.firmwareURL, sizeof(pfwUpdInf->firmwareURL)-1);

		pthread_attr_init(&updateFw_attr);
		pthread_attr_setdetachstate(&updateFw_attr,PTHREAD_CREATE_DETACHED);
		retVal = pthread_create(&fwUpMonitorthread,&updateFw_attr,(void*)&FirmwareUpdateStart, (void*)pfwUpdInf);
		if(retVal < SUCCESS) {
				APP_LOG("UPnPApp",LOG_ERR, "************Firmware Update Start thread not Created");
				retVal = FAILURE;
		}

		return retVal;
}

static int IsRuleDbCreated()
{
		int ret = 0x00;
		FILE* pfDb = fopen(RULE_DB_PATH , "r");
		if (pfDb)
		{
				ret = 0x01;
		}

		fclose(pfDb);

		return ret;
}

char*	GetRuleDBVersionIF(char* buf)
{
		char *szVersion = GetDeviceConfig(RULE_DB_VERSION_KEY);
		if (szVersion && strlen(szVersion))
		{
				strncpy(buf, szVersion, SIZE_16B-1);
		}
		else
		{
				strncpy(buf, "0", SIZE_16B-1);
		}

		return buf;
}
void	SetRuleDBVersionIF(char* buf)
{
		if (0x00 == buf)
				return;

		SaveDbVersion(buf);
}


char* GetRuleDBPathIF(char* buf)
{

  if (0x00 == buf)
    return 0x00;

  if (IsRuleDbCreated())
  {
    strncpy(buf, RULE_DB_PATH, strlen(RULE_DB_PATH));
  }


  return buf;
}
/*
 *
 * Make sure device is not in debounce test
 */

static ithread_t sensorDebounceHandle = -1;
int IsSensorDebounceTime()
{
		int ret = 0x00;

		LockLED();

		if (-1 != sensorDebounceHandle)
		{
				ret = 0x01;
		}

		UnlockLED();

		return ret;
}


void *RemoveSensorDebounce(void *args)
{
                tu_set_my_thread_name( __FUNCTION__ );
		pluginUsleep(20000000);
		APP_LOG("timer: network", LOG_DEBUG, "sensor debounce monitor expires");
		LockLED();
		sensorDebounceHandle = -1;
		UnlockLED();
		int status;
		ithread_exit(&status);
}


int CreateSensorDebounceMonitor()
{
		LockLED();
		if (-1 != sensorDebounceHandle)
		{
				ithread_cancel(sensorDebounceHandle);
				sensorDebounceHandle = -1;
				APP_LOG("timer: network", LOG_DEBUG, "sensor debounce monitor removed");
		}

		ithread_create(&sensorDebounceHandle, NULL, RemoveSensorDebounce, NULL);
		ithread_detach(sensorDebounceHandle);

		UnlockLED();

		APP_LOG("timer: network", LOG_DEBUG, "new sensor debounce monitor created");
		return 0;
}


void RestartRule4DST()
{

		//- Calculate now time to deal with edge case of 1:00 AM (DST OFF) and 3:00 AM
		time_t rawTime;
		struct tm * timeInfo;
		time(&rawTime);
		timeInfo = localtime (&rawTime);
		APP_LOG("timer: DST", LOG_DEBUG, "Local time hour:%d", timeInfo->tm_hour);


		if ((DST_TIME_NOW_OFF == timeInfo->tm_hour) ||
						(DST_TIME_NOW_ON == timeInfo->tm_hour) ||
						(DST_TIME_NOW_ON_2 == timeInfo->tm_hour)    //TODO: need to handle similar case of chatham isl. NZ, if gemtek will add support for it
			 )
		{
				//g_iDstNowTimeStatus = timeInfo->tm_hour;
				g_iDstNowTimeStatus = 0x01;
				APP_LOG("timer: DST", LOG_DEBUG, "g_iDstNowTimeStatus:%d", g_iDstNowTimeStatus);
		}
		else
		{
				g_iDstNowTimeStatus = 0x00;
		}


		APP_LOG("Rule", LOG_DEBUG, "DST changed, rule to restart to get executed again");
#ifndef PRODUCT_WeMo_LEDLight
		gRestartRuleEngine = RULE_ENGINE_RELOAD;
#else
  pMessage msg = createMessage(RULE_MESSAGE_RESTART_REQ, 0x00, 0x00);

  if (0x00 != msg)
  {
    SendMessage2App(msg);
  }
#endif
}


char* GetOverridenStatusIF(char* szOutBuff)
{
		if (0x00 == szOutBuff)
				return 0x00;

		lockRule();
		strncpy(szOutBuff, szOverridenStatus, (SIZE_256B*2)-1);
		unlockRule();

		return szOutBuff;
}

/*  @ Function:
 *    ResetOverrideStatus

 *  @ Description:
 *    clean up the storage of override rule, this will be called upon rule get executed and notified
    since it means the executing rule not overridden
 *  @ Parameters:
 *
 *
 *
 *
 */
void ResetOverrideStatus()
{
  memset(szOverridenStatus, 0x00, MAX_OVERRIDEN_STATUS_LEN);
}

/*  IsOverriddenStatus
 *
 *    Check device overridden status in case pushing to device for update
 *
 *
 *
 *
 *
 *
 ********************************************************************************************/
int  IsOverriddenStatus()
{
  int isOverridden = 0x00;
  lockRule();
  if (strlen(szOverridenStatus) > 0x00)

  isOverridden = 0x01;
  unlockRule();

  if (isOverridden)
    PushStoredOverriddenStatus();
  return 0;
}


void PushStoredOverriddenStatus()
{
  char* szCurState[1];

  szCurState[0x00] = (char*)ZALLOC(MAX_RESP_LEN);

  lockRule();
    strncpy(szCurState[0x00], szOverridenStatus, MAX_RESP_LEN-1);
  unlockRule();

    int size = strlen((const char *)szCurState);
    if (size > 0x00)
    {
        //- Reset the last one
        if ('#' == (int)szCurState[size - 1])
            szCurState[0x00][size - 1] = 0x00;
    }

  char* paramters[] = {"RuleOverrideStatus"} ;

  UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].UDN,
      SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].ServiceId, (const char **)paramters, (const char **)szCurState, 0x01);

  APP_LOG("Rule", LOG_DEBUG, "historical overridden status pushed:%s", szCurState[0x00]);

  free(szCurState[0x00]);
}
void Advertisement4TimeUpdate()
{
		if (!IsUPnPNetworkMode())
		{
				APP_LOG("UPnP", LOG_DEBUG, "Not home network mode, %s not executed", __FUNCTION__);
				return;
		}

		//- Send per service, 7 services, so no repeating required here
		UpnpSendAdvertisement(device_handle, default_advr_expire);

		APP_LOG("UPnP", LOG_DEBUG, "Not home network mode, %s executed", __FUNCTION__);
}

/*  Function:
 *    isTimeEventInOVerrideTable
 *  Description:
 *    Check the timer event in historical table or not
 *
 **/
int isTimeEventInOVerrideTable(int time, int action)
{
  int isInTable = 0x00;
  char szMatch[SIZE_64B];
  memset(szMatch, 0x00, sizeof(szMatch));
  snprintf(szMatch, sizeof(szMatch), "%d|%d", time, action);

  APP_LOG("Rule", LOG_DEBUG, "rule string to match: %s", szMatch);

  if ( (0x00 != strlen(szOverridenStatus)) && (0x00 != strstr(szOverridenStatus, szMatch)))
  {
    APP_LOG("Rule", LOG_DEBUG, "string to match found in historical overridden information: %s", szOverridenStatus);
    isInTable = 0x01;
  }
  else
  {
    if (0x00 != strlen(szOverridenStatus))
    {
      APP_LOG("Rule", LOG_DEBUG, "string to match NOT found in historical overridden information: %s", szOverridenStatus);
    }
    else
    {
      APP_LOG("Rule", LOG_DEBUG, "No historical overridden information found");
    }
  }

  return isInTable;
}

/**
 *	@brief SetRouterInfo: to set router information if there
 *		   is no setup, to make sure this is inline with from setup
 *
 *	@param szMac the mac address of router
 *	@param szSSID the ssid of the router
 *
 *  @return void
 */
void SetRouterInfo(const char* szMac, const char* szSSID)
{
		if (0x00 == szMac || 0x00 == szSSID)
				return;

		strncpy(g_routerMac, szMac, sizeof(g_routerMac)-1);
		strncpy(g_routerSsid, szSSID, sizeof(g_routerSsid)-1);
}

/**
 * @brief initSerialRequest: to initial serial number request
 *        Note: in SNS, this request was included in SetApSSID,
 *              it was NOT generic and NOT good for integration
 *
 * @return void
 */
void initSerialRequest()
{
		char szBuff[SIZE_64B];
		memset(szBuff, 0x00, sizeof(szBuff));

		char* szSerialNo = GetSerialNumber();
		if ((0x00 == szSerialNo) || (0x00 == strlen(szSerialNo)))
		{
				//-User default one
				szSerialNo = DEFAULT_SERIAL_NO;
		}

		strncpy(g_szSerialNo, szSerialNo, sizeof(g_szSerialNo)-1);

		APP_LOG("STARTUP: Device", LOG_DEBUG, "serial: %s", g_szSerialNo);
}

#ifdef PRODUCT_WeMo_Smart
#if !defined (PRODUCT_WeMo_Maker) && !defined(PRODUCT_WeMo_Echo)
/**
 * @brief  SetTemplates: Set templates.
 *                  a) If the template list is  specified, then store those
 *                     particular templates. A max of 32 templates can be
 *                     stored.
 *                  b) If no parameters are specified (i.e., if NULL is
 *                     specified), then delete all stored templates.
 * @param  pActionRequest
 * @param  request
 * @param  out
 * @param  errorString
 * @return UPNP_E_SUCCESS: On Success
 *
 * @author Christopher A F
 */
int SetTemplates (pUPnPActionRequest   pActionRequest,
                  IXML_Document       *request,
                  IXML_Document      **out,
                  const char         **errorString)
{
    int              retVal                = UPNP_E_SUCCESS;
    int              tmpRetVal             = -1;
    int              templatesRetVal       = -1;
    int              errorCode             = 0;

    char             aUpnpResp[SIZE_512B]  = {0};
    char             aTempBuf[SIZE_128B]   = {0};
    char             aErrorResp[SIZE_512B] = {0};

    unsigned char    numTemplates          = 0;
    unsigned char    index                 = 0;

    char            *sTemplateList         = NULL;
    const char      *pRulesTNGError        = NULL;
    rtng_template  **pDecodedTemplates     = NULL;

    FILE            *pFile                 = NULL;
    rtng_status_info *status_info          = NULL;

    APP_LOG ("UPNP: Device", LOG_DEBUG, "***** Entered *****");

    /* Input parameter validation */
    if ((NULL == pActionRequest) || (NULL == request))
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Invalid arguments", __LINE__);
        errorCode = retVal = UPNP_E_INVALID_PARAM;
        snprintf (aUpnpResp, sizeof (aUpnpResp), "Invalid Parameters!");
        goto CLEAN_RETURN;
    }

    /* Extract the Template List */
    sTemplateList = Util_GetFirstDocumentItem (
                                           pActionRequest->ActionRequest,
                                           "templateList");
    if ((NULL == sTemplateList) || (0 == strlen (sTemplateList)))
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Error in templateList",
                __LINE__);
        errorCode = retVal = UPNP_E_INVALID_ARGUMENT;
        snprintf (aUpnpResp, sizeof (aUpnpResp), "templateList is Empty!");
        goto CLEAN_RETURN;
    }

    /* Write the payload received to a file */
    pFile = fopen ("/tmp/Belkin_settings/LocalSetTemplates.txt", "w");
    if (NULL == pFile)
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: fopen() Error", __LINE__);
        errorCode = retVal = UPNP_E_INVALID_ARGUMENT;
        snprintf (aUpnpResp, sizeof (aUpnpResp), "fopen() Error!");
        goto CLEAN_RETURN;
    }

    fputs (sTemplateList, pFile);
    fclose (pFile);

    status_info = make_rtng_status_info();
    if( !status_info ) {
        APP_LOG("UPNPDevice",LOG_ERR,"make_rtng_status_info() failed");
        goto CLEAN_RETURN;
    }

    APP_LOG ("UPNPDevice", LOG_DEBUG, "sTemplateList: %s", sTemplateList);
    pDecodedTemplates = DecodeTemplates (sTemplateList, &numTemplates, status_info);
    APP_LOG ("UPNPDevice", LOG_DEBUG, "numTemplates: %d", numTemplates);

    /* Check number of templates */
    if (0 == numTemplates)
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG,
                 "%d: numTemplates is zero. "
                 "All stored templates will be deleted", __LINE__);
        snprintf (aUpnpResp, sizeof (aUpnpResp),
                  "Number of Templates is ZERO! "
                  "All stored templates will be deleted\n");
    }
    else
    {
        if (NULL == pDecodedTemplates)
        {
            APP_LOG ("UPNPDevice", LOG_DEBUG,
                     "%d: Error in DecodeTemplates()", __LINE__);
            errorCode = retVal = status_info->status;
            snprintf (aUpnpResp, sizeof (aUpnpResp),
                      "Error in decoding the XML!");
            goto CLEAN_RETURN;
        }

        snprintf (aUpnpResp, sizeof (aUpnpResp),
                  "Number of templates to process: %d\n", numTemplates);
    }

    for (index = 0; index < numTemplates; index++)
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG,
                 "Decoded template details are as follows");
        APP_LOG ("UPNPDevice", LOG_DEBUG, "Template[%d]: id = %s", index,
                 pDecodedTemplates[index]->header.id);
        APP_LOG ("UPNPDevice", LOG_DEBUG, "Template[%d]: description = %s",
                 index, pDecodedTemplates[index]->header.description);
        APP_LOG ("UPNPDevice", LOG_DEBUG, "Template[%d]: min_version = %d",
                 index, pDecodedTemplates[index]->header.min_version);
        APP_LOG ("UPNPDevice", LOG_DEBUG, "Template[%d]: max_version = %d",
                 index, pDecodedTemplates[index]->header.max_version);

        /* Strip [CDATA[]] from the template body */
        tmpRetVal = StripCdata ((char *) pDecodedTemplates[index]->body);
        if (0 != tmpRetVal)
        {
            APP_LOG ("UPNPDevice", LOG_DEBUG,
                     "%d: StripCdata Error for Template[%d]",
                     __LINE__, index);
            errorCode = retVal = UPNP_E_INVALID_PARAM;
            snprintf (aUpnpResp, sizeof (aUpnpResp),
                      "Error in stripping CDATA!");
            goto CLEAN_RETURN;
        }

        APP_LOG ("UPNPDevice", LOG_DEBUG, "Template[%d]: body = %s", index,
                 pDecodedTemplates[index]->body);
        pDecodedTemplates[index]->body_size =
          strlen ((const char *)pDecodedTemplates[index]->body) + 1;
        APP_LOG ("UPNPDevice", LOG_DEBUG, "Template[%d]: body_size = %d",
                 index, pDecodedTemplates[index]->body_size);
    }

    for (index = 0; index < numTemplates; index++)
    {
        /* Check if the template Id is NULL */
        if (NULL == pDecodedTemplates[index]->header.id)
        {
            APP_LOG ("UPNPDevice", LOG_DEBUG,
                     "%d: Template ID is NULL: will not be processed",
                     __LINE__);
            aTempBuf[0] = '\0';
            snprintf (aTempBuf, sizeof (aTempBuf),
                     "Template ID for template %d is NULL!\n", (index + 1));
            SAFE_STRCAT(aUpnpResp, aTempBuf);
        } /* End if (NULL == pDecodedTemplates[index]->header.id) */
    } /* End for */

    /* Invoke rtng_set_templates() to store/delete templates */
    templatesRetVal = rtng_set_templates (numTemplates, \
                             (rtng_template const * const *) pDecodedTemplates);
    errorCode = templatesRetVal;
    aTempBuf[0] = '\0';
    if (0 != templatesRetVal)
    {
        pRulesTNGError = rtng_last_error ();
        if (NULL == pRulesTNGError)
        {
            APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: rtng_last_error() Error",
                     __LINE__);
            errorCode = retVal = UPNP_E_INVALID_PARAM;
            snprintf (aUpnpResp, sizeof (aUpnpResp),
                      "Error in rtng_last_error()!");
            goto CLEAN_RETURN;
        }

        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: !!! pRulesTNGError = %s !!!",
                 __LINE__, pRulesTNGError);

        if (numTemplates)
        {
            snprintf (aTempBuf, sizeof (aTempBuf),
                      "Storing/Deletion of remaining templates Unsuccessful!\n");
        }
        else
        {
            snprintf (aTempBuf, sizeof (aTempBuf),
                      "Deletion of templates Unsuccessful!\n");
        }
        retVal = UPNP_E_INVALID_PARAM;
    }
    else
    {
        if (numTemplates)
        {
            snprintf (aTempBuf, sizeof (aTempBuf),
                      "Storing/Deletion of remaining templates Successful!\n");
        }
        else
        {
            snprintf (aTempBuf, sizeof (aTempBuf),
                      "Deletion of templates Successful!\n");
        }
    } /* End if (0 != templatesRetVal) */

    APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: %s", __LINE__, aTempBuf);
    SAFE_STRCAT(aUpnpResp, aTempBuf);

CLEAN_RETURN:
    if (NULL != sTemplateList)
        FreeXmlSource (sTemplateList);

    /* NOTE: This template pointer array deallocation code is duplicated
     * in at least 1 other place (RemoteAccess/SmartRemoteHandlers.c).
     * Recommend moving both into a common utility function in
     * rulestng_utils.c.
     */
    if (NULL != pDecodedTemplates)
    {
        /* Free the memory allocated for each template */
        for (index = 0; index < numTemplates; index++)
        {
            if (NULL != pDecodedTemplates[index])
            {
                free (pDecodedTemplates[index]);
                pDecodedTemplates[index] = NULL;
            }
        }

        free (pDecodedTemplates);
        pDecodedTemplates = NULL;
    }

    APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: %s", __LINE__, aUpnpResp);

    if (NULL == pRulesTNGError)
    {
        snprintf (&aErrorResp[0], sizeof (aErrorResp),
                  "<errorCode>%d</errorCode>"
                  "<errorString>%s</errorString>",
                  status_info->status, status_info->msg );
    }
    else
    {
        snprintf (&aErrorResp[0], sizeof (aErrorResp),
                  "<errorCode>%d</errorCode>"
                  "<errorString>%s</errorString>",
                  errorCode, pRulesTNGError);
    }

    APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: %s", __LINE__, aErrorResp);

    pActionRequest->ActionResult = NULL;
    pActionRequest->ErrCode      = 0x00;
    UpnpAddToActionResponse (&pActionRequest->ActionResult,
                             "SetTemplates",
         CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE], "templateList",
                             aErrorResp);

    APP_LOG ("UPNP: Device", LOG_DEBUG, "***** Leaving *****");
    return retVal;
}

/**
 * @brief  GetTemplates: Retrieve information of all stored templates.
 * @param  pActionRequest
 * @param  request
 * @param  out
 * @param  errorString
 * @return UPNP_E_SUCCESS: On Success
 *
 * @author Christopher A F
 */
int GetTemplates (pUPnPActionRequest   pActionRequest,
                  IXML_Document       *request,
                  IXML_Document      **out,
                  const char         **errorString)
{
    int                      retVal                       = UPNP_E_SUCCESS;
    int                      errorCode                    = 0;

    char                     aUpnpResp[SIZE_50B]          = {0};
    char                     aErrorResp[SIZE_512B]        = {0};

    bool                     encodeXml                    = FALSE;

    char                    *pEncodedTemplateList         = NULL;
    const char              *pRulesTNGError               = NULL;

    rtng_template_metainfo  *pTemplateList                = NULL;

    APP_LOG ("UPNP: Device", LOG_DEBUG, "***** Entered *****");

    /* Input parameter validation */
    if ((NULL == pActionRequest) || (NULL == request))
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Invalid arguments", __LINE__);
        errorCode = retVal = UPNP_E_INVALID_PARAM;
        snprintf (aUpnpResp, sizeof (aUpnpResp), "Invalid Parameters!");
        goto CLEAN_RETURN;
    }

    /* Invoke rtng_get_templates_info() to fetch all templates */
    pTemplateList = rtng_get_templates_info ();
    if (NULL == pTemplateList)
    {
        pRulesTNGError = rtng_last_error ();
        if (NULL == pRulesTNGError)
        {
            APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: rtng_last_error() Error",
                     __LINE__);
            errorCode = retVal = UPNP_E_INVALID_PARAM;
            snprintf (aUpnpResp, sizeof (aUpnpResp),
                      "Error in rtng_last_error()!");
            goto CLEAN_RETURN;
        }

        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: !!! pRulesTNGError = %s !!!",
              __LINE__, pRulesTNGError);

        APP_LOG ("UPNPDevice", LOG_DEBUG,
                 "%d: Error in fetching all templates",
                 __LINE__);
        retVal = UPNP_E_INVALID_PARAM;
        snprintf (aUpnpResp, sizeof (aUpnpResp),
                  "Error in fetching all templates!");
        errorCode = -1;
        goto CLEAN_RETURN;
    }
    else
    {
        if (0 == pTemplateList->count)
        {
            APP_LOG ("REMOTEACCESS", LOG_DEBUG,
                     "%d: Returned template count from RulesTNG core is ZERO",
                     __LINE__);
            retVal = UPNP_E_INVALID_PARAM;
            snprintf (aUpnpResp, sizeof (aUpnpResp),
                      "Returned template count from RulesTNG core is ZERO!");
        }

        APP_LOG ("UPNPDevice", LOG_DEBUG, "pTemplateList: [0x%p]",
                 pTemplateList);

        /* Convert pTemplateList to XML */
        pEncodedTemplateList = EncodeTemplates (pTemplateList);
        if (NULL == pEncodedTemplateList)
        {
            APP_LOG( "UPNPDevice", LOG_DEBUG, "%d: Error in EncodeTemplates()",
                     __LINE__);
            errorCode = retVal = UPNP_E_INVALID_PARAM;
            snprintf (aUpnpResp, sizeof (aUpnpResp),
                      "Error in encoding templates!");
            goto CLEAN_RETURN;
        }
        else
        {
            APP_LOG ("UPNPDevice", LOG_DEBUG, "pEncodedTemplateList: [0x%p]",
                     pEncodedTemplateList);
            encodeXml = TRUE;
        }
    }

    if (TRUE == encodeXml)
    {
        /* Encode XML */
		 char *pEscaped;

		 if((pEscaped = EscapeXML(pEncodedTemplateList)) == NULL) {
			 APP_LOG ("UPNPDevice", LOG_ERR,"EscapeXML failed");
			 errorCode = retVal = UPNP_E_INVALID_PARAM;
			 snprintf (aUpnpResp, sizeof (aUpnpResp), "EscapeXML failed");
			 encodeXml = FALSE;
			 goto CLEAN_RETURN;
		 }
		 else {
			 free(pEncodedTemplateList);
			 pEncodedTemplateList = pEscaped;
		 }

        APP_LOG ("UPNPDevice", LOG_DEBUG, "pEncodedTemplateList: %s",
                pEncodedTemplateList);

        pActionRequest->ActionResult = NULL;
        pActionRequest->ErrCode      = 0x00;
        UpnpAddToActionResponse (&pActionRequest->ActionResult,
                                 "GetTemplates",
            CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE], "templateList",
                                 pEncodedTemplateList);
    }

CLEAN_RETURN:
    if (NULL != pTemplateList)
        rtng_free_template_metainfo (pTemplateList);

    if (NULL != pEncodedTemplateList)
    {
        free (pEncodedTemplateList);
        pEncodedTemplateList = NULL;
    }

    if (FALSE == encodeXml)
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "aUpnpResp: %s", aUpnpResp);

        if (NULL == pRulesTNGError)
        {
            snprintf (&aErrorResp[0], sizeof (aErrorResp),
                      "<errorCode>%d</errorCode>"
                      "<errorString>%s</errorString>",
                      errorCode, aUpnpResp);
        }
        else
        {
            snprintf (&aErrorResp[0], sizeof (aErrorResp),
                      "<errorCode>%d</errorCode>"
                      "<errorString>%s</errorString>",
                      errorCode, pRulesTNGError);
        }

        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: %s", __LINE__, aErrorResp);

        pActionRequest->ActionResult = NULL;
        pActionRequest->ErrCode      = 0x00;
        UpnpAddToActionResponse (&pActionRequest->ActionResult,
                                 "GetTemplates",
                             CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE],
                                 "templateList", aErrorResp);
    }

    APP_LOG ("UPNP: Device", LOG_DEBUG, "***** Leaving *****");
    return retVal;
}

/**
 * @brief  SetRules: Set a rule.
 *                  a) If the rule ID and rule parameters are specified, then
 *                     store that particular rule. A max of 100 rules can be
 *                     stored.
 *                  b) If only the rule ID is specified (and the rule is
 *                     specified as NULL), then delete that particular rule.
 * @param  pActionRequest
 * @param  request
 * @param  out
 * @param  errorString
 * @return UPNP_E_SUCCESS: On Success
 *
 * @author Christopher A F
 */
int SetRules (pUPnPActionRequest   pActionRequest,
              IXML_Document       *request,
              IXML_Document      **out,
              const char         **errorString)
{
	int              retVal                 = UPNP_E_SUCCESS;
	int              tmpRetVal              = -1;
	int              rulesRetVal            = -1;
	int              errorCode              = 0;
	char             aUpnpResp[SIZE_1024B]  = {0};
	char             aTempBuf[SIZE_128B]    = {0};
	char             aErrorResp[SIZE_1024B] = {0};

	unsigned char    numRules               = 0;
	unsigned char    index                  = 0;
	unsigned char    tempIndex              = 0;

	char            *sRuleList              = NULL;
	const char      *pRulesTNGError         = NULL;
	rtng_rule      **pDecodedRules          = NULL;
	FILE            *pFile                  = NULL;
        rtng_status_info *status_info           = NULL;

	APP_LOG ("UPNP: Device", LOG_DEBUG, "***** Entered *****");

	/* Input parameter validation */
	if ((NULL == pActionRequest) || (NULL == request))
	{
		APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Invalid arguments", __LINE__);
		errorCode = retVal = UPNP_E_INVALID_PARAM;
		snprintf (aUpnpResp, sizeof (aUpnpResp), "Invalid Parameters!");
		goto CLEAN_RETURN;
	}

	/* Extract the Rule List */
	sRuleList = Util_GetFirstDocumentItem (pActionRequest->ActionRequest,
			"ruleList");
	if ((NULL == sRuleList) || (0 == strlen (sRuleList)))
	{
		APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Error in ruleList", __LINE__);
		errorCode = retVal = UPNP_E_INVALID_ARGUMENT;
		snprintf (aUpnpResp, sizeof (aUpnpResp), "RuleList is Empty!");
		goto CLEAN_RETURN;
	}

	/* Write the payload received to a file */
	pFile = fopen ("/tmp/Belkin_settings/LocalSetRules.txt", "w");
	if (NULL == pFile)
	{
		APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: fopen() Error", __LINE__);
		errorCode = retVal = UPNP_E_INVALID_ARGUMENT;
		snprintf (aUpnpResp, sizeof (aUpnpResp), "fopen() Error!");
		goto CLEAN_RETURN;
	}

	fputs (sRuleList, pFile);
	fclose (pFile);

	APP_LOG ("UPNPDevice", LOG_DEBUG, "sRuleList: %s", sRuleList);
        status_info = make_rtng_status_info();
        if( !status_info ) {
                APP_LOG("UPNPDevice",LOG_ERR,"make_rtng_status_info() failed");
                goto CLEAN_RETURN;
        }
	pDecodedRules = DecodeRules (sRuleList, &numRules, status_info );
	APP_LOG( "UPNPDevice", LOG_DEBUG,
                 "numRules: %d, status code: %d, Description: \"%s\"",
                 numRules, status_info->status, status_info->msg );

	/* Check number of rules */
	if (0 == numRules)
	{
		APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: numRules is zero. "
				"All stored Rules will be deleted", __LINE__);
		snprintf (aUpnpResp, sizeof (aUpnpResp),
				"Number of Rules is ZERO! All stored"
				" Rules will be deleted\n");
		aTempBuf[0] = '\0';

		/* Invoke rtng_set_rule() to delete all rules */
		rulesRetVal = rtng_set_rule (NULL, NULL, NULL, FALSE, 0, 0, 0, NULL);
		errorCode = rulesRetVal;
		if (0 != rulesRetVal)
		{
			pRulesTNGError = rtng_last_error ();
			if (NULL == pRulesTNGError)
			{
				APP_LOG ("UPNPDevice", LOG_DEBUG,
						"%d: rtng_last_error() Error", __LINE__);
				errorCode = retVal = UPNP_E_INVALID_PARAM;
				snprintf (aUpnpResp, sizeof (aUpnpResp),
						"Error in rtng_last_error()!");
				goto CLEAN_RETURN;
			}

			APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: !!! pRulesTNGError = "
					"%s !!!", __LINE__, pRulesTNGError);

			snprintf (aTempBuf, sizeof (aTempBuf),
					"Deletion of all rules Unsuccessful!");
			retVal = UPNP_E_INVALID_PARAM;
		}
		else
		{
			snprintf (aTempBuf, sizeof (aTempBuf),
					"Deletion of all rules Successful!");
		}
		APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: %s", __LINE__, aTempBuf);
		SAFE_STRCAT(aUpnpResp, aTempBuf);
	}
	else
	{
		if (NULL == pDecodedRules)
		{
			APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Error in DecodeRules()",
					__LINE__);
                        if( errorCode == 0 )
                          errorCode = retVal = UPNP_E_INVALID_PARAM;
                        else
                          retVal = errorCode;
			snprintf (aUpnpResp, sizeof (aUpnpResp),
                                  status_info ? status_info->msg : "Error in decoding the XML!");
                        if( status_info ) retVal = errorCode = status_info->status;
			goto CLEAN_RETURN;
		}

		snprintf (aUpnpResp, sizeof (aUpnpResp),
				"Number of Rules to process: %d\n", numRules);
	} /* End if (0 == numRules) */

	for (index = 0; index < numRules; index++)
	{
		APP_LOG ("UPNPDevice", LOG_DEBUG,
				"Decoded rule details are as follows:");
		APP_LOG ("UPNPDevice", LOG_DEBUG, "Rule[%d]: id = %s\n", index,
				pDecodedRules[index]->id);
		APP_LOG ("UPNPDevice", LOG_DEBUG, "Rule[%d]: rule_name = %s\n", index,
				pDecodedRules[index]->rule_name);
		APP_LOG ("UPNPDevice", LOG_DEBUG, "Rule[%d]: template = %s\n", index,
				pDecodedRules[index]->template_id);
		APP_LOG ("UPNPDevice", LOG_DEBUG, "Rule[%d]: min_version = %d\n",
				index, pDecodedRules[index]->min_version);
		APP_LOG ("UPNPDevice", LOG_DEBUG, "Rule[%d]: max_version = %d\n",
				index, pDecodedRules[index]->max_version);
		APP_LOG ("UPNPDevice", LOG_DEBUG, "Rule[%d]: enabled = %d\n", index,
				pDecodedRules[index]->enabled);
		APP_LOG ("UPNPDevice", LOG_DEBUG, "Rule[%d]: num_params = %d\n",
				index, pDecodedRules[index]->num_params);

		for (tempIndex = 0; tempIndex < pDecodedRules[index]->num_params;
				tempIndex++)
		{
			tmpRetVal = StripCdata ((char *)
					pDecodedRules[index]->params[tempIndex].value);
			if (0 != tmpRetVal)
			{
				APP_LOG ("UPNPDevice", LOG_DEBUG,
						"%d: StripCdata Error for Rule[%d]: param[%d]\n",
						__LINE__, index, tempIndex);
				errorCode = retVal = UPNP_E_INVALID_PARAM;
				snprintf (aUpnpResp, sizeof (aUpnpResp),
						"Error in stripping CDATA!");
				goto CLEAN_RETURN;
			}

			APP_LOG ("UPNPDevice", LOG_DEBUG,
					"Rule[%d]:Param[%d]:: params.name = %s\n", index,
					tempIndex, pDecodedRules[index]->params[tempIndex].name);
			APP_LOG ("UPNPDevice", LOG_DEBUG,
					"Rule[%d]:Param[%d]:: params.value = %s\n", index,
					tempIndex, pDecodedRules[index]->params[tempIndex].value);
		}
	}

	for (index = 0; index < numRules; index++)
	{
		/* Check if the rule Id is NULL */
		if (NULL == pDecodedRules[index]->id)
		{
			APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Rule ID is NULL"
					" :Will not be processed", __LINE__);
			aTempBuf[0] = '\0';
			snprintf (aTempBuf, sizeof (aTempBuf),
					"Rule ID for rule %d is NULL!\n", (index + 1));
			SAFE_STRCAT(aUpnpResp, aTempBuf);
			continue;
		}
		else
		{
			aTempBuf[0] = '\0';
			APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Invoking rtng_set_rule()",
					__LINE__);
			/* Invoke rtng_set_rule() to store/delete the rule */
			rulesRetVal = rtng_set_rule (pDecodedRules[index]->id,
					pDecodedRules[index]->rule_name,
					pDecodedRules[index]->template_id,
					pDecodedRules[index]->enabled,
					pDecodedRules[index]->min_version,
					pDecodedRules[index]->max_version,
					pDecodedRules[index]->num_params,
					pDecodedRules[index]->params);
			errorCode = rulesRetVal;
			if (0 != rulesRetVal)
			{
				pRulesTNGError = rtng_last_error ();
				if (NULL == pRulesTNGError)
				{
					APP_LOG ("UPNPDevice", LOG_DEBUG,
							"%d: rtng_last_error() Error", __LINE__);
					errorCode = retVal = UPNP_E_INVALID_PARAM;
					snprintf (aUpnpResp, sizeof (aUpnpResp),
							"Error in rtng_last_error()!");
					goto CLEAN_RETURN;
				}

				APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: !!! pRulesTNGError"
						" = %s !!!", __LINE__, pRulesTNGError);

				snprintf (aTempBuf, sizeof (aTempBuf),
						"Storing/Deletion of Rule ID %s Unsuccessful!\n",
						pDecodedRules[index]->id);
			}
			else
			{
				snprintf (aTempBuf, sizeof (aTempBuf),
						"Storing/Deletion of Rule ID %s Successful!\n",
						pDecodedRules[index]->id);
			}
			APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: %s", __LINE__, aTempBuf);
			SAFE_STRCAT(aUpnpResp, aTempBuf);
		} /* End if (NULL == pDecodedRules[index]->id) */
	} /* End for */

CLEAN_RETURN:
	if (NULL != pDecodedRules)
	{
		/* Free the memory allocated for each rule */
		for (index = 0; index < numRules; index++)
		{
			if (NULL != pDecodedRules[index])
			{
				free (pDecodedRules[index]);
				pDecodedRules[index] = NULL;
			}
		}

		free (pDecodedRules);
		pDecodedRules = NULL;
	}

	APP_LOG ("UPNPDevice", LOG_DEBUG, "aUpnpResp: %s", aUpnpResp);

	if ((NULL == pRulesTNGError) || (0 == strlen (pRulesTNGError)))
	{
		snprintf (&aErrorResp[0], sizeof (aErrorResp),
				"<errorCode>%d</errorCode>"
				"<errorString>%s</errorString>",
				errorCode, aUpnpResp);
	}
	else
	{
		snprintf (&aErrorResp[0], sizeof (aErrorResp),
				"<errorCode>%d</errorCode>"
				"<errorString>%s</errorString>",
				errorCode, pRulesTNGError);
	}

	APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: %s", __LINE__, aErrorResp);

	pActionRequest->ActionResult = NULL;
	pActionRequest->ErrCode      = 0x00;
	UpnpAddToActionResponse (&pActionRequest->ActionResult, "SetRules",
			CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE], "ruleList",
			aErrorResp);

	/* Actuation details for LOG header, Set rules via mobile App in Local */
	setActuation(ACTUATION_MANUAL_APP);
	setRemote("0");

	APP_LOG ("UPNP: Device", LOG_DEBUG, "***** Invoking Analytics Data *****");
	analyticsData(sRuleList, strlen(sRuleList));
	if (NULL != sRuleList)
	{
		FreeXmlSource (sRuleList);
		sRuleList = NULL;
	}

        free_rtng_status_info( status_info );

	APP_LOG ("UPNP: Device", LOG_DEBUG, "***** Leaving *****");

	return retVal;
}

/**
 * @brief  GetRules: Retrieve information of:
 *                  a) A rule whose ID is specified.
 *                  b) All rules if rule ID is specified as "NULL".
 * @param  pActionRequest
 * @param  request
 * @param  out
 * @param  errorString
 * @return UPNP_E_SUCCESS: On Success
 *
 * @author Christopher A F
 */
int GetRules (pUPnPActionRequest   pActionRequest,
              IXML_Document       *request,
              IXML_Document      **out,
              const char         **errorString)
{
    int                     retVal                   = UPNP_E_SUCCESS;
    int                     errorCode                = 0;

    unsigned int            len                      = 0;
    unsigned int            tempLen                  = 0;
    unsigned int            ruleParamCount           = 0;

    char                    aUpnpResp[SIZE_512B]     = {0};
    char                    aTempBuf[SIZE_128B]      = {0};
    char                    aErrorResp[SIZE_512B]    = {0};

    unsigned char           numRules                 = 0;
    unsigned char           index                    = 0;

    bool                    encodeXml                = FALSE;

    char                   *sRuleList                = NULL;
    char                   *pEncodedRuleList         = NULL;
    char                   *pEncodedRule             = NULL;
    char                   *pGetRuleList             = NULL;
    const char             *pRulesTNGError           = NULL;

    rtng_rule_list const   *pEncodeRuleList          = NULL;
    rtng_rule             **pDecodedRules            = NULL;
    rtng_rule      const   *pEncodeRule              = NULL;
    FILE                   *pFile                    = NULL;

    APP_LOG ("UPNP: Device", LOG_DEBUG, "***** Entered *****");

    /* Input parameter validation */
    if ((NULL == pActionRequest) || (NULL == request))
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Invalid arguments", __LINE__);
        errorCode = retVal = UPNP_E_INVALID_PARAM;
        snprintf (aUpnpResp, sizeof (aUpnpResp), "Invalid Parameters!");
        goto CLEAN_RETURN;
    }

    /* Extract the Rule List */
    sRuleList = Util_GetFirstDocumentItem (pActionRequest->ActionRequest,
                                           "ruleList");
    if ((NULL == sRuleList) || (0 == strlen (sRuleList)))
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Error in ruleList", __LINE__);
        errorCode = retVal = UPNP_E_INVALID_ARGUMENT;
        snprintf (aUpnpResp, sizeof (aUpnpResp), "RuleList is Empty!");
        goto CLEAN_RETURN;
    }

    /* Write the payload received to a file */
    pFile = fopen ("/tmp/Belkin_settings/LocalGetRules.txt", "w");
    if (NULL == pFile)
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: fopen() Error", __LINE__);
        errorCode = retVal = UPNP_E_INVALID_ARGUMENT;
        snprintf (aUpnpResp, sizeof (aUpnpResp), "fopen() Error!");
        goto CLEAN_RETURN;
    }

    fputs (sRuleList, pFile);
    fclose (pFile);

    APP_LOG ("UPNPDevice", LOG_DEBUG, "sRuleList: %s", sRuleList);
    pDecodedRules = DecodeRules (sRuleList, &numRules, NULL );
    APP_LOG ("UPNPDevice", LOG_DEBUG, "numRules: %d", numRules);
    if ((NULL == pDecodedRules) && (0 != numRules))
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Error in DecodeRules()",
                 __LINE__);
        errorCode = retVal = UPNP_E_INVALID_PARAM;
        snprintf (aUpnpResp, sizeof (aUpnpResp),
                  "Error in decoding the XML!");
        goto CLEAN_RETURN;
    }

    /* Check number of rules */
    if (0 == numRules)
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: numRules is zero. All"
                 "stored Rules will be fetched", __LINE__);
        snprintf (aUpnpResp, sizeof (aUpnpResp),
                  "Number of Rules is ZERO! All stored"
                  " Rules will be fetched\n");

        /* Invoke rtng_get_all_rules() to fetch all rules */
        pEncodeRuleList = rtng_get_all_rules ();
        if (NULL == pEncodeRuleList)
        {
            pRulesTNGError = rtng_last_error ();
            if (NULL == pRulesTNGError)
            {
                APP_LOG ("UPNPDevice", LOG_DEBUG,
                         "%d: rtng_last_error() Error", __LINE__);
                errorCode = retVal = UPNP_E_INVALID_PARAM;
                snprintf (aUpnpResp, sizeof (aUpnpResp),
                          "Error in rtng_last_error()!");
                goto CLEAN_RETURN;
            }

            APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: !!! pRulesTNGError = "
                     "%s !!!", __LINE__, pRulesTNGError);

            APP_LOG ("UPNPDevice", LOG_DEBUG,
                     "%d: Error in fetching all rules", __LINE__);
            retVal = UPNP_E_INVALID_PARAM;
            SAFE_STRCAT(aUpnpResp, "Error in fetching all rules!");
            errorCode = -1;
            goto CLEAN_RETURN;
        }
        else
        {
            if (0 == pEncodeRuleList->count)
            {
                APP_LOG ("UPNPDevice", LOG_DEBUG,
                         "%d: Returned rule count from RulesTNG core is ZERO",
                         __LINE__);
                retVal = UPNP_E_INVALID_PARAM;
                SAFE_STRCAT(aUpnpResp,
                         "Returned rule count from RulesTNG core is ZERO!");
            }

            APP_LOG ("UPNPDevice", LOG_DEBUG,
                     "%d: pEncodeRuleList->count= %d",
                     __LINE__, pEncodeRuleList->count);

            /* Calculate the total rule parameter count */
            for (index = 0; index < pEncodeRuleList->count; index++)
            {
                ruleParamCount += pEncodeRuleList->rules[index]->num_params;
            }

            APP_LOG ("UPNPDevice", LOG_DEBUG,
                     "%d: ruleParamCount = %d", __LINE__, ruleParamCount);

            APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: pGetRuleList len = %d",
                     __LINE__, ((sizeof (char) *
                             ((sizeof (rtng_rule) * pEncodeRuleList->count) +
                         (sizeof (rtng_rule_param) * pEncodeRuleList->count *
                                 ruleParamCount + BUF_RULE_PARAM_PAD_SIZE)))));

            pGetRuleList = (char *) ZALLOC((sizeof (char) *
                           ((sizeof (rtng_rule) * pEncodeRuleList->count) +
                          (sizeof (rtng_rule_param) * pEncodeRuleList->count *
                                   ruleParamCount + BUF_RULE_PARAM_PAD_SIZE))));
            if (NULL == pGetRuleList)
            {
                APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Malloc Error", __LINE__);
                errorCode = retVal = UPNP_E_INVALID_PARAM;
                SAFE_STRCAT(aUpnpResp, "Malloc Error!");
                goto CLEAN_RETURN;
            }

            APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: pGetRuleList = 0x%p",
                     __LINE__, pGetRuleList);


            /* Convert pEncodeRuleList to XML */
            pEncodedRuleList = EncodeRuleList (pEncodeRuleList);
            if (NULL == pEncodedRuleList)
            {
                APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Error in EncodeRules()",
                         __LINE__);
                errorCode = retVal = UPNP_E_INVALID_PARAM;
                SAFE_STRCAT(aUpnpResp, "Error in encoding rules!");
                goto CLEAN_RETURN;
            }
            else
            {
                len = strlen (pEncodedRuleList);
                strncpy (&pGetRuleList[0], pEncodedRuleList, len);   // <-----<<< WRONG
                pGetRuleList[len + 1] = '\0';

                encodeXml = TRUE;
            }
        }
    }
    else
    {
        snprintf (aUpnpResp, sizeof (aUpnpResp),
                  "Number of Rules to process: %d\n", numRules);

        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: pGetRuleList len = %d",
                 __LINE__, ((sizeof (char) *
                           ((sizeof (rtng_rule) * numRules) +
                           (sizeof (rtng_rule_param) * numRules *
                           MAX_RULE_PARAMETERS + BUF_RULE_PARAM_PAD_SIZE)))));

        pGetRuleList = (char *) ZALLOC((sizeof (char) *
                       ((sizeof (rtng_rule) * numRules) +
                       (sizeof (rtng_rule_param) * numRules *
                       MAX_RULE_PARAMETERS + BUF_RULE_PARAM_PAD_SIZE))));
        if (NULL == pGetRuleList)
        {
            APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Malloc Error", __LINE__);
            errorCode = retVal = UPNP_E_INVALID_PARAM;
            SAFE_STRCAT(aUpnpResp, "Malloc Error!");
            goto CLEAN_RETURN;
        }

        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: pGetRuleList = 0x%p",
                 __LINE__, pGetRuleList);


        for (index = 0; index < numRules; index++)
        {
            /* Check if the rule Id is NULL */
            if (NULL == pDecodedRules[index]->id)
            {
                APP_LOG ("UPNPDevice", LOG_DEBUG,
                         "%d: Rule ID is NULL: Will not be processed",
                         __LINE__);
                aTempBuf[0] = '\0';
                snprintf (aTempBuf, sizeof (aTempBuf),
                         "Rule ID for rule %d is NULL!\n", (index + 1));
                SAFE_STRCAT(aUpnpResp, aTempBuf);
                continue;
            }
            else
            {
                aTempBuf[0] = '\0';
                APP_LOG ("UPNPDevice", LOG_DEBUG,
                         "%d: Invoking rtng_get_rule()", __LINE__);
                /* Invoke rtng_get_rule() to get the rule */
                pEncodeRule = rtng_get_rule (pDecodedRules[index]->id);
                if (NULL == pEncodeRule)
                {
                    pRulesTNGError = rtng_last_error ();
                    if (NULL == pRulesTNGError)
                    {
                        APP_LOG ("UPNPDevice", LOG_DEBUG,
                                 "%d: rtng_last_error() Error", __LINE__);
                        errorCode = retVal = UPNP_E_INVALID_PARAM;
                        snprintf (aUpnpResp, sizeof (aUpnpResp),
                                  "Error in rtng_last_error()!");
                        goto CLEAN_RETURN;
                    }

                    APP_LOG ("UPNPDevice", LOG_DEBUG,
                             "%d: !!! pRulesTNGError = %s !!!", __LINE__,
                             pRulesTNGError);

                    snprintf (aTempBuf, sizeof (aTempBuf),
                              "Retrieval of Rule ID %s Unsuccessful!\n",
                              pDecodedRules[index]->id);
                    errorCode = -1;
                }
                else
                {
                    snprintf (aTempBuf, sizeof (aTempBuf),
                              "Retrieval of Rule ID %s Successful!\n",
                              pDecodedRules[index]->id);
                    pEncodedRule = EncodeRule (pEncodeRule);
                    if (NULL == pEncodedRule)
                    {
                        APP_LOG ("UPNPDevice", LOG_DEBUG,
                                 "%d: Error in EncodeRule() for Rule ID %s",
                                 __LINE__, pDecodedRules[index]->id);
                        aTempBuf[0] = '\0';
                        snprintf (aTempBuf, sizeof (aTempBuf),
                                  "Error in EncodeRule() for Rule ID %s",
                                  pDecodedRules[index]->id);
                        SAFE_STRCAT(aUpnpResp, aTempBuf);
                        errorCode = retVal = UPNP_E_INVALID_PARAM;
                    }
                    else
                    {
                        len = strlen (pEncodedRule);

                        if (0 == tempLen)
                        {
                            strncpy (&pGetRuleList[tempLen], pEncodedRule, len);   // <-----<<< WRONG
                            pGetRuleList[len + 1] = '\0';
                            tempLen = len;
                        }
                        else
                        {
                            strncpy (&pGetRuleList[tempLen],
                              (const char *) &pEncodedRule[RULES_HEADER_SIZE],
                                    len - RULES_HEADER_SIZE);   // <-----<<< WRONG
                            pGetRuleList[len - RULES_HEADER_SIZE + 1] = '\0';
                            tempLen = len - RULES_HEADER_SIZE;
                        }
                    }

                    if (NULL != pEncodeRule)
                        rtng_free_rule (pEncodeRule);
                }
                APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: %s", __LINE__,
                         aTempBuf);
                SAFE_STRCAT(aUpnpResp, aTempBuf);
            } /* End if (NULL == pDecodedRules[index]->id) */
        } /* End for */

        len = strlen (pGetRuleList);
        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: len of pGetRulelist = %d",
                 __LINE__, len);
        if (0 != len)
        {
            strncpy (&pGetRuleList[len], "</rules>", strlen ("</rules>"));   // <-----<<< WRONG & STUPID

            len = strlen (pGetRuleList);
            pGetRuleList[len + 1] = '\0';

            encodeXml = TRUE;
        }
    } /* End if (0 == numRules) */

    if (TRUE == encodeXml)
    {
		  char *cp;
        APP_LOG ("UPNPDevice", LOG_DEBUG, "pGetRuleList: %s",
                pGetRuleList);

     // Escape '<' and '>' characters
        if((cp = EscapeXML(pGetRuleList)) == NULL) {
            APP_LOG ("UPNPDevice", LOG_DEBUG, "EscapeXML failed");
            errorCode = retVal = UPNP_E_INVALID_PARAM;
            snprintf (aUpnpResp, sizeof (aUpnpResp),
                      "Error in encodeXML()!");
            encodeXml = FALSE;
            goto CLEAN_RETURN;
        }
		  free(pGetRuleList);
		  pGetRuleList = cp;

        APP_LOG ("UPNPDevice", LOG_DEBUG, "pGetRuleList: %s",
                 pGetRuleList);

        pActionRequest->ActionResult = NULL;
        pActionRequest->ErrCode      = 0x00;
        UpnpAddToActionResponse (&pActionRequest->ActionResult, "GetRules",
                 CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE], "ruleList",
                                 pGetRuleList);
    }

CLEAN_RETURN:
    if (NULL != sRuleList)
        FreeXmlSource (sRuleList);

    if (NULL != pGetRuleList)
    {
        free (pGetRuleList);
        pGetRuleList = NULL;
    }

    if (NULL != pEncodeRuleList)
        rtng_free_rule_list (pEncodeRuleList);

    if (NULL != pEncodedRuleList)
    {
        free (pEncodedRuleList);
        pEncodedRuleList = NULL;
    }

    if (NULL != pEncodedRule)
     {
         free (pEncodedRule);
         pEncodedRule = NULL;
     }

    if (NULL != pDecodedRules)
    {
        /* Free the memory allocated for each rule */
        for (index = 0; index < numRules; index++)
        {
            if (NULL != pDecodedRules[index])
            {
                free (pDecodedRules[index]);
                pDecodedRules[index] = NULL;
            }
        }

        free (pDecodedRules);
        pDecodedRules = NULL;
    }

    if (FALSE == encodeXml)
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "aUpnpResp: %s", aUpnpResp);

        if (NULL == pRulesTNGError)
        {
            snprintf (&aErrorResp[0], sizeof (aErrorResp),
                      "<errorCode>%d</errorCode>"
                      "<errorString>%s</errorString>",
                      errorCode, aUpnpResp);
        }
        else
        {
            snprintf (&aErrorResp[0], sizeof (aErrorResp),
                      "<errorCode>%d</errorCode>"
                      "<errorString>%s</errorString>",
                      errorCode, pRulesTNGError);
        }

        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: %s", __LINE__, aErrorResp);

        pActionRequest->ActionResult = NULL;
        pActionRequest->ErrCode      = 0x00;
        UpnpAddToActionResponse (&pActionRequest->ActionResult, "GetRules",
                 CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE], "ruleList",
                                 aErrorResp);
    }

    APP_LOG ("UPNP: Device", LOG_DEBUG, "***** Leaving *****");
    return retVal;
}

/**
 * @brief  ResetRulesTNG: Deletes templates and/or rules based on the reset
 *                        type.
 * @param  resetType [IN] - Reset type.
 *
 * @return  void
 *
 * @author Christopher A F
 */
static void ResetRulesTNG (int resetType)
{
    int    tempRetVal     = 0;
    bool   deleteRules    = FALSE;

    APP_LOG ("UPNP: Device", LOG_DEBUG, "***** Entered *****");
    APP_LOG ("UPNP: Device", LOG_DEBUG, "%d: resetType = %d", __LINE__,
             resetType);

    switch (resetType)
    {
        case META_SOFT_RESET:
        case META_REMOTE_RESET:
            {
                /* Delete all rules only */
                deleteRules = TRUE;
                break;
            }

        case META_FULL_RESET:
            {
                /* Delete all templates and all rules */
                /* Delete Templates */
                tempRetVal = rtng_set_templates (0, NULL);
                if (0 != tempRetVal)
                {
                    APP_LOG ("UPNP: Device", LOG_DEBUG, "Deletion of "
                             "templates Unsuccessful");
                }
                else
                {
                    APP_LOG ("UPNP: Device", LOG_DEBUG, "Deletion of "
                             "templates Successful");
                }

                deleteRules = TRUE;
                break;
            }

        default:
            APP_LOG ("UPNP: Device", LOG_DEBUG, "%d: Invalid reset type",
                     __LINE__);
    }

    if (TRUE == deleteRules)
    {
        /* Delete Rules */
        tempRetVal = rtng_set_rule (NULL, NULL, NULL, FALSE, 0, 0, 0, NULL);
        if (0 != tempRetVal)
        {
            APP_LOG ("UPNP: Device", LOG_DEBUG, "Deletion of "
                     "rules Unsuccessful");
        }
        else
        {
            APP_LOG ("UPNP: Device", LOG_DEBUG, "Deletion of "
                     "rules Successful");
        }
    }

    APP_LOG ("UPNP: Device", LOG_DEBUG, "***** Leaving *****");
}

/**
 * @brief  analyticsData: Called analytic data from setRules, before calling need to check for remote connection
 *
 * @param  Rules MSG and Length of MSG.
 *
 * @return  void
 *
 * @author Rahul H.
 */
void analyticsData(char *args, int len)
{
	int retVal = 0;
	char *msg = NULL;
   pthread_attr_t PthreadAttr;

	if((len <= 0) || (NULL == args))
	{
		APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "@@@@@@@@@Invalid arguments");
		APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "@@@@@@@@@In analytic Data: Length: %d, Data: %s", len, args);

		goto error_exit;
	}
	APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "@@@@@@@@@In analytic Data: Length: %d, Data ARGS: %s", len, args);

	msg = (char*)MALLOC((len+1)*sizeof(char));
	if(NULL == msg)
	{
		APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "@@@@@@@@@malloc error for msg buffer");
		goto error_exit;
	}
	strncpy(msg, args,len);

	APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "@@@@@@@@@In analytic Data: Length: %d, Data MSG: %s", len, msg);

	/* Assign memory to analytic buff, extra 128 bytes for headers */
	g_analyticBuff = (char*)ZALLOC((len+128)*sizeof(char));
	if (NULL == g_analyticBuff)
	{
		APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "g_analyticBuff memory allocation failed");
		goto error_exit;
	}

	APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "@@@@@@@Creating analytic thread");
	/* Call to analytic thread with rules payload */

   pthread_attr_init(&PthreadAttr);
   pthread_attr_setdetachstate(&PthreadAttr,PTHREAD_CREATE_DETACHED);

	retVal = pthread_create(&analyticThreadID,&PthreadAttr, &analyticsSchedule, msg);
	if (retVal != 0)
	{
		APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "Creating analyticsSchedule thread failed");
		goto error_exit;
	}
	else
	{
		APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "@@@@@@@@@@@Analytic thread created");
		goto clean_exit;
	}

error_exit:
	APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "@@@@@@@@@Error Exit");

clean_exit:
	APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "@@@@@@@@@Clean Exit");
}

/**
 * @brief  analyticsSchedule: Analytic thread
 *
 * @param  Analytic Data.
 *
 * @return  void
 *
 * @author Rahul H.
 */

void *analyticsSchedule(void *data)
{
	unsigned int analyticFlag = 0;
	unsigned long timeStamp = 0;
	unsigned int bufLen = 0;

	bufLen = strlen(data) + 128;

	char xmlTail[16] = {0,};
	SAFE_STRCPY(xmlTail, "</analytics>");

	APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "@@@@@@@In analytic thread, buffer size: %d, data: %s", bufLen, (char *) data);
	while(1)
	{
		LockSmartRemote();
		analyticFlag = g_analyticFlag;
		UnlockSmartRemote();

		/* Check if sendAnalytic API is free with global flag, wait for availability of sendAnalytic */
		if(analyticFlag == 0)
		{
			timeStamp = GetUTCTime();
			/* Add analytic Tags and prepare final XML */
			/* Fill g_analyticBuff with analytic data */
			LockSmartRemote();
			snprintf(g_analyticBuff, SIZE_64B, "<analytics action=\"rules\"><dataTS>%ld</dataTS>", timeStamp);
			strncat(g_analyticBuff, data, (bufLen-1));
			strncat(g_analyticBuff, xmlTail, (bufLen-1));

			g_analyticFlag = 1;
			UnlockSmartRemote();

			APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "@@@@@@@@Final analytic buffer: %s", g_analyticBuff);
			if(NULL != data)
			{
				APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "@@@@@@@@Freeing analytic thread memory");
				free(data);
				data = NULL;
			}
			/* Exit thread after processing and free g_analyticBuff in remoteUpdate.c */
		}
		else
		{
			/* if analytic flag is up, exit */
			APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "@@@@@@@@Analytic flag is up");
			sleep(3);
			break;
		}
	}
	APP_LOG ("REMOTE: analyticsData", LOG_DEBUG, "@@@@@@@@Exiting Analytic thread");
	pthread_exit(0);
}
#endif
#endif /* #ifdef PRODUCT_WeMo_Smart */

int ControlCloudUpload(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
    if (0x00 == pActionRequest || !pActionRequest->ActionRequest || 0x00 == strlen(pActionRequest->ActionRequest))
    {
	APP_LOG("UPNP", LOG_ERR, "Parameters error");
	return 0x01;
    }
    else
    {
	char* szAction = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "EnableUpload");

	if ((0x00 == szAction) || (0x00 == strlen(szAction)))
	{

	    pActionRequest->ActionResult = NULL;
	    pActionRequest->ErrCode = 0x01;

	    UpnpAddToActionResponse(&pActionRequest->ActionResult, "ControlCloudUpload", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],
		    "status", "unsuccess");

	    APP_LOG("UPNP: Rule", LOG_ERR, "%s: paramters error", __FUNCTION__);

	    return 0x00;
	}
	else
	{
	    int enable = atoi(szAction);

	    pActionRequest->ActionResult = NULL;
	    pActionRequest->ErrCode = 0x00;

	    if(enable)
	    {
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "ControlCloudUpload", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "WDFile", "Sending");

		if(logFile_thread == -1)
		{
		    APP_LOG("UPNP: Device", LOG_DEBUG,"Start uploading log files");

		    pthread_attr_init(&wdLog_attr);
		    pthread_attr_setdetachstate(&wdLog_attr, PTHREAD_CREATE_DETACHED);
		    pthread_create(&logFile_thread, &wdLog_attr, uploadLogFileThread, NULL);
		    SetBelkinParameter(LOG_UPLOAD_ENABLE, "1");
		    AsyncSaveData();
		}
		else
		    APP_LOG("UPNP: Device", LOG_DEBUG,"File upload thread already running");
	    }
	    else
	    {
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "ControlCloudUpload", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "WDFile", "Stopping");

		if(logFile_thread != -1)
		{
		    APP_LOG("UPNP: Device", LOG_DEBUG,"Stopping messages file uploads");
		    pthread_cancel(logFile_thread);
		    logFile_thread=-1;
		    SetBelkinParameter(LOG_UPLOAD_ENABLE, "0");
		    AsyncSaveData();
		}
		else
		    APP_LOG("UPNP: Device", LOG_DEBUG,"messages file uploads not running");
	    }

	    return UPNP_E_SUCCESS;

	}
    }
}

int ControlCloudDBUpload(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
    if (0x00 == pActionRequest || !pActionRequest->ActionRequest || 0x00 == strlen(pActionRequest->ActionRequest))
    {
	APP_LOG("UPNP", LOG_ERR, "Parameters error");
	return 0x01;
    }
    else
    {
	char* szAction = Util_GetFirstDocumentItem(pActionRequest->ActionRequest, "EnableDBUpload");

	if ((0x00 == szAction) || (0x00 == strlen(szAction)))
	{

	    pActionRequest->ActionResult = NULL;
	    pActionRequest->ErrCode = 0x01;

	    UpnpAddToActionResponse(&pActionRequest->ActionResult, "ControlCloudUpload", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE],
		    "status", "unsuccess");

	    APP_LOG("UPNP: Rule", LOG_ERR, "%s: paramters error", __FUNCTION__);

	    return 0x00;
	}
	else
	{
	    int enable = atoi(szAction);
	    pActionRequest->ActionResult = NULL;
	    pActionRequest->ErrCode = 0x00;
	    if(enable)
	    {
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "ControlCloudUpload", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "DBFile", "Sending");
		APP_LOG("UPNP: Device", LOG_DEBUG,"Enabling uploading log files");
  	        SetBelkinParameter(DB_UPLOAD_ENABLE, "1");
	    }
	    else
	    {
		UpnpAddToActionResponse(&pActionRequest->ActionResult, "ControlCloudUpload", CtrleeDeviceServiceType[PLUGIN_E_EVENT_SERVICE], "DBFile", "Stopping");
	        if(dbFile_thread != -1)
		{	
		    pthread_cancel(dbFile_thread);
                    dbFile_thread=-1;
		}
	   	SetBelkinParameter(DB_UPLOAD_ENABLE, "0");
	    }
	    return UPNP_E_SUCCESS;
	}
    }
}

/**
 * As part of WEMO-39685, it was thought to unify the XML input
 * for StoreRules in both Local and Remote mode by adding a new
 * tag "ruleDbData" in UPnP API. However, this would require the
 * App to treat rest of the XML as data to this new tag.
 * Other approach considered here was to have different parsing
 * in local and remote modes and have an API which takes as
 * parameters ruleDbVersion, processDb and ruleDbBody.
 *
 * But the effort in making these changes in both App and FW
 * seem to outweight the benefits of this change for both DEV & QA.
 * So, not going ahead with changes thought to be done in WEMO-39685.
 */
int StoreRules (pUPnPActionRequest   pActionRequest,
                     IXML_Document       *request,
                     IXML_Document      **out,
                     const char         **errorString)
{
    int              retVal                = UPNP_E_SUCCESS;
    unsigned int     len                   = 0;

    char             aUpnpResp[SIZE_128B]  = {0};

    char            *sRuleDbVersion        = NULL;
    char            *sRuleProcessDb        = NULL;
    char            *sRuleDbBody           = NULL;
    char            *pRuleDbData           = NULL;

    /* Input parameter validation */
    if ((NULL == pActionRequest) || (NULL == request))
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Invalid arguments", __LINE__);
        retVal = UPNP_E_INVALID_PARAM;
        snprintf (aUpnpResp, sizeof (aUpnpResp), "Invalid Parameters!");
        goto CLEAN_RETURN;
    }

    /* Extract the rule DB version */
    sRuleDbVersion = Util_GetFirstDocumentItem (
                                           pActionRequest->ActionRequest,
                                           "ruleDbVersion");
    /* Extract the process DB value */
    sRuleProcessDb = Util_GetFirstDocumentItem (
                                           pActionRequest->ActionRequest,
                                           "processDb");
    /* Extract the rule DB body */
    sRuleDbBody = Util_GetFirstDocumentItem (
                                           pActionRequest->ActionRequest,
                                           "ruleDbBody");
    if ((NULL == sRuleDbVersion) || (0 == strlen (sRuleDbVersion)) ||
        (NULL == sRuleProcessDb) || (0 == strlen (sRuleProcessDb)) ||
        (NULL == sRuleDbBody)    || (0 == strlen (sRuleDbBody)))
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Error in payload received",
                __LINE__);
        retVal = UPNP_E_INVALID_ARGUMENT;
        snprintf (aUpnpResp, sizeof (aUpnpResp), "Invalid Payload received!");
        goto CLEAN_RETURN;
    }

    len = strlen (sRuleDbVersion) + strlen (sRuleProcessDb) +
          strlen (sRuleDbBody);

    pRuleDbData = (char *) ZALLOC (len * sizeof (char) + SIZE_128B);
    if (NULL == pRuleDbData)
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Malloc Error", __LINE__);
        retVal = UPNP_E_INVALID_ARGUMENT;
        snprintf (aUpnpResp, sizeof (aUpnpResp), "Invalid rule DB body!");
        goto CLEAN_RETURN;
    }

    snprintf (pRuleDbData, len * sizeof (char) + SIZE_128B,
              "<ruleDbData>"
              "<ruleDbVersion>%s</ruleDbVersion>"
              "<processDb>%s</processDb>"
              "<ruleDbBody>%s</ruleDbBody>"
              "</ruleDbData>",
              sRuleDbVersion, sRuleProcessDb, sRuleDbBody);


    retVal = DecodeRuleDbData (pRuleDbData);
    if (0 == retVal)
    {
        snprintf (aUpnpResp, sizeof (aUpnpResp),
                  "Storing of rules DB Successful!\n");
    }
    else
    {
        snprintf (aUpnpResp, sizeof (aUpnpResp),
                  "Storing of rules DB failed!!\n");
    }

CLEAN_RETURN:
    if (NULL != sRuleDbVersion)
        FreeXmlSource (sRuleDbVersion);

    if (NULL != sRuleProcessDb)
        FreeXmlSource (sRuleProcessDb);

    if (NULL != sRuleDbBody)
        FreeXmlSource (sRuleDbBody);

    if (NULL != pRuleDbData)
    {
        free (pRuleDbData);
        pRuleDbData = NULL;
    }

    APP_LOG ("UPNPDevice", LOG_DEBUG, "errorInfo: %s", aUpnpResp);

    pActionRequest->ActionResult = NULL;
    pActionRequest->ErrCode      = 0x00;
    UpnpAddToActionResponse (&pActionRequest->ActionResult,
                             "StoreRules",
         CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE], "errorInfo",
                             aUpnpResp);

    return retVal;
}

int FetchRules (pUPnPActionRequest   pActionRequest,
                     IXML_Document       *request,
                     IXML_Document      **out,
                     const char         **errorString)
{
    int                      retVal                       = UPNP_E_SUCCESS;

    char                     aUpnpResp[SIZE_50B]          = {0};
    char                     aRuleDBPath[SIZE_256B]       = {0};

    char                    *pRuleDbVersion               = "";

    APP_LOG ("UPNP: Device", LOG_DEBUG, "***** Entered *****");

    /* Input parameter validation */
    if ((NULL == pActionRequest) || (NULL == request))
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: Invalid arguments", __LINE__);
        retVal = UPNP_E_INVALID_PARAM;
        snprintf (aUpnpResp, sizeof (aUpnpResp), "Invalid Parameters!");
        goto CLEAN_RETURN;
    }
    else
      snprintf (aUpnpResp, sizeof (aUpnpResp), "SUCCESS");

    /* Fetch the rule Db version */
    pRuleDbVersion = GetDeviceConfig (RULE_DB_VERSION_KEY);
    if ((NULL == pRuleDbVersion) || (0 == strlen (pRuleDbVersion)))
    {
        APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: RuleDbVersion Error", __LINE__);
        pRuleDbVersion = "0";
    }

    APP_LOG ("UPNPDevice", LOG_DEBUG, "!!! pRuleDbVersion = %s !!!", pRuleDbVersion);

    /* Fetch the rule Db path */
    snprintf (aRuleDBPath, sizeof (aRuleDBPath), "http://%s:%d/rules.db",
              g_server_ip, g_server_port);
    APP_LOG ("UPNPDevice", LOG_DEBUG, "!!! rule Db path = %s !!!", aRuleDBPath);



CLEAN_RETURN:

    pActionRequest->ActionResult = NULL;
    pActionRequest->ErrCode      = 0x00;

    UpnpAddToActionResponse (&pActionRequest->ActionResult,
		    "FetchRules",
		    CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE],
		    "ruleDbPath", aRuleDBPath);

    UpnpAddToActionResponse (&pActionRequest->ActionResult,
		    "FetchRules",
		    CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE],
		    "ruleDbVersion", pRuleDbVersion);

    UpnpAddToActionResponse (&pActionRequest->ActionResult,
		    "FetchRules",
		    CtrleeDeviceServiceType[PLUGIN_E_RULES_SERVICE],
		    "errorInfo", aUpnpResp);

    return retVal;
}

 /************************/
/* emacs settings.       */
/* Please do not remove  */
/* Local Variables:      */
/* indent-tabs-mode: nil */
/* tab-width: 4          */
/* c-basic-offset: 4     */
/* End:                  */
/************************/
