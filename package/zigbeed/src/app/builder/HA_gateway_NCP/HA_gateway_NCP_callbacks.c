//

// This callback file is created for your convenience. You may add application code
// to this file. If you regenerate this file over a previous version, the previous
// version will be overwritten and any code you have added will be lost.

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/un.h>

#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include <dirent.h>

#include "fw_rev.h"
#include "app/framework/include/af.h"
#include "app/framework/util/af-main.h"

#include "app/util/serial/command-interpreter2.h"
#include "app/framework/cli/security-cli.h"
/*#include "app/framework/cli/zcl-cli.h"*/

// ZDO
#include "app/util/zigbee-framework/zigbee-device-common.h"
#include "app/util/zigbee-framework/zigbee-device-host.h"
#include "app/util/zigbee-framework/network-manager.h"

#include "zbIPC.h"

#define ZCL_BELKIN_MFG_SPECIFIC_TEST_MODE_COMMAND_ID 0x10

/*#define DIR_ZIGBEED_CONFIG  "/tmp/Belkin_settings/"*/
#define DIR_ZIGBEED_CONFIG	ZB_JOIN_DIR "/"

#define ZBID_FILE_EXAMPLE 	DIR_ZIGBEED_CONFIG "zbid.1122334455667788"
#define ZBID_FILE_LENGTH 	(sizeof(ZBID_FILE_EXAMPLE) - 1)   // 42
#define ZBID_TYPE_FILE_EXAMPLE 	DIR_ZIGBEED_CONFIG "zbid.1122334455667788.type"
#define ZBID_TYPE_FILE_LENGTH 	(sizeof(ZBID_TYPE_FILE_EXAMPLE) - 1)   // 47

#define ZBD_FILE_REPORT_TIME 	"/tmp/zigbee.report.seconds"
#define ZBD_FILE_PRESENCE_TH 	"/tmp/zigbee.presence.th"

enum ZBD_PowerIndex {ZBD_HIGH_POWER = 0, ZBD_NORMAL_POWER = 1};

enum {
	NODE_ST_NULL,
	NODE_ST_ENDPOINT_REQUESTING,
	NODE_ST_REPORT_REQUESTING,
	NODE_ST_ADDRESS_REQUESTING,
	NODE_ST_LEAVE_REQUESTING,
	NODE_ST_CLUSTER_REQUESTING,
	NODE_ST_CIE_WRITING,
};

typedef enum {
	CLEAN_MODE_ALL,
	CLEAN_MODE_SAVE_NODEID,
} TZbdCleanMode;

#define HOMESENSE_DEST_ENDPOINT 1

#define KEYFOB_POLL_INTERVAL_SEC 7
#define KEYFOB_PRESENCE_COUNT 6

/*#define KEYFOB_THRESHOLD_SEC_NO_POLL (22 * KEYFOB_POLL_INTERVAL_SEC) // 22 * 7 = 154*/
#define KEYFOB_THRESHOLD_SEC_NO_POLL (KEYFOB_PRESENCE_COUNT * KEYFOB_POLL_INTERVAL_SEC)
#define BATTERYREPORT_THRESHOLD_SEC_NO_POLL (3 * 60 * 30)

static int s_BatteryReportThresholdSec = BATTERYREPORT_THRESHOLD_SEC_NO_POLL;

#define NULL_NODE_ID 0xffff
#define NULL_TIME_QS 0xffff
#define NULL_INDEX   0xff
#define ADDRESS_TABLE_SIZE 60
#define MAX_INDEX_ADDRESS_TABLE (ADDRESS_TABLE_SIZE - 1)

typedef struct
{
	EmberNodeId nodeId;
	EmberEUI64 eui64;
	int endpoint;
	int nodeType;
	int nodeState;
	int16u timeQSAction;
	int16u timeQSLastPoll;
	int pollCount;
	int isRepairing;
	int isKnown;
	int8u zclSequence;
} TAddressTableEntry;

enum {
	ZBD_IO_SYNC_SET ,
	ZBD_IO_ASYNC ,
	ZBD_IO_SYNC_GET,
};

typedef enum {
        ZBD_ST_INIT,
        ZBD_ST_IPC_RECV,
        ZBD_ST_ZB_MSG_SENT_STATUS,
        ZBD_ST_ZB_SET,
        ZBD_ST_ZB_GET,
        ZBD_ST_ZB_TRUSTCENTER,
} TZBD_States;

const char * s_StateNames[] = {
	"ZBD_ST_INIT",
        "ZBD_ST_IPC_RECV",
        "ZBD_ST_ZB_MSG_SENT_STATUS",
	"ZBD_ST_ZB_SET",
	"ZBD_ST_ZB_GET",
        "ZBD_ST_ZB_TRUSTCENTER",
};

const char * s_ZBEventNames[] = {
	ZBD_EVENT_OTA_STATUS,
	ZBD_EVENT_OTA_PERCENT,
	ZBD_EVENT_ERROR_STATUS,
	ZBD_EVENT_ZONE_STATUS,
	ZBD_EVENT_NODE_PRESENT,
	ZBD_EVENT_NODE_VOLTAGE,
	ZBD_EVENT_NODE_RSSI,
	ZBD_EVENT_SCANJOIN_STATUS,
	"", // terminator
};

static TZBD_States s_State = ZBD_ST_INIT;
static TZBD_States s_PrevState = ZBD_ST_INIT;

static TIPC_CMD s_Cur_CMD;
static int s_ClientFD = -1;

static int s_ServerSock, s_MaxSD;
static struct sockaddr_un s_LocalSockAddr, s_RemoteSockAddr;
static int s_LenRemote;

static struct timeval s_SockTimeout;
static fd_set s_MasterSet, s_WorkingSet;

static int16u s_TimeToAbort;

static EmberEUI64 s_LastEUI;
EmberNodeId s_LastNodeId;
EmberNodeId s_CurrentRepairNodeId;

static int32u s_OTA_Size = 0;

#define TIMEOUT_SEC	5.5

extern int zbdIasClientRemoveServer(int8u* ieeeAddress);
extern EmberApsFrame globalApsFrame;

static int8u outgoingBuffer[15];

static TAddressTableEntry addressTable[ADDRESS_TABLE_SIZE];
static char s_StrEui64[17];

static void zbdDbgPrintByteStream(unsigned char *byteStream, int length);
static int zbdGetPowerTableIdx(void);

static void zbdInitIPCServer(void);
static void zbdSetRxLNAMode(void);
static char * zbdEui64ToStr(EmberEUI64 nodeEui64);
static int zbdMakeEuiFileName(char * fileName, int sizeFileName, EmberEUI64 nodeEui64);
static int zbdMakeNodeTypeFileName(char * fileName, int sizeFileName, EmberEUI64 nodeEui64);
static int zbdMakeClustersFileName(char * fileName, int sizeFileName, EmberEUI64 nodeEui64, int endpoint);
static int zbdMakeAttributesFileName(char * fileName, int sizeFileName, EmberEUI64 nodeEui64, int endpoint, int clusterId);
static int zbdUpdateDBNodeId(EmberEUI64 newNodeEui64, EmberNodeId newNodeId);
static int zbdSaveClusters(EmberNodeId nodeId, int8u endpoint, int16u deviceId, const int16u * pclusterList, int clusterCount);
void zbdWeMoCliServiceDiscoveryCallback(const EmberAfServiceDiscoveryResult* result);
int zbdConfigureBindingCommand(EmberNodeId nodeId, int16u clusterId);
int zbdConfigureReportCommand(EmberNodeId nodeId, int16u zoneType, int16u clusterId, int16u attributeId);

static int zbdIsValidNodeIdFile(const char *fullpathFile);
static int zbdIsValidNodeTypeFile(const char *fullpathFile);
static int zbid2EUI(const char *fullpathFile, TEUID_64 nodeEUI);

static void printEUI64(int8u *eui64);
static void deleteEui64( EmberEUI64 eui64 );
static void deleteAddressTableEntry( int8u index );
static void initAddressTable( void );
static int8u getAddressIndexFromIeee(EmberEUI64 ieeeAddress);
static int8u getIeeeFromNodeId(EmberNodeId emberNodeId, EmberEUI64 eui64);
static int8u getAddressIndexFromNodeId(EmberNodeId emberNodeId);
static int8u findFreeAddressTableIndex( void );
static void printAddressTable( void );
static int zbdIsLatestEvent(EmberNodeId nodeId, int8u sequenceNumber);

static void newDeviceJoinHandler(EmberNodeId newNodeId, EmberEUI64 newNodeEui64);
static void newDeviceLeftHandler(EmberEUI64 newNodeEui64, TZbdCleanMode cleanMode);
static void zbdSetNodeType(EmberNodeId nodeId, int nodeType);
static int zbdGetNodeType(EmberNodeId nodeId);
static int zbdIsNodeSensorByAddressIndex(int8u addressIndex);
static int zbdIsNodeSensor(EmberNodeId nodeId);
static int16u zbdSetNodePollTime(EmberNodeId nodeId, const int singlePollSec, const int threshold);
static int16u zbdGetNodePollTime(EmberNodeId nodeId);
static void zbdAddressSetEndpoint(EmberNodeId nodeId, int8u endpoint);

static EmberStatus zbdSetSourceRoute(EmberNodeId id);

static int zbdReadIntFromFile(const char *fileName, int *pintType);
static int zbdReadNodeId(const char *fileName, EmberNodeId *pnodeId);
static int zbdReadNodeType(const char *fileName, int *pnodeType);
static void zbdSysEventInit(void);
static void zbdSysEventErrorCode(EmberNodeId nodeId, int errorCode, int clusterId);

static void zbdScheduleCommandRequest(EmberNodeId nodeId, int nodeState, int16u timeQS, EmberEventControl *pemberEvent);
static void zbdScheduleAddressRequest(EmberNodeId nodeId);
static void zbdScheduleEndpointsRequest(EmberNodeId nodeId, int16u timeQS);
static void zbdScheduleLeaveRequest(EmberNodeId nodeId);
static void zbdScheduleBasicClusterRequest(EmberNodeId nodeId, int8u endpoint);
static void zbdScheduleCieWriting(EmberNodeId nodeId, int16u timeQS);

static int zbdActiveEndpointsRequest(EmberNodeId paramNodeId);
static int zbdLeaveRequest(EmberNodeId paramNodeId, int8u paramFlags);
static int zbdReadBasicCluster(int16u nodeId, int8u dstEndpoint);
static int zbdIdentify(EmberNodeId paramNodeId, int8u paramEndpoint, int16u paramTime, int16u multicast);

static int IsFileExisting(char * fileName);
static EmberStatus sendCieAddressWrite( EmberNodeId nodeId, int8u endpoint );

void zbdSysEventOtaNoNextImage(void);
void zbdTriggerBindReport(EmberNodeId nodeId, int16u zoneType);

EmberEventControl delayEvent;
EmberEventControl signalEvent;
EmberEventControl nodeCommandEvent;
EmberEventControl presenceEvent;
EmberEventControl configBindingEvent;
EmberEventControl periodicEvent;
EmberEventControl wakeupSleepyEvent;
EmberEventControl readBasicClusterEvent;

void delayEventHandler(void);
void signalEventHandler(void);
void nodeCommandEventHandler(void);
void configBindingEventHandler(void);
void presenceEventHandler(void);
void periodicEventHandler(void);
void wakeupSleepyEventHandler(void);
void readBasicClusterEventHandler(void);

// Put the controls and handlers in an array.  They will be run in
// this order.
EmberEventData events[] =
 {
   { &delayEvent,    delayEventHandler },
   { &signalEvent,   signalEventHandler },
   { &nodeCommandEvent, nodeCommandEventHandler },
   { &configBindingEvent,    configBindingEventHandler },
   { &presenceEvent, presenceEventHandler },
   { &periodicEvent, periodicEventHandler },
   { &wakeupSleepyEvent, wakeupSleepyEventHandler },
   { &readBasicClusterEvent, readBasicClusterEventHandler },
   { NULL, NULL }                            // terminator
 };

static void printEUI64(int8u *eui64)
{
	int8u i;
	for (i=8; i>0; i--) {
		DBGPRINT("%02X", eui64[i-1]);
	}
}

static void deleteEui64( EmberEUI64 eui64 )
{
	int8u i;

	for(i=0; i< 8 ; i++)
		eui64[i] = 0xff;
}

static void deleteAddressTableEntry( int8u index )
{
	addressTable[index].nodeId = NULL_NODE_ID;
	deleteEui64(addressTable[index].eui64);
	addressTable[index].endpoint = 0;
	addressTable[index].nodeType = ZB_DEV_UNKNOWN;
	addressTable[index].nodeState = NODE_ST_NULL;
	addressTable[index].timeQSAction = NULL_TIME_QS;
	addressTable[index].timeQSLastPoll = NULL_TIME_QS;
	addressTable[index].pollCount = 0;
	addressTable[index].isRepairing = FALSE;
	addressTable[index].isKnown = FALSE;
	addressTable[index].zclSequence = 0;
}

static void initAddressTable( void )
{
	int8u index;
	DIR *dir;
	struct dirent *ent;
	EmberNodeId nodeid;
	char full_name[120];
	EmberEUI64 eui64;
	int node_type;

	for(index = 0; index < ADDRESS_TABLE_SIZE; index++)
	{
		deleteAddressTableEntry(index);
	}

	if ((dir = opendir (DIR_ZIGBEED_CONFIG)) != NULL)
	{
		DBGPRINT("Reading dir:%s\n\n", DIR_ZIGBEED_CONFIG);
		while ((ent = readdir (dir)) != NULL)
		{
			if (strstr(ent->d_name, "zbid."))
			{
				DBGPRINT("\n");
				snprintf(full_name, sizeof(full_name), "%s%s", DIR_ZIGBEED_CONFIG, ent->d_name);
				DBGPRINT("d_name:%s, full_name:%s \n", ent->d_name, full_name);
				if (SUCCESS == zbdReadNodeId(full_name, &nodeid))
				{
					DBGPRINT("full_name:%s, Read NodeID:0x%04X d_name:%s\n", full_name, nodeid, ent->d_name);
					if (SUCCESS == zbid2EUI(full_name, eui64))
					{
						DBGPRINT("SUCCESS: zbid2EUI\n");
						newDeviceJoinHandler(nodeid, eui64);

						zbdMakeNodeTypeFileName(full_name, sizeof(full_name), eui64);
						if (SUCCESS == zbdReadNodeType(full_name, &node_type))
						{
							index = getAddressIndexFromIeee(eui64);
							if (NULL_INDEX != index)
							{
								addressTable[index].nodeType = node_type;
							}
						}
					}
				}
			}
		}
		closedir (dir);
	}
	printAddressTable();
}

static int8u getAddressIndexFromIeee(EmberEUI64 ieeeAddress)
{
	int8u index;

	for(index = 0; index < ADDRESS_TABLE_SIZE; ++index)
		if(MEMCOMPARE(addressTable[index].eui64,ieeeAddress,EUI64_SIZE)==0)
			return index;

	return NULL_INDEX;
}

static int8u getIeeeFromNodeId(EmberNodeId emberNodeId, EmberEUI64 eui64)
{
	int8u index;

	for(index = 0; index < ADDRESS_TABLE_SIZE; ++index) {
		if(addressTable[index].nodeId == emberNodeId) {
			MEMCOPY(eui64, addressTable[index].eui64, EUI64_SIZE);
			return index;
		}
	}

	return NULL_INDEX;
}

static int8u getAddressIndexFromNodeId(EmberNodeId emberNodeId)
{
	int8u index;

	for(index = 0; index < ADDRESS_TABLE_SIZE; ++index)
		if(addressTable[index].nodeId == emberNodeId)
			return index;

	DBGPRINT("NodeId : 0x%04X not in the address table.\n", emberNodeId);
	return NULL_INDEX;
}

static int8u findFreeAddressTableIndex( void )
{
	int8u index;

	for(index = 0; index<ADDRESS_TABLE_SIZE; index++)
		if(addressTable[index].nodeId == NULL_NODE_ID)
			return index;
	return NULL_INDEX;
}

static void printAddressTable( void )
{
	int8u index, i, totalDevices = 0;

	DBGPRINT("====================\n");
	DBGPRINT("Address Table\n");
	DBGPRINT("====================\n");
	DBGPRINT("Idx \tID \tType \n");
	for(index=0; index<ADDRESS_TABLE_SIZE; index++) {
		if(addressTable[index].nodeId != NULL_NODE_ID){
			zbdSetSourceRoute(addressTable[index].nodeId);
			DBGPRINT("%02d \t0x%04X \t0x%04X \t", totalDevices, addressTable[index].nodeId, addressTable[index].nodeType);
			printEUI64(addressTable[index].eui64);
			DBGPRINT("\n");
			totalDevices++;
		}
	}
	DBGPRINT("====================\n");
	DBGPRINT("Total Devices %d\r\n", totalDevices);
	DBGPRINT("====================\n");
}

static void newDeviceJoinHandler(EmberNodeId newNodeId, EmberEUI64 newNodeEui64)
{
	int8u index = getAddressIndexFromIeee(newNodeEui64);

	DBGPRINT("INDEX 1:  %d 0x%04x\r\n", index, newNodeId);

	// printSourceRouteTable();

	if(index == NULL_INDEX)
	{
		index = findFreeAddressTableIndex();
		if(index == NULL_INDEX)
		{
			DBGPRINT("Error:  Address Table Full \n");
			zbdSysEventErrorCode(0, ZB_RET_MAX_NODE, 0);
			printAddressTable();
			return;
		}

		addressTable[index].nodeId = newNodeId;
		addressTable[index].nodeState = NODE_ST_NULL;
		MEMCOPY(addressTable[index].eui64, newNodeEui64, EUI64_SIZE);
	}
	else
	{
		if(newNodeId != addressTable[index].nodeId)
		{
			DBGPRINT("Node ID Change:  was 0x%04x, is 0x%04x\n", addressTable[index].nodeId, newNodeId);
			addressTable[index].nodeId = newNodeId;
			addressTable[index].nodeState = NODE_ST_NULL;
		}
	}
}

static void newDeviceLeftHandler(EmberEUI64 newNodeEui64, TZbdCleanMode cleanMode)
{
	int8u index;
	char full_name[80];

	DBGPRINT("newDeviceLeftHandler \n");
	index = getAddressIndexFromIeee(newNodeEui64);
	if(index == NULL_INDEX)
	{
		DBGPRINT("Node not found\n");
		return;
	}

	if ( zbdIsNodeSensorByAddressIndex(index) )
	{
		zbdIasClientRemoveServer(newNodeEui64);
	}

	deleteAddressTableEntry(index);

	zbdMakeEuiFileName(full_name, sizeof(full_name), newNodeEui64);
	if (ZBID_FILE_LENGTH != strlen(full_name))
	{
		fprintf(stderr, "Invalid name: %s \n", full_name);
		return;
	}

	char cmd_str[100];
	if (cleanMode == CLEAN_MODE_SAVE_NODEID)
	{
		snprintf(cmd_str, sizeof(cmd_str) - 1, "rm %s.*", full_name);
	}
	else
	{
		snprintf(cmd_str, sizeof(cmd_str) - 1, "rm %s*", full_name);
	}

	DBGPRINT("cmd_str:%s \n", cmd_str);
	system(cmd_str);
}

void delayEventHandler(void)
{
	EmberEUI64 eui64;
	int8u ret;

	// Disable this event until its next use.
	emberEventControlSetInactive(delayEvent);
	DBGPRINT("\n delayEventHandler \n");
	DBGPRINT("s_CurrentRepairNodeId: 0x%02X \n", s_CurrentRepairNodeId);

	ret = getIeeeFromNodeId(s_CurrentRepairNodeId, eui64);
	if (ret != NULL_INDEX)
	{
		// send get node id
		emberAfFindNodeId(eui64, zbdWeMoCliServiceDiscoveryCallback);
		DBGPRINT("emberAfFindNodeId :");
		printEUI64(eui64);
		DBGPRINT("\n");
	}
	else
	{
		DBGPRINT("ERROR Cannot find EUI for node 0x%02X \n", s_CurrentRepairNodeId);
	}
}

void signalEventHandler(void)
{
	int16u timeQS;

	// Disable this event until its next use.
	emberEventControlSetInactive(signalEvent);

	DBGPRINT("\n signalEventHandler \n");
	DBGPRINT("s_CurrentRepairNodeId: 0x%02X \n", s_CurrentRepairNodeId);

#if 0
	// send mtorr
	timeQS = (int16u) emberAfPluginConcentratorQueueDiscovery();
	if(timeQS > 20) {
		timeQS = 20;
	}
	DBGPRINT("emberAfPluginConcentratorQueueDiscovery: timeQS:%d \n", timeQS);
	// get node id
	emberEventControlSetDelayQS(delayEvent, (timeQS + 2));
#else
	emberSendManyToOneRouteRequest( EMBER_HIGH_RAM_CONCENTRATOR, EMBER_MAX_HOPS);
	DBGPRINT("emberSendManyToOneRouteRequest\n");

	emberEventControlSetDelayQS(delayEvent, 2);
#endif
}

void periodicEventHandler(void)
{
	int i;
	int16u nowQS;
	int is_empty;
	int8u ret;

	emberEventControlSetDelayQS(periodicEvent, 2);

	nowQS = halCommonGetInt16uQuarterSecondTick();

	int count_cmd = 0;
	is_empty = TRUE;
	for (i = 0; i < ADDRESS_TABLE_SIZE; ++i)
	{
		/*if (NODE_ST_ADDRESS_REQUESTING == addressTable[i].nodeState)*/
		if (addressTable[i].isRepairing)
		{
			if (nowQS >= addressTable[i].timeQSAction)
			{
				addressTable[i].isRepairing  = FALSE;
				addressTable[i].timeQSAction = NULL_TIME_QS;

				DBGPRINT("nodeId: 0x%02X \n", addressTable[i].nodeId);
				// send get node id
				emberAfFindNodeId(addressTable[i].eui64, zbdWeMoCliServiceDiscoveryCallback);
				DBGPRINT("nowQS:%d, emberAfFindNodeId :", nowQS);
				printEUI64(addressTable[i].eui64);
				DBGPRINT("\n");

				count_cmd += 1;
				if (count_cmd >= 2)
				{
					DBGPRINT("Break for next periodicEvent \n");
					break;
				}
			}
			else
			{
				is_empty = FALSE;
			}
		}
	}

	if (TRUE == is_empty)
	{
		DBGPRINT("periodicEventHandler: Inactive\n");
		emberEventControlSetInactive(periodicEvent);
	}
}

void nodeCommandEventHandler(void)
{
	int i;
	int16u nowQS;
	int is_empty;
	EmberStatus status;

	emberEventControlSetDelayQS(nodeCommandEvent, 5);

	nowQS = halCommonGetInt16uQuarterSecondTick();

	is_empty = TRUE;
	for (i = 0; i < ADDRESS_TABLE_SIZE; ++i)
	{
		if (NODE_ST_ENDPOINT_REQUESTING == addressTable[i].nodeState)
		{
			is_empty = FALSE;
			if (nowQS >= addressTable[i].timeQSAction)
			{
				if (addressTable[i].endpoint != 0)
				{
					addressTable[i].nodeState = NODE_ST_CLUSTER_REQUESTING;
				}
				else
				{
					zbdActiveEndpointsRequest(addressTable[i].nodeId);
				}
			}
		}

		if (NODE_ST_CLUSTER_REQUESTING == addressTable[i].nodeState)
		{
			if (nowQS >= addressTable[i].timeQSAction)
			{
				if (addressTable[i].nodeType != ZB_DEV_UNKNOWN)
				{
					addressTable[i].nodeState = NODE_ST_NULL;
					addressTable[i].timeQSAction = NULL_TIME_QS;
				}
				else
				{
					emberSimpleDescriptorRequest(addressTable[i].nodeId, addressTable[i].endpoint, EMBER_AF_DEFAULT_APS_OPTIONS);
					is_empty = FALSE;
				}
			}
			else
			{
				is_empty = FALSE;
			}
		}

		if (NODE_ST_CIE_WRITING == addressTable[i].nodeState)
		{
			if (nowQS >= addressTable[i].timeQSAction
					&& addressTable[i].endpoint != 0)
			{
				status = sendCieAddressWrite(addressTable[i].nodeId, addressTable[i].endpoint);
				if (status == EMBER_SUCCESS)
				{
					addressTable[i].nodeState = NODE_ST_NULL;
					addressTable[i].timeQSAction = NULL_TIME_QS;
				}
				else
				{
					is_empty = FALSE;
				}
			}
			else
			{
				is_empty = FALSE;
			}
		}
	}

	if (TRUE == is_empty)
	{
		DBGPRINT("nodeCommandEventHandler: Inactive\n");
		emberEventControlSetInactive(nodeCommandEvent);
	}
}

void wakeupSleepyEventHandler(void)
{
	int i;
	int16u nowQS;
	int is_empty;
	EmberStatus status;
	int elapsed_sec, times_up;

	emberEventControlSetDelayQS(wakeupSleepyEvent, 20); // 20 for 5 sec

	nowQS = halCommonGetInt16uQuarterSecondTick();

	is_empty = TRUE;
	times_up = 90 * 4;
	for (i = 0; i < ADDRESS_TABLE_SIZE; ++i)
	{
		if (NODE_ST_LEAVE_REQUESTING == addressTable[i].nodeState)
		{
			if (nowQS >= addressTable[i].timeQSAction)
			{
				status = zbdLeaveRequest(addressTable[i].nodeId, 0);
				DBGPRINT("wakeupSleepyEventHandler : zbdLeaveRequest 0x%04d status:%d \n",
						addressTable[i].nodeId,
						status);

				elapsed_sec = (nowQS - addressTable[i].timeQSAction) * 4;
				if (elapsed_sec > times_up)
				{
					DBGPRINT("Times up for nodeid:0x%04X giving up leave request. \n", addressTable[i].nodeId);
					addressTable[i].nodeState = NODE_ST_NULL;
					addressTable[i].timeQSAction = NULL_TIME_QS;
				}
			}
			else
			{
				is_empty = FALSE;
			}
		}
	}

	if (TRUE == is_empty)
	{
		DBGPRINT("wakeupSleepyEventHandler: Inactive\n");
		emberEventControlSetInactive(wakeupSleepyEvent);
	}
}

void readBasicClusterEventHandler(void)
{
	int i;
	int16u nowQS;
	int is_empty;
	EmberStatus status;
	int elapsed_sec, times_up;
	char fullpath_file[80];

	emberEventControlSetDelayQS(readBasicClusterEvent, 4); // 4 for 1 sec

	nowQS = halCommonGetInt16uQuarterSecondTick();

	is_empty = TRUE;
	times_up = 90 * 4;
	for (i = 0; i < ADDRESS_TABLE_SIZE; ++i)
	{
		if (addressTable[i].endpoint != 0 && addressTable[i].isKnown == FALSE)
		{
			zbdMakeAttributesFileName(fullpath_file, sizeof(fullpath_file), addressTable[i].eui64, addressTable[i].endpoint, ZCL_BASIC_CLUSTER_ID);
			if (IsFileExisting(fullpath_file))
			{
				DBGPRINT("BasicCluster file exists! fullpath_file:%s \n", fullpath_file);
				addressTable[i].isKnown = TRUE;
			}
			else
			{
				is_empty = FALSE;

				zbdReadBasicCluster(addressTable[i].nodeId, addressTable[i].endpoint);

				elapsed_sec = (nowQS - addressTable[i].timeQSAction) * 4;
				if (elapsed_sec > times_up)
				{
					DBGPRINT("Times up for nodeid:0x%04X giving up basic cluster request. \n", addressTable[i].nodeId);
					addressTable[i].isKnown = TRUE;
					addressTable[i].timeQSAction = NULL_TIME_QS;
				}
			}
		}
	}

	if (TRUE == is_empty)
	{
		DBGPRINT("readBasicClusterEvent: Inactive\n");
		emberEventControlSetInactive(readBasicClusterEvent);
	}
}

static int zbdDoBindReport(EmberNodeId nodeId, int16u zoneType, int16u clusterId, int16u attributeId)
{
	int status;

	status = zbdConfigureBindingCommand(nodeId, clusterId);
	DBGPRINT("zbdConfigureBindingCommand status:%d\n", status);

	status = zbdConfigureReportCommand(nodeId, zoneType, clusterId, attributeId);
	DBGPRINT("zbdConfigureReportCommand status:%d\n", status);

	return status;
}

void configBindingEventHandler(void)
{
	int status;
	EmberNodeId nodeId;
	int16u clusterId;
	int16u attributeId;
	int16u zoneType;
	int i;
	int16u nowQS;
	int is_empty;

	emberEventControlSetDelayQS(configBindingEvent, 1);

	nowQS = halCommonGetInt16uQuarterSecondTick();

	is_empty = TRUE;
	for (i = 0; i < ADDRESS_TABLE_SIZE; ++i)
	{
		if (NODE_ST_REPORT_REQUESTING == addressTable[i].nodeState)
		{
			if (nowQS >= addressTable[i].timeQSAction)
			{
				addressTable[i].nodeState = NODE_ST_NULL;
				addressTable[i].timeQSAction = NULL_TIME_QS;

				nodeId = addressTable[i].nodeId;
				zoneType = addressTable[i].nodeType;

				clusterId = ZCL_POWER_CONFIG_CLUSTER_ID; // battery voltage
				attributeId = ZCL_BATTERY_VOLTAGE_ATTRIBUTE_ID;
				status = zbdDoBindReport(nodeId, zoneType, clusterId, attributeId);

				clusterId = ZCL_DIAGNOSTICS_CLUSTER_ID; // rssi
				attributeId = ZCL_LAST_MESSAGE_RSSI_ATTRIBUTE_ID;
				status = zbdDoBindReport(nodeId, zoneType, clusterId, attributeId);
			}
			else
			{
				is_empty = FALSE;
			}
		}
	}

	if (TRUE == is_empty)
	{
		DBGPRINT("configBindingEventHandler: Inactive\n");
		emberEventControlSetInactive(configBindingEvent);
	}
}

static void zbdSetNodeType(EmberNodeId nodeId, int nodeType)
{
	int8u index = getAddressIndexFromNodeId(nodeId);
	if (index != NULL_INDEX)
	{
		if (addressTable[index].nodeType == nodeType)
		{
			return;
		}

		addressTable[index].nodeType = nodeType;

		char full_name[80];
		zbdMakeNodeTypeFileName(full_name, sizeof(full_name), addressTable[index].eui64);
		DBGPRINT("NodeType file  Name:%s \n", full_name);

		FILE *fp = fopen(full_name, "w");
		if (fp)
		{
			fprintf(fp, "0x%X\n", nodeType);
			fclose(fp);
		}
	}
	DBGPRINT("zbdSetNodeType: index:%d, nodeType:%d \n", index, nodeType);
}

static int zbdGetNodeType(EmberNodeId nodeId)
{
	int8u index = getAddressIndexFromNodeId(nodeId);
	int node_type = ZB_DEV_UNKNOWN;

	if (index != NULL_INDEX)
	{
		node_type = addressTable[index].nodeType;
	}

	DBGPRINT("zbdGetNodeType: index:%d, nodeId:0x%04X, node_type:0x%04X \n", index, nodeId, node_type);
	return node_type;
}

static int zbdIsNodeSensorByAddressIndex(int8u addressIndex)
{
	int ret = FALSE;

	if (addressIndex == NULL_INDEX)
		return FALSE;

	if (addressIndex < 0 || addressIndex > MAX_INDEX_ADDRESS_TABLE)
		return FALSE;

	switch(addressTable[addressIndex].nodeType)
	{
		case ZB_ZONE_FOB:
		case ZB_ZONE_MOTION:
		case ZB_ZONE_DOOR:
		case ZB_ZONE_FIRE_ALARM:
		case ZB_DEV_IAS_ZONE:
			ret = TRUE;
			break;
		default:
			ret = FALSE;
	}

	return ret;
}

static int zbdIsNodeSensor(EmberNodeId nodeId)
{
	int ret = FALSE;

	int8u index = getAddressIndexFromNodeId(nodeId);
	ret = zbdIsNodeSensorByAddressIndex(index);

	DBGPRINT("zbdIsNodeSensor index:%d, ret:%d, nodeType:0x%04x \n", index, ret, addressTable[index].nodeType);
	return ret;
}

void zbdTriggerBindReport(EmberNodeId nodeId, int16u zoneType)
{
	int16u nowQS;
	int8u index;

	nowQS = halCommonGetInt16uQuarterSecondTick();
	index = getAddressIndexFromNodeId(nodeId);

	zbdSetNodeType(nodeId, zoneType);

	if (NULL_INDEX != index)
	{
		addressTable[index].timeQSAction = nowQS + 1; // 1 = 0.25 sec
		addressTable[index].nodeState = NODE_ST_REPORT_REQUESTING;

		emberEventControlSetActive(configBindingEvent);
		DBGPRINT("zbdTriggerBindReport: nodeId:0x%04X, timeQSAction:%d, zoneType:0x%04x \n", nodeId, addressTable[index].timeQSAction, zoneType);
	}
}

static void zbdDbgPrintByteStream(unsigned char *byteStream, int length)
{
	int    i;
	DBGPRINT("Byte Stream:");
	for (i=0;i < length; i++)
	{
		DBGPRINT("%02x ", byteStream[i]);
	}
	DBGPRINT("\n");
}

static EmberStatus zbdSetSourceRoute(EmberNodeId id)
{
	int16u relayList[ZA_MAX_HOPS];
	int8u relayCount;

	if (emberFindSourceRoute(id, &relayCount, relayList)
			&& ezspSetSourceRoute(id, relayCount, relayList) != EMBER_SUCCESS)
	{
		DBGPRINT("EMBER_SOURCE_ROUTE_FAILURE \n");
		return EMBER_SOURCE_ROUTE_FAILURE;
	}

	DBGPRINT("zbdSetSourceRoute success\n");
	return EMBER_SUCCESS;
}

int zbdConfigureBindingCommand(EmberNodeId nodeId, int16u clusterId)
{
	EmberEUI64 sourceEui, destEui;
	EmberStatus status;

	/*clusterId = ZCL_POWER_CONFIG_CLUSTER_ID; // battery voltage*/
	/*clusterId = ZCL_DIAGNOSTICS_CLUSTER_ID; // rssi*/

	status = emberLookupEui64ByNodeId(nodeId, sourceEui);
	if(status != EMBER_SUCCESS) {
		DBGPRINT("No Address Table Entry for 0x%02x \n", nodeId);
		return -1;
	}

	emberAfGetEui64(destEui); // use local EUI as destination EUI

	status = emberBindRequest(nodeId,          // who gets the bind req
			sourceEui,       // source eui IN the binding
			1,               // source endpoint
			clusterId,    // cluster ID
			UNICAST_BINDING, // binding type
			destEui,         // destination eui IN the binding
			0,               // groupId for new binding
			HOMESENSE_DEST_ENDPOINT,               // destination endpoint
			EMBER_AF_DEFAULT_APS_OPTIONS);

	return status;
}

int zbdConfigureReportCommand(EmberNodeId nodeId, int16u zoneType, int16u clusterId, int16u attributeId)
{
	int16u destination = nodeId;
	EmberStatus status;
	int16u timeSec;

	/*clusterId = ZCL_POWER_CONFIG_CLUSTER_ID; // battery voltage*/
	/*attributeId = ZCL_BATTERY_VOLTAGE_ATTRIBUTE_ID;*/

	// For test purposes, 20 seconds.
	// It should be 30 minutes 30 * 60 = 1800
	switch(zoneType)
	{
		case ZB_ZONE_FOB: // FOB
		       timeSec = 12 * 60 * 60;
		       break;
		case ZB_ZONE_MOTION: // Motion
		       timeSec = 30 * 60;
		       break;
		case ZB_ZONE_DOOR: // Door/Window
		       timeSec = 30 * 60;
		       break;
		case ZB_ZONE_FIRE_ALARM: // Fire Sensor
		       timeSec = 30 * 60;
		       break;
		default:
		       timeSec = 30 * 60;
		       break;
	}

	if (IsFileExisting(ZBD_FILE_REPORT_TIME))
	{
		int time_read;
		if ( SUCCESS == zbdReadIntFromFile(ZBD_FILE_REPORT_TIME, &time_read))
		{
			timeSec = (int16u) time_read & 0xFFFF;

			s_BatteryReportThresholdSec = timeSec * 3;
			DBGPRINT("Presence: DBG Threshold Read : %d \n", s_BatteryReportThresholdSec);
		}
		else
		{
			timeSec = 5 * 60;
		}
	}

	DBGPRINT("zbdConfigureReportCommand: nodeId:0x%02X zoneType:0x%04X sec:%d clusterid:%d attribute:%d \n", destination, zoneType, timeSec, clusterId, attributeId);

	zclBufferSetup(ZCL_PROFILE_WIDE_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER,
	/*zclBufferSetup(ZCL_PROFILE_WIDE_COMMAND | ZCL_FRAME_CONTROL_SERVER_TO_CLIENT,*/
			(EmberAfClusterId) clusterId, // cluster id
			ZCL_CONFIGURE_REPORTING_COMMAND_ID);
	zclBufferAddByte(EMBER_ZCL_REPORTING_DIRECTION_REPORTED);
	zclBufferAddWord(attributeId);  // attribute id

	if (ZCL_BATTERY_VOLTAGE_ATTRIBUTE_ID == attributeId)
	{
		zclBufferAddByte(ZCL_INT8U_ATTRIBUTE_TYPE);           // type
	}
	else if (ZCL_LAST_MESSAGE_RSSI_ATTRIBUTE_ID == attributeId)
	{
		zclBufferAddByte(ZCL_INT8S_ATTRIBUTE_TYPE);           // type
	}
	else
	{
		zclBufferAddByte(ZCL_INT8U_ATTRIBUTE_TYPE);           // type
	}

	zclBufferAddWord(timeSec);  // minimum reporting interval in seconds
	zclBufferAddWord(timeSec);  // maximum reporting interval in seconds
	/*zclBufferAddByte(0);   // report any change.*/
	zbdCmdBuilt();

	status = zbdZigBeeSendCommand(destination, 1, HOMESENSE_DEST_ENDPOINT, 0);

	return status;
}

static void zbdSetTimeToAbort(void)
{
	s_TimeToAbort = halCommonGetInt16uQuarterSecondTick();
	s_TimeToAbort += (TIMEOUT_SEC * 4);
	DBGPRINT("zbdSetTimeToAbort s_TimeToAbort:%d\n", s_TimeToAbort);
}

static void zbdSetState(TZBD_States paramState)
{
	s_PrevState = s_State;
	s_State = paramState;
	DBGPRINT("[%s]--->[%s]\n", s_StateNames[s_PrevState], s_StateNames[s_State]);
}

static int zbdStateIsAsyncState(void)
{
	return (s_State == ZBD_ST_IPC_RECV);
}

static void zbdStateIOMode(int ioMode)
{
	if (ioMode == ZBD_IO_SYNC_SET)
	{
		zbdSetState(ZBD_ST_ZB_SET);
		zbdSetState(ZBD_ST_ZB_MSG_SENT_STATUS);
	}
	else if (ioMode == ZBD_IO_ASYNC)
	{
		zbdSetState(ZBD_ST_IPC_RECV);
	}
	else if (ioMode == ZBD_IO_SYNC_GET)
	{
		zbdSetState(ZBD_ST_ZB_GET);
		zbdSetState(ZBD_ST_ZB_MSG_SENT_STATUS);
	}
}

static char * zbdEui64ToStr(EmberEUI64 nodeEui64)
{
	int i;
	char str_temp[3];

	memset(s_StrEui64, 0, sizeof(s_StrEui64));

	for (i=0; i <= 7 ; ++i)
	{
		memset(str_temp, 0, sizeof(str_temp));
		sprintf(str_temp, "%02X", nodeEui64[7 - i]);

		strcat(s_StrEui64, str_temp);
	}

	return s_StrEui64;
}

static int zbdMakeEuiFileName(char * fileName, int sizeFileName, EmberEUI64 nodeEui64)
{
	if (!fileName)
	{
		return -1;
	}

	memset(fileName, 0, sizeFileName);

	/*snprintf(fileName, sizeFileName, "/tmp/Belkin_settings/zbid.%s", zbdEui64ToStr(nodeEui64));*/
	snprintf(fileName, sizeFileName, DIR_ZIGBEED_CONFIG "zbid.%s", zbdEui64ToStr(nodeEui64));
	return 0;
}

static int zbdMakeNodeTypeFileName(char * fileName, int sizeFileName, EmberEUI64 nodeEui64)
{
	int ret;

	ret = zbdMakeEuiFileName(fileName, sizeFileName, nodeEui64);
	if (ret != SUCCESS)
		return ret;

	strncat(fileName, ".type", sizeFileName - 1);

	return 0;
}

static int zbdMakeClustersFileName(char * fileName, int sizeFileName, EmberEUI64 nodeEui64, int endpoint)
{
	int ret;
	char str_temp[10];

	ret = zbdMakeEuiFileName(fileName, sizeFileName, nodeEui64);
	if (ret != SUCCESS)
		return ret;

	snprintf(str_temp, sizeof(str_temp), ".%02X", endpoint);
	strncat(fileName, str_temp, sizeFileName - 1);

	return 0;
}

static int zbdMakeAttributesFileName(char * fileName, int sizeFileName, EmberEUI64 nodeEui64, int endpoint, int clusterId)
{
	int ret;
	char str_temp[10];

	ret = zbdMakeClustersFileName(fileName, sizeFileName, nodeEui64, endpoint);
	if (ret != SUCCESS)
		return ret;

	/*snprintf(fileName, sizeFileName, "/tmp/Belkin_settings/zbid.%s.%02X.%04X", zbdEui64ToStr(nodeEui64), endpoint, clusterId);*/

	snprintf(str_temp, sizeof(str_temp), ".%04X", clusterId);
	strncat(fileName, str_temp, sizeFileName - 1);

	return 0;
}

static int zbdIsValidNodeTypeFile(const char *fullpathFile)
{
	int len;

	len = strlen(fullpathFile);
	DBGPRINT("zbdIsValidNodeTypeFile, fullpathFile:%s len:%d zbidlen:%d\n", fullpathFile, len, ZBID_TYPE_FILE_LENGTH);

	if (len != ZBID_TYPE_FILE_LENGTH)
	{
		DBGPRINT("Not Type file:%s Wrong length \n", fullpathFile);
		return FALSE;
	}

	if (strstr(fullpathFile, ".type") == NULL)
	{
		DBGPRINT("Not Type file:%s No type \n", fullpathFile);
		return FALSE;
	}

	return TRUE;
}

static int zbdIsValidNodeIdFile(const char *fullpathFile)
{
	int len;

	len = strlen(fullpathFile);
	/*DBGPRINT("zbdIsValidNodeIdFile fullpathFile:%s len:%d zbidlen:%d\n", fullpathFile, len, ZBID_FILE_LENGTH);*/

	if (len != ZBID_FILE_LENGTH)
	{
		DBGPRINT("Not NODEID file:%s Wrong length\n", fullpathFile);
		return FALSE;
	}

	return TRUE;
}

static int zbid2EUI(const char *fullpathFile, EmberEUI64 nodeEUI)
{
	unsigned char *ptr_eui = nodeEUI;

	if (!zbdIsValidNodeIdFile(fullpathFile))
	{
		return FAILURE;
	}

	/*sscanf(fullpathFile, "/tmp/Belkin_settings/zbid.%2hhX%2hhX%2hhX%2hhX%2hhX%2hhX%2hhX%2hhX", &(ptr_eui[7]), &(ptr_eui[6]), &(ptr_eui[5]), &(ptr_eui[4]), &(ptr_eui[3]), &(ptr_eui[2]), &(ptr_eui[1]), &(ptr_eui[0]) );*/
	sscanf(fullpathFile, DIR_ZIGBEED_CONFIG "zbid.%2hhX%2hhX%2hhX%2hhX%2hhX%2hhX%2hhX%2hhX", &(ptr_eui[7]), &(ptr_eui[6]), &(ptr_eui[5]), &(ptr_eui[4]), &(ptr_eui[3]), &(ptr_eui[2]), &(ptr_eui[1]), &(ptr_eui[0]) );

	printEUI64(nodeEUI);
	DBGPRINT("\n");

	return SUCCESS;
}

static int zbdUpdateDBNodeId(EmberEUI64 newNodeEui64, EmberNodeId newNodeId)
{
	FILE *fp;
	char full_name[80];

	DBGPRINT("zbdUpdateDBNodeId eui:");
	printEUI64(newNodeEui64);
	DBGPRINT(" nodeid:0x%02x \n", newNodeId);

	newDeviceJoinHandler(newNodeId, newNodeEui64);

	zbdMakeEuiFileName(full_name, sizeof(full_name), newNodeEui64);
	DBGPRINT("Full Name:%s \n", full_name);

	fp = fopen(full_name, "w");
	if (fp)
	{
		fprintf(fp, "0x%02X\n", newNodeId);
		fclose(fp);

		return SUCCESS;
	}

	return ERROR;
}

static void zbdJsonGetSeparatorString(int memberCount, char *strSep, int sizeStr)
{
	if (memberCount == 0)
	{
		strncpy(strSep, "\n", sizeStr - 1);
	}
	else
	{
		strncpy(strSep, " ,\n", sizeStr - 1);
	}
}

static void zbdJsonWriteStringMember(FILE *fp, int indentLevel, const char *strMember)
{
	if (fp && strMember)
	{
		int i;
		for (i=0; i<indentLevel; i++)
		{
			fprintf(fp, "  ");
		}
		fprintf(fp, "\"%s\"", strMember);
	}
}

static void zbdJsonWritePairString(FILE *fp, const char *strPrepend, int indentLevel, const char *strAttribute, const char *strValue)
{
	if (fp && strAttribute)
	{
		if (strPrepend)
		{
			fprintf(fp, "%s", strPrepend);
		}

		zbdJsonWriteStringMember(fp, indentLevel, strAttribute);
		fprintf(fp, ": ");

		if (strValue)
		{
			zbdJsonWriteStringMember(fp, 0, strValue);
		}
		else
		{
			fprintf(fp, "null");
		}
	}
}

static void zbdJsonWritePairWordAttribute(FILE *fp, const char *strPrepend, int indentLevel, int16u wordParam, const char *strValue)
{
	char str_attribute[30];

	sprintf(str_attribute, "0x%04X", wordParam);
	zbdJsonWritePairString(fp, strPrepend, 1, str_attribute, strValue);
}

static void zbdJsonWritePairWordAttributeList(FILE *fp, int memberCount, int indentLevel, int16u wordParam, const char *strValue)
{
	char str_sep[10];

	zbdJsonGetSeparatorString(memberCount, str_sep, sizeof(str_sep));
	zbdJsonWritePairWordAttribute(fp, str_sep, indentLevel, wordParam, strValue);
}

static void zbdJsonWritePairWordValue(FILE *fp, const char *strPrepend, int indentLevel, const char *strAttribute, int16u wordParam)
{
	char str_value[30];

	sprintf(str_value, "0x%04X", wordParam);
	zbdJsonWritePairString(fp, NULL, 1, strAttribute, str_value);
}

static void zbdJsonWriteArrayWord(FILE *fp, int indentLevel, const char *strName, const int16u *parrayWord, int arrayCount)
{
	int i;
	char str_value[30];

	zbdJsonWriteStringMember(fp, indentLevel, strName);
	fprintf(fp, ": [\n");
	for (i = 0; i < arrayCount; i++)
	{
		if (i > 0)
		{
			fprintf(fp, ",\n");
		}

		sprintf(str_value, "0x%04X", parrayWord[i]);
		zbdJsonWriteStringMember(fp, indentLevel + 1, str_value);
	}
	fprintf(fp, "\n  ]");
}

static int zbdSaveClusters(EmberNodeId nodeId, int8u endpoint, int16u deviceId, const int16u * pclusterList, int clusterCount)
{
	FILE *fp;
	char fullpath_file[80];
	EmberEUI64 eui64;

	getIeeeFromNodeId(nodeId, eui64);
	zbdMakeClustersFileName(fullpath_file, sizeof(fullpath_file), eui64, endpoint);
	DBGPRINT("zbdSaveClusters fullpath_file:%s \n", fullpath_file);

	zbdAddressSetEndpoint(nodeId, endpoint);

	fp = fopen(fullpath_file, "w+");
#if 1
	if (fp)
	{
		if (pclusterList)
		{
			fprintf(fp, "{\n");
			zbdJsonWritePairWordValue(fp, NULL, 1, "device id", deviceId);

			fprintf(fp, ",\n");
			zbdJsonWriteArrayWord(fp, 1, "clusters", pclusterList, clusterCount);

			zbdJsonWritePairString(fp, ",\n", 1, "zone type", NULL);
			fprintf(fp, "\n}\n");
		}
		fclose(fp);
		return SUCCESS;
	}
#else
	if (fp)
	{
		int i;

		fprintf(fp, "device_id\n");
		fprintf(fp, "0x%04x\n", deviceId);

		fprintf(fp, "clusters\n");
		for (i = 0; i < clusterCount; i++)
		{
			fprintf(fp, "0x%04x\n", pclusterList[i]);
		}

		fprintf(fp, "done\n");
		fclose(fp);

		return SUCCESS;
	}
#endif

	return ERROR;
}

// command to send the CIE IEEE address to the IAS Zone cluster
static EmberStatus sendCieAddressWrite( EmberNodeId nodeId, int8u endpoint )
{
	EmberStatus status;
	int i;
	EmberEUI64 eui64;

	emberAfGetEui64(eui64);

	globalApsFrame.options = EMBER_AF_DEFAULT_APS_OPTIONS;
	globalApsFrame.clusterId = ZCL_IAS_ZONE_CLUSTER_ID;
	globalApsFrame.sourceEndpoint = 0x01;
	globalApsFrame.destinationEndpoint = endpoint;

	outgoingBuffer[0] = 0x00;
	outgoingBuffer[1] = emberAfNextSequence();
	outgoingBuffer[2] = ZCL_WRITE_ATTRIBUTES_COMMAND_ID;
	outgoingBuffer[3] = LOW_BYTE(ZCL_IAS_CIE_ADDRESS_ATTRIBUTE_ID);
	outgoingBuffer[4] = HIGH_BYTE(ZCL_IAS_CIE_ADDRESS_ATTRIBUTE_ID);
	outgoingBuffer[5] = ZCL_IEEE_ADDRESS_ATTRIBUTE_TYPE;

	for(i=0; i<8; i++) {
		outgoingBuffer[6+i] = eui64[i];
	}

	status = emberAfSendUnicast(EMBER_OUTGOING_DIRECT,
			nodeId,
			&globalApsFrame,
			14,
			outgoingBuffer);

	DBGPRINT("sendCieAddressWrite nodeId:0x%04x, endpoint:%d status:0x%x \n", nodeId, endpoint, status);

	return status;
}

static void zbdSysEventInit(void)
{
	int index = 0;
	char str_cmd[80];
	int trial = 1;

	while(s_ZBEventNames[index][0])
	{
		snprintf(str_cmd, sizeof(str_cmd), "sysevent setoptions %s 0x00000002", s_ZBEventNames[index]);
		int ret = system(str_cmd);
		DBGPRINT("trial:%d ret:%d str_cmd[%d]: %s \n", trial, ret, index, str_cmd);
		if (0 == ret || 3 < trial)
		{
			index++;
			trial = 1;
		}
		else
		{
			trial++;
		}
	}
}

static void zbdSysEventSingleCode(const char* strSysEvent, EmberNodeId nodeId, int codeParam)
{
	EmberEUI64 eui64;
	char cmdstr[80];

	if (nodeId != 0 && NULL_INDEX == getAddressIndexFromNodeId(nodeId))
	{
		DBGPRINT("node:0x%04x not found in the table\n", nodeId);
	}

	memset(eui64, 0, sizeof(eui64));
	getIeeeFromNodeId(nodeId, eui64);

	// send sysevent
	snprintf(cmdstr, sizeof(cmdstr), "sysevent set %s \"%s %d\" ", strSysEvent, zbdEui64ToStr(eui64), codeParam);
	DBGPRINT("%s\n", cmdstr);
	system(cmdstr);
}

static void zbdSysEventDoubleCode(const char* strSysEvent, EmberNodeId nodeId, int codeParam1, int codeParam2)
{
	EmberEUI64 eui64;
	char cmdstr[80];

	if (nodeId != 0 && NULL_INDEX == getAddressIndexFromNodeId(nodeId))
	{
		DBGPRINT("node:0x%04x not found in the table\n", nodeId);
		return;
	}

	memset(eui64, 0, sizeof(eui64));
	getIeeeFromNodeId(nodeId, eui64);

	// send sysevent
	snprintf(cmdstr, sizeof(cmdstr), "sysevent set %s \"%s %d %d\" ", strSysEvent, zbdEui64ToStr(eui64), codeParam1, codeParam2);
	DBGPRINT("%s\n", cmdstr);
	system(cmdstr);
}

static void zbdSysEventErrorCode(EmberNodeId nodeId, int errorCode, int clusterId)
{
	zbdSysEventDoubleCode(ZBD_EVENT_ERROR_STATUS, nodeId, errorCode, clusterId);
}

void zbdSysEventOtaNoNextImage(void)
{
	zbdSysEventSingleCode(ZBD_EVENT_OTA_STATUS, s_LastNodeId, 300);
}

static void zbdScheduleCommandRequest(EmberNodeId nodeId, int nodeState, int16u timeQS, EmberEventControl *pemberEvent)
{
	int8u index;

	index = getAddressIndexFromNodeId(nodeId);
	if (NULL_INDEX != index)
	{
		addressTable[index].timeQSAction = timeQS;
		addressTable[index].nodeState = nodeState;

		emEventControlSetActive(pemberEvent);

		int16u nowQS = halCommonGetInt16uQuarterSecondTick();
		DBGPRINT("zbdScheduleCommandRequest: nodeId:0x%04X, nodeState:%d, action after QS:%d \n", nodeId, nodeState, addressTable[index].timeQSAction - nowQS);
	}
}

static void zbdScheduleAddressRequest(EmberNodeId nodeId)
{
	int16u nowQS;
	int8u index;
	int16u timeQS;
	int16u timeQSAction;

	DBGPRINT("zbdScheduleAddressRequest \n");

	zbdSetSourceRoute(nodeId);
	timeQS = (int16u) emberAfPluginConcentratorQueueDiscovery();
	if(timeQS > 20) {
		timeQS = 20;
	}
	DBGPRINT("MTORR +emberAfPluginConcentratorQueueDiscovery: timeQS:%d \n", timeQS);

	nowQS = halCommonGetInt16uQuarterSecondTick();
	timeQSAction = nowQS + timeQS + 2;

	/*zbdScheduleCommandRequest(nodeId, NODE_ST_ADDRESS_REQUESTING, timeQSAction, &periodicEvent);*/
	index = getAddressIndexFromNodeId(nodeId);
	if (NULL_INDEX != index)
	{
		addressTable[index].timeQSAction = timeQSAction;
		addressTable[index].isRepairing = TRUE;

		emEventControlSetActive(&periodicEvent);
		DBGPRINT("zbdScheduleAddressRequest: nodeId:0x%04X,  timeQSAction:%d \n", nodeId, addressTable[index].timeQSAction);
	}
}

static void zbdScheduleEndpointsRequest(EmberNodeId nodeId, int16u timeQS)
{
	int8u index;

	DBGPRINT("zbdScheduleEndpointsRequest 0x%04x, timeQS:%d \n", nodeId, timeQS);

	zbdScheduleCommandRequest(nodeId, NODE_ST_ENDPOINT_REQUESTING, timeQS, &nodeCommandEvent);
}

static void zbdScheduleLeaveRequest(EmberNodeId nodeId)
{
	int16u nowQS;
	int8u index;

	nowQS = halCommonGetInt16uQuarterSecondTick();

	zbdScheduleCommandRequest(nodeId, NODE_ST_LEAVE_REQUESTING, nowQS, &wakeupSleepyEvent);
}

static void zbdAddressSetEndpoint(EmberNodeId nodeId, int8u endpoint)
{
	int8u index;

	index = getAddressIndexFromNodeId(nodeId);
	if (NULL_INDEX != index)
	{
		DBGPRINT("zbdAddressSetEndpoint nodeId:0x%04x, endpoint:%d\n", nodeId, endpoint);
		addressTable[index].endpoint = endpoint;
	}
}

static void zbdScheduleBasicClusterRequest(EmberNodeId nodeId, int8u endpoint)
{
	int16u nowQS;
	int8u index;

	nowQS = halCommonGetInt16uQuarterSecondTick();

	/*zbdScheduleCommandRequest(nodeId, NODE_ST_BASICCLUSTER_REQUESTING, nowQS, &readBasicClusterEvent);*/
	index = getAddressIndexFromNodeId(nodeId);
	if (NULL_INDEX != index)
	{
		addressTable[index].timeQSAction = nowQS;

		emEventControlSetActive(&readBasicClusterEvent);
		DBGPRINT("zbdScheduleBasicClusterRequest: nodeId:0x%04X,  timeQSAction:%d \n", nodeId, addressTable[index].timeQSAction);
	}
}

static void zbdScheduleCieWriting(EmberNodeId nodeId, int16u timeQS)
{
	int8u index;

	DBGPRINT("zbdScheduleCieWriting 0x%04x, timeQS:%d \n", nodeId, timeQS);

	zbdScheduleCommandRequest(nodeId, NODE_ST_CIE_WRITING, timeQS, &nodeCommandEvent);
}

static int zbdIpcUdsSend(pTIPCMsg sa_resp, int aChildFD)
{
	if(sa_resp == NULL)
	{
		DBGPRINT("zbdIpcUdsSend:response structure is empty\n");
		return ERROR;
	}

	if (send(aChildFD, (TIPCMsg*)sa_resp, (IPC_CMD_HDR_LEN + sa_resp->hdr.arg_length), 0) < 0)
	{
		DBGPRINT("zbdIpcUdsSend:Socket Sent error:%s\n", strerror(errno));
	}

	return SUCCESS;
}

static void zbdFillResponseHeader(TIPCMsg *pcmdMsg, TIPCMsg *prespMsg, int argLen, int errorCode)
{
	//fill response header
	if (pcmdMsg)
	{
		prespMsg->hdr.cmd = pcmdMsg->hdr.cmd;
	}
	// Return errorCode in param1
	prespMsg->hdr.param1 = errorCode;
	prespMsg->hdr.arg_length = argLen;
}

static int zbdReplyNoPayloadResponse(TIPCMsg *pcmdMsg, int aclientFD, int errorCode)
{
	pTIPCMsg       p_resp_msg;
	IPCGeneralResp payload_resp;
	int            len_msg;

	DBGPRINT("Entering zbdReplyNoPayloadResponse errorCode:%d\n", errorCode);

	len_msg = IPC_CMD_HDR_LEN;

	p_resp_msg    = (pTIPCMsg)calloc(len_msg, 1);
	if(p_resp_msg == NULL)
	{
		DBGPRINT("calloc failed.\n");
		return ERROR;
	}

	//fill response header
	zbdFillResponseHeader(pcmdMsg, p_resp_msg, 0, errorCode);

	//send it on socket
	zbdIpcUdsSend((pTIPCMsg)p_resp_msg, aclientFD);

	free(p_resp_msg);

	DBGPRINT("Leaving zbdReplyNoPayloadResponse\n");
	return SUCCESS;
}

static int zbdReplyGeneralResponse(TIPCMsg *pcmdMsg, int aclientFD, int respValue, int errorCode)
{
	pTIPCMsg       p_resp_msg;
	IPCGeneralResp payload_resp;
	int            len_msg;

	DBGPRINT("Entering zbdReplyGeneralResponse, respValue:0x%08X, errorCode:%d \n", respValue, errorCode);

	if (errorCode != ZB_RET_SUCCESS)
	{
		return zbdReplyNoPayloadResponse(pcmdMsg, aclientFD, errorCode);
	}

	len_msg = IPC_CMD_HDR_LEN + sizeof(IPCGeneralResp);

	p_resp_msg = (pTIPCMsg)calloc(len_msg, 1);
	if(p_resp_msg == NULL)
	{
		DBGPRINT("calloc failed.\n");
		return ERROR;
	}

	//fill response header
	zbdFillResponseHeader(pcmdMsg, p_resp_msg, sizeof(IPCGeneralResp), errorCode);

	//fill response payload
	payload_resp.value = respValue;
	memcpy((char *)p_resp_msg + IPC_CMD_HDR_LEN, &payload_resp, sizeof(IPCGeneralResp));

	//send it on socket
	zbdIpcUdsSend((pTIPCMsg)p_resp_msg, aclientFD);

	free(p_resp_msg);

	DBGPRINT("Leaving zbdReplyGeneralResponse\n");
	return SUCCESS;
}

static int zbdReplyArrayResponse(TIPCMsg *pcmdMsg, int aclientFD, int byteperMember, const char *dataArr, int numMember, int devType, int errorCode)
{
	pTIPCMsg     p_resp_msg;
	int          len_msg     , len_payload;
	IPCArrayResp arr_hdr;
	int          data_bytes;

	DBGPRINT("Entering zbdReplyArrayResponse\n");

	if (errorCode != ZB_RET_SUCCESS)
	{
		return zbdReplyNoPayloadResponse(pcmdMsg, aclientFD, errorCode);
	}

	data_bytes = numMember * byteperMember;

	len_payload = sizeof(arr_hdr) + data_bytes;

	len_msg = IPC_CMD_HDR_LEN + len_payload;

	DBGPRINT("size arr_hdr:%d, numMember:%d, data_bytes:%d\n", sizeof(arr_hdr), numMember, data_bytes);

	p_resp_msg    = (pTIPCMsg)calloc(len_msg, 1);
	if(p_resp_msg == NULL)
	{
		DBGPRINT("calloc failed.\n");
		return ERROR;
	}

	//fill response header
	zbdFillResponseHeader(pcmdMsg, p_resp_msg, len_payload, errorCode);

	arr_hdr.byte_per_member = byteperMember;
	arr_hdr.n_member = numMember;
	arr_hdr.device_type = devType;

	//fill response payload
	if (dataArr)
	{
		char * ptr;
		ptr = (char *)p_resp_msg + IPC_CMD_HDR_LEN;
		memcpy(ptr, &arr_hdr, sizeof(arr_hdr));

		ptr += sizeof(arr_hdr);
		memcpy(ptr, dataArr, data_bytes);
	}

	//send it on socket
	zbdIpcUdsSend((pTIPCMsg)p_resp_msg, aclientFD);

	free(p_resp_msg);

	DBGPRINT("Leaving zbdReplyArrayResponse\n");
	return SUCCESS;
}

static int zbdReplyStrResponse(TIPCMsg *pcmdMsg, int aclientFD, char *paramStr, int strLen, int errorCode)
{
	pTIPCMsg p_resp_msg;
	int      len_msg;

	DBGPRINT("Entering zbdReplyStrResponse\n");

	if (errorCode != ZB_RET_SUCCESS)
	{
		return zbdReplyNoPayloadResponse(pcmdMsg, aclientFD, errorCode);
	}

	len_msg = IPC_CMD_HDR_LEN + strLen + 1;

	p_resp_msg    = (pTIPCMsg)calloc(len_msg, 1);
	if(p_resp_msg == NULL)
	{
		DBGPRINT("calloc failed.\n");
		return ERROR;
	}

	//fill response header
	zbdFillResponseHeader(pcmdMsg, p_resp_msg, strLen, errorCode);

	//fill response payload
	if (paramStr)
	{
		memcpy((char *)p_resp_msg + IPC_CMD_HDR_LEN, paramStr, strLen);
	}

	//send it on socket
	zbdIpcUdsSend((pTIPCMsg)p_resp_msg, aclientFD);

	free(p_resp_msg);

	DBGPRINT("Leaving zbdReplyStrResponse\n");
	return SUCCESS;
}

static int zbdSendLeaveNetwork(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret    = ZB_RET_SUCCESS;

	status = emberLeaveNetwork();
	DBGPRINT("emberLeaveNetwork status:%x\n", status);

	ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
	zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);

	return SUCCESS;
}

static int zbdLeaveRequest(EmberNodeId paramNodeId, int8u paramFlags)
{
	EmberStatus status;
	EmberEUI64  deviceAddress;

	memset(deviceAddress, 0, sizeof(EmberEUI64));

	DBGPRINT("zbdLeaveRequest: paramNodeId:0x%04x paramFlags:0x%x \n", paramNodeId, paramFlags);

	status = emberLeaveRequest(paramNodeId,
			deviceAddress,
			paramFlags,
			EMBER_AF_DEFAULT_APS_OPTIONS);

	return status;
}

static int zbdSendLeaveRequest(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int16u      destination   = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int16u      multicast     = (int16u) ((pcmdMsg->hdr.zbnode.node_id >> 16) & 0xFFFF);
	int8u       flag          = (int8u) (pcmdMsg->hdr.param1 & 0xFF);

	if (multicast)
	{
		destination = EMBER_SLEEPY_BROADCAST_ADDRESS;
	}

	status = zbdLeaveRequest(destination, flag);
	return status;
}

static int zbdSendNetworkScanJoin(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret    = ZB_RET_SUCCESS;

	status = emberAfStartSearchForJoinableNetwork();
	DBGPRINT("emberAfStartSearchForJoinableNetwork 0x%x \n",  status);

	ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
	zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);

	return status;
}

static int zbdSendGetNetworkInfo(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus            status;
	EmberNodeType          nodeTypeResult = 0xFF;
	EmberNetworkParameters networkParams;
	int                    ret            = ZB_RET_SUCCESS;
	int8u                  resp_list[2];

	status = emberAfGetNetworkParameters(&nodeTypeResult, &networkParams);
	DBGPRINT("chan [%d] pwr [%d]\n", networkParams.radioChannel, networkParams.radioTxPower);

	resp_list[0] = (int8u)networkParams.radioChannel;
	resp_list[1] = (int8u)networkParams.radioTxPower;

	ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
	zbdReplyArrayResponse(NULL, aclientFD, 1, resp_list, 2, 0, ret);

	return status;
}

static int zbdSendMTORR(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret    = ZB_RET_SUCCESS;

	status = emberSendManyToOneRouteRequest( EMBER_HIGH_RAM_CONCENTRATOR, EMBER_MAX_HOPS);
	DBGPRINT("emberSendManyToOneRouteRequest status:0x%x \n", status);

	ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
	zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);

	return status;
}

static int zbdSendFormNetwork(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret    = ZB_RET_SUCCESS;

	status = emberAfFindUnusedPanIdAndForm();
	emberAfAppPrintln("%p 0x%x", "emberAfFindUnusedPanIdAndForm",  status);

	ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
	zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);

	return status;
}

static int zbdSendChangeNetworkChannel(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret     = ZB_RET_SUCCESS;
	int8u       channel = (int8u) (pcmdMsg->hdr.param1 & 0xFF);

	DBGPRINT("zbdSendChangeNetworkChannel: chan [%d]\n", channel);

	if (channel < EMBER_MIN_802_15_4_CHANNEL_NUMBER
			|| channel > EMBER_MAX_802_15_4_CHANNEL_NUMBER)
	{
		emberAfAppPrintln("invalid channel: %d", channel);
		ret = ZB_RET_FAIL;
	}
	else
	{
		status = emberChannelChangeRequest(channel);
		emberAfAppPrintln("%p 0x%x", "emberChannelChangeRequest",  status);
		ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
	}

	zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);

	return status;
}

static int zbdSendPermitJoin(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret      = ZB_RET_SUCCESS;
	int8u       duration = (int8u) (pcmdMsg->hdr.param1 &0xFF);
	boolean     b_bcast  = TRUE;

	status = emAfPermitJoin(duration, b_bcast);  // broadcast permit join?
	DBGPRINT("emAfPermitJoin duration:%d b_bcast:%d status:0x%X \n", duration, b_bcast,  status);

	ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
	zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);

	return status;
}

static int zbdSendOnOff(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int16u      multicast = (int16u) ((pcmdMsg->hdr.zbnode.node_id >> 16) & 0xFFFF);
	int8u       dstEndpoint = (int8u) (pcmdMsg->hdr.zbnode.endpoint & 0xFF);
	int8u       onoff       = (int8u) (pcmdMsg->hdr.param1 & 0xFF);
	int8u       commandId   = (onoff == ZB_POWER_ON ? ZCL_ON_COMMAND_ID : (onoff == ZB_POWER_OFF ? ZCL_OFF_COMMAND_ID : ZCL_TOGGLE_COMMAND_ID));

	DBGPRINT("zbdSendOnOff: destination:0x%x dstEndpoint:0x%x command:0x%x\n", destination, dstEndpoint, onoff);
	/*zclSimpleClientCommand(ZCL_ON_OFF_CLUSTER_ID, commandId);*/
	zclBufferSetup((ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER| ZCL_DISABLE_DEFAULT_RESPONSE_MASK), ZCL_ON_OFF_CLUSTER_ID, commandId);
	zbdCmdBuilt();

	status = zbdZigBeeSendCommand(destination, 1, dstEndpoint, multicast);

	if ( zbdStateIsAsyncState() )
	{
		ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
		zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);
	}
	return status;
}

static int zbdSendGroupAddRemove(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int16u      multicast = 0;
	int8u       dstEndpoint = (int8u) (pcmdMsg->hdr.zbnode.endpoint & 0xFF);
	int8u       group_add   = (int8u) (pcmdMsg->hdr.param1 & 0xFF);
	int16u      group_id    = (int16u) (pcmdMsg->hdr.param2 & 0xFFFF);
	int8u       commandId   = ZCL_ADD_GROUP_COMMAND_ID;

	if (group_add == ZB_GROUP_REMOVE)
	{
		commandId = ZCL_REMOVE_GROUP_COMMAND_ID;
	}

	DBGPRINT("zbdSendGroupAddRemove: destination:0x%x dstEndpoint:0x%x group_add:%d group_id:0x%04X, commandId:0x%02x\n", destination, dstEndpoint, group_add, group_id, commandId);

	zclBufferSetup((ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER| ZCL_DISABLE_DEFAULT_RESPONSE_MASK), ZCL_GROUPS_CLUSTER_ID, commandId);
	zclBufferAddWord(group_id);
	zclBufferAddByte(0x00); // Empty String
	zbdCmdBuilt();

	status = zbdZigBeeSendCommand(destination, 1, dstEndpoint, multicast);

	if ( zbdStateIsAsyncState() )
	{
		ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
		zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);
	}
	return status;
}

static int zbdSendLevelStop(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int16u      multicast = (int16u) ((pcmdMsg->hdr.zbnode.node_id >> 16) & 0xFFFF);
	int8u       dstEndpoint = (int8u) (pcmdMsg->hdr.zbnode.endpoint & 0xFF);
	int8u       commandId   = ZCL_STOP_COMMAND_ID;

	DBGPRINT("zbdSendLevelStop: destination:0x%x dstEndpoint:0x%x\n", destination, dstEndpoint);
	zclBufferSetup((ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER| ZCL_DISABLE_DEFAULT_RESPONSE_MASK), ZCL_LEVEL_CONTROL_CLUSTER_ID, commandId);

	zbdCmdBuilt();
	status = zbdZigBeeSendCommand(destination, 1, dstEndpoint, multicast);

	if ( zbdStateIsAsyncState() )
	{
		ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
		zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);
	}
	return status;
}

static int zbdSendLevelMove(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int16u      multicast = (int16u) ((pcmdMsg->hdr.zbnode.node_id >> 16) & 0xFFFF);
	int8u       dstEndpoint = (int8u) (pcmdMsg->hdr.zbnode.endpoint & 0xFF);
	int8u       updown       = (int8u) (pcmdMsg->hdr.param1 & 0xFF);
	int8u       rate        = (int8u) (pcmdMsg->hdr.param2 & 0xFF);
	int8u       commandId   = ZCL_MOVE_WITH_ON_OFF_COMMAND_ID;

	DBGPRINT("zbdSendLevelMove: destination:0x%x dstEndpoint:0x%x updown:%d rate:0x%x\n", destination, dstEndpoint, updown, rate);
	zclBufferSetup((ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER| ZCL_DISABLE_DEFAULT_RESPONSE_MASK), ZCL_LEVEL_CONTROL_CLUSTER_ID, commandId);
	zclBufferAddByte(updown);
	zclBufferAddByte(rate);
	zbdCmdBuilt();
	status = zbdZigBeeSendCommand(destination, 1, dstEndpoint, multicast);

	if ( zbdStateIsAsyncState() )
	{
		ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
		zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);
	}
	return status;
}

static int zbdSendLevel(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int16u      multicast   = (int16u) ((pcmdMsg->hdr.zbnode.node_id >> 16) & 0xFFFF);
	int8u       dstEndpoint = (int8u) (pcmdMsg->hdr.zbnode.endpoint & 0xFF);
	int8u       level       = (int8u) (pcmdMsg->hdr.param1 & 0xFF);
	int16u      time_arg        = (int16u) (pcmdMsg->hdr.param2 & 0xFFFF);
	int         with_onoff  = pcmdMsg->hdr.param3;
	int8u       commandId;

	commandId = (with_onoff == 0) ? ZCL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID : ZCL_MOVE_TO_LEVEL_COMMAND_ID ;

	DBGPRINT("zbdSendLevel: destination:0x%x dstEndpoint:0x%x level:%d time_arg:%d onoff:%d \n", destination, dstEndpoint, level, time_arg, with_onoff);
	zclBufferSetup((ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER| ZCL_DISABLE_DEFAULT_RESPONSE_MASK), ZCL_LEVEL_CONTROL_CLUSTER_ID, commandId);
	zclBufferAddByte(level);
	zclBufferAddWord(time_arg);
	zbdCmdBuilt();
	status = zbdZigBeeSendCommand(destination, 1, dstEndpoint, multicast);

	if ( zbdStateIsAsyncState() )
	{
		ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
		zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);
	}
	return status;
}

static int zbdIdentify(EmberNodeId paramNodeId, int8u paramEndpoint, int16u paramTime, int16u multicast)
{
	EmberStatus status;
	int8u       commandId = ZCL_IDENTIFY_COMMAND_ID;

	DBGPRINT("zbdIdentify: paramNodeId:0x%x paramEndpoint:0x%x paramTime:%d\n", paramNodeId, paramEndpoint, paramTime);

	zclBufferSetup((ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER| ZCL_DISABLE_DEFAULT_RESPONSE_MASK), ZCL_IDENTIFY_CLUSTER_ID, commandId);
	zclBufferAddWord(paramTime);
	zbdCmdBuilt();

	status = zbdZigBeeSendCommand(paramNodeId, 1, paramEndpoint, multicast);

	return status;
}

static int zbdSendIdentify(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int16u      multicast = (int16u) ((pcmdMsg->hdr.zbnode.node_id >> 16) & 0xFFFF);
	int8u       dstEndpoint = (int8u) (pcmdMsg->hdr.zbnode.endpoint & 0xFF);
	int16u      time_arg        = (int16u) (pcmdMsg->hdr.param1 & 0xFFFF);

	status = zbdIdentify(destination, dstEndpoint, time_arg, multicast);

	if ( zbdStateIsAsyncState() )
	{
		ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
		zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);
	}
	return status;
}

static int zbdSendSetTestMode(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int16u      multicast   = (int16u) ((pcmdMsg->hdr.zbnode.node_id >> 16) & 0xFFFF);
	int8u       dstEndpoint = (int8u) (pcmdMsg->hdr.zbnode.endpoint & 0xFF);
	int8u       is_enabled  = (int8u) (pcmdMsg->hdr.param1 & 0xFF);

	DBGPRINT("zbdSendSetTestMode: destination:0x%x dstEndpoint:0x%x is_enabled:%d\n", destination, dstEndpoint, is_enabled);

	emberAfFillExternalManufacturerSpecificBuffer((ZCL_CLUSTER_SPECIFIC_COMMAND \
				| ZCL_MANUFACTURER_SPECIFIC_MASK \
				| ZCL_FRAME_CONTROL_CLIENT_TO_SERVER \
				| ZCL_DISABLE_DEFAULT_RESPONSE_MASK), \
			ZCL_BELKIN_MFG_SPECIFIC_CLUSTER_ID, \
			0x10B4, \
			ZCL_BELKIN_MFG_SPECIFIC_TEST_MODE_COMMAND_ID, \
			"u",
			is_enabled);

	emberAfSetCommandEndpoints(1, dstEndpoint);
	status = emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT, destination);
	/*status = zbdZigBeeSendCommand(destination, 1, dstEndpoint, multicast);*/

	if ( zbdStateIsAsyncState() )
	{
		ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
		zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);
	}
	return status;
}

static int zbdSendOTANotify(TIPCMsg *pcmdMsg, int aclientFD)
{
	int         ret         = ZB_RET_SUCCESS;
	EmberStatus status = EMBER_ERR_FATAL;
	int16u      destination  = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int16u      multicast    = (int16u) ((pcmdMsg->hdr.zbnode.node_id >> 16) & 0xFFFF);
	int8u       dstEndpoint  = (int8u) (pcmdMsg->hdr.zbnode.endpoint & 0xFF);
	int16u      payload_type = (int16u) (pcmdMsg->hdr.param1 & 0xFFFF);
	int16u      ota_policy   = (int16u) (pcmdMsg->hdr.param2 & 0xFFFF);
	int8u 	    jitter 	 = 100;

	DBGPRINT("zbdSendOTANotify: destination:0x%x dstEndpoint:0x%x payload_type:%d\n", destination, dstEndpoint, payload_type);

	{
		char cwd[1024];
		DBGPRINT( "Current Working Directory: %s\n", (NULL == getcwd(cwd, 1024) ? "UNKNOWN!" : cwd));
	}

	emAfOtaReloadStorageDevice();

	/*
	   UPGRADE_IF_SERVER_HAS_NEWER = 0,
	   DOWNGRADE_IF_SERVER_HAS_OLDER = 1,
	   REINSTALL_IF_SERVER_HAS_SAME = 2,
	   NO_NEXT_VERSION = 3,
	   */
	emAfOtaServerSetQueryPolicy(ota_policy);

	EmberAfOtaHeader header;
	EmberAfOtaImageId id = emberAfOtaStorageIteratorFirstCallback();
	if (emberAfIsOtaImageIdValid(&id))
	{
		s_OTA_Size = 0;

		DBGPRINT("MFGID:0x%2X \n", id.manufacturerId);
		DBGPRINT("ImageType:0x%2X \n", id.imageTypeId);
		DBGPRINT("FirmwareVersion:0x%2X \n", id.firmwareVersion);
		if (EMBER_AF_OTA_STORAGE_SUCCESS
				!= emberAfOtaStorageGetFullHeaderCallback(&id,
					&header)) {
			DBGPRINT("  ERROR: Could not get full header!\n");
		} else {
			DBGPRINT("  Header Version: 0x%2X\n", header.headerVersion);
			DBGPRINT("  Header Length:  %d bytes\n", header.headerLength);
			DBGPRINT("  Field Control:  0x%2X\n", header.fieldControl);
			DBGPRINT("  Manuf ID:       0x%2X\n", header.manufacturerId);
			DBGPRINT("  Image Type:     0x%2X\n", header.imageTypeId);
			DBGPRINT("  Version:        0x%4X\n", header.firmwareVersion);
			DBGPRINT("  Zigbee Version: 0x%2X\n", header.zigbeeStackVersion);
			DBGPRINT("  Header String:  %s\n",    header.headerString);
			DBGPRINT("  Image Size:     %u bytes\n",    header.imageSize);

			s_OTA_Size = header.imageSize;
		}

		/*id.firmwareVersion = 0xFFFFFFFF;*/
		/*DBGPRINT("Overwrite FirmwareVersion:0x%2X \n", id.firmwareVersion);*/

		status = emberAfOtaServerSendImageNotifyCallback(destination,
				dstEndpoint,
				payload_type,
				jitter,
				&id);

		zbdSysEventSingleCode(ZBD_EVENT_OTA_STATUS, destination, 0);

	}

	if ( zbdStateIsAsyncState() )
	{
		ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
		zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);
	}
	return status;
}

static int zbdSendColor(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int16u      multicast = (int16u) ((pcmdMsg->hdr.zbnode.node_id >> 16) & 0xFFFF);
	int8u       dstEndpoint = (int8u) (pcmdMsg->hdr.zbnode.endpoint & 0xFF);
	int16u      color_x     = (int16u) (pcmdMsg->hdr.param1 & 0xFFFF);
	int16u      color_y     = (int16u) (pcmdMsg->hdr.param2 & 0xFFFF);
	int16u      time_arg        = (int16u) (pcmdMsg->hdr.param3 & 0xFFFF);
	int8u       commandId   = ZCL_MOVE_TO_COLOR_COMMAND_ID;

	DBGPRINT("zbdSendColor: destination:0x%x dstEndpoint:0x%x color_x:0x%02X color_y:0x%02X time_arg:%d\n", destination, dstEndpoint, color_x, color_y, time_arg);
	zclBufferSetup((ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER| ZCL_DISABLE_DEFAULT_RESPONSE_MASK), ZCL_COLOR_CONTROL_CLUSTER_ID, commandId);
	zclBufferAddWord(color_x);
	zclBufferAddWord(color_y);
	zclBufferAddWord(time_arg);
	zbdCmdBuilt();
	status = zbdZigBeeSendCommand(destination, 1, dstEndpoint, multicast);

	if ( zbdStateIsAsyncState() )
	{
		ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
		zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);
	}
	return status;
}

static int zbdSendColorTemp(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int16u      multicast = (int16u) ((pcmdMsg->hdr.zbnode.node_id >> 16) & 0xFFFF);
	int8u       dstEndpoint = (int8u) (pcmdMsg->hdr.zbnode.endpoint & 0xFF);
	int16u      color_temp  = (int16u) (pcmdMsg->hdr.param1 & 0xFFFF);
	int16u      time_arg        = (int16u) (pcmdMsg->hdr.param2 & 0xFFFF);
	int8u       commandId   = ZCL_MOVE_TO_COLOR_TEMPERATURE_COMMAND_ID;

	DBGPRINT("zbdSendColorTemp: destination:0x%x dstEndpoint:0x%x color_temp:0x%02X time_arg:%d\n", destination, dstEndpoint, color_temp, time_arg);
	zclBufferSetup((ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER| ZCL_DISABLE_DEFAULT_RESPONSE_MASK), ZCL_COLOR_CONTROL_CLUSTER_ID, commandId);
	zclBufferAddWord(color_temp);
	zclBufferAddWord(time_arg);
	zbdCmdBuilt();
	status = zbdZigBeeSendCommand(destination, 1, dstEndpoint, multicast);

	if ( zbdStateIsAsyncState() )
	{
		ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
		zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);
	}
	return status;
}

static int zbdSendGlobalReadClient(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int16u      multicast = 0;
	int8u       dstEndpoint = (int8u) (pcmdMsg->hdr.zbnode.endpoint & 0xFF);
	int16u      cluster_id  = (int16u) (pcmdMsg->hdr.param1 & 0xFFFF);
	int16u      attrib_id   = (int16u) (pcmdMsg->hdr.param2 & 0xFFFF);
	int8u       command_id  = ZCL_READ_ATTRIBUTES_COMMAND_ID;

	DBGPRINT("zbdSendGlobalRead: destination:0x%x dstEndpoint:0x%x cluster_id:%d attrib_id:%d\n", destination, dstEndpoint, cluster_id, attrib_id);

	zclBufferSetup(ZCL_PROFILE_WIDE_COMMAND | ZCL_FRAME_CONTROL_SERVER_TO_CLIENT | ZCL_DISABLE_DEFAULT_RESPONSE_MASK,
                 cluster_id,
                 command_id);

	zclBufferAddWord(attrib_id);
	zbdCmdBuilt();
	status = zbdZigBeeSendCommand(destination, 1, dstEndpoint, multicast);

	if ( zbdStateIsAsyncState() )
	{
		ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
		zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);
	}

	return status;
}

static int zbdReadBasicCluster(int16u nodeId, int8u dstEndpoint)
{
	int16u      multicast   = 0;

	zclBufferSetup(ZCL_PROFILE_WIDE_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER | ZCL_DISABLE_DEFAULT_RESPONSE_MASK,
                 ZCL_BASIC_CLUSTER_ID,
                 ZCL_READ_ATTRIBUTES_COMMAND_ID);

	zclBufferAddWord(ZCL_APPLICATION_VERSION_ATTRIBUTE_ID);
	zclBufferAddWord(ZCL_MANUFACTURER_NAME_ATTRIBUTE_ID);
	zclBufferAddWord(ZCL_MODEL_IDENTIFIER_ATTRIBUTE_ID);
	zbdCmdBuilt();

	return zbdZigBeeSendCommand(nodeId, 1, dstEndpoint, multicast);
}

static int zbdSendReadBasicCluster(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int8u       dstEndpoint = (int8u) (pcmdMsg->hdr.zbnode.endpoint & 0xFF);

	status = zbdReadBasicCluster(destination, dstEndpoint);
	ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
	zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);

	return status;
}

static int zbdSendGlobalRead(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int16u      multicast   = 0;
	int8u       dstEndpoint = (int8u) (pcmdMsg->hdr.zbnode.endpoint & 0xFF);
	int16u      cluster_id  = (int16u) (pcmdMsg->hdr.param1 & 0xFFFF);
	int16u      attrib_id   = (int16u) (pcmdMsg->hdr.param2 & 0xFFFF);
	int8u       command_id  = ZCL_READ_ATTRIBUTES_COMMAND_ID;
	int8u       cmd_direction;

	DBGPRINT("zbdSendGlobalRead: destination:0x%x dstEndpoint:0x%x cluster_id:%d attrib_id:%d\n", destination, dstEndpoint, cluster_id, attrib_id);

	cmd_direction = ZCL_FRAME_CONTROL_CLIENT_TO_SERVER;

	zclBufferSetup(ZCL_PROFILE_WIDE_COMMAND | cmd_direction | ZCL_DISABLE_DEFAULT_RESPONSE_MASK,
                 cluster_id,
                 command_id);

	zclBufferAddWord(attrib_id);
	zbdCmdBuilt();

	status = zbdZigBeeSendCommand(destination, 1, dstEndpoint, multicast);

	if ( zbdStateIsAsyncState() )
	{
		ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
		zbdReplyGeneralResponse(pcmdMsg, aclientFD, ret, ret);
	}

	return status;
}

static int zbdSendGetClusters(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int8u       dstEndpoint = (int8u) (pcmdMsg->hdr.zbnode.endpoint & 0xFF);

	DBGPRINT("zbdSendGetCluster: destination:0x%x dstEndpoint:0x%x \n", destination, dstEndpoint);

	zbdSetSourceRoute(destination);

	status = emberSimpleDescriptorRequest(destination, dstEndpoint, EMBER_AF_DEFAULT_APS_OPTIONS);

	return status;
}

static int zbdActiveEndpointsRequest(EmberNodeId paramNodeId)
{
	EmberStatus status;
	zbdSetSourceRoute(paramNodeId);
	status = emberActiveEndpointsRequest(paramNodeId, EMBER_AF_DEFAULT_APS_OPTIONS);
	DBGPRINT("zbdActiveEndpointsRequest: paramNodeId:0x%x status:0x%x\n", paramNodeId, status);
	return status;
}

static int zbdSendGetActiveEndpoint(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);

	DBGPRINT("zbdSendGetActiveEndpoint: destination:0x%x \n", destination);

	status = zbdActiveEndpointsRequest(destination);
	return status;
}

static int zbdSendGetNodeID(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);

	DBGPRINT("zbdSendGetNodeID \n");

	status = emberAfFindNodeId(pcmdMsg->hdr.zbnode.node_eui, zbdWeMoCliServiceDiscoveryCallback);
	DBGPRINT("emberAfFindNodeId: status:0x%x \n", status);

	if ( zbdStateIsAsyncState() )
	{
		ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
		zbdReplyGeneralResponse(pcmdMsg, aclientFD, destination, ret);
	}

	return status;
}

static int zbdReadIntFromFile(const char *fileName, int *pintType)
{
	int ret = FAILURE;
	FILE *fp;

	/*DBGPRINT("Entering zbdReadIntFromFile fileName:%s \n", fileName);*/

	fp = fopen(fileName, "r");
	if (fp && pintType)
	{
		char *endptr;
		char str_int[10];

		memset(str_int, 0, sizeof(str_int));
		if (fgets(str_int, sizeof(str_int) - 1, fp))
		{
			int val;

			/*DBGPRINT("SUCCESS fgets file:%s \n", fileName);*/
			errno = 0;
			val = strtol(str_int, &endptr, 0);

			/*DBGPRINT("SUCCESS strol file:%s \n", fileName);*/
			/* Check for various possible errors */
			if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
					|| (errno != 0 && val == 0)
					|| (endptr == str_int))
			{
				DBGPRINT("ERROR strol file:%s \n", fileName);
				ret = FAILURE;
			}
			else
			{
				/*DBGPRINT("SUCCESS reading integer :0x%04x , fileName:%s\n", *pintType, fileName );*/
				*pintType = val;
				/*DBGPRINT("SUCCESS assigned param file:%s \n", fileName);*/
				ret = SUCCESS;
			}
		}

		fclose(fp);
		/*DBGPRINT("Closing file:%s \n", fileName);*/
	}
	else
	{
		DBGPRINT("No file:[%s]\n", fileName);
	};

	/*DBGPRINT("Leaving zbdReadIntFromFile fileName:%s \n", fileName);*/
	return ret;
}

static int zbdReadColorInformation(int16u nodeId, int8u dstEndpoint)
{
	int16u      multicast   = 0;

	zclBufferSetup(ZCL_PROFILE_WIDE_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER | ZCL_DISABLE_DEFAULT_RESPONSE_MASK,
			ZCL_COLOR_CONTROL_CLUSTER_ID,
			ZCL_READ_ATTRIBUTES_COMMAND_ID);

	zclBufferAddWord(ZCL_COLOR_CONTROL_CURRENT_X_ATTRIBUTE_ID);
	zclBufferAddWord(ZCL_COLOR_CONTROL_CURRENT_Y_ATTRIBUTE_ID);
#if 0
	zclBufferAddWord(ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID);
	zclBufferAddWord(ZCL_COLOR_CONTROL_COLOR_MODE_ATTRIBUTE_ID);
#endif
	zbdCmdBuilt();

	return zbdZigBeeSendCommand(nodeId, 1, dstEndpoint, multicast);
}

static int zbdSendReadColorInfo(TIPCMsg *pcmdMsg, int aclientFD)
{
	EmberStatus status;
	int         ret         = ZB_RET_SUCCESS;
	int16u      destination = (int16u) (pcmdMsg->hdr.zbnode.node_id & 0xFFFF);
	int8u       dstEndpoint = (int8u) (pcmdMsg->hdr.zbnode.endpoint & 0xFF);

	status = zbdReadColorInformation(destination, dstEndpoint);

	if ( zbdStateIsAsyncState() )
	{
		ret = (status != EMBER_SUCCESS) ? ZB_RET_FAIL : ZB_RET_SUCCESS;
		zbdReplyGeneralResponse(pcmdMsg, aclientFD, destination, ret);
	}

	return status;
}

static int zbdReadNodeId(const char *fileName, EmberNodeId *pnodeId)
{
	int ret = FAILURE;
	int int_value;

	/*DBGPRINT("Entering zbdReadNodeId fileName:%s \n", fileName);*/

	if (!zbdIsValidNodeIdFile(fileName))
	{
		return FAILURE;
	}

	ret = zbdReadIntFromFile(fileName, &int_value);
	*pnodeId = int_value & 0xFFFF;

	/*DBGPRINT("Leaving zbdReadNodeId fileName:%s \n", fileName);*/
	return ret;
}

static int zbdReadNodeType(const char *fileName, int *pnodeType)
{
	int ret = FAILURE;

	// Check for valid node Type file.
	if (!zbdIsValidNodeTypeFile(fileName))
	{
		return FAILURE;
	}

	ret = zbdReadIntFromFile(fileName, pnodeType);

	return ret;
}

static int zbdProcessServerIpcCmd(TIPCMsg* pcurMsg, int clientFD)
{
	EmberNodeId zb_nodeid;
	EmberStatus status = 0;
	EmberEUI64 eui64;
	char full_name[80];

	if (NULL == pcurMsg)
		return FAILURE;

	s_Cur_CMD = pcurMsg->hdr.cmd;

	DBGPRINT("zigbeed node_id:0x%04x, endpoint:%d\n", pcurMsg->hdr.zbnode.node_id, pcurMsg->hdr.zbnode.endpoint);
	DBGPRINT("cmd:%d p1:%d p2:%d p3:%d p4:%d \n",
			pcurMsg->hdr.cmd,
			pcurMsg->hdr.param1,
			pcurMsg->hdr.param2,
			pcurMsg->hdr.param3,
			pcurMsg->hdr.param4);

	memcpy(s_LastEUI, pcurMsg->hdr.zbnode.node_eui, sizeof(EmberEUI64));
	s_LastNodeId = pcurMsg->hdr.zbnode.node_id;

	zbdMakeEuiFileName(full_name, sizeof(full_name), pcurMsg->hdr.zbnode.node_eui);
	DBGPRINT("Full Name:%s\n", full_name);

	if (SUCCESS == zbdReadNodeId(full_name, &zb_nodeid))
	{
		pcurMsg->hdr.zbnode.node_id = zb_nodeid;
		DBGPRINT("** LATEST NODE ID zigbeed node_id:0x%04x \n", pcurMsg->hdr.zbnode.node_id );
	}

	zb_nodeid = pcurMsg->hdr.zbnode.node_id;
	getIeeeFromNodeId(zb_nodeid, eui64);

	zbdSetTimeToAbort();

	switch(pcurMsg->hdr.cmd)
	{
		case IPC_CMD_GET_NETWORK_STATE:
			DBGPRINT("IPC_CMD_GET_NETWORK_STATE:Unknown command: %d\n", pcurMsg->hdr.cmd);
			/*status = zbdSendNetworkState(pcurMsg, clientFD);*/
			break;

		case IPC_CMD_SET_NET_SCANJOIN:
			DBGPRINT("IPC_CMD_SET_NET_SCANJOIN \n");
			emberConcentratorStopDiscovery();
			status = zbdSendNetworkScanJoin(pcurMsg, clientFD);
			break;

		case IPC_CMD_SET_NET_FORM:
			DBGPRINT("IPC_CMD_SET_NET_FORM \n");
			status = zbdSendFormNetwork(pcurMsg, clientFD);
			break;

		case IPC_CMD_SET_NET_LEAVE:
			DBGPRINT("IPC_CMD_SET_NET_LEAVE\n");
			status = zbdSendLeaveNetwork(pcurMsg, clientFD);
			break;

		case IPC_CMD_SET_NET_CHANNEL:
			DBGPRINT("IPC_CMD_SET_NET_CHANNEL\n");
			status = zbdSendChangeNetworkChannel(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_NET_INFO:
			DBGPRINT("IPC_CMD_GET_NET_INFO \n");

			DBGPRINT("\nChild Table \n");
			printChildTable();
			DBGPRINT("\nNeighbor Table \n");
			printNeighborTable();
			DBGPRINT("\nRoute Table \n");
			printRouteTable();

			status = zbdSendGetNetworkInfo(pcurMsg, clientFD);
			break;

		case IPC_CMD_SET_PERMITJOIN:
			DBGPRINT("IPC_CMD_SET_PERMITJOIN \n");
			status = zbdSendPermitJoin(pcurMsg, clientFD);
			break;

		case IPC_CMD_SET_LEAVE_REQUEST:
			DBGPRINT("IPC_CMD_SET_LEAVE_REQUEST \n");
			if (pcurMsg->hdr.param2 != 0) // Special QA mode for sensors
			{
				DBGPRINT("Schedule LeaveRequest:0x%04X\n", zb_nodeid);
				zbdScheduleLeaveRequest(zb_nodeid);
			}
			else
			{
				if (zbdIsNodeSensor(zb_nodeid))
				{
					DBGPRINT("Sensor Type \n");
					newDeviceLeftHandler(eui64, CLEAN_MODE_SAVE_NODEID);
				}
				else
				{
					DBGPRINT("Bulb Type \n");
					zbdSetState(ZBD_ST_ZB_TRUSTCENTER);
					status = zbdSendLeaveRequest(pcurMsg, clientFD);
				}
			}
			break;

		case IPC_CMD_GET_DEVTYPE:
			DBGPRINT("IPC_CMD_GET_DEVTYPE \n");
			break;

		case IPC_CMD_GET_NODEID:
			DBGPRINT("IPC_CMD_GET_NODEID \n");

			status = zbdSendGetNodeID(pcurMsg, clientFD);
			break;

		case IPC_CMD_SET_MTORR:
			DBGPRINT("IPC_CMD_SET_MTORR \n");
			status = zbdSendMTORR(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_ENDPOINT:
			DBGPRINT("IPC_CMD_GET_ENDPOINT \n");

			zbdStateIOMode(ZBD_IO_SYNC_GET);

			status = zbdSendGetActiveEndpoint(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_IAS_ZONE_STATUS:
			DBGPRINT("IPC_CMD_GET_IAS_ZONE_STATUS \n");

			zbdStateIOMode(ZBD_IO_ASYNC);

			pcurMsg->hdr.param1 = ZCL_IAS_ZONE_CLUSTER_ID;
			pcurMsg->hdr.param2 = ZCL_ZONE_STATUS_ATTRIBUTE_ID;

			status = zbdSendGlobalRead(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_CLUSTERS:
			DBGPRINT("IPC_CMD_GET_CLUSTERS \n");
#if 0 // TEST CODE
			zbdStateIOMode(ZBD_IO_ASYNC);
			status = zbdSendReadBasicCluster(pcurMsg, clientFD);
#else
			zbdStateIOMode(ZBD_IO_SYNC_GET);

			status = zbdSendGetClusters(pcurMsg, clientFD);
#endif
			break;

		case IPC_CMD_SET_ONOFF:
			DBGPRINT("IPC_CMD_SET_ONOFF \n");
			zbdStateIOMode(ZBD_IO_ASYNC);
			status = zbdSendOnOff(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_ONOFF:
			DBGPRINT("IPC_CMD_GET_ONOFF \n");

			if (pcurMsg->hdr.param1 != 0) // if async
			{
				zbdStateIOMode(ZBD_IO_ASYNC);
			}
			else
			{
				zbdStateIOMode(ZBD_IO_SYNC_GET);
			}

			pcurMsg->hdr.param1 = ZCL_ON_OFF_CLUSTER_ID;
			pcurMsg->hdr.param2 = ZCL_ON_OFF_ATTRIBUTE_ID;

			status = zbdSendGlobalRead(pcurMsg, clientFD);
			break;

		case IPC_CMD_SET_GROUP:
			DBGPRINT("IPC_CMD_SET_GROUP \n");

			zbdStateIOMode(ZBD_IO_SYNC_SET);
			status = zbdSendGroupAddRemove(pcurMsg, clientFD);
			break;

		case IPC_CMD_SET_LEVEL:
			DBGPRINT("IPC_CMD_SET_LEVEL \n");
			zbdStateIOMode(ZBD_IO_ASYNC);
			status = zbdSendLevel(pcurMsg, clientFD);
			break;

		case IPC_CMD_SET_LEVEL_MOVE:
			DBGPRINT("IPC_CMD_SET_LEVEL_MOVE \n");

			zbdStateIOMode(ZBD_IO_SYNC_SET);
			status = zbdSendLevelMove(pcurMsg, clientFD);
			break;

		case IPC_CMD_SET_LEVEL_STOP:
			DBGPRINT("IPC_CMD_SET_LEVEL_STOP \n");

			zbdStateIOMode(ZBD_IO_SYNC_SET);
			status = zbdSendLevelStop(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_LEVEL:
			DBGPRINT("IPC_CMD_GET_LEVEL \n");

			if (pcurMsg->hdr.param1 != 0) // if async
			{
				zbdStateIOMode(ZBD_IO_ASYNC);
			}
			else
			{
				zbdStateIOMode(ZBD_IO_SYNC_GET);
			}

			pcurMsg->hdr.param1 = ZCL_LEVEL_CONTROL_CLUSTER_ID;
			pcurMsg->hdr.param2 = ZCL_CURRENT_LEVEL_ATTRIBUTE_ID;

			status = zbdSendGlobalRead(pcurMsg, clientFD);
			break;

		case IPC_CMD_SET_IDENTIFY:
			DBGPRINT("IPC_CMD_SET_IDENTIFY \n");

			zbdStateIOMode(ZBD_IO_SYNC_SET);
			status = zbdSendIdentify(pcurMsg, clientFD);
			break;

		case IPC_CMD_SET_TESTMODE:
			DBGPRINT("IPC_CMD_SET_TESTMODE \n");

			zbdStateIOMode(ZBD_IO_ASYNC);
			status = zbdSendSetTestMode(pcurMsg, clientFD);
			break;

		case IPC_CMD_SET_OTA_NOTIFY:
			DBGPRINT("IPC_CMD_SET_OTA_NOTIFY \n");

			zbdStateIOMode(ZBD_IO_SYNC_SET);
			status = zbdSendOTANotify(pcurMsg, clientFD);
			break;

		case IPC_CMD_SET_COLOR:
			DBGPRINT("IPC_CMD_SET_COLOR \n");

			zbdStateIOMode(ZBD_IO_SYNC_SET);
			status = zbdSendColor(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_COLOR:
			DBGPRINT("IPC_CMD_GET_COLOR \n");
			if (pcurMsg->hdr.param1 != 0) // if async
			{
				zbdStateIOMode(ZBD_IO_ASYNC);
			}
			else
			{
				zbdStateIOMode(ZBD_IO_SYNC_GET);
			}
			status = zbdSendReadColorInfo(pcurMsg, clientFD);
			break;

		case IPC_CMD_SET_COLOR_TEMP:
			DBGPRINT("IPC_CMD_SET_COLOR_TEMP \n");

			zbdStateIOMode(ZBD_IO_SYNC_SET);
			status = zbdSendColorTemp(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_COLOR_TEMP:
			DBGPRINT("IPC_CMD_GET_COLOR_TEMP \n");
			if (pcurMsg->hdr.param1 != 0) // if async
			{
				zbdStateIOMode(ZBD_IO_ASYNC);
			}
			else
			{
				zbdStateIOMode(ZBD_IO_SYNC_GET);
			}
			pcurMsg->hdr.param1 = ZCL_COLOR_CONTROL_CLUSTER_ID;
			pcurMsg->hdr.param2 = ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID;

			status = zbdSendGlobalRead(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_MODELCODE:
			DBGPRINT("IPC_CMD_GET_MODELCODE \n");

			if (pcurMsg->hdr.param1 != 0) // if async
			{
				zbdStateIOMode(ZBD_IO_ASYNC);
				status = zbdSendReadBasicCluster(pcurMsg, clientFD);
			}
			else
			{
				zbdStateIOMode(ZBD_IO_SYNC_GET);

				pcurMsg->hdr.param1 = ZCL_BASIC_CLUSTER_ID;
				pcurMsg->hdr.param2 = ZCL_MODEL_IDENTIFIER_ATTRIBUTE_ID;

				status = zbdSendGlobalRead(pcurMsg, clientFD);
			}
			break;

		case IPC_CMD_GET_APPVERSION:
			DBGPRINT("IPC_CMD_GET_APPVERSION \n");

			zbdStateIOMode(ZBD_IO_SYNC_GET);

			pcurMsg->hdr.param1 = ZCL_BASIC_CLUSTER_ID;
			pcurMsg->hdr.param2 = ZCL_APPLICATION_VERSION_ATTRIBUTE_ID;

			status = zbdSendGlobalRead(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_ZCLVERSION:
			DBGPRINT("IPC_CMD_GET_ZCLVERSION \n");

			zbdStateIOMode(ZBD_IO_SYNC_GET);

			pcurMsg->hdr.param1 = ZCL_BASIC_CLUSTER_ID;
			pcurMsg->hdr.param2 = ZCL_VERSION_ATTRIBUTE_ID;

			status = zbdSendGlobalRead(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_STACKVERSION:
			DBGPRINT("IPC_CMD_GET_STACKVERSION \n");

			zbdStateIOMode(ZBD_IO_SYNC_GET);

			pcurMsg->hdr.param1 = ZCL_BASIC_CLUSTER_ID;
			pcurMsg->hdr.param2 = ZCL_STACK_VERSION_ATTRIBUTE_ID;

			status = zbdSendGlobalRead(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_OTA_VERSION:
			DBGPRINT("IPC_CMD_GET_OTA_VERSION \n");

			zbdStateIOMode(ZBD_IO_SYNC_GET);

			pcurMsg->hdr.param1 = ZCL_OTA_BOOTLOAD_CLUSTER_ID;
			pcurMsg->hdr.param2 = ZCL_CURRENT_FILE_VERSION_ATTRIBUTE_ID;

			status = zbdSendGlobalReadClient(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_HWVERSION:
			DBGPRINT("IPC_CMD_GET_HWVERSION \n");

			zbdStateIOMode(ZBD_IO_SYNC_GET);

			pcurMsg->hdr.param1 = ZCL_BASIC_CLUSTER_ID;
			pcurMsg->hdr.param2 = ZCL_HW_VERSION_ATTRIBUTE_ID;

			status = zbdSendGlobalRead(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_MFGNAME:
			DBGPRINT("IPC_CMD_GET_MFGNAME \n");

			zbdStateIOMode(ZBD_IO_SYNC_GET);

			pcurMsg->hdr.param1 = ZCL_BASIC_CLUSTER_ID;
			pcurMsg->hdr.param2 = ZCL_MANUFACTURER_NAME_ATTRIBUTE_ID;

			status = zbdSendGlobalRead(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_DATE:
			DBGPRINT("IPC_CMD_GET_DATE \n");

			zbdStateIOMode(ZBD_IO_SYNC_GET);

			pcurMsg->hdr.param1 = ZCL_BASIC_CLUSTER_ID;
			pcurMsg->hdr.param2 = ZCL_DATE_CODE_ATTRIBUTE_ID;

			status = zbdSendGlobalRead(pcurMsg, clientFD);
			break;

		case IPC_CMD_GET_POWERSOURCE:
			DBGPRINT("IPC_CMD_GET_POWERSOURCE \n");

			zbdStateIOMode(ZBD_IO_SYNC_GET);

			pcurMsg->hdr.param1 = ZCL_BASIC_CLUSTER_ID;
			pcurMsg->hdr.param2 = ZCL_POWER_SOURCE_ATTRIBUTE_ID;

			status = zbdSendGlobalRead(pcurMsg, clientFD);
			break;

		default:
			DBGPRINT("zbdProcessServerIpcCmd:Unknown command: %d\n", pcurMsg->hdr.cmd);
			break;
	}

	DBGPRINT("Ember_Status_Code:0x%X \n", status);
	return SUCCESS;
}

static void zbdIPCRecv(void)
{
	int         rc;
	int         nbytes;

	memcpy(&s_WorkingSet, &s_MasterSet, sizeof(s_MasterSet));

	rc = select(s_MaxSD+1, &s_WorkingSet, NULL, NULL, &s_SockTimeout);

	if (rc < 0)
	{
		DBGPRINT("select error\n");
		return;
	}

	if (rc == 0)
	{
		/*DBGPRINT("select s_SockTimeout\n");*/
		return;
	}

	if (FD_ISSET(s_MaxSD, &s_WorkingSet))
	{
		char       *payload_buffer = NULL;
		TIPCMsg     buffer;

		s_ClientFD = accept(s_ServerSock, (struct sockaddr *)&s_RemoteSockAddr, &s_LenRemote);
		if (s_ClientFD == -1)
		{
			DBGPRINT(":Accept Failed.\n");
			return;
		}

		DBGPRINT("\nStarting IPC Receive:%d\n", s_ClientFD);
		do
		{
			memset(&buffer, 0, sizeof(TIPCMsg));
			nbytes = recv(s_ClientFD, &buffer.hdr, IPC_CMD_HDR_LEN, 0x0);
			if(nbytes < 0)
			{
				DBGPRINT("IPC Receive:header read error\n");
				break;
			}

			payload_buffer = NULL;
			if(buffer.hdr.arg_length > 0)
			{
				payload_buffer = (char*)calloc(buffer.hdr.arg_length, 1);
				if(payload_buffer == NULL )
				{
					DBGPRINT("IPC Receive:payload buffer calloc failed\n");
					break;
				}

				nbytes = recv(s_ClientFD, payload_buffer, buffer.hdr.arg_length, 0x0);
				if(nbytes < 0)
				{
					DBGPRINT("IPC Receive:payload read error\n");
					break;
				}
				buffer.arg = (void *)payload_buffer;
			}

			zbdProcessServerIpcCmd(&buffer, s_ClientFD);
		}
		while(0);

		if(payload_buffer!=NULL){free(payload_buffer); payload_buffer=NULL;}

		switch (s_State)
		{
			case ZBD_ST_IPC_RECV:
				// Clean Up
				DBGPRINT("Leaving IPC Receive:%d\n", s_ClientFD);
				close(s_ClientFD);
				break;

			case ZBD_ST_ZB_MSG_SENT_STATUS:
				break;

			default:
				break;
		}

		return;
	}
}

/** @brief Read Attributes Response
 *
 * This function is called by the application framework when a Read Attributes
 * Response command is received from an external device.  The application
 * should return TRUE if the message was processed or FALSE if it was not.
 *
 * @param clusterId The cluster identifier of this response.  Ver.: always
 * @param buffer Buffer containing the list of read attribute status records.
 * Ver.: always
 * @param bufLen The length in bytes of the list.  Ver.: always
 */
boolean emberAfReadAttributesResponseCallback(EmberAfClusterId clusterId,
                                              int8u * buffer,
                                              int16u bufLen)
{
  return FALSE;
}

/** @brief Report Attributes
 *
 * This function is called by the application framework when a Report
 * Attributes command is received from an external device.  The application
 * should return TRUE if the message was processed or FALSE if it was not.
 *
 * @param clusterId The cluster identifier of this command.  Ver.: always
 * @param buffer Buffer containing the list of attribute report records.
 * Ver.: always
 * @param bufLen The length in bytes of the list.  Ver.: always
 */
boolean emberAfReportAttributesCallback(EmberAfClusterId clusterId,
                                        int8u * buffer,
                                        int16u bufLen)
{
  return FALSE;
}

static int zbdReadBytes(int8u *byteStream, int lenStream, int numBytes, int *ptrPos, void *ptrResult)
{
	int num_read = 0;
	int idx = *ptrPos;

	if ( (idx < 0) || !(idx < lenStream) )
	{
		return FAILURE;
	}

	if ( !((numBytes + idx) <= lenStream) )
	{
		return FAILURE;
	}

	switch (numBytes)
	{
		case 1:
			*(int8u *)(ptrResult) = byteStream[idx];
			num_read = 1;
			break;

		case 2:
			*(int16u *)(ptrResult) = byteStream[idx] + (byteStream[idx+1] << 8);
			num_read = 2;
			break;

		case 4:
			*(int32u *)(ptrResult) = byteStream[idx] + (byteStream[idx+1] << 8) + (byteStream[idx+2] << 16) + (byteStream[idx+3] << 24);
			num_read = 4;
			break;

		default:
			num_read = 0;
			*ptrPos = num_read;
			return FAILURE;
	}

	*ptrPos += num_read;

	return SUCCESS;
}

static void zbdParseSaveBasicCluster(int8u *byteStream, int lenStream, char *fileName)
{
	int16u  attribute_id;
	int8u   status;
	int     idx;
	int     ret;
	int8u   attr_type;
	FILE   *fp;
	int     member_count;

	fp = fopen(fileName, "w");
	if (!fp)
	{
		return;
	}

	fprintf(fp, "{");

	member_count = 0;
	idx = 3;
	while (idx < lenStream)
	{
		/*attribute_id = message[idx] + (message[idx+1] << 8);*/
		/*idx += sizeof(attrib_id); // 5*/
		ret = zbdReadBytes(byteStream, lenStream, 2, &idx, (void *)&attribute_id);
		if (ret != SUCCESS)
			break;

		DBGPRINT("attribute_id:0x%04X \n", attribute_id);
		switch (attribute_id)
		{
			case ZCL_VERSION_ATTRIBUTE_ID:
				DBGPRINT("ZCL_VERSION_ATTRIBUTE_ID\n");
				break;
			case ZCL_APPLICATION_VERSION_ATTRIBUTE_ID:
				DBGPRINT("ZCL_APPLICATION_VERSION_ATTRIBUTE_ID\n");
				break;
			case ZCL_STACK_VERSION_ATTRIBUTE_ID:
				DBGPRINT("ZCL_STACK_VERSION_ATTRIBUTE_ID\n");
				break;
			case ZCL_HW_VERSION_ATTRIBUTE_ID:
				DBGPRINT("ZCL_HW_VERSION_ATTRIBUTE_ID\n");
				break;
			case ZCL_MANUFACTURER_NAME_ATTRIBUTE_ID:
				DBGPRINT("ZCL_MANUFACTURER_NAME_ATTRIBUTE_ID\n");
				break;
			case ZCL_MODEL_IDENTIFIER_ATTRIBUTE_ID:
				DBGPRINT("ZCL_MODEL_IDENTIFIER_ATTRIBUTE_ID\n");
				break;
			case ZCL_DATE_CODE_ATTRIBUTE_ID:
				DBGPRINT("ZCL_DATE_CODE_ATTRIBUTE_ID\n");
				break;
			case ZCL_POWER_SOURCE_ATTRIBUTE_ID:
				DBGPRINT("ZCL_POWER_SOURCE_ATTRIBUTE_ID\n");
				break;
		}

		/*status = message[idx]; // 5*/
		/*idx += sizeof(status); // 6*/
		ret = zbdReadBytes(byteStream, lenStream, 1, &idx, (void *)&status);
		if (ret != SUCCESS)
			break;

		if (status != 0x00)
		{
			zbdJsonWritePairWordAttributeList(fp, member_count, 1, attribute_id, NULL);
			member_count++;
			continue;
		}

		/*int8u attr_type = message[idx]; // 6*/
		/*idx += sizeof(attr_type); // 7*/
		ret = zbdReadBytes(byteStream, lenStream, 1, &idx, (void *)&attr_type);
		if (ret != SUCCESS)
		{
			zbdJsonWritePairWordAttributeList(fp, member_count, 1, attribute_id, NULL);
			member_count++;
			continue;
		}

		switch (attr_type)
		{
			case ZCL_CHAR_STRING_ATTRIBUTE_TYPE:
				{
					char   str_buf[33];
					int    size_buf;
					int8u  str_len = 0;
					char  *str_ret = NULL;

					memset(str_buf, 0, sizeof(str_buf));

					/*str_len = byteStream[idx]; // 7*/
					/*idx += sizeof(str_len); // 8*/
					ret = zbdReadBytes(byteStream, lenStream, 1, &idx, (void *)&str_len);
					if (ret != SUCCESS)
					{
						zbdJsonWritePairWordAttributeList(fp, member_count, 1, attribute_id, NULL);
						member_count++;
						continue;
					}

					if (str_len == 0)
					{
						zbdJsonWritePairWordAttributeList(fp, member_count, 1, attribute_id, NULL);
						member_count++;
						continue;
					}

					str_ret = &byteStream[idx]; // 8
					idx += str_len; // +6 = 14 returned length // now idx points to the next response

					size_buf = sizeof(str_buf) - 1;
					if (str_len < size_buf) size_buf = str_len;

					memcpy(str_buf, str_ret, size_buf);
					DBGPRINT("str_buf:%s \n", str_buf);
					zbdJsonWritePairWordAttributeList(fp, member_count, 1, attribute_id, str_buf);
					member_count++;
				}
				break;

			case ZCL_INT8U_ATTRIBUTE_TYPE:
			case ZCL_ENUM8_ATTRIBUTE_TYPE:
				{
					int8u byte_value;
					/*byte_value = byteStream[7];*/
					ret = zbdReadBytes(byteStream, lenStream, 1, &idx, (void *)&byte_value);
					if (ret != SUCCESS)
					{
						zbdJsonWritePairWordAttributeList(fp, member_count, 1, attribute_id, NULL);
						member_count++;
						continue;
					}

					DBGPRINT("byte_value:0x%02X \n", byte_value);
					{
						char str_value[10];
						sprintf(str_value, "0x%02X", byte_value);

						zbdJsonWritePairWordAttributeList(fp, member_count, 1, attribute_id, str_value);
						member_count++;
					}
				}
				break;
		}

	}

	fprintf(fp, "\n}\n");
	fclose(fp);
}

static int zbdIsLatestEvent(EmberNodeId nodeId, int8u sequenceNumber)
{
	int8u index = getAddressIndexFromNodeId(nodeId);
	int result = FALSE;

	if (index == NULL_INDEX)
	{
		return FALSE;
	}

	if (addressTable[index].zclSequence < sequenceNumber)
	{
		result = TRUE;
	}
	else
	{
		// Checking for the overlapped and the jumped. e.g. 0xFF -> 0x00, 0x7F -> 0x00
		int diff  = addressTable[index].zclSequence - sequenceNumber;
		if (diff > 5)
		{
			DBGPRINT("Overlapped Jumped current:0x%02X new:0x%02X", addressTable[index].zclSequence , sequenceNumber);
			result = TRUE;
		}
	}

	if (result == TRUE)
	{
		addressTable[index].zclSequence = sequenceNumber;
	}

	if (result == FALSE)
	{
		DBGPRINT("Out Of Order Sequence current:0x%02X new:0x%02X", addressTable[index].zclSequence , sequenceNumber);
	}

	return result;
}

/** @brief Pre Command Received
 *
 * This callback is the second in the Application Framework’s message
 * processing chain. At this point in the processing of incoming over-the-air
 * messages, the application has determined that the incoming message is a ZCL
 * command. It parses enough of the message to populate an
 * EmberAfClusterCommand struct. The Application Framework defines this struct
 * value in a local scope to the command processing but also makes it
 * available through a global pointer called emberAfCurrentCommand, in
 * app/framework/util/util.c. When command processing is complete, this
 * pointer is cleared.
 *
 * @param cmd   Ver.: always
 */
boolean emberAfPreCommandReceivedCallback(EmberAfClusterCommand* cmd)
{
	EmberNodeId  source_nodeid   = cmd->source;
	int          length          = cmd->bufLen;
	int8u       *message         = cmd->buffer;
	int          ret             = ZB_RET_FAIL;
	int          source_endpoint = cmd->apsFrame->sourceEndpoint;
	EmberEUI64   eui64;
	int8u        seq_num         = cmd->seqNum;

	memset(eui64, 0, sizeof(eui64));
	getIeeeFromNodeId(source_nodeid, eui64);

	DBGPRINT("emberAfPreCommandReceivedCallback\n");
	DBGPRINT("eui64:%s seq:0x%02X source_nodeid:0x%04X clusterId:0x%04X, commandId:0x%02X, length:%d \n", 
			zbdEui64ToStr(eui64), 
			seq_num, 
			source_nodeid, 
			cmd->apsFrame->clusterId, 
			cmd->commandId, 
			length);

	zbdDbgPrintByteStream(message, length);

	if (cmd->commandId == ZCL_REPORT_ATTRIBUTES_COMMAND_ID)
	{
		int16u attrib_id = HIGH_LOW_TO_INT(message[4], message[3]);
		int8u attr_type = message[5];

		switch(cmd->apsFrame->clusterId)
		{
			case ZCL_POWER_CONFIG_CLUSTER_ID:
				if (attrib_id == ZCL_BATTERY_VOLTAGE_ATTRIBUTE_ID &&
						attr_type == ZCL_INT8U_ATTRIBUTE_TYPE)
				{
					int8u voltage = message[6];
					int16u elapsed_qs;

					if (zbdGetNodeType(source_nodeid) != ZB_ZONE_FOB)
					{
						elapsed_qs = zbdSetNodePollTime(source_nodeid, 0, 1);
					}

					zbdSysEventSingleCode(ZBD_EVENT_NODE_VOLTAGE, source_nodeid, voltage);
					DBGPRINT("ZCL_BATTERY_VOLTAGE_ATTRIBUTE_ID voltage:%d elapsed sec:%d \n", voltage, (elapsed_qs / 4) );
				}
				break;

			case ZCL_DIAGNOSTICS_CLUSTER_ID:
				if (attrib_id == ZCL_LAST_MESSAGE_RSSI_ATTRIBUTE_ID &&
						attr_type == ZCL_INT8S_ATTRIBUTE_TYPE)
				{
					int8s rssi = message[6];
					zbdSysEventSingleCode(ZBD_EVENT_NODE_RSSI, source_nodeid, rssi);
					DBGPRINT("ZCL_LAST_MESSAGE_RSSI_ATTRIBUTE_ID rssi:%d \n", rssi);
				}
				break;
		}

		DBGPRINT("ZCL_REPORT_ATTRIBUTES_COMMAND_ID: id:0x04%x, type:0x%x \n", attrib_id, attr_type);
		return FALSE;
	}

	if (cmd->commandId == ZCL_ZONE_STATUS_CHANGE_NOTIFICATION_COMMAND_ID &&
				cmd->apsFrame->clusterId == ZCL_IAS_ZONE_CLUSTER_ID)
	{
		int16u zone_status = HIGH_LOW_TO_INT(message[4],message[3]);
		int8u zone_id = message[6];

		DBGPRINT("ZCL_ZONE_STATUS_CHANGE_NOTIFICATION_COMMAND_ID node:0x%04X, seq:0x%02X, zone_status:0x%04X zone_id:%d\n", 
				source_nodeid, 
				seq_num, 
				zone_status, 
				zone_id);

		if (zbdIsLatestEvent(source_nodeid, seq_num))
		{
			zbdSysEventDoubleCode(ZBD_EVENT_ZONE_STATUS, source_nodeid, zone_status, zone_id);
		}

		return FALSE;
	}

	if (cmd->commandId == ZCL_UPGRADE_END_REQUEST_COMMAND_ID &&
				cmd->apsFrame->clusterId == ZCL_OTA_BOOTLOAD_CLUSTER_ID)
	{
		int8u   status       = message[3];
		DBGPRINT("ZCL_UPGRADE_END_REQUEST_COMMAND_ID ");
		if (status == 0x00)
		{
			DBGPRINT("SUCCESS\n");
			zbdSysEventSingleCode(ZBD_EVENT_OTA_STATUS, source_nodeid, 100);
			zbdSysEventSingleCode(ZBD_EVENT_OTA_PERCENT, source_nodeid, 100);
		}
		else
		{
			DBGPRINT("ERROR\n");
			zbdSysEventSingleCode(ZBD_EVENT_OTA_STATUS, source_nodeid, 200);
		}

		return FALSE;
	}

	if (cmd->commandId == ZCL_IMAGE_BLOCK_REQUEST_COMMAND_ID &&
				cmd->apsFrame->clusterId == ZCL_OTA_BOOTLOAD_CLUSTER_ID)
	{
		int32u file_offset = message[12] + (message[13] << 8) + (message[14] << 16) + (message[15] << 24);
		if (s_OTA_Size)
		{
			int percent = (file_offset * 100) / s_OTA_Size;
			DBGPRINT("Percent:%d mod:%d\n", percent, (percent % 5));

			if ( (percent % 5) == 0 )
			{
				static int old_percent = 255; // invalid start value

				if (percent != old_percent)
				{
					zbdSysEventSingleCode(ZBD_EVENT_OTA_PERCENT, source_nodeid, percent);

					old_percent = percent;
				}
			}
		}

		return FALSE;
	}


#if 0
	{
		if(cmd->commandId == ZCL_ZONE_ENROLL_REQUEST_COMMAND_ID &&
				cmd->apsFrame->clusterId == ZCL_IAS_ZONE_CLUSTER_ID &&
				cmd->clusterSpecific == TRUE)
		{
			const char * file_name = DIR_ZIGBEED_CONFIG "zbid.iastest";
			if (access(file_name, F_OK) != -1)
			{
				// return TRUE; // DCS TEST CODE:  disable enrollment

				emberAfFillCommandIasZoneClusterZoneEnrollResponse(0,  // success
						0x03); // dummy zone

				// Note in the response, the source and destination endpoints must be
				// the opposite of the incoming command.
				emberAfSetCommandEndpoints(cmd->apsFrame->destinationEndpoint,
						cmd->apsFrame->sourceEndpoint);

				emberAfGetCommandApsFrame()->options = EMBER_AF_DEFAULT_APS_OPTIONS | EMBER_APS_OPTION_SOURCE_EUI64;

				emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT,
						cmd->source);

				return TRUE;
			}
		}
	}
#endif

	if ( s_State == ZBD_ST_ZB_MSG_SENT_STATUS )
	{
		switch(s_Cur_CMD)
		{
			case IPC_CMD_GET_MFGNAME:
			case IPC_CMD_GET_MODELCODE:
			case IPC_CMD_GET_DATE:
				{
					int16u  attribute_id = message[3] + (message[4] << 8);
					int8u   status       = message[5];
					int     str_len      = 0;
					char   *str_ret      = NULL;

					DBGPRINT("RETURN MSG: ZCL_CHAR_STRING_ATTRIBUTE_TYPE\n");

					if (status == 0x00)
					{
						int8u attr_type = message[6];
						if (attr_type == ZCL_CHAR_STRING_ATTRIBUTE_TYPE)
						{
							str_len = message[7];
							str_ret = &message[8];
							ret = ZB_RET_SUCCESS;
						}
					}

					zbdReplyStrResponse(NULL, s_ClientFD, str_ret, str_len, ret);

					close(s_ClientFD);
					zbdSetState(ZBD_ST_IPC_RECV);
					return FALSE;
				}
				break;

			case IPC_CMD_GET_LEVEL:
				{
					int16u attribute_id = message[3] + (message[4] << 8);
					int8u  status       = message[5];
					int8u  level;

					DBGPRINT("RETURN MSG: IPC_CMD_GET_LEVEL\n");

					if (status == 0x00)
					{
						int8u attr_type = message[6];
						if (attr_type == ZCL_INT8U_ATTRIBUTE_TYPE)
						{
							level = message[7];
							ret = ZB_RET_SUCCESS;
						}
					}

					zbdReplyGeneralResponse(NULL, s_ClientFD, level, ret);
					close(s_ClientFD);
					zbdSetState(ZBD_ST_IPC_RECV);
					return FALSE;
				}
				break;

			case IPC_CMD_GET_COLOR_TEMP:
				{
					int16u attribute_id = message[3] + (message[4] << 8);
					int8u  status       = message[5];
					int16u  color_temp = 0;

					DBGPRINT("RETURN MSG: IPC_CMD_GET_COLOR_TEMP\n");
					if (status == 0x00)
					{
						int8u attr_type = message[6];
						if (attr_type == ZCL_INT16U_ATTRIBUTE_TYPE)
						{
							ret = ZB_RET_SUCCESS;
							color_temp = message[7] + (message[8] << 8);
						}
					}

					zbdReplyGeneralResponse(NULL, s_ClientFD, color_temp, ret);
					close(s_ClientFD);
					zbdSetState(ZBD_ST_IPC_RECV);
					return FALSE;
				}
				break;

			case IPC_CMD_GET_COLOR:
				{
					DBGPRINT("RETURN MSG: IPC_CMD_GET_COLOR\n");
					if (length != 15)
					{
						zbdReplyNoPayloadResponse(NULL, s_ClientFD, ZB_RET_FAIL);
						close(s_ClientFD);
						zbdSetState(ZBD_ST_IPC_RECV);
						return FALSE;
					}

					int16u attribute_x = message[3] + (message[4] << 8);
					int8u  status_x    = message[5];
					int8u  type_x      = message[6];
					int16u current_x   = message[7] + (message[8] << 8);

					int16u attribute_y = message[9] + (message[10] << 8);
					int8u  status_y    = message[11];
					int8u  type_y      = message[12];
					int16u current_y   = message[13] + (message[14] << 8);

					int curr_xy = current_x | current_y << 16;
					/*int data_arr[2] = {current_x, current_y};*/
					/*DBGPRINT("x:0x%x, y:0x%x \n", data_arr[0], data_arr[1]);*/
					/*zbdReplyArrayResponse(NULL, s_ClientFD, 4, data_arr, 2, NULL, ZB_RET_SUCCESS);*/

					DBGPRINT("x:0x%x, y:0x%x xy:0x%x\n", current_x, current_y, curr_xy);
					zbdReplyGeneralResponse(NULL, s_ClientFD, curr_xy, ZB_RET_SUCCESS);
					close(s_ClientFD);
					zbdSetState(ZBD_ST_IPC_RECV);
					return FALSE;
				}
				break;

			case IPC_CMD_GET_ONOFF:
				{
					int16u attribute_id = message[3] + (message[4] << 8);
					int8u  status       = message[5];
					int8u  on_off;

					DBGPRINT("RETURN MSG: IPC_CMD_GET_ONOFF\n");

					if (status == 0x00)
					{
						int8u attr_type = message[6];
						if (attr_type == ZCL_BOOLEAN_ATTRIBUTE_TYPE)
						{
							on_off = message[7];
							ret = ZB_RET_SUCCESS;
						}
					}

					zbdReplyGeneralResponse(NULL, s_ClientFD, on_off, ret);
					close(s_ClientFD);
					zbdSetState(ZBD_ST_IPC_RECV);
					return FALSE;
				}
				break;

			case IPC_CMD_GET_OTA_VERSION:
				{
					int16u attribute_id = message[3] + (message[4] << 8);
					int8u  status       = message[5];
					int32u app_version;

					DBGPRINT("RETURN MSG: OTA CurrentFileVersion \n");

					if (status == 0x00)
					{
						int8u attr_type = message[6];
						if (attr_type == ZCL_INT32U_ATTRIBUTE_TYPE)
						{
							app_version = message[7] + (message[8] << 8) + (message[9] << 16) + (message[10] << 24);
							ret = ZB_RET_SUCCESS;
						}
					}

					zbdReplyGeneralResponse(NULL, s_ClientFD, app_version, ret);
					close(s_ClientFD);
					zbdSetState(ZBD_ST_IPC_RECV);
					return FALSE;
				}
				break;

			case IPC_CMD_GET_ZCLVERSION:
			case IPC_CMD_GET_APPVERSION:
			case IPC_CMD_GET_STACKVERSION:
			case IPC_CMD_GET_HWVERSION:
				{
					int16u attribute_id = message[3] + (message[4] << 8);
					int8u  status       = message[5];
					int8u  app_version;

					DBGPRINT("RETURN MSG: ZCL_INT8U_ATTRIBUTE_TYPE\n");

					if (status == 0x00)
					{
						int8u attr_type = message[6];
						if (attr_type == ZCL_INT8U_ATTRIBUTE_TYPE)
						{
							app_version = message[7];
							ret = ZB_RET_SUCCESS;
						}
					}

					zbdReplyGeneralResponse(NULL, s_ClientFD, app_version, ret);
					close(s_ClientFD);
					zbdSetState(ZBD_ST_IPC_RECV);
					return FALSE;
				}
				break;

			case IPC_CMD_GET_POWERSOURCE:
				{
					int16u attribute_id = message[3] + (message[4] << 8);
					int8u  status       = message[5];
					int8u  app_version;

					DBGPRINT("RETURN MSG: IPC_CMD_GET_POWERSOURCE\n");

					if (status == 0x00)
					{
						int8u attr_type = message[6];
						if (attr_type == ZCL_ENUM8_ATTRIBUTE_TYPE)
						{
							app_version = message[7];
							ret = ZB_RET_SUCCESS;
						}
					}

					zbdReplyGeneralResponse(NULL, s_ClientFD, app_version, ret);
					close(s_ClientFD);
					zbdSetState(ZBD_ST_IPC_RECV);
					return FALSE;
				}
				break;

			default:
				break;
		}
	}

	// Async response
	if (cmd->commandId == ZCL_READ_ATTRIBUTES_RESPONSE_COMMAND_ID)
	{
		int16u attribute_id = message[3] + (message[4] << 8);
		int8u  status       = message[5];
		int8u  attr_type;

		if (status == 0x00)
		{
			attr_type = message[6];
		}

		DBGPRINT("attribute:0x%04X cluster:0x%04X status:0x%02X \n", attribute_id, cmd->apsFrame->clusterId, status);

		if (cmd->apsFrame->clusterId == ZCL_ON_OFF_CLUSTER_ID)
		{
			if (status == 0x00)
			{
				if (attr_type == ZCL_BOOLEAN_ATTRIBUTE_TYPE)
				{
					int8u on_off;
					on_off = message[7];
					zbdSysEventSingleCode("zb_onoff_status", source_nodeid, on_off);
				}
			}
			return FALSE;
		}

		if (cmd->apsFrame->clusterId == ZCL_LEVEL_CONTROL_CLUSTER_ID)
		{
			if (status == 0x00)
			{
				if (attr_type == ZCL_INT8U_ATTRIBUTE_TYPE)
				{
					int8u level;
					level = message[7];
					zbdSysEventSingleCode("zb_level_status", source_nodeid, level);
				}
			}
			return FALSE;
		}

		if (cmd->apsFrame->clusterId == ZCL_COLOR_CONTROL_CLUSTER_ID
				&& attribute_id == ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID)
		{
			if (status == 0x00)
			{
				int8u attr_type = message[6];
				if (attr_type == ZCL_INT16U_ATTRIBUTE_TYPE)
				{
					ret = ZB_RET_SUCCESS;
					int16u color_temp = message[7] + (message[8] << 8);
					zbdSysEventSingleCode("zb_color_temp", source_nodeid, color_temp);
				}
			}
			return FALSE;
		}

		if (cmd->apsFrame->clusterId == ZCL_COLOR_CONTROL_CLUSTER_ID)
		{
			if (length == 15)
			{
				/*ZCL_COLOR_CONTROL_CURRENT_X_ATTRIBUTE_ID;*/
				int16u attribute_x = message[3] + (message[4] << 8);
				int8u  status_x    = message[5];
				int8u  type_x      = message[6];
				int16u current_x   = message[7] + (message[8] << 8);

				/*ZCL_COLOR_CONTROL_CURRENT_Y_ATTRIBUTE_ID;*/
				int16u attribute_y = message[9] + (message[10] << 8);
				int8u  status_y    = message[11];
				int8u  type_y      = message[12];
				int16u current_y   = message[13] + (message[14] << 8);

				if (attribute_x == ZCL_COLOR_CONTROL_CURRENT_X_ATTRIBUTE_ID
						&& attribute_y == ZCL_COLOR_CONTROL_CURRENT_Y_ATTRIBUTE_ID)
				{
					int curr_xy = current_x | current_y << 16;
					DBGPRINT("x:0x%x, y:0x%x xy:0x%x\n", current_x, current_y, curr_xy);
					zbdSysEventSingleCode("zb_color_xy", source_nodeid, curr_xy);
				}
			}
			return FALSE;
		}

		if (cmd->apsFrame->clusterId == ZCL_IAS_ZONE_CLUSTER_ID)
		{
			if (status == 0x00)
			{
				if (attr_type == ZCL_BITMAP16_ATTRIBUTE_TYPE)
				{
					int16u zone_status = message[7] + (message[8] << 8);
					DBGPRINT("zone_status:0x%04X \n", zone_status);
					zbdSysEventDoubleCode(ZBD_EVENT_ZONE_STATUS, source_nodeid, zone_status, -1);
				}
			}
			return FALSE;
		}

		if (cmd->apsFrame->clusterId == ZCL_BASIC_CLUSTER_ID)
		{
			char fullpath_file[80];
			zbdMakeAttributesFileName(fullpath_file, sizeof(fullpath_file), eui64, source_endpoint, ZCL_BASIC_CLUSTER_ID);
			DBGPRINT("zbdMakeAttributesFileName fullpath_file:%s \n", fullpath_file);
			zbdParseSaveBasicCluster(message, length, fullpath_file);
			return FALSE;
		}
	}

	return FALSE;
}

/** @brief Trust Center Join
 *
 * This callback is called from within the application framework's
 * implementation of emberTrustCenterJoinHandler or
 * ezspTrustCenterJoinHandler. This callback provides the same arguments
 * passed to the TrustCenterJoinHandler. For more information about the
 * TrustCenterJoinHandler please see documentation included in
 * stack/include/trust-center.h.
 *
 * @param newNodeId   Ver.: always
 * @param newNodeEui64   Ver.: always
 * @param parentOfNewNode   Ver.: always
 * @param status   Ver.: always
 * @param decision   Ver.: always
 */

static const char * statusStrings[] =
{
  "STANDARD_SECURITY_SECURED_REJOIN",
  "STANDARD_SECURITY_UNSECURED_JOIN",
  "DEVICE_LEFT",
  "STANDARD_SECURITY_UNSECURED_REJOIN",
  "HIGH_SECURITY_SECURED_REJOIN",
  "HIGH_SECURITY_UNSECURED_JOIN",
  "Reserved 6",
  "HIGH_SECURITY_UNSECURED_REJOIN"
  "Reserved 8",
  "Reserved 9",
  "Reserved 10",
  "Reserved 11",
  "Reserved 12",
  "Reserved 13",
  "Reserved 14",
  "Reserved 15"
};

static const char * decisionString[] =
{
  "use preconfigured key",
  "send key in the clear",
  "deny join",
  "no action",
};


void emberAfTrustCenterJoinCallback(EmberNodeId newNodeId,
                                    EmberEUI64 newNodeEui64,
                                    EmberNodeId parentOfNewNode,
                                    EmberDeviceUpdate status,
                                    EmberJoinDecision decision)
{
	int8u i;

	DBGPRINT("emberAfTrustCenterJoinCallback:\n");
	emberSerialPrintf(APP_SERIAL, "TC Join Callback 0x%2x ", newNodeId);
	emberSerialPrintf(APP_SERIAL, "%s", "0x");
	for(i=0; i<=7; i++) {
		emberSerialPrintf(APP_SERIAL,
				"%x",
				newNodeEui64[7-i]);
	}

	if(status >= 0 && status < 16)
		emberSerialPrintf(APP_SERIAL, " %s", statusStrings[status]);
	else
		emberSerialPrintf(APP_SERIAL, " %d", status);

	if (decision >= 0 && decision < 4)
		emberSerialPrintf(APP_SERIAL, " %s\n", decisionString[decision]);
	else
		emberSerialPrintf(APP_SERIAL, " %d\n", decision);

	switch (status)
	{
		case EMBER_STANDARD_SECURITY_SECURED_REJOIN:
		case EMBER_STANDARD_SECURITY_UNSECURED_REJOIN:
		case EMBER_HIGH_SECURITY_SECURED_REJOIN:
		case EMBER_HIGH_SECURITY_UNSECURED_REJOIN:
			DBGPRINT("REJOIN status: %d\n", status);
			zbdUpdateDBNodeId(newNodeEui64, newNodeId);
			int8u index = getAddressIndexFromIeee(newNodeEui64);
			if(index != NULL_INDEX)
			{
				if (zbdIsNodeSensor(newNodeId))
				{
					int16u nowQS = halCommonGetInt16uQuarterSecondTick();
					zbdScheduleCieWriting(newNodeId, nowQS);
				}
			}
			break;

		case EMBER_STANDARD_SECURITY_UNSECURED_JOIN:
		case EMBER_HIGH_SECURITY_UNSECURED_JOIN:
			DBGPRINT("JOIN status: %d\n", status);
			{
				FILE *fp = fopen(ZB_JOIN_LIST, "a+");
				if (fp)
				{
					fprintf(fp, "%02x ", newNodeId);
					for(i=0; i<=7; i++)
					{
						fprintf(fp, "%02x", newNodeEui64[i]);
					}
					fprintf(fp, " done\n");
					fclose(fp);
				};

				zbdUpdateDBNodeId(newNodeEui64, newNodeId);
				int16u nowQS = halCommonGetInt16uQuarterSecondTick();
				zbdScheduleEndpointsRequest(newNodeId, nowQS + 6);
			}
			break;

		case EMBER_DEVICE_LEFT:

			newDeviceLeftHandler(newNodeEui64, CLEAN_MODE_ALL);

			if (s_State == ZBD_ST_ZB_TRUSTCENTER)
			{
				zbdReplyGeneralResponse(NULL, s_ClientFD, ZB_RET_SUCCESS, ZB_RET_SUCCESS);
				close(s_ClientFD);
				zbdSetState(ZBD_ST_IPC_RECV);
			}
			break;
	}
}

static void zbdInitIPCServer(void)
{
	int rc;
	int msec = 100;
	int onum =1;
	int len;

	if ((s_ServerSock = socket(AF_UNIX, SOCK_STREAM, 0)) == ERROR)
	{
		DBGPRINT("Server Socket Creation Failed.\n");
		return;
	}

	rc = setsockopt(s_ServerSock, SOL_SOCKET,  SO_REUSEADDR, (char *)&onum, sizeof(onum));
	if (rc < 0)
	{
		DBGPRINT("setsockopt() failed\n");
		return;
	}

	s_LocalSockAddr.sun_family = AF_UNIX;

	strncpy(s_LocalSockAddr.sun_path, SOCK_ZB_PATH, sizeof(s_LocalSockAddr.sun_path)-1);
	unlink(s_LocalSockAddr.sun_path);
	len = strlen(s_LocalSockAddr.sun_path) + sizeof(s_LocalSockAddr.sun_family);

	if (bind(s_ServerSock, (struct sockaddr *)&s_LocalSockAddr, len) == ERROR)
	{
		DBGPRINT(":bind failed\n");
		return;
	}

	if (listen(s_ServerSock, 32) == ERROR)
	{
		DBGPRINT(":Listen Failed\n");
		return;
	}

	s_LenRemote = sizeof(s_RemoteSockAddr);

	FD_ZERO(&s_MasterSet);
	s_MaxSD = s_ServerSock;
	FD_SET(s_ServerSock, &s_MasterSet);

	s_SockTimeout.tv_sec = msec / 1000ul;
	s_SockTimeout.tv_usec = (msec % 1000ul) * 1000ul;

	zbdSetState(ZBD_ST_IPC_RECV);
}

static void zbdSetRxLNAMode(void)
{
	EzspStatus ezsp_status;
	typedef struct tagTWeMoEM357Conf {int8u port_pin; int8u cfg; int8u out;} TWeMoEM357Conf;
	TWeMoEM357Conf em357_conf[] = {
		{8, 1, 1},// PB0, output push-pull, output value is 1
		{13, 1, 1},// PB5, output push-pull, output value is 1
	};
	int num_el = sizeof(em357_conf)/sizeof(em357_conf[0]);

	if (zbdGetPowerTableIdx() != ZBD_HIGH_POWER)
	{
		return;
	}

	int i;
	for (i=0; i < num_el; i++)
	{
		int trial = 3;
		while(trial)
		{
			ezsp_status = ezspSetGpioCurrentConfiguration(
					em357_conf[i].port_pin,
					em357_conf[i].cfg,
					em357_conf[i].out);

			DBGPRINT("ezspSetGpioCurrentConfiguration status:%d, i:%d, port:%d, cfg:%d, out:%d\n", ezsp_status, i, em357_conf[i].port_pin, em357_conf[i].cfg, em357_conf[i].out );

			if (ezsp_status == EZSP_SUCCESS)
			{
				trial = 0;
			}
			else
			{
				trial--;
			}
		}
	}
}

static int IsFileExisting(char * fileName)
{
	if (access(fileName, F_OK) != -1)
		return TRUE;
	else
		return FALSE;
}

/** @brief Main Init
 *
 * This function is called from the application’s main function. It gives the
 * application a chance to do any initialization required at system startup.
 * Any code that you would normally put into the top of the application’s
 * main() routine should be put into this function.
        Note: No callback
 * in the Application Framework is associated with resource cleanup. If you
 * are implementing your application on a Unix host where resource cleanup is
 * a consideration, we expect that you will use the standard Posix system
 * calls, including the use of atexit() and handlers for signals such as
 * SIGTERM, SIGINT, SIGCHLD, SIGPIPE and so on. If you use the signal()
 * function to register your signal handler, please mind the returned value
 * which may be an Application Framework function. If the return value is
 * non-null, please make sure that you call the returned function from your
 * handler to avoid negating the resource cleanup of the Application Framework
 * itself.
 *
 */

void emberAfMainInitCallback(void)
{
	fprintf(stderr, "FW_REV:%s\n", FW_REV);

	{
		char cwd[1024];
		DBGPRINT( "Current Working Directory: %s\n", (NULL == getcwd(cwd, 1024) ? "UNKNOWN!" : cwd));
	}

	if (IsFileExisting(ZB_MODE_ROUTER))
	{
		emberConcentratorStopDiscovery();
	}

	if (IsFileExisting(ZBD_FILE_PRESENCE_TH))
	{
		int time_read;
		if ( SUCCESS == zbdReadIntFromFile(ZBD_FILE_PRESENCE_TH, &time_read))
		{
			s_BatteryReportThresholdSec = time_read;
			DBGPRINT("Presence: DBG Threshold Read : %d \n", s_BatteryReportThresholdSec);
		}
	}

	zbdSetRxLNAMode();

	initAddressTable();

	zbdInitIPCServer();

	zbdSysEventInit();

	emberAfSetEzspPolicy(EZSP_POLL_HANDLER_POLICY,
			EZSP_POLL_HANDLER_CALLBACK,
			"Poll Handler Policy",
			"Poll Handler Callback");

	emberEventControlSetDelayQS(presenceEvent, 12); // 12 for 3 secs
}

static void zbdDbgShowTickCount(void)
{
	static int tick_count;
	static int32u start_time;

	if (tick_count == 0)
	{
		start_time = halCommonGetInt32uMillisecondTick();
	}

	tick_count++;

	if (tick_count == 10000)
	{
		int32u end_time = halCommonGetInt32uMillisecondTick();

		if (end_time > start_time)
		{
			DBGPRINT("emberAfMainTickCallback 10000 Ticks  elapsed:%d ms\n", end_time - start_time);
		}

		tick_count = 0;
	}
}

/** @brief Main Tick
 *
 * Whenever main application tick is called, this callback will be called at
 * the end of the main tick execution.
 *
 */
void emberAfMainTickCallback(void)
{
	emberRunEvents(events);

#if 0
	zbdDbgShowTickCount();
#endif

	switch (s_State)
	{
		case ZBD_ST_IPC_RECV:
			zbdIPCRecv();
			break;

		case ZBD_ST_ZB_MSG_SENT_STATUS:
		case ZBD_ST_ZB_TRUSTCENTER:
			{
				int16u now_qs = halCommonGetInt16uQuarterSecondTick();

				if (s_TimeToAbort <= now_qs)
				{
					int ret = ZB_RET_FAIL;
					int i;

					if (s_State != ZBD_ST_ZB_TRUSTCENTER)
					{
						i = getAddressIndexFromIeee(s_LastEUI);
						if (i != NULL_INDEX)
						{
							DBGPRINT("addressTable[i].nodeId: 0x%02X \n", addressTable[i].nodeId);
							if (FALSE == zbdIsNodeSensor(addressTable[i].nodeId))
							{
								zbdScheduleAddressRequest(addressTable[i].nodeId);
							}
						}
					}

					DBGPRINT("\nNo Responses... Abort and resume for next cmd.\n");
					zbdReplyGeneralResponse(NULL, s_ClientFD, ZB_RET_NO_RESPONSE, ZB_RET_NO_RESPONSE);
					close(s_ClientFD);
					zbdSetState(ZBD_ST_IPC_RECV);
				}
			}
			break;

		default:
			break;
	}
}

/** @brief Pre ZDO Message Received
 *
 * This function passes the application an incoming ZDO message and gives the
 * appictation the opportunity to handle it. By default, this callback returns
 * FALSE indicating that the incoming ZDO message has not been handled and
 * should be handled by the Application Framework.
 *
 * @param emberNodeId   Ver.: always
 * @param apsFrame   Ver.: always
 * @param message   Ver.: always
 * @param length   Ver.: always
 */
boolean emberAfPreZDOMessageReceivedCallback(EmberNodeId emberNodeId,
                                             EmberApsFrame* apsFrame,
                                             int8u* message,
                                             int16u length)
{
	int i;
	int status;

	DBGPRINT("\nemberAfPreZDOMessageReceivedCallback emberNodeId:0x%04x, clusterId:0x%04x, length:%d\n", emberNodeId, apsFrame->clusterId, length);
	DBGPRINT("\n");
	for (i=0;i < length; i++)
	{
		DBGPRINT("%02x ", message[i]);
	}
	DBGPRINT("\n");

	switch (apsFrame->clusterId)
	{
		case NETWORK_ADDRESS_RESPONSE:
			DBGPRINT("NETWORK_ADDRESS_RESPONSE\n");
			break;
		case IEEE_ADDRESS_RESPONSE:
			DBGPRINT("IEEE_ADDRESS_RESPONSE\n");
			break;
		case NODE_DESCRIPTOR_RESPONSE:
			DBGPRINT("NODE_DESCRIPTOR_RESPONSE\n");
			break;
		case POWER_DESCRIPTOR_RESPONSE:
			DBGPRINT("POWER_DESCRIPTOR_RESPONSE\n");
			break;
		case SIMPLE_DESCRIPTOR_RESPONSE:
			DBGPRINT("SIMPLE_DESCRIPTOR_RESPONSE\n");
			{
				int8u zb_status = message[1];
				EmberNodeId matchId = message[2] + (message[3] << 8);
				int8u len_msg = message[4];

				if (zb_status != EMBER_SUCCESS || len_msg == 0)
				{
					DBGPRINT("Error:0x%x len_msg:%d \n", zb_status, len_msg);
					if ( s_State == ZBD_ST_ZB_MSG_SENT_STATUS )
					{
						zbdReplyGeneralResponse(NULL, s_ClientFD, zb_status, ZB_RET_FAIL);
						close(s_ClientFD);
						zbdSetState(ZBD_ST_IPC_RECV);
					}

					return FALSE;
				}

				int8u endpoint = message[5];
				int16u profile_id = message[6] + (message[7] << 8);
				int16u device_id = message[8] + (message[9] << 8);
				int8u device_version = message[10] & 0xF;
				int8u reserved = message[10] >> 4;
				int8u inClusterCount = message[11];
				int8u outClusterCount = message[12+(inClusterCount*2)];
				const int16u* inClusterList = (int16u*)&message[12];
				const int8u* data_arr = &message[12];
				const int16u* outClusterList = (int16u*)&message[13+(inClusterCount*2)];
				int sensor_joined = FALSE;

				DBGPRINT("len_msg:%d endpoint:%d, profileid:0x%04X, deviceid:0x%04X, device_version:0x%X, reserved:0x%X\n", len_msg, endpoint, profile_id, device_id, device_version, reserved);
				DBGPRINT("node id:0x%x\n", matchId);
				DBGPRINT("inClusterCount:%d\n", inClusterCount);
				for (i = 0; i < inClusterCount; i++)
				{
					DBGPRINT("inCluster[%d]:0x%04x\n", i, inClusterList[i]);
					if(inClusterList[i] == ZCL_IAS_ZONE_CLUSTER_ID)
					{
						sensor_joined = TRUE;
					}
				}
				DBGPRINT("outClusterCount:%d\n", outClusterCount);
				for (i = 0; i < outClusterCount; i++)
				{
					DBGPRINT("outCluster[%d]:0x%04x\n", i, outClusterList[i]);
				}

				zbdSetNodeType(matchId, device_id);
				zbdSaveClusters(matchId, endpoint, device_id, inClusterList, inClusterCount);
				zbdScheduleBasicClusterRequest(matchId, endpoint); // get model code, app version, mfg name
				if (sensor_joined)
				{
					int16u nowQS = halCommonGetInt16uQuarterSecondTick();
					zbdScheduleCieWriting(matchId, nowQS + 1);
				}

				if ( s_State == ZBD_ST_ZB_MSG_SENT_STATUS )
				{
					if (s_Cur_CMD == IPC_CMD_GET_CLUSTERS)
					{
						zbdReplyArrayResponse(NULL, s_ClientFD, 2, data_arr, inClusterCount, device_id, ZB_RET_SUCCESS);
						close(s_ClientFD);
						zbdSetState(ZBD_ST_IPC_RECV);
					}
					else
					{
						zbdReplyGeneralResponse(NULL, s_ClientFD, ZB_RET_OUT_OF_ORDER , ZB_RET_OUT_OF_ORDER );
						close(s_ClientFD);
						zbdSetState(ZBD_ST_IPC_RECV);
					}
				}
			}

			return FALSE;

			break;

		case ACTIVE_ENDPOINTS_RESPONSE:
			DBGPRINT("ACTIVE_ENDPOINTS_RESPONSE\n");
			{
				EmberNodeId matchId = message[2] + (message[3] << 8);
				int8u ep_count = message[4];
				int8u *ep_list = &message[5];

				for (i = 0; i < ep_count; i++)
				{
					zbdSaveClusters(emberNodeId, ep_list[i], 0, NULL, 0);
				}

				if ( s_State == ZBD_ST_ZB_MSG_SENT_STATUS )
				{
					if (s_Cur_CMD == IPC_CMD_GET_ENDPOINT)
					{
						for (i = 0; i < ep_count; i++)
						{
							/* code */
							DBGPRINT("endpoint[%d]:%d\n", i, ep_list[i]);
						}

						zbdReplyArrayResponse(NULL, s_ClientFD, 1, ep_list, ep_count, 0, ZB_RET_SUCCESS);
						close(s_ClientFD);
						zbdSetState(ZBD_ST_IPC_RECV);
					}
					else
					{
						zbdReplyGeneralResponse(NULL, s_ClientFD, ZB_RET_OUT_OF_ORDER , ZB_RET_OUT_OF_ORDER );
						close(s_ClientFD);
						zbdSetState(ZBD_ST_IPC_RECV);
					}
				}
				else
				{
					for (i = 0; i < ep_count; i++)
					{
						if ( 1 != ep_list[i])
						{
							DBGPRINT("endpoint[%d]:%d\n", i, ep_list[i]);
							// getclusters
							status = emberSimpleDescriptorRequest(emberNodeId, ep_list[i], EMBER_AF_DEFAULT_APS_OPTIONS);
							DBGPRINT("emberSimpleDescriptorRequest nodeid:0x%04x, endpoint:%d, status:%d \n", emberNodeId, ep_list[i], status);
						}
					}
				}

				return FALSE;
			}
			break;
		case MATCH_DESCRIPTORS_RESPONSE:
			DBGPRINT("MATCH_DESCRIPTORS_RESPONSE\n");
			break;
		case DISCOVERY_CACHE_RESPONSE:
			DBGPRINT("DISCOVERY_CACHE_RESPONSE\n");
			break;
		case END_DEVICE_ANNOUNCE:
			DBGPRINT("END_DEVICE_ANNOUNCE\n");
			{
				char fullpath_file[80];
				EmberEUI64 eui64;
				getIeeeFromNodeId(emberNodeId, eui64);
				zbdMakeClustersFileName(fullpath_file, sizeof(fullpath_file), eui64, 1);
				if (!IsFileExisting(fullpath_file))
				{
					int16u nowQS = halCommonGetInt16uQuarterSecondTick();
					zbdScheduleEndpointsRequest(emberNodeId, nowQS);
				}
			}
			break;
		case END_DEVICE_ANNOUNCE_RESPONSE:
			DBGPRINT("END_DEVICE_ANNOUNCE_RESPONSE\n");
			break;
		case SYSTEM_SERVER_DISCOVERY_RESPONSE:
			DBGPRINT("SYSTEM_SERVER_DISCOVERY_RESPONSE\n");
			break;
		default:
			DBGPRINT("Other Incoming ZDO reponses\n");
	};

	return FALSE;
}

void zbdWeMoCliServiceDiscoveryCallback(const EmberAfServiceDiscoveryResult* result)
{
	if (emberAfHaveDiscoveryResponseStatus(result->status))
	{
		EmberNodeId node_id = result->matchAddress;
		DBGPRINT("zbdWeMoCliServiceDiscoveryCallback Node ID:0x%x \n", node_id);

		if (result->zdoRequestClusterId == MATCH_DESCRIPTORS_REQUEST)
		{
			const EmberAfEndpointList* epList = (const EmberAfEndpointList*)result->responseData;
			DBGPRINT("Match discovery from 0x%2X, ep %d \n", node_id, epList->list[0]);
		}
		else if (result->zdoRequestClusterId == NETWORK_ADDRESS_REQUEST)
		{
			DBGPRINT("NWK Address response: 0x%2X \n", node_id);
		}
		else if (result->zdoRequestClusterId == IEEE_ADDRESS_REQUEST)
		{
			int i;
			int8u* eui64ptr = (int8u*)(result->responseData);
			DBGPRINT("IEEE Address response: ");
			/*emberAfPrintBigEndianEui64(INT8U_TO_EUI64_CAST(eui64ptr));*/
			printEUI64(eui64ptr);
			DBGPRINT("\n");
		}

		if (result->responseData)
		{
			int8u* eui64ptr = (int8u*)(result->responseData);

			DBGPRINT("NodeID:0x%x, ResponseData:", node_id);
			printEUI64(eui64ptr);
			DBGPRINT("\n");

			if (node_id != 0)
			{
				zbdUpdateDBNodeId(eui64ptr, node_id);
				zbdSysEventErrorCode(node_id, 0, 0);
			}
			else
			{
				DBGPRINT("Node ID Zero:0x%04x \n", node_id);
			}
#if 0
			// sync blocking call response
			zbdReplyGeneralResponse(NULL, s_ClientFD, node_id, 0);
			close(s_ClientFD);
			zbdSetState(ZBD_ST_IPC_RECV);
#endif
		}
	}

	if (result->status != EMBER_AF_BROADCAST_SERVICE_DISCOVERY_RESPONSE_RECEIVED)
	{
		DBGPRINT("Service Discovery done. \n");
	}
}

/** @brief Message Sent
 *
 * This function is called by the application framework from the message sent
 * handler, when it is informed by the stack regarding the message sent
 * status. All of the values passed to the emberMessageSentHandler are passed
 * on to this callback. This provides an opportunity for the application to
 * verify that its message has been sent successfully and take the appropriate
 * action. This callback should return a boolean value of TRUE or FALSE. A
 * value of TRUE indicates that the message sent notification has been handled
 * and should not be handled by the application framework.
 *
 * @param type   Ver.: always
 * @param indexOrDestination   Ver.: always
 * @param apsFrame   Ver.: always
 * @param msgLen   Ver.: always
 * @param message   Ver.: always
 * @param status   Ver.: always
 */
boolean emberAfMessageSentCallback(EmberOutgoingMessageType type,
                                   int16u indexOrDestination,
                                   EmberApsFrame* apsFrame,
                                   int16u msgLen,
                                   int8u* message,
                                   EmberStatus status)
{
	int ret;
	int16u cluster_id = 0;

	if (apsFrame)
	{
		cluster_id = apsFrame->clusterId;
	}

	DBGPRINT("emberAfMessageSentCallback \n");
	DBGPRINT("Ember_Status_Code: 0x%X type:0x%X indexOrDestination:0x%04X msgLen:0x%X\n\n", status, type, indexOrDestination, msgLen);
	zbdDbgPrintByteStream(message, msgLen);

	switch (status)
	{
		case EMBER_SUCCESS:
			if (indexOrDestination < 0xFFFC)
			{
				int8u index = getAddressIndexFromNodeId(indexOrDestination);
				if (index != NULL_INDEX)
				{
					if (addressTable[index].nodeState == NODE_ST_LEAVE_REQUESTING)
						addressTable[index].nodeState = NODE_ST_NULL;
				}
			}
			ret = ZB_RET_SUCCESS;
			break;
		case EMBER_DELIVERY_FAILED:
			ret = ZB_RET_DELIVERY_FAILED;
			if (indexOrDestination < 0xFFFC)
			{
				zbdSysEventErrorCode(indexOrDestination, ret, cluster_id);
				if (FALSE == zbdIsNodeSensor(indexOrDestination))
				{
					zbdScheduleAddressRequest(indexOrDestination);
				}
			}
			break;
		default:
			ret = ZB_RET_FAIL;

			break;
	}

	if (s_State == ZBD_ST_ZB_MSG_SENT_STATUS)
	{
		switch(s_PrevState)
		{
			case ZBD_ST_ZB_SET:
				zbdReplyGeneralResponse(NULL, s_ClientFD, ret, ret);
				close(s_ClientFD);
				zbdSetState(ZBD_ST_IPC_RECV);
				break;

			case ZBD_ST_ZB_GET:
				if (status == EMBER_SUCCESS)
				{
#if 0
					zbdSetState(ZBD_ST_ZB_REPLY_RECV);

					zbdSetTimeToAbort();
#endif
				}
				else
				{
					zbdReplyGeneralResponse(NULL, s_ClientFD, ret, ret);
					close(s_ClientFD);
					zbdSetState(ZBD_ST_IPC_RECV);
				}
				break;

			default:
				break;
		}
	}

	return FALSE;
}

/** @brief Get Current Time
 *
 * This callback is called when device attempts to get current time from the
 * hardware. If this device has means to retrieve exact time, then this method
 * should implement it. If the callback can't provide the exact time it should
 * return 0 to indicate failure. Default action is to return 0, which
 * indicates that device does not have access to real time.
 *
 */
int32u emberAfGetCurrentTimeCallback(void)
{
	return halCommonGetInt32uMillisecondTick();
}

static int16u zbdSetNodePollTime(EmberNodeId nodeId, const int singlePollSec, const int threshold)
{
	int16u elapsed_qs = NULL_TIME_QS;
	int8u index = getAddressIndexFromNodeId(nodeId);

	if (index != NULL_INDEX)
	{
		int16u now_qs = halCommonGetInt16uQuarterSecondTick();

		if (addressTable[index].timeQSLastPoll == NULL_TIME_QS)
		{
			addressTable[index].timeQSLastPoll = now_qs;
		}

		elapsed_qs = (now_qs - addressTable[index].timeQSLastPoll);
		addressTable[index].timeQSLastPoll = now_qs;

		DBGPRINT("zbdSetNodePollTime: nodeId:0x%04x, pollCount:%d, nodeType:0x%04x, index:%d, now_qs:%d elapsed_qs:%d\n",
				nodeId, addressTable[index].pollCount, addressTable[index].nodeType, index, now_qs, elapsed_qs);

		if ( (singlePollSec != 0) // only if singlepoll specified
				&& (elapsed_qs > (2 * 4 * singlePollSec)) // missed one or more poll
				&& (addressTable[index].pollCount < threshold) // before reaching the threshold
		   )
		{
			DBGPRINT("elapsed_qs:%d, Returning \n", elapsed_qs);
			addressTable[index].pollCount = 0; // count again from the start
		}

		if ( addressTable[index].pollCount == (threshold - 1) ) // entering the threshold
		{
			addressTable[index].pollCount = threshold;
			DBGPRINT("PRESENT! \n");
			zbdSysEventSingleCode(ZBD_EVENT_NODE_PRESENT, nodeId, 1);

#ifdef DBG
			char dbg_msg[150] = { 0 };

			snprintf(dbg_msg, sizeof(dbg_msg) - 1 ,
					"echo \" pollCount:%d nodeId:0x%04x, nodeType:0x%04x, index:%d, timeQSLastPoll:%d \" > \/tmp\/zigbee.dbg.%04x.present ", 
					addressTable[index].pollCount,
					nodeId,
					addressTable[index].nodeType,
					index,
					now_qs,
					nodeId);

			DBGPRINT("%s \n", dbg_msg);
			system(dbg_msg);
#endif

			return elapsed_qs;
		}

		if ( (addressTable[index].pollCount < threshold ) ) // before reaching the threshold
		{
			addressTable[index].pollCount += 1;
		}

	}

	return elapsed_qs;
}

static int16u zbdGetNodePollTime(EmberNodeId nodeId)
{
	int8u index = getAddressIndexFromNodeId(nodeId);
	DBGPRINT("zbdGetNodePollTime: nodeId:0x%04x, index:%d \n", nodeId, index);
	if (index != NULL_INDEX)
	{
		DBGPRINT("timeQSLastPoll:%d \n", addressTable[index].timeQSLastPoll);
		return addressTable[index].timeQSLastPoll;
	}
	return NULL_TIME_QS;
}


/** @brief ezspPollHandler
 *
 * This is called by EZSP when poll is initiated. See EZSP documentation for
 * more information.
 *
 * @param childId The ID of the child that is requesting data  Ver.: always
 */
void ezspPollHandler(EmberNodeId childId)
{
	if (zbdGetNodeType(childId) == ZB_ZONE_FOB)
	{
		zbdSetNodePollTime(childId, KEYFOB_POLL_INTERVAL_SEC, KEYFOB_PRESENCE_COUNT);
	}

	usleep(0);
	/*DBGPRINT("ezspPollHandler childId:0x%04X elapsed_qs:%d \n", childId, elapsed_qs);*/
}

void presenceEventHandler(void)
{
	int i;
	int16u now_qs;

	emberEventControlSetDelayQS(presenceEvent, 12); // 12 for 3 secs

	now_qs = halCommonGetInt16uQuarterSecondTick();

	for (i = 0; i < ADDRESS_TABLE_SIZE; ++i)
	{
		if (addressTable[i].timeQSLastPoll != NULL_TIME_QS)
		{
			int16u elapsed_sec = (now_qs - addressTable[i].timeQSLastPoll) / 4;
			int has_left = FALSE;
			int threshold = 0;

			switch (addressTable[i].nodeType)
			{
				case ZB_ZONE_FOB:
					threshold = KEYFOB_THRESHOLD_SEC_NO_POLL;
					/*DBGPRINT("KEYFOB_THRESHOLD_SEC_NO_POLL: elapsed_sec:%d, TH:%d \n", elapsed_sec, threshold);*/
					if ( (elapsed_sec > threshold)
							&& addressTable[i].pollCount >= KEYFOB_PRESENCE_COUNT )
					{
						DBGPRINT("KEYFOB_THRESHOLD_SEC_NO_POLL:%d \n", threshold);
						has_left = TRUE;
					}
					break;

				case ZB_ZONE_DOOR:
				case ZB_ZONE_MOTION:
				case ZB_ZONE_FIRE_ALARM:
					threshold = s_BatteryReportThresholdSec;
					/*DBGPRINT("BATTERYREPORT_THRESHOLD_SEC_NO_POLL: elapsed_sec:%d, TH:%d \n", elapsed_sec, threshold);*/
					if ( (elapsed_sec > threshold)
							&& addressTable[i].pollCount != 0 )
					{
						DBGPRINT("BATTERYREPORT_THRESHOLD_SEC_NO_POLL: elapsed_sec:%d, TH:%d \n", elapsed_sec, threshold);
						has_left = TRUE;
					}
					break;

				case ZB_DEV_UNKNOWN:
					break;

				default:
					break;
			}

			if (has_left)
			{
				addressTable[i].pollCount = 0;
				DBGPRINT("NOT PRESENT! \n");
				zbdSysEventSingleCode(ZBD_EVENT_NODE_PRESENT, addressTable[i].nodeId, 0);
			}

#ifdef DBG
			char dbg_msg[150] = { 0 };

			if ( (has_left == FALSE)
					&& (elapsed_sec > threshold)
					&& (addressTable[i].pollCount != 0) )
			{
				DBGPRINT("Presence: NOTLEFT: nodeid:0x%04X, nodetype:0x%04X, now_qs:%d, timeQSLastPoll:%d, elapsed_sec:%d TH:%d\n",
						addressTable[i].nodeId,
						addressTable[i].nodeType,
						now_qs,
						addressTable[i].timeQSLastPoll,
						elapsed_sec,
						threshold);
			}

			snprintf(dbg_msg, sizeof(dbg_msg) - 1 , 
					"echo \"left:%d, nodeid:0x%04X, nodetype:0x%04X, now_qs:%d, timeQSLastPoll:%d, elapsed_sec:%d, TH:%d \" > \/tmp\/zigbee.dbg.%04x.now ", 
					has_left,
					addressTable[i].nodeId,
					addressTable[i].nodeType,
					now_qs,
					addressTable[i].timeQSLastPoll,
					elapsed_sec,
					threshold,
					addressTable[i].nodeId);

			/*DBGPRINT("%s \n", dbg_msg);*/
			system(dbg_msg);
#endif

		}
	}
}

/** @brief Client Message Sent
 *
 * IAS Zone cluster, Client Message Sent
 *
 * @param type The type of message sent  Ver.: always
 * @param indexOrDestination The destination or address to which the message
 * was sent  Ver.: always
 * @param apsFrame The APS frame for the message  Ver.: always
 * @param msgLen The length of the message  Ver.: always
 * @param message The message that was sent  Ver.: always
 * @param status The status of the sent message  Ver.: always
 */
void emberAfIasZoneClusterClientMessageSentCallback(EmberOutgoingMessageType type,
                                                    int16u indexOrDestination,
                                                    EmberApsFrame * apsFrame,
                                                    int16u msgLen,
                                                    int8u * message,
                                                    EmberStatus status)
{
	DBGPRINT("emberAfIasZoneClusterClientMessageSentCallback destination:0x%02X, msgLen:%d, status:%d \n", indexOrDestination, msgLen, status);
	zbdDbgPrintByteStream(message, msgLen);
}

/** @brief Button Event
 *
 * This allows another module to get notification when a button is pressed and
 * released but the button joining plugin did not handle it.  This callback is
 * NOT called in ISR context so there are no restrictions on what code can
 * execute.
 *
 * @param buttonNumber The button number that was pressed.  Ver.: always
 * @param buttonPressDurationMs The length of time button was held down before
 * it was released.  Ver.: always
 */
void emberAfPluginButtonJoiningButtonEventCallback(int8u buttonNumber,
                                                   int32u buttonPressDurationMs)
{
}

/** @brief Broadcast Sent
 *
 * This function is called when a new MTORR broadcast has been successfully
 * sent by the concentrator plugin.
 *
 */
void emberAfPluginConcentratorBroadcastSentCallback(void)
{
}

/** @brief Finished
 *
 * This callback is fired when the network-find plugin is finished with the
 * forming or joining process.  The result of the operation will be returned
 * in the status parameter.
 *
 * @param status   Ver.: always
 */
void emberAfPluginNetworkFindFinishedCallback(EmberStatus status)
{
	DBGPRINT("emberAfPluginNetworkFindFinishedCallback status:%d \n", status);

	zbdSysEventSingleCode(ZBD_EVENT_SCANJOIN_STATUS, 0, status);
}

static int zbdGetPowerTableIdx(void)
{
	int ret;
	int8u tokenData[2]; // EZSP_MFG_PHY_CONFIG is 2-bytes
	ezspGetMfgToken(EZSP_MFG_PHY_CONFIG, tokenData);

	if ((tokenData[0] == 0xFE) && (tokenData[1] == 0xFF))
	{
		ret = ZBD_NORMAL_POWER;
		DBGPRINT("Normal Power Index:%d MFG TOKEN data:0x%02X 0x%02X \n", ret, tokenData[0], tokenData[1]);
	}
	else
	{
		ret = ZBD_HIGH_POWER;
		DBGPRINT("High Power Index:%d MFG TOKEN data:0x%02X 0x%02X \n", ret, tokenData[0], tokenData[1]);
	}


	return ret;
}

/** @brief Get Radio Power For Channel
 *
 * This callback is called by the framework when it is setting the radio power
 * during the discovery process. The framework will set the radio power
 * depending on what is returned by this callback.
 *
 * @param channel   Ver.: always
 */
int8s emberAfPluginNetworkFindGetRadioPowerForChannelCallback(int8u channel)
{
	int8s relative_power = EMBER_AF_PLUGIN_NETWORK_FIND_RADIO_TX_POWER;
	typedef struct tagChannelPower {int channel; int power[2];} TChannelPower;
	TChannelPower power_table[] = {
		{ 11, {-2, 6}},
		{ 12, {-2, 6}},
		{ 13, {-2, 6}},
		{ 14, {-2, 6}},
		{ 15, {-2, 6}},
		{ 16, {-2, 6}},
		{ 17, {-2, 6}},
		{ 18, {-3, 6}},
		{ 19, {-3, 6}},
		{ 20, {-3, 6}},
		{ 21, {-3, 6}},
		{ 22, {-3, 6}},
		{ 23, {-3, 6}},
		{ 24, {-3, 6}},
		{ 25, {-9, 6}},
	};
	int num_el = sizeof(power_table)/sizeof(power_table[0]);
	int idx_power;

	idx_power = zbdGetPowerTableIdx();

	int i;
	for (i=0; i < num_el; i++)
	{
		if (power_table[i].channel == channel)
		{
			relative_power = power_table[i].power[idx_power];
			DBGPRINT("i:%d table channel:%d power:%d\n", i, power_table[i].channel, relative_power);
			break;
		}
	}

	DBGPRINT("RADIO emberAfPluginNetworkFindGetRadioPowerForChannelCallback channel:%d power:%d\n", channel, relative_power);

	return relative_power;
}

/** @brief Join
 *
 * This callback is called by the plugin when a joinable network has been
 * found.  If the application returns TRUE, the plugin will attempt to join
 * the network.  Otherwise, the plugin will ignore the network and continue
 * searching.  Applications can use this callback to implement a network
 * blacklist.
 *
 * @param networkFound   Ver.: always
 * @param lqi   Ver.: always
 * @param rssi   Ver.: always
 */
boolean emberAfPluginNetworkFindJoinCallback(EmberZigbeeNetwork * networkFound,
                                             int8u lqi,
                                             int8s rssi)
{
	boolean result = TRUE;
	DBGPRINT("panId:0x%04x, channel:%d, allowing:%d, lqi:%d, rssi:%d\n", networkFound->panId, networkFound->channel, networkFound->allowingJoin, lqi, rssi);
	DBGPRINT("emberAfPluginNetworkFindJoinCallback, returning %d \n", result);
	return result;
}

/** @brief Select File Descriptors
 *
 * This function is called when the Gateway plugin will do a select() call to
 * yield the processor until it has a timed event that needs to execute.  The
 * function implementor may add additional file descriptors that the
 * application will monitor with select() for data ready.  These file
 * descriptors must be read file descriptors.  The number of file descriptors
 * added must be returned by the function (0 for none added).
 *
 * @param list A pointer to a list of File descriptors that the function
 * implementor may append to  Ver.: always
 * @param maxSize The maximum number of elements that the function implementor
 * may add.  Ver.: always
 */
int emberAfPluginGatewaySelectFileDescriptorsCallback(int* list,
                                                      int maxSize)
{
  return 0;
}

/** @brief Request Mirror
 *
 * This function is called by the Simple Metering client plugin whenever a
 * Request Mirror command is received.  The application should return the
 * endpoint to which the mirror has been assigned.  If no mirror could be
 * assigned, the application should return 0xFFFF.
 *
 * @param requestingDeviceIeeeAddress   Ver.: always
 */
int16u emberAfPluginSimpleMeteringClientRequestMirrorCallback(EmberEUI64 requestingDeviceIeeeAddress)
{
  return 0xFFFF;
}

/** @brief Remove Mirror
 *
 * This function is called by the Simple Metering client plugin whenever a
 * Remove Mirror command is received.  The application should return the
 * endpoint on which the mirror has been removed.  If the mirror could not be
 * removed, the application should return 0xFFFF.
 *
 * @param requestingDeviceIeeeAddress   Ver.: always
 */
int16u emberAfPluginSimpleMeteringClientRemoveMirrorCallback(EmberEUI64 requestingDeviceIeeeAddress)
{
  return 0xFFFF;
}

/** @brief Configured
 *
 * This callback is called by the Reporting plugin whenever a reporting entry
 * is configured, including when entries are deleted or updated.  The
 * application can use this callback for scheduling readings or measurements
 * based on the minimum and maximum reporting interval for the entry.  The
 * application should return EMBER_ZCL_STATUS_SUCCESS if it can support the
 * configuration or an error status otherwise.  Note: attribute reporting is
 * required for many clusters and attributes, so rejecting a reporting
 * configuration may violate ZigBee specifications.
 *
 * @param entry   Ver.: always
 */
EmberAfStatus emberAfPluginReportingConfiguredCallback(const EmberAfPluginReportingEntry * entry)
{
  return EMBER_ZCL_STATUS_SUCCESS;
}


