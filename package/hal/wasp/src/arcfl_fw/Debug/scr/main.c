/* Project Name:  MC13804
/  MCU:     MB95564K
/  Desc:    WIFI Slow Cooker
/
/---------------------------------------------------------------------------
  MAIN.C
  - description
  - See README.TXT for project description and disclaimer.
/---------------------------------------------------------------------------*/

#ifndef SIMULATE_DEVICE
#include "mb95560.h"
#endif

#define keepwarm_hour   24
#define low_hour     12
#define hi_hour      8
#define pul_wid      100
#define t_300us      150

#define LED1_PIN  PDR6_P64
#define LED2_PIN  PDR6_P63
#define LED3_PIN  PDR6_P62
#define TRC_PIN   PDRG_PG1

volatile unsigned char RS_buf,wemo_statu;
volatile unsigned char scan, mode, led, tmp_dat, tmp_con, trg_cnt;
volatile unsigned char adc_step, tmp_kwm, tmp_low, tmp_hi, second;
volatile unsigned char tx_step,tx_seq,monitor_timer;
volatile int  work_tmr, adc_buf, adc_min, adc_max,rec_sum;

union {
      unsigned int   word;
      struct { char  byte1;
         char  byte2;
              }byte;
   } ADC_DAT, CW0,VAL, WK_TMR;

#define  adc_dat  (ADC_DAT.word)
#define  adc_dat1 (ADC_DAT.byte.byte1)
#define  adc_dat0 (ADC_DAT.byte.byte2)

#define  m  (CW0.byte.byte1)
#define  n  (CW0.byte.byte2)
#define  cw (CW0.word)

#ifndef SIMULATE_DEVICE
// Fujitsu processor, big endian
#define  tx_sum0  (VAL.byte.byte2)
#define  tx_sum1  (VAL.byte.byte1)
#else
// Simulator running on a PC, little endian
#define  tx_sum0  (VAL.byte.byte1)
#define  tx_sum1  (VAL.byte.byte2)
#endif

#define  tx_sum   (VAL.word)

#define  minute   (WK_TMR.word)
#ifndef SIMULATE_DEVICE
// Fujitsu processor, big endian
#define  min_dat1 (WK_TMR.byte.byte1)
#define  min_dat0 (WK_TMR.byte.byte2)
#else
#define  min_dat1 (WK_TMR.byte.byte2)
#define  min_dat0 (WK_TMR.byte.byte1)
#endif

union {
   __BYTE   byte;
   struct {
      __BYTE   BIT0:1;
      __BYTE   BIT1:1;
      __BYTE   BIT2:1;
      __BYTE   BIT3:1;
      __BYTE   BIT4:1;
      __BYTE   BIT5:1;
      __BYTE   BIT6:1;
      __BYTE   BIT7:1;
      }bit;
   } FLAG_REG, LED_REG, KEY_REG, REC_NUM, UART_CON;

#define  flag  (FLAG_REG.byte)
#define  RUN_FLG  (FLAG_REG.bit.BIT0)
#define  TRC_FLG  (FLAG_REG.bit.BIT1)
#define  TRG_FLG  (FLAG_REG.bit.BIT2)
#define  BAT_FLG  (FLAG_REG.bit.BIT3)
#define  TMR_FLG  (FLAG_REG.bit.BIT4)
#define  RXD_FLG  (FLAG_REG.bit.BIT5)
#define  TMP_FLG  (FLAG_REG.bit.BIT6)

#define  RX_DLE_FLG  (UART_CON.bit.BIT0)
#define  TX_DLE_FLG  (UART_CON.bit.BIT1)

#define  led   (LED_REG.byte)
#define  LED1  (LED_REG.bit.BIT2)
#define  LED2  (LED_REG.bit.BIT1)
#define  LED3  (LED_REG.bit.BIT0)

#define key_cnt   (KEY_REG.byte)
#define KEY_FLG   (KEY_REG.bit.BIT7)

#define rec_num   (REC_NUM.byte)
#define frame_flag0  (REC_NUM.bit.BIT2)
#define frame_flag1  (REC_NUM.bit.BIT1)

volatile unsigned char freq, freq_cnt, int_count,tx_num;
   char i,j,k;
   char *rx_adr,*rec_end;
   char *tx_wr;
   const char *tx_adr;
union {
   char TX_BANK[16];
   struct {
      char CR_0;
      int WD_12,WD_34,WD_56;
          }WD0;
   struct {
      int WD_01,WD_23,WD_45,WD_67;
           }WD1;
   }TRA_BNK;


#define tx_bank   (TRA_BNK.TX_BANK)
#define respon (TRA_BNK.TX_BANK[0])
#define error_num (TRA_BNK.TX_BANK[1])
#define wd_12  (TRA_BNK.WD0.WD_12)
#define wd_34  (TRA_BNK.WD0.WD_34)
#define wd_56  (TRA_BNK.WD0.WD_56)
#define wd_01  (TRA_BNK.WD1.WD_01)
#define wd_23  (TRA_BNK.WD1.WD_23)
#define wd_45  (TRA_BNK.WD1.WD_45)
#define wd_67  (TRA_BNK.WD1.WD_67)

char rec_bank[16];
#define rec_seq   (rec_bank[0])
#define rec_com   (rec_bank[1])
#define var_id (rec_bank[2])


const char rep_wasp_ver[]={0x81,0x00,'1','.','0','2',0x00};
const char fw_ver[] = "1.00b";
const char dev_desc[] = "Slow Cooker";


// Unique value assigned by Belkin to identify this product
const char dev_id[]={0x01,0x00,0x01,0x00};

extern const char tmp_tab[172];
extern const char tab_kwm[10];
extern const char tab_low[10];
extern const char tab_hi[10];
extern const char rep_get_vals[];
extern const char rep_get_val_01[];
extern const char rep_get_att0[];
extern const char rep_get_att1[];
extern const char rep_get_enum_off[];
extern const char rep_get_enum_keepwarm[];
extern const char rep_get_enum_low[];
extern const char rep_get_enum_high[];
//#define tab_adr
//const  *tab_adr=tmp_tab-83;

//------------------------- SUB ROUTINES ---------------------------------
#ifndef SIMULATE_DEVICE
void Init_Clock(void)
{
   SYSC=0x81;
   WATR=0xFC;
   SYCC=0x08;
   while(!SYCC2_MRDY);
}


void Initsystem (void)
{
   PDR0 = 0xff;
   DDR0 = 0x08;
   PUL0 = 0x20;
   AIDRL= 0xfc;
   PDR1 = 0x04;
   DDR1 = 0x04;
   PDR6 = 0x1c;
   DDR6 = 0x1c;
   PUL6 = 0x0;
   PDRF = 0x0;
   DDRF = 0x0;
   PDRG = 0x0;
   PULG = 0x0;
   DDRG = 0x02;

   RS_buf = RSRR;
   WPCR   = 0x0f;
   ADC1   = 0x10;
   ADC2   = 0x41;
   TMCR0  = 0x10;
   TMCR1  = 0x0;
   T01DR  = 0xFF;
   T00DR  = 0xFF;
   T00CR0 = 0x0;
   T00CR1 = 0x0;
   T01CR0 = 0x0;
   T10CR0 = 0x0;
   T11CR0 = 0x0;

   EIC10  = 0x0;
   EIC20  = 0x0;
   EIC30  = 0x50;
   WDTC   = 0x35;

   do{}
   while( EIC30_EIR1 == 0 );

   EIC30_EIR1 = 0;
   T00CR1_STA = 1;

   i=5;
   do{         //AC freq check
      do{}
      while(EIC30_EIR1==0);
      m = T01DR;
      T00CR1_STA = 0;
      EIC30_EIR1 = 0;
      T00CR1_STA = 1;
      if ( m > 143 )
         n++;
     }
   while(--i);

   if ( n > 3 )
      freq = 100;
   else
      freq = 120;

   T00CR1_STA=0;
   ADC1_AD=1;
   ADC1_ADI=0;
   i=5;
   do{         //option check
      while(!ADC1_ADI)
      adc_dat+=ADD;
      ADC1_AD=1;
      ADC1_ADI=0;
     }
   while(--i);

   n=(adc_dat1>>1)&0x0F;   //load the control temp
   tmp_kwm=*(n+tab_kwm);
   tmp_low=*(n+tab_low);
   tmp_hi =*(n+tab_hi);

   // Initialize UART
   /* BGR  = 207; Reloadvalue = 207 (4MHz, 9600Baud)
   eur style BGR(FBC) = jp style BGR1(FBC)+BGR0(FBD)  */
   BGR1 = 0x0;
   BGR0 = 207;
   SMR  = 0x05;      // enable SOT, Reset, LIN mode
   SSR  = 0x02;      // enable reception interrupt
   SCR  = 0x17;      // enable transmit
   ESCR = 0x0;    //

   ADC1 = 0x0;
   EIC30=0x70;
   TMCR0=0x0;
}
#endif
/*****************************************/

void do_off(void)
{
   mode=0;
   led=0;
   RUN_FLG=0;
   TRC_FLG=0;
   T00CR0=0;
   T00CR1=0;
   TRC_PIN=0;
   work_tmr=0;
   minute=0;
   second=0;
}


void do_mode(void)
{
   second=60;
   switch(mode)
   {
      case 1:  tmp_con=tmp_kwm;
         minute=360; //6 hour
         led=0x01;
         TRC_FLG=0;
         RUN_FLG=1;
         break;
      case 2:  tmp_con=tmp_low;
         minute=600; //10 hour
         led=0x02;
         TRC_FLG=0;
         RUN_FLG=1;
         break;
      case 3:  tmp_con=tmp_hi;
         minute=360; //6 hour
         led=0x04;
         TRC_FLG=0;
         RUN_FLG=1;
         break;
      default: do_off();
   }
}

/****************************************/

#ifndef SIMULATE_DEVICE
void chk_key(void)
{
   if((PDR0&0X20)==0)
      { if(KEY_FLG==0)
         { if(--key_cnt==0)
            {
               key_cnt=0xFD;
               ++mode;
               do_mode();
            }
         }
       else
         key_cnt=0xFD;
      }
   else
      {
         if(KEY_FLG==1)
            {
               if(++key_cnt==0)
                  key_cnt=0x03;
            }
         else
            key_cnt=0x03;
      }
}

/****************************************/
void chk_adc(void)
{
   adc_buf=ADD;
   ADC1_AD=1;
   switch(++adc_step)
   {
      case 1:  adc_min=adc_buf;
         adc_dat=0;
         break;
      case 2:  if(adc_buf>=adc_min)
            adc_max=adc_buf;
         else
            {
               adc_max=adc_min;
               adc_min=adc_buf;
            }
         break;
      case 11: adc_step=0;
         if(adc_dat>=0x1FA0)
            tmp_dat=192;
         else if(adc_dat>=0x0A60)
            tmp_dat=*((((adc_dat>>5)&0x00ff)-83)+tmp_tab);
         else if(adc_dat>0x51)
            tmp_dat=62;
         else
            tmp_dat=0;
         break;
      default: if(adc_buf>adc_min)
            if(adc_buf>=adc_max)
               {
                  adc_dat+=adc_max;
                  adc_max=adc_buf;
               }
            else
               adc_dat+=adc_buf;
         else
            {
               adc_dat+=adc_min;
               adc_min=adc_buf;
            }
   }
}
#endif
/******************************************/


/******************************************/
void tx_err(void)
{
   tx_step=0;
   SSR_TIE=1;
   tx_num=3;
   tx_adr=tx_bank;

   tx_seq=0x01;
   respon=0x80;      //response
   error_num=0x04;      //error_num
}

/******************************************/
void chk_rec(void)
{
   if(++monitor_timer==250)
   {
      monitor_timer=0;
      rec_num=0;
      rx_adr=rec_bank;
      RX_DLE_FLG=0;
      //tx_err();
   }
}

/*******************************************/

/*******************************************/
void tx_copy(const char *From,int Len)
{
   tx_num+=Len;
      while(Len-- > 0) {
         *tx_wr++ = *From++;
      }
}

/*******************************************/
void do_rec(void)
{
   RXD_FLG=0;
#ifndef SIMULATE_DEVICE
// Fujitsu processor, big endian
   m=*(--rx_adr);
   n=*(--rx_adr);
#else
// Simulator running on a PC, little endian
   n=*(--rx_adr);
   m=*(--rx_adr);
#endif
   if (rec_num==0x06&&cw==rec_sum)
   {
      tx_step=0;
      SSR_TIE=1;
      tx_seq=rec_seq+0x1b;
      m=rec_com;
      n=var_id;
      if(m==0)
         {
            respon=0x80;   //NOP
            error_num=0x0;

            tx_adr=tx_bank;
            tx_num=2;
         }

      else if(m==1)
         {
            respon=0x81;
            error_num=0;
            tx_adr=tx_bank;
            tx_num=2;
            tx_wr=&tx_bank[2];
            switch(n)
            {
               case 1:  tx_copy(dev_id,sizeof(dev_id));
                  break;
               case 2:  tx_copy(fw_ver,sizeof(fw_ver));
                                 break;
                           case 3:  tx_copy(dev_desc,sizeof(dev_desc));
                              break;
                           case 6:  tx_copy(&rep_wasp_ver[2],sizeof(rep_wasp_ver)-2);
                                 break;
                              case 5:  tx_bank[2]=0x03;
                                 tx_num=3;
                                 break;
                            case 0x80:
                                 tx_bank[2]=mode;
                                 tx_num=3;
                                 break;
                            case 0x81:
                                 tx_bank[2]=min_dat0;
                  tx_bank[3]=min_dat1;
                  tx_num=4;
                  break;
            }
         }

      else if(m==3)
         {
            respon=0x83;
            error_num=0;
            tx_adr=tx_bank;
            tx_num=2;
            rx_adr=&var_id;
            tx_wr=&tx_bank[2];

            while(rx_adr < rec_end)
            {
                        switch(*rx_adr++)
                        {
                           case 1:  //WASP_VAR_DEVID;
                              tx_copy(dev_id,sizeof(dev_id));
                              break;

                              case 2:  // WASP_VAR_FW_VER
                              tx_copy(fw_ver,sizeof(fw_ver));
                                 break;

                              case 3:  // WASP_VAR_DEV_DESC
                                 tx_copy(dev_desc,sizeof(dev_desc));
                               break;

                              case 6:  // WASP_VAR_WASP_VER
                                 tx_copy(&rep_wasp_ver[2],sizeof(rep_wasp_ver)-2);
                               break;

                           case 5:  *tx_wr++=0x03;
                              tx_num+=1;
                              break;

                              case 0x80:  // Mode
                                 *tx_wr++ = mode;
                                 tx_num+=1;
                               break;

                              case 0x81:  // Time, LSB first, then MSB
                                 *tx_wr++ = min_dat0;
                                 *tx_wr++ = min_dat1;
                                 tx_num+=2;
                               break;
                           }
                  }
                }

      else
         {
#ifdef SIMULATE_DEVICE
// Simulator running on a PC, little endian, swap the bytes
         n=rec_com;
         m=var_id;
#endif
            switch(cw)
            {
            case 0x0404:   respon=0x84;
                  error_num=0x0;
                  tx_adr=tx_bank;
                  tx_num=2;
                  wemo_statu=rec_bank[3];
                  break;

            case 0x0405:   respon=0x84;
                  error_num=0x0;
                  tx_adr=tx_bank;
                  tx_num=2;
                  break;


            case 0x0480:   respon=0x84;   //0480
                  tx_adr=tx_bank;
                     tx_num=2;
                     if(rec_bank[3]<4)
                     {
                        error_num=0x0;
                        mode=rec_bank[3];
                        do_mode();
                     }
                     else
                        error_num=0x02;
                     break;

            case 0x0481:   respon=0x84;   //0481
                  error_num=0x0;

                     tx_adr=tx_bank;
                     tx_num=2;
                     min_dat0=rec_bank[3];
                     min_dat1=rec_bank[4];
                     second=60;
                     break;

            case 0x0b80:   tx_adr=rep_get_att0; //0b80
                  tx_num=12;
                  break;

            case 0x0b81:   tx_adr=rep_get_att1; //0b81
                  tx_num=19;
                  break;

            case 0x0b82:   respon=0x8b;      //0b82
                  error_num=0x02;

                  tx_adr=tx_bank;
                  tx_num=2;
                  break;

            case 0x0c80:  
                  switch(rec_bank[3])
                  {
                     case 0:
                        tx_adr=rep_get_enum_off;
                        tx_num=5;      //0c80
                        break;

                     case 1:
                        tx_adr=rep_get_enum_keepwarm;
                        tx_num=11;
                        break;

                     case 2:
                        tx_adr=rep_get_enum_low;
                        tx_num=5;
                        break;

                     case 3:
                        tx_adr=rep_get_enum_high;
                        tx_num=6;
                        break;
                  }
                  break;

            default:    respon=rec_com|0x80;
                  error_num=0x01;

                  tx_adr=tx_bank;
                  tx_num=2;
                  break;
            }
         }

      }
   else
      {
         tx_step=0;
         SSR_TIE=1;
         tx_num=2;
         tx_adr=tx_bank;

         respon=rec_com|0x80; //response
         error_num=0x04;      //error_num
      }
}

/****************************************/
void tmp_reg(void)
{
   if(TRC_FLG==0)
      {
         if(tmp_dat<tmp_con)
            TRC_FLG=1;
      }
   else
      {
         if(tmp_dat>=(tmp_con+2))
         TRC_FLG=0;
      }
}

void do_tmr(void)
{
   BAT_FLG=0;
   if(RUN_FLG==1)
      {
      tmp_reg();
      if(--second==0)
         {
         second=60;
         work_tmr++;
         if(--minute==0)
            {
            if(mode==1)
               do_off();
            else
               {
               tmp_con=tmp_kwm;
               minute=360; //10 hour
               led=0x01;
               mode=1;
               work_tmr=0;
               }
            }
         }
      }
}


/*****************************************************************************/
/* Main Routine */
/*****************************************************************************/

#ifndef SIMULATE_DEVICE
void main(void)
{
   Init_Clock();     //MCLK = source clock = 4Mhz (Main oscillator)
   Initsystem();
   InitIrqLevels();     // initialise Interrupt level register and IRQ vector table
   TBTC = 0x4D;
   __EI();        // global interrupt enable
   __set_il(3);      // set global interrupt mask to allow all IRQ levels

   do {
      do{}
      while ( TMR_FLG==0 );   //4ms

      TMR_FLG=0;
      WDTC = 0x35;
      chk_rec();
      chk_adc();
      if(scan==0)
         chk_key();
      if(RXD_FLG==1)
         do_rec();
      if(BAT_FLG==1)
         do_tmr();

      }
   while(int_count<40);
}

/*****************************************************************************/
/* Interrupts                                                                */
/*****************************************************************************/
__interrupt void int_acz(void)
{
   EIC30_EIR1=0;
   if(int_count!=0)
      {int_count=0;
      if(--freq_cnt==0)
         {
            freq_cnt=freq;
            BAT_FLG=1;
         }
      if(TRC_FLG==1)
         {
            trg_cnt=3;
            TRC_PIN=1;
            TRG_FLG=0;
            T00DR=pul_wid;
            T00CR0=0xA0;
            T00CR1=0xA0;
         }
      }
}

//---------------------------------------------
__interrupt void int_t00(void)
{
   T00CR1_IF=0;
   if(TRG_FLG==0)
      {
         TRC_PIN=0;
         if(--trg_cnt)
            {
               TRG_FLG=1;
               T00DR=t_300us;
               T00CR1_STA=1;
            }
         else
            T00CR1=0;
      }
   else
      {
         TRC_PIN=1;
         TRG_FLG=0;
         T00DR=pul_wid;
         T00CR0=0xA0;
         T00CR1=0xA0;
      }
}
#endif

//------------------------------------------------------
__interrupt void int_uart_rx(void)
{
   if(SSR_RDRF)
      {
         j=RDR_TDR;
         monitor_timer=0;
         if(j==0x10)
         {
            if(RX_DLE_FLG)
            {
               RX_DLE_FLG=0;
               *rx_adr=0x10;
               rx_adr++;
               if(frame_flag0)
               {  rec_num++;
                  if(frame_flag1)
                     RXD_FLG=1;
                  return;
               }
               else
                  rec_sum+=0x10;
                  return;
            }
            else
               RX_DLE_FLG=1;
               return;
         }

         else
         {  if(RX_DLE_FLG==1)
            {
               RX_DLE_FLG=0;
               if(j==0x01)
               {  rx_adr=rec_bank;
                  rec_num=0;
                  rec_sum=0;  }
               else if(j==0x03)
               {  rec_num=4;
                  rec_end=rx_adr;   }
               return;
            }
            else
            {
               *rx_adr=j;
               rx_adr++;
               if(frame_flag0)
               {  rec_num++;
                  if(frame_flag1)
                     RXD_FLG=1;
                  return;
               }
               else
                  rec_sum+=j;
                  return;
            }

         }


      }
   else
      SCR_CRE=1;

}

//---------------------------------------------------
__interrupt void int_uart_tx(void)
{
   if(SSR_TDRE)
      {
         if(TX_DLE_FLG==0)
            {
               switch(tx_step)
               {
               case 0:  RDR_TDR=0x10;
                     tx_step=1;
                     break;
               case 1:  RDR_TDR=0x01;
                     tx_step=2;
                     break;
               case 2:  if((RDR_TDR=tx_seq)==0x10)
                        TX_DLE_FLG=1;
                     tx_step=3;
                     tx_sum=tx_seq;
                     break;
               case 3:  if((RDR_TDR=k=*tx_adr)==0x10)
                        TX_DLE_FLG=1;
                     tx_sum+=k;
                     tx_adr++;
                     if(--tx_num==0)
                         tx_step=4;
                     break;
               case 4:  RDR_TDR=0X10;
                     tx_step=5;
                     break;
               case 5:  RDR_TDR=0X03;
                     tx_step=6;
                     break;
               case 6:if((RDR_TDR=tx_sum0)==0x10)
                        TX_DLE_FLG=1;
                     tx_step=7;
                     break;
               case 7:  if((RDR_TDR=tx_sum1)==0x10)
                     { TX_DLE_FLG=1; tx_step=8; }
                     else
                        SSR_TIE=0;
                     break;
               default: SSR_TIE=0;
                        tx_step=0;
               }
            }
         else
            {
               TX_DLE_FLG=0;
               RDR_TDR=0x10;
            }
      }
}

#ifndef SIMULATE_DEVICE
//--------------------------------------------------
__interrupt void int_timebase(void)
{
      TBTC_TBIF=0;
      TMR_FLG=1;
      int_count++;
      switch ( ++scan )
      {
         case 1:  LED3_PIN=1;
            if(LED1==1)
            LED1_PIN=0;
            break;
         case 2:  LED1_PIN=1;
            if(LED2==1)
            LED2_PIN=0;
            break;
         default: LED2_PIN=1;
            scan=0;
            if(LED3==1)
            LED3_PIN=0;
      }
}
#endif


//**********************************************************
// Data Table
//**********************************************************
//#pragma segment CODE=TABLE, locate=0xd100;

extern const char tab_kwm[10]={75, 80, 80, 80, 80, 85, 85, 85, 85, 90};
extern const char tab_low[10]={115,120,120,125,125,120,120,125,125,130};
extern const char tab_hi[10] ={125,130,135,130,135,130,135,130,135,140};

extern const char rep_get_val_01[]={0x81,0x00,0x31,0x2e,0x30,0x32};
extern const char rep_get_att0[]={0x8b,0x00,0x01,0x04,0xe8,0x03,0x00,0x03,0x4d,0x6f,0x64,0x65};
extern const char rep_get_att1[]={0x8b,0x00,0x12,0x04,0xe8,0x03,0x00,0x00,0xa0,0x05,'C','o','o','k',' ','T','i','m','e'};

extern const char rep_get_enum_off[]= {0x8c,0x00,0x4f,0x66,0x66};
extern const char rep_get_enum_keepwarm[]={0x8c,0x00,0x4b,0x65,0x65,0x70,0x20,0x57,0x61,0x72,0x6d};
extern const char rep_get_enum_low[]= {0x8c,0x00,0x4c,0x6f,0x77};
extern const char rep_get_enum_high[]={0x8c,0x00,0x48,0x69,0x67,0x68};

extern const char tmp_tab[172]={
         63, 63, 64, 64, 65, 65,
         66, 66, 67, 67, 68, 68,
         69, 69, 69, 70, 70, 71,
         71, 72, 72, 73, 73, 73,
         74, 74, 75, 75, 76, 76,
         77, 77, 77, 78, 78, 79,
         79, 80, 80, 81, 81, 82,
         82, 82, 83, 83, 84, 84,
         85, 85, 86, 86, 87, 87,
         87, 88, 88, 89, 89, 90,
         90, 91, 91, 92, 92, 93,
         93, 94, 94, 95, 95, 96,
         96, 97, 97, 98, 98, 99,
         99, 100, 100, 101, 101, 102,
         103, 103, 104, 104, 105, 105,
         106, 106, 107, 108, 108, 109,
         109, 110, 111, 111, 112, 113,
         113, 114, 115, 115, 116, 117,
         117, 118, 119, 120, 120, 121,
         122, 123, 123, 124, 125, 126,
         127, 128, 128, 129, 130, 131,
         132, 133, 134, 135, 136, 137,
         138, 139, 140, 142, 143, 144,
         145, 147, 148, 149, 150, 150,
         150, 150, 150, 150, 150, 150,
         150, 150, 150, 150, 150, 150,
         150, 150, 150, 150, 150, 150,
         150, 150, 150, 150, 150, 150,
         150, 150, 150, 150};



