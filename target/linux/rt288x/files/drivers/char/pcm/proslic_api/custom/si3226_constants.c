/*
Copyright (c) 2008 Silicon Laboratories, Inc.
2008-03-18 16:06:28 */

#include "proslic.h"
#include "si3226.h"
Si3226_General_Cfg Si3226_General_Configuration  = {
BO_DCDC_QCUK,
0x8720000L,	/* VBATR_EXPECT */
0x3D70000L,	/* VBATH_EXPECT */
0x0L,	    /* DCDC_FSW_VTHLO */
0x0L,	    /* DCDC_FSW_VHYST */
0x1300000L,	/* DCDC_VREF_MIN */
0x5A00000L,	/* DCDC_VREF_MIN_RING */
0x200000L,	/* DCDC_FSW_NORM */
0x200000L,	/* DCDC_FSW_NORM_LO */
0x200000L,	/* DCDC_FSW_RING */
0x100000L,	/* DCDC_FSW_RING_LO */
0xD980000L,	/* DCDC_DIN_LIM */
0xC00000L,	/* DCDC_VOUT_LIM */
0x0L,	    /* DCDC_DCFF_ENABLE */
0x500000L,	/* DCDC_UVHYST */
0x600000L,	/* DCDC_UVTHRESH */
0xB00000L,	/* DCDC_OVTHRESH */
0x1F00000L,	/* DCDC_OITHRESH */
0x100000L,	/* DCDC_SWDRV_POL */
0x300000L,	/* DCDC_SWFET */
0x600000L,	/* DCDC_VREF_CTRL */
0x200000L,	/* DCDC_RNGTYPE */
0x0L,	    /* DCDC_ANA_GAIN */
0x0L,	    /* DCDC_ANA_TOFF */
0x0L,	    /* DCDC_ANA_TONMIN */
0x0L,	    /* DCDC_ANA_TONMAX */
0x0L,	    /* DCDC_ANA_DSHIFT */
0x0L,	    /* DCDC_ANA_LPOLY */
0x7FeF000L,	/* COEF_P_HVIC */
0x68D000L,	/* P_TH_HVIC */
0x0,	    /* CM_CLAMP */
0x3F,	    /* AUTO */
0x0,	    /* DAA_CNTL */
0x7F,	    /* IRQEN1 */
0xFF,	    /* IRQEN2 */
0xFF,	    /* IRQEN3 */
0xFF,	    /* IRQEN4 */
0x0,	    /* ENHANCE */
0,	        /* DAA_ENABLE */
0x8000000L,	/* SCALE_KAUDIO */
0x151EB80L,	/* AC_ADC_GAIN */
};

Si3226_DTMFDec_Cfg Si3226_DTMFDec_Presets[] = {
	{0x2d40000L,0x1a660000L,0x2d40000L,0x6ba0000L,0x1dcc0000L,0x33f0000L,0xbd30000L,0x19d20000L,0x4150000L,0x188F0000L,0x4150000L,0xd970000L,0x18620000L,0xf1c0000L}
};
Si3226_GPIO_Cfg Si3226_GPIO_Configuration = {
	0,0,0,0,0,0,0
};

Si3226_CI_Cfg Si3226_CI_Presets [] = {
	{0}
};
Si3226_audioGain_Cfg Si3226_audioGain_Presets [] = {
	{0x1377080L,0},
	{0x80C3180L,0}
};

Si3226_Ring_Cfg Si3226_Ring_Presets[] ={
{ /* RING_F20_45VRMS_0VDC_LPR */
0x00050000L,		/* RTPER */
0x07EFD9D5L,		/* RINGFR */
0x001BD493L,		/* RINGAMP */
0x00000000L,		/* RINGPHAS */
0x00000000L,		/* RINGOF */
0x15E5200EL,		/* SLOPE_RING */
0x00D16348L,		/* IRING_LIM */
0x0068C6BBL,		/* RTACTH */
0x0FFFFFFFL,		/* RTDCTH */
0x00006000L,		/* RTACDB */
0x00006000L,		/* RTDCDB */
0x0064874DL,		/* VOV_RING_BAT */
0x0064874DL,		/* VOV_RING_GND */
0x049CE106L,		/* VBATR_EXPECT */
0x80,		/* RINGTALO */
0x3E,		/* RINGTAHI */
0x00,		/* RINGTILO */
0x7D,		/* RINGTIHI */
0x01000547L,		/* ADAP_RING_MIN_I */
0x00003000L,		/* COUNTER_IRING_VAL */
0x00051EB8L,		/* COUNTER_VTR_VAL */
0x0163063FL,		/* CONST_028 */
0x019E31F4L,		/* CONST_032 */
0x01F108BEL,		/* CONST_036 */
0x026D4AEEL,		/* CONST_046 */
0x00370000L,		/* RRD_DELAY */
0x00190000L,		/* RRD_DELAY2 */
0x01893740L,		/* DCDC_VREF_MIN_RNG */
0x58,		/* RINGCON */
0x01,		/* USERSTAT */
0x024E7083L,		/* VCM_RING */
0x024E7083L,		/* VCM_RING_FIXED */
0x003126E8L,		/* DELTA_VCM */
0x200000L,		/* DCDC_RNGTYPE */
},
{ /* RING_F20_45VRMS_0VDC_BAL */
0x00050000L,		/* RTPER */
0x07EFD9D5L,		/* RINGFR */
0x001BD493L,		/* RINGAMP */
0x00000000L,		/* RINGPHAS */
0x00000000L,		/* RINGOF */
0x15E5200EL,		/* SLOPE_RING */
0x00D16348L,		/* IRING_LIM */
0x0068C6BBL,		/* RTACTH */
0x0FFFFFFFL,		/* RTDCTH */
0x00006000L,		/* RTACDB */
0x00006000L,		/* RTDCDB */
0x0064874DL,		/* VOV_RING_BAT */
0x0064874DL,		/* VOV_RING_GND */
0x0565EFA2L,		/* VBATR_EXPECT */
0x80,		/* RINGTALO */
0x3E,		/* RINGTAHI */
0x00,		/* RINGTILO */
0x7D,		/* RINGTIHI */
0x01000547L,		/* ADAP_RING_MIN_I */
0x00003000L,		/* COUNTER_IRING_VAL */
0x00051EB8L,		/* COUNTER_VTR_VAL */
0x0163063FL,		/* CONST_028 */
0x019E31F4L,		/* CONST_032 */
0x01F108BEL,		/* CONST_036 */
0x026D4AEEL,		/* CONST_046 */
0x00370000L,		/* RRD_DELAY */
0x00190000L,		/* RRD_DELAY2 */
0x01893740L,		/* DCDC_VREF_MIN_RNG */
0x58,		/* RINGCON */
0x00,		/* USERSTAT */
0x02B2F7D1L,		/* VCM_RING */
0x02B2F7D1L,		/* VCM_RING_FIXED */
0x003126E8L,		/* DELTA_VCM */
0x200000L,		/* DCDC_RNGTYPE */
},
{ /* RING_F20_55VRMS_48VDC_LPR */
0x00050000L,		/* RTPER */
0x07EFD9D5L,		/* RINGFR */
0x002203D0L,		/* RINGAMP */
0x00000000L,		/* RINGPHAS */
0x049A5F69L,		/* RINGOF */
0x15E5200EL,		/* SLOPE_RING */
0x00D16348L,		/* IRING_LIM */
0x032EDF91L,		/* RTACTH */
0x0038E38EL,		/* RTDCTH */
0x00006000L,		/* RTACDB */
0x00006000L,		/* RTDCDB */
0x007ADE42L,		/* VOV_RING_BAT */
0x007ADE42L,		/* VOV_RING_GND */
0x08B5BA6CL,		/* VBATR_EXPECT */
0x80,		/* RINGTALO */
0x3E,		/* RINGTAHI */
0x00,		/* RINGTILO */
0x7D,		/* RINGTIHI */
0x0138EA01L,		/* ADAP_RING_MIN_I */
0x00003000L,		/* COUNTER_IRING_VAL */
0x00051EB8L,		/* COUNTER_VTR_VAL */
0x01A40BA6L,		/* CONST_028 */
0x01EA0D97L,		/* CONST_032 */
0x024C104FL,		/* CONST_036 */
0x02DF1463L,		/* CONST_046 */
0x00370000L,		/* RRD_DELAY */
0x00190000L,		/* RRD_DELAY2 */
0x01893740L,		/* DCDC_VREF_MIN_RNG */
0x58,		/* RINGCON */
0x01,		/* USERSTAT */
0x045ADD36L,		/* VCM_RING */
0x045ADD36L,		/* VCM_FIXED_RING */
0x003126E8L,		/* DELTA_VCM */
0x200000L,		/* DCDC_RNGTYPE */
}
};

Si3226_DCfeed_Cfg Si3226_DCfeed_Presets[] = {

    /* inputs: v_vlim=48.000, i_vlim=0.000, v_rfeed=42.800, i_rfeed=10.000,v_ilim=33.200, i_ilim=20.000,
 vcm_oh=27.000, vov_gnd=4.000, vov_bat=4.000, r_ring=320.000
 lcronhk=10.000, lcroffhk=12.000, lcrdbi=5.000, longhith=8.000, longloth=7.000, longdbi=5.000
 lcrmask=80.000, lcrmask_polrev=80.000, lcrmask_state=200.000, lcrmask_linecap=200.000 */
{ /* DCFEED_48V_20MA */
0x1DDF41C9L,    /* SLOPE_VLIM */
0x1EF68D5EL,    /* SLOPE_RFEED */
0x40A0E0L,      /* SLOPE_ILIM */
0x18AAD168L,    /* SLOPE_DELTA1 */
0x1CA39FFAL,    /* SLOPE_DELTA2 */
0x5A38633L,     /* V_VLIM */
0x5072476L,     /* V_RFEED */
0x3E67006L,     /* V_ILIM */
0xFDFAB5L,      /* CONST_RFEED */
0x5D0FA6L,      /* CONST_ILIM */
0x2D8D96L,      /* I_VLIM */
0x5B0AFBL,      /* LCRONHK */
0x6D4060L,      /* LCROFFHK */
0x8000L,        /* LCRDBI */
0x48D595L,      /* LONGHITH */
0x3FBAE2L,      /* LONGLOTH */
0x8000L,        /* LONGDBI */
0x120000L,       /* LCRMASK */
0x80000L,       /* LCRMASK_POLREV */
0x140000L,      /* LCRMASK_STATE */
0x140000L,      /* LCRMASK_LINECAP */
0x1BA5E35L,     /* VCM_OH */
0x418937L,      /* VOV_BAT */
0x418937L       /* VOV_GND */
},
{ /* DCFEED_48V_25MA */
0x1D46EA04L,    /* SLOPE_VLIM */
0x1EA4087EL,    /* SLOPE_RFEED */
0x40A0E0L,      /* SLOPE_ILIM */
0x1A224120L,    /* SLOPE_DELTA1 */
0x1D4FB32EL,    /* SLOPE_DELTA2 */
0x5A38633L,     /* V_VLIM */
0x5072476L,     /* V_RFEED */
0x3E67006L,     /* V_ILIM */
0x13D7962L,     /* CONST_RFEED */
0x74538FL,      /* CONST_ILIM */
0x2D8D96L,      /* I_VLIM */
0x5B0AFBL,      /* LCRONHK */
0x6D4060L,      /* LCROFFHK */
0x8000L,        /* LCRDBI */
0x48D595L,      /* LONGHITH */
0x3FBAE2L,      /* LONGLOTH */
0x8000L,        /* LONGDBI */
0x120000L,       /* LCRMASK */
0x80000L,       /* LCRMASK_POLREV */
0x140000L,      /* LCRMASK_STATE */
0x140000L,      /* LCRMASK_LINECAP */
0x1BA5E35L,     /* VCM_OH */
0x418937L,      /* VOV_BAT */
0x418937L       /* VOV_GND */
}
};

Si3226_Impedance_Cfg Si3226_Impedance_Presets[] ={
{  /* Si3226_600_0_0_30_0.txt - ZSYN_600_0_0 */
{0x08066500L, 0x1FF6FC80L, 0x00050B00L, 0x1FFE2680L,    /* TXACEQ */
 0x0800D000L, 0x1FF6C700L, 0x00025700L, 0x1FFB2300L},   /* RXACEQ */
{0x00151F00L, 0x1FE4D400L, 0x01E78880L, 0x00573380L,    /* ECFIR/ECIIR */
 0x020AF880L, 0x1EF79A80L, 0x00B35800L, 0x1F955080L,
 0x003E6600L, 0x1FBC7880L, 0x01E9AA00L, 0x05F27E00L},
{0x025AFA00L, 0x1C1FAD80L, 0x0185EC00L, 0x0A195E00L,    /* ZSYNTH */
 0x1DE3B180L, 0x31},
 0x08775F80L,   /* TXACGAIN */
 0x01450700L,   /* RXACGAIN */
 0x07B41580L, 0x184BEB00L, 0x07682B80L,    /* RXACHPF */
  0, 0 /* 0dB RX, 0dB TX */
},
{  /* Si3226_270_750_150_30_0.txt - ZSYN_270_750_150*/
  {0x071F7A80L,0x1FD01280L,0x00132700L,0x1FFEF980L, /* TXACEQ */
   0x0A8AA300L,0x1B9A5500L,0x008E7F00L,0x1FD7F300L}, /* RXACEQ */
  {0x0068CA00L,0x1EAE1E00L,0x0394FA00L,0x1E94AE80L,  /* ECFIR/ECIIR */
   0x0356D800L,0x0166CA80L,0x1EC16380L,0x01DE2780L,
   0x1F852300L,0x0046BE80L,0x02F17C80L,0x1EBCD280L},
  {0x028A0C00L,0x19EE4580L,0x03876100L,0x0A762700L, /* ZSYNTH */
   0x1D87A380L,0x93},
  0x08000000L, /* TXACGAIN */
  0x0109C280L, /* RXACGAIN */
   0x07BC6F00L,0x18439180L,0x0778DE00L,  /* RXHPF */
  0, 0 /* 0dB RX, 0dB TX */
},
{  /* Si3226_370_620_310_30_0.txt - ZSYN_370_620_310 */
  {0x07E59E80L,0x1FD33400L,0x1FFDF800L,0x1FFD8300L, /* TXACEQ */
   0x09F38000L,0x1C1C5A00L,0x1F94D700L,0x1FDE5800L}, /* RXACEQ */
  {0x00234480L,0x1F9CDD00L,0x01F5D580L,0x1FF39000L,  /* ECFIR/ECIIR */
   0x02C17180L,0x1FBE2500L,0x00DFFE80L,0x00441A80L,
   0x003BF800L,0x1FC42400L,0x0D9EB380L,0x1A514580L},
  {0x003ED200L,0x1F5D6B80L,0x0063B100L,0x0F12E200L, /* ZSYNTH */
   0x18EC9380L,0x8B},
  0x08000000L, /* TXACGAIN */
  0x0127C700L, /* RXACGAIN */
   0x07B51200L,0x184AEE80L,0x076A2480L,  /* RXHPF */
  0, 0 /* 0dB RX, 0dB TX */
},
{  /* Si3226_220_820_120_30_0.txt - ZSYN_220_820_120 */
  {0x06E38480L,0x1FD33B00L,0x00069780L,0x1FFCAB80L, /* TXACEQ */
   0x0A78F680L,0x1BC5C880L,0x009AEA00L,0x1FD66D80L}, /* RXACEQ */
  {0x00378B00L,0x1F3FCA00L,0x02B5ED00L,0x1F2B6200L,  /* ECFIR/ECIIR */
   0x04189080L,0x1F8A4480L,0x01113680L,0x00373100L,
   0x001DAE80L,0x1FE02F00L,0x0C89C780L,0x1B689680L},
  {0x02391100L,0x1A886080L,0x033E3B00L,0x0A136200L, /* ZSYNTH */
   0x1DEA4180L,0x8C},
  0x08000000L, /* TXACGAIN */
  0x01019200L, /* RXACGAIN */
   0x07BD1680L,0x1842EA00L,0x077A2D00L,  /* RXHPF */
  0, 0 /* 0dB RX, 0dB TX */
},
{  /* Si3226_600_0_1000_30_0.txt - ZSYN_600_0_1000 */
  {0x07F83980L,0x00056200L,0x1FFEE880L,0x1FFB1900L, /* TXACEQ */
   0x08405000L,0x001DD200L,0x0003DB80L,0x00008700L}, /* RXACEQ */
  {0x1FEE8700L,0x009B2B00L,0x000C9680L,0x03700100L,  /* ECFIR/ECIIR */
   0x1F62E400L,0x01C77400L,0x1FBBCF80L,0x0089D500L,
   0x005CFF80L,0x1FA96E80L,0x0F679480L,0x18962A80L},
  {0x00657C00L,0x1F2FA580L,0x006ADE00L,0x0FE12100L, /* ZSYNTH */
   0x181ED080L,0x57},
  0x08618D80L, /* TXACGAIN */
  0x013E3400L, /* RXACGAIN */
   0x0717CE80L,0x18E83200L,0x062F9C80L,  /* RXHPF */
  0, 0 /* 0dB RX, 0dB TX */
},
{  /* Si3226_200_680_100_30_0.txt - ZSYN_200_680_100 */
  {0x073A7B00L,0x1FCEB400L,0x0002C680L,0x1FFD0780L, /* TXACEQ */
   0x09BA8580L,0x1D2DF780L,0x006F5000L,0x1FDFE200L}, /* RXACEQ */
  {0x0004B700L,0x000F9800L,0x01201200L,0x00E1D880L,  /* ECFIR/ECIIR */
   0x03314A00L,0x1E84A580L,0x029D2380L,0x1E6F3400L,
   0x00E99200L,0x1F121100L,0x0588BC00L,0x025CAE00L},
  {0x01415C00L,0x1C98C180L,0x0225A500L,0x0A138200L, /* ZSYNTH */
   0x1DEA2280L,0x8E},
  0x08000000L, /* TXACGAIN */
  0x010DFD80L, /* RXACGAIN */
   0x07BA2180L,0x1845DF00L,0x07744380L,  /* RXHPF */
  0, 0 /* 0dB RX, 0dB TX */
},
{  /* Si3226_220_820_115_30_0.txt - ZSYN_200_820_115 */
  {0x06D56380L,0x1FDF1900L,0x00095A00L,0x1FFDAA80L, /* TXACEQ */
   0x0A596300L,0x1C067880L,0x0095EF00L,0x1FD7AF00L}, /* RXACEQ */
  {0x00687000L,0x1EAE1800L,0x03983D80L,0x1EB14B00L,  /* ECFIR/ECIIR */
   0x037B3E80L,0x016FC900L,0x1ED60100L,0x01B17D80L,
   0x1FA20D00L,0x001CE900L,0x027D3380L,0x1DBDBA80L},
  {0x00246300L,0x1E5E0580L,0x017D2300L,0x0A138100L, /* ZSYNTH */
   0x1DEA2280L,0xA7},
  0x08000000L, /* TXACGAIN */
  0x01009500L, /* RXACGAIN */
   0x07BBEE80L,0x18441200L,0x0777DD80L,  /* RXHPF */
  0, 0 /* 0dB RX, 0dB TX */
}
};

Si3226_FSK_Cfg Si3226_FSK_Presets[] ={

    /* inputs: mark freq=1200.000, space freq2200.000, amp=0.220, baud=1200.000, startStopDis=0, interrupt depth = 0 */
    { 0x2232000L, 0x77C2000L, 0x3C0000L, 0x200000L, 0x6B60000L, 0x79C0000L,0, 0 }
};

Si3226_Tone_Cfg Si3226_Tone_Presets[] ={

    /* inputs:  freq1 = 350.000, amp1 = -18.000, freq2 = 440.000, amp2 = -18.000, ta1 = 0.000, ti1 = 0.000, ta2 = 0.000, ti2 = 0.000*/
    { {0x7B30000L, 0x3A000L, 0x0L, 0x0, 0x0, 0x0, 0x0}, {0x7870000L, 0x4A000L, 0x0L, 0x0, 0x0, 0x0, 0x0}, 0x66 },
    /* inputs:  freq1 = 480.000, amp1 = -18.000, freq2 = 620.000, amp2 = -18.000, ta1 = 0.500, ti1 = 0.500, ta2 = 0.500, ti2 = 0.500*/
    { {0x7700000L, 0x52000L, 0x0L, 0xA0, 0xF, 0xA0, 0xF}, {0x7120000L, 0x6A000L, 0x0L, 0xA0, 0xF, 0xA0, 0xF}, 0x66 },
    /* inputs:  freq1 = 480.000, amp1 = -18.000, freq2 = 440.000, amp2 = -18.000, ta1 = 2.000, ti1 = 4.000, ta2 = 2.000, ti2 = 4.000*/
    { {0x7700000L, 0x52000L, 0x0L, 0x80, 0x3E, 0x0, 0x7D}, {0x7870000L, 0x4A000L, 0x0L, 0x80, 0x3E, 0x0, 0x7D}, 0x66 },
    /* inputs:  freq1 = 480.000, amp1 = -18.000, freq2 = 620.000, amp2 = -18.000, ta1 = 0.300, ti1 = 0.200, ta2 = 0.300, ti2 = 0.200*/
    { {0x7700000L, 0x52000L, 0x0L, 0x60, 0x9, 0x40, 0x6}, {0x7120000L, 0x6A000L, 0x0L, 0x60, 0x9, 0x40, 0x6}, 0x66 },
    /* inputs:  freq1 = 480.000, amp1 = -18.000, freq2 = 620.000, amp2 = -18.000, ta1 = 0.200, ti1 = 0.200, ta2 = 0.200, ti2 = 0.200*/
    { {0x7700000L, 0x52000L, 0x0L, 0x40, 0x6, 0x40, 0x6}, {0x7120000L, 0x6A000L, 0x0L, 0x40, 0x6, 0x40, 0x6}, 0x66 }
};

Si3226_PCM_Cfg Si3226_PCM_Presets[] ={
	{ 0x3, 0x0, 0x0, 0x0 },
    /* inputs:  u-law narrowband positive edge, dtx positive edge, both disabled, tx hwy = A, rx hwy = A */
    { 0x1, 0x0, 0x0, 0x0 },
    { 0x0, 0x0, 0x0, 0x0 },
    { 0x8, 0x0, 0x0, 0x0 }
};
