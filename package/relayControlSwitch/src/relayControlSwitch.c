#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#define GPIO_DEV "/dev/gpio"

/* must sync with ralink_gpio.h */
#define RALINK_GPIO_SET_DIR_IN      0x11
#define RALINK_GPIO_ENABLE_INTP     0x08
#define RALINK_GPIO_REG_IRQ         0x0A
#define RALINK_GPIO_IRQ             20
#define TO_STR(arg)                 #arg

//#define GPIO_DEV                    "/dev/gpio"
#define RALINK_GPIO_SET_DIR_IN      (0x11)
#define RALINK_GPIO_SET_DIR_OUT     (0x12)
#define RALINK_GPIO_READ            (0x02)
#define RALINK_GPIO_WRITE           (0x03)
#define RALINK_GPIO_GET_MODE        (0x123)
#define RALINK_GPIO_SET_MODE        (0x321)

enum {
    gpio2100,
    gpio2722,
};

typedef struct {
    unsigned int irq;   //request irq pin number
    pid_t pid;          //process id to notify
} ralink_gpio_reg_info;

static char *saved_pidfile;
static int gpio_pin;


char* itoa(int value, char* result, int base)
{
    // check that the base if valid
    if (base < 2 || base > 36) { *result = '\0'; return result; }

    char* ptr = result, *ptr1 = result, tmp_char;
    int tmp_value;

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while ( value );

    // Apply negative sign
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
    return result;
}

int gpio_read_int(int r, unsigned long *value)
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

unsigned long gpio_test_read_all_value(void)
{
    unsigned long d;
    char output[50];
    int i = 0;
    int len = 0;

//  gpio_set_dir_in_all(gpio2100);
    gpio_read_int(gpio2100, &d);
    printf("\nGPIO value 21~00:\n");
    printf("HEX: 0x%08X\n", d);
    printf("BIT: ");
    for(i = 21; i>=0; i--)
        printf("%02d ", i);
    printf("\n");

    memset(output, 0, sizeof(output));
    itoa(d, output, 2);
    printf("BIN:");
    len = strlen(output);
    for(i = 21; i>=0; i--) {
        if(i >= len)
            printf("  0");
        else
            printf("  %c", output[len-i-1]);
    }
    printf("\n");

    return d;
}

int gpio_set_dir_out(int r, int pinNO)
{
    int fd, req;

    fd = open(GPIO_DEV, O_RDONLY);
    if (fd < 0) {
        perror(GPIO_DEV);
        return -1;
    }
    req = RALINK_GPIO_SET_DIR_OUT;
    if (ioctl(fd, req, 1<<pinNO) < 0) {
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

    fd = open(GPIO_DEV, O_RDWR);
    if (fd < 0) {
        perror(GPIO_DEV);
        return -1;
    }
    req = RALINK_GPIO_WRITE;

    if (ioctl(fd, req, value) < 0) {
        perror("ioctl");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

/*
 * gpio interrupt handler -
 *   SIGUSR1 - reboot
 *   SIGUSR2 - restore default
 */
static void IrqHandler(int signum)
{
	unsigned long d;
	unsigned long pin_value;

	d = gpio_test_read_all_value();
	pin_value = d&(1<<18);

	//set gpio direction to output
    gpio_set_dir_out(gpio2100, 18); // GPIO 18 - Relay switch

	if(pin_value != 0)
	{
		d &= ~(1<<18);
	}
	else
	{
		d |= (1<<18);
	}
	
	//set gpio value
    gpio_write_int(gpio2100, d);

	#if 0	
	if (signum == SIGUSR1) {
        printf("reboot..\n");
	} else if (signum == SIGUSR2) {
		printf("load default and reboot..\n");
	}
	#endif
}

/*
 * init gpio interrupt -
 *   1. config gpio interrupt mode
 *   2. register my pid and request gpio pin 0
 *   3. issue a handler to handle SIGUSR1 and SIGUSR2
 */
int initGpio(char *irq)
{
	int fd;
	ralink_gpio_reg_info info;

	info.pid = getpid();
    if(irq)
        info.irq = atoi(irq);
    else
        info.irq = RALINK_GPIO_IRQ;

	fd = open(GPIO_DEV, O_RDONLY);
	if (fd < 0) {
		perror(GPIO_DEV);
		return -1;
	}
	if (info.irq < 24) {
		if (ioctl(fd, RALINK_GPIO_SET_DIR_IN, (1<<info.irq)) < 0)
			goto ioctl_err;
	}

	//enable gpio interrupt
	if (ioctl(fd, RALINK_GPIO_ENABLE_INTP) < 0)
		goto ioctl_err;

	//register my information
	if (ioctl(fd, RALINK_GPIO_REG_IRQ, &info) < 0)
		goto ioctl_err;
	close(fd);

	//issue a handler to handle SIGUSR1 and SIGUSR2
	signal(SIGUSR1, IrqHandler);
	signal(SIGUSR2, IrqHandler);
	return 0;

ioctl_err:
	perror("ioctl");
	close(fd);
	return -1;
}

static void pidfile_delete(void)
{
	if (saved_pidfile) unlink(saved_pidfile);
}

int pidfile_acquire(const char *pidfile)
{
	int pid_fd;
	if (!pidfile) return -1;

	pid_fd = open(pidfile, O_CREAT | O_WRONLY, 0644);
	if (pid_fd < 0) {
		printf("Unable to open pidfile %s: %m\n", pidfile);
	} else {
		lockf(pid_fd, F_LOCK, 0);
		if (!saved_pidfile)
			atexit(pidfile_delete);
		saved_pidfile = (char *) pidfile;
	}
	return pid_fd;
}

void pidfile_write_release(int pid_fd)
{
	FILE *out;

	if (pid_fd < 0) return;

	if ((out = fdopen(pid_fd, "w")) != NULL) {
		fprintf(out, "%d\n", getpid());
		fclose(out);
	}
	lockf(pid_fd, F_UNLCK, 0);
	close(pid_fd);
}

int main(int argc,char **argv)
{
	pid_t pid;
	int fd;

    if(argc == 2) {
		gpio_pin = atoi(argv[1]);
        if (initGpio(argv[1]) != 0)
            exit(EXIT_FAILURE);
    }
    else {
		gpio_pin = 20;
        if (initGpio(TO_STR(RALINK_GPIO_IRQ)) != 0)
            exit(EXIT_FAILURE);
    }

	fd = pidfile_acquire("/var/run/relayControlSwitch.pid");
	pidfile_write_release(fd);

	while (1) {
		pause();
	}
	exit(EXIT_SUCCESS);
}

