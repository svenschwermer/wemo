/***************************************************************************
*
*
* Common.h
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
#ifndef __COMMON_H
#define __COMMON_H

#define FALSE     0
#define TRUE      (!FALSE)

//Connection Modes
#define BROADCAST 0
#define UNICAST 1
/*HAN Commands Identifier*/
#define OP_H_COMMAND_BASE                    0

#define OP_H_RD_CONFIG                       OP_H_COMMAND_BASE+1
#define OP_H_WR_CONFIG                       OP_H_COMMAND_BASE+2
#define OP_H_RD_CONFIG_R                     OP_H_COMMAND_BASE+2

#define OP_H_RESET                           OP_H_COMMAND_BASE+3

#define OP_H_VER                             OP_H_COMMAND_BASE+4
#define OP_H_VER_R                           OP_H_COMMAND_BASE+5

#define OP_H_FORM                            OP_H_COMMAND_BASE+6
#define OP_H_LEAVE                           OP_H_COMMAND_BASE+7
#define OP_H_PJOIN                           OP_H_COMMAND_BASE+8

#define OP_H_ANNOUNCE                        OP_H_COMMAND_BASE+9

#define OP_H_NET_ST_Q                        OP_H_COMMAND_BASE+10
#define OP_H_NET_ST_R                        OP_H_COMMAND_BASE+11

#define OP_H_TX_CONFIRM                      OP_H_COMMAND_BASE+12

#define OP_H_IDENTIFY                        OP_H_COMMAND_BASE+13

#define OP_H_RD_PCTI                         OP_H_COMMAND_BASE+14
#define OP_H_RD_PCTI_R                       OP_H_COMMAND_BASE+15

#define OP_H_RD_PCTS                         OP_H_COMMAND_BASE+16
#define OP_H_RD_PCTS_R                       OP_H_COMMAND_BASE+17
#define OP_H_WR_PCTS                         OP_H_COMMAND_BASE+18

#define OP_H_RD_TEMP                         OP_H_COMMAND_BASE+19
#define OP_H_RD_TEMP_R                       OP_H_COMMAND_BASE+20

#define OP_H_RD_TIME                         OP_H_COMMAND_BASE+21
#define OP_H_RD_TIME_R                       OP_H_COMMAND_BASE+22
#define OP_H_WR_TIME                         OP_H_COMMAND_BASE+23

#define OP_H_DR                              OP_H_COMMAND_BASE+24
#define OP_H_DR_CANCEL                       OP_H_COMMAND_BASE+25
#define OP_H_DR_CAN_ALL                      OP_H_COMMAND_BASE+26
#define OP_H_DR_REP                          OP_H_COMMAND_BASE+27

#define OP_H_WR_PROFILE                      OP_H_COMMAND_BASE+28
#define OP_H_RD_PROFILE                      OP_H_COMMAND_BASE+29
#define OP_H_RD_PROFILE_R                    OP_H_COMMAND_BASE+30

#define OP_H_WR_METER                        OP_H_COMMAND_BASE+31

#define OP_H_WR_TOU                          OP_H_COMMAND_BASE+32

#define OP_H_MESSAGE                         OP_H_COMMAND_BASE+33
#define OP_H_MSG_CONFIRM                     OP_H_COMMAND_BASE+34
#define OP_H_MSG_CAN                         OP_H_COMMAND_BASE+35

#define OP_H_PUB_PRICE                       OP_H_COMMAND_BASE+36
#define OP_H_TIER_LABELS                     OP_H_COMMAND_BASE+37
#define OP_H_POLL                            OP_H_COMMAND_BASE+38
#define OP_H_POLL_R                          OP_H_COMMAND_BASE+39
#define OP_H_WR_METER_FORMATTING             OP_H_COMMAND_BASE+40
#define OP_H_WR_NETWORK_KEY                  OP_H_COMMAND_BASE+41

#define OP_H_RD_DEVICE_HEALTH                OP_H_COMMAND_BASE+42
#define OP_H_RD_DEVICE_HEALTH_R              OP_H_COMMAND_BASE+43

#define OP_H_DEV_COUNT                       OP_H_COMMAND_BASE+99

//------------New commands from V 1.7.0 ---- July 20, 2010
#define     OP_H_ADD_PAN_DEVICE                     OP_H_COMMAND_BASE + 44
#define     OP_H_DELETE_PAN_DEVICE                  OP_H_COMMAND_BASE + 45
#define     OP_H_DEVICES_TABLE_REQ              OP_H_COMMAND_BASE + 46
#define     OP_H_DEVICES_TABLE_RESP             OP_H_COMMAND_BASE + 47
#define     OP_H_DEVICE_STATUS_REQ              OP_H_COMMAND_BASE + 48
#define     OP_H_DEVICE_STATUS_RESP              OP_H_COMMAND_BASE + 49
#define     OP_H_DEVICE_EVENT_IND              OP_H_COMMAND_BASE + 50
#define     OP_H_WR_SECURITY_REQ          OP_H_COMMAND_BASE + 51
#define     OP_H_RD_SECURITY_REQ          OP_H_COMMAND_BASE + 52
#define     OP_H_RD_SECURITY_RESP            OP_H_COMMAND_BASE + 53
#define     OP_H_UTILITY_ERROR_IND              OP_H_COMMAND_BASE + 54

#define     OP_H_DRLC_ATTRIBUTE_REQ             OP_H_COMMAND_BASE + 55
#define     OP_H_DRLC_ATTRIBUTE_RESP            OP_H_COMMAND_BASE + 56

#define  OP_SERIAL_NO_READ_REQ                     OP_H_COMMAND_BASE + 57
#define  OP_SERIAL_NO_READ_RESP                    OP_H_COMMAND_BASE + 58
#define  OP_SERIAL_NO_WRITE_REQ                 OP_H_COMMAND_BASE + 59

#define OP_H_DRLC_W_ATTRIBUTE_REQ                   OP_H_COMMAND_BASE + 60
#define OP_H_CANCEL_JOINING                         OP_H_COMMAND_BASE + 61

/*Meter Commands Identifier*/
#define OP_M_COMMAND_BASE 100

#define OP_M_RD_CONFIG                       OP_M_COMMAND_BASE+1
#define OP_M_WR_CONFIG                       OP_M_COMMAND_BASE+2
#define OP_M_RD_CONFIG_R                     OP_M_COMMAND_BASE+2

#define OP_M_RESET                           OP_M_COMMAND_BASE+3

#define OP_M_VER                             OP_M_COMMAND_BASE+4

#define OP_M_VER_R                           OP_M_COMMAND_BASE+5

#define OP_M_JOIN                            OP_M_COMMAND_BASE+6
#define OP_M_LEAVE                           OP_M_COMMAND_BASE+7

#define OP_M_RD_TIME                         OP_M_COMMAND_BASE+8
#define OP_M_RD_TIME_R                       OP_M_COMMAND_BASE+9

#define OP_M_NET_ST_Q                        OP_M_COMMAND_BASE+10
#define OP_M_NET_ST_R                        OP_M_COMMAND_BASE+11

#define OP_M_DR_START                        OP_M_COMMAND_BASE+12
#define OP_M_DR_STOP                         OP_M_COMMAND_BASE+13
#define OP_M_DR_OVR                          OP_M_COMMAND_BASE+14

#define OP_M_MSG_START                       OP_M_COMMAND_BASE+15
#define OP_M_MSG_STOP                        OP_M_COMMAND_BASE+16
#define OP_M_MSG_CONF                        OP_M_COMMAND_BASE+17

#define OP_M_PRICE_START                     OP_M_COMMAND_BASE+18
#define OP_M_PRICE_STOP                      OP_M_COMMAND_BASE+19

#define OP_M_RD_METER                        OP_M_COMMAND_BASE+20
#define OP_M_RD_METER_R                      OP_M_COMMAND_BASE+21
#define OP_M_RD_METER_FORMATTING             OP_M_COMMAND_BASE+22
#define OP_M_RD_METER_FORMATTING_R           OP_M_COMMAND_BASE+23

#define OP_M_NETWORK_SURVEY                  OP_M_COMMAND_BASE+24
#define OP_M_NETWORK_SURVEY_R                OP_M_COMMAND_BASE+25

#define OP_M_RD_DRLC_CACHE                   OP_M_COMMAND_BASE+26
#define OP_M_RD_DRLC_CACHE_R                 OP_M_COMMAND_BASE+27
#define OP_M_RD_PRICE_CACHE                  OP_M_COMMAND_BASE+28
#define OP_M_RD_PRICE_CACHE_R                OP_M_COMMAND_BASE+29

#define OP_M_NETWORK_ERROR_REPORT            OP_M_COMMAND_BASE+30

//----------- new command ------------------------------------------------
#define OP_M_RD_INSTALLATION_CODE               OP_M_COMMAND_BASE + 31      //Read installation code of meter radio
#define OP_M_RD_INSTALLATION_CODE_R             OP_M_COMMAND_BASE + 32      //Installation code response, see data struct

#define OP_M_RD_SECURITY_OPTION                 OP_M_COMMAND_BASE + 33      //Read radio security option information
#define OP_M_RD_SECURITY_OPTION_R               OP_M_COMMAND_BASE + 34      //Radio security option response
#define OP_M_WR_SECURITY_OPTION                 OP_M_COMMAND_BASE + 35      //Write security option
#define OP_M_WR_DRLC_REPORT                     OP_M_COMMAND_BASE + 36       //DRLC user opt in  
#define OP_M_RD_METER_ATTRIBUTE_REQ             OP_M_COMMAND_BASE + 37
#define OP_M_RD_METER_ATTRIBUTE_RESP            OP_M_COMMAND_BASE + 38
#define OP_M_RD_METER_CONFIRM_MESSAGE           OP_M_COMMAND_BASE + 39
#define OP_M_RD_LAST_MESSAGE                    OP_M_COMMAND_BASE + 40
#define OP_M_RD_METER_FORMATTING_TEST_1         OP_M_COMMAND_BASE + 41
#define OP_M_RD_METER_FORMATTING_TEST_2         OP_M_COMMAND_BASE + 42

#define OP_M_RD_METER_TOU_REQ                   OP_M_COMMAND_BASE + 43
#define OP_M_RD_METER_TOU_RESP                  OP_M_COMMAND_BASE + 44

#define OP_M_RD_METER_STATUS_REQ                OP_M_COMMAND_BASE + 45
#define OP_M_RD_METER_STATUS_RESP               OP_M_COMMAND_BASE + 46

#define OP_M_RD_METER_READING_INFO_REQ          OP_M_COMMAND_BASE + 47
#define OP_M_RD_METER_READING_INFO_RESP         OP_M_COMMAND_BASE + 48

#define OP_M_RD_METER_HISTORICAL_CONSUMPTION_REQ          OP_M_COMMAND_BASE + 49
#define OP_M_RD_METER_HISTORICAL_CONSUMPTION_RESP          OP_M_COMMAND_BASE + 50

#define     OP_M_ERT_CONFIG_ERT                             OP_M_COMMAND_BASE + 51
#define     OP_M_ERT_METER_INFO_REQ                             OP_M_COMMAND_BASE + 52
#define     OP_M_ERT_USAGE_IND                              OP_M_COMMAND_BASE  + 53
#define     OP_M_ERT_METER_INFO_RESP                            OP_M_COMMAND_BASE  + 54
#define     OP_M_DRLC_ATTRIBUTES_REQ                        OP_M_COMMAND_BASE  + 55
#define     OP_M_DRLC_ATTRIBUTES_RESP                       OP_M_COMMAND_BASE  + 56
#define     OP_M_RADIO_TYPE_REQ                             OP_M_COMMAND_BASE  + 57
#define     OP_M_RADIO_TYPE_RESP                            OP_M_COMMAND_BASE  + 58
#define     OP_M_ERT_STATUS_IND                             OP_M_COMMAND_BASE  + 59


//#define   OP_SERIAL_NO_READ_REQ                     OP_M_COMMAND_BASE + 51
//#define   OP_SERIAL_NO_READ_RESP                    OP_M_COMMAND_BASE + 52


/*Miscellaneous EDA commands */
#define MISC_EDA_BASE                        300

#define RD_INFO                              MISC_EDA_BASE+1
#define RD_INFO_R                         MISC_EDA_BASE+2
//#define   OP_SERIAL_NO_READ_REQ                     MISC_EDA_BASE + 3
//#define   OP_SERIAL_NO_READ_RESP                    MISC_EDA_BASE + 4
//#define   OP_SERIAL_NO_WRITE_REQ                 MISC_EDA_BASE + 5
#define OP_FIRMWARE_UPDATE_REQ                      MISC_EDA_BASE + 6
#define OP_FIRMWARE_UPDATE_RESP                     MISC_EDA_BASE + 7



/* Structure header corresponds to the Packet header and will be a part
 * of every command structure
 */

/* GPIO commands handled by EventDaemon */
#define GPIO_MSG_BASE                        400

#define GPIO_BUTTON_STATUS                   GPIO_MSG_BASE+1
#define GPIO_DR_STATUS                       GPIO_MSG_BASE+2
#define GPIO_METER_STATUS                    GPIO_MSG_BASE+3
#define GPIO_HAN_STATUS                      GPIO_MSG_BASE+4
#define GPIO_METER_ACTIVITY                  GPIO_MSG_BASE+5
#define GPIO_HAN_ACTIVITY                    GPIO_MSG_BASE+6
#define GPIO_UIQ_ACTIVITY                    GPIO_MSG_BASE+7

struct pktHeader {
   UINT16      magicNumber;
   UINT16      versionNum;
   UINT16      pktOpCode;
   UINT16      pktSeqNum;
   UINT16      pktSource;
   UINT16      pktDestination;
   UINT16      pktLength;
};

typedef struct pktHeader PACKET_HDR, *pPACKET_HDR;

/* Some useful common defines */
#define PACKET_SOURCE_APP              1
#define PACKET_SOURCE_HAN              2
#define PACKET_SOURCE_METER            3
#define PACKET_SOURCE_UIQ              4
#define PACKET_SOURCE_DAEMON           5
#define PACKET_SOURCE_GPIO             6

#define PACKET_DEST_APP                1
#define PACKET_DEST_HAN                2
#define PACKET_DEST_METER              3
#define PACKET_DEST_UIQ                4
#define PACKET_DEST_DAEMON             5
#define PACKET_DEST_GPIO               6

/* Port definitions */
#define PORT_DAEMON_TO_UIQ    58000
#define PORT_EDA_TO_DAEMON    666
#define PORT_DAEMON_TO_EDA    667
#define PORT_APP_TO_DAEMON    7778
#define PORT_DAEMON_TO_APP    8887

// EDA exit codes used for test modes
#define EXIT_TEST_TIMEOUT     198   // response within expected time
#define EXIT_ERT_FAILED       199   // Invalid manufacture date received

#define EXIT_ZIGBEE_SUCESS              0x00
#define EXIT_ZIGBEE_ESP_FAILED          201
#define EXIT_ZIGBEE_METER_FAILED        202

#define EXIT_ZIGBEE_METER_CHECK_FAILED  203
#define EXIT_ERT_METER_CHECK_FAILED     204

#define     MTP_ZIGBEE_CHECK    0x01
#define     MTP_ERT_CHECK       0x02


struct ledControlT
{
    UINT8       ledID;              //0x00 - Front Panel, 0x01 - Rear Panel, 0x02 - Both panel
    UINT8       redDutyCycle;     //
    UINT8       greenDutyCycle;
    UINT8       blueDutyCycle;
    UINT8       onTime;             //0.1 * onTime, 0xFFFF, forever
    UINT8       offTime;            //0.1 * offTime, 0xFFFF, forever
};

typedef struct ledControlT LedControl, *pLedControl;


void    ledProcessCommand(char* szCommand, LedControl* pControl);

extern int g_sHanNetworkSatus;
extern UINT16 MeterNetworkDown;

extern int g_bIsRequestRadioFirmware;
extern int g_iRadioFirmwareId;
extern int gErtType;

extern int     g_isRequestZigBeeCheck;
extern struct timeval tZigBeeCheckTimer;
extern struct timeval  tZigBeeMeterCheckTimer;

#define     ZIGBEE_METER            0x01
#define     ERT_METER               0x03
#define     EDA_ERT_METER_CONNECTED     0x01
#define     EDA_ERT_METER_DISCONNECTED  0x00

extern UINT8    g_iMeterType;       //-0x01, ZigBee, 0x03, Ert 

extern UINT8    g_bIsErtMeterConnected; //- 0x00 Ert not connected, 0x01 ERT connected


extern UINT8    g_bGetMeterFirstMsg;
extern UINT8    g_bGetESPFirstMsg;
extern UINT8    g_bIsEDALinkRemoved;
extern UINT8    g_bIsMeterReleased;

//- Change the HAN security option from 
extern int g_iHanSecurityMode;
extern int g_bIsHanSecurityMode;

//-ERT related
struct ertSetupT
{
    UINT32      ertType;
    UINT32      ertID;
};

typedef struct slotTypeInfo
{
    UINT8       slot[3];

} SlotTypeInfo;


typedef struct ertConnectionStatus
{
    UINT8   status;     /** 0x00: disconnected; 0x01 connected*/
}ErtStatusIndication;

typedef struct ertSetupT ErtSetup, *pErtSetup;

extern char *RadioTypeDesc[];
#define MTP_ESP_CHECK_TIMEOUT   5  //seconds

#endif

