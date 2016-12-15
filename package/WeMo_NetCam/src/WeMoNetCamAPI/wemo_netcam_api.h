/*
 * wemo_netcam_api.h
 *
 *  Created on: Nov 16, 2012
 *      Author: simonx
 */

#ifndef WEMO_NETCAM_API_H_
#define WEMO_NETCAM_API_H_

#define	       SENSOR_NOMOTION                         0x00
#define        SENSOR_MOTION                            0x01
#define        WEMO_NETCAM_SUCCESS                   0x00
#define        WEMO_NETCAM_UNSCCESS                  0x01

/**
 * @ Function: intWeMoNetCam
 * @ Description:
 *      to initialize the communication from NetCam and WeMo
 *      WEMO process should call it once-time when system initialization or before communication
 *      with WeMo process.
 * @ return
 *      if SUCCESS, 0x00 return; otherwise other value will return
 *      enum:
 *              - 0x01: initialization failure
 *              - 0x02: already initialized
 */

//typedef int (*IPCClientCallback)(void* msg);
typedef int (*IPCClientCallback)(int msg);

int initWeMoNetCam(IPCClientCallback pCallback);

/**
 * @ Function: initWeMoNetCamClient
 * @ Description:
 *      to initialize the communication from NetCam and WeMo
 *      NetCam process should call it once-time when system initialization or before communication
 *      with WeMo process.
 * @ return
 *      if SUCCESS, 0x00 return; otherwise other value will return
 *      enum:
 *              - 0x01: initialization failure
 *              - 0x02: already initialized
 */
int initWeMoNetCamClient();



/**
 *
 *
 *
 */
void Motion_Sense(int motion_state);

typedef enum {LOG_E_INFO, LOG_E_WARNING, LOG_E_ERROR, LOG_E_EXCEPTION} LOG_E_LEVEL;
int APP_LOG(LOG_E_LEVEL logLevel, char *fmt, ...);

#endif /* WEMO_NETCAM_API_H_ */
