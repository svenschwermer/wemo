#ifdef PRODUCT_WeMo_LEDLight

#include <pthread.h>
#include <curl/curl.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "global.h"
#include "defines.h"
#include "subdevice.h"
#include "sd_configuration.h"
#include "logger.h"
#include "mxml.h"
#include "utils.h"
#include "httpsWrapper.h"
#include "wifiHndlr.h"
#include "controlledevice.h"
#include "ledLightUPnPHandler.h"
#include "remoteAccess.h"
#include "ledLightRemoteAccess.h"
#include "event_queue.h"
#include "remote_event.h"
#include "utlist.h"
#include "utlist.h"
#include "rule.h"

#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif

#define  _SD_MODULE_
#include "sd_tracer.h"

#define FW_UPDATE_STATUS    0
#define FW_UPDATE_PROGRESS  1

typedef struct {
  char szDeviceID[MAX_ID_LEN];
  int nSubDeviceFWUpdateState;
} OTAFirmwareStatus;

static int DataStoresExtractXMLTags(mxml_node_t *begin, char *pKey,
                                    char *pKeyValue, int nKeyValBufLen);
static int SaveDataStores(char *pXMLBuff);
static int parseGetDataStoresXMLRequest (char *pXMLRequest, unsigned int *nDataStores, char ***pDataStoreName);
static int retrieveFileSize (const char *pFileName, unsigned int *pFileSize);
static int prepareGetDataStoresResponse (char **pXMLResponse, unsigned int nDataStores,
                                         char **pDataStoreName, char ***pDataStoreValue);
static int handleALLDataStoresRequest (char **pXMLResponse, char ***pALLDataStoresName,
                                       char ***pALLDataStoresValue, unsigned int *pnALLDataStores);

static pthread_mutex_t upnp_mutex_lock;
static pthread_mutex_t upnp_event_mutex_lock;

static pthread_t fw_update_tid;
static pthread_t close_service_tid;

// static pthread_t fw_download_tid;
// static char szDownloadURL[512];
// static bool bfwDownloading = false;

static int sysevent_fwupdate = 0;
static token_t tid_fwupdate;
static bool bfwUpgrading = false;
static sqlite3 *s_PresetDB = NULL;

static OTAFirmwareStatus sFWUpdateStatusTable[MAX_DEVICES];

static int sSubDeviceFWUpdateState = OTA_FW_UPGRADE_SUCCESS;

sysevent_t fwupdate_events[] = {
  {"zb_ota_status", TUPLE_FLAG_EVENT, {0,0}},
  {"zb_ota_percent", TUPLE_FLAG_EVENT, {0,0}},
};

typedef struct {
  char szDeviceList[512];
  char szFirmwareLink[256];
  int nUpgradePolicy;
} OTAFirmwareInfo;


// bool bWithStatus = false;

extern char g_szHomeId[SIZE_20B];
extern PluginDevice SocketDevice;
extern UpnpDevice_Handle device_handle;

extern long DiffMillis(struct timeval *time1, struct timeval *time2);

//=========================================================================
// Internal Functions
//=========================================================================

static int get_response_len(const char *pszRespXML)
{
  int nLen = 0;

  while( pszRespXML[nLen] )
  {
    nLen++;
  }

  return nLen;
}

bool is_registered_ids_empty()
{
  int i = 0, nCount = 0;
  SD_DeviceList device_list;

  nCount = SD_GetDeviceList(&device_list);
  for( i = 0; i < nCount; i++ )
  {
    if( 0 == strlen(device_list[i]->RegisteredID.Data) )
    {
      return true;
    }
  }
  return false;
}

void create_fwupdate_sysevent(void)
{
  int nEvent = 0;

  sysevent_fwupdate = open_sysevent("sysevent_fwupdate", &tid_fwupdate);
  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "sysevent_fwupdate = %d, token = %d", sysevent_fwupdate, tid_fwupdate);
  nEvent = sizeof(fwupdate_events) / sizeof(fwupdate_events[0]);
  register_sysevent(sysevent_fwupdate, tid_fwupdate, fwupdate_events, nEvent);
}

void destroy_fwupdate_sysevent(void)
{
  int nEvent = sizeof(fwupdate_events) / sizeof(fwupdate_events[0]);

  if( sysevent_fwupdate )
  {
    unregister_sysevent(sysevent_fwupdate, tid_fwupdate, fwupdate_events, nEvent);
    close_sysevent(sysevent_fwupdate, tid_fwupdate);
  }
}

void init_upnp_event_lock()
{
  pthread_mutex_init(&upnp_event_mutex_lock, NULL);
}

void destroy_upnp_event_lock()
{
  pthread_mutex_destroy(&upnp_event_mutex_lock);
}

void upnp_event_lock()
{
  pthread_mutex_lock(&upnp_event_mutex_lock);
}

void upnp_event_unlock()
{
  pthread_mutex_unlock(&upnp_event_mutex_lock);
}

void init_upnp_lock()
{
  pthread_mutex_init(&upnp_mutex_lock, NULL);
}

void destroy_upnp_lock()
{
  pthread_mutex_destroy(&upnp_mutex_lock);
}

void upnp_lock()
{
  pthread_mutex_lock(&upnp_mutex_lock);
}

void upnp_unlock()
{
  pthread_mutex_unlock(&upnp_mutex_lock);
}

#if 0
<StateEvent>
    <DeviceID>00158D0000317973</DeviceID>
    <EventType>FWUpdateStatus</EventType>
    <Value>1</Value>
</StateEvent>
#endif

static char *CreateStatEventXML(char *pDeviceId, int nEventType, int nStatus)
{
  mxml_node_t *pRespXml = NULL;
  mxml_node_t *pNodeStateEvent = NULL;
  mxml_node_t *pNodeDeviceID = NULL;
  mxml_node_t *pNodeEventType = NULL;
  mxml_node_t *pNodeValue = NULL;

  char *pszResp = NULL;
  char szDeviceID[MAX_ID_LEN] = {0,};
  char szValue[MAX_VAL_LEN] = {0,};

  SD_Device device;
  SD_DeviceID deviceID;

  pRespXml = mxmlNewXML("1.0");
  pNodeStateEvent = mxmlNewElement(pRespXml, "StateEvent");

  if( pDeviceId )
  {
    SAFE_STRCPY(szDeviceID, pDeviceId);
  }

  snprintf(szValue, sizeof(szValue), "%d", nStatus);

  pNodeDeviceID = mxmlNewElement(pNodeStateEvent, "DeviceID");
  mxmlNewText(pNodeDeviceID, 0, szDeviceID);
  pNodeEventType = mxmlNewElement(pNodeStateEvent, "EventType");
  if( nEventType == FW_UPDATE_STATUS )
  {
    mxmlNewText(pNodeEventType, 0, "FWUpdateStatus");
  }
  else if( nEventType == FW_UPDATE_PROGRESS )
  {
    mxmlNewText(pNodeEventType, 0, "FWUpdateProgress");
  }
  pNodeValue = mxmlNewElement(pNodeStateEvent, "Value");
  mxmlNewText(pNodeValue, 0, szValue);

  if( nEventType == FW_UPDATE_STATUS )
  {
    MakeDeviceID(szDeviceID, &deviceID);
    if( SD_GetDevice(&deviceID, &device) )
    {
      pNodeValue = mxmlNewElement(pNodeStateEvent, "FwVersion");
      mxmlNewText(pNodeValue, 0, device->FirmwareVersion);
    }
  }

  if( pRespXml )
  {
    pszResp = mxmlSaveAllocString(pRespXml, MXML_NO_CALLBACK);
  }

  if( pRespXml )
  {
    mxmlDelete(pRespXml);
  }

  return pszResp;

}

void upnp_subdevice_fwupdate(char *pDeviceId, int nEventType, int nStatus)
{
  char *pStatusEvent[1] = {"SubDeviceFWUpdate"};
  char *pStatusValue[1] = {NULL};
  char *pszRespXML = NULL;

  int nRet = 0;

  upnp_event_lock();

  pStatusValue[0] = NULL;

  if( nEventType == FW_UPDATE_STATUS )
  {
    pszRespXML = CreateStatEventXML(pDeviceId, nEventType, nStatus);
  }
  else if( nEventType == FW_UPDATE_PROGRESS )
  {
    pszRespXML = CreateStatEventXML(pDeviceId, nEventType, nStatus);
  }

  if( pszRespXML )
  {
    pStatusValue[0] = EscapeXML(pszRespXML);
    if( pStatusValue[0] )
    {
      nRet = UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_BRIDGE_SERVICE].UDN,
                        SocketDevice.service_table[PLUGIN_E_BRIDGE_SERVICE].ServiceId,
                        (const char **)pStatusEvent, (const char **)pStatusValue, 1);

      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "UpnpNotify: return = %d", nRet);
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "UpnpNotify: EventType = %s, EventValue = %s",
                pStatusEvent[0], pStatusValue[0]);
    }
  }

  if( pStatusValue[0] )
  {
    free(pStatusValue[0]);
  }

  if( pszRespXML )
  {
    free(pszRespXML);
  }

  upnp_event_unlock();
}

bool is_sensor_device(char *device_id)
{
  SD_Device device;
  SD_DeviceID deviceID;

  MakeDeviceID(device_id, &deviceID);

  if( SD_GetDevice(&deviceID, &device) && (0 == strcmp(device->DeviceType, ZB_SENSOR)) )
  {
    return true;
  }

  return false;
}

bool IsPIRSensor(char *device_id)
{
  SD_Device device;
  SD_DeviceID deviceID;

  MakeDeviceID(device_id, &deviceID);

  if(SD_GetDevice(&deviceID, &device))
  {
    if(0 == strncmp(device->ModelCode, SENSOR_MODELCODE_PIR, strlen(SENSOR_MODELCODE_PIR)))
    {
      return true;
    }
  }
  return false;
}

bool IsAlarmSensor(char *device_id)
{
  SD_Device device;
  SD_DeviceID deviceID;

  MakeDeviceID(device_id, &deviceID);

  if(SD_GetDevice(&deviceID, &device))
  {
    if(0 == strncmp(device->ModelCode, SENSOR_MODELCODE_ALARM, strlen(SENSOR_MODELCODE_ALARM)))
    {
      return true;
    }
  }
  return false;
}

unsigned long GetLastEventTimeStamp(char *device_id)
{
  SD_Device device;
  SD_DeviceID deviceID;

  MakeDeviceID(device_id, &deviceID);
  if(SD_GetDevice(&deviceID, &device))
  {
    return device->LastReportedTime;
  }
  return 0;
}

void upnp_event_notification(bool bAvailable, char *device_id, char *capability_id, char *value)
{
  // char *pEventType[1] = {"StatusChange"};
  char *pBulbEventType[1] = {"StatusChange"};
  char *pBulbEventValue[1] = {NULL};

  char *pSensorEventType[1] = {"SensorChange"};
  char *pSensorEventValue[1] = {NULL};

  char *pszRespXML = NULL;
  int nRet = 0;

  upnp_event_lock();

  pBulbEventValue[0] = NULL;
  pSensorEventValue[0] = NULL;

  bool bSensorDevice = is_sensor_device(device_id);

  pszRespXML = CreateStatusChangeXML(bAvailable, bSensorDevice, device_id, capability_id, value);

  if( pszRespXML )
  {
    if( bSensorDevice )
    {
      pSensorEventValue[0] = EscapeXML(pszRespXML);

      if( pSensorEventValue[0] )
      {
        nRet = UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_BRIDGE_SERVICE].UDN,
                SocketDevice.service_table[PLUGIN_E_BRIDGE_SERVICE].ServiceId,
                (const char **)pSensorEventType, (const char **)pSensorEventValue, 1);

        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "UpnpNotify: return = %d", nRet);
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "UpnpNotify: EventType = %s, Capability = %s, EventValue = %s",
                  pSensorEventType[0], capability_id, value);
        DEBUG_DUMP(ULOG_UPNP, UL_DEBUG, pSensorEventValue[0], get_response_len(pSensorEventValue[0]));
      }
    }
    else
    {
      pBulbEventValue[0] = EscapeXML(pszRespXML);

      if( pBulbEventValue[0] )
      {
        nRet = UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_BRIDGE_SERVICE].UDN,
                SocketDevice.service_table[PLUGIN_E_BRIDGE_SERVICE].ServiceId,
                (const char **)pBulbEventType, (const char **)pBulbEventValue, 1);

        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "UpnpNotify: return = %d", nRet);
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "UpnpNotify: EventType = %s, Capability = %s, EventValue = %s",
                  pBulbEventType[0], capability_id, value);
        DEBUG_DUMP(ULOG_UPNP, UL_DEBUG, pBulbEventValue[0], get_response_len(pBulbEventValue[0]));
      }
    }
  }

  if( pBulbEventValue[0] )
  {
    free(pBulbEventValue[0]);
  }

  if( pSensorEventValue[0] )
  {
    free(pSensorEventValue[0]);
  }

  if( pszRespXML )
  {
    free(pszRespXML);
  }

  upnp_event_unlock();
}

void SendBulbOTAEvent(unsigned event_type, char *device_id, int status, unsigned event)
{
  if( (event_type == FW_UPDATE_STATUS) && (event & SE_LOCAL) )
  {
    upnp_subdevice_fwupdate(device_id, FW_UPDATE_STATUS, status);
  }

  if( (event_type == FW_UPDATE_PROGRESS) && (event & SE_LOCAL) )
  {
    upnp_subdevice_fwupdate(device_id, FW_UPDATE_PROGRESS, status);
  }

  if( (event_type == FW_UPDATE_STATUS) && (event & SE_REMOTE) )
  {
    struct DeviceInfo DevInfo;

    SD_DeviceID deviceID;
    char capability_id[CAPABILITY_MAX_ID_LENGTH] = {0,};
    char capability_value[CAPABILITY_MAX_VALUE_LENGTH] = {0,};

    MakeDeviceID(device_id, &deviceID);

    memset(&DevInfo, 0x00, sizeof(struct DeviceInfo));

    GetCacheDeviceOnOff(deviceID, capability_id, capability_value);

    DevInfo.bGroupDevice = false;

    SAFE_STRCPY(DevInfo.szDeviceID, device_id);

    DevInfo.CapInfo.bAvailable = true;
    SAFE_STRCPY(DevInfo.CapInfo.szCapabilityID, capability_id);
    SAFE_STRCPY(DevInfo.CapInfo.szCapabilityValue, capability_value);

    trigger_remote_event(WORK_EVENT, UPDATE_SUBDEVICE, "device", "update", &DevInfo, sizeof(struct DeviceInfo));
  }
}

void SendDeviceEvent(bool result, bool group, char *device_id, char *capability_id, char *value, unsigned event)
{
  struct DeviceInfo DevInfo;

  if (event & SE_LOCAL)
  {
    upnp_event_notification(result, device_id, capability_id, value);
  }

  if (event & SE_REMOTE)
  {
    memset(&DevInfo, 0x00, sizeof(struct DeviceInfo));

    DevInfo.bGroupDevice = group;

    SAFE_STRCPY(DevInfo.szDeviceID, device_id);

    DevInfo.CapInfo.bAvailable = result;
    SAFE_STRCPY(DevInfo.CapInfo.szCapabilityID, capability_id);
    SAFE_STRCPY(DevInfo.CapInfo.szCapabilityValue, value);

    trigger_remote_event(WORK_EVENT, UPDATE_SUBDEVICE, "device", "update", &DevInfo, sizeof(struct DeviceInfo));
  }
}

int GetIDs(char *pIDs, char szIDs[][MAX_ID_LEN])
{
  char *p = NULL, *q = NULL;
  int row = 0;

  if( NULL == pIDs )
    return row;

  p = pIDs;
  q = strchr(p, ',');

  while( NULL != q )
  {
    if( (q - p) <= sizeof(szIDs[row]) )
    {
      strncpy(szIDs[row], p, q-p);
    }

    row++;
    p = q + 1;
    q = strchr(p, ',');

    if( row >= MAX_IDS )
    {
      return MAX_IDS;
    }
  }

  if( strlen(p) != 0 )
  {
    SAFE_STRCPY(szIDs[row], p);
    row++;
  }

  return row;
}

int GetValues(char *pValues, char szValues[][MAX_VAL_LEN])
{
  char *p = NULL, *q = NULL;
  int row = 0;

  if( NULL == pValues )
    return row;

  p = pValues;
  q = strchr(p, ',');

  while( NULL != q )
  {
    if( (q - p) <= sizeof(szValues[row]) )
    {
      strncpy(szValues[row], p, q-p);
    }

    row++;
    p = q + 1;
    q = strchr(p, ',');

    if( row >= MAX_VALUES )
    {
      return MAX_VALUES;
    }
  }

  if( strlen(p) != 0 )
  {
    SAFE_STRCPY(szValues[row], p);
    row++;
  }

  return row;
}

int GetNames(char *pNames, char szNames[][MAX_NAME_LEN])
{
  char *p = NULL, *q = NULL;
  int row = 0;

  if( NULL == pNames )
    return row;

  p = pNames;
  q = strchr(p, ';');

  while( NULL != q )
  {
    if( (q - p) <= sizeof(szNames[row]) )
    {
      strncpy(szNames[row], p, q-p);
    }

    row++;
    p = q + 1;
    q = strchr(p, ';');

    if( row >= MAX_VALUES )
    {
      return MAX_VALUES;
    }
  }

  if( strlen(p) != 0 )
  {
    SAFE_STRCPY(szNames[row], p);
    row++;
  }

  return row;
}

char* CreateStatusChangeXML(bool bAvailabeDevice, bool bSensorDevice, char *pDeviceID, char *pCapabilityID, char *pValue)
{
  mxml_node_t *pRespXml = NULL;
  mxml_node_t *pNodeStateEvent = NULL;
  mxml_node_t *pNodeDeviceID = NULL;
  mxml_node_t *pNodeCapabilityID = NULL;
  mxml_node_t *pNodeValue = NULL;
  mxml_node_t *pNodeTimeStamp = NULL;

  char *pszResp = NULL;
  char szDeviceID[512] = {0,};
  char szCapabilityID[MAX_ID_LEN] = {0,};
  char szValue[MAX_VAL_LEN] = {0,};

  int nTimeStamp = 0;
  char szTimeStamp[SIZE_64B] = {0,};

  pRespXml = mxmlNewXML("1.0");
  pNodeStateEvent = mxmlNewElement(pRespXml, "StateEvent");

  if( pDeviceID )
  {
    SAFE_STRCPY(szDeviceID, pDeviceID);
  }
  if( pCapabilityID )
  {
    SAFE_STRCPY(szCapabilityID, pCapabilityID);
  }
  if( pValue )
  {
    SAFE_STRCPY(szValue, pValue);
  }

  if( bAvailabeDevice )
  {
    pNodeDeviceID = mxmlNewElement(pNodeStateEvent, "DeviceID");
    mxmlElementSetAttr(pNodeDeviceID, "available", "YES");
  }
  else
  {
    pNodeDeviceID = mxmlNewElement(pNodeStateEvent, "DeviceID");
    mxmlElementSetAttr(pNodeDeviceID, "available", "NO");
  }
  mxmlNewText(pNodeDeviceID, 0, szDeviceID);
  pNodeCapabilityID = mxmlNewElement(pNodeStateEvent, "CapabilityId");
  mxmlNewText(pNodeCapabilityID, 0, szCapabilityID);
  pNodeValue = mxmlNewElement(pNodeStateEvent, "Value");
  mxmlNewText(pNodeValue, 0, szValue);

  if( bSensorDevice )
  {
    pNodeTimeStamp = mxmlNewElement(pNodeStateEvent, "statusTS");
    if( IsPIRSensor(pDeviceID) || IsAlarmSensor(pDeviceID))
    {
        nTimeStamp = GetLastEventTimeStamp(pDeviceID);
    }
    else
    {
        nTimeStamp = (int)GetUTCTime();
    }
    snprintf(szTimeStamp, sizeof(szTimeStamp), "%d", nTimeStamp);
    mxmlNewText(pNodeTimeStamp, 0, szTimeStamp);
  }

  if( pRespXml )
  {
    pszResp = mxmlSaveAllocString(pRespXml, MXML_NO_CALLBACK);
  }

  if( pRespXml )
  {
    mxmlDelete(pRespXml);
  }

  return pszResp;
}

bool GetCapabilityID(char *pszProfileName, SD_CapabilityID *pCapabilityID)
{
  if( NULL == pszProfileName )
  {
    return false;
  }

  SD_CapabilityList list;
  int i = 0, nCount = 0;

  nCount = SD_GetCapabilityList(&list);
  for( i = 0; i < nCount; i++ )
  {
    if( 0 == strcmp(list[i]->ProfileName, pszProfileName) )
    {
      memcpy(pCapabilityID, &list[i]->CapabilityID, sizeof(SD_CapabilityID));
      return true;
    }
  }

  return false;
}

void GetDeviceCapabilityIDs(const SD_Device Device, char *pszCapIDs)
{
  int i = 0, nLen = 0;

  if( NULL == pszCapIDs )
  {
    return;
  }

  pszCapIDs[0] = 0x00;

  for( i = 0; i < Device->CapabilityCount; i++ )
  {
    if( 0 != strcmp(Device->Capability[i]->ProfileName, "Identify") )
    {
      strncat(pszCapIDs, Device->Capability[i]->CapabilityID.Data, CAPABILITY_MAX_ID_LENGTH);
      strncat(pszCapIDs, ",", 1);
    }
  }

  nLen = strlen(pszCapIDs);
  if( nLen )
  {
    pszCapIDs[nLen-1] = 0x00;
  }

  // DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "Device CapabilityIDs = %s", pszCapIDs);
}

void GetCacheDeviceOnOff(SD_DeviceID deviceID, char *capability_id, char *capability_value)
{
  int i = 0;

  if( (NULL == capability_id) || (NULL == capability_value) )
  {
    return;
  }

  SD_Device device;

  //[WEMO-42914] - Check NULL to avoid SIGSEGV
  if( SD_GetDevice(&deviceID, &device) )
  {
    for( i = 0; i < device->CapabilityCount; i++ )
    {
      if( 0 == strcmp(device->Capability[i]->ProfileName, "OnOff") )
      {
        memcpy(capability_id, device->Capability[i]->CapabilityID.Data, CAPABILITY_MAX_ID_LENGTH);
        memcpy(capability_value, device->CapabilityValue[i], CAPABILITY_MAX_VALUE_LENGTH);
        break;
      }
    }
  }
}

void GetCacheDeviceHomeSense(SD_DeviceID deviceID, char *capability_id, char *capability_value)
{
  int i = 0;

  if( (NULL == capability_id) || (NULL == capability_value) )
  {
    return;
  }

  SD_Device device;

  //[WEMO-42914] - Check NULL to avoid SIGSEGV
  if( SD_GetDevice(&deviceID, &device) )
  {
    for( i = 0; i < device->CapabilityCount; i++ )
    {
      if( 0 == strcmp(device->Capability[i]->ProfileName, "IASZone") )
      {
        memcpy(capability_id, device->Capability[i]->CapabilityID.Data, CAPABILITY_MAX_ID_LENGTH);
        memcpy(capability_value, device->CapabilityValue[i], CAPABILITY_MAX_VALUE_LENGTH);
        break;
      }
      else if( 0 == strcmp(device->Capability[i]->ProfileName, "SensorConfig") )
      {
        memcpy(capability_id, device->Capability[i]->CapabilityID.Data, CAPABILITY_MAX_ID_LENGTH);
        memcpy(capability_value, device->CapabilityValue[i], CAPABILITY_MAX_VALUE_LENGTH);
        break;
      }
      else if( 0 == strcmp(device->Capability[i]->ProfileName, "SensorTestMode") )
      {
        memcpy(capability_id, device->Capability[i]->CapabilityID.Data, CAPABILITY_MAX_ID_LENGTH);
        memcpy(capability_value, device->CapabilityValue[i], CAPABILITY_MAX_VALUE_LENGTH);
        break;
      }
    }
  }
}

void GetCacheDeviceCapabilityValues(const SD_Device Device, char *pszCapValues)
{
  int i = 0, nLen = 0;
  char szValue[CAPABILITY_MAX_VALUE_LENGTH];

  if( NULL == pszCapValues )
  {
    return;
  }

  pszCapValues[0] = 0x00;

  for( i = 0; i < Device->CapabilityCount; i++ )
  {
      
    if( 0 != strcmp(Device->Capability[i]->ProfileName, "Identify") )
    {
      memset(szValue, 0x00, sizeof(szValue));
      memcpy(szValue, Device->CapabilityValue[i], CAPABILITY_MAX_VALUE_LENGTH);

      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "Cache Device CapabCapality: %s -- bilityValue = %s....dp", Device->Capability[i]->CapabilityID.Data,szValue);

      ConvertCapabilityValue(szValue, ',', ':');
      strncat(pszCapValues, szValue, strlen(szValue));
      strncat(pszCapValues, ",", 1);
    }
  }

  nLen = strlen(pszCapValues);

  if( nLen )
  {
    pszCapValues[nLen-1] = 0x00;
  }

  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetCacheDeviceCapabilityValues: CapabilityValues = %s", pszCapValues);
}

#if 0
bool GetDeviceCapabilityValues(const SD_Device Device, char *pszCapIDs, char *pszCapValues)
{
  int nLen = 0;
  bool bStatus = false;

  if( NULL == pszCapValues )
  {
    return false;
  }

  pszCapIDs[0] = 0x00;
  pszCapValues[0] = 0x00;

  bStatus = SD_GetDeviceCapabilityValues(Device, pszCapIDs, pszCapValues, SD_CACHE);

  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "DeviceID = %s, CapabilityIDs = %s, CapabilityValues = %s",
             Device->EUID.Data, pszCapIDs, pszCapValues);

  return bStatus;
}
#endif

bool GetDeviceCapabilityValues(const SD_Device Device, char *pszCapValues)
{
  int i = 0, nLen = 0;
  char szValue[CAPABILITY_MAX_VALUE_LENGTH];
  bool bStatus = false;

  if( NULL == pszCapValues )
  {
    return false;
  }

  pszCapValues[0] = 0x00;

  for( i = 0; i < Device->CapabilityCount; i++ )
  {
    if( 0 == strcmp(Device->Capability[i]->ProfileName, "Identify") )
    {
      continue;
    }

    if( 0 == strcmp(Device->Capability[i]->ProfileName, "LevelControl.Move") ||
        0 == strcmp(Device->Capability[i]->ProfileName, "LevelControl.Stop") )
    {
      strncat(pszCapValues, ",", 1);
    }
    else
    {
      memset(szValue, 0x00, sizeof(szValue));

      bStatus = SD_GetDeviceCapabilityValue((SD_DeviceID*)&(Device->EUID),
                                        (SD_CapabilityID*)&(Device->Capability[i]->CapabilityID),
                                        szValue, 0, SD_CACHE);

      //This means zigbee bulb is power off or is not avaiable due to communication error
      if( false == bStatus )
      {
        strncat(pszCapValues, ",", 1);
      }
      else
      {
        // DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "Device CapabilityValue = %s", szValue);
        // replace(szValue, ",", ":");
        ConvertCapabilityValue(szValue, ',', ':');
        strncat(pszCapValues, szValue, strlen(szValue));
        strncat(pszCapValues, ",", 1);
      }
    }
  }

  nLen = strlen(pszCapValues);

  if( nLen )
  {
    pszCapValues[nLen-1] = 0x00;
  }

  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "DeviceID = %s, CapabilityValues = %s", Device->EUID.Data, pszCapValues);

  return bStatus;
}

void GetGroupCapabilityIDs(const SD_Group Group, char *pszCapIDs)
{
  int i = 0, nLen = 0;

  if( NULL == pszCapIDs )
  {
    return;
  }

  pszCapIDs[0] = 0x00;

  for( i = 0; i < Group->CapabilityCount; i++ )
  {
    if( 0 != strcmp(Group->Capability[i]->ProfileName, "Identify") )
    {
      strncat(pszCapIDs, Group->Capability[i]->CapabilityID.Data, CAPABILITY_MAX_ID_LENGTH);
      strncat(pszCapIDs, ",", 1);
    }
  }

  nLen = strlen(pszCapIDs);
  if( nLen )
  {
    pszCapIDs[nLen-1] = 0x00;
  }

  // DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "Group CapabilityIDs = %s", pszCapIDs);
}

void GetCacheGroupCapabilityValues(const SD_Group Group, char *pszCapValues)
{
  int i = 0, nLen = 0;
  char szValue[CAPABILITY_MAX_VALUE_LENGTH];

  if( NULL == pszCapValues )
  {
    return;
  }

  pszCapValues[0] = 0x00;

  for( i = 0; i < Group->CapabilityCount; i++ )
  {
    if( 0 != strcmp(Group->Capability[i]->ProfileName, "Identify") )
    {
      memset(szValue, 0x00, sizeof(szValue));
      memcpy(szValue, Group->CapabilityValue[i], CAPABILITY_MAX_VALUE_LENGTH);

      // DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "Cache Group CapabilityValue = %s", szValue);

      ConvertCapabilityValue(szValue, ',', ':');
      strncat(pszCapValues, szValue, strlen(szValue));
      strncat(pszCapValues, ",", 1);
    }
  }

  nLen = strlen(pszCapValues);

  if( nLen )
  {
    pszCapValues[nLen-1] = 0x00;
  }

  // DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "Cache Group CapabilityValues = %s", pszCapValues);
}

void GetGroupCapabilityValues(const SD_Group Group, char *pszCapValues)
{
  int i = 0, nLen = 0;
  char szValue[CAPABILITY_MAX_VALUE_LENGTH];

  if( NULL == pszCapValues )
  {
    return;
  }

  pszCapValues[0] = 0x00;

  for( i = 0; i < Group->CapabilityCount; i++ )
  {
    if( 0 == strcmp(Group->Capability[i]->ProfileName, "Identify") )
    {
      continue;
    }

    if( 0 == strcmp(Group->Capability[i]->ProfileName, "LevelControl.Move") ||
        0 == strcmp(Group->Capability[i]->ProfileName, "LevelControl.Stop") )
    {
      strncat(pszCapValues, ",", 1);
    }
    else
    {
      memset(szValue, 0x00, sizeof(szValue));
      SD_GetGroupCapabilityValueCache((SD_GroupID*)&(Group->ID),
                                (SD_CapabilityID*)&(Group->Capability[i]->CapabilityID),
                                szValue, SD_CACHE);
      // DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "Group CapabilityValue = %s", szValue);
      // replace(szValue, ",", ":");
      ConvertCapabilityValue(szValue, ',', ':');
      strncat(pszCapValues, szValue, strlen(szValue));
      strncat(pszCapValues, ",", 1);
    }
  }

  nLen = strlen(pszCapValues);

  if( nLen )
  {
    pszCapValues[nLen-1] = 0x00;
  }

  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "Group CapabilityValues = %s", pszCapValues);
}

bool IsProfileCapabilityID(char *pProfileName, SD_CapabilityID *pCapabilityID)
{
  int i = 0;
  SD_CapabilityList CapabilityList;

  int nCount = SD_GetCapabilityList(&CapabilityList);

  for( i = 0; i < nCount; i++ )
  {
    if( 0 == strcmp(CapabilityList[i]->CapabilityID.Data, pCapabilityID->Data) &&
        0 == strcmp(CapabilityList[i]->ProfileName, pProfileName) )
    {
      return true;
    }
  }

  return false;
}

void MakeGroupID(char *pszGroupID, SD_GroupID *pGroupID)
{
  if( NULL == pszGroupID || NULL == pGroupID )
    return;

  memset(pGroupID->Data, 0x00, GROUP_MAX_ID_LENGTH);

  pGroupID->Type = SD_GROUP_ID;
  SAFE_STRCPY(pGroupID->Data, pszGroupID);
}

void MakeDeviceID(char *pszDeviceID, SD_DeviceID *pDeviceID)
{
  if( NULL == pszDeviceID || NULL == pDeviceID )
    return;

  memset(pDeviceID->Data, 0x00, DEVICE_MAX_ID_LENGTH);

  pDeviceID->Type = SD_DEVICE_EUID;
  SAFE_STRCPY(pDeviceID->Data, pszDeviceID);
}

void MakeCapabilityID(char *pszCapabilityID, SD_CapabilityID *pCapabilityID)
{
  if( NULL == pszCapabilityID || NULL == pCapabilityID )
    return;

  memset(pCapabilityID->Data, 0x00, CAPABILITY_MAX_ID_LENGTH);

  pCapabilityID->Type = SD_CAPABILITY_ID;
  SAFE_STRCPY(pCapabilityID->Data, pszCapabilityID);
}

void MakeRegisterDeviceID(char *pszCloudDeviceID, SD_DeviceID *pRegisterDeviceID)
{
  if( NULL == pszCloudDeviceID || NULL == pRegisterDeviceID )
    return;

  memset(pRegisterDeviceID->Data, 0x00, DEVICE_MAX_ID_LENGTH);

  pRegisterDeviceID->Type = SD_DEVICE_REGISTERED_ID;
  SAFE_STRCPY(pRegisterDeviceID->Data, pszCloudDeviceID);
}

void MakeRegisterCapabilityID(char *pszCloudCapID, SD_CapabilityID *pRegisterCapID)
{
  if( NULL == pszCloudCapID || NULL == pRegisterCapID )
    return;

  memset(pRegisterCapID->Data, 0x00, CAPABILITY_MAX_ID_LENGTH);

  pRegisterCapID->Type = SD_CAPABILITY_REGISTERED_ID;
  SAFE_STRCPY(pRegisterCapID->Data, pszCloudCapID);
}

mxml_node_t* MakePairedDeviceInfos(mxml_node_t* pCurrentNode, int nDeviceCnt, SD_DeviceList Devices)
{
  int i = 0;

  mxml_node_t *pNode = NULL;
  mxml_node_t *pNodeDeviceList = NULL;
  mxml_node_t *pNodeDeviceInfo = NULL;
  mxml_node_t *pNodeDeviceInfos = NULL;

  char szCapIDs[MAX_CAPIDS_LEN], szCapValues[MAX_CAPVALS_LEN];

  if( NULL == pCurrentNode )
  {
    return pCurrentNode;
  }

  SD_Group group;

  pNodeDeviceList = mxmlNewElement(pCurrentNode, "DeviceList");

  pNode = mxmlNewElement(pNodeDeviceList, "DeviceListType");
  mxmlNewText(pNode, 0, "Paired");

  pNodeDeviceInfos = mxmlNewElement(pNodeDeviceList, "DeviceInfos");

  for( i = 0; i < nDeviceCnt; i++ )
  {
    //Paired and Device is not included in Group
    if( Devices[i]->Paired && (false == SD_GetGroupOfDevice(&Devices[i]->EUID, &group)) )
    {
      memset(szCapIDs, 0x00, sizeof(szCapIDs));
      memset(szCapValues, 0x00, sizeof(szCapValues));
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "Device[%d]: EUID:%s--FriendlyName:%s--CapCount:%d....dp",i,Devices[i]->EUID.Data, Devices[i]->FriendlyName, Devices[i]->CapabilityCount);  
      GetDeviceCapabilityIDs(Devices[i], szCapIDs);
      GetCacheDeviceCapabilityValues(Devices[i], szCapValues);

      //DeviceInfo has n-th node
      pNodeDeviceInfo = mxmlNewElement(pNodeDeviceInfos, "DeviceInfo");

      pNode = mxmlNewElement(pNodeDeviceInfo, "DeviceIndex");
      mxmlNewInteger(pNode, i);

      pNode = mxmlNewElement(pNodeDeviceInfo, "DeviceID");
      mxmlNewText(pNode, 0, Devices[i]->EUID.Data);

      pNode = mxmlNewElement(pNodeDeviceInfo, "FriendlyName");
      mxmlNewText(pNode, 0, Devices[i]->FriendlyName);

      pNode = mxmlNewElement(pNodeDeviceInfo, "IconVersion");
      mxmlNewText(pNode, 0, "1");

      pNode = mxmlNewElement(pNodeDeviceInfo, "FirmwareVersion");
      mxmlNewText(pNode, 0, Devices[i]->FirmwareVersion);

      pNode = mxmlNewElement(pNodeDeviceInfo, "CapabilityIDs");
      mxmlNewText(pNode, 0, szCapIDs);

      pNode = mxmlNewElement(pNodeDeviceInfo, "CurrentState");
      mxmlNewText(pNode, 0, szCapValues);

      pNode = mxmlNewElement(pNodeDeviceInfo, "Manufacturer");
      mxmlNewText(pNode, 0, Devices[i]->ManufacturerName);

      pNode = mxmlNewElement(pNodeDeviceInfo, "ModelCode");
      mxmlNewText(pNode, 0, Devices[i]->ModelCode);

      pNode = mxmlNewElement(pNodeDeviceInfo, "productName");
      mxmlNewText(pNode, 0,getProductName(Devices[i]->ModelCode));

      pNode = mxmlNewElement(pNodeDeviceInfo, "WeMoCertified");

      if( Devices[i]->Certified )
      {
        mxmlNewText(pNode, 0, "YES");
      }
      else
      {
        mxmlNewText(pNode, 0, "NO");
      }
    }
  }

  return pNodeDeviceList;
}

mxml_node_t* MakeUnpairedDeviceInfos(mxml_node_t* pCurrentNode, int nDeviceCnt, SD_DeviceList Devices)
{
  int i = 0;

  mxml_node_t *pNode = NULL;
  mxml_node_t *pNodeDeviceList = NULL;
  mxml_node_t *pNodeDeviceInfo = NULL;
  mxml_node_t *pNodeDeviceInfos = NULL;

  char szCapIDs[MAX_CAPIDS_LEN], szCapValues[MAX_CAPVALS_LEN];

  if( NULL == pCurrentNode )
  {
    return pCurrentNode;
  }

  pNodeDeviceList = mxmlNewElement(pCurrentNode, "DeviceList");

  pNode = mxmlNewElement(pNodeDeviceList, "DeviceListType");
  mxmlNewText(pNode, 0, "Unpaired");

  pNodeDeviceInfos = mxmlNewElement(pNodeDeviceList, "DeviceInfos");

  for( i = 0; i < nDeviceCnt; i++ )
  {
    if( false == Devices[i]->Paired )
    {
      memset(szCapIDs, 0x00, sizeof(szCapIDs));
      memset(szCapValues, 0x00, sizeof(szCapValues));

      //DeviceInfo has n-th node
      pNodeDeviceInfo = mxmlNewElement(pNodeDeviceInfos, "DeviceInfo");

      pNode = mxmlNewElement(pNodeDeviceInfo, "DeviceIndex");
      mxmlNewInteger(pNode, i);

      pNode = mxmlNewElement(pNodeDeviceInfo, "DeviceID");
      mxmlNewText(pNode, 0, Devices[i]->EUID.Data);

      pNode = mxmlNewElement(pNodeDeviceInfo, "FriendlyName");
      mxmlNewText(pNode, 0, Devices[i]->FriendlyName);

      pNode = mxmlNewElement(pNodeDeviceInfo, "IconVersion");
      mxmlNewText(pNode, 0, "1");

      pNode = mxmlNewElement(pNodeDeviceInfo, "FirmwareVersion");
      mxmlNewText(pNode, 0, Devices[i]->FirmwareVersion);

      GetDeviceCapabilityIDs(Devices[i], szCapIDs);
      GetCacheDeviceCapabilityValues(Devices[i], szCapValues);

      pNode = mxmlNewElement(pNodeDeviceInfo, "CapabilityIDs");
      mxmlNewText(pNode, 0, szCapIDs);

      pNode = mxmlNewElement(pNodeDeviceInfo, "CurrentState");
      mxmlNewText(pNode, 0, szCapValues);

      pNode = mxmlNewElement(pNodeDeviceInfo, "Manufacturer");
      mxmlNewText(pNode, 0, Devices[i]->ManufacturerName);

      pNode = mxmlNewElement(pNodeDeviceInfo, "ModelCode");
      mxmlNewText(pNode, 0, Devices[i]->ModelCode);

      pNode = mxmlNewElement(pNodeDeviceInfo, "productName");
      mxmlNewText(pNode, 0, getProductName(Devices[i]->ModelCode));

      pNode = mxmlNewElement(pNodeDeviceInfo, "WeMoCertified");

      if( Devices[i]->Certified )
      {
        mxmlNewText(pNode, 0, "YES");
      }
      else
      {
        mxmlNewText(pNode, 0, "NO");
      }
    }
  }

  return pCurrentNode;
}

mxml_node_t* MakePairedDeviceGroupInfos(mxml_node_t* pCurrentNode, int nGroupCnt, SD_GroupList Groups)
{
  int i = 0, j = 0;

  mxml_node_t *pNode = NULL;
  mxml_node_t *pNodeGroupInfo = NULL;
  mxml_node_t *pNodeGroupInfos = NULL;
  mxml_node_t *pNodeDeviceInfo = NULL;
  mxml_node_t *pNodeDeviceInfos = NULL;

  char szCapIDs[MAX_CAPIDS_LEN], szCapValues[MAX_CAPVALS_LEN];

  if( NULL == pCurrentNode || 0 == nGroupCnt )
  {
    return pCurrentNode;
  }

  // pCurrentNode --> <DeviceList>
  //Paired and Device is included in Group
  pNodeGroupInfos = mxmlNewElement(pCurrentNode, "GroupInfos");

  for( i = 0; i < nGroupCnt; i++ )
  {
    //Paired and Device is not included in Group
    memset(szCapIDs, 0x00, sizeof(szCapIDs));
    memset(szCapValues, 0x00, sizeof(szCapValues));

    //GroupInfo has n-th node
    pNodeGroupInfo = mxmlNewElement(pNodeGroupInfos, "GroupInfo");

    pNode = mxmlNewElement(pNodeGroupInfo, "GroupID");
    mxmlNewText(pNode, 0, Groups[i]->ID.Data);

    pNode = mxmlNewElement(pNodeGroupInfo, "GroupName");
    mxmlNewText(pNode, 0, Groups[i]->Name);

    GetGroupCapabilityIDs(Groups[i], szCapIDs);
    GetCacheGroupCapabilityValues(Groups[i], szCapValues);

    pNode = mxmlNewElement(pNodeGroupInfo, "GroupCapabilityIDs");
    mxmlNewText(pNode, 0, szCapIDs);

    pNode = mxmlNewElement(pNodeGroupInfo, "GroupCapabilityValues");
    mxmlNewText(pNode, 0, szCapValues);

    pNodeDeviceInfos = mxmlNewElement(pNodeGroupInfo, "DeviceInfos");

    for( j = 0; j < Groups[i]->DeviceCount; j++ )
    {
      memset(szCapIDs, 0x00, sizeof(szCapIDs));
      memset(szCapValues, 0x00, sizeof(szCapValues));

      SD_Device device = Groups[i]->Device[j];

      GetDeviceCapabilityIDs(device, szCapIDs);
      GetCacheDeviceCapabilityValues(device, szCapValues);

      pNodeDeviceInfo = mxmlNewElement(pNodeDeviceInfos, "DeviceInfo");

      pNode = mxmlNewElement(pNodeDeviceInfo, "DeviceIndex");
      mxmlNewInteger(pNode, j);

      pNode = mxmlNewElement(pNodeDeviceInfo, "DeviceID");
      mxmlNewText(pNode, 0, device->EUID.Data);

      pNode = mxmlNewElement(pNodeDeviceInfo, "FriendlyName");
      mxmlNewText(pNode, 0, device->FriendlyName);

      pNode = mxmlNewElement(pNodeDeviceInfo, "IconVersion");
      mxmlNewText(pNode, 0, "1");

      pNode = mxmlNewElement(pNodeDeviceInfo, "FirmwareVersion");
      mxmlNewText(pNode, 0, device->FirmwareVersion);

      pNode = mxmlNewElement(pNodeDeviceInfo, "CapabilityIDs");
      mxmlNewText(pNode, 0, szCapIDs);

      pNode = mxmlNewElement(pNodeDeviceInfo, "CurrentState");
      mxmlNewText(pNode, 0, szCapValues);

      pNode = mxmlNewElement(pNodeDeviceInfo, "Manufacturer");
      mxmlNewText(pNode, 0, device->ManufacturerName);

      pNode = mxmlNewElement(pNodeDeviceInfo, "ModelCode");
      mxmlNewText(pNode, 0, device->ModelCode);

      pNode = mxmlNewElement(pNodeDeviceInfo, "productName");
      mxmlNewText(pNode, 0, getProductName(device->ModelCode));

      pNode = mxmlNewElement(pNodeDeviceInfo, "WeMoCertified");

      if( device->Certified )
      {
        mxmlNewText(pNode, 0, "YES");
      }
      else
      {
        mxmlNewText(pNode, 0, "NO");
      }

    }
  }

  return pCurrentNode;
}

char* CreateEndDeviceListXML(char *pszReqListType)
{
  SD_DeviceList Devices;
  SD_GroupList Groups;

  mxml_node_t *pRespXml = NULL;
  mxml_node_t *pRootNode = NULL;
  mxml_node_t *pNode = NULL;

  char *pszResp = NULL;

  int i = 0;
  int nPaired = 0, nUnpaired = 0;

  int nDeviceCnt = SD_GetDeviceList(&Devices);
  int nGroupCnt = SD_GetGroupList(&Groups);

  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetEndDevices: nDeviceCnt(%d), nGroupCnt(%d)", nDeviceCnt, nGroupCnt);

  if( (0 == nDeviceCnt) && (0 == nGroupCnt) )
  {
    return pszResp;
  }

  for( i = 0; i < nDeviceCnt; i++ )
  {
    if( Devices[i]->Paired )
    {
      nPaired++;
    }
    else
    {
      nUnpaired++;
    }
  }

  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetEndDevices: nPaired(%d), nUnpaired(%d), ListType(%s)",
            nPaired, nUnpaired, pszReqListType);

  if( (0 == nPaired) && (0 == strcmp(pszReqListType, "PAIRED_LIST")) )
  {
    return pszResp;
  }
  if( (0 == nUnpaired) && (0 == strcmp(pszReqListType, "UNPAIRED_LIST")) )
  {
    return pszResp;
  }
  if( (0 == nPaired && 0 == nUnpaired) && (0 == strcmp(pszReqListType, "ALL_LIST")) )
  {
    return pszResp;
  }
  if( (0 == nPaired && 0 == nUnpaired) && (0 == strcmp(pszReqListType, "SCAN_LIST")) )
  {
    return pszResp;
  }

  pRespXml = mxmlNewXML("1.0");
  pRootNode = mxmlNewElement(pRespXml, "DeviceLists");

  if( nPaired && (0 == strcmp(pszReqListType, "ALL_LIST") || 0 == strcmp(pszReqListType, "SCAN_LIST") || 0 == strcmp(pszReqListType, "PAIRED_LIST")) )
  {
    pNode = MakePairedDeviceInfos(pRootNode, nDeviceCnt, Devices);
    pNode = MakePairedDeviceGroupInfos(pNode, nGroupCnt, Groups);
  }

  if( nUnpaired && (0 == strcmp(pszReqListType, "ALL_LIST") || 0 == strcmp(pszReqListType, "SCAN_LIST") || 0 == strcmp(pszReqListType, "UNPAIRED_LIST")) )
  {
    pNode = MakeUnpairedDeviceInfos(pRootNode, nDeviceCnt, Devices);
  }

  if( pRespXml )
  {
    pszResp = mxmlSaveAllocString(pRespXml, MXML_NO_CALLBACK);
  }

  if( pRespXml )
  {
    mxmlDelete(pRespXml);
  }

  return pszResp;
}


//=========================================================================
// WeMo-Bridge UPnP Actions
//=========================================================================

//------------------------------------------------------------------------
//The new action commands of WeMoBridge-LEDLight are here.
//------------------------------------------ OpenNetwork -----------------

#define ZIGBEED_CNT       "ps | grep -v grep | grep -c zigbeed > /tmp/zigbeed_cnt"
#define ZIGBEED_CMD       "/sbin/zigbeed -d &"
#define FLAG_SETUP_DONE   "/tmp/Belkin_settings/flag_setup_done"
#define NETWORK_FORMING   "/tmp/Belkin_settings/sd_network_forming"

static bool IsRunZigbeed(void)
{
  int nZigbeed = 0;
  char szCommand[80] = {0,};

  snprintf(szCommand, sizeof(szCommand), "%s", ZIGBEED_CNT);
  system(szCommand);
  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "ZIGBEED_CNT = %s", szCommand);

  FILE *fp = fopen("/tmp/zigbeed_cnt", "rb");
  if( NULL == fp )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "read Error %s", strerror(errno));
    nZigbeed = 0;
  }
  else
  {
    fscanf(fp, "%d", &nZigbeed);
    fclose(fp);
  }

  return (nZigbeed == 2);
}

static bool IsNetworkForming(void)
{
  struct stat st;
  bool retVal = false;

  if( stat(NETWORK_FORMING, &st) == 0 )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "sd_network_forming is already existed...");
    retVal = true;
  }
  return retVal;
}

static bool IsZigbeeSetupDone(void)
{
  struct stat st;
  bool retVal = false;

  if( stat(FLAG_SETUP_DONE, &st) == 0 )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "flag_setup_done is already existed...");
    retVal = true;
  }

  return retVal;
}

static void FinishZigbeeSetup(void)
{
  char szCommand[80] = {0,};

  snprintf(szCommand, sizeof(szCommand), "touch %s", FLAG_SETUP_DONE);
  system(szCommand);

  SD_SetZBMode(WEMO_LINK_ZC);
}

static void RunZigeebd(void)
{
  char szCommand[80] = {0,};

  snprintf(szCommand, sizeof(szCommand), "%s", ZIGBEED_CMD);
  System(szCommand);
  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "ZIGBEED_CMD = %s", szCommand);
}

static void RunZigbeeSetup()
{
  if( !IsZigbeeSetupDone() )
  {
    if( !IsRunZigbeed() )
    {
      RunZigeebd();
      sleep(1);
    }

    if( IsRunZigbeed() && !IsNetworkForming() )
    {
      SD_NetworkForming();
      sleep(1);
    }

    if( IsRunZigbeed() && IsNetworkForming() )
    {
      FinishZigbeeSetup();
    }
  }
}

int OpenNetwork(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int twice = 0;
  int retVal = UPNP_E_SUCCESS;

  upnp_lock();

  RunZigbeeSetup();

  char* pDevUDN = Util_GetFirstDocumentItem(request, "DevUDN");
  if( NULL == pDevUDN || 0 == strlen(pDevUDN) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "OpenNetwork: DevUDN is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    //Check if the device'udn is same.
    if( 0 != strcmp(pDevUDN, g_szUDN_1) )
    {
      ( *errorString ) = "Invalid Argument";
      retVal = UPNP_E_INVALID_ARGUMENT;
    }
    else
    {
      for( twice = 2; twice; twice-- )
      {
        // Open Zigbee network...
        if( SD_OpenNetwork() )
        {
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "OpenNetwork is Success...");

          // create a response
          if( UpnpAddToActionResponse( out, "OpenNetwork",
                    CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                    NULL, NULL ) != UPNP_E_SUCCESS )
          {
            ( *out ) = NULL;
            ( *errorString ) = "Internal Error";
            DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "OpenNetwork: Internal Error");
            retVal = UPNP_E_INTERNAL_ERROR;
          }
          else
          {
            retVal = UPNP_E_SUCCESS;
          }
          break;
        }
        else
        {
          retVal = UPNP_E_OPEN_NETWORK;

          //If OpenNetwork is failed, check if sd_network_forming is existed, if not, we call SD_NetworkForming().
          if( !IsRunZigbeed() )
          {
            RunZigeebd();
            pluginUsleep(WAIT_500MS);
          }

          if( !IsNetworkForming() )
          {
            SD_NetworkForming();
          }

          if( IsRunZigbeed() && IsNetworkForming() )
          {
            FinishZigbeeSetup();
          }

          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "OpenNetwork is Failed...");
        }
      }
    }
  }

  if( retVal == UPNP_E_OPEN_NETWORK )
  {
    ( *out ) = NULL;
    ( *errorString ) = "OpenNetwork Error";
  }

  FreeXmlSource(pDevUDN);

  upnp_unlock();

  return retVal;
}

int CloseNetwork(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int retVal = UPNP_E_SUCCESS;

  upnp_lock();

  char* pDevUDN = Util_GetFirstDocumentItem(request, "DevUDN");
  if( NULL == pDevUDN || 0 == strlen(pDevUDN) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "CloseNetwork: DevUDN is not existed");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    //Check if the device'udn is same.
    if( 0 != strcmp(pDevUDN, g_szUDN_1) )
    {
      ( *errorString ) = "Invalid Argument";
      retVal = UPNP_E_INVALID_ARGUMENT;
    }
    else
    {
      // Close Zigbee network...
      SD_CloseNetwork();

      // create a response
      if( UpnpAddToActionResponse( out, "CloseNetwork",
                CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                NULL, NULL ) != UPNP_E_SUCCESS )
      {
        ( *out ) = NULL;
        ( *errorString ) = "Internal Error";
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "CloseNetwork: Internal Error");
        retVal = UPNP_E_INTERNAL_ERROR;
      }
    }
  }

  FreeXmlSource(pDevUDN);

  upnp_unlock();

  return retVal;
}

int GetEndDevices(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int retVal = UPNP_E_SUCCESS;

  upnp_lock();

  RunZigbeeSetup();

  char* pDevUDN = Util_GetFirstDocumentItem(request, "DevUDN");
  char* pReqListType = Util_GetFirstDocumentItem(request, "ReqListType");
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "DevUDN %s, pReqListType %s....dp", pDevUDN, pReqListType);
  if( NULL == pDevUDN || 0 == strlen(pDevUDN) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetEndDevices: DevUDN is not existed");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( NULL == pReqListType || 0 == strlen(pReqListType) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetEndDevices: ReqListType is not existed");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }
  else
  {
    if( (0 == strcmp(pReqListType, "ALL_LIST")) ||
        (0 == strcmp(pReqListType, "SCAN_LIST")) ||
        (0 == strcmp(pReqListType, "PAIRED_LIST")) ||
        (0 == strcmp(pReqListType, "UNPAIRED_LIST")) )
    {
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetEndDevices: ReqListType is %s", pReqListType);
    }
    else
    {
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetEndDevices: ReqListType is not valid");
      ( *errorString ) = "Invalid Argument";
      retVal = UPNP_E_INVALID_ARGUMENT;
    }
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    char *pszResponse = NULL;

    //Check if the device'udn is same.
    if( 0 != strcmp(pDevUDN, g_szUDN_1) )
    {
      ( *errorString ) = "Invalid Argument";
      retVal = UPNP_E_INVALID_ARGUMENT;
    }
    else
    {
      //Create Response XML data using subdevice lib
      pszResponse = CreateEndDeviceListXML(pReqListType);

      if( pszResponse )
      {
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetEndDevices: === pszResponse ===");
        DEBUG_DUMP(ULOG_UPNP, UL_DEBUG, pszResponse, get_response_len(pszResponse));
        // create a response
        if( UpnpAddToActionResponse( out, "GetEndDevices",
                  CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                  "DeviceLists", pszResponse ) != UPNP_E_SUCCESS )
        {
          ( *out ) = NULL;
          ( *errorString ) = "Internal Error";
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetEndDevices: Internal Error");
          retVal = UPNP_E_INTERNAL_ERROR;
        }

        free(pszResponse);
      }
      else
      {
        // create a response
        if( UpnpAddToActionResponse( out, "GetEndDevices",
                  CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                  "DeviceLists", "0" ) != UPNP_E_SUCCESS )
        {
          ( *out ) = NULL;
          ( *errorString ) = "Internal Error";
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetEndDevices: Internal Error");
          retVal = UPNP_E_INTERNAL_ERROR;
        }
      }
    }
  }

  FreeXmlSource(pDevUDN);
  FreeXmlSource(pReqListType);

  upnp_unlock();

  return retVal;
}

int AddDeviceName(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  bool bSuccess = true;

  int retVal = UPNP_E_SUCCESS;
  SD_CapabilityID IdentifyCapID;

  upnp_lock();

  char* pDeviceIDs = Util_GetFirstDocumentItem(request, "DeviceIDs");
  if( NULL == pDeviceIDs || 0 == strlen(pDeviceIDs) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "DeviceIDs is not existed");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  char* pFriendlyNames = Util_GetFirstDocumentItem(request, "FriendlyNames");
  if( NULL == pFriendlyNames || 0 == strlen(pFriendlyNames) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "pFriendlyNames is not existed");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    int i = 0, nLen = 0, idx = 0, retry_cnt = 0;
    char szDeviceIDs[MAX_DEVICES][MAX_ID_LEN];
    char szFriendlyNames[MAX_DEVICES][MAX_VAL_LEN];
    char szErrDeviceIDs[MAX_RESP_LEN];

    struct SubDevices subDevices;

    SD_Device device;
    SD_DeviceID deviceID;

    memset(&subDevices, 0x00, sizeof(struct SubDevices));
    memset(&szDeviceIDs, 0x00, sizeof(szDeviceIDs));
    memset(&szFriendlyNames, 0x00, sizeof(szFriendlyNames));
    memset(szErrDeviceIDs, 0x00, sizeof(szErrDeviceIDs));

    int nDevice = 0, nFriendlyName = 0;
    //Get the device id list that is extracted from DeviceIDs input parameter.
    nDevice = GetIDs(pDeviceIDs, szDeviceIDs);

    if( nDevice > MAX_DEVICES )
    {
      nDevice = MAX_DEVICES;
    }

    nFriendlyName = GetValues(pFriendlyNames, szFriendlyNames);

    for( i = 0; i < nDevice; i++ )
    {
      MakeDeviceID(szDeviceIDs[i], &deviceID);

      if( false == SD_AddDevice(&deviceID) )
      {
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_AddDevice(%d, %s) is Failed...", deviceID.Type, deviceID.Data);
        strncat(szErrDeviceIDs, deviceID.Data, strlen(deviceID.Data));
        strncat(szErrDeviceIDs, ",", 1);
      }
      else
      {
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_AddDevice(%d, %s) is Success...", deviceID.Type, deviceID.Data);
        memcpy(&subDevices.szDeviceIDs[idx], deviceID.Data, MAX_ID_LEN);
        subDevices.nDevice = ++idx;

        if( szFriendlyNames[i][0] && SD_SetDeviceProperty(&deviceID, SD_DEVICE_FRIENDLY_NAME, szFriendlyNames[i]) )
        {
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_SetDeviceProperty(%s, %s) is Success...",
                    deviceID.Data, szFriendlyNames[i]);
        }
        else
        {
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_SetDeviceProperty(%s) is Failed...", deviceID.Data);

          strncat(szErrDeviceIDs, deviceID.Data, strlen(deviceID.Data));
          strncat(szErrDeviceIDs, ",", 1);
        }

        if( SD_GetDevice(&deviceID, &device) && (0 == strcmp(device->DeviceType, ZB_LIGHTBULB)) )
        {
          if( GetCapabilityID("Identify", &IdentifyCapID) )
          {
            for( retry_cnt = 0; retry_cnt < 3; retry_cnt++ )
            {
              if( SD_SetDeviceCapabilityValue(&deviceID, &IdentifyCapID, "1", SE_NONE) )
              {
                DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_SetDeviceCapabilityValue(%s, Identify:%s) is Success...", deviceID.Data, IdentifyCapID.Data);
                break;
              }
            }
          }
          SetSubDeviceFWUpdateState(szDeviceIDs[i], OTA_FW_UPGRADE_SUCCESS);
        }
      }
    }

    if( 0 != (nLen = strlen(szErrDeviceIDs)) )
    {
      szErrDeviceIDs[nLen-1] = 0x00;

      if( 1 == nDevice )
      {
        bSuccess = false;
      }
    }

    // create a response
    if( UpnpAddToActionResponse( out, "AddDeviceName",
              CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
              "ErrorDeviceIDs", szErrDeviceIDs ) != UPNP_E_SUCCESS )
    {
      ( *out ) = NULL;
      ( *errorString ) = "Internal Error";
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "AddDeviceName: Internal Error");
      retVal = UPNP_E_INTERNAL_ERROR;
    }

    if( bSuccess )
    {
      trigger_remote_event(WORK_EVENT, ADD_SUBDEVICE, "subdevice", "add", &subDevices, sizeof(struct SubDevices));
    }
  }

  FreeXmlSource(pDeviceIDs);
  FreeXmlSource(pFriendlyNames);

  upnp_unlock();

  return retVal;
}

int AddDevice(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  bool bSuccess = true;

  int retVal = UPNP_E_SUCCESS;
  SD_CapabilityID IdentifyCapID;

  upnp_lock();

  char* pDeviceIDs = Util_GetFirstDocumentItem(request, "DeviceIDs");
  if( NULL == pDeviceIDs || 0 == strlen(pDeviceIDs) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "AddDevice: DeviceIDs is not existed");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    int i = 0, nLen = 0, idx = 0, retry_cnt = 0;
    struct SubDevices subDevices;

    int nDevice = 0;
    char szDeviceIDs[MAX_DEVICES][MAX_ID_LEN];
    char szErrDeviceIDs[MAX_RESP_LEN];

    SD_Device device;
    SD_DeviceID deviceID;

    memset(&subDevices, 0x00, sizeof(struct SubDevices));
    memset(&szDeviceIDs, 0x00, sizeof(szDeviceIDs));
    memset(szErrDeviceIDs, 0x00, sizeof(szErrDeviceIDs));

    //Get the device id list that is extracted from DeviceIDs input parameter.
    nDevice = GetIDs(pDeviceIDs, szDeviceIDs);
    if( nDevice > MAX_DEVICES )
    {
      nDevice = MAX_DEVICES;
    }

    for( i = 0; i < nDevice; i++ )
    {
      MakeDeviceID(szDeviceIDs[i], &deviceID);

      if( false == SD_AddDevice(&deviceID) )
      {
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_AddDevice(%d, %s) is Failed...", deviceID.Type, deviceID.Data);
        strncat(szErrDeviceIDs, szDeviceIDs[i], strlen(szDeviceIDs[i]));
        strncat(szErrDeviceIDs, ",", 1);
      }
      else
      {
	/*This device ID may have been added after hard reset of end device, Clear all of its old presets*/
	deleteAllPreset(deviceID.Data);
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_AddDevice(%d, %s) is Success...", deviceID.Type, deviceID.Data);
        memcpy(&subDevices.szDeviceIDs[idx], deviceID.Data, MAX_ID_LEN);
        subDevices.nDevice = ++idx;

        if( SD_GetDevice(&deviceID, &device) && (0 == strcmp(device->DeviceType, ZB_LIGHTBULB)) )
        {
          if( GetCapabilityID("Identify", &IdentifyCapID) )
          {
            for( retry_cnt = 0; retry_cnt < 3; retry_cnt++ )
            {
              if( SD_SetDeviceCapabilityValue(&deviceID, &IdentifyCapID, "1", SE_NONE) )
              {
                DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_SetDeviceCapabilityValue(%s, Identify:%s) is Success...", deviceID.Data, IdentifyCapID.Data);
                break;
              }
            }
          }
          SetSubDeviceFWUpdateState(szDeviceIDs[i], OTA_FW_UPGRADE_SUCCESS);
        }
      }
    }

    if( 0 != (nLen = strlen(szErrDeviceIDs)) )
    {
      szErrDeviceIDs[nLen-1] = 0x00;

      if( 1 == nDevice )
      {
        bSuccess = false;
      }
    }

    // create a response
    if( UpnpAddToActionResponse( out, "AddDevice",
              CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
              "ErrorDeviceIDs", szErrDeviceIDs ) != UPNP_E_SUCCESS )
    {
      ( *out ) = NULL;
      ( *errorString ) = "Internal Error";
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "AddDevice: Internal Error");
      retVal = UPNP_E_INTERNAL_ERROR;
    }

    if( bSuccess )
    {
      trigger_remote_event(WORK_EVENT, ADD_SUBDEVICE, "subdevice", "add", &subDevices, sizeof(struct SubDevices));
    }
  }

  FreeXmlSource(pDeviceIDs);

  upnp_unlock();

  return retVal;
}

int RemoveDevice(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int retVal = UPNP_E_SUCCESS;

  upnp_lock();

  char* pDeviceIDs = Util_GetFirstDocumentItem(request, "DeviceIDs");
  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "pDeviceIDs<%s>.....dp", pDeviceIDs);
  if( NULL == pDeviceIDs || 0 == strlen(pDeviceIDs) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "RemoveDevice: DeviceIDs is not existed");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    int i = 0, nLen = 0;
    char szDeviceIDs[MAX_DEVICES][MAX_ID_LEN];
    char szErrDeviceIDs[MAX_RESP_LEN];
    char szRegisterDeviceID[DEVICE_MAX_ID_LENGTH];
    SD_DeviceID deviceID;
    SD_Device device;

    memset(&szDeviceIDs, 0x00, sizeof(szDeviceIDs));
    memset(szErrDeviceIDs, 0x00, sizeof(szErrDeviceIDs));

    //Get the device id list that is extracted from DeviceIDs input parameter.
    int nDeviceIDs = GetIDs(pDeviceIDs, szDeviceIDs);
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "nDeviceIDs<%d>.....dp", nDeviceIDs);
    if( nDeviceIDs > MAX_DEVICES ) //MAX_DEVICES = 56
    {
      nDeviceIDs = MAX_DEVICES;
    }

    for( i = 0; i < nDeviceIDs; i++ )
    {
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "szDeviceIDs[%d]<%s>.....dp", i, szDeviceIDs[i]);
      MakeDeviceID(szDeviceIDs[i], &deviceID);

      if( SD_GetDevice(&deviceID, &device) )
      {
        if( is_registered_ids_empty() )
        {
            trigger_remote_event(WORK_EVENT, GET_HOME_DEVICES, "device", "home",
                                g_szHomeId, sizeof(g_szHomeId));
        }
        else
        {
           DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "RegisteredID is existed...");
        }

        memset(szRegisterDeviceID, 0x00, DEVICE_MAX_ID_LENGTH);
        SAFE_STRCPY(szRegisterDeviceID, device->RegisteredID.Data);
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_RemoveDevice(%s) RegisteredID = %s",
                  deviceID.Data, device->RegisteredID.Data);

        // Delete rule by Bulb id
        //BR_DeleteWeeklyCalendarById(deviceID.Data, 0);

        if( SD_RemoveDevice(&deviceID) )
        {
	  /*Clear all of its presets on soft reset*/
	  deleteAllPreset(deviceID.Data);
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_RemoveDevice(%s) is Success...", deviceID.Data);
          //If remote access enable option is turn on, deletes subdevice from the cloud server after calling RemoveDevice() upnp action.
          if( szRegisterDeviceID[0] )
          {
            DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "DEL_SUBDEVICE(%s) is triggered...", szRegisterDeviceID);
            trigger_remote_event(WORK_EVENT, DEL_SUBDEVICE, "subdevice", "delete",
                                szRegisterDeviceID, sizeof(szRegisterDeviceID));
          }
        }
        else
        {
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_RemoveDevice(%s) is Failed...", deviceID.Data);
          strncat(szErrDeviceIDs, szDeviceIDs[i], strlen(szDeviceIDs[i]));
          strncat(szErrDeviceIDs, ",", 1);
        }
      }
      else
      {
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_GetDevice(%s) is Failed...", deviceID.Data);
        strncat(szErrDeviceIDs, szDeviceIDs[i], strlen(szDeviceIDs[i]));
        strncat(szErrDeviceIDs, ",", 1);
      }
    }

    if( 0 != (nLen = strlen(szErrDeviceIDs)) )
    {
      szErrDeviceIDs[nLen-1] = 0x00;
    }

    // create a response
    if( UpnpAddToActionResponse( out, "RemoveDevice",
              CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
              "ErrorDeviceIDs", szErrDeviceIDs ) != UPNP_E_SUCCESS )
    {
      ( *out ) = NULL;
      ( *errorString ) = "Internal Error";
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "RemoveDevice: Internal Error");
      retVal = UPNP_E_INTERNAL_ERROR;
    }
  }

  FreeXmlSource(pDeviceIDs);

  upnp_unlock();

  return retVal;
}

int PerformSetDeviceStatus(char *pXmlBuff, char *pszErrorDeviceIDs)
{
  mxml_node_t *tree = NULL;
  mxml_node_t *node = NULL;
  mxml_node_t *group_node = NULL;
  mxml_node_t *devid_node = NULL;
  mxml_node_t *capid_node = NULL;
  mxml_node_t *capval_node = NULL;

  int id = 0, nCount = 0, nValue = 0, nLen = 0, nCommandFailed = 0;
  int nDeviceStatus = 0, retry_cnt = 0;
  bool bSuccess = false;

  char szCapIDs[MAX_IDS][MAX_ID_LEN];
  char szCapValues[MAX_VALUES][MAX_VAL_LEN];

  if( pszErrorDeviceIDs )
  {
    pszErrorDeviceIDs[0] = 0x00;
  }
  else
  {
    return UPNP_E_INVALID_ARGUMENT;
  }

  tree = mxmlLoadString(NULL, pXmlBuff, MXML_OPAQUE_CALLBACK);

  if( NULL == tree )
  {
    return UPNP_E_PARSING;
  }

  //First, find the DeviceStatus element in xml buffer.
  for( node = mxmlFindElement(tree, tree, "DeviceStatus", NULL, NULL, MXML_DESCEND);
       node != NULL;
       node = mxmlFindElement(node, tree, "DeviceStatus", NULL, NULL, MXML_DESCEND) )
  {
    group_node = mxmlFindElement(node, tree, "IsGroupAction", NULL, NULL, MXML_DESCEND);
    devid_node = mxmlFindElement(node, tree, "DeviceID", NULL, NULL, MXML_DESCEND);
    capid_node = mxmlFindElement(node, tree, "CapabilityID", NULL, NULL, MXML_DESCEND);
    capval_node = mxmlFindElement(node, tree, "CapabilityValue", NULL, NULL, MXML_DESCEND);

    if( !group_node || !devid_node || !capid_node || !capval_node )
    {
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "Not Find some elemenet");
      mxmlDelete(tree);
      return UPNP_E_PARSING;
    }

    char *pIsGroup = (char *)mxmlGetOpaque(group_node);
    char *pDeviceID = (char *)mxmlGetOpaque(devid_node);
    char *pCapabilityID = (char *)mxmlGetOpaque(capid_node);
    char *pCapabilityValue = (char *)mxmlGetOpaque(capval_node);

    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "%s = %s", "IsGroupAction", pIsGroup);
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "%s = %s", "DeviceID", pDeviceID);
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "%s = %s", "CapabilityID", pCapabilityID);
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "%s = %s", "CapabilityValue", pCapabilityValue);

    memset(&szCapIDs, 0x00, sizeof(szCapIDs));
    memset(&szCapValues, 0x00, sizeof(szCapValues));

    if( 0 == strcmp(pIsGroup, "YES") )
    {
      SD_GroupID groupID;
      SD_CapabilityID capID;

      MakeGroupID(pDeviceID, &groupID);

      //check if capabilityID has the capability id list of end device (i.e. 1000,1001,1002)
      nCount = GetIDs(pCapabilityID, szCapIDs);
      nValue = GetValues(pCapabilityValue, szCapValues);

      for( id = 0; id < nCount; id++ )
      {
        MakeCapabilityID(szCapIDs[id], &capID);

        pluginUsleep(WAIT_10MS);

        for( retry_cnt = 0; retry_cnt < 3; retry_cnt++ )
        {
          if( (bSuccess = SD_SetGroupCapabilityValue(&groupID, &capID, szCapValues[id], SE_LOCAL + SE_REMOTE)) )
          {
            break;
          }
        }

        if( bSuccess )
        {
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_SetGroupCapabilityValue(%s,%s,%s) is Success...",
                    groupID.Data, capID.Data, szCapValues[id]);
        }
        else
        {
          nCommandFailed++;
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_SetGroupCapabilityValue(%s,%s,%s) is Failed...",
                    groupID.Data, capID.Data, szCapValues[id]);
        }
      }

      if( nCommandFailed )
      {
        strncat(pszErrorDeviceIDs, pDeviceID, strlen(pDeviceID));
        strncat(pszErrorDeviceIDs, ",", 1);
        nCommandFailed = 0;
      }
    }
    else
    {
      SD_DeviceID deviceID;
      SD_CapabilityID capID;

      nCommandFailed = 0;

      MakeDeviceID(pDeviceID, &deviceID);

      //check if CapabilityID has the capability id list of end device (i.e. 1000,1001,1002)
      //check if CapabilityValue has the value list of end device (i.e. 0,50:80,300:300:300)
      nCount = GetIDs(pCapabilityID, szCapIDs);
      nValue = GetValues(pCapabilityValue, szCapValues);

      for( id = 0; id < nCount; id++ )
      {
        MakeCapabilityID(szCapIDs[id], &capID);

        // pluginUsleep(WAIT_10MS);

        if( (bSuccess = SD_SetDeviceCapabilityValue(&deviceID, &capID, szCapValues[id], SE_LOCAL + SE_REMOTE)) )
        {
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_SetDeviceValue(%s,%s,%s) is Success...",
                    deviceID.Data, capID.Data, szCapValues[id]);
        }
        else
        {
          nCommandFailed++;
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SD_SetDeviceValue(%s,%s,%s) is Failed...",
                    deviceID.Data, capID.Data, szCapValues[id]);
        }
      }

      if( nCount == nCommandFailed )
      {
        strncat(pszErrorDeviceIDs, pDeviceID, strlen(pDeviceID));
        strncat(pszErrorDeviceIDs, ",", 1);
        nCommandFailed = 0;
      }
    }

    nDeviceStatus++;
  }

  if( 0 != (nLen = strlen(pszErrorDeviceIDs)) )
  {
    pszErrorDeviceIDs[nLen-1] = 0x00;
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetDeviceStatus: pszErrorDeviceIDs = %s", pszErrorDeviceIDs);
  }

  mxmlDelete(tree);

  if( 0 == nDeviceStatus )
  {
    return UPNP_E_PARSING;
  }

  return UPNP_E_SUCCESS;
}

int SetDeviceStatus(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int retVal = UPNP_E_SUCCESS;

  char szErrorDeviceIDs[MAX_DEVIDS_LEN];

  upnp_lock();

  char* pXmlBuff = Util_GetFirstDocumentItem(request, "DeviceStatusList");
  if( NULL == pXmlBuff || 0 == strlen(pXmlBuff) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetDeviceStatus: DeviceStatusList is not existed");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    retVal = PerformSetDeviceStatus(pXmlBuff, szErrorDeviceIDs);
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "PerformSetDeviceStatus: retVal = %d", retVal);
    if( retVal == UPNP_E_PARSING )
    {
      ( *out ) = NULL;
      ( *errorString ) = "XML Parsing Error";
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetDeviceStatus: XML Parsing Error");
      retVal = UPNP_E_PARSING;
    }
    else
    {
      // create a response
      if( UpnpAddToActionResponse( out, "SetDeviceStatus",
                CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                "ErrorDeviceIDs", szErrorDeviceIDs ) != UPNP_E_SUCCESS )
      {
        ( *out ) = NULL;
        ( *errorString ) = "Internal Error";
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetDeviceStatus: Internal Error");
        retVal = UPNP_E_INTERNAL_ERROR;
      }
    }
  }

  FreeXmlSource(pXmlBuff);

  upnp_unlock();

  return retVal;
}

char* CreateDeviceStatusXML(char *pszDeviceIDs)
{
  mxml_node_t *pRespXml = NULL;
  mxml_node_t *pNodeDeviceStatusList = NULL;
  mxml_node_t *pNodeDeviceStatus = NULL;
  mxml_node_t *pNodeIsGroupAction = NULL;
  mxml_node_t *pNodeDeviceID = NULL;
  mxml_node_t *pNodeCapabilityID = NULL;
  mxml_node_t *pNodeCapabilityValue = NULL;
  mxml_node_t *pNodeLastEventTimeStamp = NULL;

  int i = 0, nGroup = 0, nDevice = 0;
  bool bAvailabeDevice = false;

  char *pszResp = NULL;
  char szDeviceIDs[MAX_DEVICES][MAX_ID_LEN];
  char szCapIDs[MAX_CAPIDS_LEN], szCapValues[MAX_CAPVALS_LEN];
  char szLastEventTS[32] = {0,};

  SD_DeviceID deviceID;
  SD_Device device;

  SD_GroupID groupID;
  SD_Group group;

  memset(&szDeviceIDs, 0x00, sizeof(szDeviceIDs));

  //Get the device id list that is extracted from DeviceIDs input parameter.
  int nDeviceIDs = GetIDs(pszDeviceIDs, szDeviceIDs);
  if( nDeviceIDs > MAX_DEVICES )
  {
    nDeviceIDs = MAX_DEVICES;
  }

  if( 0 == nDeviceIDs )
  {
    return pszResp;
  }

  pRespXml = mxmlNewXML("1.0");
  pNodeDeviceStatusList = mxmlNewElement(pRespXml, "DeviceStatusList");

  //Get DeviceStatus info from subdevice lib
  for( i = 0; i < nDeviceIDs; i++ )
  {
    memset(szCapIDs, 0x00, sizeof(szCapIDs));
    memset(szCapValues, 0x00, sizeof(szCapValues));

    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetDeviceStatus: szDeviceIDs[%d] = %s", i, szDeviceIDs[i]);

    MakeGroupID(szDeviceIDs[i], &groupID);
    MakeDeviceID(szDeviceIDs[i], &deviceID);

    if( SD_GetDevice(&deviceID, &device) )
    {
#if 0
      if( GetDeviceCapabilityValues(device, szCapIDs, szCapValues) )
#endif
      GetDeviceCapabilityIDs(device, szCapIDs);
      if( GetDeviceCapabilityValues(device, szCapValues) )
      {
        bAvailabeDevice = true;
      }
      else
      {
        bAvailabeDevice = false;
      }

      pNodeDeviceStatus = mxmlNewElement(pNodeDeviceStatusList, "DeviceStatus");

      pNodeIsGroupAction = mxmlNewElement(pNodeDeviceStatus, "IsGroupAction");
      mxmlNewText(pNodeIsGroupAction, 0, "NO");

      pNodeDeviceID = mxmlNewElement(pNodeDeviceStatus, "DeviceID");
      if( bAvailabeDevice )
      {
        mxmlElementSetAttr(pNodeDeviceID, "available", "YES");
      }
      else
      {
        mxmlElementSetAttr(pNodeDeviceID, "available", "NO");
      }
      mxmlNewText(pNodeDeviceID, 0, deviceID.Data);

      pNodeCapabilityID = mxmlNewElement(pNodeDeviceStatus, "CapabilityID");
      mxmlNewText(pNodeCapabilityID, 0, szCapIDs);

      pNodeCapabilityValue = mxmlNewElement(pNodeDeviceStatus, "CapabilityValue");
      mxmlNewText(pNodeCapabilityValue, 0, szCapValues);

      snprintf(szLastEventTS, sizeof(szLastEventTS), "%lu", device->LastReportedTime);
      pNodeLastEventTimeStamp = mxmlNewElement(pNodeDeviceStatus, "LastEventTimeStamp");
      mxmlNewText(pNodeLastEventTimeStamp, 0, szLastEventTS);

      nDevice++;
    }
    else if( SD_GetGroup(&groupID, &group) )
    {
      pNodeDeviceStatus = mxmlNewElement(pNodeDeviceStatusList, "DeviceStatus");

      pNodeIsGroupAction = mxmlNewElement(pNodeDeviceStatus, "IsGroupAction");
      mxmlNewText(pNodeIsGroupAction, 0, "YES");

      pNodeDeviceID = mxmlNewElement(pNodeDeviceStatus, "DeviceID");
      mxmlElementSetAttr(pNodeDeviceID, "available", "YES");
      mxmlNewText(pNodeDeviceID, 0, groupID.Data);

      GetGroupCapabilityIDs(group, szCapIDs);
      GetGroupCapabilityValues(group, szCapValues);

      pNodeCapabilityID = mxmlNewElement(pNodeDeviceStatus, "CapabilityID");
      mxmlNewText(pNodeCapabilityID, 0, szCapIDs);

      pNodeCapabilityValue = mxmlNewElement(pNodeDeviceStatus, "CapabilityValue");
      mxmlNewText(pNodeCapabilityValue, 0, szCapValues);

      nGroup++;
    }
  }

  if( nDevice || nGroup )
  {
    if( pRespXml )
    {
      pszResp = mxmlSaveAllocString(pRespXml, MXML_NO_CALLBACK);
    }
  }

  if( pRespXml )
    mxmlDelete(pRespXml);

  return pszResp;
}


int GetDeviceStatus(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int retVal = UPNP_E_SUCCESS;
  struct timeval start;
  struct timeval now;
  long diff;

  upnp_lock();

  char* pDeviceIDs = Util_GetFirstDocumentItem(request, "DeviceIDs");
  if( (NULL == pDeviceIDs) || ((NULL != pDeviceIDs) && 0 == strlen(pDeviceIDs)) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetDeviceStatus: DeviceIDs is not existed");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetDeviceStatus === start ===");
    gettimeofday(&start, NULL);

    char *pszResponse = CreateDeviceStatusXML(pDeviceIDs);

    if( pszResponse )
    {
      gettimeofday(&now, NULL);
      diff = DiffMillis(&now, &start);
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetDeviceStatus === pszResponse ===  %d ms", diff);
      //DEBUG_DUMP(ULOG_UPNP, UL_DEBUG, pszResponse, get_response_len(pszResponse));

      // create a response
      if( UpnpAddToActionResponse( out, "GetDeviceStatus",
                CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                "DeviceStatusList", pszResponse ) != UPNP_E_SUCCESS )
      {
        ( *out ) = NULL;
        ( *errorString ) = "Internal Error";
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetDeviceStatus: Internal Error");
        retVal = UPNP_E_INTERNAL_ERROR;
      }

      free(pszResponse);
    }
    else
    {
      ( *out ) = NULL;
      ( *errorString ) = "No Devices";
      retVal = UPNP_E_NO_DEVICES;
    }
  }

  FreeXmlSource(pDeviceIDs);

  upnp_unlock();

  return retVal;
}

//Test Code : Generate the IDs of capabiliy profiles
char* GetCapabilityPorfileIDs(void)
{
  int i = 0, nCount = 0;
  SD_CapabilityList CapsList;

  char *pszIDs = NULL;

  nCount = SD_GetCapabilityList(&CapsList);

  if( nCount <= 0 )
  {
    return pszIDs;
  }

  pszIDs = calloc(1, SIZE_256B);

  for( i = 0; i < nCount; i++ )
  {
    if( 0 == strcmp(CapsList[i]->ProfileName, "Identify") )
    {
      continue;
    }
    strncat(pszIDs, CapsList[i]->CapabilityID.Data, strlen(CapsList[i]->CapabilityID.Data));
    strncat(pszIDs, ",", 1);
  }

  int nLen = strlen(pszIDs);
  pszIDs[nLen -1] = 0x00;

  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetCapabilityPorfileIDs = %s", pszIDs);

  return pszIDs;
}

int GetCapabilityProfileIDList(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int retVal = UPNP_E_SUCCESS;

  upnp_lock();

  char* pDevUDN = Util_GetFirstDocumentItem(request, "DevUDN");

  if( NULL == pDevUDN || 0 == strlen(pDevUDN) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetCapabilityProfileIDList: DevUDN is not existed");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    if( 0 != strcmp(pDevUDN, g_szUDN_1) )
    {
      ( *errorString ) = "Invalid Argument";
      retVal = UPNP_E_INVALID_ARGUMENT;
    }
    else
    {
      //Get the IDs of capability profiles from the capability module.
      char *pszResponse = GetCapabilityPorfileIDs();
      if( pszResponse )
      {
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetCapabilityProfileIDList: ==== pszResponse ====");
        DEBUG_DUMP(ULOG_UPNP, UL_DEBUG, pszResponse, get_response_len(pszResponse));
        // create a response
        if( UpnpAddToActionResponse( out, "GetCapabilityProfileIDList",
                  CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                  "CapabilityProfileIDList", pszResponse ) != UPNP_E_SUCCESS )
        {
          ( *out ) = NULL;
          ( *errorString ) = "Internal Error";
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetCapabilityProfileIDList: Internal Error");
          retVal = UPNP_E_INTERNAL_ERROR;
        }

        free(pszResponse);
      }
      else
      {
        ( *out ) = NULL;
        ( *errorString ) = "No CapabilityID";
        retVal = UPNP_E_NO_CAPABILITYID;
      }
    }
  }

  FreeXmlSource(pDevUDN);

  upnp_unlock();

  return retVal;
}

//=================================================================================================================
// Test Code : Creating the capabilityprofiles list xml data for response packet
char* CreateCapabilityProfilesListXML(int nCapIDs, char szCapIDs[][MAX_ID_LEN])
{
  mxml_node_t *pRespXml = NULL;
  mxml_node_t *pNodeCapabilityProfiles = NULL;
  mxml_node_t *pNodeCapabilityProfile = NULL;
  mxml_node_t *pNodeCapabilityID = NULL;
  mxml_node_t *pNodeCapabilitySpec = NULL;
  mxml_node_t *pNodeCapabilityProfileName = NULL;
  mxml_node_t *pNodeCapabilityAttrName = NULL;
  mxml_node_t *pNodeCapabilityDataType = NULL;
  mxml_node_t *pNodeCapabilityNameValue = NULL;
  mxml_node_t *pNodeCapabilityControl = NULL;

  int i = 0, j = 0, nCount = 0;
  int nResponse = 0;
  char *pszResp = NULL;
  SD_CapabilityList CapsList;

  pRespXml = mxmlNewXML("1.0");
  pNodeCapabilityProfiles = mxmlNewElement(pRespXml, "CapabilityProfileList");

  nCount = SD_GetCapabilityList(&CapsList);

  for( i = 0; i < nCapIDs; i++ )
  {
    for( j = 0; j < nCount; j++ )
    {
      if( 0 == strcmp(CapsList[j]->ProfileName, "Identify") )
      {
        continue;
      }

      if( 0 == strcmp(szCapIDs[i], CapsList[j]->CapabilityID.Data) )
      {
        pNodeCapabilityProfile = mxmlNewElement(pNodeCapabilityProfiles, "CapabilityProfile");

        pNodeCapabilityID = mxmlNewElement(pNodeCapabilityProfile, "CapabilityID");
        mxmlNewText(pNodeCapabilityID, 0, CapsList[j]->CapabilityID.Data);

        pNodeCapabilitySpec = mxmlNewElement(pNodeCapabilityProfile, "CapabilitySpec");
        mxmlNewText(pNodeCapabilitySpec, 0, CapsList[j]->Spec);

        pNodeCapabilityProfileName = mxmlNewElement(pNodeCapabilityProfile, "CapabilityProfileName");
        mxmlNewText(pNodeCapabilityProfileName, 0, CapsList[j]->ProfileName);

        pNodeCapabilityAttrName = mxmlNewElement(pNodeCapabilityProfile, "CapabilityAttrName");
        mxmlNewText(pNodeCapabilityAttrName, 0, CapsList[j]->AttrName);

        pNodeCapabilityDataType = mxmlNewElement(pNodeCapabilityProfile, "CapabilityDataType");
        mxmlNewText(pNodeCapabilityDataType, 0, CapsList[j]->DataType);

        pNodeCapabilityNameValue = mxmlNewElement(pNodeCapabilityProfile, "CapabilityNameValue");
        mxmlNewText(pNodeCapabilityNameValue, 0, CapsList[j]->NameValue);

        pNodeCapabilityControl = mxmlNewElement(pNodeCapabilityProfile, "CapabilityControl");
        mxmlNewText(pNodeCapabilityControl, 0, CapsList[j]->Control);

        nResponse++;
      }
    }
  }

  if( nResponse )
  {
    if( pRespXml )
    {
      pszResp = mxmlSaveAllocString(pRespXml, MXML_NO_CALLBACK);
    }
  }

  if( pRespXml )
    mxmlDelete(pRespXml);

  return pszResp;
}

//================================================================================================================

int GetCapabilityProfileList(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int retVal = UPNP_E_SUCCESS;
  char szCapIDs[MAX_PROFILES][MAX_ID_LEN];

  upnp_lock();

  char* pCapIDs = Util_GetFirstDocumentItem(request, "CapabilityIDs");

  if( NULL == pCapIDs || 0 == strlen(pCapIDs) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetCapabilityProfileList: CapabilityIDs is not existed");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    memset(&szCapIDs, 0x00, sizeof(szCapIDs));
    //Get the device id list that is extracted from DeviceIDs input parameter.
    int nCapIDs = GetIDs(pCapIDs, szCapIDs);
    if( nCapIDs > MAX_PROFILES )
    {
      nCapIDs = MAX_PROFILES;
    }

    char *pszResponse = CreateCapabilityProfilesListXML(nCapIDs, szCapIDs);
    if( pszResponse )
    {
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetCapabilityProfileList: ==== pszResponse ====");
      DEBUG_DUMP(ULOG_UPNP, UL_DEBUG, pszResponse, get_response_len(pszResponse));
      // create a response
      if( UpnpAddToActionResponse( out, "GetCapabilityProfileList",
                CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                "CapabilityProfileList", pszResponse ) != UPNP_E_SUCCESS )
      {
        ( *out ) = NULL;
        ( *errorString ) = "Internal Error";
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetCapabilityProfileList: Internal Error");
        retVal = UPNP_E_INTERNAL_ERROR;
      }

      free(pszResponse);
    }
    else
    {
      ( *out ) = NULL;
      ( *errorString ) = "No CapabilityProfileList";
      retVal = UPNP_E_CAPABILITYPROFILELIST;
    }
  }

  FreeXmlSource(pCapIDs);

  upnp_unlock();

  return retVal;
}

char* CreateDeviceCapabilitiesXML(char* pszDeviceIDs)
{
  mxml_node_t *pRespXml = NULL;
  mxml_node_t *pNodeDevCaps = NULL;
  mxml_node_t *pNodeDevCap = NULL;
  mxml_node_t *pNodeDeviceID = NULL;
  mxml_node_t *pNodeCapIDs = NULL;

  char *pszResp = NULL;
  int i = 0, nDeviceCapability = 0;
  char szCapIDs[MAX_CAPIDS_LEN];
  char szDeviceIDs[MAX_DEVICES][MAX_ID_LEN];

  SD_DeviceID deviceID;
  SD_Device device;

  memset(&szDeviceIDs, 0x00, sizeof(szDeviceIDs));

  //Get the device id list that is extracted from DeviceIDs input parameter.
  int nDeviceIDs = GetIDs(pszDeviceIDs, szDeviceIDs);
  if( nDeviceIDs > MAX_DEVICES )
  {
    nDeviceIDs = MAX_DEVICES;
  }

  if( 0 == nDeviceIDs )
  {
    return pszResp;
  }

  pRespXml = mxmlNewXML("1.0");
  pNodeDevCaps = mxmlNewElement(pRespXml, "DeviceCapabilities");

  for( i = 0; i < nDeviceIDs; i++ )
  {
    MakeDeviceID(szDeviceIDs[i], &deviceID);

    if( SD_GetDevice(&deviceID, &device) )
    {
      pNodeDevCap = mxmlNewElement(pNodeDevCaps, "DeviceCapability");

      pNodeDeviceID = mxmlNewElement(pNodeDevCap, "DeviceID");
      mxmlNewText(pNodeDeviceID, 0, szDeviceIDs[i]);

      GetDeviceCapabilityIDs(device, szCapIDs);

      pNodeCapIDs = mxmlNewElement(pNodeDevCap, "CapabilityIDs");
      mxmlNewText(pNodeCapIDs, 0, szCapIDs);

      nDeviceCapability++;
    }
  }

  if( nDeviceCapability )
  {
    if( pRespXml )
    {
      pszResp = mxmlSaveAllocString(pRespXml, MXML_NO_CALLBACK);
    }
  }

  if( pRespXml )
    mxmlDelete(pRespXml);

  return pszResp;
}

int GetDeviceCapabilities(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int retVal = UPNP_E_SUCCESS;

  upnp_lock();

  char* pDeviceIDs = Util_GetFirstDocumentItem(request, "DeviceIDs");
  if( (NULL == pDeviceIDs) || ((NULL != pDeviceIDs) && 0 == strlen(pDeviceIDs)) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetDeviceCapabilities: DeviceIDs is not existed");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    char *pszResponse = CreateDeviceCapabilitiesXML(pDeviceIDs);

    if( pszResponse )
    {
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetDeviceCapabilities: ==== pszResponse ====");
      DEBUG_DUMP(ULOG_UPNP, UL_DEBUG, pszResponse, get_response_len(pszResponse));
      // create a response
      if( UpnpAddToActionResponse( out, "GetDeviceCapabilities",
                CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                "DeviceCapabilities", pszResponse ) != UPNP_E_SUCCESS )
      {
        ( *out ) = NULL;
        ( *errorString ) = "Internal Error";
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetDeviceCapabilities: Internal Error");
        retVal = UPNP_E_INTERNAL_ERROR;
      }

      free(pszResponse);
    }
    else
    {
      ( *out ) = NULL;
      ( *errorString ) = "No DeviceCapability";
      retVal = UPNP_E_DEVICECAPABILITY;
    }
  }

  FreeXmlSource(pDeviceIDs);

  upnp_unlock();

  return retVal;
}

int PerformCreateGroup(char *pXmlBuff, int *pnDevices)
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

  int i = 0, j = 0, nRet = UPNP_E_PARSING;
  int nDeviceID = 0, nCapIDs = 0, nCapValues = 0;
  int nGroup = 0, nDevices = 0;
  int retry_cnt = 0;

  bool bSuccess = false;
  bool bUpdateGroup = false;
  bool bAlreadySetting = false;

  SD_Group group;
  SD_GroupID groupID;
  SD_DeviceID deviceID[MAX_DEVICES];

  // SD_CapabilityID OnOffCapID, LevelCapID;
  SD_CapabilityID GroupCapID;

  // char *pErrorCode = NULL;
  char szGroupName[MAX_NAME_LEN];
  char szDeviceIDs[MAX_DEVICES][MAX_ID_LEN];
  char szGroupCapIDs[MAX_GROUPS][MAX_ID_LEN];
  char szGroupCapValues[MAX_GROUPS][MAX_VAL_LEN];

  *pnDevices = 0;

  tree = mxmlLoadString(NULL, pXmlBuff, MXML_OPAQUE_CALLBACK);
  if( NULL == tree )
  {
    return UPNP_E_PARSING;
  }

  //First, find the <SetGroupDetails> element in xml buffer.
  for( node = mxmlFindElement(tree, tree, "CreateGroup", NULL, NULL, MXML_DESCEND);
       node != NULL;
       node = mxmlFindElement(node, tree, "CreateGroup", NULL, NULL, MXML_DESCEND) )
  {
    nCapIDs = 0;
    nCapValues = 0;
    nDeviceID = 0;

    memset(szGroupName, 0x00, sizeof(szGroupName));
    memset(&szDeviceIDs, 0x00, sizeof(szDeviceIDs));
    memset(&szGroupCapIDs, 0x00, sizeof(szGroupCapIDs));
    memset(&szGroupCapValues, 0x00, sizeof(szGroupCapValues));

    for( i = 0; i < 5; i++ )
    {
      find_node = mxmlFindElement(node, tree, pszElementName[i], NULL, NULL, MXML_DESCEND);

      if( NULL == find_node )
      {
        continue;
      }

      char *pValue = (char *)mxmlGetOpaque(find_node);
      if( pValue )
      {
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "%s = %s", pszElementName[i], pValue);
      }

      //GroupID is mandatory, the other elements are optional...
      if( 0 == strcmp(pszElementName[i], "GroupID") )
      {
        MakeGroupID(pValue, &groupID);
      }
      else if( 0 == strcmp(pszElementName[i], "GroupName") )
      {
        SAFE_STRCPY(szGroupName, pValue);
      }
      else if( 0 == strcmp(pszElementName[i], "DeviceIDList") )
      {
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
        nCapIDs = GetIDs(pValue, szGroupCapIDs);
        for( j = 0; j < nCapIDs; j++ )
        {
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "PerformCreateGroup: CapabilityID = %s", szGroupCapIDs[j]);
        }
      }
      else if( 0 == strcmp(pszElementName[i], "GroupCapabilityValues") )
      {
        nCapValues = GetValues(pValue, szGroupCapValues);
        for( j = 0; j < nCapValues; j++ )
        {
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "PerformCreateGroup: CapabilityValue = %s", szGroupCapValues[j]);
        }
      }
    }

    if( SD_GetGroup(&groupID, &group) )
    {
      bUpdateGroup = true;
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "PerformCreateGroup: groupID(%s) is already existed...", groupID.Data);

      //cease of removing the device id from the group
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "PerformCreateGroup: group device cnt = %d, nDeviceID = %d",
                group->DeviceCount, nDeviceID);

      if( group->DeviceCount > nDeviceID )
      {
        nRet = UPNP_E_SUCCESS;
        if( nCapIDs && nCapValues )
        {
          for( j = 0; j < nCapIDs; j++ )
          {
            MakeCapabilityID(szGroupCapIDs[j], &GroupCapID);
            if( IsProfileCapabilityID("OnOff", &GroupCapID)
                || IsProfileCapabilityID("LevelControl", &GroupCapID)
                || IsProfileCapabilityID("SleepFader", &GroupCapID)
                || IsProfileCapabilityID("ColorControl", &GroupCapID)
                || IsProfileCapabilityID("ColorTemperature", &GroupCapID)
                || IsProfileCapabilityID("IASZone", &GroupCapID)
                || IsProfileCapabilityID("SensorConfig", &GroupCapID)
                || IsProfileCapabilityID("SensorTestMode", &GroupCapID)
                || IsProfileCapabilityID("SensorKeyPress", &GroupCapID) )
            {
              if( SD_SetGroupCapabilityValue(&groupID, &GroupCapID, szGroupCapValues[j], SE_NONE) )
              {
                DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "PerformCreateGroup: SD_SetGroupCapabilityValue(%s,%s)",
                          GroupCapID.Data, szGroupCapValues[j]);
                bSuccess = true;
              }
              pluginUsleep(WAIT_1SEC);
            }
          }
        }

        bAlreadySetting = true;

        if( false == bSuccess )
        {
          nRet = UPNP_E_COMMAND;
        }
      }
    }
    else
    {
      bUpdateGroup = false;
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "PerformCreateGroup: groupID(%s) is created...", groupID.Data);
    }

    nDevices = SD_SetGroupDevices(&groupID, szGroupName, deviceID, nDeviceID);
    *pnDevices = nDevices;

    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "PerformCreateGroup: SD_SetGroupDevices [%d] devices Success...", nDevices);

    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "Delete rules and SleepFader of individual devices. total devices: %d \n", nDeviceID);

    usleep(WAIT_500MS);

    // Rule Manual Toggled history will be inherited.
    //BR_ManualToggleDataInheritance(groupID.Data);

    for( j = 0 ; j < nDeviceID; j++ )
    {
      deviceID[j].Type = SD_DEVICE_EUID;

      // Delete rule by device ID respectively.
      //BR_DeleteWeeklyCalendarById(deviceID[j].Data, 0);

      // Delete SleepFader Reserved timer.
      SM_CancelReservedCapabilityUpdateById(deviceID[j].Data);

      // Clear cache value
      SD_ClearSleepFaderCache(&deviceID[j]);
    }

    if( nDevices )
    {
      if( false == bAlreadySetting )
      {
        nRet = UPNP_E_SUCCESS;

        if( nCapIDs && nCapValues )
        {
          for( j = 0; j < nCapIDs; j++ )
          {
            MakeCapabilityID(szGroupCapIDs[j], &GroupCapID);
            DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "   New created, Capability name: %s \n", GroupCapID.Data);

            if( IsProfileCapabilityID("OnOff", &GroupCapID)
            || IsProfileCapabilityID("LevelControl", &GroupCapID)
            || IsProfileCapabilityID("SleepFader", &GroupCapID)
            || IsProfileCapabilityID("ColorControl", &GroupCapID)
            || IsProfileCapabilityID("ColorTemperature", &GroupCapID)
            || IsProfileCapabilityID("IASZone", &GroupCapID)
            || IsProfileCapabilityID("SensorConfig", &GroupCapID)
            || IsProfileCapabilityID("SensorTestMode", &GroupCapID)
            || IsProfileCapabilityID("SensorKeyPress", &GroupCapID) )
            {
              if( SD_SetGroupCapabilityValue(&groupID, &GroupCapID, szGroupCapValues[j], SE_NONE) )
              {
                 DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "PerformCreateGroup: SD_SetGroupCapabilityValue(%s,%s)",
                           GroupCapID.Data, szGroupCapValues[j]);
                 bSuccess = true;
              }
              pluginUsleep(WAIT_1SEC);
            }
          }
        }
      }

      if( false == bSuccess )
      {
        nRet = UPNP_E_COMMAND;
      }

      if( is_registered_ids_empty() )
      {
        trigger_remote_event(WORK_EVENT, GET_HOME_DEVICES, "device", "home", g_szHomeId, sizeof(g_szHomeId));
      }

      if( bUpdateGroup )
      {
        trigger_remote_event(WORK_EVENT, UPDATE_GROUPDEVICE, "group", "update", &groupID, sizeof(SD_GroupID));
      }
      else
      {
        trigger_remote_event(WORK_EVENT, CREATE_GROUPDEVICE, "group", "create", &groupID, sizeof(SD_GroupID));
      }
    }
    else
    {
      nRet = UPNP_E_COMMAND;
    }

    nGroup++;
  }

  mxmlDelete(tree);

  if( 0 == nGroup )
  {
    return UPNP_E_PARSING;
  }

  return nRet;
}

// Grouping command for end devices
int CreateGroup(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int nDevices = 0;
  int retVal = UPNP_E_SUCCESS;

  upnp_lock();

  char* pReqXML = Util_GetFirstDocumentItem(request, "ReqCreateGroup");

  if( NULL == pReqXML || 0 == strlen(pReqXML) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "CreateGroup: pReqCreateGroup is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    char szResponse[10] = {0,};
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "CreateGroup: pReqXML= %s", pReqXML);
    PerformCreateGroup(pReqXML, &nDevices);
    snprintf(szResponse, sizeof(szResponse), "%d", nDevices);

    // Create UPnP response for CreateGroup
    if( UpnpAddToActionResponse( out, "CreateGroup",
              CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
              "RespCreateGroup", szResponse ) != UPNP_E_SUCCESS )
    {
      ( *out ) = NULL;
      ( *errorString ) = "Internal Error";
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetGroup: Internal Error");
      retVal = UPNP_E_INTERNAL_ERROR;
    }
  }

  FreeXmlSource(pReqXML);

  upnp_unlock();

  return retVal;
}

int DeleteGroup(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int retVal = UPNP_E_SUCCESS;

  char szResponse[10] = {0,};

  upnp_lock();

  char* pGroupID = Util_GetFirstDocumentItem(request, "GroupID");

  if( NULL == pGroupID || 0 == strlen(pGroupID) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "DeleteGroup: pGroupID is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    SD_GroupID groupID;

    MakeGroupID(pGroupID, &groupID);

    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "DeleteGroup: GroupID = %s", pGroupID);

    //BR_DeleteWeeklyCalendarById(pGroupID, 1);

    // Reserve UPnP Notification for individual bulb.
    SD_ReserveNotificationiForDeleteGroup(&groupID);

    // Delete SleepFader Reserved timer.
    SM_CancelReservedCapabilityUpdate(&groupID);

    // Clear cache value
    SD_ClearSleepFaderCache(&groupID);

    if( SD_DeleteGroup(&groupID) )
    {
      SAFE_STRCPY(szResponse, "1");
      trigger_remote_event(WORK_EVENT, DELETE_GROUPDEVICE, "group", "delete", &groupID, sizeof(SD_GroupID));
      /*Clear all presets on group delete*/
      deleteAllPreset(pGroupID);
    }
    else
    {
      SAFE_STRCPY(szResponse, "0");
    }

    // Create UPnP response for DeleteGroup
    if( UpnpAddToActionResponse( out, "DeleteGroup",
              CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
              "ResponseStatus", szResponse ) != UPNP_E_SUCCESS )
    {
      ( *out ) = NULL;
      ( *errorString ) = "Internal Error";
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "DeleteGroup: Internal Error");
      retVal = UPNP_E_INTERNAL_ERROR;
    }
  }

  FreeXmlSource(pGroupID);

  upnp_unlock();

  return retVal;
}

char* CreateGetGroupsResponseXML(struct RespGetGroups *pRespGroups)
{
  mxml_node_t *pRespXml = NULL;
  mxml_node_t *pNodeGroupID = NULL;
  mxml_node_t *pNodeGroupName = NULL;
  mxml_node_t *pNodeDeviceIDs = NULL;
  mxml_node_t *pNodeGroupInfo = NULL;
  mxml_node_t *pNodeGroupInfos = NULL;

  int i = 0;
  char *pszResp = NULL;

  pRespXml = mxmlNewXML("1.0");
  pNodeGroupInfos = mxmlNewElement(pRespXml, "GroupInfos");

  for( i = 0; i < pRespGroups->nGroups; i++ )
  {
    struct RespGroup *pGroup = &pRespGroups->respGroups[i];

    pNodeGroupInfo = mxmlNewElement(pNodeGroupInfos, "GroupInfo");

    pNodeGroupID = mxmlNewElement(pNodeGroupInfo, "GroupID");
    mxmlNewText(pNodeGroupID, 0, pGroup->szGroupID);

    pNodeGroupName = mxmlNewElement(pNodeGroupInfo, "GroupName");
    mxmlNewText(pNodeGroupName, 0, pGroup->szGroupName);

    pNodeDeviceIDs = mxmlNewElement(pNodeGroupInfo, "DeviceIDs");
    mxmlNewText(pNodeDeviceIDs, 0, pGroup->szDeviceIDs);
  }

  if( pRespGroups->nGroups )
  {
    if( pRespXml )
    {
      pszResp = mxmlSaveAllocString(pRespXml, MXML_NO_CALLBACK);
    }
  }

  if( pRespXml )
  {
    mxmlDelete(pRespXml);
  }

  return pszResp;
}

int GetGroups(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int retVal = UPNP_E_SUCCESS;

  upnp_lock();

  char* pDevUDN = Util_GetFirstDocumentItem(request, "DevUDN");

  if( NULL == pDevUDN || 0 == strlen(pDevUDN) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetGroups: DevUDN is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    //Check if the device'udn is the same of Current WeMo Bridge.
    if( 0 != strcmp(pDevUDN, g_szUDN_1) )
    {
      ( *errorString ) = "Invalid Argument";
      retVal = UPNP_E_INVALID_ARGUMENT;
    }
    else
    {
      int nGroups = 0, i = 0, j = 0;
      SD_GroupList groupList;
      struct RespGetGroups respGetGroups;

      memset(&respGetGroups, 0x00, sizeof(respGetGroups));

      nGroups = SD_GetGroupList(&groupList);
      if( nGroups > MAX_GROUPS )
      {
        nGroups = MAX_GROUPS;
      }

      for( i = 0; i < nGroups; i++ )
      {
        respGetGroups.nGroups++;

        SAFE_STRCPY(respGetGroups.respGroups[i].szGroupID, groupList[i]->ID.Data);
        SAFE_STRCPY(respGetGroups.respGroups[i].szGroupName, groupList[i]->Name);
        for( j = 0; j < groupList[i]->DeviceCount; j++ )
        {
          strncat(respGetGroups.respGroups[i].szDeviceIDs, groupList[i]->Device[j]->EUID.Data, DEVICE_MAX_ID_LENGTH);
          strncat(respGetGroups.respGroups[i].szDeviceIDs, ",", 1);
        }

        if( j )
        {
          respGetGroups.respGroups[i].szDeviceIDs[strlen(respGetGroups.respGroups[i].szDeviceIDs)-1] = 0x00;
        }
      }

      // Create a Response XML of Groups.
      char *pszResponse = CreateGetGroupsResponseXML(&respGetGroups);

      if( pszResponse )
      {
        // Create a UPnP response for GetGroup
        if( UpnpAddToActionResponse( out, "GetGroups",
                  CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                  "RespGetGroups", pszResponse ) != UPNP_E_SUCCESS )
        {
          ( *out ) = NULL;
          ( *errorString ) = "Internal Error";
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetGroups: Internal Error");
          retVal = UPNP_E_INTERNAL_ERROR;
        }

        free(pszResponse);
      }
      else
      {
        // Create a UPnP response for GetGroup
        if( UpnpAddToActionResponse( out, "GetGroups",
                  CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                  "RespGetGroups", "0" ) != UPNP_E_SUCCESS )
        {
          ( *out ) = NULL;
          ( *errorString ) = "Internal Error";
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetGroups: Internal Error");
          retVal = UPNP_E_INTERNAL_ERROR;
        }
      }
    }
  }

  FreeXmlSource(pDevUDN);

  upnp_unlock();

  return retVal;
}

int SetDeviceName(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int retVal = UPNP_E_SUCCESS;

  SD_DeviceID deviceID;
  char szResponse[10] = {0,};

  upnp_lock();

  char* pDeviceID = Util_GetFirstDocumentItem(request, "DeviceID");
  char* pFriendlyName = Util_GetFirstDocumentItem(request, "FriendlyName");

  if( NULL == pDeviceID || 0 == strlen(pDeviceID) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetDeviceName: DeviceID is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }
  if( NULL == pFriendlyName || 0 == strlen(pFriendlyName) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetDeviceName: FriendlyName is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    MakeDeviceID(pDeviceID, &deviceID);

    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "DeviceID = %s, FriendlyName = %s", pDeviceID, pFriendlyName);

    if( SD_SetDeviceProperty(&deviceID, SD_DEVICE_FRIENDLY_NAME, pFriendlyName) )
    {
      SAFE_STRCPY(szResponse, "1");

      struct DeviceInfo *pDevInfo = NULL;
      pDevInfo = (struct DeviceInfo *)calloc(1, sizeof(struct DeviceInfo));
      if( pDevInfo )
      {
        SD_Device device;
        //[WEMO-42914] - Check NULL to avoid SIGSEGV
        if( SD_GetDevice(&deviceID, &device) )
        {
          memcpy(pDevInfo->szDeviceID, pDeviceID, sizeof(pDevInfo->szDeviceID));

          memcpy(pDevInfo->CapInfo.szCapabilityID, device->Capability[0]->CapabilityID.Data,
                sizeof(pDevInfo->CapInfo.szCapabilityID));
          memcpy(pDevInfo->CapInfo.szCapabilityValue, device->CapabilityValue[0],
                sizeof(pDevInfo->CapInfo.szCapabilityValue));
          pDevInfo->CapInfo.bAvailable = true;
          pDevInfo->bGroupDevice = false;

          trigger_remote_event(WORK_EVENT, UPDATE_SUBDEVICE, "device", "update", pDevInfo, sizeof(struct DeviceInfo));
        }
        else
        {
          SAFE_STRCPY(szResponse, "0");
        }

        free(pDevInfo);
      }
    }
    else
    {
      SAFE_STRCPY(szResponse, "0");
    }

    // Create a UPnP response for GetGroup
    if( UpnpAddToActionResponse( out, "SetDeviceName",
              CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
              "Status", szResponse ) != UPNP_E_SUCCESS )
    {
      ( *out ) = NULL;
      ( *errorString ) = "Internal Error";
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetGroups: Internal Error");
      retVal = UPNP_E_INTERNAL_ERROR;
    }
  }

  FreeXmlSource(pDeviceID);
  FreeXmlSource(pFriendlyName);

  upnp_unlock();

  return retVal;
}


int SetDeviceNames(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int retVal = UPNP_E_SUCCESS;

  SD_DeviceID deviceID;

  upnp_lock();

  char* pDeviceIDs = Util_GetFirstDocumentItem(request, "DeviceIDs");
  char* pFriendlyNames = Util_GetFirstDocumentItem(request, "FriendlyNames");

  int nDevice = 0, nFriendlyName = 0;
  char szDeviceIDs[MAX_DEVICES][MAX_ID_LEN];
  char szFridendlyNames[MAX_DEVICES][MAX_NAME_LEN];
  char szErrDeviceIDs[MAX_RESP_LEN];

  if( NULL == pDeviceIDs || 0 == strlen(pDeviceIDs) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetDeviceName: DeviceID is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }
  if( NULL == pFriendlyNames || 0 == strlen(pFriendlyNames) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetDeviceName: FriendlyName is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    int i = 0;
    struct SubDevices subDevices;

    SD_Device device;
    SD_DeviceID deviceID;

    memset(&subDevices, 0x00, sizeof(struct SubDevices));
    memset(&szDeviceIDs, 0x00, sizeof(szDeviceIDs));
    memset(&szFridendlyNames, 0x00, sizeof(szFridendlyNames));
    memset(szErrDeviceIDs, 0x00, sizeof(szErrDeviceIDs));

    //Get the device id list that is extracted from DeviceIDs input parameter.
    nDevice = GetIDs(pDeviceIDs, szDeviceIDs);
    nFriendlyName = GetNames(pFriendlyNames, szFridendlyNames);

    if( nDevice > MAX_DEVICES )
    {
      nDevice = MAX_DEVICES;
    }
    if( nFriendlyName > MAX_DEVICES )
    {
      nFriendlyName = MAX_DEVICES;
    }
	if(nDevice == nFriendlyName)
	{
      for( i = 0; i < nDevice; i++ )
      {
        MakeDeviceID(szDeviceIDs[i], &deviceID);
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "DeviceID = %s, FriendlyName = %s", szDeviceIDs[i], szFridendlyNames[i]);
        if( SD_SetDeviceProperty(&deviceID, SD_DEVICE_FRIENDLY_NAME, szFridendlyNames[i]) )
        {
          struct DeviceInfo *pDevInfo = NULL;
          pDevInfo = (struct DeviceInfo *)calloc(1, sizeof(struct DeviceInfo));
          if( pDevInfo )
          {
            SD_Device device;
            //[WEMO-42914] - Check NULL to avoid SIGSEGV
            if( SD_GetDevice(&deviceID, &device) )
            {
              memcpy(pDevInfo->szDeviceID, szDeviceIDs[i], sizeof(pDevInfo->szDeviceID));

              memcpy(pDevInfo->CapInfo.szCapabilityID, device->Capability[0]->CapabilityID.Data,
                sizeof(pDevInfo->CapInfo.szCapabilityID));
              memcpy(pDevInfo->CapInfo.szCapabilityValue, device->CapabilityValue[0],
                sizeof(pDevInfo->CapInfo.szCapabilityValue));
              pDevInfo->CapInfo.bAvailable = true;
              pDevInfo->bGroupDevice = false;

              trigger_remote_event(WORK_EVENT, UPDATE_SUBDEVICE, "device", "update", pDevInfo, sizeof(struct DeviceInfo));
            }
            else
            {
              DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetDeviceName(%d, %s) is Failed, cannot get device!", szDeviceIDs[i], szFridendlyNames[i]);
              strncat(szErrDeviceIDs, szDeviceIDs[i], strlen(szDeviceIDs[i]));
              strncat(szErrDeviceIDs, ",", 1);
            }

            free(pDevInfo);
          }
        }
        else
        {
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetDeviceName(%d, %s) is Failed...", szDeviceIDs[i], szFridendlyNames[i]);
          strncat(szErrDeviceIDs, szDeviceIDs[i], strlen(szDeviceIDs[i]));
          strncat(szErrDeviceIDs, ",", 1);
        }
      }
	}else{
	  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetDeviceName: Device IDs and FriendlyNames are not matched!!!");
	  ( *errorString ) = "Invalid Argument";
	  retVal = UPNP_E_INVALID_ARGUMENT;
	}
  }

  if(retVal == UPNP_E_INVALID_ARGUMENT)
  {
    if( UpnpAddToActionResponse( out, "SetDeviceNames",
        CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
        "ErrorDeviceIDs", pDeviceIDs ) != UPNP_E_SUCCESS )
    {
      ( *out ) = NULL;
      ( *errorString ) = "Internal Error";
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetGroups: Internal Error");
      retVal = UPNP_E_INTERNAL_ERROR;
    }
  }
  else
  {
    if( UpnpAddToActionResponse( out, "SetDeviceNames",
	    CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
	    "ErrorDeviceIDs", szErrDeviceIDs ) != UPNP_E_SUCCESS )
    {
      ( *out ) = NULL;
      ( *errorString ) = "Internal Error";
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "GetGroups: Internal Error");
      retVal = UPNP_E_INTERNAL_ERROR;
    }
  }

  FreeXmlSource(pDeviceIDs);
  FreeXmlSource(pFriendlyNames);

  upnp_unlock();

  return retVal;
}

int IdentifyDevice(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int i = 0, retVal = UPNP_E_SUCCESS;
  int nError = 1;

  SD_Device device;
  SD_DeviceID deviceID;

  char szResponse[10] = {0,};

  upnp_lock();

  char* pDeviceID = Util_GetFirstDocumentItem(request, "DeviceID");

  if( NULL == pDeviceID || 0 == strlen(pDeviceID) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "IdentifyDevice: DeviceID is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    MakeDeviceID(pDeviceID, &deviceID);

    //0.35s off/on, 0.35s off/on, 0.35s off/on (Using the capability of toggle on/off)
    if( SD_GetDevice(&deviceID, &device) )
    {
      for( i = 0; i < device->CapabilityCount; i++ )
      {
        if( 0 == strcmp(device->Capability[i]->ProfileName, "Identify") )
        {
          if( SD_SetDeviceCapabilityValue(&deviceID, &(device->Capability[i]->CapabilityID), "3", SE_NONE) )
          {
            nError = 0;
          }
          else
          {
            nError = 1;
          }
        }
      }
    }

    if( nError )
    {
      SAFE_STRCPY(szResponse, "0");
    }
    else
    {
      SAFE_STRCPY(szResponse, "1");
    }

    // Create a UPnP response for IdentifyDevice
    if( UpnpAddToActionResponse( out, "IdentifyDevice",
              CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
              "Status", szResponse ) != UPNP_E_SUCCESS )
    {
      ( *out ) = NULL;
      ( *errorString ) = "Internal Error";
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "IdentifyDevice: Internal Error");
      retVal = UPNP_E_INTERNAL_ERROR;
    }
  }

  FreeXmlSource(pDeviceID);

  upnp_unlock();

  return retVal;
}

int QAControl(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  bool bSuccess = false;

  int retVal = UPNP_E_SUCCESS;
  char szResponse[10] = {0,};

  upnp_lock();

  char* pCommand = Util_GetFirstDocumentItem(request, "Command");
  char* pParameter = Util_GetFirstDocumentItem(request, "Parameter");

  if( NULL == pCommand || 0 == strlen(pCommand) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "QAControl: Command is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( retVal == UPNP_E_SUCCESS )
  {
    if( NULL == pParameter || 0 == strlen(pParameter) )
    {
      bSuccess = SD_SetQAControl(pCommand, "");
    }
    else
    {
      bSuccess = SD_SetQAControl(pCommand, pParameter);
    }

    if( bSuccess )
    {
      SAFE_STRCPY(szResponse, "1");
    }
    else
    {
      SAFE_STRCPY(szResponse, "0");
    }

    // Create a UPnP response for QAControl
    if( UpnpAddToActionResponse( out, "QAControl",
              CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
              "Status", szResponse ) != UPNP_E_SUCCESS )
    {
      ( *out ) = NULL;
      ( *errorString ) = "Internal Error";
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "IdentifyDevice: Internal Error");
      retVal = UPNP_E_INTERNAL_ERROR;
    }
  }

  FreeXmlSource(pCommand);
  FreeXmlSource(pParameter);

  upnp_unlock();

  return retVal;
}


// Support the bulb OTA firmware update
static int EnableSSLOptions(CURL *curl)
{
  //TODO : Check the below code is needed when releasing final f/w image
  char certfile[64] = "/sbin/BuiltinObjectToken-GoDaddyClass2CA.crt";
  char certloc[32] = "/sbin";

  // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

  //char certfile[32] = "ca-certificates.crt";
  //char certloc[32] = "../sbin";

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_CERTINFO, 0L);
  curl_easy_setopt(curl, CURLOPT_CAINFO, certfile);
  curl_easy_setopt(curl, CURLOPT_CAPATH, certloc);

  return 0;
}

static bool isValidDownloadURL(char *pDownloadURL)
{
  CURL *curl;
  bool bValidURL = false;
  int resp = 0;
  long resp_code = 0;
  char *https = NULL;

  curl = curl_easy_init();
  if( !curl )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "isValidDownloadURL: curl initialize error");
    return false;
  }

  curl_easy_setopt(curl, CURLOPT_URL, pDownloadURL);
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

  https = strstr(pDownloadURL, "https://");

  if( https )
  {
    EnableSSLOptions(curl);
  }

  if( CURLE_OK == (resp = curl_easy_perform(curl)) )
  {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp_code);
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "isValidDownloadURL: resp_code = %d", resp_code);

    if( resp_code < 400 )
    {
      bValidURL = true;
    }
  }
  else
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "isValidDownloadURL: curl_easy_perform error = %d", resp);

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp_code);
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "isValidDownloadURL: resp_code = %d", resp_code);
  }

  curl_easy_cleanup(curl);

  return bValidURL;
}

int DownloadFirmware(char *pFirmwareLink)
{
  int rc = 0;

  if( !isValidDownloadURL(pFirmwareLink) )
  {
    rc = -1;
    return rc;
  }

  if( 0 != access("/tmp/Belkin_settings/ota-files/", F_OK) )
  {
    mkdir("/tmp/Belkin_settings/ota-files", 0777);
  }

  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] DownloadFirmware: url = %s", pFirmwareLink);

  if( 0 == access("/tmp/Belkin_settings/ota-files/fw.ota", F_OK) )
  {
    remove("/tmp/Belkin_settings/ota-files/fw.ota");
  }

  rc = webAppFileDownload(pFirmwareLink, "/tmp/Belkin_settings/ota-files/fw.ota");
  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] webAppFileDownload rc = (%d)...", rc);

  return rc;
}

void* fwUpdateThread(void *arg)
{
  int i = 0, nDevice = 0;
  int rc = 0, no_event_tick_cnt = 0, once = 0;
  int receive_event = 0, old_receive_event = 0;

  int name_len = 0, value_len = 0;
  char name[80] = {0,}, value[128] = {0,};
  char DeviceID[MAX_ID_LEN] = {0,};
  char StatusCode[10] = {0,};
  char* pSpace = NULL;

  char szDeviceIDs[MAX_DEVICES][MAX_ID_LEN];

  OTAFirmwareInfo fw_info;
  OTAFirmwareInfo* pfw_info = (OTAFirmwareInfo*)arg;

  SAFE_STRCPY(fw_info.szDeviceList, pfw_info->szDeviceList);
  SAFE_STRCPY(fw_info.szFirmwareLink, pfw_info->szFirmwareLink);

  fw_info.nUpgradePolicy = pfw_info->nUpgradePolicy;

  bfwUpgrading = true;

  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] fwUpdateThread is starting");

  memset(&szDeviceIDs, 0x00, sizeof(szDeviceIDs));

  //Extract device count from the device list parameter...
  nDevice = GetIDs(fw_info.szDeviceList, szDeviceIDs);

  //SetCurrSubDeviceFWUpdateState(OTA_FW_DOWNLOADING);

  for( i = 0; i < nDevice; i++ )
  {
    SetSubDeviceFWUpdateState(szDeviceIDs[i], OTA_FW_DOWNLOADING);

    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] DeviceID = %s firmware downloading status = %d",
               szDeviceIDs[i], GetSubDeviceFWUpdateState(szDeviceIDs[i]));

    SendBulbOTAEvent(FW_UPDATE_STATUS, szDeviceIDs[i],
                    GetSubDeviceFWUpdateState(szDeviceIDs[i]), SE_LOCAL + SE_REMOTE);
  }

  rc = DownloadFirmware(fw_info.szFirmwareLink);

  for( i = 0; i < nDevice; i++ )
  {
    if( rc )
    {
      //Downloading is failed...
      SetSubDeviceFWUpdateState(szDeviceIDs[i], OTA_FW_DOWNLOAD_FAIL);
    }
    else
    {
      //Downloading is success...
      SetSubDeviceFWUpdateState(szDeviceIDs[i], OTA_FW_DOWNLOAD_SUCCESS);
    }

    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] DeviceID = %s firmware downloading status = %d",
               szDeviceIDs[i], GetSubDeviceFWUpdateState(szDeviceIDs[i]));

    SendBulbOTAEvent(FW_UPDATE_STATUS, szDeviceIDs[i],
                    GetSubDeviceFWUpdateState(szDeviceIDs[i]), SE_LOCAL + SE_REMOTE);

  }

  if( rc )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] DownloadFirmware is failed...");
  }
  else
  {
    for( i = 0 ; i < nDevice ; i++ )
    {
      SD_DeviceID deviceID;
      MakeDeviceID(szDeviceIDs[i], &deviceID);

      sleep(1);

      if( false == SD_ReloadDeviceInfo(&deviceID) ||
          false == SD_SetDeviceOTA(&deviceID, fw_info.nUpgradePolicy) )
      {
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] UpgradeSubDeviceFirmware: SD_SetDeviceOTA is failed...");
        SetSubDeviceFWUpdateState(szDeviceIDs[i], OTA_FW_NOT_UPGRADE);
        SendBulbOTAEvent(FW_UPDATE_STATUS, szDeviceIDs[i],
                        GetSubDeviceFWUpdateState(szDeviceIDs[i]), SE_LOCAL + SE_REMOTE);
        continue;
      }

      while(1)
      {
        name_len = sizeof(name);
        value_len = sizeof(value);

        memset(name, 0x00, name_len);
        memset(value, 0x00, value_len);

        rc = wait_nonblock_sysevent(sysevent_fwupdate, tid_fwupdate, name, &name_len, value, &value_len);

        if( rc != 0 )
        {
          pluginUsleep(WAIT_500MS);

          no_event_tick_cnt++;

          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] fwUpdateThread: no_event_tick_cnt = %d, receive_event = %d",
                    no_event_tick_cnt, receive_event);

          if( no_event_tick_cnt >= 300 )
          {
            no_event_tick_cnt = 0;
            //no_event_tick_cnt(60) and receive_event(0) : if there is no receive a event from zigbee module until tick_cnt reaches out 60 times, it seems to be that f/w upgrade is failed...
            if( !receive_event )
            {
              DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] FW_UPDATE_STATUS: 6, no receive_event");
              SetSubDeviceFWUpdateState(szDeviceIDs[i], OTA_FW_NOT_UPGRADE);
              SendBulbOTAEvent(FW_UPDATE_STATUS, szDeviceIDs[i],
                              GetSubDeviceFWUpdateState(szDeviceIDs[i]), SE_LOCAL + SE_REMOTE);
              break;
            }
            else if( old_receive_event == receive_event )
            {
              DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] FW_UPDATE_STATUS: 6, no receive_event");
              SetSubDeviceFWUpdateState(szDeviceIDs[i], OTA_FW_NOT_UPGRADE);
              SendBulbOTAEvent(FW_UPDATE_STATUS, szDeviceIDs[i],
                              GetSubDeviceFWUpdateState(szDeviceIDs[i]), SE_LOCAL + SE_REMOTE);
              break;
            }
            else if( old_receive_event != receive_event )
            {
              DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] FW_UPDATE_STATUS: old_receive_event = %d, receive_event = %d",
                        old_receive_event, receive_event);
              old_receive_event = receive_event;
            }
          }
          else if(no_event_tick_cnt == 1)
          {
            old_receive_event = receive_event;
          }
          continue;
        }
        else
        {
          no_event_tick_cnt = 0;
          receive_event++;
        }

        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] fwUpdateThread: got event(%s:%d, %s:%d)",
                  name, name_len, value, value_len);

        /* Retrieve values of Value and ZB's eui64 */
        pSpace = strchr(value, 0x20);

        if( pSpace )
        {
          memset(DeviceID, 0x00, sizeof(DeviceID));
          memset(StatusCode, 0x00, sizeof(StatusCode));

          *pSpace = 0x00;
          SAFE_STRCPY(DeviceID, value);
          pSpace++;
          SAFE_STRCPY(StatusCode, pSpace);
        }

        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] fwUpdateThread: DeviceID = %s, StatusCode = %s", DeviceID, StatusCode);

        if( (0 == strcmp(name, "zb_ota_status") &&
            (0 == strcmp(StatusCode, "100") || 0 == strcmp(StatusCode, "200")) ) )
        {
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] SD_ReloadDeviceInfo(deviceID = %s)", deviceID.Data);
          pluginUsleep(WAIT_5SEC);
          for( once = 3; once; once-- )
          {
            if( SD_ReloadDeviceInfo(&deviceID) )
            {
              DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] SD_ReloadDeviceInfo is success...");
              break;
            }
            else
            {
              DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] SD_ReloadDeviceInfo is failed...");
              pluginUsleep(WAIT_1SEC);
            }
          }
          // receive_event++;
          SetSubDeviceFWUpdateState(szDeviceIDs[i], OTA_FW_UPGRADE_SUCCESS);
          SendBulbOTAEvent(FW_UPDATE_STATUS, szDeviceIDs[i],
                          GetSubDeviceFWUpdateState(szDeviceIDs[i]), SE_LOCAL + SE_REMOTE);
          break;
        }
        else if( 0 == strcmp(name, "zb_ota_status") && 0 == strcmp(StatusCode, "300") )
        {
          SetSubDeviceFWUpdateState(szDeviceIDs[i], OTA_FW_NOT_UPGRADE);
          SendBulbOTAEvent(FW_UPDATE_STATUS, szDeviceIDs[i],
                          GetSubDeviceFWUpdateState(szDeviceIDs[i]), SE_LOCAL + SE_REMOTE);
          break;
        }
        else if( 0 == strcmp(name, "zb_ota_percent") )
        {
          if( 0 == strcmp(StatusCode, "100") )
          {
            continue;
          }
          SetSubDeviceFWUpdateState(szDeviceIDs[i], OTA_FW_UPGRADING);
          SendBulbOTAEvent(FW_UPDATE_STATUS, szDeviceIDs[i],
                            GetSubDeviceFWUpdateState(szDeviceIDs[i]), SE_LOCAL + SE_REMOTE);
          SendBulbOTAEvent(FW_UPDATE_PROGRESS, szDeviceIDs[i], atoi(StatusCode), SE_LOCAL);
        }
      }
    }

    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] fwUpdateThread: exit");
  }

  SD_UnSetDeviceOTA();

  destroy_fwupdate_sysevent();
  bfwUpgrading = false;

  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] fwUpdateThread is finished");

  pthread_exit(NULL);
}


int createFWUpdateThread(char *pDeviceList, char *pFirmwareLink, int nUpgradePolicy)
{
  int rc = 0;
  static OTAFirmwareInfo fw_info;

  SAFE_STRCPY(fw_info.szDeviceList, pDeviceList);
  SAFE_STRCPY(fw_info.szFirmwareLink, pFirmwareLink);

  fw_info.nUpgradePolicy = nUpgradePolicy;

  rc = pthread_create(&fw_update_tid, NULL, fwUpdateThread, &fw_info);

  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] createFWUpdateThread rc=(%d)...", rc);

  if( 0 == rc )
  {
    pthread_detach(fw_update_tid);
  }

  return rc;
}

void SetSubDeviceFWUpdateState(char *pszDeviceID, int fwSubDeviceUpdate)
{
  int i = 0;

  for( i = 0; i < MAX_DEVICES; i++ )
  {
    if( strlen(sFWUpdateStatusTable[i].szDeviceID) && (0 == strcmp(sFWUpdateStatusTable[i].szDeviceID, pszDeviceID)) )
    {
      sFWUpdateStatusTable[i].nSubDeviceFWUpdateState = fwSubDeviceUpdate;
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] DeviceID = %d: %s firmware downloading status = %d", i, sFWUpdateStatusTable[i].szDeviceID, sFWUpdateStatusTable[i].nSubDeviceFWUpdateState);
      return;
    }
    else if( strlen(sFWUpdateStatusTable[i].szDeviceID) && (0 != strcmp(sFWUpdateStatusTable[i].szDeviceID, pszDeviceID)) )
    {
      continue;
    }
    else if( 0 == strlen(sFWUpdateStatusTable[i].szDeviceID) )
    {
      SAFE_STRCPY(sFWUpdateStatusTable[i].szDeviceID, pszDeviceID);
      sFWUpdateStatusTable[i].nSubDeviceFWUpdateState = fwSubDeviceUpdate;
      return;
    }
  }
}

int GetSubDeviceFWUpdateState(char *pszDeviceID)
{
  int i = 0;

  for( i = 0; i < MAX_DEVICES; i++ )
  {
    if( 0 == strcmp(sFWUpdateStatusTable[i].szDeviceID, pszDeviceID) )
    {
      return sFWUpdateStatusTable[i].nSubDeviceFWUpdateState;
    }
  }

  return OTA_FW_UPGRADE_SUCCESS;
}

void SetCurrSubDeviceFWUpdateState(int fwSubDeviceUpdate)
{
  sSubDeviceFWUpdateState = fwSubDeviceUpdate;
}

int GetCurrSubDeviceFWUpdateState()
{
  return sSubDeviceFWUpdateState;
}

int StartUpgradeSubDeviceFirmware(char *pDeviceList, char *pFirmwareLink)
{
  int rc = 0;
  int state = -1;

  state = getCurrFWUpdateState();
  if( (state == FM_STATUS_DOWNLOADING) ||
      (state == FM_STATUS_DOWNLOAD_SUCCESS) ||
      (state == FM_STATUS_UPDATE_STARTING) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] StartUpgradeSubDeviceFirmware: [FAILED] Link F/W is in upgrading!!!");
    rc = -1;
  }
  else if( false == bfwUpgrading )
  {
    create_fwupdate_sysevent();
    if( 0 != createFWUpdateThread(pDeviceList, pFirmwareLink, 0) )
    {
      DEBUG_LOG(ULOG_UPNP, UL_ERROR, "[OTA] createFWUpdateThread: Failed to create");
      rc = -1;
    }
  }
  else
  {
    DEBUG_LOG(ULOG_UPNP, UL_ERROR, "[OTA] UpgradeSubDeviceFirmware is already running...");
    rc = -1;
  }

  return rc;
}


int UpgradeSubDeviceFirmware(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out,
                              const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int retVal = UPNP_E_SUCCESS;
  int nUpgradePolicy = -1;
  char szResponse[10] = {0,};
  int state = -1;

  state = getCurrFWUpdateState();

  upnp_lock();

  char* pDeviceList = Util_GetFirstDocumentItem(request, "DeviceList");
  char* pFirmwareLink = Util_GetFirstDocumentItem(request, "FirmwareLink");
  char* pUpgradePolicy = Util_GetFirstDocumentItem(request, "UpgradePolicy");

  if( (state == FM_STATUS_DOWNLOADING) ||
      (state == FM_STATUS_DOWNLOAD_SUCCESS) ||
      (state == FM_STATUS_UPDATE_STARTING) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] UpgradeSubDeviceFirmware: [FAILED] Link F/W is in upgrading!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( NULL == pDeviceList || 0 == strlen(pDeviceList) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] UpgradeSubDeviceFirmware: pDeviceList is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( NULL == pFirmwareLink || 0 == strlen(pFirmwareLink) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] UpgradeSubDeviceFirmware: pFirmwareLink is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( NULL == pUpgradePolicy || 0 == strlen(pUpgradePolicy) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] UpgradeSubDeviceFirmware: pUpgradePolicy is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }
  else
  {
    if( (0 == strcmp(pUpgradePolicy, "Upgrade")) || (0 == strcmp(pUpgradePolicy, "0")) )
    {
      nUpgradePolicy = 0;
    }
    else if( (0 == strcmp(pUpgradePolicy, "Downgrade")) || (0 == strcmp(pUpgradePolicy, "1")) )
    {
      nUpgradePolicy = 1;
    }
    else if( (0 == strcmp(pUpgradePolicy, "Reinstall")) || (0 == strcmp(pUpgradePolicy, "2")) )
    {
      nUpgradePolicy = 2;
    }
    else
    {
      ( *errorString ) = "Invalid Argument";
      retVal = UPNP_E_INVALID_ARGUMENT;
    }
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] UpgradeSubDeviceFirmware: UpgradePolicy = %d", nUpgradePolicy);
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    if( false == bfwUpgrading )
    {
      SAFE_STRCPY(szResponse, "1");

      create_fwupdate_sysevent();
      if( 0 == createFWUpdateThread(pDeviceList, pFirmwareLink, nUpgradePolicy) )
      {
        SAFE_STRCPY(szResponse, "0");
      }
      else
      {
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] createFWUpdateThread: Failed to create");
      }
    }
    else
    {
      SAFE_STRCPY(szResponse, "4");
    }

    // Create a UPnP response for UpgradeSubDeviceFirmware
    if( UpnpAddToActionResponse( out, "UpgradeSubDeviceFirmware",
              CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
              "Status", szResponse ) != UPNP_E_SUCCESS )
    {
      ( *out ) = NULL;
      ( *errorString ) = "Internal Error";
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] UpgradeSubDeviceFirmware: Internal Error");
      retVal = UPNP_E_INTERNAL_ERROR;
    }
  }

  FreeXmlSource(pDeviceList);
  FreeXmlSource(pFirmwareLink);
  FreeXmlSource(pUpgradePolicy);

  upnp_unlock();

  return retVal;
}

int isSubdeviceFirmwareUpdating(void)
{
  DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "[OTA] isSubdeviceFirmwareUpdating = %s\n", ((false == bfwUpgrading)?"FALSE":"TRUE"));
  return ((false == bfwUpgrading)?0:1);
}

//SetZigbeeMode(ZR)
int SetZigbeeMode(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out,
                  const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int nError = 0;
  int nCurrZBMode = 0;
  int nZBChangeMode = 0;
  int retVal = UPNP_E_SUCCESS;

  char szResponse[10] = {0,};

  upnp_lock();

  char *pZigbeeMode = Util_GetFirstDocumentItem(request, "ZigbeeMode");

  if( NULL == pZigbeeMode || 0 == strlen(pZigbeeMode) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetZigbeeMode: pZigbeeMode is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }
  else
  {
    if( (0 == strcmp(pZigbeeMode, "ZR")) || (0 == strcmp(pZigbeeMode, "2")) )
    {
      nZBChangeMode = WEMO_LINK_ZR;
    }
    else
    {
      ( *errorString ) = "Invalid Argument";
      retVal = UPNP_E_INVALID_ARGUMENT;
    }
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetZigbeeMode: nZigbeeMode = %d", nZBChangeMode);
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    nCurrZBMode = SD_GetZBMode();

    if( (WEMO_LINK_ZR == nZBChangeMode) && (WEMO_LINK_PRISTINE_ZC == nCurrZBMode) )
    {
      //PRISTINE_ZC --> ZR
      //Call Join (ZB_CMD_NET_SCANJOIN) command
      SD_SetZBMode(WEMO_LINK_ZR);

      SD_LeaveNetwork();

      pluginUsleep(WAIT_500MS);

      if( !SD_ScanJoin() )
      {
        nError = 1;
      }
      else
      {
        nError = 0;
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "ScanJoin is Success...");
      }
    }
    else if( (WEMO_LINK_ZR == nZBChangeMode) && (WEMO_LINK_ZC == nCurrZBMode) )
    {
      //ZC --> ZR (It does not allow to change mode from ZC to ZR)
      //Not Supported
      nError = 2;
    }
    else if( (WEMO_LINK_ZR == nZBChangeMode) && (WEMO_LINK_ZR == nCurrZBMode) )
    {
      //ZR --> ZR
      nError = 0;
    }

    snprintf(szResponse, sizeof(szResponse), "%d", nError);

    // Create a UPnP response for SetZigbeeMode
    if( UpnpAddToActionResponse( out, "SetZigbeeMode",
              CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
              "Status", szResponse ) != UPNP_E_SUCCESS )
    {
      ( *out ) = NULL;
      ( *errorString ) = "Internal Error";
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetZigbeeMode: Internal Error");
      retVal = UPNP_E_INTERNAL_ERROR;
    }
  }

  FreeXmlSource(pZigbeeMode);

  upnp_unlock();

  return retVal;
}

int ScanZigbeeJoin(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int retVal = UPNP_E_SUCCESS;

  upnp_lock();

  char* pDevUDN = Util_GetFirstDocumentItem(request, "DevUDN");
  if( NULL == pDevUDN || 0 == strlen(pDevUDN) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "ScanZigbeeJoin: DevUDN is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    //Check if the device'udn is same.
    if( 0 != strcmp(pDevUDN, g_szUDN_1) )
    {
      ( *errorString ) = "Invalid Argument";
      retVal = UPNP_E_INVALID_ARGUMENT;
    }
    else
    {
      // Scan Zigbee Join...
      if( SD_ScanJoin() )
      {
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "ScanJoin is Success...");

        // create a response
        if( UpnpAddToActionResponse( out, "ScanZigbeeJoin",
                  CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                  NULL, NULL ) != UPNP_E_SUCCESS )
        {
          ( *out ) = NULL;
          ( *errorString ) = "Internal Error";
          DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "ScanZigbeeJoin: Internal Error");
          retVal = UPNP_E_INTERNAL_ERROR;
        }
      }
      else
      {
        ( *out ) = NULL;
        ( *errorString ) = "ScanZigbeeJoin Error";
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "ScanZigbeeJoin is Failed...");
        retVal = UPNP_E_SCANJOIN;
      }
    }
  }

  FreeXmlSource(pDevUDN);

  upnp_unlock();

  return retVal;
}

/**
 * @name     DataStoresExtractXMLTags
 *
 * @brief    Function which provides the address of the value of the requested
 *           XML tag
 *
 * @param    begin            Pointer to an XML Node. This is a parent node
 *                            and whose children have to be searched for the
 *                            requested tag name
 * @param    pKey             Pointer that holds the address of the name of an
 *                            XML tag to be searched for in the XML tree
 * @param    pDataStoreValue  Pointer that will point to the memory that houses
 *                            the tag's value when returned
 * @param    nKeyValBufLen    Length of buffer that holds the Key Value
 *
 * @return   UPNP status codes
 */

static int DataStoresExtractXMLTags(mxml_node_t *begin, char *pKey,
                                    char *pKeyValue, int nKeyValBufLen)
{
    mxml_node_t *node = NULL;

    char *qKeyValue = NULL;
    int retVal = UPNP_E_SUCCESS;

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Entering %s", __FUNCTION__);
    if( !begin || !pKey || !pKeyValue || (0 == nKeyValBufLen) )
    {
        DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "Invalid Input Arguments" );
        retVal = UPNP_E_INVALID_ARGUMENT;
    }

    if( UPNP_E_SUCCESS == retVal )
    {
        // Search the tree for the node whose name is string 'pKey'.
        node = mxmlFindElement(begin, begin, pKey, NULL, NULL, MXML_DESCEND );

        if( !node )
        {
            DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "Could not find element %s", pKey );
            retVal = UPNP_E_PARSING;
        }

        if( UPNP_E_SUCCESS == retVal )
        {
            qKeyValue = (char *)mxmlGetOpaque( node );

            if( NULL == qKeyValue )
            {
                DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "'qKeyValue' pointer is pointing to NULL.");
                retVal = UPNP_E_PARSING;
            }

            if( UPNP_E_SUCCESS == retVal )
            {
                // Copy the string to the argument variable. This is required since the
                // node will be deleted in this function itself and all memory will be lost
                strncpy( pKeyValue, qKeyValue, nKeyValBufLen );
                DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "%s = %s", pKey, pKeyValue );
            }
        }
    }

    if( node )
    {
        mxmlDelete( node );
    }

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Leaving %s", __FUNCTION__);
    return retVal;
}

/**
 * @name     SaveDataStores
 *
 * @brief    Function which scans for the multiple Data Stores that might
 *           be present in the request XML. It later updates the Data Stores
 *           values onto FW
 *
 * @param    pXMLBuff         Pointer to an XML. This usually points to a memory
 *                            where the entire XML is stored
 *
 * @return   UPNP status codes
 */
static int SaveDataStores(char *pXMLBuff)
{
    mxml_node_t *tree = NULL;
    mxml_node_t *node = NULL;
    mxml_node_t *temp = NULL;

    char *pDataStoreValue = NULL;
    char *pDataStoreName = NULL;
    char *qKeyValue      = NULL;

    int retVal = UPNP_E_SUCCESS;
    int nDataStore = 0;
    int nFileWSuccess = 0;

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Entering %s", __FUNCTION__);
    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "pXMLBuff = %s", pXMLBuff);

    tree = mxmlLoadString (NULL, pXMLBuff, MXML_OPAQUE_CALLBACK);
    if (NULL == tree)
    {
        DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "Value of 'tree' pointer is NULL.");
        retVal = UPNP_E_PARSING;
    }

    if (UPNP_E_SUCCESS == retVal)
    {
        // Allocating memory to store XML data corresponding to 'Name' tag.
        pDataStoreName = (char *) calloc (SIZE_50B, sizeof (char));

        if (NULL == pDataStoreName)
        {
            DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "Not enough heap memory to allocate. Calloc failed." );
            retVal = UPNP_E_OUTOF_MEMORY;
        }

        if (UPNP_E_SUCCESS == retVal)
        {
            // Allocating memory to store XML data corresponding to 'Value' tag.
            pDataStoreValue = (char *) calloc (SIZE_1024B, sizeof (char));

            if (NULL == pDataStoreValue)
            {
                DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "Not enough heap memory to allocate. Calloc failed." );
                retVal = UPNP_E_OUTOF_MEMORY;
            }
        }

        if (UPNP_E_SUCCESS == retVal)
        {
            // Look for multiple child nodes named 'DataStore'
            for (node = mxmlFindElement (tree, tree, "Name", NULL, NULL, MXML_DESCEND);
                 node != NULL;
                 node = mxmlFindElement (node, tree, "Name", NULL, NULL, MXML_DESCEND))
            {
                nDataStore++;

                memset (pDataStoreName, 0x00, SIZE_50B);
                memset (pDataStoreValue, 0x00, SIZE_1024B);

                /* Copy the name */
                qKeyValue = (char *) mxmlGetOpaque (node);
                if (NULL == qKeyValue)
                {
                    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "'qKeyValue' pointer is pointing to NULL.");
                    retVal = UPNP_E_PARSING;
                }
                else
                {
                    strncpy (pDataStoreName, qKeyValue, strlen (qKeyValue));
                    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Name = %s", pDataStoreName);
                }

                temp = mxmlFindElement (node, tree, "Value", NULL, NULL, MXML_DESCEND);
                if (NULL == temp)
                {
                    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "mxmlFindElement Error");
                    retVal = UPNP_E_PARSING;
                }
                else
                {
                    /* Copy the value */
                    qKeyValue = (char *) mxmlGetOpaque (temp);
                    if (NULL == qKeyValue)
                    {
                        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "'qKeyValue' pointer is pointing to NULL.");
                        retVal = UPNP_E_PARSING;
                    }
                    else
                    {
                        strncpy (pDataStoreValue, qKeyValue, strlen (qKeyValue));
                        DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "Value = %s", pDataStoreValue);
                    }
                }

                if (UPNP_E_SUCCESS == retVal)
                {
                    // Writing to the requested Data Store.
                    if (SD_SetDataStores (pDataStoreName, pDataStoreValue))
                    {
                        nFileWSuccess++;
                    }
                    else
                    {
                        // A file read error
                        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "DataStore %s could not be written to FW", pDataStoreName);
                    }
                }
            }
        }

        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "nDataStore = %d, nFileWSuccess = %d", nDataStore, nFileWSuccess);
        // This case occurs when any of the Data Stores could not be written onto FW
        if (nDataStore != nFileWSuccess)
        {
            retVal = UPNP_E_FILE_NOT_FOUND;
        }

        if (pDataStoreName)
        {
            free (pDataStoreName);
        }

        if (pDataStoreValue)
        {
            free (pDataStoreValue);
        }

        if (temp)
        {
            mxmlDelete (temp);
        }

        if (node)
        {
            mxmlDelete (node);
        }

        if (tree)
        {
            mxmlDelete (tree);
        }
    }

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Leaving %s", __FUNCTION__);

    return retVal;
}

/**
 *  @name      SetDataStores
 *
 *  @brief     Function which writes the Emergency Data Store values to a
 *             file stored in FW
 */

int SetDataStores(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
    ( *out ) = NULL;
    ( *errorString ) = NULL;

    char szResponse[SIZE_4B] = {0};
    int retVal = UPNP_E_SUCCESS;
    char *pXMLBuff = NULL;

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Entering %s", __FUNCTION__);

    upnp_lock();

    pXMLBuff = Util_GetFirstDocumentItem( request, "DataStores" );

    if( (NULL == pXMLBuff) || (0 == strlen( pXMLBuff )) )
    {
        DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "SetDataStores: DataStores does not exist" );
        ( *errorString ) = "Invalid Argument";
        retVal = UPNP_E_INVALID_ARGUMENT;
    }

    if( UPNP_E_SUCCESS == retVal )
    {
        retVal = SaveDataStores( pXMLBuff );

        switch( retVal )
        {
            case UPNP_E_SUCCESS:
                SAFE_STRCPY(szResponse, "0");
                break;

            case UPNP_E_PARSING:
                ( *errorString ) = "XML Parsing Error";
                SAFE_STRCPY(szResponse, "2");
                break;

            case UPNP_E_FILE_NOT_FOUND:
                ( *errorString ) = "Emergency Contacts List Write Error. Please check Firmware Logs";
                SAFE_STRCPY(szResponse, "1");
                break;

            case UPNP_E_INVALID_ARGUMENT:
                ( *errorString ) = "Invalid Arguments";

            default:
                SAFE_STRCPY(szResponse, "1");
                break;
        }

        // Create a UPnP response for SetDataStores
        if( UpnpAddToActionResponse( out, "SetDataStores",
                CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                "ErrorCode", szResponse ) != UPNP_E_SUCCESS )
        {
            ( *out ) = NULL;
            ( *errorString ) = "Internal Error";
            DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "SetDataStores: Internal Error");
            retVal = UPNP_E_INTERNAL_ERROR;
        }
    }

    upnp_unlock();

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Leaving %s", __FUNCTION__);

    return retVal;
}

static int retrieveFileSize (const char *pFileName, unsigned int *pFileSize)
{
    int            retVal       = UPNP_E_SUCCESS;
    int            fd           = -1;
    char           szFile[128]  = {0};

    struct         stat buf;

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Entering %s", __FUNCTION__);

    /* Input parameter validation */
    if ((NULL == pFileName) || (NULL == pFileSize))
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "GetDataStores: Invalid Arguments");
        retVal = UPNP_E_INVALID_ARGUMENT;
        goto CLEAN_RETURN;
    }

    memset (&buf, 0, sizeof (struct stat));
    if (strstr (pFileName, ".txt"))
    {
        snprintf (szFile, sizeof (szFile), "%s%s", DATA_STORE_PATH, pFileName);
    }
    else
    {
    snprintf (szFile, sizeof(szFile), "%s%s.txt", DATA_STORE_PATH, pFileName);
    }
    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "File Name = %s", szFile);

    fd = open (szFile, O_RDWR);
    if (-1 == fd)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Error opening file %s", pFileName);
        retVal = UPNP_E_PARSING;
        goto CLEAN_RETURN;
    }

    retVal = fstat (fd, &buf);
    if (0 != retVal)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "GetDataStores: fstat Error");
        retVal = UPNP_E_PARSING;
        goto CLEAN_RETURN;
    }

    *pFileSize = buf.st_size;
    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "File Size = %d", *pFileSize);

CLEAN_RETURN:
    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Leaving %s", __FUNCTION__);
    return retVal;
}

static int handleALLDataStoresRequest (char **pXMLResponse, char ***pALLDataStoresName,
                                      char ***pALLDataStoresValue, unsigned int *pnALLDataStores)
{
    unsigned int index = 0;
    unsigned int *pDataStoreNamesSize = NULL;
    DIR *dp = NULL;
    struct dirent *dptr = NULL;
    unsigned int *pDataStoresFileSize = NULL;
    int retVal = UPNP_E_SUCCESS;
    unsigned int sumDataStoresNamesSizes = 0;
    unsigned int sumDataStoresFilesSizes = 0;
    char szXMLBuff[SIZE_2048B] = {0};

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Entering %s", __FUNCTION__);
    if (NULL == pnALLDataStores)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "handleALLDataStoresRequest: Invalid Arguments");
        retVal = UPNP_E_INVALID_ARGUMENT;
        goto CLEAN_RETURN;
    }
    dp = opendir (DATA_STORE_PATH);
    if (NULL == dp)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Cannot open Data Store directory %s", DATA_STORE_PATH);
        goto CLEAN_RETURN;
    }

    // Computing the number of valid Data Store files
    while (NULL != (dptr = readdir (dp)))
    {
        if ((0 != strcmp (dptr -> d_name, ".")) && (0 != strcmp (dptr -> d_name, "..")))
        {
            (*pnALLDataStores)++;
        }
    }
    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "*pnALLDataStores = %d", *pnALLDataStores);

    // Allocating memory to hold the Data Store file name sizes
    pDataStoreNamesSize = (unsigned int *) calloc (*pnALLDataStores, sizeof (unsigned int));
    if (NULL == pDataStoreNamesSize)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "GetDataStores: Memory Allocation failed");
        retVal = UPNP_E_OUTOF_MEMORY;
        goto CLEAN_RETURN;
    }

    // Rewinding directory pointer
    rewinddir (dp);

    index = 0;

    // Filling the Data Store Names size array with respective values
    while (NULL != (dptr = readdir (dp)))
    {
        if ((0 != strcmp (dptr->d_name, ".")) && (0 != strcmp (dptr -> d_name, "..")))
        {
            pDataStoreNamesSize[index] = strlen (dptr->d_name);
            DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Size of the name of Data Store file %s is %d",
                       dptr -> d_name, pDataStoreNamesSize[index]);
            sumDataStoresNamesSizes += pDataStoreNamesSize[index];
            index++;
        }
    }

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Sum of the Data Stores Name sizes = %d", sumDataStoresNamesSizes);

    // Allocating memory for the array of pointers that will hold the Data Store names
    *pALLDataStoresName = (char **) calloc (*pnALLDataStores, sizeof (char *));
    if (NULL == *pALLDataStoresName)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "GetDataStores: Memory Allocation failed");
        retVal = UPNP_E_OUTOF_MEMORY;
        goto CLEAN_RETURN;
    }

    // Allocating memory to store the different Data Store Names
    for (index = 0; index < *pnALLDataStores; index++)
    {
        (*pALLDataStoresName)[index] = (char *) calloc (pDataStoreNamesSize[index] + 1, sizeof (char));

        if (NULL == (*pALLDataStoresName)[index])
        {
            DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "GetDataStores: Memory Allocation failed");
            retVal = UPNP_E_OUTOF_MEMORY;
            goto CLEAN_RETURN;
        }
    }

    // Rewinding directory pointer
    rewinddir (dp);

    index = 0;

    // Copying the Data Store Names onto data structures
    while (NULL != (dptr = readdir (dp)))
    {
        if ((0 != strcmp (dptr -> d_name, ".")) && (0 != strcmp (dptr -> d_name, "..")))
        {
            strncpy ((*pALLDataStoresName)[index], dptr -> d_name, strlen (dptr -> d_name));
            index++;
        }
    }

    // Allocating memory to hold the file sizes of the requested Data Store files
    pDataStoresFileSize = (unsigned int *) calloc (*pnALLDataStores, sizeof (unsigned int));
    if (NULL == pDataStoresFileSize)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Memory allocation Failed.");
        retVal = UPNP_E_OUTOF_MEMORY;
        goto CLEAN_RETURN;
    }

    for (index = 0; index < *pnALLDataStores; index++)
    {
        retVal = retrieveFileSize ((*pALLDataStoresName)[index], &pDataStoresFileSize[index]);
        if(UPNP_E_SUCCESS != retVal)
        {
            DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "GetDataStores: retrieveFileSize Error");
            goto CLEAN_RETURN;
        }

        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Size of Data Store '%s' is '%d'",
                   (*pALLDataStoresName)[index], pDataStoresFileSize[index]);

        sumDataStoresFilesSizes += pDataStoresFileSize[index];
    }

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Sum of the Data Stores File sizes = %d", sumDataStoresFilesSizes);

    // Allocating memory for the array of pointers that shall store Data Store file values
    *pALLDataStoresValue = (char **) calloc (*pnALLDataStores, sizeof (char *));
    if (NULL == *pALLDataStoresValue)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Memory allocation Failed.");
        retVal = UPNP_E_OUTOF_MEMORY;
        goto CLEAN_RETURN;
    }

    // Allocating memory to store individual Data Store File values
    // Reading the Data Store File values
    for (index = 0; index < *pnALLDataStores; index++)
    {
        (*pALLDataStoresValue)[index] = (char *) calloc (pDataStoresFileSize[index] + 1,
                                                         sizeof (char));

        if (NULL == (*pALLDataStoresValue)[index])
        {
           DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Memory allocation failed to store value of Data Store %s",
                      (*pALLDataStoresName)[index]);
           retVal = UPNP_E_OUTOF_MEMORY;
           goto CLEAN_RETURN;
        }

        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "(*pALLDataStoresValue)[%d] = 0x%x", index, (*pALLDataStoresValue)[index]);

        if (NULL == (SD_GetDataStores((*pALLDataStoresName)[index], (*pALLDataStoresValue)[index],
                                      pDataStoresFileSize[index])))
        {
            DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Reading contents of Data Store file '%s' failed",
                       (*pALLDataStoresName)[index]);
            retVal = UPNP_E_PARSING;
            goto CLEAN_RETURN;
        }
    }

    // Allocating memory to form the XML response
    *pXMLResponse = (char *) calloc (sumDataStoresNamesSizes + sumDataStoresFilesSizes + SIZE_2048B,
                                     sizeof (char));
    if (NULL == *pXMLResponse)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Memory allocation Failed for response.");
        retVal = UPNP_E_PARSING;
        goto CLEAN_RETURN;
    }

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "pXMLResponse = 0x%x", *pXMLResponse);

    // Forming the XML response for each Data Store
    for (index = 0; index < *pnALLDataStores; index++)
    {
        snprintf (szXMLBuff, pDataStoreNamesSize[index] + pDataStoresFileSize[index] + SIZE_256B,
                  "<DataStore><Name>%s</Name><Value>%s</Value></DataStore>",
                  (*pALLDataStoresName)[index], (*pALLDataStoresValue)[index]);

        strncat (*pXMLResponse, szXMLBuff, strlen (szXMLBuff));
    }
    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "pXMLResponse = %s", *pXMLResponse);

CLEAN_RETURN:

    if (pDataStoreNamesSize)
    {
        free (pDataStoreNamesSize);
        pDataStoreNamesSize = NULL;
    }

    if (pDataStoresFileSize)
    {
        free (pDataStoresFileSize);
        pDataStoresFileSize = NULL;
    }

    closedir (dp);

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Leaving %s", __FUNCTION__);
    return retVal;
}

static int prepareGetDataStoresResponse (char **pXMLResponse, unsigned int nDataStores,
                                         char **pDataStoreName, char ***pDataStoreValue)
{
    unsigned int *pFileSize = NULL;
    int retVal = UPNP_E_SUCCESS;
    unsigned int index = 0;
    unsigned int fileSize = 0;
    unsigned int nALLDataStores = 0;
    char szXMLBuff[SIZE_1024B] = {0};
    char **pALLDataStoresName = NULL;

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Entering %s", __FUNCTION__);

    if (NULL == pDataStoreName)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "GetDataStores: Invalid Arguments");
        retVal = UPNP_E_INVALID_ARGUMENT;
        goto CLEAN_RETURN;
    }

    if ((1 == nDataStores) && (0 == strcmp(pDataStoreName[index], "ALL")))
    {
        // This is the case where all the Data Store files in the directory
        // '/tmp/Belkin_settings/data_stores/' have to be retrieved.

        retVal = handleALLDataStoresRequest (pXMLResponse, &pALLDataStoresName, pDataStoreValue, &nALLDataStores);
        if (UPNP_E_SUCCESS != retVal)
        {
            DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "GetDataStores: handleALLDataStoresRequest Error");
            retVal = UPNP_E_PARSING;
        }
        goto CLEAN_RETURN;
    }

    // Allocating memory to hold the file sizes of the requested Data Store files
    pFileSize = (unsigned int *) calloc (nDataStores, sizeof (unsigned int));
    if (NULL == pFileSize)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Memory allocation Failed.");
        retVal = UPNP_E_OUTOF_MEMORY;
        goto CLEAN_RETURN;
    }
    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "pFileSize = 0x%x", pFileSize);

    // Preparing an array of file sizes of various Data Store files
    for (index = 0; index < nDataStores; index++)
    {
        retVal = retrieveFileSize (pDataStoreName[index], &fileSize);
        if(UPNP_E_SUCCESS != retVal)
        {
            DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "GetDataStores: retrieveFileSize Error");
            retVal = UPNP_E_PARSING;
            goto CLEAN_RETURN;
        }

        pFileSize[index] = fileSize;
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "File size of the DataStore number %d = %d", index, pFileSize[index]);
    }

    // Allocating memory for the array of pointers that shall store Data Store file values
    *pDataStoreValue = (char **) calloc (nDataStores, sizeof (char *));
    if (NULL == *pDataStoreValue)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Memory allocation Failed.");
        retVal = UPNP_E_OUTOF_MEMORY;
        goto CLEAN_RETURN;
    }
    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "pDataStoreValue = 0x%x", *pDataStoreValue);

    // Allocating memory to store individual Data Store File values
    // Reading the Data Store File values
    for (index = 0; index < nDataStores; index++)
    {
        (*pDataStoreValue)[index] = (char *) calloc (pFileSize[index] + 1, sizeof (char));
        if (NULL == (*pDataStoreValue)[index])
        {
           DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Memory allocation failed to store value of Data Store %d.", index);
           retVal = UPNP_E_OUTOF_MEMORY;
           goto CLEAN_RETURN;
        }
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "pDataStoreValue[%d] = 0x%x", index, (*pDataStoreValue)[index]);

        if (NULL == (SD_GetDataStores(pDataStoreName[index], (*pDataStoreValue)[index], pFileSize[index])))
        {
            DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Reading contents of Data Store file '%s' failed", pDataStoreName[index]);
            retVal = UPNP_E_PARSING;
            goto CLEAN_RETURN;
        }
    }

    // Allocating memory to form the XML response
    *pXMLResponse = (char *) calloc (SIZE_8192B, sizeof (char));     // To Do: Make the allocation size dynamic
    if (NULL == *pXMLResponse)
    {
            DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Memory allocation Failed for response.");
            retVal = UPNP_E_PARSING;
            goto CLEAN_RETURN;
    }
    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "pXMLResponse = 0x%x", *pXMLResponse);

    // Forming the XML response for each Data Store
    for (index = 0; index < nDataStores; index++)
    {
        snprintf (szXMLBuff, SIZE_1024B, "<DataStore><Name>%s</Name><Value>%s</Value></DataStore>",
                  pDataStoreName[index], (*pDataStoreValue)[index]);

        strncat (*pXMLResponse, szXMLBuff, strlen (szXMLBuff));
    }

CLEAN_RETURN:

    if (pFileSize)
    {
        free (pFileSize);
        pFileSize = NULL;
    }

    if (pALLDataStoresName)
    {
        for (index = 0; index < nALLDataStores; index++)
        {

            if (pALLDataStoresName[index])
            {
                free (pALLDataStoresName[index]);
                pALLDataStoresName[index] = NULL;
            }
        }

        free (pALLDataStoresName);
        pALLDataStoresName = NULL;
    }

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Leaving %s", __FUNCTION__);
    return retVal;
}

static int parseGetDataStoresXMLRequest (char *pXMLRequest, unsigned int *nDataStores, char ***pDataStoreName)
{
    int            retVal     = 0;
    unsigned int   index      = 0;

    char          *pVal       = NULL;

    mxml_node_t   *tree       = NULL;
    mxml_node_t   *node       = NULL;

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Entering %s", __FUNCTION__);

    /* Input parameter validation */
    if (NULL == nDataStores)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "GetDataStores: Invalid Arguments");
        retVal = UPNP_E_INVALID_ARGUMENT;
        goto CLEAN_RETURN;
    }

    // Load the string 'pXMLRequest' into an XML node tree
    tree = mxmlLoadString (NULL, pXMLRequest, MXML_OPAQUE_CALLBACK);
    if (NULL == tree)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "No <DataStores> node. Wrong XML passed.");
        retVal = UPNP_E_PARSING;
        goto CLEAN_RETURN;
    }

    /* Calculate the total number of records present in the request */
    for (node = mxmlFindElement (tree, tree, "Name", NULL, NULL, MXML_DESCEND);
         node != NULL;
         node = mxmlFindElement (node, tree, "Name", NULL, NULL, MXML_DESCEND))
    {
         (*nDataStores)++;
    }

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "nDataStores = %d", *nDataStores);

    /* Allocate memory for the total number of records present in the request.
     * NOTE: The calling function will de-allocate the memory.
     */
    *pDataStoreName = (char **) calloc (*nDataStores, sizeof (char *));//*
    if (NULL == *pDataStoreName) // *
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Memory allocation Failed.");
        retVal = UPNP_E_PARSING;
        goto CLEAN_RETURN;
    }

    /* Retrieve the names of the records present in the request.
     * Allocate memory for the same as required.
     * NOTE: The calling function will de-allocate the memory.
     */
    for (node = mxmlFindElement (tree, tree, "Name", NULL, NULL, MXML_DESCEND);
         node != NULL;
         node = mxmlFindElement (node, tree, "Name", NULL, NULL, MXML_DESCEND))
    {
        pVal = (char *) mxmlGetOpaque (node);
        if (NULL == pVal)
        {
            DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "'mxmlGetOpaque' returned NULL.");
            retVal = UPNP_E_PARSING;
            goto CLEAN_RETURN;
        }
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "pVal = %s", pVal);

        (*pDataStoreName)[index] = (char *) calloc (strlen (pVal) + 1, sizeof (char)); //
        if (NULL == (*pDataStoreName)[index])
        {
           DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Memory allocation Failed for each record.");
           retVal = UPNP_E_PARSING;
           goto CLEAN_RETURN;
        }

        strncpy ((*pDataStoreName)[index], pVal, strlen (pVal));
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "pDataStoreName[%d] = %s", index, (*pDataStoreName)[index]);
        index++;
    }

CLEAN_RETURN:
    if (node)
    {
        mxmlDelete (node);
    }

    if (tree)
    {
       mxmlDelete (tree);
    }

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Leaving %s", __FUNCTION__);
    return retVal;
}

int GetDataStores (pUPnPActionRequest   pActionRequest,
                   IXML_Document       *request,
                   IXML_Document      **out,
                   const char         **errorString)
{
    ( *out ) = NULL;
    ( *errorString ) = NULL;

    int            retVal          = UPNP_E_SUCCESS;

    unsigned int   index           = 0;
    unsigned int   nDataStores     = 0;

    char           aErrorResp[16]  = "2";

    char          *pXMLBuff        = NULL;
    char          *pXMLResp        = NULL;
    char         **pDataStoreName  = NULL;
    char         **pDataStoreValue = NULL;

    upnp_lock();

    pXMLBuff = Util_GetFirstDocumentItem( request, "DataStores" );

    if( (NULL == pXMLBuff) || (0 == strlen( pXMLBuff )) )
    {
        DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "GetDataStores: 'DataStores' tag does not exist" );
        ( *errorString ) = "Invalid Argument";
        retVal = UPNP_E_INVALID_ARGUMENT;
        goto CLEAN_RETURN;
    }

    retVal = parseGetDataStoresXMLRequest (pXMLBuff, &nDataStores, &pDataStoreName);
    if( UPNP_E_SUCCESS != retVal )
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "parseGetDataStoresXMLRequest Error");
        retVal = UPNP_E_PARSING;

        if( UpnpAddToActionResponse( out, "GetDataStores",
                            CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                            "ErrorCode", &aErrorResp) != UPNP_E_SUCCESS )
        {
            ( *out ) = NULL;
            ( *errorString ) = "Internal Error";
            DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "GetDataStores: Internal Error" );
            retVal = UPNP_E_INTERNAL_ERROR;
            goto CLEAN_RETURN;
        }

        goto CLEAN_RETURN;
    }

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "pDataStoreName = %p, pDataStoreName[%d] = %p, String pDataStoreName[%d] = %s", pDataStoreName, index, pDataStoreName[index], index, pDataStoreName[index]);

    retVal = prepareGetDataStoresResponse (&pXMLResp, nDataStores, pDataStoreName, &pDataStoreValue);
    if (UPNP_E_SUCCESS != retVal)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "prepareGetDataStoresResponse Error");
        retVal = UPNP_E_PARSING;

        if( UpnpAddToActionResponse( out, "GetDataStores",
                            CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                            "ErrorCode", &aErrorResp) != UPNP_E_SUCCESS )
        {
            DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "Not enough heap memory to allocate. Calloc failed." );
            retVal = UPNP_E_OUTOF_MEMORY;
        }

        goto CLEAN_RETURN;
    }

    if (UPNP_E_SUCCESS == retVal)
    {
        // Creating a UPnP success response for GetDataStores
        if( UpnpAddToActionResponse( out, "GetDataStores",
                                CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                                "DataStores", pXMLResp) != UPNP_E_SUCCESS )
        {
            ( *out ) = NULL;
            ( *errorString ) = "Internal Error";
            DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "GetDataStores: Internal Error" );
            retVal = UPNP_E_INTERNAL_ERROR;
            goto CLEAN_RETURN;
        }
    }

CLEAN_RETURN:
    if (pXMLResp)
    {
        free (pXMLResp);
        pXMLResp = NULL;
    }

    if (pDataStoreName)
    {
        for (index = 0; index < nDataStores; index++)
        {
            if (pDataStoreName[index])
            {
                free (pDataStoreName[index]);
                pDataStoreName[index] = NULL;
            }
        }

        free (pDataStoreName);
        pDataStoreName = NULL;
    }

    if (pDataStoreValue)
    {
        for (index = 0; index < nDataStores; index++)
        {

            if (pDataStoreValue[index])
            {
                free (pDataStoreValue[index]);
                pDataStoreValue[index] = NULL;
            }
        }

        free (pDataStoreValue);
        pDataStoreValue = NULL;
    }

    upnp_unlock();

    return retVal;
}

int DeleteDataStores (pUPnPActionRequest   pActionRequest,
                      IXML_Document       *request,
                      IXML_Document      **out,
                      const char         **errorString)
{
    (*out) = NULL;
    (*errorString) = NULL;

    int             retVal          = UPNP_E_SUCCESS;

    unsigned int    index           = 0;
    unsigned int    nDataStores     = 0;

    char            aXMLResp[4]     = "0";
    char            aErrorResp[4]   = "3";
    char            szFile[256]     = {0};

    char           *pXMLBuff        = NULL;
    char          **pDataStoreName  = NULL;

    DIR            *dp              = NULL;
    struct dirent  *dptr            = NULL;
    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Entering %s", __FUNCTION__);

    upnp_lock();
    pXMLBuff = Util_GetFirstDocumentItem (request, "DataStores");
    if ((NULL == pXMLBuff) || (0 == strlen (pXMLBuff)))
    {
        DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "DeleteDataStores: 'DataStores' tag does not exist" );
        retVal = UPNP_E_INVALID_ARGUMENT;
        goto CLEAN_RETURN;
    }

    retVal = parseGetDataStoresXMLRequest (pXMLBuff, &nDataStores, &pDataStoreName);
    if (UPNP_E_SUCCESS != retVal)
    {
        DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "parseGetDataStoresXMLRequest Error");
        retVal = UPNP_E_PARSING;
        goto CLEAN_RETURN;
    }

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "nDataStores = %d", nDataStores);

    for (index = 0; index < nDataStores; index++)
    {
        if (0 == strcmp (pDataStoreName[index], "ALL"))
        {
            /* Delete all files in the directory */
            dp = opendir (DATA_STORE_PATH);
            if (NULL == dp)
            {
                DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "DeleteDataStores: opendir() Error" );
                retVal = UPNP_E_INTERNAL_ERROR;
                goto CLEAN_RETURN;
            }

            while (dptr = readdir (dp))
            {
                if ((0 != strcmp (dptr->d_name, ".")) && (0 != strcmp (dptr->d_name, "..")))
                {
                    snprintf (szFile, sizeof (szFile), "%s%s", DATA_STORE_PATH, dptr->d_name);
                    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "File Name to delete = %s", szFile);
                    remove (szFile);
                }
            }

            retVal = rmdir (DATA_STORE_PATH);
            if (0 != retVal)
            {
                DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "DeleteDataStores: rmdir() Error" );
                retVal = UPNP_E_INTERNAL_ERROR;
                goto CLEAN_RETURN;
            }
        }
        else
        {
            /* Delete the specific file in the directory */
            snprintf (szFile, sizeof (szFile), "%s%s.txt", DATA_STORE_PATH, pDataStoreName[index]);
            DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "File Name to delete = %s", szFile);

            retVal = remove (szFile);
            if (0 != retVal)
            {
                DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "DeleteDataStores: remove() Error" );
                retVal = UPNP_E_INTERNAL_ERROR;
                goto CLEAN_RETURN;
            }
        }
    }

    if (UPNP_E_SUCCESS == retVal)
    {
        if( UpnpAddToActionResponse( out, "DeleteDataStores",
                                CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                                "ErrorCode", &aXMLResp) != UPNP_E_SUCCESS )
        {
            ( *out ) = NULL;
            ( *errorString ) = "Internal Error";
            DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "DeleteDataStores: Internal Error" );
            retVal = UPNP_E_INTERNAL_ERROR;
            goto CLEAN_RETURN;
        }
    }
CLEAN_RETURN:
    if (dp)
    {
        closedir (dp);
        dp = NULL;
    }


    if (pDataStoreName)
    {
        for (index = 0; index < nDataStores; index++)
        {
            if (pDataStoreName[index])
            {
                free (pDataStoreName[index]);
                pDataStoreName[index] = NULL;
            }
        }

        free (pDataStoreName);
        pDataStoreName = NULL;
    }

    if (UPNP_E_SUCCESS != retVal)
    {
        if (UpnpAddToActionResponse (out, "DeleteDataStores",
                            CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                            "ErrorCode", &aErrorResp) != UPNP_E_SUCCESS )
        {
            ( *out ) = NULL;
            ( *errorString ) = "Internal Error";
            DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "GetDataStores: Internal Error" );
            retVal = UPNP_E_INTERNAL_ERROR;
        }
    }

    DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "Leaving %s", __FUNCTION__);
    upnp_unlock();

    return retVal;
}

void* CloseServiceThread(void *arg)
{
  ControlleeDeviceStop();
  StopPluginCtrlPoint();

  ExitInetMonitorThread();
  ExitCheckInetConnectThread();

  sleep(5);

  trigger_remote_event(WORK_EVENT, QUIT_THREAD, "worker", "quit", NULL, 0);

  pthread_exit(NULL);
}

int CloseZigbeeRouterSetup(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
  ( *out ) = NULL;
  ( *errorString ) = NULL;

  int rc = 0;
  int retVal = UPNP_E_SUCCESS;

  upnp_lock();

  char* pDevUDN = Util_GetFirstDocumentItem(request, "DevUDN");
  if( NULL == pDevUDN || 0 == strlen(pDevUDN) )
  {
    DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "CloseZigbeeRouterSetup: DevUDN is not existed!!!");
    ( *errorString ) = "Invalid Argument";
    retVal = UPNP_E_INVALID_ARGUMENT;
  }

  if( UPNP_E_SUCCESS == retVal )
  {
    //Check if the device'udn is same.
    if( 0 != strcmp(pDevUDN, g_szUDN_1) )
    {
      ( *errorString ) = "Invalid Argument";
      retVal = UPNP_E_INVALID_ARGUMENT;
    }
    else
    {
      rc = pthread_create(&close_service_tid, NULL, CloseServiceThread, NULL);
      DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "Create CloseServiceThread rc = %d", rc);

      if( 0 == rc )
      {
        pthread_detach(close_service_tid);
      }

      // create a response
      if( UpnpAddToActionResponse( out, "CloseZigbeeRouterSetup",
                CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE],
                NULL, NULL ) != UPNP_E_SUCCESS )
      {
        ( *out ) = NULL;
        ( *errorString ) = "Internal Error";
        DEBUG_LOG(ULOG_UPNP, UL_DEBUG, "CloseZigbeeRouterSetup: Internal Error");
        retVal = UPNP_E_INTERNAL_ERROR;
      }
    }
  }

  FreeXmlSource(pDevUDN);

  upnp_unlock();

  return retVal;
}

#define PRESET_DB_URL				"/tmp/Belkin_settings/Preset.db"
#define MAX_PRESET_INFO_COLUMN			4
#define MAX_PRESET_VERSION_INFO_COLUMN		3
#define MAX_PRESET_TAGS_IN_XML			6

void deInitDevicePreset(int isDeleteDB)
{
	/*close preset DB*/
	DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - close preset DB!", __FUNCTION__);
	CloseDB(s_PresetDB);

	/*check if DB needs to be deleted*/
	if(isDeleteDB)
	{
		/*delete preset DB*/
		DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - delete preset DB on hard reset!", __FUNCTION__);
		DeleteDB(PRESET_DB_URL);
	}

	/*destroy preset mutex lock*/
	destroy_preset_lock();
}

void initDevicePreset(void)
{
	struct stat FileInfo;

	/*init preset mutex lock*/
	init_preset_lock();

	/*Table property for storing all preset info corresponding to a unique device ID or group ID*/
	TableDetails DevicePresetInfo[MAX_PRESET_INFO_COLUMN] =
	{{"id","string"},{"name","string"},{"value","string"},{"type","string"}};

	/*Table property for storing preset version info corresponding to a unique device ID or group ID*/
	TableDetails DevicePresetVersionInfo[MAX_PRESET_VERSION_INFO_COLUMN] = {{"isGroupID","string"},{"id","string"},{"version","double"}};

	/*check if Preset DB file already exists*/
	if(stat(PRESET_DB_URL, &FileInfo) != -1)
	{
		DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - Preset DB already Exists!", __FUNCTION__);
		/*init Preset DB*/
		if(InitDB(PRESET_DB_URL,&s_PresetDB))
			goto INIT_FAIL;
		DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - Preset DB Init done!", __FUNCTION__);
		return;
	}
	else
	{
		/*init Preset DB*/
		if(!InitDB(PRESET_DB_URL,&s_PresetDB))
		{
			/*create Preset info table*/
			if(WeMoDBCreateTable(&s_PresetDB, "DEVICEPRESETINFO", DevicePresetInfo, 0, MAX_PRESET_INFO_COLUMN))
				goto INIT_FAIL;
			/*create Preset version info tables*/
			if(WeMoDBCreateTable(&s_PresetDB, "DEVICEPRESETVERSIONINFO", DevicePresetVersionInfo, 0, MAX_PRESET_VERSION_INFO_COLUMN))
				goto INIT_FAIL;
			DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - Preset DB CREATED, Init done!", __FUNCTION__);
			return;
		}
		else
			goto INIT_FAIL;
	}

INIT_FAIL:
	DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - Exiting on Preset DB Init Fail!", __FUNCTION__);
}

double getPresetVersionFromDB(char *ppresetID)
{
	int rows=0,columns=0;
	char **ppsDataArray=NULL;
	char query[SIZE_256B];
	double currentPresetVersion = -1;

	memset(query, 0, sizeof(query));
	/*create a query to fetch current preset version of a particular preset ID*/
	snprintf(query, sizeof(query), "SELECT version FROM DEVICEPRESETVERSIONINFO WHERE id=\"%s\";", ppresetID);

	/*fetch query data*/
	if(!WeMoDBGetTableData(&s_PresetDB, query, &ppsDataArray,&rows,&columns))
	{
		/*check if we have some data or not.*/
		if(rows && columns)
		{
			/*Version found corresponding to given preset ID. validate version*/
			DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - Fetched %d rows, %d columns!", __FUNCTION__,rows, columns);
			currentPresetVersion = strtoul(ppsDataArray[1],NULL,0);
		}
		else
		{
			/*no entry found, that means it will be a new entry in a preset table*/
			DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - No entry found!", __FUNCTION__);
			currentPresetVersion = 0;
		}
	}

	DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - currentPresetVersion:%lf", __FUNCTION__,currentPresetVersion);
	WeMoDBTableFreeResult(&ppsDataArray,&rows,&columns);
	return currentPresetVersion;
}

int processAndSetDevicePreset(char *pPresetXMLBuff)
{
	int retVal = UPNP_E_SUCCESS;
	double newPresetVersion = 0;
	double currentPresetVersion = 0;
	char condition[SIZE_64B]={0};

	/*char array, hold various preset xml tag values*/
	char presetVersion[SIZE_32B]={0},isPresetGroupID[SIZE_32B]={0},presetID[SIZE_32B]={0},presetName[SIZE_32B]={0},presetValue[SIZE_32B]={0},presetType[SIZE_32B]={0};
	/*char array, hold various column values in preset db*/
	char version[SIZE_32B]={0},isGroupID[SIZE_32B]={0},id[SIZE_32B]={0},name[SIZE_32B]={0},value[SIZE_32B]={0},type[SIZE_32B]={0};

	/*array of const char pointers to hold preset xml tag names*/
	char *pPresetParamSet[MAX_PRESET_TAGS_IN_XML+1] = {"presetList", "version", "isGroupID", "id", "name", "value", "type"};

	/*array of char pointers to hold preset xml tag values*/
	char *pPresetValueSet[MAX_PRESET_TAGS_IN_XML] = {presetVersion, isPresetGroupID, presetID, presetName, presetValue, presetType};

	/*array of struct to hold preset row column values in preset db*/
	ColDetails presetColInfo[] = {{"id",id},{"name",name},{"value",value},{"type",type}};

	/*array of struct to hold version row column values in preset db*/
	ColDetails presetVersionColInfo[] = {{"isGroupID",isGroupID},{"id",id},{"version",version}};

	DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "processAndSetDevicePreset: XML received is:%s", pPresetXMLBuff);

	/*parse preset xml data and fill all preset palace holder array*/
	retVal = parseXMLData(pPresetXMLBuff, pPresetParamSet, pPresetValueSet, MAX_PRESET_TAGS_IN_XML);
	if(UPNP_E_SUCCESS != retVal)
	{
		DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "parseDevicePresetDataXMLRequest Error");
		retVal = UPNP_E_PARSING;
		goto CLEAN_RETURN;
	}

	/*get current verion of this preset ID from version DB*/
	currentPresetVersion = getPresetVersionFromDB(presetID);

	/*New preset version corresponding to reset ID*/
	newPresetVersion = strtoul(presetVersion,NULL,0);

	/*validate version, New version should always be greater than current version*/
	if(currentPresetVersion >= newPresetVersion)
	{
		DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "setDevicePreset: %s:Invalid preset Version, Not setting Preset!", presetVersion);
		retVal = FAILURE;
		goto CLEAN_RETURN;
	}

	/*fill column values*/
	snprintf(version, sizeof(version), "%s", presetVersion);
	snprintf(isGroupID, sizeof(isGroupID), "'%s'", isPresetGroupID);
	snprintf(id, sizeof(id), "'%s'", presetID);
	snprintf(name, sizeof(name), "'%s'", presetName);
	snprintf(value, sizeof(value), "'%s'", presetValue);
	snprintf(type, sizeof(type), "'%s'", presetType);

	/*insert new preset row in preset db*/
	retVal = WeMoDBInsertInTable(&s_PresetDB, "DEVICEPRESETINFO", presetColInfo, MAX_PRESET_INFO_COLUMN);

	/*check if new entry of update*/
	if(currentPresetVersion)
	{
		/*update version*/
		snprintf(condition, sizeof(condition), "id=\"%s\"", presetID);
		retVal = WeMoDBUpdateTable(&s_PresetDB, "DEVICEPRESETVERSIONINFO", presetVersionColInfo, MAX_PRESET_VERSION_INFO_COLUMN, condition);
	}
	else
		/*new entry*/
		retVal = WeMoDBInsertInTable(&s_PresetDB, "DEVICEPRESETVERSIONINFO", presetVersionColInfo, MAX_PRESET_VERSION_INFO_COLUMN);

CLEAN_RETURN:
	return retVal;
}

int setDevicePreset(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	char *aErrorResp = NULL;
	int retVal = UPNP_E_SUCCESS;
	char *pPresetXMLBuff = NULL;

	*out = NULL;
	*errorString = NULL;

	/*get preset xml data from request*/
	pPresetXMLBuff = Util_GetFirstDocumentItem (request, "devicePresetData");
	if((NULL == pPresetXMLBuff) || (0 == strlen (pPresetXMLBuff)))
	{
		DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "setDevicePreset: 'devicePresetData' tag does not exist" );
		retVal = UPNP_E_INVALID_ARGUMENT;
		goto CLEAN_RETURN;
	}

	/*parse preset xml data, validate it and insert in preset DB*/
	retVal = processAndSetDevicePreset(pPresetXMLBuff);

CLEAN_RETURN:

	/*set response strings*/
	if(UPNP_E_SUCCESS != retVal)
		aErrorResp = "Error";
	else
	{
		/*Update Cloud*/
		CreateUpdatePresetDataThread(pPresetXMLBuff, POST, "preset");
		aErrorResp = "Success";
	}

	/*send UPnP response*/
	if(UPNP_E_SUCCESS != UpnpAddToActionResponse(out, "setDevicePreset", CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE], "responseStatus", aErrorResp))
	{
		*errorString = "Error";
		DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "setDevicePreset: Internal Error" );
		retVal = UPNP_E_INTERNAL_ERROR;
	}
	return retVal;
}

int createPresetRespXML(char *isGroupID, char *presetID, double currentPresetVersion, char **pPresetRespXMLBuff)
{
	int retVal = UPNP_E_SUCCESS;
	int rows=0,columns=0,arraySize=0;
	char query[SIZE_256B];
	char **ppsDataArray=NULL;
	int respXMLLen = 0;
	int i=0;

	memset(query, 0, sizeof(query));
	/*create a query to fetch preset info of a particular preset ID*/
	snprintf(query, sizeof(query), "SELECT name,value,type FROM DEVICEPRESETINFO WHERE id=\"%s\";", presetID);

	/*fetch query data*/
	if(!WeMoDBGetTableData(&s_PresetDB, query, &ppsDataArray,&rows,&columns))
	{
		/*check if we have some data or not.*/
		if(rows && columns)
		{
			arraySize = (rows + 1)*columns;
			/*Version found corresponding to given preset ID. validate version*/
			DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - Fetched %d rows, %d columns, %d arraySize", __FUNCTION__,rows, columns, arraySize);
		}
		else
		{
			/*no entry found*/
			DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - No entry found!", __FUNCTION__);
			WeMoDBTableFreeResult(&ppsDataArray,&rows,&columns);
		}
	}

	/*calculate respXML size, 265 bytes for fixed xml tags + preset data*/
	respXMLLen = SIZE_256B + SIZE_64B*arraySize;

	*pPresetRespXMLBuff = calloc(1, respXMLLen);
	if(NULL == *pPresetRespXMLBuff)
	{
		DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - Calloc failed!", __FUNCTION__);
		return FAILURE;
	}

	snprintf(*pPresetRespXMLBuff, respXMLLen, "%s", "<devicePreset>\n");

	snprintf((*pPresetRespXMLBuff)+strlen(*pPresetRespXMLBuff), respXMLLen-strlen(*pPresetRespXMLBuff), "<version>%.0f</version>\n", currentPresetVersion);
	snprintf((*pPresetRespXMLBuff)+strlen(*pPresetRespXMLBuff), respXMLLen-strlen(*pPresetRespXMLBuff), "<isGroupID>%s</isGroupID>\n", isGroupID);
	snprintf((*pPresetRespXMLBuff)+strlen(*pPresetRespXMLBuff), respXMLLen-strlen(*pPresetRespXMLBuff), "<id>%s</id>\n", presetID);

	snprintf((*pPresetRespXMLBuff)+strlen(*pPresetRespXMLBuff), respXMLLen-strlen(*pPresetRespXMLBuff), "%s", "<presets>\n");
	for(i = columns; i < arraySize; i+= columns)
	{
		snprintf((*pPresetRespXMLBuff)+strlen(*pPresetRespXMLBuff), respXMLLen-strlen(*pPresetRespXMLBuff), "%s", "<preset>\n");

		snprintf((*pPresetRespXMLBuff)+strlen(*pPresetRespXMLBuff), respXMLLen-strlen(*pPresetRespXMLBuff), "<name>%s</name>\n", ppsDataArray[i]);
		snprintf((*pPresetRespXMLBuff)+strlen(*pPresetRespXMLBuff), respXMLLen-strlen(*pPresetRespXMLBuff), "<value>%s</value>\n", ppsDataArray[i+1]);
		snprintf((*pPresetRespXMLBuff)+strlen(*pPresetRespXMLBuff), respXMLLen-strlen(*pPresetRespXMLBuff), "<type>%s</type>\n", ppsDataArray[i+2]);

		snprintf((*pPresetRespXMLBuff)+strlen(*pPresetRespXMLBuff), respXMLLen-strlen(*pPresetRespXMLBuff), "%s", "</preset>\n");
	}

	snprintf((*pPresetRespXMLBuff)+strlen(*pPresetRespXMLBuff), respXMLLen-strlen(*pPresetRespXMLBuff), "%s", "</presets>\n");

	snprintf((*pPresetRespXMLBuff)+strlen(*pPresetRespXMLBuff), respXMLLen-strlen(*pPresetRespXMLBuff), "%s", "</devicePreset>\n");

	WeMoDBTableFreeResult(&ppsDataArray,&rows,&columns);

	DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "createPresetRespXML: pPresetRespXMLBuff:%s", *pPresetRespXMLBuff);
	return retVal;
}

int processAndGetDevicePreset(char *pPresetXMLBuff, char **pPresetRespXMLBuff)
{
	int retVal = UPNP_E_SUCCESS;
	double currentPresetVersion = -1;
	char *pPresetRespXMLTempBuff = NULL;
	int respXMLLen = 0;

	/*char array hold various preset xml tag values*/
	char isPresetGroupID[SIZE_32B]={0},presetID[SIZE_32B]={0};

	/*array of const char pointers to hold preset xml tag names*/
	char *pPresetParamSet[3] = {"presetList", "isGroupID", "id"};

	/*array of char pointers to hold preset xml tag values*/
	char *pPresetValueSet[2] = {isPresetGroupID, presetID};

	DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "processAndGetDevicePreset: XML received is:%s", pPresetXMLBuff);

	/*parse preset xml data and fill all preset palace holder array*/
	retVal = parseXMLData(pPresetXMLBuff, pPresetParamSet, pPresetValueSet, 2);
	if(UPNP_E_SUCCESS != retVal)
	{
		DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "parseDevicePresetDataXMLRequest Error");
		retVal = UPNP_E_PARSING;
		goto CLEAN_RETURN;
	}

	/*get version of this preset ID from version DB*/
	currentPresetVersion = getPresetVersionFromDB(presetID);

	/*get preset data corresponding to this presetID from preset DB and create preset response xml*/
	retVal = createPresetRespXML(isPresetGroupID, presetID, currentPresetVersion, &pPresetRespXMLTempBuff);
	if((UPNP_E_SUCCESS != retVal) || (NULL == pPresetRespXMLTempBuff))
		goto CLEAN_RETURN;

	/*Add presetList tag*/
	respXMLLen = strlen(pPresetRespXMLTempBuff)+SIZE_64B;
	*pPresetRespXMLBuff = calloc(1, respXMLLen);
	if(NULL == *pPresetRespXMLBuff)
	{
		DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - Calloc failed!", __FUNCTION__);
		retVal = FAILURE;
	}
	else
	{
		snprintf(*pPresetRespXMLBuff, respXMLLen, "%s", "<presetList>\n");
		snprintf((*pPresetRespXMLBuff)+strlen(*pPresetRespXMLBuff), respXMLLen-strlen(*pPresetRespXMLBuff), "%s", pPresetRespXMLTempBuff);
		snprintf((*pPresetRespXMLBuff)+strlen(*pPresetRespXMLBuff), respXMLLen-strlen(*pPresetRespXMLBuff), "%s", "</presetList>\n");

		DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "processAndGetDevicePreset: final pPresetRespXMLBuff:%s", *pPresetRespXMLBuff);
	}

CLEAN_RETURN:
	free(pPresetRespXMLTempBuff);pPresetRespXMLTempBuff=NULL;
	return retVal;
}

int getDevicePreset(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	char *aErrorResp = NULL;
	int retVal = UPNP_E_SUCCESS;
	char *pPresetXMLBuff = NULL;
	char *pPresetRespXMLBuff = NULL;

	*out = NULL;
	*errorString = NULL;

	/*get preset xml data from request*/
	pPresetXMLBuff = Util_GetFirstDocumentItem (request, "devicePresetData");
	if((NULL == pPresetXMLBuff) || (0 == strlen (pPresetXMLBuff)))
	{
		DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "getDevicePreset: 'devicePresetData' tag does not exist" );
		retVal = UPNP_E_INVALID_ARGUMENT;
		goto CLEAN_RETURN;
	}

	/*parse preset xml data, validate it and create preset response xml*/
	retVal = processAndGetDevicePreset(pPresetXMLBuff, &pPresetRespXMLBuff);

CLEAN_RETURN:

	/*get response strings*/
	if(UPNP_E_SUCCESS != retVal)
		aErrorResp = "Error";
	else
		aErrorResp = pPresetRespXMLBuff;

	/*send UPnP response*/
	if(UPNP_E_SUCCESS != UpnpAddToActionResponse(out, "getDevicePreset", CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE], "devicePresetData", aErrorResp))
	{
		*errorString = "Error";
		DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "getDevicePreset: Internal Error" );
		retVal = UPNP_E_INTERNAL_ERROR;
	}

	/*free response xml buffer*/
	if(pPresetRespXMLBuff)free(pPresetRespXMLBuff);pPresetRespXMLBuff=NULL;

	return retVal;
}

int getHomePreset(char **pHomePresetRespXMLBuff)
{
	int retVal = UPNP_E_SUCCESS;
	int rows=0,columns=0,arraySize=0;
	int i,respXMLLen = 0;
	double presetVersion = -1;
	char *pPresetRespXMLBuff = NULL;
	char **ppsDataArray=NULL;
	char query[SIZE_256B];

	/*create a query to fetch all data from preset version table.*/
	memset(query, 0, sizeof(query));
	snprintf(query, sizeof(query), "SELECT * FROM DEVICEPRESETVERSIONINFO;");

	/*fetch query data*/
	if(!WeMoDBGetTableData(&s_PresetDB, query, &ppsDataArray,&rows,&columns))
	{
		/*check if we have some data or not.*/
		if(rows && columns)
		{
			/*Version found corresponding to given preset ID. validate version*/
			arraySize = (rows + 1)*columns;
			DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - Fetched %d rows, %d columns!", __FUNCTION__,rows, columns);
		}
		else
		{
			/*no entry found, that means it will be a new entry in a preset table*/
			DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - No entry found!", __FUNCTION__);
			retVal = FAILURE;
			goto CLEAN_RETURN;
		}
	}

	/*taking 64 bytes of buffer data to store start/end presetList tag data,
	  respXMLLen will increase whenever a new devicePreset set is added into the main xml*/
	respXMLLen = SIZE_64B;

	/*allocate memory for start/end presetList tag*/
	*pHomePresetRespXMLBuff = calloc(1, respXMLLen);
	if(NULL == *pHomePresetRespXMLBuff)
	{
		DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "%s - Calloc failed!", __FUNCTION__);
		retVal = FAILURE;
		goto CLEAN_RETURN;
	}

	/*add presetList start tag*/
	snprintf(*pHomePresetRespXMLBuff, respXMLLen, "%s", "<presetList>\n");

	/*create preset xml for all available devices. skipped one row, containing column names*/
	for(i = columns; i < arraySize; i+= columns)
	{
		/*convert preset version*/
		presetVersion = strtoul(ppsDataArray[i+2],NULL,0);

		/*get preset xml data corresponding to this presetID from preset DB and create preset response xml*/
		retVal = createPresetRespXML(ppsDataArray[i], ppsDataArray[i+1], presetVersion, &pPresetRespXMLBuff);
		if((UPNP_E_SUCCESS == retVal) && (NULL != pPresetRespXMLBuff))
		{
			/*reallocate main xml buffer memory*/
			*pHomePresetRespXMLBuff = realloc(*pHomePresetRespXMLBuff, strlen(pPresetRespXMLBuff)+1);

			/*recalculate total length so far*/
			respXMLLen += strlen(pPresetRespXMLBuff)+1;

			/*append device preset xml set in main xml buffer*/
			snprintf((*pHomePresetRespXMLBuff)+strlen(*pHomePresetRespXMLBuff), respXMLLen-strlen(*pHomePresetRespXMLBuff), "%s", pPresetRespXMLBuff);

			/*free device preset xml set, prepare it for new device ID*/
			free(pPresetRespXMLBuff);pPresetRespXMLBuff=NULL;
		}
	}

	/*add presetList start tag*/
	snprintf((*pHomePresetRespXMLBuff)+strlen(*pHomePresetRespXMLBuff), respXMLLen-strlen(*pHomePresetRespXMLBuff), "%s", "</presetList>\n");

CLEAN_RETURN:
	WeMoDBTableFreeResult(&ppsDataArray,&rows,&columns);
	return retVal;
}

int processAndDeleteDevicePreset(char *pPresetXMLBuff)
{
	int retVal = UPNP_E_SUCCESS;
	double currentPresetVersion = 0;
	double newPresetVersion = 0;
	char condition[SIZE_128B]={0};

	/*char array hold various preset xml tag values*/
	char presetVersion[SIZE_32B]={0},isPresetGroupID[SIZE_32B]={0},presetID[SIZE_32B]={0},presetName[SIZE_32B]={0},presetValue[SIZE_32B]={0},presetType[SIZE_32B]={0};

	/*array of const char pointers to hold preset xml tag names*/
	char *pPresetParamSet[MAX_PRESET_TAGS_IN_XML+1] = {"presetList", "version", "isGroupID", "id", "name", "value", "type"};

	/*array of char pointers to hold preset xml tag values*/
	char *pPresetValueSet[MAX_PRESET_TAGS_IN_XML] = {presetVersion, isPresetGroupID, presetID, presetName, presetValue, presetType};

        /*char array, hold various column values in preset db*/
        char isGroupID[SIZE_32B]={0};

        /*array of struct to hold version row column values in preset db*/
        ColDetails presetVersionColInfo[2] = {{"version",presetVersion},{"isGroupID",isGroupID}};

	DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "processAndDeleteDevicePreset: XML received is:%s", pPresetXMLBuff);

	/*parse preset xml data and fill all preset palace holder array*/
	retVal = parseXMLData(pPresetXMLBuff, pPresetParamSet, pPresetValueSet, MAX_PRESET_TAGS_IN_XML);
	if(UPNP_E_SUCCESS != retVal)
	{
		DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "parseDevicePresetDataXMLRequest Error");
		retVal = UPNP_E_PARSING;
		goto CLEAN_RETURN;
	}

	/*get current version of this preset ID from version DB*/
	currentPresetVersion = getPresetVersionFromDB(presetID);

	/*New preset version corresponding to reset ID*/
	newPresetVersion = strtoul(presetVersion,NULL,0);

	/*validate version, New version should always be greater than current version*/
	if(currentPresetVersion >= newPresetVersion)
	{
		DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "deleteDevicePreset: %s:Invalid preset Version, Not deleting Preset!", presetVersion);
		retVal = FAILURE;
		goto CLEAN_RETURN;
	}

	/*create query to delete preset entry from db*/
	snprintf(condition, sizeof(condition), "id=\"%s\" AND name=\"%s\" AND type=\"%s\"", presetID,presetName,presetType);
	/*execute query to delete preset entry from db*/
	retVal = WeMoDBDeleteEntry(&s_PresetDB, "DEVICEPRESETINFO", condition);

	/*update preset isGroupID info in preset isGroupID db*/
	snprintf(isGroupID, sizeof(isGroupID), "'%s'", isPresetGroupID);

	/*create query to update preset entry version in db*/
	memset(condition, 0, sizeof(condition));
	snprintf(condition, sizeof(condition), "id=\"%s\"", presetID);
	/*execute query to update preset entry in db*/
	retVal = WeMoDBUpdateTable(&s_PresetDB, "DEVICEPRESETVERSIONINFO", presetVersionColInfo, 2, condition);

CLEAN_RETURN:
	return retVal;
}

int deleteDevicePreset(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	char *aErrorResp = NULL;
	int retVal = UPNP_E_SUCCESS;
	char *pPresetXMLBuff = NULL;

	*out = NULL;
	*errorString = NULL;

	/*get preset xml data from request*/
	pPresetXMLBuff = Util_GetFirstDocumentItem (request, "devicePresetData");
	if((NULL == pPresetXMLBuff) || (0 == strlen (pPresetXMLBuff)))
	{
		DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "deleteDevicePreset: 'devicePresetData' tag does not exist" );
		retVal = UPNP_E_INVALID_ARGUMENT;
		goto CLEAN_RETURN;
	}

	/*parse preset xml data, validate it and delete preset data from DB*/
	retVal = processAndDeleteDevicePreset(pPresetXMLBuff);

CLEAN_RETURN:

	/*set response strings*/
	if(UPNP_E_SUCCESS != retVal)
		aErrorResp = "Error";
	else
	{
		/*Update Cloud*/
		CreateUpdatePresetDataThread(pPresetXMLBuff, PUT, "preset/delete");
		aErrorResp = "Success";
	}

	/*send UPnP response*/
	if(UPNP_E_SUCCESS != UpnpAddToActionResponse(out, "deleteDevicePreset", CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE], "responseStatus", aErrorResp))
	{
		*errorString = "Error";
		DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "deleteDevicePreset: Internal Error" );
		retVal = UPNP_E_INTERNAL_ERROR;
	}
	return retVal;
}

int deleteAllPreset(char *presetID)
{
	int retVal = UPNP_E_SUCCESS;
	char condition[SIZE_128B]={0};

	DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "deleteAllPreset: presetID=%s", presetID);

	/*create query to delete preset entry from db*/
	snprintf(condition, sizeof(condition), "id=\"%s\"", presetID);

	/*execute query to delete preset entry from db*/
	retVal = WeMoDBDeleteEntry(&s_PresetDB, "DEVICEPRESETINFO", condition);

	/*execute query to delete version entry from db*/
	retVal = WeMoDBDeleteEntry(&s_PresetDB, "DEVICEPRESETVERSIONINFO", condition);

	return retVal;
}
int processAndDeleteDeviceAllPreset(char *pPresetXMLBuff)
{
	int retVal = UPNP_E_SUCCESS;

	/*char array hold various preset xml tag values*/
	char presetID[SIZE_32B]={0};

	/*array of const char pointers to hold preset xml tag names*/
	char *pPresetParamSet[2] = {"presetList", "id"};

	char *pPresetValueSet[1] = {presetID};

	DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "processAndDeleteDeviceAllPreset: XML received is:%s", pPresetXMLBuff);

	/*parse preset xml data and fill all preset palace holder array*/
	retVal = parseXMLData(pPresetXMLBuff, pPresetParamSet, pPresetValueSet, 1);
	if(UPNP_E_SUCCESS != retVal)
	{
		DEBUG_LOG (ULOG_UPNP, UL_DEBUG, "processAndDeleteDeviceAllPreset Error");
		retVal = UPNP_E_PARSING;
		goto CLEAN_RETURN;
	}

	/*Clear all presets for this ID*/
	retVal = deleteAllPreset(presetID);

CLEAN_RETURN:
	return retVal;
}

int deleteDeviceAllPreset(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString)
{
	char *aErrorResp = NULL;
	int retVal = UPNP_E_SUCCESS;
	char *pPresetXMLBuff = NULL;

	*out = NULL;
	*errorString = NULL;

	/*get preset xml data from request*/
	pPresetXMLBuff = Util_GetFirstDocumentItem (request, "devicePresetData");
	if((NULL == pPresetXMLBuff) || (0 == strlen (pPresetXMLBuff)))
	{
		DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "deleteDeviceAllPreset: 'devicePresetData' tag does not exist" );
		retVal = UPNP_E_INVALID_ARGUMENT;
		goto CLEAN_RETURN;
	}

	/*parse preset xml data, validate it and delete preset data from DB*/
	retVal = processAndDeleteDeviceAllPreset(pPresetXMLBuff);

CLEAN_RETURN:

	/*set response strings*/
	if(UPNP_E_SUCCESS != retVal)
		aErrorResp = "Error";
	else
	{
		/*Update Cloud*/
		CreateUpdatePresetDataThread(pPresetXMLBuff, PUT, "preset/delete");
		aErrorResp = "Success";
	}

	/*send UPnP response*/
	if(UPNP_E_SUCCESS != UpnpAddToActionResponse(out, "deleteDeviceAllPreset", CtrleeDeviceServiceType[PLUGIN_E_BRIDGE_SERVICE], "responseStatus", aErrorResp))
	{
		*errorString = "Error";
		DEBUG_LOG( ULOG_UPNP, UL_DEBUG, "deleteDeviceAllPreset: Internal Error" );
		retVal = UPNP_E_INTERNAL_ERROR;
	}
	return retVal;
}

//End -- Implementation the UPnP action command of WeMoBridge_LEDLight

#endif

