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
#include "guardian.h"

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

void plugin_led_set_new(int ledcount);
int GPIO_read_bit(u32 port,u32 *get_value);
int GPIO_write_bit(u32 port,u32 set_value);
int GPIO_write_bit_22_27(u32 port,u32 set_value);


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
int GPIO_write_proc(struct file *file, const char __user *buffer, unsigned long count, void *data);
int GPIO_read_proc_22_27(char *buf, char **start, off_t off, int count, int *eof, void *data);
int GPIO_write_proc_22_27(struct file *file, const char __user *buffer, unsigned long count, void *data);

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
				if(0==last_status){
					//printk(KERN_EMERG "----------------- MOTION -----------------\n");
				}
				last_status = 1;
			}
			else
			{
				motion_sensor_status = 0;
				if(1==last_status) {
					//printk(KERN_EMERG "----------------- NO MOTION -----------------\n");
				}
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
		printk(KERN_EMERG "################## en plugin_led_set 0 ###################\n");
		GPIO_write_bit(WIFI_AMBER_LED, LED_OFF);
		GPIO_write_bit(WIFI_BLUE_LED, LED_OFF);	

		plugin_gpio_led = LED_OFF ;

		PLUGIN_LED_GPIO.expires = jiffies + (1 * (HZ/2));
		PLUGIN_LED_GPIO.function = WIFI_LED_BLUE_BLINK;		
		add_timer(&PLUGIN_LED_GPIO);
		printk(KERN_EMERG "################## ex plugin_led_set 0 ###################\n");

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
		printk(KERN_EMERG "################## en plugin_led_set 6 ###################\n");
		GPIO_write_bit(WIFI_AMBER_LED, LED_OFF);
		GPIO_write_bit(WIFI_BLUE_LED, LED_OFF);	

		plugin_gpio_led = LED_OFF ;

		PLUGIN_LED_GPIO.expires = jiffies + (1 * (HZ/2));
		PLUGIN_LED_GPIO.function = WIFI_LED_AMBER_BLINK_300;		
		add_timer(&PLUGIN_LED_GPIO);
		printk(KERN_EMERG "################## ex plugin_led_set 6 ###################\n");
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
	GPIO_write_bit_22_27(gpio->GPIO_PIN_NUM, &ret );

	sprintf(buf, "%d\n", ret);
	return 1;
}

int GPIO_write_proc(struct file *file, const char __user *buffer, unsigned long count, void *data)
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

int gemtek_test_plug_write_proc(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	return 0;
}

int gemtek_test_plug_read_proc(char *buf, char **start, off_t off, int count, int *eof, void *data)
{
	plugin_led_set_new(4);
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

void WIFI_LED_BLUE_BLINK_NEW(unsigned long data){

	if(plugin_gpio_led == LED_OFF){
		GPIO_write_bit_22_27(WIFI_BLUE_LED, LED_ON);	
		plugin_gpio_led = LED_ON;
	}
	else{
		GPIO_write_bit_22_27(WIFI_BLUE_LED, LED_OFF);	
		plugin_gpio_led = LED_OFF;
	}

	PLUGIN_LED_GPIO.expires = jiffies + (7 * (HZ/10));
	add_timer(&PLUGIN_LED_GPIO);		
	return ;
}

void WIFI_LED_BLUE_OFF_NEW(unsigned long data){
	GPIO_write_bit_22_27(WIFI_BLUE_LED, LED_OFF);	
	return ;
}

void WIFI_LED_AMBER_BLINK_NEW(unsigned long data){

	if(plugin_gpio_led == LED_OFF){
		GPIO_write_bit_22_27(WIFI_AMBER_LED, LED_ON);	
		plugin_gpio_led = LED_ON;
	}
	else{
		GPIO_write_bit_22_27(WIFI_AMBER_LED, LED_OFF);	
		plugin_gpio_led = LED_OFF;
	}

	PLUGIN_LED_GPIO.expires = jiffies + (7 * (HZ/10));
	add_timer(&PLUGIN_LED_GPIO);		
	return ;
}

void WIFI_LED_CHANGE_BLINK_NEW(unsigned long data){

	if(plugin_gpio_led == LED_OFF){
		GPIO_write_bit_22_27(WIFI_AMBER_LED, LED_OFF);
		GPIO_write_bit_22_27(WIFI_BLUE_LED, LED_ON);		
		plugin_gpio_led = LED_ON;
	}
	else{
		GPIO_write_bit_22_27(WIFI_BLUE_LED, LED_OFF);
		GPIO_write_bit_22_27(WIFI_AMBER_LED, LED_ON);		
		plugin_gpio_led = LED_OFF;
	}

	PLUGIN_LED_GPIO.expires = jiffies + (7 * (HZ/10));
	add_timer(&PLUGIN_LED_GPIO);		
	return ;
}

void WIFI_LED_AMBER_BLINK_300_NEW(unsigned long data){

	if(plugin_gpio_led == LED_OFF){
		GPIO_write_bit_22_27(WIFI_AMBER_LED, LED_ON);	
		plugin_gpio_led = LED_ON;
	}
	else{
		GPIO_write_bit_22_27(WIFI_AMBER_LED, LED_OFF);	
		plugin_gpio_led = LED_OFF;
	}

	PLUGIN_LED_GPIO.expires = jiffies + (3 * (HZ/10));
	add_timer(&PLUGIN_LED_GPIO);		
	return ;
}

void plugin_led_set_new(int ledcount)
{

	del_timer_sync(&PLUGIN_LED_GPIO);

	if(ledcount==0)
	{
		printk(KERN_EMERG "################## en plugin_led_set_new 0 ###################\n");
		GPIO_write_bit_22_27(WIFI_AMBER_LED, LED_OFF);
		GPIO_write_bit_22_27(WIFI_BLUE_LED, LED_OFF);	

		plugin_gpio_led = LED_OFF ;

		PLUGIN_LED_GPIO.expires = jiffies + (1 * (HZ/2));
		PLUGIN_LED_GPIO.function = WIFI_LED_BLUE_BLINK_NEW;		
		add_timer(&PLUGIN_LED_GPIO);
		printk(KERN_EMERG "################## ex plugin_led_set_new 0 ###################\n");

	}
	else if(ledcount==1) 
	{
		GPIO_write_bit_22_27(WIFI_AMBER_LED, LED_OFF);
		GPIO_write_bit_22_27(WIFI_BLUE_LED, LED_OFF);	

		plugin_gpio_led = LED_OFF ;

		GPIO_write_bit_22_27(WIFI_BLUE_LED, LED_ON);	
		PLUGIN_LED_GPIO.expires = jiffies + (30 * HZ);
		PLUGIN_LED_GPIO.function = WIFI_LED_BLUE_OFF_NEW;		
		add_timer(&PLUGIN_LED_GPIO);	
	}
	else if(ledcount==2) 
	{
		GPIO_write_bit_22_27(WIFI_BLUE_LED, LED_OFF);
		GPIO_write_bit_22_27(WIFI_AMBER_LED, LED_ON);
	}
	else if(ledcount==3) 
	{
		GPIO_write_bit_22_27(WIFI_AMBER_LED, LED_OFF);
		GPIO_write_bit_22_27(WIFI_BLUE_LED, LED_OFF);	

		plugin_gpio_led = LED_OFF ;

		PLUGIN_LED_GPIO.expires = jiffies + (1 * (HZ/2));
		PLUGIN_LED_GPIO.function = WIFI_LED_AMBER_BLINK_NEW;		
		add_timer(&PLUGIN_LED_GPIO);
	}
	else if(ledcount==4)
	{
		GPIO_write_bit_22_27(WIFI_BLUE_LED, LED_OFF);
		GPIO_write_bit_22_27(WIFI_AMBER_LED, LED_OFF);
	}
	else if(ledcount==5) 
	{
		GPIO_write_bit_22_27(WIFI_AMBER_LED, LED_OFF);
		GPIO_write_bit_22_27(WIFI_BLUE_LED, LED_OFF);	

		plugin_gpio_led = LED_OFF ;

		PLUGIN_LED_GPIO.expires = jiffies + (1 * (HZ/2));
		PLUGIN_LED_GPIO.function = WIFI_LED_CHANGE_BLINK_NEW;		
		add_timer(&PLUGIN_LED_GPIO);
	}
	else if(ledcount==6) 
	{
		printk(KERN_EMERG "################## en plugin_led_set_new 6 ###################\n");
		GPIO_write_bit_22_27(WIFI_AMBER_LED, LED_OFF);
		GPIO_write_bit_22_27(WIFI_BLUE_LED, LED_OFF);	

		plugin_gpio_led = LED_OFF ;

		PLUGIN_LED_GPIO.expires = jiffies + (1 * (HZ/2));
		PLUGIN_LED_GPIO.function = WIFI_LED_AMBER_BLINK_300_NEW;		
		add_timer(&PLUGIN_LED_GPIO);
		printk(KERN_EMERG "################## ex plugin_led_set_new 6 ###################\n");
	}
	else if(ledcount==7)
	{
		GPIO_write_bit_22_27(WIFI_BLUE_LED, LED_ON);
		GPIO_write_bit_22_27(WIFI_AMBER_LED, LED_ON);
	}

}

static int __init LED_init(void)
{	
	unsigned int ret = 0;

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
	init_timer(&PLUGIN_LED_GPIO);
	plugin_led_set_new(0);

	GPIO_read_bit(RESET_BUTTON,&ret);
	//printk(KERN_EMERG "################## Do not Restore to Factory Defaults %d###################\n", ret);
	if(!ret) //Button is pressed.
	{
		plugin_led_set_new(6);
		printk(KERN_EMERG "################## Restore to Factory Defaults ###################\n");
		sprintf(btn_action, "RESET NOW!");
		//printk(KERN_EMERG "################## Restore to Factory Defaults, btn_action=[%s] ###################\n", btn_action);
	}
	else
		printk(KERN_EMERG "################## Don't Restore to Factory Defaults ###################\n");
	
	
	return 1;
}

void __init wemo_gpio_init()
{
	u32 gpiomode;
	u32 reg1_gpio_dir;
	u32 reg1_gpio_data;
	u32 set_mode;
	u32 reg2_gpio_dir;
	u32 reg2_gpio_data;
	int i;

	printk(KERN_EMERG "GPIO init for babymonitor ...\n");
	gpiomode = le32_to_cpu(*(volatile u32 *)(RALINK_REG_GPIOMODE));
	gpiomode |= RALINK_GPIOMODE_DFT;
	*(volatile u32 *)(RALINK_REG_GPIOMODE) = cpu_to_le32(gpiomode);

	/* set GPIO11(Watch Dog Enable), GPIO7(Watch Dog Input), GPIO10(SW Reset), GPIO13 (WPS PBC) to input mode */
	reg1_gpio_dir = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));
	set_mode = 0xFFFFD37F;
	reg1_gpio_dir |= set_mode; // set bit 7, 10, 11,13 to 0 (Input)
	*(volatile u32 *)(RALINK_REG_PIODIR) = cpu_to_le32(reg1_gpio_dir);
	reg1_gpio_dir = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));

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

	/* init Reg1 GPIO direction*/
	set_mode = 0xFFFFD7FE;// set bit 0, 11, 13 to 0 (Input)
	reg1_gpio_dir &= set_mode;
	*(volatile u32 *)(RALINK_REG_PIODIR) = cpu_to_le32(reg1_gpio_dir);
	reg1_gpio_dir = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));
	printk("set GPIO 0, 11, 13 to 0 Input\n");

	set_mode = 0x00005000;// set GPIO 12, 14 to 1 (Output)
	reg1_gpio_dir |= set_mode;
	*(volatile u32 *)(RALINK_REG_PIODIR) = cpu_to_le32(reg1_gpio_dir);
	reg1_gpio_dir = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODIR));
	printk("set GPIO 12, 14 to 1 Output\n");
	printk("init Reg1 GPIO direction = %x\n", reg1_gpio_dir);

	/* init Reg2 GPIO direction*/
	reg2_gpio_dir = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO5140DIR));
	set_mode = 0x0000001E; // set GPIO 23,24,25,26 to 1 (1:output 0:input)
	reg2_gpio_dir |= set_mode;	
	*(volatile u32 *)(RALINK_REG_PIO5140DIR) = cpu_to_le32(reg2_gpio_dir);
	printk("set GPIO 23, 24, 25, 26 to 1 Output\n");
	printk("init Reg2 GPIO direction = %x\n", reg2_gpio_dir);

	/* init Reg2 GPIO Data*/
	reg2_gpio_data = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO5140DATA));
	set_mode = 0xFFFFFFFB; // set GPIO 24 to 0 (1:LED OFF 0:LED ON)
	reg2_gpio_data &= set_mode;	
	*(volatile u32 *)(RALINK_REG_PIO5140DATA) = cpu_to_le32(reg2_gpio_data); 

	reg2_gpio_data = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIO5140DATA));
	set_mode = 0x0000001A; // set GPIO 23,25,26 to 1 (1:LED OFF 0:LED ON)
	reg2_gpio_data |= set_mode;	
	*(volatile u32 *)(RALINK_REG_PIO5140DATA) = cpu_to_le32(reg2_gpio_data); 

	/* init Reg1 GPIO Data*/
	reg1_gpio_data = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODATA));
	set_mode = 0x00005000; // set GPIO 12, 14 to 1 (1:LED OFF 0:LED ON)
	reg1_gpio_data |= set_mode;	
	*(volatile u32 *)(RALINK_REG_PIODATA) = cpu_to_le32(reg1_gpio_data);

	LED_init();
}

int wemo_gpio_ioctl(unsigned int req,unsigned long arg)
{
	int Ret = -ENOIOCTLCMD;

	switch (arg) {
	case RALINK_GPIO_SET_CUSTOM_LED:
	{
		int *ledval = (int *) arg;
		printk(KERN_ERR "%s: write gpio led RALINK_GPIO_SET_CUSTOM_LED P:0x%x, V:0x%x\n",
				 __FUNCTION__,ledval,*((unsigned int *)ledval));
		plugin_led_set_new(*ledval);
		Ret = 0;
	}
	break;

	default:
		return -ENOIOCTLCMD;
	}

	return Ret;
}
EXPORT_SYMBOL(wemo_gpio_ioctl);


void wemo_gpio_irq_handler()
{
}
EXPORT_SYMBOL(wemo_gpio_irq_handler);

void __exit wemo_gpio_exit(void)
{
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
	remove_proc_entry("GEMTEK_BTN_TEST_PLUG", NULL);
}

EXPORT_SYMBOL(wemo_gpio_init);
