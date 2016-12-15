#define SUPPORT_RESET_BUTTON		1
#define SUPPORT_WPS_BUTTON      1
#define SUPPORT_WPS_LED         1
#define SUPPORT_GEMTEK_MP_TEST  1
#define GPIO_STAT_OUT_LOW       0
#define GPIO_STAT_OUT_HIGH      1
#define GPIO_STAT_IN_LOW        2
#define GPIO_STAT_IN_HIGH       3

/*
 * Override ralink_gpio.h defaults
*/
#undef  RALINK_GPIOMODE_EPHY_BT
#define RALINK_GPIOMODE_EPHY_BT		0x4000  //SW_PHY_LED (Big and Small LED)

#undef  RALINK_GPIOMODE_SPI_CS1
#define RALINK_GPIOMODE_SPI_CS1		0x600000

#undef  RALINK_GPIOMODE_DFT
#define RALINK_GPIOMODE_DFT		(RALINK_GPIOMODE_EPHY_BT) | (RALINK_GPIOMODE_UARTF)


/*First GPIO Register GPIO0~GPIO21 can be used*/
#define PORT0				0x00000001	
#define PORT1				0x00000002
#define PORT2				0x00000004
#define PORT3				0x00000008
#define PORT4				0x00000010	
#define PORT5				0x00000020	
#define PORT6				0x00000040	
#define PORT7				0x00000080 //Watch Dog Input
#define PORT8				0x00000100	
#define PORT9				0x00000200	
#define PORT10			0x00000400 //SW Reset	
#define PORT11			0x00000800 //Watch Dog Enable	
#define PORT12			0x00001000	
#define PORT13			0x00002000 //WPS PBC	
#define PORT14			0x00004000	
#define PORT15			0x00008000
#define PORT16			0x00010000
#define PORT17			0x00020000	
#define PORT18			0x00040000	
#define PORT19			0x00080000
#define PORT20			0x00100000	
#define PORT21			0x00200000

/*Second GPIO Register GPI22~GPIO27 can be used*/	
#define PORT22			0x00000001 //Small Green LED
#define PORT23 	    0x00000002
#define PORT24      0x00000004 //Big Amber LED
#define PORT25      0x00000008 //Small Amber LED
#define PORT26      0x00000010 //Big Green LED 
#define PORT27      0x00000020

#define WATCH_DOG_INPUT  PORT7
#define WATCH_DOG_ENABLE PORT11
#define RESET_BUTTON     PORT10
#define WPS_BUTTON       PORT13

#define SMALL_GREEN_LED  PORT22
#define BIG_AMBER_LED    PORT24
#define SMALL_AMBER_LED  PORT25
#define BIG_GREEN_LED    PORT26


//#define WIFI_AMBER_LED	PORT7
//#define WIFI_BLUE_LED	PORT11
#define WIFI_AMBER_LED	PORT11
#define WIFI_BLUE_LED	PORT7
#define POWER_BLUE_LED  PORT9
#define POWER_BUTTON    PORT12
#define RELAY_CONTROL	PORT13
//#define RESET_BUTTON    PORT10
#define MOTION_SENSOR	PORT14



#define RESET_TIME			 10
#define LED_ON            0
#define LED_OFF           1

#define WatchD_ON         1
#define WatchD_OFF        0

#define GMTK_CLOSE_SPEED_METER 0
#define GMTK_SYSTEM_READY 1
#define GMTK_SYSTEM_BOOT 2
#define GMTK_SPEED_METER_BOOT_UP_READY 3
#define GMTK_SPEED_METER_BOOT_UP 4
#define GMTK_WL_SEC_DISABLED 5
#define GMTK_WL_SEC_ENABLED 6
#define GMTK_LAN_LINK_DOWN 7
#define GMTK_LAN_LINK_UP 8
#define GMTK_LAN_LINK_ERROR 9
#define GMTK_WAN_MODEM_DOWN 10
#define GMTK_WAN_MODEM_UP 11
#define GMTK_WAN_MODEM_ERROR 12
#define GMTK_WAN_INTERNET_DOWN 13
#define GMTK_WAN_INTERNET_CONNECTING 14
#define GMTK_WAN_INTERNET_CONNECTED 15
#define GMTK_WAN_INTERNET_ERROR 16

__inline__ void wemo_gpio_irq_handler(void){}
void __init wemo_gpio_init(void);
void __exit wemo_gpio_exit(void);
int GPIO_read_bit(u32 port,u32 *get_value);
int GPIO_write_bit(u32 port,u32 set_value);
int wemo_gpio_ioctl(unsigned int req,unsigned long arg);

