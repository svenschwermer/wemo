// Copyright 2013 Silicon Laboratories Inc.

#include "app/framework/include/af.h"
#include "app/framework/plugin/end-device-support/end-device-support.h"

// *****************************************************************************
// Globals

static int8u numPollsFailingLimit;
static boolean enablePollCompletedCallback;

// *****************************************************************************
// Functions

// This is called to scheduling polling events for the network(s).  We only
// care about end device networks.  For each of those, the NCP will be told to
// poll for joined networks or not to poll otherwise.
void emberAfSchedulePollEventCallback(void)
{
  int8u i;
  for (i = 0; i < EMBER_SUPPORTED_NETWORKS; i++) {
    if (EMBER_END_DEVICE <= emAfNetworks[i].nodeType) {
      EmAfPollingState *state = &emAfPollingStates[i];
      int32u lastPollIntervalTimeQS = state->pollIntervalTimeQS;
      emberAfPushNetworkIndex(i);
      if (emberNetworkState() == EMBER_JOINED_NETWORK) {
        state->pollIntervalTimeQS = emberAfGetCurrentPollIntervalQsCallback();
      } else {
        state->pollIntervalTimeQS = 0;
      }

      // schedule for poll when following attr changes state:
      // 1) poll interval
      // 2) enablePollCompletedCallback
      if (state->pollIntervalTimeQS != lastPollIntervalTimeQS
          || emAfEnablePollCompletedCallback != enablePollCompletedCallback) {
        EmberStatus status;
        int8u ncpFailureLimit;
        enablePollCompletedCallback = emAfEnablePollCompletedCallback;
        if (emAfEnablePollCompletedCallback) {
          ncpFailureLimit = 0;
          numPollsFailingLimit = EMBER_AF_PLUGIN_END_DEVICE_SUPPORT_MAX_MISSED_POLLS;
        } else {
          ncpFailureLimit = EMBER_AF_PLUGIN_END_DEVICE_SUPPORT_MAX_MISSED_POLLS;
          numPollsFailingLimit = 0;
        }

        status = ezspPollForData(state->pollIntervalTimeQS,
                                 EMBER_EVENT_QS_TIME,
                                 ncpFailureLimit);
        if (status != EMBER_SUCCESS) {
          emberAfCorePrintln("poll nwk %d: 0x%x", i, status);
        }
      }
      emberAfPopNetworkIndex();
    }
  }
}

// The NCP schedules and manages polling, so we do not schedule our own events
// and therefore this handler should never fire.
void emberAfPluginEndDeviceSupportPollingNetworkEventHandler(void)
{
}

// This function is called when a poll completes and explains what happend with
// the poll.  If no ACKs are received from the parent, we will try to find a
// new parent.
void ezspPollCompleteHandler(EmberStatus status)
{
  emAfPollCompleteHandler(status, numPollsFailingLimit);
}

void emberAfPreNcpResetCallback(void)
{
  // Reset the poll intervals so the NCP will be instructed to poll if
  // necessary.
  int8u i;
  for (i = 0; i < EMBER_SUPPORTED_NETWORKS; i++) {
    emAfPollingStates[i].pollIntervalTimeQS = 0;
    emAfPollingStates[i].numPollsFailing = 0;
  }
  numPollsFailingLimit = EMBER_AF_PLUGIN_END_DEVICE_SUPPORT_MAX_MISSED_POLLS;
  enablePollCompletedCallback = FALSE;
}
