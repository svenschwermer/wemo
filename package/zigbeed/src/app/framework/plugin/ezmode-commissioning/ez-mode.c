// *****************************************************************************
// * ez-mode.c
// *
// * This file provides a function set for initiating ez-mode commissioning
// * as both a client and a server.
// *
// * Copyright 2013 Silicon Laboratories, Inc.                              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/af-main.h"
#include "ez-mode.h"

//------------------------------------------------------------------------------
// Forward Declaration

EmberEventControl emberAfPluginEzmodeCommissioningStateEventControl;
static void serviceDiscoveryCallback(const EmberAfServiceDiscoveryResult *result);
static void createBinding(int8u *address);

//------------------------------------------------------------------------------
// Globals

#define stateEvent emberAfPluginEzmodeCommissioningStateEventControl

static int8u currentIdentifyingEndpoint;
static EmberNodeId currentIdentifyingAddress;

static int8u ezmodeClientEndpoint;

static EmberAfEzModeCommissioningDirection bindingDirection;
static const int16u* clusterIdsForEzModeMatch;
static int8u clusterIdsForEzModeMatchLength;
static int16u ezmodeClientCluster;

typedef enum {
  EZMODE_OFF,
  EZMODE_BROAD_PJOIN,
  EZMODE_IDENTIFY,
  EZMODE_MATCH,
  EZMODE_BIND,
  EZMODE_BOUND
} EzModeState;

static EzModeState ezModeState = EZMODE_OFF;

// We assume the first endpoint is the one to use for end-device bind / EZ-Mode
#define ENDPOINT_INDEX 0

//------------------------------------------------------------------------------

void emberAfPluginEzmodeCommissioningStateEventHandler(void) {
  EmberStatus status;
  EmberEUI64 add;
  emberEventControlSetInactive(stateEvent);
  switch (ezModeState) {
    case EZMODE_BROAD_PJOIN:
      emberAfCorePrintln("<ezmode bpjoin>");
      emAfPermitJoin(180, TRUE); //Send out a broadcast pjoin
      ezModeState = EZMODE_IDENTIFY;
      emberEventControlSetDelayMS(stateEvent, MILLISECOND_TICKS_PER_SECOND);
      break;
    case EZMODE_IDENTIFY:
      emberAfCorePrintln("<ezmode identify>");
      emAfPermitJoin(180, TRUE); //Send out a broadcast pjoin
      emberAfFillCommandIdentifyClusterIdentifyQuery();
      emberAfSetCommandEndpoints(ezmodeClientEndpoint, EMBER_BROADCAST_ENDPOINT);
      emberAfSendCommandBroadcast(0xffff);
      break;
    case EZMODE_MATCH:
      emberAfCorePrintln("<ezmode match>");
      emberAfFindClustersByDeviceAndEndpoint(currentIdentifyingAddress,
                                             currentIdentifyingEndpoint,
                                             serviceDiscoveryCallback);
      break;
    case EZMODE_BIND:
      emberAfCorePrintln("<ezmode bind>");
  	  status = emberLookupEui64ByNodeId(currentIdentifyingAddress, add);
      if (status == EMBER_SUCCESS) {
        createBinding(add);
        break;
      }    
      status = emberAfFindIeeeAddress(currentIdentifyingAddress, serviceDiscoveryCallback);
      emberAfCorePrintln("<find ieee status: %d>", status);
      break;
    case EZMODE_BOUND:
      emberAfCorePrintln("<ezmode bound>");
      break;
    default:
      break;
  }
}

/** EZ-MODE CLIENT **/
/**
 * Kicks off the ezmode commissioning process by sending out
 * an identify query command to the given endpoint
 *
 * input:
 *   endpoint:
 *   direction: server to client / client to server
 *   clusterIds: list of cluster ids. *NOTE* The API only keeps the pointer to
 *     to the data structure. The data is expected to exist throughout the
 *     ezmode-commissioning calls.
 *   clusterIdsLength: # of ids defined in clusterIds
 */
EmberStatus emberAfEzmodeClientCommission(int8u endpoint,
                                          EmberAfEzModeCommissioningDirection direction,
                                          int16u* clusterIds,
                                          int8u  clusterIdsLength) {
  // sanity check inputs...
  if (!clusterIds){
    return EMBER_BAD_ARGUMENT;
  }

  bindingDirection = direction;
  clusterIdsForEzModeMatchLength = clusterIdsLength;

  if (clusterIdsLength > 0){
    clusterIdsForEzModeMatch = clusterIds;
  } else {
    clusterIdsForEzModeMatch = NULL;
  }

  ezmodeClientEndpoint = endpoint;
  ezModeState = EZMODE_BROAD_PJOIN;
  emberEventControlSetActive(stateEvent);
  return EMBER_SUCCESS;
}

boolean emberAfIdentifyClusterIdentifyQueryResponseCallback(int16u timeout) {
  // ignore our own broadcast and only take the first identify
  if (emberGetNodeId() == emberAfCurrentCommand()->source ||
      ezModeState != EZMODE_IDENTIFY)
    return FALSE;
  
  currentIdentifyingAddress = emberAfCurrentCommand()->source;
  currentIdentifyingEndpoint = emberAfCurrentCommand()->apsFrame->sourceEndpoint;
  ezModeState = EZMODE_MATCH;
  emberEventControlSetActive(stateEvent);
  
  return TRUE;
}

static void createBinding(int8u *address) {
  // create binding
  int8u i = 0;
  EmberBindingTableEntry candidate;
  EmberStatus status;
    
  // first look for a duplicate binding, we should not add duplicates
  for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++)
  {
     status = emberGetBinding(i, &candidate);

     if ((status == EMBER_SUCCESS)
           && (candidate.type == EMBER_UNICAST_BINDING)
           && (candidate.local == ezmodeClientEndpoint )
           && (candidate.clusterId == ezmodeClientCluster )
           && (candidate.remote == currentIdentifyingEndpoint )
           && (MEMCOMPARE(candidate.identifier, 
                 address, 
                 EUI64_SIZE) == 0))
     {
       ezModeState = EZMODE_BOUND;
       emberEventControlSetActive(stateEvent);
	   return;
     }
   }
	
   for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++) {
     if (emberGetBinding(i, &candidate) == EMBER_SUCCESS
         && candidate.type == EMBER_UNUSED_BINDING) {
       candidate.type = EMBER_UNICAST_BINDING;
       candidate.local = ezmodeClientEndpoint;
       candidate.remote = currentIdentifyingEndpoint;
       candidate.clusterId = ezmodeClientCluster;
       MEMCOPY(candidate.identifier, address, EUI64_SIZE);
       status = emberSetBinding(i, &candidate);
       if (status == EMBER_SUCCESS) {
         ezModeState = EZMODE_BOUND;
         emberEventControlSetActive(stateEvent);
         break;
       }
     }
   }
}

static void serviceDiscoveryCallback(const EmberAfServiceDiscoveryResult *result)
{ 
	int8u i = 0;
	int8u j = 0;
  if (emberAfHaveDiscoveryResponseStatus(result->status)) {
    if (result->zdoRequestClusterId == SIMPLE_DESCRIPTOR_REQUEST) {
      EmberAfClusterList *list = (EmberAfClusterList*)result->responseData;
      int8u clusterCount;
      const int16u* clusterList;

      // decide where to create the binding (server/client)
      if (bindingDirection == EMBER_AF_EZMODE_COMMISSIONING_CLIENT_TO_SERVER){
        clusterCount = list->inClusterCount;
        clusterList = list->inClusterList;
      } else { // EMBER_AF_EZMODE_COMMISSIONING_SERVER_TO_CLIENT
        clusterCount = list->outClusterCount;
        clusterList = list->outClusterList;
      }

      for (i = 0; i < clusterCount; i++) {
        int16u cluster = clusterList[i];
        for (j = 0; j < clusterIdsForEzModeMatchLength; j++) {
          if (cluster == clusterIdsForEzModeMatch[j]) {
            ezModeState = EZMODE_BIND;
            ezmodeClientCluster = cluster;
            emberEventControlSetActive(stateEvent);
            break;
          }
        }
        if (ezModeState == EZMODE_BIND) {
          break;
        }
      }
    } else if (result->zdoRequestClusterId == IEEE_ADDRESS_REQUEST) {
      emberAfCorePrintln("<create binding>");
      createBinding((int8u *)result->responseData);
    }
  }
}


/** EZ-MODE SERVER **/
/**
 * Puts the device into identify mode for the given endpoint
 * this is all that an ezmode server is responsible for
 */

EmberStatus emberAfEzmodeServerCommission(int8u endpoint) {
  return emberAfEzmodeServerCommissionWithTimeout(endpoint, EMBER_AF_PLUGIN_EZMODE_COMMISSIONING_IDENTIFY_TIMEOUT);
}

EmberStatus emberAfEzmodeServerCommissionWithTimeout(int8u endpoint, int16u identifyTimeoutSeconds) {
  EmberAfStatus afStatus;
  if ((identifyTimeoutSeconds < 1) || (identifyTimeoutSeconds > 254)){
    return EMBER_BAD_ARGUMENT;
  }
  afStatus =  emberAfWriteAttribute(endpoint,
                                    ZCL_IDENTIFY_CLUSTER_ID,
                                    ZCL_IDENTIFY_TIME_ATTRIBUTE_ID,
                                    CLUSTER_MASK_SERVER,
                                    (int8u *)&identifyTimeoutSeconds,
                                    ZCL_INT16U_ATTRIBUTE_TYPE);
  if (afStatus != EMBER_ZCL_STATUS_SUCCESS) {
    return EMBER_BAD_ARGUMENT;
  } else {
    return EMBER_SUCCESS;
  }
}
