/***************************************************************************
*
*
* smartdevice.c
*
* Created by Belkin International, Software Engineering on Feb 6, 2013
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
#ifdef PRODUCT_WeMo_Smart
#include <ithread.h>
#include <upnp.h>
#include <sys/time.h>
#include <math.h>
#include <stdarg.h>


#include "global.h"
#include "defines.h"
#include "fw_rev.h"
#include "logger.h"
#include "wifiHndlr.h"
#include "controlledevice.h"
#include "gpio.h"
#include "wasp_api.h"
#include "wasp.h"
#include "WemoDB.h"

#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "itc.h"
#include "fwDl.h"
#include "remoteAccess.h"
#include "pktStructs.h"
#include "rule.h"
#include "plugin_ctrlpoint.h"
#include "utils.h"
#include "mxml.h"
#include "sigGen.h"
#include "watchDog.h"
#include "upnpCommon.h"
#include "osUtils.h"
#include "gpioApi.h"
#include "SmartRemoteAccess.h"
#include "notify.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

/* TNG notifications */
static unsigned long int g_ruleExecutionTS;

extern UpnpDevice_Handle device_handle;
extern WaspVarDesc **g_attrVarDescArray;
extern int g_attrCount;
extern int g_notificationFlag; //notifcation check flag
extern int g_attributeFlag[100];
extern int g_NotifSerialNumber;
extern sqlite3 *NotificationDB;
extern unsigned long int g_notifTimestamp;	

extern attrDetails notifattrdetails[10];
extern int g_numberofattributes;

extern char g_remotebuff[2048];
extern char* g_attrName;
extern char* g_prevValue;
extern char* g_curValue;

extern int g_remoteNotify;
extern int g_remoteAPNSNotify;

char g_attributeValue[32];
char g_attributeName[32];
char g_remotebuffAPNS[2048];
int g_notifattrFlag = 0;

extern int g_rulesTNGquiet;
extern int g_rulesTNGquit;
extern int g_rulesTNGmsg_count;

notify_receiver tempfptr = NULL;
notify_receiver receiverfptr = NULL;
static pthread_mutex_t   rulestng_mutex;
pthread_t rulestng_notification_thread;
int rtng_notificationFlag;
notify_receiver receiver;

int SetWASPSmartAttribute(unsigned char attributeID, unsigned char attributeType, char* newstatus);
extern void sigusr1(int signum);
void sendnotification_rulestng(enum notification_destination dest, const rtng_id rule_id,
              const char *msg, size_t len);

char convertSmartValues(unsigned char vartype, WaspVariable WaspAttr, int flag, char* getAttrResp)
{
    char retval=0;
    int attr;
    if(TO_SMART == flag) {
        switch(vartype) {
			case WASP_VARTYPE_ENUM:
                attr = atoi(getAttrResp);
                WaspAttr.Val.Enum = attr;
            break;
            case WASP_VARTYPE_PERCENT:
                attr = atoi(getAttrResp);
                WaspAttr.Val.Percent = attr;
            break;
            case WASP_VARTYPE_TEMP:
                attr = atoi(getAttrResp);
                WaspAttr.Val.Temperature = attr;
            break;
            case WASP_VARTYPE_TIME32:
                attr = atoi(getAttrResp);
                WaspAttr.Val.TimeTenths = attr;
            break;
            case WASP_VARTYPE_TIME16:
                attr = atoi(getAttrResp);
                WaspAttr.Val.TimeSecs = attr;
            break;
            case WASP_VARTYPE_BOOL:
                attr = atoi(getAttrResp);
                WaspAttr.Val.Boolean = attr;
            break;
            case WASP_VARTYPE_STRING:
                WaspAttr.Val.String = getAttrResp;
            break;
            case WASP_VARTYPE_BLOB:
                WaspAttr.Val.Blob.Data = getAttrResp;
            break;
            case WASP_VARTYPE_UINT8:
                attr = atoi(getAttrResp);
                WaspAttr.Val.U8 = attr;
            break;
            case WASP_VARTYPE_INT8:
                attr = atoi(getAttrResp);
                WaspAttr.Val.I8 = attr;
            break;
            case WASP_VARTYPE_UINT16:
                attr = atoi(getAttrResp);
                WaspAttr.Val.U16 = attr;
            break;
            case WASP_VARTYPE_INT16:
                attr = atoi(getAttrResp);
                WaspAttr.Val.I16 = attr;
            break;
            case WASP_VARTYPE_UINT32:
                attr = atoi(getAttrResp);
                WaspAttr.Val.U32 = attr;
            break;
            case WASP_VARTYPE_INT32:
                attr = atoi(getAttrResp);
                WaspAttr.Val.I32 = attr;
            break;
            case WASP_VARTYPE_TIME_M16:
                attr = atoi(getAttrResp);
                WaspAttr.Val.TimeMins = attr;
            break;
            default:
                APP_LOG("UPNPDevice", LOG_ERR, "Invalid attribute type");
			break;
        }
    }
    else { /*FROM_SMART*/
        switch(vartype) 
		{
			APP_LOG("SmartModule", LOG_ERR, "Inside convertSmartValues switch(g_attrVarDescArray[i]->Type)");
            case WASP_VARTYPE_ENUM:
                sprintf(getAttrResp, "%d",WaspAttr.Val.Enum);
            break;
            
			case WASP_VARTYPE_PERCENT:
                sprintf(getAttrResp, "%hu",WaspAttr.Val.Percent);
            break;
            
			case WASP_VARTYPE_TEMP:
                sprintf(getAttrResp, "%h",WaspAttr.Val.Temperature);
            break;
			
			case WASP_VARTYPE_TIME32:
                sprintf(getAttrResp, "%d",WaspAttr.Val.TimeTenths);
			break;
			
			case WASP_VARTYPE_TIME16:
                sprintf(getAttrResp, "%hu",WaspAttr.Val.TimeSecs);
			break;
			
			case WASP_VARTYPE_BOOL:
                sprintf(getAttrResp, "%d",WaspAttr.Val.Boolean);
			break;
            
			case WASP_VARTYPE_STRING:
                sprintf(getAttrResp, "%s",WaspAttr.Val.String);
            break;
            
			case WASP_VARTYPE_BLOB:
                sprintf(getAttrResp, "%s",WaspAttr.Val.Blob.Data);
            break;
            
			case WASP_VARTYPE_UINT8:
                sprintf(getAttrResp, "%d",WaspAttr.Val.U8);
            break;
            
			case WASP_VARTYPE_INT8:
                sprintf(getAttrResp, "%d",WaspAttr.Val.I8);
            break;
            
			case WASP_VARTYPE_UINT16:
                sprintf(getAttrResp, "%d",WaspAttr.Val.U16);
            break;
				
			case WASP_VARTYPE_INT16:
                sprintf(getAttrResp, "%d",WaspAttr.Val.I16);
			break;
			
			case WASP_VARTYPE_UINT32:
                sprintf(getAttrResp, "%d",WaspAttr.Val.U32);
			break;
            
			case WASP_VARTYPE_INT32:
                sprintf(getAttrResp, "%d",WaspAttr.Val.I32);
			break;
            
			case WASP_VARTYPE_TIME_M16:
                sprintf(getAttrResp, "%d",WaspAttr.Val.TimeMins);
			break;
            
			default:
                APP_LOG("UPNPDevice", LOG_ERR, "Invalid attribute type");
			break;
        }
    }
    return retval;
}

void SmartStateChangeNotify(char *szCurState)
{
	int ret=-1;
	char* paramters[] = {"attributeList"} ;

	if (!IsUPnPNetworkMode())
	{
		//- Not report since not on router or internet
		APP_LOG("UPNP", LOG_DEBUG, "Notification:Smart Device Status: Not in home network, ignored");
		return;
	}

	/* Encode XML */
	char *notifResp = encodeUnicodeXML(szCurState);

	ret = UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].UDN, 
			SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].ServiceId, (const char **)paramters, (const char **)&notifResp, 1);

	if(UPNP_E_SUCCESS != ret)
	{
		APP_LOG("UPNP", LOG_DEBUG, "########## UPnP Notification ERROR #########");
	}
	else {
		APP_LOG("UPNP", LOG_DEBUG, "State change notified...");
		remoteAccessUpdateStatusTSParams(1);
	}
	memset(notifResp, 0x00, sizeof(strlen(notifResp)));
	memset(szCurState, 0x00, sizeof(strlen(szCurState)));
}

int SetWASPSmartAttribute(unsigned char attributeID, unsigned char attributeType, char* newstatus)
{
	WaspVariable WaspAttr;
	int retstatus = 0x00;
	int Err,attr;
	float tempVal;	
		
		memset(&WaspAttr,0,sizeof(WaspVariable));
		WaspAttr.ID = attributeID;
		WaspAttr.Type = attributeType;

            switch(attributeType)
            {
                case WASP_VARTYPE_ENUM:
					attr = atoi(newstatus);
					WaspAttr.Val.Enum = attr;
                break;
                
				case WASP_VARTYPE_PERCENT:
					tempVal = atof(newstatus);
					WaspAttr.Val.Percent = tempVal * 10;
                break;
				
                case WASP_VARTYPE_TEMP:
					tempVal = atof(newstatus);
					WaspAttr.Val.Temperature = tempVal * 10;
                break;
				
                case WASP_VARTYPE_TIME32:
					attr = atoi(newstatus);
					WaspAttr.Val.TimeTenths = attr;
                break;
				
                case WASP_VARTYPE_TIME16:
					attr = atoi(newstatus);
					WaspAttr.Val.TimeSecs = attr;
                break;
				
                case WASP_VARTYPE_BOOL:
					attr = atoi(newstatus);
					WaspAttr.Val.Boolean = attr;
                break;
				
                case WASP_VARTYPE_STRING:
					WaspAttr.Val.String = newstatus;
                break;
				
                case WASP_VARTYPE_BLOB:
					WaspAttr.Val.Blob.Data = newstatus;
                break;
                
				case WASP_VARTYPE_UINT8:
					attr = atoi(newstatus);
					WaspAttr.Val.U8 = attr;
                break;
				
                case WASP_VARTYPE_INT8:
					attr = atoi(newstatus);
					WaspAttr.Val.I8 = attr;
                break;
				
                case WASP_VARTYPE_UINT16:
					attr = atoi(newstatus);
					WaspAttr.Val.U16 = attr;
                break;
				
                case WASP_VARTYPE_INT16:
					attr = atoi(newstatus);
					WaspAttr.Val.I16 = attr;
                break;
				
                case WASP_VARTYPE_UINT32:
					attr = atoi(newstatus);
					WaspAttr.Val.U32 = attr;
                break;
				
                case WASP_VARTYPE_INT32:
					attr = atoi(newstatus);
					WaspAttr.Val.I32 = attr;
                break;

                case WASP_VARTYPE_TIME_M16:
					attr = atoi(newstatus);
					WaspAttr.Val.TimeMins = attr;
                break;
				
                default:
					APP_LOG("UPNPDevice", LOG_ERR, "Invalid attribute type");
				break;
            }

            if((Err = WASP_SetVariable(&WaspAttr)) != WASP_OK) {
                if(Err >= 0) {
                    Err = -Err;
                }
                retstatus = Err;
            }
   return retstatus;
}

int SetSmartAttribute(int attributeID, int newaction)
{
	int ret = 0x00;
	int i,j=-1;
	unsigned char AttributeID, AttributeType;
	char newAction[SIZE_4B]={0,};
	
	for(i =0; i < g_attrCount; i++)
	{
		if(attributeID == (int)g_attrVarDescArray[i]->ID)
		{
			j = i;
			APP_LOG("UPNP", LOG_DEBUG, "ID matching in WASP database table");
			break;
		}
		APP_LOG("UPNP", LOG_ERR, "ID not matching in WASP database table");
		return 0x01;
	}
	
	if(g_attrVarDescArray[j]->Usage == 4 || g_attrVarDescArray[j]->Usage == 3)
	{
			AttributeID = g_attrVarDescArray[j]->ID;
			AttributeType = g_attrVarDescArray[j]->Type;
			snprintf(newAction,sizeof(newaction), "%d", newaction);
		    APP_LOG("UPNP", LOG_DEBUG, "Passing parameters to WASP ID is %d, Type is %d and Value is %s", AttributeID,AttributeType,newAction);
			
			ret = SetWASPSmartAttribute(AttributeID, AttributeType, newAction);
	}
	else
	{
		APP_LOG("UPNPDevice", LOG_ERR, "The attribute ID dont have set properties");
		ret = 0x01;
	}
	return ret;	
}

/*
 * Get the current Jarden device Status as requested the remote cloud server.
 */
int GetSmartAttributesRemote(SmartParameters *SmartParams, int i)
{
	int Ret=0x00;
	WaspVariable WaspAttr;
	int tempInt1 = 0;
	int tempInt2 = 0;
	int Err;

	/* Get the current mode setting */
	memset(&WaspAttr,0,sizeof(WaspVariable));
	WaspAttr.ID = g_attrVarDescArray[i]->ID;
	WaspAttr.Type = g_attrVarDescArray[i]->Type;

	if((Err = WASP_GetVariable(&WaspAttr)) != WASP_OK) 
	{
		APP_LOG("UPNPDevice", LOG_DEBUG, "At WASP Error");
		if(Err >= 0) {
			Err = -Err;
		}
		Ret = Err;
	}
	else 
	{
		if(WaspAttr.Pseudo.bPseudoVar) 
		{
			// Display Pseudo variable information
			if(WaspAttr.Pseudo.TimeStamp == 0) 
			{
				APP_LOG("UPNPDevice", LOG_DEBUG, "Timestamp for %s: not set", g_attrVarDescArray[i]->Name);
				Ret = -1;
			}
			else 
			{
				sprintf(SmartParams->status, "%ld", WaspAttr.Pseudo.TimeStamp);
				strncpy(SmartParams->Name, g_attrVarDescArray[i]->Name, sizeof(SmartParams->Name)-1);
				APP_LOG("REMOTE", LOG_DEBUG, "GetSmartAttributesRemote - Attribute:%s, Value:%s", SmartParams->Name, 
				SmartParams->status);				   
				Ret = 0;
			}
		}
		else 
		{		
			switch(g_attrVarDescArray[i]->Type)
			{
				case WASP_VARTYPE_ENUM:
					Ret = snprintf(&SmartParams->status,sizeof(SmartParams->status),"%d",WaspAttr.Val.Enum);
					break;

				case WASP_VARTYPE_PERCENT:
					tempInt1 = WaspAttr.Val.Percent/10;
					tempInt2 = WaspAttr.Val.Percent%10;	
					Ret = snprintf(SmartParams->status,sizeof(SmartParams->status),"%d.%d", tempInt1, tempInt2);
					break;

				case WASP_VARTYPE_TEMP:
					tempInt1 = WaspAttr.Val.Temperature/10;
					tempInt2 = WaspAttr.Val.Temperature%10;
					if(tempInt2 < 0)
					{
						tempInt2 = -tempInt2;
					}
					Ret = snprintf(SmartParams->status,sizeof(SmartParams->status),"%d.%d", tempInt1, tempInt2);
					break;

				case WASP_VARTYPE_TIME32:
					Ret = snprintf(SmartParams->status,sizeof(SmartParams->status),"%d",WaspAttr.Val.TimeTenths);
					break;

				case WASP_VARTYPE_TIME16:
					Ret = snprintf(SmartParams->status,sizeof(SmartParams->status),"%hu",WaspAttr.Val.TimeSecs);
					break;

				case WASP_VARTYPE_BOOL:
					Ret = snprintf(SmartParams->status,sizeof(SmartParams->status),"%d",WaspAttr.Val.Boolean);
					break;

				case WASP_VARTYPE_STRING:
					Ret = snprintf(SmartParams->status,sizeof(SmartParams->status),"%s",WaspAttr.Val.String);
					break;

				case WASP_VARTYPE_BLOB:
					Ret = snprintf(SmartParams->status,sizeof(SmartParams->status),"%s",WaspAttr.Val.Blob.Data);
					break;

				case WASP_VARTYPE_UINT8:
					Ret = snprintf(SmartParams->status,sizeof(SmartParams->status),"%d",WaspAttr.Val.U8);
					break;

				case WASP_VARTYPE_INT8:
					Ret = snprintf(SmartParams->status,sizeof(SmartParams->status),"%d",WaspAttr.Val.I8);
					break;

				case WASP_VARTYPE_UINT16:
					Ret = snprintf(SmartParams->status,sizeof(SmartParams->status),"%d",WaspAttr.Val.U16);
					break;

				case WASP_VARTYPE_INT16:
					Ret = snprintf(SmartParams->status,sizeof(SmartParams->status),"%d",WaspAttr.Val.I16);
					break;

				case WASP_VARTYPE_UINT32:
					Ret = snprintf(SmartParams->status,sizeof(SmartParams->status),"%d",WaspAttr.Val.U32);
					break;

				case WASP_VARTYPE_INT32:
					Ret = snprintf(SmartParams->status,sizeof(SmartParams->status),"%d",WaspAttr.Val.I32);
					break;

				case WASP_VARTYPE_TIME_M16:
					Ret = snprintf(SmartParams->status,sizeof(SmartParams->status),"%d",WaspAttr.Val.TimeMins);
					break;

				default:
					APP_LOG("UPNPDevice", LOG_ERR, "Invalid attribute type 0x%x",WaspAttr.Type);
					break;
			}
			strncpy(SmartParams->Name, g_attrVarDescArray[i]->Name, sizeof(SmartParams->Name)-1);
			APP_LOG("REMOTE", LOG_DEBUG, "GetSmartAttributesRemote - Attribute:%s, Value:%s", SmartParams->Name, 
					SmartParams->status);				   
			Ret = 0;
		}
	}

	return Ret;
}

/*
 * Get the current Jarden device Status as requested the remote cloud server.
 */
int SetSmartAttributesRemote(SmartParameters *SmartParams)
{
	int Ret=0;
	unsigned char AttributeID, AttributeType;
	char newAction[30]={0};
	
	AttributeID = SmartParams->AttrID;
	AttributeType = SmartParams->AttrType;
	strncpy(newAction, SmartParams->status, sizeof(newAction));	
	//APP_LOG("SmartDevice", LOG_DEBUG, "SetSmartAttributesRemote - request is %c and %c and %s", AttributeID, AttributeType, newAction);
	
	Ret = SetWASPSmartAttribute(AttributeID, AttributeType, &newAction);
	
  return Ret;
}

/*----State Change Notification----*/
void UpdateNotifStructure(char* Name, char* PrevValue, char* CurValue)
{
	LockSmart();
	strcpy(notifattrdetails[g_numberofattributes].Name, Name);            // <-----<<< DANGER
	strcpy(notifattrdetails[g_numberofattributes].PrevValue, PrevValue);  // <-----<<< DANGER
	strcpy(notifattrdetails[g_numberofattributes].CurValue, CurValue);    // <-----<<< DANGER
	g_numberofattributes++;
	g_notifattrFlag = 1;
	UnlockSmart();
}
/*----State Change Notification----*/

/*--------rulestng notification----*/
void UpnpRulesTngNotify(char *szCurState)
{
	int ret=-1;
	char* paramters[] = {"rulesNotify"} ;

	if (!IsUPnPNetworkMode())
	{
		//- Not report since not on router or internet
		APP_LOG("UPNP", LOG_DEBUG, "Notification:Smart Device Status: Not in home network, ignored");
		return;
	}

	/* Encode XML */
	char *notifResp = encodeUnicodeXML(szCurState);

	ret = UpnpNotify(device_handle, SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].UDN, 
			SocketDevice.service_table[PLUGIN_E_EVENT_SERVICE].ServiceId, (const char **)paramters, (const char **)&notifResp, 1);

	if(UPNP_E_SUCCESS != ret)
	{
		APP_LOG("UPNP", LOG_DEBUG, "########## UPnP Notification ERROR #########");
	}
	else {
		APP_LOG("UPNP", LOG_DEBUG, "Rules TNG notified...");
		remoteAccessUpdateStatusTSParams(1);
	}
	memset(notifResp, 0x00, sizeof(notifResp));
	memset(szCurState, 0x00, sizeof(szCurState));
}

void sendnotification_rulestng (enum  notification_destination   dest,
                                const rtng_id                    rule_id,
                                const char                      *msg,
                                size_t                           len)
{
    unsigned int     rulesTNGNotifyRespLen         = 0;

    char             rulesTNGNotifyResp[SIZE_512B] = {0};
    char            *pRuleMsg                      = NULL;

    APP_LOG ("RulesTNGNotify", LOG_DEBUG, "%d: ### Entered ###", __LINE__);

    if ((NULL == msg) || (NULL == rule_id) || (0 == len))
    {
        APP_LOG ("RulesTNGNotify", LOG_DEBUG, "%d: Invalid Parameters", \
                 __LINE__);
        goto CLEAN_RETURN;
    }

    APP_LOG ("RulesTNGNotify", LOG_DEBUG, "%d: dest = %d", __LINE__, dest);
    APP_LOG ("RulesTNGNotify", LOG_DEBUG, "%d: rule_id = %s", __LINE__, \
             rule_id);
    APP_LOG ("RulesTNGNotify", LOG_DEBUG, "%d: msg = %s", __LINE__, msg);
    APP_LOG ("RulesTNGNotify", LOG_DEBUG, "%d: len = %d", __LINE__, len);

    pRuleMsg = (char *) MALLOC (sizeof (char) * len + 1);
#if 0
    if (NULL == pRuleMsg)
    {
        APP_LOG ("RulesTNGNotify", LOG_DEBUG, "%d: Malloc Error", __LINE__);
        goto CLEAN_RETURN;
    }
#endif
    memset (pRuleMsg, 0, sizeof (char) * len + 1);

    APP_LOG ("RulesTNGNotify", LOG_DEBUG, "%d: pRuleMsg = 0x%x", \
             __LINE__, pRuleMsg);

    strncpy (pRuleMsg, msg, len);
    pRuleMsg[len] = '\0';

    APP_LOG ("RulesTNGNotify", LOG_DEBUG, "%d: pRuleMsg = %s", \
             __LINE__, pRuleMsg);

    /* Time Stamp */
    g_ruleExecutionTS = GetUTCTime();

    /* Prepare the Rules TNG notification data to be sent to the destination */
    snprintf (rulesTNGNotifyResp, sizeof (rulesTNGNotifyResp), \
              "<ruleID>%s</ruleID><ruleMsg><![CDATA[ %s  ]]>" \
              "</ruleMsg><ruleExecutionTS>%ld</ruleExecutionTS>", \
              rule_id, pRuleMsg, g_ruleExecutionTS);

    rulesTNGNotifyRespLen = strlen (rulesTNGNotifyResp);

    APP_LOG ("RulesTNGNotify", LOG_DEBUG, "%d: rulesTNGNotifyRespLen" \
             ": %d", __LINE__, rulesTNGNotifyRespLen);
    APP_LOG ("RulesTNGNotify", LOG_DEBUG, "%d: rulesTNGNotifyResp" \
             ": %s", __LINE__, rulesTNGNotifyResp);

    switch (dest)
    {
        /* Notify local (mobile) application */
        case NOTIFY_IN_APP:
            {
                UpnpRulesTngNotify (rulesTNGNotifyResp);
            }
            break;

        /* Notify cloud application */
        case NOTIFY_SYSTEM:
            {
                strncpy (g_remotebuffAPNS, rulesTNGNotifyResp, \
                         rulesTNGNotifyRespLen);
                g_remotebuffAPNS [rulesTNGNotifyRespLen] = '\0';

                APP_LOG ("RulesTNGNotify", LOG_DEBUG, "%d: g_remotebuffAPNS = %s", \
                         __LINE__, g_remotebuffAPNS);

                LockSmartRemote();
                g_remoteAPNSNotify = 1;
                UnlockSmartRemote();
            }
            break;

        /* Notify both cloud and mobile app */
        case NOTIFY_BOTH:
            {
                /* Notify cloud */
                strncpy (g_remotebuffAPNS, rulesTNGNotifyResp, \
                         rulesTNGNotifyRespLen);
                g_remotebuffAPNS [rulesTNGNotifyRespLen] = '\0';

                APP_LOG ("RulesTNGNotify", LOG_DEBUG, "%d: g_remotebuffAPNS = %s", \
                         __LINE__, g_remotebuffAPNS);

                /* Notify App */
                UpnpRulesTngNotify (rulesTNGNotifyResp);

                LockSmartRemote();
                g_remoteAPNSNotify = 1;
                UnlockSmartRemote();
            }
            break;

        default:
            APP_LOG ("RulesTNGNotify", LOG_DEBUG, "%d: Bad Notification type", \
                     __LINE__);
            break;
    }

    g_rulesTNGmsg_count++;

CLEAN_RETURN:
    APP_LOG ("UPNPDevice", LOG_DEBUG, "%d: pRuleMsg = 0x%x", \
         __LINE__, pRuleMsg);

    if (NULL != pRuleMsg)
    {
        free (pRuleMsg);
        pRuleMsg = NULL;
    }

    APP_LOG ("RulesTNGNotify", LOG_DEBUG, "%d: ### Leaving ###", __LINE__);
}

void* attr_compare(void* arg)
{
	char str1[256];
	int i;
	char **s8ResultArray=NULL;
	int s32NoOfRows=0,s32NoOfCols=0;
	int s32RowCntr=0,s32ArraySize=0;
	int local_attributeFlag[100];
	int notif_type;
	while(1)
	{
		if(g_notificationFlag == 1)
		{
			sleep(3); // waiting for all attr changes to be recorded by wasp
			notif_type = 3; //Both local and remote notification

			LockSmartRemote();
			g_notificationFlag = 0;
			UnlockSmartRemote();
			APP_LOG("Notification", LOG_DEBUG, "Got notify attributes");

			sendnotification(notif_type);

			LockSmart();
			//clearthestructures;
			memset(notifattrdetails,0,sizeof(notifattrdetails));
			//thread sync flag
			g_notifattrFlag = 0;
			//set g_numberofattributes to 0;
			g_numberofattributes = 0;
			UnlockSmart();
		}
		sleep(1); 
	} 			
}

void printnotifstruct()
{
    int i = 0;
    
    for(i=0;i<g_numberofattributes;i++)
    {
        APP_LOG("Notification", LOG_DEBUG, "notifattrdetails[%d].Name is %s", i,notifattrdetails[i].Name);
        APP_LOG("Notification", LOG_DEBUG, "notifattrdetails[%d].PrevValue is %s", i,notifattrdetails[i].PrevValue);
        APP_LOG("Notification", LOG_DEBUG, "notifattrdetails[%d].CurValue is %s", i,notifattrdetails[i].CurValue);
    }
}


void sendnotification(int NotificationType)
{
	APP_LOG("Notification", LOG_DEBUG, "Inside sendnotification. NotificationType is %d", NotificationType);
	int i;

	switch (NotificationType)
	{
		case 3:
			{
				char sznotifState[256] = {0,};
				char sznotifRBuf[256] = {0,};
				char sznotifLBuf[2048] = {0,};
				char remoteNotifHead[64] = {0,};
				char remoteNotifTail[32] = {0,};

				sprintf(remoteNotifHead, "<attributeLists action=\"notify\">");
				sprintf(remoteNotifTail, "</attributeLists>");

				for(i=0;i<g_numberofattributes;i++)
				{

					/* Preparing notification XML */
					LockSmart();
					sprintf(sznotifState, "<attribute><name>%s</name><value>%s</value><prevalue>%s</prevalue><ts>%ld</ts></attribute>", notifattrdetails[i].Name,notifattrdetails[i].CurValue,notifattrdetails[i].PrevValue, g_notifTimestamp);
					strncat(sznotifLBuf, sznotifState, sizeof(sznotifLBuf));
					memset(sznotifState, 0, 256);
					UnlockSmart();
				}

				LockSmartRemote();
				strncat(g_remotebuff, remoteNotifHead, sizeof(g_remotebuff));
				strncat(g_remotebuff, sznotifLBuf, sizeof(g_remotebuff));
				strncat(g_remotebuff, remoteNotifTail, sizeof(g_remotebuff));
				UnlockSmartRemote();

				SmartStateChangeNotify(sznotifLBuf);

				LockSmartRemote();
				g_remoteNotify = 1;
				UnlockSmartRemote();

				memset(sznotifLBuf, 0, 2048);
			}
			break;
	}

}

int SetSmartBlobStorageRemote(char *pSmartParamName, char *pSmartParamValue)
{
        int retVal = PLUGIN_SUCCESS;
        APP_LOG("SetSmartBlobStorageRemote", LOG_DEBUG,"parseBlobStorageXMLData Called from Remote");
        retVal = parseBlobStorageXMLData(pSmartParamName, pSmartParamValue);
        return retVal;
}

int GetSmartBlobStorageRemote(char *pSmartParamName, char *pSmartParamValue)
{
        int retVal = PLUGIN_SUCCESS;
        char file_path[SIZE_64B]={0,};
        FILE* s_fpBlobHandle = 0x00;
        char dataValue[SIZE_256B]={0,};

        APP_LOG("GetSmartBlobStorageRemote", LOG_DEBUG,"pSmartParamName from APP:\n%s", pSmartParamName);

        snprintf(file_path,SIZE_64B,"/tmp/Belkin_settings/%s.txt",pSmartParamName);
        APP_LOG("GetSmartBlobStorageRemote", LOG_DEBUG, "File Path is:\n%s", file_path);

        s_fpBlobHandle = fopen(file_path, "r");
        if(s_fpBlobHandle)
        {
                APP_LOG("GetSmartBlobStorageRemote", LOG_DEBUG, "Blob file found, loading");
                //fscanf(s_fpBlobHandle, "%s", dataValue);
                fgets(dataValue, sizeof(dataValue), s_fpBlobHandle);
                APP_LOG("GetSmartBlobStorageRemote", LOG_DEBUG, "Blob Storage Value is %s", dataValue);
                pSmartParamValue = dataValue;
        }
        else
        {
                APP_LOG("GetBlobStorage", LOG_DEBUG, "Blob file Not found");
                pSmartParamValue = NULL;
                retVal = PLUGIN_FAILURE;
        }
        return retVal;
}
#endif /* #ifdef PRODUCT_WeMo_Smart */
