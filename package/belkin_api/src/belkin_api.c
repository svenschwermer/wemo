/***************************************************************************
*
*
*
*
* Created by Belkin International, Software Engineering on XX/XX/XX.
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
/*
 ============================================================================
 Name        : belkin_api.c
 Author      : Abhishek.Agarwal
 Version     : 30 April' 2012
 Copyright   :
 Description : 
 ============================================================================
 */

#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/types.h>
#include <linux/watchdog.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <ctype.h>
#include <assert.h>
#include <syslog.h>
#include "fcntl.h"
#include "belkin_api.h"
#include <sys/stat.h>
#include "ra_ioctl.h"
#include <syslog.h>
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */
#include <sys/resource.h>

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE	(!FALSE)
#endif

#ifdef PRODUCT_WeMo_NetCam
#if defined(NETCAM_HD_V2) || defined(NETCAM_LS)
#include "nvram_hd.h"
#else
#include "nvram.h"
#endif
#endif

#define S_SIZE 20

// #define NTP_DEBUG
#ifdef NTP_DEBUG
#define NTP_LOG(...) syslog(LOG_DEBUG,__VA_ARGS__)
#else
#define NTP_LOG(...)
#endif

#define FILENAME_MAC "/tmp/macAddress.txt"
#define MACADDR_LEN     18
#define HWADDR "HWaddr"
#define inaddrr(x) (*(struct in_addr *) &ifr->x[sizeof sa.sin_port])
#ifdef _OPENWRT_
#define IFACE_MAC "ra0"
#else
#define IFACE_MAC "apcli0"
#endif
#define TZINDEX "timezone_index"
#define RESTORE_STATE "restore_state"
#define IPADDR_LEN     18
#define SYNCTIME_LASTTIMEZONE		"LastTimeZone"
#define SYNCTIME_DSTSUPPORT		"DstSupportFlag"

#define MAX_BUFFER_LEN     256
#define MAX_VAL_LEN     128
#define NUM_SEC_IN_HR      3600

static char gIPaddress[IPADDR_LEN];
#define FILE_NTP "/tmp/NtpTimeInfo"
#define NTP_STARTUP_CMD	"/usr/sbin/ntpclient -s -l -D -p 123 -h "
char gMacAddress[MACADDR_LEN];
char gWanGWIpAddress[IPADDR_LEN];
int g_lastTimeSync = 0;
//- It is a absolute time zone, should not be relative one from mobile app
float g_lastTimeZone = 0.0;

static int gWiredStatus = 0;  // -1 => Wired link up

// 1, 2, 3 America, 
static char* g_sGlobalNTPszServer[] = 
{   
    "192.43.244.18",		//- America - 0 (time.nist.gov)
    "192.5.41.41",      //- America- 1
    "207.200.81.113",   //- America- 2
    "129.132.2.21",     //- EU - 3
    "128.250.36.3",     //- ANZ - 4
    "129.132.2.21",     //- APC - 5
    "132.163.4.102",    // America secondary - 6	(time-b.timefreq.bldrdoc.gov)
    "192.5.41.209",     // America Secondary - 7
    "208.184.49.9",     // America secondary -
    "130.149.17.8",     // EU secondary 0x09
    "130.123.2.99",     // ANZ secondary 0x0A
    "130.149.17.8"      // APC secondary 0x0B
};

/* 
   Daylight savings time rule lookup table.
 
   ******** Unfortunately this is currently wrong by design ********
 
   WeMo sets its timezone from information sent to it by the user's smart
   phone via Upnp.  The Upnp call provides the timezone's current offset
   from UTC, a flag indicating if there the timezone has daylight savings
   time, and a flag indicating if daylight savings time is currently in effect.
 
   The timezone offset UTC essentially gives us the timezone's longitude,
   but no information is given to allow us to determine the latitude.
   To say the least latitude makes a HUGE difference to daylight savings
   time calculations.  For example daylight savings time is active in the
	summer which is around June in the northern hemisphere and around December
	in the southern hemisphere.
 
   This table does the best it can by picking a daylight saving time
   rule that is correct for maximum number of potential customers.
 
   The data in this table was mined from the IANA time zone database
   (AKA tzdata, tz database, or Olson database) that shipped with
   Ubuntu Linux 14.04.
 
   This table contains a single entry per timezone offset, the
   entries that are commented out are either duplicates of the active entry
   or are (hopefully) rules for areas that are likely to have fewer
   WeMo users that the selected entry.
 
   Ticket WEMO-36023 has been created to track this issue.
 
   Excerpt from 'man tzset' reproduced here for convenience.
 
   The std string specifies the name of the timezone and must be three or more
   alphabetic characters. The offset string immediately follows std and
   specifies the time value to be added to the local time to get
   Coordinated Universal Time (UTC). The offset is positive if the local
   timezone is west of the Prime Meridian and negative if it is east. The hour
   must be between 0 and 24, and the minutes and seconds 0 and 59.
 
   The second format is used when there is daylight saving time:
   	std offset dst [offset],start[/time],end[/time]
 
   There are no spaces in the specification. The initial std and offset
   specify the standard timezone, as described above. The dst string and
   offset specify the name and offset for the corresponding daylight
   saving timezone. If the offset is omitted, it default to one hour ahead of
	standard time.
 
   The start field specifies when daylight saving time goes into effect and
   the end field specifies when the change is made back to standard time.
 
   Mm.w.d This specifies day d (0 <= d <= 6) of week w
   (1 <= w <= 5) of month m (1 <= m <= 12).
   Week 1 is the first week in which day d occurs and week 5 is the last week
   in which day d occurs. Day 0 is a Sunday.
 
   The time fields specify when, in the local time currently in effect,
   the change to the other time occurs. If omitted, the default is 02:00:00.
 
   Here is an example for New Zealand, where the standard time (NZST) is 12
   hours ahead of UTC, and daylight saving time (NZDT), 13 hours ahead of UTC,
   runs from the first Sunday in October to the third Sunday in March, and the
   changeovers happen at the default time of 02:00:00:
 
	TZ="NZST-12:00:00NZDT-13:00:00,M10.1.0,M3.3.0"
*/
#define MINS(x,y)	((x*60) + y)
struct {
	int GmtOffset;
	const char *TzString;
} DstLookup[] = {
	{MINS(-13,0),"M9.5.0/3,M4.1.0/4"},	//  Apia: WSST-13WSDT
	{MINS(-12,45),"M9.5.0/2:45,M4.1.0/3:45"},	//  CHAT: CHAST-12:45CHADT
	{MINS(-12,0),"M9.5.0,M4.1.0/3"},		//  NZ: NZST-12NZDT
// {MINS(-12,0),"M11.1.0,M1.3.4/75"},	//  Fiji: FJT-12FJST
	{MINS(-10,30),"M10.1.0,M4.1.0"},		//  Lord_Howe: LHST-10:30LHDT-11
	{MINS(-10,0),"M10.1.0,M4.1.0/3"},	//  Currie: AEST-10AEDT
#if 0
	{MINS(-10,0),"M10.1.0,M4.1.0/3"},	//  Hobart: AEST-10AEDT
	{MINS(-10,0),"M10.1.0,M4.1.0/3"},	//  NSW: AEST-10AEDT
	{MINS(-10,0),"M10.1.0,M4.1.0/3"},	//  Victoria: AEST-10AEDT
#endif

	{MINS(-9,30),"M10.1.0,M4.1.0/3"},	//  Adelaide: ACST-9:30ACDT
//	{MINS(-9,30),"M10.1.0,M4.1.0/3"},	//  Yancowinna: ACST-9:30ACDT
	{MINS(-4,0),"M3.5.0/4,M10.5.0/5"},	//  Baku: AZT-4AZST

//	{MINS(-2,0),"M3.4.4/26,M10.5.0"},	//  Israel: IST-2IDT
	{MINS(-2,0),"M3.5.0/0,M10.5.0/0 "},	//  Beirut: EET-2EEST
#if 0
	{MINS(-2,0),"M3.5.0/3,M10.5.0/4"},	//  Athens: EET-2EEST
	{MINS(-2,0),"M3.5.0/3,M10.5.0/4"},	//  Bucharest: EET-2EEST
	{MINS(-2,0),"M3.5.0/3,M10.5.0/4"},	//  Chisinau: EET-2EEST
	{MINS(-2,0),"M3.5.0/3,M10.5.0/4"},	//  EET: EET-2EEST
	{MINS(-2,0),"M3.5.0/3,M10.5.0/4"},	//  Kiev: EET-2EEST
	{MINS(-2,0),"M3.5.0/3,M10.5.0/4"},	//  Mariehamn: EET-2EEST
	{MINS(-2,0),"M3.5.0/3,M10.5.0/4"},	//  Nicosia: EET-2EEST
	{MINS(-2,0),"M3.5.0/3,M10.5.0/4"},	//  Riga: EET-2EEST
	{MINS(-2,0),"M3.5.0/3,M10.5.0/4"},	//  Sofia: EET-2EEST
	{MINS(-2,0),"M3.5.0/3,M10.5.0/4"},	//  Tallinn: EET-2EEST
	{MINS(-2,0),"M3.5.0/3,M10.5.0/4"},	//  Turkey: EET-2EEST
	{MINS(-2,0),"M3.5.0/3,M10.5.0/4"},	//  Uzhgorod: EET-2EEST
	{MINS(-2,0),"M3.5.0/3,M10.5.0/4"},	//  Vilnius: EET-2EEST
	{MINS(-2,0),"M3.5.0/3,M10.5.0/4"},	//  Zaporozhye: EET-2EEST

	{MINS(-2,0),"M3.5.4/24,M10.5.5/1"},	//  Amman: EET-2EEST
	{MINS(-2,0),"M3.5.4/24,M9.3.6/144"},//  Gaza: EET-2EEST
	{MINS(-2,0),"M3.5.4/24,M9.3.6/144"},//  Hebron: EET-2EEST
	{MINS(-2,0),"M3.5.5/0,M10.5.5/0"},	//  Damascus: EET-2EEST
	{MINS(-2,0),"M4.5.5/0,M9.5.4/24"},	//  Egypt: EET-2EEST
#endif

	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Amsterdam: CET-1CEST
#if 0
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Andorra: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Belgrade: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Berlin: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Bratislava: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Brussels: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Budapest: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  CET: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Ceuta: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Copenhagen: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Gibraltar: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Jan_Mayen: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Luxembourg: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Madrid: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Malta: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  MET: MET-1MEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Monaco: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Paris: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Poland: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  San_Marino: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Stockholm: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Tirane: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Vienna: CET-1CEST
	{MINS(-1,0),"M3.5.0,M10.5.0/3"},		//  Zurich: CET-1CEST
	{MINS(-1,0),"M9.1.0,M4.1.0"},			//  Windhoek: WAT-1WAST
#endif

	{MINS(0,0),"M3.5.0/1,M10.5.0"},		//  Eire: GMT0BST
#if 0
	{MINS(0,0),"M3.5.0/1,M10.5.0"},		//  Eire: GMT0IST
	{MINS(0,0),"M3.5.0/1,M10.5.0"},		//  Faeroe: WET0WEST
	{MINS(0,0),"M3.5.0/1,M10.5.0"},		//  Madeira: WET0WEST
	{MINS(0,0),"M3.5.0/1,M10.5.0"},		//  Portugal: WET0WEST
	{MINS(0,0),"M3.5.0/1,M10.5.0"},		//  WET: WET0WEST
	{MINS(0,0),"M3.5.0/1,M10.5.0/3"},	//  Troll: UTC0CEST-2
	{MINS(0,0),"M3.5.0,M10.5.0/3"},		//  El_Aaiun: WET0WEST
	{MINS(0,0),"M3.5.0,M10.5.0/3"},		//  Casablanca: WET0WEST
#endif

	{MINS(1,0),"M3.5.0/0,M10.5.0/1"},	//  Azores: AZOT1AZOST
//	{MINS(1,0),"M3.5.0/0,M10.5.0/1"},	//  Scoresbysund: EGT1EGST

	{MINS(3,0),"M10.3.0/0,M2.3.0/0"},	//  Sao_Paulo: BRT3BRST
#if 0
	{MINS(3,0),"M3.5.0/-2,M10.5.0/-1"},	//  Godthab: WGT3WGST
	{MINS(3,0),"M3.2.0,M11.1.0"},			//  Miquelon: PMST3PMDT
	{MINS(3,0),"M10.1.0,M3.2.0"},			//  Montevideo: UYT3UYST
#endif

	{MINS(3,30),"M3.2.0,M11.1.0"},		//  Newfoundland: NST3:30NDT

	{MINS(4,0),"M3.2.0,M11.1.0"},			//  Atlantic: AST4ADT
#if 0
	{MINS(4,0),"M3.2.0,M11.1.0"},			//  Bermuda: AST4ADT
	{MINS(4,0),"M3.2.0,M11.1.0"},			//  Glace_Bay: AST4ADT
	{MINS(4,0),"M3.2.0,M11.1.0"},			//  Goose_Bay: AST4ADT
	{MINS(4,0),"M3.2.0,M11.1.0"},			//  Moncton: AST4ADT
	{MINS(4,0),"M3.2.0,M11.1.0"},			//  Thule: AST4ADT
	{MINS(4,0),"M9.1.6/24,M4.4.6/24"},	//  Palmer: CLT4CLST
	{MINS(4,0),"M9.1.6/24,M4.4.6/24"},	//  Santiago: CLT4CLST
	{MINS(4,0),"M10.1.0/0,M3.4.0/0"},	//  Asuncion: PYT4PYST
	{MINS(4,0),"M10.3.0/0,M2.3.0/0"},	//  Campo_Grande: AMT4AMST
	{MINS(4,0),"M10.3.0/0,M2.3.0/0"},	//  Cuiaba: AMT4AMST
#endif

	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Detroit: EST5EDT
#if 0
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Eastern: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  EST5EDT: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Fort_Wayne: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Iqaluit: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Louisville: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Marengo: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Monticello: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Montreal: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Nassau: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  New_York: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Nipigon: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Pangnirtung: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Petersburg: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  posixrules: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Prince: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Thunder_Bay: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Vevay: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Vincennes: EST5EDT
	{MINS(5,0),"M3.2.0,M11.1.0"},			//  Winamac: EST5EDT
	{MINS(5,0),"M3.2.0/0,M11.1.0/1"},	//  Cuba: CST5CDT
#endif

	{MINS(6,0),"M3.2.0,M11.1.0"},			//  Center: CST6CDT
#if 0	
	{MINS(6,0),"M3.2.0,M11.1.0"},			//  Central: CST6CDT
	{MINS(6,0),"M3.2.0,M11.1.0"},			//  Chicago: CST6CDT
	{MINS(6,0),"M3.2.0,M11.1.0"},			//  CST6CDT: CST6CDT
	{MINS(6,0),"M3.2.0,M11.1.0"},			//  Knox_IN: CST6CDT
	{MINS(6,0),"M3.2.0,M11.1.0"},			//  Matamoros: CST6CDT
	{MINS(6,0),"M3.2.0,M11.1.0"},			//  Menominee: CST6CDT
	{MINS(6,0),"M3.2.0,M11.1.0"},			//  Beulah: CST6CDT
	{MINS(6,0),"M3.2.0,M11.1.0"},			//  New_Salem: CST6CDT
	{MINS(6,0),"M3.2.0,M11.1.0"},			//  Rainy_River: CST6CDT
	{MINS(6,0),"M3.2.0,M11.1.0"},			//  Rankin_Inlet: CST6CDT
	{MINS(6,0),"M3.2.0,M11.1.0"},			//  Resolute: CST6CDT
	{MINS(6,0),"M3.2.0,M11.1.0"},			//  Tell_City: CST6CDT
	{MINS(6,0),"M4.1.0,M10.5.0"},			//  Bahia_Banderas: CST6CDT
	{MINS(6,0),"M4.1.0,M10.5.0"},			//  Cancun: CST6CDT
	{MINS(6,0),"M4.1.0,M10.5.0"},			//  Merida: CST6CDT
	{MINS(6,0),"M4.1.0,M10.5.0"},			//  Mexico_City: CST6CDT
	{MINS(6,0),"M4.1.0,M10.5.0"},			//  Monterrey: CST6CDT
	{MINS(6,0),"M9.1.6/22,M4.4.6/22"},	//  Easter: EAST6EASST
#endif
 
	{MINS(7,0),"M3.2.0,M11.1.0"},			//  Boise: MST7MDT
#if 0
	{MINS(7,0),"M3.2.0,M11.1.0"},			//  Cambridge_Bay: MST7MDT
	{MINS(7,0),"M4.1.0,M10.5.0"},			//  Chihuahua: MST7MDT
	{MINS(7,0),"M3.2.0,M11.1.0"},			//  Inuvik: MST7MDT
	{MINS(7,0),"M4.1.0,M10.5.0"},			//  Mazatlan: MST7MDT
	{MINS(7,0),"M3.2.0,M11.1.0"},			//  Mountain: MST7MDT
	{MINS(7,0),"M3.2.0,M11.1.0"},			//  MST7MDT: MST7MDT
	{MINS(7,0),"M3.2.0,M11.1.0"},			//  Navajo: MST7MDT
	{MINS(7,0),"M3.2.0,M11.1.0"},			//  Ojinaga: MST7MDT
	{MINS(7,0),"M3.2.0,M11.1.0"},			//  Yellowknife: MST7MDT
#endif
	{MINS(8,0),"M3.2.0,M11.1.0"},			//  Los_Angeles: PST8PDT
#if 0
	{MINS(8,0),"M3.2.0,M11.1.0"},			//  Dawson: PST8PDT
	{MINS(8,0),"M3.2.0,M11.1.0"},			//  Ensenada: PST8PDT
	{MINS(8,0),"M3.2.0,M11.1.0"},			//  Pacific: PST8PDT
	{MINS(8,0),"M3.2.0,M11.1.0"},			//  PST8PDT: PST8PDT
	{MINS(8,0),"M3.2.0,M11.1.0"},			//  Yukon: PST8PDT
	{MINS(8,0),"M4.1.0,M10.5.0"},			//  Santa_Isabel: PST8PDT
#endif

	{MINS(9,0),"M3.2.0,M11.1.0"},			//  Anchorage: AKST9AKDT
#if 0
	{MINS(9,0),"M3.2.0,M11.1.0"},			//  Juneau: AKST9AKDT
	{MINS(9,0),"M3.2.0,M11.1.0"},			//  Nome: AKST9AKDT
	{MINS(9,0),"M3.2.0,M11.1.0"},			//  Sitka: AKST9AKDT
	{MINS(9,0),"M3.2.0,M11.1.0"},			//  Yakutat: AKST9AKDT
	{MINS(10,0),"M3.2.0,M11.1.0,0"},		//  Atka: HAST10HADT
	{MINS(10,0),"M3.2.0,M11.1.0"},
#endif

	{0,NULL}	// end of table
};
#undef MINS

//- To NA, TODO: to move to a configuration file
#define NA_NTP_SERVER_NUMBER 6
static int g_slstNANtpServer[6] = { 0x00, 0x06, 0x01, 0x07, 0x02, 0x08 };
//- To Eu, TODO: to move to a configuration file. If can not reach EU then use ANZ
#define EU_NTP_SERVER_NUMBER 4
static int g_slstEUNtpServer[4] = {0x03, 0x09, 0x04, 0x0A };

//- To ANZ, TODO: to move to a configuration file. If can not reach ANZ the nuse EU
#define ANZ_NTP_SERVER_NUMBER 4
static int g_slstANZNtpServer[4] = { 0x04, 0x0A, 0x03, 0x09 };

//- To APC, TODO: to move to a configuration file.
#define APC_NTP_SERVER_NUMBER 2
static int g_slstAPCNtpServer[2] = {0x05, 0x0B};

static void Minutes2HrsMinutes(int TotalMinutes,int *pHours,int *pMinutes);
static void WriteTzInfo(char *TZ,int MinutesWest);
static int convertMon(char *mon);
static int convertDay(char *day);
int static TzOffset2MinutesWest(char *Offset);

static int chkMACAddress(const char * mac_addr)
{
    int i = 0;
    int s = 0;

    while( *mac_addr )
    {
        if( isxdigit(*mac_addr) )
        {
            i++;
        }
        else if( *mac_addr == ':' || *mac_addr == '-' )
        {
            if( i == 0 || i / 2 - 1 != s )
                break;
            ++s;
        }
        else
        {
            s = -1;
        }
        ++mac_addr;
    }

    return (i == 12 && (s == 5 || s == 0));
}

static int change_mac_addr(const char * mac_addr)
{
    char buf[MAX_VAL_LEN];
    int retVal = BELKIN_SUCCESS;

    memset(buf, 0x0, sizeof(buf));

    retVal = chkMACAddress(mac_addr);

    if( !retVal )
    {
        return BELKIN_FAILURE;
    }

    snprintf(buf, sizeof(buf), "ifconfig %s down", IFACE_MAC);
    retVal = system(buf);

    memset(buf, 0x0, sizeof(buf));
    snprintf(buf, sizeof(buf), "ifconfig %s hw ether %s", IFACE_MAC, mac_addr);
    retVal = system(buf);

    memset(buf, 0x0, sizeof(buf));
    snprintf(buf, sizeof(buf), "ifconfig %s up", IFACE_MAC);
    retVal = system(buf);

    strncpy(gMacAddress, mac_addr, sizeof(gMacAddress));

    return BELKIN_SUCCESS;
}

static int check_parametername(char* ParameterName)
{
    assert(ParameterName);
    return ParameterName ? 0 : 1; //0 means success
}

static int check_parametervalue(char* ParameterValue)
{
    assert(ParameterValue);
    return BELKIN_SUCCESS; //0 means success
}

//START: Interfaces with NTP

// This function does nothing, it is provided for compatibility with the old
// libgemtek_api.so.  OpenWRT based code calls SetTimeAndTZ() instead.
int SetNTP(char *ServerIP, int Index, int EnableDaylightSaving)
{
	return 0;
}


// Return zero if we set time and left ntpclient as a daemon
int RunNTP()
{
	time_t TimeUTC = 0;
	FILE *fp;
	struct tm timeinfo;
	char buf[MAX_BUFFER_LEN] = {'\0'};
	int date, year, hr, min, sec;
	char day[8] = {'\0'};
	char mon[8] = {'\0'};
	struct stat fileInfo;
	int *pListServer = NULL;
	int cntServerNumber = 0;
	int i;
	int serverIndex = 0;
	int GmtOffset;
	int Ret = BELKIN_FAILURE;	// Assume the worse

	GmtOffset = -TzOffset2MinutesWest(GetBelkinParameter(SYNCTIME_LASTTIMEZONE));

	NTP_LOG("%s: GmtOffset: %d\n",__FUNCTION__,GmtOffset);
	do {
		if(GmtOffset <= (-2 * 60)) {
		// North america
			NTP_LOG("%s: North america",__FUNCTION__);
			cntServerNumber = NA_NTP_SERVER_NUMBER;
			pListServer     = g_slstNANtpServer;
		}
		else if(GmtOffset <= (-1 * 60)) {
		// The Azores 
			NTP_LOG("%s: The Azores",__FUNCTION__);
			cntServerNumber = ANZ_NTP_SERVER_NUMBER;
			pListServer     = g_slstANZNtpServer;
#if 1
		// The original code used asia for the Azores, was this an unnoticed bug 
		// or intentional ??  Be consertive and do the same thing
			cntServerNumber = APC_NTP_SERVER_NUMBER;
			pListServer     = g_slstAPCNtpServer;
#endif
		}
		else if(GmtOffset <= (1 * 60)) {
		// England and France
			NTP_LOG("%s: England or France",__FUNCTION__);
			cntServerNumber = EU_NTP_SERVER_NUMBER;
			pListServer     = g_slstEUNtpServer;
		}
		else if(GmtOffset <= (3 * 30)) {
		// Europe (Use asia for Europe)
			NTP_LOG("%s: Europe",__FUNCTION__);
			cntServerNumber = APC_NTP_SERVER_NUMBER;
			pListServer     = g_slstAPCNtpServer;
		}
		else {
		// Asia Pacific
			NTP_LOG("%s: Asia Pacific",__FUNCTION__);
			cntServerNumber = APC_NTP_SERVER_NUMBER;
			pListServer     = g_slstAPCNtpServer;
		}

		if(cntServerNumber == 0) {
			syslog(LOG_ERR,"%s: can not find the location index\n",__FUNCTION__);        
			break;
		}

		unlink(FILE_NTP);
	// kill off any ntpclients running in daemon mode
		system("killall -KILL ntpclient");

	// Try all available servers
		for(i = 0; i < cntServerNumber; i++) {
			serverIndex = pListServer[i];
			memset(buf, 0x00, sizeof(buf));
			snprintf(buf,sizeof(buf),"ntpclient -c 1 -h %s -i 1", 
						g_sGlobalNTPszServer[serverIndex]);
			NTP_LOG("%s: Trying server %d, IP: %s",__FUNCTION__,i+1,
					  g_sGlobalNTPszServer[serverIndex]);
			if(system(buf) == 0) {
				lstat(FILE_NTP, &fileInfo);
				if((off_t)fileInfo.st_size != 0) {
					syslog(LOG_INFO,
							 "%s: time successfully retrieved from ntp server %s\n",
							 __FUNCTION__,g_sGlobalNTPszServer[serverIndex]);

					if((fp = fopen(FILE_NTP, "r")) == NULL) {
						perror("File opening error");
						return 0;
					}

					fscanf(fp,"%s %s %d %d:%d:%d %d",day,mon,&date,&hr,&min,&sec,&year);
					fclose(fp);

					unlink(FILE_NTP);

					timeinfo.tm_sec = sec;
					timeinfo.tm_min = min;
					timeinfo.tm_hour = hr;
					timeinfo.tm_mday = date;
					timeinfo.tm_year = year - 1900;
					timeinfo.tm_mon = convertMon(mon);
					timeinfo.tm_wday = convertDay(day);

					TimeUTC = mktime(&timeinfo);
					if(TimeUTC == 0) {
						syslog(LOG_ERR,"%s#%d: Internal error\n",__FUNCTION__,__LINE__);        
					}
					break;
				}
			}
		}
	} while(FALSE);


	if(TimeUTC == 0) {
		syslog(LOG_INFO,"%s: NTP unable to set time\n",__FUNCTION__);
	}
	else do {
		int MallocSize;
		char *CmdLine;

		SetTime(TimeUTC,0,0);

	// Start ntpclient running in daemon mode to keep the time synced

	// The OpenWRT startup script (ntpclient.hotplug) starts ntpclient like this:
	// $NTPC ${COUNT:+-c $COUNT} ${INTERVAL:+-i $INTERVAL} -s -l -D -p $PORT -h $SERVER 2> /dev/null
	// Do the same more or less

		MallocSize = strlen(NTP_STARTUP_CMD) + 
						 strlen(g_sGlobalNTPszServer[serverIndex]) + 1;
		if((CmdLine = (char *) malloc(MallocSize)) == NULL) {
         syslog(LOG_ERR,"%s#%d: malloc failed\n",__FUNCTION__,__LINE__);
			break;
		}
		strcpy(CmdLine,NTP_STARTUP_CMD);
		strcat(CmdLine,g_sGlobalNTPszServer[serverIndex]);

		NTP_LOG("%s: running %s\n",__FUNCTION__,CmdLine);
		if(System(CmdLine) != 0) {
			syslog(LOG_ERR,"%s: running ntpclient in daemon mode failed - %m\n",
					 __FUNCTION__);
		}
		free(CmdLine);
		Ret = BELKIN_SUCCESS;
	} while(FALSE);

	return Ret;
}

#define NUM_MONTHS	12
#define NUM_DAYS	7
int convertMon(char *mon)
{
    int i;
    char *months[NUM_MONTHS] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};

    for( i = 0; i < NUM_MONTHS; i++ )
    {
        if( (strcasecmp(mon, months[i])) == 0 )
            break;
    }
    return i;
}

static int convertDay(char *day) {

    char *days[NUM_DAYS] = {"Sun", "Mon", "Tue", "Wed", "Thur", "Fri", "Sat"};
    int i;

    for(i=0; i<NUM_DAYS; i++)
	if((strcasecmp(day, days[i])) == 0)
	    break;

    return i;
}

#define FILE_NTP "/tmp/NtpTimeInfo"
// Return system time.  This function exists for compatibility with old
// code that was written to use the GemTek API which set system time to
// local time so the standard POSIX time functions didn't work correctly.
unsigned long int GetUTCTime(void)
{
	return (unsigned long int) time(NULL);
}
//END: Interfaces with NTP

//Start: Interfaces with Firmware Update
//remaining
int Firmware_Update(char* File_Path)
{
    char update_command[MAX_BUFFER_LEN] = {0,};
    int  system_rtn = 256;
    /* 
     * If request lands here, we are definitely upgrading OpenWRT to OpenWRT firmware image 
     * Do not mind the file extension here, its a upd image and not the gpg image
     */
    if(!File_Path || !strlen(File_Path))
    {
    printf("Invalid file path..\n");
    return BELKIN_FAILURE;
    }

    memset(update_command, 0x0, sizeof(update_command));
    snprintf(update_command, sizeof(update_command), "sh -x /sbin/firmware_update.sh %s", File_Path);

    system_rtn = system(update_command);
    if (system_rtn == 0)
    return BELKIN_SUCCESS;
    else
        return BELKIN_FAILURE;
}
//END: Interfaces with Firmware Update

//Start: Interfaces with New_Firmware Update
int New_Firmware_Update(char* File_Path, int is_signed)
{
    char update_command[MAX_BUFFER_LEN] = { 0x00 };
    int  system_rtn = 256;

    if(!File_Path || !strlen(File_Path))
    {
        fprintf(stderr, "Invalid file path..\n");
        return BELKIN_FAILURE;
    }

    /*
     * If the image is not signed, it should be named "/tmp/firmware.img".
     * The update script(firmware_update.sh) skips signature checking if file is "/tmp/firmware.img".
    */
    if (is_signed)
        snprintf(update_command, sizeof(update_command), "sh -x /sbin/firmware_update.sh %s", File_Path);
    else
        snprintf(update_command, sizeof(update_command), "mv %s /tmp/firmware.img; sh -x /sbin/firmware_update.sh /tmp/firmware.img", File_Path);

    system_rtn = system(update_command);
    if (system_rtn == 0)
    return BELKIN_SUCCESS;
    else
        return BELKIN_FAILURE;
}
//END: Interfaces with New_Firmware Update

//Start: Interfaces with Update System Time

#define ADD_TZ(...) 										\
	Wrote = snprintf(&TZ[Next],Left,__VA_ARGS__);\
	Next += Wrote;											\
	Left -= Wrote;											\
	if(Left <= 1 || Next >= sizeof(TZ)) {  		\
		break;												\
	}															\


// Set the system time and C run time library's TZ variable
// NB: Index and EnableDaylightSaving are ignored, they
// are present for compatibility with the old libgemtek_api.so
int SetTime(unsigned int SecondsUTC, int Index, int bEnableDST)
{
	struct timeval now;
	time_t TimeNow;
	time_t DeltaT;
	int i;
	int Hours;
	int Minutes;
	int MinutesWest;
	char TZ[80];
	int Wrote;
	int Next = 0;
	int Left = sizeof(TZ);
	char *Offset = GetBelkinParameter(SYNCTIME_LASTTIMEZONE);
	int Ret = 0;	// assume the best

	if(strlen(Offset) == 0) {
	// Not set, assume UTC
		Offset = "0";
	}
// Set global values for IsApplyTimeSync()
   g_lastTimeSync = (int) SecondsUTC;
   g_lastTimeZone = atof(Offset);

	bEnableDST = *GetBelkinParameter(SYNCTIME_DSTSUPPORT) == '1' ? TRUE : FALSE;

	NTP_LOG("%s: SecondsUTC: %u, Offset: %s, bEnableDST: %d\n",
			 __FUNCTION__,SecondsUTC,Offset,bEnableDST);

	MinutesWest = TzOffset2MinutesWest(Offset);
	Minutes2HrsMinutes(MinutesWest,&Hours,&Minutes);

// Example TZ format: "NZST-12:00:00NZDT-13:00:00,M10.1.0,M3.3.0"

	do {
		ADD_TZ("STD%d:%02d",Hours,Minutes);

		if(bEnableDST) {
		// This timezone has daylight saving time
			for(i = 0; DstLookup[i].TzString != NULL; i++) {
				if(DstLookup[i].GmtOffset == MinutesWest) {
					ADD_TZ("DST,%s",DstLookup[i].TzString);
					break;
				}
			}
			if(DstLookup[i].TzString == NULL) {
				syslog(LOG_ERR,"%s#%d: Internal error, couldn't find offset %d",
						 __FUNCTION__,__LINE__,MinutesWest);
			}
		}

		NTP_LOG("%s: TZ=%s\n",__FUNCTION__,TZ);
		WriteTzInfo(TZ,MinutesWest);
	} while(FALSE);

	if(Left <= 1 || Next >= sizeof(TZ)) {
		syslog(LOG_ERR,"%s#%d: Internal error, Left: %d, Next: %d\n",
				 __FUNCTION__,__LINE__,Left,Next);
	}

	TimeNow = time(NULL);

	if(TimeNow > SecondsUTC) {
		DeltaT = TimeNow - SecondsUTC;
	}
	else {
		DeltaT = SecondsUTC - TimeNow;
	}

// If time being set is "close" to the system time then don't set the
// system time.  The assumption is that ntpd is running and it can set
// a better time than the App can via UPnP calls over the WiFi network.
	if(DeltaT > 60) {
		now.tv_sec = SecondsUTC;
		now.tv_usec = 0;

		if(settimeofday(&now,NULL) != 0) {
			syslog(LOG_ERR,"%s#%d: settimeofday failed - %m\n",
					 __FUNCTION__,__LINE__);
			Ret = -1;
		}
	}
	else {
		syslog(LOG_INFO,"%s: DeltaT %d, not setting system time\n",__FUNCTION__,
				 (int) DeltaT);
	}

	return Ret;
}
#undef ADD_TZ

//END: Interfaces with Update System Time

//Start: Interfaces with Device Info
char* GetMACAddress(void)
{
    char *p = NULL;
    FILE *fp =  NULL;
    char buf[MAX_VAL_LEN] = {0,};
    char line[MAX_BUFFER_LEN] = {0,};

    int retVal = 0;

    if( strlen(gMacAddress) && strcmp(gMacAddress, "00:00:00:00:00:00") )
    {
        printf("Cached MAC Address = %s \n", gMacAddress);
        return gMacAddress;
    }

    snprintf(buf, sizeof(buf), "ifconfig %s > %s", IFACE_MAC, FILENAME_MAC);

    retVal = system(buf);

    fp = fopen(FILENAME_MAC, "rb");
    if( !fp )
    {
       perror("File opening error");
       strncpy(gMacAddress, "00:00:00:00:00:00", sizeof(gMacAddress));
       return gMacAddress;
    }

    memset(line, 0x0, MAX_BUFFER_LEN);
    while( fgets(line, MAX_BUFFER_LEN, fp) )
    {
       if( (p = strstr(line, HWADDR)) )
       {
           break;
       }
       memset(line, 0x0, MAX_BUFFER_LEN);
    }

    if( !p )
    {
       //printf("NO MAC address is configured\n");
       if( fp )
       {
           fclose(fp);
       }

       strncpy(gMacAddress, "00:00:00:00:00:00", sizeof(gMacAddress));

       memset(buf, 0x0, MAX_VAL_LEN);
       snprintf(buf, sizeof(buf) , "rm %s", FILENAME_MAC);
       retVal = system(buf);

       return gMacAddress;
    }

    p += strlen(HWADDR);
    p++;
    p[MACADDR_LEN-1] = '\0';

    strncpy(gMacAddress, p, sizeof(gMacAddress));

    if( fp )
    {
       fclose(fp);
    }

    memset(buf, 0x0, MAX_VAL_LEN);
    snprintf(buf, sizeof(buf), "rm %s", FILENAME_MAC);
    retVal = system(buf);

    printf("MAC Address: %s\n", gMacAddress);

    return gMacAddress;
}

/*
 * Get the MAC address of interface.
 * 
 * intf is the interface name
 * 
 * Returns null terminated string containing MAC with ":"'s.
 * To maintain compatibility with previous version, also stores string
 * in global variable gMacAddress.  :-(
 * If there is a problem, it returns (and stores) an all zeros address
 * @return The MAC address (or "00:...")
 */
#ifndef MAX_MAC_LEN
#define MAX_MAC_LEN        20
#endif
void GetMACAddress_ext(char * intf, char *buff) {
   struct ifreq s;
   int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
   strcpy(s.ifr_name, intf);
   if (0 == ioctl(fd, SIOCGIFHWADDR, &s)) {
      const unsigned char* mac=(unsigned char*)s.ifr_hwaddr.sa_data;
      snprintf( buff, MAX_MAC_LEN,
            "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
   } else {
      strncpy( buff, "00:00:00:00:00:00",
            sizeof(buff) );
   }

   return;
}

int SetMACAddress(char *mac_addr)
{
    int retVal = BELKIN_SUCCESS;

    retVal = change_mac_addr(mac_addr);

    return retVal;
}

#ifdef PRODUCT_WeMo_NetCam
#define NVRAM_SN_KEY "SerialNumber"
char g_sSerialNumber[15];
#endif

char* GetSerialNumber(void)
{
#ifdef PRODUCT_WeMo_NetCam
   //- Generate Serial Number from MAC since it is NetCam hardware,
   //- there is no real serial number yet
   char *szTmp = GetMACAddress();
   char *previousSN = NULL;

   memset(g_sSerialNumber, 0x00, 15);
   int len = strlen(szTmp);
   int i = 0x00;
   int j = 0x00;
   for (;i < len; i++)
   {
      if (szTmp[i] != ':')
      {
         g_sSerialNumber[j] = szTmp[i];
         j++;
      }
   }

   //- construct serial number now
   char szSerialTmp[15] = {0x00};

   /* The supplier ID for Seedonk is 32.  The supplier ID for WNC is
	 * 23.  This image is for Seedonk but we are using 23.  Why?  A long
	 * time ago, someone made the mistake of entering 23.  We changed it
	 * to 32 at one point, but changing the serial numbers of existing
	 * netcams caused much grief for the cloud services.  So we have
	 * changed it back to 23.  Perhaps one day we will make it 32.
	 */
   strcpy(szSerialTmp, "23");
   strncat(szSerialTmp, g_sSerialNumber + 3, 4);
   //strncat(szSerialTmp, __DATE__ + 9, 2 );
   //strncat(szSerialTmp, "01", 2 );
   // All netcams but Linksys WNC are sub-type 1.  WNC is 3.
   // See Architecture-Designs/WeMo_Identification_Schema.doc for
   // serial # schema
#if !defined(NETCAM_LS)
   strcat(szSerialTmp, "V01");
#else
   strcat(szSerialTmp, "V03");
#endif
   strcat(szSerialTmp, g_sSerialNumber + 7);
   strncpy(g_sSerialNumber, szSerialTmp, 14);

   /* Fetch previous serial number & compare with new one.  If they
		 are different (including if there wasn't one to begin with) then
		 hand off for sharing with cloud and possible storage */
   previousSN = GetBelkinParameter( NVRAM_SN_KEY );
   if( strcmp( previousSN, g_sSerialNumber )) {
      printf( "Previous SN (%s) and new (%s) do not match.\n", 
            previousSN, g_sSerialNumber );
      // Sends to cloud and saves when done
      send_sn_to_cloud_and_save( g_sSerialNumber );      
   } else {
      printf( "Previous SN (%s) and new (%s) DO match.\n", 
            previousSN, g_sSerialNumber );
   }

   return g_sSerialNumber;
#else
    char *paramN = "SerialNumber";
    char *paramV = NULL;

    paramV = GetBelkinParameter(paramN);
    return paramV;
#endif
}

int SetSerialNumber(char *serial_number)
{
    char *paramN = "SerialNumber";

    return (SetBelkinParameter(paramN, serial_number));
}
//End: Interfaces with Device Info

//Start: Interfaces with Device LAN Info
#define LAN_IFACE "ra0"
#define WAN_IFACE "apcli0"
#define WIRED_IFACE  "eth2"

static char *getIPAddress(char *if_name)
{
    int sock=-1;
    struct ifreq *ifr;
    struct ifreq ifrr;
    struct sockaddr_in sa;
    char ifname[MAX_VAL_LEN] = {'\0'};
    int ret;

    strcpy(ifname, if_name);
    memset(gIPaddress, 0x00, sizeof(gIPaddress));

    if( 0 > (sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) )
    {
        fprintf(stderr, "Cannot open socket.\n");
        strncpy(gIPaddress, "0.0.0.0", sizeof(gIPaddress));
        return gIPaddress;
    }

    ifr = &ifrr;

    ifrr.ifr_addr.sa_family = AF_INET;

    strncpy(ifrr.ifr_name, ifname, sizeof(ifrr.ifr_name));

    if( ioctl(sock, SIOCGIFADDR, ifr) < 0 )
    {
        //printf("No %s interface.\n", ifname);
        ret = close(sock);
        if( ret < 0 )
        {
            perror("Error in closing socket");
        }

        strncpy(gIPaddress, "0.0.0.0", sizeof(gIPaddress));
        return gIPaddress;
    }

    strcpy(gIPaddress, inet_ntoa(inaddrr(ifr_addr.sa_data)));
    printf("getIPAddress : address for %s: %s\n", ifname, gIPaddress);

    ret = close(sock);

    if( ret < 0 )
    {
        perror("Error in closing socket");
    }

    return gIPaddress;
}

char* GetLanIPAddress(void)
{
    return getIPAddress(LAN_IFACE);
}
//END: Interfaces with Device LAN Info

//Start: Interfaces with Device WAN Info
#ifdef PRODUCT_WeMo_NetCam
#if defined(NETCAM_HD_V2) || defined(NETCAM_LS)
#define CMD_WAN_IP "ifconfig br0 | grep 'inet addr:' | cut -d':' -f2 | cut -d' ' -f1"
#elif defined (NETCAM_HD_V1) || defined(NETCAM_NetCam)
#define CMD_WAN_IP "ifconfig apcli0 | grep 'inet addr:' | cut -d':' -f2 | cut -d' ' -f1"
#endif
#endif
char* GetWanIPAddress(void)
{
#ifdef PRODUCT_WeMo_NetCam
    char command[MAX_BUFFER_LEN];
    char line[MAX_BUFFER_LEN];
    //nvram_get(RT2860_NVRAM, "WrxWLanIP", szTmpIp, 64);

    memset(gIPaddress, 0x00, sizeof(gIPaddress));
    memset(command, 0x00, sizeof(command));
    memset(line, 0x00, sizeof(line));

    snprintf(command, sizeof(command), "%s", CMD_WAN_IP);
    FILE *pipe = popen(command, "r");
    if (pipe == NULL)
    {
	printf("Popen Error %s", strerror(errno));
	resetSystem();
    }
    while( fgets(line, MAX_BUFFER_LEN, pipe) )
    {   
	line[strlen(line)-1] = '\0';
	printf("GetWanIPAddress: line = %s\n", line);
	break;
    }

    if (0x00 == strlen(line))
   {
       strcpy(gIPaddress, "0.0.0.0");
      printf("CAN NOT READ the WAN IP #######\n");
   }
   else
   {
        strncpy(gIPaddress, line, sizeof(gIPaddress)-1);
	printf("gWanIpAddress: = %s\n", gIPaddress);
   }
    pclose(pipe);
   return gIPaddress;
#else
    return getIPAddress(gWiredStatus == -1 ? WIRED_IFACE : WAN_IFACE);
#endif
}


#define CMD_GW "route -n | grep UG > /tmp/gateway.txt"
#define FILENAME_GW "/tmp/gateway.txt"
#define DEFAULT "0.0.0.0"

#ifdef PRODUCT_WeMo_NetCam
#define CMD_DEF_GW "netstat -nr | grep UG | cut -d' ' -f10"
#else
#define CMD_DEF_GW "route -n | grep 'UG[ \t]' | awk '{print $2}' > /tmp/gateway.txt"
#endif

char* UpdateWanDefaultGateway(void)
{
    char line[MAX_BUFFER_LEN];
    char command[MAX_BUFFER_LEN];

    memset(command, 0x00, sizeof(command));
    snprintf(command, sizeof(command), "%s", CMD_DEF_GW);
    system(CMD_DEF_GW);

    strncpy(gWanGWIpAddress, "0.0.0.0", sizeof(gWanGWIpAddress));

    FILE *fp = fopen(FILENAME_GW, "rb");

    if( fp )
    {
        while( fgets(line, MAX_BUFFER_LEN, fp) )
        {
            printf("UpdateWanDefaultGateway: line = %s\n", line);
            memset(gWanGWIpAddress, 0x00, sizeof(gWanGWIpAddress));
            strncpy(gWanGWIpAddress, line, sizeof(gWanGWIpAddress));
            break;
        }
        fclose(fp);
    }

    printf("UpdateWanDefaultGateway: GATEWAY = %s\n", gWanGWIpAddress);
    return gWanGWIpAddress;
}
char* GetWanDefaultGateway(void)
{
    char line[MAX_BUFFER_LEN];
    char command[MAX_BUFFER_LEN];

#ifdef PRODUCT_WeMo_NetCam
   memset(gWanGWIpAddress, 0x00, sizeof(gWanGWIpAddress));
    //nvram_get(RT2860_NVRAM, "MyGateIP", gWanGWIpAddress, sizeof(gWanGWIpAddress));

    memset(command, 0x00, sizeof(command));
    snprintf(command, sizeof(command), "%s", CMD_DEF_GW);
    FILE *pipe = popen(command, "r");
    if (pipe == NULL)
    {
	printf("Popen Error %s", strerror(errno));
	resetSystem();
    }
    while( fgets(line, MAX_BUFFER_LEN, pipe) )
    {
	printf("GetWanDefaultGateway: line = %s\n", line);
	memset(gWanGWIpAddress, 0x00, sizeof(gWanGWIpAddress));
	strncpy(gWanGWIpAddress, line, sizeof(gWanGWIpAddress));
	break;
    }
    pclose(pipe);
#else
    memset(command, 0x00, sizeof(command));
    snprintf(command, sizeof(command), "%s", CMD_DEF_GW);
    system(CMD_DEF_GW);

    strncpy(gWanGWIpAddress, "0.0.0.0", sizeof(gWanGWIpAddress));

    FILE *fp = fopen(FILENAME_GW, "rb");

    if( fp )
    {
        while( fgets(line, MAX_BUFFER_LEN, fp) )
        {
            printf("GetWanDefaultGateway: line = %s\n", line);
            memset(gWanGWIpAddress, 0x00, sizeof(gWanGWIpAddress));
            strncpy(gWanGWIpAddress, line, sizeof(gWanGWIpAddress));
            break;
        }
        fclose(fp);
    }
#endif
    printf("GetWanDefaultGateway: GATEWAY = %s\n", gWanGWIpAddress);
    return gWanGWIpAddress;


}

//END: Interfaces with Device WAN Info

//Start: Interfaces with AP
int GetEnableAP(void)
{
    return BELKIN_SUCCESS;
}
//END: Interfaces with AP

//Start: Interfaces with Save Belkin Parameters
//Set the values in NVRAM. The name=value format
int SetBelkinParameter(char* ParameterName, char* ParameterValue)
{
    int retVal = BELKIN_SUCCESS;
    int argCount = 3;
    char *argValue[3];
    int i = 0, len =0;

    printf("%s - ParameterName = %s , ParameterValue = %s\n", __FUNCTION__, ParameterName, ParameterValue);

    retVal = check_parametername(ParameterName);
    if( retVal )
    {
        return BELKIN_FAILURE;
    }

    retVal = check_parametervalue(ParameterValue);
    if( retVal )
    {
        return BELKIN_FAILURE;
    }

    argValue[0] = (char *)malloc(10);
    strcpy(argValue[0], "nvram_set");

    argValue[1] = (char *)malloc((strlen(ParameterName)+1));
    strcpy(argValue[1], ParameterName);

    len = strlen(ParameterValue);
    if( len )
    {
        argValue[2] = (char *)malloc((len+3));
        strcpy(argValue[2], ParameterValue);
    }
    else
    {
        --argCount;
    }

#ifndef PRODUCT_WeMo_NetCam
    nvramset(argCount, argValue);
#else
    if(!strcmp(ParameterName,"home_id") 
	    || !strcmp(ParameterName,"plugin_key") 
	    || !strcmp(ParameterName,"restore_state") 
	    || !strcmp(ParameterName,"RouterMac"))
    {
	printf("%s|%d - Parameter = %s|%s\n", __FUNCTION__, __LINE__, ParameterName, ParameterValue);
	retVal = nvram_set(RT2860_NVRAM_2, ParameterName, ParameterValue);
    }
    else
    { 
	printf("%s|%d - Parameter = %s|%s\n", __FUNCTION__, __LINE__, ParameterName, ParameterValue);
   retVal = nvram_bufset(RT2860_NVRAM, ParameterName, ParameterValue);
    }
#endif

    for( i = 0; i < argCount; i++ )
    {
        free(argValue[i]);
    }

    return retVal;
}

char persistenceVal[MAX_BUFFER_LEN][MAX_BUFFER_LEN];
static int indexVal;

char* GetBelkinParameter(char* ParameterName)
{
#ifndef PRODUCT_WeMo_NetCam
    char *argValue[] = { "nvram_get", ParameterName };
    static size_t argCount = sizeof( argValue ) / sizeof( argValue[0] );
#else
    char *argValue = ParameterName;
#endif
    char paramVal[MAX_BUFFER_LEN] = {};  /* Empty braces cause zero-fill */
    char *retVal = "";

    if (!check_parametername(ParameterName)) {
#ifndef PRODUCT_WeMo_NetCam
      nvramget(argCount, argValue, paramVal);
#else
      if(!strcmp(ParameterName,"home_id") 
	      || !strcmp(ParameterName,"plugin_key") 
	      || !strcmp(ParameterName,"restore_state") 
	      || !strcmp(ParameterName,"RouterMac"))
      {
	  nvram_get(RT2860_NVRAM_2, argValue, paramVal, 128);
	  printf("%s|%d - ParameterName = %s|%s\n", __FUNCTION__, __LINE__, ParameterName, paramVal);
      }
      else
      { 
      nvram_get(RT2860_NVRAM, argValue, paramVal, 128);
	  printf("%s|%d - ParameterName = %s|%s\n", __FUNCTION__, __LINE__, ParameterName, paramVal);
      }
#endif
      if ( *paramVal ) {
   retVal = strncpy( persistenceVal[indexVal],
           paramVal,
           sizeof( persistenceVal[indexVal] ) - 1 );
   /* Assuming max MAX_BUFFER_LEN variables are to be saved */
   indexVal = (indexVal + 1) % MAX_BUFFER_LEN;
      }
    }

    return retVal;
}

int UnSetBelkinParameter(char* ParameterName)
{
#ifndef PRODUCT_WeMo_NetCam
    int retVal = BELKIN_SUCCESS;
    int i;
    int argCount = 2;
    char *argValue[2];

    //printf("%s - ParameterName = %s\n", __FUNCTION__, ParameterName);

    retVal = check_parametername(ParameterName);

    if( retVal )
    {
       return BELKIN_FAILURE;
    }

    argValue[0] = (char *)malloc(10);
    strcpy(argValue[0], "nvram_set");

    argValue[1] = (char *)malloc((strlen(ParameterName)+1));
    strcpy(argValue[1], ParameterName);

    nvramset(argCount, argValue);

    for( i = 0; i < argCount; i++ )
    {
       free(argValue[i]);
   }
#else
   if ((0x00 != ParameterName) && (0x00 != strlen(ParameterName)))
   {
      SetBelkinParameter(ParameterName, "");
   }  
#endif
    return BELKIN_SUCCESS;
}
//END: Interfaces with Save Belkin Parameters

//Start: Interfaces with Save Setting and Reboot
int ResetToFactoryDefault(int runScript)
{
    char restoreBuf[10] = {'\0'};

    system("nvram restore");

    sprintf(restoreBuf, "%d", runScript);
    SetBelkinParameter(RESTORE_STATE, restoreBuf);

    System("rm -f /tmp/Belkin_settings/icon*");
    System("rm -f /tmp/Belkin_settings/rules/*");
    System("rm -f /tmp/Belkin_settings/rules*.db");
    System("rm -f /tmp/Belkin_settings/data_stores/*");
    sleep(5);
    system("reboot");

    return BELKIN_SUCCESS;
}

//remaining
int SaveSetting(void)
{
#ifdef PRODUCT_WeMo_NetCam
   nvram_commit(RT2860_NVRAM);
#else
   NvramCommit();
#endif
    return BELKIN_SUCCESS;
}
//END: Interfaces with Save Setting and Reboot

//Start: Interfaces with WiFi LED Status Control
int SetWiFiLED(int state)
{
    char buf[MAX_BUFFER_LEN] = {'\0'};
    int retVal = 0;

    snprintf(buf, sizeof(buf), "echo %d > /proc/PLUGIN_LED_GPIO", state);

    retVal = system(buf);

    return BELKIN_SUCCESS;
}
//END: Interfaces with WiFi LED Status Control


//Start: Interfaces with Motion Sensor Status Control
/*
/proc/MOTION_SENSOR_STATUS
/proc/MOTION_SENSOR_SET_DELAY
/proc/MOTION_SENSOR_SET_SENSITIVITY
*/
int EnableMotionSensorDetect(int isEnable)
{
    /* Funtion: MOTION_SENSOR_STATUS_write_proc.
       sensor_enable==1 implies START MOTION SENSOR DETECT
     */
    FILE *fp;
    char *fname = "/proc/MOTION_SENSOR_STATUS";

    fp = fopen(fname, "w");
    if( !fp )
    {
       return BELKIN_FAILURE;
    }

    fprintf(fp, "%d", isEnable);
    fclose(fp);

    return BELKIN_SUCCESS;
}

int SetMotionSensorDelay(int Delay_Seconds, int Sensitivity_Percent)
{
    /*Will  try to set in "/proc/MOTION_SENSOR_SET_DELAY"
    Function: MOTION_SENSOR_SET_DELAY_proc
     */
    FILE *fp,*fp1;
    char *fname = "/proc/MOTION_SENSOR_SET_DELAY";
    char *fname1 = "/proc/MOTION_SENSOR_SET_SENSITIVITY";

    fp = fopen(fname, "w");
    if(!fp)
    {
       return BELKIN_FAILURE;
    }

    fprintf(fp, "%d", Delay_Seconds);
    fclose(fp);

    fp1 = fopen(fname1, "w");
    if(!fp1)
    {
       return BELKIN_FAILURE;
    }

    fprintf(fp1, "%d", Sensitivity_Percent);
    fclose(fp1);

    return BELKIN_SUCCESS;
}
//END: Interfaces with Motion Sensor Status Control

//Start: Interfaces with Motion Enable/Disable Watch Dog
#define DEVICE_FILE_NAME "/dev/watchdog"
int EnableWatchDog(int isEnable, int seconds)
{
    int fd, ret_val;

    fd = open(DEVICE_FILE_NAME, O_WRONLY);

    if( fd < 0 )
    {
        perror("watchdog");
        printf("???????????????????? Unable to open watchdog: Device busy ??????????????????\n");
        return BELKIN_FAILURE;
    }

    if( isEnable )
    {
        if( ioctl(fd, WDIOC_SETTIMEOUT, &seconds) )
        {
            perror("Set Timeout");
            if( fd )
            {
                close(fd);
            }
            return(-1);
        }
        printf("The timeout was set to %d seconds\n", seconds);
        if( ioctl(fd, WDIOC_GETTIMEOUT, &ret_val) )
        {
            perror("Get Timeout");
            if( fd )
            {
                close(fd);
            }
            return BELKIN_FAILURE;
        }
        printf("GETTIMEOUT Returned %d seconds\n", ret_val);
        if( ret_val != seconds )
        {
            perror("Get Timeout is not equal to Set Timeout");
            if( (write(fd,"V",1)) == -1 )
            {
                perror("Magic Close");
                printf("!!!!!!!!Magic Close Failed\n");
            }
            else
            {
                printf("Magic Close Success\n");
            }
            if(close(fd) == -1)
            {
                printf("!!!!!Watchdog Not Closed\n");
            }
            return BELKIN_FAILURE;
        }
        if( close(fd) == -1 )
        {
            printf("!!!!!Watchdog Not Closed\n");
        }
    }
    else
    {
        if( (write(fd,"V",1)) == -1 )
        {
            perror("Magic Close");
            printf("!!!!!!!!Magic Close Failed\n");
        }
        else
        {
            printf("Magic Close Success\n");
        }
        if( close(fd) == -1 )
        {
            printf("!!!!!Watchdog Not Closed\n");
        }
    }

    return BELKIN_SUCCESS;
}

int SyncWatchDogTimer(void)
{
    int fd;

    fd = open(DEVICE_FILE_NAME, O_RDWR);

    if( fd < 0 )
    {
        printf("IN SyncWatchDogTimer: ???????????? Unable to open watchdog: Device busy ??????????\n");
        perror("watchdog");
        return BELKIN_FAILURE;
    }

    if( ioctl(fd, WDIOC_KEEPALIVE, 0) )
    {
        perror("keepalive");
    }

    write(fd, "w", 1);

    close(fd);

    printf("%s Success\n", __FUNCTION__);

    return BELKIN_SUCCESS;
}

int GetRebootStatus(int *Status, unsigned long int *UTC_seconds)
{
    int fd;
    unsigned long argp;

    fd = open(DEVICE_FILE_NAME, O_RDONLY);

    if( fd < 0 )
    {
        perror("watchdog");
        return BELKIN_FAILURE;
    }

    if( ioctl(fd, WDIOC_GETBOOTSTATUS, &argp) )
    {
        perror("WDIOC_GETBOOTSTATUS");
    }

    close(fd);

    Status = (int *)argp;
    UTC_seconds = (unsigned long int *)argp;

    return BELKIN_SUCCESS;
}

int SetActivityLED(int state)
{
    char buf[MAX_BUFFER_LEN] = {'\0'};
    int retVal = 0;

    sprintf(buf, "echo %d > /proc/ACT_LED_GPIO", state);

    retVal = system(buf);

    return BELKIN_SUCCESS;
}

//END: Interfaces with Motion Enable/Disable Watch Dog

/* 
   Test if a carrier is present on the wired Ethernet port (eth2).
   We can't do this in a generic way because the Ralink drivers don't
   implement carrier status.  We use Ralink IOCTL to read the status
   directly from the PHY.
 
   Since no WeMo products to date use any PHYs other than 0 we will
   power them down unconditionally when this routine is called.  The
   Ralink SOC runs HOT, powering down the unused PHYS saves a significant
   amount of power which results in a cooler and more green product.
 
   If bPowerDown is true we will also power down PHY0 if carrier is not
   present when this routine is called, otherwise it is lft powered up.
 
   returns:
      0 - Link Down
      -1 = Link up
      > 0 - error
*/
int WiredEthernetUp(int bPowerDown)
{
   int Fd;
   struct ifreq ifr;
   ra_mii_ioctl_data mii;
   int loops = 0;
   int i;
   int Ret = 0;

   strncpy(ifr.ifr_name,"eth2",sizeof(ifr.ifr_name));
   ifr.ifr_data = (void *)&mii;

   if((Fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      Ret = errno;
   }
   else {
   // Power down all PHYs except 0 to save power
      mii.reg_num = 0;  // control register
      for(i = 0; i < 5; i++) {
         mii.phy_id = (__u16) i;
         mii.val_in = (__u16) (i == 0 ? 0x3100 : 0x3900);

         if((Ret = ioctl(Fd,RAETH_MII_WRITE,&ifr)) < 0) {
            Ret = errno;
            break;
         }
      }
   }

   while(Ret == 0) {
      mii.phy_id = 0;   // Phy 0
      mii.reg_num = 1;  // status register
      if(ioctl(Fd,RAETH_MII_READ,&ifr) < 0) {
         Ret = errno;
         break;
      }
   // Check if link_status (bit 2) is set in the MII status register
      if(mii.val_out & 0x4) {
      // Link is up
         Ret = -1;    // return -1 on good link
         break;
      }
      else if(loops++ >= 10) {
      // Give up after one second
      // if there's no link disable the PHY to save power.  
         if(bPowerDown) {
            mii.val_in = (__u16) 0x3900;
            mii.reg_num = 0;
            if(ioctl(Fd,RAETH_MII_WRITE,&ifr) < 0) {
               Ret = errno;
               break;
            }
         }
      // return 0 on link down
         Ret = 0;
         break;
      }
      else {
      // Sleep for 100 millseconds and then try again
         usleep(100000);   // 100 milliseconds
      }
   }

   if(Fd >= 0) {
      close(Fd);
   }

   gWiredStatus = Ret;
   return Ret;
}

static void Minutes2HrsMinutes(int TotalMinutes,int *pHours,int *pMinutes)
{
	*pHours = TotalMinutes / 60;
	*pMinutes = TotalMinutes % 60;
	if(TotalMinutes < 0) {
		*pMinutes = -*pMinutes;
	}
}

// Note: The compiler generations warnings about a NULL argument for the 
// first argument to settimeofday() in the following function, however
// a null argument is correct when setting just the  timezone.
// Disable the warning for this function
#pragma GCC diagnostic ignored "-Wnonnull"

// Write TZ string to /tmp/TZ, add it to the environment and
// set the timezone in the kernel.
static void WriteTzInfo(char *TZ,int MinutesWest)
{
	struct timezone TimeZone;
	FILE *fp = fopen("/tmp/TZ","w");

	if(fp != NULL) {
		if(fprintf(fp,"%s\n",TZ) <= 0) {
			syslog(LOG_ERR,"%s: fprintf failed - %m",__FUNCTION__);
		}
		fclose(fp);
	}
	else {
		syslog(LOG_ERR,"%s: fopen failed - %m",__FUNCTION__);
	}

// Set TZ in the environment and then call tzset to ensure
// the run time library's idea of the time zone is up to date.
	setenv("TZ",TZ,1);
	tzset();

	memset(&TimeZone,0,sizeof(TimeZone));

// workaround for warp_clock() on first invocation
	settimeofday(NULL,&TimeZone); 

	TimeZone.tz_minuteswest = MinutesWest;
	if(strstr(TZ,"DST") != NULL) {
		TimeZone.tz_dsttime = 1;
	}

	if(settimeofday(NULL,&TimeZone) != 0) {
		syslog(LOG_ERR,"%s#%d: settimeofday failed - %m\n",
				 __FUNCTION__,__LINE__);
	}

#ifdef NTP_DEBUG
	if(gettimeofday(NULL,&TimeZone) != 0) {
		syslog(LOG_ERR,"%s#%d: gettimeofday failed - %m\n",
				 __FUNCTION__,__LINE__);
	}
	else {
		NTP_LOG("%s: gettimeofday returned tz_minuteswest: %d\n",__FUNCTION__,
				  TimeZone.tz_minuteswest);
	}
#endif
}
// Turn warnings about NULL arguments back on
#pragma GCC diagnostic warning "-Wnonnull"


// Offset - number of hours time timezone is CURRENTLY offset from UTC
// bIsDst - TRUE if DST is currently active
// bEnableDST - time zone has daylight savings time
int SetTimeAndTZ(time_t TimeUTC,char *Offset,int bIsDst,int bEnableDST)
{
	int Hours;
	char TZ[16];
	char *cp = strchr(Offset,'.');
	int Ret = FALSE;	// assume the worse

	NTP_LOG("%s: Offset: %s, bIsDst: %d, bEnableDST: %d\n",__FUNCTION__,
			  Offset,bIsDst,bEnableDST);

	do {
		if(sscanf(Offset,"%d",&Hours) != 1) {
			syslog(LOG_ERR,"%s: Unable to parse TimeZone '%s'",__FUNCTION__,Offset);
			break;
		}

		if(bIsDst) {
		// The phone sends the *current* offset from UTC, reverse the
		// daylight savings time adjustment if it's in effect

		// NB: this NOT correct everywhere in the world since some
		// countries have daylight savings time offsets that aren't an
		// hour, but we don't have enough information to do it correctly.
			Hours--;
		}

	// Save absolute timezone offset from UTC in flash
		if(cp != NULL) {
			snprintf(TZ,sizeof(TZ),"%d%s",Hours,cp);
		}
		else {
			snprintf(TZ,sizeof(TZ),"%d.0",Hours);
		}

		NTP_LOG("%s: Setting " SYNCTIME_LASTTIMEZONE " to %s\n",__FUNCTION__,TZ);
		SetBelkinParameter(SYNCTIME_LASTTIMEZONE,TZ);
	// save Daylight savings enabled in flash
		NTP_LOG("%s: Setting " SYNCTIME_DSTSUPPORT " to %d\n",__FUNCTION__,
				  bEnableDST);
		SetBelkinParameter(SYNCTIME_DSTSUPPORT,bEnableDST ?  "1" : "0");
		Ret = SetTime(TimeUTC,0,0);
	} while(FALSE);

	return Ret;
}

int static TzOffset2MinutesWest(char *Offset)
{
	int Hours;
	int MinutesEast = 0;
	int i;
	char *cp = strchr(Offset,'.');

	do {
		if(cp != NULL) {
		// Convert the fractional part
			cp++;
			if(sscanf(cp,"%d",&MinutesEast) != 1) {
				syslog(LOG_ERR,"%s#%d: Unable to parse TimeZone '%s'",
						 __FUNCTION__,__LINE__,Offset);
				break;
			}
			MinutesEast *= 60;
			i = strlen(cp);

			while(i-- > 0) {
				MinutesEast /= 10;
			}
		}
		if(sscanf(Offset,"%d",&Hours) != 1) {
			syslog(LOG_ERR,"%s#%d: Unable to parse TimeZone '%s'",
					 __FUNCTION__,__LINE__,Offset);
			break;
		}
		Hours *= 60;
		if(Hours < 0) {
			MinutesEast = Hours - MinutesEast;
		}
		else {
			MinutesEast += Hours;
		}
	} while(FALSE);

	return -MinutesEast;
}
// Wrapper for system command that closes all handles before calling system()
// to prevent the child process from inheriting open file handles and sockets.
int System(const char *command)
{
   struct rlimit Limits;
   int i;
   pid_t pid;
   int Ret = 0;

   if((pid = fork()) < 0) {
      syslog(LOG_ERR,"%s: fork failed - %m",__FUNCTION__);
   }
   else if(pid == 0) {
   // child, close all handles
      Limits.rlim_cur = 1024; // just a default in case getrlimit fails
      getrlimit(RLIMIT_NOFILE,&Limits);

      for(i = 0; i < Limits.rlim_cur; i++) {
      // No need to log errors, the handles might not actually be open
      // since we're just blindly closing all possible handles
         close(i);
      }
   // run the command and pass on the exit status our parent
      exit(system(command));
   }
   else {
   // Parent
      if(waitpid(pid,&Ret,0) == -1) {
         syslog(LOG_ERR,"%s: waitpid failed - %m",__FUNCTION__);
         Ret = errno;
      }
   }

   return Ret;
}


