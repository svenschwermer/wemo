#ifdef PRODUCT_WeMo_LEDLight

#include "global.h"
#include "defines.h"
#include "types.h"
#include "pthread.h"
#include "remoteAccess.h"
#include "mxml.h"
#include "logger.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "gpio.h"
#include "cloudIF.h"
#include "utils.h"
#include "wifiSetup.h"
#include "httpsWrapper.h"
#include "controlledevice.h"
#include "natClient.h"

#include "thready_utils.h"

#include "remote_event.h"
#include "ledLightRemoteAccess.h"

#include "timer.h"

#include "subdevice.h"

#include "rule.h"

#define  _SD_MODULE_
#include "sd_tracer.h"
#define CAPABILITY_ID_STATUS_CHANGE "10500"
#define SENSOR_STATUS_CHANGE 0x00001
#define DATA_STORE_NOTHING_FOUND_FOR_NAME 2

int g_initRegisterSubDevices = 0;

extern char g_szSerialNo[SIZE_64B];
extern char g_szPluginCloudId[SIZE_16B];
extern char g_szFirmwareVersion[SIZE_64B];
extern char g_szWiFiMacAddress[SIZE_64B];
extern char g_szFirmwareURLSignature[MAX_FW_URL_SIGN_LEN];


extern int gSignalStrength;

extern char gIcePrevTransactionId[PLUGIN_MAX_REQUESTS][TRANSACTION_ID_LEN];
extern int gIceRunning;
extern int gDataInProgress;

#define INVALID_THREAD_ID -1

static pthread_t remote_worker_tid = INVALID_THREAD_ID;
static pthread_mutex_t preset_mutex_lock;

static int sent_notification = 0;
static int sent_bulb_notification = 0;

static timer_t timer_id;

static bool running_worker = true;

char g_szErrorCode[128] = {0,};

int updateDevice(void);

int update_all_subdevice(void);
void updateSubdevice(struct DeviceInfo *pDevInfo);


void init_preset_lock()
{
	osUtilsCreateLock(&preset_mutex_lock);
}

void destroy_preset_lock()
{
  osUtilsDestroyLock(&preset_mutex_lock);
}

void preset_lock()
{
  osUtilsGetLock(&preset_mutex_lock);
}

void preset_unlock()
{
  osUtilsReleaseLock(&preset_mutex_lock);
}

void timer_handler(int signum, siginfo_t *info, void *data)
{
  if( SIGRTMAX == signum )
  {
    if( 0 == sent_notification || 0 == sent_bulb_notification )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "timer_handler()...");
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "connState = (%d) and ntpupdate = (%d) and enableRemote = (%d)...",
              getCurrentClientState(), IsNtpUpdate(), gpluginRemAccessEnable);

      if( (STATE_CONNECTED == getCurrentClientState()) && IsNtpUpdate() && gpluginRemAccessEnable )
      {
         sent_notification = updateDevice();
         sent_bulb_notification = update_all_subdevice();
      }
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "stop and destroy timer()...");
      stop_timer(timer_id);
      destroy_timer(timer_id);
    }
  }
}

int update_all_subdevice(void)
{
  int nFailed = 0;
  int nDevice = 0, i = 0, j = 0;
  SD_DeviceList devices;

  struct DeviceInfo *pDevInfo = NULL;

  nDevice = SD_GetDeviceList(&devices);

  for( i = 0; i < nDevice; i++ )
  {
    pDevInfo = (struct DeviceInfo *)calloc(1, sizeof(struct DeviceInfo));

    if( pDevInfo )
    {
      memcpy(pDevInfo->szDeviceID, devices[i]->EUID.Data, sizeof(pDevInfo->szDeviceID));

      if( devices[i]->Paired )
      {
        for( j = 0; j < devices[i]->CapabilityCount; j++ )
        {
          if( 0 == strcmp(devices[i]->Capability[j]->ProfileName, "OnOff") )
          {
            memcpy(pDevInfo->CapInfo.szCapabilityID, devices[i]->Capability[j]->CapabilityID.Data,
                  sizeof(pDevInfo->CapInfo.szCapabilityID));
            memcpy(pDevInfo->CapInfo.szCapabilityValue, devices[i]->CapabilityValue[0],
                  sizeof(pDevInfo->CapInfo.szCapabilityValue));
          }
        }
      }

      SetSubDeviceFWUpdateState(pDevInfo->szDeviceID, OTA_FW_UPGRADE_SUCCESS);
      // updateSubdevice(pDevInfo);
      trigger_remote_event(WORK_EVENT, UPDATE_SUBDEVICE, "device", "update", pDevInfo, sizeof(struct DeviceInfo));
      free(pDevInfo);
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "update_all_subdevice() Failed");
      nFailed++;
    }

    sleep(1);
  }

  if( nFailed )
  {
    return 0;
  }

  return 1;
}

int isStatusOK(char *pszRespHeader)
{
  if( pszRespHeader )
  {
    if( (NULL != strstr(pszRespHeader, "200 OK")) || (NULL != strstr(pszRespHeader, "200")) )
    {
      return 1;
    }
  }

  return 0;
}

int GetStatusCode(char *pszRespHeader)
{
  int nStatusCode = 0;
  char szStatusCode[5] = {0,};
  char *pBeg = NULL, *pEnd = NULL;

  if( pszRespHeader )
  {
    if( (pBeg = strstr(pszRespHeader, "HTTP/1.1")) )
    {
      if( (pEnd = strchr(pBeg, ' ')) )
      {
        pBeg = pEnd + 1;
        if( (pEnd = strchr(pBeg, ' ')) )
        {
          strncpy(szStatusCode, pBeg, pEnd - pBeg);
          nStatusCode = atoi(szStatusCode);
          return nStatusCode;
        }
      }
    }
  }

  return nStatusCode;
}

int GetNextStatusCode(char *pszRespHeader)
{
  int nStatusCode = 0;
  char *pPos = NULL;

  if( pszRespHeader )
  {
    if( NULL != (pPos = strstr(pszRespHeader, "100 Continue")) ||
        NULL != (pPos = strstr(pszRespHeader, "100")) )
    {
      nStatusCode = GetStatusCode(pPos);
    }
  }

  return nStatusCode;
}

char* GetErrorCodeStr(char *pszRespBody)
{
  mxml_node_t *tree = NULL;
  mxml_node_t *find_node = NULL;

  tree = mxmlLoadString(NULL, pszRespBody, MXML_OPAQUE_CALLBACK);

  if( NULL == tree )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Can not make the xml body...");
    return NULL;
  }

  find_node = mxmlFindElement(tree, tree, "statusCode", NULL, NULL, MXML_DESCEND);

  if( find_node )
  {
    char *pStatusCode = (char *)mxmlGetOpaque(find_node);
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pStatusCode = %s", pStatusCode);
    if( 0 == strcmp(pStatusCode, "F") )
    {
      find_node = mxmlFindElement(find_node, tree, "code", NULL, NULL, MXML_DESCEND);
      if( find_node )
      {
        char *pErrorCode = (char *)mxmlGetOpaque(find_node);
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pErrorCode = %s", pErrorCode);
        SAFE_STRCPY(g_szErrorCode, pErrorCode);
        mxmlDelete(tree);
        return g_szErrorCode;
      }
    }
  }

  mxmlDelete(tree);
  return NULL;
}

char* getElemenetValue(mxml_node_t *curr_node, mxml_node_t *start_node, mxml_node_t **pfind_node, char *pszElementName)
{
  char *pValue = NULL;

  *pfind_node = mxmlFindElement(curr_node, start_node, pszElementName, NULL, NULL, MXML_DESCEND);

  if( NULL == *pfind_node )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "%s tag not present in xml payload", pszElementName);
    return pValue;
  }

  pValue = (char *)mxmlGetOpaque(*pfind_node);
  if( pValue )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "%s value is %s", pszElementName, pValue);
  }
  else
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "%s value is not found...", pszElementName);
  }

  return pValue;
}

int GetSubDeviceAvailableStatus(SD_Device device)
{
  int nStatus = 0;
  char szValue[CAPABILITY_MAX_VALUE_LENGTH] = {0,};

  if( device->Available == SD_ALIVE )
  {
    SAFE_STRCPY(szValue, device->CapabilityValue[1]);
    if( szValue[0] )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "DeviceID = %s, Value = %s", device->EUID.Data, szValue);
      nStatus = atoi(szValue);
    }
  }
  else if( device->Available == SD_DEAD )
  {
    //3 means bulb is unavailable...
    nStatus = 3;
  }

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "GetSubDeviceAvailableStatus: DeviceID = %s, Status = %d",
            device->EUID.Data, nStatus);

  return nStatus;
}

//========================================================================================
// Create WeMoLED RemoteAccess Thraed
//========================================================================================

void initRegisters(void)
{
  if( gpluginRemAccessEnable )
  {
    int nStatusCode = 0;
    char* pErrorCode = NULL;
    char* pPluginId = GetBelkinParameter(DEFAULT_PLUGIN_CLOUD_ID);

    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Initial Register sub devices and capability");

    if( ( nStatusCode = RegisterSubDevices(pPluginId, NULL, &pErrorCode)) )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "RegisterSubDevices(pPluginId = %s, nStatusCode = %d)", pPluginId, nStatusCode);
      if( (200 != nStatusCode) && (NULL != pErrorCode) )
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure: RegisterSubDevices(ErrorCode = %s)", pErrorCode);
        // DeleteSubDevices();
      }
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure: RegisterSubDevices(pPluginId = %s)", pPluginId);
    }

    if( (nStatusCode = RegisterCapabilityProfiles(&pErrorCode)) )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "RegisterCapabilityProfiles(nStatusCode = %d)", nStatusCode);
        if( pErrorCode )
        {
          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure: RegisterCapabilityProfiles(ErrorCode = %s)", pErrorCode);
      }
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure: RegisterCapabilityProfiles");
    }

    g_initRegisterSubDevices = 1;
  }
}

void addSubdevice(struct SubDevices *pSubDevices)
{
  if( gpluginRemAccessEnable )
  {
    int i = 0;
    int nStatusCode = 0;
    char *pErrorCode = NULL;
    //If remote access enable option is turn on, register subdevices to the cloud server after calling AddDevice() upnp action.
    char *pPluginId = GetBelkinParameter(DEFAULT_PLUGIN_CLOUD_ID);

    SetCurrSubDeviceFWUpdateState(OTA_FW_UPGRADE_SUCCESS);

    if( pPluginId && pPluginId[0])
    {
      if( (nStatusCode = RegisterSubDevices(pPluginId, pSubDevices, &pErrorCode)) )
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "AddDevice: RegisterSubDevices(pPluginId = %s, StatusCode = %d)",
                  pPluginId, nStatusCode);

        for( i = 0; i < pSubDevices->nDevice; i++ )
        {
          SD_DeviceID deviceID;
          char capability_id[CAPABILITY_MAX_ID_LENGTH] = {0,};
          char capability_value[CAPABILITY_MAX_VALUE_LENGTH] = {0,};

          MakeDeviceID(pSubDevices->szDeviceIDs[i], &deviceID);

          GetCacheDeviceOnOff(deviceID, capability_id, capability_value);
          if (capability_id[0] == 0) {
            GetCacheDeviceHomeSense(deviceID, capability_id, capability_value);
          }
          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "GetCacheDeviceOnOff: deviceID = %s, id = %s, value = %s",
                    deviceID.Data, capability_id, capability_value);
          SendDeviceEvent(true, false, pSubDevices->szDeviceIDs[i], capability_id, capability_value, SE_REMOTE);
        }

        if( pErrorCode )
        {
          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "AddDevice: RegisterSubDevices(pPluginId = %s, ErrorCode = %s)", pPluginId, pErrorCode);
        }
      }
      else
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "AddDevice: Failure RegisterSubDevices(pPluginId=%s)", pPluginId);
      }
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "AddDevice: Bridge Device PluginID is not existed");
    }
  }
}

void delSubdevice(char *pszRegisterDeviceID)
{
  if( gpluginRemAccessEnable && pszRegisterDeviceID )
  {
    char *pErrorCode = NULL;

    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Delete subdevice(%s)", pszRegisterDeviceID);

    int nStatusCode = DeleteSubDevice(pszRegisterDeviceID, &pErrorCode);
    if( (nStatusCode != 200) && pErrorCode )
    {
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "DeleteSubDevice is Failed : ErrorCode = %s...", pErrorCode);
    }
    else
    {
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "DeleteSubDevice : StatusCode = %d...", nStatusCode);
    }
  }
}

void delSubdevices(void)
{
  if( gpluginRemAccessEnable )
  {
    int nStatusCode = 0;
    char* pErrorCode = NULL;
    char *pPluginId = GetBelkinParameter(DEFAULT_PLUGIN_CLOUD_ID);

    if( pPluginId && pPluginId[0] )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Delete subdevices");

      nStatusCode = DeleteSubDevices(pPluginId, &pErrorCode);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "DeleteSubDevices(pPluginId = %s, nStatusCode = %d)", pPluginId, nStatusCode);
      if( (200 != nStatusCode) && (NULL != pErrorCode) )
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure: DeleteSubDevices(ErrorCode = %s)", pErrorCode);
      }
    }
  }
}

void handleGroupDevice(int event, SD_GroupID *pGroupID)
{
  int nStatusCode = 0;
  char *pErrorCode = NULL;

  if( gpluginRemAccessEnable && g_szHomeId[0] )
  {
    nStatusCode = 0;
    pErrorCode = NULL;

    switch( event )
    {
      case CREATE_GROUPDEVICE:
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Create Group in handleGroupDevice...............");
        nStatusCode = CreateGroups(POST, g_szHomeId, pGroupID->Data, &pErrorCode);
        break;
      case UPDATE_GROUPDEVICE:
        nStatusCode = CreateGroups(PUT, g_szHomeId, pGroupID->Data, &pErrorCode);
        break;
      case DELETE_GROUPDEVICE:
        nStatusCode = DeleteGroups(g_szHomeId, pGroupID->Data, &pErrorCode);
        break;
    }

    if( nStatusCode )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "handleGroupDevice(event = %d, HomeId = %s, GroupID = %s, StatusCode = %d)",
                event, g_szHomeId, pGroupID->Data, nStatusCode);
    }
  }
}

int updateDevice(void)
{
  int nStatusCode = 0;

  if( gpluginRemAccessEnable && isNotificationEnabled() )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Update Device Status");

    char *pErrorCode = NULL;
    char *pPluginId = GetBelkinParameter(DEFAULT_PLUGIN_CLOUD_ID);
    if( pPluginId && pPluginId[0] )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Call SendDeviceStateNotification to notify event to the Cloud...");
      if( 200 == (nStatusCode = SendDeviceStateNotification(pPluginId, NULL, NULL, &pErrorCode)) )
      {
        return 1;
      }
    }
  }
  else
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "gpluginRemAccessEnable = (%d)...", gpluginRemAccessEnable);
  }
  return 0;
}

void updateSubdevice(struct DeviceInfo *pDevInfo)
{
  if( gpluginRemAccessEnable )
  {
    int nStatusCode = 0;
    char *pErrorCode = NULL;

    char *pPluginId = GetBelkinParameter(DEFAULT_PLUGIN_CLOUD_ID);

    if( pPluginId && pPluginId[0] )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "updateSubdevice: pluginId = %s", pPluginId);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "updateSubdevice: pDevInfo->szDeviceID = %s", pDevInfo->szDeviceID);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "updateSubdevice: pDevInfo->CapInfo.szCapabilityID = %s",
                pDevInfo->CapInfo.szCapabilityID);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "updateSubdevice: pDevInfo->CapInfo.szCapabilityValue = %s",
                pDevInfo->CapInfo.szCapabilityValue);

      nStatusCode = SendDeviceStateNotification(pPluginId, pDevInfo->szDeviceID, pDevInfo, &pErrorCode);
      if( (nStatusCode != 200) && pErrorCode )
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SendDeviceStateNotification: StatusCode = %d, ErrorCode = %s", nStatusCode, pErrorCode);
      }
      else
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SendDeviceStateNotification: StatusCode = %d", nStatusCode);
      }
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SendDeviceStateNotification: Bridge Device PluginID is not existed");
    }
  }
}

void getHomeDevices(char *pHomeID)
{
  char *pErrorCode = NULL;

  int nStatusCode = GetHomeDevices(pHomeID, &pErrorCode);

  if( (nStatusCode != 200) && pErrorCode )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "GetHomeDevices is Failed : ErrorCode = %s...", pErrorCode);
  }
  else
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "GetHomeDevices : StatusCode = %d...", nStatusCode);
  }
}

void* RemoteWorkerThread(void *arg)
{
  event_t *event;

  tu_set_my_thread_name( __FUNCTION__ );

  while( running_worker )
  {
    if( !gpluginRemAccessEnable )
    {
       pluginUsleep(WAIT_30MS);
       continue;
    }

    event = wait_work_event(0);
    if( NULL == event )
    {
      pluginUsleep(WAIT_30MS);
      continue;
    }

    switch( event->nType )
    {
      case QUIT_THREAD:
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Exit RemoteWorkerThread...");
        running_worker = false;
        break;
      case INIT_REGISTER:
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Init Register subdevices and capability...");
        initRegisters();
        break;
      case UPDATE_CAPABILITY:
        {
          int nStatusCode = 0;
          char* pErrorCode = NULL;

          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Update Capability Profiles...");
          if( (nStatusCode = RegisterCapabilityProfiles(&pErrorCode)) )
          {
            DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "RegisterCapabilityProfiles(nStatusCode = %d)", nStatusCode);
            if( pErrorCode )
            {
              DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure: RegisterCapabilityProfiles(ErrorCode = %s)", pErrorCode);
            }
            }
            else
            {
            DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure: RegisterCapabilityProfiles");
          }
        }
        break;
      case QUERY_CAPABILITY:
        {
          int nStatusCode = 0;
          char* pErrorCode = NULL;

          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Query Capability Profiles...");
          if( (nStatusCode = QueryCapabilityProfileList(&pErrorCode)) )
          {
            DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "QueryCapabilityProfileList(nStatusCode = %d)", nStatusCode);
              if( pErrorCode )
              {
              DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure: QueryCapabilityProfileList(ErrorCode = %s)", pErrorCode);
            }
          }
          else
          {
            DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure: QueryCapabilityProfileList");
          }
        }
        break;
      case ADD_SUBDEVICE:
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Add subdevice...");
        addSubdevice((struct SubDevices *)event->pData);
        break;
      case DEL_SUBDEVICE:
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Del subdevice...");
        delSubdevice((char *)event->pData);
        break;
      case DEL_SUBDEVICES:
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Del subdevices...");
        delSubdevices();
        break;
      case CREATE_GROUPDEVICE:
      case UPDATE_GROUPDEVICE:
      case DELETE_GROUPDEVICE:
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Handle groupdevice...");
        handleGroupDevice(event->nType, (SD_GroupID *)event->pData);
        break;
      case UPDATE_DEVICE:
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Send device status notification...");
        updateDevice();
        break;
      case UPDATE_SUBDEVICE:
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Send subdevice status notification...");
        updateSubdevice((struct DeviceInfo *)event->pData);
        break;
      case GET_HOME_DEVICES:
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Get HomeDevices...");
        getHomeDevices((char *)event->pData);
      default:
        break;
    }

    if( event->pData )
    {
      free(event->pData);
    }
    free(event);

    pluginUsleep(WAIT_50MS);
  }

  //free resource
  destroy_work_event();

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Exit RemoteWorkerThread and Destroy WorkEvent...");
  pthread_exit(NULL);
}

int createRemoteThread(void)
{
  int rc = 0;

  create_work_event();

  rc = pthread_create(&remote_worker_tid, NULL, RemoteWorkerThread, NULL);
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "createRemoteWorkerThread rc=(%d)...", rc);
  if( 0 == rc )
  {
    pthread_detach(remote_worker_tid);
  }

  if( create_timer("updatedevice", &timer_id, timer_handler, 60, 60) )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failed to create timer handler");
  }

  return rc;
}

void destroyRemoteThread(void)
{
  if( INVALID_THREAD_ID != remote_worker_tid )
  {
    trigger_remote_event(WORK_EVENT, QUIT_THREAD, "worker", "quit", NULL, 0);
  }
}


//========================================================================================
// INPUT : nMethod(POST(1), PUT(2), GET(0), DELETE(8)), pszURL(http://host:8443/resource),
//         pReqData(Request Body), pRespData(Response Body)
//========================================================================================
int SendHttpRequestWithAuth(int nMethod, char *pszURL, char *pReqData, char **ppRespData)
{
  UserAppSessionData *pUsrAppSsnData = NULL;
  UserAppData *pUsrAppData = NULL;
  authSign *assign = NULL;

  int wret = 0, nDataLen = 0;
  int nStatusCode = 0;

  *ppRespData = NULL;

  if( 0 == strlen(g_szHomeId) && 0 == strlen(g_szPluginPrivatekey) )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Remote Access is not Enabled..not trying");
    resetFlashRemoteInfo(NULL);
    return nStatusCode;
  }

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "MAC Address of plug-in device is %s", GetMACAddress());
  char *sMac = utilsRemDelimitStr(GetMACAddress(), ":");
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Serial Number of plug-in device is %s", GetSerialNumber());
  char *sSer = GetSerialNumber();
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Key for plug-in device is %s", g_szPluginPrivatekey);

  assign = createAuthSignature(sMac, sSer, g_szPluginPrivatekey);
  if( !assign )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "\n Signature Structure returned NULL\n");
    goto on_return;
  }

  pUsrAppData = (UserAppData *)malloc(sizeof(UserAppData));
  if( !pUsrAppData )
  {
    goto on_return;
  }

  memset(pUsrAppData, 0x0, sizeof(UserAppData));
  pUsrAppSsnData = webAppCreateSession(0);
  if( !pUsrAppSsnData )
  {
    goto on_return;
  }

  snprintf(pUsrAppData->url, sizeof(pUsrAppData->url)-1, "%s", pszURL);
  SAFE_STRCPY(pUsrAppData->keyVal[0].key, "Content-Type");
  SAFE_STRCPY(pUsrAppData->keyVal[0].value, "application/xml");
  SAFE_STRCPY(pUsrAppData->keyVal[1].key, "Accept");
  SAFE_STRCPY(pUsrAppData->keyVal[1].value, "application/xml");
  SAFE_STRCPY(pUsrAppData->keyVal[2].key, "Authorization");
  SAFE_STRCPY(pUsrAppData->keyVal[2].value, assign->signature);
  SAFE_STRCPY(pUsrAppData->keyVal[3].key, "X-Belkin-Client-Type-Id");
  SAFE_STRCPY(pUsrAppData->keyVal[3].value, g_szClientType);

  pUsrAppData->keyValLen = 4;

  if( pReqData )
  {
    SAFE_STRCPY(pUsrAppData->inData, pReqData);
    nDataLen = strlen(pReqData);
    pUsrAppData->inDataLength = nDataLen;

    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "--- inData --- ");
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "inDataLength = %d", pUsrAppData->inDataLength);
    DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pUsrAppData->inData, pUsrAppData->inDataLength);
  }
  else
  {
    pUsrAppData->inDataLength = 0;
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "--- inData --- ");
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "inDataLength = %d....dp", pUsrAppData->inDataLength);
    DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pUsrAppData->inData, pUsrAppData->inDataLength);
  }

  char *check = strstr(pUsrAppData->url, "https://");
  if( check )
  {
    pUsrAppData->httpsFlag = 1;
  }

  pUsrAppData->disableFlag = 1;

  wret = webAppSendData(pUsrAppSsnData, pUsrAppData, nMethod);

  if( wret )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Some error encountered in sending HTTP request, errorCode %d", wret);
    goto on_return;
  }

  if( pUsrAppData->outHeaderLength )
  {
    nStatusCode = pUsrAppData->nStatusCode;

    if( nStatusCode == 100 )
    {
        nStatusCode = GetNextStatusCode(pUsrAppData->outHeader);
        if( nStatusCode == 0 )
          nStatusCode = 100;
    }
  }

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Length of HTTP Header = %d ", pUsrAppData->outHeaderLength);
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "StatusCode of HTTP Response = %d ", nStatusCode);
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Some response for HTTP request from cloud is received");

  if( pUsrAppData->outDataLength > 0 )
  {
    *ppRespData = malloc((pUsrAppData->outDataLength+1)*sizeof(char));
    if( *ppRespData )
    {
      memset(*ppRespData, 0x00, (pUsrAppData->outDataLength+1));
      memcpy(*ppRespData, pUsrAppData->outData, pUsrAppData->outDataLength);
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "malloc error for Response Body\n");
      nStatusCode = 0;
    }
  }
  else
  {
    pUsrAppData->outDataLength = 0;
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "No response body for HTTP request from cloud is received");
  }

on_return:
  if( pUsrAppData )
  {
    if( pUsrAppData->outHeaderLength > 0 )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Header retuned by server");
      DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pUsrAppData->outHeader, pUsrAppData->outHeaderLength);
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "No header data");
    }

    if( pUsrAppData->outDataLength > 0 )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Data retuned by server");
      DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pUsrAppData->outData, pUsrAppData->outDataLength);
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "No payload data");
    }

    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "--- Header Data returned by server ---");
    DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pUsrAppData->outHeader, pUsrAppData->outHeaderLength);

    // for( i = 0; i < pUsrAppData->outHeaderLength && i < DATA_BUF_LEN; i++ )
    // {
    //   printf("%c", pUsrAppData->outHeader[i]);
    // }
    // printf("\n");

    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "--- Data returned by server ---");
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "%s",pUsrAppData->outData);

    // for( i = 0; i < pUsrAppData->outDataLength && i < DATA_BUF_LEN; i++ )
    // {
    //   printf("%c",pUsrAppData->outData[i]);
    // }
    // printf("\n");
  }

  if( pUsrAppSsnData )
  {
    webAppDestroySession( pUsrAppSsnData );
  }

  if( pUsrAppData )
  {
    if( pUsrAppData->outData )
    {
      free(pUsrAppData->outData);
    }
    free(pUsrAppData);
  }

  if( assign )
  {
    free(assign);
  }

  return nStatusCode;
}


char* CreateCapsProfilesXML(void)
{
  int i = 0, nCount = 0, nRegsiterCnt = 0;

  char *pszXML = NULL;

  mxml_node_t *pXml = NULL;
  mxml_node_t *pNodeCapabilityProfiles = NULL;
  mxml_node_t *pNodeCapabilityProfile = NULL;
  mxml_node_t *pNodeCapabilityType = NULL;
  mxml_node_t *pNodeCapabilityID = NULL;
  mxml_node_t *pNodeCapabilitySpec = NULL;
  mxml_node_t *pNodeCapabilityName = NULL;
  mxml_node_t *pNodeCapabilityDataType = NULL;
  mxml_node_t *pNodeCapabilityValue = NULL;
  mxml_node_t *pNodeType = NULL;

  SD_CapabilityList CapsList;

  nCount = SD_GetCapabilityList(&CapsList);

  if( 0 == nCount )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateCapsProfilesXML: SD_GetCapabilityList is 0...");
    return pszXML;
  }

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateCapsProfilesXML: SD_GetCapabilityList = %d", nCount);

  pXml = mxmlNewXML("1.0");
  if( NULL == pXml )
  {
    return pszXML;
  }

  pNodeCapabilityProfiles = mxmlNewElement(pXml, "capabilityProfiles");

  for( i = 0; i < nCount; i++ )
  {
    if( CapsList[i]->RegisteredID.Data[0] )
    {
      nRegsiterCnt++;
      continue;
    }

    if( 0 == strcmp(CapsList[i]->ProfileName, "Identify") ||
        0 == strcmp(CapsList[i]->ProfileName, "LevelControl.Move") ||
        0 == strcmp(CapsList[i]->ProfileName, "LevelControl.Stop") )
    {
      continue;
    }

    pNodeCapabilityProfile = mxmlNewElement(pNodeCapabilityProfiles, "capabilityProfile");

    pNodeCapabilityType = mxmlNewElement(pNodeCapabilityProfile, "capabilityType");
    pNodeType = mxmlNewElement(pNodeCapabilityType, "type");
    /*
        CapabiltyType : LinearCtl, BinaryCtl, LevelCtl, ZoneCtrl
    */
    if( 0 == strcmp(CapsList[i]->ProfileName, "OnOff") )
    {
      mxmlNewText(pNodeType, 0, "BinaryCtl");
    }
    else if( 0 == strcmp(CapsList[i]->ProfileName, "LevelControl") )
    {
      mxmlNewText(pNodeType, 0, "LevelCtl");
    }
    else if( 0 == strcmp(CapsList[i]->ProfileName, "SleepFader") ||
             0 == strcmp(CapsList[i]->ProfileName, "ColorControl") ||
             0 == strcmp(CapsList[i]->ProfileName, "ColorTemperature") )
    {
      mxmlNewText(pNodeType, 0, "LinearCtl");
    }
    else if( 0 == strcmp(CapsList[i]->ProfileName, "IASZone") )
    {
      mxmlNewText(pNodeType, 0, "ZoneCtl");
    }

    pNodeCapabilityID = mxmlNewElement(pNodeCapabilityProfile, "capabilityId");
    mxmlNewText(pNodeCapabilityID, 0, CapsList[i]->CapabilityID.Data);

    pNodeCapabilityName = mxmlNewElement(pNodeCapabilityProfile, "profileName");
    mxmlNewText(pNodeCapabilityName, 0, CapsList[i]->ProfileName);

    //specifiction : 0(ZigBee), 1(Wifi)
    pNodeCapabilitySpec = mxmlNewElement(pNodeCapabilityProfile, "specification");
    mxmlNewText(pNodeCapabilitySpec, 0, "0");

    //dataType : 0(Bit), 1(Integer), 2(Discrete), 3(String)
    pNodeCapabilityDataType = mxmlNewElement(pNodeCapabilityProfile, "dataType");

    if( 0 == strcmp(CapsList[i]->ProfileName, "OnOff") )
    {
      mxmlNewText(pNodeCapabilityDataType, 0, "0");
    }
    else if( 0 == strcmp(CapsList[i]->ProfileName, "LevelControl") ||
             0 == strcmp(CapsList[i]->ProfileName, "SleepFader") ||
             0 == strcmp(CapsList[i]->ProfileName, "ColorControl") ||
             0 == strcmp(CapsList[i]->ProfileName, "ColorTemperature") )
    {
      mxmlNewText(pNodeCapabilityDataType, 0, "1");
    }

    pNodeCapabilityValue = mxmlNewElement(pNodeCapabilityProfile, "values");
    mxmlNewText(pNodeCapabilityValue, 0, CapsList[i]->NameValue);
  }

  if( nRegsiterCnt )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateCapsProfilesXML: Register nCount = %d", nRegsiterCnt);
  }
  else
  {
    if( nCount )
    {
      pszXML = mxmlSaveAllocString(pXml, MXML_NO_CALLBACK);
    }
  }

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateCapsProfilesXML: Capability nCount = %d", nCount);

  mxmlDelete(pXml);

  return pszXML;
}

int parseCapabilityProfiles(char *pszRespBody)
{
  mxml_node_t *tree = NULL;
  mxml_node_t *node = NULL;
  mxml_node_t *find_node = NULL;

  int nRet = PLUGIN_SUCCESS;

  SD_CapabilityID capID;
  SD_CapabilityID registerCapID;

  char *pId = NULL, *pCapId = NULL;

  tree = mxmlLoadString(NULL, pszRespBody, MXML_OPAQUE_CALLBACK);
  if( NULL == tree )
  {
    return PLUGIN_FAILURE;
  }

  //First, find the capabilityProfile element in xml buffer.
  for( node = mxmlFindElement(tree, tree, "capabilityProfile", NULL, NULL, MXML_DESCEND);
       node != NULL;
       node = mxmlFindElement(node, tree, "capabilityProfile", NULL, NULL, MXML_DESCEND) )
  {
    find_node = mxmlFindElement(node, tree, "id", NULL, NULL, MXML_DESCEND);
    if( find_node )
    {
      pId = (char *)mxmlGetOpaque(find_node);
      if( pId )
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "capabilityProfile Id = %s", pId);
        MakeRegisterCapabilityID(pId, &registerCapID);
      }
    }

    find_node = mxmlFindElement(node, tree, "capabilityId", NULL, NULL, MXML_DESCEND);
    if( find_node )
    {
      pCapId = (char *)mxmlGetOpaque(find_node);
      if( pCapId )
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "capabilityId = %s", pCapId);
        MakeCapabilityID(pCapId, &capID);
      }
    }

    if( pId && pCapId )
    {
      SD_SetCapabilityRegisteredID(&capID, &registerCapID);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SD_SetCapabilityRegisteredID(%s, %s)", pId, pCapId);
    }
    else
    {
      nRet = PLUGIN_FAILURE;
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "id and capabilityId are NULL...");
    }
  }

  mxmlDelete(tree);

  return nRet;
}

//=============================================================
// Register capability profiles to the Cloud server
// Resource : POST /capabilityprofiles
// URL : https://host:8443/apis/http/capabilityprofiles
// INPUT :
// OUTPUT : HTTP Status Code or 0 (Failure)
//=============================================================
int RegisterCapabilityProfiles(char **ppErrCode)
{
  char *pRespBody = NULL;
  int nStatusCode = 0;
  char szURL[128] = {0,};

  char *pCapsProfilesXML = CreateCapsProfilesXML();

  if( pCapsProfilesXML )
  {
    snprintf(szURL, sizeof(szURL), "https://%s:8443/apis/http/device/capabilityprofiles", BL_DOMAIN_NM);
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "RegisterCapabilityProfiles: URL = %s", szURL);

    nStatusCode = SendHttpRequestWithAuth(POST, szURL, pCapsProfilesXML, &pRespBody);
    if( (nStatusCode == 200) && pRespBody )
    {
      if( PLUGIN_FAILURE == parseCapabilityProfiles(pRespBody) )
      {
        nStatusCode = 0;
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure : parseCapabilityProfiles");
      }
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "RegisterCapabilityProfiles RespBody: %s", pRespBody);
      free(pRespBody);
    }
    else if( nStatusCode && pRespBody )
    {
      //Parsing Error XML
      *ppErrCode = GetErrorCodeStr(pRespBody);
      free(pRespBody);
    }
    free(pCapsProfilesXML);
  }

  return nStatusCode;
}

char* CreateSubDevicesXML(char *pluginId, struct SubDevices *pSubDevices)
{
  mxml_node_t *pXml = NULL;
  mxml_node_t *pNodeDevices = NULL;
  mxml_node_t *pNodeDevice = NULL;
  mxml_node_t *pNodeParent = NULL;
  mxml_node_t *pNodeMacAddress = NULL;
  mxml_node_t *pNodeDescription = NULL;
  mxml_node_t *pNodeModelCode = NULL;
  mxml_node_t *pNodeProductName = NULL;
  mxml_node_t *pNodeProductType = NULL;
  mxml_node_t *pNodeManufacturer = NULL;
  mxml_node_t *pNodeCertified = NULL;
  mxml_node_t *pNodeFirmwareVersion = NULL;
  mxml_node_t *pNodeIconVersion = NULL;
  mxml_node_t *pNodeCapsProfiles = NULL;
  mxml_node_t *pNodeCapsProfile = NULL;
  mxml_node_t *pNodeCapsID = NULL;
  mxml_node_t *pAttributeList = NULL;
  mxml_node_t *pAttribute = NULL;
  mxml_node_t *pNodeValue = NULL;


  int i = 0, j = 0;
  int nDevice = 0, nPaired = 0;

  char *pszXML = NULL;
  char szCapabilityID[20] = {0,};

  SD_Device device;
  SD_DeviceList devices;
  SD_DeviceID deviceId;

  //Getting all unregistered and paired device list from subdevice lib
  if( NULL == pSubDevices )
  {
    nDevice = SD_GetDeviceList(&devices);
  }
  else
  {
    nDevice = pSubDevices->nDevice;
  }

  pXml = mxmlNewXML("1.0");
  if( NULL == pXml )
  {
    return pszXML;
  }

  pNodeDevices = mxmlNewElement(pXml, "devices");

  if( pSubDevices )
  {
    for( i = 0; i < nDevice; i++ )
    {
      MakeDeviceID(pSubDevices->szDeviceIDs[i], &deviceId);

      if( SD_GetDevice(&deviceId, &device) && device->Paired )
      {
        pNodeDevice = mxmlNewElement(pNodeDevices, "device");

        pNodeParent = mxmlNewElement(pNodeDevice, "parentPluginId");
        mxmlNewText(pNodeParent, 0, pluginId);

        pNodeMacAddress = mxmlNewElement(pNodeDevice, "macAddress");
        mxmlNewText(pNodeMacAddress, 0, pSubDevices->szDeviceIDs[i]);

        pNodeDescription = mxmlNewElement(pNodeDevice, "friendlyName");
        mxmlNewText(pNodeDescription, 0, device->FriendlyName);

        pNodeModelCode = mxmlNewElement(pNodeDevice, "modelCode");
        mxmlNewText(pNodeModelCode, 0, device->ModelCode);

        pNodeProductName = mxmlNewElement(pNodeDevice, "productName");
        mxmlNewText(pNodeProductName, 0, getProductName((char*)device->ModelCode));

        pNodeProductType = mxmlNewElement(pNodeDevice, "productType");

        if ( 0 == strcmp (device->ModelCode, SENSOR_MODELCODE_DW) ||
             0 == strcmp (device->ModelCode, SENSOR_MODELCODE_FOB) ||
             0 == strcmp (device->ModelCode, SENSOR_MODELCODE_ALARM) ||
             0 == strcmp (device->ModelCode, SENSOR_MODELCODE_PIR) )
        {
          mxmlNewText(pNodeProductType, 0, "AlertSensor");
        }
        else
        {
          mxmlNewText(pNodeProductType, 0, "Lighting");
        }

        pNodeFirmwareVersion = mxmlNewElement(pNodeDevice, "firmwareVersion");
        mxmlNewText(pNodeFirmwareVersion, 0, device->FirmwareVersion);

        pNodeIconVersion = mxmlNewElement(pNodeDevice, "iconVersion");
        mxmlNewText(pNodeIconVersion, 0, "Unknown");

        pNodeCapsProfiles = mxmlNewElement(pNodeDevice, "deviceCapabilityProfiles");
        for( j = 0; j < device->CapabilityCount; j++ )
        {
          if( 0 != strcmp(device->Capability[j]->ProfileName, "Identify") &&
              0 != strcmp(device->Capability[j]->ProfileName, "LevelControl.Move") &&
              0 != strcmp(device->Capability[j]->ProfileName, "LevelControl.Stop") )
          {
            pNodeCapsProfile = mxmlNewElement(pNodeCapsProfiles, "deviceCapabilityProfile");
            pNodeCapsID = mxmlNewElement(pNodeCapsProfile, "capabilityId");
            snprintf(szCapabilityID, sizeof(szCapabilityID), "%s", device->Capability[j]->CapabilityID.Data);
            mxmlNewText(pNodeCapsID, 0, szCapabilityID);
          }
        }

        pAttributeList = mxmlNewElement(pNodeDevice, "attributeLists");
        pAttribute = mxmlNewElement(pAttributeList, "attribute");
        pNodeManufacturer = mxmlNewElement(pAttribute, "name");
        mxmlNewText(pNodeManufacturer, 0, "Manufacturer");
        pNodeValue = mxmlNewElement(pAttribute, "value");
        mxmlNewText(pNodeValue, 0, device->ManufacturerName);

        pAttribute = mxmlNewElement(pAttributeList, "attribute");
        pNodeCertified = mxmlNewElement(pAttribute, "name");
        mxmlNewText(pNodeCertified, 0, "WeMoCertified");
        pNodeValue = mxmlNewElement(pAttribute, "value");
        if( device->Certified )
        {
          mxmlNewText(pNodeValue, 0, "YES");
        }
        else
        {
          mxmlNewText(pNodeValue, 0, "NO");
        }

        nPaired++;
      }
    }
  }
  else
  {
    for( i = 0; i < nDevice; i++ )
    {
      if( devices[i]->Paired )
      {
        pNodeDevice = mxmlNewElement(pNodeDevices, "device");

        pNodeParent = mxmlNewElement(pNodeDevice, "parentPluginId");
        mxmlNewText(pNodeParent, 0, pluginId);

        pNodeMacAddress = mxmlNewElement(pNodeDevice, "macAddress");
        mxmlNewText(pNodeMacAddress, 0, devices[i]->EUID.Data);

        pNodeDescription = mxmlNewElement(pNodeDevice, "friendlyName");
        mxmlNewText(pNodeDescription, 0, devices[i]->FriendlyName);

        pNodeModelCode = mxmlNewElement(pNodeDevice, "modelCode");
        mxmlNewText(pNodeModelCode, 0, devices[i]->ModelCode);

        pNodeProductName = mxmlNewElement(pNodeDevice, "productName");
        mxmlNewText(pNodeProductName, 0, getProductName((char*)devices[i]->ModelCode));

        pNodeProductType = mxmlNewElement(pNodeDevice, "productType");

        if ( 0 == strcmp (devices[i]->ModelCode, SENSOR_MODELCODE_DW) ||
             0 == strcmp (devices[i]->ModelCode, SENSOR_MODELCODE_FOB) ||
             0 == strcmp (devices[i]->ModelCode, SENSOR_MODELCODE_ALARM) ||
             0 == strcmp (devices[i]->ModelCode, SENSOR_MODELCODE_PIR) )
        {
          mxmlNewText(pNodeProductType, 0, "AlertSensor");
        }
        else
        {
          mxmlNewText(pNodeProductType, 0, "Lighting");
        }

        pNodeFirmwareVersion = mxmlNewElement(pNodeDevice, "firmwareVersion");
        mxmlNewText(pNodeFirmwareVersion, 0, devices[i]->FirmwareVersion);

        pNodeIconVersion = mxmlNewElement(pNodeDevice, "iconVersion");
        mxmlNewText(pNodeIconVersion, 0, "Unknown");

        pNodeCapsProfiles = mxmlNewElement(pNodeDevice, "deviceCapabilityProfiles");
        for( j = 0; j < devices[i]->CapabilityCount; j++ )
        {
          if( 0 != strcmp(devices[i]->Capability[j]->ProfileName, "Identify") &&
              0 != strcmp(devices[i]->Capability[j]->ProfileName, "LevelControl.Move") &&
              0 != strcmp(devices[i]->Capability[j]->ProfileName, "LevelControl.Stop") )
          {
            pNodeCapsProfile = mxmlNewElement(pNodeCapsProfiles, "deviceCapabilityProfile");
            pNodeCapsID = mxmlNewElement(pNodeCapsProfile, "capabilityId");
            snprintf(szCapabilityID, sizeof(szCapabilityID), "%s", devices[i]->Capability[j]->CapabilityID.Data);
            mxmlNewText(pNodeCapsID, 0, szCapabilityID);
          }
        }

        pAttributeList = mxmlNewElement(pNodeDevice, "attributeLists");
        pAttribute = mxmlNewElement(pAttributeList, "attribute");
        pNodeManufacturer = mxmlNewElement(pAttribute, "name");
        mxmlNewText(pNodeManufacturer, 0, "Manufacturer");
        pNodeValue = mxmlNewElement(pAttribute, "value");
        mxmlNewText(pNodeValue, 0, devices[i]->ManufacturerName);

        pAttribute = mxmlNewElement(pAttributeList, "attribute");
        pNodeCertified = mxmlNewElement(pAttribute, "name");
        mxmlNewText(pNodeCertified, 0, "WeMoCertified");
        pNodeValue = mxmlNewElement(pAttribute, "value");
        if( devices[i]->Certified )
        {
          mxmlNewText(pNodeValue, 0, "YES");
        }
        else
        {
          mxmlNewText(pNodeValue, 0, "NO");
        }

        nPaired++;
      }
    }
  }

  if( nPaired )
  {
    pszXML = mxmlSaveAllocString(pXml, MXML_NO_CALLBACK);
  }

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateSubDevicesXML: Device nCount = %d", nDevice);

  mxmlDelete(pXml);

  return pszXML;
}

int parseSubDevicesResp(char *pszRespBody)
{
  mxml_node_t *tree = NULL;
  mxml_node_t *node = NULL;
  mxml_node_t *cap_node = NULL;
  mxml_node_t *find_node = NULL;

  int nRet = PLUGIN_SUCCESS;

  SD_DeviceID devID;
  SD_DeviceID registerDevID;
  SD_CapabilityID capID;
  SD_CapabilityID registerCapID;

  char *pPluginId = NULL, *pMacAddr = NULL;
  char *pId = NULL, *pCapabilityId = NULL;

  tree = mxmlLoadString(NULL, pszRespBody, MXML_OPAQUE_CALLBACK);
  if( NULL == tree )
  {
    return PLUGIN_FAILURE;
  }

  //First, find the DeviceStatus element in xml buffer.
  for( node = mxmlFindElement(tree, tree, "device", NULL, NULL, MXML_DESCEND);
       node != NULL;
       node = mxmlFindElement(node, tree, "device", NULL, NULL, MXML_DESCEND) )
  {
    find_node = mxmlFindElement(node, tree, "pluginId", NULL, NULL, MXML_DESCEND);
    if( find_node )
    {
      pPluginId = (char *)mxmlGetOpaque(find_node);
    }

    find_node = mxmlFindElement(node, tree, "macAddress", NULL, NULL, MXML_DESCEND);
    if( find_node )
    {
      pMacAddr = (char *)mxmlGetOpaque(find_node);
    }

    if( pPluginId && pMacAddr )
    {
      MakeDeviceID(pMacAddr, &devID);
      MakeRegisterDeviceID(pPluginId, &registerDevID);

      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pluginId = %s", pPluginId);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "macAddress = %s", pMacAddr);

      if( SD_SetDeviceRegisteredID(&devID, &registerDevID) )
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Success : SD_SetDeviceRegisteredID(%s,%s)", pPluginId, pMacAddr);
      }
      else
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure : SD_SetDeviceRegisteredID(%s,%s)", pPluginId, pMacAddr);
      }
    }

    for( cap_node = mxmlFindElement(tree, tree, "deviceCapabilityProfile", NULL, NULL, MXML_DESCEND);
         cap_node != NULL;
         cap_node = mxmlFindElement(cap_node, tree, "deviceCapabilityProfile", NULL, NULL, MXML_DESCEND) )
    {
      find_node = mxmlFindElement(cap_node, tree, "Id", NULL, NULL, MXML_DESCEND);
      if( find_node )
      {
        pId = (char *)mxmlGetOpaque(find_node);
      }

      find_node = mxmlFindElement(cap_node, tree, "capabilityId", NULL, NULL, MXML_DESCEND);
      if( find_node )
      {
        pCapabilityId = (char *)mxmlGetOpaque(find_node);
      }

      if( pId && pCapabilityId )
      {
        MakeCapabilityID(pCapabilityId, &capID);
        MakeRegisterCapabilityID(pId, &registerCapID);

        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Id = %s", pId);
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "capabilityId = %s", pCapabilityId);

        if( SD_SetCapabilityRegisteredID(&capID, &registerCapID) )
        {
          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Success : SD_SetCapabilityRegisteredID(%s,%s)", pId, pCapabilityId);
        }
        else
        {
          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure : SD_SetCapabilityRegisteredID(%s,%s)", pId, pCapabilityId);
        }
      }
      else
      {
        nRet = PLUGIN_FAILURE;
      }
    }
  }

  mxmlDelete(tree);

  return nRet;
}

//=============================================================
// Register the SubDevices to the Cloud server
// Resource : POST /devices
// URL : https://host:8443/apis/http/device/devices
// INPUT :
//=============================================================
int RegisterSubDevices(char *pluginId, struct SubDevices *pSubDevices, char **ppErrCode)
{
  char *pRespBody = NULL;
  int nStatusCode = 0;
  char szURL[128] = {0,};

  char *pSubDevicesXML = CreateSubDevicesXML(pluginId, pSubDevices);

  *ppErrCode = NULL;

  if( pSubDevicesXML )
  {
    snprintf(szURL, sizeof(szURL), "https://%s:8443/apis/http/device/devices", BL_DOMAIN_NM);
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "RegisterSubDevices: URL = %s", szURL);
    DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pSubDevicesXML, strlen(pSubDevicesXML));

    nStatusCode = SendHttpRequestWithAuth(POST, szURL, pSubDevicesXML, &pRespBody);
    if( nStatusCode == 200 && pRespBody )
    {
      if( PLUGIN_FAILURE == parseSubDevicesResp(pRespBody) )
      {
        nStatusCode = 0;
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure : parseSubDevicesResp");
      }

      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "RegisterSubDevices RespBody : %s", pRespBody);
      free(pRespBody);
    }
    else if( nStatusCode && pRespBody )
    {
      //Parsing Error XML
      *ppErrCode = GetErrorCodeStr(pRespBody);

      free(pRespBody);
    }
    free(pSubDevicesXML);
  }

  return nStatusCode;
}

//=======================================================================
// Resource : POST /device/capabilityprofilefilter
// URL      : https://host:8443/apis/http/device/capabilityprofilefilter
// INPUT    :
// <?xml version="1.0" encoding="UTF-8"?>
// <capabilityFilter>
//     <capabilityIds>
//         <capabilityId></capabilityId>
//         <capabilityId></capabilityId>
//     </capabilityIds>
// </capabilityFilter>
//=======================================================================
char* CreateQueryCapabilityProfileXML()
{
  int i = 0, nCount = 0;
  mxml_node_t *pXml = NULL;
  mxml_node_t *pRootNode = NULL;
  mxml_node_t *pNodeCapabilityIDs = NULL;
  mxml_node_t *pNodeCapabilityID = NULL;

  char *pszXML = NULL;

  SD_CapabilityList list;

  nCount = SD_GetCapabilityList(&list);

  if( nCount == 0 )
  {
    return pszXML;
  }

  pXml = mxmlNewXML("1.0");

  if( NULL == pXml )
  {
    return pszXML;
  }

  pRootNode = mxmlNewElement(pXml, "capabilityFilter");
  pNodeCapabilityIDs = mxmlNewElement(pRootNode, "capabilityIds");

  for( i = 0; i < nCount; i++ )
  {
    if( 0 == strcmp(list[i]->ProfileName, "Identify") ||
        0 == strcmp(list[i]->ProfileName, "LevelControl.Move") ||
        0 == strcmp(list[i]->ProfileName, "LevelControl.Stop") )
    {
      continue;
    }

    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CapabilityID = %s", list[i]->CapabilityID.Data);

    pNodeCapabilityID = mxmlNewElement(pNodeCapabilityIDs, "capabilityId");
    mxmlNewText(pNodeCapabilityID, 0, list[i]->CapabilityID.Data);
  }

  pszXML = mxmlSaveAllocString(pXml, MXML_NO_CALLBACK);

  mxmlDelete(pXml);

  return pszXML;
}

int parseCapabilityProfileResp(char *pszRespBody, int *pnUpdate)
{
  mxml_node_t *tree = NULL;
  mxml_node_t *node = NULL;
  mxml_node_t *find_node = NULL;

  int nRet = PLUGIN_SUCCESS;
  int i = 0, nCount = 0, nMatached = 0;

  char *pCapId = NULL;

  tree = mxmlLoadString(NULL, pszRespBody, MXML_OPAQUE_CALLBACK);

  if( NULL == tree )
  {
    return PLUGIN_FAILURE;
  }

  SD_CapabilityList list;

  nCount = SD_GetCapabilityList(&list);

  //First, find the capabilityProfile element in xml buffer.
  for( node = mxmlFindElement(tree, tree, "capabilityProfile", NULL, NULL, MXML_DESCEND);
       node != NULL;
       node = mxmlFindElement(node, tree, "capabilityProfile", NULL, NULL, MXML_DESCEND) )
  {
    find_node = mxmlFindElement(node, tree, "capabilityId", NULL, NULL, MXML_DESCEND);
    if( find_node )
    {
      pCapId = (char *)mxmlGetOpaque(find_node);
      if( pCapId )
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "capabilityId = %s", pCapId);
        for( i = 0; i < nCount; i++ )
        {
          if( 0 == strcmp(list[i]->ProfileName, "Identify") ||
              0 == strcmp(list[i]->ProfileName, "LevelControl.Move") ||
              0 == strcmp(list[i]->ProfileName, "LevelControl.Stop") )
          {
            continue;
          }
          if( 0 == strcmp(list[i]->CapabilityID.Data, pCapId) )
          {
            nMatached++;
          }
        }
      }
    }
  }

  if( nMatached == (nCount - 3) )
  {
    *pnUpdate = 0;
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "No Updated: Toalal = [%d], nMatached = [%d]", (nCount-3), nMatached);
  }
  else
  {
    *pnUpdate = 1;
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Updated: Toalal = [%d],  nMatached = [%d]", (nCount-3), nMatached);
  }

  mxmlDelete(tree);

  return nRet;
}

int QueryCapabilityProfileList(char **ppErrCode)
{
  char *pRespBody = NULL;
  int nRet = 0, nStatusCode = 0, nUpdate = 0;
  char szURL[128] = {0,};

  char *pQueryXML = CreateQueryCapabilityProfileXML();

  *ppErrCode = NULL;

  if( pQueryXML )
  {
    snprintf(szURL, sizeof(szURL), "https://%s:8443/apis/http/device/capabilityprofilefilter", BL_DOMAIN_NM);
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "QueryCapabilityProfileList: URL = %s", szURL);
    DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pQueryXML, strlen(pQueryXML));

    nStatusCode = SendHttpRequestWithAuth(POST, szURL, pQueryXML, &pRespBody);
    if( nStatusCode == 200 && pRespBody )
    {
      nRet = parseCapabilityProfileResp(pRespBody, &nUpdate);
      if( nRet == PLUGIN_FAILURE )
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure : parseCapabilitProfileResp");
      }
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "QueryCapabilityProfileList RespBody : %s", pRespBody);
      free(pRespBody);
    }
    else if( nStatusCode && pRespBody )
    {
      //Parsing Error XML
      *ppErrCode = GetErrorCodeStr(pRespBody);
      free(pRespBody);
    }

    free(pQueryXML);
  }

  if( nUpdate )
  {
    if( (nStatusCode = RegisterCapabilityProfiles(ppErrCode)) )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "RegisterCapabilityProfiles(nStatusCode = %d)", nStatusCode);
      if( *ppErrCode )
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure: RegisterCapabilityProfiles(ErrorCode = %s)", *ppErrCode);
      }
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure: RegisterCapabilityProfiles");
    }
  }

  return nStatusCode;
}
int DeleteSubDevices(char *pPluginId, char **ppErrCode)
{
  char *pRespBody = NULL;
  int nStatusCode = 0;
  char szURL[128] = {0,};

  snprintf(szURL, sizeof(szURL), "https://%s:8443/apis/http/device/devices/%s", BL_DOMAIN_NM, pPluginId);
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "DeleteSubDevices: URL = %s", szURL);

  nStatusCode = SendHttpRequestWithAuth(DELETE, szURL, NULL, &pRespBody);
  if( (nStatusCode != 200) && pRespBody )
  {
    //Parsing Error XML
    *ppErrCode = GetErrorCodeStr(pRespBody);
    free(pRespBody);
  }

  return nStatusCode;
}

//=============================================================
// Delete a SubDevice from the Cloud server
// Resource : DELETE /devices/<deviceid>
// URL : https://host:8443/apis/http/device/devices/<deviceid>
// INPUT : pCloudDeviceID is generated by the Cloud Server.
//=============================================================
int DeleteSubDevice(const char *pCloudDeviceID, char **ppErrCode)
{
  char *pRespBody = NULL;
  int nStatusCode = 0;
  char szURL[128] = {0,};

  snprintf(szURL, sizeof(szURL), "https://%s:8443/apis/http/device/devices/%s", BL_DOMAIN_NM, pCloudDeviceID);
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "DeleteSubDevice: URL = %s", szURL);

  nStatusCode = SendHttpRequestWithAuth(DELETE, szURL, NULL, &pRespBody);
  if( (nStatusCode != 200) && pRespBody )
  {
    //Parsing Error XML
    *ppErrCode = GetErrorCodeStr(pRespBody);
    free(pRespBody);
  }

  return nStatusCode;
}

char* CreateGroupsReqXML(char *pGroupID)
{
  mxml_node_t *pXml = NULL;
  mxml_node_t *pNode = NULL;
  mxml_node_t *pNodeGroups = NULL;
  mxml_node_t *pNodeGroup = NULL;
  mxml_node_t *pNodeDevices = NULL;
  mxml_node_t *pNodeDevice = NULL;
  mxml_node_t *pNodeCapabilities = NULL;
  mxml_node_t *pNodeCapability = NULL;

  int i = 0;
  char *pszXML = NULL;
  char szCurrentValue[CAPABILITY_MAX_VALUE_LENGTH] = {0,};

  SD_Group group;
  SD_GroupID groupID;

  pXml = mxmlNewXML("1.0");

  if( NULL == pXml )
  {
    return pszXML;
  }

  pNodeGroups = mxmlNewElement(pXml, "groups");

  MakeGroupID(pGroupID, &groupID);

  if( SD_GetGroup(&groupID, &group) )
  {
    pNodeGroup = mxmlNewElement(pNodeGroups, "group");
    pNode = mxmlNewElement(pNodeGroup, "referenceId");
    mxmlNewText(pNode, 0, pGroupID);
    pNode = mxmlNewElement(pNodeGroup, "groupName");
    mxmlNewText(pNode, 0, group->Name);
    pNode = mxmlNewElement(pNodeGroup, "iconVersion");
    mxmlNewText(pNode, 0, "1");

    pNodeDevices = mxmlNewElement(pNodeGroup, "devices");

    for( i = 0; i < group->DeviceCount; i++ )
    {
      pNodeDevice = mxmlNewElement(pNodeDevices, "device");
      pNode = mxmlNewElement(pNodeDevice, "deviceId");
      mxmlNewText(pNode, 0, group->Device[i]->RegisteredID.Data);
    }

    pNodeCapabilities = mxmlNewElement(pNodeGroup, "capabilities");

    for( i = 0; i < group->CapabilityCount; i++ )
    {
      if( 0 != strcmp(group->Capability[i]->ProfileName, "Identify") &&
          0 != strcmp(group->Capability[i]->ProfileName, "LevelControl.Move") &&
          0 != strcmp(group->Capability[i]->ProfileName, "SensorTestMode") &&
          0 != strcmp(group->Capability[i]->ProfileName, "LevelControl.Stop") )
      {
        pNodeCapability = mxmlNewElement(pNodeCapabilities, "capability");
        pNode = mxmlNewElement(pNodeCapability, "capabilityId");
        mxmlNewText(pNode, 0, group->Capability[i]->CapabilityID.Data);
        pNode = mxmlNewElement(pNodeCapability, "currentValue");

        memset(szCurrentValue, 0x00, sizeof(szCurrentValue));

        if( SD_GetGroupCapabilityValueCache(&groupID, &group->Capability[i]->CapabilityID, szCurrentValue, SD_CACHE) )
        {
          if( (0 == strcmp(group->Capability[i]->ProfileName, "SleepFader")) &&
              (0 == szCurrentValue[0] || 0 == strlen(szCurrentValue)) )
          {
            snprintf(szCurrentValue, sizeof(szCurrentValue), "%s", "0:0");
          }
          mxmlNewText(pNode, 0, szCurrentValue);
        }
        else
        {
          mxmlNewText(pNode, 0, "3");
        }
    }
    }

    pszXML = mxmlSaveAllocString(pXml, MXML_NO_CALLBACK);
  }
  else
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateGroupsReqXML: groupID is not existed in Groups = %d", pGroupID);
  }

  mxmlDelete(pXml);

  return pszXML;
}

//=====================================================================
// Add Groups
// Resource : POST /groups/<homeid>
// URL : https://host:8443/apis/http/groups
// INPUT : pHomeID is generated by the Cloud Server.
//=====================================================================
int CreateGroups(int nMethod, char *pHomeID, char *pGroupID, char **ppErrCode)
{
  char *pRespBody = NULL;
  int nStatusCode = 0;
  char szURL[128] = {0,};

  char *pGroupsXML = CreateGroupsReqXML(pGroupID);

  *ppErrCode = NULL;

  if( pGroupsXML )
  {
    snprintf(szURL, sizeof(szURL), "https://%s:8443/apis/http/device/groups/%s", BL_DOMAIN_NM, pHomeID);
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateGroups: URL = %s", szURL);
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateGroups: reqXML....dp:::: %s", pGroupsXML);
    nStatusCode = SendHttpRequestWithAuth(nMethod, szURL, pGroupsXML, &pRespBody);
    if( nStatusCode == 200 && pRespBody )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateGroups: RespBody : %s", pRespBody);
      free(pRespBody);
    }
    else if( nStatusCode && pRespBody )
    {
      //Parsing Error XML
      *ppErrCode = GetErrorCodeStr(pRespBody);
      free(pRespBody);
    }
    free(pGroupsXML);
  }

  return nStatusCode;
}

//=====================================================================
// Delete Groups
// Resource : DELETE /groups/<homeid>/<groupid>
// URL : https://host:8443/apis/http/groups
// INPUT : pHomeID is generated by Cloud Server.
//         pGroupID is generated by iOS App.
//=====================================================================
int DeleteGroups(char *pHomeID, char *pGroupID, char **ppErrCode)
{
  char *pRespBody = NULL;
  int nStatusCode = 0;
  char szURL[128] = {0,};

  *ppErrCode = NULL;

  snprintf(szURL, sizeof(szURL), "https://%s:8443/apis/http/device/groups/%s/%s", BL_DOMAIN_NM, pHomeID, pGroupID);
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "DeleteGroups: URL = %s", szURL);

    
  nStatusCode = SendHttpRequestWithAuth(DELETE, szURL, NULL, &pRespBody);
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "DeleteGroups-->pRespBody:\n %s", pRespBody);
  if( (nStatusCode != 200) && pRespBody )
  {
    //Parsing Error XML
    *ppErrCode = GetErrorCodeStr(pRespBody);
    free(pRespBody);
  }
  else if( pRespBody )
  {
    free(pRespBody);
  }

  return nStatusCode;
}

void makeUpdateDeviceNode(mxml_node_t *pRootNode)
{
  mxml_node_t *pNode = NULL;
  mxml_node_t *pNodeDevice = NULL;
  // mxml_node_t *pNodeCapProfiles = NULL;

  int nTimeStamp, fw_state = 0;
  char szTimeStamp[SIZE_64B] = {0,};

  pNodeDevice = mxmlNewElement(pRootNode, "device");

  pNode = mxmlNewElement(pNodeDevice, "parentPluginId");
  mxmlNewText(pNode, 0, "");

  pNode = mxmlNewElement(pNodeDevice, "macAddress");
  mxmlNewText(pNode, 0, g_szWiFiMacAddress);

  pNode = mxmlNewElement(pNodeDevice, "serialNumber");
  mxmlNewText(pNode, 0, g_szSerialNo);

  pNode = mxmlNewElement(pNodeDevice, "description");
  mxmlNewText(pNode, 0, "Bridge for LEDLight");

  pNode = mxmlNewElement(pNodeDevice, "modelCode");
  mxmlNewText(pNode, 0, "Bridge");

  pNode = mxmlNewElement(pNodeDevice, "productName");
  mxmlNewText(pNode, 0, "Bridge");

  pNode = mxmlNewElement(pNodeDevice, "status");
  mxmlNewText(pNode, 0, "1");

  pNode = mxmlNewElement(pNodeDevice, "statusTS");
  nTimeStamp = (int)GetUTCTime();
  snprintf(szTimeStamp, sizeof(szTimeStamp), "%d", nTimeStamp);
  mxmlNewText(pNode, 0, szTimeStamp);

  pNode = mxmlNewElement(pNodeDevice, "firmwareVersion");
  mxmlNewText(pNode, 0, g_szFirmwareVersion);

  pNode = mxmlNewElement(pNodeDevice, "fwUpgradeStatus");
  // mxmlNewText(pNode, 0, "4");
  fw_state = getCurrFWUpdateState();
  mxmlNewInteger(pNode, fw_state);

  pNode = mxmlNewElement(pNodeDevice, "signalStrength");
  mxmlNewInteger(pNode, gSignalStrength);
}

bool makeSubdeviceNode(mxml_node_t *pRootNode, char *pPluginId, char *pDeviceId, struct DeviceInfo *pDevInfo)
{
  int nTimeStamp = 0;
  int j = 0;
  int nStatus = 0, nFWUpdateState = 0;
  char szTimeStamp[SIZE_64B] = {0,};
  bool bResult = false;
  char szCurrentValue[CAPABILITY_MAX_VALUE_LENGTH] = {0,};

  mxml_node_t *pNode = NULL;
  mxml_node_t *pNodeDevice = NULL;
  mxml_node_t *pNodeCapProfiles = NULL;
  mxml_node_t *pNodeCapProfile = NULL;
  int ruleId = 0;
  int cap_value = 0;

  SD_Device device;
  SD_DeviceID deviceID;
  MakeDeviceID(pDeviceId, &deviceID);

  if( SD_GetDevice(&deviceID, &device) )
  {
    pNodeDevice = mxmlNewElement(pRootNode, "device");

    pNode = mxmlNewElement(pNodeDevice, "parentPluginId");
    mxmlNewText(pNode, 0, pPluginId);

    pNode = mxmlNewElement(pNodeDevice, "macAddress");
    mxmlNewText(pNode, 0, device->EUID.Data);

    pNode = mxmlNewElement(pNodeDevice, "friendlyName");
    mxmlNewText(pNode, 0, device->FriendlyName);

    pNode = mxmlNewElement(pNodeDevice, "modelCode");
    mxmlNewText(pNode, 0, device->ModelCode);

    pNode = mxmlNewElement(pNodeDevice, "productName");
    mxmlNewText(pNode, 0, getProductName((char*)device->ModelCode));

    pNode = mxmlNewElement(pNodeDevice, "productType");
    if ( 0 == strcmp (device->ModelCode, SENSOR_MODELCODE_DW) ||
         0 == strcmp (device->ModelCode, SENSOR_MODELCODE_FOB) ||
         0 == strcmp (device->ModelCode, SENSOR_MODELCODE_ALARM) ||
         0 == strcmp (device->ModelCode, SENSOR_MODELCODE_PIR) )
    {
      mxmlNewText(pNode, 0, "AlertSensor");
    }
    else
    {
      mxmlNewText(pNode, 0, "Lighting");
    }

    pNode = mxmlNewElement(pNodeDevice, "status");
    nStatus = GetSubDeviceAvailableStatus(device);
    mxmlNewInteger(pNode, nStatus);

    pNode = mxmlNewElement(pNodeDevice, "statusTS");
    nTimeStamp = (int)GetUTCTime();
    snprintf(szTimeStamp, sizeof(szTimeStamp), "%d", nTimeStamp);
    mxmlNewText(pNode, 0, szTimeStamp);

    pNode = mxmlNewElement(pNodeDevice, "firmwareVersion");
    mxmlNewText(pNode, 0, device->FirmwareVersion);

    pNode = mxmlNewElement(pNodeDevice, "fwUpgradeStatus");
    //nFWUpdateState = GetCurrSubDeviceFWUpdateState();
    nFWUpdateState = GetSubDeviceFWUpdateState(pDeviceId);
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "makeSubdeviceNode: Bulb FirmwareUpgrade = %d", nFWUpdateState);
    mxmlNewInteger(pNode, nFWUpdateState);

    pNode = mxmlNewElement(pNodeDevice, "signalStrength");
    mxmlNewText(pNode, 0, "100");

    pNodeCapProfiles = mxmlNewElement(pNodeDevice, "deviceCapabilityProfiles");


    for (j=0; j<device->CapabilityCount; j++)
    {
      if (0 == strcmp(device->Capability[j]->ProfileName, "Identify") ||
          0 == strcmp(device->Capability[j]->ProfileName, "LevelControl.Move") ||
          0 == strcmp(device->Capability[j]->ProfileName, "LevelControl.Stop") ) {
        continue;
      }
     if (0 == strcmp(device->Capability[j]->CapabilityID.Data, pDevInfo->CapInfo.szCapabilityID))
     {
         pNodeCapProfile = mxmlNewElement(pNodeCapProfiles, "deviceCapabilityProfile");

         pNode = mxmlNewElement(pNodeCapProfile, "capabilityId");
         //DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "makeSubdeviceNode: CapabilityID.Data = %s", device->Capability[j]->CapabilityID.Data);
         mxmlNewText(pNode, 0, device->Capability[j]->CapabilityID.Data);

         //pNodeCapValue = mxmlNewElement(pNodeCapProfile, "currentValue");
         pNode = mxmlNewElement(pNodeCapProfile, "currentValue");
          ConvertCapabilityValue(pDevInfo->CapInfo.szCapabilityValue, ',', ':');
          mxmlNewText(pNode, 0, pDevInfo->CapInfo.szCapabilityValue);
      }
      else
      {
       continue;
        //   SD_GetDeviceCapabilityValue(&deviceID, &(device->Capability[j]->CapabilityID), szCurrentValue, 0, SD_CACHE);
        //   ConvertCapabilityValue(szCurrentValue, ',', ':');
        //   mxmlNewText(pNode, 0, szCurrentValue);
      }

      sscanf(pDevInfo->CapInfo.szCapabilityValue, "%x", &cap_value);

      APP_LOG("DEVICE:makeSubdeviceNode", LOG_DEBUG, "Capability Value:%x",cap_value);

      if ((0 == strcmp (pDevInfo->CapInfo.szCapabilityID, CAPABILITY_ID_STATUS_CHANGE)) &&
          (0 == strcmp (pDevInfo->CapInfo.szCapabilityID, device->Capability[j]->CapabilityID.Data)))
      {
        /*Updating the rule information if rule is present */
        if (SUCCESS == doesNotificationRuleExist(device, cap_value, &ruleId))
	      {
	        if ( false == makerulenode(pNodeDevice, device, cap_value, ruleId))
	          {
	             DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "makerulenode is NOT available");
	          }
	       }
      }

      if (0 == strcmp(device->Capability[j]->ProfileName, "IASZone") ) {
        int n = snprintf(NULL, 0, "%lu", device->LastReportedTime);
        char buf[n+1];
        snprintf(buf, n+1, "%lu", device->LastReportedTime);
        pNode = mxmlNewElement(pNodeCapProfile, "ts");

        mxmlNewText (pNode, 0, buf);
        //DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "makeSubdeviceNode: LastReportedTime =  %lu, %s", device->LastReportedTime, buf);
      }
      else if (0 == strcmp(device->Capability[j]->ProfileName, "SensorKeyPress") ) {
        int n = snprintf(NULL, 0, "%lu", device->LastReportedTime);
        char buf[n+1];
        snprintf(buf, n+1, "%lu", device->LastReportedTimeExt);
        pNode = mxmlNewElement(pNodeCapProfile, "ts");
        mxmlNewText (pNode, 0, buf);
        //DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "=makeSubdeviceNode: LastReportedTimeExt = %lu, %s", device->LastReportedTimeExt, buf);
      }
    }
    bResult = true;
  }

  else
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "makeSubdeviceNode is NOT available");
  }

  return bResult;
}

bool makeGroupdeviceNode(mxml_node_t *pRootNode, char *pPluginId, char *pDeviceId, struct DeviceInfo *pDevInfo)
{
  int i = 0;
  int nDevice = 0, nTimeStamp = 0;
  int nStatus = 0;
  char szTimeStamp[SIZE_64B] = {0,};
  bool bResult = false;
  int ruleId = 0;
  int cap_value = 0;

  mxml_node_t *pNode = NULL;
  mxml_node_t *pNodeDevice = NULL;
  mxml_node_t *pNodeCapProfiles = NULL;
  mxml_node_t *pNodeCapProfile = NULL;

  SD_Device device;
  SD_Group group;
  SD_GroupID groupID;

  MakeGroupID(pDeviceId, &groupID);

  if( SD_GetGroup(&groupID, &group) )
  {
    nDevice = group->DeviceCount;
    for( i = 0; i < nDevice; i++ )
    {
      device = group->Device[i];

      pNodeDevice = mxmlNewElement(pRootNode, "device");

      pNode = mxmlNewElement(pNodeDevice, "parentPluginId");
      mxmlNewText(pNode, 0, pPluginId);

      pNode = mxmlNewElement(pNodeDevice, "macAddress");
      mxmlNewText(pNode, 0, device->EUID.Data);

      pNode = mxmlNewElement(pNodeDevice, "friendlyName");
      mxmlNewText(pNode, 0, device->FriendlyName);

      pNode = mxmlNewElement(pNodeDevice, "modelCode");
      mxmlNewText(pNode, 0, device->ModelCode);

      pNode = mxmlNewElement(pNodeDevice, "productName");
      mxmlNewText(pNode, 0, getProductName((char*)device->ModelCode));

      pNode = mxmlNewElement(pNodeDevice, "productType");

      if ( 0 == strcmp (device->ModelCode, SENSOR_MODELCODE_DW) ||
           0 == strcmp (device->ModelCode, SENSOR_MODELCODE_FOB) ||
           0 == strcmp (device->ModelCode, SENSOR_MODELCODE_ALARM) ||
           0 == strcmp (device->ModelCode, SENSOR_MODELCODE_PIR) )
      {
        mxmlNewText(pNode, 0, "AlertSensor");
      }
      else
      {
        mxmlNewText(pNode, 0, "Lighting");
      }

      pNode = mxmlNewElement(pNodeDevice, "status");
      nStatus = GetSubDeviceAvailableStatus(device);
      mxmlNewInteger(pNode, nStatus);

      pNode = mxmlNewElement(pNodeDevice, "statusTS");
      nTimeStamp = (int)GetUTCTime();
      snprintf(szTimeStamp, sizeof(szTimeStamp), "%d", nTimeStamp);
      mxmlNewText(pNode, 0, szTimeStamp);

      pNode = mxmlNewElement(pNodeDevice, "firmwareVersion");
      mxmlNewText(pNode, 0, device->FirmwareVersion);

      pNode = mxmlNewElement(pNodeDevice, "fwUpgradeStatus");
      mxmlNewText(pNode, 0, "4");

      pNode = mxmlNewElement(pNodeDevice, "signalStrength");
      mxmlNewText(pNode, 0, "100");

      pNodeCapProfiles = mxmlNewElement(pNodeDevice, "deviceCapabilityProfiles");

      pNodeCapProfile = mxmlNewElement(pNodeCapProfiles, "deviceCapabilityProfile");

      pNode = mxmlNewElement(pNodeCapProfile, "capabilityId");
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "makeGroupdeviceNode: szCapabilityID = %s",
                pDevInfo->CapInfo.szCapabilityID);
      mxmlNewText(pNode, 0, pDevInfo->CapInfo.szCapabilityID);

      pNode = mxmlNewElement(pNodeCapProfile, "currentValue");
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "makeGroupdeviceNode: szCurrentValue = %s",
                pDevInfo->CapInfo.szCapabilityValue);
      ConvertCapabilityValue(pDevInfo->CapInfo.szCapabilityValue, ',', ':');
      mxmlNewText(pNode, 0, pDevInfo->CapInfo.szCapabilityValue);


      sscanf(pDevInfo->CapInfo.szCapabilityValue, "%x", &cap_value);

      APP_LOG("DEVICE:makeSubdeviceNode", LOG_DEBUG, "Capability Value:%x", cap_value);

      if (0 == strcmp (pDevInfo->CapInfo.szCapabilityID, CAPABILITY_ID_STATUS_CHANGE))
      {
          /*Updating the rule information if rule is present */
        if(SUCCESS == doesNotificationRuleExist(device, cap_value, &ruleId))
	{
	    if ( false == makerulenode(pNodeDevice, device, cap_value, ruleId))
	    {
	        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "makerulenode is NOT available");
	    }
	}
      }

    }
    bResult = true;

  }
  else
  {
     DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "makeGroupdeviceNode is NOT available");
  }

  return bResult;
}

char* CreateStatusNotificationReqXML(char *pPluginId, char *pDeviceId, struct DeviceInfo *pDevInfo)
{
  mxml_node_t *pXml = NULL;
  mxml_node_t *pRootNode = NULL;

  char *pszXML = NULL;

  pXml = mxmlNewXML("1.0");
  if( NULL == pXml )
  {
    return pszXML;
  }

  pRootNode = mxmlNewElement(pXml, "devices");

  if( NULL == pDeviceId )
  {
    makeUpdateDeviceNode(pRootNode);
  }
  else if( pDeviceId && pDevInfo )
  {
    if( pDevInfo->bGroupDevice )
    {
      if ( false == makeGroupdeviceNode(pRootNode, pPluginId, pDeviceId, pDevInfo))
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "makeGroupdeviceNode is NOT available");
        mxmlDelete(pXml);
        return NULL;
      }
    }
    else
    {
      if ( false == makeSubdeviceNode(pRootNode, pPluginId, pDeviceId, pDevInfo))
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "makeSubdeviceNode is NOT available");
        mxmlDelete(pXml);
        return NULL;
      }
    }
  }

  pszXML = mxmlSaveAllocString(pXml, MXML_NO_CALLBACK);

  mxmlDelete(pXml);

  return pszXML;
}

//=====================================================================
// Device State Update Notification
// Resource : PUT /device/devices?notifyApp="false"
// URL : https://host:8443/apis/http/device/devices
//=====================================================================
int SendDeviceStateNotification(char *pPluginId, char *pDeviceId, struct DeviceInfo *pDevInfo, char **ppErrorCode)
{
  char *pRespBody = NULL;
  int nStatusCode = 0;
  char szURL[128] = {0,};

  *ppErrorCode = NULL;

  char *pStatusNotificationXML = CreateStatusNotificationReqXML(pPluginId, pDeviceId, pDevInfo);

  if( pStatusNotificationXML )
  {
    snprintf(szURL, sizeof(szURL), "https://%s:8443/apis/http/device/devices?notifyApp=false", BL_DOMAIN_NM);
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SendDeviceStateNotification: URL = %s", szURL);
    DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pStatusNotificationXML, strlen(pStatusNotificationXML));

    nStatusCode = SendHttpRequestWithAuth(PUT, szURL, pStatusNotificationXML, &pRespBody);
    if( nStatusCode == 200 && pRespBody )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SendDeviceStateNotification: StatusCode : %d", nStatusCode);

      free(pRespBody);
    }
    else if( nStatusCode && pRespBody )
    {
      //Parsing Error XML
      *ppErrorCode = GetErrorCodeStr(pRespBody);
      free(pRespBody);
    }
    free(pStatusNotificationXML);
  }

  return nStatusCode;
}

void updateRegisteredSubdeviceIDs(char *pszRespBody)
{
  mxml_node_t *tree = NULL;
  mxml_node_t *node = NULL;
  mxml_node_t *pluginId_node = NULL;
  mxml_node_t *macAddress_node = NULL;

  bool bUpdated = false;
  int i = 0, nCount = 0;

  SD_DeviceList device_list;
  SD_DeviceID deviceID, registeredID;

  tree = mxmlLoadString(NULL, pszRespBody, MXML_OPAQUE_CALLBACK);
  if( NULL == tree )
  {
    return;
  }

  nCount = SD_GetDeviceList(&device_list);

  //First, find the device element in xml buffer.
  for( node = mxmlFindElement(tree, tree, "device", NULL, NULL, MXML_DESCEND);
       node != NULL;
       node = mxmlFindElement(node, tree, "device", NULL, NULL, MXML_DESCEND) )
  {
    pluginId_node = mxmlFindElement(node, tree, "pluginId", NULL, NULL, MXML_DESCEND);
    macAddress_node = mxmlFindElement(node, tree, "macAddress", NULL, NULL, MXML_DESCEND);

    if( pluginId_node && macAddress_node )
    {
      char *pPluginId = (char *)mxmlGetOpaque(pluginId_node);
      char *pMacAddress = (char *)mxmlGetOpaque(macAddress_node);
      if( pPluginId && pMacAddress )
      {
        //update registerId of subdevice by using pluginId that is received from the server.
        for( i = 0; i < nCount; i++ )
        {
          if( 0 == strcmp(pMacAddress, device_list[i]->EUID.Data) )
          {
            MakeDeviceID(pMacAddress, &deviceID);
            MakeRegisterDeviceID(pPluginId, &registeredID);
            SD_SetDeviceRegisteredID(&deviceID, &registeredID);
            bUpdated = true;
          }
        }

        if( bUpdated )
        {
          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pluginId = %s, macAddress = %s : is updated success", pPluginId, pMacAddress);
        }
        else
        {
          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pluginId = %s, macAddress = %s : is updated failure", pPluginId, pMacAddress);
        }
      }
    }
  }

  mxmlDelete(tree);
}

int GetHomeDevices(char *pHomeID, char **ppErrorCode)
{
  char *pRespBody = NULL;
  int nStatusCode = 0;
  char szURL[128] = {0,};

  *ppErrorCode = NULL;

  snprintf(szURL, sizeof(szURL), "https://%s:8443/apis/http/device/homeDevices/%s", BL_DOMAIN_NM, pHomeID);
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "GetHomeDevices: URL = %s", szURL);

  nStatusCode = SendHttpRequestWithAuth(GET, szURL, NULL, &pRespBody);

  if( nStatusCode == 200 && pRespBody )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "GetHomeDevices: Status = %d, RespBody : %s", nStatusCode, pRespBody);
    updateRegisteredSubdeviceIDs(pRespBody);
    free(pRespBody);
  }
  else if( nStatusCode && pRespBody )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "GetHomeDevices: Status = %d, RespBody : %s", nStatusCode, pRespBody);
    *ppErrorCode = GetErrorCodeStr(pRespBody);
    free(pRespBody);
  }

  return nStatusCode;
}


//====================================================================================
//Request and Response XML Handler of Update Device Capability(SetDeviceStatusHandler)
//====================================================================================

char* CreateUpdateDeviceCapabilitiesRespXML(struct DeviceStatus *pDevStatus)
{
  mxml_node_t *pXml = NULL;
  mxml_node_t *pNodeStatus = NULL;
  mxml_node_t *pNodeSetDeviceStatus = NULL;
  mxml_node_t *pNodePluginId = NULL;
  mxml_node_t *pNodePluginMacAddr = NULL;
  mxml_node_t *pNodePluginModelCode = NULL;
  mxml_node_t *pNodeDeviceProductName = NULL;
  mxml_node_t *pNodeDevices = NULL;
  mxml_node_t *pNodeDevice = NULL;
  mxml_node_t *pNodeCloudDeviceID = NULL;
  mxml_node_t *pNodeDeviceID = NULL;
  mxml_node_t *pNodeSerialNo = NULL;
  mxml_node_t *pNodeUniqueId = NULL;
  mxml_node_t *pNodeNotify = NULL;
  mxml_node_t *pNodeFirmwareVer = NULL;
  mxml_node_t *pNodeSignal = NULL;
  mxml_node_t *pNodeDeviceModelCode = NULL;
  mxml_node_t *pNodePluginProductName = NULL;
  mxml_node_t *pNodeDeviceProductType = NULL;

  mxml_node_t *pNodeDevCapProfiles = NULL;
  mxml_node_t *pNodeDevCapProfile = NULL;
  mxml_node_t *pNodeDevCapID = NULL;
  mxml_node_t *pNodeDevCurValue = NULL;

  mxml_node_t *pNodeResultCodes = NULL;
  mxml_node_t *pNodeResultCode = NULL;
  mxml_node_t *pNodeCode = NULL;
  mxml_node_t *pNodeDesc = NULL;

  int i = 0, j = 0, nStatus = 0;
  char *pszXML = NULL;
  char szStatusCode[MAX_VAL_LEN] = {0,};

  SD_Device device;
  SD_DeviceID deviceID;

  pXml = mxmlNewXML("1.0");

  if( 0 == pDevStatus->nDeviceCount || NULL == pXml )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateUpdateDeviceCapabilitiesRespXML: Device Count = %d, or XML is NULL", pDevStatus->nDeviceCount);
    return pszXML;
  }

  pNodeSetDeviceStatus = mxmlNewElement(pXml, "pluginSetDeviceStatus");

  pNodeStatus = mxmlNewElement(pNodeSetDeviceStatus, "statusCode");
  mxmlNewText(pNodeStatus, 0, "S");

  if( pDevStatus->szPluginId[0] )
  {
    pNodePluginId = mxmlNewElement(pNodeSetDeviceStatus, "pluginId");
    mxmlNewText(pNodePluginId, 0, pDevStatus->szPluginId);
  }
  pNodePluginMacAddr = mxmlNewElement(pNodeSetDeviceStatus, "macAddress");
  mxmlNewText(pNodePluginMacAddr, 0, pDevStatus->szPluginMacAddress);
  if( pDevStatus->szPluginModelCode[0] )
  {
    pNodePluginModelCode = mxmlNewElement(pNodeSetDeviceStatus, "modelCode");
    mxmlNewText(pNodePluginModelCode, 0, pDevStatus->szPluginModelCode);
    pNodePluginProductName = mxmlNewElement(pNodeSetDeviceStatus, "productName");
    mxmlNewText(pNodePluginProductName, 0, getProductName(pDevStatus->szPluginModelCode));
  }

  pNodeDevices = mxmlNewElement(pNodeSetDeviceStatus, "devices");

  for( i = 0; i < pDevStatus->nDeviceCount; i++ )
  {
    MakeDeviceID(pDevStatus->device[i].szDeviceID, &deviceID);

    if( SD_GetDevice(&deviceID, &device) )
    {
      memset(szStatusCode, 0x00, sizeof(szStatusCode));

      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SD_GetDevice(SubDeviceID = %s) is Success...", deviceID.Data);

      pNodeDevice = mxmlNewElement(pNodeDevices, "device");

      pNodeCloudDeviceID = mxmlNewElement(pNodeDevice, "deviceId");
      mxmlNewText(pNodeCloudDeviceID, 0, pDevStatus->device[i].szCloudDeviceID);

      pNodeDeviceID = mxmlNewElement(pNodeDevice, "macAddress");
      mxmlNewText(pNodeDeviceID, 0, pDevStatus->device[i].szDeviceID);

      pNodeStatus = mxmlNewElement(pNodeDevice, "statusCode");
      mxmlNewText(pNodeStatus, 0, "S");
      pNodeResultCodes = mxmlNewElement(pNodeDevice, "resultCodes");
      pNodeResultCode = mxmlNewElement(pNodeResultCodes, "resultCode");
      pNodeCode = mxmlNewElement(pNodeResultCode, "code");
      mxmlNewText(pNodeCode, 0, "200");
      pNodeDesc = mxmlNewElement(pNodeResultCode, "description");
      mxmlNewText(pNodeDesc, 0, "Success");

      pNodeSerialNo = mxmlNewElement(pNodeDevice, "serialNumber");
      mxmlNewText(pNodeSerialNo, 0, device->Serial);

      pNodeUniqueId = mxmlNewElement(pNodeDevice, "uniqueId");
      mxmlNewText(pNodeUniqueId, 0, "");

      pNodeDeviceModelCode = mxmlNewElement(pNodeDevice, "modelCode");
      mxmlNewText(pNodeDeviceModelCode, 0, device->ModelCode);

      pNodeDeviceProductName = mxmlNewElement(pNodeDevice, "productName");
      mxmlNewText(pNodeDeviceProductName, 0, getProductName((char*)device->ModelCode));

      pNodeDeviceProductType = mxmlNewElement(pNodeDevice, "productType");

        if ( 0 == strcmp (device->ModelCode, SENSOR_MODELCODE_DW) ||
             0 == strcmp (device->ModelCode, SENSOR_MODELCODE_FOB) ||
             0 == strcmp (device->ModelCode, SENSOR_MODELCODE_ALARM) ||
             0 == strcmp (device->ModelCode, SENSOR_MODELCODE_PIR) )
        {
          mxmlNewText(pNodeDeviceProductType, 0, "AlertSensor");
        }
        else
        {
          mxmlNewText(pNodeDeviceProductType, 0, "Lighting");
        }

      pNodeStatus = mxmlNewElement(pNodeDevice, "status");

      nStatus = GetSubDeviceAvailableStatus(device);
      mxmlNewInteger(pNodeStatus, nStatus);

      pNodeNotify = mxmlNewElement(pNodeDevice, "notificationType");
      mxmlNewText(pNodeNotify, 0, "");

      pNodeFirmwareVer = mxmlNewElement(pNodeDevice, "firmwareVersion");
      mxmlNewText(pNodeFirmwareVer, 0, device->FirmwareVersion);

      pNodeSignal = mxmlNewElement(pNodeDevice, "signalStrength");
      mxmlNewText(pNodeSignal, 0, "");

      pNodeDevCapProfiles = mxmlNewElement(pNodeDevice, "deviceCapabilityProfiles");

      for( j = 0; j < pDevStatus->device[i].nCapabilityCount; j++ )
      {
        pNodeDevCapProfile = mxmlNewElement(pNodeDevCapProfiles, "deviceCapabilityProfile");
        pNodeDevCapID = mxmlNewElement(pNodeDevCapProfile, "capabilityId");
        mxmlNewText(pNodeDevCapID, 0, pDevStatus->device[i].devCapability[j].szCapabilityID);

        //Get Current Value of Device
        pNodeDevCurValue = mxmlNewElement(pNodeDevCapProfile, "currentValue");
        mxmlNewText(pNodeDevCurValue, 0, pDevStatus->device[i].devCapability[j].szCapabilityValue);
      }
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SubDeviceID = %s is not existed in WeMoBridge", deviceID.Data);

      pNodeDevice = mxmlNewElement(pNodeDevices, "device");

      pNodeCloudDeviceID = mxmlNewElement(pNodeDevice, "deviceId");
      mxmlNewText(pNodeCloudDeviceID, 0, pDevStatus->device[i].szCloudDeviceID);

      pNodeDeviceID = mxmlNewElement(pNodeDevice, "macAddress");
      mxmlNewText(pNodeDeviceID, 0, pDevStatus->device[i].szDeviceID);

      pNodeStatus = mxmlNewElement(pNodeDevice, "statusCode");
      mxmlNewText(pNodeStatus, 0, "F");
      pNodeResultCodes = mxmlNewElement(pNodeDevice, "resultCodes");
      pNodeResultCode = mxmlNewElement(pNodeResultCodes, "resultCode");
      pNodeCode = mxmlNewElement(pNodeResultCode, "code");
      mxmlNewText(pNodeCode, 0, "400");
      pNodeDesc = mxmlNewElement(pNodeResultCode, "description");
      mxmlNewText(pNodeDesc, 0, "No Device");
    }
  }

  pszXML = mxmlSaveAllocString(pXml, MXML_NO_CALLBACK);

  mxmlDelete(pXml);

  return pszXML;
}

//Update Device's Capabilities
int SetDeviceStatusHandler(void *hndl, char *xmlBuf, unsigned int buflen, void *data_sock, void* remoteInfo, char* transaction_id)
{
  mxml_node_t *tree = NULL;
  mxml_node_t *find_node = NULL, *device_node = NULL, *devcap_node = NULL;

  int retVal = PLUGIN_SUCCESS;
  char *pszResponse = NULL;
  char *pPluginId = NULL, *pMacAddress = NULL, *pModelCode = NULL;
  char *pCloudDeviceId = NULL, *pDeviceId = NULL, *pDeviceModelCode = NULL, *pStatusCode = NULL;
  char *pDeviceProductType = NULL;
  char *pCapabilityID = NULL, *pCurrentValue = NULL;
  struct DeviceStatus *pDevStatus = NULL;

  char szErrorResponse[MAX_DATA_LEN];
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "inside SetDeviceStatusHandler");
  if( !xmlBuf )
  {
    retVal = PLUGIN_FAILURE;
    return retVal;
  }

  pDevStatus = (struct DeviceStatus *)calloc(1, sizeof(struct DeviceStatus));
  if( !pDevStatus )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failed to allocat DeviceStatus");
    retVal = PLUGIN_FAILURE;
    return retVal;
  }

  if( buflen )
    CHECK_PREV_TSX_ID(E_PLUGINSETDEVICESTATUS, transaction_id, retVal);

  tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

  if( NULL == tree )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "XML String %s couldn't be loaded", xmlBuf);
    retVal = PLUGIN_FAILURE;
    goto handler_exit1;
  }

  pPluginId = getElemenetValue(tree, tree, &find_node, "pluginId");
  pMacAddress = getElemenetValue(tree, tree, &find_node, "macAddress");
  pModelCode = getElemenetValue(tree, tree, &find_node, "modelCode");

  if( pPluginId )
  {
    SAFE_STRCPY(pDevStatus->szPluginId, pPluginId);
  }
  if( pMacAddress )
  {
    SAFE_STRCPY(pDevStatus->szPluginMacAddress, pMacAddress);
  }
  if( pModelCode )
  {
    SAFE_STRCPY(pDevStatus->szPluginModelCode, pModelCode);
  }

  //Get mac address of the device
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "MacAddress of WeMo Bridge is %s", g_szWiFiMacAddress);

  if( !strcmp(pDevStatus->szPluginMacAddress, g_szWiFiMacAddress) )
  {
    int nDevice = 0, nCapability = 0;
    struct Device *pDevice = NULL;
    struct DeviceCapability *pDevCapability = NULL;

    for( device_node = mxmlFindElement(tree, tree, "device", NULL, NULL, MXML_DESCEND);
        device_node != NULL;
        device_node = mxmlFindElement(device_node, tree, "device", NULL, NULL, MXML_DESCEND) )
    {
      //Get pointer of struct Device
      pDevice = &(pDevStatus->device[nDevice]);

      //Set end device status as requested...
      pCloudDeviceId = getElemenetValue(device_node, tree, &find_node, "deviceId");
      pDeviceId = getElemenetValue(device_node, tree, &find_node, "macAddress");
      pDeviceModelCode = getElemenetValue(device_node, tree, &find_node, "modelCode");
      pDeviceProductType = getElemenetValue(device_node, tree, &find_node, "productType");
      pStatusCode = getElemenetValue(device_node, tree, &find_node, "status");
      if( pCloudDeviceId )
      {
        SAFE_STRCPY(pDevice->szCloudDeviceID, pCloudDeviceId);
      }
      if( pDeviceId )
      {
        SAFE_STRCPY(pDevice->szDeviceID, pDeviceId);
      }
      if( pDeviceModelCode )
      {
        SAFE_STRCPY(pDevice->szDeviceModelCode, pDeviceModelCode);
      }

      if ( pDeviceProductType )
      {
        if ( 0 == strcmp (pDeviceModelCode, SENSOR_MODELCODE_DW) ||
             0 == strcmp (pDeviceModelCode, SENSOR_MODELCODE_FOB) ||
             0 == strcmp (pDeviceModelCode, SENSOR_MODELCODE_ALARM) ||
             0 == strcmp (pDeviceModelCode, SENSOR_MODELCODE_PIR) )
        {
          SAFE_STRCPY(pDevice->szDeviceProductType, "AlertSensor");
        }
        else
        {
          SAFE_STRCPY(pDevice->szDeviceProductType, "Lighting");
        }
      }
      if( pStatusCode )
      {
        SAFE_STRCPY(pDevice->szStatusCode, pStatusCode);
      }

      for( devcap_node = mxmlFindElement(device_node, tree, "deviceCapabilityProfile", NULL, NULL, MXML_DESCEND);
          devcap_node != NULL;
          devcap_node = mxmlFindElement(devcap_node, tree, "deviceCapabilityProfile", NULL, NULL, MXML_DESCEND) )
      {
          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Get pointer of struct DeviceCapability....dp");
        //Get pointer of struct DeviceCapability
        pDevCapability = &(pDevStatus->device[nDevice].devCapability[nCapability]);

        pCapabilityID = getElemenetValue(devcap_node, tree, &find_node, "capabilityId");
        if( pCapabilityID )
        {
          SAFE_STRCPY(pDevCapability->szCapabilityID, pCapabilityID);
        }
        pCurrentValue = getElemenetValue(devcap_node, tree, &find_node, "currentValue");
        if( pCurrentValue )
        {
          SAFE_STRCPY(pDevCapability->szCapabilityValue, pCurrentValue);

          //Set capability value to the device...
          if( pDeviceId && pCapabilityID )
          {
             DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "//Set capability value to the device....dp");
            SD_DeviceID deviceID;
            SD_CapabilityID capID;

            MakeDeviceID(pDeviceId, &deviceID);
            MakeCapabilityID(pCapabilityID, &capID);

            // Save Manually toggled time for rules

            DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SD_SetDeviceCapabilityValue(%s, %s, %s, %d)", deviceID.Data, capID.Data, pCurrentValue, SE_LOCAL + SE_REMOTE);
            if( SD_SetDeviceCapabilityValue(&deviceID, &capID, pCurrentValue, SE_LOCAL + SE_REMOTE) )
            {
              DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SD_SetDeviceCapabilityValue(%s, %s, %s) command is success...", deviceID.Data, capID.Data, pCurrentValue);
              pDevCapability->nStatus = 1;
            }
            else
            {
              DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SD_SetDeviceCapabilityValue(%s, %s, %s) command is failed...", deviceID.Data, capID.Data, pCurrentValue);
              pDevCapability->nStatus = 0;
            }

            pluginUsleep(WAIT_100MS);
          }
        }
        nCapability++;
        pDevStatus->device[nDevice].nCapabilityCount = nCapability;
      }

      nDevice++;
      pDevStatus->nDeviceCount = nDevice;
    }

    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pluginSetDeviceStatus XML parsing is Success...");
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Start Creating UpdateDeviceCapabilities Response XML...");

    //Create Response XML
    pszResponse = CreateUpdateDeviceCapabilitiesRespXML(pDevStatus);

    if( pszResponse )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "UpdateDeviceCapabilities Response XML is %s", pszResponse);

      if( buflen )
      {
        retVal = SendNatPkt(hndl, pszResponse, remoteInfo, transaction_id, data_sock, E_PLUGINSETDEVICESTATUS);
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "==== SetDeviceStatusHandler response packet =====")
        DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pszResponse, strlen(pszResponse));
      }

      free(pszResponse);
    }
    else
    {
      if( 0 == pDevStatus->nDeviceCount && pDevStatus->szPluginId[0] && pDevStatus->szPluginMacAddress[0] )
      {
        memset(szErrorResponse, 0x00, MAX_DATA_LEN);
        if ( 0 == strcmp (pDevStatus->szPluginModelCode, SENSOR_MODELCODE_DW) ||
             0 == strcmp (pDevStatus->szPluginModelCode, SENSOR_MODELCODE_FOB) ||
             0 == strcmp (pDevStatus->szPluginModelCode, SENSOR_MODELCODE_ALARM) ||
             0 == strcmp (pDevStatus->szPluginModelCode, SENSOR_MODELCODE_PIR) )
        {
        snprintf(szErrorResponse, MAX_DATA_LEN, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><pluginSetDeviceStatus><statusCode>F</statusCode> \
            <pluginId>%s</pluginId><macAddress>%s</macAddress><modelCode>%s</modelCode><productType>%s</productType></pluginSetDeviceStatus>",
            pDevStatus->szPluginId, pDevStatus->szPluginMacAddress, pDevStatus->szPluginModelCode, "AlertSensor");
        }
        else
        {
          snprintf(szErrorResponse, MAX_DATA_LEN, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><pluginSetDeviceStatus><statusCode>F</statusCode> \
            <pluginId>%s</pluginId><macAddress>%s</macAddress><modelCode>%s</modelCode><productType>%s</productType></pluginSetDeviceStatus>",
            pDevStatus->szPluginId, pDevStatus->szPluginMacAddress, pDevStatus->szPluginModelCode, "Lighting");
        }

        if( buflen )
        {
          retVal = SendNatPkt(hndl, szErrorResponse, remoteInfo, transaction_id, data_sock, E_PLUGINSETDEVICESTATUS);
          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "==== SetDeviceStatusHandler error response packet =====")
          DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, szErrorResponse, strlen(szErrorResponse));
        }
      }
      else
      {
        retVal = PLUGIN_FAILURE;
      }
    }
  }
  else
  {
    //Create Error Response and Send it...
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Mac received %s doesn't match with plugin Mac %s", pDevStatus->szPluginMacAddress, g_szWiFiMacAddress);

    memset(szErrorResponse, 0x00, MAX_DATA_LEN);
    if ( 0 == strcmp (pDevStatus->szPluginModelCode, SENSOR_MODELCODE_DW) ||
             0 == strcmp (pDevStatus->szPluginModelCode, SENSOR_MODELCODE_FOB) ||
             0 == strcmp (pDevStatus->szPluginModelCode, SENSOR_MODELCODE_ALARM) ||
             0 == strcmp (pDevStatus->szPluginModelCode, SENSOR_MODELCODE_PIR) )
        {
          snprintf(szErrorResponse, MAX_DATA_LEN, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><pluginSetDeviceStatus><statusCode>F</statusCode> \
            <pluginId>%s</pluginId><macAddress>%s</macAddress><modelCode>%s</modelCode><productType>%s</productType></pluginSetDeviceStatus>",
            pDevStatus->szPluginId, pDevStatus->szPluginMacAddress, pDevStatus->szPluginModelCode, "AlertSensor");
        }
        else
        {
    snprintf(szErrorResponse, MAX_DATA_LEN, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><pluginSetDeviceStatus><statusCode>F</statusCode> \
            <pluginId>%s</pluginId><macAddress>%s</macAddress><modelCode>%s</modelCode><productType>%s</productType></pluginSetDeviceStatus>",
            pDevStatus->szPluginId, pDevStatus->szPluginMacAddress, pDevStatus->szPluginModelCode, "Lighting");
        }


    if( buflen )
    {
      retVal = SendNatPkt(hndl, szErrorResponse, remoteInfo, transaction_id, data_sock, E_PLUGINSETDEVICESTATUS);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "==== SetDeviceStatusHandler error response packet =====")
      DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, szErrorResponse, strlen(szErrorResponse));
    }
  }

handler_exit1:

  if( tree )
    mxmlDelete(tree);

  if( pDevStatus )
    free(pDevStatus);

  return retVal;
}

char* CreatGetDevicesDetailsRespXML(struct DeviceStatus *pDevStatus)
{
  mxml_node_t *pXml = NULL;
  mxml_node_t *pNodeDeviceStatus = NULL;
  mxml_node_t *pNodePluginId = NULL;
  mxml_node_t *pNodePluginMacAddr = NULL;
  mxml_node_t *pNodePluginProductName = NULL;
  mxml_node_t *pNodePluginModelCode = NULL;
  mxml_node_t *pNodeDevices = NULL;
  mxml_node_t *pNodeDevice = NULL;
  mxml_node_t *pNodeCloudDeviceID = NULL;
  mxml_node_t *pNodeDeviceID = NULL;
  mxml_node_t *pNodeSerialNo = NULL;
  mxml_node_t *pNodeUniqueId = NULL;
  mxml_node_t *pNodeNotify = NULL;
  mxml_node_t *pNodeFirmwareVer = NULL;
  mxml_node_t *pNodeSignal = NULL;
  mxml_node_t *pNodeDeviceModelCode = NULL;
  mxml_node_t *pNodeDeviceProductName= NULL;
  mxml_node_t *pNodeDevCapProfiles = NULL;
  mxml_node_t *pNodeDevCapProfile = NULL;
  mxml_node_t *pNodeDevCapID = NULL;
  mxml_node_t *pNodeDevCurValue = NULL;
  mxml_node_t *pNodeDevLastEventTimeStamp = NULL;

  mxml_node_t *pNodeStatus = NULL;
  mxml_node_t *pNodeResultCode = NULL;
  mxml_node_t *pNodeResultCodes = NULL;
  mxml_node_t *pNodeCode = NULL;
  mxml_node_t *pNodeDesc = NULL;


  int i = 0, j = 0, nStatus = 0;
  char *pszXML = NULL;
  char szCurrentValue[CAPABILITY_MAX_VALUE_LENGTH] = {0,};

  SD_Device device;
  SD_DeviceID deviceID;

  pXml = mxmlNewXML("1.0");
  if( 0 == pDevStatus->nDeviceCount || NULL == pXml )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreatGetDevicesDetailsRespXML: Device Count = %d, or XML is NULL",
              pDevStatus->nDeviceCount);
    return pszXML;
  }

  pNodeDeviceStatus = mxmlNewElement(pXml, "pluginDeviceStatus");

  //status and result code of this response xml packet...
  pNodeStatus = mxmlNewElement(pNodeDeviceStatus, "statusCode");
  mxmlNewText(pNodeStatus, 0, "S");

  //WeMo Bridge Plugin Device's information...
  if( pDevStatus->szPluginId[0] )
  {
    pNodePluginId = mxmlNewElement(pNodeDeviceStatus, "pluginId");
    mxmlNewText(pNodePluginId, 0, pDevStatus->szPluginId);
  }
  pNodePluginMacAddr = mxmlNewElement(pNodeDeviceStatus, "macAddress");
  mxmlNewText(pNodePluginMacAddr, 0, pDevStatus->szPluginMacAddress);
  if( pDevStatus->szPluginModelCode[0] )
  {
    pNodePluginModelCode = mxmlNewElement(pNodeDeviceStatus, "modelCode");
    mxmlNewText(pNodePluginModelCode, 0, pDevStatus->szPluginModelCode);
    pNodePluginProductName = mxmlNewElement(pNodeDeviceStatus, "productName");
    mxmlNewText(pNodePluginProductName, 0, getProductName(pDevStatus->szPluginModelCode));
  }

  //End Devices's information...
  pNodeDevices = mxmlNewElement(pNodeDeviceStatus, "devices");

  for( i = 0; i < pDevStatus->nDeviceCount; i++ )
  {
    MakeDeviceID(pDevStatus->device[i].szDeviceID, &deviceID);

    if( SD_GetDevice(&deviceID, &device) )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SD_GetDevice(SubDeviceID = %s) is Success...", deviceID.Data);

      pNodeDevice = mxmlNewElement(pNodeDevices, "device");

      pNodeCloudDeviceID = mxmlNewElement(pNodeDevice, "deviceId");
      mxmlNewText(pNodeCloudDeviceID, 0, pDevStatus->device[i].szCloudDeviceID);

      pNodeDeviceID = mxmlNewElement(pNodeDevice, "macAddress");
      mxmlNewText(pNodeDeviceID, 0, pDevStatus->device[i].szDeviceID);

      pNodeStatus = mxmlNewElement(pNodeDevice, "statusCode");
      mxmlNewText(pNodeStatus, 0, "S");

      pNodeResultCodes = mxmlNewElement(pNodeDevice, "resultCodes");
      pNodeResultCode = mxmlNewElement(pNodeResultCodes, "resultCode");
      pNodeCode = mxmlNewElement(pNodeResultCode, "code");
      mxmlNewText(pNodeCode, 0, "200");
      pNodeDesc = mxmlNewElement(pNodeResultCode, "description");
      mxmlNewText(pNodeDesc, 0, "Success");

      pNodeSerialNo = mxmlNewElement(pNodeDevice, "serialNumber");
      mxmlNewText(pNodeSerialNo, 0, device->Serial);

      pNodeUniqueId = mxmlNewElement(pNodeDevice, "uniqueId");
      mxmlNewText(pNodeUniqueId, 0, "");

      pNodeDeviceModelCode = mxmlNewElement(pNodeDevice, "modelCode");
      mxmlNewText(pNodeDeviceModelCode, 0, device->ModelCode);

     pNodeDeviceProductName = mxmlNewElement(pNodeDevice, "productName");
     mxmlNewText(pNodeDeviceProductName, 0, getProductName((char*)device->ModelCode));

      pNodeStatus = mxmlNewElement(pNodeDevice, "status");

      nStatus = GetSubDeviceAvailableStatus(device);
      mxmlNewInteger(pNodeStatus, nStatus);

      pNodeNotify = mxmlNewElement(pNodeDevice, "notificationType");
      mxmlNewText(pNodeNotify, 0, "");

      pNodeFirmwareVer = mxmlNewElement(pNodeDevice, "firmwareVersion");
      mxmlNewText(pNodeFirmwareVer, 0, device->FirmwareVersion);

      pNodeSignal = mxmlNewElement(pNodeDevice, "signalStrength");
      mxmlNewText(pNodeSignal, 0, "");

      pNodeDevCapProfiles = mxmlNewElement(pNodeDevice, "deviceCapabilityProfiles");

      for( j = 0; j < device->CapabilityCount; j++ )
      {
        if( 0 != strcmp(device->Capability[j]->ProfileName, "Indentify") )
        {
          pNodeDevCapProfile = mxmlNewElement(pNodeDevCapProfiles, "deviceCapabilityProfile");
          pNodeDevCapID = mxmlNewElement(pNodeDevCapProfile, "capabilityId");
          mxmlNewText(pNodeDevCapID, 0, device->Capability[j]->CapabilityID.Data);

          pNodeDevCurValue = mxmlNewElement(pNodeDevCapProfile, "currentValue");

          //Get Current Value of Device
          memset(szCurrentValue, 0x00, sizeof(szCurrentValue));
          SD_GetDeviceCapabilityValue(&deviceID, &(device->Capability[j]->CapabilityID), szCurrentValue, 0, SD_CACHE);
          if( szCurrentValue[0] )
          {
            // replace(szCurrentValue, ",", ":");
            ConvertCapabilityValue(szCurrentValue, ',', ':');
            mxmlNewText(pNodeDevCurValue, 0, szCurrentValue);
          }
          else
          {
            mxmlNewText(pNodeDevCurValue, 0, "");
          }

          if (0 == strcmp(device->Capability[j]->ProfileName, "IASZone") ) {
            int n = snprintf(NULL, 0, "%lu", device->LastReportedTime);
            char buf[n+1];
            snprintf(buf, n+1, "%lu", device->LastReportedTime);
            pNodeDevLastEventTimeStamp = mxmlNewElement(pNodeDevCapProfile, "ts");

            mxmlNewText (pNodeDevLastEventTimeStamp, 0, buf);
            //DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "makeSubdeviceNode: LastReportedTime =  %lu, %s", device->LastReportedTime, buf);
          }
          else if (0 == strcmp(device->Capability[j]->ProfileName, "SensorKeyPress") ) {
            int n = snprintf(NULL, 0, "%lu", device->LastReportedTime);
            char buf[n+1];
            snprintf(buf, n+1, "%lu", device->LastReportedTimeExt);
            pNodeDevLastEventTimeStamp = mxmlNewElement(pNodeDevCapProfile, "ts");
            mxmlNewText (pNodeDevLastEventTimeStamp, 0, buf);
            //DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "=makeSubdeviceNode: LastReportedTimeExt = %lu, %s", device->LastReportedTimeExt, buf);
          }

          //pNodeDevLastEventTimeStamp = mxmlNewElement(pNodeDevCapProfile, "LastEventTimeStamp");
          //mxmlNewText(pNodeDevLastEventTimeStamp, 0, device->LastReportedTime);
        }
      }
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SubDeviceID = %s is not existed in WeMoBridge", deviceID.Data);

      pNodeDevice = mxmlNewElement(pNodeDevices, "device");

      pNodeCloudDeviceID = mxmlNewElement(pNodeDevice, "deviceId");
      mxmlNewText(pNodeCloudDeviceID, 0, pDevStatus->device[i].szCloudDeviceID);

      pNodeDeviceID = mxmlNewElement(pNodeDevice, "macAddress");
      mxmlNewText(pNodeDeviceID, 0, pDevStatus->device[i].szDeviceID);

      pNodeStatus = mxmlNewElement(pNodeDevice, "statusCode");
      mxmlNewText(pNodeStatus, 0, "F");
      pNodeResultCodes = mxmlNewElement(pNodeDevice, "resultCodes");
      pNodeResultCode = mxmlNewElement(pNodeResultCodes, "resultCode");
      pNodeCode = mxmlNewElement(pNodeResultCode, "code");
      mxmlNewText(pNodeCode, 0, "400");
      pNodeDesc = mxmlNewElement(pNodeResultCode, "description");
      mxmlNewText(pNodeDesc, 0, "No Device");
    }
  }

  pszXML = mxmlSaveAllocString(pXml, MXML_NO_CALLBACK);

  mxmlDelete(pXml);

  return pszXML;
}


// Get Devices List
int GetDeviceStatusHandler(void *hndl, char *xmlBuf, unsigned int buflen, void* data_sock , void* remoteInfo, char* transaction_id)
{
  int retVal = PLUGIN_SUCCESS;
  mxml_node_t *tree = NULL;
  mxml_node_t *find_node = NULL;
  mxml_node_t *device_node = NULL;

  char *pPluginId = NULL, *pMacAddress = NULL, *pModelCode = NULL;
  char *pCloudDeviceId = NULL, *pDeviceId = NULL, *pDeviceModelCode = NULL;
  char szErrorResponse[MAX_DATA_LEN];

  int nDevice = 0;
  struct DeviceStatus *pDevStatus = NULL;
  struct Device *pDevice = NULL;

  if( !xmlBuf )
  {
    retVal = PLUGIN_FAILURE;
    return retVal;
  }

  pDevStatus = (struct DeviceStatus *)calloc(1, sizeof(struct DeviceStatus));
  if( !pDevStatus )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failed to allocate DeviceStatus");
    retVal = PLUGIN_FAILURE;
    return retVal;
  }

  if( buflen )
    CHECK_PREV_TSX_ID(E_PLUGINDEVICESTATUS, transaction_id, retVal);

  tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

  if( NULL == tree )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "XML String %s couldn't be loaded", xmlBuf);

    retVal = PLUGIN_FAILURE;
    goto handler_exit1;
  }

  //Get infomation of WeMo Bridge device.
  pPluginId = getElemenetValue(tree, tree, &find_node, "pluginId");
  pMacAddress = getElemenetValue(tree, tree, &find_node, "macAddress");
  pModelCode = getElemenetValue(tree, tree, &find_node, "modelCode");

  if( pPluginId )
  {
    SAFE_STRCPY(pDevStatus->szPluginId, pPluginId);
  }
  if( pMacAddress )
  {
    SAFE_STRCPY(pDevStatus->szPluginMacAddress, pMacAddress);
  }
  if( pModelCode )
  {
    SAFE_STRCPY(pDevStatus->szPluginModelCode, pModelCode);
  }

  for( device_node = mxmlFindElement(tree, tree, "device", NULL, NULL, MXML_DESCEND);
      device_node != NULL;
      device_node = mxmlFindElement(device_node, tree, "device", NULL, NULL, MXML_DESCEND) )
  {
    pDevice = &(pDevStatus->device[nDevice]);

    pCloudDeviceId = getElemenetValue(device_node, tree, &find_node, "deviceId");
    pDeviceId = getElemenetValue(device_node, tree, &find_node, "macAddress");
    pDeviceModelCode = getElemenetValue(device_node, tree, &find_node, "modelCode");

    if( pCloudDeviceId )
    {
        SAFE_STRCPY(pDevice->szCloudDeviceID, pCloudDeviceId);
    }
    if( pDeviceId )
    {
        SAFE_STRCPY(pDevice->szDeviceID, pDeviceId);
    }
    if( pDeviceModelCode )
    {
        SAFE_STRCPY(pDevice->szDeviceModelCode, pDeviceModelCode);
    }

    nDevice++;
  }

  pDevStatus->nDeviceCount = nDevice;

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pluginDeviceStatus xml parsing is Success...");
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Start Creating GetDevicesDetails Response XML...")

  //Create Response XML
  char *pszResponse = CreatGetDevicesDetailsRespXML(pDevStatus);

  if( pszResponse )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "==== GetDevicesDetails Response XML ====");

    if( buflen )
    {
      retVal = SendNatPkt(hndl, pszResponse, remoteInfo, transaction_id, data_sock, E_PLUGINDEVICESTATUS);
      DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pszResponse, strlen(pszResponse));
    }

    free(pszResponse);
  }
  else
  {
    if( 0 == pDevStatus->nDeviceCount && pDevStatus->szPluginId[0] && pDevStatus->szPluginMacAddress[0] )
    {
      memset(szErrorResponse, 0x00, MAX_DATA_LEN);
      if (strlen(pDevStatus->szPluginModelCode) == 0)
      {
        SAFE_STRCPY(pDevStatus->szPluginModelCode, "Bridge");
      }

      snprintf(szErrorResponse, MAX_DATA_LEN, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><pluginDeviceStatus><statusCode>F</statusCode> \
      <pluginId>%s</pluginId><macAddress>%s</macAddress><modelCode>%s</modelCode></pluginDeviceStatus>",
      pDevStatus->szPluginId, pDevStatus->szPluginMacAddress, pDevStatus->szPluginModelCode);

      if( buflen )
      {
        retVal = SendNatPkt(hndl, szErrorResponse, remoteInfo, transaction_id, data_sock, E_PLUGINDEVICESTATUS);
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "==== GetDevicesDetails Error Response XML ====");
        DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, szErrorResponse, strlen(szErrorResponse));
      }
    }
    else
    {
      retVal = PLUGIN_FAILURE;
    }
  }

handler_exit1:

  if( tree )
    mxmlDelete(tree);

  if( pDevStatus )
    free(pDevStatus);

  return retVal;
}


char* SetDataStoreRespXML(int nStatus)
{
  mxml_node_t *pXml = NULL;
  mxml_node_t *pNodePlugin = NULL;
  mxml_node_t *pNodeRecipientId = NULL;
  mxml_node_t *pNodeMacAddress = NULL;
  mxml_node_t *pNodeStatus = NULL;
  mxml_node_t *pNodeFriendlyName = NULL;
  mxml_node_t *pNodeStatusTS = NULL;
  mxml_node_t *pNodeFirmwareVersion = NULL;
  mxml_node_t *pNodeFWUpgradeStatus = NULL;
  mxml_node_t *pNodeSignalStrength = NULL;
  mxml_node_t *pNodeErrorCode = NULL;

  char *pszXML = NULL;
  char szStatus[MAX_VAL_LEN] = {0,};

  pXml = mxmlNewXML("1.0");

  if( NULL == pXml )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SetDataStoreRespXML: XML is NULL");
    return pszXML;
  }

  snprintf(szStatus, MAX_VAL_LEN, "%d", nStatus);

  pNodePlugin = mxmlNewElement(pXml, "plugin");

  pNodeRecipientId = mxmlNewElement(pNodePlugin, "recipientId");
  mxmlNewText(pNodeRecipientId, 0, g_szPluginCloudId);

  pNodeMacAddress = mxmlNewElement(pNodePlugin, "macAddress");
  mxmlNewText(pNodeMacAddress, 0, g_szWiFiMacAddress);

  pNodeStatus = mxmlNewElement(pNodePlugin, "status");
  if (nStatus >= 0 )
  {
    mxmlNewText(pNodeStatus, 0, szStatus);
  }

  pNodeFriendlyName = mxmlNewElement(pNodePlugin, "friendlyName");
  mxmlNewText(pNodeFriendlyName, 0, "Bridge");

  pNodeStatusTS = mxmlNewElement(pNodePlugin, "statusTS");

  pNodeFirmwareVersion = mxmlNewElement(pNodePlugin, "firmwareVersion");
  mxmlNewText(pNodeFirmwareVersion, 0, g_szFirmwareVersion);

  pNodeFWUpgradeStatus = mxmlNewElement(pNodePlugin, "fwUpgradeStatus");
  mxmlNewText(pNodeFWUpgradeStatus, 0, "4");

  pNodeSignalStrength = mxmlNewElement(pNodePlugin, "signalStrength");
  mxmlNewText(pNodeSignalStrength, 0, "100");

  pNodeErrorCode = mxmlNewElement(pNodePlugin, "errorCode");
  mxmlNewText(pNodeErrorCode, 0, "0");


  pszXML = mxmlSaveAllocString(pXml, MXML_NO_CALLBACK);

  mxmlDelete(pXml);

  return pszXML;
}


char* GetDataStoreRespXML(int nStatus, char *contactInfo)
{
  mxml_node_t *pXml = NULL;
  mxml_node_t *pNodePlugin = NULL;
  mxml_node_t *pNodeRecipientId = NULL;
  mxml_node_t *pNodeMacAddress = NULL;
  mxml_node_t *pNodeStatus = NULL;
  mxml_node_t *pNodeStatusTS = NULL;
  mxml_node_t *pNodeFriendlyName = NULL;
  mxml_node_t *pNodeFirmwareVersion = NULL;
  mxml_node_t *pNodeFWUpgradeStatus = NULL;
  mxml_node_t *pNodeSignalStrength = NULL;
  mxml_node_t *pNodeDataStore = NULL;
  mxml_node_t *pNodeDataStores = NULL;
  mxml_node_t *pNodeName = NULL;
  mxml_node_t *pNodeValue = NULL;

  char *pszXML = NULL;
  char szStatus[MAX_VAL_LEN] = {0,};

  pXml = mxmlNewXML("1.0");

  if( NULL == pXml )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "GetDataStoreRespXML: XML is NULL");
    return pszXML;
  }

  snprintf(szStatus, MAX_VAL_LEN, "%d", nStatus);

  pNodePlugin = mxmlNewElement(pXml, "plugin");

  pNodeRecipientId = mxmlNewElement(pNodePlugin, "recipientId");
  mxmlNewText(pNodeRecipientId, 0, g_szPluginCloudId);

  pNodeMacAddress = mxmlNewElement(pNodePlugin, "macAddress");
  mxmlNewText(pNodeMacAddress, 0, g_szWiFiMacAddress);

  pNodeStatus = mxmlNewElement(pNodePlugin, "status");
  mxmlNewText(pNodeStatus, 0, szStatus);

  pNodeFriendlyName = mxmlNewElement(pNodePlugin, "friendlyName");
  mxmlNewText(pNodeFriendlyName, 0, "Bridge");

  pNodeStatusTS = mxmlNewElement(pNodePlugin, "statusTS");
  mxmlNewText(pNodeStatusTS, 0, "");

  pNodeFirmwareVersion = mxmlNewElement(pNodePlugin, "firmwareVersion");
  mxmlNewText(pNodeFirmwareVersion, 0, g_szFirmwareVersion);

  pNodeFWUpgradeStatus = mxmlNewElement(pNodePlugin, "fwUpgradeStatus");
  mxmlNewText(pNodeFWUpgradeStatus, 0, "4");

  pNodeSignalStrength = mxmlNewElement(pNodePlugin, "signalStrength");
  mxmlNewText(pNodeSignalStrength, 0, "100");

  pNodeDataStores = mxmlNewElement(pNodePlugin, "dataStores action = \"GetDataStores\"");
  //mxmlNewText(pNodeDataStores, 0, "");

  pNodeDataStore = mxmlNewElement(pNodeDataStores, "dataStore");
  //mxmlNewText(pNodeDataStore, 0, contactInfo);

  pNodeName = mxmlNewElement(pNodeDataStore, "name");
  mxmlNewText(pNodeName, 0, "EmergencyContacts");

  pNodeValue = mxmlNewElement(pNodeDataStore, "value");

  if (NULL != contactInfo)
  {
    mxmlNewText(pNodeValue, 0, contactInfo);
  }
  else
  {
    mxmlNewText(pNodeValue, 0, "Error: Contact Data is Empty");
  }

  pszXML = mxmlSaveAllocString(pXml, MXML_NO_CALLBACK);

  mxmlDelete(pXml);

  return pszXML;
}


bool isSleepFaderValue(char *capability_id, char *capability_value)
{
  int i = 0, v1 = 0, v2 = 0, nCount = 0;

  SD_CapabilityList list;

  nCount = SD_GetCapabilityList(&list);

  for( i = 0; i < nCount; i++ )
  {
    if( (0 == strcmp(list[i]->ProfileName, "LevelControl")) &&
        (0 == strcmp(list[i]->CapabilityID.Data, capability_id)) )
    {
      if( (sscanf(capability_value, "%d:%d", &v1, &v2) < 2) || (0 < v1) || (v2 < 300) )
      {
        return false;
      }
      else
      {
        return true;
      }
    }
  }
  return false;
}

bool GetCurrentGroupLevelControl(char *pszGroupID, char *pszValue)
{
  int i = 0, nCount = 0;
  char szValue[CAPABILITY_MAX_VALUE_LENGTH];

  if( NULL == pszValue )
  {
    return false;
  }

  pszValue[0] = 0x00;

  SD_GroupID groupID;

  MakeGroupID(pszGroupID, &groupID);

  SD_CapabilityList list;

  nCount = SD_GetCapabilityList(&list);

  for( i = 0; i < nCount; i++ )
  {
    if( (0 == strcmp(list[i]->ProfileName, "LevelControl")) )
    {
      memset(szValue, 0x00, sizeof(szValue));
      if( SD_GetGroupCapabilityValueCache(&groupID, &list[i]->CapabilityID, szValue, SD_CACHE) )
      {
        strncat(pszValue, szValue, strlen(szValue));
        return true;
      }
    }
  }
  return false;
}

char* CreateSetGroupDetailsRespXML(struct Groups *pGroups)
{
  mxml_node_t *pXml = NULL;
  mxml_node_t *pNodeSetGroupDetails = NULL;
  mxml_node_t *pNodePluginId = NULL;
  mxml_node_t *pNodePluginMacAddr = NULL;
  mxml_node_t *pNodePluginModelCode = NULL;
  mxml_node_t *pNodePluginProductName = NULL;
  mxml_node_t *pNodeGroups = NULL;
  mxml_node_t *pNodeGroup = NULL;
  mxml_node_t *pNodeGroupId = NULL;
  mxml_node_t *pNodeGroupName = NULL;
  mxml_node_t *pNodeReferenceId = NULL;

  mxml_node_t *pNodeDevices = NULL;
  mxml_node_t *pNodeDevice = NULL;
  mxml_node_t *pNodeCloudDeviceID = NULL;
  mxml_node_t *pNodeMacAddress = NULL;
  mxml_node_t *pNodeFirmwareVer = NULL;
  mxml_node_t *pNodeSignal = NULL;

  mxml_node_t *pNodeCaps = NULL;
  mxml_node_t *pNodeCap = NULL;
  mxml_node_t *pNodeCapID = NULL;
  mxml_node_t *pNodeCurValue = NULL;

  mxml_node_t *pNodeStatus = NULL;
  mxml_node_t *pNodeResultCode = NULL;
  mxml_node_t *pNodeResultCodes = NULL;
  mxml_node_t *pNodeCode = NULL;
  mxml_node_t *pNodeDesc = NULL;


  int i = 0, j = 0;
  char *pszXML = NULL;
  char szCode[10] = {0,};
  char szValue[CAPABILITY_MAX_VALUE_LENGTH] = {0,};

  pXml = mxmlNewXML("1.0");
  if( 0 == pGroups->nGroupCount || NULL == pXml )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateSetGroupDetailsRespXML: Group Count = %d, or XML is NULL",
            pGroups->nGroupCount);
    return pszXML;
  }

  pNodeSetGroupDetails = mxmlNewElement(pXml, "setGroupDetails");

  //status and result code of this response xml packet...
  pNodeStatus = mxmlNewElement(pNodeSetGroupDetails, "statusCode");
  mxmlNewText(pNodeStatus, 0, "S");

  //WeMo Bridge Plugin Device's information...
  if( pGroups->szPluginId[0] )
  {
    pNodePluginId = mxmlNewElement(pNodeSetGroupDetails, "pluginId");
    mxmlNewText(pNodePluginId, 0, pGroups->szPluginId);
  }
  pNodePluginMacAddr = mxmlNewElement(pNodeSetGroupDetails, "macAddress");
  mxmlNewText(pNodePluginMacAddr, 0, pGroups->szPluginMacAddress);
  if( pGroups->szPluginModelCode[0] )
  {
    pNodePluginModelCode = mxmlNewElement(pNodeSetGroupDetails, "modelCode");
    mxmlNewText(pNodePluginModelCode, 0, pGroups->szPluginModelCode);
    pNodePluginProductName = mxmlNewElement(pNodeSetGroupDetails, "productName");
    mxmlNewText(pNodePluginProductName, 0, getProductName(pGroups->szPluginModelCode));
  }

  pNodeGroups = mxmlNewElement(pNodeSetGroupDetails, "groups");

  for( i = 0; i < pGroups->nGroupCount; i++ )
  {
    pNodeGroup = mxmlNewElement(pNodeGroups, "group");

    pNodeGroupId = mxmlNewElement(pNodeGroup, "id");
    mxmlNewText(pNodeGroupId, 0, pGroups->group[i].szID);

    pNodeReferenceId = mxmlNewElement(pNodeGroup, "referenceId");
    mxmlNewText(pNodeReferenceId, 0, pGroups->group[i].szReferenceID);

    pNodeGroupName = mxmlNewElement(pNodeGroup, "groupName");
    mxmlNewText(pNodeGroupName, 0, pGroups->group[i].szName);

    pNodeStatus = mxmlNewElement(pNodeGroup, "statusCode");
    if( 200 == pGroups->group[i].nErrorCode )
    {
      mxmlNewText(pNodeStatus, 0, "S");
    }
    else
    {
      mxmlNewText(pNodeStatus, 0, "F");
    }
    pNodeResultCodes = mxmlNewElement(pNodeGroup, "resultCodes");
    pNodeResultCode = mxmlNewElement(pNodeResultCodes, "resultCode");

    pNodeCode = mxmlNewElement(pNodeResultCode, "code");
    snprintf(szCode, sizeof(szCode), "%d", pGroups->group[i].nErrorCode);
    mxmlNewText(pNodeCode, 0, szCode);

    pNodeDesc = mxmlNewElement(pNodeResultCode, "description");
    mxmlNewText(pNodeDesc, 0, pGroups->group[i].szErrorMsg);

    pNodeDevices = mxmlNewElement(pNodeGroup, "devices");

    for( j = 0; j < pGroups->group[i].nDeviceCount; j++ )
    {
      pNodeDevice = mxmlNewElement(pNodeDevices, "device");

      pNodeCloudDeviceID = mxmlNewElement(pNodeDevice, "deviceId");
      mxmlNewText(pNodeCloudDeviceID, 0, pGroups->group[i].szCloudDeviceID[j]);

      pNodeMacAddress = mxmlNewElement(pNodeDevice, "macAddress");
      mxmlNewText(pNodeMacAddress, 0, pGroups->group[i].szDeviceID[j]);

      pNodeFirmwareVer = mxmlNewElement(pNodeDevice, "firmwareVersion");
      mxmlNewText(pNodeFirmwareVer, 0, "");

      pNodeSignal = mxmlNewElement(pNodeDevice, "signalStrength");
      mxmlNewText(pNodeSignal, 0, "");
    }

    pNodeCaps = mxmlNewElement(pNodeGroup, "capabilities");

    for( j = 0; j < pGroups->group[i].nCapabilityCount; j++ )
    {
      pNodeCap = mxmlNewElement(pNodeCaps, "capability");

      pNodeCapID = mxmlNewElement(pNodeCap, "capabilityId");
      mxmlNewText(pNodeCapID, 0, pGroups->group[i].devCapability[j].szCapabilityID);

      pNodeCurValue = mxmlNewElement(pNodeCap, "currentValue");

      //TODO : Need to Young's comment
      //check if the capability value is sleep fader
      if( isSleepFaderValue(pGroups->group[i].devCapability[j].szCapabilityID,
                            pGroups->group[i].devCapability[j].szCapabilityValue) )
      {
        if( GetCurrentGroupLevelControl(pGroups->group[i].szReferenceID, szValue) )
        {
          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "GetCurrentGroupLevelControl: value = %s",
                  szValue);
          mxmlNewText(pNodeCurValue, 0, szValue);
        }
        else
        {
          mxmlNewText(pNodeCurValue, 0, pGroups->group[i].devCapability[j].szCapabilityValue);
        }
      }
      else
      {
        mxmlNewText(pNodeCurValue, 0, pGroups->group[i].devCapability[j].szCapabilityValue);
      }
    }
  }

  pszXML = mxmlSaveAllocString(pXml, MXML_NO_CALLBACK);

  mxmlDelete(pXml);

  return pszXML;
}


/***
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<setGroupDetails >
<refreshTimer>0</refreshTimer>
<pluginId>810002479</pluginId>
<macAddress>000C4353504A</macAddress>
<modelCode>Bridge</modelCode>
<groups>
<group><id>27</id>
<referenceId>1387286584</referenceId>
<groupName>Lighting Group</groupName>
<devices>
<device>
<deviceId>810002481</deviceId>
<macAddress>ED793100008D1500</macAddress>
</device>
<device>
<deviceId>810002483</deviceId>
<macAddress>477A3100008D1500</macAddress>
</device>
</devices>
<capabilities>
<capability>
<capabilityId>10006</capabilityId>
<currentValue>1</currentValue>
</capability>
</capabilities>
</group>
</groups>
</setGroupDetails>
***/


//Update Group's Capabilities
int SetGroupDetailsHandler(void *hndl, char *xmlBuf, unsigned int buflen, void* data_sock , void* remoteInfo, char* transaction_id)
{
  int retVal = PLUGIN_SUCCESS;

  mxml_node_t *tree = NULL;
  mxml_node_t *find_node = NULL;
  mxml_node_t *group_node = NULL;
  mxml_node_t *device_node = NULL;
  mxml_node_t *cap_node = NULL;

  char *pPluginId = NULL, *pMacAddress = NULL, *pModelCode = NULL;
  char *pGroupId = NULL, *pReferenceId = NULL, *pGroupName = NULL;
  char *pCloudDeviceId = NULL, *pDeviceId = NULL, *pCapabilityID = NULL, *pCurrentValue = NULL;

  char szErrorResponse[MAX_DATA_LEN];

  int nGroup = 0, nDevice = 0, nCapability = 0, retry_cnt = 0;

  struct Groups *pGroups = NULL;
  struct Group *pGroup = NULL;

  bool bSuccess = false;

  SD_GroupID groupID;

  if( !xmlBuf )
  {
    retVal = PLUGIN_FAILURE;
    return retVal;
  }

  pGroups = (struct Groups *)calloc(1, sizeof(struct Groups));
  if( !pGroups )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SetGroupDetails: Failed to allocate Groups");
    retVal = PLUGIN_FAILURE;
    return retVal;
  }

  if( buflen )
    CHECK_PREV_TSX_ID(E_SETGROUPDETAILS, transaction_id, retVal);

  tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

  if( NULL == tree )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "XML String %s couldn't be loaded", xmlBuf);

    retVal = PLUGIN_FAILURE;
    goto handler_exit1;
  }

  //Get infomation of WeMo Bridge device.
  pPluginId = getElemenetValue(tree, tree, &find_node, "pluginId");
  pMacAddress = getElemenetValue(tree, tree, &find_node, "macAddress");
  pModelCode = getElemenetValue(tree, tree, &find_node, "modelCode");

  if( pPluginId )
  {
    SAFE_STRCPY(pGroups->szPluginId, pPluginId);
  }
  if( pMacAddress )
  {
    SAFE_STRCPY(pGroups->szPluginMacAddress, pMacAddress);
  }
  if( pModelCode )
  {
    SAFE_STRCPY(pGroups->szPluginModelCode, pModelCode);
  }

  for( group_node = mxmlFindElement(tree, tree, "group", NULL, NULL, MXML_DESCEND);
      group_node != NULL;
      group_node = mxmlFindElement(group_node, tree, "group", NULL, NULL, MXML_DESCEND) )
  {
    pGroup = &(pGroups->group[nGroup]);

    pGroupId = getElemenetValue(group_node, tree, &find_node, "id");
    pReferenceId = getElemenetValue(group_node, tree, &find_node, "referenceId");
    pGroupName = getElemenetValue(group_node, tree, &find_node, "groupName");

    if( pGroupId )
    {
      SAFE_STRCPY(pGroup->szID, pGroupId);
    }
    if( pReferenceId )
    {
      SAFE_STRCPY(pGroup->szReferenceID, pReferenceId);
      MakeGroupID(pReferenceId, &groupID);
    }
    if( pGroupName )
    {
      SAFE_STRCPY(pGroup->szName, pGroupName);
    }

    for( device_node = mxmlFindElement(group_node, tree, "device", NULL, NULL, MXML_DESCEND);
        device_node != NULL;
        device_node = mxmlFindElement(device_node, tree, "device", NULL, NULL, MXML_DESCEND) )
    {
      pCloudDeviceId = getElemenetValue(device_node, tree, &find_node, "deviceId");
      if( pCloudDeviceId )
      {
        SAFE_STRCPY(pGroup->szCloudDeviceID[nDevice], pCloudDeviceId);
      }
      pDeviceId = getElemenetValue(device_node, tree, &find_node, "macAddress");
      if( pDeviceId )
      {
        SAFE_STRCPY(pGroup->szDeviceID[nDevice], pDeviceId);
      }
      nDevice++;
    }
    pGroup->nDeviceCount = nDevice;

    for( cap_node = mxmlFindElement(group_node, tree, "capability", NULL, NULL, MXML_DESCEND);
        cap_node != NULL;
        cap_node = mxmlFindElement(cap_node, tree, "capability", NULL, NULL, MXML_DESCEND) )
    {
      pCapabilityID = getElemenetValue(cap_node, tree, &find_node, "capabilityId");
      pCurrentValue = getElemenetValue(cap_node, tree, &find_node, "currentValue");
      if( pCapabilityID )
      {
        SAFE_STRCPY(pGroup->devCapability[nCapability].szCapabilityID, pCapabilityID);
      }
      if( pCurrentValue )
      {
        SAFE_STRCPY(pGroup->devCapability[nCapability].szCapabilityValue, pCurrentValue);

        //Set capability value to the Group...
        if( pReferenceId && pCapabilityID )
        {
          SD_CapabilityID capID;

          MakeCapabilityID(pCapabilityID, &capID);

          // Save Manually toggled time for rules

          for( retry_cnt = 0; retry_cnt < 3; retry_cnt++ )
          {
            if( (bSuccess = SD_SetGroupCapabilityValue(&groupID, &capID, pCurrentValue, SE_LOCAL + SE_REMOTE)) )
            {
              pluginUsleep(WAIT_50MS);
              break;
            }
          }

          if( bSuccess )
          {
            DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SD_SetGroupCapabilityValue(%s, %s, %s) command is success...", groupID.Data, capID.Data, pCurrentValue)
            pGroup->devCapability[nCapability].nStatus = 1;
          }
          else
          {
            DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SD_SetGroupCapabilityValue(%s, %s, %s) command is failed...", groupID.Data, capID.Data, pCurrentValue)
            pGroup->devCapability[nCapability].nStatus = 0;
          }
        }
      }
      nCapability++;
    }
    pGroup->nCapabilityCount = nCapability;

    if( bSuccess )
    {
      pGroups->group[nGroup].nErrorCode = 200;
      SAFE_STRCPY(pGroups->group[nGroup].szErrorMsg, "Success SetGroupCapabilityValue");
    }
    else
    {
      pGroups->group[nGroup].nErrorCode = 400;
      SAFE_STRCPY(pGroups->group[nGroup].szErrorMsg, "Failure SetGroupCapabilityValue");
    }

    nGroup++;
  }

  pGroups->nGroupCount = nGroup;

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SetGroupDetails xml parsing is Success...");
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Start Creating SetGroupDetails Response XML...")

  //Create Response XML
  char *pszResponse = CreateSetGroupDetailsRespXML(pGroups);

  if( pszResponse )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "==== SetGroupDetails Response XML ====");

    if( buflen )
    {
      retVal = SendNatPkt(hndl, pszResponse, remoteInfo, transaction_id, data_sock, E_SETGROUPDETAILS);
      DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pszResponse, strlen(pszResponse));
    }

    free(pszResponse);
  }
  else
  {
    if( 0 == pGroups->nGroupCount && pGroups->szPluginId[0] && pGroups->szPluginMacAddress[0] )
    {
      memset(szErrorResponse, 0x00, MAX_DATA_LEN);
      snprintf(szErrorResponse, MAX_DATA_LEN, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><setGroupDetails><statusCode>F</statusCode> \
      <pluginId>%s</pluginId><macAddress>%s</macAddress><modelCode>%s</modelCode></setGroupDetails>",
      pGroups->szPluginId, pGroups->szPluginMacAddress, pGroups->szPluginModelCode);

      if( buflen )
      {
        retVal = SendNatPkt(hndl, szErrorResponse, remoteInfo, transaction_id, data_sock, E_SETGROUPDETAILS);
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "==== SetGroupDetails Error Response XML ====");
        DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, szErrorResponse, strlen(szErrorResponse));
      }
    }
    else
    {
      retVal = PLUGIN_FAILURE;
    }
  }

handler_exit1:

  if( tree )
    mxmlDelete(tree);

  if( pGroups )
    free(pGroups);

  return retVal;
}

/*
<?xml version="1.0" encoding="UTF-8"?>
<groups xmlns:modelns="http://datamodel.components.cs.belkin.com/">
   <group>
      <id>116</id> --> what is meaning of id?
      <referenceId>9223372036854775807</referenceId>
      <groupName>JUnit1_1</groupName>
      <iconVersion>57</iconVersion>
      <devices>
         <device>
            <deviceId>870612</deviceId>
         </device>
         <device>
            <deviceId>870604</deviceId>
         </device>
      </devices>
      <capabilities>
         <capability>
            <capabilityId>1</capabilityId>
            <currentValue>0</currentValue>
         </capability>
      </capabilities>
   </group>
</groups>
*/

char* CreateGroupsRespXML(int nStatus, int nGroups, char szGroupIDs[][MAX_ID_LEN])
{
  mxml_node_t *pXml = NULL;
  mxml_node_t *pNodePlugin = NULL;
  mxml_node_t *pNodeRecipientId = NULL;
  mxml_node_t *pNodeMacAddress = NULL;
  mxml_node_t *pNodeStatus = NULL;
  mxml_node_t *pNodeFirmwareVersion = NULL;
  mxml_node_t *pNodeGroups = NULL;
  mxml_node_t *pNodeGroup = NULL;
  mxml_node_t *pNodeCapabilities = NULL;
  mxml_node_t *pNodeCapability = NULL;
  mxml_node_t *pNodeCapabilityID = NULL;
  mxml_node_t *pNodeCurValue = NULL;

  int i = 0, j = 0;

  char *pszXML = NULL;
  char szStatus[MAX_VAL_LEN] = {0,};

  SD_GroupID groupID;
  SD_Group group;

  pXml = mxmlNewXML("1.0");

  if( NULL == pXml )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateGroupsRespXML: XML is NULL");
    return pszXML;
  }

  snprintf(szStatus, MAX_VAL_LEN, "%d", nStatus);

  pNodePlugin = mxmlNewElement(pXml, "plugin");

  pNodeRecipientId = mxmlNewElement(pNodePlugin, "recipientId");
  mxmlNewText(pNodeRecipientId, 0, g_szPluginCloudId);

  pNodeMacAddress = mxmlNewElement(pNodePlugin, "macAddress");
  mxmlNewText(pNodeMacAddress, 0, g_szWiFiMacAddress);

  pNodeStatus = mxmlNewElement(pNodePlugin, "status");
  mxmlNewText(pNodeStatus, 0, szStatus);

  pNodeFirmwareVersion = mxmlNewElement(pNodePlugin, "firmwareVersion");
  mxmlNewText(pNodeFirmwareVersion, 0, g_szFirmwareVersion);

  if( nGroups )
  {
    pNodeGroups = mxmlNewElement(pNodePlugin, "groups");
    for( i = 0; i < nGroups; i++ )
    {
      pNodeGroup = mxmlNewElement(pNodeGroups, "group");
      pNodeCapabilities = mxmlNewElement(pNodeGroup, "capabilities");
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateGroupsRespXML: GroupID = %s", szGroupIDs[i])
      MakeGroupID(szGroupIDs[i], &groupID);
      if( SD_GetGroup(&groupID, &group) )
      {
        for( j = 0; j < group->CapabilityCount; j++ )
        {
          if( 0 == strcmp(group->Capability[j]->ProfileName, "Identify") ||
              0 == strcmp(group->Capability[j]->ProfileName, "LevelControl.Move") ||
              0 == strcmp(group->Capability[j]->ProfileName, "LevelControl.Stop") ||
              0 == strcmp(group->Capability[j]->ProfileName, "SensorTestMode") )
          {
            continue;
          }
          pNodeCapability = mxmlNewElement(pNodeCapabilities, "capability");
          pNodeCapabilityID = mxmlNewElement(pNodeCapability, "capabilityId");
          mxmlNewText(pNodeCapabilityID, 0, group->Capability[j]->CapabilityID.Data);
          pNodeCurValue = mxmlNewElement(pNodeCapability, "currentValue");
          mxmlNewText(pNodeCurValue, 0, group->CapabilityValue[j]);
        }
      }
    }
  }

  pszXML = mxmlSaveAllocString(pXml, MXML_NO_CALLBACK);

  mxmlDelete(pXml);

  return pszXML;
}

int RemoteSetDataStores (void *hndl, char *xmlBuf, unsigned int buflen, void*data_sock, void* remoteInfo,char* transaction_id)
{
  int retVal = PLUGIN_SUCCESS;
  int nStatus = 0;
  char *pName = NULL, *pValue = NULL;
  mxml_node_t *tree = NULL;
  mxml_node_t *find_node = NULL;

  char szErrorResponse[MAX_DATA_LEN];

  if( !xmlBuf )
  {
    retVal = PLUGIN_FAILURE;
    return retVal;
  }

  tree = mxmlLoadString (NULL, xmlBuf, MXML_OPAQUE_CALLBACK);
  if( NULL == tree )
  {
    return PLUGIN_FAILURE;
  }

  if( buflen )
    CHECK_PREV_TSX_ID(E_SETDATASTORE, transaction_id, retVal);

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SetDataStores XML String loaded is");
  DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, xmlBuf, buflen);

  pName = getElemenetValue(tree, tree, &find_node, "name");
  pValue = getElemenetValue(tree, tree, &find_node, "value");

  if( (NULL != pName) && (NULL != pValue) )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Name = %s", pName);
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Value = %s", pValue);

    if (true == (SD_SetDataStores (pName, pValue)))
      nStatus = 0;
  }
  else if( NULL != pName )
  {
    SD_DeleteDataStores (pName);
    nStatus = -1;
  }
  //Parsing xml data and set emergency contact data

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SetDataStores xml parsing is Success...");
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Start Creating SetDataStores Response XML...")

  //Create Response XML
  char *pszResponse = SetDataStoreRespXML(nStatus);

  if( pszResponse )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "==== SetDataStores Response XML ====");

    if( buflen )
    {
      retVal = SendNatPkt(hndl, pszResponse, remoteInfo, transaction_id, data_sock, E_SETDATASTORE);
      DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pszResponse, strlen(pszResponse));
    }

    free(pszResponse);
  }

  else
  {
   // To do Error response
    memset(szErrorResponse, 0x00, MAX_DATA_LEN);

    snprintf(szErrorResponse, MAX_DATA_LEN, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><plugin><statusCode>F</statusCode> \
    <pluginId>%s</pluginId><macAddress>%s</macAddress></plugin>",
    g_szPluginCloudId, g_szWiFiMacAddress);

    retVal = SendNatPkt(hndl, szErrorResponse, remoteInfo, transaction_id, data_sock, E_GETDATASTORE);
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "==== SetDataStore Error Response XML ====");
    DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, szErrorResponse, strlen(szErrorResponse));
  }


handler_exit1:

  return retVal;
}


int RemoteGetDataStores (void *hndl, char *xmlBuf, unsigned int buflen, void*data_sock, void* remoteInfo,char* transaction_id)
{
  int retVal = PLUGIN_SUCCESS;
  int nStatus = 0;
  char *pName = NULL, *pData = NULL;
  char *pszResponse = NULL;
  mxml_node_t *tree = NULL;
  mxml_node_t *find_node = NULL;

  char szErrorResponse[MAX_DATA_LEN] = {0, };
  char getDataValue[MAX_DATA_LEN] = {0, };

  if( !xmlBuf )
  {
    retVal = PLUGIN_FAILURE;
    return retVal;
  }

  tree = mxmlLoadString (NULL, xmlBuf, MXML_OPAQUE_CALLBACK);
  if( NULL == tree )
  {
    return PLUGIN_FAILURE;
  }

  if( buflen )
    CHECK_PREV_TSX_ID(E_GETDATASTORE, transaction_id, retVal);

  find_node = mxmlFindElement(tree, tree, "macAddress", NULL, NULL, MXML_DESCEND);

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "GetDataStores XML String loaded is");
  DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, xmlBuf, buflen);

  pName = getElemenetValue(tree, tree, &find_node, "name");
  if( pName )
  {
    pData = SD_GetDataStores(pName, getDataValue, MAX_DATA_LEN);
    if( pData )
    {
      nStatus = 0;
    }
    else
    {
      nStatus = -1;
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pData is NULL");
    }
  }
  else
  {
      nStatus = -1;
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "pName is NULL");
  }

  if (0 == nStatus )
  {
      //Create Response XML
      //char *pszResponse = GetDataStoreRespXML(nStatus, pData);

      pszResponse = (char *) calloc (SIZE_8192B, sizeof (char));
      if( NULL == pszResponse )
      {
          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Memory Allocation failed.");
      }

      else
      {
          //Parsing xml data and get emergency contact data
          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "GetDataStores xml parsing is Success...");
          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Start Creating GetDataStores Response XML...")

          snprintf (pszResponse, SIZE_8192B * sizeof (char),
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<plugin>"
            "<recipientId>%s</recipientId>"
            "<macAddress>%s</macAddress>"
            "<status>%d</status>"
            "<friendlyName>%s</friendlyName>"
            "<statusTS></statusTS>"
            "<firmwareVersion>%s</firmwareVersion>"
            "<fwUpgradeStatus>%d</fwUpgradeStatus>"
            "<signalStrength>%d</signalStrength>"
            "<dataStores action=\"GetDataStores\">"
            "<dataStore>"
            "<name>%s</name>"
            "<value>%s</value>"
            "</dataStore></dataStores></plugin>",
            g_szPluginCloudId, g_szWiFiMacAddress, nStatus,
            g_szFriendlyName, g_szFirmwareVersion, getCurrFWUpdateState(),
            gSignalStrength, pName, pData);

            if( buflen )
            {
                DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "==== GetDataStores Response XML ====");
                retVal = SendNatPkt(hndl, pszResponse, remoteInfo, transaction_id, data_sock, E_GETDATASTORE);
                DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pszResponse, strlen(pszResponse));
            }

            free(pszResponse);
            pszResponse = NULL;
        }
  }

  else if (-1 == nStatus)
  {
      memset(szErrorResponse, 0x00, MAX_DATA_LEN);

      snprintf (szErrorResponse, MAX_DATA_LEN,
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                "<plugin>"
                "<recipientId>%s</recipientId>"
                "<macAddress>%s</macAddress>"
                "<status>%d</status>"
                "<friendlyName>%s</friendlyName>"
                "<statusTS></statusTS>"
                "<firmwareVersion>%s</firmwareVersion>"
                "<fwUpgradeStatus>%d</fwUpgradeStatus>"
                "<signalStrength>%d</signalStrength>"
                "<errorCode>%d<errorCode></plugin>",
                g_szPluginCloudId, g_szWiFiMacAddress, nStatus,
                g_szFriendlyName, g_szFirmwareVersion, getCurrFWUpdateState(),
                gSignalStrength, DATA_STORE_NOTHING_FOUND_FOR_NAME);

       DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "==== GetDataStore Error Response XML ====");
       retVal = SendNatPkt(hndl, szErrorResponse, remoteInfo, transaction_id, data_sock, E_GETDATASTORE);
       DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, szErrorResponse, strlen(szErrorResponse));
  }

  else
  {

  }

handler_exit1:

  return retVal;
}

int remoteAccessSetDevicePreset(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void*remoteInfo,char*transaction_id)
{
	int retVal = PLUGIN_SUCCESS;
	mxml_node_t *tree=NULL;
	mxml_node_t *find_node=NULL;
	char statusResp[MAX_DATA_LEN] = {0};
	char *sMac = NULL;
	char macAddress[MAX_MAC_LEN];
	char *presetXMLData = NULL;
	int status = -1;

	if (!xmlBuf)
	{
		retVal = PLUGIN_FAILURE;
		goto handler_exit1;
	}
	CHECK_PREV_TSX_ID(E_SETDEVICEPRESET,transaction_id,retVal);
	tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

	if (tree){
    		DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "XML String loaded is %s", xmlBuf);
		find_node = mxmlFindElement(tree, tree, "macAddress", NULL, NULL, MXML_DESCEND);
		if (find_node) {
			strncpy(macAddress, (find_node->child->value.opaque), sizeof(macAddress)-1);
			sMac = utilsRemDelimitStr(GetMACAddress(), ":");
			if (!strncmp(macAddress, sMac, strlen(sMac))) {
				presetXMLData = strstr(xmlBuf, "<setDevicePreset>");
				if (presetXMLData)
				{
					retVal = processAndSetDevicePreset(presetXMLData);
					if(PLUGIN_SUCCESS == retVal)
						status = 1;
					else
						DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "set device preset failed!");

					snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><status>%d</status></plugin>",sMac,status);

					retVal=SendNatPkt(hndl,statusResp,remoteInfo,transaction_id,data_sock,E_SETDEVICEPRESET);
				}
				else
				{
					DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "tag not present in xml payload");
					retVal = PLUGIN_FAILURE;
				}
			} else {
				DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Mac received %s doesn't match with Link Mac %s", macAddress, sMac);
				retVal = PLUGIN_FAILURE;
			}
			mxmlDelete(find_node);
		}else {
			DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Link macAddress tag not present in xml payload");
			retVal = PLUGIN_FAILURE;
		}
		mxmlDelete(tree);
	}else{
		DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "XML String %s couldn't be loaded", xmlBuf);
		retVal = PLUGIN_FAILURE;
	}
handler_exit1:
	if (sMac) {free(sMac); sMac = NULL;}
	return retVal;
}

int remoteAccessGetDevicePreset(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void*remoteInfo,char*transaction_id)
{
	int retVal = PLUGIN_SUCCESS;
	mxml_node_t *tree=NULL;
	mxml_node_t *find_node=NULL;
	char statusResp[MAX_DATA_LEN];
	char *sMac = NULL;
	char macAddress[MAX_MAC_LEN];
	char *presetXMLData = NULL;
	char *presetRespXMLBuff = NULL;
	int status = -1;

	if (!xmlBuf)
	{
		retVal = PLUGIN_FAILURE;
		goto handler_exit1;
	}
	CHECK_PREV_TSX_ID(E_GETDEVICEPRESET,transaction_id,retVal);
	tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

	if (tree){
    		DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "XML String loaded is %s", xmlBuf);
		find_node = mxmlFindElement(tree, tree, "macAddress", NULL, NULL, MXML_DESCEND);
		if (find_node) {
			strncpy(macAddress, (find_node->child->value.opaque), sizeof(macAddress)-1);
			sMac = utilsRemDelimitStr(GetMACAddress(), ":");
			if (!strncmp(macAddress, sMac, strlen(sMac))) {
				presetXMLData = strstr(xmlBuf, "<getDevicePreset>");
				if (presetXMLData)
				{
					retVal = processAndGetDevicePreset(presetXMLData, &presetRespXMLBuff);
					if(PLUGIN_SUCCESS == retVal)
					{
						status = 1;
						snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><status>%d</status>%s</plugin>",sMac,status,presetRespXMLBuff);

					}
					else
					{
						DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "get device preset failed!");
						snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><status>%d</status></plugin>",sMac,status);
					}

					retVal=SendNatPkt(hndl,statusResp,remoteInfo,transaction_id,data_sock,E_GETDEVICEPRESET);
				}
				else
				{
					DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "tag not present in xml payload");
					retVal = PLUGIN_FAILURE;
				}
			} else {
				DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Mac received %s doesn't match with Link Mac %s", macAddress, sMac);
				retVal = PLUGIN_FAILURE;
			}
			mxmlDelete(find_node);
		}else {
			DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Link macAddress tag not present in xml payload");
			retVal = PLUGIN_FAILURE;
		}
		mxmlDelete(tree);
	}else{
		DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "XML String %s couldn't be loaded", xmlBuf);
		retVal = PLUGIN_FAILURE;
	}
handler_exit1:
	/*free response xml buffer*/
	if(presetRespXMLBuff) {free(presetRespXMLBuff);presetRespXMLBuff=NULL;}
	if (sMac) {free(sMac); sMac = NULL;}
	return retVal;
}

int remoteAccessDeleteDevicePreset(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void*remoteInfo,char*transaction_id)
{
	int retVal = PLUGIN_SUCCESS;
	mxml_node_t *tree=NULL;
	mxml_node_t *find_node=NULL;
	char statusResp[MAX_DATA_LEN];
	char *sMac = NULL;
	char macAddress[MAX_MAC_LEN];
	char *presetXMLData = NULL;
	int status = -1;

	if (!xmlBuf)
	{
		retVal = PLUGIN_FAILURE;
		goto handler_exit1;
	}
	CHECK_PREV_TSX_ID(E_DELETEDEVICEPRESET,transaction_id,retVal);
	tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

	if (tree){
    		DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "XML String loaded is %s", xmlBuf);
		find_node = mxmlFindElement(tree, tree, "macAddress", NULL, NULL, MXML_DESCEND);
		if (find_node) {
			strncpy(macAddress, (find_node->child->value.opaque), sizeof(macAddress)-1);
			sMac = utilsRemDelimitStr(GetMACAddress(), ":");
			if (!strncmp(macAddress, sMac, strlen(sMac))) {
				presetXMLData = strstr(xmlBuf, "<deleteDevicePreset>");
				if (presetXMLData)
				{
					retVal = processAndDeleteDevicePreset(presetXMLData);
					if(PLUGIN_SUCCESS == retVal)
						status = 1;
					else
						DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "delete device preset failed!");

					snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><status>%d</status></plugin>",sMac,status);

					retVal=SendNatPkt(hndl,statusResp,remoteInfo,transaction_id,data_sock,E_DELETEDEVICEPRESET);
				}
				else
				{
					DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "tag not present in xml payload");
					retVal = PLUGIN_FAILURE;
					goto handler_exit1;
				}
			} else {
				DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Mac received %s doesn't match with Link Mac %s", macAddress, sMac);
				retVal = PLUGIN_FAILURE;
			}
			mxmlDelete(find_node);
		}else {
			DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Link macAddress tag not present in xml payload");
			retVal = PLUGIN_FAILURE;
		}
		mxmlDelete(tree);
	}else{
		DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "XML String %s couldn't be loaded", xmlBuf);
		retVal = PLUGIN_FAILURE;
	}
handler_exit1:
	if (sMac) {free(sMac); sMac = NULL;}
	return retVal;
}

int remoteAccessDeleteDeviceAllPreset(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void*remoteInfo,char*transaction_id)
{
	int retVal = PLUGIN_SUCCESS;
	mxml_node_t *tree=NULL;
	mxml_node_t *find_node=NULL;
	char statusResp[MAX_DATA_LEN];
	char *sMac = NULL;
	char macAddress[MAX_MAC_LEN];
	char *presetXMLData = NULL;
	int status = -1;

	if (!xmlBuf)
	{
		retVal = PLUGIN_FAILURE;
		goto handler_exit1;
	}
	CHECK_PREV_TSX_ID(E_DELETEDEVICEALLPRESET,transaction_id,retVal);
	tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

	if (tree){
    		DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "XML String loaded is %s", xmlBuf);
		find_node = mxmlFindElement(tree, tree, "macAddress", NULL, NULL, MXML_DESCEND);
		if (find_node) {
			strncpy(macAddress, (find_node->child->value.opaque), sizeof(macAddress)-1);
			sMac = utilsRemDelimitStr(GetMACAddress(), ":");
			if (!strncmp(macAddress, sMac, strlen(sMac))) {
				presetXMLData = strstr(xmlBuf, "<deleteDeviceAllPreset>");
				if (presetXMLData)
				{
					retVal = processAndDeleteDeviceAllPreset(presetXMLData);
					if(PLUGIN_SUCCESS == retVal)
						status = 1;
					else
						DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "delete device all presets failed!");

					snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><status>%d</status></plugin>",sMac,status);

					retVal=SendNatPkt(hndl,statusResp,remoteInfo,transaction_id,data_sock,E_DELETEDEVICEALLPRESET);
				}
				else
				{
					DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "tag not present in xml payload");
					retVal = PLUGIN_FAILURE;
				}
			} else {
				DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Mac received %s doesn't match with Link Mac %s", macAddress, sMac);
				retVal = PLUGIN_FAILURE;
			}
			mxmlDelete(find_node);
		}else {
			DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Link macAddress tag not present in xml payload");
			retVal = PLUGIN_FAILURE;
		}
		mxmlDelete(tree);
	}else{
		DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "XML String %s couldn't be loaded", xmlBuf);
		retVal = PLUGIN_FAILURE;
	}
handler_exit1:
	if (sMac) {free(sMac); sMac = NULL;}
	return retVal;
}

int PerformRemoteCreateGroup(char *pXmlBuff, int *pnGroups, char szGroupIDs[][MAX_ID_LEN])
{
  mxml_node_t *tree = NULL;
  mxml_node_t *node = NULL;
  mxml_node_t *find_node = NULL;

  char *pszElementName[5] = {
    "GroupID",
    "GroupName",
    "DeviceIDList",
    "GroupCapabilityIDs",
    "GroupCapabilityValues"
  };

  int i = 0, j = 0, nRet = PLUGIN_FAILURE;
  int nGroups = 0, nDevices = 0;
  int nDeviceID = 0, nCapIDs = 0, nCapValues = 0, retry_cnt = 0;

  bool bSuccess = false;

  SD_GroupID groupID;
  SD_DeviceID deviceID[MAX_DEVICES];

  char szGroupName[MAX_NAME_LEN];
  char szDeviceIDs[MAX_DEVICES][MAX_ID_LEN];
  char szGroupCapIDs[MAX_GROUPS][MAX_ID_LEN];
  char szGroupCapValues[MAX_GROUPS][MAX_VAL_LEN];

  *pnGroups = 0;

  tree = mxmlLoadString(NULL, pXmlBuff, MXML_OPAQUE_CALLBACK);
  if( NULL == tree )
  {
    return PLUGIN_FAILURE;
  }

  //First, find the <SetGroupDetails> element in xml buffer.
  for( node = mxmlFindElement(tree, tree, "CreateGroup", NULL, NULL, MXML_DESCEND);
       node != NULL;
       node = mxmlFindElement(node, tree, "CreateGroup", NULL, NULL, MXML_DESCEND) )
  {
    nCapIDs = 0;
    nCapValues = 0;
    nDeviceID = 0;

    for( i = 0; i < 5; i++ )
    {
      find_node = mxmlFindElement(node, tree, pszElementName[i], NULL, NULL, MXML_DESCEND);

      if( NULL == find_node )
      {
        continue;
      }

      char *pValue = (char *)mxmlGetOpaque(find_node);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "%s = %s", pszElementName[i], pValue);

      //GroupID is mandatory, the other elements are optional...
      if( 0 == strcmp(pszElementName[i], "GroupID") )
      {
        MakeGroupID(pValue, &groupID);
        memcpy(&szGroupIDs[nGroups], pValue, MAX_ID_LEN);
      }
      else if( 0 == strcmp(pszElementName[i], "GroupName") )
      {
        memset(szGroupName, 0x00, sizeof(szGroupName));
        SAFE_STRCPY(szGroupName, pValue);
      }
      else if( 0 == strcmp(pszElementName[i], "DeviceIDList") )
      {
        memset(&szDeviceIDs, 0x00, sizeof(szDeviceIDs));
        nDeviceID = GetIDs(pValue, szDeviceIDs);
        if( nDeviceID > MAX_DEVICES )
        {
          nDeviceID = MAX_DEVICES;
        }
        for( j = 0 ; j < nDeviceID; j++ )
        {
          MakeDeviceID(szDeviceIDs[j], &deviceID[j]);
        }
      }
      else if( 0 == strcmp(pszElementName[i], "GroupCapabilityIDs") )
      {
        memset(&szGroupCapIDs, 0x00, sizeof(szGroupCapIDs));
        nCapIDs = GetIDs(pValue, szGroupCapIDs);
        for( j = 0; j < nCapIDs; j++ )
        {
          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "PerformRemoteCreateGroup: CapabilityID = %s", szGroupCapIDs[j]);
        }
      }
      else if( 0 == strcmp(pszElementName[i], "GroupCapabilityValues") )
      {
        memset(&szGroupCapValues, 0x00, sizeof(szGroupCapValues));
        nCapValues = GetValues(pValue, szGroupCapValues);
        for( j = 0; j < nCapValues; j++ )
        {
          DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "PerformRemoteCreateGroup: CapabilityValue = %s", szGroupCapValues[j]);
        }
      }

      nDevices = SD_SetGroupDevices(&groupID, szGroupName, deviceID, nDeviceID);

      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Delete rules and SleepFader of individual devices. total devices: %d \n", nDeviceID);
      for( j = 0 ; j < nDeviceID; j++ )
      {
        deviceID[j].Type = SD_DEVICE_EUID;


        // Delete SleepFader Reserved timer.
        SM_CancelReservedCapabilityUpdateById(deviceID[j].Data);

        // Clear cache value
        SD_ClearSleepFaderCache(&deviceID[j]);
      }

      // Rule Manual Toggled history will be inherited.

      if( nDevices )
      {
        nRet = PLUGIN_SUCCESS;

        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "PerformRemoteCreateGroup: SD_SetGroupDevices [%d] devices Success...", nDevices)

        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "PerformRemoteCreateGroup: nCapIDs = %d, nCapValues = %d", nCapIDs, nCapValues);

        if( (nCapIDs && nCapValues) && (nCapIDs == nCapValues) )
        {
          for( j = 0; j < nCapIDs; j++ )
          {
            SD_CapabilityID capabilityID;

            MakeCapabilityID(szGroupCapIDs[j], &capabilityID);
            if( IsProfileCapabilityID("OnOff", &capabilityID)
                    || IsProfileCapabilityID("LevelControl", &capabilityID)
                    || IsProfileCapabilityID("SleepFader", &capabilityID)
                    || IsProfileCapabilityID("ColorControl", &capabilityID)
                    || IsProfileCapabilityID("ColorTemperature", &capabilityID)
                    || IsProfileCapabilityID("IASZone", &capabilityID)
                    || IsProfileCapabilityID("SensorConfig", &capabilityID)
                    || IsProfileCapabilityID("SensorKeyPress", &capabilityID) )
            {
              for( retry_cnt = 0; retry_cnt < 3; retry_cnt++ )
              {
                if( (bSuccess = SD_SetGroupCapabilityValue(&groupID, &capabilityID, szGroupCapValues[j], SE_NONE)) )
                {
                  break;
                }
              }
              pluginUsleep(WAIT_200MS);
            }

            if( bSuccess )
            {
              DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "PerformRemoteCreateGroup: ID = %s, Value = %s is success", szGroupCapIDs[j], szGroupCapValues[j]);
            }
            else
            {
              DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "PerformRemoteCreateGroup: ID = %s, Value = %s is failed", szGroupCapIDs[j], szGroupCapValues[j]);
              nRet = PLUGIN_FAILURE;
            }
            if( j == 1 )
            {
              pluginUsleep(WAIT_100MS);
            }
          }
        }
      }
      else
      {
        nRet = PLUGIN_FAILURE;
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "PerformRemoteCreateGroup: SD_SetGroupDevices [%d] devices...", nDevices)
      }
    }

    nGroups++;
    *pnGroups = nGroups;
  }

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "PerformRemoteCreateGroup: nGroups = %d", nGroups);

  mxmlDelete(tree);

  return nRet;
}

/*
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<CreateGroup>
<![CDATA[ <CreateGroup>
<GroupID>1381214810</GroupID>
<GroupName>Group1</GroupName>
<DeviceIDList>012509132000BE00,032509132000BE00,052509132000BE00</DeviceIDList>
<GroupCapabilityIDs>10006,10008</GroupCapabilityIDs>
<GroupCapabilityValues>0,50:80</GroupCapabilityValues>
</CreateGroup> ]]>
</CreateGroup>

<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<CreateGroup>
<macAddress>000C43305010</macAddress>
<refreshTimer>0</refreshTimer>
<GroupID>101</GroupID>
<GroupName>IT_Group1</GroupName>
<DeviceIDList>8A683100008D1500</DeviceIDList>
<GroupCapabilityIDs>10008</GroupCapabilityIDs>
<GroupCapabilityValues>50:80</GroupCapabilityValues>
</CreateGroup>
*/

int CreateGroupHandler(void *hndl, char *xmlBuf, unsigned int buflen, void* data_sock , void* remoteInfo, char* transaction_id)
{
  int nRet = PLUGIN_FAILURE;
  int retVal = PLUGIN_SUCCESS;
  int nStatus = 0, nGroups = 0;
  char szGroupIDs[MAX_GROUPS][MAX_ID_LEN];

  if( !xmlBuf )
  {
    retVal = PLUGIN_FAILURE;
    return retVal;
  }

  memset(&szGroupIDs, 0x00, sizeof(szGroupIDs));

  if( buflen )
    CHECK_PREV_TSX_ID(E_CREATEGROUP, transaction_id, retVal);

  //Parsing xml data and run group command to create group
  if( PLUGIN_FAILURE == (nRet = PerformRemoteCreateGroup(xmlBuf, &nGroups, szGroupIDs)) )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateGroupHandler: PerformRemoteCreateGroup is Failed...");
    nStatus = -1;
  }
  else
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateGroupHandler: PerformRemoteCreateGroup is Success, group cnt = [%d]...", nGroups);
    nStatus = 1;
  }

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "CreateGroup xml parsing is Success...");
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Start Creating CreateGroup Response XML...")

  //Create Response XML
  char *pszResponse = CreateGroupsRespXML(nStatus, nGroups, szGroupIDs);

  if( pszResponse )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "==== CreateGroup Response XML ====");

    if( buflen )
    {
      retVal = SendNatPkt(hndl, pszResponse, remoteInfo, transaction_id, data_sock, E_CREATEGROUP);
      DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pszResponse, strlen(pszResponse));
    }

    free(pszResponse);
  }

handler_exit1:

  return retVal;
}


int PerformRemoteDeleteGroup(char *pGroupID)
{
  int retVal = PLUGIN_FAILURE;

  SD_GroupID groupID;

  if( pGroupID )
  {
    MakeGroupID(pGroupID, &groupID);


    // Reserve UPnP Notification for individual bulb.
    SD_ReserveNotificationiForDeleteGroup(&groupID);

    // Delete SleepFader Reserved timer.
    SM_CancelReservedCapabilityUpdate(&groupID);

    // Clear cache value
    SD_ClearSleepFaderCache(&groupID);

    //Delete the Group
    if( SD_DeleteGroup(&groupID) )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SD_DeleteGroup(%s) is Success...", groupID.Data);
      retVal = PLUGIN_SUCCESS;
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "SD_DeleteGroup(%s) is Failed...", groupID.Data);
    }
  }

  return retVal;
}

/*
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<DeleteGroup>
<referenceId>1381214810</referenceId>
</DeleteGroup>
*/

int DeleteGroupHandler(void *hndl, char *xmlBuf, unsigned int buflen, void* data_sock , void* remoteInfo, char* transaction_id)
{
  int retVal = PLUGIN_SUCCESS;

  mxml_node_t *tree = NULL;
  mxml_node_t *group_node = NULL;
  mxml_node_t *find_node = NULL;

  int nRet = 0, nStatus = 0;
  int nGroups = 0;
  char *pGroupID = NULL;
  char szGroupIDs[MAX_GROUPS][MAX_ID_LEN];

  if( !xmlBuf )
  {
    retVal = PLUGIN_FAILURE;
    return retVal;
  }

  if( buflen )
    CHECK_PREV_TSX_ID(E_DELETEGROUP, transaction_id, retVal);

  tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

  if( NULL == tree )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "DeleteGroup: XML String %s couldn't be loaded", xmlBuf);

    retVal = PLUGIN_FAILURE;
    goto handler_exit1;
  }

  memset(&szGroupIDs, 0x00, sizeof(szGroupIDs));

  for( group_node = mxmlFindElement(tree, tree, "DeleteGroup", NULL, NULL, MXML_DESCEND);
      group_node != NULL;
      group_node = mxmlFindElement(group_node, tree, "DeleteGroup", NULL, NULL, MXML_DESCEND) )
  {
    pGroupID = getElemenetValue(group_node, tree, &find_node, "referenceId");
    if( PLUGIN_FAILURE == (nRet = PerformRemoteDeleteGroup(pGroupID)) )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "DeleteGroupHandler: PerformRemoteDeleteGroup is Failed...");
      nStatus = 1;
    }
    else
    {
      deleteAllPreset(pGroupID);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "DeleteGroupHandler: PerformRemoteDeleteGroup is Success...");
      nStatus = 1;
    }
    SAFE_STRCPY(szGroupIDs[nGroups], pGroupID);
    nGroups++;
  }

  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "DeleteGroup xml parsing is Success...");
  DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Start Creating DeleteGroup Response XML...")

  //Create Response XML
  char *pszResponse = CreateGroupsRespXML(nStatus, nGroups, szGroupIDs);
  if( pszResponse )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "==== DeleteGroup Response XML ====");

    if( buflen )
    {
      retVal = SendNatPkt(hndl, pszResponse, remoteInfo, transaction_id, data_sock, E_DELETEGROUP);
      DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pszResponse, strlen(pszResponse));
    }

    free(pszResponse);
  }

handler_exit1:

  if( tree )
    mxmlDelete(tree);

  return retVal;
}

int remoteAccessChangeFriendlyName(void* hndl, char* xmlBuf, unsigned int buflen,
                                    void* data_sock, void* remoteInfo, char* transaction_id)
{
  int retVal = PLUGIN_SUCCESS;
  mxml_node_t *tree = NULL;
  mxml_node_t *find_node = NULL;
  char *pPluginId = NULL, *pMacAddress = NULL, *pFriendlyName = NULL;
  char statusResp[SIZE_256B];
  SD_DeviceID deviceID;

  if( !xmlBuf )
  {
    retVal = PLUGIN_FAILURE;
    goto handler_exit1;
  }

  CHECK_PREV_TSX_ID(E_CHANGEFRIENDLYNAME, transaction_id, retVal);  //TBD

  tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

  if( tree )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "ChangeFriendlyName XML String loaded is");
    DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, xmlBuf, buflen);

    pPluginId = getElemenetValue(tree, tree, &find_node, "pluginId");
    pMacAddress = getElemenetValue(tree, tree, &find_node, "macAddress");
    pFriendlyName = getElemenetValue(tree, tree, &find_node, "friendlyName");

    if( (NULL != pPluginId) && (NULL != pMacAddress) && (NULL != pFriendlyName) )
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "PluginID = %s", pPluginId);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Bulb EUID = %s", pMacAddress);
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "FriendlyName = %s", pFriendlyName);

      MakeDeviceID(pMacAddress, &deviceID);
      memset(statusResp, 0x0, SIZE_256B);

      if( SD_SetDeviceProperty(&deviceID, SD_DEVICE_FRIENDLY_NAME, pFriendlyName) )
      {
        //Create xml response
        snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><recipientId>%s</recipientId><macAddress>%s</macAddress><status>1</status><friendlyName>%s</friendlyName></plugin>\n", pPluginId, pMacAddress, pFriendlyName);

      }
      else
      {
        snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><recipientId>%s</recipientId><macAddress>%s</macAddress><status>0</status><friendlyName>%s</friendlyName></plugin>\n", pPluginId, pMacAddress, pFriendlyName);
      }

      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "friendlyName response created is ");
      DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, statusResp, strlen(statusResp));

      //Send this response towards cloud synchronously using same data socket
      retVal = SendNatPkt(hndl, statusResp, remoteInfo, transaction_id, data_sock, E_CHANGEFRIENDLYNAME); //TBD
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "ChangeFriendlyName all tag not present in xml payload");
      retVal = PLUGIN_FAILURE;
    }

    mxmlDelete(tree);
  }
  else
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "ChangeFriendlyName XML String %s couldn't be loaded", xmlBuf);
    retVal = PLUGIN_FAILURE;
  }

handler_exit1:
  remoteAccessUpdateStatusTSParams (0xFF);
  return retVal;
}

int remoteAccessResetNameRulesData(void* hndl, char* xmlBuf, unsigned int buflen,
                                    void* data_sock, void* remoteInfo, char* transaction_id)
{
  int retVal = PLUGIN_SUCCESS;
  mxml_node_t *tree = NULL;
  mxml_node_t *find_node = NULL;
  char statusResp[SIZE_1024B];
  char *pMacAddress = NULL, *pResetType = NULL;
  int nResetType = 0;
  SD_DeviceID deviceID;
  SD_Device device;

  if( !xmlBuf )
  {
    retVal = PLUGIN_FAILURE;
    goto handler_exit1;
  }

  CHECK_PREV_TSX_ID(E_RESETNAMERULESDATA, transaction_id, retVal);  //TBD
  tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

  if( tree )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "ResetNameRulesData XML String loaded is %s", xmlBuf);
    DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, xmlBuf, buflen);

    pMacAddress = getElemenetValue(tree, tree, &find_node, "macAddress");
    pResetType = getElemenetValue(tree, tree, &find_node, "resetType");

    if( (NULL != pMacAddress) && (NULL != pResetType) )
    {
      nResetType = atoi(pResetType);
      memset(statusResp, 0x0, SIZE_256B);

      MakeDeviceID(pMacAddress, &deviceID);

      //[WEMO-42914] - Check NULL to avoid SIGSEGV
      if( !SD_GetDevice(&deviceID, &device) )
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "ResetNameRulesData device is not available");
        retVal = PLUGIN_FAILURE;
        mxmlDelete(tree);
        goto handler_exit1;
      }

      if( ((0 == strcmp(device->ManufacturerName, "Osram Lightify")) || (strstr(device->ManufacturerName, "OSRAM"))) &&
          ((0 == strcmp(device->ModelCode, "LIGHTIFY Gardenspot RGB")) || (0 == strcmp(device->ModelCode, "Gardenspot RGB"))) )
      {
        SAFE_STRCPY((char*)device->FriendlyName, (char*)"Osram Lightify Garden RGB");
      }
      else if( ((0 == strcmp(device->ManufacturerName, "Osram Lightify")) || (strstr(device->ManufacturerName, "OSRAM"))) &&
               ((0 == strcmp(device->ModelCode, "LIGHTIFY A19 Tunable White")) || (0 == strcmp(device->ModelCode, "Classic A60 TW"))) )
      {
        SAFE_STRCPY((char*)device->FriendlyName, (char*)"Osram Lightify TW60");
      }
      else if( ((0 == strcmp(device->ManufacturerName, "Osram Lightify")) || (strstr(device->ManufacturerName, "OSRAM"))) &&
               ((0 == strcmp(device->ModelCode, "LIGHTIFY Flex RGBW")) || (0 == strcmp(device->ModelCode, "Flex RGBW"))) )
      {
        SAFE_STRCPY((char*)device->FriendlyName, (char*)"Osram Lightify Flex RGBW");
      }
      else if( ((0 == strcmp(device->ManufacturerName, "Osram Lightify")) || (strstr(device->ManufacturerName, "OSRAM"))) &&
               (0 == strcmp(device->ModelCode, "iQBR30")) )
      {
        SAFE_STRCPY((char*)device->FriendlyName, (char*)"OSRAM Lightify BR30");
      }
      else
      {
        SAFE_STRCPY((char*)device->FriendlyName, (char*)"Lightbulb");
      }

      if( SD_SetDeviceProperty(&deviceID, SD_DEVICE_FRIENDLY_NAME, device->FriendlyName))
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Bulb friendlyName is %s", device->FriendlyName);
      }
      else
      {
        DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Bulb friendlyName is failed to change the name");
      }

      resetFNGlobalsToDefaults(nResetType);
      ExecuteReset(nResetType);

      snprintf(statusResp,sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><macAddress>%s</macAddress><status>1</status><friendlyName>%s</friendlyName><iconVersion>%d</iconVersion></plugin>", pMacAddress, device->FriendlyName, 0);

      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "ResetNameRulesData response created is ");
      DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, statusResp, strlen(statusResp));

      //Send this response towards cloud synchronously using same data socket
      retVal = SendNatPkt(hndl, statusResp, remoteInfo, transaction_id, data_sock, E_RESETNAMERULESDATA); //TBD
    }
    else
    {
      DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "ResetNameRulesData macAddress tag not present in xml payload");
      retVal = PLUGIN_FAILURE;
    }
    mxmlDelete(tree);
  }
  else
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "ResetNameRulesData XML String %s couldn't be loaded", xmlBuf);
    retVal = PLUGIN_FAILURE;
  }

handler_exit1:
  return retVal;
}

#define WEMO_FW   1
#define BULB_FW   2

static bool is_wemo_macaddress(char *pMacAddress)
{
  bool result = false;

  if( 0 == strcmp(pMacAddress, g_szWiFiMacAddress) )
  {
    result = true;
  }

  return result;
}

static bool is_subdevice_macaddress(char *pMacAddress)
{
  bool result = false;

  SD_Device device;
  SD_DeviceID deviceID;

  MakeDeviceID(pMacAddress, &deviceID);

  if( SD_GetDevice(&deviceID, &device) )
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Got device(%s) success ...", deviceID.Data);
    result = true;
  }
  else
  {
    DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Got device(%s) failed ...", deviceID.Data);
  }

  return result;
}

char* GetCurrSubDeviceFWVersion(char *pFirmwareLink)
{
  static char szFwVersion[80];

  char *pBegPos = strrchr(pFirmwareLink, '/');

  if( pBegPos )
  {
    char *pEndPos = strrchr(pFirmwareLink, '.');
    if( pEndPos )
    {
      strncpy(szFwVersion, pBegPos+1, pEndPos-pBegPos-1);
    }
  }
  return szFwVersion;
}

int LinkUpgradeFwVersion(void *hndl, char *xmlBuf, unsigned int buflen, void *data_sock,
                        void* remoteInfo,char* transaction_id )
{
  int retVal = PLUGIN_SUCCESS, ret = PLUGIN_SUCCESS;
  pluginUpgradeFwStatus *pUpdFwStatus = NULL;
  mxml_node_t *tree = NULL;
  mxml_node_t *find_node = NULL, *node = NULL;
  char statusResp[MAX_FILE_LINE];
  int dwldStartTime = 0, fwUnSignMode = 0;
  char FirmwareURL[MAX_FW_URL_LEN];
  bool bDeviceUpgrade = false;
  int i = 0, fwType = 0;
  int nUpgradBulbCnt = 0;
  char szResponse[256];
  char szBulbFirmwareURL[MAX_FW_URL_LEN];
  char szBulbDeviceIDList[MAX_DEVICES * MAX_ID_LEN];
  char szDeviceIDs[MAX_DEVICES][MAX_ID_LEN];

  if (!xmlBuf)
  {
    retVal = PLUGIN_FAILURE;
    goto handler_exit1;
  }

  CHECK_PREV_TSX_ID(E_UPGRADEFWVERSION,transaction_id,retVal);
  pUpdFwStatus = (pluginUpgradeFwStatus*)calloc(1, sizeof(pluginUpgradeFwStatus));

  if (!pUpdFwStatus)
  {
    retVal = PLUGIN_FAILURE;
    goto handler_exit1;
  }

  tree = mxmlLoadString(NULL, xmlBuf, MXML_OPAQUE_CALLBACK);

  if (tree)
  {
    APP_LOG("REMOTEACCESS", LOG_DEBUG, "XML String loaded is %s", xmlBuf);

    memset(&szDeviceIDs, 0x00, sizeof(szDeviceIDs));
    memset(szResponse, 0x00, sizeof(szResponse));
    memset(szBulbFirmwareURL, 0x00, sizeof(szBulbFirmwareURL));
    memset(szBulbDeviceIDList, 0x00, sizeof(szBulbDeviceIDList));

    //find_node = mxmlFindElement(tree, tree, "macAddress", NULL, NULL, MXML_DESCEND);
    for( node = mxmlFindElement(tree, tree, "plugin", NULL, NULL, MXML_DESCEND);
         node != NULL;
         node = mxmlFindElement(node, tree, "plugin", NULL, NULL, MXML_DESCEND) )
    {
      find_node = mxmlFindElement(node, tree, "macAddress", NULL, NULL, MXML_DESCEND);

      char *pMacAddress = getElemenetValue(node, tree, &find_node, "macAddress");

      if( pMacAddress )
      {
        APP_LOG("REMOTEACCESS", LOG_DEBUG, "Mac Address value is %s", pMacAddress);

        SAFE_STRCPY(pUpdFwStatus->macAddress, pMacAddress);

        //Get mac address of the device
        APP_LOG("REMOTEACCESS", LOG_DEBUG, "Mac Address of plug-in is %s", g_szWiFiMacAddress);

        if( is_wemo_macaddress(pUpdFwStatus->macAddress) )
        {
          bDeviceUpgrade = true;
          fwType = WEMO_FW;
        }
        else if( is_subdevice_macaddress(pUpdFwStatus->macAddress) )
        {
          bDeviceUpgrade = true;
          fwType = BULB_FW;
        }

        if( bDeviceUpgrade )
        {
          //Get FW upgrade url signature
          char *pSignature = getElemenetValue(node, tree, &find_node, "signature");
          if( pSignature )
          {
            APP_LOG("REMOTEACCESS", LOG_DEBUG, "Signature of plug-in upgrade firmware req in XML is %s",
                    pSignature);
            memset(g_szFirmwareURLSignature, 0x0, sizeof(g_szFirmwareURLSignature));
            SAFE_STRCPY(g_szFirmwareURLSignature, pSignature);
            APP_LOG("REMOTEACCESS", LOG_DEBUG, "Signature of plug-in upgrade firmware req is %s",
                    g_szFirmwareURLSignature);
          }

          //Get FW upgrade download Start Time
          char *pdwlStartTime = getElemenetValue(node, tree, &find_node, "downloadStartTime");
          if( pdwlStartTime )
          {
            APP_LOG("REMOTEACCESS", LOG_DEBUG, "download Start Time of plug-in upgrade firmware req in XML is %s",
                    pdwlStartTime);
            dwldStartTime = atoi(pdwlStartTime);
            dwldStartTime = dwldStartTime * 60; //Seconds
            APP_LOG("REMOTEACCESS", LOG_DEBUG, "download Start Time of plug-in upgrade firmware req is %d",
                    dwldStartTime);
          }

          //Get FW upgrade url
          char *pfwDwldUrl = getElemenetValue(node, tree, &find_node, "firmwareDownloadUrl");
          if( pfwDwldUrl )
          {
            APP_LOG("REMOTEACCESS", LOG_DEBUG, "URL of plug-in upgrade firmware req in XML is %s",
                    pfwDwldUrl);
            memset(FirmwareURL, 0x0, sizeof(FirmwareURL));
            SAFE_STRCPY(FirmwareURL, pfwDwldUrl);
            APP_LOG("REMOTEACCESS", LOG_DEBUG, "URL of plug-in upgrade firmware req is %s", FirmwareURL);
          }
          else
          {
            APP_LOG("REMOTEACCESS", LOG_ERR, "firmwareDownloadUrl Complete Information  not present in xml payload");
            retVal = PLUGIN_FAILURE;
            mxmlDelete(tree);
            goto handler_exit1;
          }

          if( fwType == WEMO_FW )
          {
            fwUnSignMode = PLUGIN_SUCCESS;
            /* Start upgrade firmware as requested, Call firmware update cloudIF routine */
            ret = CloudUpgradeFwVersion(dwldStartTime, FirmwareURL, fwUnSignMode);
            if( retVal < PLUGIN_SUCCESS )
            {
              APP_LOG("REMOTEACCESS", LOG_ERR, "FirmwareUpdateStart thread create failed...");
              retVal = PLUGIN_FAILURE;
              mxmlDelete(tree);
              goto handler_exit1;
            }

            //Get plug-in details as well as firmware upgrade status and version
            pUpdFwStatus->pluginId = strtol(g_szPluginCloudId, NULL, 0);
            SAFE_STRCPY(pUpdFwStatus->macAddress, g_szWiFiMacAddress);
            pUpdFwStatus->fwUpgradeStatus = getCurrFWUpdateState();
            SAFE_STRCPY(pUpdFwStatus->fwVersion, g_szFirmwareVersion);

            //Create xml response
            memset(statusResp, 0x0, MAX_FILE_LINE);
            snprintf(statusResp, sizeof(statusResp), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugin><recipientId>%lu</recipientId><macAddress>%s</macAddress><fwUpgradeStatus>%d</fwUpgradeStatus><firmwareVersion>%s</firmwareVersion></plugin>\n", pUpdFwStatus->pluginId, pUpdFwStatus->macAddress, pUpdFwStatus->fwUpgradeStatus, pUpdFwStatus->fwVersion);

            APP_LOG("REMOTEACCESS", LOG_DEBUG, "plug-in Firmware upgrade status response created is...");
            DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, statusResp, strlen(statusResp));

            retVal = SendNatPkt(hndl, statusResp, remoteInfo, transaction_id, data_sock, E_UPGRADEFWVERSION);
          }
          else if( fwType == BULB_FW )
          {
            nUpgradBulbCnt++;

            strncat(szBulbDeviceIDList, pUpdFwStatus->macAddress, strlen(pUpdFwStatus->macAddress));
            strncat(szBulbDeviceIDList, ",", 1);
            SAFE_STRCPY(szBulbFirmwareURL, FirmwareURL);
          }
        }
        else
        {
          APP_LOG("REMOTEACCESS", LOG_ERR, "Mac received %s doesn't match with plugin or bulb Mac",
                  pUpdFwStatus->macAddress);
          retVal = PLUGIN_FAILURE;
        }
      }
      else
      {
        APP_LOG("REMOTEACCESS", LOG_ERR, "macAddress tag not present in xml payload");
        retVal = PLUGIN_FAILURE;
      }
    }

    if( nUpgradBulbCnt && (fwType == BULB_FW) )
    {
      szBulbDeviceIDList[strlen(szBulbDeviceIDList)-1] = 0x00;

      APP_LOG("REMOTEACCESS", LOG_DEBUG, "BulbList = %s", szBulbDeviceIDList);

      if( 0 != StartUpgradeSubDeviceFirmware(szBulbDeviceIDList, szBulbFirmwareURL) )
      {
        APP_LOG("REMOTEACCESS", LOG_ERR, "Bulb FirmwareUpdateStart thread create failed...");
        retVal = PLUGIN_FAILURE;
        mxmlDelete(tree);
        goto handler_exit1;
      }

      int nDeviceCnt = GetIDs(szBulbDeviceIDList, szDeviceIDs);

      APP_LOG("REMOTEACCESS", LOG_DEBUG, "nUpgradBulbCnt = %d, nDeviceCnt = %d", nUpgradBulbCnt, nDeviceCnt);

      //Create xml response
      memset(statusResp, 0x0, MAX_FILE_LINE);
      SAFE_STRCPY(statusResp, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><plugins>");

      for( i = 0; i < nUpgradBulbCnt; i++ )
      {
        //Get plug-in details as well as firmware upgrade status and version
        pUpdFwStatus->pluginId = strtol(g_szPluginCloudId, NULL, 0);
        //pUpdFwStatus->fwUpgradeStatus = GetCurrSubDeviceFWUpdateState();
        pUpdFwStatus->fwUpgradeStatus = GetSubDeviceFWUpdateState(szDeviceIDs[i]);
        SAFE_STRCPY(pUpdFwStatus->fwVersion, GetCurrSubDeviceFWVersion(szBulbFirmwareURL));
        SAFE_STRCPY(pUpdFwStatus->macAddress, szDeviceIDs[i]);

        snprintf(szResponse, sizeof(szResponse), "<plugin><recipientId>%lu</recipientId><macAddress>%s</macAddress><fwUpgradeStatus>%d</fwUpgradeStatus><firmwareVersion>%s</firmwareVersion></plugin>\n", pUpdFwStatus->pluginId, pUpdFwStatus->macAddress, pUpdFwStatus->fwUpgradeStatus, pUpdFwStatus->fwVersion);

        strncat(statusResp, szResponse, strlen(szResponse));

      }

      strncat(statusResp, "</plugins>", strlen("</plugins>"));

      APP_LOG("REMOTEACCESS", LOG_DEBUG, "plug-in Firmware upgrade status response created is...");
      DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, statusResp, strlen(statusResp));

      retVal = SendNatPkt(hndl, statusResp, remoteInfo, transaction_id, data_sock, E_UPGRADEFWVERSION);
    }

    mxmlDelete(tree);
  }
  else
  {
      APP_LOG("REMOTEACCESS", LOG_ERR, "XML String %s couldn't be loaded", xmlBuf);
      retVal = PLUGIN_FAILURE;
  }

handler_exit1:
  if( pUpdFwStatus )
  {
    free(pUpdFwStatus);
    pUpdFwStatus = NULL;
  }
  return retVal;
}

int parseUpdatePresetDataResp(pRespBody)
{
	return PLUGIN_SUCCESS;
}

void *updatePresetData(void *arg)
{
	char *pRespBody = NULL;
	int nStatusCode = 0;
	char szURL[SIZE_128B] = {0};

	PPRESET_UPDATE_DATE pPresetUpdateData = (PPRESET_UPDATE_DATE)arg;

	preset_lock();

	if(pPresetUpdateData)
	{
		snprintf(szURL, sizeof(szURL), "https://%s:8443/apis/http/device/presets/%s", BL_DOMAIN_NM, pPresetUpdateData->urlString);

		DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "updatePresetData: URL = %s", szURL);
		DEBUG_DUMP(ULOG_REMOTE, UL_DEBUG, pPresetUpdateData->presetXMLData, strlen(pPresetUpdateData->presetXMLData));

		nStatusCode = SendHttpRequestWithAuth(pPresetUpdateData->method, szURL, pPresetUpdateData->presetXMLData, &pRespBody);
		if(nStatusCode == 200 && pRespBody)
		{
			if(PLUGIN_FAILURE == parseUpdatePresetDataResp(pRespBody))
			{
				nStatusCode = 0;
				DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "Failure : parseSubDevicesResp");
			}
			DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "RegisterSubDevices RespBody : %s", pRespBody);
		}
		else if(nStatusCode && pRespBody)
		{
			DEBUG_LOG(ULOG_REMOTE, UL_DEBUG, "updatePresetData: Error string = %s", GetErrorCodeStr(pRespBody));
		}

		if(pPresetUpdateData->presetXMLData)free(pPresetUpdateData->presetXMLData);pPresetUpdateData->presetXMLData=NULL;
		if(pPresetUpdateData)free(pPresetUpdateData);pPresetUpdateData=NULL;
		if(pRespBody)free(pRespBody);
	}

	preset_unlock();

	return NULL;
}

int CreateUpdatePresetDataThread(char *pPresetData, int method, char *urlString)
{
	int retVal = 0;
	pthread_t preset_update_tid;
	PPRESET_UPDATE_DATE pPresetUpdateData = NULL;

	/*allocate memory for PresetUpdateData structure*/
	pPresetUpdateData = calloc(1, sizeof(PRESET_UPDATE_DATE));
	if(NULL == pPresetUpdateData)
	{
		DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "calloc failed");
		return FAILURE;
	}

	/*allocate memory for PresetUpdateData xml data*/
	pPresetUpdateData->presetXMLData = calloc(1, strlen(pPresetData)+1);
	if(NULL == pPresetUpdateData->presetXMLData)
	{
		free(pPresetUpdateData);
		DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "calloc failed");
		return FAILURE;
	}

	/*fill method*/
	pPresetUpdateData->method = method;
	/*copy xml data*/
	snprintf(pPresetUpdateData->presetXMLData, strlen(pPresetData), "%s", pPresetData);
	/*copy url string*/
	snprintf(pPresetUpdateData->urlString, SIZE_64B, "%s", urlString);

	/*create updatePresetData thread*/
	retVal = pthread_create(&preset_update_tid, NULL, updatePresetData, pPresetUpdateData);
	if(0 == retVal)
	{
		pthread_detach(preset_update_tid);
		DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "Successfuly Created Update Preset Data Thread");
	}
	else
		DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "Failed to Create Update Preset Data Thread retVal=(%d)...", retVal);

	return retVal;
}

#endif /* PRODUCT_WeMo_LEDLight */
