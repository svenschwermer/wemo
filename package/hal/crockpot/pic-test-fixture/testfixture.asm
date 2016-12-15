; Jarden CrockPot interface test fixture firmware
;
; The test fixture connects an LED to each output,
; this code simply drives each output low in succession
; for 1/2 second.
;
; Production - PIC16F1828
;
; Pin usage:
;
; RA0 - ICDDAT   - P1.4         (not driven, tested by programming the part)
; RA1 - ICDCLK   - P1.5         (not driven, tested by programming the part)
; RA2 - COM1
; RA3 - MCLR/VPP - P1.1         (not driven, tested by programming the part)
; RA4 - COM2
; RA5 - Input
;
; RB4 - COM3
; RB5 - Rxd
; RB6 - COM4
; RB7 - Txd
;
; RC0 - LED1
; RC1 - LED2
; RC2 - LED3
; RC3 - LED4
; RC4 - LED5
; RC5 - LED6
; RC6 - LED7
; RC7 - LED8

        ERRORLEVEL   -302

        IFDEF __16F1828
#include <p16f1828.inc>
#define FSR     FSR0
#define INDF    INDF0
#define BAUDCTL BAUDCON
    __CONFIG _CONFIG1, _FOSC_INTOSC & _WDTE_OFF & _PWRTE_OFF & _MCLRE_ON & _CP_OFF & _CPD_OFF & _BOREN_ON & _CLKOUTEN_OFF & _IESO_OFF & _FCMEN_OFF
    __CONFIG _CONFIG2, _WRT_OFF & _PLLEN_OFF & _STVREN_OFF & _BORV_HI & _LVP_OFF
        endif


#define COM1    PORTA,2
#define COM2    PORTA,4
#define COM3    PORTB,4
#define COM4    PORTB,6

#define LED1    PORTC,0
#define LED2    PORTC,1
#define LED3    PORTC,2
#define LED4    PORTC,3
#define LED5    PORTC,4
#define LED6    PORTC,5
#define LED7    PORTC,6
#define LED8    PORTC,7

#define BIT_INPUT       PORTA,5

#define RXD     PORTB,5
#define TXD     PORTB,7

#define TIME_CONSTANT1  0xbd
#define TIME_CONSTANT2  0x28
#define TIME_CONSTANT3  0x6


        cblock     0x20
delay1
delay2
delay3

        endc
     
        org 0
Start:
        
        ; Set clock to 8Mhz, internal
        banksel OSCCON          ;Bank 1
        movlw   0x72            ;8Mhz internal clock
        movwf   OSCCON          ;
        
        banksel ANSELA          ;Bank 3
        clrf    ANSELA          ;disable analog inputs
        clrf    ANSELB          ;
        clrf    ANSELC          ;
        banksel PIR1            ;Bank 0
        clrf    FSR0H
        
        ;set all ports to output
        banksel TRISA           ;Bank 1
        clrf    TRISA
        clrf    TRISB
        clrf    TRISC
        
        ;precondition all outputs high
        banksel PORTA           ;Bank 0
        movlw   0xff
        movwf   PORTA           ;
        movwf   PORTB           ;
        movwf   PORTC           ;
     
MainLoop:
        bcf     LED1            ;
        call    delay           ;wait 1/2 second
        bsf     LED1
        
        bcf     LED2            ;
        call    delay           ;wait 1/2 second
        bsf     LED2
        
        bcf     LED3            ;
        call    delay           ;wait 1/2 second
        bsf     LED3
        
        bcf     LED4            ;
        call    delay           ;wait 1/2 second
        bsf     LED4
        
        bcf     LED5            ;
        call    delay           ;wait 1/2 second
        bsf     LED5
        
        bcf     LED6            ;
        call    delay           ;wait 1/2 second
        bsf     LED6
        
        bcf     LED7            ;
        call    delay           ;wait 1/2 second
        bsf     LED7
        
        bcf     LED8            ;
        call    delay           ;wait 1/2 second
        bsf     LED8
        
        bcf     COM1            ;
        call    delay           ;wait 1/2 second
        bsf     COM1
        
        bcf     COM2            ;
        call    delay           ;wait 1/2 second
        bsf     COM2
        
        bcf     COM3
        call    delay           ;wait 1/2 second
        bsf     COM3
        
        bcf     COM4
        call    delay           ;wait 1/2 second
        bsf     COM4
        
        bcf     BIT_INPUT       ;
        call    delay           ;wait 1/2 second
        bsf     BIT_INPUT       ;
        
        bcf     RXD
        call    delay           ;wait 1/2 second
        bsf     RXD
        
        bcf     TXD
        call    delay           ;wait 1/2 second
        bsf     TXD
        goto    MainLoop        ;Do it all again
        
        
delay:  movlw   TIME_CONSTANT1  ;
        movwf   delay1          ;
        movlw   TIME_CONSTANT2  ;
        movwf   delay2          ;
        movlw   TIME_CONSTANT3  ;
        movwf   delay3          ;
delayloop:
        decfsz  delay1,f        ;       
        goto    delayloop       ;     
        decfsz  delay2,f        ;       
        goto    delayloop       ;     
        decfsz  delay3,f        ;       
        goto    delayloop       ;     
        return
        
     end
     
