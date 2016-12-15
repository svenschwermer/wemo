/*
 * wemo_netcam_ipc.c
 *
 *  Created on: Nov 16, 2012
 *      Author: simonx
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <fcntl.h>


#include "wemo_netcam_ipc.h"
#include "wemo_netcam_api.h"
#include "ddbg.h"

static pthread_t s_serverTaskThread = 0x00;
static pthread_t s_clientTaskThread = 0x00;

#define IPC_WEMO_NETCAM_PATH "/tmp/netcamipc.socket"
const int   MAX_CONNS           = 5;

static int netcam_client_sock = -1;

static sem_t s_gMotionMutex;

static IPCClientCallback g_spServerCallback = 0x00;

void SetSocketNonblocking(int sock)
{
	int opts;

	opts = fcntl(sock, F_GETFL);
	if (opts < 0) {
		perror("fcntl(F_GETFL)");
		exit(0x01);
	}
	opts = (opts | O_NONBLOCK);
	if (fcntl(sock, F_SETFL, opts) < 0) {
		perror("fcntl(F_SETFL)");
		exit(0x01);
	}
	return;
}

int SIZE_MSG(MESSAGE_T* pMsg)
{
    int size = 0x00;

    return size;
}

void            createIPCClient()
{
      //sem_init(&s_gMotionMutex, 0, 1);

      int rc = pthread_create(&s_clientTaskThread, 0x00, IPCClientTask, 0x00);
      if (0x00 != rc)
      {
          APP_LOG(LOG_E_WARNING, "Create IPC client thread failure");
      }
      else
      {
          //pthread_join(s_clientTaskThread, 0x00);
          pthread_detach(s_clientTaskThread);
      }
}

/**
 * IPCClientTask:     
 *      IPC client task to deal with connection, data receiving and etc
 *      Please note that, now it only support one connection and also one way 
 *      communication 
 *
 *
 */
void*           IPCClientTask(void* data)
{
      //APP_LOG(LOG_E_WARNING, "IPC client task running");
	int     sockfd;
	struct  sockaddr_in servaddr;

	//tu_set_my_thread_name( __FUNCTION__ );
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_port = htons(8891);
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	char* szMotion = "{1:Motion}";

	int nSent = sendto(sockfd, szMotion, strlen(szMotion), MSG_DONTWAIT, (struct sockaddr*)&servaddr, sizeof(servaddr));
	if (nSent > 0)
	{
		//printf("Data sent success:%s\n", szMotion);	
	}
	else
	{
		DDBG("Data sent failure:%s", szMotion);	
	}

    close(sockfd);

    return NULL;
}

void            IPCClientRegisterCallback(IPCClientCallback pCallback)
{

}

int             IPCSendMsg(SOCKET_ID socket_id, char* pData, int size)
{
    return WEMO_NETCAM_SUCCESS;
}

/******************Here all server related started***************************/
void            createIPCServer()
{
    int rc = pthread_create(&s_serverTaskThread, 0x00, IPCServerTask, 0x00);
    if (0x00 != rc)
    {
        APP_LOG(LOG_E_WARNING, "Create IPC server thread failure");
    }
    else
    {
        pthread_detach(s_serverTaskThread);
    }
}

#define			MAX_RECV_TIMEOUT	6	//2 seconds, to be changed later


/**
 * 
 *
 *
 *
 */
void*           IPCServerTask(void* data)
{
    APP_LOG(LOG_E_WARNING, "IPC server task running");

    int     sockfd;
 	struct sockaddr_in servaddr, cliaddr;
	fd_set read_fds; 

	//tu_set_my_thread_name( __FUNCTION__ );
	FD_ZERO(&read_fds);

 	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	int reuse = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_port = htons(8891);
	int rc = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	char recv_buff[1024];
	
    if (-1 == rc)
    {
		int exit_status = 0x00;
        perror("bind() failed:");
        pthread_exit(&exit_status);
    }
	int from_len = sizeof(cliaddr);
	
	printf("WeMo IPC server started\n");
	struct timeval tv;

	tv.tv_sec = 6;  /* 30 Secs Timeout */
	tv.tv_usec = 0;  // Not init'ing this can cause strange errors

	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

	for (;;)
	{
		 memset(recv_buff, 0x00, sizeof(recv_buff));
		#if 0		
		 struct timeval tv;
		 tv.tv_sec 	= MAX_RECV_TIMEOUT;
		 tv.tv_usec 	= 0;

		 int resp = select(sockfd + 1, &read_fds, (fd_set *)0x00, (fd_set *)0x00, &tv);
		 if (0x00 == resp)
	 	 {
			//- Timeout
			char signal = 0x00;
			DDBG("WeMo IPC: read timeout");
			if (0x00 != g_spServerCallback)
			{
				g_spServerCallback(&signal);
			}
		 }
		 else if (-1 == resp)
		 {
			// error
			 perror("select() error");
			 exit(0x00);
		 }
		 else
		 {
			int rect = recvfrom(sockfd, recv_buff, 1024, 0, 0x00, 0x00);
			 if (rect > 0x00)
			 {
				DDBG("IPC: data received: %s", recv_buff);
				char signal = 1;
				if (0x00 != g_spServerCallback)
				{
					g_spServerCallback(&signal);
				}
			 }
		 }
		#endif
			
		 int rect = recvfrom(sockfd, recv_buff, 1024, 0, 0x00, 0x00);
		 DDBG( "IPC: got %d bytes: \"%s\"", rect, recv_buff );
		 if (rect > 0x00)
		 {
			//printf("IPC: data received: %s\n", recv_buff);
			int signal = 1;
			if (0x00 != g_spServerCallback)
			{
				g_spServerCallback(signal);
			} else 
			  DDBG( "g_spServerCallback is NULL" );
		 }
		else
		{
			int signal = 0;
			if (0x00 != g_spServerCallback)
			{
				DDBG("IPC: data received: read time-out");				
				g_spServerCallback(signal);
			} else 
			  DDBG( "g_spServerCallback is NULL" );
		}
	
	}// -End for

}


int RegisteredCallBack(IPCClientCallback pCallBack)
{
    g_spServerCallback = pCallBack;
    printf("IPC: callback function registered:0x%08X", pCallBack);
    return 0;
}

/*
 *  Function:   
        notifyMotion()
 *  This API is to notify the motion event from NetCam
 *  really only call when socket connected eventually otherwise it will not do any thing
 *
 */
int notifyMotion(int state)
{
  DDBG( "(%d)", state );
	/*    
	if ( -1 != netcam_client_sock)
    {
      int rc = sem_post(&s_gMotionMutex);
      if (rc != 0x00)
      {
          perror("sem_post() failure");
      }
    }
	*/

	//createIPCClient();
	IPCClientTask(0x00);

 return 0;
}
