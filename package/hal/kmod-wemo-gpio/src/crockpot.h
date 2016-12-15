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

#define	WM_DIG1		1		// Bit to drive the LS digit on the display
#define	WM_DIG2		2		
#define	WM_DIG3		4
#define	WM_DIG4		8  	// Bit to drive the MS digit on the display
#define	WM_DIGITS	(WM_DIG1 | WM_DIG2 | WM_DIG3 | WM_DIG4)

#define	WM_SEG_A		0x10		// Segment A drive bit
#define	WM_SEG_B		0x20
#define	WM_SEG_E		0x40
#define	WM_SEG_F		0x80
#define	WM_SEG_G		0x100
#define	WM_SEG_H		0x200
#define	WM_SEGMENTS	(WM_SEG_A | WM_SEG_B | WM_SEG_E | WM_SEG_F | WM_SEG_G)

// Map of which segments are on for which displayed digit
#define	SEGS_0		(WM_SEG_A | WM_SEG_B | WM_SEG_E | WM_SEG_F)
#define	SEGS_1		(WM_SEG_B)
#define	SEGS_2		(WM_SEG_A | WM_SEG_B | WM_SEG_E | WM_SEG_G)
#define	SEGS_3		(WM_SEG_A | WM_SEG_B | WM_SEG_G)
#define	SEGS_4		(WM_SEG_B | WM_SEG_F | WM_SEG_G)
#define	SEGS_5		(WM_SEG_A | WM_SEG_F | WM_SEG_G)
#define	SEGS_6		(WM_SEG_A | WM_SEG_E | WM_SEG_F | WM_SEG_G)
#define	SEGS_7		(WM_SEG_A | WM_SEG_B)
#define	SEGS_8		(WM_SEG_A | WM_SEG_B | WM_SEG_E | WM_SEG_F | WM_SEG_G)
#define	SEGS_9		(WM_SEG_A | WM_SEG_B | WM_SEG_F | WM_SEG_G)

void wemo_gpio_irq_handler(void);
void __init wemo_gpio_init(void);
int wemo_gpio_ioctl(unsigned int req,unsigned long arg);
void __exit wemo_gpio_exit(void);

