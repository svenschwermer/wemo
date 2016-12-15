/* THIS SAMPLE CODE IS PROVIDED AS IS AND IS SUBJECT TO ALTERATIONS. FUJITSU */
/* MICROELECTRONICS ACCEPTS NO RESPONSIBILITY OR LIABILITY FOR ANY ERRORS OR */
/* ELIGIBILITY FOR ANY PURPOSES.                                             */
/*            (C) Fujitsu Microelectronics (Shanghai) Co., LTD.              */
/*---------------------------------------------------------------------------
  VECTORS.C
  - Interrupt level (priority) setting
  - Interrupt vector definition
  27.02.08  1.00   HWe    V30L29
-----------------------------------------------------------------------------*/

 #include "mb95560.h"

/*---------------------------------------------------------------------------
   InitIrqLevels()

   This function  pre-sets all interrupt control registers. It can be used
   to set all interrupt priorities in static applications. If this file
   contains assignments to dedicated resources, verify  that the
   appropriate controller is used.

   NOTE: value 0xFF disables the interrupt and value 0 sets highest priority.
   NOTE: For all resource interrupts exists 3 Interrupt level registers (ILRx).
         Each register sets the level for 4 different resources (IRQx).
   NOTE: This list is prepared for the 8FX-emulation chip MB95FV100 'Horn'.
         Not all resources will be supported by all 8FX-devices
-----------------------------------------------------------------------------*/

void InitIrqLevels(void)
{
/*  ILRx                  IRQs defined by ILRx */

    ILR0 = 0x3F;      //  IRQ0:  external interrupt ch4
                      //  IRQ1:  external interrupt ch5
                      //  IRQ2:  external interrupt ch2 | ch6
                      //  IRQ3:  external interrupt(ch3)| ch7	<<--- Level 0

    ILR1 = 0xB3;      //  IRQ4:  UART/SIO ch0
                      //  IRQ5:  8/16-bit timer ch0 (lower)		<<--- Level 0
                      //  IRQ6:  8/16-bit timer ch0 (upper)
                      //  IRQ7:  LIN-UART (reception)		<<--- Level 2

    ILR2 = 0xFE;      //  IRQ8:  LIN-UART (transmission)		<<--- Level 2
                      //  IRQ9:  8/16-bit PPG ch1 (lower) | UART/SIO ch1
                      //  IRQ10: 8/16-bit PPG ch1 (upper) | I2C ch1
                      //  IRQ11: 16-bit reload timer ch0

    ILR3 = 0xFF;      //  IRQ12: 8/16-bit PPG ch0 (upper)
                      //  IRQ13: 8/16-bit PPG ch0 (lower)
                      //  IRQ14: 8/16-bit timer ch1 (upper)
                      //  IRQ15: 16-bit PPG ch0 + ch2

    ILR4 = 0x7F;      //  IRQ16: 16-bit reload timer ch1 | I2C ch0
                      //  IRQ17: 16-bit PPG ch1
                      //  IRQ18: 10-biat A/D-converter
                      //  IRQ19: Timebase timer			<<--- Level 1

    ILR5 = 0xFF;      //  IRQ20: Watch timer / counter
                      //  IRQ21: external interrupt ch 8-11
                      //  IRQ22: 8/16-bit timer ch1 (lower) | external interrupt ch 12-15
                      //  IRQ23: Flash | Custom ch1
}

/*---------------------------------------------------------------------------
   Prototypes

   Add your own prototypes here. Each vector definition needs is proto-
   type. Either do it here or include a header file containing them.
-----------------------------------------------------------------------------*/

__interrupt void DefaultIRQHandler (void);
__interrupt void int_uart_rx (void);
__interrupt void int_uart_tx(void);
__interrupt void int_t00(void);
__interrupt void int_acz(void);
__interrupt void int_timebase(void);

/*---------------------------------------------------------------------------
   Vector definiton

   Use following statements to define vectors.
   All resource related vectors are predefined.
   Remaining software interrupts can be added hereas well.
-----------------------------------------------------------------------------*/

#pragma intvect DefaultIRQHandler 0   	//  IRQ0:  external interrupt ch0 | ch4
#pragma intvect DefaultIRQHandler 1   	//  IRQ1:  external interrupt ch1 | ch5
#pragma intvect DefaultIRQHandler 2   	//  IRQ2:  external interrupt ch2 | ch6
#pragma intvect int_acz 	    3   	//  IRQ3:  external interrupt ch3 | ch7

#pragma intvect DefaultIRQHandler 4   //  IRQ4:  UART/SIO ch0
#pragma intvect int_t00	    5   //  IRQ5:  8/16-bit timer ch0 (lower)
#pragma intvect DefaultIRQHandler 6   //  IRQ6:  8/16-bit timer ch0 (upper)
#pragma intvect int_uart_rx       7   //  IRQ7:  LIN-UART (reception)

#pragma intvect int_uart_tx	    8   //  IRQ8:  LIN-UART (transmission)
#pragma intvect DefaultIRQHandler 9   //  IRQ9:  8/16-bit PPG ch1 (lower) | UART/SIO ch1
#pragma intvect DefaultIRQHandler 10  //  IRQ10: 8/16-bit PPG ch1 (upper) | I2C ch1
#pragma intvect DefaultIRQHandler 11  //  IRQ11: 16-bit reload timer ch0

#pragma intvect DefaultIRQHandler 12  //  IRQ12: 8/16-bit PPG ch0 (upper)
#pragma intvect DefaultIRQHandler 13  //  IRQ13: 8/16-bit PPG ch0 (lower)
#pragma intvect DefaultIRQHandler 14  //  IRQ14: 8/16-bit timer ch1 (upper)
#pragma intvect DefaultIRQHandler 15  //  IRQ15: 16-bit PPG ch0 + ch2

#pragma intvect DefaultIRQHandler 16  //  IRQ16: 16-bit reload timer ch1 | I2C ch0
#pragma intvect DefaultIRQHandler 17  //  IRQ17: 16-bit PPG ch1
#pragma intvect DefaultIRQHandler 18  //  IRQ18: 10-biat A/D-converter
#pragma intvect int_timebase      19  //  IRQ19: Timebase timer

#pragma intvect DefaultIRQHandler 20  //  IRQ20: Watch timer / counter
#pragma intvect DefaultIRQHandler 21  //  IRQ21: external interrupt ch 8-11
#pragma intvect DefaultIRQHandler 22  //  IRQ22: 8/16-bit timer ch1 (lower) | external interrupt ch 12-15
#pragma intvect DefaultIRQHandler 23  //  IRQ23: Flash | Custom ch1

// Because of a compiler bug the following workaround will replace
// the #pragma intvect 22/23 instruction (only needed for Propack Version
// (only needed for Propack Version < REV300013)

//#pragma asm
// 	.SECTION	INTVECT23,CONST,LOCATE=0FFCCH
// 	.DATA.W 	_DefaultIRQHandler               //  IRQ22: 8/16-bit timer ch1 (lower) | external interrupt ch 12-15
// 	.SECTION	INTVECT22,CONST,LOCATE=0FFCEH
// 	.DATA.W 	_DefaultIRQHandler               //  IRQ23: Flash | Custom ch1
//#pragma endasm

/*---------------------------------------------------------------------------
   DefaultIRQHandler()

   This function is a placeholder for all vector definitions.
   Either use your own placeholder or add necessary code here
   (the real used resource interrupt handlers should be defined in the main.c).
-----------------------------------------------------------------------------*/

__interrupt
void DefaultIRQHandler (void)
{
    __DI();             // disable interrupts
    while(1)
        __wait_nop();   // halt system
}
