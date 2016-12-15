/*
 * wemo_netcam_api.c
 *
 *  Created on: Nov 16, 2012
 *      Author: simonx
 */
#include <stdarg.h>
#include <stdio.h>

#include "wemo_netcam_api.h"
#include "wemo_netcam_ipc.h"

static int s_IsWeMoIntitialized         = 0x00;

static int s_IsWeMoClientInitialized    = 0x00;

int initWeMoNetCam(IPCClientCallback pCallback)
{
    if ( 0x00 == s_IsWeMoIntitialized)
    {
        RegisteredCallBack(pCallback);
		createIPCServer();
        s_IsWeMoIntitialized = 0x01;
    }
    else
    {
      printf("IPC: IPC server already started\n");
	  return 0x02;
    }

    return WEMO_NETCAM_SUCCESS;
}

int initWeMoNetCamClient()
{
  if ( 0x00 == s_IsWeMoClientInitialized)
  {
      //createIPCClient();
	  printf("WeMo IPC communication created\n");
      s_IsWeMoClientInitialized = 0x01;
  }
  else
  {
    return 0x02;
  }

      return WEMO_NETCAM_SUCCESS;
}

void Motion_Sense(int motion_state) {
  notifyMotion(0x01);
}

int APP_LOG(LOG_E_LEVEL logLevel, char *fmt, ...)
{
    va_list ap;
    char buf[200];
    int rc;

    va_start( ap, fmt );
    rc = vsnprintf(buf, 200, fmt, ap);
    va_end( ap );

    printf("[>>CAM<<]%s\n", buf);
    return rc;
}
