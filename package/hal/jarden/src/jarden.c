#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>

#include "jarden.h"
#define	JARDEND_PRIVATE
#include "jardend.h"

static int gSocket = -1;
static AdrUnion gHisAdr;

#ifdef DEBUG
#define LOG(format, ...) printf(format, ## __VA_ARGS__)
#else
#define LOG(format, ...) syslog(LOG_ERR,format, ## __VA_ARGS__)
#endif
static int GetDataFromDaemon(int Type,void *p);

void DumpHex(void *data, int len);

static int UdpSend(int fd,AdrUnion *pToAdr,JardendMsg *pMsg,int MsgLen)
{
	int Ret = 0;	// assume the best
	int BytesSent;

	BytesSent = sendto(fd,pMsg,MsgLen,0,&pToAdr->s,sizeof(pToAdr->s));
	if(BytesSent != MsgLen) {
		LOG("%s: sendto failed - %s\n",__FUNCTION__,strerror(errno));
		Ret = -errno;
	}

	return Ret;
}

// Initialize Hardware Abstraction Layer for use
// Returns:
// 	  0 - SUCCESS
// 	> 0 - standard Linux errno error code
//		< 0 - HAL error code (see ERR_HAL_*)
int HAL_Init()
{
   int Ret = 0;

	if((gSocket = socket(AF_INET,SOCK_DGRAM,0)) == -1) {
		Ret = -errno;
		LOG("%s: socket() failed, %s\n",__FUNCTION__,strerror(Ret));
	}

	gHisAdr.i.sin_family = AF_INET;
	gHisAdr.PORT = htons(JARDEND_PORT);
	gHisAdr.ADDR = inet_addr("127.0.0.1");   // localhost

	openlog("libJarden.so",0,LOG_USER);

   return Ret;
}

// Return the current mode decoded from the product LEDS.
//		< 0 - HAL error code (see ERR_HAL_*)
JARDEN_MODE HAL_GetMode(void)
{
	JARDEN_MODE Mode;
	int Err = GetDataFromDaemon(CMD_GET_MODE,&Mode);
	if(Err != 0) {
		Mode = (JARDEN_MODE) Err;
	}
	return Mode;
}

// Set cooking mode
// Legal values are MODE_OFF, MODE_WARM, MODE_LOW or MODE_HIGH.
//		< 0 - HAL error code (see ERR_HAL_*)
int HAL_SetMode(JARDEN_MODE Mode)
{
	int Ret = 0;	// assume the best

	switch(Mode) {

	case MODE_OFF:
        case MODE_ON:
            	break;
	case MODE_WARM:
	case MODE_LOW:
	case MODE_HIGH:
		/*if(PRODUCT_CROCKPOT != gProductType) {
		    printf("Inside if(PRODUCT_CROCKPOT != gProductType) \n");
		    Ret = -EINVAL;
		}*/ 
		break;
	default:
		Ret = -EINVAL;
		break;
	}
	
	printf("Ret is %d \n ", Ret);
	if(Ret == 0) {
	// Set the new mode
		Ret = GetDataFromDaemon(CMD_SET_MODE,&Mode);
		
	}
	printf("Return value from hal_setmode is %d \n", Ret);
	return Ret;
}

// Return number of minutes of cook time remaining from product LED display.
// A value of 0 indicates that the cockpot is not in a cooking mode
//		< 0 - HAL error code (see ERR_HAL_*)
int HAL_GetCookTime(void)
{
	int Ret;
	int Err = GetDataFromDaemon(CMD_GET_COOK_TIME,&Ret);
	//printf("Err = GetDataFromDaemon(CMD_GET_COOK_TIME,&Ret) returns %d \n", Err);
	//printf("Ret received by  HAL_GetCookTime(void) %d \n", Ret);
	if(Err != 0) {
		Ret = Err;
	}
	return Ret;
}

// Set number of minutes to cook time at currently selected temperature.
// The current mode must be MODE_LOW or MODE_HIGH to set a cook time.
//
// Note: requested value will be rounded up to nearest 30 minute increment
// (CrockPot hardware limitations)
//		< 0 - HAL error code (see ERR_HAL_*)
int HAL_SetCookTime(int Minutes)
{
	int Ret = 0;	// assume the best
	//JardendMsg Request;

    /* if(PRODUCT_CROCKPOT == gProductType) */ {  
        do {
            if(Minutes < 0 || Minutes > MAX_COOK_TIME) {
            // Argment out of range
                Ret = -EINVAL;
                break;
            }
        // Set cook time
            Ret = GetDataFromDaemon(CMD_SET_COOK_TIME,&Minutes);
        } while(FALSE);
    }
    return Ret;
}


/*
 * Return number of minutes of delay time remaining from product.
 * < 0 - error
 */
int HAL_GetDelayTime(void)
{
	int Ret;
	int Err = GetDataFromDaemon(CMD_GET_DELAY,&Ret);
	if(Err != 0) {
		Ret = Err;
	}
	return Ret;
}

/*
 * Set number of minutes to delay the togle operation.
 * If the device is ON, it will be powered OFF after this delay minutes
 * If the device is OFF, it will be powered ON after this delay minutes
 * < 0 - error
 */
int HAL_SetDelayTime(int Minutes)
{
	int Ret = 0;	// assume the best
	JardendMsg Request;
    /* if(PRODUCT_CROCKPOT != gProductType) */ { 
        do {
            if(Minutes <= 0 || Minutes > MAX_DELAY_TIME) {
            // Argment out of range
                Ret = -EINVAL;
                break;
            }
            // Set cook time
            Ret = GetDataFromDaemon(CMD_SET_DELAY,&Minutes);
        } while(FALSE);
    }
	return Ret;
}

static int GetDataFromDaemon(int Type,void *p)
{
	int Ret = 0;	// assume the best
	int Err;
	int BytesRead;
	int MsgLen;
	int DataLen = 0;
	int ReturnedDataLen;
	fd_set ReadFdSet;
	struct timeval Timeout;
	JardendMsg Request;
	JardendMsg Reply;
	socklen_t Len = sizeof(gHisAdr);
	
	do {
		if(p == NULL) {
		// Make sure we were passed a return data pointer
			Ret = EINVAL;
			LOG("%s: ERROR - passed null pointer\n",__FUNCTION__);
			break;
		}

		if(gSocket < 0) {
			if((Ret = HAL_Init()) != 0) {
				break;
			}
		}

		MsgLen = sizeof(Request.Hdr);
		memset(&Request,0,MsgLen);
		Request.Hdr.Cmd = Type;

		switch(Type) {
		case CMD_SET_MODE:
			DataLen = sizeof(Request.u.Mode);
			printf("Datalen is %d \n",DataLen);			
			break;
		case CMD_SET_COOK_TIME:
			DataLen = sizeof(Request.u.IntData);
			break;
        	case CMD_SET_DELAY:
            		DataLen = sizeof(Request.DelayTime);
            		break;

    		case CMD_GET_MODE:
    		case CMD_GET_COOK_TIME:

    		ReturnedDataLen = sizeof(Reply.u.IntData);
		break;

    		case CMD_GET_DELAY:
    			ReturnedDataLen = sizeof(Reply.DelayTime);
    			break;

    default:
    	break;
		}

	if(DataLen != 0) {
            if(Type == CMD_SET_DELAY){
                MsgLen = offsetof(JardendMsg,DelayTime) + DataLen;
                memcpy(&Request.DelayTime,p,DataLen);
            }
            else{
                MsgLen = offsetof(JardendMsg,u) + DataLen;
		//printf("MsgLen is %d \n",MsgLen);
                memcpy(&Request.u,p,DataLen);
            }

		}

        if((Ret = UdpSend(gSocket,&gHisAdr,&Request,MsgLen)) != 0) {
		break;
		}

// Wait for data then read it
      FD_ZERO(&ReadFdSet);
      FD_SET(gSocket,&ReadFdSet);
      Timeout.tv_sec = 1;
      Timeout.tv_usec = 0;

      if((Err = select(gSocket+1,&ReadFdSet,NULL,NULL,&Timeout)) < 0) {
         LOG("%s: select failed - %s\n",__FUNCTION__,strerror(errno));
			Ret = -errno;
         break;
      }

		if(Err == 0) {
			LOG("%s: timeout waiting for data\n",__FUNCTION__);
			Ret = ERR_HAL_DAEMON_TIMEOUT;
			break;
      }

		BytesRead = recvfrom(gSocket,&Reply,sizeof(Reply),0,&gHisAdr.s,&Len);
		
		//printf("BytesRead is %d \n", BytesRead);

		if(BytesRead > 0) {
			if(Reply.Hdr.Cmd != (Type | CMD_RESPONSE)) {
			// Opps
				Ret = ERR_HAL_INVALID_RESPONSE_TYPE;
			}
			else {
				//int ReturnedDataLen = BytesRead-offsetof(JardendMsg,DelayTime);
				//int ReturnedDataLen = 4;

				Ret = Reply.Hdr.Err;
				if(ReturnedDataLen > 0) {

					if(Type == CMD_GET_MODE){
						memcpy(p,&Reply.u.IntData,ReturnedDataLen);
					}
					else if(Type == CMD_GET_DELAY){
					memcpy(p,&Reply.DelayTime,ReturnedDataLen);
					}
					else if(Type == CMD_GET_COOK_TIME){
					memcpy(p,&Reply.u.IntData,ReturnedDataLen);
					}
					else{
						/*Do nothing*/
						
					}

				}
			}
		}
	} while (FALSE);
	
	
	return Ret;
}

// Shutdown the HAL deamon.
//
// 	  0 - SUCCESS
// 	> 0 - standard Linux errno error code
//		< 0 - HAL error code (see ERR_HAL_*)
int HAL_Shutdown()
{
	MsgHeader Dummy;
	return GetDataFromDaemon(CMD_EXIT,&Dummy);
}

// Free and release resources used by HAL
//
// 	  0 - SUCCESS
// 	> 0 - standard Linux errno error code
//		< 0 - HAL error code (see ERR_HAL_*)
int HAL_Cleanup()
{
	if(gSocket >= 0) {
		close(gSocket);
		gSocket = -1;
	}

	return 0;
}

const char *HAL_strerror(int Err)
{
	const char *Ret = NULL;

	switch(Err) {
	case ERR_HAL_INVALID_RESPONSE_TYPE:
		Ret = "ERR_HAL_INVALID_RESPONSE_TYPE";
		break;

	case ERR_HAL_DAEMON_TIMEOUT:
		Ret = "ERR_HAL_DAEMON_TIMEOUT";
		break;

	case ERR_HAL_WRONG_MODE:
		Ret = "ERR_HAL_WRONG_MODE";
		break;
    case ERR_HAL_WRONG_CMD:
        Ret = "ERR_HAL_WRONG_CMD";
        break;
	default:
		Ret = strerror(-Err);
	}

	return Ret;
}

