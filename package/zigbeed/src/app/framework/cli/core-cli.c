// *****************************************************************************
// * core-cli.c
// *
// * Core CLI commands used by all applications regardless of profile.
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

// common include file
#include "app/framework/util/common.h"
#include "app/framework/util/af-main.h"

#include "app/util/serial/command-interpreter2.h"
#include "stack/include/library.h"
#include "app/framework/security/af-security.h"

#if !defined(EZSP_HOST)
  #include "stack/include/cbke-crypto-engine.h"  // emberGetCertificate()
#endif

#include "app/framework/cli/core-cli.h"
#include "app/framework/cli/custom-cli.h"
#include "app/framework/cli/network-cli.h"
#include "app/framework/cli/plugin-cli.h"
#include "app/framework/cli/security-cli.h"
#include "app/framework/cli/zdo-cli.h"
#include "app/framework/cli/option-cli.h"
#include "app/framework/plugin/test-harness/test-harness-cli.h"
#include "app/framework/util/af-event.h"
#include "app/util/counters/counters-cli.h"

#include "app/framework/plugin/gateway/gateway-support-cli.h"

#include "app/util/common/library.h"

#if !defined(XAP2B)
  #define PRINT_FULL_COUNTER_NAMES
#endif

//------------------------------------------------------------------------------

void emberCommandActionHandler(const CommandAction action)
{
  emberAfPushNetworkIndex(emAfCliNetworkIndex);
  (*action)();
  emberAfPopNetworkIndex();

#if defined(EMBER_QA)
  emberSerialPrintfLine(APP_SERIAL, "CLI Finished");
#endif
}

extern EmberCommandEntry cbkeCommands[];

#if !defined(ZA_CLI_MINIMAL) && !defined(ZA_CLI_FULL) && !defined(EMBER_AF_GENERATE_CLI)
  // Define this to satisfy external references.
  EmberCommandEntry emberCommandTable[] = { { NULL } };
#endif

#if defined(ZA_CLI_MINIMAL) || defined(ZA_CLI_FULL)

#if defined(PRINT_FULL_COUNTER_NAMES)
static PGM_NO_CONST PGM_P counterStrings[] = {
  EMBER_COUNTER_STRINGS
};
#endif

void printMfgString(void)
{
  int8u mfgString[MFG_STRING_MAX_LENGTH + 1];
  emberAfFormatMfgString(mfgString);
  
  // Note:  We use '%s' here because this is a RAM string.  Normally
  // most strings are literals or constants in flash and use '%p'.
  emberAfAppPrintln("MFG String: %s", mfgString);
}

static void printPacketBuffers(void)
{
  emberAfAppPrintln("Buffs: %d / %d",
                    emAfGetPacketBufferFreeCount(),
                    emAfGetPacketBufferTotalCount());
}

boolean printSmartEnergySecurityInfo(void)
{
#ifdef EMBER_AF_HAS_SECURITY_PROFILE_SE
  boolean securityGood = TRUE;
  emberAfAppPrint("SE Security Info [");
  {
    // for SE security, print the state of ECC, CBKE, and the programmed Cert
    EmberCertificateData cert;
    EmberStatus status = emberGetCertificate(&cert);

    // check the status of the ECC library
    if (emberGetLibraryStatus(EMBER_ECC_LIBRARY_ID)
        & EMBER_LIBRARY_PRESENT_MASK) {
      emberAfAppPrint("RealEcc ");
    } else {
      emberAfAppPrint("NoEcc ");
      securityGood = FALSE;
    }

    // status of EMBER_LIBRARY_NOT_PRESENT means the CBKE is not present
    // in the image.  We don't know anything about the certificate.
    if (status == EMBER_LIBRARY_NOT_PRESENT) {
      emberAfAppPrint("NoCbke UnknownCert");
      securityGood = FALSE;
    } else {
      emberAfAppPrint("RealCbke ");

      // status of EMBER_SUCCESS means the cert is ok
      if (status == EMBER_SUCCESS) {
        emberAfAppPrint("GoodCert");
      }
      // status of EMBER_ERR_FATAL means the cert failed
      else if (status == EMBER_ERR_FATAL) {
        emberAfAppPrint("BadCert");
        securityGood = FALSE;
      }
    }
    emberAfAppPrintln("]");
  }
  emberAfAppFlush();
  return securityGood;
#else
  return FALSE;
#endif
}

void emAfCliCountersCommand(void)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
  int8u i;
  int16u counters[EMBER_COUNTER_TYPE_COUNT];
  
  emAfCopyCounterValues(counters);
  for (i=0; i < EMBER_COUNTER_TYPE_COUNT; i++) {
#if defined(PRINT_FULL_COUNTER_NAMES)
    emberAfAppPrintln("%d: %p = %d",
                      i,
                      counterStrings[i],
                      counters[i]);
#else
    emberAfAppPrintln("%d: %d",
                      i,
                      counters[i]);
#endif
    emberAfAppFlush();
  }

  emberAfAppPrintln("");
  printPacketBuffers();
#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
}

// info
void emAfCliInfoCommand(void)
{
  EmberNodeType nodeTypeResult = 0xFF;
  int8u commandLength;
  EmberEUI64 myEui64;
  EmberNetworkParameters networkParams;
  emberStringCommandArgument(-1, &commandLength);
  printMfgString();
  emberAfGetEui64(myEui64);
  emberAfGetNetworkParameters(&nodeTypeResult, &networkParams);
  emberAfAppPrint("node [");
  emberAfAppDebugExec(emberAfPrintBigEndianEui64(myEui64));
  emberAfAppFlush();
  emberAfAppPrintln("] chan [%d] pwr [%d]",
                    networkParams.radioChannel,
                    networkParams.radioTxPower);
  emberAfAppPrint("panID [0x%2x] nodeID [0x%2x] ",
                 networkParams.panId,
                 emberAfGetNodeId());
  emberAfAppFlush();
  emberAfAppPrint("xpan [0x");
  emberAfAppDebugExec(emberAfPrintBigEndianEui64(networkParams.extendedPanId));
  emberAfAppPrintln("]");
  emberAfAppFlush();

  emAfCliVersionCommand();
  emberAfAppFlush();

  emberAfAppPrint("nodeType [");
  if (nodeTypeResult != 0xFF) {
    emberAfAppPrint("0x%x", nodeTypeResult);
  } else {
    emberAfAppPrint("unknown");
  }
  emberAfAppPrintln("]");
  emberAfAppFlush();

  emberAfAppPrint("%p level [%x]", "Security", emberAfGetSecurityLevel());

  printSmartEnergySecurityInfo();

  emberAfAppPrint("network state [%x] ", emberNetworkState());
  printPacketBuffers();
  emberAfAppFlush();

  // print the endpoint information
  {
    int8u i, j;
    emberAfAppPrintln("Ep cnt: %d", emberAfEndpointCount());
    // loop for each endpoint
    for (i = 0; i < emberAfEndpointCount(); i++) {
      EmberAfEndpointType *et = emAfEndpoints[i].endpointType;
      emberAfAppPrint("ep %d [endpoint %p, device %p] ",
                      emberAfEndpointFromIndex(i),
                      (emberAfEndpointIndexIsEnabled(i)
                       ? "enabled"
                       : "disabled"),
                      emberAfIsDeviceEnabled(emberAfEndpointFromIndex(i))
                      ? "enabled" 
                      : "disabled");
      emberAfAppPrintln("nwk [%d] profile [0x%2x] devId [0x%2x] ver [0x%x]",
                        emberAfNetworkIndexFromEndpointIndex(i),
                        emberAfProfileIdFromIndex(i),
                        emberAfDeviceIdFromIndex(i),
                        emberAfDeviceVersionFromIndex(i));    
      // loop for the clusters within the endpoint
      for (j = 0; j < et->clusterCount; j++) {
        EmberAfCluster *zc = &(et->cluster[j]);
        emberAfAppPrint("    %p cluster: 0x%2x ", 
                       (emberAfClusterIsClient(zc)
                        ? "out(client)"
                        : "in (server)" ),
                       zc->clusterId);
        emberAfAppDebugExec(emberAfDecodeAndPrintCluster(zc->clusterId));
        emberAfAppPrintln("");
        emberAfAppFlush();
      }
      emberAfAppFlush();
    }
  }

  {
    PGM_P names[] = {
      EMBER_AF_GENERATED_NETWORK_STRINGS
    };
    int8u i;
    emberAfAppPrintln("Nwk cnt: %d", EMBER_SUPPORTED_NETWORKS);
    for (i = 0; i < EMBER_SUPPORTED_NETWORKS; i++) {
      emberAfAppPrintln("nwk %d [%p]", i, names[i]);
      emberAfAppPrintln("  nodeType [0x%x]", emAfNetworks[i].nodeType);
      emberAfAppPrintln("  securityProfile [0x%x]", emAfNetworks[i].securityProfile);
    }
  }
}
#endif

#if defined(ZA_CLI_MINIMAL) || defined(ZA_CLI_FULL)

//------------------------------------------------------------------------------
// "debugprint" commands.

#ifdef ZA_CLI_FULL

void printOnCommand(void)
{
  int16u area = (int16u)emberUnsignedCommandArgument(0);
  emberAfPrintOn(area);
}

void printOffCommand(void)
{
  int16u area = (int16u)emberUnsignedCommandArgument(0);
  emberAfPrintOff(area);
}

#ifndef EMBER_AF_GENERATE_CLI

static PGM_P debugPrintOnOffCommandArguments[] = {
  "Number of the specified print area.",
  NULL,
};

static EmberCommandEntry debugPrintCommands[] = {
  emberCommandEntryAction("status", 
                          emberAfPrintStatus,
                          "",
                          "Print the status of all the debug print areas."),
  emberCommandEntryAction("all_on",
                          emberAfPrintAllOn, 
                          "",
                          "Turn all debug print areas on."),

  emberCommandEntryAction("all_off",
                          emberAfPrintAllOff, 
                          "",
                          "Turn all debug print areas off."),

  emberCommandEntryActionWithDetails("on",
                                     printOnCommand, 
                                     "v",
                                     "Turn on the printing for the specified area.",
                                     debugPrintOnOffCommandArguments),

  emberCommandEntryActionWithDetails("off",
                                     printOffCommand, 
                                     "v",
                                     "Turn off the printing for the specified area.",
                                     debugPrintOnOffCommandArguments),
  emberCommandEntryTerminator(),
};
#endif // EMBER_AF_GENERATE_CLI

#endif

//------------------------------------------------------------------------------
// Miscellaneous commands.

void helpCommand(void)
{

#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
  EmberCommandEntry *commandFinger = emberCommandTable;
  for (; commandFinger->name != NULL; commandFinger++) {
    emberAfAppPrintln("%p", commandFinger->name);
    emberAfAppFlush();
  }
#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
}

void resetCommand(void)
{
  halReboot();
}

void echoCommand(void)
{
  int8u echoOn = (int8u)emberUnsignedCommandArgument(0);
  if ( echoOn ) {
    emberCommandInterpreterEchoOn();
  } else {
    emberCommandInterpreterEchoOff();
  }
}

void printEvents(void)
{
  int8u i = 0;
  int32u nowMS32 = halCommonGetInt32uMillisecondTick();
  while (emAfEvents[i].control != NULL) {
    emberAfCorePrint("%p  : ", emAfEventStrings[i]);
    if (emAfEvents[i].control->status == EMBER_EVENT_INACTIVE) {
      emberAfCorePrintln("inactive");
    } else {
      emberAfCorePrintln("%l ms", emAfEvents[i].control->timeToExecute - nowMS32);
    }
    i++;
  }
}

//------------------------------------------------------------------------------
// "endpoint" commands

void endpointPrint(void)
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  int8u i;
  for (i = 0; i < emberAfEndpointCount(); i++) {
    emberAfCorePrint("EP %d: %p ", 
                     emAfEndpoints[i].endpoint,
                     (emberAfEndpointIndexIsEnabled(i)
                      ? "Enabled"
                      : "Disabled"));
    emAfPrintEzspEndpointFlags(emAfEndpoints[i].endpoint);
    emberAfCorePrintln("");
  }
}

void enableDisableEndpoint(void)
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  boolean enable = (emberCurrentCommand->name[0] == 'e'
                    ? TRUE
                    : FALSE);
  if (!emberAfEndpointEnableDisable(endpoint, 
                                    enable)) {
    emberAfCorePrintln("Error:  Unknown endpoint.");
  }
}

#ifndef EMBER_AF_GENERATE_CLI
static EmberCommandEntry endpointCommands[] = {
  emberCommandEntryAction("print",  endpointPrint, "",
                          "Print the status of all the endpoints."),
  emberCommandEntryAction("enable", enableDisableEndpoint, "u",
                          "Enables the endpoint for processing ZCL messages."),
  emberCommandEntryAction("disable", enableDisableEndpoint, "u",
                          "Disable the endpoint from processing ZCL messages."),
  
  emberCommandEntryTerminator(),
};

//------------------------------------------------------------------------------
// Commands

static PGM_P readCommandArguments[] = {
  "Endpoint",
  "Cluster ID",
  "Attribute ID",
  "Server Attribute (boolean)",
  NULL,
};

static PGM_P timeSyncCommandArguments[] = {
  "Node ID",
  "source endpoint",
  "dest endpoint",
  NULL,
};

static PGM_P writeCommandArguments[] = {
  "Endpoint",
  "Cluster ID",
  "Attribute ID",
  "Server Attribute (boolean)",
  "Data Bytes",
  NULL,
};

static PGM_P bindSendCommandArguments[] = {
  "Source Endpoint",
  NULL,
};

static PGM_P rawCommandArguments[] = {
  "Cluster ID",
  "Data Bytes",
  NULL,
};

static PGM_P sendCommandArguments[] = {
  "Node ID",
  "Source Endpoint",
  "Dest Endpoint",
  NULL,
};

static PGM_P sendMulticastCommandArguments[] = {
  "Broadcast address",
  "Group Address",
  "Source Endpoint",
  NULL,
};

EmberCommandEntry emberCommandTable[] = {

#ifdef ZA_CLI_FULL

    #if (defined(ZCL_USING_KEY_ESTABLISHMENT_CLUSTER_CLIENT) \
         && defined(ZCL_USING_KEY_ESTABLISHMENT_CLUSTER_SERVER))
      emberCommandEntrySubMenu("cbke",
                               emberAfPluginKeyEstablishmentCommands, 
                               "Commands to initiate CBKE"),
    #endif

    #ifdef EMBER_AF_PRINT_ENABLE
      emberCommandEntrySubMenu("print",      printCommands, ""),
      emberCommandEntrySubMenu("debugprint", debugPrintCommands, ""),
    #endif

    #if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
      emberCommandEntryAction("version",
                              emAfCliVersionCommand, 
                              "",
                              "Print the version number of the ZNet software."),

      LIBRARY_COMMANDS       // Defined in app/util/common/library.h
    #endif

    emberCommandEntrySubMenu("cha",      
                             changeKeyCommands,
                             "Commands to change the default NWK or Link Key"),
    emberCommandEntrySubMenu("interpan",
                             interpanCommands,
                             "Commands to send interpan ZCL messages."),
    emberCommandEntrySubMenu("option",
                             emAfOptionCommands,
                             "Commands to change the application options"),

    emberCommandEntryActionWithDetails("read",
                                       emAfCliReadCommand, 
                                       "uvvu",
                                       "Construct a read attributes command to be sent.",
                                       readCommandArguments),

    emberCommandEntryActionWithDetails("time",
                                       emAfCliTimesyncCommand,           
                                       "vuu",
                                       "Send a read attribute for the current time",
                                       timeSyncCommandArguments),

    emberCommandEntryActionWithDetails("write",            
                                       emAfCliWriteCommand,              
                                       "uvvuub",
                                       "Construct a write attributes command to send.",
                                       writeCommandArguments),

    emberCommandEntrySubMenu("zcl",
                             zclCommands,
                             "Commands for constructing ZCL cluster commands."),

#endif // ZA_CLI_FULL

  emberCommandEntryActionWithDetails("bsend",
                                     emAfCliBsendCommand,              
                                     "u",
                                     "Send using a binding.",
                                     bindSendCommandArguments),

  emberCommandEntrySubMenu("keys",
                           keysCommands,
                           "Commands to print or set the security keys"),

  emberCommandEntrySubMenu("network",
                           networkCommands,
                           "Commands to form or join a network."),

  emberCommandEntryActionWithDetails("raw",
                                     emAfCliRawCommand,                
                                     "vb",
                                     "Create a manually formatted message.",
                                     rawCommandArguments),

  emberCommandEntryActionWithDetails("send",
                                     emAfCliSendCommand,               
                                     "vuu",
                                     "Send the previously constructed command via unicast.",
                                     sendCommandArguments),

  emberCommandEntryActionWithDetails("send_multicast",
                                     emAfCliSendCommand,               
                                     "vu",
                                     "Send the previously constructed command via multicast.",
                                     sendMulticastCommandArguments),

  emberCommandEntrySubMenu("security", emAfSecurityCommands, 
                           "Commands for setting/getting security parameters."),
  emberCommandEntryAction("counters", emAfCliCountersCommand, "",
                          "Print the stack counters"),
  emberCommandEntryAction("help",     helpCommand, "",
                          "Print the list of commands."),
  emberCommandEntryAction("reset", resetCommand, "",
                          "Perform a software reset of the device."),
  emberCommandEntryAction("echo",  echoCommand, "u",
                          "Turn on/off command interpreter echoing."),
  emberCommandEntryAction("events",  printEvents, "",
                          "Print the list of timer events."),
  emberCommandEntrySubMenu("endpoint", endpointCommands,
                           "Commands to manipulate the endpoints."),
  
#ifndef EMBER_AF_CLI_DISABLE_INFO
  emberCommandEntryAction("info", emAfCliInfoCommand, "", \
                          "Print infomation about the network state, clusters, and endpoints"),
#endif
  
  EMBER_AF_PLUGIN_COMMANDS
  ZDO_COMMANDS
  CUSTOM_COMMANDS
  TEST_HARNESS_CLI_COMMANDS
  #ifndef EZSP_HOST
    #ifdef EMBER_APPLICATION_HAS_COUNTER_ROLLOVER_HANDLER
      COUNTER_COMMANDS
    #endif
  #endif

  EMBER_AF_PLUGIN_GATEWAY_COMMANDS

  emberCommandEntryTerminator(),
};

#endif // EMBER_AF_GENERATE_CLI

#else 
// Stubs
void enableDisableEndpoint(void)
{
}

void endpointPrint(void)
{
}

void printOffCommand(void)
{
}

void printOnCommand(void)
{
}

void echoCommand(void)
{
}

void emAfCliCountersCommand(void)
{
}

void emAfCliInfoCommand(void)
{
}

void helpCommand(void)
{
}

void resetCommand(void)
{
}
#endif // defined(ZA_CLI_MINIMAL) || defined(ZA_CLI_FULL)
