/*
 ***************************************************************************
 * (c) Copyright, Belkin International
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 ***************************************************************************
 *
 */

#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/module.h>

#include "ralink_gpio.h"
#include "sns.h"

// sh: define POWER_DBL_CLICK to create a /proc/POWER_BUTTON_STATE
// to provide double click support for the power button
// 
// /proc/POWER_BUTTON_STATE:
// 0: button not pressed
// 1: button was clicked once
// 2: button was double clicked
// 3: button was held for a "long" time.

// #define POWER_DBL_CLICK	1

#if 0
#define DEBUGP(format, args...) printk(KERN_EMERG "%s:%s: " format, __FILE__, __FUNCTION__, ## args)
#else
#define DEBUGP(x, args...)
#endif

extern ralink_gpio_led_info ralink_gpio_led_data[];
extern ralink_gpio_reg_info ralink_gpio_info[];

int motion_sensor_status = 0;
int motion_sensor_delay = 0;
int motion_sensor_sensitivity = 0;
int motion_count = 0;
int motion_delay_timer_count = 0;

#ifdef POWER_DBL_CLICK
#include <linux/proc_fs.h>
static void LogBadState(void);
static void LogInvalidState(void);
static void WatchPowerButton(unsigned long unused);
static int PowerButtonStateReadProc(char *buf,char **start,off_t off,int count,int *eof,void *data);

struct {
	const char *ProcName;
	read_proc_t *ReadProc;
	write_proc_t *WriteProc;
} WeMoProcs[] = {
	{"POWER_BUTTON_STATE",PowerButtonStateReadProc,(write_proc_t *) NULL},
	{NULL}
};

typedef enum {
	IDLE = 0,	// Nothing happening yet

	CLICKED,		// Button released for > MaxClickDelay in CLICKED_W state

	DBL_CLICKED,// Button pressed for > MinClickJiffies & < MaxClickJiffies
					// in PRESSED_2 state
	HELD,			// Button has been pressed for > MaxClickTime

	PRESSED_1,	// Button just pressed the first time

	CLICKED_W,	// Button pressed for > MinClickJiffies & < MaxClickJiffies
					// waiting for MaxClickDelay before declaring single click

	PRESSED_2,	// Button pressed the second time

} BUTTON_STATE;

BUTTON_STATE PowerButtonState = IDLE;
// Reported to user:
//		0 - button inactive
//		1 - button clicked
//		2 - button double clicked
//		3 - button held a "long" time
int ReportedButtonState = IDLE;

// The default double click time in windows is 1/2 second, i.e.
// All times are in jiffies
static unsigned int MinClickTime = 2;	// debounce, clicks less than this are ignored
static unsigned int MaxClickTime = HZ;	// button held longer than this is being HELD
static unsigned int DoubleClickTime = HZ / 2;
static unsigned int ClickClearDelay = HZ;	// length of time to report key status

static int PowerButtonLast = 1;
static u64 ButtonReportedTime = 0;	// time ReportedButtonState last changed
static u64 LastChangeTime = 0;		// time button or PowerButtonState last changed
static u64 ClickStarted = 0;

static struct timer_list PowerButtonTimer;
#endif

static int __init LED_init(void);

/* Gemtek GPIO start*/


#define Reg1_GPIO_Start 0
#define Reg1_GPIO_End   21
#define Reg2_GPIO_Start 22
#define Reg2_GPIO_End   27

#define GPIO_TOTAL 28 

#define GPIO_TEXT_LEN 10

struct GPIO_data_d {
	char GPIO_NAME[GPIO_TEXT_LEN];
	u32 GPIO_PIN_NUM;
}GPIO_data[GPIO_TOTAL];

int GPIO_write_bit_22_27(u32 port,u32 set_value);
int GPIO_read_bit_22_27(u32 port,u32 *get_value);

unsigned char GPIO_LIST_NAME[GPIO_TOTAL][GPIO_TEXT_LEN] =
{
	"GPIO0" ,
	"GPIO1" ,
	"GPIO2" ,
	"GPIO3" ,
	"GPIO4" ,
	"GPIO5" ,
	"GPIO6" ,
	"GPIO7" ,
	"GPIO8" ,
	"GPIO9" ,
	"GPIO10",
	"GPIO11",
	"GPIO12",
	"GPIO13",
	"GPIO14",
	"GPIO15",
	"GPIO16",
	"GPIO17",
	"GPIO18",
	"GPIO19",
	"GPIO20",
	"GPIO21",
	"GPIO22",
	"GPIO23",
	"GPIO24",
	"GPIO25",
	"GPIO26",
	"GPIO27"
};

u32 GPIO_LIST_PIN[GPIO_TOTAL] =
{
	PORT0,
	PORT1,
	PORT2,
	PORT3,
	PORT4,
	PORT5,
	PORT6,
	PORT7,
	PORT8,
	PORT9,
	PORT10,
	PORT11,
	PORT12,
	PORT13,
	PORT14,
	PORT15,
	PORT16,
	PORT17,
	PORT18,
	PORT19,
	PORT20,
	PORT21,
	PORT22,
	PORT23,
	PORT24,
	PORT25,
	PORT26,
	PORT27
};

int GPIO_read_proc(char *buf, char **start, off_t off, int count, int *eof, void *data);
int GPIO_write_proc(struct file *file, const char __user *buffer,  unsigned long count, void *data);
int GPIO_read_proc_22_27(char *buf, char **start, off_t off, int count, int *eof, void *data);
int GPIO_write_proc_22_27(struct file *file, const char __user *buffer,  unsigned long count, void *data);

char btn_action[12]="", easy_link_btn_action[12] = "0";

 

struct proc_dir_entry *create_proc_read_write_entry(const char *name,mode_t mode, struct proc_dir_entry *base, read_proc_t *read_proc, write_proc_t *write_proc, void * data)
{
	struct proc_dir_entry *res=create_proc_entry(name,0x1FF,base);

	if (res) {
		res->read_proc=read_proc;
		res->write_proc=write_proc;
		res->data=data;
	}
	return res;
}

static int GPIO_init(void) 
{
	int i;
	
	DEBUGP("Gemtek_GPIO_init\n");	
	

	for(i = Reg1_GPIO_Start;i <= Reg1_GPIO_End; i++)
	{
		strcpy(GPIO_data[i].GPIO_NAME, GPIO_LIST_NAME[i]);
		GPIO_data[i].GPIO_PIN_NUM = GPIO_LIST_PIN[i];
		if(create_proc_read_write_entry(GPIO_data[i].GPIO_NAME,0,NULL,GPIO_read_proc,GPIO_write_proc,(void *)&GPIO_data[i]) == NULL)
		{
			DEBUGP("%s : fail\n", GPIO_data[i].GPIO_NAME);
		}
		else
		{
			DEBUGP("%s : success\n", GPIO_data[i].GPIO_NAME);			
		}
	}
	
	for(i = Reg2_GPIO_Start;i <= Reg2_GPIO_End; i++)
	{
		strcpy(GPIO_data[i].GPIO_NAME, GPIO_LIST_NAME[i]);
		GPIO_data[i].GPIO_PIN_NUM = GPIO_LIST_PIN[i];
		if(create_proc_read_write_entry(GPIO_data[i].GPIO_NAME,0,NULL,GPIO_read_proc_22_27,GPIO_write_proc_22_27,(void *)&GPIO_data[i]) == NULL)
		{
			DEBUGP("%s : fail\n", GPIO_data[i].GPIO_NAME);
		}
		else
		{
			DEBUGP("%s : success\n", GPIO_data[i].GPIO_NAME);			
		}
	}
		
	return 0;		
}


struct timer_list BIG_LED_GPIO;
struct timer_list PLUGIN_LED_GPIO;
struct timer_list MOTION_SENSOR_GPIO;
int plugin_gpio_led = LED_OFF ;
struct timer_list SMALL_LED_GPIO;
struct timer_list NET_GPIO;
int ledcount=0;
int net_gpio_led = LED_OFF ;
int big_gpio_led = LED_OFF ;
int small_gpio_led = LED_OFF ;

void BIG_LED_GREEN_BLINK(unsigned long data){

	if(big_gpio_led == LED_OFF){
		GPIO_write_bit_22_27(BIG_GREEN_LED,LED_ON);	
		big_gpio_led = LED_ON;
	}
	else{
		GPIO_write_bit_22_27(BIG_GREEN_LED,LED_OFF);	
		big_gpio_led = LED_OFF;
	}

		BIG_LED_GPIO.expires = jiffies + (1 * (HZ/4));
		add_timer(&BIG_LED_GPIO);		
		return ;
}
void BIG_LED_AMBER_BLINK(unsigned long data){

	if(big_gpio_led == LED_OFF){
		GPIO_write_bit_22_27(BIG_AMBER_LED,LED_ON);	
		big_gpio_led = LED_ON;
	}
	else{
		GPIO_write_bit_22_27(BIG_AMBER_LED,LED_OFF);	
		big_gpio_led = LED_OFF;
	}

		BIG_LED_GPIO.expires = jiffies + (1 * (HZ/4));
		add_timer(&BIG_LED_GPIO);		
		return ;
}

void WIFI_LED_BLUE_BLINK(unsigned long data){

	if(plugin_gpio_led == LED_OFF){
		GPIO_write_bit(WIFI_BLUE_LED, LED_ON);	
		plugin_gpio_led = LED_ON;
	}
	else{
		GPIO_write_bit(WIFI_BLUE_LED, LED_OFF);	
		plugin_gpio_led = LED_OFF;
	}

		PLUGIN_LED_GPIO.expires = jiffies + (7 * (HZ/10));
		add_timer(&PLUGIN_LED_GPIO);		
		return ;
}
int last_status=0;
void Detect_Motion_Scan(unsigned long data){

	u32 ret = 0;
	GPIO_read_bit(MOTION_SENSOR, &ret);
	//printk("Motion=[%X]\n", ret);
	
	if(motion_sensor_delay <= 0)
	{
		motion_sensor_status = ret; // No delay, give PIN status directly
		printk("motion_sensor_status=[%d], Motion=[%d]\n", motion_sensor_status, ret);
	}
	else
	{// Delay Enabled
//		printk("motion_sensor_status=[%d], Motion=[%d]\n", motion_sensor_status, ret);
		motion_delay_timer_count++;
		if(ret == 1)
			motion_count++;
			
		if(motion_delay_timer_count >= 2*motion_sensor_delay) // 500ms per detect, so *2
		{
			motion_delay_timer_count = 0; // Reset timer
//			printk(KERN_EMERG "motion_count=[%d], motion_sensor_delay=[%d], percent=[%d/%d], motion_sensor_sensitivity=[%d]\n", motion_count, motion_sensor_delay, motion_count, 2*motion_sensor_delay, motion_sensor_sensitivity);
			if( (motion_count*100) >= (motion_sensor_sensitivity*2*motion_sensor_delay) )
			{
				motion_sensor_status = 1;
				if(0==last_status)
					printk(KERN_EMERG "----------------- MOTION -----------------\n");
				last_status = 1;
			}
			else
			{
				motion_sensor_status = 0;
				if(1==last_status)
					printk(KERN_EMERG "----------------- NO MOTION -----------------\n");
				last_status = 0;
			}
			
			motion_count = 0; // Reset Motion Count
		}
	}

	
	/* Do Scan every 500 ms */
	MOTION_SENSOR_GPIO.expires = jiffies + (1 * (HZ/2));
	MOTION_SENSOR_GPIO.function = Detect_Motion_Scan;	
	add_timer(&MOTION_SENSOR_GPIO);		
	
	return ;
}

void WIFI_LED_BLUE_OFF(unsigned long data){

//	if(plugin_gpio_led == LED_OFF){
//		GPIO_write_bit(WIFI_BLUE_LED, LED_ON);	
//		plugin_gpio_led = LED_ON;
//	}
//	else{
		GPIO_write_bit(WIFI_BLUE_LED, LED_OFF);	
//		plugin_gpio_led = LED_OFF;
//	}

//		PLUGIN_LED_GPIO.expires = jiffies + (7 * (HZ/10));
//		add_timer(&PLUGIN_LED_GPIO);		
		return ;
}

void WIFI_LED_AMBER_BLINK(unsigned long data){

	if(plugin_gpio_led == LED_OFF){
		GPIO_write_bit(WIFI_AMBER_LED, LED_ON);	
		plugin_gpio_led = LED_ON;
	}
	else{
		GPIO_write_bit(WIFI_AMBER_LED, LED_OFF);	
		plugin_gpio_led = LED_OFF;
	}

		PLUGIN_LED_GPIO.expires = jiffies + (7 * (HZ/10));
		add_timer(&PLUGIN_LED_GPIO);		
		return ;
}

void WIFI_LED_AMBER_BLINK_300(unsigned long data){

	if(plugin_gpio_led == LED_OFF){
		GPIO_write_bit(WIFI_AMBER_LED, LED_ON);	
		plugin_gpio_led = LED_ON;
	}
	else{
		GPIO_write_bit(WIFI_AMBER_LED, LED_OFF);	
		plugin_gpio_led = LED_OFF;
	}

		PLUGIN_LED_GPIO.expires = jiffies + (3 * (HZ/10));
		add_timer(&PLUGIN_LED_GPIO);		
		return ;
}

void WIFI_LED_CHANGE_BLINK(unsigned long data){

	if(plugin_gpio_led == LED_OFF){
		GPIO_write_bit(WIFI_AMBER_LED, LED_OFF);
		GPIO_write_bit(WIFI_BLUE_LED, LED_ON);		
		plugin_gpio_led = LED_ON;
	}
	else{
		GPIO_write_bit(WIFI_BLUE_LED, LED_OFF);
		GPIO_write_bit(WIFI_AMBER_LED, LED_ON);		
		plugin_gpio_led = LED_OFF;
	}

		PLUGIN_LED_GPIO.expires = jiffies + (7 * (HZ/10));
		add_timer(&PLUGIN_LED_GPIO);		
		return ;
}

void SMALL_LED_GREEN_BLINK(unsigned long data){

	if(small_gpio_led == LED_OFF){
		GPIO_write_bit_22_27(SMALL_GREEN_LED,LED_ON);	
		small_gpio_led = LED_ON;
	}
	else{
		GPIO_write_bit_22_27(SMALL_GREEN_LED,LED_OFF);	
		small_gpio_led = LED_OFF;
	}

		SMALL_LED_GPIO.expires = jiffies + (1 * (HZ/4));
		add_timer(&SMALL_LED_GPIO);		
		return ;
}
void SMALL_LED_AMBER_BLINK(unsigned long data){

	if(small_gpio_led == LED_OFF){
		GPIO_write_bit_22_27(SMALL_AMBER_LED  ,LED_ON);	
		small_gpio_led = LED_ON;
	}
	else{
		GPIO_write_bit_22_27(SMALL_AMBER_LED  ,LED_OFF);	
		small_gpio_led = LED_OFF;
	}

		SMALL_LED_GPIO.expires = jiffies + (1 * (HZ/4));
		add_timer(&SMALL_LED_GPIO);		
		return ;
}

/*
big_led_set

0	Big LED Green Solid
1	Big LED Green Blink
2	Big LED Amber Solid
3	Big LED Amber Blink
4	No LED
*/

void big_led_set(int ledcount)	{
return;
	
	del_timer_sync(&BIG_LED_GPIO);
	
	if(ledcount==0)
	{
		GPIO_write_bit_22_27(BIG_GREEN_LED,LED_ON);
		GPIO_write_bit_22_27(BIG_AMBER_LED,LED_OFF);				
	}	
	if(ledcount==1)
	{
		GPIO_write_bit_22_27(BIG_GREEN_LED,LED_OFF);
		GPIO_write_bit_22_27(BIG_AMBER_LED,LED_OFF);	
		
		big_gpio_led = LED_OFF ;

		BIG_LED_GPIO.expires = jiffies + (1 * (HZ/2));
		BIG_LED_GPIO.function = BIG_LED_GREEN_BLINK;		
		add_timer(&BIG_LED_GPIO);

	}
	else if(ledcount==2) 
	{
		GPIO_write_bit_22_27(BIG_GREEN_LED,LED_OFF);
		GPIO_write_bit_22_27(BIG_AMBER_LED,LED_ON);	
	}
	else if(ledcount==3) 
	{
		GPIO_write_bit_22_27(BIG_GREEN_LED,LED_OFF);
		GPIO_write_bit_22_27(BIG_AMBER_LED,LED_OFF);	
				
		big_gpio_led = LED_OFF ;

		BIG_LED_GPIO.expires = jiffies + (1 * (HZ/2));
		BIG_LED_GPIO.function = BIG_LED_AMBER_BLINK;		
		add_timer(&BIG_LED_GPIO);
	}
	else if(ledcount==4) 
	{
		GPIO_write_bit_22_27(BIG_GREEN_LED,LED_OFF);
		GPIO_write_bit_22_27(BIG_AMBER_LED,LED_OFF);	
	}
	else if(ledcount==5){ //For Easytest
		GPIO_write_bit_22_27(BIG_GREEN_LED,LED_ON);
		GPIO_write_bit_22_27(BIG_AMBER_LED,LED_ON);
	}
}

/*
plugin_led_set

0	LED Blue Blink, 700 ms ON, 700 ms OFF
1	LED Blue 30s ON, then OFF
2	LED Amber Solid ON
3	LED Amber Blink, 700 ms ON, 700 ms OFF
4	No LED
5	700 ms LED Blue, 700 ms LED Amber
*/

void plugin_led_set(int ledcount)
{

	
	del_timer_sync(&PLUGIN_LED_GPIO);
	
	if(ledcount==0)
	{
		GPIO_write_bit(WIFI_AMBER_LED, LED_OFF);
		GPIO_write_bit(WIFI_BLUE_LED, LED_OFF);	
		
		plugin_gpio_led = LED_OFF ;

		PLUGIN_LED_GPIO.expires = jiffies + (1 * (HZ/2));
		PLUGIN_LED_GPIO.function = WIFI_LED_BLUE_BLINK;		
		add_timer(&PLUGIN_LED_GPIO);

	}
	else if(ledcount==1) 
	{
		GPIO_write_bit(WIFI_AMBER_LED, LED_OFF);
		GPIO_write_bit(WIFI_BLUE_LED, LED_OFF);	
		
		plugin_gpio_led = LED_OFF ;
		
		GPIO_write_bit(WIFI_BLUE_LED, LED_ON);	
		PLUGIN_LED_GPIO.expires = jiffies + (30 * HZ);
		PLUGIN_LED_GPIO.function = WIFI_LED_BLUE_OFF;		
		add_timer(&PLUGIN_LED_GPIO);	
	}
	else if(ledcount==2) 
	{
		GPIO_write_bit(WIFI_BLUE_LED, LED_OFF);
		GPIO_write_bit(WIFI_AMBER_LED, LED_ON);
	}
	else if(ledcount==3) 
	{
		GPIO_write_bit(WIFI_AMBER_LED, LED_OFF);
		GPIO_write_bit(WIFI_BLUE_LED, LED_OFF);	
		
		plugin_gpio_led = LED_OFF ;

		PLUGIN_LED_GPIO.expires = jiffies + (1 * (HZ/2));
		PLUGIN_LED_GPIO.function = WIFI_LED_AMBER_BLINK;		
		add_timer(&PLUGIN_LED_GPIO);
	}
	else if(ledcount==4)
	{
		GPIO_write_bit(WIFI_BLUE_LED, LED_OFF);
		GPIO_write_bit(WIFI_AMBER_LED, LED_OFF);
	}
	else if(ledcount==5) 
	{
		GPIO_write_bit(WIFI_AMBER_LED, LED_OFF);
		GPIO_write_bit(WIFI_BLUE_LED, LED_OFF);	
		
		plugin_gpio_led = LED_OFF ;

		PLUGIN_LED_GPIO.expires = jiffies + (1 * (HZ/2));
		PLUGIN_LED_GPIO.function = WIFI_LED_CHANGE_BLINK;		
		add_timer(&PLUGIN_LED_GPIO);
	}
	else if(ledcount==6) 
	{
		GPIO_write_bit(WIFI_AMBER_LED, LED_OFF);
		GPIO_write_bit(WIFI_BLUE_LED, LED_OFF);	
		
		plugin_gpio_led = LED_OFF ;

		PLUGIN_LED_GPIO.expires = jiffies + (1 * (HZ/2));
		PLUGIN_LED_GPIO.function = WIFI_LED_AMBER_BLINK_300;		
		add_timer(&PLUGIN_LED_GPIO);
	}
	else if(ledcount==7)
        {
                GPIO_write_bit(WIFI_BLUE_LED, LED_ON);
                GPIO_write_bit(WIFI_AMBER_LED, LED_ON);
        }
	
}

/*
small_led_set

0	Small LED Blue Solid
1	Small LED Blue Blink
2	Small LED Amber Solid
3	Small LED Amber Blink
4	No LED
*/

void small_led_set(int ledcount)	{
return;	
	del_timer_sync(&SMALL_LED_GPIO);
	
	if(ledcount==0)
	{
		GPIO_write_bit_22_27(SMALL_GREEN_LED,LED_ON);
		GPIO_write_bit_22_27(SMALL_AMBER_LED,LED_OFF);				
	}	
	if(ledcount==1)
	{
		GPIO_write_bit_22_27(SMALL_GREEN_LED,LED_OFF);
		GPIO_write_bit_22_27(SMALL_AMBER_LED,LED_OFF);	
		
		small_gpio_led = LED_OFF ;

		SMALL_LED_GPIO.expires = jiffies + (1 * (HZ/2));
		SMALL_LED_GPIO.function = SMALL_LED_GREEN_BLINK;		
		add_timer(&SMALL_LED_GPIO);

	}
	else if(ledcount==2) 
	{
		GPIO_write_bit_22_27(SMALL_GREEN_LED,LED_OFF);
		GPIO_write_bit_22_27(SMALL_AMBER_LED,LED_ON);	
	}
	else if(ledcount==3) 
	{
		GPIO_write_bit_22_27(SMALL_GREEN_LED,LED_OFF);
		GPIO_write_bit_22_27(SMALL_AMBER_LED,LED_OFF);	
				
		small_gpio_led = LED_OFF ;

		SMALL_LED_GPIO.expires = jiffies + (1 * (HZ/2));
		SMALL_LED_GPIO.function = SMALL_LED_AMBER_BLINK;		
		add_timer(&SMALL_LED_GPIO);
	}
	else if(ledcount==4) 
	{
		GPIO_write_bit_22_27(SMALL_GREEN_LED,LED_OFF);
		GPIO_write_bit_22_27(SMALL_AMBER_LED,LED_OFF);	
	}
	else if(ledcount==5){
		GPIO_write_bit_22_27(SMALL_GREEN_LED,LED_ON);
		GPIO_write_bit_22_27(SMALL_AMBER_LED,LED_ON);
	}
}

int BIG_LED_GPIO_proc(struct file *file, const char __user *buffer,  unsigned long count, void *data)
{
	int len = 0;
	char buf[20]={0};

	len = count;

	if(copy_from_user(buf, buffer, len))
	{
		DEBUGP("fail.\n");		
		return -EFAULT;
	}
	sscanf(buf,"%d",&ledcount);
	DEBUGP("BIG_LED_GPIO_proc ledcount=%d\n",ledcount);
	
	big_led_set(ledcount);

	return len;
} 

int PLUGIN_LED_GPIO_proc(struct file *file, const char __user *buffer,  unsigned long count, void *data)
{
	int len = 0;
	char buf[20]={0};

	len = count;

	if(copy_from_user(buf, buffer, len))
	{
		DEBUGP("fail.\n");		
		return -EFAULT;
	}
	sscanf(buf,"%d",&ledcount);
	//DEBUGP("PLUGIN_LED_GPIO_proc ledcount=%d\n",ledcount);
	printk(KERN_EMERG "PLUGIN_LED_GPIO_proc ledcount=%d\n",ledcount);
	
	plugin_led_set(ledcount);

	return len;
} 

int MOTION_SENSOR_STATUS_read_proc(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;

	len = sprintf(buf, "%d", motion_sensor_status);
	
	return len;
} 

int MOTION_SENSOR_STATUS_write_proc(struct file *file, const char __user *buffer,  unsigned long count, void *data)
{
	int len = 0;
	int sensor_enable = 0;
	char buf[20]={0};

	len = count;

	if(copy_from_user(buf, buffer, len))
	{
		DEBUGP("fail.\n");		
		return -EFAULT;
	}
	sscanf(buf,"%d",&sensor_enable);	

	del_timer_sync(&MOTION_SENSOR_GPIO);

	motion_sensor_status = 0;
	
	if(sensor_enable==0)
	{
		printk(KERN_EMERG "MOTION_SENSOR_STATUS_write_proc: Stop Motion Detect!!\n");	
	}
	else if(sensor_enable==1)
	{
		printk(KERN_EMERG "MOTION_SENSOR_STATUS_write_proc: Start Motion Detect!!\n");	
		MOTION_SENSOR_GPIO.expires = jiffies + (1 * (HZ/2));
		MOTION_SENSOR_GPIO.function = Detect_Motion_Scan;		
		add_timer(&MOTION_SENSOR_GPIO);
	}
	else	printk(KERN_EMERG "MOTION_SENSOR_STATUS_write_proc: ERROR!!\n");

	return len;
} 

int MOTION_SENSOR_SET_DELAY_proc(struct file *file, const char __user *buffer,  unsigned long count, void *data)
{
	int len = 0;
	char buf[20]={0};

	len = count;

	if(copy_from_user(buf, buffer, len))
	{
		DEBUGP("fail.\n");		
		return -EFAULT;
	}
	sscanf(buf,"%d",&motion_sensor_delay);
	
	if(motion_sensor_delay < 0)
		printk(KERN_EMERG "MOTION_SENSOR_SET_DELAY_proc ERROR!! motion_sensor_delay=[%d]\n", motion_sensor_delay);
	else
		printk(KERN_EMERG "MOTION_SENSOR_SET_DELAY_proc SUCCESS!! motion_sensor_delay=[%d]\n",motion_sensor_delay);
return len;	

	del_timer_sync(&MOTION_SENSOR_GPIO);
	
	if(ledcount==0)
	{
		printk(KERN_EMERG "Do nothing :p\n");	
	}
	else if(ledcount==1)
	{
		MOTION_SENSOR_GPIO.expires = jiffies + (1 * (HZ/2));
		MOTION_SENSOR_GPIO.function = Detect_Motion_Scan;		
		add_timer(&MOTION_SENSOR_GPIO);
	}

	return len;
} 

int MOTION_SENSOR_SET_SENSITIVITY_proc(struct file *file, const char __user *buffer,  unsigned long count, void *data)
{
	int len = 0;
	char buf[20]={0};

	len = count;

	if(copy_from_user(buf, buffer, len))
	{
		DEBUGP("fail.\n");		
		return -EFAULT;
	}
	sscanf(buf,"%d",&motion_sensor_sensitivity);
	
	if(motion_sensor_sensitivity < 0 || motion_sensor_sensitivity > 100)
		printk(KERN_EMERG "MOTION_SENSOR_SET_DELAY_proc ERROR!! motion_sensor_sensitivity=[%d]\n", motion_sensor_sensitivity);
	else
		printk(KERN_EMERG "MOTION_SENSOR_SET_DELAY_proc SUCCESS!! motion_sensor_sensitivity=[%d]\n",motion_sensor_sensitivity);
return len;	
	
	del_timer_sync(&MOTION_SENSOR_GPIO);
	
	if(ledcount==0)
	{
		printk(KERN_EMERG "Do nothing :p\n");	
	}
	else if(ledcount==1)
	{
		MOTION_SENSOR_GPIO.expires = jiffies + (1 * (HZ/2));
		MOTION_SENSOR_GPIO.function = Detect_Motion_Scan;		
		add_timer(&MOTION_SENSOR_GPIO);
	}

	return len;
} 

int SMALL_LED_GPIO_proc(struct file *file, const char __user *buffer,  unsigned long count, void *data)
{
	int len = 0;
	char buf[20]={0};

	len = count;

	if(copy_from_user(buf, buffer, len))
	{
		DEBUGP("fail.\n");		
		return -EFAULT;
	}
	sscanf(buf,"%d",&ledcount);
	DEBUGP("SMALL_LED_GPIO_proc ledcount=%d\n",ledcount);
	
	small_led_set(ledcount);

	return len;
}



int GPIO_read_proc(char *buf, char **start, off_t off, int count,  int *eof, void *data)
{
	u32 ret = 0;
	struct GPIO_data_d *gpio = (struct GPIO_data_d *)data;
	GPIO_read_bit(gpio->GPIO_PIN_NUM, &ret );

	sprintf(buf, "%d\n", ret);
	return 1;
}

int GPIO_read_proc_22_27(char *buf, char **start, off_t off, int count,  int *eof, void *data)
{
	u32 ret = 0;
	struct GPIO_data_d *gpio = (struct GPIO_data_d *)data;
	GPIO_read_bit_22_27(gpio->GPIO_PIN_NUM, &ret );
	
	sprintf(buf, "%d\n", ret);
	return 1;
}

int GPIO_write_proc(struct file *file, const char __user *buffer,  unsigned long count, void *data)
{
	int len = 0;
	char buf[10];
	struct GPIO_data_d *gpio = (struct GPIO_data_d *)data;

	if(count >= 10)
	{
		len = 9;
	}
	else
	{
		len = count;
	}
	
	if(copy_from_user(buf, buffer, len))
	{		
		return -EFAULT;
	}	 
	if(buf[0]=='1')
		GPIO_write_bit(gpio->GPIO_PIN_NUM, 1);
	else
		GPIO_write_bit(gpio->GPIO_PIN_NUM, 0);

	return len;	
}

int GPIO_write_proc_22_27(struct file *file, const char __user *buffer,  unsigned long count, void *data)
{
	int len = 0;
	char buf[10];
	struct GPIO_data_d *gpio = (struct GPIO_data_d *)data;
	if(count >= 10)
	{
		len = 9;
	}
	else
	{
		len = count;
	}
	
	if(copy_from_user(buf, buffer, len))
	{		
		return -EFAULT;
	}	 
	if(buf[0]=='1')
		GPIO_write_bit_22_27(gpio->GPIO_PIN_NUM, 1);
	else
		GPIO_write_bit_22_27(gpio->GPIO_PIN_NUM, 0);

	return len;	
}  

int GPIO_setdir(u32 port,int dir_value)
{
		u32 value,tmp;
		
		tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));

		if(dir_value==RALINK_GPIO_DIR_OUT)
			value=tmp | port;
		else
			value=tmp & (0xffffffff^port);

		
		*(volatile u32 *)(RALINK_REG_PIODIR) = cpu_to_le32(value);
		return 1;
}

int GPIO_setdir_22_27(u32 port,int dir_value)
{
		u32 value,tmp;
		
		tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO5140DIR));

		if(dir_value==RALINK_GPIO_DIR_OUT)
			value=tmp | port;
		else
			value=tmp & (0xffffffff^port);

		
		*(volatile u32 *)(RALINK_REG_PIO5140DIR) = cpu_to_le32(value);
		return 1;
}

int GPIO_write_bit(u32 port,u32 set_value)
{	
		u32 value,tmp;

		GPIO_setdir(port,RALINK_GPIO_DIR_OUT);		
		tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODATA));

		if(set_value==1)
			value=tmp | port;
		else
			value=tmp & (0xffffffff^port);
		
		*(volatile u32 *)(RALINK_REG_PIODATA) = cpu_to_le32(value);	
		return 1;	
}

int GPIO_write_bit_22_27(u32 port,u32 set_value)
{	
		u32 value,tmp;

		GPIO_setdir_22_27(port,RALINK_GPIO_DIR_OUT);
		tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO5140DATA));
		if(set_value==1)
			value=tmp | port;
		else
			value=tmp & (0xffffffff^port);
		
		*(volatile u32 *)(RALINK_REG_PIO5140DATA) = cpu_to_le32(value);	
		return 1;	
}

int GPIO_read_bit(u32 port,u32 *get_value)
{
	u32 tmp;

		GPIO_setdir(port,RALINK_GPIO_DIR_IN);
		//read gpio port
		*get_value=0;
		tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODATA));
		*get_value=tmp & port;
		if(*get_value>0)
			*get_value=1;
		else
			*get_value=0;
		
		return 1;	
}
int GPIO_read_bit_22_27(u32 port,u32 *get_value)
{
	u32 tmp;

		GPIO_setdir_22_27(port,RALINK_GPIO_DIR_IN);
		//read gpio port	
		*get_value=0;
		tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO5140DATA));
		*get_value=tmp & port;
		if(*get_value>0)
			*get_value=1;
		else
			*get_value=0;
		
		return 1;	
}

/* Gemtek GPIO end*/


int set_GPIO_status(u32 pin_num, char value)
{
	if(value == GPIO_STAT_OUT_LOW)
	{
		GPIO_write_bit(pin_num,GPIO_STAT_OUT_LOW);
		//DEBUGP("set GPIO Line 0x%04x to OUT LOW.\n", pin_num);
	}
	else if(value == GPIO_STAT_OUT_HIGH)
	{
		GPIO_write_bit(pin_num,GPIO_STAT_OUT_HIGH);
		//DEBUGP("set GPIO Line 0x%04x to OUT HIGH.\n", pin_num);
	}
	else if(value == GPIO_STAT_IN_LOW)
	{		
		//DEBUGP("[Not implement]set GPIO Line 0x%04x to IN LOW.\n", pin_num);
	}
	else if(value == GPIO_STAT_IN_HIGH)
	{
		//DEBUGP("[Not implement]set GPIO Line 0x%04x to IN HIGH.\n", pin_num);
	}
	else
	{
		//DEBUGP("GPIO %d, Unknown value[0x%04x]...do nothing.\n", pin_num, value);
		return -1;
	}	
	
	return 0;
}


#if SUPPORT_WPS_BUTTON
struct timer_list EasyLink;
int	EasyLinkCount = 0;

/* Gemtek EasyLink button init */
static int easylink_btn_read_proc(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;

	len = sprintf(buf, easy_link_btn_action);

	sprintf(easy_link_btn_action, "0");
	
	return len;
}

void EASY_LINKScan(void)
{
	unsigned int ret = 0;
	
  GPIO_read_bit(WPS_BUTTON, &ret);
  
	if(!ret) //Button is pressed.
	{
		EasyLinkCount++;
		printk(KERN_EMERG "easylink_btn : easylink_btn is %d...press\n", EasyLinkCount);
		if(EasyLinkCount>=5)
		{ 
		    printk(KERN_EMERG "easylink_btn : easylink_btn is %d > 5...press\n", EasyLinkCount);
		    sprintf(easy_link_btn_action, "1");
		}
	}
	else //Button is released.
	{
		//printk(KERN_EMERG "easylink_btn : easylink_btn is %d..release\n", EasyLinkCount);
		if(EasyLinkCount > 0 && EasyLinkCount < RESET_TIME)
		{
			//printk(KERN_EMERG "easylink_btn : easylink_btn reboot\n");
			sprintf(easy_link_btn_action, "2");
		}
		EasyLinkCount = 0;
	}
	return;	
}

void EasyLinkCheck(unsigned long data)
{
	EASY_LINKScan();
	EasyLink.expires = jiffies + HZ;
	add_timer(&EasyLink);
	return;
}
/* Gemtek EasyLink button init End */
#endif

#if SUPPORT_WPS_LED	
struct timer_list WPSLED;

#define WPS_FAIL '0'
#define WPS_SUCCESS '1'
#define WPS_SUCCESS_NONE '2'
#define WPS_IN_PROGRESS '3'
#define WPS_SESSION_OVERLAP '4'
#define WPS_TIMEOUT '5'
#define WPS_TIMEOUT_NONE '6'
#define WPS_RESET '8'
#define WPS_RESET_NONE '9'
#define WPS_UNKNOWN 'x'
#define WPS_LED_STOP 0
#define WPS_LED_REPEAT 99999
#define WPS_LED_SOLID_BLUE -99999

#define WPS_LED_ARRAY_SIZE 100
int LED_array[WPS_LED_ARRAY_SIZE] = {0};
int LED_index = 0;

void print_LED_array(void)
{
	int i = 0;
	for(i = 0;i< WPS_LED_ARRAY_SIZE;i++)
	{
		DEBUGP("LED_array[%d] : %d\n", i, LED_array[i]);
	}
}

char WPS_LED_STATUS = WPS_UNKNOWN;

void WPS_check_LED(unsigned long data)
{
	int WPS_LED_PERIOD = 0;
	if(LED_index < WPS_LED_ARRAY_SIZE)
	{
		if(LED_array[LED_index] != WPS_LED_STOP)
		{
			if(LED_array[LED_index] == WPS_LED_REPEAT)
			{
				LED_index = 0;
			}

			if( (WPS_LED_STATUS == WPS_FAIL) || (WPS_LED_STATUS == WPS_TIMEOUT) || (WPS_LED_STATUS == WPS_TIMEOUT_NONE) || (WPS_LED_STATUS == WPS_SESSION_OVERLAP))
			{
				if(LED_array[LED_index] == WPS_LED_SOLID_BLUE)
				{
					WPS_LED_PERIOD = 1;	
				}
				else if(LED_array[LED_index] > 0)
				{
          small_led_set(2);																					
					WPS_LED_PERIOD = LED_array[LED_index] * 10;
				}
				else if(LED_array[LED_index] < 0)
				{
          small_led_set(4);																						
					WPS_LED_PERIOD = LED_array[LED_index] *  -10;
				}
			}
			else
			{
				if(LED_array[LED_index] == WPS_LED_SOLID_BLUE)
				{
					WPS_LED_PERIOD = 1;
				}				
				else if(LED_array[LED_index] > 0)
				{
          small_led_set(0);																							
					WPS_LED_PERIOD = LED_array[LED_index] * 10;
				}
				else if(LED_array[LED_index] < 0)
				{
          small_led_set(4);	
					WPS_LED_PERIOD = LED_array[LED_index] * -10;
				}
			}
			DEBUGP(KERN_EMERG "*******WPS_check_LED : Set WPSLED : %d, period : %d[%d]\n", LED_index, WPS_LED_PERIOD, LED_array[LED_index]);
			WPSLED.expires = jiffies + WPS_LED_PERIOD * (HZ/10) ;
			LED_index++;
			WPSLED.function = WPS_check_LED;	
			add_timer(&WPSLED);	
		}
		else
		{
			DEBUGP("WPS_check_LED : Stop\n");
      small_led_set(4);
			del_timer_sync(&WPSLED);	
		}
	}
	return;
}
void WPS_init_LED(void)
{
		DEBUGP("WPS_init_LED : Stop all WPSLED\n");
		
		printk(KERN_EMERG "WPS BUTTON press!!\n");

		del_timer_sync(&WPSLED);
		LED_index = 0;
		print_LED_array();
		WPS_check_LED(0);
}

int wps_write_proc(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	char buf[10] = {0};
	int len = 0;
	int x = 0;
	DEBUGP("WPS WRITE\n");

	if(count >= 10)
	{
		len = 9;
	}
	else
	{
		len = count;
	}
		
	if(copy_from_user(buf, buffer, len))
	{
		DEBUGP("WPS WRITE : fail.\n");		
		return -EFAULT;
	}

	DEBUGP("WPS WRITE : get data...[%s].\n", buf);

	if((buf[0] == WPS_FAIL) && (WPS_LED_STATUS != WPS_FAIL))
	{
		
    memset(LED_array,0,sizeof(int)*100);
    WPS_LED_STATUS = WPS_FAIL;
		LED_array[0] = 1;
		LED_array[1] = -1;
		LED_array[2] = WPS_LED_REPEAT;
		WPS_init_LED();		
	}
	else if((buf[0] == WPS_SUCCESS) && (WPS_LED_STATUS != WPS_SUCCESS))
	{
		memset(LED_array,0,sizeof(int)*100);
    WPS_LED_STATUS = WPS_SUCCESS;
		//LED_array[0] = 3000;
		LED_array[0] = 30;
		LED_array[1] = WPS_LED_STOP;
		WPS_init_LED();
	}	
	else if((buf[0] == WPS_SUCCESS_NONE) && (WPS_LED_STATUS != WPS_SUCCESS_NONE))
	{
		memset(LED_array,0,sizeof(int)*100);
    WPS_LED_STATUS = WPS_SUCCESS_NONE;
		LED_array[0] = 3000;
		LED_array[1] = -1;
		LED_array[2] = WPS_LED_STOP;
		WPS_init_LED();
	}		
	else if((buf[0] == WPS_IN_PROGRESS) && (WPS_LED_STATUS != WPS_IN_PROGRESS))
	{
	  memset(LED_array,0,sizeof(int)*100);
  	WPS_LED_STATUS = WPS_IN_PROGRESS;
		LED_array[0] = 2;
		LED_array[1] = -1;
		LED_array[2] = WPS_LED_REPEAT;
		WPS_init_LED();
	}	
	else if((buf[0] == WPS_SESSION_OVERLAP) && (WPS_LED_STATUS != WPS_SESSION_OVERLAP))
	{
		memset(LED_array,0,sizeof(int)*100);
    WPS_LED_STATUS = WPS_SESSION_OVERLAP;
		
		for(x=0;x<50;x++)
		{
			if((x%2) == 0)
			{
				LED_array[x] = 1;
			}
			else
			{
				LED_array[x] = -1;
			}
		}
		LED_array[50] = -1; 
    LED_array[51] = WPS_LED_STOP; 
		WPS_init_LED();			
	}
	else if((buf[0] == WPS_TIMEOUT) && (WPS_LED_STATUS != WPS_TIMEOUT))
	{
    memset(LED_array,0,sizeof(int)*100);
    WPS_LED_STATUS = WPS_TIMEOUT;		
	
		for(x=0;x<50;x++)
		{
			if((x%2) == 0)
			{
				LED_array[x] = 1;
			}
			else
			{
				LED_array[x] = -1;
			}
		}
		LED_array[50] = -1; 
    LED_array[51] = WPS_LED_STOP; 
	
		WPS_init_LED();		
	}	
	else if((buf[0] == WPS_TIMEOUT_NONE) && (WPS_LED_STATUS != WPS_TIMEOUT_NONE))
	{
	  memset(LED_array,0,sizeof(int)*100);
    WPS_LED_STATUS = WPS_TIMEOUT_NONE;
    LED_array[0] = 1;
		LED_array[1] = -1;
		LED_array[2] = WPS_LED_REPEAT;


		for(x=0;x<50;x++)
		{
			if((x%2) == 0)
			{
				LED_array[x] = 1;
			}
			else
			{
				LED_array[x] = -1;
			}
		}
		LED_array[50] = -1;
		LED_array[51] = WPS_LED_STOP;

		WPS_init_LED();		
	}		
	else if((buf[0] == WPS_RESET) && (WPS_LED_STATUS != WPS_RESET))
	{
		memset(LED_array,0,sizeof(int)*100);
    WPS_LED_STATUS = WPS_RESET;
		LED_array[0] = -1;
		LED_array[0] = WPS_LED_STOP;
		WPS_init_LED();			
	}	
	else if((buf[0] == WPS_RESET_NONE) && (WPS_LED_STATUS != WPS_RESET_NONE))
	{
		memset(LED_array,0,sizeof(int)*100);
    WPS_LED_STATUS = WPS_RESET_NONE;
		LED_array[0] = -1;
		LED_array[1] = WPS_LED_STOP;
		WPS_init_LED();			
	}		
	return len;
}  
			   
          
int wps_read_proc(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
	int ret = 0;
	int len = 0;
		
	DEBUGP("WPS READ\n");

	len = sprintf(buf, "%d\n", ret);	
	return len;
}
#endif

#if SUPPORT_RESET_BUTTON

/* Gemtek Reset button init */
struct timer_list Reset;
int	ResetCount = 0;

static int reset_btn_read_proc(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;

	len = sprintf(buf, btn_action);

	return len;
}

void ResetScan(void)
{
	unsigned int ret = 0;

  GPIO_read_bit(RESET_BUTTON,&ret);

	if(!ret) //Button is pressed.
	{
		big_led_set(2);
		ResetCount++;
		printk( "reset_btn : reset_btn is %d...press\n", ResetCount);
		
		if(ResetCount>=RESET_TIME)
		{ 
		    printk(KERN_EMERG "reset_btn : reset_btn is %d > %d...press\n", ResetCount,RESET_TIME);
		    sprintf(btn_action, "RESET NOW!");
		}
	}
	else //Button is released.
	{
		if(ResetCount > 0 && ResetCount < RESET_TIME)
		{
 			sprintf(btn_action, "REBOOT NOW!");
		}
		ResetCount = 0;
	}

	return;
}

void ResetCheck(unsigned long data)
{
	ResetScan();
	Reset.expires = jiffies + HZ;
	add_timer(&Reset);
	return;
}
/* Gemtek Reset button init End*/
#endif

#if SUPPORT_GEMTEK_MP_TEST	
struct timer_list GTK_MP;

void GEMTEK_MP_Scan(void)
{
	unsigned int ret = 0, ret1 = 0;

	ret = 0;
  	GPIO_read_bit(POWER_BUTTON, &ret);

	ret1 = 0;
	GPIO_read_bit(RESET_BUTTON,&ret1);
	
	if((ret == 1) && (ret1 == 0)) //RESET Button is pressed.
	{
		printk(KERN_EMERG "RESET BUTTON press!! - Dark all LED\n");
		plugin_led_set(7);
		plugin_led_set(4);
		GPIO_write_bit(POWER_BLUE_LED, 0);
		GPIO_write_bit(POWER_BLUE_LED, 1);
		
	}
	else if((ret == 0) && (ret1 == 1)) //POWER Button is pressed.
	{	
		printk(KERN_EMERG "POWER BUTTON press!! - Lite up all LED\n");
		plugin_led_set(4);
		plugin_led_set(7);
		GPIO_write_bit(POWER_BLUE_LED, 1);
		GPIO_write_bit(POWER_BLUE_LED, 0);
	}
	
/*	
  	ret = 0;
  	GPIO_read_bit(WPS_BUTTON, &ret);

	ret1 = 0;
	GPIO_read_bit(RESET_BUTTON,&ret1);
	  
	if((ret == 1) && (ret1 == 0)) //RESET Button is pressed.
	{
		printk(KERN_EMERG "RESET BUTTON press!! - Dark all LED\n");
		big_led_set(4);
		printk(KERN_EMERG "RESET BUTTON press!! - Dark all LED\n");
		small_led_set(4);
	}
	else if((ret == 0) && (ret1 == 1)) //WPS Button is pressed.
	{	

		printk(KERN_EMERG "WPS BUTTON press!! - Lite up all LED\n");
		big_led_set(5);
		printk(KERN_EMERG "WPS BUTTON press!! - Lite up all LED\n");
		small_led_set(5);
	}
	else //Other
	{
	}
*/	
	return;	
}
void GEMTEK_MP_Check(unsigned long data)
{
	del_timer_sync(&GTK_MP);/* Fix Kernel Panic */

	GEMTEK_MP_Scan();
	GTK_MP.expires = jiffies + HZ;
	add_timer(&GTK_MP);
	return;
}

int gemtek_test_write_proc(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	return 0;
}

int gemtek_test_read_proc(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0;
	int i = 0;
	
	DEBUGP("GEMTEK TEST\n");
	
	remove_proc_entry("resetDefault", NULL);
  	remove_proc_entry("easylink", NULL);
 	remove_proc_entry("WPS_LED", NULL);

	for(i = 0;i < GPIO_TOTAL; i++)
	{
		remove_proc_entry(GPIO_LIST_NAME[i], NULL);
	}

		remove_proc_entry("SPEED", NULL);
	
#if SUPPORT_RESET_BUTTON	
	del_timer_sync(&Reset);
#endif
#if SUPPORT_WPS_BUTTON	
	del_timer_sync(&EasyLink);
#endif
#if SUPPORT_WPS_LED	
	del_timer_sync(&WPSLED);
#endif

	del_timer_sync(&GTK_MP);/* Fix Kernel Panic */

	GTK_MP.expires = jiffies + HZ;
	GTK_MP.function = GEMTEK_MP_Check;	
	add_timer(&GTK_MP);
	
	len = sprintf(buf, "GEMTEK TEST\n");	
	return len;
}
#endif


void __exit wemo_gpio_exit(void)
{
	int i = 0;

	del_timer_sync(&PLUGIN_LED_GPIO);
	del_timer_sync(&MOTION_SENSOR_GPIO);
#if SUPPORT_GEMTEK_MP_TEST	
	del_timer_sync(&GTK_MP);
	remove_proc_entry("GEMTEK_BTN_TEST", NULL);
#endif

#if SUPPORT_RESET_BUTTON	
	del_timer_sync(&Reset);
	remove_proc_entry("resetDefault", NULL);
#endif

	remove_proc_entry("PLUGIN_LED_GPIO", NULL);
	remove_proc_entry("MOTION_SENSOR_STATUS", NULL);
	remove_proc_entry("MOTION_SENSOR_SET_DELAY", NULL);
	remove_proc_entry("MOTION_SENSOR_SET_SENSITIVITY", NULL);
	remove_proc_entry("GEMTEK_BTN_TEST_PLUG", NULL);
	for(i = 0; i < GPIO_TOTAL; i++) {
		remove_proc_entry(GPIO_LIST_NAME[i],NULL);
	}
#ifdef POWER_DBL_CLICK
	remove_proc_entry("POWER_BUTTON_STATE", NULL);
	del_timer(&PowerButtonTimer);
#endif
	printk("SNS gpio driver exited\n");
}

static int __init LED_init(void)
{	
	printk(KERN_EMERG "Gemtek LED init...\n");

#if SUPPORT_RESET_BUTTON	
	/* Gemtek Reset button init */
	create_proc_read_entry("resetDefault",0,NULL,reset_btn_read_proc,NULL);
	init_timer(&Reset);
//	Reset.expires = jiffies + HZ;
//	Reset.function = ResetCheck;
//	add_timer(&Reset);
	/* Gemtek Reset button init End */
#endif	

#if SUPPORT_WPS_BUTTON	
		/* Gemtek EasyLink button init */
		create_proc_read_entry("easylink",0,NULL,easylink_btn_read_proc,NULL);	
//		init_timer(&EasyLink);	
//		EasyLink.expires = jiffies + HZ;
//		EasyLink.function = EasyLinkCheck;	
//		add_timer(&EasyLink);
		/* Gemtek EasyLink button init End */
#endif
	
#if SUPPORT_WPS_LED		
		if(create_proc_read_write_entry("WPS_LED",0,NULL,wps_read_proc,wps_write_proc,NULL) == NULL)
		{
			DEBUGP("WPS LED : fail\n");
		}
		else
		{
			DEBUGP("WPS LED : success\n");			
		}
			init_timer(&WPSLED);	
			WPSLED.function = WPS_check_LED;	
#endif
	
#if SUPPORT_GEMTEK_MP_TEST	
		init_timer(&GTK_MP);
		if(create_proc_read_write_entry("GEMTEK_BTN_TEST",0,NULL,gemtek_test_read_proc,gemtek_test_write_proc,NULL) == NULL)
		{
			DEBUGP("GEMTEK TEST : fail\n");
		}
		else
		{
			DEBUGP("GEMTEK TEST : success\n");			
		}
#endif			
	
		/* Gemtek GPIO */
		GPIO_init();
		
		/* Gemtek BELKIN BIG LED GPIO init */
		if(create_proc_read_write_entry("BIG_LED_GPIO",0,NULL,NULL,BIG_LED_GPIO_proc,NULL))
			DEBUGP("create_proc_read_write_entry  BIG_LED_GPIO: fail\n");
		else
			DEBUGP("create_proc_read_write_entry   BIG_LED_GPIO: success\n");
			
		  init_timer(&BIG_LED_GPIO);
		/* Gemtek BELKIN BIG LED GPIO End */
		
		/* Gemtek BELKIN SMALL LED GPIO init */
		if(create_proc_read_write_entry("SMALL_LED_GPIO",0,NULL,NULL,SMALL_LED_GPIO_proc,NULL))
			DEBUGP("create_proc_read_write_entry  SMALL_LED_GPIO: fail\n");
		else
			DEBUGP("create_proc_read_write_entry   SMALL_LED_GPIO: success\n");
			
		init_timer(&SMALL_LED_GPIO);
		/* Gemtek BELKIN SMALL LED GPIO End */
    	
		//GPIO_write_bit(WATCH_DOG_ENABLE, WatchD_OFF);
		//GPIO_write_bit(WATCH_DOG_INPUT, WatchD_OFF);

		/* Gemtek GPIO End */
		
		GPIO_write_bit_22_27(BIG_GREEN_LED,LED_OFF);
		GPIO_write_bit_22_27(BIG_AMBER_LED,LED_ON);

#ifdef PLUGIN_BOARD_NEW		
		/* PLUGIN LED GPIO init */
		if(create_proc_read_write_entry("PLUGIN_LED_GPIO",0,NULL,NULL,PLUGIN_LED_GPIO_proc,NULL))
			DEBUGP("create_proc_read_write_entry  PLUGIN_LED_GPIO: fail\n");
		else
			DEBUGP("create_proc_read_write_entry  PLUGIN_LED_GPIO: success\n");
			
		  init_timer(&PLUGIN_LED_GPIO);
		/* PLUGIN LED GPIO End */
		
		plugin_led_set(0);
		
  		GPIO_read_bit(RESET_BUTTON,&ret);
  		//GPIO_read_bit(POWER_BUTTON,&ret);// Reset Button not Ready, use Power Button to simulate.
		if(!ret) //Button is pressed.
		{
			plugin_led_set(6);
			printk(KERN_EMERG "################## Restore to Factory Defaults ###################\n");
			sprintf(btn_action, "RESET NOW!");
			//printk(KERN_EMERG "################## Restore to Factory Defaults, btn_action=[%s] ###################\n", btn_action);
		}
		else
			printk(KERN_EMERG "################## Don't Restore to Factory Defaults ###################\n");
		
		//MOTION_SENSOR_GPIO
		if(create_proc_read_write_entry("MOTION_SENSOR_STATUS",0,NULL,MOTION_SENSOR_STATUS_read_proc,MOTION_SENSOR_STATUS_write_proc,NULL))
			DEBUGP("create_proc_read_write_entry  MOTION_SENSOR_STATUS: fail\n");
		else
			DEBUGP("create_proc_read_write_entry  MOTION_SENSOR_STATUS: success\n");
			
		if(create_proc_read_write_entry("MOTION_SENSOR_SET_DELAY",0,NULL,NULL,MOTION_SENSOR_SET_DELAY_proc,NULL))
			DEBUGP("create_proc_read_write_entry  MOTION_SENSOR_SET_DELAY: fail\n");
		else
			DEBUGP("create_proc_read_write_entry  MOTION_SENSOR_SET_DELAY: success\n");
		
		if(create_proc_read_write_entry("MOTION_SENSOR_SET_SENSITIVITY",0,NULL,NULL,MOTION_SENSOR_SET_SENSITIVITY_proc,NULL))
			DEBUGP("create_proc_read_write_entry  MOTION_SENSOR_SET_SENSITIVITY: fail\n");
		else
			DEBUGP("create_proc_read_write_entry  MOTION_SENSOR_SET_SENSITIVITY: success\n");
			
		init_timer(&MOTION_SENSOR_GPIO);
#endif

		return 1;
}

int wemo_gpio_ioctl(unsigned int req,unsigned long arg)
{
	return -ENOIOCTLCMD;
}

void __init wemo_gpio_init()
{
	u32 gpiomode;
	u32 reg1_gpio_dir;
	u32 reg1_gpio_pull;
	u32 reg2_gpio_dir;
	u32 reg2_gpio_data;
	u32 set_mode;
	int i;

	//config these pins to gpio mode
	gpiomode = le32_to_cpu(*(volatile u32 *)(RALINK_REG_GPIOMODE));

#if defined (CONFIG_RALINK_RT3052) || defined (CONFIG_RALINK_RT2883) || defined(CONFIG_RALINK_RT3883) || defined(CONFIG_RALINK_RT5350)
	gpiomode &= ~0x1C;  //clear bit[2:4]UARTF_SHARE_MODE
#endif

	gpiomode |= RALINK_GPIOMODE_DFT;
	*(volatile u32 *)(RALINK_REG_GPIOMODE) = cpu_to_le32(gpiomode);

	//enable gpio interrupt
	*(volatile u32 *)(RALINK_REG_INTENA) = cpu_to_le32(RALINK_INTCTL_PIO);
	for (i = 0; i < RALINK_GPIO_NUMBER; i++) {
		ralink_gpio_info[i].irq = i;
		ralink_gpio_info[i].pid = 0;
	}
	
		/* set GPIO11(Watch Dog Enable), GPIO7(Watch Dog Input), GPIO10(SW Reset), GPIO13 (WPS PBC) to input mode */
	reg1_gpio_dir = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));
	printk(KERN_EMERG "reg1_gpio_dir ori = [0x%08X]\n", reg1_gpio_dir);

#ifdef PLUGIN_BOARD_NEW
printk(" ################################################\n");
printk(" #                                              #\n");
printk(" #            SDK - PLUGIN_BOARD_DVT            #\n");
printk(" #                                              #\n");
printk(" ################################################\n");

	set_mode = 0x00002A80;// set GPIO 7, 9, 11, 13 to 1 (Output)
	//u32 set_mode = 0x00002880;// set GPIO 7, 11, 13 to 1 (Output)
	reg1_gpio_dir |= set_mode;
	*(volatile u32 *)(RALINK_REG_PIODIR) = cpu_to_le32(reg1_gpio_dir);
	reg1_gpio_dir = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));
	printk(KERN_EMERG "reg1_gpio_dir after 1 = [0x%08X]\n", reg1_gpio_dir);
	
	set_mode = 0xFFFFABFF;// set GPIO 10, 12, 14 to 0 (Iutput)
	//set_mode = 0xFFFFA9FF;// set GPIO 9, 10, 12, 14 to 0 (Input)
	reg1_gpio_dir &= set_mode;
	*(volatile u32 *)(RALINK_REG_PIODIR) = cpu_to_le32(reg1_gpio_dir);
	reg1_gpio_dir = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));
	printk(KERN_EMERG "reg1_gpio_dir after 2 = [0x%08X]\n", reg1_gpio_dir);
		
#else
printk("################################################\n");
printk("#                                              #\n");
printk("#            SDK - PLUGIN_BOARD_EVB            #\n");
printk("#                                              #\n");
printk("################################################\n");
	set_mode = 0xFFFFD37F;// set bit 7, 10, 11,13 to 0 (Input)
	reg1_gpio_dir &= set_mode;
	*(volatile u32 *)(RALINK_REG_PIODIR) = cpu_to_le32(reg1_gpio_dir);
	reg1_gpio_dir = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));
	printk(KERN_EMERG "reg1_gpio_dir old after = [0x%08X]\n", reg1_gpio_dir);
#endif


	/* motion sensor pull down */
	reg1_gpio_pull = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PULLEN));
	set_mode = 0x04000000;//set bit26 to 1 
	reg1_gpio_pull |= set_mode;
	*(volatile u32 *)(RALINK_REG_PULLEN) = cpu_to_le32(reg1_gpio_pull);

	/* close Watch Dog */
/*	u32 reg1_gpio_data = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODATA));
	set_mode = 0xFFFFF7FF;//set bit11(Watch Dog Enable) to 0
	reg1_gpio_data &= set_mode;
	*(volatile u32 *)(RALINK_REG_PIODATA) = cpu_to_le32(reg1_gpio_data);
*/	
	/* init Reg2 GPIO direction*/
	reg2_gpio_dir = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO5140DIR));
	set_mode = 0x0000003D; // set GPIO 22,24,25,26 to 1 (1:output 0:input)
	reg2_gpio_dir |= set_mode;	
	*(volatile u32 *)(RALINK_REG_PIO5140DIR) = cpu_to_le32(reg2_gpio_dir);
	
	/* init Reg2 GPIO Data*/
	reg2_gpio_data = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO5140DATA));
	set_mode = 0x0000003D; // set GPIO 22,24,25,26 to 1 (1:LED OFF 0:LED ON)
	reg2_gpio_data |= set_mode;	
	*(volatile u32 *)(RALINK_REG_PIO5140DATA) = cpu_to_le32(reg2_gpio_data);


#ifdef CONFIG_RALINK_GPIO_LED
	ralink_gpio_led_init_timer();
#endif

#ifdef POWER_DBL_CLICK
	for(i = 0; WeMoProcs[i].ProcName != NULL; i++) {
		struct proc_dir_entry *p;
		p = create_proc_read_write_entry(WeMoProcs[i].ProcName,0,NULL,
													WeMoProcs[i].ReadProc,
													WeMoProcs[i].WriteProc,NULL);
		if(p == NULL) {
			printk(KERN_EMERG "%s: failed to register WeMoProc %s\n",
					 __FUNCTION__,WeMoProcs[i].ProcName);
		}
		else {
			printk(KERN_NOTICE "%s: registered WeMoProc %s\n",
					 __FUNCTION__,WeMoProcs[i].ProcName);
		}
	}
// Start watching the power button
	init_timer(&PowerButtonTimer);
	PowerButtonTimer.expires = jiffies + 1;
	PowerButtonTimer.function = WatchPowerButton;
	add_timer(&PowerButtonTimer);
#endif		

	printk("SNS gpio driver initialized\n");
	LED_init();
}

#ifdef POWER_DBL_CLICK
static void LogBadState()
{
	printk(KERN_ERR "Error: PowerButtonLast is %d in PowerButtonState %d\n",
			 PowerButtonLast,PowerButtonState);
}

static void LogInvalidState()
{
	printk(KERN_ERR "Error: Invalid PowerButtonState %d\n",
			 PowerButtonState);
	PowerButtonState = IDLE;
}


static void WatchPowerButton(unsigned long unused)
{
	int Current;

	GPIO_read_bit(POWER_BUTTON,&Current);

	if(PowerButtonLast != Current) {
	// Button has changed state
		unsigned int DeltaT = jiffies - LastChangeTime;
		PowerButtonLast = Current;
		LastChangeTime = jiffies;
		if(Current) {
		// Button released
			switch(PowerButtonState) {
			case IDLE:
			case CLICKED:
			case DBL_CLICKED:
			// shouldn't happen
				LogBadState();
				break;

			case PRESSED_1:
				if(DeltaT >= MinClickTime) {
				// A possible single click
					PowerButtonState = CLICKED_W;
				}
				else {
				// Clicked too fast, ignore it
					PowerButtonState = IDLE;
				}
				break;

			case HELD:
				PowerButtonState = IDLE;
				if((jiffies - ButtonReportedTime) > ClickClearDelay) {
				// Clear HELD state if it has been present long enough to be noticed
					ReportedButtonState = IDLE;
				}
				break;

			case PRESSED_2:
				if((jiffies - ClickStarted) < DoubleClickTime) {
				// A double click
					PowerButtonState = DBL_CLICKED;
					ReportedButtonState = DBL_CLICKED;
					ButtonReportedTime = jiffies;
				}
				else {
					PowerButtonState = IDLE;
				}
				break;

			case CLICKED_W:
				break;

			default:
				LogInvalidState();
				break;
			}
		}
		else {
		// Button pressed
			switch(PowerButtonState) {
			case IDLE:
			case CLICKED:
			case DBL_CLICKED:
			// New sequence starting
				ClickStarted = jiffies;
				PowerButtonState = PRESSED_1;
				break;

			case PRESSED_1:
			case HELD:
			case PRESSED_2:
			// shouldn't happen
				LogBadState();
				break;

			case CLICKED_W:
			// Not a single click, either a double click or a long hold
				PowerButtonState = PRESSED_2;
				break;

			default:
				LogInvalidState();
				break;
			}
		}
	}
	else if(!Current) {
	// Button has not changed state and it's pressed
		switch(PowerButtonState) {
			case PRESSED_1:
			case PRESSED_2:
				if((jiffies - ClickStarted) > MaxClickTime) {
				// Button is being held
					PowerButtonState = HELD;
					ReportedButtonState = HELD;
					ButtonReportedTime = jiffies;
				}
				break;

		case IDLE:
		case CLICKED_W:
		case CLICKED:
		case DBL_CLICKED:
		// shouldn't happen
			LogBadState();
			break;

		case HELD:
			break;

		default:
			LogInvalidState();
			break;
		}
	}
	else {
	// Button has not changed state, it's not pressed
		switch(PowerButtonState) {
		case IDLE:
		case HELD:
		case CLICKED:
		case DBL_CLICKED:
			break;

		case PRESSED_1:
		case PRESSED_2:
		// shouldn't happen
			LogBadState();
			break;

		case CLICKED_W:
			if((jiffies - ClickStarted) >= DoubleClickTime) {
			// Too late to be a double click
				PowerButtonState = CLICKED;
				ReportedButtonState = CLICKED;
				ButtonReportedTime = jiffies;
			}
			break;

		default:
			LogInvalidState();
			break;
		}
	}


	switch(ReportedButtonState) {
		case IDLE:
			break;

		case HELD:
			if(Current && (jiffies - ButtonReportedTime) > ClickClearDelay) {
				ReportedButtonState = IDLE;
			}
			break;

		case CLICKED:
		case DBL_CLICKED:
			if((jiffies - ButtonReportedTime) > ClickClearDelay) {
			// Clear last actions
				ReportedButtonState = IDLE;
			}
			break;

		case PRESSED_1:
		case PRESSED_2:
		case CLICKED_W:
		// shouldn't happen
			LogBadState();
			break;

		default:
			LogInvalidState();
			break;
	}

	PowerButtonTimer.expires = jiffies + 1;
	add_timer(&PowerButtonTimer);
}

static int PowerButtonStateReadProc(char *buf,char **start,off_t off,int count,int *eof,void *data)
{
	buf[0] = ReportedButtonState + '0';
	buf[1] = '\n';
	buf[2] = 0;
	return 2;
}

#endif

EXPORT_SYMBOL(wemo_gpio_ioctl);
