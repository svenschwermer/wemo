/***************************************************************************
*
*
* gemtek_function.h
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
int start_ntpc(void);
int stop_ntpc(void);
int Set_NtpTimezone(int index);
int Ntp_set_sec(unsigned int Seconds);
int check_secnod(unsigned int Seconds);
int check_ipaddr(char * ip);
int check_ssid(const char * ssid);
int check_APSSID_exist(void);
int check_lan_ipaddr(char * ip);
int check_subnetmask(char * ip);
int check_pskkey(const char * szKey);
int check_wpspin(const char * pin);
int check_apclient_bssid(const char * szBSSID);
int check_apclientkey(const char * szKey);
int check_parametername(char* ParameterName);
int check_parametervalue(char* ParameterValue);
int check_mac_addr(const char * mac_addr);
int change_mac_addr(const char * mac_addr);
int Site_Survey_Request();
