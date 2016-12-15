/*
 * wemo_netcam_ipc.h
 *
 *  Created on: Nov 16, 2012
 *      Author: simonx
 *
 *
 * This header file consist of two parts, the IPC client and IPC server respectively
 * The IPC client will talk with IPC server, IPC client running in a stand-alone process and
 * IPC server running in a stand-alone process as well.
 * PLEASE note that only local socket AF_UNIX will be used for IPC communication for future extendability
 */

#ifndef WEMO_NETCAM_IPC_H_
#define WEMO_NETCAM_IPC_H_
#include "wemo_netcam_api.h"

#define         MSG_SENSOR_MOTION           0x01        //- From NetCam->WeMo
#define         MSG_SENSOR_NO_MOTION        0x00        //- From NetCam->WeMo
#define         MSG_SENSITIVITY_WRITE_REQ   0x02        //- From WeMo->NetCam
#define         MSG_SENSITIVITY_RED_REQ     0x03        //- From WeMo->NetCam
#define         SOCKET_ID                   int

typedef struct __message_
{
    int         ID;             //- ID
    char*       payload;        //- buffer for the real message
    int         size;           //- the size of the payload

}MESSAGE_T;


int SIZE_MSG(MESSAGE_T* pMsg);

void            createIPCClient();
void*           IPCClientTask(void*);

void            IPCClientRegisterCallback(IPCClientCallback pCallback);

/*********************************************************************************/
int             IPCSendMsg(SOCKET_ID socket_id, char* pData, int size);

/********************************************************************************/
void            createIPCServer();
void*           IPCServerTask(void*);

int notifyMotion(int state);

int RegisteredCallBack(IPCClientCallback pCallBack);
#endif /* WEMO_NETCAM_IPC_H_ */
