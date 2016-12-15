/*

*/
#include "rt_config.h"


CH_FREQ_MAP CH_HZ_ID_MAP[]=
		{
			{1, 2412},
			{2, 2417},
			{3, 2422},
			{4, 2427},
			{5, 2432},
			{6, 2437},
			{7, 2442},
			{8, 2447},
			{9, 2452},
			{10, 2457},
			{11, 2462},
			{12, 2467},
			{13, 2472},
			{14, 2484},

			/*  UNII */
			{36, 5180},
			{40, 5200},
			{44, 5220},
			{48, 5240},
			{52, 5260},
			{56, 5280},
			{60, 5300},
			{64, 5320},
			{149, 5745},
			{153, 5765},
			{157, 5785},
			{161, 5805},
			{165, 5825},
			{167, 5835},
			{169, 5845},
			{171, 5855},
			{173, 5865},
						
			/* HiperLAN2 */
			{100, 5500},
			{104, 5520},
			{108, 5540},
			{112, 5560},
			{116, 5580},
			{120, 5600},
			{124, 5620},
			{128, 5640},
			{132, 5660},
			{136, 5680},
			{140, 5700},
						
			/* Japan MMAC */
			{34, 5170},
			{38, 5190},
			{42, 5210},
			{46, 5230},
					
			/*  Japan */
			{184, 4920},
			{188, 4940},
			{192, 4960},
			{196, 4980},
			
			{208, 5040},	/* Japan, means J08 */
			{212, 5060},	/* Japan, means J12 */   
			{216, 5080},	/* Japan, means J16 */
};

INT	CH_HZ_ID_MAP_NUM = (sizeof(CH_HZ_ID_MAP)/sizeof(CH_FREQ_MAP));

CH_DESC Country_Region0_ChDesc_2GHZ[] =
{
	{1, 11, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region1_ChDesc_2GHZ[] =
{
	{1, 13, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region2_ChDesc_2GHZ[] =
{
	{10, 2, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region3_ChDesc_2GHZ[] =
{
	{10, 4, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region4_ChDesc_2GHZ[] =
{
	{14, 1, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region5_ChDesc_2GHZ[] =
{
	{1, 14, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region6_ChDesc_2GHZ[] =
{
	{3, 7, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region7_ChDesc_2GHZ[] =
{
	{5, 9, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region31_ChDesc_2GHZ[] =
{
	{1, 11, CHANNEL_DEFAULT_PROP},
	{12, 3, CHANNEL_PASSIVE_SCAN},
	{}
};

CH_DESC Country_Region32_ChDesc_2GHZ[] =
{
	{1, 11, CHANNEL_DEFAULT_PROP},
	{12, 2, CHANNEL_PASSIVE_SCAN},	
	{}
};

CH_DESC Country_Region33_ChDesc_2GHZ[] =
{
	{1, 14, CHANNEL_DEFAULT_PROP},
	{}
};

COUNTRY_REGION_CH_DESC Country_Region_ChDesc_2GHZ[] =
{
	{REGION_0_BG_BAND, Country_Region0_ChDesc_2GHZ},
	{REGION_1_BG_BAND, Country_Region1_ChDesc_2GHZ},
	{REGION_2_BG_BAND, Country_Region2_ChDesc_2GHZ},
	{REGION_3_BG_BAND, Country_Region3_ChDesc_2GHZ},
	{REGION_4_BG_BAND, Country_Region4_ChDesc_2GHZ},
	{REGION_5_BG_BAND, Country_Region5_ChDesc_2GHZ},
	{REGION_6_BG_BAND, Country_Region6_ChDesc_2GHZ},
	{REGION_7_BG_BAND, Country_Region7_ChDesc_2GHZ},
	{REGION_31_BG_BAND, Country_Region31_ChDesc_2GHZ},
	{REGION_32_BG_BAND, Country_Region32_ChDesc_2GHZ},
	{REGION_33_BG_BAND, Country_Region33_ChDesc_2GHZ},
	{}
};

UINT16 const Country_Region_GroupNum_2GHZ = sizeof(Country_Region_ChDesc_2GHZ) / sizeof(COUNTRY_REGION_CH_DESC);

CH_DESC Country_Region0_ChDesc_5GHZ[] =
{
	{36, 8, CHANNEL_DEFAULT_PROP},
	{149, 5, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region1_ChDesc_5GHZ[] =
{
	{36, 8, CHANNEL_DEFAULT_PROP},
	{100, 11, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region2_ChDesc_5GHZ[] =
{
	{36, 8, CHANNEL_DEFAULT_PROP},
	{}	
};

CH_DESC Country_Region3_ChDesc_5GHZ[] =
{
	{52, 4, CHANNEL_DEFAULT_PROP},
	{149, 4, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region4_ChDesc_5GHZ[] =
{
	{149, 5, CHANNEL_DEFAULT_PROP},
	{}
};
CH_DESC Country_Region5_ChDesc_5GHZ[] =
{
	{149, 4, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region6_ChDesc_5GHZ[] =
{
	{36, 4, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region7_ChDesc_5GHZ[] =
{
	{36, 8, CHANNEL_DEFAULT_PROP},
	{100, 11, CHANNEL_DEFAULT_PROP},
	{149, 7, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region8_ChDesc_5GHZ[] =
{
	{52, 4, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region9_ChDesc_5GHZ[] =
{
	{36, 8 , CHANNEL_DEFAULT_PROP},
	{100, 5, CHANNEL_DEFAULT_PROP},
	{132, 3, CHANNEL_DEFAULT_PROP},
	{149, 5, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region10_ChDesc_5GHZ[] =
{
	{36,4, CHANNEL_DEFAULT_PROP},
	{149, 5, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region11_ChDesc_5GHZ[] =
{
	{36, 8, CHANNEL_DEFAULT_PROP},
	{100, 6, CHANNEL_DEFAULT_PROP},
	{149, 4, CHANNEL_DEFAULT_PROP},
	{}		
};

CH_DESC Country_Region12_ChDesc_5GHZ[] =
{
	{36, 8, CHANNEL_DEFAULT_PROP},
	{100, 11, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region13_ChDesc_5GHZ[] =
{
	{52, 4, CHANNEL_DEFAULT_PROP},
	{100, 11, CHANNEL_DEFAULT_PROP},
	{149, 4, CHANNEL_DEFAULT_PROP},
	{}	
};

CH_DESC Country_Region14_ChDesc_5GHZ[] =
{
	{36, 8, CHANNEL_DEFAULT_PROP},
	{100, 5, CHANNEL_DEFAULT_PROP},
	{136, 2, CHANNEL_DEFAULT_PROP},
	{149, 5, CHANNEL_DEFAULT_PROP},
	{}	
};

CH_DESC Country_Region15_ChDesc_5GHZ[] =
{
	{149, 7, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region16_ChDesc_5GHZ[] =
{
	{52, 4, CHANNEL_DEFAULT_PROP},
	{149, 5, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region17_ChDesc_5GHZ[] =
{
	{36, 4, CHANNEL_DEFAULT_PROP},
	{149, 4, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region18_ChDesc_5GHZ[] =
{
	{36, 8, CHANNEL_DEFAULT_PROP},
	{100, 5, CHANNEL_DEFAULT_PROP},
	{132, 3, CHANNEL_DEFAULT_PROP},
	{}	
};

CH_DESC Country_Region19_ChDesc_5GHZ[] =
{
	{56, 3, CHANNEL_DEFAULT_PROP},
	{100, 11, CHANNEL_DEFAULT_PROP},
	{149, 4, CHANNEL_DEFAULT_PROP},
	{}
};

CH_DESC Country_Region20_ChDesc_5GHZ[] =
{
	{36, 8, CHANNEL_DEFAULT_PROP},
	{100, 7, CHANNEL_DEFAULT_PROP},
	{149, 4, CHANNEL_DEFAULT_PROP},
	{}		
};

CH_DESC Country_Region21_ChDesc_5GHZ[] =
{
	{36, 8, CHANNEL_DEFAULT_PROP},
	{100, 11, CHANNEL_DEFAULT_PROP},
	{149, 4, CHANNEL_DEFAULT_PROP},
	{}		
};


COUNTRY_REGION_CH_DESC Country_Region_ChDesc_5GHZ[] =
{
	{REGION_0_A_BAND, Country_Region0_ChDesc_5GHZ},
	{REGION_1_A_BAND, Country_Region1_ChDesc_5GHZ},
	{REGION_2_A_BAND, Country_Region2_ChDesc_5GHZ},
	{REGION_3_A_BAND, Country_Region3_ChDesc_5GHZ},
	{REGION_4_A_BAND, Country_Region4_ChDesc_5GHZ},
	{REGION_5_A_BAND, Country_Region5_ChDesc_5GHZ},
	{REGION_6_A_BAND, Country_Region6_ChDesc_5GHZ},
	{REGION_7_A_BAND, Country_Region7_ChDesc_5GHZ},
	{REGION_8_A_BAND, Country_Region8_ChDesc_5GHZ},
	{REGION_9_A_BAND, Country_Region9_ChDesc_5GHZ},
	{REGION_10_A_BAND, Country_Region10_ChDesc_5GHZ},
	{REGION_11_A_BAND, Country_Region11_ChDesc_5GHZ},
	{REGION_12_A_BAND, Country_Region12_ChDesc_5GHZ},
	{REGION_13_A_BAND, Country_Region13_ChDesc_5GHZ},
	{REGION_14_A_BAND, Country_Region14_ChDesc_5GHZ},
	{REGION_15_A_BAND, Country_Region15_ChDesc_5GHZ},
	{REGION_16_A_BAND, Country_Region16_ChDesc_5GHZ},
	{REGION_17_A_BAND, Country_Region17_ChDesc_5GHZ},
	{REGION_18_A_BAND, Country_Region18_ChDesc_5GHZ},
	{REGION_19_A_BAND, Country_Region19_ChDesc_5GHZ},
	{REGION_20_A_BAND, Country_Region20_ChDesc_5GHZ},
	{REGION_21_A_BAND, Country_Region21_ChDesc_5GHZ},
	{}
};

UINT16 const Country_Region_GroupNum_5GHZ = sizeof(Country_Region_ChDesc_5GHZ) / sizeof(COUNTRY_REGION_CH_DESC);

UINT16 TotalChNum(PCH_DESC pChDesc)
{
	UINT16 TotalChNum = 0;
	
	while(pChDesc->FirstChannel)
	{
		TotalChNum += pChDesc->NumOfCh;
		pChDesc++;
	}
	
	return TotalChNum;
}

UCHAR GetChannel_5GHZ(PCH_DESC pChDesc, UCHAR index)
{
	while (pChDesc->FirstChannel)
	{
		if (index < pChDesc->NumOfCh)
			return pChDesc->FirstChannel + index * 4;
		else
		{
			index -= pChDesc->NumOfCh;
			pChDesc++;
		}
	}

	return 0;
}

UCHAR GetChannel_2GHZ(PCH_DESC pChDesc, UCHAR index)
{

	while (pChDesc->FirstChannel)
	{
		if (index < pChDesc->NumOfCh)
			return pChDesc->FirstChannel + index;
		else
		{
			index -= pChDesc->NumOfCh;
			pChDesc++;
		}
	}

	return 0;
}

UCHAR GetChannelFlag(PCH_DESC pChDesc, UCHAR index)
{

	while (pChDesc->FirstChannel)
	{
		if (index < pChDesc->NumOfCh)
			return pChDesc->ChannelProp;
		else
		{
			index -= pChDesc->NumOfCh;
			pChDesc++;
		}
	}

	return 0;
}

#ifdef EXT_BUILD_CHANNEL_LIST
CH_REGION ChRegion[] =
{
		{	/* Antigua and Berbuda*/
			"AG",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, FALSE},	/* 5G, ch 100~140*/
				{ 0},							/* end*/
			}
		},

		{	/* Argentina*/ 
			"AR",
			CE,
			{
				{ 1,   11, 30, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 52,  4,  30, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 149, 5,  30, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Aruba*/
			"AW",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, FALSE},	/* 5G, ch 100~140*/
				{ 0},							/* end*/
			}
		},

		{	/* Australia*/ 
			"AU",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, FALSE},	/* 5G, ch 100~140*/
			/*	{ 149,    4,  N/A, BOTH, FALSE},		5G, ch 149~161 No Rf power */
				{ 0},							/* end*/
			}
		},

		{	/* Austria*/ 
			"AT",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE},		/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE},		/* 5G, ch 100~140*/
			/*	{ 149,     7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Bahamas*/
			"BS",
			CE,
			{
				{ 1,   11, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  17, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  24, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11, 24, BOTH, TRUE},		/* 5G, ch 100~140*/
				{ 149, 5,  30, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Barbados*/
			"BB",
			CE,
			{
				{ 1,   11, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  17, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  24, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11, 24, BOTH, TRUE}, 	/* 5G, ch 100~140*/
				{ 149, 5,  30, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Bermuda*/
			"BM",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  24, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, FALSE},	/* 5G, ch 100~140*/
				{ 0},							/* end*/
			}
		},

		{	/* Brazil*/
			"BR",
			CE,
			{
				{ 1,   11, 30, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE},		/* 5G, ch 100~140*/
				{ 149, 5,  30, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Belgium*/
			"BE",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Bulgaria*/
			"BG",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},
		{	/* Bolivia*/
			"BO",
			CE,
			{
				{ 1,   13, 30, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  17, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  24, BOTH, FALSE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 24, BOTH, FALSE}, 	/* 5G, ch 100~140*/
				{ 149, 5,  30, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Canada*/
			"CA",
			CE,
			{
				{ 1,   13, 30, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  17, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  24, BOTH, TRUE},		/* 5G, ch 52~64*/
				{ 100, 8,  24, BOTH, TRUE}, 	/* 5G, ch 100~140, no ch 120~128 */
				{ 149, 5,  30, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Cayman IsLands*/
			"KY",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  24, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, FALSE},	/* 5G, ch 100~140*/
				{ 0},							/* end*/
			}
		},

		{	/* Chile*/
			"CL",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  22, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  22, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 149, 5,  22, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* China*/
			"CN",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 149, 5,  33, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Colombia*/
			"CO",
			CE,
			{
				{ 1,   11, 30, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  17, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  24, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, FALSE},	/* 5G, ch 100~140*/
				{ 0},							/* end*/
			}
		},

		{	/* Costa Rica*/
			"CR",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  17, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  24, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 149, 4,  30, BOTH, FALSE},	/* 5G, ch 149~161*/
				{ 0},							/* end*/
			}
		},

		{	/* Cyprus*/
			"CY",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  24, IDOR, TRUE},		/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE},		/* 5G, ch 100~140*/
				{ 0},							/* end*/
			}
		},

		{	/* Czech_Republic*/
			"CZ",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/

			}
		},

		{	/* Denmark*/
			"DK",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/

			}
		},

		{	/* Dominican Republic*/
			"DO",
			CE,
			{
				{ 1,   11, 30, BOTH, FALSE},	/* 2.4 G, ch 0*/
				{ 36,  4,  17, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  24, IDOR, FALSE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 24, BOTH, FALSE}, 	/* 5G, ch 100~140*/
				{ 149, 5,  30, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Equador*/
			"EC",
			CE,
			{
				{   1, 13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{  36,  4, 17, IDOR, FALSE},	/* 5G, ch 36~48*/
				{  52,  4, 24, IDOR, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11, 24, BOTH, FALSE},	/* 5G, ch 100~140*/
				{ 149,  5, 30, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* El Salvador*/
			"SV",
			CE,
			{
				{ 1,   13,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,   23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,   30, BOTH, TRUE},	/* 5G, ch 52~64*/
				{ 149, 4,   36, BOTH, TRUE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Finland*/
			"FI",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* France*/
			"FR",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,   4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Germany*/
			"DE",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,   4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Greece*/
			"GR",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,   4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Guam*/
			"GU",
			CE,
			{
				{ 1,   11,  20, BOTH, FALSE},	/* 2.4 G, ch 1~11*/
				{ 36,  4,   17, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,   24, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11,  30, BOTH, FALSE},	/* 5G, ch 100~140*/
				{ 149,  5,  30, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Guatemala*/
			"GT",
			CE,
			{
				{ 1,   13,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,   17, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,   24, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 149,  4,  30, BOTH, FALSE},	/* 5G, ch 149~161*/
				{ 0},							/* end*/
			}
		},

		{	/* Haiti*/
			"HT",
			CE,
			{
				{   1, 13,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{  36,  4,  17, BOTH, FALSE},	/* 5G, ch 36~48*/
				{  52,  4,  24, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11,  24, BOTH, FALSE}, 	/* 5G, ch 100~140*/
				{ 149,  5,  30, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Honduras*/
			"HN",
			CE,
			{
				{ 1,   13,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 149,  4,  27, BOTH, FALSE},	/* 5G, ch 149~161*/
				{ 0},							/* end*/
			}
		},

		{	/* Hong Kong*/
			"HK",
			CE,
			{
				{ 1,   13,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,   4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  23, IDOR, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11,  30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
				{ 149,  4,  36, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Hungary*/
			"HU",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Iceland*/
			"IS",
			CE,
			{
				{ 1,   13,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,   4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  23, IDOR, TRUE},	/* 5G, ch 52~64*/
				{ 100, 11,  30, BOTH, TRUE},	/* 5G, ch 100~140*/
				{ 0},							/* end*/
			}
		},

		{	/* India*/
			"IN",
			CE,
			{
				{ 1,   11,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,   4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  23, IDOR, FALSE},	/* 5G, ch 52~64*/
				{ 149, 	5,  23, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Indonesia*/
			"ID",
			CE,
			{
				{ 149, 	4,  36, BOTH, FALSE},	/* 5G, ch 149~161*/
				{ 0},							/* end*/
			}
		},

		{	/* Ireland*/
			"IE",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Israel*/
			"IL",
			CE,
			{
				{ 1,   13,  20, IDOR, FALSE},	/* 2.4 G, ch 1~3*/
				{ 36, 	4,  23, IDOR, FALSE},	/* 2.4 G, ch 4~9*/
				{ 52, 	4,  23, IDOR, FALSE},	/* 2.4 G, ch 10~13*/
				{ 0},							/* end*/
			}
		},

		{	/* Italy*/
			"IT",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Japan*/
			"JP",
			JAP,
			{
				{ 1,   14,  20, BOTH, FALSE},	/* 2.4 G, ch 1~14*/
				{ 36, 	4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  23, IDOR, FALSE}, 	/* 5G, ch 52~64*/
				{ 100, 11,  23, BOTH, TRUE}, 	/* 5G, ch 100~140*/
				{ 0},							/* end*/
			}
		},

		{	/* Jordan*/
			"JO",
			CE,
			{
				{ 1,   13,  20, IDOR, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36, 	4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 0},							/* end*/
			}
		},

		{	/* Kuwait*/
			"KW",
			CE,
			{
				{ 1,   14,  20, BOTH, FALSE},	/* 2.4 G, ch 1~14*/
				{ 36, 	4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 0},							/* end*/
			}
		},

		{	/* Latvia*/
			"LV",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Liechtenstein*/
			"LI",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Lithuania*/
			"LT",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Luxemburg*/
			"LU",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Malaysia*/
			"MY",
			CE,
			{
				{ 1,   13,  27, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36, 	4,  30, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52, 	4,  30, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 149,  5,  30, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Malta*/
			"MT",
			CE,
			{
				{ 1,   13,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36, 	4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52, 	4,  23, IDOR, TRUE},	/* 5G, ch 52~64*/
				{ 100, 11,  30, BOTH, TRUE},	/* 5G, ch 100~140*/
				{ 0},							/* end*/
			}
		},

		{	/* Morocco*/
			"MA",
			CE,
			{
				{ 1,   13,  10, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36, 	4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 149,  5,  30, IDOR, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Mexico*/
			"MX",
			CE,
			{
				{ 1,   11,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36, 	4,  17, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52, 	4,  23, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11,  30, BOTH, FALSE}, 	/* 5G, ch 100~140*/
				{ 149,  5,  30, ODOR, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Netherlands*/
			"NL",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* New Zealand*/
			"NZ",
			CE,
			{
				{ 1,   13,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36, 	4,  23, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52, 	4,  23, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11,  30, BOTH, FALSE}, 	/* 5G, ch 100~140*/
				{ 149,  4,  30, BOTH, FALSE},	/* 5G, ch 149~161*/
				{ 0},							/* end*/
			}
		},

		{	/* Norway*/
			"NO",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Peru*/
			"PE",
			CE,
			{
				{ 1,   13,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,   4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  23, IDOR, FALSE}, 	/* 5G, ch 52~64*/
				{ 100, 11,  30, BOTH, FALSE}, 	/* 5G, ch 100~140*/
				{ 149,  5,  27, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Portugal*/
			"PT",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Poland*/
			"PL",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Romania*/
			"RO",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Russia*/
			"RU",
			CE,
			{
				{ 1,   11,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,   4,  17, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  24, IDOR, FALSE}, 	/* 5G, ch 52~64*/	
				{ 100, 11,  30, BOTH, FALSE}, 	/* 5G, ch 100~140*/
				{ 149,  4,  30, IDOR, FALSE},	/* 5G, ch 149~161*/
				{ 0},							/* end*/
			}
		},

		{	/* Saudi Arabia*/
			"SA",
			CE,
			{
				{ 1,   13,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,   4,  23, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  23, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 149,  5,  23, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Serbia_and_Montenegro*/
			"CS",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Singapore*/
			"SG",
			CE,
			{
				{ 1,   13,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,   4,  23, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  23, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 149,  5,  20, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Slovakia*/
			"SK",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Slovenia*/
			"SI",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* South Africa*/
			"ZA",
			CE,
			{
				{ 1,   13,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,   4,  30, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  30, IDOR, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11,  30, BOTH, TRUE},	/* 5G, ch 100~140*/
				{ 149,  4,  30, BOTH, FALSE},	/* 5G, ch 149~161*/
				{ 0},							/* end*/
			}
		},

		{	/* South Korea*/
			"KR",
			CE,
			{
				{ 1,   13,  20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,   4,  20, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  20, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 100,  8,  20, BOTH, FALSE},	/* 5G, ch 100~128*/
				{ 149,  5,  20, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Spain*/
			"ES",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Sweden*/
			"SE",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Switzerland*/
			"CH",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Taiwan*/
			"TW",
			CE,
			{
				{ 1,   11,  30, BOTH, FALSE},	/* 2.4 G, ch 1~11*/
				{ 52,   4,  23, IDOR, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11,  20, BOTH, TRUE},	/* 5G, ch 100~140*/
				{ 149,  5,  20, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Turkey*/
			"TR",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* UK*/
			"GB",
			CE,
			{
				{ 1,   13, 20, BOTH, FALSE},	/* 2.4 G, ch 1~13*/
				{ 36,  4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,  4,  23, IDOR, TRUE}, 	/* 5G, ch 52~64*/
				{ 100, 11, 30, BOTH, TRUE}, 	/* 5G, ch 100~140*/
			/*	{ 149,	   7,  36, N/A, N/A},		5G, ch 149~173 No Geography  */
				{ 0},							/* end*/
			}
		},

		{	/* Ukraine*/
			"UA",
			CE,
			{
				{ 1,   11,  20, BOTH, FALSE},	/* 2.4 G, ch 1~11*/
				{ 36,   4,  23, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  23, IDOR, FALSE}, 	/* 5G, ch 52~64*/
				{ 0},							/* end*/
			}
		},

		{	/* United_Arab_Emirates*/
			"AE",
			CE,
			{
				{ 1,   13,  20, BOTH, FALSE},	/* 2.4 G, ch 1~11*/
				{ 0},							/* end*/
			}
		},

		{	/* United_States*/
			"US",
			FCC,
			{
				{ 1,   11,  30, BOTH, FALSE},	/* 2.4 G, ch 1~11*/
				{ 36,   4,  17, IDOR, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  24, BOTH, TRUE},	/* 5G, ch 52~64*/
				{ 100, 11,  24, BOTH, TRUE},	/* 5G, ch 100~140*/
				{ 149,  5,  30, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},

		{	/* Venezuela*/
			"VE",
			CE,
			{
				{ 1,   11,  20, BOTH, FALSE},	/* 2.4 G, ch 1~11*/
				{ 149,  4,  27, BOTH, FALSE},	/* 5G, ch 149~161*/
				{ 0},							/* end*/
			}
		},

		{	/* Default*/
			"",
			CE,
			{
				{ 1,   14,  255, BOTH, FALSE},	/* 2.4 G, ch 1~14*/
				{ 36,   4,  255, BOTH, FALSE},	/* 5G, ch 36~48*/
				{ 52,   4,  255, BOTH, FALSE},	/* 5G, ch 52~64*/
				{ 100, 11,  255, BOTH, FALSE},	/* 5G, ch 100~140*/
				{ 149,  5,  255, BOTH, FALSE},	/* 5G, ch 149~165*/
				{ 0},							/* end*/
			}
		},
};

CH_REGION ChRegion[] =
{
	{"AL", CE, Country_AL_ChDesp, TRUE}, /* Albania */
	{"DZ", CE, Country_DZ_ChDesp, TRUE}, /* Algeria */
	{"AR", CE, Country_AR_ChDesp, TRUE}, /* Argentina */
	{"AM", CE, Country_AM_ChDesp, TRUE}, /* Armenia */
	{"AW", CE, Country_AW_ChDesp, TRUE}, /* Aruba */
	{"AU", CE, Country_AU_ChDesp, TRUE}, /* Australia */
	{"AT", CE, Country_AT_ChDesp, TRUE}, /* Austria */
	{"AZ", CE, Country_AZ_ChDesp, TRUE}, /* Azerbaijan */
	{"BH", CE, Country_BH_ChDesp, TRUE}, /* Bahrain */
	{"BD", CE, Country_BD_ChDesp, TRUE}, /* Bangladesh */
	{"BB", CE, Country_BB_ChDesp, TRUE}, /* Barbados */
	{"BY", CE, Country_BY_ChDesp, TRUE}, /* Belarus */
	{"BE", CE, Country_BE_ChDesp, TRUE}, /* Belgium */
	{"BZ", CE, Country_BZ_ChDesp, TRUE}, /* Belize */
	{"BO", CE, Country_BO_ChDesp, TRUE}, /* Bolivia */
	{"BA", CE, Country_BA_ChDesp, TRUE}, /* Bosnia and Herzegovina */
	{"BR", CE, Country_BR_ChDesp, TRUE}, /* Brazil */
	{"BN", CE, Country_BN_ChDesp, TRUE}, /* Brunei Darussalam */
	{"BG", CE, Country_BG_ChDesp, TRUE}, /* Bulgaria */
	{"KH", CE, Country_KH_ChDesp, TRUE}, /* Cambodia */
	{"CA", FCC,Country_CA_ChDesp, FALSE}, /* Canada */
	{"CL", CE, Country_CL_ChDesp, TRUE}, /* Chile */
	{"CN", CE, Country_CN_ChDesp, TRUE}, /* China */
	{"CO", CE, Country_CO_ChDesp, TRUE}, /* Colombia */
	{"CR", CE, Country_CR_ChDesp, TRUE}, /* Costa Rica */
	{"HR", CE, Country_HR_ChDesp, TRUE}, /* Croatia */
	{"CY", CE, Country_CY_ChDesp, TRUE}, /* Cyprus */
	{"CZ", CE, Country_CZ_ChDesp, TRUE}, /* Czech Republic */
	{"DK", CE, Country_DK_ChDesp, TRUE}, /* Denmark */
	{"DO", CE, Country_DO_ChDesp, TRUE}, /* Dominican Republic */
	{"EC", CE, Country_EC_ChDesp, TRUE}, /* Ecuador */
	{"EG", CE, Country_EG_ChDesp, TRUE}, /* Egypt */
	{"SV", CE, Country_SV_ChDesp, TRUE}, /* El Salvador */
	{"EE", CE, Country_EE_ChDesp, TRUE}, /* Estonia */
	{"FI", CE, Country_FI_ChDesp, TRUE}, /* Finland */
	{"FR", CE, Country_FR_ChDesp, TRUE}, /* France */
	{"GE", CE, Country_GE_ChDesp, TRUE}, /* Georgia */
	{"DE", CE, Country_DE_ChDesp, TRUE}, /* Germany */
	{"GR", CE, Country_GR_ChDesp, TRUE}, /* Greece */
	{"GL", CE, Country_GL_ChDesp, TRUE}, /* Greenland */
	{"GD", CE, Country_GD_ChDesp, TRUE}, /* Grenada */
	{"GU", CE, Country_GU_ChDesp, TRUE}, /* Guam */
	{"GT", CE, Country_GT_ChDesp, TRUE}, /* Guatemala */
	{"HT", CE, Country_HT_ChDesp, TRUE}, /* Haiti */
	{"HN", CE, Country_HN_ChDesp, TRUE}, /* Honduras */
	{"HK", CE, Country_HK_ChDesp, TRUE}, /* Hong Kong */
	{"HU", CE, Country_HU_ChDesp, TRUE}, /* Hungary */
	{"IS", CE, Country_IS_ChDesp, TRUE}, /* Iceland */
	{"IN", CE, Country_IN_ChDesp, TRUE}, /* India */
	{"ID", CE, Country_ID_ChDesp, TRUE}, /* Indonesia */
	{"IR", CE, Country_IR_ChDesp, TRUE}, /* Iran, Islamic Republic of */
	{"IE", CE, Country_IE_ChDesp, TRUE}, /* Ireland */
	{"IL", CE, Country_IL_ChDesp, TRUE}, /* Israel */
	{"IT", CE, Country_IT_ChDesp, TRUE}, /* Italy */
	{"JM", CE, Country_JM_ChDesp, TRUE}, /* Jamaica */
	{"JP", JAP,Country_JP_ChDesp, FALSE}, /* Japan */		
	{"JO", CE, Country_JO_ChDesp, TRUE}, /* Jordan */	
	{"KZ", CE, Country_KZ_ChDesp, TRUE}, /* Kazakhstan */			
	{"KE", CE, Country_KE_ChDesp, TRUE}, /* Kenya */	
	{"KP", CE, Country_KP_ChDesp, TRUE}, /* Korea, Democratic People's Republic of */
	{"KR", CE, Country_KR_ChDesp, TRUE}, /* Korea, Republic of */			
	{"KW", CE, Country_KW_ChDesp, TRUE}, /* Kuwait */			
	{"LV", CE, Country_LV_ChDesp, TRUE}, /* Latvia */			
	{"LB", CE, Country_LB_ChDesp, TRUE}, /* Lebanon */			
	{"LI", CE, Country_LI_ChDesp, TRUE}, /* Liechtenstein */			
	{"LT", CE, Country_LT_ChDesp, TRUE}, /* Lithuania */			
	{"LU", CE, Country_LU_ChDesp, TRUE}, /* Luxembourg */			
	{"MO", CE, Country_MO_ChDesp, TRUE}, /* Macao */			
	{"MK", CE, Country_MK_ChDesp, TRUE}, /* Macedonia, Republic of */			
	{"MY", CE, Country_MY_ChDesp, TRUE}, /* Malaysia */			
	{"MT", CE, Country_MT_ChDesp, TRUE}, /* Malta */			
	{"MX", CE, Country_MX_ChDesp, TRUE}, /* Mexico */			
	{"MC", CE, Country_MC_ChDesp, TRUE}, /* Monaco */			
	{"MA", CE, Country_MA_ChDesp, TRUE}, /* Morocco */			
	{"NP", CE, Country_NP_ChDesp, TRUE}, /* Nepal */			
	{"NL", CE, Country_NL_ChDesp, TRUE}, /* Netherlands */			
	{"AN", CE, Country_AN_ChDesp, TRUE}, /* Netherlands Antilles */			
	{"NZ", CE, Country_NZ_ChDesp, TRUE}, /* New Zealand */			
	{"NO", CE, Country_NO_ChDesp, TRUE}, /* Norway */			
	{"OM", CE, Country_OM_ChDesp, TRUE}, /* Oman */		
	{"PK", CE, Country_PK_ChDesp, TRUE}, /* Pakistan */		
	{"PA", CE, Country_PA_ChDesp, TRUE}, /* Panama */	
	{"PG", CE, Country_PG_ChDesp, TRUE}, /* Papua New Guinea */	
	{"PE", CE, Country_PE_ChDesp, TRUE}, /* Peru */			
	{"PH", CE, Country_PH_ChDesp, TRUE}, /* Philippines */		
	{"PL", CE, Country_PL_ChDesp, TRUE}, /* Poland */			
	{"PT", CE, Country_PT_ChDesp, TRUE}, /* Portuga l*/			
	{"PR", CE, Country_PR_ChDesp, TRUE}, /* Puerto Rico */			
	{"QA", CE, Country_QA_ChDesp, TRUE}, /* Qatar */			
	{"RO", CE, Country_RO_ChDesp, TRUE}, /* Romania */			
	{"RU", CE, Country_RU_ChDesp, TRUE}, /* Russian Federation */			
	{"BL", CE, Country_BL_ChDesp, TRUE}, /* Saint Barth'elemy */			
	{"SA", CE, Country_SA_ChDesp, TRUE}, /* Saudi Arabia */			
	{"SG", CE, Country_SG_ChDesp, TRUE}, /* Singapore */			
	{"SK", CE, Country_SK_ChDesp, TRUE}, /* Slovakia */			
	{"SI", CE, Country_SI_ChDesp, TRUE}, /* Slovenia */					
	{"ZA", CE, Country_ZA_ChDesp, TRUE}, /* South Africa */					
	{"ES", CE, Country_ES_ChDesp, TRUE}, /* Spain */				
	{"LK", CE, Country_LK_ChDesp, TRUE}, /* Sri Lanka */				
	{"SE", CE, Country_SE_ChDesp, TRUE}, /* Sweden */					
	{"CH", CE, Country_CH_ChDesp, TRUE}, /* Switzerland */					
	{"SY", CE, Country_SY_ChDesp, TRUE}, /* Syrian Arab Republic */					
	{"TW", FCC,Country_TW_ChDesp, FALSE}, /* Taiwan */			
	{"TH", CE, Country_TH_ChDesp, TRUE}, /* Thailand */					
	{"TT", CE, Country_TT_ChDesp, TRUE}, /* Trinidad and Tobago */			
	{"TN", CE, Country_TN_ChDesp, TRUE}, /* Tunisia */				
	{"TR", CE, Country_TR_ChDesp, TRUE}, /* Turkey */					
	{"UA", CE, Country_UA_ChDesp, TRUE}, /* Ukraine */					
	{"AE", CE, Country_AE_ChDesp, TRUE}, /* United Arab Emirates */					
	{"GB", CE, Country_GB_ChDesp, TRUE}, /* United Kingdom */			
	{"US", FCC,Country_US_ChDesp, FALSE}, /* United States */			
	{"UY", CE, Country_UY_ChDesp, TRUE}, /* Uruguay */					
	{"UZ", CE, Country_UZ_ChDesp, TRUE}, /* Uzbekistan */				
	{"VE", CE, Country_VE_ChDesp, TRUE}, /* Venezuela */				
	{"VN", CE, Country_VN_ChDesp, TRUE}, /* Viet Nam */					
	{"YE", CE, Country_YE_ChDesp, TRUE}, /* Yemen */					
	{"ZW", CE, Country_ZW_ChDesp, TRUE}, /* Zimbabwe */	
	{"EU", CE, Country_EU_ChDesp, TRUE}, /* Europe */
	{"NA", FCC,Country_NA_ChDesp, FALSE}, /* North America */
	{"WO", CE, Country_WO_ChDesp, TRUE}, /* World Wide */
	{""  , 0,  NULL, FALSE}	     , /* End */	
};

static PCH_REGION GetChRegion(
	IN PUCHAR CntryCode)
{
	INT loop = 0;
	PCH_REGION pChRegion = NULL;

	while (strcmp((PSTRING) ChRegion[loop].CountReg, "") != 0)
	{
		if (strncmp((PSTRING) ChRegion[loop].CountReg, (PSTRING) CntryCode, 2) == 0)
		{
			pChRegion = &ChRegion[loop];
			break;
		}
		loop++;
	}

	if (pChRegion == NULL)
		pChRegion = &ChRegion[loop];
	return pChRegion;
}

static VOID ChBandCheck(
	IN UCHAR PhyMode,
	OUT PUCHAR pChType)
{
	switch(PhyMode)
	{
		case PHY_11A:
#ifdef DOT11_N_SUPPORT
		case PHY_11AN_MIXED:
		case PHY_11N_5G:
#endif /* DOT11_N_SUPPORT */
			*pChType = BAND_5G;
			break;
		case PHY_11ABG_MIXED:
#ifdef DOT11_N_SUPPORT
		case PHY_11AGN_MIXED:
		case PHY_11ABGN_MIXED:
#endif /* DOT11_N_SUPPORT */
			*pChType = BAND_BOTH;
			break;

		default:
			*pChType = BAND_24G;
			break;
	}
}

static UCHAR FillChList(
	IN PRTMP_ADAPTER pAd,
	IN PCH_DESP pChDesp,
	IN UCHAR Offset, 
	IN UCHAR increment,
	IN UCHAR regulatoryDomain)
{
	INT i, j, l;
	UCHAR channel;

	j = Offset;
	for (i = 0; i < pChDesp->NumOfCh; i++)
	{
		channel = pChDesp->FirstChannel + i * increment;
/*New FCC spec restrict the used channel under DFS */
#ifdef CONFIG_AP_SUPPORT	
#ifdef DFS_SUPPORT
		if ((pAd->CommonCfg.bIEEE80211H == 1) && (pAd->CommonCfg.RadarDetect.RDDurRegion == FCC) && (pAd->CommonCfg.bDFSIndoor == 1))
		{
			if ((channel >= 116) && (channel <=128))
				continue;
		}
		else if ((pAd->CommonCfg.bIEEE80211H == 1) && (pAd->CommonCfg.RadarDetect.RDDurRegion == FCC) && (pAd->CommonCfg.bDFSIndoor == 0))
		{
			if ((channel >= 100) && (channel <= 140))
				continue;
		}			

#endif /* DFS_SUPPORT */
#endif /* CONFIG_AP_SUPPORT */
		for (l=0; l<MAX_NUM_OF_CHANNELS; l++)
		{
			if (channel == pAd->TxPower[l].Channel)
			{
				pAd->ChannelList[j].Power = pAd->TxPower[l].Power;
				pAd->ChannelList[j].Power2 = pAd->TxPower[l].Power2;
#ifdef DOT11N_SS3_SUPPORT
					pAd->ChannelList[j].Power3 = pAd->TxPower[l].Power3;
#endif /* DOT11N_SS3_SUPPORT */
				break;
			}
		}
		if (l == MAX_NUM_OF_CHANNELS)
			continue;

		pAd->ChannelList[j].Channel = pChDesp->FirstChannel + i * increment;
		pAd->ChannelList[j].MaxTxPwr = pChDesp->MaxTxPwr;
		pAd->ChannelList[j].DfsReq = pChDesp->DfsReq;
		pAd->ChannelList[j].RegulatoryDomain = regulatoryDomain;
		j++;
	}
	pAd->ChannelListNum = j;

	return j;
}


static inline VOID CreateChList(
	IN PRTMP_ADAPTER pAd,
	IN PCH_REGION pChRegion,
	IN UCHAR Geography)
{
	INT i;
	UCHAR offset = 0;
	PCH_DESP pChDesp;
	UCHAR ChType;
	UCHAR increment;
	UCHAR regulatoryDomain;

	if (pChRegion == NULL)
		return;

	ChBandCheck(pAd->CommonCfg.PhyMode, &ChType);

	for (i=0; i<10; i++)
	{
		pChDesp = &pChRegion->ChDesp[i];
		if (pChDesp->FirstChannel == 0)
			break;

		if (ChType == BAND_5G)
		{
			if (pChDesp->FirstChannel <= 14)
				continue;
		}
		else if (ChType == BAND_24G)
		{
			if (pChDesp->FirstChannel > 14)
				continue;
		}

		if ((pChDesp->Geography == BOTH)
			|| (Geography == BOTH)
			|| (pChDesp->Geography == Geography))
        {
			if (pChDesp->FirstChannel > 14)
                increment = 4;
            else
                increment = 1;
			regulatoryDomain = pChRegion->DfsType;
			offset = FillChList(pAd, pChDesp, offset, increment, regulatoryDomain);
        }
	}
}


VOID BuildChannelListEx(
	IN PRTMP_ADAPTER pAd)
{
	PCH_REGION pChReg;

	pChReg = GetChRegion(pAd->CommonCfg.CountryCode);
	CreateChList(pAd, pChReg, pAd->CommonCfg.Geography);
}

VOID BuildBeaconChList(
	IN PRTMP_ADAPTER pAd,
	OUT PUCHAR pBuf,
	OUT	PULONG pBufLen)
{
	INT i;
	ULONG TmpLen;
	PCH_REGION pChRegion;
	PCH_DESP pChDesp;
	UCHAR ChType;

	pChRegion = GetChRegion(pAd->CommonCfg.CountryCode);

	if (pChRegion == NULL)
		return;

	ChBandCheck(pAd->CommonCfg.PhyMode, &ChType);
	*pBufLen = 0;

	for (i=0; i<10; i++)
	{
		pChDesp = &pChRegion->ChDesp[i];
		if (pChDesp->FirstChannel == 0)
			break;

		if (ChType == BAND_5G)
		{
			if (pChDesp->FirstChannel <= 14)
				continue;
		}
		else if (ChType == BAND_24G)
		{
			if (pChDesp->FirstChannel > 14)
				continue;
		}

		if ((pChDesp->Geography == BOTH)
			|| (pChDesp->Geography == pAd->CommonCfg.Geography))
		{
			MakeOutgoingFrame(pBuf + *pBufLen,		&TmpLen,
								1,                 	&pChDesp->FirstChannel,
								1,                 	&pChDesp->NumOfCh,
								1,                 	&pChDesp->MaxTxPwr,
								END_OF_ARGS);
			*pBufLen += TmpLen;
		}
	}
}
#endif /* EXT_BUILD_CHANNEL_LIST */


#ifdef ED_MONITOR
COUNTRY_PROP CountryProp[]=
{
	{"AL", CE, TRUE}, /* Albania */
	{"DZ", CE, TRUE }, /* Algeria */
	{"AR", CE, TRUE }, /* Argentina */
	{"AM", CE, TRUE }, /* Armenia */
	{"AW", CE, TRUE }, /* Aruba */
	{"AU", CE, TRUE }, /* Australia */
	{"AT", CE, TRUE }, /* Austria */
	{"AZ", CE, TRUE }, /* Azerbaijan */
	{"BH", CE, TRUE }, /* Bahrain */
	{"BD", CE, TRUE }, /* Bangladesh */
	{"BB", CE, TRUE }, /* Barbados */
	{"BY", CE, TRUE }, /* Belarus */
	{"BE", CE, TRUE }, /* Belgium */
	{"BZ", CE, TRUE }, /* Belize */
	{"BO", CE, TRUE }, /* Bolivia */
	{"BA", CE, TRUE }, /* Bosnia and Herzegovina */
	{"BR", CE, TRUE }, /* Brazil */
	{"BN", CE, TRUE }, /* Brunei Darussalam */
	{"BG", CE, TRUE }, /* Bulgaria */
	{"KH", CE, TRUE }, /* Cambodia */
	{"CA", FCC, FALSE}, /* Canada */
	{"CL", CE, TRUE }, /* Chile */
	{"CN", CE, TRUE }, /* China */
	{"CO", CE, TRUE }, /* Colombia */
	{"CR", CE, TRUE }, /* Costa Rica */
	{"HR", CE, TRUE }, /* Croatia */
	{"CY", CE, TRUE }, /* Cyprus */
	{"CZ", CE, TRUE }, /* Czech Republic */
	{"DK", CE, TRUE }, /* Denmark */
	{"DO", CE, TRUE }, /* Dominican Republic */
	{"EC", CE, TRUE }, /* Ecuador */
	{"EG", CE, TRUE }, /* Egypt */
	{"SV", CE, TRUE }, /* El Salvador */
	{"EE", CE, TRUE }, /* Estonia */
	{"FI", CE, TRUE }, /* Finland */
	{"FR", CE, TRUE }, /* France */
	{"GE", CE, TRUE }, /* Georgia */
	{"DE", CE, TRUE }, /* Germany */
	{"GR", CE, TRUE }, /* Greece */
	{"GL", CE, TRUE }, /* Greenland */
	{"GD", CE, TRUE }, /* Grenada */
	{"GU", CE, TRUE }, /* Guam */
	{"GT", CE, TRUE }, /* Guatemala */
	{"HT", CE, TRUE }, /* Haiti */
	{"HN", CE, TRUE }, /* Honduras */
	{"HK", CE, TRUE }, /* Hong Kong */
	{"HU", CE, TRUE }, /* Hungary */
	{"IS", CE, TRUE }, /* Iceland */
	{"IN", CE, TRUE }, /* India */
	{"ID", CE, TRUE }, /* Indonesia */
	{"IR", CE, TRUE }, /* Iran, Islamic Republic of */
	{"IE", CE, TRUE }, /* Ireland */
	{"IL", CE, TRUE }, /* Israel */
	{"IT", CE, TRUE }, /* Italy */
	{"JM", CE, TRUE }, /* Jamaica */
	{"JP", JAP, FALSE}, /* Japan */		
	{"JO", CE, TRUE }, /* Jordan */	
	{"KZ", CE, TRUE }, /* Kazakhstan */			
	{"KE", CE, TRUE }, /* Kenya */	
	{"KP", CE, TRUE }, /* Korea, Democratic People's Republic of */
	{"KR", CE, TRUE }, /* Korea, Republic of */			
	{"KW", CE, TRUE }, /* Kuwait */			
	{"LV", CE, TRUE }, /* Latvia */			
	{"LB", CE, TRUE }, /* Lebanon */			
	{"LI", CE, TRUE }, /* Liechtenstein */			
	{"LT", CE, TRUE }, /* Lithuania */			
	{"LU", CE, TRUE }, /* Luxembourg */			
	{"MO", CE, TRUE }, /* Macao */			
	{"MK", CE, TRUE }, /* Macedonia, Republic of */			
	{"MY", CE, TRUE }, /* Malaysia */			
	{"MT", CE, TRUE }, /* Malta */			
	{"MX", CE, TRUE }, /* Mexico */			
	{"MC", CE, TRUE }, /* Monaco */			
	{"MA", CE, TRUE }, /* Morocco */			
	{"NP", CE, TRUE }, /* Nepal */			
	{"NL", CE, TRUE }, /* Netherlands */			
	{"AN", CE, TRUE }, /* Netherlands Antilles */			
	{"NZ", CE, TRUE }, /* New Zealand */			
	{"NO", CE, TRUE }, /* Norway */			
	{"OM", CE, TRUE }, /* Oman */		
	{"PK", CE, TRUE }, /* Pakistan */		
	{"PA", CE, TRUE }, /* Panama */	
	{"PG", CE, TRUE }, /* Papua New Guinea */	
	{"PE", CE, TRUE }, /* Peru */			
	{"PH", CE, TRUE }, /* Philippines */		
	{"PL", CE, TRUE }, /* Poland */			
	{"PT", CE, TRUE }, /* Portuga l*/			
	{"PR", CE, TRUE }, /* Puerto Rico */			
	{"QA", CE, TRUE }, /* Qatar */			
	{"RO", CE, TRUE }, /* Romania */			
	{"RU", CE, TRUE }, /* Russian Federation */			
	{"BL", CE, TRUE }, /* Saint Barth'elemy */			
	{"SA", CE, TRUE }, /* Saudi Arabia */			
	{"SG", CE, TRUE }, /* Singapore */			
	{"SK", CE, TRUE }, /* Slovakia */			
	{"SI", CE, TRUE }, /* Slovenia */					
	{"ZA", CE, TRUE }, /* South Africa */					
	{"ES", CE, TRUE }, /* Spain */				
	{"LK", CE, TRUE }, /* Sri Lanka */				
	{"SE", CE, TRUE }, /* Sweden */					
	{"CH", CE, TRUE }, /* Switzerland */					
	{"SY", CE, TRUE }, /* Syrian Arab Republic */					
	{"TW", FCC, FALSE}, /* Taiwan */			
	{"TH", CE, TRUE }, /* Thailand */					
	{"TT", CE, TRUE }, /* Trinidad and Tobago */			
	{"TN", CE, TRUE }, /* Tunisia */				
	{"TR", CE, TRUE }, /* Turkey */					
	{"UA", CE, TRUE }, /* Ukraine */					
	{"AE", CE, TRUE }, /* United Arab Emirates */					
	{"GB", CE, TRUE }, /* United Kingdom */			
	{"US", FCC, FALSE}, /* United States */			
	{"UY", CE, TRUE }, /* Uruguay */					
	{"UZ", CE, TRUE }, /* Uzbekistan */				
	{"VE", CE, TRUE }, /* Venezuela */				
	{"VN", CE, TRUE }, /* Viet Nam */					
	{"YE", CE, TRUE }, /* Yemen */					
	{"ZW", CE, TRUE }, /* Zimbabwe */	
	{"EU", CE, TRUE }, /* Europe */
	{"NA", FCC, FALSE}, /* North America */
	{"WO", CE, FALSE}, /* World Wide */
	{""  , 0, FALSE}	     , /* End */	
};

static PCOUNTRY_PROP GetCountryProp(
	IN PUCHAR CntryCode)
{
	INT loop = 0;
	PCOUNTRY_PROP pCountryProp = NULL;

	while (strcmp((PSTRING) CountryProp[loop].CountReg, "") != 0)
	{
		if (strncmp((PSTRING) CountryProp[loop].CountReg, (PSTRING) CntryCode, 2) == 0)
		{
			pCountryProp = &CountryProp[loop];
			break;
		}
		loop++;
	}

	/* Default: use WO*/
	if (pCountryProp == NULL)
		pCountryProp = GetCountryProp("WO");

	return pCountryProp;
}

BOOLEAN GetEDCCASupport(
	IN PRTMP_ADAPTER pAd)
{
	BOOLEAN ret = FALSE;

#ifdef EXT_BUILD_CHANNEL_LIST
	PCH_REGION pChReg;
	
	pChReg = GetChRegion(pAd->CommonCfg.CountryCode);

	if ((pChReg->DfsType == CE) && (pChReg->edcca_on == TRUE) )
	{
		// actually need to check PM's table in CE country
		ret = TRUE;
	}
#else
	PCOUNTRY_PROP pCountryProp;
	
	pCountryProp = GetCountryProp(pAd->CommonCfg.CountryCode);

	if ((pCountryProp->DfsType == CE) && (pCountryProp->edcca_on == TRUE))
	{
		// actually need to check PM's table in CE country
		ret = TRUE;
	}
#endif

	return ret;
	
}
#endif /* ED_MONITOR */

#ifdef DOT11_N_SUPPORT
static BOOLEAN IsValidChannel(
	IN PRTMP_ADAPTER pAd,
	IN UCHAR channel)

{
	INT i;

	for (i = 0; i < pAd->ChannelListNum; i++)
	{
		if (pAd->ChannelList[i].Channel == channel)
			break;
	}

	if (i == pAd->ChannelListNum)
		return FALSE;
	else
		return TRUE;
}


static UCHAR GetExtCh(
	IN UCHAR Channel,
	IN UCHAR Direction)
{
	CHAR ExtCh;

	if (Direction == EXTCHA_ABOVE)
		ExtCh = Channel + 4;
	else
		ExtCh = (Channel - 4) > 0 ? (Channel - 4) : 0;

	return ExtCh;
}


VOID N_ChannelCheck(
	IN PRTMP_ADAPTER pAd)
{
	/*UCHAR ChannelNum = pAd->ChannelListNum;*/
	UCHAR Channel = pAd->CommonCfg.Channel;

	if ((pAd->CommonCfg.PhyMode >= PHY_11ABGN_MIXED) && (pAd->CommonCfg.RegTransmitSetting.field.BW  == BW_40))
	{
		if (Channel > 14)
		{
			if ((Channel == 36) || (Channel == 44) || (Channel == 52) || (Channel == 60) || (Channel == 100) || (Channel == 108) ||
			    (Channel == 116) || (Channel == 124) || (Channel == 132) || (Channel == 149) || (Channel == 157))
			{
				pAd->CommonCfg.RegTransmitSetting.field.EXTCHA = EXTCHA_ABOVE;
			}
			else if ((Channel == 40) || (Channel == 48) || (Channel == 56) || (Channel == 64) || (Channel == 104) || (Channel == 112) ||
					(Channel == 120) || (Channel == 128) || (Channel == 136) || (Channel == 153) || (Channel == 161))
			{
				pAd->CommonCfg.RegTransmitSetting.field.EXTCHA = EXTCHA_BELOW;
			}
			else
			{
				pAd->CommonCfg.RegTransmitSetting.field.BW  = BW_20;
			}
		}
		else
		{
			do
			{
				UCHAR ExtCh;
				UCHAR Dir = pAd->CommonCfg.RegTransmitSetting.field.EXTCHA;
				ExtCh = GetExtCh(Channel, Dir);
				if (IsValidChannel(pAd, ExtCh))
					break;

				Dir = (Dir == EXTCHA_ABOVE) ? EXTCHA_BELOW : EXTCHA_ABOVE;
				ExtCh = GetExtCh(Channel, Dir);
				if (IsValidChannel(pAd, ExtCh))
				{
					pAd->CommonCfg.RegTransmitSetting.field.EXTCHA = Dir;
					break;
				}
				pAd->CommonCfg.RegTransmitSetting.field.BW  = BW_20;
			} while(FALSE);

			if (Channel == 14)
			{
				pAd->CommonCfg.RegTransmitSetting.field.BW  = BW_20;
				/*pAd->CommonCfg.RegTransmitSetting.field.EXTCHA = EXTCHA_NONE; We didn't set the ExtCh as NONE due to it'll set in RTMPSetHT()*/
			}
		}
	}

#ifdef CONFIG_AP_SUPPORT
#ifdef DOT11N_DRAFT3
#endif /* DOT11N_DRAFT3 */
#endif /* CONFIG_AP_SUPPORT */

}


VOID N_SetCenCh(
	IN PRTMP_ADAPTER pAd)
{
	if (pAd->CommonCfg.RegTransmitSetting.field.BW == BW_40)
	{
		if (pAd->CommonCfg.RegTransmitSetting.field.EXTCHA == EXTCHA_ABOVE)
		{

			pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel + 2;
		}
		else
		{
			if (pAd->CommonCfg.Channel == 14)
				pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel - 1;
			else
				pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel - 2;
		}
	}
	else
	{
		pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel;
	}
}
#endif /* DOT11_N_SUPPORT */


UINT8 GetCuntryMaxTxPwr(
	IN PRTMP_ADAPTER pAd,
	IN UINT8 channel)
{
	int i;
	for (i = 0; i < pAd->ChannelListNum; i++)
	{
		if (pAd->ChannelList[i].Channel == channel)
			break;
	}

	if (i == pAd->ChannelListNum)
		return 0xff;
#ifdef SINGLE_SKU
	if (pAd->CommonCfg.bSKUMode == TRUE)
	{
		UINT deltaTxStreamPwr = 0;

#ifdef DOT11_N_SUPPORT
		if ((pAd->CommonCfg.PhyMode >= PHY_11ABGN_MIXED) && (pAd->CommonCfg.TxStream == 2))
			deltaTxStreamPwr = 3; /* If 2Tx case, antenna gain will increase 3dBm*/
#endif /* DOT11_N_SUPPORT */
		if (pAd->ChannelList[i].RegulatoryDomain == FCC)
		{
			/* FCC should maintain 20/40 Bandwidth, and without antenna gain */
#ifdef DOT11_N_SUPPORT
			if ((pAd->CommonCfg.PhyMode >= PHY_11ABGN_MIXED) &&
				(pAd->CommonCfg.RegTransmitSetting.field.BW == BW_40) &&
				(channel == 1 || channel == 11))
				return (pAd->ChannelList[i].MaxTxPwr - pAd->CommonCfg.BandedgeDelta - deltaTxStreamPwr);
			else
#endif /* DOT11_N_SUPPORT */
				return (pAd->ChannelList[i].MaxTxPwr - deltaTxStreamPwr);
		}
		else if (pAd->ChannelList[i].RegulatoryDomain == CE)
		{
			return (pAd->ChannelList[i].MaxTxPwr - pAd->CommonCfg.AntGain - deltaTxStreamPwr);
		}
		else
			return 0xff;
	}
	else
#endif /* SINGLE_SKU */
		return pAd->ChannelList[i].MaxTxPwr;
}


/* for OS_ABL */
VOID RTMP_MapChannelID2KHZ(
	IN UCHAR Ch,
	OUT UINT32 *pFreq)
{
	int chIdx;
	for (chIdx = 0; chIdx < CH_HZ_ID_MAP_NUM; chIdx++)
	{
		if ((Ch) == CH_HZ_ID_MAP[chIdx].channel)
		{
			(*pFreq) = CH_HZ_ID_MAP[chIdx].freqKHz * 1000;
			break;
		}
	}
	if (chIdx == CH_HZ_ID_MAP_NUM)
		(*pFreq) = 2412000;
}

/* for OS_ABL */
VOID RTMP_MapKHZ2ChannelID(
	IN ULONG Freq,
	OUT INT *pCh)
{
	int chIdx;
	for (chIdx = 0; chIdx < CH_HZ_ID_MAP_NUM; chIdx++)
	{
		if ((Freq) == CH_HZ_ID_MAP[chIdx].freqKHz)
		{
			(*pCh) = CH_HZ_ID_MAP[chIdx].channel;
			break;
		}
	}
	if (chIdx == CH_HZ_ID_MAP_NUM)
		(*pCh) = 1;
}

