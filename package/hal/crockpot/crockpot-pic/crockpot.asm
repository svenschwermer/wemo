; Jarden CrockPot interface.
;
; Monitoring:
; 4 x 7 segment LED showing cook time remaining
; 3 status LEDs showing mode: keep warm, low or high
; 
; Driving:
; 1 multiplexed input which monitors
; 4 spst switches: Mode, Up, Down, and Off.
;
; Production - PIC16F1828
;
; Initial Software development - 16F690 on Microchip
; low pin count evaluation board.
;
; Pin usage:
;
; RA0 - ICDDAT   - P1.4
; RA1 - ICDCLK   - P1.5
; RA2 - COM1     - IC1.20
; RA3 - MCLR/VPP - P1.1
; RA4 - COM2     - IC1.19
; RA5 - Input    - IC1.10
;       (dynamic in/out)
;
; RB4 - COM3     - IC1.18
; RB5 - Rxd      - Serial data input from RT5350 TXD2
; RB6 - COM4     - IC1.17
; RB7 - Txd      - Serial data output to RT5350 RXD2 via 3.3/5 voltage divider
;
; RC0 - LED1     - IC1.2
; RC1 - LED2     - IC1.4
; RC2 - LED3     - IC1.8
; RC3 - LED4     - IC1.16
; RC4 - LED5     - IC1.1
; RC5 - LED6     - IC1.3
; RC6 - LED7     - IC1.9
; RC7 - LED8     - IC1.11

;   A PIC microprocessor on the Crockpot monitors the 4 - 7 segment LED
;   displays and 3 mode LEDS and sends the data to us when the data changes.
;   The data is sent in 5 bytes:
; 
;   Byte 1:
;       0x80 - No status LEDs on
;       0x81 - "Keep Warm" LED on
;       0x82 - "Low" LED on
;       0x84 - "High" LED on
;       0x87 - All status LEDs on (they flash on and off at power up)
;       0x88 - Firmware version string in Byte 2... Byte 5
; 
;   Byte 2:
;       Bit7 - 0
;       Bit6 - Digit 1 Segment A
;       Bit5 - Digit 1 Segment B
;       Bit4 - Digit 1 Segment C
;       Bit3 - Digit 1 Segment D
;       Bit2 - Digit 1 Segment E
;       Bit1 - Digit 1 Segment F
;       Bit0 - Digit 1 Segment G
; 
;   Byte 3:
;       Bit7 - 0
;       Bit6 - Digit 2 Segment A
;       Bit5 - Digit 2 Segment B
;       Bit4 - Digit 2 Segment C
;       Bit3 - Digit 2 Segment D
;       Bit2 - Digit 2 Segment E
;       Bit1 - Digit 2 Segment F
;       Bit0 - Digit 2 Segment G
; 
;   Byte 4:
;       Bit7 - 0
;       Bit6 - Digit 3 Segment A
;       Bit5 - Digit 3 Segment B
;       Bit4 - Digit 3 Segment C
;       Bit3 - Digit 3 Segment D
;       Bit2 - Digit 3 Segment E
;       Bit1 - Digit 3 Segment F
;       Bit0 - Digit 3 Segment G
; 
;   Byte 5:
;       Bit7 - 0
;       Bit6 - Digit 4 Segment A
;       Bit5 - Digit 4 Segment B
;       Bit4 - Digit 4 Segment C
;       Bit3 - Digit 4 Segment D
;       Bit2 - Digit 4 Segment E
;       Bit1 - Digit 4 Segment F
;       Bit0 - Digit 4 Segment G
; 
;   A single byte of data is sent to the PIC to control
;   the 4 push buttons:
; 
;   'O' - "Push" the Off button
;   'U' - "Push" the Up button
;   'D' - "Push" the Down button
;   'S' - "Push" the Select button
;
;   'V' - send firmware version and then start main loop
; 
;       any other value does nothing.
;

        ERRORLEVEL   -302

        IFDEF __16F690
#include <p16F690.inc>
     __config (_INTRC_OSC_NOCLKOUT & _WDT_OFF & _PWRTE_OFF & _MCLRE_OFF & _CP_OFF & _BOR_ON & _IESO_OFF & _FCMEN_OFF)
#define INTERNAL_8MHZ	0x71
        endif

        IFDEF __16F1828
#include <p16f1828.inc>
#define FSR     FSR0
#define INDF    INDF0
#define BAUDCTL BAUDCON
#define INTERNAL_8MHZ	0x72
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

INPUT_LOW macro
        banksel TRISA           ;Bank 1
        bcf     TRISA,5         ;enable output
        banksel PIR1            ;Bank 0
        endm

INPUT_HIGH macro
        banksel TRISA           ;Bank 1
        bsf     TRISA,5         ;tristate output
        banksel PIR1            ;Bank 0
        endm

; Define bits in PressButtons
#define BUTTON_OFF      0
#define BUTTON_UP       1
#define BUTTON_DOWN     2
#define BUTTON_MODE     3

;
#define WARM_LED_BIT    New1,0
#define LOW_LED_BIT     New1,1
#define HIGH_LED_BIT    New1,2

     
        cblock     0x20
LoopCount

Old1
Old2
Old3
Old4
Old5
Old6
Old7
Old8
Old9
Old10
                
Last1
Last2
Last3
Last4
Last5

New1
New2
New3
New4
New5


OutCount
OutPtr

PressButtons

RxData

        endc

        cblock 0x70     ; put these up in unbanked RAM
W_Save
STATUS_Save
FSR_Save
        endc

     
        org 0
        goto      Start
        nop
        nop
        nop
ISR:
        movwf   W_Save
        movf    STATUS,w
        movwf   STATUS_Save
        banksel PIR1            ;Bank 0
        btfss   PIR1,RCIF       ;Rx ready interrupt ?
        goto    isr1            ;nope
        
        ; Get command character
        banksel RCREG           ;
        movfw   RCREG           ;
        banksel PIR1            ;Bank 0
        movwf   RxData          ;
        
        INPUT_HIGH              ;release any button that is pressed
        clrf    PressButtons    ;clear any previous command
        
        banksel RCSTA           ;
        btfss   RCSTA,OERR
        goto    isr3
        ; clear over run error
        bcf     RCSTA,CREN      ;Clear continous receive enable
        nop
        bsf     RCSTA,CREN      ;
        
isr3:        
        banksel PIR1            ;Bank 0
        movlw   'V'             ;Version request ?
        subwf   RxData,w        ;
        btfss   STATUS,Z        ;
        goto    isr2            ;nope
        call    sendv           ;
        
isr2:   movlw   'O'             ;Push Off button ?
        subwf   RxData,w        ;
        btfsc   STATUS,Z        ;skip if not
        bsf     PressButtons,BUTTON_OFF

        movlw   'U'             ;Push Up button ?
        subwf   RxData,w        ;
        btfsc   STATUS,Z        ;skip if not
        bsf     PressButtons,BUTTON_UP

        movlw   'D'             ;Push Down button ?
        subwf   RxData,w        ;
        btfsc   STATUS,Z        ;skip if not
        bsf     PressButtons,BUTTON_DOWN

        movlw   'S'             ;Push Select button ?
        subwf   RxData,w        ;
        btfsc   STATUS,Z        ;skip if not
        bsf     PressButtons,BUTTON_MODE
        
isr1:   
        banksel PIE1            ;Bank 1
        btfss   PIE1,TXIF       ;Tx ready interrupt enabled ?
        goto    ExitISR         ;nope
        banksel PIR1            ;Bank 0
        btfss   PIR1,TXIF       ;Tx ready interrupt ?
        goto    ExitISR         ;nope
        
        decfsz  OutCount,f
        goto    TxNext  

        ;Nothing more to send
        banksel PIE1            ;bank 1
        bcf     PIE1,TXIE       ;disable TX interrupt
        goto    ExitISR

TxNext:
        movfw   FSR
        movwf   FSR_Save
        movfw   OutPtr
        incf    OutPtr,f        ;
        movwf   FSR
        movf    INDF,w          ;
	banksel	TXREG           ;
        movwf   TXREG           ;
        movfw   FSR_Save
        movwf   FSR
               
ExitISR:
        movf      STATUS_Save,w
        movwf     STATUS
        swapf     W_Save,f
        swapf     W_Save,w
        retfie

        ; Send version
sendv:  movlw   d'8'            ;
        movwf   OutCount        ;
        movlw   low Old1        ;
        movwf   OutPtr          ;

        movlw   0x88            ;
        movwf   Old1            ;
        movlw   'V'             ;
        movwf   Old2            ;
        movlw   '0'             ;
        movwf   Old3            ;
        movlw   '.'             ;
        movwf   Old4            ;
        movlw   '0'             ;
        movwf   Old5            ;
        movlw   '3'             ;
        movwf   Old6            ;
        movlw   0x0             ;
        movwf   Old7            ;
        
        banksel PIE1            ;Bank 1
        bsf     PIE1,TXIE       ;enable TX interrupt
        banksel PIR1            ;Bank 0
        return                  ;

Start:
        
        ; Set clock to 8Mhz, internal
        banksel OSCCON          ;Bank 1
        movlw   INTERNAL_8MHZ	;8Mhz internal clock
        movwf   OSCCON          ;
        
        ;Initialize uart for 9600, 8 data bits, no parity
        banksel SPBRG           ;Bank 1
        movlw   d'12'           ;9600 divider, 8 Mhz clock, BRGH = 0, BRG16 = 0
        movwf   SPBRG           ;
        clrf    BAUDCTL         ;BRG16 =0, SCKP = 0
        
        IFDEF __16F690
        banksel ANSEL           ;Bank 2
        clrf    ANSEL           ;disable analog inputs
        clrf    ANSELH          ;disable analog inputs
        banksel PIR1            ;Bank 0
        endif
        
        IFDEF __16F1828
        banksel ANSELA          ;Bank 3
        clrf    ANSELA          ;disable analog inputs
        clrf    ANSELB          ;
        clrf    ANSELC          ;
        banksel PIR1            ;Bank 0
        clrf    FSR0H
        endif
        
        banksel	RCSTA
        bsf     RCSTA,SPEN      ;Serial port enable
	banksel	RCREG
        movfw   RCREG           ;make sure FIFO is empty
        movfw   RCREG
	banksel	RCSTA
        bsf     RCSTA,CREN      ;Continous recieve enable
        
        banksel TXSTA           ;
        movlw   0x20            ;TX enable, low speed async mode
        movwf   TXSTA           ;
        
	banksel	PIE1            ;
        bsf     PIE1,RCIF       ;enable Rx interrupt
	banksel	INTCON          ;
        bsf     INTCON,PEIE     ;enable peripheral interrupts
        bsf     INTCON,GIE      ;enable global interrupts
        banksel PIR1            ;Bank 0
        bcf     BIT_INPUT       ;precondition Input bit as low
        
;       call    sendv           ;
        clrf    PressButtons    ;clear any previous command
        clrf    OutCount        ;

; Set RA0 and RA1 to output mode and drive them low.
; These are ICD/ICSP programming pins as they are floating
; when not connected to a programmer.  We've see random
; problems with the flash image being corrupted after
; it was once programmed and verified correctly.
; Cindy Otto from Micrchip suggested this may help.

        banksel TRISA           ;Bank 1
        bcf     TRISA,0         ;enable output on RA0
        bcf     TRISA,1         ;enable output on RA1
        banksel PORTA           ;Bank 0
        bcf     PORTA,0         ;set RA0 low
        bcf     PORTA,1         ;set RA1 low

     
; Init new bytes for next pass
MainLoop:
        bcf     BIT_INPUT
        movlw   0x80            ;
        movwf   New1            ;
        clrf    New2
        clrf    New3
        clrf    New4
        clrf    New5
        movlw   2               ;set Loop counter
        movwf   LoopCount       ;
        

; Wait for LED1 to go active
WaitLED1:
        btfsc   LED1
        goto    WaitLED1
        btfsc   LED1            ; read it again to be sure
        goto    WaitLED1
        
        ; Set bits that are active
        btfsc   COM1            ;
        bsf     New5,6
        btfsc   COM2            ;
        bsf     New4,6
        btfsc   COM3            ;
        bsf     New3,6
        btfsc   COM4            ;
        bsf     New2,6          ;

; Wait for LED2 to go active
WaitLED2:
        btfsc   LED2
        goto    WaitLED2
        btfsc   LED2            ; read it again to be sure
        goto    WaitLED2
        
        ; Set bits that are active
        btfsc   COM1            ;
        bsf     New5,5
        btfsc   COM2            ;
        bsf     New4,5
        btfsc   COM3            ;
        bsf     New3,5
        btfsc   COM4            ;
        bsf     New2,5          ;

        
; Wait for LED3 to go active
WaitLED3:
        btfsc   LED3
        goto    WaitLED3
        btfsc   LED3            ; read it again to be sure
        goto    WaitLED3
        
        btfss   PressButtons,BUTTON_OFF
        goto    Led3
        
        INPUT_LOW               ;"Press" the "Off" button
        
        ; Set bits that are active
Led3:   btfsc   COM1            ;
        bsf     New5,4
        btfsc   COM2            ;
        bsf     New4,4
        btfsc   COM3            ;
        bsf     New3,4
        btfsc   COM4            ;
        bsf     New2,4          ;
        btfss   PressButtons,BUTTON_OFF
        goto    WaitLED4

        ;wait for LED3 to go high
Led3D:  btfss   LED3
        goto    Led3D
        
        INPUT_HIGH              ;release the "Off" button if it was pressed

; Wait for LED4 to go active
WaitLED4:
        btfsc   LED4
        goto    WaitLED4
        btfsc   LED4            ; read it again to be sure
        goto    WaitLED4
        
        ; Set bits that are active
        btfsc   COM1            ;
        bsf     New5,3
        btfsc   COM2            ;
        bsf     New4,3
        btfsc   COM3            ;
        bsf     New3,3
        btfsc   COM4            ;
        bsf     New2,3          ;
 
; Wait for LED5 to go active
WaitLED5:
        btfsc   LED5
        goto    WaitLED5
        btfsc   LED5            ; read it again to be sure
        goto    WaitLED5
        
        btfss   PressButtons,BUTTON_MODE
        goto    Led5
        
        INPUT_LOW               ;"Press" the "Mode" button
        
        ; Set bits that are active
Led5:   btfsc   COM1            ;
        bsf     New5,2
        btfsc   COM2            ;
        bsf     New4,2
        btfsc   COM3            ;
        bsf     New3,2
        btfsc   COM4            ;
        bsf     New2,2          ;
        
        btfss   PressButtons,BUTTON_MODE
        goto    WaitLED6

        ;wait for LED5 to go high
Led5D:  btfss   LED5
        goto    Led5D
        
        INPUT_HIGH              ;release the "Mode" button if it was pressed
        
; Wait for LED6 to go active
WaitLED6:
        btfsc   LED6
        goto    WaitLED6
        btfsc   LED6            ; read it again to be sure
        goto    WaitLED6
        
        ; Set bits that are active
        btfsc   COM1            ;
        bsf     New5,1
        btfsc   COM2            ;
        bsf     New4,1
        btfsc   COM3            ;
        bsf     New3,1
        btfsc   COM4            ;
        bsf     New2,1          ;
        
; Wait for LED7 to go active
WaitLED7:
        btfsc   LED7
        goto    WaitLED7
        btfsc   LED7            ; read it again to be sure
        goto    WaitLED7

        btfss   PressButtons,BUTTON_DOWN
        goto    Led7
        
        INPUT_LOW               ;"Press" the "Down" button
        
        ; Set bits that are active
Led7:   btfsc   COM1            ;
        bsf     New5,0
        btfsc   COM2            ;
        bsf     New4,0
        btfsc   COM3            ;
        bsf     New3,0
        btfsc   COM4            ;
        bsf     New2,0          ;

        btfss   PressButtons,BUTTON_DOWN
        goto    WaitLED8        ;

        ;wait for LED7 to go high
Led7D:  btfss   LED7
        goto    Led7D
        
        INPUT_HIGH              ;release the "Down" button if it was pressed

; Wait for LED8 to go active
WaitLED8:
        btfsc   LED8
        goto    WaitLED8
        btfsc   LED8            ; read it again to be sure
        goto    WaitLED8

        btfss   PressButtons,BUTTON_UP
        goto    Led8
        
        INPUT_LOW               ;"Press" the "Up" button
        
        ;NB the schematic appears to have an error, the LOW and WARM
        ;LEDs are swapped
Led8:   btfsc   COM4            ;
        bsf     HIGH_LED_BIT    ;
        btfsc   COM2            ;
        bsf     LOW_LED_BIT     ;
        btfsc   COM3            ;
        bsf     WARM_LED_BIT    ;

        btfss   PressButtons,BUTTON_UP
        goto    Led8b

        ;wait for LED8 to go high
Led8D:  btfss   LED8
        goto    Led8D

        INPUT_HIGH              ;release the "Up" button if it was pressed

Led8b:  decfsz  LoopCount,f
        goto    WaitLED1        ;
        
        movlw   2               ;set Loop counter
        movwf   LoopCount       ;
        
        ; New1...New5 updated, check and see if the data is stable

        movf    Last1,w         ;      
        subwf   New1,w          ;       
        btfss   STATUS,Z        ;skip if no change
        goto    NotStable       ;       
        movf    Last2,w         ;      
        subwf   New2,w          ;       
        btfss   STATUS,Z        ;skip if no change
        goto    NotStable       ;       
        movf    Last3,w         ;      
        subwf   New3,w          ;       
        btfss   STATUS,Z        ;skip if no change
        goto    NotStable       ;       
        movf    Last4,w         ;      
        subwf   New4,w          ;       
        btfss   STATUS,Z        ;skip if no change
        goto    NotStable       ;       
        movf    Last5,w         ;      
        subwf   New5,w          ;       
        btfss   STATUS,Z        ;skip if no change
        goto    NotStable;

        movf    OutCount,w      ;
        btfss   STATUS,Z        ;
        goto    MainLoop        ;still sending last change, don't send now
        
        ; New1...New5 and stable, check and see if the data has changed
        movf    Old1,w          ;
        subwf   New1,w          ;
        btfss   STATUS,Z        ;skip if no change
        goto    NewData         ;
        movf    Old2,w          ;
        subwf   New2,w          ;
        btfss   STATUS,Z        ;skip if no change
        goto    NewData         ;
        movf    Old3,w          ;
        subwf   New3,w          ;
        btfss   STATUS,Z        ;skip if no change
        goto    NewData         ;
        movf    Old4,w          ;
        subwf   New4,w          ;
        btfss   STATUS,Z        ;skip if no change
        goto    NewData         ;
        movf    Old5,w          ;
        subwf   New5,w          ;
        btfsc   STATUS,Z        ;skip if changed
        goto    MainLoop        ;
        
        ;Send new data
NewData:        
        movf    New1,w          ;
        movwf   Old1
        movf    New2,w          ;
        movwf   Old2
        movf    New3,w          ;
        movwf   Old3
        movf    New4,w          ;
        movwf   Old4
        movf    New5,w          ;
        movwf   Old5

        movlw   d'6'            ;Send 5 bytes
        movwf   OutCount        ;
        movlw   low Old1        ;starting at Old1
        movwf   OutPtr          ;
        
        banksel PIE1            ;Bank 1
        bsf     PIE1,TXIE       ;enable TX interrupt
        banksel PIR1            ;Bank 0
        goto    MainLoop        ;Do it all again
        
NotStable:
        movf    New1,w          ;
        movwf   Last1
        movf    New2,w          ;
        movwf   Last2
        movf    New3,w          ;
        movwf   Last3
        movf    New4,w          ;
        movwf   Last4
        movf    New5,w          ;
        movwf   Last5
        goto    MainLoop        ;Do it all again
        
        
     end
     
