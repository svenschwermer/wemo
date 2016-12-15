#ifdef PRODUCT_WeMo_LEDLight

#ifndef _LEDLIGHT_REMOTE_HANDLER_H_
#define _LEDLIGHT_REMOTE_HANDLER_H_

#include "ledLightUPnPHandler.h"

#define GET     0
#define POST    1
#define PUT     2
#define DELETE  8

struct SubDevices {
  int nDevice;
  char szDeviceIDs[MAX_DEVICES][MAX_ID_LEN];
};

struct DeviceCapability {
  int nStatus;
  char szCapabilityID[CAPABILITY_MAX_ID_LENGTH];
  char szCapabilityValue[CAPABILITY_MAX_VALUE_LENGTH];
};

struct Device {
  char szCloudDeviceID[MAX_ID_LEN];
  char szDeviceID[MAX_ID_LEN];
  char szDeviceModelCode[MAX_VAL_LEN];
  char szDeviceProductType[MAX_VAL_LEN];
  char szStatusCode[10];
  int nCapabilityCount;
  struct DeviceCapability devCapability[5];
};

struct DeviceStatus {
  char szPluginId[MAX_ID_LEN];
  char szPluginMacAddress[MAX_ID_LEN];
  char szPluginModelCode[MAX_VAL_LEN];
  int nDeviceCount;
  struct Device device[MAX_DEVICES];
};

struct Group {
  char szID[MAX_ID_LEN];
  char szReferenceID[MAX_ID_LEN];
  char szName[MAX_VAL_LEN];
  int nErrorCode;
  char szErrorMsg[MAX_VAL_LEN];
  int nDeviceCount;
  char szCloudDeviceID[MAX_DEVICES][MAX_ID_LEN];
  char szDeviceID[MAX_DEVICES][MAX_ID_LEN];
  int nCapabilityCount;
  struct DeviceCapability devCapability[5];
};

struct Groups {
  char szPluginId[MAX_ID_LEN];
  char szPluginMacAddress[MAX_ID_LEN];
  char szPluginModelCode[MAX_VAL_LEN];
  int nGroupCount;
  struct Group group[MAX_GROUPS];
};

// Thread functions for handling the remote apis of WeMo_LEDLigt
int createRemoteThread(void);
void destroyRemoteThread(void);
void *updatePresetData(void*);

typedef struct PRESET_UPDATE_DATE_
{
	int method;
	char *presetXMLData;
	char urlString[SIZE_64B];
}PRESET_UPDATE_DATE,*PPRESET_UPDATE_DATE;



//====================================================================================================================================
char* getElemenetValue(mxml_node_t *curr_node, mxml_node_t *start_node, mxml_node_t **pfind_node, char *pszElementName);

int RegisterSubDevices(char *pPluginId, struct SubDevices *pSubDevices, char **ppErrCode);
int RegisterCapabilityProfiles(char **ppErrCode);
int DeleteSubDevice(const char *pCloudDeviceID, char **ppErrCode);
int DeleteSubDevices(char *pPluginId, char **ppErrCode);
int GetHomeDevices(char *pHomeID, char **ppErrCode);
int QueryCapabilityProfileList(char **ppErrCode);

int SendDeviceStateNotification(char *pPluginId, char *pDeviceId, struct DeviceInfo *pDevInfo, char **ppErrCode);

int CreateGroups(int nMethod, char *pHomeID, char *pGroupID, char **ppErrCode);
int DeleteGroups(char *pHomeID, char *pGroupID, char **ppErrCode);

//UpdateDeviceCapabilities handler
int SetDeviceStatusHandler(void *hndl, char *xmlBuf, unsigned int buflen, void *data_sock, void* remoteInfo, char* transaction_id);
//GetDevicesDetails handler
int GetDeviceStatusHandler(void *hndl, char *xmlBuf, unsigned int buflen, void* data_sock , void* remoteInfo, char* transaction_id);
//UpdateGroupCapabilitires handler
int SetGroupDetailsHandler(void *hndl, char *xmlBuf, unsigned int buflen, void* data_sock , void* remoteInfo, char* transaction_id);
//Create Group handler
int CreateGroupHandler(void *hndl, char *xmlBuf, unsigned int buflen, void* data_sock , void* remoteInfo, char* transaction_id);
//Delete Group handler
int DeleteGroupHandler(void *hndl, char *xmlBuf, unsigned int buflen, void* data_sock , void* remoteInfo, char* transaction_id);

int LinkUpgradeFwVersion(void *hndl, char *xmlBuf, unsigned int buflen, void *data_sock,
                        void* remoteInfo,char* transaction_id);
int RemoteSetDataStores(void *hndl, char *xmlBuf, unsigned int buflen, void*data_sock, void* remoteInfo,char* transaction_id);

int RemoteGetDataStores(void *hndl, char *xmlBuf, unsigned int buflen, void*data_sock, void* remoteInfo,char* transaction_id);

/*preset APIs*/
void init_preset_lock();
void destroy_preset_lock();
void preset_lock();
void preset_unlock();
int remoteAccessSetDevicePreset(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void*remoteInfo,char*transaction_id);
int remoteAccessGetDevicePreset(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void*remoteInfo,char*transaction_id);
int remoteAccessDeleteDevicePreset(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void*remoteInfo,char*transaction_id);
int remoteAccessDeleteDeviceAllPreset(void *hndl,char *xmlBuf, unsigned int buflen, void *data_sock,void*remoteInfo,char*transaction_id);
int CreateUpdatePresetDataThread(char *pPresetData, int method, char *urlString);

//====================================================================================================================================

#endif /* _LEDLIGHT_REMOTE_HANDLER_H_ */

#endif /* PRODUCT_WeMo_LEDLight */
