#ifndef __NETCAM_PROFILE__H__
#define __NETCAM_PROFILE__H__

#define     PROFILE_SUCCESS     0x00
#define     PROFILE_FAILURE     0x01

#if defined(NETCAM_LS)
#  define NETCAM_NAME_PREFIX "LinksysWNC"
#else
#  define NETCAM_NAME_PREFIX "NetCam"
#endif

/**< Name of NVRAM variable containing netcam "friendly name" */
#define FRIENDLY_NAME_VAR "FriendlyName"

/* Seedonk login name */
#define SEEDONK_LOGIN_NAME_VAR "SeedonkOwner"

/**< Name of NVRAM variable containing netcam "camera name".
 * This value is set and managed by Seedonk.
 */ 
#define CAMERA_NAME_VAR "CameraName"

/**
 * Callback for motion detection integration
 *
 */
//int DetectMotionIF(char* data);
int DetectMotionIF(int data);

/** 
 * Entry point for NetCam sensor task
 *
 */
void StartNetCamSensorTask();
int NetCamCheckNeworkState();

char* SetNetCamProfileApSSID();

char* GetNetCamNameFromCloud(const char* szLogName, const char* szMacAddress, char* destBuff);

#define     NETCAM_URL      "https://www.seedonk.com/camsetup/retrievecamera.php?"

//e.g "https://www.seedonk.com/camsetup/retrievecamera.php?login=mingtest&mac=D4:12:BB:01:10:8C&key=b0f036dd66d37449819e6dd183b4785"

int IsHomeRouterHotChanged();

void RunNetworkWatcherTask();


/**
 *  @brief  AsyncRequestNetCamFriendlyName
 *        It is to async request the device friendly name form NetCam cloud, 
 *        Please note that, there is no push notification mechanisam for NetCam name
 *        change
 *  
 *  @return void
 */
void AsyncRequestNetCamFriendlyName();


/**
 *  @brief  RestoreNetCam
 *        It is to restore NetCam paramters so that 
 *  
 *  @return void
 */
void RestoreNetCam();


/**
 *  @brief  ProcessNetCamTimeZoneEvent
 *        To process the push notification information from other SNS devices 
 *  
 *  @return void
 */
void ProcessNetCamTimeZoneEvent(char* szEvent);


/**
 *  @brief  SetNetCamName
 *        To check and set default name from NetCam 
 *  
 *  @return void
 */

void SetNetCamName();

void InitNetCamProfile();


/**
 *  @brief  StartDNSTask()
 *          To start the DNS task for NAT initilization
 *
 *  @return void
 */
void StartDNSTask();


void InitNetCamNameSession();

void StartNetCamNameRequestTask();

/**
 *	@brief IsTimeZoneSetupByNetCam
 *			To check the timezone set up by NetCam app already OR not
 *			
 *
 *	@return int
 *		0x01	Setup already by NetCam, will WeMo iOS app will ingonore
 *
 *		0x00	
 */
int IsTimeZoneSetupByNetCam();

#define		NETCAM_TIME_ZONE_KEY		"NetCamTimeZoneFlag"




//- NetCam Time zone, city and etc implementation related

#if 0
typedef struct _short_tz
{
	char 		szShortName[64];
	char 		szLocation[64];
	char 		szShortLocaltion[16];
	float 		ftTimeZoneValue;
	
} SHORT_TZ_T;



static SHORT_TZ_T g_sLstAllTimeZone[] = 
{
	{"A",           "Alpha Time Zone", 						"Military",					1.0},
	{"ACDT",        "Australian Central Daylight Time", 	"Australia	"					10.50},
	{"ACST", 		 "Australian Central Standard Time", 	"Australia", 					9.50},
	{"ADT", 		 "Atlantic Daylight Time", 				"Atlantic	", 					-3.0},
	{"ADT",			 "Atlantic Daylight Time", 				"North America",				-3.0},
	{"AEDT",		 "Australian Eastern Daylight Time", 	"Australia",					11.0},
	{"AEST",		 "Australian Eastern Standard Time", 	"Australia",					10.0},
	{"AFT",			 "Afghanistan Time", 						"Asia", 						4.30},
	{"AKDT",		 "Alaska Daylight Time", 					"North America",				-8.0},
	{"AKST",		 "Alaska Standard Time",					"North America", 				-9.0},
	{"ALMT"	,		 "Alma-Ata Time",							"Asia", 						6.0},
	{"AMST",		"Armenia Summer Time",					"Asia",							5.0},
	{"AMST", 		"Amazon Summer Time", 					"South America",				-3.0},
	{"AMT",			"Armenia Time",							"Asia",							4.0}, 
	{"AMT",			"Amazon Time",								"South America",				-4.0}, 
	{"ANAST", 		"Anadyr Summer Time",						"Asia"							12.0},
	{"ANAT",		"Anadyr Time",								"Asia",							12.0},
	{"AQTT",		"Aqtobe Time",								"Asia",							5.0}, 
	{"ART",			"Argentina Time",							"South America",				-3.0}, 
	{"AST",			"Arabia Standard Time",					"Asia",							3.0}, 
	{"AST",			"Atlantic Standard Time",					"Atlantic",					-4.0}, 
	{"AST",			"Atlantic Standard Time",					"Caribbean,"	 				-4.0}, 
	{"AST",			"Atlantic Standard Time",					"North America",	 			-4.0}, 
	{"AWDT",		"Australian Western Daylight Time",		"Australia",					9.0}, 
	{"AWST",		"Australian Western Standard Time",		"Australia",	 				8.0}, 
	{"AZOST", 		"Azores Summer Time",						"Atlantic",					0.0},
	{"AZOT",		"Azores Time",								"Atlantic",	 				-1.0},
	{"AZST",		"Azerbaijan Summer Time",					"Asia",							5.0}, 
	{"AZT",			"Azerbaijan Time",						"Asia",							4.0}, 
	{"B",			"Bravo Time Zone",						"Military",	 				2.0}, 
	{"BNT",			"Brunei Darussalam Time",					"Asia",							8.0}, 
	{"BOT",			"Bolivia Time",							"South America",	 			-4.0},
	{"BRST",		"Brasilia Summer Time",					"South America", 				-2.0}, 
	{"BRT",			"Brasília time",							"South America",	 			-3.0}, 
	{"BST",			"Bangladesh Standard Time",				"Asia",		 					6.0},
	{"BST",			"British Summer Time",					"Europe",	 					1.0},
	{"BTT",			"Bhutan Time",								"Asia",							6.0}, 
	{"C",			"Charlie Time Zone",						"Military",					3.0}, 
	{"CAST",		"Casey Time",								"Antarctica",					8.0},
	{"CAT",			"Central Africa Time",					"Africa",	 					2.0}, 
	{"CCT",			"Cocos Islands Time",						"Indian Ocean",	 			6.50}, 
	{"CDT",			"Cuba Daylight Time",						"Caribbean",	 				-4.0}, 
	{"CDT",			"Central Daylight Time",					"North America",	 			-5.0}, 
	{"CEST",		"Central European Summer Time",			"Europe",						2.0}, 
	{"CET",			"Central European Time",					"Africa",	 					1.0},
	{"CET",			"Central European Time",					"Europe",						1.0},
	{"CHADT",		"Chatham Island Daylight Time",			"Pacific",					    13.75}, 
	{"CHAST",		"Chatham Island Standard Time",			"Pacific",						12.75}, 
	{"CKT",			"Cook Island Time",						"Pacific",						10.0}, 
	{"CLST",		"Chile Summer Time",						"South America",				-3.0}, 
	{"CLT",			"Chile Standard Time",					"South America",			 	-4.0}, 
	{"COT",			"Colombia Time",							"South America",	 			-5.0},  
	{"CST",			"China Standard Time",					"Asia",							8.0}, 
	{"CST",			"Central Standard Time",					"Central America",	 		-6.0}, 
	{"CST",			"Cuba Standard Time",						"Caribbean",	 				-5.0}, 
	{"CST",			"Central Standard Time",					"North America",	 			-6.0}, 
	{"CVT",			"Cape Verde Time",						"Africa",						-1.0},
	{"CXT",			"Christmas Island Time",					"Australia",	 				7.0}, 
	{"ChST"			"Chamorro Standard Time",					"Pacific",	 					10.0}, 
	{"D",			"Delta Time Zone",						"Military",	 				4.0}, 
	{"DAVT",		"Davis Time",								"Antarctica",	 				7.0}, 
	{"E",			"Echo Time Zone",							"Military",	 				5.0}, 
	{"EASST",		"Easter Island Summer Time",				"Pacific",						-5.0}, 
	{"EAST",		"Easter Island Standard Time",			"Pacific",						-6.0}, 
	{"EAT",			"Eastern Africa Time",					"Africa",	 					3.0}, 
	{"EAT",			"East Africa Time",						"Indian Ocean",				3.0}, 
	{"ECT",			"Ecuador Time",							"South America",	 			5.0}, 
	{"EDT",			"Eastern Daylight Time",					"Caribbean",	 				-4.0}, 
	{"EDT",			"Eastern Daylight Time",					"North America",	 			-4.0}, 
	{"EDT",			"Eastern Daylight Time",					"Pacific",						11.0}, 
	{"EEST",		"Eastern European Summer Time",			"Africa",						3.0}, 
	{"EEST",		"Eastern European Summer Time",			"Asia",	 						3.0}, 
	{"EEST",		"Eastern European Summer Time",			"Europe	",						3.0}, 
	{"EET",			"Eastern European Time",					"Africa",	 					2.0}, 
	{"EET",			"Eastern European Time",					"Asia",							2.0}, 
	{"EET",			"Eastern European Time",					"Europe",						2.0}, 
	{"EGST",		"Eastern Greenland Summer Time",			"North America",				2.0},	
	{"EGT",			"East Greenland Time",					"North America",				-1.0},
	{"EST",			"Eastern Standard Time",					"Central America",	 		-5.0}, 
	{"EST",			"Eastern Standard Time",					"Caribbean",					-5.0}, 
	{"EST",			"Eastern Standard Time",					"North America",	 			-5.0}, 
	{"ET",			"Tiempo del Este",						"Central America",			-5.0}, 
	{"ET",			"Tiempo del Este",						"Caribbean",					-5.0}, 
	{"ET",			"Tiempo Del Este", 						"North America",	 			-5.0}, 
	{"F",			"Foxtrot Time Zone",						"Military",					6.0}, 
	{"FJST",		"Fiji Summer Time",						"Pacific",						13.0}, 
	{"FJT",			"Fiji Time",								"Pacific",						12.0}, 
	{"FKST",		"Falkland Islands Summer Time",			"South America",	 			-3.0}, 
	{"FKT",			"Falkland Island Time",					"South America",				-4.0}, 
	{"FNT",			"Fernando de Noronha Time",				"South America",	 			-2.0}, 
	{"G",			"Golf Time Zone",							"Military",					7.0}, 
	{"GALT",		"Galapagos Time",							"Pacific",						-6.0}, 
	{"GAMT",		"Gambier Time",							"Pacific",						-9.0}, 
	{"GET",			"Georgia Standard Time",					"Asia",							4.0}, 
	{"GFT",			"French Guiana Time",						"South America",				-3.0}, 
	{"GILT",		"Gilbert Island Time",					"Pacific",	 					12.0}, 
	{"GMT",			"Greenwich Mean Time",					"Africa	",						0.0},
	{"GMT",			"Greenwich Mean Time",					"Europe",						0.0},	
	{"GST",			"Gulf Standard Time",						"Asia",	 						4.0}, 
	{"GYT",			"Guyana Time",								"South America",				-4.0}, 
	{"H",			"Hotel Time Zone",						"Military",					8.0}, 
	{"HAA",			"Heure Avancée de l'Atlantique",			"Atlantic",					-3.0}, 
	{"HAA",			"Heure Avancée de l'Atlantique",			"North America",				-3.0}, 
	{"HAC",			"Heure Avancée du Centre",				"North America",	 			-5.0}, 
	{"HADT",		"Hawaii-Aleutian Daylight Time",			"North America",	 			-9.0}, 
	{"HAE",			"Heure Avancée de l'Est",					"Caribbean",	 				-4.0}, 
	{"HAE",			"Heure Avancée de l'Est", 				"North America",				-4.0}, 
	{"HAP",			"Heure Avancée du Pacifique",			"North America",				-7.0}, 
	{"HAR",			"Heure Avancée des Rocheuses",			"North America",				-6.0}, 
	{"HAST",		"Hawaii-Aleutian Standard Time",			"North America",	 			-10.0}, 
	{"HAT",			"Heure Avancée de Terre-Neuve",			"North America",	 			-2.50}, 
	{"HAY",			"Heure Avancée du Yukon",					"North America	", 				-8.0}, 
	{"HKT",			"Hong Kong Time",							"Asia",	 						8.0}, 
	{"HLV",			"Hora Legal de Venezuela",				"South America",	 			-4.50},
	{"HNA",			"Heure Normale de l'Atlantique",			"Atlantic",					-4.0}, 
	{"HNA",			"Heure Normale de l'Atlantique"			"Caribbean",					-4.0}, 
	{"HNA",			"Heure Normale de l'Atlantique",			"North America",				-4.0}, 
	{"HNC",			"Heure Normale du Centre",				"Central America",			-6.0}, 
	{"HNC",			"Heure Normale du Centre"					"North America",				-6.0}, 
	{"HNE",			"Heure Normale de l'Est",					"Central America",			-5.0}, 
	{"HNE",			"Heure Normale de l'Est",					"Caribbean",					-5.0}, 
	{"HNE",			"Heure Normale de l'Est",					"North America",				-5.0}, 
	{"HNP",			"Heure Normale du Pacifique",			"North America",				-8.0}, 
	{"HNR",			"Heure Normale des Rocheuses",			"North America",				-7.0}, 
	{"HNT",			"Heure Normale de Terre-Neuve",			"North America",	 			-3.50},
	{"HNY",			"Heure Normale du Yukon",					"North America",				-9.0}, 
	{"HOVT",		"Hovd Time",								"Asia",		 					7.0}, 
	{"I",			"India Time Zone", 						"Military",					9.0}, 
	{"ICT",			"Indochina Time",							"Asia",							7.0}, 
	{"IDT",			"Israel Daylight Time",					"Asia",							3.0},
	{"IOT",			"Indian Chagos Time",						"Indian Ocean",	 			6.0}, 
	{"IRDT",		"Iran Daylight Time",						"Asia",	 						4.50}, 
	{"IRKST",		"Irkutsk Summer Time",					"Asia",							9.0}, 
	{"IRKT",		"Irkutsk Time",							"Asia",							9.0}, 
	{"IRST",		"Iran Standard Time",						"Asia",							3.50}, 
	{"IST",			"Israel Standard Time",					"Asia",							2.0}, 
	{"IST",			"India Standard Time",					"Asia",							5.50}, 
	{"IST",			"Irish Standard Time",					"Europe",						1.0},
	{"JST",			"Japan Standard Time",					"Asia",							9.0}, 
	{"K",			"Kilo Time Zone",							"Military",					10.0}, 
	{"KGT",			"Kyrgyzstan Time",						"Asia",							6.0}, 
	{"KRAST",		"Krasnoyarsk Summer Time",				"Asia",							8.0}, 
	{"KRAT",		"Krasnoyarsk Time"						"Asia",	 						8.0}, 
	{"KST",			"Korea Standard Time",					"Asia",							9.0}, 
	{"KUYT",		"Kuybyshev Time",							"Europe",						4.0}, 
	{"L",			"Lima Time Zone",							"Military",					11.0}, 
	{"LHDT",		"Lord Howe Daylight Time",				"Australia",					11.0}, 
	{"LHST",		"Lord Howe Standard Time"					"Australia",					10.50}, 
	{"LINT",		"Line Islands Time",						"Pacific",						14.0}, 
	{"M",			"Mike Time Zone",							"Military",					12.0}, 
	{"MAGST",		"Magadan Summer Time",					"Asia",							12.0}, 
	{"MAGT",		"Magadan Time",							"Asia",,						12.0}, 
	{"MART",		"Marquesas Time",							"Pacific",	 					-9.50}, 
	{"MAWT",		"Mawson Time",								"Antarctica",	 				5.0}, 
	{"MDT",			"Mountain Daylight Time",					"North America",	 			-6.0}, 
	{"MESZ",		"Mitteleuropäische Sommerzeit",			"Europe",						2.0}, 
	{"MEZ",			"Mitteleuropäische Zeit",					"Africa",						1.0},
	{"MHT",			"Marshall Islands Time",					"Pacific",						12.0}, 
	{"MMT",			"Myanmar Time",							"Asia",	 						6.50}, 
	{"MSD",			"Moscow Daylight Time",					"Europe",						4.0}, 
	{"MSK",			"Moscow Standard Time",					"Europe",						4.0}, 	
	{"MST",			"Mountain Standard Time",					"North America",				-7.0}, 
	{"MUT",			"Mauritius Time",							"Africa",						4.0}, 
	{"MVT",			"Maldives Time",							"Asia",							5.0}, 
	{"MYT",			"Malaysia Time",							"Asia",							8.0}, 
	{"N",			"November Time Zone",						"Military",					-1.0},
	{"NCT",			"New Caledonia Time",						"Pacific",						11.0},
	{"NDT",			"Newfoundland Daylight Time",			"North America",				-2.50}, 
	{"NFT",			"Norfolk Time",							"Australia",					11.50},
	{"NOVST",		"Novosibirsk Summer Time",				"Asia",							7.0}, 
	{"NOVT",		"Novosibirsk Time",						"Asia",	 						6.0}, 
	{"NPT",			"Nepal Time", 								"Asia",	 						5.75}, 
	{"NST",			"Newfoundland Standard Time",			"North America",	 			-3:50},
	{"NUT",			"Niue Time",								"Pacific",						-11.0}, 
	{"NZDT",		"New Zealand Daylight Time",				"Antarctica",					13.0},
	{"NZDT",		"New Zealand Daylight Time",				"Pacific",						13.0},
	{"NZST",		"New Zealand Standard Time",				"Antarctica",					12.0},
	{"NZST",		"New Zealand Standard Time",				"Pacific",						12.0}, 
	{"O",			"Oscar Time Zone",						"Military",	 				2.0},
	{"OMSST	",		"Omsk Summer Time",						"Asia",							7.0}, 
	{"OMST",		"Omsk Standard Time",						"Asia",							7.0}, 
	{"P",			"Papa Time Zone",							"Military",	 				-3.0}, 
	{"PDT",			"Pacific Daylight Time",					"North America",				-7.0}, 
	{"PET",			"Peru Time",								"South America",	 			-5.0}, 
	{"PETST",		"Kamchatka Summer Time",					"Asia",							12.0}, 
	{"PETT",		"Kamchatka Time",							"Asia",	 						12.0}, 
	{"PGT",			"Papua New Guinea Time",					"Pacific",						10.0}, 
	{"PHOT",		"Phoenix Island Time",					"Pacific",						13.0},
	{"PHT",			"Philippine Time",						"Asia",							8.0}, 
	{"PKT",			"Pakistan Standard Time",					"Asia",							5.0}, 
	{"PMDT",		"Pierre & Miquelon Daylight Time",		"North America",	 			-2.0}, 
	{"PMST",		"Pierre & Miquelon Standard Time",		"North America",				-3.0}, 
	{"PONT",		"Pohnpei Standard Time",					"Pacific",						11.0}, 
	{"PST",			"Pacific Standard Time",					"North America",				-8.0}, 
	{"PST",			"Pitcairn Standard Time",					"Pacific",						-8.0}, 
	{"PT",			"Tiempo del Pacífico",					"North America", 				-8.0}, 
	{"PWT",			"Palau Time",								"Pacific",						9.0}, 
	{"PYST",		"Paraguay Summer Time",					"South America",	 			-3.0}, 
	{"PYT",			"Paraguay Time	South", 					"America",						-4.0}, 
	{"Q",			"Quebec Time Zone",						"Military",					-4.0}, 
	{"R",			"Romeo Time Zone",						"Military",					-5.0}, 
	{"RET",			"Reunion Time",							"Africa",	 					4.0}, 
	{"S",			"Sierra Time Zone",						"Military",					-6.0},
	{"SAMT",		"Samara Time",								"Europe",						4.0},
	{"SAST",		"South Africa Standard Time",			"Africa",						2.0}, 
	{"SBT",			"Solomon IslandsTime",					"Pacific",	 					11.0}, 
	{"SCT",			"Seychelles Time",						"Africa",						4.0}, 
	{"SGT",			"Singapore Time",							"Asia",							8.0}, 
	{"SRT",			"Suriname Time",							"South America",				-3.0},
	{"SST"			"Samoa Standard Time",					"Pacific",						-11.0},
	{"T",			"Tango Time Zone",						"Military",	 				-7.0}, 
	{"TAHT",		"Tahiti Time",								"Pacific",	 					-10.0}, 
	{"TFT",			"French Southern and Antarctic Time"	"Indian Ocean",				5.0}, 
	{"TJT",			"Tajikistan Time",						"Asia",	 						5.0}, 
	{"TKT",			"Tokelau Time",							"Pacific",						13.0},
	{"TLT",			"East Timor Time",						"Asia",							9.0}, 
	{"TMT",			"Turkmenistan Time",						"Asia",							5.0}, 
	{"TVT",			"Tuvalu Time",								"Pacific",						12.0}, 
	{"U",			"Uniform Time Zone",						"Military",					-8.0}, 
	{"ULAT",		"Ulaanbaatar Time",						"Asia",	 						8}, 
	{"UYST",		"Uruguay Summer Time",					"South America",				-2.0}, 
	{"UYT",			"Uruguay Time",							"South America",	 			-3.0}, 
	{"UZT",			"Uzbekistan Time",						"Asia",	 						5 .0},
	{"V",			"Victor Time Zone",						"Military",					-9.0}, 
	{"VET",			"Venezuelan Standard Time"				"South America",				-4.50},
	{"VLAST",		"Vladivostok Summer Time",				"Asia",,						11.0}, 
	{"VLAT",		"Vladivostok Time",						"Asia",							11.0}, 
	{"VUT",			"Vanuatu Time",							"Pacific",	 					11.0}, 
	{"W",			"Whiskey Time Zone",						"Military",					-10.0}, 
	{"WAST",		"West Africa Summer Time",				"Africa",						2.0}, 
	{"WAT",			"West Africa Time",						"Africa",						1.0},
	{"WEST",		"Western European Summer Time",			"Africa",						1.0},
	{"WEST",		"Western European Summer Time",			"Europe",						1.0},
	{"WESZ",		"Westeuropäische Sommerzeit",			"Africa",						1.0},
	{"WET",			"Western European Time",					"Africa",						0.0},	
	{"WET",			"Western European Time",					"Europe	",						0.0},
	{"WEZ",			"Westeuropäische Zeit",					"Europe	",						0.0},
	{"WFT",			"Wallis and Futuna Time",					"Pacific",						12.0},
	{"WGST",		"Western Greenland Summer Time",			"North America",				-2.0}, 
	{"WGT",			"West Greenland Time",					"North America", 				-3.0}, 
	{"WIB",			"Western Indonesian Time",				"Asia",							7.0},
	{"WIT",			"Eastern Indonesian Time",				"Asia",							9.0},
	{"WITA",		"Central Indonesian Time",				"Asia",							8.0}, 
	{"WST",			"Western Sahara Summer Time",			"Africa",						1.0},
	{"WST",			"West Samoa Time",						"Pacific",						13.0}, 
	{"WT",			"Western Sahara Standard Time",			"Africa	",						0.0},	
	{"X",			"X-ray Time Zone",						"Military",					-11.0}, 
	{"Y",			"Yankee Time Zone",						"Military",					-12.0}, 
	{"YAKST",		"Yakutsk Summer Time",					"Asia",							10.0}, 
	{"YAKT",		"Yakutsk Time",							"Asia",							10.0}, 
	{"YAPT",		"Yap Time",								"Pacific",						10.0}, 
	{"YEKST",		"Yekaterinburg Summer Time",				"Asia",							6.0}, 
	{"YEKT",		"Yekaterinburg Time",						"Asia",							6.0}, 
	{"Z",			"Zulu Time Zone",							"Military",					0.0}
	
}

int GetTimeZoneDSTInfo(const char* szCurrentTimeZone, const char* szOriginalTimeZone, float* ftTimeZone, int* IsDSTEnable);
#endif
int InitNetCamTimeZoneAndDST();

/**
 *
 *
 */
int UpdateNetCamTimeZoneInfo(const char* szRegion);

void InitTimeZoneInfo();

void StartMonitorCameraNameThread();

#endif
