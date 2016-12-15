#ifdef PRODUCT_WeMo_LEDLight

#ifndef _LEDLIGHT_UPNP_HANDLER_H_
#define _LEDLIGHT_UPNP_HANDLER_H_

#include "subdevice.h"
#include "ulog.h"

//Add the new structure data for GetEndDevices action command in WeMoBridge_LED_Light
#define PAIRED_LIST   1
#define UNPAIRED_LIST 2

#define BULB_CONTROL  1
#define CAPS_PROFILE  2

/* WeMoBridge LED Light */
#define MAX_LISTS     2
#define MAX_DEVICES   56
#define MAX_IDS       56
#define MAX_VALUES    56
#define MAX_PROFILES  10
#define MAX_GROUPS    56

#define MAX_ID_LEN        32
#define MAX_VAL_LEN       256
#define MAX_NAME_LEN      50
#define MAX_DEVIDS_LEN    (MAX_ID_LEN * MAX_DEVICES)
#define MAX_GRPIDS_LEN    (MAX_ID_LEN * MAX_GROUPS)
#define MAX_CAPIDS_LEN    (MAX_ID_LEN * MAX_PROFILES)
#define MAX_CAPVALS_LEN   (CAPABILITY_MAX_VALUE_LENGTH * MAX_PROFILES)


#define OTA_FW_DOWNLOADING        0
#define OTA_FW_DOWNLOAD_SUCCESS   1
#define OTA_FW_DOWNLOAD_FAIL      2
#define OTA_FW_UPGRADING          3
#define OTA_FW_UPGRADE_SUCCESS    4
#define OTA_FW_INVALID_IMAGE      5
#define OTA_FW_NOT_UPGRADE        6
//#define OTA_FW_UPGRADE_FAIL       6

struct CapabilityInfo {
  bool bAvailable;
  char szCapabilityID[CAPABILITY_MAX_ID_LENGTH];
  char szCapabilityValue[CAPABILITY_MAX_VALUE_LENGTH];
};

struct DeviceInfo {
  bool bGroupDevice;
  char szDeviceID[MAX_ID_LEN];
  struct CapabilityInfo CapInfo;
};

struct RespGroup {
  char szGroupID[MAX_ID_LEN];
  char szGroupName[MAX_NAME_LEN];
  char szDeviceIDs[MAX_DEVIDS_LEN];
};

struct RespGetGroups {
  int nGroups;
  struct RespGroup respGroups[MAX_GROUPS];
};

// Local API interface
void init_upnp_lock();
void destroy_upnp_lock();
void upnp_lock();
void upnp_unlock();

void init_upnp_event_lock();
void destroy_upnp_event_lock();
void upnp_event_lock();
void upnp_event_unlock();

void upnp_event_notification(bool bAvailable, char *device_id, char *capabiliy_id, char *value);

void SendDeviceEvent(bool result, bool group, char *device_id, char *capabiliy_id, char *value, unsigned event);

void MakeGroupID(char *pszGroupID, SD_GroupID *pGroupID);
void MakeDeviceID(char *pszDeviceID, SD_DeviceID *pDeviceID);
void MakeCapabilityID(char *pszCapabilityID, SD_CapabilityID *pCapabilityID);
void MakeRegisterDeviceID(char *pszCloudDeviceID, SD_DeviceID *pRegisterDeviceID);
void MakeRegisterCapabilityID(char *pszCloudCapID, SD_CapabilityID *pRegisterCapID);

bool IsProfileCapabilityID(char *pProfileName, SD_CapabilityID *pCapabilityID);

char* CreateDeviceStatusXML(char *pszDeviceIDs);
char* CreateDeviceCapabilitiesXML(char* pszDeviceIDs);
char* CreateCapabilityProfilesListXML(int nCapIDs, char szCapIDs[][MAX_ID_LEN]);
char* CreateStatusChangeXML(bool bAvailabeDevice, bool bSensorDevice, char *pDeviceID, char *pCapabilityID, char *pValue);

char* GetCapabilityPorfileIDs(void);

int GetIDs(char *pIDs, char szIDs[][MAX_ID_LEN]);
int GetValues(char *pValues, char szValues[][MAX_VAL_LEN]);
int GetNames(char *pNames, char szNames[][MAX_NAME_LEN]);

int PerformSetDeviceStatus(char *pXmlBuff, char *pszErrorDeviceIDs);
int PerformCreateGroup(char *pXmlBuff, int *pnDevices);

void GetCacheDeviceOnOff(SD_DeviceID deviceID, char *capability_id, char *value);
void GetCacheDeviceHomeSense(SD_DeviceID deviceID, char *capability_id, char *value);

void SetCurrSubDeviceFWUpdateState(int fwSubDeviceUpdate);
int GetCurrSubDeviceFWUpdateState();

void SetSubDeviceFWUpdateState(char *pszDeviceID, int fwSubDeviceUpdate);
int GetSubDeviceFWUpdateState(char *pszDeviceID);

int StartUpgradeSubDeviceFirmware(char *pDeviceList, char *pFirmwareLink);

int isSubdeviceFirmwareUpdating(void);

/*preset APIs*/
int processAndSetDevicePreset(char *pPresetXMLBuff);
int processAndGetDevicePreset(char *pPresetXMLBuff, char **pPresetRespXMLBuff);
int processAndDeleteDevicePreset(char *pPresetXMLBuff);
int processAndDeleteDeviceAllPreset(char *pPresetXMLBuff);
int deleteAllPreset(char *presetID);

// UPnP Action command for WeMo Bridge
int OpenNetwork(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int CloseNetwork(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int AddDeviceName(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int AddDevice(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int RemoveDevice(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int SetDeviceStatus(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetDeviceStatus(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetEndDevices(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetCapabilityProfileIDList(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetCapabilityProfileList(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetDeviceCapabilities(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int CreateGroup(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int DeleteGroup(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetGroups(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int SetDeviceName(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int SetDeviceNames(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int IdentifyDevice(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetManufactureData(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int QAControl(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int UpgradeSubDeviceFirmware(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int SetZigbeeMode(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int ScanZigbeeJoin(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int SetDataStores(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int GetDataStores(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int DeleteDataStores(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int CloseZigbeeRouterSetup(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int setDevicePreset(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int getDevicePreset(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int deleteDevicePreset(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);
int deleteDeviceAllPreset(pUPnPActionRequest pActionRequest, IXML_Document *request, IXML_Document **out, const char **errorString);

#endif  //_LEDLIGHT_UPNP_HANDLER_H_

#endif  //PRODUCT_WeMo_LEDLight
