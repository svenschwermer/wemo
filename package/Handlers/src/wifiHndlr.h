/***************************************************************************
*
*
* wifiHndlr.h
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
#ifndef _WIFI_HNDLR_H_
#define _WIFI_HNDLR_H_

#include "wifiSetup.h"

#define SIGNAL_ROLL_COUNT 5
#define	SIGNATURE_LEN	SIZE_256B
#define MAX_SCAN_FAIL_CNT 5
#ifdef WeMo_INSTACONNECT
#define APCLI_UNCONFIGURED	0x00
#define APCLI_CONFIGURING	0x01
#define APCLI_CONFIGURED	0x02
#define INSTA_AP_SSID	"WeMo.Insta"
#define INSTA_AP_AUTH	"WPAPSK"
#define INSTA_AP_ENC	"AES"
#endif
extern int gWiFiConfigured;

void ledStatusCtrl();
void *ledStatusInitThread(void *args);
void chkConnStatus (void) ;
void* checkInetConnectivity(void *args);
void StopInetTask(void);
int initWiFiHandler();
int getCurrentAPList(PMY_SITE_SURVEY,int *);
void EnableSiteSurvey();

int checkInetStatus(int packets);

int connectHomeNetwork(int chan,char *pAuth,char *pEnc,char *pSSID,char *pPass);
int connectHiddenNetwork(int *channel, char *pSSID,char *pAuth,char *pEnc,char *pPass, int boot);
int bootConnectHomeNetwork(int chan,char *pSSID,char *pAuth,char *pEnc,char *pPass) ;
int setAPIfState(char *mode);
int setClientIfState (char *mode);
int getWiFiStatsCounters (PWIFI_STAT_COUNTERS);
int setAPChnl(char *channel);
char *wifiGetIP (char *pInterface);
int resetNetworkParams();
int IsNtpUpdate(void);
int chksignalstrength (void);

void StopWiFiPairingTask();

int SetCurrentClientState(int curState);

void initClientStatus();

void getRouterEssidMac (const char *p_essid, const char *p_macaddr, const char *pInterface);
void encryptSignature(char *input, char *output);
int isSetupRequested(void);
int setSetupRequested(int state);
int threadConnectHiddenNetwork(PWIFI_PAIR_PARAMS pWiFi);
int threadConnectHomeNetwork(int chan,char *pSSID,char *pAuth,char *pEnc,char *pPass);
int decryptSignature(char *input, char *output);
void createPasswordKeyData(void);
void SetAppSSIDCommand(void);
int decryptWithMacKeydata(char *input, char *output);
void encryptWithMacKeydata(char *input, char *finalstr);
int decryptPassword(char *input, char *output);
char *base64Decode (char *sEnc, unsigned int *decLen);
void checkScanFailCount (int count, int *scanFailCnt, int maxFailCnt);
#ifdef WeMo_INSTACONNECT
int connectToInstaAP(int chan);
void configureBridgeAp(const char *pSsid, const char *pAuth, const char *pEnc, const char *pPass, int channel);
void undoInstaApSettings(void);
void encryptWithRouterInfoKeydata(char *input, char *finalstr);
int decryptWithRouterInfoKeydata(char *input, char *output);
#endif

#ifdef WeMo_SMART_SETUP
int pairToRouter(PWIFI_PAIR_PARAMS pWiFi);
#endif

int checkDataBytesFromRouter(void);
int checkPingStatus(int packets, char *pszSite);
int createNTPUpdateThread();

void ExitCheckInetConnectThread(void);
void ExitInetMonitorThread(void);

#endif //_WIFI_HNDLR_H_
