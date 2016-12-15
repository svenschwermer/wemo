/***************************************************************************
 *
 *
 * simulatedOccupancy.c
 *
 * Created by Belkin International, Software Engineering on XX/XX/XX.
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
#ifdef SIMULATED_OCCUPANCY
#include "pthread.h"
#include "simulatedOccupancy.h"
#include "utils.h"
#include "wifiHndlr.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

pthread_mutex_t gSimulatedOccupancyLock;
SimulatedDevice* gpSimulatedDevice = NULL;
extern char g_szHomeId[SIZE_20B];

void initSimulatedOccupancyLock(void)
{
    osUtilsCreateLock(&gSimulatedOccupancyLock);
}

void LockSimulatedOccupancy(void)
{
    osUtilsGetLock(&gSimulatedOccupancyLock);
}

void UnlockSimulatedOccupancy(void)
{
    osUtilsReleaseLock(&gSimulatedOccupancyLock);
}

void simulatedOccupancyInit(void)
{
    if(!gpSimulatedDevice)
    {
	gpSimulatedDevice = (SimulatedDevice*)CALLOC(1, sizeof(SimulatedDevice));
#if 0
	if(!gpSimulatedDevice)
	{
	    APP_LOG("Rule", LOG_CRIT, "Away Rule memory allocation failure");
	    resetSystem();
	}
#endif
	char *pFirstTime = GetBelkinParameter (SIM_FIRST_ON_TIME);
	if (0x00 != pFirstTime && (0x00 != strlen(pFirstTime))){
	    gpSimulatedDevice->firstOnTime = atoi(pFirstTime);
	}else{
	    gpSimulatedDevice->firstOnTime = SIMULATED_FIRST_ON_TIME;
	}

	char *pMinTime = GetBelkinParameter (SIM_MIN_ON_TIME);
	if (0x00 != pMinTime && (0x00 != strlen(pMinTime))){
	    gpSimulatedDevice->minOnTime = atoi(pMinTime);
	}else{
	    gpSimulatedDevice->minOnTime = SIMULATED_MIN_ON_TIME;
	}

	char *pMaxTime = GetBelkinParameter (SIM_MAX_ON_TIME);
	if (0x00 != pMaxTime && (0x00 != strlen(pMaxTime))){
	    gpSimulatedDevice->maxOnTime = atoi(pMaxTime);
	}else{
	    gpSimulatedDevice->maxOnTime = SIMULATED_MAX_ON_TIME;
	}

	char *pDevCnt = GetBelkinParameter (SIM_DEVICE_COUNT);
	if (0x00 != pDevCnt && (0x00 != strlen(pDevCnt))){
	    gpSimulatedDevice->totalCount = atoi(pDevCnt);
	}else{
	    gpSimulatedDevice->totalCount = 0;
	}

	char *pManTrigDate = GetBelkinParameter (SIM_MANUAL_TRIGGER_DATE);
	if (0x00 != pManTrigDate && (0x00 != strlen(pManTrigDate))){
	    strncpy(gpSimulatedDevice->manualTriggerDate, pManTrigDate, sizeof(gpSimulatedDevice->manualTriggerDate)-1);
	}

	APP_LOG("REMOTEACCESS", LOG_ERR, "Simulated: FirstOntime: %d, MinimumOnTime: %d, MaximumOnTime: %d, device count: %d, ManualTriggerDate: %s", gpSimulatedDevice->firstOnTime, gpSimulatedDevice->minOnTime, gpSimulatedDevice->maxOnTime, gpSimulatedDevice->totalCount, gpSimulatedDevice->manualTriggerDate);

	initSimulatedOccupancyLock();
    }
} 

void simulatedStartControlPoint(void)
{
    if(-1 != ctrlpt_handle)
    {
	APP_LOG("DEVICE:rule", LOG_DEBUG, "CONTROL POINT ALREADY RUNNING....");
	return;
    }

    char* ip_address = NULL;
    ip_address = wifiGetIP(INTERFACE_CLIENT);
    APP_LOG("DEVICE:rule", LOG_DEBUG, "Start control point: ip_address: %s", ip_address);
    if (ip_address && (0x00 != strcmp(ip_address, DEFAULT_INVALID_IP_ADDRESS)))
    {
        int ret=StartPluginCtrlPoint(ip_address, 0x00);
        if(UPNP_E_INIT_FAILED==ret)
        {
            APP_LOG("UPNP", LOG_DEBUG,"UPNP on error: %d", ret);
            APP_LOG("UPNP", LOG_DEBUG,"################### Wemo App going to be reset ###################");
            resetSystem();
        }
	APP_LOG("DEVICE:rule", LOG_DEBUG, "Started control point");
    }
}

int getRandomTime(int fromTime, int toTime, int ruleStartTime, int ruleEndTime)
{
    int startTime = 0x00, endTime = 0x00, randomTime = 0x00, curTime = 0x00;
    int now = daySeconds();

    APP_LOG("DEVICE:rule", LOG_DEBUG, "Rule fromtime: %d, totime: %d, starttime: %d and endtime: %d, now: %d", fromTime, toTime, ruleStartTime, ruleEndTime, now);

    if(now < ruleStartTime)
    	curTime = ruleStartTime;
    else
    	curTime = now;

    APP_LOG("DEVICE:rule", LOG_DEBUG, "Current time is rule first timer: %d", curTime);

    if((curTime + fromTime) >= ruleEndTime)
    {
	startTime = curTime;
	APP_LOG("DEVICE:rule", LOG_DEBUG, "Start time is current time: %d", startTime);
    }
    else
    {
	startTime = (curTime + fromTime);
	APP_LOG("DEVICE:rule", LOG_DEBUG, "Start time: %d", startTime);
    }

    if((curTime + toTime) >= ruleEndTime)
    {
	endTime = ruleEndTime;
	APP_LOG("DEVICE:rule", LOG_DEBUG, "End time is rule end time: %d", endTime);
    }
    else
    {
	endTime = (curTime + toTime);
	APP_LOG("DEVICE:rule", LOG_DEBUG, "End time: %d", endTime);
    }

    randomTime = SIMULATED_RANDOM_TIME(startTime, endTime);
    
    APP_LOG("DEVICE:rule", LOG_DEBUG, "Random time: %d", randomTime);
    return randomTime;
}

int adjustFirstTimer(int startTimer, int endTimer)
{
    int randomduration = 0;
    int simulatedduration = 0;
    int nowTime = daySeconds();

    if(gpSimulatedDevice->selfIndex == 0) /* for first device in rule */
    {
	if( nowTime >= (startTimer+gpSimulatedDevice->firstOnTime) )
	{
	    APP_LOG("DEVICE:rule", LOG_DEBUG, "nowTime: %d > first random time: %d", nowTime, (startTimer+gpSimulatedDevice->firstOnTime));
	    return simulatedduration; 
	}
	/* here we are not considering -15 minutes window, as it leads to many other complications
	due to overlapping of this timer in window of another away rule, causing suitation like 2 away rules going active at same time
	since current rules DB framework doesn't allow this, so required additional handling in generic rule engine 
	even that additional handling could cause many other edge cases to arise */
	randomduration = getRandomTime(0, gpSimulatedDevice->firstOnTime, startTimer, endTimer); // in next 15 minutes 
	simulatedduration = randomduration - nowTime;
	APP_LOG("DEVICE:rule", LOG_DEBUG, "First device, simulated duration: %d", simulatedduration);
    }
    else      /* later devices */
    {
	randomduration = getRandomTime(gpSimulatedDevice->minOnTime, gpSimulatedDevice->maxOnTime, startTimer, endTimer);// 30 - 180 mins
	simulatedduration = randomduration - nowTime;
	APP_LOG("DEVICE:rule", LOG_DEBUG, "Later device, simulated duration: %d", simulatedduration);
    }

    gpSimulatedDevice->randomTimeToToggle = randomduration;

    return simulatedduration; 
}

void unsetSimulatedData(void)
{
    APP_LOG("Upnp", LOG_DEBUG,"unset Simulated Data...");
    SimulatedDevData *headNode = NULL;
    SimulatedDevData *node= NULL;

    LockSimulatedOccupancy();

    if(gpSimulatedDevice)
    {
	if(gpSimulatedDevice->pDevInfo)	//free simulated device list 
	{
	    free(gpSimulatedDevice->pDevInfo);
	    gpSimulatedDevice->pDevInfo= NULL;
	}
	if(gpSimulatedDevice->pUpnpRespInfo) //free collected info of other devices 
	{
	    headNode = gpSimulatedDevice->pUpnpRespInfo;
	    while(headNode)
	    {
		node = headNode;
		headNode = headNode->next;
		free(node);
	    }
	    gpSimulatedDevice->pUpnpRespInfo = NULL; 
	}

	free(gpSimulatedDevice);
	gpSimulatedDevice = NULL;
    }

    UnlockSimulatedOccupancy();		

    APP_LOG("Upnp", LOG_DEBUG,"unset Simulated Data done...");
}

int setSimulatedRuleFile(void *deviceList)
{
    APP_LOG("Upnp", LOG_ERR,"simulated rule file write...");
    FILE *fps = NULL;
    int ret = PLUGIN_SUCCESS;
    char *deviceListXml = NULL;
    char *localdeviceListXml = NULL;

    deviceListXml = (char*)deviceList;
    if(deviceListXml)
    {
	APP_LOG("UPNP", LOG_DEBUG, "device list XML is: %s", deviceListXml);
	localdeviceListXml = (char*)CALLOC(1, (strlen(deviceListXml)+SIZE_64B));
	if (!localdeviceListXml)
	    resetSystem();
        snprintf(localdeviceListXml, (strlen(deviceListXml)+SIZE_64B), "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>%s", deviceListXml);
	APP_LOG("UPNP", LOG_DEBUG, "XML modified is:\n %s\n", localdeviceListXml);
	fps = fopen(SIMULATED_RULE_FILE_PATH, "w");
	if (!fps)
	{
	    APP_LOG("Upnp", LOG_ERR,"simulated rule file error write");
	    ret = PLUGIN_FAILURE;
	    if (localdeviceListXml){free(localdeviceListXml); localdeviceListXml=NULL;} 
	    return ret;
	}
	fwrite(localdeviceListXml, sizeof(char), strlen(localdeviceListXml), fps);
	fclose(fps);
    }
    else
	ret = PLUGIN_FAILURE;

    APP_LOG("Upnp", LOG_ERR,"simulated rule file write done...");
    if (localdeviceListXml){free(localdeviceListXml); localdeviceListXml=NULL;} 
    return ret;
}

int parseSimulatedRuleFile(char *ruleBuf, SimulatedDevInfo *psDevList)
{
    int retVal = PLUGIN_SUCCESS, index = 0;
    mxml_node_t *tree;
    mxml_node_t *first_node, *chNode;
    mxml_node_t *top_node;
    mxml_index_t *node_index=NULL;
    char *simulatedRuleXml = NULL;
    int found = 0;

    APP_LOG("Upnp", LOG_DEBUG,"parseSimulatedRuleFile...");
    simulatedRuleXml = (char*)ruleBuf;
    if (!simulatedRuleXml)
    { 
	APP_LOG("Upnp", LOG_DEBUG,"simulatedRuleXml error");
	return PLUGIN_FAILURE;
    }

    APP_LOG("RULE", LOG_DEBUG, "XML received is:\n %s\n", simulatedRuleXml);

    tree = mxmlLoadString(NULL, simulatedRuleXml, MXML_OPAQUE_CALLBACK);
    if (tree)
    {
	APP_LOG("Upnp", LOG_DEBUG,"simulatedRuleXml tree");
	top_node = mxmlFindElement(tree, tree, "SimulatedRuleData", NULL, NULL, MXML_DESCEND);

	node_index = mxmlIndexNew(top_node,"Device", NULL);
	if (!node_index)
	{
	    APP_LOG("RULE", LOG_DEBUG, "node index error");
	    retVal = PLUGIN_FAILURE;
	    goto out;
	}

	first_node = mxmlIndexReset(node_index);
	if (!first_node)
	{
	    APP_LOG("RULE", LOG_DEBUG, "first node error");
	    retVal = PLUGIN_FAILURE;
	    goto out;
	}

	while (first_node != NULL)
	{
	    APP_LOG("RULE", LOG_DEBUG, "first node");
	    first_node = mxmlIndexFind(node_index,"Device", NULL);
	    if (first_node)
	    {
		APP_LOG("RULE", LOG_DEBUG, "first child node");
		chNode = mxmlFindElement(first_node, tree, "UDN", NULL, NULL, MXML_DESCEND);
		if (chNode && chNode->child)
		{
		    APP_LOG("RULE", LOG_DEBUG,"The node %s with value is %s\n", chNode->value.element.name, chNode->child->value.opaque);
		    if(!strcmp(chNode->child->value.opaque, g_szUDN_1))
		    {
			found = 1;
			APP_LOG("RULE", LOG_DEBUG,"!!!!!!!!! UDN MATCHED: %s|%s !!!!!!!!!", chNode->child->value.opaque, g_szUDN_1);
		    }
		    /* copy the data for caller */
		    strncpy(psDevList[index].UDN, chNode->child->value.opaque, sizeof(psDevList[index].UDN) - 1);
		}

		chNode = mxmlFindElement(first_node, tree, "index", NULL, NULL, MXML_DESCEND);
		if (chNode && chNode->child)
		{
		    APP_LOG("RULE", LOG_DEBUG,"The node %s with value is %s\n", chNode->value.element.name, chNode->child->value.opaque);
		    if(found)
		    {
			gpSimulatedDevice->selfIndex = atoi(chNode->child->value.opaque);
			APP_LOG("RULE", LOG_DEBUG,"!!!!!!!!! SELF INDEX: %d !!!!!!!!!", gpSimulatedDevice->selfIndex);
			found = 0;
		    }
		    /* copy the data for caller */
		    psDevList[index].devIndex = atoi(chNode->child->value.opaque);
		}
		APP_LOG("RULE", LOG_DEBUG, "psDevList[%d]: UDN: %s, device index: %d", index, psDevList[index].UDN, psDevList[index].devIndex);
	    }
	    /* advance to next memory location */
	    index++;
	}
	APP_LOG("RULE", LOG_DEBUG, "loop completed...");

	mxmlDelete(tree);
	APP_LOG("RULE", LOG_DEBUG, "tree deleted...");
    }
    else
	APP_LOG("RULE", LOG_ERR, "Could not load tree");
out:
    APP_LOG("RULE", LOG_DEBUG, "deleting index...");
    if (node_index) mxmlIndexDelete(node_index);
    APP_LOG("RULE", LOG_DEBUG, "returning...");
    return retVal;
}

char* getSimulatedRuleBuffer(void)
{
    FILE *fps = NULL;
    long lSize = 0;
    char *buffer = NULL;
    size_t result = 0;
    APP_LOG("Upnp", LOG_DEBUG,"getSimulatedRuleBuffer...");

    fps = fopen (SIMULATED_RULE_FILE_PATH, "r" );
    if (!fps)
    {
	APP_LOG("Simulated", LOG_ERR,"simulated rule file error");
	return 0x00;
    }

    fseek (fps, 0 , SEEK_END);
    lSize = ftell (fps);
    rewind (fps);

    buffer = (char*)CALLOC ( 1, (sizeof(char) * lSize) );
#if 0
    if (!buffer)
    {
	APP_LOG("Simulated", LOG_ERR,"calloc error");
	fclose (fps);
	return 0x00;
    }
#endif
    result = fread (buffer, sizeof(char), lSize, fps);
    if (result != lSize)
    {
	APP_LOG("Simulated", LOG_ERR,"simulated rule file read error");
	free(buffer);
	fclose (fps);
	return 0x00;
    }

    APP_LOG("Simulated", LOG_DEBUG, "Simulated XML read from file is: %s", buffer);
    fclose (fps);
    return buffer;
}

int parseSimulatedRule(void)
{
    int retVal = FAILURE;
    FILE* fpSimulatedRuleHandle = NULL;
    SimulatedDevInfo *devList = NULL;
    char *ruleBuf = NULL;

    APP_LOG("Upnp", LOG_DEBUG,"parse Simulated Rule...");
    fpSimulatedRuleHandle = fopen(SIMULATED_RULE_FILE_PATH, "r");
    if(!fpSimulatedRuleHandle)
    {
	APP_LOG("Rule", LOG_ERR, "Simulated Rule file not found, returning");
	return retVal;
    }
    else
    {
	fclose(fpSimulatedRuleHandle);
	APP_LOG("Rule", LOG_DEBUG, "Simulated Rule file found");

	ruleBuf = getSimulatedRuleBuffer();
	if(!ruleBuf)
	{
	    APP_LOG("Rule", LOG_DEBUG, "rule buffer is null, returning");
	    return retVal;
	}

	int numDevices = gpSimulatedDevice->totalCount;
	APP_LOG("Rule", LOG_DEBUG, "number of simulated rule device: %d", numDevices);
    //[WEMO-42920] - Exit parsing when no device found
    if (numDevices == 0)
    {
        APP_LOG("Rule", LOG_DEBUG, "No simulated device found, exit parsing rule from file.");
        return retVal;
    }

	devList = (SimulatedDevInfo *)CALLOC(numDevices, sizeof(SimulatedDevInfo));
#if 0	
        if(!devList)
	{
	    APP_LOG("Rule", LOG_DEBUG, "Memory allocation failed for Simulated Rule, returning");
	    resetSystem();
	}
#endif
	if((retVal = parseSimulatedRuleFile(ruleBuf, devList)) != SUCCESS)
	{
	    APP_LOG("Rule", LOG_DEBUG, "parse Simulated Rule File failure, returning");
	    free(ruleBuf);
	    free(devList);
	    return retVal;
	}
	APP_LOG("Rule", LOG_DEBUG, "parse Simulated Rule File success...");

	LockSimulatedOccupancy();
	gpSimulatedDevice->pDevInfo= devList;
	UnlockSimulatedOccupancy();
	free(ruleBuf);
	APP_LOG("Rule", LOG_DEBUG, "parse Simulated Rule success...");
	retVal = SUCCESS;
    }

    APP_LOG("Rule", LOG_DEBUG, "parse Simulated Rule done...");
    return retVal;
}

int setSimDevicesBinaryStateCount(int *devOff, int *devOn)
{
    APP_LOG("Rule", LOG_DEBUG, "setSimDevicesBinaryStateCount...");
    *devOff = 0;
    *devOn = 0;

    LockLED();
    int curState = GetCurBinaryState();
    UnlockLED();

    if(curState)
    {
	(*devOn)++;
	APP_LOG("Simulated",LOG_DEBUG, "BinaryState: %d, DevNumOn: %d", curState, *devOn);
    }
    else	
    {
	(*devOff)++;
	APP_LOG("Simulated",LOG_DEBUG, "BinaryState: %d, DevNumOff: %d", curState, *devOff);
    }

    if (0x00 == gpSimulatedDevice->pUpnpRespInfo)
    {

	APP_LOG("Simulated",LOG_DEBUG, "No simulated devices data");
	return FAILURE;
    }

    LockSimulatedOccupancy();
    SimulatedDevData *tmpNode =  gpSimulatedDevice->pUpnpRespInfo;
    if (tmpNode)
    {
	while (tmpNode)
	{
	    APP_LOG("Simulated",LOG_DEBUG, "Device: %s", tmpNode->sDevInfo.UDN);
	    if(tmpNode->binaryState)
	    {
		(*devOn)++;
		APP_LOG("Simulated",LOG_DEBUG, "BinaryState: %d, DevNumOn: %d", tmpNode->binaryState, *devOn);
	    }
	    else
	    {
		(*devOff)++;
		APP_LOG("Simulated",LOG_DEBUG, "BinaryState: %d, DevNumOff: %d", tmpNode->binaryState, *devOff);
	    }
	    tmpNode = tmpNode->next;
	}
    }
    UnlockSimulatedOccupancy();

    APP_LOG("Rule", LOG_DEBUG, "setSimDevicesBinaryStateCount done...");
    return SUCCESS;
}

int updateDiscoveredDevicesListInfo(void)
{
	APP_LOG("UPNP: Device", LOG_DEBUG, "Simulated Rule: update Discovered Devices List Info"); 
	pCtrlPluginDeviceNode tmpdevnode;
	SimulatedDevInfo *simdeviceinf;
	char szUDN[SIZE_256B] = {'\0',};

	int simdevicecnt = -1, i = 0, index = 0, matched = 0; 

	if (0x00 == g_pGlobalPluginDeviceList)
	{
		APP_LOG("UPnPCtrPt",LOG_DEBUG, "No device in discovery list");
		return index;
	}

	LockSimulatedOccupancy();
	simdeviceinf = gpSimulatedDevice->pDevInfo;
	simdevicecnt = gpSimulatedDevice->totalCount;
	UnlockSimulatedOccupancy();

	APP_LOG("Rule", LOG_DEBUG, "updateDiscoveredDevicesListInfo...: %d", simdevicecnt);
	for(i = 0; i < simdevicecnt; i++)
	{
		matched = 0x00;
		memset(szUDN, 0, sizeof(szUDN));

		if(strstr(simdeviceinf[i].UDN, "uuid:Bridge") != NULL)
		{
			strncpy(szUDN, simdeviceinf[i].UDN, BRIDGE_UDN_LEN);
		}
		else if(strstr(simdeviceinf[i].UDN, "uuid:Maker") != NULL)
                {
                        strncpy(szUDN, simdeviceinf[i].UDN, MAKER_UDN_LEN);
                        strncat(szUDN, ":sensor:switch", sizeof(szUDN)-strlen(szUDN)-1);
                }
		else
			strncpy(szUDN, simdeviceinf[i].UDN, sizeof(szUDN)-1);

		APP_LOG("UPNP: Device", LOG_DEBUG, "Input UDN: %s converted UDN: %s", simdeviceinf[i].UDN, szUDN);

		LockDeviceSync();
		tmpdevnode = g_pGlobalPluginDeviceList;

		while (tmpdevnode)
		{
			if(strcmp(tmpdevnode->device.UDN, szUDN) == 0)  //a simulated device
			{
				index++;
			}

	if (!tmpdevnode->next)
	    break;

			tmpdevnode = tmpdevnode->next;
		}

		UnlockDeviceSync();
	}

	if(index == (simdevicecnt-1))   //exclude self count
	{
		APP_LOG("UPNP: Device", LOG_DEBUG, "Simulated Rule: all simulated Discovered Devices - index: %d", index);
		return 0x01; 
	}

	APP_LOG("UPNP: Device", LOG_DEBUG, "Simulated Rule: update Discovered Devices List Info done...%d|%d", index, simdevicecnt); 
	return 0x00;
}

int simulatedToggleBinaryState(int new_state)
{
	int ret;
	APP_LOG("Rule", LOG_DEBUG, "simulatedToggleBinaryState...");
	ret = SetRuleAction(new_state, ACTUATION_AWAY_RULE);
	APP_LOG("Rule", LOG_DEBUG, "simulatedToggleBinaryState done...");
	return ret;
}

int checkLastManualToggleState(void)
{
    int reset=0;
    /* check if manually toggled today */
    char date[SIZE_16B];
    char savedDate[SIZE_16B];
    char savedEndTime[SIZE_16B];

    /*check manual trigger date present*/
    if(!strlen(gpSimulatedDevice->manualTriggerDate))
    {
	    reset = 1;
	    return reset;
    }
    
    APP_LOG("Rule", LOG_DEBUG, "manual trigger date: %s", gpSimulatedDevice->manualTriggerDate);

    memset(date, 0, sizeof(date));
    memset(savedDate, 0, sizeof(savedDate));
    memset(savedEndTime, 0, sizeof(savedEndTime));

    todaysDate(date);
    sscanf(gpSimulatedDevice->manualTriggerDate,  "%[^:]:%s", savedDate, savedEndTime);
    APP_LOG("Rule", LOG_DEBUG, "Saved date: %s, saved end time: %s", savedDate, savedEndTime);
    if(!strcmp(savedDate, date))
    {
	APP_LOG("Rule", LOG_DEBUG, "manual trigger date is matching: %s with today's date: %s", savedDate, date);
	if(atoi(savedEndTime) == gpSimulatedDevice->ruleEndTime)
	{
	    APP_LOG("Rule", LOG_DEBUG, "manual trigger endtime: %d is matching with rule endtime: %d", atoi(savedEndTime), gpSimulatedDevice->ruleEndTime);
	}
	else
	    reset=1;
    }
    else
	reset=1;

    if(reset)
    {
	APP_LOG("Rule", LOG_DEBUG, "manual trigger date is not matching: %s with today's date: %s", gpSimulatedDevice->manualTriggerDate, date);
	memset(gpSimulatedDevice->manualTriggerDate, 0, sizeof(gpSimulatedDevice->manualTriggerDate));
	UnSetBelkinParameter(SIM_MANUAL_TRIGGER_DATE);
	AsyncSaveData();
	APP_LOG("Rule", LOG_DEBUG, "unset ManualTriggerDate: %s", gpSimulatedDevice->manualTriggerDate);
    }

    return reset;
}

void todaysDate(char *date)
{
    time_t rawTime;
    struct tm * timeInfo;
    time(&rawTime);
    timeInfo = localtime (&rawTime);
    snprintf(date, SIZE_32B, "%d%d%d", timeInfo->tm_mday, timeInfo->tm_mon, (timeInfo->tm_year + 1900));
}

void saveManualTriggerData(void)
{
    char manualTriggerDate[SIZE_32B];
    int endtime = 0; 
    memset(manualTriggerDate, 0, sizeof(manualTriggerDate));

    todaysDate(manualTriggerDate);
    endtime = gpSimulatedDevice->ruleEndTime;
    snprintf(manualTriggerDate+strlen(manualTriggerDate), sizeof(manualTriggerDate)-strlen(manualTriggerDate),":%d", endtime);
    APP_LOG("Rule", LOG_DEBUG, "Manual trigger date is: %s", manualTriggerDate);
    strncpy(gpSimulatedDevice->manualTriggerDate, manualTriggerDate, sizeof(gpSimulatedDevice->manualTriggerDate)-1);
    SetBelkinParameter(SIM_MANUAL_TRIGGER_DATE, manualTriggerDate);
    AsyncSaveData();
}

void enqueManualToggleNotification(int ruleId)
{
    SRulesQueue *qNode = NULL;
    int ruleMsgLen = strlen("RULECONFIG1") + 1;
    /*fill notification data*/
    qNode =  (SRulesQueue*)CALLOC(1, sizeof(SRulesQueue)); 
#if 0
    if(NULL == qNode)
    {
	APP_LOG("DEVICE:rule", LOG_DEBUG, "Memory Allocation Failed!");
	resetSystem();
    }
#endif
    qNode->ruleID = ruleId;
    qNode->notifyRuleID = atoi(g_szHomeId); 
    qNode->ruleMSG = (char*)CALLOC(1, ruleMsgLen); 
#if 0
    if(NULL == qNode->ruleMSG)
    {
	APP_LOG("DEVICE:rule", LOG_DEBUG, "Memory Allocation Failed!");
	resetSystem();
    }
#endif
    strncpy(qNode->ruleMSG, "RULECONFIG1", ruleMsgLen-1);
    qNode->ruleFreq = 300;

    qNode->ruleTS =  GetUTCTime();

    APP_LOG("DEVICE:rule", LOG_DEBUG, "Rule Queue Data\nruleID:%d\nnotifyRuleID:%d\nruleMSG:%s\nruleFreq:%d\nruleTS:%lu\n", qNode->ruleID,qNode->notifyRuleID,qNode->ruleMSG,qNode->ruleFreq,qNode->ruleTS);
    enqueueRuleQ(qNode);

    return;
}

extern SRuleInfo *gpsRuleList;
void sendManualToggleNotification(void)
{
	SRuleInfo *psRule = gpsRuleList;

	if(gRuleHandle[e_AWAY_RULE].ruleCnt)
	{
		while(psRule != NULL)
		{
			if((e_AWAY_RULE == psRule->ruleType) && (psRule->isActive))
			{
				APP_LOG("DEVICE:rule", LOG_DEBUG, "Rule ID %d is Active, Enqueue manual toggle notification!", psRule->ruleId);
				enqueManualToggleNotification(psRule->ruleId);
				break;
			}
			psRule = psRule->psNext;
		}
	}
}

void notifyManualToggle(void)
{
    int	cntdiscoveryCounter = SIMULATED_DISCOVERY_RETRIES;
    int isAllSimDevFound = 0;

    sendManualToggleNotification();

    /*making away rule count to zero so that countdown timer can execute it active*/
    gRuleHandle[e_AWAY_RULE].ruleCnt = 0;
#ifndef PRODUCT_WeMo_LEDLight
    checkAndExecuteCountdownTimer(g_PowerStatus);
#endif

    APP_LOG("Rule", LOG_DEBUG, "notify Manual Toggle...");
    saveManualTriggerData();

    /* discovery loop */
    while(cntdiscoveryCounter)
    {
	APP_LOG("UPNP: Device", LOG_DEBUG, "##################### Simulated Rule: Control point to discover: %d|%d ###################", 
		cntdiscoveryCounter, SIMULATED_DISCOVERY_RETRIES);

	/* always discover in this case */
	CtrlPointDiscoverDevices();
	APP_LOG("Rule", LOG_DEBUG, "notify Manual Toggle: discovery done");

	/* sleep for a while to allow device discovery and creation of device list */
	pluginUsleep((MAX_DISCOVER_TIMEOUT + 1)*1000000);

	/* update Discover list to set skip 0 or 1
	   return 1 if all simulated devices found else 0 
	   and update DevNumOff on last discovery retry */
	if(updateDiscoveredDevicesListInfo())
	{
	    isAllSimDevFound = 0x01;
	    APP_LOG("Rule", LOG_DEBUG, "notify Manual Toggle: all devices discovered: %d", isAllSimDevFound);
	}

	if(isAllSimDevFound || (cntdiscoveryCounter == 0x01))
	{
	    APP_LOG("Rule", LOG_DEBUG, "notify Manual Toggle: all devices discovered or all discovery retries done...");
	    break;
	}

	cntdiscoveryCounter--;
	APP_LOG("Rule", LOG_DEBUG, "notify Manual Toggle: discovery count: %d", cntdiscoveryCounter);
	pluginUsleep(SIMULATED_DISCOVERY_SLEEP*1000000);
    }//end of discovery loop 

	/* send action to the simluated device list: to notify about manual trigger */
#ifndef PRODUCT_WeMo_LEDLight
    PluginCtrlPointSendActionSimulated(PLUGIN_E_EVENT_SERVICE, "NotifyManualToggle");
    APP_LOG("Rule", LOG_DEBUG, "notify Manual Toggle send action done...");
#else
    PluginCtrlPointSendActionAll(PLUGIN_E_EVENT_SERVICE, "NotifyManualToggle", 0x00, 0x00, 0x00);
    APP_LOG("Rule", LOG_DEBUG, "notify Manual Toggle send action done...");
   
    // unsetSimDevSkipFlag();
#endif
    /*stop away task for the day*/
    stopExecutorThread(e_AWAY_RULE);   

    APP_LOG("Rule", LOG_DEBUG, "notify Manual Toggle done...");
}

int evaluateNextActions(int startTimer, int endTimer)
{
    APP_LOG("Rule", LOG_DEBUG, "evaluateNextActions...");
    /* get data from other devices in the rule */
    int	cntdiscoveryCounter = SIMULATED_DISCOVERY_RETRIES;
    int isAllSimDevFound = 0, upnpRespWait = SIMULATED_UPNP_RESP_WAIT;
    int deviceOnForTS = 0, simulatedduration = 0, randomduration = 0, duration = 0; 
    int devNumOff = -1, devNumOn = -1;


    if(startTimer >= endTimer)
    {
	APP_LOG("DEVICE:rule", LOG_DEBUG, "start >= end time");
	return 0;
    }


    /* TODO: Walk through the discovered device list & for devices which are in SimulatedDevInfo list, set skip =0  else skip =1  
       - Enhance Plugin Ctrl Point Send Action All to send action whenever skip = 0 and skip otherwise 
       - Call Action Get Simulated Rule Data which will return index, BinaryState, RemTimeToToggle 
       - Fill the data in SimulatedDevData structure & keep reference to the head in some global variable gpsSimulatedDevData  
       - Traverse the list starting from gpsSimulatedDevData and count gNumOff and gNumOn devices including self
       - Maintain the gOnFor variable as is done for Insight right now
     */
    if(!strlen(gpSimulatedDevice->manualTriggerDate))
    {
	/* discovery loop */
	while(cntdiscoveryCounter)
	{
	    APP_LOG("UPNP: Device", LOG_DEBUG, "##################### Simulated Rule: Control point to discover: %d|%d ###################", 
		    cntdiscoveryCounter, SIMULATED_DISCOVERY_RETRIES);

	    /* always discover in this case */
	    CtrlPointDiscoverDevices();
	    APP_LOG("Rule", LOG_DEBUG, "discovery done");

	    /* sleep for a while to allow device discovery and creation of device list */
	    pluginUsleep((MAX_DISCOVER_TIMEOUT + 1)*1000000);

	    /* update Discover list to set skip 0 or 1
	       return 1 if all simulated devices found else 0 
	       and update devNumOff on last discovery retry 
	     */
	    if(updateDiscoveredDevicesListInfo())
	    {
		isAllSimDevFound = 0x01;
		APP_LOG("Rule", LOG_DEBUG, "all devices discovered: %d", isAllSimDevFound);
	    }

	    if(isAllSimDevFound || (cntdiscoveryCounter == 0x01))
	    {
		APP_LOG("Rule", LOG_DEBUG, "all devices discovered or all discovery retries done...");
		break;
	    }

	    cntdiscoveryCounter--;
	    APP_LOG("Rule", LOG_DEBUG, "discovery counter: %d", cntdiscoveryCounter);
	    pluginUsleep(SIMULATED_DISCOVERY_SLEEP*1000000);
	}//end of discovery loop 

	/* send action to all simulated devices list: to get index, BinaryState, RemTimeToToggle */

	PluginCtrlPointSendActionSimulated(PLUGIN_E_EVENT_SERVICE, "GetSimulatedRuleData");


	/* SimulatedDevData structure fills in GetSimulatedRuleDataResponse CallBack */
	APP_LOG("Rule", LOG_DEBUG, "send action done");

	/* wait, to make sure all upnp action responses received */ 
	while(upnpRespWait)
	{
	    pluginUsleep(1000000);
	    if(gpSimulatedDevice->upnpRespTotalCount == (gpSimulatedDevice->totalCount - 1))
	    {
		APP_LOG("Rule", LOG_DEBUG, "got upnp action responses from all devices");
		break;
	    }
	    upnpRespWait--;
	}
	gpSimulatedDevice->upnpRespTotalCount = 0x00;

	/* set simulated devices binary state count: DevNumOn and DevNumOff */
	setSimDevicesBinaryStateCount(&devNumOff, &devNumOn);
	APP_LOG("Rule", LOG_DEBUG, "ON/OFF count inc done");

	if((devNumOn+devNumOff) < gpSimulatedDevice->totalCount)
	{
	    devNumOff += (gpSimulatedDevice->totalCount - (devNumOn+devNumOff));
	    APP_LOG("Rule", LOG_DEBUG, "DevNumOff: %d", devNumOff);
	}	

	/* compute device ON time from now time */
	LockLED();
	int curState = GetCurBinaryState();
	UnlockLED();
	time_t curTime = (int) GetUTCTime();
	if(curState)
	{ 
	    deviceOnForTS = (curTime - gpSimulatedDevice->onTS);
	    APP_LOG("Rule", LOG_DEBUG, "deviceOnForTS: %d, curTime: %d DevSelfOnTS: %d", deviceOnForTS, curTime, gpSimulatedDevice->onTS);
	}
	duration = daySeconds();
	APP_LOG("Rule", LOG_DEBUG, "duration: %d", duration);

	/* 
	   - Device woke up to take action but is ON for less than 30 mins, so don't toggle 
	   - Device woke up to take action but is OFF right not but already MAX allowed devices are ON, so don't toggle 
	   - ((GetCurBinaryState() == 0x1) && (gNumOn == 0)) is already taken care by gNumOn including this device
	   - TODO: Just schedule for the remaining time and enjoy
	 */
	if( ((deviceOnForTS < gpSimulatedDevice->minOnTime) && (GetCurBinaryState() == 0x1)) ||
		((GetCurBinaryState() == 0x0) && (devNumOn == SIMULATED_MAX_ON_ALLOWED)))
	{
	    randomduration = getRandomTime(gpSimulatedDevice->minOnTime, gpSimulatedDevice->maxOnTime, startTimer, endTimer);// 30 - 180mins
	    simulatedduration = (randomduration - duration) + SIMULATED_DURATION_ADDLTIME;
	    APP_LOG("DEVICE:rule", LOG_DEBUG, "updated simulated duration = %d", simulatedduration);
	}

	/* 
	   - Only one device running within the rule set, keep ON 30 mins and then toggle to OFF (taken in 1st if)
	   - When OFF, schedule next toggle time b/w current and current + 15 mins (why not toggle right away?)
	   - So... this condition doesnt bring up anything special
	 */
	else
	{
	    APP_LOG("Rule", LOG_DEBUG, "curState: %d", curState);
	    if((devNumOn + devNumOff) == 1)
	    {
		APP_LOG("Rule", LOG_DEBUG, "(DevNumOn + DevNumOff): %d", (devNumOn + devNumOff));
		// ON
		if(curState)
		{
		    randomduration = getRandomTime(0, gpSimulatedDevice->firstOnTime, startTimer, endTimer);	// between 15mins
		    simulatedduration = (randomduration - duration) + SIMULATED_DURATION_ADDLTIME;
		    APP_LOG("DEVICE:rule", LOG_DEBUG, "updated simulated duration = %d", simulatedduration);
		}
		//OFF
		else
		{
		    randomduration = (duration + gpSimulatedDevice->minOnTime);	// 30mins
		    simulatedduration = (randomduration - duration) + SIMULATED_DURATION_ADDLTIME;
		    APP_LOG("DEVICE:rule", LOG_DEBUG, "updated simulated duration = %d", simulatedduration);
		}
	    }
	    else
	    {
		if(curState && (devNumOff == (gpSimulatedDevice->totalCount - 1))) // all devices OFF, this can be OFF but ON should be in 15 mins
		{
		    randomduration = getRandomTime(0, gpSimulatedDevice->firstOnTime, startTimer, endTimer);	// between 15mins
		    simulatedduration = (randomduration - duration) + SIMULATED_DURATION_ADDLTIME;
		    APP_LOG("DEVICE:rule", LOG_DEBUG, "updated simulated duration = %d", simulatedduration);
		}
		else
		{
		    randomduration = getRandomTime(gpSimulatedDevice->minOnTime, gpSimulatedDevice->maxOnTime, startTimer, endTimer);//30- 180
		    simulatedduration = (randomduration - duration) + SIMULATED_DURATION_ADDLTIME;
		    APP_LOG("DEVICE:rule", LOG_DEBUG, "updated simulated duration = %d", simulatedduration);
		}
	    }

	    simulatedToggleBinaryState(!curState);
	}

	gpSimulatedDevice->randomTimeToToggle = randomduration;
    }

    APP_LOG("Rule", LOG_DEBUG, "evaluateNextActions done...");
    return simulatedduration;
}

void cleanupAwayRule(int ruleReload)
{
    gRuleHandle[e_AWAY_RULE].ruleCnt = 0;
    if(!ruleReload && gpSimulatedDevice && !strlen(gpSimulatedDevice->manualTriggerDate))
	    simulatedToggleBinaryState(0);
    unsetSimulatedData();
    if(!gProcessData)
	StopPluginCtrlPoint();
}

void setSimulatedStatus (char *status)
{
		int ts = GetUTCTime();
		snprintf(status, SIZE_256B, "<ruleID>%d</ruleID><ruleMsg><![CDATA[%s]]></ruleMsg><ruleFreq>300</ruleFreq><ruleExecutionTS>%d</ruleExecutionTS><productCode>WeMo</productCode>", atoi(g_szHomeId), "RULECONFIG1", ts);
		APP_LOG("REMOTEACCESS", LOG_DEBUG, "simStatus: %s", status);
}

void inline updateManualToggleStatus(char *httpStatus)
{
    if(gRuleHandle[e_AWAY_RULE].ruleCnt && (gpSimulatedDevice && strlen(gpSimulatedDevice->manualTriggerDate)))
    {
	char simStatus[SIZE_256B];
	memset(simStatus, 0, sizeof(simStatus));
	setSimulatedStatus(simStatus);
	strncat(httpStatus, simStatus, SIZE_2048B-1);
    }
}
 
#endif
