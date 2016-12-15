/***************************************************************************
 *
 *
 *
 * Created by Belkin International, Software Engineering on Aug 13, 2013
 * Copyright (c) 2012-2013 Belkin International, Inc. and/or its affiliates. All rights reserved.
 *
 * Belkin International, Inc. retains all right, title and interest (including all
 * intellectual property rights) in and to this computer program, which is
 * protected by applicable intellectual property laws.  Unless you have obtained
 * a separate written license from Belkin International, Inc., you are not authorized
 * to utilize all or a part of this computer program for any purpose (including
 * reproduction, distribution, modification, and compilation into object code)
 * and you must immediately destroy or return to Belkin International, Inc
 * all copies of this computer program.  If you are licensed by Belkin International, Inc., your
 * rights to utilize this computer program are limited by the terms of that license.
 *
 * To obtain a license, please contact Belkin International, Inc.
 *
 * This computer program contains trade secrets owned by Belkin International, Inc.
 * and, unless unauthorized by Belkin International, Inc. in writing, you agree to
 * maintain the confidentiality of this computer program and related information
 * and to not disclose this computer program and related information to any
 * other person or entity.
 *
 * THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND BELKIN INTERNATIONAL, INC.
 * EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING THE WARRANTIES OF
 * MERCHANTIBILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND NON-INFRINGEMENT.
 *
 *
 ***************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/stat.h>

#include "ulog.h"
#include "subdevice.h"
//-----------------------------------------------------------------------------

static char sUsage[]=
                "\n"
                "Usage: \n"
                "       %s  <command> <parameters> \n"
                "command: \n"
                "       list                   : list paired devices and groups \n"
                "       device 0               : show details of device[0] \n"
                "       open 25 pair           : open sub device network joining for <25> seconds and <pair> if joined \n"
                "       group G1 ABCD 0 1      : create/set a group with ID <G1>, name <ABCD> and include device[0], device[1] \n"
                "       set_device 0 1 255:30  : set value of device[0] capability[1] as (255:30) \n"
                "       set_group  0 1 255:30  : set value of  group[0] capability[1] as (255:30) \n"
                "       clear                  : unpair all joined devices and clear stored list - reboot required - \n"
                "\n";

static char* sPostMessage = "";

//-----------------------------------------------------------------------------
void TestCapabilityList()
{
    SD_CapabilityList   list;
    int                 count, i;

    count = SD_GetCapabilityList(&list);

    printf("\nCapabilityList:%d\n\n", count);

    for (i = 0; i < count; i++)
    {
        printf("%s %s\n", list[i]->CapabilityID.Data, list[i]->ProfileName);
    }

    puts("");
}
//-----------------------------------------------------------------------------
void TestDeviceList()
{
    SD_DeviceList       device_list;
    SD_Device           device;
    SD_DeviceID         device_id;
    SD_Capability       capability;
    SD_CapabilityID     capability_id;
    int                 count, d, c;
    char                value[CAPABILITY_MAX_VALUE_LENGTH];

    count = SD_GetDeviceList(&device_list);

    printf("\nDeviceList:%d\n", count);

    for (d = 0; d < count; d++)
    {
        device      = device_list[d];
        device_id   = device->EUID;

        printf("\nDevice[%d]\n", d);
        printf("EUID: %s\n", device->EUID.Data);
        printf("RegisteredID: %s\n", device->RegisteredID.Data);
        printf("ModelCode: %s\n", device->ModelCode);
        printf("FirmwareVerion: %s\n", device->FirmwareVersion);
        printf("FriendlyName: %s\n", device->FriendlyName);
        printf("ManufacturerName: %s\n", device->ManufacturerName);
        printf("Certified: %X\n", device->Certified);
        printf("Capabilities: %d\n", device->CapabilityCount);

        for (c = 0; c < device->CapabilityCount; c++)
        {
            capability    = device->Capability[c];
            capability_id = capability->CapabilityID;

            printf("Capability[%d] %s %s(%s)\n", c, capability->CapabilityID.Data, capability->ProfileName, capability->AttrName);

            if (strstr(capability->Control, "R"))
                SD_GetDeviceCapabilityValue(&device_id, &capability_id, value, 0, SD_REAL);
        }
    }

    puts("");
}
//-----------------------------------------------------------------------------
void TestGroupList()
{
    SD_GroupList        group_list;
    SD_Group            group;
    SD_Device           device;
    SD_Capability       capability;
    SD_CapabilityID     capability_id;
    int                 count, g, d, c;
    char                value[CAPABILITY_MAX_VALUE_LENGTH];

    count = SD_GetGroupList(&group_list);

    printf("GroupList:%d\n", count);

    for (g = 0; g < count; g++)
    {
        group = group_list[g];

        printf("\nGroup[%d] %s %s\n", g, group->ID.Data, group->Name);

        printf("Devices: %d\n", group->DeviceCount);

        for (d = 0; d < group->DeviceCount; d++)
        {
            device = group->Device[d];

            printf("Device[%d] %s\n", d, device->EUID.Data);
        }

        printf("Capabilities: %d\n", group->CapabilityCount);

        for (c = 0; c < group->CapabilityCount; c++)
        {
            capability    = group->Capability[c];
            capability_id = capability->CapabilityID;

            printf("Capability[%d] %s(%s)\n", c, capability->ProfileName, capability->AttrName);

            if (strstr(capability->Control, "R"))
                SD_GetGroupCapabilityValue(&group->ID, &capability_id, value, SD_REAL);
        }
    }

    puts("");
}
//-----------------------------------------------------------------------------
void TestDeviceDetails(int argc, char* argv[])
{
    // device 0

    SD_DeviceList       list;
    SD_Device           device;
    int                 di;
    int                 count;
    const char*         details;

    puts("\nTestDeviceDetails() \n");

    if (argc < 3)
    {
        puts("* Insufficient parameters \n");
        return;
    }

    sscanf(argv[2], "%d", &di);

    count = SD_GetDeviceList(&list);

    if (count <= di)
    {
        printf("* Invalid device index: %d\n\n", di);
        return;
    }

    device = list[di];

    if (SD_GetDeviceDetails(&device->EUID, &details))
    {
        printf("\nDevice[%d]: %s\n\n", di, device->EUID.Data);
        printf("%s\n", details);
    }

    SD_ReloadDeviceInfo(&device->EUID);
}
//-----------------------------------------------------------------------------
void TestOpenNetwork(unsigned duration, bool pair)
{
    SD_DeviceList       list;
    SD_Device           device;
    int                 count;
    int                 s, d;

    unsigned            start, current, passed;

    printf("OpenNetwork() \n");

    start = time(0);

    SD_OpenNetwork();

    for (s=0; s < duration; s++)
    {
        count = SD_GetDeviceList(&list);

        if (pair)
        {
            for (d = 0; d < count; d++)
            {
                device = list[d];

                if (!device->Paired)
                    SD_AddDevice(&device->EUID);
            }
        }

        printf(":%02d  Devices:%d \n", duration - s, count);

        sleep(1);

        current = time(0);
        passed  = current - start;

        if (passed >= duration)
            break;
    }
}
//-----------------------------------------------------------------------------
void TestCloseNetwork()
{
    SD_CloseNetwork();
}
//-----------------------------------------------------------------------------
void TestJoin(int argc, char* argv[])
{
    // open 25 pair

    unsigned    duration = 0;
    bool        pair     = 0;

    puts("\nTestJoin() \n");

    if (2 < argc)
        sscanf(argv[2], "%d", &duration);

    if (3 < argc)
        pair = (strcmp("pair", argv[3]) == 0);

    if (duration < 5)
    {
        duration = 5;
        printf("Using default duration %d\n", duration);
    }

    SD_SetQAControl("SetDeviceScanOption", "CommandRetryInterval:200");

    TestOpenNetwork(duration, pair);

    TestCloseNetwork();

    TestDeviceList();
}
//-----------------------------------------------------------------------------
void TestGroup(int argc, char* argv[])
{
    // group ABCD 0 1

    int                 result;
    SD_DeviceList       list;
    SD_Device           device;
    char*               group_id_text;
    char*               group_name;
    int                 a, d, all_devices, group_devices;
    SD_GroupID          group_id;
    SD_DeviceID         devices[GROUP_MAX_DEVICES];
    SD_Group            group;


    puts("\nTestGroup() \n");

    if (argc < 3)
    {
        puts("* GroupID required\n");
        return;
    }
    group_id_text = argv[2];

    group_name = argv[3];

    all_devices = SD_GetDeviceList(&list);

    group_devices = 0;

    printf("  group(%s,%s)\n", group_id_text, group_name);

    for (a = 4; a < argc; a++)
    {
        sscanf(argv[a], "%d", &d);

        if (d < all_devices)
        {
            if (group_devices < GROUP_MAX_DEVICES)
            {
                device = list[d];

                devices[group_devices] = device->EUID;
                group_devices++;

                printf("  + device[%d] \n", d);
            }
            else
                break;
        }
    }

    strncpy(group_id.Data, group_id_text, GROUP_MAX_ID_LENGTH);

    result = SD_SetGroupDevices(&group_id, group_name, devices, group_devices);
    printf("  result: %d\n", result);

    if (group_devices && SD_GetGroupOfDevice(&devices[0], &group))
        printf("  group: %s\n", group->ID.Data);

    TestGroupList();
}
//-----------------------------------------------------------------------------
void TestSetDeviceValue(int argc, char* argv[])
{
    // set_device 0 1 255,100

    SD_DeviceList       list;
    SD_Device           device;
    SD_Capability       capability;
    int                 di, ci;
    char*               value = "";
    int                 count, once;
    bool                result;

    puts("\nTestSetDeviceValue() \n");

    if (argc < 4)
    {
        puts("* Insufficient parameters \n");
        return;
    }

    sscanf(argv[2], "%d", &di);
    sscanf(argv[3], "%d", &ci);

    if (4 < argc)
        value = argv[4];

    printf("  set: device[%d] capability[%d] %s \n\n", di, ci, value);

    count = SD_GetDeviceList(&list);

    for (once=1; once; once--)
    {
        if (count <= di)
        {
            printf("* Invalid device index: %d\n\n", di);
            break;
        }

        device = list[di];

        if (device->CapabilityCount <= ci)
        {
            printf("* Invalid capability index: %d\n\n", ci);
            break;
        }

        capability = device->Capability[ci];

        result = SD_SetDeviceCapabilityValue(&device->EUID, &capability->CapabilityID, value, SE_LOCAL);

        printf("\n  result: %d (%s)\n\n", result, device->CapabilityValue[ci]);
    }
}
//-----------------------------------------------------------------------------
void TestSetGroupValue(int argc, char* argv[])
{
    // set_group 0 1 255,100

    SD_GroupList        list;
    SD_Group            group;
    SD_Capability       capability;
    int                 gi, ci;
    char*               value = "";
    int                 count, once;
    bool                result;

    puts("\nTestSetGroupValue() \n");

    if (argc < 4)
    {
        puts("* Insufficient parameters \n");
        return;
    }

    sscanf(argv[2], "%d", &gi);
    sscanf(argv[3], "%d", &ci);

    if (4 < argc)
        value = argv[4];

    printf("  set: group[%d] capability[%d] (%s) \n\n", gi, ci, value);

    count = SD_GetGroupList(&list);

    for (once=1; once; once--)
    {
        if (count <= gi)
        {
            printf("* Invalid group index: %d\n\n", gi);
            break;
        }

        group = list[gi];

        if (group->CapabilityCount <= ci)
        {
            printf("* Invalid capability index: %d\n\n", ci);
            break;
        }

        capability = group->Capability[ci];

        result = SD_SetGroupCapabilityValue(&group->ID, &capability->CapabilityID, value, SE_LOCAL);

        printf("\n  result: %d (%s)\n\n", result, group->CapabilityValue[ci]);
    }
}
//-----------------------------------------------------------------------------
void TestClearList()
{
    SD_DeviceList list;

    if (!SD_GetDeviceList(&list))
        return;

    printf("\nUnpairing all devices and clearing stored list ... \n\n");

    SD_ClearAllStoredList();

    sPostMessage = "\n Please reboot ... \n";
}
//-----------------------------------------------------------------------------
void TestQAControl(int argc, char* argv[])
{
    char*   command;
    char*   parameter;

    if (argc < 3)
    {
        puts("* Insufficient parameters \n");
        return;
    }

   command = argv[2];

   if (3 < argc)
        parameter = argv[3];
   else
        parameter = 0;

    SD_SetQAControl(command, parameter);
}
//-----------------------------------------------------------------------------
void TestSetRegisteredID()
{
    SD_DeviceList       device_list;
    SD_Device           device;
    SD_DeviceID         registered_id;
    int                 count, d;

    printf("\n%s()\n\n", __FUNCTION__);

    count = SD_GetDeviceList(&device_list);

    for (d = 0; d < count; d++)
    {
        device = device_list[d];

        registered_id.Type = SD_DEVICE_REGISTERED_ID;
        sprintf(registered_id.Data, "RD%04d", d);
        printf("%s \n", registered_id.Data);

        SD_SetDeviceRegisteredID(&device->EUID, &registered_id);
    }

    puts("");
}
//-----------------------------------------------------------------------------
void TestCapabilityValueExtension()
{
    SD_CapabilityID     ctrl_level = {SD_CAPABILITY_ID, "10008" };
    SD_CapabilityID     ctrl_sleep = {SD_CAPABILITY_ID, "30008" };

    SD_DeviceList       list;
    SD_Device           device;
    int                 count;
    char*               input;
    char                output[CAPABILITY_MAX_VALUE_LENGTH];
    bool                result;

    printf("\n%s()\n\n", __FUNCTION__);

    count = SD_GetDeviceList(&list);

    if (!count)
        return;

    device = list[0];

    input = "128:10";
    result = SD_SetDeviceCapabilityValue(&device->EUID, &ctrl_level, input, SE_LOCAL);

    input = "40:123456789";
    result = SD_SetDeviceCapabilityValue(&device->EUID, &ctrl_sleep, input, SE_LOCAL);

    result = SD_GetDeviceCapabilityValue(&device->EUID, &ctrl_level, output, 0, SD_REAL);
    result = SD_GetDeviceCapabilityValue(&device->EUID, &ctrl_sleep, output, 0, SD_REAL);

    sleep(5);
}

#define ZIGBEED_CNT       "ps | grep -v grep | grep -c zigbeed > /tmp/zigbeed_cnt"
#define ZIGBEED_CMD       "/sbin/zigbeed -d &"
#define FLAG_SETUP_DONE   "/tmp/Belkin_settings/flag_setup_done"
#define NETWORK_FORMING   "/tmp/Belkin_settings/sd_network_forming"

static int CheckZigbeed(void)
{
  int nZigbeed = 0;
  char szCommand[80] = {0,};

  snprintf(szCommand, sizeof(szCommand), "%s", ZIGBEED_CNT);
  system(szCommand);
  printf("ZIGBEED_CNT = %s\n", szCommand);

  FILE *fp = fopen("/tmp/zigbeed_cnt", "rb");
  if( NULL == fp )
  {
    printf("read Error %s\n", strerror(errno));
    nZigbeed = 0;
  }
  else
  {
    fscanf(fp, "%d", &nZigbeed);
    fclose(fp);
  }

  return nZigbeed;
}

static int RunZigeebd(void)
{
  struct stat st;
  int nZigbeed = 0;
  char szCommand[80] = {0,};

  if( stat(FLAG_SETUP_DONE, &st) == 0 )
  {
    printf("flag_setup_done is already existed...\n");
    return 1;
  }

  if( (nZigbeed = CheckZigbeed()) )
  {
    printf("Zigbeed is already running, nZigbeed = %d\n", nZigbeed);
    return 1;
  }

  snprintf(szCommand, sizeof(szCommand), "%s", ZIGBEED_CMD);
  System(szCommand);
  printf("ZIGBEED_CMD = %s\n", szCommand);

  sleep(1);

  nZigbeed = CheckZigbeed();

  printf("nZigbeed = %d\n", nZigbeed);

  if( nZigbeed )
  {
    memset(szCommand, 0x00, sizeof(szCommand));
    snprintf(szCommand, sizeof(szCommand), "touch %s", FLAG_SETUP_DONE);
    system(szCommand);

    if( stat(NETWORK_FORMING, &st) != 0 )
    {
      printf("zigbee network forming is not created...\n");
      SD_OpenInterface(false);
    }
    return 1;
  }

  return 0;
}

//-----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    char*   command;

    if (argc < 2)
    {
        printf(sUsage, argv[0]);
        return 0;
    }

    command = argv[1];

    puts("");

    ulog_debug_brief_output = ulog_debug_console;
    ulog_debug_output = ulog_debugf_console;

    RunZigeebd();

    if (!SD_OpenInterface(false))
    {
        return -1;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if (strcmp("list", command) == 0)
    {
        TestCapabilityList();
        TestDeviceList();
        TestGroupList();
    }
    else if (strcmp("open", command) == 0)
    {
        TestJoin(argc, argv);
    }
    else if (strcmp("device", command) == 0)
    {
        TestDeviceDetails(argc, argv);
    }
    else if (strcmp("group", command) == 0)
    {
        TestGroup(argc, argv);
    }
    else if (strcmp("set_device", command) == 0)
    {
        TestSetDeviceValue(argc, argv);
    }
    else if (strcmp("set_group", command) == 0)
    {
        TestSetGroupValue(argc, argv);
    }
    else if (strcmp("clear", command) == 0)
    {
        TestClearList();
    }
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    else if (strcmp("QA", command) == 0)
    {
        TestQAControl(argc, argv);
    }
    else if (strcmp("TestRegisteredID", command) == 0)
    {
        TestSetRegisteredID();
    }
    else if (strcmp("TestCapabilityValueExtension", command) == 0)
    {
        TestCapabilityValueExtension();
    }
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    else
    {
        printf("\n* Invalid Command: %s\n", command);
        printf(sUsage, argv[0]);
    }

    SD_CloseInterface(false);

    printf("%s\n", sPostMessage);

	return 0;
}
//-----------------------------------------------------------------------------


