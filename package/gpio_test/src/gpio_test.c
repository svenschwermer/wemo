#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>

#define GPIO_DEV                    "/dev/gpio"
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

int gpio_set_dir_in_all(int r)
{
    int fd, req;

    fd = open(GPIO_DEV, O_RDONLY);
    if (fd < 0) {
        perror(GPIO_DEV);
        return -1;
    }
    req = RALINK_GPIO_SET_DIR_IN;
    if (ioctl(fd, req, 0xffffffff) < 0) {
        perror("ioctl");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

int gpio_set_dir_in(int r, int pinNO)
{
    int fd, req;

    fd = open(GPIO_DEV, O_RDONLY);
    if (fd < 0) {
        perror(GPIO_DEV);
        return -1;
    }
    req = RALINK_GPIO_SET_DIR_IN;
    if (ioctl(fd, req, 1<<pinNO) < 0) {
        perror("ioctl");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
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

int gpiomode_write_int(int r, int value)
{
    int fd, req;

    fd = open(GPIO_DEV, O_RDWR);
    if (fd < 0) {
        perror(GPIO_DEV);
        return -1;
    }
    req = RALINK_GPIO_SET_MODE;

    if (ioctl(fd, req, value) < 0) {
        perror("ioctl");
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

void gpio_test_write(int gpioVal, int pinNO)
{
    char output[50];
    int i = 0;
    int len = 0;

    memset(output, 0, sizeof(output));
    itoa(gpioVal, output, 2);
    printf("\ngpio write:\n");
    printf("HEX: 0x%08X\n", gpioVal);
    printf("BIT: ");
    for(i = 21; i>=0; i--)
        printf("%02d ", i);
    printf("\n");
    printf("BIN:");
    len = strlen(output);
    for(i = 21; i>=0; i--) {
        if(i >= len)
            printf("  0");
        else
            printf("  %c", output[len-i-1]);
    }
    printf("\n");

    //set gpio direction to output
    gpio_set_dir_out(gpio2100, pinNO);

    //set gpio value
    gpio_write_int(gpio2100, gpioVal);
}

void gpio_test_set(int gpioMode, int pinNO)
{
    char output[50];
    int i = 0;
    int len = 0;

    memset(output, 0, sizeof(output));
    itoa(gpioMode, output, 2);
    printf("\ngpio write:\n");
    printf("HEX: 0x%08X\n", gpioMode);
    printf("BIT: ");
    for(i = 21; i>=0; i--)
        printf("%02d ", i);
    printf("\n");
    printf("BIN:");
    len = strlen(output);
    for(i = 21; i>=0; i--) {
        if(i >= len)
            printf("  0");
        else
            printf("  %c", output[len-i-1]);
    }
    printf("\n");

    //set gpio mode
    gpiomode_write_int(gpio2100, gpioMode);
}

void usage(char *cmd)
{
    printf("Usage: %s r                         - read current GPIO value\n", cmd);
    printf("       %s w <PIN number> <value>    - write GPIO value to PIN(output)\n", cmd);
    printf("       %s g                         - get current GPIO mode\n", cmd);
    printf("       %s s <name> <value>          - set value to offset(mode)\n", cmd);
    printf("\n       <name> must be jtag/uartl/spi/i2c\n");
    exit(0);
}

unsigned long gpio_test_read_value(void)
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

unsigned long gpio_test_get_mode(void)
{
    int i, fd, req, len;

    char output[50];
    unsigned long value = 0;
    fd = open(GPIO_DEV, O_RDONLY);
    if (fd < 0) {
        perror(GPIO_DEV);
        return;
    }

    req = RALINK_GPIO_GET_MODE;

    if (ioctl(fd, req, &value) < 0) {
        perror("ioctl");
        close(fd);
        return;
    }
    close(fd);

    printf("\nGPIO mode 21~00:\n");
    printf("HEX: 0x%08X\n", value);
    printf("BIT: ");
    for(i = 21; i>=0; i--)
        printf("%02d ", i);
    printf("\n");

    memset(output, 0, sizeof(output));
    itoa(value, output, 2);
    printf("BIN:");
    len = strlen(output);
    for(i = 21; i>=0; i--) {
        if(i >= len)
            printf("  0");
        else
            printf("  %c", output[len-i-1]);
    }
    printf("\n");

    return value;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
        usage(argv[0]);

    if( !strcmp(argv[1], "r")) {
        if (argc != 2)
            usage(argv[0]);
        else {
            gpio_test_read_value();
        }
    } else if( !strcmp(argv[1], "w")) {
        if (argc != 4)
            usage(argv[0]);
        else {
            unsigned long gpioVal = (unsigned long)gpio_test_read_value();
            int pinNO = atoi(argv[2]);
            if(argv[3][0] == '0')
                gpioVal&=~(1<<pinNO);
            else if(argv[3][0] == '1')
                gpioVal|=(1<<pinNO);
            gpio_test_write(gpioVal, pinNO);
        }
    } else if( !strcmp(argv[1], "g")) {
        if (argc != 2)
            usage(argv[0]);
        else {
            gpio_test_get_mode();
        }
    } else if( !strcmp(argv[1], "s")) {
        if (argc != 4)
            usage(argv[0]);
        else {
            unsigned long gpioMode = (unsigned long)gpio_test_get_mode();
            int pinNO = 0;
            if(!strcmp(argv[2], "jtag"))
                pinNO = 6;
            else if(!strcmp(argv[2], "uartl"))
                pinNO = 5;
            else if (!strcmp(argv[2], "spi"))
                pinNO = 1;
            else if (!strcmp(argv[2], "i2c"))
                pinNO = 0;
            else {
                printf("Usage: %s s <name> <value> - set value to offset(mode)\n", argv[0]);
                printf("\n       <name> must be jtag/uartl/spi/i2c\n");
                return 1;
            }
            if(argv[3][0] == '0')
                gpioMode&=~(1<<pinNO);
            else if(argv[3][0] == '1')
                gpioMode|=(1<<pinNO);
            gpio_test_set(gpioMode, pinNO);
        }
    } else {
        usage(argv[0]);
    }
    return 0;
}

