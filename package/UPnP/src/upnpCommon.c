/***************************************************************************
*
*
* common.c
*
* Copyright (c) 2000-2003 Intel Corporation
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* - Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* - Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
* - Neither name of Intel Corporation nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*
***************************************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>

#include "global.h"
#include "upnpCommon.h"
#include "logger.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

char *Util_GetElementValue(IXML_Element *element)
{
        IXML_Node *child = ixmlNode_getFirstChild((IXML_Node *)element);
        char *temp = NULL;

        if (child != 0 && ixmlNode_getNodeType(child) == eTEXT_NODE)
                temp = strdup(ixmlNode_getNodeValue(child));

        return temp;
}

IXML_NodeList *Util_GetFirstServiceList(IXML_Document *doc)
{
        IXML_NodeList *ServiceList = NULL;
        IXML_NodeList *servlistnodelist = NULL;
        IXML_Node *servlistnode = NULL;

        servlistnodelist =
                ixmlDocument_getElementsByTagName(doc, "serviceList");
        if (servlistnodelist && ixmlNodeList_length(servlistnodelist)) {
                /* we only care about the first service list, from the root
                 * device */
                servlistnode = ixmlNodeList_item(servlistnodelist, 0);
                /* create as list of DOM nodes */
                ServiceList = ixmlElement_getElementsByTagName(
                        (IXML_Element *)servlistnode, "service");
        }
        if (servlistnodelist)
                ixmlNodeList_free(servlistnodelist);

        return ServiceList;
}

#define OLD_FIND_SERVICE_CODE
#ifdef OLD_FIND_SERVICE_CODE
#else
/*
 * Obtain the service list
 *    n == 0 the first
 *    n == 1 the next in the device list, etc..
 */
static IXML_NodeList *Util_GetNthServiceList(
        /*! [in] . */
        IXML_Document *doc,
        /*! [in] . */
        unsigned int n)
{
        IXML_NodeList *ServiceList = NULL;
        IXML_NodeList *servlistnodelist = NULL;
        IXML_Node *servlistnode = NULL;

        /*  ixmlDocument_getElementsByTagName()
         *  Returns a NodeList of all Elements that match the given
         *  tag name in the order in which they were encountered in a preorder
         *  traversal of the Document tree.
         *
         *  return (NodeList*) A pointer to a NodeList containing the
         *                      matching items or NULL on an error.      */
        APP_LOG("Common",LOG_DEBUG, "Util_GetNthServiceList called : n = %d\n", n);
        servlistnodelist =
                ixmlDocument_getElementsByTagName(doc, "serviceList");
        if (servlistnodelist &&
            ixmlNodeList_length(servlistnodelist) &&
            n < ixmlNodeList_length(servlistnodelist)) {
                /* For the first service list (from the root device),
                 * we pass 0 */
                /*servlistnode = ixmlNodeList_item( servlistnodelist, 0 );*/

                /* Retrieves a Node from a NodeList} specified by a
                 *  numerical index.
                 *
                 *  return (Node*) A pointer to a Node or NULL if there was an
                 *                  error. */
                servlistnode = ixmlNodeList_item(servlistnodelist, n);
                if (!servlistnode) {
                        /* create as list of DOM nodes */
                        ServiceList = ixmlElement_getElementsByTagName(
                                (IXML_Element *)servlistnode, "service");
                } else
                        APP_LOG("Common",LOG_DEBUG, "%s(%d): ixmlNodeList_item(nodeList, n) returned NULL\n",
                                __FILE__, __LINE__);
        }
        if (servlistnodelist)
                ixmlNodeList_free(servlistnodelist);

        return ServiceList;
}
#endif
char *Util_GetValueFromXmlString(const char* input, const char *tag, char **isAvail)
{
	const char *start, *end,*tmpStr;
	char *out = NULL;
	char *buf =NULL;
	if((tmpStr = strstr(input, tag)) != NULL)
	{
		if(isAvail && !strcmp(tag,"available=") && strstr(input, "NO"))
		{
			char *outstr = "NO";
			buf = MALLOC(strlen(outstr)+1);
			strncpy(buf,outstr,strlen(outstr));
			*isAvail = buf;
		}
		start = strstr(tmpStr,">");
		start +=1;
		if((end = strstr(start, "<")) != NULL)
		{
			out = MALLOC(end - start + 1);
			if(out != NULL)
			{
				strncpy(out, start, (end - start));
				return out;
			}
		}
	}
	return NULL;
}
char *Util_GetFirstDocumentItem(IXML_Document *doc, const char *item)
{
    IXML_NodeList 	*nodeList = NULL;
    IXML_Node 		*textNode = NULL;
    IXML_Node 		*tmpNode = NULL;
    char 			*ret = NULL;
    nodeList = ixmlDocument_getElementsByTagName(doc, (char *)item);
    if (nodeList)
	{
       tmpNode = ixmlNodeList_item(nodeList, 0);
	   if (tmpNode)
	   {
	   		textNode = ixmlNode_getFirstChild(tmpNode);
			if (!textNode)
			{
                APP_LOG("Common",LOG_DEBUG, "%s(%d): (BUG) ixmlNode_getFirstChild(%s) returned NULL",
                        __FILE__, __LINE__, item);
                ret = strdup("");
                if (nodeList)
                	ixmlNodeList_free(nodeList);

				return ret;
             }

			ret = strdup(ixmlNode_getNodeValue(textNode));

			if (!ret)
			{
                APP_LOG("Common",LOG_DEBUG, "%s(%d): ixmlNode_getNodeValue returned NULL\n",
                        __FILE__, __LINE__);
                ret = strdup("");
                if (nodeList)
                	ixmlNodeList_free(nodeList);

				return ret;
            }

        }
	   else
	   {
	        APP_LOG("Common",LOG_DEBUG, "%s(%d): ixmlNodeList_item(nodeList, 0) returned NULL\n",
	                __FILE__, __LINE__);
	   }
    }

	if (nodeList)
		ixmlNodeList_free(nodeList);

	return ret;
}

char *Util_GetFirstElementItem(IXML_Element *element, const char *item)
{
    IXML_NodeList *nodeList = NULL;
    IXML_Node *textNode 	= NULL;
    IXML_Node *tmpNode 		= NULL;
    char *ret = NULL;

    nodeList = ixmlElement_getElementsByTagName(element, (char *)item);
    if (nodeList == NULL)
	{
            APP_LOG("Common",LOG_DEBUG, "%s(%d): Error finding %s in XML Node\n",
                    __FILE__, __LINE__, item);
            return NULL;
    }

	tmpNode = ixmlNodeList_item(nodeList, 0);
    if (!tmpNode)
	{
            APP_LOG("Common",LOG_DEBUG, "%s(%d): Error finding %s value in XML Node\n",
                    __FILE__, __LINE__, item);
            ixmlNodeList_free(nodeList);
            return NULL;
    }

	textNode = ixmlNode_getFirstChild(tmpNode);
    ret = strdup(ixmlNode_getNodeValue(textNode));

	if (!ret)
	{
            APP_LOG("Common",LOG_DEBUG, "%s(%d): Error allocating memory for %s in XML Node\n",
                    __FILE__, __LINE__, item);
            ixmlNodeList_free(nodeList);
            return NULL;
    }

	ixmlNodeList_free(nodeList);

    return ret;
}

void Util_PrintEventType(Upnp_EventType S)
{
        switch (S) {
        /* Discovery */
        case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
                APP_LOG("Common",LOG_DEBUG, "UPNP_DISCOVERY_ADVERTISEMENT_ALIVE\n");
                break;
        case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
                APP_LOG("Common",LOG_DEBUG, "UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE\n");
                break;
        case UPNP_DISCOVERY_SEARCH_RESULT:
                APP_LOG("Common",LOG_DEBUG,  "UPNP_DISCOVERY_SEARCH_RESULT\n");
                break;
        case UPNP_DISCOVERY_SEARCH_TIMEOUT:
                APP_LOG("Common",LOG_DEBUG,  "UPNP_DISCOVERY_SEARCH_TIMEOUT\n");
                break;
        /* SOAP */
        case UPNP_CONTROL_ACTION_REQUEST:
                APP_LOG("Common",LOG_DEBUG, "UPNP_CONTROL_ACTION_REQUEST\n");
                break;
        case UPNP_CONTROL_ACTION_COMPLETE:
                APP_LOG("Common",LOG_DEBUG, "UPNP_CONTROL_ACTION_COMPLETE\n");
                break;
        case UPNP_CONTROL_GET_VAR_REQUEST:
                APP_LOG("Common",LOG_DEBUG, "UPNP_CONTROL_GET_VAR_REQUEST\n");
                break;
        case UPNP_CONTROL_GET_VAR_COMPLETE:
                APP_LOG("Common",LOG_DEBUG, "UPNP_CONTROL_GET_VAR_COMPLETE\n");
                break;
        /* GENA */
        case UPNP_EVENT_SUBSCRIPTION_REQUEST:
                APP_LOG("Common",LOG_DEBUG, "UPNP_EVENT_SUBSCRIPTION_REQUEST\n");
                break;
        case UPNP_EVENT_RECEIVED:
                APP_LOG("Common",LOG_DEBUG, "UPNP_EVENT_RECEIVED\n");
                break;
        case UPNP_EVENT_RENEWAL_COMPLETE:
                APP_LOG("Common",LOG_DEBUG, "UPNP_EVENT_RENEWAL_COMPLETE\n");
                break;
        case UPNP_EVENT_SUBSCRIBE_COMPLETE:
                APP_LOG("Common",LOG_DEBUG, "UPNP_EVENT_SUBSCRIBE_COMPLETE\n");
                break;
        case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
                APP_LOG("Common",LOG_DEBUG, "UPNP_EVENT_UNSUBSCRIBE_COMPLETE\n");
                break;
        case UPNP_EVENT_AUTORENEWAL_FAILED:
                APP_LOG("Common",LOG_DEBUG, "UPNP_EVENT_AUTORENEWAL_FAILED\n");
                break;
        case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
                APP_LOG("Common",LOG_DEBUG, "UPNP_EVENT_SUBSCRIPTION_EXPIRED\n");
                break;
        }
}

int Util_PrintEvent(Upnp_EventType EventType, void *Event)
{
        ithread_mutex_lock(&display_mutex);

        APP_LOG("Common",LOG_DEBUG,
                "======================================================================\n"
                "----------------------------------------------------------------------\n");
        Util_PrintEventType(EventType);
        switch (EventType) {
        /* SSDP */
        case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
        case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
        case UPNP_DISCOVERY_SEARCH_RESULT: {
                struct Upnp_Discovery *d_event = (struct Upnp_Discovery *)Event;

                APP_LOG("Common",LOG_DEBUG, "ErrCode     =  %s(%d)\n",
                        UpnpGetErrorMessage(d_event->ErrCode), d_event->ErrCode);
                APP_LOG("Common",LOG_DEBUG, "Expires     =  %d\n",  d_event->Expires);
                APP_LOG("Common",LOG_DEBUG, "DeviceId    =  %s\n",  d_event->DeviceId);
                APP_LOG("Common",LOG_DEBUG, "DeviceType  =  %s\n",  d_event->DeviceType);
                APP_LOG("Common",LOG_DEBUG, "ServiceType =  %s\n",  d_event->ServiceType);
                APP_LOG("Common",LOG_DEBUG, "ServiceVer  =  %s\n",  d_event->ServiceVer);
                APP_LOG("Common",LOG_DEBUG, "Location    =  %s\n",  d_event->Location);
                APP_LOG("Common",LOG_DEBUG, "OS          =  %s\n",  d_event->Os);
                APP_LOG("Common",LOG_DEBUG, "Ext         =  %s\n",  d_event->Ext);
                break;
        }
        case UPNP_DISCOVERY_SEARCH_TIMEOUT:
                /* Nothing to print out here */
                break;
        /* SOAP */
        case UPNP_CONTROL_ACTION_REQUEST: {
                struct Upnp_Action_Request *a_event =
                        (struct Upnp_Action_Request *)Event;
                char *xmlbuff = NULL;

                APP_LOG("Common",LOG_DEBUG, "ErrCode     =  %s(%d)\n",
                        UpnpGetErrorMessage(a_event->ErrCode), a_event->ErrCode);
                APP_LOG("Common",LOG_DEBUG, "ErrStr      =  %s\n", a_event->ErrStr);
                APP_LOG("Common",LOG_DEBUG, "ActionName  =  %s\n", a_event->ActionName);
                APP_LOG("Common",LOG_DEBUG, "UDN         =  %s\n", a_event->DevUDN);
                APP_LOG("Common",LOG_DEBUG, "ServiceID   =  %s\n", a_event->ServiceID);
                if (a_event->ActionRequest) {
                        xmlbuff = ixmlPrintNode((IXML_Node *)a_event->ActionRequest);
                        if (xmlbuff) {
                                APP_LOG("Common",LOG_DEBUG, "ActRequest  =  %s\n", xmlbuff);
                                ixmlFreeDOMString(xmlbuff);
                        }
                        xmlbuff = NULL;
                } else {
                        APP_LOG("Common",LOG_DEBUG, "ActRequest  =  (null)\n");
                }
                if (a_event->ActionResult) {
                        xmlbuff = ixmlPrintNode((IXML_Node *)a_event->ActionResult);
                        if (xmlbuff) {
                                APP_LOG("Common",LOG_DEBUG, "ActResult   =  %s\n", xmlbuff);
                                ixmlFreeDOMString(xmlbuff);
                        }
                        xmlbuff = NULL;
                } else {
                        APP_LOG("Common",LOG_DEBUG, "ActResult   =  (null)\n");
                }
                break;
        }
        case UPNP_CONTROL_ACTION_COMPLETE: {
                struct Upnp_Action_Complete *a_event =
                        (struct Upnp_Action_Complete *)Event;
                char *xmlbuff = NULL;

                APP_LOG("Common",LOG_DEBUG, "ErrCode     =  %s(%d)\n",
                        UpnpGetErrorMessage(a_event->ErrCode), a_event->ErrCode);
                APP_LOG("Common",LOG_DEBUG, "CtrlUrl     =  %s\n", a_event->CtrlUrl);
                if (a_event->ActionRequest) {
                        xmlbuff = ixmlPrintNode((IXML_Node *)a_event->ActionRequest);
                        if (xmlbuff) {
                                APP_LOG("Common",LOG_DEBUG, "ActRequest  =  %s\n", xmlbuff);
                                ixmlFreeDOMString(xmlbuff);
                        }
                        xmlbuff = NULL;
                } else {
                        APP_LOG("Common",LOG_DEBUG, "ActRequest  =  (null)\n");
                }
                if (a_event->ActionResult) {
                        xmlbuff = ixmlPrintNode((IXML_Node *)a_event->ActionResult);
                        if (xmlbuff) {
                                APP_LOG("Common",LOG_DEBUG, "ActResult   =  %s\n", xmlbuff);
                                ixmlFreeDOMString(xmlbuff);
                        }
                        xmlbuff = NULL;
                } else {
                        APP_LOG("Common",LOG_DEBUG, "ActResult   =  (null)\n");
                }
                break;
        }
        case UPNP_CONTROL_GET_VAR_REQUEST: {
                struct Upnp_State_Var_Request *sv_event =
                        (struct Upnp_State_Var_Request *)Event;

                APP_LOG("Common",LOG_DEBUG, "ErrCode     =  %s(%d)\n",
                        UpnpGetErrorMessage(sv_event->ErrCode), sv_event->ErrCode);
                APP_LOG("Common",LOG_DEBUG, "ErrStr      =  %s\n", sv_event->ErrStr);
                APP_LOG("Common",LOG_DEBUG, "UDN         =  %s\n", sv_event->DevUDN);
                APP_LOG("Common",LOG_DEBUG, "ServiceID   =  %s\n", sv_event->ServiceID);
                APP_LOG("Common",LOG_DEBUG, "StateVarName=  %s\n", sv_event->StateVarName);
                APP_LOG("Common",LOG_DEBUG, "CurrentVal  =  %s\n", sv_event->CurrentVal);
                break;
        }
        case UPNP_CONTROL_GET_VAR_COMPLETE: {
                struct Upnp_State_Var_Complete *sv_event =
                        (struct Upnp_State_Var_Complete *)Event;

                APP_LOG("Common",LOG_DEBUG, "ErrCode     =  %s(%d)\n",
                        UpnpGetErrorMessage(sv_event->ErrCode), sv_event->ErrCode);
                APP_LOG("Common",LOG_DEBUG, "CtrlUrl     =  %s\n", sv_event->CtrlUrl);
                APP_LOG("Common",LOG_DEBUG, "StateVarName=  %s\n", sv_event->StateVarName);
                APP_LOG("Common",LOG_DEBUG, "CurrentVal  =  %s\n", sv_event->CurrentVal);
                break;
        }
        /* GENA */
        case UPNP_EVENT_SUBSCRIPTION_REQUEST: {
                struct Upnp_Subscription_Request *sr_event =
                        (struct Upnp_Subscription_Request *)Event;

                APP_LOG("Common",LOG_DEBUG, "ServiceID   =  %s\n", sr_event->ServiceId);
                APP_LOG("Common",LOG_DEBUG, "UDN         =  %s\n", sr_event->UDN);
                APP_LOG("Common",LOG_DEBUG, "SID         =  %s\n", sr_event->Sid);
                break;
        }
        case UPNP_EVENT_RECEIVED: {
                struct Upnp_Event *e_event = (struct Upnp_Event *)Event;
                char *xmlbuff = NULL;

                APP_LOG("Common",LOG_DEBUG, "SID         =  %s\n", e_event->Sid);
                APP_LOG("Common",LOG_DEBUG, "EventKey    =  %d\n", e_event->EventKey);
                xmlbuff = ixmlPrintNode((IXML_Node *)e_event->ChangedVariables);
                APP_LOG("Common",LOG_DEBUG, "ChangedVars =  %s\n", xmlbuff);
                ixmlFreeDOMString(xmlbuff);
                xmlbuff = NULL;
                break;
        }
        case UPNP_EVENT_RENEWAL_COMPLETE: {
                struct Upnp_Event_Subscribe *es_event =
                        (struct Upnp_Event_Subscribe *)Event;

                APP_LOG("Common",LOG_DEBUG, "SID         =  %s\n", es_event->Sid);
                APP_LOG("Common",LOG_DEBUG, "ErrCode     =  %s(%d)\n",
                        UpnpGetErrorMessage(es_event->ErrCode), es_event->ErrCode);
                APP_LOG("Common",LOG_DEBUG, "TimeOut     =  %d\n", es_event->TimeOut);
                break;
        }
        case UPNP_EVENT_SUBSCRIBE_COMPLETE:
        case UPNP_EVENT_UNSUBSCRIBE_COMPLETE: {
                struct Upnp_Event_Subscribe *es_event =
                        (struct Upnp_Event_Subscribe *)Event;

                APP_LOG("Common",LOG_DEBUG, "SID         =  %s\n", es_event->Sid);
                APP_LOG("Common",LOG_DEBUG, "ErrCode     =  %s(%d)\n",
                        UpnpGetErrorMessage(es_event->ErrCode), es_event->ErrCode);
                APP_LOG("Common",LOG_DEBUG, "PublisherURL=  %s\n", es_event->PublisherUrl);
                APP_LOG("Common",LOG_DEBUG, "TimeOut     =  %d\n", es_event->TimeOut);
                break;
        }
        case UPNP_EVENT_AUTORENEWAL_FAILED:
        case UPNP_EVENT_SUBSCRIPTION_EXPIRED: {
                struct Upnp_Event_Subscribe *es_event =
                        (struct Upnp_Event_Subscribe *)Event;

                APP_LOG("Common",LOG_DEBUG, "SID         =  %s\n", es_event->Sid);
                APP_LOG("Common",LOG_DEBUG, "ErrCode     =  %s(%d)\n",
                        UpnpGetErrorMessage(es_event->ErrCode), es_event->ErrCode);
                APP_LOG("Common",LOG_DEBUG, "PublisherURL=  %s\n", es_event->PublisherUrl);
                APP_LOG("Common",LOG_DEBUG, "TimeOut     =  %d\n", es_event->TimeOut);
                break;
        }
        }
        APP_LOG("Common",LOG_DEBUG,
                "----------------------------------------------------------------------\n"
                "======================================================================\n"
                "\n\n\n");

        ithread_mutex_unlock(&display_mutex);

        return 0;
}

int Util_FindAndParseService(IXML_Document *DescDoc, const char *location,
        const char *serviceType, char **serviceId, char **eventURL, char **controlURL)
{
        unsigned int i;
        unsigned long length;
        int found = 0;
        int ret;
#ifdef OLD_FIND_SERVICE_CODE
#else /* OLD_FIND_SERVICE_CODE */
        unsigned int sindex = 0;
#endif /* OLD_FIND_SERVICE_CODE */
        char *tempServiceType = NULL;
        char *baseURL = NULL;
        const char *base = NULL;
        char *relcontrolURL = NULL;
        char *releventURL = NULL;
        IXML_NodeList *serviceList = NULL;
        IXML_Element *service = NULL;

        baseURL = Util_GetFirstDocumentItem(DescDoc, "URLBase");
        if (baseURL)
                base = baseURL;
        else
                base = location;
#ifdef OLD_FIND_SERVICE_CODE
        serviceList = Util_GetFirstServiceList(DescDoc);
#else /* OLD_FIND_SERVICE_CODE */
        for (sindex = 0;
             (serviceList = Util_GetNthServiceList(DescDoc , sindex)) != NULL;
             sindex++) {
                tempServiceType = NULL;
                relcontrolURL = NULL;
                releventURL = NULL;
                service = NULL;
#endif /* OLD_FIND_SERVICE_CODE */
                length = ixmlNodeList_length(serviceList);
                for (i = 0; i < length; i++) {
                        service = (IXML_Element *)ixmlNodeList_item(serviceList, i);
                        tempServiceType = Util_GetFirstElementItem(
                                (IXML_Element *)service, "serviceType");
                        if (tempServiceType && strcmp(tempServiceType, serviceType) == 0) {
                                *serviceId = Util_GetFirstElementItem(service, "serviceId");
                                relcontrolURL = Util_GetFirstElementItem(service, "controlURL");
                                releventURL = Util_GetFirstElementItem(service, "eventSubURL");
                                *controlURL = MALLOC(strlen(base) + strlen(relcontrolURL) + 1);
                                if (*controlURL) {
                                        ret = UpnpResolveURL(base, relcontrolURL, *controlURL);
                                        if (ret != UPNP_E_SUCCESS)
                                                APP_LOG("Common",LOG_DEBUG, "Error generating controlURL from %s + %s\n",
                                                        base, relcontrolURL);
                                }
                                *eventURL = MALLOC(strlen(base) + strlen(releventURL) + 1);
                                if (*eventURL) {
                                        ret = UpnpResolveURL(base, releventURL, *eventURL);
                                        if (ret != UPNP_E_SUCCESS)
                                                APP_LOG("Common",LOG_DEBUG, "Error generating eventURL from %s + %s\n",
                                                        base, releventURL);
                                }

				if (relcontrolURL) {
					free(relcontrolURL);
					relcontrolURL = NULL;
				}
				if (releventURL) {
                                	free(releventURL);
	                                releventURL = NULL;
				}
                                found = 1;
                                break;
                        }
			if (tempServiceType) {
	                        free(tempServiceType);
        	                tempServiceType = NULL;
			}
                }
		if (tempServiceType) {
	                free(tempServiceType);
        	        tempServiceType = NULL;
		}
                if (serviceList)
                        ixmlNodeList_free(serviceList);
                serviceList = NULL;
#ifdef OLD_FIND_SERVICE_CODE
#else /* OLD_FIND_SERVICE_CODE */
        }
#endif /* OLD_FIND_SERVICE_CODE */
	if (baseURL)
	        free(baseURL);

        return found;
}



void FreeXmlSource(char* obj)
{
	if (obj) {
		free(obj);
		obj = NULL;
	}
}

void FreeResource(void* obj)
{
	if (0x00 != obj) {
		free(obj);
		obj = NULL;
	}
}

void ConvertCapabilityValue(char *o_string, char s_ch, char r_ch)
{
    char *pos = NULL;
    char *pBeg = NULL;

    int nPos = 0;

    pBeg = o_string;

    while( (pos = strchr(pBeg, s_ch)) )
    {
        *pos = r_ch;
        nPos = pos - pBeg + 1;
        pBeg = pBeg + nPos;
    }
}

