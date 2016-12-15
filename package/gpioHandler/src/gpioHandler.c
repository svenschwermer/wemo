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
#ifdef PRODUCT_WeMo_LEDLight
#define RALINK_GPIO_IRQ             0
#else
#define RALINK_GPIO_IRQ             17//0
#endif
#define TO_STR(arg)                 #arg

typedef struct {
    unsigned int irq;   //request irq pin number
    pid_t pid;          //process id to notify
} ralink_gpio_reg_info;

static char *saved_pidfile;

void loadDefault(int chip_id)
{
    switch(chip_id)
    {
        case 5350:
            system("killall dropbear; sleep 1; mtd -r erase rootfs_data");
            break;
        default:
            printf("%s:Wrong chip id\n",__FUNCTION__);
            break;
    }
}

/*
 * gpio interrupt handler -
 *   SIGUSR1 - reboot
 *   SIGUSR2 - restore default
 */
static void IrqHandler(int signum)
{
	if (signum == SIGUSR1) {
        printf("reboot..\n");
		system("reboot");
	} else if (signum == SIGUSR2) {
		printf("load default and reboot..\n");
		loadDefault(5350);
	}
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
        if (initGpio(argv[1]) != 0)
            exit(EXIT_FAILURE);
    }
    else {
        if (initGpio(TO_STR(RALINK_GPIO_IRQ)) != 0)
            exit(EXIT_FAILURE);
    }

	fd = pidfile_acquire("/var/run/gpioHandler.pid");
	pidfile_write_release(fd);

	while (1) {
		pause();
	}
	exit(EXIT_SUCCESS);
}

