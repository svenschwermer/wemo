/***************************************************************************
*
*
* ixp4xx_gpio.h
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
#ifndef __IXP4XX_GPIO_H
#define __IXP4XX_GPIO_H

#define SYSTEM_READY 1
#define SYSTEM_BOOT 0

#define WL_ENABLE 1
#define WL_DISABLE 0

#define WL_SEC_ENABLE 1
#define WL_SEC_DISABLE 0

#define WAN_LINK_DOWN 0
#define WAN_LINK_UP 1
#define WAN_LINK_UNKNOWN 2

#define WAN_IP_STATUS_IP_FAIL 0
#define WAN_IP_STATUS_IP_OK 1
#define WAN_IP_STATUS_INIT 2

#define WAN_PING_FAIL 0
#define WAN_PING_OK 1
#define WAN_PING_INIT 2

#define LAN_LINK_DOWN 0
#define LAN_LINK_UP 1
#define LAN_LINK_UNKNOWN 2

#define GPIO_STAT_OUT_LOW '0'
#define GPIO_STAT_OUT_HIGH '1'
#define GPIO_STAT_IN_LOW '2'
#define GPIO_STAT_IN_HIGH '3'

/*
//////////////////////////////////////////////////////////////////
//	Marked Original GPIO 			//
//									//
//////////////////////////////////////////////////////////////////
#define BELKIN_GPIO_CLOSE_SPEED_METER  system("echo 0 > /proc/SPEED;");

#define BELKIN_GPIO_SYSTEM_READY system("echo 0 > /proc/GPIO9;");//Need Modified
#define BELKIN_GPIO_SYSTEM_BOOT  system("echo 1 > /proc/GPIO9;");//Need Modified

#define BELKIN_SPEED_METER_BOOT_UP_READY system("echo 0 > /proc/SPEED_BOOT_UP;");
#define BELKIN_SPEED_METER_BOOT_UP       system("echo 6 > /proc/SPEED_BOOT_UP;");

#define BELKIN_GPIO_WL_SEC_DISABLED   system("echo 1 > /proc/GPIO13;echo 1 > /proc/GPIO12;");
#define BELKIN_GPIO_WL_SEC_ENABLED    system("echo 0 > /proc/GPIO13;echo 1 > /proc/GPIO12;");


#define NO_WIRE_WIRELESS_COMPUTER 			system("echo 1 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ");
#define BELKIN_GPIO_WL_CLI_NOT_ASSOC 		NO_WIRE_WIRELESS_COMPUTER
#define BELKIN_GPIO_WL_CLI_ASSOC     			system("echo 0 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ;");
#define BELKIN_GPIO_WL_CLI_ASSOC_ERROR		system("echo 0 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 1 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ;");

//#define BELKIN_GPIO_LAN_LINK_DOWN system("echo 1 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ");
#define BELKIN_GPIO_LAN_LINK_DOWN 				NO_WIRE_WIRELESS_COMPUTER
#define BELKIN_GPIO_LAN_LINK_UP 					system("echo 1 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 1 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ;echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ;echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ;");
#define BELKIN_GPIO_LAN_LINK_ERROR 			system("echo 0 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ;");

#define BELKIN_GPIO_LAN_WL_LINK_UP 			system("echo 0 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 1 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4");
#define BELKIN_GPIO_LAN_UP_WL_ERROR			system("echo 0 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 1 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ;");
#define BELKIN_GPIO_LAN_ERROR_WL_UP			system("echo 0 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 1 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ;");
#define BELKIN_GPIO_LAN_ERROR_WL_ERROR		system("echo 0 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 1 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ; echo 1 > /proc/GPIO14 ; echo 0 > /proc/GPIO4 ; echo 1 > /proc/GPIO4 ;");


#define BELKIN_GPIO_WAN_MODEM_DOWN system("echo 1 > /proc/GPIO8;echo 1 > /proc/GPIO11;");
#define BELKIN_GPIO_WAN_MODEM_UP system("echo 1 > /proc/GPIO8;echo 0 > /proc/GPIO11;");
#define BELKIN_GPIO_WAN_MODEM_ERROR system("echo 0 > /proc/GPIO8;echo 1 > /proc/GPIO11;");

#define BELKIN_GPIO_WAN_INTERNET_DOWN system("echo 1 > /proc/GPIO5;echo 1 > /proc/GPIO6;");//echo 1 > /proc/GPIO8;
#define BELKIN_GPIO_WAN_INTERNET_CONNECTING system("echo 0 > /proc/GPIO5;echo 1 > /proc/GPIO6;");//echo 1 > /proc/GPIO8;
#define BELKIN_GPIO_WAN_INTERNET_CONNECTED system("echo 0 > /proc/GPIO5;echo 1 > /proc/GPIO6;"); //echo 0 > /proc/GPIO8;
#define BELKIN_GPIO_WAN_INTERNET_ERROR system("echo 1 > /proc/GPIO5;echo 0 > /proc/GPIO6;");//echo 1 > /proc/GPIO8;



#define BELKIN_GPIO_USB_DOWN system("echo 0 > /proc/USB_BLINK;echo 1 > /proc/GPIO23;echo 1 > /proc/GPIO22;");
#define BELKIN_GPIO_USB_READY system("echo 0 > /proc/USB_BLINK;echo 0 > /proc/GPIO23;echo 1 > /proc/GPIO22;");
#define BELKIN_GPIO_USB_ERROR system("echo 1 > /proc/GPIO23;echo 1 > /proc/USB_BLINK;");

*/

#define GMTK_CLOSE_SPEED_METER 0
#define GMTK_SYSTEM_READY 1
#define GMTK_SYSTEM_BOOT 2
#define GMTK_SPEED_METER_BOOT_UP_READY 3
#define GMTK_SPEED_METER_BOOT_UP 4
#define GMTK_WL_SEC_DISABLED 5
#define GMTK_WL_SEC_ENABLED 6
#define GMTK_LAN_LINK_DOWN 7
#define GMTK_LAN_LINK_UP 8
#define GMTK_LAN_LINK_ERROR 9
#define GMTK_WAN_MODEM_DOWN 10
#define GMTK_WAN_MODEM_UP 11
#define GMTK_WAN_MODEM_ERROR 12
#define GMTK_WAN_INTERNET_DOWN 13
#define GMTK_WAN_INTERNET_CONNECTING 14
#define GMTK_WAN_INTERNET_CONNECTED 15
#define GMTK_WAN_INTERNET_ERROR 16
#define GMTK_WL_CLI_NOT_ASSOC 17
#define GMTK_WL_CLI_ASSOC 18
#define GMTK_WL_CLI_ASSOC_ERROR 19
#define GMTK_USB_DOWN 20
#define GMTK_USB_READY 21
#define GMTK_USB_ERROR 22

//+++Eric add 
#define GMTK_NO_WIRE_WIRELESS_COMPUTER 23
#define GMTK_LAN_WL_LINK_UP 24
#define GMTK_LAN_UP_WL_ERROR 25
#define GMTK_LAN_ERROR_WL_UP 26
#define GMTK_LAN_ERROR_WL_ERROR 27
//---Eric add 


//#define BELKIN_GPIO_CLOSE_SPEED_METER			system("echo 0 > /proc/SPEED;");//gemtek_ledctrl(GMTK_CLOSE_SPEED_METER);
#define BELKIN_GPIO_SYSTEM_READY      				gemtek_ledctrl(GMTK_SYSTEM_READY);
#define BELKIN_GPIO_SYSTEM_BOOT					gemtek_ledctrl(GMTK_SYSTEM_BOOT);
//#define BELKIN_SPEED_METER_BOOT_UP_READY		system("echo 0 > /proc/SPEED_BOOT_UP;");//gemtek_ledctrl(GMTK_SPEED_METER_BOOT_UP_READY);
//#define BELKIN_SPEED_METER_BOOT_UP				system("echo 6 > /proc/SPEED_BOOT_UP;");//gemtek_ledctrl(GMTK_SPEED_METER_BOOT_UP);

#define BELKIN_GPIO_WL_SEC_DISABLED				gemtek_ledctrl(GMTK_WL_SEC_DISABLED);
#define BELKIN_GPIO_WL_SEC_ENABLED				gemtek_ledctrl(GMTK_WL_SEC_ENABLED);

//=========================Wired and Wireless Compuer LED Start=======================
#define NO_WIRE_WIRELESS_COMPUTER				gemtek_ledctrl(GMTK_NO_WIRE_WIRELESS_COMPUTER);
#define BELKIN_GPIO_WL_CLI_NOT_ASSOC				NO_WIRE_WIRELESS_COMPUTER
#define BELKIN_GPIO_WL_CLI_ASSOC					gemtek_ledctrl(GMTK_WL_CLI_ASSOC);
#define BELKIN_GPIO_WL_CLI_ASSOC_ERROR			gemtek_ledctrl(GMTK_WL_CLI_ASSOC_ERROR);

#define BELKIN_GPIO_LAN_LINK_DOWN					NO_WIRE_WIRELESS_COMPUTER
#define BELKIN_GPIO_LAN_LINK_UP						gemtek_ledctrl(GMTK_LAN_LINK_UP);
#define BELKIN_GPIO_LAN_LINK_ERROR					gemtek_ledctrl(GMTK_LAN_LINK_ERROR);


#define BELKIN_GPIO_LAN_WL_LINK_UP 				gemtek_ledctrl(GMTK_LAN_WL_LINK_UP);
#define BELKIN_GPIO_LAN_UP_WL_ERROR				gemtek_ledctrl(GMTK_LAN_UP_WL_ERROR);
#define BELKIN_GPIO_LAN_ERROR_WL_UP				gemtek_ledctrl(GMTK_LAN_ERROR_WL_UP);
#define BELKIN_GPIO_LAN_ERROR_WL_ERROR			gemtek_ledctrl(GMTK_LAN_ERROR_WL_ERROR	);
//=========================Wired and Wireless Compuer LED End=======================

#define BELKIN_GPIO_WAN_MODEM_DOWN				gemtek_ledctrl(GMTK_WAN_MODEM_DOWN);
#define BELKIN_GPIO_WAN_MODEM_UP					gemtek_ledctrl(GMTK_WAN_MODEM_UP);
#define BELKIN_GPIO_WAN_MODEM_ERROR			gemtek_ledctrl(GMTK_WAN_MODEM_ERROR);

#define BELKIN_GPIO_WAN_INTERNET_DOWN			gemtek_ledctrl(GMTK_WAN_INTERNET_DOWN);
#define BELKIN_GPIO_WAN_INTERNET_CONNECTING	gemtek_ledctrl(GMTK_WAN_INTERNET_CONNECTING);
#define BELKIN_GPIO_WAN_INTERNET_CONNECTED	gemtek_ledctrl(GMTK_WAN_INTERNET_CONNECTED);
#define BELKIN_GPIO_WAN_INTERNET_ERROR			gemtek_ledctrl(GMTK_WAN_INTERNET_ERROR);


/*
#define BELKIN_GPIO_WAN_MODEM_DOWN				system("echo 4 > /proc/MODEM_GPIO;");
#define BELKIN_GPIO_WAN_MODEM_UP					system("echo 0 > /proc/MODEM_GPIO;");
#define BELKIN_GPIO_WAN_MODEM_ERROR			system("echo 3 > /proc/MODEM_GPIO;");

#define BELKIN_GPIO_WAN_INTERNET_DOWN			system("echo 4 > /proc/NET_GPIO;");
#define BELKIN_GPIO_WAN_INTERNET_CONNECTING	system("echo 1 > /proc/NET_GPIO;");
#define BELKIN_GPIO_WAN_INTERNET_CONNECTED	system("echo 0 > /proc/NET_GPIO;"); 
#define BELKIN_GPIO_WAN_INTERNET_ERROR			system("echo 3 > /proc/NET_GPIO;");

*/


#define BELKIN_GPIO_USB_DOWN						gemtek_ledctrl(GMTK_USB_DOWN);//system("echo 0 > /proc/USB_BLINK;echo 1 > /proc/GPIO23;echo 1 > /proc/GPIO22;");//
#define BELKIN_GPIO_USB_READY						gemtek_ledctrl(GMTK_USB_READY);//system("echo 0 > /proc/USB_BLINK;echo 0 > /proc/GPIO23;echo 1 > /proc/GPIO22;");//
#define BELKIN_GPIO_USB_ERROR						gemtek_ledctrl(GMTK_USB_ERROR);//system("echo 1 > /proc/GPIO23;echo 1 > /proc/USB_BLINK;");//



void system_ready_update(int state);
void wl_status_update(int state);
void wl_security_update(int state);
void wan_link_update(int state);
void wan_ip_status_update(int state);
void wan_ping_update(int state);

void check_system_ready_GPIO(void);
void check_wl_stat_GPIO(void);
void check_LAN_stat_GPIO(void);
void check_WAN_stat_GPIO(void);

int IXP4XX_GPIO_set(int line, char value);

#define CHAR            signed char
#define INT             signed int
#define SHORT           signed short
#define UINT            unsigned int 
#undef  ULONG           
#define ULONG           unsigned int
#define USHORT          unsigned short
#define UCHAR           unsigned char

#define     IEEE80211_ADDR_LEN      6
#define    IEEE80211_RATE_MAXSIZE  30
#define     IEEE80211_RATE_VAL              0x7f
#define IEEE80211_NODE_AUTH     0x0001
#define IEEE80211_IOCTL_STA_INFO    (SIOCDEVPRIVATE+6)

#define RTPRIV_IOCTL_GET_MAC_TABLE					(SIOCIWFIRSTPRIV + 0x0F)
#define RTPRIV_IOCTL_GET_WL_ERROR					(SIOCIWFIRSTPRIV + 0x13)
//+++Eric add for WIN LOGO
#define RTPRIV_IOCTL_SET_WIN_LOGO					(SIOCIWFIRSTPRIV + 0X17)
//---Eric add for WIN LOGO
#define MAC_ADDR_LEN                    6
#define MAX_LEN_OF_MAC_TABLE            32 //256
// MIMO Tx parameter, ShortGI, MCS, STBC, etc.  these are fields in TXWI. Don't change this definition!!!

//+++Eric add 
//	int WLANLED_A =1 ;
//	int WLANLED_B =1 ;
//	int LANLED_A =1 ;
//	int LANLED_B = 1;
//---Eric add


typedef union  _MACHTTRANSMIT_SETTING {
	struct	{
	unsigned short   	MCS:7;                 // MCS
	unsigned short		BW:1;	//channel bandwidth 20MHz or 40 MHz
	unsigned short		ShortGI:1;
	unsigned short		STBC:2;	//SPACE 
	unsigned short		rsv:3;	 
	unsigned short		MODE:2;	// Use definition MODE_xxx.  
	}	field;
	unsigned short		word;
 } MACHTTRANSMIT_SETTING, *PMACHTTRANSMIT_SETTING;

typedef struct _RT_802_11_MAC_ENTRY {
    unsigned char       Addr[MAC_ADDR_LEN];
    unsigned char       Aid;
    unsigned char       Psm;     // 0:PWR_ACTIVE, 1:PWR_SAVE
    unsigned char               MimoPs;  // 0:MMPS_STATIC, 1:MMPS_DYNAMIC, 3:MMPS_Enabled
    MACHTTRANSMIT_SETTING	TxRate;
} RT_802_11_MAC_ENTRY, *PRT_802_11_MAC_ENTRY;

typedef struct _RT_802_11_MAC_TABLE {
    unsigned long       Num;
    RT_802_11_MAC_ENTRY Entry[MAX_LEN_OF_MAC_TABLE];
} RT_802_11_MAC_TABLE, *PRT_802_11_MAC_TABLE;

struct ieee80211req_sta_info {
    u_int16_t   isi_len;        /* length (mult of 4) */
    u_int16_t   isi_freq;       /* MHz */
    u_int32_t   isi_flags;      /* channel flags */
    u_int16_t   isi_state;      /* state flags */
    u_int8_t    isi_authmode;       /* authentication algorithm */
    u_int8_t    isi_rssi;
    u_int8_t    isi_capinfo;        /* capabilities */
    u_int8_t    isi_athflags;       /* Atheros capabilities */
    u_int8_t    isi_erp;        /* ERP element */
    u_int8_t    isi_macaddr[IEEE80211_ADDR_LEN];
    u_int8_t    isi_nrates;
                        /* negotiated rates */
    u_int8_t    isi_rates[IEEE80211_RATE_MAXSIZE];
    u_int8_t    isi_txrate;     /* index to isi_rates[] */
    u_int16_t   isi_ie_len;     /* IE length */
    u_int16_t   isi_associd;        /* assoc response */
    u_int16_t   isi_txpower;        /* current tx power */
    u_int16_t   isi_vlan;       /* vlan tag */
    u_int16_t   isi_txseqs[17];     /* seq to be transmitted */
    u_int16_t   isi_rxseqs[17];     /* seq previous for qos frames*/
    u_int16_t   isi_inact;      /* inactivity timer */
    u_int16_t   isi_htcap;      /* HT capabilities */
    /* XXX frag state? */
    /* variable length IE data */
};

#define RT_PRIV_IOCTL								(SIOCIWFIRSTPRIV + 0x01)
#define RT_OID_SYNC_RT61                            0x0D010750
#define RT_OID_WSC_QUERY_STATUS						((RT_OID_SYNC_RT61 + 0x01) & 0xffff)
#define RT_OID_WSC_PIN_CODE							((RT_OID_SYNC_RT61 + 0x02) & 0xffff)
#define RTPRIV_IOCTL_WSC_PROFILE                    (SIOCIWFIRSTPRIV + 0x12)

#define CHAR            signed char
#define INT             signed int
#define SHORT           signed short
#define UINT            unsigned int 
#undef  ULONG           
#define ULONG           unsigned int
#define USHORT          unsigned short
#define UCHAR           unsigned char
#endif


#define WAN_FAILED	0	/* WAN does not established */
#define WAN_ALIVE	1	/* WAN packet is increasing */
#define WAN_IDLED	2	/* WAN packet do not change */
#define WAN_RESET	4	/* WAN down and trying to up */
#define PING_FAILED	5
#define PING_SUCCESS	6

//#define PING_ADDRESS_1 "heartbeat.belkin.com"
//#define PING_ADDRESS_2 "www.belkin.com"

#define PING_ADDRESS_3 "heartbeat.belkin.com"
#define PING_ADDRESS_1 "www.belkin.com"
#define PING_ADDRESS_2 "a.root-servers.net"

/* 2010_0609_v1.10 BELKIN Internet Connectivity Check for NetNext */
//int NetNext_2010_WAN_STATUS = 0;
#define NetNext_WAN_ALIVE	"1"
#define NetNext_WAN_NEED_RESTART	"2"
#define NetNext_WAN_UNKNOWN	"3"
#define NetNext_WAN_DETECTING	"4"
