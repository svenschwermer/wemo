/***************************************************************************
*
*
* gemtek_api.h
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
#ifndef		__GEMTEK__API__
#define 	__GEMTEK__API__

#include "ixp4xx_gpio.h"
#include "gemtek_function.h"

#define Gemtek_Success 0
#define Gemtek_Error -1


// 1	Interfaces with NTP
int EnableNTP(int isEnable);
int SetNTP(char *  ServerIP,int Index, int EnableDaylightSaving);
int RunNTP();
unsigned long int GetNTPUpdatedTime(void);
unsigned long int GetUTCTime(void);


// 2	Interfaces with ECO - Time Scheduler /*not implement*/
typedef  struct scheduler 
{ 
  unsigned int    start_day; // 1 ~ 7 
  unsigned int    end_day; // 1 ~ 7
  unsigned int    start_time_hour; // 00 ~ 23
  unsigned int    start_time_minute; // 00 ~ 59
  unsigned int    end_time_hour; // 00 ~ 23
  unsigned int    end_time_minute; // 00 ~ 59
  int	switch_state; // 0: OFF , 1: ON
  struct ECO_Scheduler * next;
}  ECO_Scheduler; 

int EnableECO(int isEnable);
int SetECOScheduler(ECO_Scheduler* Rule);
ECO_Scheduler* GetECOScheduler(void);
int RemoveECOScheduler(ECO_Scheduler* Rule);
ECO_Scheduler* GetECOSchedulerStatus(void);


// 3	Interfaces with Firmware Update
int Firmware_Update(char* File_Path);

// 4	Interfaces with Update System Time
int SetTime(unsigned int Seconds, int Index, int EnableDaylightSaving);
unsigned long int GetSystemTime(void);

// 5	Interfaces with Device Info
char* GetMACAddress(void);
int SetMACAddress(char *mac_addr);
char* GetSerialNumber(void);
int SetSerialNumber(char *serial_number);
char* GetUDN(void);
int SetUDN(char *udn);

// 6	Interfaces with Device LAN Info
char* GetLanIPAddress(void);
int SetLanIPAddress(char *lan_ipaddr);
char* GetLanSubnetMask(void);
int SetLanSubnetMask(char* lan_netmask);

// 7	Interfaces with Device WAN Info
char* GetWanIPAddress(void);
char* GetWanSubnetMask(void);
char* GetWanDefaultGateway(void);

// 8	Interfaces with AP
int EnableAP(int isEnable);
int GetEnableAP(void);
int SetAPSSID(const char* szSSID);
const char* GetAPSSID(void);

//void SetAPAuthMode(char * mode);
//char * GetAPAuthMode(void);
enum AUTH_MODE_E {
    OPEN = 0,
    WEPAUTO,
    WPAPSK,
    WPA2PSK,
    WPAPSKWPA2PSK
};

int SetAPAuthMode(enum AUTH_MODE_E mode);
enum AUTH_MODE_E GetAPAuthMode(void);

//void SetAPEncrypType(int type);
//int GetAPEncrypType(void);
enum ENCRYP_TYPE_E {
    NONE = 0,
    WEP,
    TKIP,
    AES,
    TKIPAES
};

int SetAPEncrypType(enum ENCRYP_TYPE_E type);
enum ENCRYP_TYPE_E GetAPEncrypType(void);

int SetAPPSKKey(const char* szKey);
const char* GetAPPSKKey(void);
int SetAPChannel(int channel);
int GetAPChannel(void);

enum WPS_MODE_E {
    PBC = 0,
    PIN
};

typedef char* WPS_PIN;

int SetWPSMode(enum WPS_MODE_E mode);
int SetWPSPin(WPS_PIN pin);
//enum WPS_MODE_E GetWPSMode(void);
WPS_PIN GetWPSPin(void);
PRT_802_11_MAC_ENTRY GetMacApTable(); /*not implement*/

// 9	Interfaces with AP-Client
//int EnableApClientAutoCon(int isEnable);
//int GetEnableApClientAutoCon(void);
int GetApClientConnStatus();
int EnableApClient(int isEnable);
int GetEnableApClient(void);
int SetAPClientSSID(const char* szSSID);
const char* GetAPClientSSID(void);
int SetApClientBSsid(const char* szBSSID);
const char* GetApClientBSsid(void);
int SetApClientAuthMode(enum AUTH_MODE_E mode);
enum AUTH_MODE_E GetApClientAuthMode(void);
int SetApClientEncrypType(enum ENCRYP_TYPE_E type);
enum ENCRYP_TYPE_E GetApClientEncrypType(void);
int SetApClientDefaultKeyID (int id);
int GetApClientDefaultKeyID (void);
int SetApClientKey(int iKeyID, const char* szKey);
const char* GetApClientKey(int iKeyID);
int SetApClientPSKKey(const char* szKey);
const char* GetApClientPSKKey(void);
int SetApClientWscSSID(const char* szWscSSID);
int SetApClientWPSPin(WPS_PIN pin);
WPS_PIN GetApClientWPSPin(void);

/*
typedef  struct _SITE_SURVEY 
{ 
  unsigned char    channel; 
  unsigned short    rssi; 
  unsigned char    ssid[33]; 
  unsigned char    bssid[6]; 
  unsigned char    security[9];
  struct StationNode* next;
}  StationNode; 
*/

typedef struct _SITE_SURVEY {       
       char channel[4];
       char ssid[64];
       char bssid[32];
       char security[32];
       char signal[8];
       char mode[8];
       char extch[8];
       char nt[4];
       char wps[4];
       struct _SITE_SURVEY *next;
}StationNode; 

StationNode* RequestSiteSurvey();
int GetApClientErrorStatus(void); /*not implement*/

// 10	Interfaces with Save Belkin Parameters
int SetBelkinParameter(char* ParameterName, char* ParameterValue);
char* GetBelkinParameter(char* ParameterName);
int UnSetBelkinParameter(char* ParameterName);

// 11 Interfaces with Save Setting and Reboot
int SaveSettingReboot(void);
//int ResetToFactoryDefault(void);
int ResetToFactoryDefault(int runScript);
int SaveSettingRestartWireless(void);
int SaveSetting(void);

// 12	Interfaces with WiFi LED Status Control
int SetWiFiLED(int state);


// 13	Interfaces with Motion Sensor Status Control
int EnableMotionSensorDetect(int isEnable);
int SetMotionSensorDelay(int Delay_Seconds, int Sensitivity_Percent);

// 14   Interfaces with Motion Enable/Disable Watch Dog
int EnableWatchDog(int isEnable, int seconds);
void WDGTestCpu(void);
int SyncWatchDogTimer(void);
int GetRebootStatus(int *Status, unsigned long int *UTC_seconds);

#endif
