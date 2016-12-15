#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <ctype.h>
#include <pthread.h>
#include <semaphore.h>
#include <regex.h>

#include <openssl/md5.h>

#include "belkin_api.h"
#include "ddbg.h"
#include "netcam_profile.h"
#include "wemo_netcam_api.h"
#include "wifiSetup.h"
#include "global.h"
#include "curl/curl.h"
#include "httpsWrapper.h"
#include "thready_utils.h"

//Additional Headers/includes
#include "remoteAccess.h"
#include "natClient.h"

extern void MotionSensorInd();
extern int checkPingStatus(int packets, char *pszSite);

#ifdef USE_SEEDONK_CLOUD_FOR_NETCAM_NAME
static void LockFriendlyName();
static void UnlockFriendlyName();
#endif

static pthread_t g_stMotionThread = -1; /*!< The motion detection thread */
static pthread_t g_stNTPThread = -1;    /*!< the NTP thread handler*/

static pthread_t g_stNetworkWatcherThread = -1; /*!< The Network watcher thread */

#define NEXT_NTP_TIME   60*60*6 //- 6 hours
#define MAX_NETWORK_INQUIRY_TIMEOUT 20 /*!< 2 second per suggestion from Ralink */
#define MAX_ROUTER_CONNECTION_INQUIRY_TIMEOUT 3

//- To compatible with Gemtek SNS
static unsigned long    s_lLastSensoringUpdateTime              = 0x00;
static unsigned long    s_lLastNoSensoringUpdateTime    = 0x00;
#define                                 MAX_SENSOR_MSG_TIMEOUT          20      // 20 seconds

extern char g_szApSSID[64];
extern char g_szFriendlyName[180];

extern int gNTPTimeSet;
extern int computeDstToggleTime(int updateDstime);

#ifdef USE_SEEDONK_CLOUD_FOR_NETCAM_NAME
static pthread_t g_sAsyncFriendlyNameThread = -1;   /*!< the handler for friendlyName request toward */
static int  g_sIsFriendlyNameReadable       = 0x01; /*!< the flag for friendlyName request*/

static pthread_mutex_t g_sFriendlyNameSem     = PTHREAD_MUTEX_INITIALIZER;
#endif

static pthread_t g_sNetCamDNSThread = -1; /*!< The handler for task of DNS*/

static pthread_t g_sMonitorCameraNameThread = -1;

static char g_sCurrentTZ[128] = {0x00};

//- Try to use semaphore to reduce overload of the read from Seedonk server

//static sem_t g_sNetCamSem;

void InitTimeZoneInfo()
{
		char* szTZ= GetBelkinParameter("TZ");

		if ((0x00 != szTZ) &&
						(0x00 != strlen(szTZ)))
		{
				strncpy(g_sCurrentTZ, szTZ, 128);
		}

}

#ifdef USE_SEEDONK_CLOUD_FOR_NETCAM_NAME
void InitNetCamNameSession()
{
		sem_init(&g_sNetCamSem, 0, 0);
}


void ReInitNetCamNameSession()
{
		sem_destroy(&g_sNetCamSem);
		sem_init(&g_sNetCamSem, 0, 0);
}

static void LockFriendlyName()
{
		pthread_mutex_lock(&g_sFriendlyNameSem);
}

static void UnlockFriendlyName()
{
		pthread_mutex_unlock(&g_sFriendlyNameSem);
}

static int GetFriendlyNameStatus()
{
		int rect = 0x00;
		LockFriendlyName();
		rect = g_sIsFriendlyNameReadable;
		UnlockFriendlyName();

		return rect;
}

static int SetFriendlyNameStatus(int newState)
{
		int rect = 0x00;
		LockFriendlyName();
		g_sIsFriendlyNameReadable = newState;
		rect = g_sIsFriendlyNameReadable;
		printf("NetCam: name readable state:%d\n", newState);
		UnlockFriendlyName();

		return rect;
}

/**
 *  @brief  This friendly name task created each time when read local friendlyname, in case
 *          other phone change it and not informed to wemoApp
 *
 *
 *
 */
void* NetCamGetFriendlyNameThreadTask()
{
		tu_set_my_thread_name( __FUNCTION__ );
		printf("NetCam: FriendlyName async request task running\n");
		while (1)
		{
				sem_wait(&g_sNetCamSem);
				SetNetCamProfileApSSID();
				ReInitNetCamNameSession();
		}
}
#endif

/**
 *  @brief the thread to watch network status
 *      g_stNetworkWatcherThread
 *
 *
 *
 */
void* NetworkWatcherThreadTask()
{
		tu_set_my_thread_name( __FUNCTION__ );
		printf("NetCam: network watcher thread task running\n");
		//- start inquiry until 60 seconds so that boots-up done
		sleep(60);
		while (1)
		{
				/* update signal strength periodically, WeMo mobile App may need it */		  
				chksignalstrength();
				if (!IsLocalRouterConnected())
				{
						printf("Router not connected at all, system quit and will be reloaded\n");
						exit(01);
				}
				else
				{
						sleep(MAX_NETWORK_INQUIRY_TIMEOUT);
				}
		}
}

void RunNetworkWatcherTask()
{
		int rect = pthread_create(&g_stNetworkWatcherThread, 0x00, (void *)NetworkWatcherThreadTask, 0x00);

		if (0x00 == rect)
		{
				//- success
				pthread_detach(g_stNetworkWatcherThread);
		}
		else
		{
				printf("IPC: Network watcher thread creation failed\n");
		}
}

/**
 *  The NTP thread
 *
 *
 */
void* NTPMainThreadTask()
{
		tu_set_my_thread_name( __FUNCTION__ );
		printf("NTP: NTP thread running\n");
		sleep(500);
		while (1)
		{
				int rect = system("/sbin/ntp.sh&");

				if (0x00 == rect)
				{
						printf("NTP updated success\n");
						/*
						 * To set the Network status to internet connected so that remote access than can start work;
						 * If time not sync correctly, remote access should never attempt at all
						 */
						SetCurrentClientState(STATE_CONNECTED);
						//- wait about 6 hours to sync again. this 6 hours comes from Network team suggestion
						gNTPTimeSet = 1;
						computeDstToggleTime(1);
						//InitNetCamTimeZoneAndDST();
						sleep(NEXT_NTP_TIME);       /*Wait another 6 hours per suggestion from Network*/
				}
				else
				{
						//- Failed, sleep 10 seconds to try it again
						sleep(60);
				}
		}
}

/**
 *  @brief The motion thread
 *
 *
 *
 */
void* MotionMainThreadTask()
{
		tu_set_my_thread_name( __FUNCTION__ );
		printf("IPC: motion thread running\n");
		initWeMoNetCam(DetectMotionIF);
		return NULL;
}

/**
 *  @brief Callback for motion detection, which
 *      registered to IPC server
 *
 *
 *
 */
#define MAX_MOTION_TIMEOUT 6 //-4 seconds
int DetectMotionIF(int data)
		//int DetectMotionIF(char* data)
{
		struct timeval tv;
		time_t curTime;
		gettimeofday(&tv, NULL);
		curTime = tv.tv_sec;

		DDBG( "DetectMotionIF(%d)\n", data );
		if (0x01 == data)
		{
				if ((curTime - s_lLastSensoringUpdateTime) >= MAX_SENSOR_MSG_TIMEOUT)
				{
						//printf("IPC: Motion event received\n");
						SetSensorStatus(0x01);
						MotionSensorInd();
						s_lLastSensoringUpdateTime            = curTime;
						s_lLastNoSensoringUpdateTime  = 0x00;
				}
		}
		else
		{
				//printf("IPC: No motion event received\n");
				if ((curTime - s_lLastNoSensoringUpdateTime) >= MAX_SENSOR_MSG_TIMEOUT)
				{
						//printf("IPC: No motion event received and notified\n");
						SetSensorStatus(0x00);
						NoMotionSensorInd();
						s_lLastNoSensoringUpdateTime = curTime;
						s_lLastSensoringUpdateTime     = 0x00;
				}
		}
		return 0x00;
}

/**
 * The task to start the Motion detection
 *
 */
void StartNetCamSensorTask()
{
		int rect = pthread_create(&g_stMotionThread, 0x00, (void *)MotionMainThreadTask, 0x00);

		DDBG( "StartNetCamSensorTask()\n" );
		if (0x00 == rect)
		{
				//- success
				pthread_detach(g_stMotionThread);
		}
		else
		{
				printf("IPC: motion thread creation failure\n");
		}
}


void StartNTPTask()
{
		if (-1 != g_stNTPThread)
		{
				pthread_cancel(g_stNTPThread);
		}

		int rect = pthread_create(&g_stNTPThread, 0x00, (void *)NTPMainThreadTask, 0x00);

		if (0x00 == rect)
		{
				//- success
				pthread_detach(g_stNTPThread);
		}
		else
		{
				printf("IPC: NTP thread creation failure\n");
		}
}

/**
 *  @brief The check the local router connected, if ESSID not present, that means disconnected
 *      iwpriv apcli0 show connStatus
 *      NOTE: For some reason, the ioctl for IOCTL_SHOW not working, working with Ralink
 *
 *
 *
 *  @return int if Connected then return
 */
#define MIN_SSID_LEN            6               //- 5 if SSID not present
#define MAX_PING_NUMBER         3               //- ping re-try times
int IsLocalRouterConnected()
{
		int rect = 0x00;      //- Assume it is connected

		char *pGateway = GetWanDefaultGateway();

		if ((0x00 == pGateway) || (0x00 == strlen(pGateway)))
		{
				rect = 0x00;
				return rect;
		}

		/* 0.0.0.0 isn't a valid gateway address */
		if (0 == strcmp(pGateway, "0.0.0.0")) {
				rect = 0x00;
				return rect;
		}

		char szCommand[256] = {0x00};
		sprintf(szCommand, "ping -c 3 %s>/dev/null", pGateway);

		/**
		 *    Run 5 times so that system can break and then continue
		 */
		int retry_times = 0x00;
		do
		{
				int ping_resp = system(szCommand);
				if (0x00 == ping_resp)
				{
						rect = 0x01;
						break;
				}
#if 0
				printf("pGateway is %s", pGateway)

				    if( 100 != checkPingStatus(3, pGateway) )
				    {
					printf("checkPingStatus(%s) is alived...", pGateway);
					rect = 0x01;
					break;
				    }
				    else
				    {
					printf("checkPingStatus(%s) is dead...", pGateway);
				    }
#endif
				retry_times++;
				sleep(1);
		} while (retry_times < MAX_PING_NUMBER);


		if(MAX_PING_NUMBER == retry_times)
		{
				printf("###################### Network disconnected ########################\n");
				rect = 0x00;
		}

		return rect;
}

/**
 *  @brief To check network status by check
 *      - Local connection, then
 *      - IP address
 *      - then public internet connection, could from NTP update, NTP update should
 *          have priority since even cloud connected, without time sync, still failure
 *
 */
int NetCamCheckNeworkState()
{
		int rect = 0xFF;

		while (1)
		{

				if (IsLocalRouterConnected())
				{
						/* Make a force check again, otherwise wait
						 */
						printf("Network: network is connected");
						rect = 0x00;
						break;
				}
				else
				{
						printf("Network: invalid network information (router not connected yet), wait until next loop\n");
						sleep(MAX_ROUTER_CONNECTION_INQUIRY_TIMEOUT);
				}

		}

		return rect;
}

#define MD5_CONST_KEY "s4P+cs&qiv-u-15hkdy5t0("

/**
 *  Get MD5 string and then access Seedonk cloud
 *
 *
 *
 */
char* GenerateMD5Str(const char* srcSeed, char* destBuff)
{
	if (0x00 == srcSeed || 0x00 == strlen(srcSeed) || 0x00 == destBuff)
		return 0x00;    //- NULL back

	unsigned char szRawTmpKey[128] = {0x00};
	char szTmp[2] = {0x00};
	int i = 0x00;

	MD5(srcSeed, strlen(srcSeed), szRawTmpKey);

	for (; i < MD5_DIGEST_LENGTH; i++)
	{
		memset(szTmp, 0x00, 2);
		sprintf(szTmp, "%02x", szRawTmpKey[i]);
		strcat(destBuff, szTmp);
	}

	//printf("NetCam:MD5:seed:%s key:%s\n", srcSeed, destBuff);

	return destBuff;
}

/**
 *  Get Seedonk defined key
 *
 *
 *
 */
char* GetNetCamMD5Key(const char* szLoginName, const char* szMacAddress, char* srcKey)
{
	//- TODO: to check the data validation here

	//unsigned char szRawKey[64] = {0x00};

	GenerateMD5Str(MD5_CONST_KEY, srcKey);

	char szTmpSeed[256] = {0x00};
	strncat(szTmpSeed, szLoginName, 16);
	strncat(szTmpSeed, szMacAddress, 18);
	strcat(szTmpSeed, srcKey);
	memset(srcKey, 0x00, sizeof(srcKey));
	GenerateMD5Str(szTmpSeed, srcKey);

	return srcKey;
}

#ifdef USE_SEEDONK_CLOUD_FOR_NETCAM_NAME
static char g_sNetCamReadName[64];
size_t cbHttpsCallback(void *ptr, size_t size, size_t count, void *stream)
{
		printf("response: %s\n", (char*) ptr);
		if ((ptr) && (count > 0x00))
		{
				strncpy(g_sNetCamReadName, (char*)ptr, 64);
		}
		return (count);
}

/*
 *  ExecuteHttpsGet
 *      Make a https request for Seedonk CGI
 *
 */
char* ExecuteHttpsGet(const char* destURL, char* szResponse)
{
		CURL *curl;
		CURLcode res;
		curl_global_init(CURL_GLOBAL_DEFAULT);
		curl = curl_easy_init();
		if(curl)
		{
				printf("LIBCURL: %s\n", destURL);
				curl_easy_setopt(curl, CURLOPT_URL, destURL);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cbHttpsCallback);

				/* Perform the request, res will get the return code */
				res = curl_easy_perform(curl);
				/* Check for errors */
				if(res != CURLE_OK)
				{
						fprintf(stderr, "curl_easy_perform() failed: %s\n",
										curl_easy_strerror(res));
				}
				else
				{
						fprintf(stderr, "curl_easy_perform() with success\n");
				}

				/* always cleanup */
				curl_easy_cleanup(curl);
		}
		else
		{
				fprintf(stderr, "can not get CURL resource");
		}

		curl_global_cleanup();

		return g_sNetCamReadName;

}

/**
 *  GetNetCamNameFromCloudEx
 *      interim function call to get NetCam name from Seedon cloud
 *
 */

#define MAX_FM_TIMEOUT 20
char* GetNetCamNameFromCloudEx(const char* szLogName, const char* szMacAddress, char* destBuff)
{
		char szAuthKey[128] = {0x00};

		char* strRet = GetNetCamMD5Key(szLogName, szMacAddress, szAuthKey);

		if ((0x00 == strRet) || (0x00 == strlen(strRet)))
		{
				printf("NetCam:MD5: can not get MD5 key\n");
				return 0x00;
		}

		//printf("NetCam:MD5: final key:%s\n", szAuthKey);


		//- Construct URL: e.g. "https://www.seedonk.com/camsetup/retrievecamera.php?login=mingtest&mac=D4:12:BB:01:10:8C&key=b0f036dd66d37449819e6dd183b4785"
		char* szNetCamURL[512] = {0x00};
		sprintf(szNetCamURL, "%slogin=%s&mac=%s&key=%s", NETCAM_URL, szLogName, szMacAddress, szAuthKey);

		//printf("NetCam: GetName:URL:%s\n", szNetCamURL);

		//- clean it up
		memset(g_sNetCamReadName, 0x00, sizeof(g_sNetCamReadName));

		CURL *curl;
		CURLcode res;
		curl_global_init(CURL_GLOBAL_DEFAULT);
		curl = curl_easy_init();
		if(curl)
		{
				printf("LIBCURL: %s\n", szNetCamURL);
				char certfile[32] = "ca-certificates.crt";
				char certloc[32] = "../sbin";
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
				curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
				curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, MAX_FM_TIMEOUT);
				curl_easy_setopt(curl, CURLOPT_TIMEOUT, MAX_FM_TIMEOUT);
				curl_easy_setopt(curl, CURLOPT_CERTINFO, 0L);
				curl_easy_setopt(curl, CURLOPT_CAINFO, certfile);
				curl_easy_setopt(curl, CURLOPT_CAPATH, certloc);
				curl_easy_setopt(curl, CURLOPT_URL, szNetCamURL);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cbHttpsCallback);

				/* Perform the request, res will get the return code */
				res = curl_easy_perform(curl);
				/* Check for errors */
				if(res != CURLE_OK)
				{
						fprintf(stderr, "curl_easy_perform() failed: %s\n",
										curl_easy_strerror(res));
				}
				else
				{
						fprintf(stderr, "curl_easy_perform() with success\n");
				}

				/* always cleanup */
				curl_easy_cleanup(curl);
		}
		else
		{
				fprintf(stderr, "can not get CURL resource");
		}

		curl_global_cleanup();
		strcpy(destBuff, g_sNetCamReadName);
		return g_sNetCamReadName;

}
#endif



/*
 * Create a fake friendly name for times when the camera's actual user
 * selected name cannot be determined.  This is a combination of the
 * string NETCAM_NAME_PREFIX, the last 4 digits of the MAC address and
 * the suffix " Motion".
 * @param buffer The receiver of the create name
 * @param buffer_size Size of buffer.  Name will be clipped to fit if needed
 */
void makeFakeFriendlyName( char *buffer, size_t buffer_size ) {
		char *mac_digits = GetMACAddress();
		/* the MAC number is stored as an ASCII string, 6 pairs of digits
			 with ":"'s in between like so:
			 1111111
			 01234567890123456
xx:xx:xx:xx:xx:xx
So for our purposes we want characters 12, 13, 15 & 16 */
		char digits[5] = { mac_digits[12], mac_digits[13], 
				mac_digits[15], mac_digits[16], '\0' };
		snprintf( buffer, buffer_size, "%s%s %s", NETCAM_NAME_PREFIX, digits, "Motion" );
}

/* ---------------------------------------------------------------------------
 * Extracts name and login from friendly name NVRAM variable
 * It's in the format of <name>;LoginName=<login name>
 *
 * INPUT
 *     buf - text buffer to hold friendly name
 *     bufsize - size of buffer
 * OUTPUT
 *     camName - camera name (points to memory location in buf, or null)
 *     loginName - login name (points to memory location in buf, or null)
 * RETURN
 *     PARSE_FAIL - failed to parse 
 *     PARSE_SUCCESS - got both camera name and login name
 *     PRASE_DEPRECATED - old style deprecated format
 *     PARSE_CAMNAME_ONLY - got only camera name but no login name
 * --------------------------------------------------------------------------*/
#define PARSE_FAIL         (0)
#define PARSE_SUCCESS      (1)
#define PARSE_DEPRECATED   (2)
#define PARSE_CAMNAME_ONLY (3)
static int ParseFriendlyName(char *buf, int bufsize,
				char **camName, char **loginName)
{
		char *value;
		regex_t re;
		regmatch_t m[3];
		int rc;

		*camName = NULL;
		*loginName = NULL;

		/* get friendly name from NVRAM */
		value = GetBelkinParameter(FRIENDLY_NAME_VAR);
		if (value)
		{
				strncpy(buf, value, bufsize);
		}
		else
		{
				DDBG("FriendlyName not found in NVRAM");
				return PARSE_FAIL;
		}

		/* parse friendly name <camera name>;LoginName=<login name> */
		rc = regcomp(&re, "^(.*);LoginName=(.*)$", REG_EXTENDED);
		if (rc != 0)
		{
				DDBG("Cannot parse FriendlyName: regcomp failed");
				return PARSE_FAIL;
		}

		rc = regexec(&re, buf, 3, m, 0);
		regfree(&re);

		if (rc == 0)
		{
				*camName = &buf[m[1].rm_so];
				buf[m[1].rm_eo] = '\0';
				*loginName = &buf[m[2].rm_so];
				buf[m[2].rm_eo] = '\0';
				return PARSE_SUCCESS;
		}

		/* check for deprecated format <camera name>,<login name> */
		rc = regcomp(&re, "^(.*),(.*)$", REG_EXTENDED);
		if (rc != 0)
		{
				DDBG("Cannot parse FriendlyName: regcomp failed");
				return PARSE_FAIL;
		}

		rc = regexec(&re, buf, 3, m, 0);
		regfree(&re);

		if (rc == 0)
		{
				DDBG("FriendlyName: has deprecated format");
				*camName = &buf[m[1].rm_so];
				buf[m[1].rm_eo] = '\0';
				*loginName = &buf[m[2].rm_so];
				buf[m[2].rm_eo] = '\0';
				return PARSE_DEPRECATED;
		}

		/* assume FriendlyName is set to camera name only */
		DDBG("FriendlyName has invalid format - %s", buf);
		/* in this case we return the whole string as camera name */
		*camName = buf;
		return PARSE_CAMNAME_ONLY;
}

/* ---------------------------------------------------------------------------
 * Contstructs FriendlyName NVRAM variable, and write to flash
 *
 * INPUT
 *     camName - camera name 
 *     loginName - login name (login name can be NULL)
 * --------------------------------------------------------------------------*/
static void ConstructFriendlyName(char *camName, char *loginName)
{
		char buf[256];


		/* format is <camera name>;LoginName=<login name> */
		if (loginName)
		{
				snprintf(buf, sizeof(buf), "%s;LoginName=%s", camName, loginName);
		}
		else
		{
				/* login name not found */
				snprintf(buf, sizeof(buf), "%s;LoginName=", camName);
		}
		buf[sizeof(buf) - 1] = '\0';

		/* save it */
		SetBelkinParameter(FRIENDLY_NAME_VAR, buf);
		SaveSetting();
}

extern char* GetBelkinParameter(char* ParameterName);
char g_szNetCamLogName[128];
char g_szMyDeviceMac[64];

char* SetNetCamProfileApSSID()
{
#ifdef USE_SEEDONK_CLOUD_FOR_NETCAM_NAME
		char szCamName[256] = {0x00};
		char *strtok_r_temp;

		printf("NetCam: %s, Login:%s, mac:%s\n", __FUNCTION__, g_szNetCamLogName, g_szMyDeviceMac);
		GetNetCamNameFromCloudEx(g_szNetCamLogName, g_szMyDeviceMac, szCamName);
		if (0x00 != strlen(szCamName))
		{
				printf("NetCam:GetName: response from NetCam server:%s\n", g_sNetCamReadName);
				char* pData = strtok_r(szCamName, ":",&strtok_r_temp);
				if (pData)
				{
						pData = strtok_r(0x00, ":",&strtok_r_temp);
						if (pData)
						{
								pData = pData + 1; //- Move one to delete "
								if (strlen(pData) > 0x02)
								{
										char szShortName[64] = {0x00};
										int len = strlen(pData);
										pData[len - 3]  = 0x00;       //- Delete last two
										strncpy(szShortName, pData, 64);
										if (0x00 == strlen(szShortName))
										{
												//- Short name is empty due to timeout OR wrong request
												makeFakeFriendlyName( szShortName, sizeof( szShortName ));
										}

										//- Quick resolution NOW, TODO: to optimize the storage
										char szJointName[256] = {0x00};
										strcpy(szJointName, szShortName);
										strcat(szJointName, ",");
										strcat(szJointName, g_szNetCamLogName);
										if (0x00 != strcmp(szJointName, g_szFriendlyName))
										{
												printf("NetCam: login name changed, now it is: %s\n", szJointName);
												SetBelkinParameter("FriendlyName", szJointName);
												strncpy(g_szFriendlyName, szJointName, 180);
												SaveSetting();
										}
								}
						}
				}
				else
				{
						printf("NetCam: read name from server error\n");
				}
		}
		else
		{
				printf("NetCam: GetName: Holly ##### no netcam received for login name:%s", g_sNetCamReadName);
		}
#else
		char cameraName[256], nvramVal[256];
		char *value, *camName, *loginName;

		printf("NetCam: %s, Login:%s, mac:%s\n", __FUNCTION__, g_szNetCamLogName, g_szMyDeviceMac);

		/* Newer firmware uses CameraName as friendly name */
		/* FriendlyName is still necessary for some of the older code */
		value = GetBelkinParameter(CAMERA_NAME_VAR);
		if (value)
		{
				strncpy(cameraName, value, sizeof(cameraName));
		}
		else
		{
				cameraName[0] = '\0';
		}

		if(*cameraName)
		{
				/* set the global to camera name */
				DDBG("camera name: \"%s\"", cameraName);
				strncpy(g_szFriendlyName, cameraName, sizeof(g_szFriendlyName));
				SetBelkinParameter(FRIENDLY_NAME_VAR, g_szFriendlyName);
				SaveSetting();
		}
		else
		{
				/* Firmware upgrade from older firmware may have the deprecated
					 friendly name set instead. If we don't find camera name,
					 we use friendly name for backward compatibility */
				DDBG( "camera name: not found in nvram, using friendly name" );

				/* update global and CameraName with this info */
				SetBelkinParameter(CAMERA_NAME_VAR, g_szFriendlyName);
				SaveSetting();
		}
#endif
		return NULL;
}

/**
 * GetNetCamNameFromCloud
 *  Get NetCam from NetCam cloud/seedonk
 *  Please note that the key is calculated base on device information
 *
 */
char* GetNetCamNameFromCloud(const char* szLogName, const char* szMacAddress, char* destBuff)
{
		char szAuthKey[128] = {0x00};

		char* strRet = GetNetCamMD5Key(szLogName, szMacAddress, szAuthKey);

		if ((0x00 == strRet) || (0x00 == strlen(strRet)))
		{
				printf("NetCam:MD5: can not get MD5 key\n");
				return 0x00;
		}

		//printf("NetCam:MD5: final key:%s\n", szAuthKey);


		//- Construct URL: e.g. "https://www.seedonk.com/camsetup/retrievecamera.php?login=mingtest&mac=D4:12:BB:01:10:8C&key=b0f036dd66d37449819e6dd183b4785"
		char szNetCamURL[512] = {0x00};
		sprintf(szNetCamURL, "%slogin=%s&mac=%s&key=%s", NETCAM_URL, szLogName, szMacAddress, szAuthKey);

		//printf("NetCam: GetName:URL:%s\n", szNetCamURL);


		UserAppSessionData *pUsrAppSsnData = NULL;
		UserAppData *pUsrAppData = NULL;

		pUsrAppData = (UserAppData *)malloc(sizeof(UserAppData));

		if (0x00 == pUsrAppData)
		{
				printf("NetCam:GetName: can not allocate memory\n");
				return 0x00;
		}

		memset(pUsrAppData, 0x0, sizeof(UserAppData));

		pUsrAppSsnData = webAppCreateSession(0);

		if (0x00 == pUsrAppSsnData)
		{
				printf("NetCam:GetName: pUsrAppSsnData: memory allocation failure\n");
				return 0x00;
		}

		strcpy(pUsrAppData->url, szNetCamURL);

		strcat(pUsrAppData->url,"registerPlugin");
		strcpy(pUsrAppData->keyVal[0].key, "Content-Type");
		strcpy(pUsrAppData->keyVal[0].value, "application/xml");
		strcpy(pUsrAppData->keyVal[1].key, "Accept");
		strcpy(pUsrAppData->keyVal[1].value, "application/xml");
		pUsrAppData->keyValLen = 0x02;

		pUsrAppData->httpsFlag = 1;
		pUsrAppData->disableFlag = 0x00;

		//- GET
		int wret = webAppSendData(pUsrAppSsnData, pUsrAppData, 0x00);

		if (wret)
		{
				printf("NetCam:GetName: webAppSendData executed failure: RESP: %d\n", wret);
				return 0x00;
		}
		printf("NetCam:GetName:%s, size:%d, header: %s\n", pUsrAppData->outData, pUsrAppData->outDataLength, pUsrAppData->outHeader);

		if (pUsrAppSsnData)
		{
				webAppDestroySession(pUsrAppSsnData);
				pUsrAppSsnData = 0x00;
		}

		free(pUsrAppData);

		return 0x00;
}

/*  #define WIFI_CLIENT_SSID            "ClientSSID"
#define WIFI_CLIENT_PASS            "ClientPass"
#define WIFI_CLIENT_AUTH            "ClientAuth"
#define WIFI_CLIENT_ENCRYP          "ClientEncryp"
#define WIFI_AP_CHAN                        "APChannel"
#define WIFI_ROUTER_MAC                     "RouterMac"
#define WIFI_ROUTER_SSID            "RouterSsid"
#define SYNCTIME_LASTTIMEZONE           "LastTimeZone"
#define SYNCTIME_DSTSUPPORT         "DstSupportFlag"
#define LASTDSTENABLE                       "LastDstEnable"
#define NOTIFICATION_VALUE          "NotificationFlag"
 */

int IsHomeRouterHotChanged()
{
		/** Get router SSID and mac address from NVRAM */

		/** Get Router SSID from WiFi driver that is current */


		char* szOldSSID = GetBelkinParameter("RouterSsid");
		char* szOldMac  = GetBelkinParameter("RouterMac");

		char routerssid[64] = {0x00};
		char routerMac[20]  = {0x00};
		char* szResetFlag = GetBelkinParameter("restore_state");
		int IsChanged = 0x00;

		if ((0x00 != szResetFlag) && (0x00 != strlen(szResetFlag)))
		{
				int cntFlag = atoi(szResetFlag);
				if (0x01 == cntFlag)
				{
						printf("Reset: NetCam once reset from NetCam, will be de-register from Cloud\n");
						IsChanged = 0x01;
				}
		}

		return IsChanged;

		getRouterEssidMac(routerssid, routerMac, INTERFACE_CLIENT);


		/**Router changed*/
		if ((0x00 != szOldSSID) &&
						(0x00 != szOldMac))
		{
				if ( (0x00 != strcmp(szOldSSID, routerssid)) &&
								(0x00 != strcmp(szOldMac, routerMac))
					 )
				{
						printf("NetCam: router changed, restore all information\n");
						IsChanged = 0x01;
				}
		}
		else
		{
				printf("NetCam: router changed from empty, restore all information\n");
				IsChanged = 0x01;

		}

		SetBelkinParameter("RouterSsid", routerssid);
		SetBelkinParameter("RouterMac", routerMac);

		SetRouterInfo(routerMac, routerssid);

		return IsChanged;
}

void StartNetCamNameRequestTask()
{
#ifdef USE_SEEDONK_CLOUD_FOR_NETCAM_NAME
		int rect = pthread_create(&g_sAsyncFriendlyNameThread, 0x00, (void *)NetCamGetFriendlyNameThreadTask, 0x00);

		if (0x00 == rect)
		{
				//- success
				pthread_detach(g_sAsyncFriendlyNameThread);
		}
		else
		{
				printf("NetCam: Friendly name request thread creation failure\n");
		}
#endif
}

/**
 *  @ brief Please see note in header file
 */
void AsyncRequestNetCamFriendlyName()
{
#ifdef USE_SEEDONK_CLOUD_FOR_NETCAM_NAME
		sem_post(&g_sNetCamSem);
		printf("################### NetCam name request event sent #####################\n");
#endif
}

extern void invokeNatReInit();
extern int resetFlashRemoteInfo(pluginRegisterInfo*);
extern int setRemoteRestoreParam (int);

void RestoreNetCam()
{
		char name_buffer[20];
		makeFakeFriendlyName( name_buffer, sizeof( name_buffer ));
		char *homeId = GetBelkinParameter("home_id");
		char *pluginKey = GetBelkinParameter("plugin_key");
		if ((homeId && pluginKey) && (0x00 == strlen(homeId) ) && (0x00 == strlen(pluginKey))) 
		{
		    printf("Remote Access is not Enabled, unset everything");
		    resetFlashRemoteInfo(NULL);
		    SetBelkinParameter("RouterMac", "");
		}
		else
		{
		    printf("Remote Access was Enabled, keep home_id, plugin_key and set restore_state 1");
		    setRemoteRestoreParam (0x1);
		}
		SetBelkinParameter("FriendlyName", name_buffer);
		system("rm -f /tmp/Belkin_settings/rule.txt");
		system("rm -f /tmp/Belkin_settings/rules.db");
		system("rm -f /tmp/Belkin_settings/*");
		SetBelkinParameter("RouterSsid", "");
		SaveSetting();

		//invokeNatReInit();

		printf("NetCam: restore called\n");

}


void ProcessNetCamTimeZoneEvent(char* szEvent)
{
		if (IsTimeZoneSetupByNetCam())
		{
				//- Time zone set already from NetCam, push ignored
				printf("Netcam: Time zone sync ingnored since NetCam already set it up\n");
				return;
		}

		int index = 0x00;

		if ((0x00 == szEvent) ||
						(0x00 == strlen(szEvent)))
		{

				printf("Netcam: Time zone sync push notification wrong\n");
				return;
		}

		printf("NetCam: time zone sync push notifcaiton received: %s\n", szEvent);

		char* oldIndex = GetBelkinParameter("timezone_index");
		if ((0x00 == oldIndex) || (0x00 == strlen(oldIndex)) )
		{
				//- Sync it
				//index = atoi(szEvent);
				sscanf(szEvent, "%d", &index);
				printf("NetCam: to set NTP time zone index to: %d\n", index);
				SetNTP(0x00, index, 0x00);
				StartNTPTask();
		}
		else
		{
				//- sync from phone already, so do nothing
		}

}



void SetNetCamName()
{
		char* szTmpName = GetBelkinParameter("FriendlyName");
		if (0x00 == szTmpName || 0x00 == strlen(szTmpName))
		{
				//- The Friendly Name is empty, so using the default one
				char szJointName[256] = {0x00};
				makeFakeFriendlyName( szJointName, sizeof( szJointName ));
				SetBelkinParameter("FriendlyName", szJointName);

				//- Not really necessary here to save the login name here?
		}
		else
		{
				//- Not empty? how to deal wit this situation?

		}

}

void InitNetCamProfile()
{
		char szApSSID[64] = {0x00};
		char szTmpMac[64] = {0x00};
		char *szApMac;
		char nvramVal[256], loginNameBuf[256], *camName, *loginName, *newloginName;
		char defaultCamName[256];

                camName = GetBelkinParameter(CAMERA_NAME_VAR);
                if (strlen(camName))  /* set CameraName as FriendlyName */
                {
				DDBG("camera name: \"%s\"", camName);
				strncpy(g_szFriendlyName, camName, sizeof(g_szFriendlyName)-1);
                }
                else
                {
				camName = GetBelkinParameter(FRIENDLY_NAME_VAR);
				if (strlen(camName) == 0) {
						makeFakeFriendlyName(defaultCamName, sizeof(defaultCamName));
						camName = defaultCamName;
				}
		}		  
		
		loginName = GetBelkinParameter(SEEDONK_LOGIN_NAME_VAR);

		/* set global cam name & login name */
		strncpy(g_szFriendlyName, camName, sizeof(g_szFriendlyName));
		SetBelkinParameter(FRIENDLY_NAME_VAR, g_szFriendlyName);
		SaveSetting();
#ifdef NETCAM_LS
		char *alt_loginname;
		alt_loginname = GetBelkinParameter("SeedonkOwnerMail");
		if (alt_loginname) {
				strncpy(g_szNetCamLogName, alt_loginname, sizeof(g_szNetCamLogName));
				alt_loginname = NULL;
		}
#else
		strncpy(g_szNetCamLogName, loginName, sizeof(g_szNetCamLogName));
#endif

		/* init other information */
		getRouterEssidMac(szApSSID, szTmpMac, INTERFACE_AP);
		if (strlen(szApSSID) != 0)
		{
				strncpy(g_szApSSID, szApSSID, sizeof(g_szApSSID));
		}

		szApMac = GetMACAddress();
		if (strlen(szApMac) != 0)
		{
				strcpy(g_szMyDeviceMac, szApMac);
				printf("********************** ra0 MAC address is: %s\n", g_szMyDeviceMac);
		}
}



/**
 *  @brief The motion thread
 *
 *
 *
 */
#define MAX_DNS_RETRY_TIMES 2
#define BELKIN_DOMAIN_NAME "www.belkin.com"
#define ROOTSERVER_DOMAIN_NAME "A.root-servers.net"

/**
 *  \ @function: NetCamDNSThreadTask
 *
 *  \ @brief: the task to watch network internet status, if connected, DNS each 5 minutes
 *            considering the motion detection and vedio streaming overload is high from NetCam
 *            as well, when not connected, DNS each one minutes
 *
 *
 *  \ @return void*
 ****************************************************************************/
#define MAX_CONNECTED_DNS_TIMEOUT           300 /*!< 5 minutes, 300 seconds*/
#define MAX_DISCONNECTED_DNS_TIMEOUT        60 /*!< 1 minutes, 60 seconds*/

void* NetCamDNSThreadTask()
{
		tu_set_my_thread_name( __FUNCTION__ );
		printf("NetCam: DNS task running\n");
		while (1)
		{
				char ipArr[10][64];
				int num=0;
				int rect = 0x01;
				int loop;
				while (loop < MAX_DNS_RETRY_TIMES)
				{
						remoteParseDomainLkup(BELKIN_DOMAIN_NAME, ipArr, &num);
						if(num > 0)
						{
								rect = 0x00;
								break;
						}
						loop++;
						sleep(1);
				}

				if (0x00 != rect)
				{
						loop = 0x00;
						//- Do another DNS again
						num = 0x00;
						while (loop < MAX_DNS_RETRY_TIMES)
						{
								remoteParseDomainLkup(ROOTSERVER_DOMAIN_NAME, ipArr, &num);
								if(num > 0)
								{
										rect = 0x00;
										break;
								}

								loop++;
								sleep(1);
						}
				}

				if (0x00 != rect)
				{
						// Internet not connected
						SetCurrentClientState(STATE_INTERNET_NOT_CONNECTED);
						printf("NetCam:DNS: Internet not connected, check in 60 seconds again\n");
						sleep(MAX_DISCONNECTED_DNS_TIMEOUT);
				}
				else
				{
						SetCurrentClientState(STATE_CONNECTED);
						sleep(MAX_CONNECTED_DNS_TIMEOUT);
				}
		}
}


void StartDNSTask()
{

		int rect = pthread_create(&g_sNetCamDNSThread, 0x00, (void *)NetCamDNSThreadTask, 0x00);

		if (0x00 == rect)
		{
				//- success
				pthread_detach(g_sNetCamDNSThread);
		}
		else
		{
				printf("NetCam: DNS task created with failure\n");
		}

}


int IsTimeZoneSetupByNetCam()
{
		int rect = 0x01;

		char* szNetCamTimeZoneFlag = GetBelkinParameter(NETCAM_TIME_ZONE_KEY);
		if (
						(0x00 == szNetCamTimeZoneFlag) ||
						(0x00 == strlen(szNetCamTimeZoneFlag)) ||
						(0x00 == strcmp(szNetCamTimeZoneFlag, "0"))
			 )
		{
				//- Not set yet
				rect = 0x00;
		}

		return rect;
}


#include <time.h>
extern char *tzname[];

struct _short_tz
{
		char            szShortName[64];
		char            szLocation[64];
		char            szShortLocaltion[64];
		float           ftTimeZoneValue;
		int                            IsDSTSupport;

};

typedef struct _short_tz tTimeZone;


tTimeZone g_sLstAllTimeZone[] = {
		{"A",       "Alpha Time Zone",                                "Military",             1.0, 0},
		{"ACDT",    "Australian Central Daylight Time",       "Australia",                                            10.50, 1},
		{"ACST",    "Australian Central Standard Time",       "Australia",                                            9.50, 1},
		{"ADT",     "Atlantic Daylight Time",                 "Atlantic",                                             -3.0, 0},
		{"ADT",     "Atlantic Daylight Time",   "North America",      -3.0, 0},
		{"AEDT",    "Australian Eastern Daylight Time",       "Australia",            11.0, 1},
		{"AEST",    "Australian Eastern Standard Time",       "Australia",            10.0, 1},
		{"AFT",     "Afghanistan Time",               "Asia",         4.30, 0},
		{"AKDT",    "Alaska Daylight Time",                                   "North America",        -8.0, 1},
		{"AKST",    "Alaska Standard Time",     "North America",                                      -9.0, 1},
		{"ALMT" ,   "Alma-Ata Time",                                          "Asia",         6.0, 1},
		{"AMST",    "Armenia Summer Time",                            "Asia",                                                         5.0, 1},
		{"AMST",    "Amazon Summer Time",                                     "South America",        -3.0, 1},
		{"AMT",     "Armenia Time",             "Asia",                                                               4.0, 1},
		{"AMT",     "Amazon Time",  "South America",        -4.0, 0},
		{"ANAST",   "Anadyr Summer Time",                                     "Asia",                                                         12.0, 0},
		{"ANAT",    "Anadyr Time", "Asia",    12.0, 0},
		{"AQTT",    "Aqtobe Time",          "Asia",             5.0},
		{"ART",     "Argentina Time",       "South America",        -3.0},
		{"AST",     "Arabia Standard Time",         "Asia",             3.0},
		{"AST",     "Atlantic Standard Time",   "Atlantic",             -4.0},
		{"AST",     "Atlantic Standard Time",   "Caribbean",            -4.0},
		{"AST",     "Atlantic Standard Time",   "North America",        -4.0},
		{"AWDT",    "Australian Western Daylight Time", "Australia",            9.0},
		{"AWST",    "Australian Western Standard Time", "Australia",            8.0},
		{"AZOST",   "Azores Summer Time",       "Atlantic",             0.0},
		{"AZOT",    "Azores Time",          "Atlantic",             -1.0},
		{"AZST",    "Azerbaijan Summer Time",   "Asia",             5.0},
		{"AZT",     "Azerbaijan Time",          "Asia",             4.0},
		{"B",       "Bravo Time Zone",          "Military",             2.0},
		{"BNT",     "Brunei Darussalam Time",   "Asia",             8.0},
		{"BOT",     "Bolivia Time",             "South America",        -4.0},
		{"BRST",    "Brasilia Summer Time",         "South America",        -2.0},
		{"BRT",     "Brasília time",        "South America",        -3.0},
		{"BST",     "Bangladesh Standard Time",     "Asia",             6.0},
		{"BST",     "British Summer Time",          "Europe",           1.0},
		{"BTT",     "Bhutan Time",          "Asia",             6.0},
		{"C",       "Charlie Time Zone",        "Military",             3.0},
		{"CAST",    "Casey Time",           "Antarctica",           8.0},
		{"CAT",     "Central Africa Time",          "Africa",           2.0},
		{"CCT",     "Cocos Islands Time",       "Indian Ocean",             6.50},
		{"CDT",     "Cuba Daylight Time",       "Caribbean",            -4.0},
		{"CDT",     "Central Daylight Time",    "North America",        -5.0},
		{"CEST",    "Central European Summer Time",     "Europe",           2.0},
		{"CET",     "Central European Time",    "Africa",           1.0},
		{"CET",     "Central European Time",    "Europe",           1.0},
		{"CHADT",   "Chatham Island Daylight Time",     "Pacific",      13.75},
		{"CHAST",   "Chatham Island Standard Time",     "Pacific",          12.75},
		{"CKT",     "Cook Island Time",         "Pacific",          10.0},
		{"CLST",    "Chile Summer Time",        "South America",        -3.0},
		{"CLT",     "Chile Standard Time",          "South America",        -4.0},
		{"COT",     "Colombia Time",        "South America",        -5.0},
		{"CST",     "China Standard Time",          "Asia",             8.0},
		{"CST",     "Central Standard Time",    "Central America",          -6.0},
		{"CST",     "Cuba Standard Time",       "Caribbean",            -5.0},
		{"CST",     "Central Standard Time",    "North America",        -6.0},
		{"CVT",     "Cape Verde Time",          "Africa",           -1.0},
		{"CXT",     "Christmas Island Time",    "Australia",            7.0},
		{"ChST",    "Chamorro Standard Time",   "Pacific",              10.0},
		{"D",       "Delta Time Zone",          "Military",             4.0},
		{"DAVT",    "Davis Time",           "Antarctica",           7.0},
		{"E",       "Echo Time Zone",       "Military",             5.0},
		{"EASST",   "Easter Island Summer Time",    "Pacific",          -5.0},
		{"EAST",    "Easter Island Standard Time",      "Pacific",          -6.0},
		{"EAT",     "Eastern Africa Time",          "Africa",           3.0},
		{"EAT",     "East Africa Time",         "Indian Ocean",             3.0},
		{"ECT",     "Ecuador Time",             "South America",        5.0},
		{"EDT",     "Eastern Daylight Time",    "Caribbean",            -4.0},
		{"EDT",     "Eastern Daylight Time",    "North America",        -4.0},
		{"EDT",     "Eastern Daylight Time",    "Pacific",          11.0},
		{"EEST",    "Eastern European Summer Time",     "Africa",           3.0},
		{"EEST",    "Eastern European Summer Time",     "Asia",             3.0},
		{"EEST",    "Eastern European Summer Time",     "Europe ",          3.0},
		{"EET",     "Eastern European Time",    "Africa",           2.0},
		{"EET",     "Eastern European Time",    "Asia",             2.0},
		{"EET",     "Eastern European Time",    "Europe",           2.0},
		{"EGST",    "Eastern Greenland Summer Time",            "North America",        2.0},
		{"EGT",     "East Greenland Time",          "North America",        -1.0},
		{"EST",     "Eastern Standard Time",    "Central America",          -5.0},
		{"EST",     "Eastern Standard Time",    "Caribbean",            -5.0},
		{"EST",     "Eastern Standard Time",    "North America",        -5.0},
		{"ET",      "Tiempo del Este",          "Central America",          -5.0},
		{"ET",      "Tiempo del Este",          "Caribbean",            -5.0},
		{"ET",      "Tiempo Del Este",          "North America",        -5.0},
		{"F",       "Foxtrot Time Zone",        "Military",             6.0},
		{"FJST",    "Fiji Summer Time",         "Pacific",          13.0},
		{"FJT",     "Fiji Time",            "Pacific",          12.0},
		{"FKST",    "Falkland Islands Summer Time",     "South America",        -3.0},
		{"FKT",     "Falkland Island Time",         "South America",        -4.0},
		{"FNT",     "Fernando de Noronha Time",     "South America",        -2.0},
		{"G",       "Golf Time Zone",       "Military",             7.0},
		{"GALT",    "Galapagos Time",       "Pacific",          -6.0},
		{"GAMT",    "Gambier Time",             "Pacific",          -9.0},
		{"GET",     "Georgia Standard Time",    "Asia",             4.0},
		{"GFT",     "French Guiana Time",       "South America",        -3.0},
		{"GILT",    "Gilbert Island Time",          "Pacific",          12.0},
		{"GMT",     "Greenwich Mean Time",          "Africa ",          0.0},
		{"GMT",     "Greenwich Mean Time",          "Europe",           0.0},
		{"GST",     "Gulf Standard Time",       "Asia",             4.0},
		{"GYT",     "Guyana Time",          "South America",        -4.0},
		{"H",       "Hotel Time Zone",          "Military",             8.0},
		{"HAA",     "Heure Avancée de l'Atlantique",            "Atlantic",             -3.0},
		{"HAA",     "Heure Avancée de l'Atlantique",            "North America",        -3.0},
		{"HAC",     "Heure Avancée du Centre",      "North America",        -5.0},
		{"HADT",    "Hawaii-Aleutian Daylight Time",            "North America",        -9.0},
		{"HAE",     "Heure Avancée de l'Est",   "Caribbean",            -4.0},
		{"HAE",     "Heure Avancée de l'Est",       "North America",        -4.0},
		{"HAP",     "Heure Avancée du Pacifique",       "North America",        -7.0},
		{"HAR",     "Heure Avancée des Rocheuses",      "North America",        -6.0},
		{"HAST",    "Hawaii-Aleutian Standard Time",            "North America",        -10.0},
		{"HAT",     "Heure Avancée de Terre-Neuve",     "North America",        -2.50},
		{"HAY",     "Heure Avancée du Yukon",   "North America  ",      -8.0},
		{"HKT",     "Hong Kong Time",       "Asia",             8.0},
		{"HLV",     "Hora Legal de Venezuela",      "South America",        -4.50},
		{"HNA",     "Heure Normale de l'Atlantique",            "Atlantic",             -4.0},
		{"HNA",     "Heure Normale de l'Atlantique",		  "Caribbean",            -4.0},
		{"HNA",     "Heure Normale de l'Atlantique",            "North America",        -4.0},
		{"HNC",     "Heure Normale du Centre",      "Central America",          -6.0},
		{"HNC",     "Heure Normale du Centre",      "North America",        -6.0},
		{"HNE",     "Heure Normale de l'Est",   "Central America",          -5.0},
		{"HNE",     "Heure Normale de l'Est",   "Caribbean",            -5.0},
		{"HNE",     "Heure Normale de l'Est",   "North America",        -5.0},
		{"HNP",     "Heure Normale du Pacifique",       "North America",        -8.0},
		{"HNR",     "Heure Normale des Rocheuses",      "North America",        -7.0},
		{"HNT",     "Heure Normale de Terre-Neuve",     "North America",        -3.50},
		{"HNY",     "Heure Normale du Yukon",   "North America",        -9.0},
		{"HOVT",    "Hovd Time",            "Asia",             7.0},
		{"I",       "India Time Zone",          "Military",             9.0},
		{"ICT",     "Indochina Time",       "Asia",             7.0},
		{"IDT",     "Israel Daylight Time",         "Asia",             3.0},
		{"IOT",     "Indian Chagos Time",       "Indian Ocean",             6.0},
		{"IRDT",    "Iran Daylight Time",       "Asia",             4.50},
		{"IRKST",   "Irkutsk Summer Time",          "Asia",             9.0},
		{"IRKT",    "Irkutsk Time",             "Asia",             9.0},
		{"IRST",    "Iran Standard Time",       "Asia",             3.50},
		{"IST",     "Israel Standard Time",         "Asia",             2.0},
		{"IST",     "India Standard Time",          "Asia",             5.50},
		{"IST",     "Irish Standard Time",          "Europe",           1.0},
		{"JST",     "Japan Standard Time",          "Asia",             9.0},
		{"K",       "Kilo Time Zone",       "Military",             10.0},
		{"KGT",     "Kyrgyzstan Time",          "Asia",             6.0},
		{"KRAST",   "Krasnoyarsk Summer Time",      "Asia",             8.0},
		{"KRAT",    "Krasnoyarsk Time",            "Asia",             8.0},
		{"KST",     "Korea Standard Time",          "Asia",             9.0},
		{"KUYT",    "Kuybyshev Time",       "Europe",           4.0},
		{"L",       "Lima Time Zone",       "Military",             11.0},
		{"LHDT",    "Lord Howe Daylight Time",      "Australia",            11.0},
		{"LHST",    "Lord Howe Standard Time",   "Australia",            10.50},
		{"LINT",    "Line Islands Time",        "Pacific",          14.0},
		{"M",       "Mike Time Zone",       "Military",             12.0},
		{"MAGST",   "Magadan Summer Time",          "Asia",             12.0},
		{"MAGT",    "Magadan Time",             "Asia",            12.0},
		{"MART",    "Marquesas Time",       "Pacific",          -9.50},
		{"MAWT",    "Mawson Time",          "Antarctica",           5.0},
		{"MDT",     "Mountain Daylight Time",   "North America",        -6.0},
		{"MESZ",    "Mitteleuropäische Sommerzeit",     "Europe",           2.0},
		{"MEZ",     "Mitteleuropäische Zeit",   "Africa",           1.0},
		{"MHT",     "Marshall Islands Time",    "Pacific",          12.0},
		{"MMT",     "Myanmar Time",             "Asia",             6.50},
		{"MSD",     "Moscow Daylight Time",         "Europe",           4.0},
		{"MSK",     "Moscow Standard Time",         "Europe",           4.0},
		{"MST",     "Mountain Standard Time",   "North America",        -7.0},
		{"MUT",     "Mauritius Time",       "Africa",           4.0},
		{"MVT",     "Maldives Time",        "Asia",             5.0},
		{"MYT",     "Malaysia Time",        "Asia",             8.0},
		{"N",       "November Time Zone",       "Military",             -1.0},
		{"NCT",     "New Caledonia Time",       "Pacific",          11.0},
		{"NDT",     "Newfoundland Daylight Time",       "North America",        -2.50},
		{"NFT",     "Norfolk Time",             "Australia",            11.50},
		{"NOVST",   "Novosibirsk Summer Time",      "Asia",             7.0},
		{"NOVT",    "Novosibirsk Time",         "Asia",             6.0},
		{"NPT",     "Nepal Time",           "Asia",             5.75},
		{"NST",     "Newfoundland Standard Time",   "North America",        -3.50},
		{"NUT",     "Niue Time",            "Pacific",          -11.0},
		{"NZDT",    "New Zealand Daylight Time",    "Antarctica",           13.0},
		{"NZDT",    "New Zealand Daylight Time",    "Pacific",          13.0},
		{"NZST",    "New Zealand Standard Time",    "Antarctica",           12.0},
		{"NZST",    "New Zealand Standard Time",    "Pacific",          12.0},
		{"O",       "Oscar Time Zone",          "Military",             2.0},
		{"OMSST ",  "Omsk Summer Time",         "Asia",             7.0},
		{"OMST",    "Omsk Standard Time",       "Asia",             7.0},
		{"P",       "Papa Time Zone",       "Military",             -3.0},
		{"PDT",     "Pacific Daylight Time",    "North America",        -7.0},
		{"PET",     "Peru Time",    "South America",        -5.0},
		{"PETST",   "Kamchatka Summer Time",    "Asia",             12.0},
		{"PETT",    "Kamchatka Time",       "Asia",             12.0},
		{"PGT",     "Papua New Guinea Time",    "Pacific",          10.0},
		{"PHOT",    "Phoenix Island Time",          "Pacific",          13.0},
		{"PHT",     "Philippine Time",          "Asia",             8.0},
		{"PKT",     "Pakistan Standard Time",   "Asia",             5.0},
		{"PMDT",    "Pierre & Miquelon Daylight Time",  "North America",        -2.0},
		{"PMST",    "Pierre & Miquelon Standard Time",  "North America",        -3.0},
		{"PONT",    "Pohnpei Standard Time",    "Pacific",          11.0},
		{"PST",     "Pacific Standard Time",    "North America",        -8.0},
		{"PST",     "Pitcairn Standard Time",   "Pacific",          -8.0},
		{"PT",      "Tiempo del Pacífico",          "North America",        -8.0},
		{"PWT",     "Palau Time",           "Pacific",          9.0},
		{"PYST",    "Paraguay Summer Time",         "South America",        -3.0},
		{"PYT",     "Paraguay Time  South",     "America",          -4.0},
		{"Q",       "Quebec Time Zone",         "Military",             -4.0},
		{"R",       "Romeo Time Zone",          "Military",             -5.0},
		{"RET",     "Reunion Time",             "Africa",           4.0},
		{"S",       "Sierra Time Zone",         "Military",             -6.0},
		{"SAMT",    "Samara Time",          "Europe",           4.0},
		{"SAST",    "South Africa Standard Time",       "Africa",           2.0},
		{"SBT",     "Solomon IslandsTime",          "Pacific",          11.0},
		{"SCT",     "Seychelles Time",          "Africa",           4.0},
		{"SGT",     "Singapore Time",       "Asia",             8.0},
		{"SRT",     "Suriname Time",        "South America",        -3.0},
		{"SST",     "Samoa Standard Time",          "Pacific",          -11.0},
		{"T",       "Tango Time Zone",          "Military",             -7.0},
		{"TAHT",    "Tahiti Time",          "Pacific",          -10.0},
		{"TFT",     "French Southern and Antarctic Time",    "Indian Ocean",             5.0},
		{"TJT",     "Tajikistan Time",          "Asia",             5.0},
		{"TKT",     "Tokelau Time",             "Pacific",          13.0},
		{"TLT",     "East Timor Time",          "Asia",             9.0},
		{"TMT",     "Turkmenistan Time",        "Asia",             5.0},
		{"TVT",     "Tuvalu Time",          "Pacific",          12.0},
		{"U",       "Uniform Time Zone",        "Military",             -8.0},
		{"ULAT",    "Ulaanbaatar Time",         "Asia",             8},
		{"UYST",    "Uruguay Summer Time",          "South America",        -2.0},
		{"UYT",     "Uruguay Time",             "South America",        -3.0},
		{"UZT",     "Uzbekistan Time",          "Asia",            5.0},
		{"V",       "Victor Time Zone",         "Military",             -9.0},
		{"VET",     "Venezuelan Standard Time",      "South America",        -4.50},
		{"VLAST",   "Vladivostok Summer Time",      "Asia",            11.0},
		{"VLAT",    "Vladivostok Time",         "Asia",             11.0},
		{"VUT",     "Vanuatu Time",             "Pacific",          11.0},
		{"W",       "Whiskey Time Zone",        "Military",             -10.0},
		{"WAST",    "West Africa Summer Time",      "Africa",           2.0},
		{"WAT",     "West Africa Time",         "Africa",           1.0},
		{"WEST",    "Western European Summer Time",     "Africa",           1.0},
		{"WEST",    "Western European Summer Time",     "Europe",           1.0},
		{"WESZ",    "Westeuropäische Sommerzeit",       "Africa",           1.0},
		{"WET",     "Western European Time",    "Africa",           0.0},
		{"WET",     "Western European Time",    "Europe ",          0.0},
		{"WEZ",     "Westeuropäische Zeit",         "Europe ",          0.0},
		{"WFT",     "Wallis and Futuna Time",   "Pacific",          12.0},
		{"WGST",    "Western Greenland Summer Time",            "North America",        -2.0},
		{"WGT",     "West Greenland Time",          "North America",        -3.0},
		{"WIB",     "Western Indonesian Time",      "Asia",             7.0},
		{"WIT",     "Eastern Indonesian Time",      "Asia",             9.0},
		{"WITA",    "Central Indonesian Time",      "Asia",             8.0},
		{"WST",     "Western Sahara Summer Time",       "Africa",           1.0},
		{"WST",     "West Samoa Time",          "Pacific",          13.0},
		{"WT",      "Western Sahara Standard Time",     "Africa ",          0.0},
		{"X",       "X-ray Time Zone",          "Military",             -11.0},
		{"Y",       "Yankee Time Zone",         "Military",             -12.0},
		{"YAKST",   "Yakutsk Summer Time",          "Asia",             10.0},
		{"YAKT",    "Yakutsk Time",             "Asia",             10.0},
		{"YAPT",    "Yap Time",             "Pacific",          10.0},
		{"YEKST",   "Yekaterinburg Summer Time",    "Asia",             6.0},
		{"YEKT",    "Yekaterinburg Time",       "Asia",             6.0},
		{"Z",       "Zulu Time Zone",       "Military",             0.0}

};


int GetTimeZoneDSTInfo(const char* szCurrentTimeZone, const char* szOriginalTimeZone, float* ftTimeZone, int* IsDSTEnable)
{
		int rect = 0x01;

		int cntTimeZone = sizeof(g_sLstAllTimeZone)/(sizeof(tTimeZone));
		printf("Time zone: look for %s\n", szCurrentTimeZone);

		int K = 0x00;
		for (; K < cntTimeZone; K++)
		{

				if (0x00 == strcmp(szCurrentTimeZone, g_sLstAllTimeZone[K].szShortName))
				{
						//- Found it
						*ftTimeZone = g_sLstAllTimeZone[K].ftTimeZoneValue;
						rect = 0x00;
						*IsDSTEnable = g_sLstAllTimeZone[K].IsDSTSupport;
						break;
				}
		}


		if (K == cntTimeZone)
				rect = 0x01;

		if (0x00 == rect)
		{
				printf("Time zone: DST enable: %d, time zone: %f\n", *IsDSTEnable, *ftTimeZone);
		}
		else
				printf("Can not find timezone iformation");

		return rect;
}



int InitNetCamTimeZoneAndDST()
{
		int isEnable = 0x00;
		float ftTimeZone = 0.0;
		//int rect = GetTimeZoneDSTInfo(tzname[0x00], tzname[0x01], &ftTimeZone, &isEnable);
		GetTimeZoneDSTInfo(tzname[0x00], tzname[0x01], &ftTimeZone, &isEnable);
		char szMyTimeZone[16] = {0x00};
		sprintf(szMyTimeZone, "%f", ftTimeZone);
		SetBelkinParameter("LastTimeZone", szMyTimeZone);
		if (isEnable)
		{
				printf("DST ON: %d, calculating the DST shifting time\n", isEnable);
				computeDstToggleTime(0x01);
		}

		return 0;
}


/*!
 *      \fuction:               UpdateNetCamTimeZoneInfo
 *      \brief                  to set timezone in line with NetCam algorithm
 *      \param                  szRegion        region string from time zone table
 *      \return
 *                      0x00: SUCCESS
 *                      0x01: UN_SUCCESS
 */

int UpdateNetCamTimeZoneInfo(const char* szRegion)
{
		printf("Check the region: %s\n", szRegion);
		int rect = 0x01;
		int isChanged = 0x00;
		if ((0x00 == szRegion) ||
						(0x00 == strlen(szRegion)))
		{
				printf("Time zone: no time zone region information, ignored\n");
				return rect;
		}

		if (0x00 == strlen(g_sCurrentTZ))
		{
				//-Read the region information
				char* szTZ = GetBelkinParameter("TZ");
				if (0x00 != szTZ)
				{
						strncpy(g_sCurrentTZ, szTZ, 128);
				}
				isChanged = 0x01;
		}

		if (strcmp(g_sCurrentTZ, szRegion))
		{
				isChanged = 0x01;
				printf("Region changed: %s- %s\n", g_sCurrentTZ, szRegion);
				strncpy(g_sCurrentTZ, szRegion, 128);
		}

		if (isChanged)
		{
				char szSysCmd[128] = {0x00};
				sprintf(szSysCmd, "echo %s>/etc/TZ", g_sCurrentTZ);
				system(szSysCmd);
				SetBelkinParameter("TZ", g_sCurrentTZ);
				printf("Time zone: %s\n", szSysCmd);
				memset(szSysCmd, 0x00, 128);
				system("/sbin/ntp.sh&");
				AsyncSaveData();
		}

		return 0;
}

/* -------------------------------------------------------------------------
 * For some reason, Seedonk is unable to call an IPC that we could provide,
 * similar to the motion event, to report that camera name has changed.
 * Instead, all they do is silently update the NVRAM (CameraName) with the
 * new camera name.
 *
 * This is a work around to "poll" the camera name and report to the cloud
 * in case the name changed.
 *
 * This thread wakes up once in a while, checks/updates the camera name, and
 * trigger cloud update if necessary.
 * ------------------------------------------------------------------------ */
#define MONITOR_CAMERA_NAME_INTERVAL 60
static void *MonitorCameraNameThread()
{
  char *cameraName, *loginName;
  
  tu_set_my_thread_name( __FUNCTION__ );
  printf("NetCam: MonitorCameraName thread running\n");
  while (1) {
    sleep(MONITOR_CAMERA_NAME_INTERVAL);
    
    cameraName = GetBelkinParameter(CAMERA_NAME_VAR);
    if (strlen(cameraName))  {
      if (strncmp(cameraName, g_szFriendlyName, sizeof(g_szFriendlyName)) != 0) {
	/* update camera name */
	strncpy(g_szFriendlyName, cameraName, sizeof(g_szFriendlyName));
	UpdateXML2Factory();
	AsyncSaveData();
	/* trigger cloud update */
	setPluginStatusTS();
      }
    }
	
    /* update loginName name */
    loginName = GetBelkinParameter(SEEDONK_LOGIN_NAME_VAR);
    if (strlen(loginName)) {
      if (strncmp(loginName, g_szNetCamLogName, sizeof(g_szNetCamLogName)) != 0) {
	strncpy(g_szNetCamLogName, loginName, sizeof(g_szNetCamLogName)); 
	/* trigger cloud update */
	setPluginStatusTS();
      }
    } 
    
  }
}


/* -------------------------------------------------------------------------
 * For some reason, Seedonk is unable to call an IPC that we could provide,
 * similar to the motion event, to report that camera name has changed.
 * Instead, all they do is silently update the NVRAM (CameraName) with the
 * new camera name.
 *
 * This is a work around to "poll" the camera name and report to the cloud
 * in case the name changed.
 *
 * This function starts the monitoring thread.
 * ------------------------------------------------------------------------ */
void StartMonitorCameraNameThread()
{
		int rc = pthread_create(&g_sMonitorCameraNameThread,
						0,
						(void*) MonitorCameraNameThread,
						0);

		if (rc == 0)
		{
				pthread_detach(g_sMonitorCameraNameThread);
		}
		else
		{
				printf("NetCam: MonitorCameraName thread creation failure\n");
		}
}

#if 1
static pthread_t g_updsntocloudThread = -1; /*!< The update serial number to cloud thread */
#define NVRAM_SN_KEY "SerialNumber"
#define DEFAULT_PLUGIN_PRIVATE_KEY "plugin_key"
extern char g_szClientType[SIZE_128B];
extern char g_sSerialNumber[15];

static void parseSNChangeResp(void *snResp, char **errcode) {

		char *snRespXml = NULL;
		mxml_node_t *tree = NULL;
		mxml_node_t *find_node = NULL;
		char *pcode = NULL;

		if(!snResp)
				return;

		snRespXml = (char*)snResp;

		tree = mxmlLoadString(NULL, snRespXml, MXML_OPAQUE_CALLBACK);
		if (tree){
				DDBG("XML String Loaded is %s\n", snRespXml);
				find_node = mxmlFindElement(tree, tree, "code", NULL, NULL, MXML_DESCEND);
				if (find_node && find_node->child) {
						DDBG("The Error Code set in the node is %s\n", find_node->child->value.opaque);
						pcode = (char*)calloc(1, SIZE_256B);
						if (pcode) {
								strncpy(pcode, (find_node->child->value.opaque), SIZE_256B-1);
								*errcode = pcode;
								DDBG("The error code set from cloud is %s\n", *errcode);
						}
				}
		}
		if (find_node) mxmlDelete(find_node);
		if(tree) mxmlDelete(tree);
		find_node = NULL;
		tree = NULL;
		return;
}
static void* UpdateSNToCloudThreadTask()
{
		UserAppSessionData *pUsrAppSsnData = NULL;
		UserAppData *pUsrAppData = NULL;
		authSign *assign = NULL;
		char *prevSn = NULL, *pvtKey = NULL, *pMAC = NULL;
		int retVal = PLUGIN_SUCCESS;
		char tmpURL[SIZE_128B];
		char snData[SIZE_256B];
		char *pcode = NULL;

		tu_set_my_thread_name( __FUNCTION__ );

		pUsrAppSsnData = webAppCreateSession(0);
		prevSn = GetBelkinParameter(NVRAM_SN_KEY);
		pvtKey = GetBelkinParameter(DEFAULT_PLUGIN_PRIVATE_KEY);
		pMAC = utilsRemDelimitStr(GetMACAddress(), ":");
		while (1) {
				pUsrAppData = (UserAppData *)calloc(1, sizeof(UserAppData));
				if(!pUsrAppData) {
						DDBG("Memory error, exit .................\n");
						g_updsntocloudThread = -1;
						exit(01);
				}
				DDBG( "Pre-requisite for updation new serial number \"%s\" to cloud %d \n", g_sSerialNumber, getCurrentClientState());
				if ( (getCurrentClientState() == STATE_CONNECTED) && (pvtKey) && (strlen(pvtKey) != 0x00)) {
						if ((prevSn) && (strlen(prevSn) != 0x00)) {
								DDBG( "Creating auth header with previous serial number \"%s\" \n", prevSn );
								assign = createAuthSignature(pMAC, prevSn, pvtKey);
						}else {
								DDBG( "Creating auth header with new serial number \"%s\" \n", g_sSerialNumber );
								assign = createAuthSignature(pMAC, g_sSerialNumber, pvtKey);
						}
						if(!assign)
						{
								DDBG("Memory error, exit .................\n");
								g_updsntocloudThread = -1;
								exit(01);
						}
						/*URL should be https://api.xbcs.net:8443/apis/http/plugin/deviceProperty/<macAddress> */
						memset(tmpURL, 0x00, SIZE_128B);
						snprintf(tmpURL,sizeof(tmpURL), "https://%s:8443/apis/http/plugin/deviceProperty/%s", BL_DOMAIN_NM, pMAC);
						strncpy(pUsrAppData->url, tmpURL, sizeof(pUsrAppData->url)-1);
						strncpy( pUsrAppData->keyVal[0].key, "Content-Type", sizeof(pUsrAppData->keyVal[0].key)-1);   
						strncpy( pUsrAppData->keyVal[0].value, "application/xml", sizeof(pUsrAppData->keyVal[0].value)-1);   
						strncpy( pUsrAppData->keyVal[1].key, "Accept", sizeof(pUsrAppData->keyVal[1].key)-1);   
						strncpy( pUsrAppData->keyVal[1].value, "application/xml", sizeof(pUsrAppData->keyVal[1].value)-1);   
						strncpy( pUsrAppData->keyVal[2].key, "Authorization", sizeof(pUsrAppData->keyVal[2].key)-1);   
						strncpy( pUsrAppData->keyVal[2].value, assign->signature, sizeof(pUsrAppData->keyVal[2].value)-1);   
						strncpy( pUsrAppData->keyVal[3].key, "X-Belkin-Client-Type-Id", sizeof(pUsrAppData->keyVal[3].key)-1);   
						strncpy( pUsrAppData->keyVal[3].value, g_szClientType, sizeof(pUsrAppData->keyVal[3].value)-1);   
						pUsrAppData->keyValLen = 4;

						//Create xml data to send
						memset(snData, 0x00, SIZE_256B);
						/*
							 <plugin>
							 <uniqueIduniqueId</udn>
							 <serialNumber></serialNumber>  
							 <description></description>  
							 <firmwareVersionfirmwareVersion</fwVersion>
							 <fwUpgradeStatus></fwUpgradeStatus>
							 </plugin>
						 */
						snprintf(snData, sizeof(snData), "<plugin><uniqueId>uuid:%s-1_0-%s</uniqueId><serialNumber>%s</serialNumber></plugin>", \
										(char*)getDeviceUDNString(), g_sSerialNumber, g_sSerialNumber);
						strncpy( pUsrAppData->inData, snData, sizeof(pUsrAppData->inData)-1);
						pUsrAppData->inDataLength = strlen(snData);
						char *check = strstr(pUsrAppData->url, "https://");
						if(check)
								pUsrAppData->httpsFlag = 1;
						pUsrAppData->disableFlag = 1;    

						DDBG( "Sending new serial number \"%s\" and old serial number with authentication \"%s\"\n", g_sSerialNumber, prevSn );
						DDBG( "Sending data \"%s\" to url \"%s\"\n", snData, tmpURL );
						//Send request to cloud
						retVal = webAppSendData( pUsrAppSsnData, pUsrAppData, 18);  
						if (retVal)
						{
								DDBG("Curl error encountered in send status to cloud  , errorCode %d \n", retVal);
						}
						DDBG("Curl response received from cloud , responseCode %d \n", pUsrAppData->outResp);
						if (pUsrAppData->outResp == 200) {
								DDBG( "Saving serial number \"%s\" to \"%s\"\n", g_sSerialNumber, NVRAM_SN_KEY );
								SetBelkinParameter( NVRAM_SN_KEY, g_sSerialNumber );
								break;
						} else if (pUsrAppData->outResp == 403) {
								parseSNChangeResp(pUsrAppData->outData, &pcode);
								if ((pcode)){
										DDBG("########AUTH FAILURE (403) : %s: ########", pcode);
										if (!strncmp(pcode, "ERR_002", strlen("ERR_002"))) {
												CheckUpdateRemoteFailureCount();
										}
										free(pcode); 
										pcode= NULL;
								}
						}
				} else {
						DDBG("Network state not connected or key not set, will retry \n");
				}
				if (assign) {
						free(assign);
						assign = NULL;
				}
				if (pUsrAppData) {
						if (pUsrAppData->outData) {
								free(pUsrAppData->outData);
								pUsrAppData->outData = NULL;
						}
						free(pUsrAppData);
						pUsrAppData = NULL;
				}
				pvtKey = GetBelkinParameter(DEFAULT_PLUGIN_PRIVATE_KEY);
				printf("****************Current state = %d, pvt=%s**************\n", getCurrentClientState(), pvtKey);
				pluginUsleep(REMOTE_STATUS_SEND_RETRY_TIMEOUT);
		}
		DDBG( "Out of Updating cloud and Saving serial number \"%s\" \n", g_sSerialNumber );
		if (pUsrAppSsnData) {
				webAppDestroySession ( pUsrAppSsnData );
				pUsrAppSsnData = NULL;
		}
		if (pUsrAppData) {
				if (pUsrAppData->outData) {
						free(pUsrAppData->outData);
						pUsrAppData->outData = NULL;
				}
				free(pUsrAppData);
				pUsrAppData = NULL;
		}
		if (pMAC) {
				free(pMAC);
				pMAC = NULL;
		}
		if (assign) {
				free(assign);
				assign = NULL;
		}
		g_updsntocloudThread = -1;
}


/* @brief Send serial number to cloud & save.
 * This non-blocking call schedules sending the provided serial number
 * to the cloud.  If successful, it also stores it in nvram.
 * @param sn Serial number.
 */
void send_sn_to_cloud_and_save( const char *sn )
{
		/* Send to cloud here.  Operation should retry on error.  On
		 * success, save to nvram at key NVRAM_SN_KEY like so:

		 DDBG( "Saving serial number \"%s\" to \"%s\"\n", sn, NVRAM_SN_KEY );
		 SetBelkinParameter( NVRAM_SN_KEY, sn );
		 */

		/* Once the above process has been launched, return to caller to
		 * prevent blocking other functions.
		 */
		if (g_updsntocloudThread == -1) {
				int rect = pthread_create(&g_updsntocloudThread, 0x00, (void *)UpdateSNToCloudThreadTask, 0x00);

				DDBG( "Start UpdateSNToCloudThreadTask()\n" );
				if (0x00 == rect)
				{
						DDBG("Netcam: Update serial number to cloud thread creation success\n");
						//- success
						pthread_detach(g_updsntocloudThread);
				}
				else
				{
						DDBG("Netcam: Update serial number to cloud thread creation failure\n");
				}
		}else {
				DDBG("Netcam: Update serial number to cloud thread is already running\n");
		}
}
#endif

