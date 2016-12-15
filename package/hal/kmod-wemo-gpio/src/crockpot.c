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

#include "ralink_gpio.h"
#include "crockpot.h"

#if 0
#define DEBUGP(format, args...) printk(KERN_EMERG "%s:%s: " format, __FILE__, __FUNCTION__, ## args)
#else
#define DEBUGP(x, args...)
#endif

static int TempLedDisplayValue;
static int LedDisplayValue;
static int LowLed;
static int HighLed;
static int WarmLed;

static struct ProcData_tag {
	const char *ProcEntryName;
	void *DataPtr;
} ProcData[] = {
	{"crockpot_display",&LedDisplayValue},
	{"crockpot_low",&LowLed},
	{"crockpot_high",&HighLed},
	{"crockpot_warm",&WarmLed},
	{NULL}
};

static int crockpot_read_proc(char *buf, char **start, off_t off, int count,  int *eof, void *data)
{
	sprintf(buf,"%d\n",*((int *) data));
	return 1;
}

void wemo_gpio_irq_handler()
{
	u32 tmp = le32_to_cpu(*(volatile u32 *)(RALINK_REG_PIODATA));
	int DigitSelect = tmp & WM_DIGITS;  // digit select is active high
	int Segments = ~tmp & WM_SEGMENTS;	// segments are active low
	int Multiplier = 0;
	int Value = 0;

// Each of the 4 LED digit drive lines cause an interrupt.  
// Decode the currently displayed digit and update the count.
// When we get a complete update (all 4 digits) the update the
// user readable value.

	switch(DigitSelect) {
	case WM_DIG1:
		Multiplier = 1;
		TempLedDisplayValue = 0;	// clear temp total for new scan
	// the colon between the hours and minutes is driven by seg H now... 
	// not very interesting
		break;

	case WM_DIG2:
		Multiplier = 10;
	// the Warm LED is driven by seg H now
		if(Segments & WM_SEG_H) {
		// Warm LED is off
			WarmLed = 0;
		}
		else {
		// Warm LED is on
			WarmLed = 1;
		}
		break;

	case WM_DIG3:
		Multiplier = 100;
	// the Low LED is driven by seg H now
		if(Segments & WM_SEG_H) {
		// Low LED is off
			LowLed = 0;
		}
		else {
		// Low LED is on
			LowLed = 1;
		}
		break;

	case WM_DIG4:
		Multiplier = 1000;
	// the High LED is driven by seg H now
		if(Segments & WM_SEG_H) {
		// High LED is off
			HighLed = 0;
		}
		else {
		// High LED is on
			HighLed = 1;
		}
		break;

	default:
		DEBUGP("Invalid DigitSelect value: 0x%x\n",DigitSelect);
		Multiplier = 0;
		break;
	}

	if(Multiplier != 0) {
		switch(Segments) {
		case SEGS_0:
			break;

		case SEGS_1:
			Value = 1;
			break;

		case SEGS_2:
			Value = 2;
			break;

		case SEGS_3:
			Value = 3;
			break;

		case SEGS_4:
			Value = 4;
			break;

		case SEGS_5:
			Value = 5;
			break;

		case SEGS_6:
			Value = 6;
			break;

		case SEGS_7:
			Value = 7;
			break;

		case SEGS_8:
			Value = 8;
			break;

		case SEGS_9:
			Value = 9;
			break;

		default:
			DEBUGP("Invalid Segments value: 0x%x\n",Segments);
			Value = 0;
		}
	}

	TempLedDisplayValue += Value * Multiplier;

	if(DigitSelect == WM_DIG4) {
	// Update user readable total
		LedDisplayValue = TempLedDisplayValue;
	}
}

int wemo_gpio_ioctl(unsigned int req,unsigned long arg)
{
	return -ENOIOCTLCMD;
}

void __init wemo_gpio_init()
{
	struct ProcData_tag *p = ProcData;

	printk(KERN_EMERG "GPIO init for CrockPot ...\n");

	while(p->ProcEntryName != NULL) {
		if(create_proc_read_entry(p->ProcEntryName,
										  0,
										  NULL,
										  crockpot_read_proc,
										  p->DataPtr) == NULL)
		{
			DEBUGP("create_proc_read_entry for %s failed\n",p->ProcEntryName);
		}
		p++;
	}
}

void __exit wemo_gpio_exit(void)
{
	struct ProcData_tag *p = ProcData;

	while(p->ProcEntryName != NULL) {
		remove_proc_entry(p->ProcEntryName,NULL);
		p++;
	}
}

