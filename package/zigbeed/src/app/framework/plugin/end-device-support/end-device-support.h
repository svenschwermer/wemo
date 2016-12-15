// Copyright 2013 Silicon Laboratories Inc.

#ifdef EMBER_AF_STAY_AWAKE_WHEN_NOT_JOINED
  #error "EMBER_AF_STAY_AWAKE_WHEN_NOT_JOINED is deprecated.  Please use option in the 'End Device Support'  plugin."
#endif

extern boolean emAfStayAwakeWhenNotJoined;
extern boolean emAfForceEndDeviceToStayAwake;
extern boolean emAfEnablePollCompletedCallback;

void emberAfForceEndDeviceToStayAwake(boolean stayAwake);
void emAfPrintSleepDuration(int32u sleepDurationMS, int8u eventIndex);
void emAfPrintForceAwakeStatus(void);
#define emAfOkToSleep() (emberAfGetCurrentSleepControl() < EMBER_AF_STAY_AWAKE)

typedef struct {
  int32u pollIntervalTimeQS;
  int8u numPollsFailing;
} EmAfPollingState;
extern EmAfPollingState emAfPollingStates[];
void emAfPollCompleteHandler(EmberStatus status, int8u limit);
