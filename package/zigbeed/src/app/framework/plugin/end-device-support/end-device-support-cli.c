// *****************************************************************************
// * end-device-support-cli.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************


#include "app/framework/include/af.h"
#include "app/framework/util/common.h"
#include "app/util/serial/command-interpreter2.h"
#include "end-device-support.h"


// *****************************************************************************
// Forward declarations

void emberAfPluginEndDeviceSupportStatusCommand(void);
void emberAfPluginEndDeviceSupportStayAwakeCommand(void);
void emberAfPluginEndDeviceSupportAwakeWhenNotJoinedCommand(void);
void emberAfPluginEndDeviceSupportPollCompletedCallbackCommand(void);

// *****************************************************************************
// Globals

#ifndef EMBER_AF_GENERATE_CLI
EmberCommandEntry emberAfPluginEndDeviceSupportCommands[] = {
  emberCommandEntryAction("status", emberAfPluginEndDeviceSupportStatusCommand, "", 
                          "Display the status of the End Device's polling and sleeping"),

  emberCommandEntryAction("force-awake", emberAfPluginEndDeviceSupportStayAwakeCommand,  "u", 
                          "Sets whether the CLI forces the device to stay awake"),

  emberCommandEntryAction("awake-when-not-joined", emberAfPluginEndDeviceSupportAwakeWhenNotJoinedCommand, "u",
                          "Sets whether the device stays awake when not joined to a Zigbee network."),

  emberCommandEntryAction("poll-completed-callback", emberAfPluginEndDeviceSupportPollCompletedCallbackCommand, "u", 
                          "Sets whether the device's poll completed callback function is enabled"),

  emberCommandEntryTerminator()
};
#endif // EMBER_AF_GENERATE_CLI

PGM_P sleepControlStrings[] = {
  "EMBER_AF_OK_TO_HIBERNATE",
  "EMBER_AF_OK_TO_NAP",
  "EMBER_AF_STAY_AWAKE",
};


// *****************************************************************************
// Functions

void emberAfPluginEndDeviceSupportStatusCommand(void)
{
  PGM_P names[] = {
    EMBER_AF_GENERATED_NETWORK_STRINGS
  };
  int8u i;

  emberAfCorePrintln("End Device Sleep/Poll Information");
  emberAfCorePrintln("EMBER_END_DEVICE_TIMEOUT:       %d", EMBER_END_DEVICE_POLL_TIMEOUT);
  emberAfCorePrintln("EMBER_END_DEVICE_TIMEOUT_SHIFT: %d", EMBER_END_DEVICE_POLL_TIMEOUT_SHIFT);
#ifndef EMBER_AF_HAS_RX_ON_WHEN_IDLE_NETWORK
  emberAfCoreFlush();
  emberAfCorePrintln("Stay awake when not joined:     %p",
                     (emAfStayAwakeWhenNotJoined
                      ? "yes"
                      : "no"));
  emberAfCoreFlush();
  emberAfCorePrintln("Forced stay awake:              %p",
                     (emAfForceEndDeviceToStayAwake
                      ? "yes"
                      : "no"));
  emberAfCoreFlush();
#endif
  emberAfCorePrintln("Poll completed callback: %p",
                     (emAfEnablePollCompletedCallback
                      ? "yes"
                      : "no"));
  emberAfCoreFlush();

  for (i = 0; i < EMBER_SUPPORTED_NETWORKS; i++) {
    if (EMBER_END_DEVICE <= emAfNetworks[i].nodeType) {
      emberAfPushNetworkIndex(i);
      emberAfCorePrintln("nwk %d [%p]", i, names[i]);
      emberAfCorePrintln("  Current Poll Interval (qs):   %l",
                         emberAfGetCurrentPollIntervalQsCallback());
      emberAfCorePrintln("  Long Poll Interval (qs):      %l",
                         emberAfGetLongPollIntervalQsCallback());
      if (EMBER_SLEEPY_END_DEVICE <= emAfNetworks[i].nodeType) {
        emberAfCorePrintln("  Short Poll Interval (qs):     %l",
                           emberAfGetShortPollIntervalQsCallback());
        emberAfCoreFlush();
        emberAfCorePrintln("  Wake Timeout (qs):            %l",
                           emberAfGetWakeTimeoutQsCallback());
        emberAfCoreFlush();
        emberAfCorePrintln("  Wake Timeout Bitmask:         0x%4x",
                           emberAfGetWakeTimeoutBitmaskCallback());
        emberAfCoreFlush();
        emberAfCorePrintln("  Current App Tasks:            0x%4x",
                           emberAfGetCurrentAppTasksCallback());
        emberAfCorePrintln("  Current Sleep Control         %p",
                           sleepControlStrings[emberAfGetCurrentSleepControlCallback()]);
        emberAfCoreFlush();
        emberAfCorePrintln("  Default Sleep Control         %p",
                           sleepControlStrings[emberAfGetDefaultSleepControlCallback()]);
        emberAfCoreFlush();
        emberAfPopNetworkIndex();
      }
    }
  }
}

void emberAfPluginEndDeviceSupportStayAwakeCommand(void)
{
  boolean stayAwake = (boolean)emberUnsignedCommandArgument(0);
  if (stayAwake) {
    emberAfCorePrintln("Forcing device to stay awake");
  } else {
    emberAfCorePrintln("Allowing device to go to sleep");
  }
  emberAfForceEndDeviceToStayAwake(stayAwake);
}

void emberAfPluginEndDeviceSupportAwakeWhenNotJoinedCommand(void)
{
  emAfStayAwakeWhenNotJoined = (boolean)emberUnsignedCommandArgument(0);
}

void emberAfPluginEndDeviceSupportPollCompletedCallbackCommand(void)
{
  emAfEnablePollCompletedCallback = (boolean)emberUnsignedCommandArgument(0);
}
