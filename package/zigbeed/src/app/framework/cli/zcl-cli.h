// *******************************************************************
// * zcl-cli.h
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#if !defined(EMBER_AF_GENERATE_CLI)
void emAfCliSendCommand(void);
void emAfCliBsendCommand(void);
void emAfCliReadCommand(void);
void emAfCliWriteCommand(void);
void emAfCliTimesyncCommand(void);
void emAfCliRawCommand(void);
void emAfCliAddReportEntryCommand(void);
#endif

void zclSimpleCommand(int8u frameControl,
                      int16u clusterId, 
                      int8u commandId);

extern EmberCommandEntry keysCommands[];
extern EmberCommandEntry interpanCommands[];
extern EmberCommandEntry printCommands[];
extern EmberCommandEntry zclCommands[];
extern EmberCommandEntry certificationCommands[];

#define zclSimpleClientCommand(clusterId, commandId) \
  zclSimpleCommand(ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER, \
                   (clusterId), \
                   (commandId))

#define zclSimpleServerCommand(clusterId, commandId)             \
  zclSimpleCommand(ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_SERVER_TO_CLIENT, \
                   (clusterId), \
                   (commandId))

void zclBufferAddByte(int8u byte);
void zclBufferAddWord(int16u word);
void zclBufferAddInt32(int32u value);
void zbdCmdBuilt(void);
void zclBufferSetup(int8u frameType, int16u clusterId, int8u commandId);
EmberStatus zbdZigBeeSendCommand(int16u destination, int8u srcEndpoint, int8u dstEndpoint, int16u flagMulticast);

