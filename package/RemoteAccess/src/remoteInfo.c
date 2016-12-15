/***************************************************************************
*
*
* remoteInfo.c
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>

#include "mxml.h"
#include "defines.h"
#include "logger.h"
#include "remoteAccess.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

mxml_node_t *gSmartDevTree = NULL;

int loadSmartDevXml() {
	FILE *fps = NULL;
	fps = fopen(SMARTDEVXML, "r");
	if (!fps) {
		APP_LOG("REMOTEACCESS", LOG_ERR,"file error r\n");
		fps = fopen(SMARTDEVXML, "w+");
		if (!fps) {
			APP_LOG("REMOTEACCESS", LOG_ERR,"file errori w\n");
			return PLUGIN_FAILURE;
		}
	}
	if (gSmartDevTree) {
		mxmlDelete(gSmartDevTree);
		gSmartDevTree = NULL;
	}
	gSmartDevTree = mxmlLoadFile(NULL, fps, MXML_OPAQUE_CALLBACK);
	if (gSmartDevTree) {
		APP_LOG("REMOTEACCESS", LOG_DEBUG,"gSmartDevTree not null\n");
		char *check = mxmlSaveAllocString(gSmartDevTree, MXML_NO_CALLBACK);
		APP_LOG("REMOTEACCESS", LOG_HIDE,"gSmartDevTree after save and load %s\n", check);
		free(check);
		check = NULL;
		fclose(fps);
		return PLUGIN_SUCCESS;
	}else {
		APP_LOG("REMOTEACCESS", LOG_ERR,"gSmartDevTree null\n");
		fclose(fps);
		return PLUGIN_FAILURE;//2
	}
	if (fps) fclose(fps);
	return PLUGIN_FAILURE;
}

int saveSmartDevXml(void *xmlstr) {
	FILE *fps = NULL;
	mxml_node_t *smarttree = NULL;
	int ret = PLUGIN_FAILURE;

	smarttree = (mxml_node_t*)xmlstr;
	if (smarttree) {
		fps = fopen(SMARTDEVXML, "w+");
		if (!fps) {
			APP_LOG("REMOTEACCESS", LOG_ERR,"file errori w\n");
			ret = PLUGIN_FAILURE;
			return ret;
		}
		ret = mxmlSaveFile(smarttree, fps, MXML_NO_CALLBACK);
		fclose(fps);
		ret = loadSmartDevXml();
	}else {
		ret = PLUGIN_FAILURE;
	}
	return ret;
}

int createSmartXmlIfno(void *smartDev) {
	int ret = PLUGIN_FAILURE;
	char *tempTree = NULL;
	mxml_node_t *xml;
	mxml_node_t *device_node;
	mxml_node_t *top_node,*add_node;// *add_nodeid,*add_nodekey,*add_nodename;
	smartDevDets *dets = (smartDevDets*)smartDev;

	if (!dets) return PLUGIN_FAILURE;

	xml = mxmlNewXML("1.0");
	if(xml) {
		top_node = mxmlNewElement(xml, "smartDevicelist");
		if (top_node) {
			device_node = mxmlNewElement(top_node, "smartDevice");
			if(device_node) {
				add_node = mxmlNewElement(device_node, "smartId");
				mxmlNewOpaque(add_node, dets->smartId);
				add_node = mxmlNewElement(device_node, "smartKey");
				mxmlNewOpaque(add_node, dets->smartKey);
				add_node = mxmlNewElement(device_node, "smartName");
				mxmlNewOpaque(add_node, dets->smartName);
			}else {
				ret = PLUGIN_FAILURE;
				goto on_return;
			}
		}else {
			ret = PLUGIN_FAILURE;
			goto on_return;
		}
		tempTree = mxmlSaveAllocString(xml, MXML_NO_CALLBACK);
		APP_LOG("REMOTEACCESS", LOG_HIDE,"!gSmartDevTree alloc string is %s\n", tempTree);
		free(tempTree);
		tempTree = NULL;
		ret = saveSmartDevXml(xml);
		if (ret == PLUGIN_SUCCESS) {
			ret = PLUGIN_SUCCESS;
			goto on_return;
		}else {
			ret = PLUGIN_FAILURE;
			goto on_return;
		}
	}else {
		ret = PLUGIN_FAILURE;
		goto on_return;
	}
on_return:
	if (xml) mxmlDelete(xml);
	return ret;
}

int checkSmartEntry(void *smartDev) {
	int found = 0, retVal = PLUGIN_FAILURE;
	mxml_node_t *find_node;
	mxml_node_t *top_node;
	smartDevDets *dets = (smartDevDets*)smartDev;

	if (!dets) return PLUGIN_FAILURE;
	if (gSmartDevTree) {
		top_node = mxmlFindElement(gSmartDevTree, gSmartDevTree, "smartDevicelist", NULL, NULL, MXML_DESCEND);
		find_node = top_node;
		while (find_node) {
			find_node = mxmlWalkNext(find_node, top_node, MXML_DESCEND);
			if (find_node && (find_node->child)) {
				APP_LOG("REMOTEACCESS", LOG_HIDE,"The node %s with value is %s\n", find_node->value.element.name, find_node->child->value.opaque);
				if (!strcmp(find_node->value.element.name, "smartId")) {
					if (!strcmp(find_node->child->value.opaque, dets->smartId)) {
						APP_LOG("REMOTEACCESS", LOG_HIDE,"Match found for the node %s with value is %s\n", find_node->value.element.name, find_node->child->value.opaque);
						found = 1;
						break;
					}
				}
			}
		}
		if (found == 1) {
			APP_LOG("REMOTEACCESS", LOG_ERR,"Match found so returning 0\n");
			retVal = PLUGIN_SUCCESS;
			goto on_return;
		}else {
			APP_LOG("REMOTEACCESS", LOG_ERR,"Match not found so returning 1\n");
			retVal = (PLUGIN_SUCCESS+1);
			goto on_return;
		}
	}else {
		retVal = PLUGIN_FAILURE;
		goto on_return;
	}
on_return:
	return retVal;
}

int updateSmartXml(void *smartDev) {
	int retVal = PLUGIN_FAILURE, found = 0;
	mxml_node_t *first_node, *chNode;
	mxml_node_t *top_node;
	mxml_index_t *node_index=NULL;

	smartDevDets *dets = (smartDevDets*)smartDev;
	if (!dets) return PLUGIN_FAILURE;

	if (gSmartDevTree) {
		APP_LOG("REMOTEACCESS", LOG_DEBUG,"\nyes gSmartDevTree\n");
		top_node = mxmlFindElement(gSmartDevTree, gSmartDevTree, "smartDevicelist", NULL, NULL, MXML_DESCEND);
		node_index = mxmlIndexNew(top_node,"smartDevice", NULL);
		if (!node_index) {
			retVal = PLUGIN_FAILURE;
			goto out;
		}
		APP_LOG("REMOTEACCESS", LOG_DEBUG,"\nyes gSmartDevTree 1\n");
		first_node = mxmlIndexReset(node_index);
		if (!first_node) {
			retVal = PLUGIN_FAILURE;
			goto out;
		}
		APP_LOG("REMOTEACCESS", LOG_DEBUG,"\nyes gSmartDevTree 2\n");
		while (first_node != NULL) {
			first_node = mxmlIndexFind(node_index,"smartDevice", NULL);
			if (first_node) {
				chNode = mxmlFindElement(first_node, gSmartDevTree, "smartId", NULL, NULL, MXML_DESCEND);
				if (chNode && chNode->child) {
					APP_LOG("REMOTEACCESS", LOG_HIDE,"The node %s with value is %s\n", chNode->value.element.name, chNode->child->value.opaque);
					if (!strcmp(chNode->child->value.opaque, dets->smartId)) {
						chNode = mxmlFindElement(first_node, gSmartDevTree, "smartKey", NULL, NULL, MXML_DESCEND);
						if (chNode && chNode->child) {
							APP_LOG("REMOTEACCESS", LOG_HIDE,"The node %s with value is %s\n", chNode->value.element.name, chNode->child->value.opaque);
							mxmlSetOpaque(chNode->child, dets->smartKey);
							APP_LOG("REMOTEACCESS", LOG_HIDE,"The node %s with value is %s\n", chNode->value.element.name, chNode->child->value.opaque);
						}
						chNode = mxmlFindElement(first_node, gSmartDevTree, "smartName", NULL, NULL, MXML_DESCEND);
						if (chNode && chNode->child) {
							APP_LOG("REMOTEACCESS", LOG_DEBUG,"The node %s with value is %s\n", chNode->value.element.name, chNode->child->value.opaque);
							mxmlSetOpaque(chNode->child, dets->smartName);
							APP_LOG("REMOTEACCESS", LOG_DEBUG,"The node %s with value is %s\n", chNode->value.element.name, chNode->child->value.opaque);
						}
						found = 1;
						break;
					}
				}
			}
		}
		if (found == 1) {
			char *tempTree = mxmlSaveAllocString(gSmartDevTree, MXML_NO_CALLBACK);
			APP_LOG("REMOTEACCESS", LOG_HIDE,"!gSmartDevTree alloc string is %s\n", tempTree);
			free(tempTree);
			retVal = saveSmartDevXml(gSmartDevTree);
		}
	}
out:
	if (node_index) mxmlIndexDelete(node_index);
	return retVal;
}

int addSmartXml(void *smartDev) {
	int retVal = PLUGIN_FAILURE;
	mxml_node_t *device_node;
	mxml_node_t *top_node,*add_node;// *add_nodeid,*add_nodekey,*add_nodename;

	smartDevDets *dets = (smartDevDets*)smartDev;
	if (!dets) return PLUGIN_FAILURE;

	if (gSmartDevTree) {
		top_node = mxmlFindElement(gSmartDevTree, gSmartDevTree, "smartDevicelist", NULL, NULL, MXML_DESCEND);
		if (top_node) {
			device_node = mxmlNewElement(top_node, "smartDevice");
			if(device_node) {
				add_node = mxmlNewElement(device_node, "smartId");
				mxmlNewOpaque(add_node, dets->smartId);
				add_node = mxmlNewElement(device_node, "smartKey");
				mxmlNewOpaque(add_node, dets->smartKey);
				add_node = mxmlNewElement(device_node, "smartName");
				mxmlNewOpaque(add_node, dets->smartName);
				retVal = PLUGIN_SUCCESS;
			}else {
				retVal = PLUGIN_FAILURE;
			}
		}else {
			retVal = PLUGIN_FAILURE;
		}
		if (retVal == PLUGIN_SUCCESS) {
			char *tempTree = mxmlSaveAllocString(gSmartDevTree, MXML_NO_CALLBACK);
			APP_LOG("REMOTEACCESS", LOG_HIDE,"gSmartDevTree alloc string is %s\n", tempTree);
			free(tempTree);
			tempTree = NULL;
			retVal = saveSmartDevXml(gSmartDevTree);
			if (retVal == PLUGIN_SUCCESS) {
				retVal = PLUGIN_SUCCESS;
			}
		}
	}else {
		retVal = PLUGIN_FAILURE;
	}
	return retVal;
}

int numSmartDevInTree() {
	int n = 0;
	int retVal = PLUGIN_FAILURE;
	mxml_node_t *first_node;
	mxml_node_t *top_node;
	mxml_index_t *node_index;
	if (gSmartDevTree) {
		APP_LOG("REMOTEACCESS", LOG_DEBUG,"\nyes gSmartDevTree\n");
		top_node = mxmlFindElement(gSmartDevTree, gSmartDevTree, "smartDevicelist", NULL, NULL, MXML_DESCEND);
		node_index = mxmlIndexNew(top_node,"smartDevice", NULL);
		if (!node_index) {
			retVal = PLUGIN_FAILURE;
			goto out;
		}
		APP_LOG("REMOTEACCESS", LOG_DEBUG,"\nyes gSmartDevTree 1\n");
		first_node = mxmlIndexReset(node_index);
		if (!first_node) {
			retVal = PLUGIN_FAILURE;
			goto out;
		}
		APP_LOG("REMOTEACCESS", LOG_DEBUG,"\nyes gSmartDevTree 2\n");
		while (first_node != NULL) {
			first_node = mxmlIndexFind(node_index,"smartDevice", NULL);
			if (first_node) n++;
		}
		mxmlIndexDelete(node_index);
	}else {
		n = 0;
	}
out:
	return n;
}

smartDevDets* readSmartXml(int *count) {
	int retVal = PLUGIN_FAILURE, n = 0, i = 0;
	mxml_node_t *first_node, *chNode;
	mxml_node_t *top_node;
	mxml_index_t *node_index=NULL;

	n = numSmartDevInTree();
	APP_LOG("REMOTEACCESS", LOG_DEBUG,"Number of smart device %d\n", n);
	if (n <= 0) {
		retVal = PLUGIN_FAILURE;
		goto out;
	}

	smartDevDets *dets = (smartDevDets*)CALLOC(1, (sizeof(smartDevDets)*n));
	if (!dets) {
		retVal = PLUGIN_FAILURE;
		goto out;
	}
	*count = n;

	if (gSmartDevTree) {
		APP_LOG("REMOTEACCESS", LOG_DEBUG,"\nyes gSmartDevTree\n");
		top_node = mxmlFindElement(gSmartDevTree, gSmartDevTree, "smartDevicelist", NULL, NULL, MXML_DESCEND);
		node_index = mxmlIndexNew(top_node,"smartDevice", NULL);
		if (!node_index) {
			retVal = PLUGIN_FAILURE;
			goto out;
		}
		APP_LOG("REMOTEACCESS", LOG_DEBUG,"\nyes gSmartDevTree 1\n");
		first_node = mxmlIndexReset(node_index);
		if (!first_node) {
			retVal = PLUGIN_FAILURE;
			goto out;
		}
		APP_LOG("REMOTEACCESS", LOG_DEBUG,"\nyes gSmartDevTree 2\n");
		i = 0;
		while (first_node != NULL) {
			first_node = mxmlIndexFind(node_index,"smartDevice", NULL);
			if (first_node) {
				chNode = mxmlFindElement(first_node, gSmartDevTree, "smartId", NULL, NULL, MXML_DESCEND);
				if (chNode && chNode->child) {
					APP_LOG("REMOTEACCESS", LOG_HIDE,"The node %s with value is %s\n", chNode->value.element.name, chNode->child->value.opaque);
					strncpy(dets[i].smartId, chNode->child->value.opaque, sizeof(dets[i].smartId)-1);
				}
				chNode = mxmlFindElement(first_node, gSmartDevTree, "smartKey", NULL, NULL, MXML_DESCEND);
				if (chNode && chNode->child) {
					APP_LOG("REMOTEACCESS", LOG_HIDE,"The node %s with value is %s\n", chNode->value.element.name, chNode->child->value.opaque);
					strncpy(dets[i].smartKey, chNode->child->value.opaque, sizeof(dets[i].smartKey)-1);
				}
				chNode = mxmlFindElement(first_node, gSmartDevTree, "smartName", NULL, NULL, MXML_DESCEND);
				if (chNode && chNode->child) {
					APP_LOG("REMOTEACCESS", LOG_DEBUG,"The node %s with value is %s\n", chNode->value.element.name, chNode->child->value.opaque);
					strncpy(dets[i].smartName, chNode->child->value.opaque, sizeof(dets[i].smartName)-1);
				}
				++i;
			}
		}
		if (i > 0) {
			retVal = PLUGIN_SUCCESS;
		}
	}
out:
	if (node_index) mxmlIndexDelete(node_index);
	if (retVal == PLUGIN_FAILURE) {
		*count = 0;
		return NULL;
	}
	if (retVal == 0) {
		*count = n;
		return dets;
	}
	return NULL;
}

smartDevDets* readSmartXmlOne(char *smartId) {
	int retVal = PLUGIN_FAILURE, found = 0;
	mxml_node_t *first_node, *chNode;
	mxml_node_t *top_node;
	mxml_index_t *node_index=NULL;

	smartDevDets *dets = (smartDevDets*)CALLOC(1, (sizeof(smartDevDets)));
	if (!dets) {
		retVal = PLUGIN_FAILURE;
		goto out;
	}

	if (gSmartDevTree) {
		APP_LOG("REMOTEACCESS", LOG_DEBUG,"\nyes gSmartDevTree\n");
		top_node = mxmlFindElement(gSmartDevTree, gSmartDevTree, "smartDevicelist", NULL, NULL, MXML_DESCEND);
		node_index = mxmlIndexNew(top_node,"smartDevice", NULL);
		if (!node_index) {
			retVal = PLUGIN_FAILURE;
			goto out;
		}
		APP_LOG("REMOTEACCESS", LOG_DEBUG,"\nyes gSmartDevTree 1\n");
		first_node = mxmlIndexReset(node_index);
		if (!first_node) {
			retVal = PLUGIN_FAILURE;
			goto out;
		}
		APP_LOG("REMOTEACCESS", LOG_DEBUG,"\nyes gSmartDevTree 2\n");
		while (first_node != NULL) {
			first_node = mxmlIndexFind(node_index,"smartDevice", NULL);
			if (first_node) {
				chNode = mxmlFindElement(first_node, gSmartDevTree, "smartId", NULL, NULL, MXML_DESCEND);
				if (chNode && chNode->child) {
					if (!strcmp(chNode->child->value.opaque, smartId)){
						APP_LOG("REMOTEACCESS", LOG_HIDE,"The node %s with value is %s\n", chNode->value.element.name, chNode->child->value.opaque);
						strncpy(dets->smartId, chNode->child->value.opaque, sizeof(dets->smartId)-1);
						chNode = mxmlFindElement(first_node, gSmartDevTree, "smartKey", NULL, NULL, MXML_DESCEND);
						if (chNode && chNode->child) {
							APP_LOG("REMOTEACCESS", LOG_HIDE,"The node %s with value is %s\n", chNode->value.element.name, chNode->child->value.opaque);
							strncpy(dets->smartKey, chNode->child->value.opaque, sizeof(dets->smartKey)-1);
						}
						chNode = mxmlFindElement(first_node, gSmartDevTree, "smartName", NULL, NULL, MXML_DESCEND);
						if (chNode && chNode->child) {
							APP_LOG("REMOTEACCESS", LOG_DEBUG,"The node %s with value is %s\n", chNode->value.element.name, chNode->child->value.opaque);
							strncpy(dets->smartName, chNode->child->value.opaque, sizeof(dets->smartName)-1);
						}
						found = 1;
						break;
					}
				}
			}
		}
		if (found == 1) {
			retVal = PLUGIN_SUCCESS;
		}
	}
out:
	if (node_index) mxmlIndexDelete(node_index);
	if (retVal == PLUGIN_FAILURE) {
		return NULL;
	}
	if (retVal == PLUGIN_SUCCESS) {
		return dets;
	}
	return NULL;
}
