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
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termios.h>
#include <ctype.h>
#include <syslog.h>
#include <linux/serial.h>

#define	JARDEND_PRIVATE
#include "jarden.h"
#include "jardend.h"


#define LOG(format, ...) \
	if(gDebug) \
		printf(format, ## __VA_ARGS__); \
	else \
		syslog(LOG_ERR,format, ## __VA_ARGS__)

#define OPTION_STRING      "adD:mM:tT:sv"

//#ifndef SIMULATE
//#error "Only simulated operation is currently supported"
//int gSimulated = TRUE;
//#else
// Simulated functionality
//int gSimulated = FALSE;
//#endif

time_t StartTime = 0;
int gCookTime = 0;

JARDEN_MODE gCurrentMode = MODE_UNKNOWN;
JARDEN_MODE gLastMode = MODE_UNKNOWN;
JARDEN_MODE gDesiredMode = MODE_IDLE;
int gDesiredCookTime = -1;

int gTimeLeft;			// in minutes
JardendMsg gReply;
int gNewMode = -1;
int gNewCookTime = -1;
int gDelayTime = -1;
int gNewDelayTime = -1;
struct timeval gTimeNow;

// Number of seconds since program start.
// It might wrap, but it sure as hell won't go backwards until it does.
// All timers should be based on this, not the absolute time of day which
// may jump when the time of day is updated from the Internet.
time_t UpTime = 0;
time_t NextTimeOut = 0;

int gVerbose = 0;
int gDebug = 0;
int gAccelerateTime = FALSE;
int gIpcSocket = -1;

char *DevicePath = NULL;
int gRunMainLoop = TRUE;
int gSerialFd = -1;
AdrUnion gHisAdr;
int gResponseCmd = 0;
struct timeval gCmdTimer;

const char *ModeStrings[] = {
	"***Internal Error***",
	"is unknown",
	"is OFF",
	"was just turned on",
	"is keeping food WARM",
	"is cooking at LOW temperature",
	"is cooking at HIGH temperature",
	"is Powered ON",
};

const char *ModeLookup[] = {
	"***Internal Error***",
	"unknown",
	"OFF",
	"IDLE",
	"WARM",
	"LOW",
	"HIGH"
	"ON",
};
//#define NUM_MODES		8

void Usage(void);
int MainLoop(void);
int sio_async_init(char *Device);
int UdpInit(u16 Port);
void IpcRead(int fd);
int UdpSend(int fd,AdrUnion *pToAdr,JardendMsg *pMsg,int MsgLen);
int IPCTest(int Type);
int ProcessRxData(int fd);
void UpdateMode(void);
void SendCmd(char *Cmd);
void GetTimeNow();
void SetTimeout(struct timeval *p,int Timeout);
int TimeLapse(struct timeval *p);
void DumpHex(void *data, int len);

void Usage()
{
   printf("jardend $Revision: 1.2 $ compiled "__DATE__ " "__TIME__"\n");
   printf("Usage: jardend [options]\n");
   printf("\t  -p - Product to simulate using simulator\n");
   printf("\t  -a - Accelerate time for testing (60 x faster)\n");
   printf("\t  -d - debug\n");
   printf("\t  -D - set serial device path, i.e. /dev/ttyS0\n");
   printf("\t  -e - Send EXIT command to daemon to exit.\n");
   printf("\t  -m - Get and display current mode.\n");
   printf("\t  -M <mode> - Set mode to HIGH, LOW, WARM, ON or OFF.\n");
   printf("\t  -s - Simulate operation w/o hardware.\n");
   printf("\t  -t - Get and display current cooking time left.\n");
   printf("\t  -T <minutes> - Set cook time.\n");
   printf("\t  -v - verbose output\n");
}


int main(int argc, char *argv[])
{
   int Option;
   int Ret = 0;
	int bSendQuit = FALSE;
	int bTestCommand = FALSE;
	int bGetMode = FALSE;
	int bGetCookTime = FALSE;
	int bDaemon = FALSE;
	int bDelayOperation = FALSE;
	int bGetDelayTime = FALSE;

/*Assigning Crockpot as default device for the simulator*/
	gProductType = PRODUCT_CROCKPOT;
	

   while ((Option = getopt(argc, argv,OPTION_STRING)) != -1) {
      switch (Option) {
		case 'C':
			gProductType = PRODUCT_CROCKPOT;
			break;
		case 'I':
			gProductType = PRODUCT_SUNBEAMIRON;
			break;
		case 'F':
			gProductType = PRODUCT_MRCOFFEE;
			break;
		case 'P':
			gProductType = PRODUCT_OSTERPETFEEDER;
			break;
		case 'a':
			gAccelerateTime = TRUE;
			printf("Accelerated cook time selected (1 second = 1 minute)\n");
			break;

		case 'd':
			gDebug = 1;
			break;

        	case 'D':
			if(DevicePath != NULL) {
			   free(DevicePath);
			}
			DevicePath = strdup(optarg);
			bDaemon = TRUE;
			break;

		case 'e':
			bTestCommand = TRUE;
			bSendQuit = TRUE;
			break;

		case 'm':
			bTestCommand = TRUE;
			bGetMode = TRUE;
			break;

		case 'M':
			bTestCommand = TRUE;
			if(strcasecmp(optarg,"off") == 0) {
				gNewMode = (int) MODE_OFF;
			}
			else if(strcasecmp(optarg,"on") == 0) {
				gNewMode = (int) MODE_ON;
			}
			else if(strcasecmp(optarg,"warm") == 0) {
				gNewMode = (int) MODE_WARM;
			}
			else if(strcasecmp(optarg,"low") == 0) {
				gNewMode = (int) MODE_LOW;
			}
			else if(strcasecmp(optarg,"high") == 0) {
				gNewMode = (int) MODE_HIGH;
			}
			else {
				printf("Error: Invalid mode '%s'.\n",optarg);
				Ret = EINVAL;
				bTestCommand = FALSE;
			}
			break;

		case 's':
			gSimulated = TRUE;
				gCurrentMode = MODE_IDLE;
			break;

			case 't':
				bTestCommand = TRUE;
				bGetCookTime = TRUE;
				break;

		case 'T':
			if(sscanf(optarg,"%d",&gNewCookTime) != 1 ||
				gNewCookTime <= 0 || gNewCookTime > MAX_COOK_TIME)
			{
				printf("Error: Invalid cook time.\n");
				Ret = EINVAL;
			}
			else  {
				bTestCommand = TRUE;
			}
			break;

		/*case 'T':
			if(sscanf(optarg,"%d",&gNewDelayTime) != 1 ||
					gNewDelayTime <= 0 || gNewDelayTime > MAX_DELAY_TIME)
			{
				printf("Error: Invalid delay time.\n");
				Ret = EINVAL;
			}
			else  {
				bTestCommand = TRUE;
                	        bDelayOperation = TRUE;
			}
            break;
		*/

        /*case 't':
            bTestCommand = TRUE;
			bGetDelayTime = TRUE;
			break;
		*/
        case 'v':
            gVerbose++;
            break;

		default:
			Ret = EINVAL;
           		break;
      	}
   }

	if(Ret != 0 || (!bTestCommand && !bDaemon && !gSimulated)) {
		printf("jardend wrong param: Ret=%d,bTestCommand:%d,bDaemon:%d,gSimulated:%d \n",Ret,bTestCommand,bDaemon,gSimulated);
		Usage();
		exit(Ret);
	}

	if(Ret == 0) do {
		if(gDebug) {
			printf("jardend $Revision: 1.0 $ compiled "__DATE__ " "__TIME__"\n");
		}

		if(bTestCommand) {
			if(gNewMode != -1) {
				Ret = IPCTest(CMD_SET_MODE);
			}

			if(bGetMode) {
				Ret = IPCTest(CMD_GET_MODE);
			}

			if(gNewCookTime != -1) {
				Ret = IPCTest(CMD_SET_COOK_TIME);
			}

			if(bGetCookTime) {
				Ret = IPCTest(CMD_GET_COOK_TIME);
			}

			if(bSendQuit) {
				Ret = IPCTest(CMD_EXIT);
			}

			if(bDelayOperation) {
				Ret = IPCTest(CMD_SET_DELAY);
			}

            if(bGetDelayTime) {
				Ret = IPCTest(CMD_GET_DELAY);
			}
		}
		else {
			openlog("jardend",0,LOG_DAEMON);
			Ret = MainLoop();
		}
	} while(FALSE);

   return Ret;
}

int MainLoop()
{
   int Ret = 0;
   int Err;
   time_t TimeNow;
   time_t LastTime = 0;
   time_t DeltaT;
   time_t UpTime;

   gCmdTimer.tv_sec = 0;
   do {
		if(!gSimulated) {
			if((gSerialFd = sio_async_init(DevicePath)) < 0) {
				LOG("%s: sio_async_init() failed\n",__FUNCTION__);
				Ret = -gSerialFd;
				break;
			}
		}
		else {
			LOG("%s: operating in simulated mode.\n",__FUNCTION__);
		}

		if(((gIpcSocket = UdpInit(JARDEND_PORT))) < 0) {
			LOG("%s: UdpInit() failed (%d)\n",__FUNCTION__,gIpcSocket);
		}
   } while(FALSE);

	if(Ret == 0 && !gDebug) do {
      pid_t pid;

      printf("Becoming a Daemon ...\n");

      // Fork daemon
      if((pid = fork()) < 0) {
         perror("Forking");
         Ret = errno;
			break;
      }
      else if(pid != 0) {
      // parent
			exit(0);
			break;
      }

   // must be the child
      if(setsid() == -1) {
         LOG("%s: setsid() failed: %s\n",__FUNCTION__,strerror(errno));
			Ret = errno;
			break;
      }

      if(daemon(1, 0) < 0) {
         LOG("%s: daemon(1,0) failed: %s\n",__FUNCTION__,strerror(errno));
			Ret = errno;
      }
	} while(FALSE);

	if(!gDebug) {
		LOG("jardend $Revision: 1.2 $ compiled "__DATE__ " "__TIME__" starting\n");
	}

	if(!gSimulated) {
	// Send version request to sync with PIC
		SendCmd("V");
	}

// Run "forever loop" until error or exit
   while(gRunMainLoop && Ret == 0) {
		fd_set ReadFdSet;
		struct timeval Timeout;
		int MaxFd = gSerialFd;

		GetTimeNow();
      FD_ZERO(&ReadFdSet);
		if(!gSimulated) {
			FD_SET(gSerialFd,&ReadFdSet);
		}
      FD_SET(gIpcSocket,&ReadFdSet);

		if(gIpcSocket > gSerialFd) {
			MaxFd = gIpcSocket;
		}

		if(gCmdTimer.tv_sec != 0 && TimeLapse(&gCmdTimer) >= 0) {
		// Push the "Select" button again
			gCmdTimer.tv_sec = 0;
			SendCmd("S");
		}

		if(gCmdTimer.tv_sec != 0) {
		// Calculate timeout
         Timeout.tv_usec = gCmdTimer.tv_usec - gTimeNow.tv_usec;
         Timeout.tv_sec = gCmdTimer.tv_sec - gTimeNow.tv_sec;
         if(Timeout.tv_usec < 0) {
            Timeout.tv_usec += 1000000;
            Timeout.tv_sec--;
         }
		}
		else {
			Timeout.tv_sec = 0;
			Timeout.tv_usec = 500000;	// 1/2 second
		}

      if((Err = select(MaxFd+1,&ReadFdSet,NULL,NULL,&Timeout)) < 0) {
         LOG("%s: select failed - %s\n",__FUNCTION__,strerror(errno));
			Ret = errno;
         break;
      }

		GetTimeNow();

		if(!gSimulated) {
			if(FD_ISSET(gSerialFd,&ReadFdSet)) {
				ProcessRxData(gSerialFd);
			}
		}

      if(FD_ISSET(gIpcSocket,&ReadFdSet)) {
			IpcRead(gIpcSocket);
		}
   }

   if(gSerialFd >= 0) {
      close(gSerialFd);
   }

	if(gIpcSocket >= 0) {
		close(gIpcSocket);
	}

   if(DevicePath != NULL) {
      free(DevicePath);
   }

   return Ret;
}


#define SERIAL_OPEN_FLAGS O_FSYNC | O_APPEND | O_RDWR | O_NOCTTY | O_NDELAY
// return fd or <0 if on error
int sio_async_init(char *Device)
{
   int fd = -1;
   int Err = -1;  // assume the worse
  struct termios options;
  struct serial_struct ser;

  do {
     // open the port, set the stuff
     if((fd = open(Device,SERIAL_OPEN_FLAGS)) < 0) {
        printf("Couldn't open \"%s\": %s\n",Device,strerror(errno));
        Err = -errno;
        break;
     }

     if(tcgetattr(fd,&options) < 0) {
       printf("Unable to read port configuration: %s\n",strerror(errno));
       Err = -errno;
       break;
     }

     options.c_oflag &= ~OPOST;
     options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  // raw mode
     options.c_iflag &= ~(IXON | IXOFF | IXANY);  /* disable software flow control */
     options.c_iflag |=  IGNBRK;            /* ignore break */
     options.c_iflag &= ~ISTRIP;            /* do not strip high bit */
     options.c_iflag &= ~(INLCR | ICRNL);   /* do not modify CR/NL   */
     options.c_cflag |=  (CLOCAL | CREAD);  /* enable the receiver and set local mode */
     options.c_cflag &= ~CRTSCTS;           /* disable hardware flow control */

     options.c_cflag &= ~CSIZE;             /* Mask the character size bits */
     options.c_cflag |= CS8;                /* 8 data bits */
     options.c_cflag &= ~PARENB;            /* no parity */
     options.c_cflag &= ~CSTOPB;            /* 1 stop bit */


     if(cfsetspeed(&options,B57600) == -1) {
        printf("cfsetspeed() failed: %s\n",strerror(errno));
        Err = -errno;
        break;
     }

     if(tcsetattr(fd,TCSANOW,&options) < 0) {
        printf("tcsetattr() failed: %s\n",strerror(errno));
        Err = -errno;
        break;
     }

     Err = 0;
  } while(FALSE);

  if(Err == 0) {
     if(gVerbose) {
        printf("\"%s\" opened successfully @ 57600 baud\n",Device);
     }
     Err = fd;
  }
  else if(fd >= 0) {
     close(fd);
  }

  return Err;
}

// Initialize communications on UDP port <port>.
// Enables messages to be received on the specified port from other
// applications. This function must be called once and only once for
// each port the application wishes to receive message on.
// return == 0: Ok, otherwise error code
int UdpInit(u16 Port)
{
   int Ret = 0;   // assume the best
   AdrUnion MyAdr;
   int On = 1;
   int Socket = -1;

   do {
      if((Socket = socket(AF_INET,SOCK_DGRAM,0)) == -1) {
         Ret = -errno;
         LOG("%s: socket() failed, %s\n",__FUNCTION__,strerror(Ret));
         break;
      }

      if(setsockopt(Socket,SOL_SOCKET,SO_REUSEADDR,(char *) &On,
                    sizeof(On)) == -1)
      {
         LOG("%s: Warning - setsockopt() failed: %s\n",__FUNCTION__,
                 strerror(errno));
      }

      MyAdr.i.sin_family = AF_INET;
      MyAdr.PORT = htons(Port);
		MyAdr.ADDR = inet_addr("127.0.0.1");   // localhost

      if(bind(Socket,&MyAdr.s,sizeof(MyAdr)) == -1) {
         Ret = -errno;
         LOG("%s: bind() failed for %s:%d %s\n",__FUNCTION__,
                 inet_ntoa(MyAdr.i.sin_addr),Port,strerror(errno));
         break;
      }
   } while(FALSE);

   if(Ret == 0) {
		Ret = Socket;
	}
	else {
      if(Socket != -1) {
         close(Socket);
      }
   }

   return Ret;
}

void IpcRead(int fd)
{
	JardendMsg Request;
	int BytesRead;
	socklen_t Len = sizeof(gHisAdr);
	int DataLen = -1;

	memset(&gReply,0,sizeof(gReply));

	BytesRead = recvfrom(fd,&Request,sizeof(Request),0,&gHisAdr.s,&Len);
	if(BytesRead <= 0) {
		LOG("%s: read failed: %d (%s)\n",__FUNCTION__,errno,strerror(errno));
	}
	else {
	// We read something
		if(gVerbose) {
			printf("%s: Got request:\n",__FUNCTION__);
			DumpHex(&Request,BytesRead);
		}
		gReply.Hdr.Cmd = Request.Hdr.Cmd | CMD_RESPONSE;
		if(gSimulated) {
			UpdateMode();
		}

		switch(Request.Hdr.Cmd) {
			case CMD_EXIT:
				gRunMainLoop = FALSE;
				DataLen = 0;
				LOG("%s: Received CMD_EXIT, exiting...\n",__FUNCTION__);
				break;

			case CMD_GET_MODE:
				gReply.u.Mode = gCurrentMode;
				DataLen = sizeof(gReply.u.Mode);
				break;

			case CMD_SET_MODE:
				if(gSimulated) {
					gCurrentMode = Request.u.Mode;
				}
				else if(gCurrentMode != Request.u.Mode){
				// Need to change modes
					gDesiredMode = Request.u.Mode;
					if(Request.u.Mode == MODE_OFF) {
						SendCmd("O");
					}
					else {
					// Start changing modes by "pushing" the select button
						SendCmd("S");
					}
				}
				DataLen = 0;
				break;

			case CMD_GET_COOK_TIME:
				gReply.u.IntData = gTimeLeft;
				DataLen = sizeof(gReply.u.IntData);
				break;

			case CMD_SET_COOK_TIME:
				if(gCurrentMode == MODE_LOW || gCurrentMode == MODE_HIGH) {
					if(gSimulated) {
						time(&StartTime);
						gCookTime = ((Request.u.IntData + 29) / 30) * 30;
					}
					else {
						gDesiredCookTime = ((Request.u.IntData + 29) / 30) * 30;
						if(gTimeLeft < gDesiredCookTime) {
						// Push the "Up" button
							SendCmd("U");
						}
						else if(gTimeLeft > gDesiredCookTime) {
						// push the "Down" button
							SendCmd("D");
						}
						else {
						// Nothing to do
							gDesiredCookTime = -1;
						}
					}
					gReply.u.IntData = 0;
				}
				else {
					gReply.Hdr.Err = ERR_HAL_WRONG_MODE;
				}
				DataLen = 0;
				break;
            case CMD_GET_DELAY:
                gReply.DelayTime = gTimeLeft;
				DataLen = sizeof(gReply.DelayTime);
                break;
            case CMD_SET_DELAY:
                if(gCurrentMode == MODE_ON || gCurrentMode == MODE_OFF) {
					if(gSimulated) {
						time(&StartTime);
						gDelayTime = Request.DelayTime;
					}
                }
                else {
					gReply.Hdr.Err = ERR_HAL_WRONG_MODE;
				}
                DataLen = 0;
                break;
			default:
				LOG("%s: Invalid request 0x%x ignored.\n",__FUNCTION__,
					 Request.Hdr.Cmd);
				break;
		}

		if(DataLen >= 0) {
			DataLen += offsetof(JardendMsg,u);
			UdpSend(fd,&gHisAdr,&gReply,sizeof(gReply.Hdr)+DataLen);
		}
	}
}

int UdpSend(int fd,AdrUnion *pToAdr,JardendMsg *pMsg,int MsgLen)
{
	int Ret = 0;	// assume the best
	int BytesSent;

	if(gVerbose > 1) {
		printf("%s: Sending %d bytes:\n",__FUNCTION__,MsgLen);
		DumpHex(pMsg,MsgLen);
	}
	BytesSent = sendto(fd,pMsg,MsgLen,0,&pToAdr->s,sizeof(pToAdr->s));
	if(BytesSent != MsgLen) {
		LOG("%s: sendto failed - %s\n",__FUNCTION__,strerror(errno));
		Ret = -errno;
	}

	return Ret;
}

int IPCTest(int Type)
{
   int Ret = 0;
	int bHalInitialized = FALSE;
	JARDEN_MODE Mode;
	int Ret_temp;

	do {
      if((Ret = HAL_Init()) != 0) {
         printf("%s: HAL_Init() failed, %s\n",__FUNCTION__,HAL_strerror(Ret));
         break;
      }
		bHalInitialized = TRUE;

		switch(Type) {
            case CMD_GET_MODE:
            	Ret_temp = HAL_GetMode();
                Ret = (int) Ret_temp;
                if((Ret = (int) HAL_GetMode()) < 0) {
                    printf("%s: HAL_GetMode() failed: %s\n",__FUNCTION__,
                             HAL_strerror(Ret));
                    break;
                }
                else if(Ret >= NUM_MODES) {
                    printf("Error: HAL_GetMode returned invalid mode (0x%x)\n",Ret);
                }
                else {
                    printf("Jarden %s.\n",ModeStrings[Ret]);
                }
                break;

			case CMD_SET_MODE:

                if((Ret = HAL_SetMode(gNewMode)) < 0) {
                    LOG("%s: HAL_SetMode() failed: %s\n",__FUNCTION__,
                         HAL_strerror(Ret));
                    break;
                }
				break;

			case CMD_GET_COOK_TIME:
				if((Ret = HAL_GetCookTime()) < 0) {
					printf("%s: HAL_GetCookTime() failed: %s\n",__FUNCTION__,
							 HAL_strerror(Ret));
					break;
				}
				else {
					printf("Ret = HAL_GetCookTime() is %d \n",Ret);
					printf("Cook time remaining: %02d:%02d\n",Ret/60,Ret%60);
				}
				break;

			case CMD_SET_COOK_TIME:
				if((Ret = HAL_SetCookTime(gNewCookTime)) < 0) {
					printf("%s: HAL_SetCookTime() failed: %s\n",__FUNCTION__,
							 HAL_strerror(Ret));
					break;
				}
				else {
					printf("Requested cook time: %02d:%02d\n",
							 gNewCookTime/60,gNewCookTime%60);
				}
				break;

			case CMD_EXIT:
				if((Ret = HAL_Shutdown()) != 0) {
					printf("%s: HAL_Shutdown() failed: %s\n",__FUNCTION__,
							 HAL_strerror(Ret));
					break;
				}
				printf("\nDaemon Exiting\n");
				break;
            case CMD_SET_DELAY:
                if((Ret = HAL_SetDelayTime(gNewDelayTime)) < 0) {
					printf("%s: HAL_SetDelayTime() failed: %s\n",__FUNCTION__,
							 HAL_strerror(Ret));
					break;
				}
				else {
					printf("Requested delay time: %02d:%02d\n",
							 gNewDelayTime/60,gNewDelayTime%60);
				}
                break;

            case CMD_GET_DELAY:
				if((Ret = HAL_GetDelayTime()) < 0) {
					printf("%s: HAL_GetDelayTime() failed: %s\n",__FUNCTION__,
							 HAL_strerror(Ret));
					break;
				}
				else {
					printf("DelayTime is : %02d:%02d\n",Ret/60,Ret%60);
				}
				break;


			default:
				printf("%s#%d: Internal error\n",__FUNCTION__,__LINE__);
				break;
		}
	} while(FALSE);

	if(bHalInitialized && (Ret = HAL_Cleanup()) != 0) {
		printf("%s: HAL_Cleanup() failed, %s\n",__FUNCTION__,
				 HAL_strerror(Ret));
	}

   return Ret;
}

/*
   A PIC microprocessor on the Jarden monitors the 4 - 7 segment LED
	displays and 3 mode LEDS and sends the data to us when the data changes
   and periodically.  The data is sent in 5 bytes:

   Byte 1:
   	Bit7 - 1
   	Bit6 - 0
   	Bit5 - 0
   	Bit4 - 0
   	Bit3 - 1 if firmware version string (bit 0..2 must be zero)
   	Bit2 - "High" LED
   	Bit1 - "Low" LED
   	Bit0 - "Keep Warm" LED

   Byte 2:
   	Bit7 - 0
   	Bit6 - Digit 1 Segment A
   	Bit5 - Digit 1 Segment B
   	Bit4 - Digit 1 Segment C
   	Bit3 - Digit 1 Segment D
   	Bit2 - Digit 1 Segment E
   	Bit1 - Digit 1 Segment F
   	Bit0 - Digit 1 Segment G

   Byte 3:
   	Bit7 - 0
   	Bit6 - Digit 2 Segment A
   	Bit5 - Digit 2 Segment B
   	Bit4 - Digit 2 Segment C
   	Bit3 - Digit 2 Segment D
   	Bit2 - Digit 2 Segment E
   	Bit1 - Digit 2 Segment F
   	Bit0 - Digit 2 Segment G

   Byte 4:
   	Bit7 - 0
   	Bit6 - Digit 3 Segment A
   	Bit5 - Digit 3 Segment B
   	Bit4 - Digit 3 Segment C
   	Bit3 - Digit 3 Segment D
   	Bit2 - Digit 3 Segment E
   	Bit1 - Digit 3 Segment F
   	Bit0 - Digit 3 Segment G

   Byte 5:
   	Bit7 - 0
   	Bit6 - Digit 4 Segment A
   	Bit5 - Digit 4 Segment B
   	Bit4 - Digit 4 Segment C
   	Bit3 - Digit 4 Segment D
   	Bit2 - Digit 4 Segment E
   	Bit1 - Digit 4 Segment F
   	Bit0 - Digit 4 Segment G

   A single byte of data is sent to the PIC to control
   the 4 push buttons:

   'O' - "Push" the Off button
   'U' - "Push" the Up button
   'D' - "Push" the Down button
   'S' - "Push" the Select button
   'V' - Request firmware version string

	any other value does nothing.
*/

char Lookup[] = {
	0x7e,
	0x30,
	0x6d,
	0x79,
	0x33,
	0x5b,
	0x5f,
	0x70,
	0x7f,
	0x7b
};

char Translate(u8 Byte)
{
	int i = 0;
	char Ret = '?';

	for(i = 0; i < 10; i++) {
		if(Byte == Lookup[i]) {
			Ret = i + '0';
			break;
		}
	}
	return Ret;
}

int ProcessRxData(int fd)
{
   int bFrameFound = FALSE;
   int BytesRead;
	int Ret = 0;	// Assume no error, no new data
	typedef enum {
		STATE_GET_START,
		STATE_GET_DATA,
		STATE_GET_FW_VER,
	} RxState;

	static RxState State = STATE_GET_START;
	static u8 Data[16];
	static int RxCount = 0;
	u8 RxBuf[16];
	int i;
	char CookTime[6];
	int Hours;
	int Minutes;

	do {
		BytesRead = read(fd,RxBuf,sizeof(RxBuf));
		if(BytesRead < 0) {
			if(errno != EAGAIN) {
				Ret = errno;
				LOG("%s: read failed - %s\n",__FUNCTION__,strerror(errno));
			}
			break;
		}


		if(gVerbose > 2) {
			printf("%s: read %d bytes\n",__FUNCTION__,BytesRead);
			DumpHex(RxBuf,BytesRead);
		}

		for(i = 0; i < BytesRead; i++) {
			u8 Byte = RxBuf[i];
			Data[RxCount++] = Byte;

			switch(State) {
				case STATE_GET_START:
					if(Byte == 0x80 || Byte == 0x81 || Byte == 0x82 || Byte == 0x84 ||
						Byte == 0x87)
					{	// Valid first byte for Display data
						if(gVerbose > 2) {
							printf("Got Start byte 0x%x\n",Byte);
						}
						State = STATE_GET_DATA;
					}
					else if(Byte == 0x88) {
					// Firmware Version string
						State = STATE_GET_FW_VER;
					}
					else {
						RxCount = 0;
					}
					break;

				case STATE_GET_DATA:
					if(RxCount == 5) {
					// We have a complete frame
						switch(Data[0]) {
						case 0x80:
						// At power up all three LEDs flash on and off at once,
						// after a button is pessed all three LEDs off mean OFF
							if(gCurrentMode != MODE_IDLE) {
								gCurrentMode = MODE_OFF;
							}
							break;

						case 0x87:
							// At power up all three LEDs flash on and off at once
							gCurrentMode = MODE_IDLE;
							break;

						case 0x81:
							gCurrentMode = MODE_LOW;
							break;

						case 0x82:
							gCurrentMode = MODE_WARM;
							break;

						case 0x84:
							gCurrentMode = MODE_HIGH;
							break;

							default:
								LOG("%s: Data[0] 0x%x is invalid.\n",__FUNCTION__,
									 Data[0]);
						}

						if(gCurrentMode != gLastMode) {
						// Mode has changed
							if(gDesiredMode != MODE_IDLE) {
							// We're changing the modes
								SendCmd(" ");
								if(gCurrentMode != gDesiredMode) {
								// Need to push the mode button again after a rest
									SetTimeout(&gCmdTimer,50);
								}
								else {
								// We're done.
									gDesiredMode = MODE_IDLE;
									LOG("%s: Mode successfully changed to %s.\n",
										 __FUNCTION__,ModeLookup[gCurrentMode]);
								}
							}
						}

						if(Data[1] == 0 && Data[2] == 0 && Data[3] == 0 &&
							Data[4] == 0)
						{	// Cook time display is blank
							gTimeLeft = 0;
						}
						else {
							CookTime[0] = Translate(Data[1]);
							CookTime[1] = Translate(Data[2]);
							CookTime[2] = ':';
							CookTime[3] = Translate(Data[3]);
							CookTime[4] = Translate(Data[4]);
							CookTime[5] = 0;

							if(sscanf(CookTime,"%d:%d",&Hours,&Minutes) != 2) {
								LOG("%s: Ignored invalid cook time '%s'\n",__FUNCTION__,
									 CookTime);
							}
							else {
								gTimeLeft = Hours * 60 + Minutes;
							}
						}

						if(gDesiredCookTime != -1 && gTimeLeft == gDesiredCookTime) {
							gDesiredCookTime = -1;
							LOG("%s: Mode successfully set cook time to %d:%02d.\n",
								 __FUNCTION__,gTimeLeft/60,gTimeLeft%60);
							SendCmd(" ");	// release button
						}

						if(gVerbose) {
							DumpHex(Data,5);
						}

						if(gDebug) {
							printf("Got display data: %s %02d:%02d\n",
									 ModeLookup[gCurrentMode],gTimeLeft/60,
									 gTimeLeft%60);
						}

						RxCount = 0;
						State = STATE_GET_START;
					}
					break;

				case STATE_GET_FW_VER:
					if(Byte == 0) {
					// End of string
						if(Data[1] == 'V') {
						// Valid version string, log it
							LOG("%s: Received PIC firmware version '%s'\n",__FUNCTION__,
								 &Data[1]);
						}
						else if(gDebug) {
							printf("%s: Received invalid firmware version string:\n",
									 __FUNCTION__);
							DumpHex(Data,RxCount);
						}
						RxCount = 0;
						State = STATE_GET_START;
					}
					else if(RxCount == sizeof(Data)) {
					// Buffer overflow, ignore the data
						if(gDebug) {
							printf("%s: Buffer overflow getting firmware version string:\n",
									 __FUNCTION__);
							DumpHex(Data,RxCount);
						}
						RxCount = 0;
						State = STATE_GET_START;
					}
					break;

				default:
					LOG("%s: Invalid state 0x%x, resetting\n",__FUNCTION__,State);
					State = STATE_GET_START;
					break;
			}
		}
	} while(FALSE);

	if(bFrameFound && Ret == 0) {
		Ret = bFrameFound;
	}

	return Ret;
}

void UpdateMode(void)
{
	time_t TimeNow;

	gTimeLeft = 0;

	if(1)/* PRODUCT_CROCKPOT == gProductType) */ { 
		if(gCookTime > 0) {
		    if(gCurrentMode == MODE_LOW || gCurrentMode == MODE_HIGH) {
		        time(&TimeNow);
		        if(gAccelerateTime) {
		        // decrement time remaing each 1 second
		            gTimeLeft = gCookTime - (TimeNow-StartTime);
		        }
		        else {
		        // decrement time remaing once a minute
		            gTimeLeft = gCookTime - (TimeNow-StartTime)/60;
		        }

		        if(gTimeLeft <= 0) {
		        // Cook time is complete change to warming mode
		            gTimeLeft = 0;
		            gCookTime = 0;
		            gCurrentMode = MODE_WARM;
		        }
		    }
		}
	}
	else {
		if(gDelayTime > 0) {
		    if(gCurrentMode == MODE_ON || gCurrentMode == MODE_OFF) {
		        time(&TimeNow);
		        if(gAccelerateTime) {
		        // decrement time remaing each 1 second
		            gTimeLeft = gDelayTime - (TimeNow-StartTime);
		        }
		        else {
		        // decrement time remaing once a minute
		            gTimeLeft = gDelayTime - (TimeNow-StartTime)/60;
		        }

		        if(gTimeLeft <= 0) {
		        // Cook time is complete change to warming mode
		            gTimeLeft = 0;
		            gDelayTime = 0;
		            gCurrentMode = !gCurrentMode;
		        }
		    }
		}
	}
}

void SendCmd(char *Cmd)
{
	int WriteLen = strlen(Cmd);

	if(gDebug) {
		printf("%s: Sending command '%s'\n",__FUNCTION__,Cmd);
	}
	if(write(gSerialFd,Cmd,WriteLen) != WriteLen) {
		LOG("%s: write failed sending '%s' - %s\n",__FUNCTION__,Cmd,
			 strerror(errno));
	}
}

void GetTimeNow()
{
   struct timezone tz;
   gettimeofday(&gTimeNow,&tz);
}

// Set a timeout for <Timeout> milliseconds from now
void SetTimeout(struct timeval *p,int Timeout)
{
   long microseconds;

   GetTimeNow();
   microseconds = gTimeNow.tv_usec + ((Timeout % 1000) * 1000);

   p->tv_usec = microseconds % 1000000;
   p->tv_sec  = gTimeNow.tv_sec + (Timeout / 1000) + microseconds / 1000000;
}

// Return number of milliseconds of time since p
int TimeLapse(struct timeval *p)
{
   int DeltaSeconds = gTimeNow.tv_sec - p->tv_sec;
	int Ret;

   if(DeltaSeconds > (0x7fffffff / 1000)) {
   // TimeLapse is too big to express in milliseconds, just return
   // the max we can.
      return 0x7fffffff;
   }
   Ret = (DeltaSeconds * 1000) +
          ((gTimeNow.tv_usec - p->tv_usec) / 1000);

	return Ret;
}

void DumpHex(void *data, int len)
{
   int i = 0;
   int j;
   u8 *cp;
   u8 *cp1;
   char temp[17];
   int Adr = 0;

   cp = (u8 *) data;
   cp1 = (u8 *) data;
   while(len--) {
      printf("%02X ",*cp++);
      if(++i == 16) {
         for(j = 0; j <16; j++) {
            if(isprint(*cp1)) {
               temp[j] = *cp1;
            }
            else {
               temp[j] = '.';
            }
            cp1++;
         }
         temp[j] = 0;
         printf("  %s\n",temp);
         Adr += 16;
         i = 0;
      }
   }

   if(i) {
      for(j = 0; j <i; j++) {
         if(isprint(*cp1)) {
            temp[j] = *cp1;
         }
         else {
            temp[j] = '.';
         }
         cp1++;
      }
      temp[j] = 0;
      printf("  %s\n",temp);
   }
}


