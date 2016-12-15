/* 
*  Created by Belkin International, Software Engineering on XX/XX/XX.
* Copyright (c) 2012-2013 Belkin International, Inc. and/or its affiliates.
* All rights reserved.
*
* Belkin International, Inc. retains all right, title and interest (including
* all intellectual property rights) in and to this computer program, which is
* protected by applicable intellectual property laws.  Unless you have obtained
* a separate written license from Belkin International, Inc., you are not
* authorized to utilize all or a part of this computer program for any purpose
* (including reproduction, distribution, modification, and compilation into
* object code) and you must immediately destroy or return to Belkin
* International, Inc all copies of this computer program.  If you are
* licensed by Belkin International, Inc., your rights to utilize this
* computer program are limited by the terms of that license.
*
* To obtain a license, please contact Belkin International, Inc.
*
* This computer program contains trade secrets owned by Belkin International,
* Inc. and, unless unauthorized by Belkin International, Inc. in writing,
* you agree to maintain the confidentiality of this computer program and
* related information and to not disclose this computer program and related
* information to any other person or entity.
*
* THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND
* BELKIN INTERNATIONAL, INC. EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR
* IMPLIED, INCLUDING THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A
* PARTICULAR PURPOSE, TITLE, AND NON-INFRINGEMENT.
*
*
*  Hardware connections:
*     24 - P2.0/STE0    - (out) future SPI CS
*     23 - P1.7/UCLK0   - (out) future SCLK
*     22 - P1.6/SOMI0   - (out) future MISO
*     21 - P1.5/SIM00   - (out) future MOSI
*     20 - P1.4/URXD0   - (in)  serial receive data from WeMo Smart module
*     19 - P1.3/UTXD0   - (out) serial transmit data to WeMo Smart Module
*     18 - P1.2/TA0     - (in)  Low going pulses from flow meter (Timer_A3)
*     17 - P1.1         - (out)
*     16 - DVCC
*     15 - P2.7/XT2IN   - 12 Mhz Crystal
*     14 - P2.6/XT2OUT  - 12 Mhz Crystal
*     13 - DVSS
*     12 - P1.0         - (out) LED, 0 = off, 1 = on
*     11 - Reset
*     10 - test
*      9 - nc
*      8 - nc
*      7 - VREF
*      6 - VSS
*      5 - AVCC
*      4 - nc
*      3 - nc
*      2 - A0.0-
*      1 - A0.0+
***************************************************************************/

#include <msp430.h> 
#include <string.h>
#include <time.h>

#include "wasp_embedded.h"
#include "wasp.h"
#include "echo.h"

// define TEMP_SENSOR_TEST to use the internal temperature sensor
// for the A/D input rather than A0.0.  This allows the code
// to be debugged on the dev board without adding any analog
// hardware.
#define  TEMP_SENSOR_TEST     1

// Define this to send the A/D values to the console in as old ASCII
// #define  ASCII_SERIAL_OUTPUT  1

#define enable_interrupts() do { _EINT(); } while(0)
#define disable_interrupts() do { _DINT(); } while(0)

#ifndef FALSE
#define FALSE  0
#endif

#ifndef TRUE
#define TRUE   (!FALSE)
#endif

#define SMCLK  6000000  // SMCLK = MCLK / 2 = 6 Mhz

u8 gMsgReady;
u8 gRxOff;
u8 Ticker;  // Incremented on each A/D interrupt (244.14 hz/4.096 millseconds)

// The following timer is decremented once every .9436 seconds
u8 gWaspTimer;
#define WASP_TO   5  // about a 4 to 5 second timeout

#define TX_BUF_SIZE  48
u8  TxBuf[TX_BUF_SIZE];

#ifdef ASCII_SERIAL_OUTPUT
#include <stdio.h>

char *pAsciiOut;
u8 gNewSample;
u16 gPressure;
#endif

// Offsets in TxBuf to various items
#define  RESP_OFFSET    0
#define  RCODE_OFFSET   1
#define  DATA_OFFSET    2

// 32 bit Device ID, little endian order
const u8 DeviceID[4] = {0x1,0x0,0x2,0x0};
const char FirmwareVersion[] = "0.07";
const char DeviceDesc[] = "Echo";
const char WaspVersion[] = "1.06";
const u8 OverallState = 0x3;     // "On"
u8 WemoStatus;

const char PressureDesc[] = {
   WASP_VARTYPE_UINT16,
   WASP_USAGE_MONITORED,
   DESC_U16(WASP_POLL_NEVER),
   DESC_U16(0),DESC_U16(0xffff), // MinMaxU16
};

const char Pressure[] = "Pressure";
const char Min[] = "Min ";
const char Max[] = "Max ";

const char FlowDesc[] = {
   WASP_VARTYPE_UINT16,
   WASP_USAGE_MONITORED,
   DESC_U16(WASP_POLL_NEVER),
   DESC_U16(0),DESC_U16(0xffff), // MinMaxU16
   'F','l','o','w'
};

const char DataDesc[] = {
   WASP_VARTYPE_BLOB,
   WASP_USAGE_MONITORED,
   DESC_U16(WASP_ASYNC_UPDATE),
// MinMaxU32
   DESC_U32((long)sizeof(EchoData)),DESC_U32((long)sizeof(EchoData)), 
   'D','a','t','a'
};


/* Global variables */
EchoDataMsg gEvenMsg;   // Even data buffer
u8 gEvenSamples;        // Number of pressure samples in even buffer

EchoDataMsg gOddMsg;    // Odd data buffer
u8 gOddSamples;         // Number of pressure samples in odd buffer

#define SENDING_EVEN    1
#define SENDING_ODD     2
u8 gDataSendState;

u8 gTxLen;
u16 RecNum;

DebugMsg gDebugMsg;
#define gDebug gDebugMsg.Data

struct {
   u8 AsyncEnabled:1;
   u8 SendingEven:1;
   u8 SendingOdd:1;
   u8 TxMsgWaiting:1;
   u8 ChangeBaudRate:1;
   u8 HighBaudRate:1;
} gFlags;

/* Device Variables */
volatile u16 gLastPressure;
volatile u16 gFlowcount;

void turn_on_led(void)
{
   P1OUT |= BIT0;
}

void turn_off_led(void)
{
   P1OUT &= ~BIT0;
}

u8 CopyVar(u8 VarID);
void TxCopy(void *From,u8 Len);
u8 SendMsg(u8 *Msg,u8 MsgLen,u8 bAsync);
u8 SendEchoData(u8 Cmd);
void SetBaudRate(u16 Baudrate);

void main(void)
{
// shutoff watchdog
   WDTCTL = WDTPW +WDTHOLD;      // Stop Watchdog Timer

// setup ports
   P1DIR = BIT0 | BIT1 | BIT3 | BIT5 | BIT6 | BIT7;

// Special functions: 
//    p1.3 - UART - P1SEL.3 = 1, P1SEL2.3 = 0
//    p1.4 - UART - P1SEL.4 = 1, P1SEL2.4 = 0
// 
   P1SEL = BIT3 + BIT4;
   P1SEL2 = 0;
// Enable internal pullup resistor on P1.2
   P1OUT = BIT2;
   P1REN = BIT2;  // disable pullup/pulldown resistors

   P2DIR = BIT0;
// Special functions: 
//    p2.6 - XT2IN  - P2SEL.6 = 1
//    p2.7 - XT2OUT - P2SEL.7 = 1

   P2SEL = BIT6 + BIT7;
   P2SEL2 = 0;
   P2REN = 0;  // disable pullup/pulldown resistors

// Setup clock, enable 12 Mhz XT2 oscillator
   BCSCTL1 &= ~XT2OFF;
   BCSCTL1 |= XTS;
   BCSCTL3 |= XT2S_2;   // Mode 2 for XTS 2 -> 16 Mhz

// Wait for clock to stabilize
   do {
      IFG1 &= ~OFIFG;
      gLastPressure = 0xFF;
      while(gLastPressure--);
   } while (OFIFG & IFG1);

// Select XTS2/1 clock (12 Mhz) for MCLK, MCLK/2 for SMCLK (6 Mhz), 
   BCSCTL2 |= SELM_2 + DIVM_0 + DIVS_1+ SELS;

// Initialize global data
   gMsgReady = gLastPressure = gDebug.MaxPresure = gFlowcount = gEvenSamples = 
   gOddSamples = 0;
   gDebug.MinPresure = 0xffff;

// Initialize A/D converter
// 
// SD24CTL:
// We need 250 samples / second.  With a 1024 (the max) oversampling
// rate that means a A/D clock of 250 * 1024 = 256,000 Hz or 12Mhz / 46.875
// or 1 / 48 for an actual sampling rate of 244.14
// 
// 256000 Hz fM = MCLK / 12 = MCLK / 1 (SD24DIV) / 48 (SD24XDIV)
// MCLK = 0,0
// SD24DIV /1 = 0,0  = !SD24DIV1, !SD24DIV0
// SD24XDIVX / 48 = 1,1 = SD24XDIV1, SD24XDIV0

   SD24CTL = SD24XDIV0 | SD24XDIV1 | SD24REFON;

#ifndef TEMP_SENSOR_TEST

// SD24INCTL0:
// Input from A0.0 = 0,0,0 = = !SD24INCH2, !SD24INCH1, !SD24INCH0
// Setup analog front end for about 100 millivolts full scale
// Vfsr = (Vref/2) / Gain = .600 / Gain so we want a gain of 6.
// The closest that is available is 4 = 0,1,0 = !SD24GAIN0, SD24GAIN1, !SD24GAIN1
// This sets the full scale reading to 150 millivolts

   SD24INCTL0 = SD24GAIN1;
#else

// SD24INCTL0:
// Input from Temp sensor = 1,1,0 = SD24INCH2, SD24INCH1, !SD24INCH0
// Setup analog front end for about 600 millivolts full scale
// Vfsr = (Vref/2) / Gain = .600 / Gain so we want a gain of 1.
// 1  = 0,0,0

   SD24INCTL0 = SD24INCH2 | SD24INCH1;
#endif

// Unipolar A/D Output format
// Single channel, continuous conversions
// Enable interrupts
// Binary format
// oversampling ratio: 1024 = SD24XOSR, !SD24OSR1, SD24OSR0

   SD24CCTL0 = SD24UNI | SD24IE | SD24XOSR | SD24OSR0;

// Start the A/D conversions
   SD24CCTL0 |= SD24SC;

   U0CTL = SWRST;
   U0CTL |= CHAR;
   U0TCTL = SSEL1;   // Select SMCLK 

#ifdef ASCII_SERIAL_OUTPUT
   SetBaudRate(57600);
#else
   SetBaudRate(WASP_DEFAULT_BAUDRATE);
#endif
   U0MCTL = 0x00;
   U0CTL &= ~SWRST;
// Enable UART
   U0ME = UTXE0 | URXE0;

   WaspInit();

// Setup p1.2 to interrupt on the falling edge
   P1IES = BIT2;
   P1IE  = BIT2;

   IE1 = URXIE0;

#ifdef ASCII_SERIAL_OUTPUT
   gNewSample = FALSE;
   sprintf((char *) TxBuf,"\r\nEcho V%s\r\n",FirmwareVersion);
   pAsciiOut = (char *) TxBuf;
   IE1 |= UTXIE0;
#endif

   enable_interrupts();

   for( ; ; ) {
#ifdef ASCII_SERIAL_OUTPUT
      if(pAsciiOut == NULL) {
      // Last value has been sent
         if(gNewSample) {
            gNewSample = FALSE;
            for( ; ; ) {
               gPressure = gLastPressure;
               if(gPressure == gLastPressure) {
                  break;
               }
            }
            sprintf((char *) TxBuf,"\r%x",gPressure);
            pAsciiOut = (char *) TxBuf;
            TXBUF0 = *pAsciiOut++;
            IE1 |= UTXIE0;
         }
      }
#endif
      if(U0TCTL & TXEPT){
      // WASP transmitter is idle
         if(gFlags.SendingEven) {
         // Finished sending the even buffer
            gEvenSamples = 0;
            gFlags.SendingEven = FALSE;
         }
         else if(gFlags.SendingOdd) {
         // Finished sending the odd buffer
            gOddSamples = 0;
            gFlags.SendingOdd = FALSE;
         }

         if(gFlags.TxMsgWaiting) {
            gFlags.TxMsgWaiting = FALSE;
            SendMsg(TxBuf,gTxLen,FALSE);
         }

         if(WaspTxReady()) {
         // Kick off the serial output interrupt handler
            TXBUF0 = GetWaspTxData();
            IE1 |= UTXIE0;
         }
         else if(gFlags.ChangeBaudRate) {
            gFlags.ChangeBaudRate = FALSE;
            gFlags.HighBaudRate = TRUE;
            SetBaudRate(*((unsigned int *)&RxBuf[gRxOff]));
         }
      }

      if(gFlags.HighBaudRate && gWaspTimer == 0) {
      // WASP timeout, reset to the default baudrate and wait 
      // for communications with the WeMo SmartModule to
      // resume.
         gFlags.HighBaudRate = FALSE;
         gFlags.AsyncEnabled = FALSE;
         SetBaudRate(WASP_DEFAULT_BAUDRATE);
      }

      if(gMsgReady) {
         gWaspTimer = WASP_TO;
         gRxOff = CMD_OFFSET;
         gMsgReady = 0;
         gTxLen = 0;
         TxBuf[gTxLen++] = RxBuf[CMD_OFFSET] | WASP_CMD_RESP;
         TxBuf[gTxLen++] = 0;

         switch(RxBuf[gRxOff++]) {
            case WASP_CMD_NOP:
            // Reply to NOP command
               break;

            case WASP_CMD_GET_VALUE:
               switch(RxBuf[gRxOff]) {
                  case WASP_VAR_ECHO_DATA:
                     TxBuf[RCODE_OFFSET] = WASP_ERR_DATA_NOTRDY;
                     if(!gFlags.AsyncEnabled) {
                        SendEchoData(WASP_CMD_GET_VALUE);
                        gTxLen = 0;
                     }
                     break;

                  default:
                     CopyVar(RxBuf[gRxOff]);
                     break;
               }
               break;

            case WASP_CMD_GET_VALUES:
               while(gRxOff < RxMsgLen) {
                  if(CopyVar(RxBuf[gRxOff++]) == 0) {
                     break;
                  }
               }
               break;

            case WASP_CMD_GET_VAR_ATTRIBS:
               switch(RxBuf[ARG_OFFSET]) {
                  case WASP_VAR_PRESSURE:
                  case WASP_VAR_PRESSURE_MIN:
                  case WASP_VAR_PRESSURE_MAX:
                     TxCopy((void *)PressureDesc,sizeof(PressureDesc));
                     switch(RxBuf[ARG_OFFSET]) {
                        case WASP_VAR_PRESSURE_MIN:
                           TxCopy((void *)Min,sizeof(Min) - 1);
                           break;

                        case WASP_VAR_PRESSURE_MAX:
                           TxCopy((void *)Max,sizeof(Max) - 1);
                           break;

                        default:
                           break;
                     }

                     if(RxBuf[ARG_OFFSET] >= WASP_VAR_PRESSURE &&
                        RxBuf[ARG_OFFSET] <= WASP_VAR_PRESSURE_MAX) 
                     {
                        TxCopy((void *)Pressure,sizeof(Pressure) - 1);
                     }
                     break;

                  case WASP_VAR_FLOW:
                     TxCopy((void *)FlowDesc,sizeof(FlowDesc));
                     break;

                  case WASP_VAR_ECHO_DATA:
                     TxCopy((void *)DataDesc,sizeof(DataDesc));
                     break;

                  default:
                     TxBuf[RCODE_OFFSET] = WASP_ERR_ARG;
                     break;
               }
               break;

            case WASP_CMD_ASYNC_ENABLE:
               switch(RxBuf[ARG_OFFSET]) {
                  case 0:
                     gFlags.AsyncEnabled = FALSE;
                     break;

                  case 1:
                     gFlags.AsyncEnabled = TRUE;
                     break;

                  default:
                     TxBuf[RCODE_OFFSET] = WASP_ERR_ARG;
                     break;
               }
               break;

            case WASP_CMD_ASYNC_DATA:
               if(RxBuf[gRxOff] == WASP_VAR_ECHO_DATA) {
               // Special handling
                  if(!SendEchoData(WASP_CMD_ASYNC_DATA)) {
                     gTxLen = 0;
                  }
               }
               else {
               // Unknown variable, return an error
                  TxBuf[RCODE_OFFSET] = WASP_ERR_ARG;
               }
               break;

            case WASP_CMD_GET_BUTTONS:
            // Just return an empty list since we don't have any buttons.
               break;

            case WASP_CMD_SET_VALUE:
            case WASP_CMD_SET_VALUES:
               while(gRxOff < RxMsgLen) {
                  if(CopyVar(RxBuf[gRxOff++]) == 0) {
                     break;
                  }
               }
               break;

            case WASP_CMD_GET_BAUDRATES:
            // Just list 57600, our desired baudrate
               *((long *)&TxBuf[gTxLen]) = 57600;
               gTxLen += sizeof(long);
               break;

            case WASP_CMD_SET_BAUDRATE:
            // Just send an OK for now, we'll actually change
            // the baud rate after the reply has been sent
               gFlags.ChangeBaudRate = TRUE;
               break;

            default:
               TxBuf[RCODE_OFFSET] = WASP_ERR_CMD;
               break;
         }

         if(gTxLen != 0) {
            if(gFlags.SendingEven || gFlags.SendingOdd) {
            // Need to wait
               if(gFlags.TxMsgWaiting) {
                  if(gDebug.RespOvfl != 0xff) {
                     gDebug.RespOvfl++;
                  }
               }
               gFlags.TxMsgWaiting = TRUE;
            }
            else {
               SendMsg(TxBuf,gTxLen,FALSE);
            }
         }
      }

      if((U0TCTL & TXEPT) && gFlags.AsyncEnabled) {
      // Async transmission enabled and WASP transmitter is idle
      // Try to send data
         SendEchoData(WASP_CMD_ASYNC_DATA);
      }
   }
}


// interrupt handlers
#pragma vector=USART0RX_VECTOR
__interrupt void USART0_RX (void)
{
//  TXBUF0 = RXBUF0;
   if(ParseWaspData(RXBUF0) > 0) {
      gMsgReady = 1;
   }
}


#pragma vector=USART0TX_VECTOR
__interrupt void USART0_TX (void)
{
#ifdef ASCII_SERIAL_OUTPUT
   if(pAsciiOut != NULL) {
      TXBUF0 = *pAsciiOut++;
      if(*pAsciiOut == 0) {
         pAsciiOut = NULL;
      }
   }
   else {
   // Nothing more to send
      IE1 &= ~UTXIE0;
   }
#else
   if(WaspTxReady()) {
      TXBUF0 = GetWaspTxData();
   }
   else {
   // Nothing more to send
      IE1 &= ~UTXIE0;
   }
#endif
}

// Timer A0 interrupt service routine
#pragma vector=TIMERA0_VECTOR
__interrupt void Timer_A_clock (void)
{
  P1OUT ^= BIT0;                            // Toggle P1.0
  TACCTL0 &= ~CCIFG;
  TACCTL0 &= ~COV;
}

// Timer_A1 Interrupt Vector
#pragma vector=TIMERA1_VECTOR
__interrupt void Timer_A_flow_meter(void)
{
   TACCTL1 &= ~CCIFG;
}

// SD24 Interrupt
#pragma vector=SD24_VECTOR
__interrupt void SD24AISR(void)
{
   if(SD24IV & SD24IV_SD24MEM0) {
      gLastPressure = SD24MEM0;

      if(gDebug.MaxPresure < gLastPressure) {
         gDebug.MaxPresure = gLastPressure;
      }
      else if(gDebug.MinPresure > gLastPressure) {
         gDebug.MinPresure = gLastPressure;
      }

#ifdef ASCII_SERIAL_OUTPUT
      gNewSample = TRUE;
#else
      if(gEvenSamples < NUM_PRESSURE_READINGS) {
         gEvenMsg.Data.Pressure[gEvenSamples++] = gLastPressure;
         if(gEvenSamples == NUM_PRESSURE_READINGS) {
            RecNum++;
            if(!gFlags.AsyncEnabled) {
            // Not streaming yet
               if(gFlags.SendingOdd) {
               // Reuse the Even buffer next
                  gEvenSamples = 0;
               }
               else {
               // Use the Odd buffer next
                  gOddSamples = 0;
               }
            }
         }
      }
      else if(gOddSamples < NUM_PRESSURE_READINGS) {
         gOddMsg.Data.Pressure[gOddSamples++] = gLastPressure;
         if(gOddSamples == NUM_PRESSURE_READINGS) {
            RecNum++;
            if(!gFlags.AsyncEnabled) {
            // Not streaming yet
               if(gFlags.SendingEven) {
               // Reuse the Odd buffer next
                  gOddSamples = 0;
               }
               else {
               // Use the Even buffer next
                  gEvenSamples = 0;
               }
            }
         }
      }
      else if(gDebug.DataOvfl != 0xff) {
      // no room left in the inn
         gDebug.DataOvfl++;
      }

      if(Ticker++ == 0) {
         if(gWaspTimer > 0) {
         // WASP activity timer running, decrement it
            gWaspTimer--;
         }
      }
#endif
   }
}

#pragma vector=PORT1_VECTOR
__interrupt void Port1ISR(void)
{
   gFlowcount++;
// Clear interrupt
   P1IFG &= ~BIT2;
}

#pragma vector=PORT2_VECTOR
__interrupt void Port2ISR(void)
{
}

#pragma vector=NMI_VECTOR
__interrupt void NmiISR(void)
{
}

#pragma vector=WDT_VECTOR
__interrupt void WdtISR(void)
{
}

// Return 0 on error
u8 CopyVar(u8 VarID)
{
   u8 Len = 0;
   u16 x = 1;  // Assume R/O variable
   volatile void *Var;

   switch(VarID) {
      case WASP_VAR_WEMO_STATUS:
         Var = (void *) &WemoStatus;
         Len = sizeof(WemoStatus);
         x = 2;   // Write only
         break;

      case WASP_VAR_WEMO_STATE:
         Var = (void *) &OverallState;
         Len = sizeof(OverallState);
         x = 3;   // R/W
         break;

      case WASP_VAR_DEVID:
         Var = (void *) DeviceID;
         Len = sizeof(DeviceID);
         break;

      case WASP_VAR_FW_VER:
         Var = (void *) FirmwareVersion;
         Len = sizeof(FirmwareVersion);
         break;

      case WASP_VAR_DEV_DESC:
         Var = (void *) DeviceDesc;
         Len = sizeof(DeviceDesc);
         break;

      case WASP_VAR_WASP_VER:
         Var = (void *) WaspVersion;
         Len = sizeof(WaspVersion);
         break;

      case WASP_VAR_PRESSURE:
         Var = (void *)  &gLastPressure;
         Len = sizeof(gLastPressure);
         break;

      case WASP_VAR_PRESSURE_MIN:
         Var = (void *) &gDebug.MinPresure;
         Len = sizeof(gDebug.MinPresure);
         break;

      case WASP_VAR_PRESSURE_MAX:
         Var = (void *) &gDebug.MaxPresure;
         Len = sizeof(gDebug.MaxPresure);
         break;

      case WASP_VAR_FLOW:
         Var = (void *) &gFlowcount;
         Len = sizeof(gFlowcount);
         break;

      default:
         TxBuf[RCODE_OFFSET] = WASP_ERR_ARG;
         gTxLen = 2;
         return 0;
   }

   if(Len > 0) {
      if(RxBuf[CMD_OFFSET] == WASP_CMD_GET_VALUE ||
         RxBuf[CMD_OFFSET] == WASP_CMD_GET_VALUES) 
      {  // Read request
         if((x & 1) == 0) {
         // Write only variable
            TxBuf[RCODE_OFFSET] = WASP_ERR_ARG;
            gTxLen = 2;
            return 0;
         }

         if(Len + gTxLen > TX_BUF_SIZE) {
            TxBuf[RCODE_OFFSET] = WASP_ERR_OVFL;
            gTxLen = 2;
            return 0;
         }

         if(Len == 2) {
         // Special handling for 16 bit values which are mostly written
         // by interrupt handlers.  Read the value in a loop until we get 
         // the  same value twice, this ensures that the MSB and LSB bytes
         // are consistent.

            for( ; ; ) {
               x = *((u16 *) Var);
               if(x == *((u16 *) Var)) {
                  break;
               }
            }
            Var = (void *) &x;
         }

         memcpy(&TxBuf[gTxLen],(void *) Var,Len);
         gTxLen += Len;

         if(VarID == WASP_VAR_PRESSURE_MIN) {
         // Reset the minimum on read
            gDebug.MinPresure = 0xffff;
         }
         else if(VarID == WASP_VAR_PRESSURE_MAX) {
         // Reset the max on read
            gDebug.MaxPresure = 0;
         }
      }
      else {
      // WASP_CMD_SET_VALUE or WASP_CMD_SET_VALUES

         if((x & 2) == 0) {
         // Read only variable
            TxBuf[RCODE_OFFSET] = WASP_ERR_ARG;
            gTxLen = 2;
            return 0;
         }

         memcpy((void *)Var,(void *) &RxBuf[gRxOff],Len);
         gRxOff += Len;
      }
   }

   return Len;
}

void TxCopy(void *From,u8 Len)
{
   if(Len + gTxLen > TX_BUF_SIZE) {
   // This shouldn't happen!  Timeout the watchdog
      for( ; ; ) {
         int x = 0;
         x++;
      }
   }

   memcpy(&TxBuf[gTxLen],From,Len);
   gTxLen += Len;
}

u8 SendMsg(u8 *Msg,u8 MsgLen,u8 bAsync)
{
   if(SendWaspMsg(Msg,MsgLen,bAsync)) {
   // Error, WASP busy
      if(gDebug.TxBusyErr != 0xff) {
         gDebug.TxBusyErr++;
      }
      return 0;
   }
   return 1;
}

// Return 0 on success
u8 SendEchoData(u8 Cmd)
{
   EchoDataMsg *p;

   if(gEvenSamples == NUM_PRESSURE_READINGS) {
   // Even data ready
      gFlags.SendingEven = TRUE;
      p = &gEvenMsg;
   }
   else if(gOddSamples == NUM_PRESSURE_READINGS) {
   // Odd data ready
      gFlags.SendingOdd = TRUE;
      p = &gOddMsg;
   }
   else {
   // No data is ready
      TxBuf[RCODE_OFFSET] = WASP_ERR_DATA_NOTRDY;
      return 1;
   }

   p->Data.RecNum = RecNum;
   p->Data.FlowCount = gFlowcount;
   if(Cmd == WASP_CMD_ASYNC_DATA) {
      p->Hdr[0] = Cmd | WASP_CMD_RESP; // Cmd
      p->Hdr[1] = 0; // Rcode
      p->Hdr[2] = WASP_VAR_ECHO_DATA;  // VarID
      SendMsg(&p->Hdr[0],sizeof(*p),TRUE);
   }
   else {
      p->Hdr[1] = Cmd | WASP_CMD_RESP; // Cmd
      p->Hdr[2] = 0; // Rcode
      SendMsg(&p->Hdr[1],sizeof(*p)-1,FALSE);
   }

   return 0;
}

void SetBaudRate(u16 Baudrate)
{
   U0BR0 = (unsigned char) ((SMCLK/Baudrate) & 0xff);
   U0BR1 = (unsigned char) ((SMCLK/Baudrate) >> 8);
}


