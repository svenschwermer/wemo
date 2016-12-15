// This file is generated by AppBuilder. Please do not edit manually.
// 
//

// Enclosing macro to prevent multiple inclusion
#ifndef __AF_ENDPOINT_CONFIG__
#define __AF_ENDPOINT_CONFIG__


// Fixed number of defined endpoints
#define FIXED_ENDPOINT_COUNT 1




// Generated attributes
#define GENERATED_ATTRIBUTES { \
    { 0x0000, ZCL_INT8U_ATTRIBUTE_TYPE, 1, (ATTRIBUTE_MASK_SINGLETON), { (int8u*)0x01 } }, /* 0 / Basic / ZCL version*/\
    { 0x0007, ZCL_ENUM8_ATTRIBUTE_TYPE, 1, (ATTRIBUTE_MASK_SINGLETON), { (int8u*)0x00 } }, /* 1 / Basic / power source*/\
    { 0x0000, ZCL_INT16U_ATTRIBUTE_TYPE, 2, (ATTRIBUTE_MASK_WRITABLE), { (int8u*)0x0000 } }, /* 2 / Identify / identify time*/\
    { 0x0000, ZCL_UTC_TIME_ATTRIBUTE_TYPE, 4, (ATTRIBUTE_MASK_WRITABLE|ATTRIBUTE_MASK_SINGLETON), { NULL } }, /* 3 / Time / time*/\
    { 0x0001, ZCL_BITMAP8_ATTRIBUTE_TYPE, 1, (ATTRIBUTE_MASK_WRITABLE|ATTRIBUTE_MASK_SINGLETON), { (int8u*)0x00 } }, /* 4 / Time / time status*/\
  }


// Cluster function static arrays
#define GENERATED_FUNCTION_ARRAYS \
const EmberAfGenericClusterFunction emberAfFuncArrayIdentifyClusterServer[] = { (EmberAfGenericClusterFunction)emberAfIdentifyClusterServerInitCallback,(EmberAfGenericClusterFunction)emberAfIdentifyClusterServerAttributeChangedCallback}; \
const EmberAfGenericClusterFunction emberAfFuncArrayTimeClusterServer[] = { (EmberAfGenericClusterFunction)emberAfTimeClusterServerInitCallback}; \
const EmberAfGenericClusterFunction emberAfFuncArrayIasZoneClusterClient[] = { (EmberAfGenericClusterFunction)emberAfIasZoneClusterClientInitCallback,(EmberAfGenericClusterFunction)emberAfIasZoneClusterClientMessageSentCallback}; \
const EmberAfGenericClusterFunction emberAfFuncArrayOtaBootloadClusterServer[] = { (EmberAfGenericClusterFunction)emberAfOtaBootloadClusterServerInitCallback}; \


// Clusters defitions
#define GENERATED_CLUSTERS { \
    { 0x0000, (EmberAfAttributeMetadata*)&(generatedAttributes[0]), 0, 0, (CLUSTER_MASK_CLIENT), NULL,  },    \
    { 0x0000, (EmberAfAttributeMetadata*)&(generatedAttributes[0]), 2, 0, (CLUSTER_MASK_SERVER), NULL,  },    \
    { 0x0003, (EmberAfAttributeMetadata*)&(generatedAttributes[2]), 0, 0, (CLUSTER_MASK_CLIENT), NULL,  },    \
    { 0x0003, (EmberAfAttributeMetadata*)&(generatedAttributes[2]), 1, 2, (CLUSTER_MASK_SERVER| CLUSTER_MASK_INIT_FUNCTION| CLUSTER_MASK_ATTRIBUTE_CHANGED_FUNCTION), emberAfFuncArrayIdentifyClusterServer, },    \
    { 0x0004, (EmberAfAttributeMetadata*)&(generatedAttributes[3]), 0, 0, (CLUSTER_MASK_CLIENT), NULL,  },    \
    { 0x0005, (EmberAfAttributeMetadata*)&(generatedAttributes[3]), 0, 0, (CLUSTER_MASK_CLIENT), NULL,  },    \
    { 0x0006, (EmberAfAttributeMetadata*)&(generatedAttributes[3]), 0, 0, (CLUSTER_MASK_CLIENT), NULL,  },    \
    { 0x0008, (EmberAfAttributeMetadata*)&(generatedAttributes[3]), 0, 0, (CLUSTER_MASK_CLIENT), NULL,  },    \
    { 0x000A, (EmberAfAttributeMetadata*)&(generatedAttributes[3]), 2, 0, (CLUSTER_MASK_SERVER| CLUSTER_MASK_INIT_FUNCTION), emberAfFuncArrayTimeClusterServer, },    \
    { 0x0300, (EmberAfAttributeMetadata*)&(generatedAttributes[5]), 0, 0, (CLUSTER_MASK_CLIENT), NULL,  },    \
    { 0x0500, (EmberAfAttributeMetadata*)&(generatedAttributes[5]), 0, 0, (CLUSTER_MASK_CLIENT| CLUSTER_MASK_INIT_FUNCTION| CLUSTER_MASK_MESSAGE_SENT_FUNCTION), emberAfFuncArrayIasZoneClusterClient, },    \
    { 0x001A, (EmberAfAttributeMetadata*)&(generatedAttributes[5]), 0, 0, (CLUSTER_MASK_CLIENT), NULL,  },    \
    { 0x0020, (EmberAfAttributeMetadata*)&(generatedAttributes[5]), 0, 0, (CLUSTER_MASK_CLIENT), NULL,  },    \
    { 0x0B01, (EmberAfAttributeMetadata*)&(generatedAttributes[5]), 0, 0, (CLUSTER_MASK_CLIENT), NULL,  },    \
    { 0x0B03, (EmberAfAttributeMetadata*)&(generatedAttributes[5]), 0, 0, (CLUSTER_MASK_CLIENT), NULL,  },    \
    { 0x0B05, (EmberAfAttributeMetadata*)&(generatedAttributes[5]), 0, 0, (CLUSTER_MASK_CLIENT), NULL,  },    \
    { 0x0019, (EmberAfAttributeMetadata*)&(generatedAttributes[5]), 0, 0, (CLUSTER_MASK_SERVER| CLUSTER_MASK_INIT_FUNCTION), emberAfFuncArrayOtaBootloadClusterServer, },    \
    { 0x0702, (EmberAfAttributeMetadata*)&(generatedAttributes[5]), 0, 0, (CLUSTER_MASK_CLIENT), NULL,  },    \
  }


// Endpoint types
#define GENERATED_ENDPOINT_TYPES {        \
    { (EmberAfCluster*)&(generatedClusters[0]), 18, 2 }, \
  }


// Networks
#define EMBER_AF_GENERATED_NETWORKS { \
  {ZA_COORDINATOR, EMBER_AF_SECURITY_PROFILE_HA}, \
}
#define EMBER_AF_GENERATED_NETWORK_STRINGS  \
  "Primary", \


// Cluster manufacturer codes
#define GENERATED_CLUSTER_MANUFACTURER_CODES {      \
{0x00, 0x00} \
  }
#define GENERATED_CLUSTER_MANUFACTURER_CODE_COUNT 0

// Attribute manufacturer codes
#define GENERATED_ATTRIBUTE_MANUFACTURER_CODES {      \
{0x00, 0x00} \
  }
#define GENERATED_ATTRIBUTE_MANUFACTURER_CODE_COUNT 0


// Largest attribute size is needed for various buffers
#define ATTRIBUTE_LARGEST 4
// Total size of singleton attributes
#define ATTRIBUTE_SINGLETONS_SIZE 7

// Total size of attribute storage
#define ATTRIBUTE_MAX_SIZE 2

// Array of endpoints that are supported
#define FIXED_ENDPOINT_ARRAY { 1 }

// Array of profile ids
#define FIXED_PROFILE_IDS { 260 }

// Array of profile ids
#define FIXED_DEVICE_IDS { 80 }

// Array of profile ids
#define FIXED_DEVICE_VERSIONS { 0 }

// Array of endpoint types supported on each endpoint
#define FIXED_ENDPOINT_TYPES { 0 }

// Array of networks supported on each endpoint
#define FIXED_NETWORKS { 0 }


// Code used to configure the cluster event mechanism
#define EMBER_AF_GENERATED_EVENT_CODE \
  EmberEventControl emberAfIdentifyClusterServerTickCallbackControl1; \
  EmberEventControl emberAfTimeClusterServerTickCallbackControl1; \
  EmberEventControl emberAfOtaBootloadClusterServerTickCallbackControl1; \
  extern EmberEventControl emberAfPluginButtonJoiningButton0EventControl; \
  extern void emberAfPluginButtonJoiningButton0EventHandler(void); \
  extern EmberEventControl emberAfPluginButtonJoiningButton1EventControl; \
  extern void emberAfPluginButtonJoiningButton1EventHandler(void); \
  extern EmberEventControl emberAfPluginConcentratorUpdateEventControl; \
  extern void emberAfPluginConcentratorUpdateEventHandler(void); \
  extern EmberEventControl emberAfPluginIdentifyFeedbackProvideFeedbackEventControl; \
  extern void emberAfPluginIdentifyFeedbackProvideFeedbackEventHandler(void); \
  extern EmberEventControl emberAfPluginNetworkFindTickEventControl; \
  extern void emberAfPluginNetworkFindTickEventHandler(void); \
  extern EmberEventControl emberAfPluginIasZoneClientStateMachineEventControl; \
  extern void emberAfPluginIasZoneClientStateMachineEventHandler(void); \
  extern EmberEventControl emberAfPluginEzmodeCommissioningStateEventControl; \
  extern void emberAfPluginEzmodeCommissioningStateEventHandler(void); \
  extern EmberEventControl emberAfPluginReportingTickEventControl; \
  extern void emberAfPluginReportingTickEventHandler(void); \
  static void clusterTickWrapper(EmberEventControl *control, EmberAfTickFunction callback, int8u endpoint) \
  { \
    emberAfPushEndpointNetworkIndex(endpoint); \
    emberEventControlSetInactive(*control); \
    (*callback)(endpoint); \
    emberAfPopNetworkIndex(); \
  } \
  void emberAfIdentifyClusterServerTickCallbackWrapperFunction1(void) { clusterTickWrapper(&emberAfIdentifyClusterServerTickCallbackControl1, &emberAfIdentifyClusterServerTickCallback, 1); } \
  void emberAfTimeClusterServerTickCallbackWrapperFunction1(void) { clusterTickWrapper(&emberAfTimeClusterServerTickCallbackControl1, &emberAfTimeClusterServerTickCallback, 1); } \
  void emberAfOtaBootloadClusterServerTickCallbackWrapperFunction1(void) { clusterTickWrapper(&emberAfOtaBootloadClusterServerTickCallbackControl1, &emberAfOtaBootloadClusterServerTickCallback, 1); } \


// EmberEventData structs used to populate the EmberEventData table
#define EMBER_AF_GENERATED_EVENTS   \
  { &emberAfIdentifyClusterServerTickCallbackControl1, emberAfIdentifyClusterServerTickCallbackWrapperFunction1 }, \
  { &emberAfTimeClusterServerTickCallbackControl1, emberAfTimeClusterServerTickCallbackWrapperFunction1 }, \
  { &emberAfOtaBootloadClusterServerTickCallbackControl1, emberAfOtaBootloadClusterServerTickCallbackWrapperFunction1 }, \
  { &emberAfPluginButtonJoiningButton0EventControl, emberAfPluginButtonJoiningButton0EventHandler }, \
  { &emberAfPluginButtonJoiningButton1EventControl, emberAfPluginButtonJoiningButton1EventHandler }, \
  { &emberAfPluginConcentratorUpdateEventControl, emberAfPluginConcentratorUpdateEventHandler }, \
  { &emberAfPluginIdentifyFeedbackProvideFeedbackEventControl, emberAfPluginIdentifyFeedbackProvideFeedbackEventHandler }, \
  { &emberAfPluginNetworkFindTickEventControl, emberAfPluginNetworkFindTickEventHandler }, \
  { &emberAfPluginIasZoneClientStateMachineEventControl, emberAfPluginIasZoneClientStateMachineEventHandler }, \
  { &emberAfPluginEzmodeCommissioningStateEventControl, emberAfPluginEzmodeCommissioningStateEventHandler }, \
  { &emberAfPluginReportingTickEventControl, emberAfPluginReportingTickEventHandler }, \


#define EMBER_AF_GENERATED_EVENT_STRINGS   \
  "Identify Cluster Server EP 1",  \
  "Time Cluster Server EP 1",  \
  "Over the Air Bootloading Cluster Server EP 1",  \
  "Button Form/Join Code Plugin Button0",  \
  "Button Form/Join Code Plugin Button1",  \
  "Concentrator Support Plugin Update",  \
  "Identify Feedback Plugin ProvideFeedback",  \
  "Network Find Plugin Tick",  \
  "IAS Zone Client Plugin StateMachine",  \
  "EZ-Mode Commissioning Plugin State",  \
  "Reporting Plugin Tick",  \


// The length of the event context table used to track and retrieve cluster events
#define EMBER_AF_EVENT_CONTEXT_LENGTH 3

// EmberAfEventContext structs used to populate the EmberAfEventContext table
#define EMBER_AF_GENERATED_EVENT_CONTEXT { 0x1, 0x3, FALSE, EMBER_AF_OK_TO_HIBERNATE, &emberAfIdentifyClusterServerTickCallbackControl1}, \
{ 0x1, 0xa, FALSE, EMBER_AF_OK_TO_HIBERNATE, &emberAfTimeClusterServerTickCallbackControl1}, \
{ 0x1, 0x19, FALSE, EMBER_AF_OK_TO_HIBERNATE, &emberAfOtaBootloadClusterServerTickCallbackControl1}


#define EMBER_AF_GENERATED_PLUGIN_INIT_FUNCTION_DECLARATIONS \
  void emberAfPluginConcentratorInitCallback(void); \
  void emberAfPluginNetworkFindInitCallback(void); \
  void emberAfPluginAddressTableInitCallback(void); \
  void emberAfPluginReportingInitCallback(void); \


#define EMBER_AF_GENERATED_PLUGIN_INIT_FUNCTION_CALLS \
  emberAfPluginConcentratorInitCallback(); \
  emberAfPluginNetworkFindInitCallback(); \
  emberAfPluginAddressTableInitCallback(); \
  emberAfPluginReportingInitCallback(); \


#define EMBER_AF_GENERATED_PLUGIN_NCP_INIT_FUNCTION_DECLARATIONS \
  void emberAfPluginConcentratorNcpInitCallback(boolean memoryAllocation); \
  void emberAfPluginAddressTableNcpInitCallback(boolean memoryAllocation); \


#define EMBER_AF_GENERATED_PLUGIN_NCP_INIT_FUNCTION_CALLS \
  emberAfPluginConcentratorNcpInitCallback(memoryAllocation); \
  emberAfPluginAddressTableNcpInitCallback(memoryAllocation); \


#define EMBER_AF_GENERATED_PLUGIN_STACK_STATUS_FUNCTION_DECLARATIONS \
  void emberAfPluginNetworkFindStackStatusCallback(EmberStatus status); \


#define EMBER_AF_GENERATED_PLUGIN_STACK_STATUS_FUNCTION_CALLS \
  emberAfPluginNetworkFindStackStatusCallback(status); \


#define EMBER_AF_GENERATED_PLUGIN_MESSAGE_SENT_FUNCTION_DECLARATIONS \
  void emberAfPluginConcentratorMessageSentCallback(EmberOutgoingMessageType type, \
                    int16u indexOrDestination, \
                    EmberApsFrame *apsFrame, \
                    EmberStatus status, \
                    int16u messageLength, \
                    int8u *messageContents); \


#define EMBER_AF_GENERATED_PLUGIN_MESSAGE_SENT_FUNCTION_CALLS \
  emberAfPluginConcentratorMessageSentCallback(type, \
                    indexOrDestination, \
                    apsFrame, \
                    status, \
                    messageLength, \
                    messageContents); \

#endif // __AF_ENDPOINT_CONFIG__