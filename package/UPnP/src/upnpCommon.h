/***************************************************************************
*
*
* common.h
*
* Copyright (c) 2012-2014 Belkin International, Inc. and/or its affiliates.
* All rights reserved.
*
* Permission to use, copy, modify, and/or distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
*
*
* THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
* INCIDENTAL, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
* RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
* NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH
* THE USE OR PERFORMANCE OF THIS SOFTWARE.
*
*
***************************************************************************/

#ifndef _UPNP_COMMON_H_
#define _UPNP_COMMON_H_

#include "ithread.h"
#include "ixml.h" /* for IXML_Document, IXML_Element */
#include "upnp.h" /* for Upnp_EventType */
#include "upnptools.h"

ithread_mutex_t display_mutex;

char *Util_GetElementValue(
        /*! [in] The DOM node from which to extract the value. */
        IXML_Element *element);

/*!
 * \brief Given a DOM node representing a UPnP Device Description Document,
 * this routine parses the document and finds the first service list
 * (i.e., the service list for the root device).  The service list
 * is returned as a DOM node list. The NodeList must be freed using
 * NodeList_free.
 *
 * \return The service list is returned as a DOM node list.
 */
IXML_NodeList *Util_GetFirstServiceList(
        /*! [in] The DOM node from which to extract the service list. */
        IXML_Document *doc);

/*!
 * \brief Given a document node, this routine searches for the first element
 * named by the input string item, and returns its value as a string.
 * String must be freed by caller using free.
 */
char *Util_GetFirstDocumentItem(
        /*! [in] The DOM document from which to extract the value. */
        IXML_Document *doc,
        /*! [in] The item to search for. */
        const char *item);

/*!
 * \brief Given a DOM element, this routine searches for the first element
 * named by the input string item, and returns its value as a string.
 * The string must be freed using free.
 */
char *Util_GetFirstElementItem(
        /*! [in] The DOM element from which to extract the value. */
        IXML_Element *element,
        /*! [in] The item to search for. */
        const char *item);

/*!
 * \brief Prints a callback event type as a string.
 */
void Util_PrintEventType(
        /*! [in] The callback event. */
        Upnp_EventType S);

/*!
 * \brief Prints callback event structure details.
 */
int Util_PrintEvent(
        /*! [in] The type of callback event. */
        Upnp_EventType EventType,
        /*! [in] The callback event structure. */
        void *Event);

/*!
 * \brief This routine finds the first occurance of a service in a DOM
 * representation of a description document and parses it.  Note that this
 * function currently assumes that the eventURL and controlURL values in
 * the service definitions are full URLs.  Relative URLs are not handled here.
 */
int Util_FindAndParseService (
        /*! [in] The DOM description document. */
        IXML_Document *DescDoc,
        /*! [in] The location of the description document. */
        const char *location,
        /*! [in] The type of service to search for. */
        const char *serviceType,
        /*! [out] The service ID. */
        char **serviceId,
        /*! [out] The event URL for the service. */
        char **eventURL,
        /*! [out] The control URL for the service. */
        char **controlURL);

/*!
 * \brief Prototype for displaying strings. All printing done by the device,
 * control point, and sample util, ultimately use this to display strings
 * to the user.
 */

/*!
 * \brief Releases Resources held by sample util.
 */




void FreeXmlSource(char* obj);
void FreeResource(void* obj);

void ConvertCapabilityValue(char *o_string, char s_ch, char r_ch);

typedef struct Upnp_Action_Request *pUPnPActionRequest;


#endif	//_COMMON_UTILS_H_
