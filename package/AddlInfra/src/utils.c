/***************************************************************************
*
*
* utils.c
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
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include "defines.h"
#include "types.h"
#include "utils.h"
#include "logger.h"
#include "global.h"
#ifdef _OPENWRT_
#include "belkin_api.h"
#else
#include "gemtek_api.h"
#endif
#include "mxml.h"
#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */
#ifdef PRODUCT_WeMo_LEDLight
#include "controlledevice.h"
#include "bridge_sensor_rule.h"
#endif

//byte separator
#if 1
const char cSep = ':';
const char delim = ':';
#else
const char cSep = 0;
#endif

void resetSystem(void)
{
	APP_LOG("utils",LOG_DEBUG, "Reseting System");
	curl_global_cleanup();
#ifdef PRODUCT_WeMo_LEDLight
	BS_FreeReactor();
#endif
	exit(0);
}


#ifdef __STR_UTILS__
/************************************************************************
 * Function: utilsStrToByte8
 *     convert ":" separated hex string to byte array of size 8
 *  Parameters:
 *     pstrId - hex string
 *     pbyId - converted byte array
 *  Return:
 *     Returns NULL or pbyId
************************************************************************/
unsigned char* utilsStrToByte8(const char *pstrId, unsigned char* pbyId)
{
	int iConunter = 0;
	for (iConunter = 0; iConunter < 8; ++iConunter)
	{
		unsigned int iNumber = 0;
		char ch;
		//Convert letter into lower case.
		ch = tolower (*pstrId++);
		if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
		{
			return NULL;
		}

		//Convert into number.
		//a. If chareater is digit then ch - '0'
		//b. else (ch - 'a' + 10) it is done because addition of 10 takes correct value.
		iNumber = isdigit (ch) ? (ch - '0') : (ch - 'a' + 10);
		ch = tolower (*pstrId);
		if ((iConunter < 7 && ch != cSep) || (iConunter == 7 && ch != '\0' && !isspace (ch)))
		{
			++pstrId;
			if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
			{
				return NULL;
			}
			iNumber <<= 4;
			iNumber += isdigit (ch) ? (ch - '0') : (ch - 'a' + 10);
			ch = *pstrId;
			if (iConunter < 7 && ch != cSep)
			{
				return NULL;
			}
		}
		/* Store result.  */
		pbyId[iConunter] = (unsigned char) iNumber;
		/* Skip cSep.  */
		++pstrId;
	}
	return pbyId;
}
/************************************************************************
 * Function: utilsConvertToInt
 *     convert to integer, similar to atoi
 *  Parameters:
 *     rValue - ASCII/text value
 *  Return:
 *     Returns converted integer value
************************************************************************/
int utilsConvertToInt(char* rValue)
{
	 int i = 0, value = 0, digit = 0;
	 char	tempVal[SIZE_64B];

	 if ( rValue == '\0' )
		 return 0;

	 memset(tempVal, 0x0, SIZE_64B);
	 memcpy(tempVal, rValue, (SIZE_64B-1));

   for ( i = 0, value = 0; rValue [ i ] != '\0'; ++i )
   {
      digit = rValue [ i ] - '0'; /* get value of current digit character */
      value = 10 * value + digit;
   }
	 return value;
}

/************************************************************************
 * Function: utilsConvertToDouble
 *     convert to double, similar to atof
 *  Parameters:
 *     rValue - ASCII/text value
 *  Return:
 *     Returns converted double value
************************************************************************/
double utilsConvertToDouble(char* rValue)
{
	 int i = 0;
	 double  value = 0., digit = 0., divider = 1.;
	 char	tempVal[SIZE_64B];

	 if ( rValue == '\0' )
		 return 0;

	 memset(tempVal, 0x0, SIZE_64B);
	 memcpy(tempVal, rValue, (SIZE_64B-1));

	 while ( isspace( rValue [ i++ ] ) );

   for ( i = 0; isdigit( rValue [ i ] ); ++i )
   {
      digit = rValue [ i ] - '0'; /* get value of current digit character */
      value = 10 * value + digit;
   }
	 if ( rValue [ i ] == '.' )
	 {
      i++;
			while ( isdigit( rValue [ i ] ) )
			{
				 digit = rValue [ i ] - '0';
				 value = 10 * value + digit;
				 i++;
				 divider *= 10;
			}
      value = value/divider;
	 }
	 return value;
}

/************************************************************************
 * Function: utilsBytesToDouble
 *     converting byte array to a double taking care of denominations
 *  Parameters:
 *     pbyId -  byte array
 *     size - number of elements in array.
 *  Return:
 *     Returns converted double value
************************************************************************/
double utilsBytesToDouble(unsigned char* pbyId, unsigned char size) {
  double value = 0, bvalue = 256, tvalue = 0;
  int i = 0;
  for (i = 0; i < size; i++) {
    if (i==0) value = 1;
    else value *= bvalue;
    tvalue += (pbyId[size-1-i]) * value;
  }
  return tvalue;
}
/************************************************************************
 * Function: utilsStrToByte16
 *     convert ":" separated hex string to byte array of size 16
 *  Parameters:
 *     pstrId - hex string
 *     pbyId - converted byte array
 *  Return:
 *     Returns NULL or pbyId
************************************************************************/
unsigned char* utilsStrToByte16(const char *pstrId, unsigned char* pbyId)
{
	int iConunter = 0;
	for (iConunter = 0; iConunter < 16; ++iConunter)
	{
		unsigned int iNumber = 0;
		char ch;
		//Convert letter into lower case.
		ch = tolower (*pstrId++);
		if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
		{
			return NULL;
		}
		//Convert into number.
		//a. If chareater is digit then ch - '0'
		//b. else (ch - 'a' + 10) it is done because addition of 10 takes correct value.
		iNumber = isdigit (ch) ? (ch - '0') : (ch - 'a' + 10);
		ch = tolower (*pstrId);
		if ((iConunter < 15 && ch != delim) || (iConunter == 15 && ch != '\0' && !isspace (ch)))
		{
			++pstrId;
			if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
			{
				return NULL;
			}
			iNumber <<= 4;
			iNumber += isdigit (ch) ? (ch - '0') : (ch - 'a' + 10);
			ch = *pstrId;
			if (iConunter < 15 && ch != delim)
			{
				return NULL;
			}
		}
		/* Store result.  */
		pbyId[iConunter] = (unsigned char) iNumber;
		/* Skip delim.  */
		++pstrId;
	}
	return pbyId;
}
/************************************************************************
 * Function: utilsByteToStr
 *     coverts 8 byte hex array to string
 *  Parameters:
 *     pbyId -  hex byte array
 *     pstrId - converted hex string
 *  Return:
 *     Returns NULL or pstrId
************************************************************************/
unsigned char *utilsByteToStr(unsigned char *pbyId, char* pstrId)
{
	snprintf(pstrId, (2*strlen((char *)pbyId)), "%02x%c%02x%c%02x%c%02x%c%02x%c%02x%c%02x%c%02x", pbyId[0] & 0xff,
						cSep, pbyId[1]& 0xff, cSep, pbyId[2]& 0xff, cSep, pbyId[3]& 0xff,
						cSep, pbyId[4]& 0xff, cSep, pbyId[5]& 0xff, cSep, pbyId[6]& 0xff,
						cSep, pbyId[7]& 0xff);
	return (unsigned char*)pstrId;
}
/************************************************************************
 * Function: utilsByteToStrG
 *     coverts specified byte hex array to string
 *  Parameters:
 *     bytebuf -  hex byte array
 *		 len - length
 *     s - converted hex string
 *  Return:
 *     Returns NULL or s
************************************************************************/
unsigned char* utilsByteToStrG(unsigned char *bytebuf, int len, char *s)
{
  int i;
	char *p = NULL;
	p = s;
	for (i=0;i<len;i++)
	{
		if (i == (len-1)) {
			snprintf(p, (2*strlen((char *)bytebuf)), "%02x",((*bytebuf++)&(0xFF)));
			p+=3;
		}else {
			snprintf(p, (2*strlen((char *)bytebuf)), "%02x:",((*bytebuf++)&(0xFF)));
			p+=3;
		}
	}
	return (unsigned char*)s;
}
/************************************************************************
 * Function: utilsByteToStrGS
 *     coverts specified byte hex array to string
 *  Parameters:
 *     bytebuf -  hex byte array
 *		 len - length
 *     s - converted hex string
 *  Return:
 *     Returns NULL or s
************************************************************************/
unsigned char* utilsByteToStrGS(unsigned char *bytebuf, int len, char *s)
{
  int i;
	char *p = NULL;
	p = s;
	for (i=0;i<len;i++)
	{
		snprintf(p, (2*strlen((char *)bytebuf)), "%02x",((*bytebuf++)&(0xFF)));
		p+=2;
	}
	return (unsigned char*)s;
}
/************************************************************************
 * Function: utilsByteToStrAS
 *     coverts specified byte array to string
 *  Parameters:
 *     bytebuf -  byte array
 *		 len - length
 *     s - converted hex string
 *  Return:
 *     Returns NULL or s
************************************************************************/
unsigned char* utilsByteToStrAS(unsigned char *bytebuf, int len, char *s)
{
  int i;
	char *p = NULL;
	p = s;
	for (i=0;i<len;i++)
	{
		snprintf(p, (2*strlen((char *)bytebuf)), "%c",(*bytebuf++));
		p+=1;
	}
	return (unsigned char*)s;
}
/************************************************************************
 * Function: BCDtoDecimal
 *     To convert current active price from BCD to Decimal format
 *  Parameters:
 *     valBCD - BCD value of current price
 *  Return:
 *  	 decimal equivalent of the BCD
 ************************************************************************/
int BCDtoDecimal(int valBCD)
{
	int valDecimal = 0, loop = 0, valbcd = valBCD;

	for (loop = 0; valbcd; loop++)
	{
		valDecimal += (valbcd & 0x0F)*(pow(10,loop));
		valbcd >>= 4;
	}
	return valDecimal;
}
/************************************************************************
 * Function: HexStrToBytesStr
 *     To convert hex string to byte array
 *  Parameters:
 *     str - hex string
 *		 len - lenght of hex string
 *		 buffer - byte array output
 *  Return:
 *  	 NULL or byte array output
 ************************************************************************/
unsigned char* HexStrToBytesStr(char *str, int len, unsigned char *buffer)
{
	char c;
	int bx = 0, sx = 0;

	if ((strlen(str) == 0) || (((strlen(str))%2) != 0) || (len == 0)) {
		return NULL;
	}
	for (bx = 0, sx = 0; bx < len/2; ++bx, ++sx)
	{
		// Convert first half of byte
		c = str[sx];
		buffer[bx] = ((c > '9' ? (c > 'Z' ? (c - 'a' + 10) : (c - 'A' + 10)) : (c - '0')) << 4);
		// Convert second half of byte
		c = str[++sx];
		buffer[bx] |= (c > '9' ? (c > 'Z' ? (c - 'a' + 10) : (c - 'A' + 10)) : (c - '0'));
	}
	return buffer;
}


#endif // - end of __STR_UTILS__

int utilsReplaceFileString(FILE* updfp, FILE* origfp ,const  char *str,char *value)
{
    char    ret_val =0;
    char    key_string_buf[SIZE_256B];
    char    replaced_string[SIZE_256B];
    char    ret_str[SIZE_256B];
    int   n=0;

    while(fgets((char*)ret_str,SIZE_256B,origfp)!=NULL)
    {

	if(sscanf((char*)ret_str,"%s",key_string_buf)<=0)
	{
	    continue;
	}
	if(!strcmp((char*)key_string_buf,"#"))
	{
	    //      continue;
	}

	if(strlen((char*)key_string_buf)==0)
	{
	    continue;
	}

	if(!strncasecmp((char*)str,(char*)key_string_buf,strlen(str)))
	{
	    snprintf(replaced_string, sizeof(replaced_string),"%s=%s", str, value);
	    printf("------%d-%d-%d\n", n,strlen(ret_str),strlen(replaced_string));
	    fwrite(replaced_string,strlen(replaced_string),1,updfp);
	    fputc('\n',updfp);
	    ret_val=1;
	}
	else
	{
	    fwrite(ret_str,strlen(ret_str),1,updfp);
	}
    }
    if(!ret_val )
    {
	snprintf(replaced_string, sizeof(replaced_string), "%s=%s", str, value);
	fwrite(replaced_string,strlen(replaced_string),1,updfp);
    }

    rewind(origfp);
    return ret_val;
}

char *utilsRemDelimitStr(char *src, char *key)
{
	char *dest = NULL;
	size_t len_src = 0;
	size_t len_key = 0;
	int found = -1;
	int i = 0;
	int j = 0;
	int k = 0;

	len_src = strlen( src );
	len_key = strlen( key );
	/*
	 ** Allocate memory for the destination and initialise it
	 */
	dest = (char *) ZALLOC( sizeof( char ) * len_src + 1 );
	/*
	 ** MAIN LOOP. For each character in the source, we check against the key.
	 ** We use the 'found' boolean to evaluate whether we need to copy or not.
	 */
	for ( i = 0; i < len_src; i++ ){
		found = 0;
		for ( j = 0; j < len_key; j++ ){
			if ( src[i] == key[j] )
				found = 1;
		}
		/*
		 ** Copy the character if it was NOT found in the key
		 */
		if (0 == found){
			dest[k] = src[i];
			k++;
		}
	}
	/*
	 ** Return the destination pointer to the main function
	 */
	return ( dest );
}

void remoteParseDomainLkup(char *dname, char ipArr[][SIZE_64B], int *num)
{
	struct hostent *he = NULL;


	*num = 0;
	he = gethostbyname (dname);
	if (he)
	{
	    unsigned int i=0;
	       while ( he -> h_addr_list[i] != NULL) {
		    strncpy(ipArr[*num], (inet_ntoa( *( struct in_addr*)( he -> h_addr_list[i]))), SIZE_64B-1);
		    *num = *num + 1;
		    i++;
	       }
	}
	return;
}

#define MICROS_PER_SECOND 1000000
#define NANOS_PER_MICROSEC 1000
int pluginUsleep (unsigned int delay)
{
    struct timespec ts, rem;
    int rc = 0;

    if(delay >= MICROS_PER_SECOND) {
        ts.tv_sec = delay/MICROS_PER_SECOND;
        ts.tv_nsec = (delay%MICROS_PER_SECOND) * NANOS_PER_MICROSEC;
    } else {
        ts.tv_sec = 0;
        ts.tv_nsec = delay * NANOS_PER_MICROSEC;
    }
    memset(&rem, 0x0, sizeof(struct timespec));
    rc = nanosleep (&ts,&rem);

    while(rc != 0) {
				APP_LOG("UPnP: Device",LOG_DEBUG, "Interrupt!!!. Sleeping for remaining %u seconds", (unsigned int) rem.tv_sec);
				if(EINVAL == rc) {
						APP_LOG("UPnP: Device",LOG_DEBUG, "Interval %u is not correct", (unsigned int) rem.tv_sec);
						break;
				}
	ts.tv_sec = rem.tv_sec;
	ts.tv_nsec = rem.tv_nsec;
	memset(&rem, 0x0, sizeof(struct timespec));
	rc = nanosleep (&ts,&rem);
    }

    return rc;
}

void* saveSettingThread(void *arg)
{
	pluginUsleep(50);
	APP_LOG("UPnP: Device",LOG_DEBUG,
			"SaveSetting Thread running");
	SaveSetting();
	return NULL;
}

char *convertSSID(char *ssid)
{
    char rawSSID[MAX_ESSID_LEN+1], tmp[MAX_ESSID_LEN+1];

    memset(rawSSID, '\0', MAX_ESSID_LEN+1);
    memset(tmp, '\0', MAX_ESSID_LEN+1);

    if(!strncasecmp(ssid, "0x",2)) {

	strncpy(tmp, (ssid + 2), sizeof(tmp)-1);
	strncpy(ssid, tmp, MAX_ESSID_LEN);

	convertInRawBytes(ssid, rawSSID);
	strncpy(ssid, rawSSID, MAX_ESSID_LEN);
    }
    return ssid;
}

char *convertMonth(int month)
{
    char *monthArray[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

    return monthArray[month];
}

int utilsReplaceString(char* fName, const  char *str,char *value)
{
    FILE *updfp=NULL;
    FILE *origfp=NULL;
    char new_file[SIZE_128B];
    snprintf(new_file, sizeof(new_file), "%s.temp",fName);
    /*Opening Temp file */
    if(!str) return -1;
    if((updfp=fopen(new_file,"w+") )== NULL)
    {
	perror("fopen");
	return -1;
    }
    /* Opening exixting file */
    if((origfp=fopen(fName,"rw+"))==NULL)
    {
	perror("fopen");
	fclose(updfp);
	updfp = NULL;
	unlink(new_file);
	return -1;
    }
    if(origfp!=NULL && updfp !=NULL&&(str!=NULL))
    {
	utilsReplaceFileString(updfp,origfp,str,value);
	fclose(origfp);
	fclose(updfp);
	origfp = NULL;
	updfp = NULL;
	unlink(fName);
	rename(new_file,fName);
    }
		if (!origfp) {fclose(origfp); origfp = NULL;}
		if (!updfp) {fclose(updfp); updfp = NULL;}
    return 0;
}

/************************************************************************
 * Function: isStrPrintAble
 *     checks for the printable characters in the char string
 *  Parameters:
 *     pInPutStr -  char array
 *     strLen - length
 *  Return:
 *     Returns int , true/false
************************************************************************/
int isStrPrintAble(char *pInPutStr, int strLen)
{
    unsigned char i=0;
    for (i=0; i<strLen; i++)
    {
        if ((pInPutStr[i] < 0x20) || (pInPutStr[i] > 0x7E))
            return 0;
    }

    return 1;
}

/************************************************************************
 * Function: convertInRawBytes
 *     Converts the hex char string to string
 *  Parameters:
 *     ssidStr - hex string
 *     str - converted string
 *  Return:
 *     Returns int
************************************************************************/
int convertInRawBytes(char *ssidStr, char *str)
{
    int i=0,len=0;
    unsigned char ch1, ch2;

    len = strlen(ssidStr);

    for(i=0; i<len; i = i+2) {
        ch1 = convert(ssidStr[i]);
        ch2 = convert(ssidStr[i+1]);
        str[i/2] = ((ch1 << 4) | ch2);
    }

    return 0;
}

/************************************************************************
 * Function: convert
 *     This is the helper function to convertInRawBytes
 *     converts the hex characters to int values
 *  Parameters:
 *     ch -  char
 *  Return:
 *     Returns char
************************************************************************/
char convert(unsigned char ch)
{
    switch (ch)
    {
        case 'A':
        case 'a':
            return 10;
        case 'B':
        case 'b':
            return 11;
        case 'C':
        case 'c':
            return 12;
        case 'D':
        case 'd':
            return 13;
        case 'E':
        case 'e':
            return 14;
        case 'F':
        case 'f':
            return 15;
        default:
            if(ch >= 48 || ch <= 57) {
                return (ch - 48);
            } else
                return 0;
    }
}

/** Startup banner.
 * Print a simple but visually distinct startup banner to stdout.
 * Intended use: call from early in main().
 */
void startup_banner(void) {
  printf(
"************************************************************************\n"
"*                                                                      *\n"
"*                    WEMO APPLICATION STARTING                         *\n"
"*                                                                      *\n"
"************************************************************************\n"
);
}

/*
   Add XML items and optionally values to a mxml_node_t node from an
   XmlElementList array.

   Returns 0 on success or a source file number on error.

   Possible data types:
   	'1' ... '9' - scaled Integer with N decimal places
		'b' - Belkin NVRAM parameter
   	'd' - Decimal value
   	'n' - No value, just add tag
   	's' - String
   	'S' - pointer to string pointer

   For example:

	XmlElementList Elements[] = {
		{"CountryCode","country_code",'b'},
		{"FirmwareVersion",g_szFirmwareVersion,'s'},
   	{"signalStrength",&gSignalStrength,'d'},
   	{"USB",NULL,'n'},
   	{NULL}	// end of table
   };

   do {
	...

		if((ErrLine = AddXmlElements(pNode,Elements)) != 0) {
		  break;
		}
   ...
   while(FALSE);

	if(ErrLine != 0) {
		APP_LOG("UPNPDevice",LOG_ERR,"AddXmlElements failed on line %d",ErrLine);
	}


*/
int AddXmlElements(mxml_node_t *pTop,XmlElementList *pList)
{
	int ErrLine = 0;
	mxml_node_t *pNew;
	char *cp;
	int Div;
	int x;
	int y;
	char Fmt[8];	// "%d.%nd"

	while(pList->Tag != NULL) {
		if((pNew = mxmlNewElement(pTop,pList->Tag)) == NULL) {
			ErrLine = __LINE__;
			break;
		}
		switch(pList->type) {
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			// Scaled integer
			// Calculate scaling factor (10,100, 1000...)
				Div = 10;
				x = (pList->type - '1');
				while(x-- > 0) {
					Div *= 10;
				}

				x = *((int *) pList->Value);
				y = x % Div;
				x /= Div;
				if(y < 0) {
					y = -y;
				}
				snprintf(Fmt,sizeof(Fmt),"%%d.%%0%dd",Div);
				if(mxmlNewTextf(pNew,FALSE,Fmt,x,y) == NULL) {
					ErrLine = __LINE__;
				}
				break;

			case 'b':	// get value from NVRAM
				cp = GetBelkinParameter((char *) pList->Value);
				if(cp != NULL) {
					if(*cp != 0) {
						if(mxmlNewText(pNew,FALSE,cp) == NULL) {
							ErrLine = __LINE__;
						}
					}
				}
				break;

			case 'd':	// Decimal value
				if(mxmlNewInteger(pNew,*((int *) pList->Value)) == NULL) {
					ErrLine = __LINE__;
				}
				break;

			case 's':	// String value
				if(mxmlNewText(pNew,FALSE,(char *) pList->Value) == NULL) {
					ErrLine = __LINE__;
				}
				break;

			case 'S':	// String value via pointer to a char pointer
				cp = *((char **) pList->Value);
				if(cp != NULL && *cp != 0) {
					if(mxmlNewText(pNew,FALSE,cp) == NULL) {
						ErrLine = __LINE__;
					}
				}
				break;

			case 'n':	// Null value
				break;

			default:
				ErrLine = __LINE__;
				break;
		}
		if(ErrLine != 0) {
			break;
		}
		pList++;
	}

	return ErrLine;
}


// Encode '<', '>', '&', '"', '\'' characters.
// Returned string must be free'ed by caller
char* EscapeXML(char *pszRespBuf)
{
	int Strlen;
	char *cp;
	char *cp1;
	int OutBufLen = 0;
	char *Ret = NULL;
	char c;

	//& --> "&amp;"
	//< --> "&lt;"
	//> --> "&gt;"
	//" --> "&quot;"
	//' --> "&apos;"

	#define MAX_ESC_LEN		6


	do {
		if(pszRespBuf == NULL) {
			APP_LOG("Notification",LOG_DEBUG,"Error: called with NULL pointer");
			break;
		}
		if((Strlen = strlen(pszRespBuf)) == 0) {
			APP_LOG("Notification",LOG_DEBUG,"Error: called with NULL string");
			break;
		}
		// Calculate the size of the output string
		OutBufLen = Strlen + 1;
		cp = pszRespBuf;

		while( (c = *cp++) != 0 )
		{
			if( c == '<' || c == '>' || c == '&' || c == '"' || c == '\'' )
			{
				// need 4 or more bytes to escape these characters, so use max escpace string length to calculate it simply.
				OutBufLen += MAX_ESC_LEN;
			}
		}

		if((Ret = (char *) MALLOC(OutBufLen)) == NULL) {
			APP_LOG("Notification",LOG_ERR,"Malled failed");
			break;
		}

		cp = pszRespBuf;
		cp1 = Ret;

		while( (c = *cp++) != 0 )
		{
			if( c == '<' )
			{
				strcpy(cp1, "&lt;");
				cp1 += 4;
			}
			else if( c == '>' )
			{
				strcpy(cp1, "&gt;");
				cp1 += 4;
			}
			else if( c == '&' )
			{
				strcpy(cp1, "&amp;");
				cp1 += 5;
			}
			else if( c == '"')
			{
				strcpy(cp1, "&quot;");
				cp1 += 6;
			}
			else if( c == '\'' )
			{
				strcpy(cp1, "&apos;");
				cp1 += 6;
			}
			else
			{
				*cp1++ = c;
			}
		}

		*cp1++ = 0;

	} while(FALSE);

	return Ret;
}


