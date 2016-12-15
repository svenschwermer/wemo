/*
 * Copyright 2010 Tristan Lelong
 * 
 * This file is part of xmodem.
 * 
 * xmodem is free software: you can redistribute it and/or modify it under 
 * the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or 
 * (at your option) any later version.
 * 
 * xmodem is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along 
 * with xmodem. If not, see http://www.gnu.org/licenses/.
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>

#define HEAD			3
#define CSUM		2
#define PKT_LEN 		128
#define CRCXMODEM	0x0000
#define RALINK_GPIO_HAS_5124            1
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

void signal_handler(int signum)
{
	printf("gpio tester: signal ");
	if (signum == SIGUSR1)
		printf("SIGUSR1");
	else if (signum == SIGUSR2)
		printf("SIGUSR2");
	else
		printf("%d", signum);
	printf(" received\n", signum);
}

void gpio_test_intr(int gpio_num)
{
	//set gpio direction to input
	gpio_set_dir(gpio2100, gpio_in);

	//enable gpio interrupt
	gpio_enb_irq();

	//register my information
	gpio_reg_info(gpio_num);

	//issue a handler to handle SIGUSR1
	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);

	//wait for signal
	pause();

	//disable gpio interrupt
	gpio_dis_irq();
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

void em357_bootloadermode(void)
{
	int value;

	value = gpio_test_read();
	value &= ~(1<<2);
	value &= ~(1<<11);

	gpio_test_write(value);
	
	usleep(150*10000);
	value = gpio_test_read();

	value |= (1<<2);

	gpio_test_write(value);

	usleep(150*10000);
	value = gpio_test_read();
}

void em357_recover(void)
{
	int value;

	value = gpio_test_read();
	value &= ~(1<<2);
	value |= (1<<11);

	gpio_test_write(value);
	
	usleep(150*10000);
	value = gpio_test_read();

	value |= (1<<2);

	gpio_test_write(value);

	usleep(150*10000);
	value = gpio_test_read();
}
///////////////////////////////////////

void xmodem_interrupt(int sign)
{
	xmodem_running = 0;
	close(serial);

	exit(0);
}

int xmodem_calculate_crc(unsigned int crc, unsigned char data)
{
	crc  = (unsigned char)(crc >> 8) | (crc << 8);
	crc ^= data;
	crc ^= (unsigned char)(crc & 0xff) >> 4;
	crc ^= (crc << 8) << 4;
	crc ^= ((crc & 0xff) << 4) << 1;
	return crc;
}

void xmodem_usage(const char * argv0)
{
	printf("usage: %s [-m <mode> -s <speed>] -p <serial port> -i <input file>\n", argv0);
	printf("options are:\n");
	printf("\t-s <baud>: 1200 1800 2400 4800 9600 19200 38400 57600 230400 115200. default is 115200\n");
	printf("\t-m <mode>: [5-8] [N|E|O] [1-2] ex: 7E2. default is 8N1 \n");
	printf("\t-p <serial port>\n");
	printf("\t-i <file>\n");

	exit(0);
}

struct termios origtermios;
void xmodem_configure_serial(int port, int speed, char * mode)
{
	struct termios options;

	tcgetattr(serial, &options);
	memcpy(&origtermios, &options, sizeof(struct termios));
	options.c_cflag |= (CLOCAL | CREAD);

	switch(speed) {
		case 1200:
			cfsetispeed(&options, B1200);
		break;
		case 1800:
			cfsetispeed(&options, B1800);
		break;
		case 2400:
			cfsetispeed(&options, B2400);
		break;
		case 4800:
			cfsetispeed(&options, B4800);
		break;
		case 9600:
			cfsetispeed(&options, B9600);
		break;
		case 19200:
			cfsetispeed(&options, B19200);
		break;
		case 38400:
			cfsetispeed(&options, B38400);
		break;
		case 57600:
			cfsetispeed(&options, B57600);
		break;
		case 230400:
			cfsetispeed(&options, B230400);
		break;
		case 115200:
		default:
			cfsetispeed(&options, B115200);
		break;
	}

	options.c_cflag &= ~CSIZE;
	switch(mode[0]) {
		case '5':
			options.c_cflag |= CS5;
		break;
		case '6':
			options.c_cflag |= CS6;
		break;
		case '7':
			options.c_cflag |= CS7;
		break;
		case '8':
		default:
			options.c_cflag |= CS8;
		break;
	}

	switch(mode[1]) {
		case 'E':
			options.c_cflag |= PARENB;
			options.c_cflag &= ~PARODD;
		break;
		case 'O':
			options.c_cflag |= PARENB;
			options.c_cflag |= PARODD;
		break;
		case 'N':
			options.c_cflag &= ~PARENB;
		default:
		break;
	}

	switch(mode[2]) {
		case '1':
			options.c_cflag &= ~CSTOPB;
		break;
		case '2':
		default:
			options.c_cflag |= CSTOPB;
		break;
	}

	options.c_cflag &= ~CRTSCTS;
	options.c_iflag = 0;
	options.c_oflag = 0;
	options.c_lflag = 0;
	options.c_cc[VMIN] = 1;
	options.c_cc[VTIME] = 0;
	tcflush(serial, TCIFLUSH);
	tcsetattr(serial, TCSANOW, &options);
}

unsigned short crc16_ccitt (const void *buf, int len )
{
	unsigned short crc = 0;
	while( len-- ) {
		int i;
		crc ^= *(char *)buf++ << 8;
		for( i = 0; i < 8; ++i ) {
			if( crc & 0x8000 )
				crc = (crc << 1) ^ 0x1021;
			else
				crc = crc << 1;
		}
	}
	return crc;
}

int main(int argc, char ** argv)
{
	int opt = 0;
	int cpt = 1;
	int retry = 0;
	int oldcpt = 0;
	int pkt = 0;
	int speed = 115200;

	char * mode = "8N1";
	const char * output = NULL;
	const char * input = NULL;
	unsigned char send_buf[PKT_LEN+HEAD+1];
	unsigned char recv_buf[16];

	struct timespec delta;
	
	signal(SIGINT, xmodem_interrupt);

	if(argc < 3) {
		xmodem_usage(argv[0]);
	}

	while((opt = getopt(argc, argv, "i:s:m:p:")) != -1) {
		switch (opt) {
			case 'i':
				input = optarg;
				break;
			case 'p':
				output = optarg;
				break;
			case 's':
				speed = atoi(optarg);
				break;
			case 'm':
				mode = optarg;
				break;
			default:
				printf("unknown option '%c'\n", opt);
				xmodem_usage(argv[0]);
				break;
		}
	}

	if(!input) {
		printf("missing input file %d\n", argc);
		xmodem_usage(argv[0]);
	}
	if(!output) {
		printf("missing serial port\n");
		xmodem_usage(argv[0]);
	}

	em357_bootloadermode();

	printf("########################################\n");
	printf("send file %s on %s\n", input, output);
	printf("########################################\n");
	serial = open(output, O_RDWR | O_NOCTTY | O_NDELAY);

	if(serial < 0) {
		printf("error opening '%s'\n",output);
		exit(-1);
	}

	fcntl(serial, F_SETFL, 0);
	xmodem_configure_serial(serial, speed, mode);

	FILE * fin = fopen(input, "r");
	if(fin == NULL) {
		printf("error opening '%s'\n",input);
		close(serial);
		exit(-1);
	}

	fseek(fin, 0, SEEK_END);
	int nb = ftell(fin);
	fseek(fin, 0, SEEK_SET);
	pkt = nb/PKT_LEN+((nb%PKT_LEN)?1:0); 
	printf("-- %d pkt to be sent\n", pkt);

	printf("-- wait for synchro\n"); 

	int chk = 0;
	char buf[10], recv_promt[200];;
	memset(buf, 0, sizeof(buf));
	memset(recv_promt, 0, sizeof(recv_promt));
	
	buf[0] = 0xd;
	write(serial, buf, 1);
	
	usleep(500*1000);

	buf[0] = 0x31;
	write(serial, buf, 1);

	while(xmodem_running) {
		delta.tv_sec = 0;
		delta.tv_nsec = 1000;
		nanosleep(&delta, NULL);

		memset(recv_buf, 0, sizeof(recv_buf));
		int size = read(serial, recv_buf, 1);

		if(size == 1) {
			if(recv_buf[0] == 'C') {
				printf("[WNC] got 'C' from em357\n");
				break;
			}
		}
	}
	printf("-- got 0x%02x: start transfer\n", recv_buf[0]);

	while(!feof(fin)) {
		int i;
		union  {
			unsigned int checksum;
			unsigned char val[4];
		} csum;
		int size_in = PKT_LEN;
		int size_out = 0;

		delta.tv_sec = 0;
		delta.tv_nsec = 1000;

		nanosleep(&delta, NULL);

		if(oldcpt < cpt) {
			retry = 0;
			memset(send_buf, 0, sizeof(send_buf));

			send_buf[0] = 0x01;
			send_buf[1] = cpt;
			send_buf[2] = 255 - cpt;

			size_in = fread(send_buf+3, 1, PKT_LEN, fin);

			unsigned short ccrc = crc16_ccitt(&send_buf[3], PKT_LEN);
			send_buf[PKT_LEN+HEAD] = (ccrc>>8) & 0xFF;
			send_buf[PKT_LEN+HEAD+1] = ccrc & 0xFF;

			oldcpt++;
		}

		retry++;

		size_out = write(serial, send_buf, PKT_LEN+HEAD+CSUM);
		if(size_out != PKT_LEN+HEAD+CSUM) {
			printf("ERROR: sent %d/%d bytes\n", size_out, PKT_LEN+HEAD+CSUM);
			exit(-1);
		} else {
			int total = 0;
			while(xmodem_running) {
				int size = read(serial, recv_buf, 1);
				total += size;
				if(total == 1) {
					if(recv_buf[0] == 0x15) {
						printf("ERROR[%02d] NACK\n", retry);
						break;
					} else if(recv_buf[0] == 0x06) {
						printf("OK[%02d/%02d]\r", cpt, pkt);
						fflush(stdout);
						cpt++;
						break;
					} else {
						printf("WARN: received 0x%02x ", recv_buf[0]);
					}
				} else {
					if(total % 20 == 1) {
						printf("WARN: received ");
					}
					printf("0x%02x ", recv_buf[0]);
					if(total % 20 == 0) {
						printf("\n");
					}
				}
			}
		}
	}
	printf("\n");

	printf("-- end of file\n"); 
	send_buf[0] = 0x04;
	write(serial, send_buf, 1);

	printf("-- EOT sent\n"); 

	int size_ans = read(serial, recv_buf, 1);
	if(size_ans == 1) {
		if(recv_buf[0] == 0x15) {
			printf("ERROR NACK\n");
		} else if(recv_buf[0] == 0x06) {
			printf("OK <EOT> ACKed\n"); 
		} else {
			printf("WARN: unexpected received 0x%02x ", recv_buf[0]);
		}
	}
	
	fclose(fin);
	printf("-- done\n"); 

	em357_recover();
	tcflush(serial, TCIFLUSH);
	tcsetattr(serial, TCSANOW, &origtermios);

	printf("########################################\n"); 
	printf("Transfert complete.\n");
	printf("########################################\n");

	close(serial);

	printf("\n");
	return 0;
}

