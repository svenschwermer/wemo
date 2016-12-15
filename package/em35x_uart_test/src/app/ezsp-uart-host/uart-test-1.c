/** @file uart-test-1.c
 *  @brief ASH protocol reset and startup test
 * 
 * <!-- Copyright 2009-2010 by Ember Corporation. All rights reserved.   *80*-->
 */

#include PLATFORM_HEADER
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <sys/ioctl.h>

#include <sys/ioctl.h>

#include "stack/include/ember-types.h"
#include "hal/micro/generic/ash-protocol.h"
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-io.h"
#include "app/ezsp-uart-host/ash-host-ui.h"

#define HEAD			3
#define CSUM		2
#define PKT_LEN 		128
#define CRCXMODEM	0x0000

int serial = 0;
int xmodem_running = 1;

///////////////////////////////////////
#define GPIO_DEV	"/dev/gpio"

#define RALINK_GPIO_SET_DIR			0x01
#define RALINK_GPIO_SET_DIR_IN		0x11
#define RALINK_GPIO_SET_DIR_OUT	0x12
#define RALINK_GPIO_READ			0x02
#define RALINK_GPIO_WRITE			0x03
#define RALINK_GPIO_SET				0x21
#define RALINK_GPIO_CLEAR			0x31
#define RALINK_GPIO_ENABLE_INTP	0x08
#define RALINK_GPIO_DISABLE_INTP	0x09
#define RALINK_GPIO_REG_IRQ		0x0A
#define RALINK_GPIO_LED_SET			0x41

/* * bit is the unit of length */
#if defined (RALINK_GPIO_HAS_2722)
#define RALINK_GPIO_NUMBER		28
#elif defined (RALINK_GPIO_HAS_4524)
#define RALINK_GPIO_NUMBER		46
#elif defined (RALINK_GPIO_HAS_5124)
#define RALINK_GPIO_NUMBER		52
#elif defined (RALINK_GPIO_HAS_7224)
#define RALINK_GPIO_NUMBER		73
#elif defined (RALINK_GPIO_HAS_9524)
#define RALINK_GPIO_NUMBER		96
#elif defined (RALINK_GPIO_HAS_9532)
#define RALINK_GPIO_NUMBER		73
#else
#define RALINK_GPIO_NUMBER		24
#endif

#define RALINK_GPIO_LED_INFINITY	4000

typedef struct {	
	unsigned int irq;		//request irq pin number	
	pid_t pid;			//process id to notify
} ralink_gpio_reg_info;

typedef struct {	
	int gpio;				//gpio number (0 ~ 23)	
	unsigned int on;		//interval of led on
	unsigned int off;		//interval of led off
	unsigned int blinks;	//number of blinking cycles
	unsigned int rests;	//number of break cycles
	unsigned int times;	//blinking times
} ralink_gpio_led_info;

enum {
	gpio_in,
	gpio_out,
};

enum {
	gpio2100,
	gpio2722,
};

int gpio_set_dir(int r, int dir)
{
	int fd, req;

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
	if (dir == gpio_in) {
		req = RALINK_GPIO_SET_DIR_IN;
	}
	else {
		req = RALINK_GPIO_SET_DIR_OUT;
	}
//	if (ioctl(fd, req, 0xffffffff) < 0) {
	if (ioctl(fd, req, 1<<14) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int gpio_read_int(int r, int *value)
{
	int fd, req;

	*value = 0;
	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}

	req = RALINK_GPIO_READ;
	if (ioctl(fd, req, value) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int gpio_write_int(int r, int value)
{
	int fd, req;

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}

	req = RALINK_GPIO_WRITE;
    	//printf("[WNC_tsunghao] : %s val = 0x%X\n", __func__, value );
	if (ioctl(fd, req, value) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int gpio_enb_irq(void)
{
	int fd;

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
	if (ioctl(fd, RALINK_GPIO_ENABLE_INTP) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int gpio_dis_irq(void)
{
	int fd;

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
	if (ioctl(fd, RALINK_GPIO_DISABLE_INTP) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int gpio_reg_info(int gpio_num)
{
	int fd;
	ralink_gpio_reg_info info;

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
	info.pid = getpid();
	info.irq = gpio_num;
	if (ioctl(fd, RALINK_GPIO_REG_IRQ, &info) < 0) {
		perror("ioctl");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

//void gpio_test_write(void)
void gpio_test_write(int val)
{
	//set gpio direction to output
	gpio_set_dir(gpio2100, gpio_out);

	//turn off LEDs
	//printf("[WNC_tsunghao] : %s val = 0x%X\n", __func__, val );
	gpio_write_int(gpio2100, val);

	return;
	sleep(3);

	//turn on all LEDs;
	gpio_write_int(gpio2100, 0);
}

//void gpio_test_read(void)
int gpio_test_read(void)
{
	int i, d;

	gpio_set_dir(gpio2100, gpio_in);
	gpio_read_int(gpio2100, &d);
	//printf("gpio 21~00 = 0x%x\n", d);

	return d;
}

void gpio_set_led(int argc, char *argv[])
{
	int fd;
	ralink_gpio_led_info led;

	led.gpio = atoi(argv[2]);
	if (led.gpio < 0 || led.gpio >= RALINK_GPIO_NUMBER) {
		printf("gpio number %d out of range (should be 0 ~ %d)\n", led.gpio, RALINK_GPIO_NUMBER);
		return;
	}
	led.on = (unsigned int)atoi(argv[3]);
	if (led.on > RALINK_GPIO_LED_INFINITY) {
		printf("on interval %d out of range (should be 0 ~ %d)\n", led.on, RALINK_GPIO_LED_INFINITY);
		return;
	}
	led.off = (unsigned int)atoi(argv[4]);
	if (led.off > RALINK_GPIO_LED_INFINITY) {
		printf("off interval %d out of range (should be 0 ~ %d)\n", led.off, RALINK_GPIO_LED_INFINITY);
		return;
	}
	led.blinks = (unsigned int)atoi(argv[5]);
	if (led.blinks > RALINK_GPIO_LED_INFINITY) {
		printf("number of blinking cycles %d out of range (should be 0 ~ %d)\n", led.blinks, RALINK_GPIO_LED_INFINITY);
		return;
	}
	led.rests = (unsigned int)atoi(argv[6]);
	if (led.rests > RALINK_GPIO_LED_INFINITY) {
		printf("number of resting cycles %d out of range (should be 0 ~ %d)\n", led.rests, RALINK_GPIO_LED_INFINITY);
		return;
	}
	led.times = (unsigned int)atoi(argv[7]);
	if (led.times > RALINK_GPIO_LED_INFINITY) {
		printf("times of blinking %d out of range (should be 0 ~ %d)\n", led.times, RALINK_GPIO_LED_INFINITY);
		return;
	}

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return;
	}
	if (ioctl(fd, RALINK_GPIO_LED_SET, &led) < 0) {
		perror("ioctl");
		close(fd);
		return;
	}
	close(fd);
}

static void printResults(EzspStatus status);

int main( int argc, char *argv[] )
{
  EzspStatus status;
  
  if (!ashProcessCommandOptions(argc, argv)) {
    printf("Exiting.\n");
    return 1;
  }

// Open serial port - verifies the name and permissions
// Disable RTS/CTS since the open might fail if CTS is not asserted.
  printf("\nOpening serial port... ");
  ashWriteConfig(resetMethod, ASH_RESET_METHOD_NONE);
  ashWriteConfig(rtsCts, FALSE);
  status = ashResetNcp();           // opens the serial port
  printResults(status);

// Check for RSTACK frame from the NCP - this verifies that the NCP is powered
// and not stuck in reset, and for the serial connection it verifies baud rate,
// character structure and that the NCP TXD to host RXD connection works.

  //printf("\nManually reset the network co-processor, then press enter:");
  printf("\nreset the network co-processor, then press enter:");

//WNC0_tsunghao Wed 21 Aug 2013 06:39:20 AM CST
#if 0
    int bcmgpio_fd = open("/dev/gpio", O_RDWR);	
    struct gpio_ioctl gpio;	
    if (bcmgpio_fd == -1)
    {		
        printf ("Failed to open /dev/gpio\n");		
    }	
    else 
    {	
        gpio.val = 0;
        gpio.mask = (unsigned long) 1 << 2;
    
        if (ioctl(bcmgpio_fd, RALINK_GPIO_SET_DIR_OUT, &gpio) < 0) 
        {		
		printf ("invalid RALINK_GPIO_SET_DIR_OUT\n");
        }
		 
        if (bcmgpio_fd!= -1) 
        {		
            close (bcmgpio_fd);
            bcmgpio_fd = -1;
        }
    }


    bcmgpio_fd = open("/dev/gpio", O_RDWR);
    if (bcmgpio_fd == -1)
    {		
        printf ("Failed to open /dev/gpio\n");		
    }	
    else 
    {	
        gpio.val = 0;
        gpio.mask = (unsigned long) 1 << 2;
        if (ioctl(bcmgpio_fd, RALINK_GPIO_RESET_EM357, &gpio) < 0) 
        {		
            printf ("invalid RALINK_GPIO_WRITE (pull low)\n");
        }
		 
        if (bcmgpio_fd!= -1) 
        {		
            close (bcmgpio_fd);
            bcmgpio_fd = -1;
        }
    }

    usleep(1000*1000);
#else
    //unsigned gpioVal = 0;
    //gpioVal = (unsigned long)gpio_test_read();
    //gpio_test_write(gpioVal + 4);
    //gpioVal = (unsigned long)gpio_test_read();

    int gpioVal;
    gpioVal = gpio_test_read();
    gpioVal &= ~(1<<2);

    gpio_test_write(gpioVal);

    usleep(150*10000);
    gpioVal = gpio_test_read();
    
    gpioVal |= (1<<2);
    
    gpio_test_write(gpioVal);

    usleep(150*10000);
    gpioVal = gpio_test_read();
#endif //WNC_DEL
//WNC0

    printf("reset finish\n");

  //(void)getchar();
  status = ashStart();              // waits for RSTACK
  printf("\nWaiting for RSTACK from network co-processor... ");
  printResults(status);

// Send a RST frame to the NCP, and listen for RSTACK coming back - this 
// verifies the data connection from the host TXD to the NCP RXD.
  printf("\nClosing and re-opening serial port... ");
  ashSerialClose();
  ashWriteConfig(resetMethod, ASH_RESET_METHOD_RST);
  status = ashResetNcp();         // opens the serial port
  printResults(status);
  printf("\nSending RST frame to network co-processor... ");
  status = ashStart();            // sends RST and waits for RSTACK
  printResults(status);
  return 0;
} 

static void printResults(EzspStatus status)
{
  if (status == EZSP_SUCCESS) {
    printf("succeeded.\n");
    return;
  } else {
    printf("failed.\n");
    switch (status) {
    case EZSP_ASH_HOST_FATAL_ERROR:
      printf("Host error: %s (0x%02X).\n", ashErrorString(ashError), ashError);
      break;
    case EZSP_ASH_NCP_FATAL_ERROR:
      printf("NCP error: %s (0x%02X).\n", ashErrorString(ncpError), ncpError);
      break;
    default:
      printf("\nEZSP error: %s (0x%02X).\n", ashEzspErrorString(status), status);
      break;
    }
  }
  exit(1);
}

//------------------------------------------------------------------------------
// EZSP callback function stubs

void ezspErrorHandler(EzspStatus status)
{}

void ezspTimerHandler(int8u timerId)
{}

void ezspStackStatusHandler(
      EmberStatus status) 
{}

void ezspNetworkFoundHandler(EmberZigbeeNetwork *networkFound,
                             int8u lastHopLqi,
                             int8s lastHopRssi)
{}

void ezspScanCompleteHandler(
      int8u channel,
      EmberStatus status) 
{}

void ezspMessageSentHandler(
      EmberOutgoingMessageType type,
      int16u indexOrDestination,
      EmberApsFrame *apsFrame,
      int8u messageTag,
      EmberStatus status,
      int8u messageLength,
      int8u *messageContents)
{}

void ezspIncomingMessageHandler(
      EmberIncomingMessageType type,
      EmberApsFrame *apsFrame,
      int8u lastHopLqi,
      int8s lastHopRssi,
      EmberNodeId sender,
      int8u bindingIndex,
      int8u addressIndex,
      int8u messageLength,
      int8u *messageContents) 
{}