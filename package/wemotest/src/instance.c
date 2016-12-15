/***************************************************************************
*
*
*
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
/*
 * hookup.c
 *
 *  Created on: Oct 3, 2012
 *      Author: simon.xiang
 */

#include "instance.h"
#include "instanceIF.h"

/*
    Field   Size    Description
    Preamble        1byte   0xFE, the start of a command frame
    Command ID      4 bytes Identifier of request command or response result
    Result Code     4 bytes Identifier of request result, 0x0000 indicating success, 0xFFFF indicating invalid command
    Sequence Number 4 bytes Increment by one for each command, wrap to zero when it reaches the maximum value.
    Data Size       4 bytes The effective data size of request response, the value indicating the data size of "Data Content" in bytes
    Data Content    0-n bytes       Data content
    CheckSum        4 bytes CheckSum of entire data frame through "Preamble" to "Data Content"
*/

extern int SEQUENCE_NUMBER;

/**
 * Check the programming state of individual process
 *      @param src char* the process name to check
 *
 */
void executeProgrammingStatusReq(char* src, int size)
{
    char szUDPBuf[1024] = {0x00};
    int totalSize = 0x00;

    printf("execute: %s: %s process check\n", __FUNCTION__, src);
    if ((0x00 == src) || (0x00 == strlen(src)))
    {
        printf("No payload, please check request command\n");
        responseError(UDP_PROGRAMMING_STATE_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
        return;
    }
    char szCommand[64] = {0x00};
    sprintf(szCommand, "ps | grep %s", src);
    int rect = system(szCommand);
    if (0x00 == rect)
    {
        //- Good request
        szUDPBuf[0x00] = 0xFE;
        totalSize = calculateDataSize(0x00);
        createHeaderPayload(UDP_PROGRAMMING_STATE_RESP, 0x00, ++SEQUENCE_NUMBER, 0x00, szUDPBuf);
        executeResponse(szUDPBuf, totalSize);
    }
    else
    {
        //- bad request
        responseError(UDP_PROGRAMMING_STATE_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
    }
}

/**
 * Check bootloader information on the board
 * Please note that, it assumes the boot-loader information is stored already in
 * environment
 *
 */
void executeBootloaderReq(char* src, int size)
{
    printf("execute: %s\n", __FUNCTION__);

    char szUDPBuf[1024] = {0x00};
    int totalSize = 0x00;
    char* szCommand = " fw_printenv BootloaderInfo | cut -d'=' -f2>/tmp/wemotest.txt";
    int rect = system(szCommand);
    if (0x00 == rect)
    {
        //- Good request
    	char szScript[1024] = {0x00};
    	readScriptResult("/tmp/wemotest.txt", szScript, 1024, 0x00);
    	printf("Request boot-loader information success:%s\n", szScript);
        totalSize = calculateDataSize(strlen(szScript));
        createHeaderPayload(UDP_BOOTLOADER_VERSION_RESP, 0x00, ++SEQUENCE_NUMBER, strlen(szScript), szUDPBuf);
        createPayloadStr(szUDPBuf + PLAYLOAD_BODY_INDEX, szScript);
        executeResponse(szUDPBuf, totalSize);
    }
    else
    {
    	printf("Request boot-loader information failure\n");
    	responseError(UDP_BOOTLOADER_VERSION_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
    }
}


void executeFirmwareVersionReq(char* src, int size)
{
      printf("execute: %s\n", __FUNCTION__);
      char szUDPBuf[1024] = {0x00};
      int totalSize = 0x00;
      char* szCommand = "cat /etc/fw_version | grep \"Version: \" | cut -d' ' -f2>/tmp/wemotest.txt";
      int rect = system(szCommand);
      if (0x00 == rect)
      {
          //- Good request
          char szScript[1024] = {0x00};
          readScriptResult("/tmp/wemotest.txt", szScript, 1024, 0x00);
          printf("Request WiFi version success:%s\n", szScript);
          totalSize = calculateDataSize(strlen(szScript));
          createHeaderPayload(UDP_FIRMWARE_VERSION_RESP, 0x00, ++SEQUENCE_NUMBER, strlen(szScript), szUDPBuf);
          createPayloadStr(szUDPBuf + PLAYLOAD_BODY_INDEX, szScript);
          executeResponse(szUDPBuf, totalSize);
      }
      else
      {
          responseError(UDP_FIRMWARE_VERSION_RESP, 0xFFFF, ++SEQUENCE_NUMBER);

      }
}

void executeWiFiVersionReq(char* src, int size)
{
    printf("execute: %s\n", __FUNCTION__);
    char szUDPBuf[1024] = {0x00};
    int totalSize = 0x00;
    char* szCommand = "dmesg | grep \"Driver version:\" | cut -d' ' -f3>/tmp/wemotest.txt";
    int rect =  system("iwpriv ra0 show driverinfo");
    if (0x00 == rect)
    {
        //- Good request
        rect = system(szCommand);
        char szScript[1024] = {0x00};
        readScriptResult("/tmp/wemotest.txt", szScript, 1024, 0x00);
        printf("Request WiFi version success:%s\n", szScript);
        //- Here please wrap the code
        //- Good request
        totalSize = calculateDataSize(strlen(szScript));
         createHeaderPayload(UDP_WIFI_DRIVER_RESP, 0x00, ++SEQUENCE_NUMBER, strlen(szScript), szUDPBuf);
         createPayloadStr(szUDPBuf + PLAYLOAD_BODY_INDEX, szScript);
         executeResponse(szUDPBuf, totalSize);
    }
    else
    {
        //- Bad request
       printf("Request WiFi version failure\n");
       responseError(UDP_FIRMWARE_VERSION_RESP, 0xFFFF, ++SEQUENCE_NUMBER);

    }
}

void executeSoundCardVersionReq(char* src, int size)
{
  printf("execute: %s\n", __FUNCTION__);

      char szUDPBuf[1024] = {0x00};
      int totalSize = 0x00;

      char* szCommand = "cat /proc/asound/version>/tmp/wemotest.txt;\
          cat /usr/share/alsa/alsa.conf | grep defaults.pcm.dmix.rate>>/tmp/wemotest.txt;\
          amixer sget Mic | grep Mono:>>/tmp/wemotest.txt";
      int rect = system(szCommand);
      if (0x00 == rect)
      {
          //- Good request
          char szScript[1024] = {0x00};
          readScriptResult("/tmp/wemotest.txt", szScript, 1024, 0x01);
          printf("Read country code success:%s\n", szScript);
          //- Here please wrap the code
          //- Good request
          totalSize = calculateDataSize(strlen(szScript)) ;
          createHeaderPayload(UDP_SOUND_CARD_VERSION_RESP, 0x00, ++SEQUENCE_NUMBER, strlen(szScript), szUDPBuf);
          createPayloadStr(szUDPBuf + PLAYLOAD_BODY_INDEX, szScript);
          executeResponse(szUDPBuf, totalSize);
      }
      else
      {
          //- Bad request
         printf("Read sound card version failure");
         responseError(UDP_SOUND_CARD_VERSION_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
      }
}
void executeVideoCardVersionReq(char* src, int size)
{
    printf("execute: %s\n", __FUNCTION__);
    responseError(UDP_VIDEO_CARD_VERSION_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
}
void executeSerialNumberReq(char* src, int size)
{
    printf("execute: %s\n", __FUNCTION__);

   //"nvram_get SerialNumber | cut -d'=' -f2"
   char szUDPBuf[1024] = {0x00};
   int totalSize = 0x00;
   char* szCommand = "nvram_get SerialNumber | cut -d'=' -f2>/tmp/wemotest.txt";
   int rect = system(szCommand);
   if (0x00 == rect)
   {
       //- Good request
       char szScript[1024] = {0x00};
       readScriptResult("/tmp/wemotest.txt", szScript, 1024, 0x00);
       printf("Request serial number success:%s\n", szScript);
       totalSize = calculateDataSize(strlen(szScript)) ;
       createHeaderPayload(UDP_SERIAL_NUMBER_RESP, 0x00, ++SEQUENCE_NUMBER, strlen(szScript), szUDPBuf);
       createPayloadStr(szUDPBuf + PLAYLOAD_BODY_INDEX, szScript);
       executeResponse(szUDPBuf, totalSize);
   }
   else
   {
       //- Bad request
      printf("Request serial number failure\n");
      responseError(UDP_SERIAL_NUMBER_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
   }

}

void executeRA0MacReq(char* src, int size)
{
    printf("execute: %s\n", __FUNCTION__);
    char szUDPBuf[1024] = {0x00};
    int totalSize = 0x00;
    //- Here simulator just use eth2 to replace the ra0
    //char* szCommand = "ifconfig ra0 | grep \"inet addr\" | cut -d: -f2 | cut -d' ' -f1 >"; //- Command to request IP address
    //char* szCommand = "ifconfig ra0 | grep \"HWaddr \" | cut -d' ' -f12>macaddress.txt";
    char* szCommand = "ifconfig ra0 | grep \"HWaddr \" | cut -d' ' -f12>/tmp/wemotest.txt";
    int rect = system(szCommand);
    if (0x00 == rect)
    {
        //- Good request
        char szScript[1024] = {0x00};
        readScriptResult("/tmp/wemotest.txt", szScript, 1024, 0x00);
        printf("Request ra0 MAC success:%s\n", szScript);
        createHeaderPayload(UDP_WIFI_RA0_MAC_RESP, 0x00, ++SEQUENCE_NUMBER, strlen(szScript), szUDPBuf);
        totalSize = calculateDataSize(strlen(szScript));
        createPayloadStr(szUDPBuf + PLAYLOAD_BODY_INDEX, szScript);
        executeResponse(szUDPBuf, totalSize);
    }
    else
    {
        //- Bad request
       printf("Request ra0 MAC failure\n");
       responseError(UDP_WIFI_RA0_MAC_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
    }

}
void executeAPCLI0MacReq(char* src, int size)
{
    printf("execute: %s\n", __FUNCTION__);
    responseError(UDP_WIFI_APCLI0_MAC_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
}
void executeCoutryCodeReq(char* src, int size)
{
    printf("execute: %s\n", __FUNCTION__);

    char szUDPBuf[1024] = {0x00};
    int totalSize = 0x00;

    char* szCommand = "cat /etc/Wireless/RT2860/RT2860.dat | grep CountryCode | cut -d'=' -f2 >/tmp/wemotest.txt";
    int rect = system(szCommand);
    if (0x00 == rect)
    {
        //- Good request
        char szScript[1024] = {0x00};
        readScriptResult("/tmp/wemotest.txt", szScript, 1024, 0x00);
        printf("Read country code success:%s\n", szScript);
        //- Here please wrap the code
        //- Good request
        totalSize = calculateDataSize(strlen(szScript)) ;
        createHeaderPayload(UDP_WIFI_COUNTRY_CODE_RESP, 0x00, ++SEQUENCE_NUMBER, strlen(szScript), szUDPBuf);
        createPayloadStr(szUDPBuf + PLAYLOAD_BODY_INDEX, szScript);
        executeResponse(szUDPBuf, totalSize);
    }
    else
    {
        //- Bad request
       printf("Read country code failure\n");
       responseError(UDP_WIFI_COUNTRY_CODE_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
    }

}
void executeRegionCodeReq(char* src, int size)
{
    printf("execute: %s\n", __FUNCTION__);

     char szUDPBuf[1024] = {0x00};
     int totalSize = 0x00;

     char* szCommand = "cat /etc/Wireless/RT2860/RT2860.dat | grep CountryRegion= | cut -d'=' -f2>/tmp/wemotest.txt";
     int rect = system(szCommand);
     if (0x00 == rect)
     {
         //- Good request
         char szScript[1024] = {0x00};
         readScriptResult("/tmp/wemotest.txt", szScript, 1024, 0x00);
         printf("Read region code success:%s\n", szScript);
         //- Here please wrap the code
         //- Good request
         totalSize = calculateDataSize(strlen(szScript)) ;
         createHeaderPayload(UDP_WIFI_REGION_CODE_RESP, 0x00, ++SEQUENCE_NUMBER, strlen(szScript), szUDPBuf);
         createPayloadStr(szUDPBuf + PLAYLOAD_BODY_INDEX, szScript);
         executeResponse(szUDPBuf, totalSize);
     }
     else
     {
         //- Bad request
        printf("Read country code failure\n");
        responseError(UDP_WIFI_REGION_CODE_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
     }

}

void executeRa0IPAddressReq(char* src, int size)
{
    printf("execute: %s\n", __FUNCTION__);
    char szUDPBuf[1024] = {0x00};
    int totalSize = 0x00;
    //- Here simulator just use eth2 to replace the ra0
    char* szCommand = "ifconfig ra0 | grep \"inet addr\" | cut -d: -f2 | cut -d' ' -f1 >/tmp/wemotest.txt"; //- Command to request IP address
    int rect = system(szCommand);
    if (0x00 == rect)
    {
        //- Good request
        char szScript[1024] = {0x00};
        readScriptResult("/tmp/wemotest.txt", szScript, 1024, 0x00);
        printf("Request ra0 IP success:%s\n", szScript);
        totalSize = calculateDataSize(strlen(szScript)) ;
        createHeaderPayload(UDP_WIFI_RA0_IP_RESP, 0x00, ++SEQUENCE_NUMBER, strlen(szScript), szUDPBuf);
        createPayloadStr(szUDPBuf + PLAYLOAD_BODY_INDEX, szScript);
        executeResponse(szUDPBuf, totalSize);
    }
    else
    {
        //- Bad request
       printf("Request ra0 IP failure\n");
       responseError(UDP_WIFI_RA0_MAC_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
    }

}

void executeGPIOReadReq(char* src, int size)
{
  printf("execute: %s\n", __FUNCTION__);
  char szCommand[256] = {0x00};
  sprintf(szCommand, "cat /etc/proc/GPIO%s>/tmp/wemotest.txt", src);
  char szUDPBuf[1024] = {0x00};
  int totalSize = 0x00;

  int rect = system(szCommand);
    if (0x00 == rect)
    {
        //- Good request
        char szScript[1024] = {0x00};
        readScriptResult("/tmp/wemotest.txt", szScript, 1024, 0x00);
        printf("Request SSID success:%s\n", szScript);
        //- Here please wrap the code
        //- Good request
        totalSize = calculateDataSize(strlen(szScript));
        createHeaderPayload(UDP_GPIO_READ_RESP, 0x00, ++SEQUENCE_NUMBER, strlen(szScript), szUDPBuf);
        createPayloadStr(szUDPBuf + PLAYLOAD_BODY_INDEX, szScript);
        executeResponse(szUDPBuf, totalSize);
    }
    else
    {
        //- Bad request
       printf("Request SSID failure\n");
       responseError(UDP_GPIO_READ_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
    }
}

void executeGPIOWriteReq(char* src, int size)
{
     printf("execute: %s, param:%s\n", __FUNCTION__, src);

     if ((0x00 == src) || (strlen(src) <= 2))
     {
         printf("GPIO command parameters not valid, please check\n");
         responseError(UDP_GPIO_WRITE_REQ, 0x0001, ++SEQUENCE_NUMBER);
         return;
     }

     char szCommand[256] = {0x00};
     char szOnOff[2] = {0x00};
     int index = strlen(src) - 1;
     strncpy(szOnOff, src + index, 1);
     src[index] = 0x00;

     sprintf(szCommand, "echo %s>/etc/GPIO%s", szOnOff, src);

     printf("GPIO command: %s\n", szCommand);

     char szUDPBuf[1024] = {0x00};
     int totalSize = 0x00;

     int rect = system(szCommand);
     if (0x00 == rect)
     {
         totalSize = calculateDataSize(0x00);
         createHeaderPayload(UDP_GPIO_WRITE_REQ, 0x00, ++SEQUENCE_NUMBER, 0x00, szUDPBuf);
         executeResponse(szUDPBuf, totalSize);
     }
     else
     {
         //- Bad request
        printf("Request SSID failure\n");
        responseError(UDP_GPIO_WRITE_REQ, 0xFFFF, ++SEQUENCE_NUMBER);
     }
}

void executeAudioRecordingReq(char* src, int size)
{
   printf("execute: %s, param:%s\n", __FUNCTION__, src);

   char szCommand[256] = {0x00};
   sprintf(szCommand, "arecord -d %s -f S16_LE -r 8000 /tmp/file.wav", src);
   char szUDPBuf[1024] = {0x00};
   int totalSize = 0x00;

   int rect = system(szCommand);
   if (0x00 == rect)
   {
       system("mv /tmp/file.wav /tmp/Belkin_settings/file.wav");
       totalSize = calculateDataSize(0x00);
       createHeaderPayload(UDP_AUDIO_RECORDING_RESP, 0x00, ++SEQUENCE_NUMBER, 0x00, szUDPBuf);
       executeResponse(szUDPBuf, totalSize);
   }
   else
   {
       //- Bad request
      printf("Request SSID failure\n");
      responseError(UDP_AUDIO_RECORDING_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
   }
}

void executeSSIDReq(char* src, int size)
{
    printf("execute: %s\n", __FUNCTION__);
    char szUDPBuf[1024] = {0x00};
    int totalSize = 0x00;
    //- Here simulator just use eth2 to replace the ra0
    //char* szCommand = "ifconfig eth2 | grep \"inet addr\" | cut -d: -f2 | cut -d' ' -f1 >/tmp/wemotest.txt"; //- Command to request IP address
    char* szCommand = "iwconfig ra0 | grep ESSID | cut -d'\"' -f2>/tmp/wemotest.txt";

    int rect = system(szCommand);
    if (0x00 == rect)
    {
        //- Good request
        char szScript[1024] = {0x00};
        readScriptResult("/tmp/wemotest.txt", szScript, 1024, 0x00);
        printf("Request SSID success:%s\n", szScript);
        //- Here please wrap the code
        //- Good request
        totalSize = calculateDataSize(strlen(szScript));
        createHeaderPayload(UDP_WIFI_SSID_RESP, 0x00, ++SEQUENCE_NUMBER, strlen(szScript), szUDPBuf);
        createPayloadStr(szUDPBuf + PLAYLOAD_BODY_INDEX, szScript);
        executeResponse(szUDPBuf, totalSize);
    }
    else
    {
        //- Bad request
       printf("Request SSID failure\n");
       responseError(UDP_WIFI_SSID_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
    }
}

void executeWriteMaclReq(char* src, int size)
{
    printf("execute: %s: new mac value:%s\n", __FUNCTION__, src);
    char szUDPBuf[1024] = {0x00};
    int totalSize = 0x00;
    
    //- XX:XX:XX:XX:XX:XX
    char szCommand[64] = {0x00};
    char szMacValue[4] = {0x00};
    //-
    szMacValue[0x00] = src[0x03];
    szMacValue[0x01] = src[0x04];
    szMacValue[0x02] = src[0x00];
    szMacValue[0x03] = src[0x01];
    
    memset(szCommand, 0x00, 64);
    sprintf(szCommand, "iwpriv ra0 e2p 4=%s", szMacValue);
    printf("command: %s\n", szCommand);
    system(szCommand);
    usleep(500);
    
    memset(szCommand, 0x00, 64);
    memset(szMacValue, 0x00, 4);
    szMacValue[0x00] = src[0x09];
    szMacValue[0x01] = src[10];
    szMacValue[0x02] = src[0x06];
    szMacValue[0x03] = src[0x07];
    
    sprintf(szCommand, "iwpriv ra0 e2p 6=%s", szMacValue);
    printf("command: %s\n", szCommand);
    system(szCommand);
    usleep(500);

    memset(szCommand, 0x00, 64);
    memset(szMacValue, 0x00, 4);
    szMacValue[0x00] = src[15];
    szMacValue[0x01] = src[16];
    szMacValue[0x02] = src[12];
    szMacValue[0x03] = src[13];
    
    sprintf(szCommand, "iwpriv ra0 e2p 8=%s", szMacValue);
    printf("command: %s\n", szCommand);
    system(szCommand);
    usleep(500);

    totalSize = calculateDataSize(0x00);
    createHeaderPayload(UDP_WIFI_RA0_MAC_CHANGE_RESP, 0x00, ++SEQUENCE_NUMBER, 0x00, szUDPBuf);
    //createPayloadStr(szUDPBuf + PLAYLOAD_BODY_INDEX, szScript);

    executeResponse(szUDPBuf, totalSize);
}

void executeWriteSerialReq(char* src, int size)
{
    printf("execute: %s\n", __FUNCTION__);
    char szUDPBuf[1024] = {0x00};
    int totalSize = 0x00;
    //- Here simulator just use eth2 to replace the ra0
    //char* szCommand = "ifconfig eth2 | grep \"inet addr\" | cut -d: -f2 | cut -d' ' -f1 >/tmp/wemotest.txt"; //- Command to request IP address
    //char* szCommand = "nvram_get SerialNumber | cut -d'=' -f2>/tmp/wemotest.txt";
    char szCommand[256] = {0x00};
    sprintf(szCommand, "nvram_set SerialNumber %s", src);

    int rect = system(szCommand);
    if (0x00 == rect)
    {
        //- Good request
        memset(szCommand, 0x00, 256);
        strcpy(szCommand, "nvram_get SerialNumber | cut -d'=' -f2>/tmp/wemotest.txt");
        rect = system(szCommand);
        char szScript[1024] = {0x00};
        readScriptResult("/tmp/wemotest.txt", szScript, 1024, 0x00);
        printf("Write sierial success:%s\n", szScript);
        //- Here please wrap the code
        //- Good request
        totalSize = calculateDataSize(strlen(szScript));
        createHeaderPayload(UDP_WIFI_WRITE_SERIAL_RESP, 0x00, ++SEQUENCE_NUMBER, strlen(szScript), szUDPBuf);
        createPayloadStr(szUDPBuf + PLAYLOAD_BODY_INDEX, szScript);
        executeResponse(szUDPBuf, totalSize);
    }
    else
    {
        //- Bad request
       printf("Write serial number failure\n");
       responseError(UDP_WIFI_WRITE_SERIAL_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
    }

}

/*
     #define         SEQ_LEN         4
     #define         RESULT_LEN      4
     #define         COMMAND_LEN     4
     #define         FIX_LEN         1
 *
 */
/**
 *
 * @param playload
 * @param playloadindicator
 * @return
 */
int calculateDataSize(int palyloadlen)
{
    return (FIX_LEN + COMMAND_LEN + RESULT_LEN + SEQ_LEN + PAYLOAD_LEN + palyloadlen);
}

/**
 * The uniform interface for report bad command or invalid command
 *
 *
 *
 */
void responseError(int commandID, int responseCode, int SEQ)
{
  printf("UDP error report: command: %04X, RESP: %04X, SEQ: %04X\n", commandID, responseCode, SEQ);
  char szUDP[1024] = {0x00};
  createHeaderPayload(commandID, responseCode, SEQ, 0x00, szUDP);
  int  totalSize = calculateDataSize(0x00);
  printHex();
  executeResponse(szUDP, totalSize);
}



void ConnectWiFi(char* src, int size)
{
    printf("execute:%s, load: %s, size:%d\n", __FUNCTION__, src, size);
    if ((0x00 == src) || (0x00 == strlen(src)))
    {
        printf("Connect WiFi parameter failure\n");
        responseError(UDP_WFI_CONNECT_WIFI_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
        return;
    }

    //- Do a site survey

    char* szCommand = "iwpriv ra0 set SiteSurvey=1";

    int ret = system(szCommand);
    sleep(5);

    char szWiFiCommand[1024] = {0x00};
    //- Get the open network channel information
    sprintf(szWiFiCommand, "iwpriv ra0 get_site_survey | grep \"%s\" | cut -d' ' -f1>/tmp/wemotest.txt", src);
    printf("WiFi command: %s\n", szWiFiCommand);

    ret = system(szWiFiCommand);

    if (0x00 != ret)
    {
        printf("WiFi command execute failure\n");
        responseError(UDP_WFI_CONNECT_WIFI_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
    }
    else
    {
        printf("WiFi command executed success\n");
        char szScript[1024] = {0x00};
        ret = system("cat /tmp/wemotest.txt");
        if (0x00 != ret)
        {
            printf("Can not find the target dump file: /tmp/wemotest.txt\n");
            responseError(UDP_WFI_CONNECT_WIFI_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
        }
        else
        {
            readScriptResult("/tmp/wemotest.txt", szScript, 1024, 0x00);

            if (0x00 == strlen(szScript))
            {
              printf("No channel information read\n");
              responseError(UDP_WFI_CONNECT_WIFI_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
            }

            else
            {
                printf("Channel info:%s\n", szScript);
                //- TODO: check channel inform
                memset(szWiFiCommand, 0x00, sizeof(szWiFiCommand));
                sprintf(szWiFiCommand, "iwpriv apcli0 set ApCliEnable=0;iwpriv apcli0 set Channel=%s;iwpriv ra0 set Channel=%s", szScript, szScript);
                printf("Pairing command: %s\n", szWiFiCommand);
                //ret = system(szWiFiCommand);
                sleep(1);

                printf("WiFi command start\n");

                ret = system("iwpriv apcli0 set ApCliEnable=0;iwpriv apcli0 set ApCliAuthMode=WPA2PSK;iwpriv apcli0 set ApCliEncrypType=AES");
                ret = system("iwpriv apcli0 set ApCliWPAPSK=12345678");

                memset(szWiFiCommand, 0x00, sizeof(szWiFiCommand));
                sprintf(szWiFiCommand, "iwpriv ra0 set Channel=%s;iwpriv apcli0 set Channel=%s, iwpriv apcli0 set ApCliSsid=%s", szScript, szScript, src);
                ret = system(szWiFiCommand);
                ret = system("iwpriv apcli0 set ApCliEnable=1");

                printf("WiFi command end!!\n");

                if (0x00 != ret)
                {
                    printf("No channel information read\n");
                    responseError(UDP_WFI_CONNECT_WIFI_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
                }
                else
                {
                    sleep(5);
                    //- TODO: add more check for IP address and connected
                    memset(szWiFiCommand, 0x00, sizeof(szWiFiCommand));
                    sprintf(szWiFiCommand, "iwconfig apcli0 | grep \"%s\"", src);
                    int resp = system(szWiFiCommand);
                    if (0x00 == resp)
                    {
                        char szUDPBuf[1024] = {0x00};
                        int totalSize = 0x00;
                        totalSize = calculateDataSize(0x00);
                        printf("Connection to %s success\n", src);
                        createHeaderPayload(UDP_WFI_CONNECT_WIFI_RESP, 0x00, ++SEQUENCE_NUMBER, strlen(szScript), szUDPBuf);
                        executeResponse(szUDPBuf, totalSize);
                    }
                    else
                    {
                        printf("Connection to %s failure\n", src);
                        responseError(UDP_WFI_CONNECT_WIFI_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
                    }
                }
            }
        }
    }

}

void executeGetFriendlyName(char* src, int size)
{
     char szUDPBuf[1024] = {0x00};
     int totalSize = 0x00;
     //- Here simulator just use eth2 to replace the ra0
     //char* szCommand = "ifconfig eth2 | grep \"inet addr\" | cut -d: -f2 | cut -d' ' -f1 >/tmp/wemotest.txt"; //- Command to request IP address
     char* szCommand = "nvram_get FriendlyName | cut -d'=' -f2>/tmp/wemotest.txt";

     int rect = system(szCommand);
     if (0x00 == rect)
     {
         //- Good request
         char szScript[1024] = {0x00};
         readScriptResult("/tmp/wemotest.txt", szScript, 1024, 0x00);
         printf("Request friendly name success:%s\n", szScript);
         //- Here please wrap the code
         //- Good request
         totalSize = calculateDataSize(strlen(szScript));
         createHeaderPayload(UDP_DEVICE_FRIENDLY_NAME_RESP, 0x00, ++SEQUENCE_NUMBER, strlen(szScript), szUDPBuf);
         createPayloadStr(szUDPBuf + PLAYLOAD_BODY_INDEX, szScript);
         executeResponse(szUDPBuf, totalSize);
     }
     else
     {
         //- Bad request
        printf("Request friendly name failure\n");
        responseError(UDP_DEVICE_FRIENDLY_NAME_RESP, 0xFFFF, ++SEQUENCE_NUMBER);
     }
}
