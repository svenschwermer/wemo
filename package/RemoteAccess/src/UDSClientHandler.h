/***************************************************************************
*
*
* UDSClientHandler.h
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

#ifndef __UDS_CLIENT_H__
#define __UDS_CLIENT_H__

void* UDS_ProcessRemoteData(void *arg);
int UDS_SendClientDataResponse(char *UDSClientData, int dataLen);

void UDS_SetNatData();
void UDS_SetRemoteEnableStatus();
void UDS_SetCurrentClientState();
void UDS_SetNTPTimeSyncStatus();
#ifndef _OPENWRT_
void UDS_pj_dst_data_os(int index, char *ltimezone, int dstEnable);
#endif
void UDS_SetTurnServerEnvIPaddr(char *turnServerEnvIPaddr);

int UDS_remoteAccessInit();
int UDS_pluginNatInitialized();
int UDS_invokeNatReInit(int initType);
int UDS_invokeNatDestroy();

#endif /*__UDS_CLIENT_H__*/
