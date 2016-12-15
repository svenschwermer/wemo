/**
 * File: hal/micro/generic/button-stub.c
 * Description: stub implementation of button functions.
 *
 * Copyright 2005 by Ember Corporation. All rights reserved.
 */

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "hal/hal.h"

int8u button0State = BUTTON_RELEASED;
int8u button1State = BUTTON_RELEASED;

void halInternalInitButton(void) {}

int8u halButtonState(int8u button) { 
  switch (button) {
    case BUTTON0:
      return button0State;
    case BUTTON1:
      return button1State;
    default:
      break;
  }
  return BUTTON_RELEASED;
}

int8u halButtonPinState(int8u button) { return 0; }

int16u halGpioIsr(int16u interrupt, int16u pcbContext) { return 0; }

int16u halTimerIsr(int16u interrupt, int16u pcbContext) { return 0; }

#if defined(UNIX_HOST) || defined(UNIX_HOST_SIM)
// I have no idea why this is necessary
void halButtonIsr(int8u button, int8u state) {}
#endif

void simulatedButtonIsr(int8u button, boolean isPress) {
  switch (button) {
    case BUTTON0: 
      {
        int8u bs = (isPress ? BUTTON_PRESSED : BUTTON_RELEASED);
        if (bs != button0State) {
          button0State = bs;
          halButtonIsr(button, button0State);
        }
      }
      break;
    case BUTTON1: 
      {
        int8u bs = (isPress ? BUTTON_PRESSED : BUTTON_RELEASED);
        if (bs != button1State) {
          button1State = bs;
          halButtonIsr(button, button1State);
        }
      }
      break;
    default:
      break;
  }
}
