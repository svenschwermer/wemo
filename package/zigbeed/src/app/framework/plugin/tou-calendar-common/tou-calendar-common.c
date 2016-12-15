// ****************************************************************************
// * tou-calendar-common.c
// *
// *
// * Copyright 2013 by Silicon Labs. All rights reserved.                  *80*
// ****************************************************************************

#include "app/framework/include/af.h"
#include "tou-calendar-common.h"

//-----------------------------------------------------------------------------
// Globals

static EmberAfTouCalendarStruct calendars[EMBER_AF_PLUGIN_TOU_CALENDAR_COMMON_TOTAL_CALENDARS];

//-----------------------------------------------------------------------------

void emberAfPluginTouCalendarCommonInitCallback(int8u endpoint)
{
  int8u i;
  for (i = 0; i < EMBER_AF_PLUGIN_TOU_CALENDAR_COMMON_TOTAL_CALENDARS; i++) {
    MEMSET(&(calendars[i]), 0, sizeof(EmberAfTouCalendarStruct));
    calendars[i].calendarId = EMBER_AF_PLUGIN_TOU_CALENDAR_COMMON_INVALID_CALENDAR_ID;
  }
}

EmberAfTouCalendarStruct* emberAfPluginTouCalendarCommonGetLocalCalendar(int8u index)
{
  if (index < EMBER_AF_PLUGIN_TOU_CALENDAR_COMMON_TOTAL_CALENDARS) {
    return &(calendars[index]);
  }
  return NULL;
}

EmberAfTouCalendarStruct* emberAfPluginTouCalendarCommonGetCalendarById(int32u calendarId,
                                                                        int32u providerId)
{
  int8u i;
  for (i = 0; i < EMBER_AF_PLUGIN_TOU_CALENDAR_COMMON_TOTAL_CALENDARS; i++) {
    if (calendars[i].providerId == providerId
        && calendars[i].calendarId == calendarId) {
      return &(calendars[i]);
    }
  }
  return NULL;
}
