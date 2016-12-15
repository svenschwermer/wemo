/***************************************************************************
*
* Created by Belkin International, Software Engineering on Aug 13, 2013
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "zigbee_api.h"
#include "subdevice.h"

//-----------------------------------------------------------------------------
/*


 ZigBee Cluster
================
    
    # level control cluster: 
    ## movetolevel command
        - level - INT8U
        - transitionTime - INT16U (1/10 sec)

        level : unsigned 8bit value of level that bulb will reach.
        transition time: unsigned 16bit value, tenth of a second.


    # color control cluster: 
    ## movetocolor command
        - colorX - INT16U (1.0 = 65535/65535)
        - colorY - INT16U (1.0 = 65535/65535)
        - transitionTime - INT16U (1/10 sec)

        ColorX and ColorY fields.: unsigned 16bit value, according to the CIE 1931 Color Space.
        transition time: unsigned 16bit value, tenth of a second.


 Endpoints, Clusters
=====================

    Endpoints:  mostly 1, max 255
    Clusters:   mostly 7,  


  * Multiple Endpoints Consideration:
    
    Device_1 ---+--- Endpoint_1 ---+--- Cluster A
     (EUID)     |                  |           
                |                  +--- Cluster B
                |
                |
                +--- Endpoint_2 ---+--- Cluster A
                |                  |
                |                  +--- Cluster C
                |                  |
                |                  +--- Cluster E
                |
                |
                +--- Endpoint_3 ---+--- Cluster A
                                   |
                                   +--- Cluster F

  * Quick approach 1:

    Device_1_1 ----- Endpoint_1 ---+--- Cluster A
                                   |
                                   +--- Cluster B

    Device_1_2 ----- Endpoint_2 ---+--- Cluster A
                                   |
                                   +--- Cluster C
                                   |
                                   +--- Cluster E

    Device_1_2 ----- Endpoint_2 ---+--- Cluster A
                                   |
                                   +--- Cluster F


 ZB_JOIN_LIST 
==============

        "/tmp/Belkin_settings/zbdjoinlist.lst"

        <node_id> <node_eui> <done>

         "%x"             MSB 
          |                |
          v                v
        408f 0022a3000000afa0 done
        afb7 0022a3000000afa0 done

*/
//-----------------------------------------------------------------------------












