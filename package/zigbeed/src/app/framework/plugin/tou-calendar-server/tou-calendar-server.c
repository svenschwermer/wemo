// ****************************************************************************
// * tou-calendar-server.c
// *
// *
// * Copyright 2013 by Silicon Labs. All rights reserved.                  *80*
// ****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/plugin/tou-calendar-common/tou-calendar-common.h"

//-----------------------------------------------------------------------------
// Globals 

// Each schedule entry is 3 bytes (2 bytes for start time, and 1 byte for data)
#define SCHEDULE_ENTRY_SIZE 3

// Season start date (4-bytes) and week ID ref (1-byte)
#define SEASON_ENTRY_SIZE 5  

// Special day date (4 bytes) and Day ID ref (1-byte)
#define SPECIAL_DAY_ENTRY_SIZE 5

#define MAX_CALENDAR_NAME_SIZE 13   // per the spec

//-----------------------------------------------------------------------------

void emberAfTouCalendarClusterServerInitCallback(int8u endpoint)
{
}

static EmberAfTouCalendarStruct* getCalendarHandleIfNotFound(int32u providerId,
                                                             int32u issuerCalendarId)
{
  EmberAfTouCalendarStruct* calendar = emberAfPluginTouCalendarCommonGetCalendarById(providerId,
                                                                                     issuerCalendarId);
  if (calendar == NULL) {
    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_NOT_FOUND);
  }
  return calendar;
}

boolean emberAfTouCalendarClusterSswgGetCalendarCallback(int32u startTime,
                                                         int32u minimumIssuerEventId,
                                                         int8u numberOfCalendars,
                                                         int8u calendarType,
                                                         int32u providerId)
{
  boolean responseGenerated = FALSE;
  int8u i;
  int8u nameData[MAX_CALENDAR_NAME_SIZE + 1];
  for (i = 0; 
       i < EMBER_AF_PLUGIN_TOU_CALENDAR_COMMON_TOTAL_CALENDARS
       && (numberOfCalendars == 0 || i < numberOfCalendars);
       i++) {
    EmberAfTouCalendarStruct* calendar = emberAfPluginTouCalendarCommonGetLocalCalendar(i);
    if (calendar != NULL
        && startTime <= calendar->startTimeUtc
        && (minimumIssuerEventId == 0xFFFFFFFF
            || minimumIssuerEventId <= calendar->issuerEventId)
        && (providerId == 0xFFFFFFFF
            || providerId == calendar->providerId)) {

      responseGenerated = TRUE;
      MEMSET(nameData, 0, MAX_CALENDAR_NAME_SIZE + 1);
      MEMCOPY(nameData, calendar->name, MAX_CALENDAR_NAME_SIZE);
      emberAfFillCommandTouCalendarClusterSswgPublishCalendar(calendar->providerId,
                                                              calendar->issuerEventId,
                                                              calendar->calendarId,
                                                              calendar->startTimeUtc,
                                                              calendar->calendarType,
                                                              calendar->name,
                                                              calendar->numberOfSeasons,
                                                              calendar->numberOfWeekProfiles,
                                                              calendar->numberOfDayProfiles);
      emberAfSendResponse();
    }
  }

  if (!responseGenerated) {
    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_NOT_FOUND);
  }
  return TRUE;
}

boolean emberAfTouCalendarClusterSswgGetDayProfilesCallback(int32u providerId,
                                                            int32u issuerCalendarId,
                                                            int8u dayId,
                                                            int8u numberOfDays)
{
  int8u scheduleEntriesData[SCHEDULE_ENTRY_SIZE
                            * EMBER_AF_PLUGIN_TOU_CALENDAR_COMMON_SCHEDULE_ENTRIES_MAX];

  int8u i;
  EmberAfTouCalendarStruct* calendar = getCalendarHandleIfNotFound(providerId,
                                                                   issuerCalendarId);
  if (calendar == NULL) {
    return TRUE;
  }

  for (i = 0; i < calendar->numberOfDayProfiles; i++) {
    int8u j;
    for (j = 0; j < calendar->normalDays[i].numberOfScheduleEntries; j++) {
      emberAfCopyInt16u(scheduleEntriesData,
                        j * SCHEDULE_ENTRY_SIZE,
                        calendar->normalDays[i].scheduleEntries[j].minutesFromMidnight);
      scheduleEntriesData[j * SCHEDULE_ENTRY_SIZE + 2] = calendar->normalDays[i].scheduleEntries[j].data;
    }
    emberAfFillCommandTouCalendarClusterSswgPublishDayProfile(calendar->providerId,
                                                              calendar->issuerEventId,
                                                              calendar->calendarId,
                                                              calendar->normalDays[i].id,
                                                              calendar->normalDays[i].numberOfScheduleEntries,
                                                              // 0 because the spec is unclear how to segment fragments
                                                              0,  // command-index
                                                              1,  // total commands
                                                              scheduleEntriesData,
                                                              SCHEDULE_ENTRY_SIZE * calendar->normalDays[i].numberOfScheduleEntries);
    emberAfSendResponse();
  }

  return TRUE;
}

boolean emberAfTouCalendarClusterSswgGetWeekProfilesCallback(int32u providerId,
                                                             int32u issuerCalendarId,
                                                             int8u weekId,
                                                             int8u numberOfWeeks)
{
  int8u i;
  EmberAfTouCalendarStruct* calendar = getCalendarHandleIfNotFound(providerId,
                                                                   issuerCalendarId);
  if (calendar == NULL) {
    return TRUE;
  }

  for (i = 0; i < EMBER_AF_PLUGIN_TOU_CALENDAR_COMMON_WEEK_PROFILE_MAX; i++) {
    if (calendar->weeks[i].id == weekId) {
      emberAfFillCommandTouCalendarClusterSswgPublishWeekProfile(calendar->providerId,
                                                                 calendar->issuerEventId,
                                                                 calendar->calendarId,
                                                                 weekId,
                                                                 calendar->weeks[i].daysArray[0]->id,
                                                                 calendar->weeks[i].daysArray[1]->id,
                                                                 calendar->weeks[i].daysArray[2]->id,
                                                                 calendar->weeks[i].daysArray[3]->id,
                                                                 calendar->weeks[i].daysArray[4]->id,
                                                                 calendar->weeks[i].daysArray[5]->id,
                                                                 calendar->weeks[i].daysArray[6]->id);
      emberAfSendResponse();
      return TRUE;
    }
  }

  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_NOT_FOUND);

  return TRUE;
}

boolean emberAfTouCalendarClusterSswgGetSeasonsCallback(int32u providerId,
                                                        int32u issuerCalendarId)
{
  int8u i;
  int8u seasonData[SEASON_ENTRY_SIZE * EMBER_AF_PLUGIN_TOU_CALENDAR_COMMON_SEASON_PROFILE_MAX];
  EmberAfTouCalendarStruct* calendar = getCalendarHandleIfNotFound(providerId,
                                                                   issuerCalendarId);
  if (calendar == NULL) {
    return TRUE;
  }

  for (i = 0; i < calendar->numberOfSeasons; i++) {
    emberAfCopyInt32u(seasonData,
                      i * SEASON_ENTRY_SIZE,
                      calendar->seasons[i].startDate);
  }
  // For now we don't support segmenting commands since it isn't clear in the 
  // spec how this is done.  APS Fragmentation would be better since it is
  // already used by other clusters.
  emberAfFillCommandTouCalendarClusterSswgPublishSeasons(calendar->providerId,
                                                         calendar->issuerEventId,
                                                         calendar->calendarId,
                                                         0, 
                                                         1,
                                                         seasonData,
                                                         (calendar->numberOfSeasons
                                                          * SEASON_ENTRY_SIZE));

  return FALSE;
}

boolean emberAfTouCalendarClusterSswgGetSpecialDaysCallback(int32u startTime,
                                                            int8u numberOfEvents,
                                                            int8u calendarType,
                                                            int32u providerId,
                                                            int32u issuerCalendarId)
{
  int8u i;
  EmberAfTouCalendarStruct* calendar = getCalendarHandleIfNotFound(providerId,
                                                                   issuerCalendarId);
  if (calendar == NULL) {
    return TRUE;
  }
  if ((calendarType & calendar->calendarType) == 0) {
    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_NOT_FOUND);
    return TRUE;
  }

  {
    // This is a big array since the default special days is 50, per the spec.
    // We surround with braces to prevent the error conditions above from requiring 
    // the stack space necessay to answer a request that returns real data.
    int8u specialDayData[EMBER_AF_PLUGIN_TOU_CALENDAR_COMMON_SPECIAL_DAY_PROFILE_MAX
                         * SPECIAL_DAY_ENTRY_SIZE];

    for (i = 0; i < calendar->numberOfSpecialDayProfiles; i++) {
      emberAfCopyInt32u(specialDayData,
                        i * SPECIAL_DAY_ENTRY_SIZE,
                        calendar->specialDays[i].startDate),
      specialDayData[i * SPECIAL_DAY_ENTRY_SIZE + 4] = calendar->specialDays[i].day->id;
    }
    // We only support sending a single command in response.  We assume fragmentation
    // will be used since it is not clear to use the other method (sub-commands) as the
    // Smart Energy spec is unclear.
    emberAfFillCommandTouCalendarClusterSswgPublishSpecialDays(calendar->providerId,
                                                               calendar->issuerEventId,
                                                               calendar->startTimeUtc,
                                                               calendar->calendarType,
                                                               calendar->numberOfSpecialDayProfiles,
                                                               0,  // command index
                                                               1,  // total commands
                                                               specialDayData,
                                                               SPECIAL_DAY_ENTRY_SIZE
                                                               * calendar->numberOfSpecialDayProfiles);
    emberAfSendResponse();
  }


  return TRUE;
}


