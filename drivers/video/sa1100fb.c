/*
 *  linux/drivers/video/sa1100fb.c
 *
 *  Copyright (C) 1999 Eric A. Thomas
 *   Based on acornfb.c Copyright (C) Russell King.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	        StrongARM 1100 LCD Controller Frame Buffer Driver
 *
 * Please direct your questions and comments on this driver to the following
 * email address:
 *
 *	linux-arm-kernel@lists.arm.linux.org.uk
 *
 * Clean patches should be sent to the ARM Linux Patch System.  Please see the
 * following web page for more information:
 *
 *	http://www.arm.linux.org.uk/developer/patches/info.shtml
 *
 * Thank you.
 *
 * Known problems:
 *	- With the Neponset plugged into an Assabet, LCD powerdown
 *	  doesn't work (LCD stays powered up).  Therefore we shouldn't
 *	  blank the screen.
 *	- We don't limit the CPU clock rate nor the mode selection
 *	  according to the available SDRAM bandwidth.
 *
 * Other notes:
 *	- Linear grayscale palettes and the kernel.
 *	  Such code does not belong in the kernel.  The kernel frame buffer
 *	  drivers do not expect a linear colourmap, but a colourmap based on
 *	  the VT100 standard mapping.
 *
 *	  If your _userspace_ requires a linear colourmap, then the setup of
 *	  such a colourmap belongs _in userspace_, not in the kernel.  Code
 *	  to set the colourmap correctly from user space has been sent to
 *	  David Neuer.  It's around 8 lines of C code, plus another 4 to
 *	  detect if we are using grayscale.
 *
 *	- The following must never be specified in a panel definition:
 *	     LCCR0_LtlEnd, LCCR3_PixClkDiv, LCCR3_VrtSnchL, LCCR3_HorSnchL
 *
 *	- The following should be specified:
 *	     either LCCR0_Color or LCCR0_Mono
 *	     either LCCR0_Sngl or LCCR0_Dual
 *	     either LCCR0_Act or LCCR0_Pas
 *	     either LCCR3_OutEnH or LCCD3_OutEnL
 *	     either LCCR3_PixRsEdg or LCCR3_PixFlEdg
 *	     either LCCR3_ACBsDiv or LCCR3_ACBsCntOff
 *
 * Code Status:
 * 1999/04/01:
 *	- Driver appears to be working for Brutus 320x200x8bpp mode.  Other
 *	  resolutions are working, but only the 8bpp mode is supported.
 *	  Changes need to be made to the palette encode and decode routines
 *	  to support 4 and 16 bpp modes.  
 *	  Driver is not designed to be a module.  The FrameBuffer is statically
 *	  allocated since dynamic allocation of a 300k buffer cannot be 
 *	  guaranteed. 
 *
 * 1999/06/17:
 *	- FrameBuffer memory is now allocated at run-time when the
 *	  driver is initialized.    
 *
 * 2000/04/10: Nicolas Pitre <nico@cam.org>
 *	- Big cleanup for dynamic selection of machine type at run time.
 *
 * 2000/07/19: Jamey Hicks <jamey@crl.dec.com>
 *	- Support for Bitsy aka Compaq iPAQ H3600 added.
 *
 * 2000/08/07: Tak-Shing Chan <tchan.rd@idthk.com>
 *	       Jeff Sutherland <jsutherland@accelent.com>
 *	- Resolved an issue caused by a change made to the Assabet's PLD 
 *	  earlier this year which broke the framebuffer driver for newer 
 *	  Phase 4 Assabets.  Some other parameters were changed to optimize
 *	  for the Sharp display.
 *
 * 2000/08/09: Kunihiko IMAI <imai@vasara.co.jp>
 *	- XP860 support added
 *
 * 2000/08/19: Mark Huang <mhuang@livetoy.com>
 *	- Allows standard options to be passed on the kernel command line
 *	  for most common passive displays.
 *
 * 2000/08/29:
 *	- s/save_flags_cli/local_irq_save/
 *	- remove unneeded extra save_flags_cli in sa1100fb_enable_lcd_controller
 *
 * 2000/10/10: Erik Mouw <J.A.K.Mouw@its.tudelft.nl>
 *	- Updated LART stuff. Fixed some minor bugs.
 *
 * 2000/10/30: Murphy Chen <murphy@mail.dialogue.com.tw>
 *	- Pangolin support added
 *
 * 2000/10/31: Roman Jordan <jor@hoeft-wessel.de>
 *	- Huw Webpanel support added
 *
 * 2000/11/23: Eric Peng <ericpeng@coventive.com>
 *	- Freebird add
 *
 * 2001/02/07: Jamey Hicks <jamey.hicks@compaq.com> 
 *	       Cliff Brake <cbrake@accelent.com>
 *	- Added PM callback
 *
 * 2001/05/26: <rmk@arm.linux.org.uk>
 *	- Fix 16bpp so that (a) we use the right colours rather than some
 *	  totally random colour depending on what was in page 0, and (b)
 *	  we don't de-reference a NULL pointer.
 *	- remove duplicated implementation of consistent_alloc()
 *	- convert dma address types to dma_addr_t
 *	- remove unused 'montype' stuff
 *	- remove redundant zero inits of init_var after the initial
 *	  memzero.
 *	- remove allow_modeset (acornfb idea does not belong here)
 *
 * 2001/05/28: <rmk@arm.linux.org.uk>
 *	- massive cleanup - move machine dependent data into structures
 *	- I've left various #warnings in - if you see one, and know
 *	  the hardware concerned, please get in contact with me.
 *
 * 2001/05/31: <rmk@arm.linux.org.uk>
 *	- Fix LCCR1 HSW value, fix all machine type specifications to
 *	  keep values in line.  (Please check your machine type specs)
 *
 * 2001/06/10: <rmk@arm.linux.org.uk>
 *	- Fiddle with the LCD controller from task context only; mainly
 *	  so that we can run with interrupts on, and sleep.
 *	- Convert #warnings into #errors.  No pain, no gain. ;)
 *
 * 2001/06/14: <rmk@arm.linux.org.uk>
 *	- Make the palette BPS value for 12bpp come out correctly.
 *	- Take notice of "greyscale" on any colour depth.
 *	- Make truecolor visuals use the RGB channel encoding information.
 *
 * 2001/07/02: <rmk@arm.linux.org.uk>
 *	- Fix colourmap problems.
 *
 * 2001/07/13: <abraham@2d3d.co.za>
 *	- Added support for the ICP LCD-Kit01 on LART. This LCD is
 *	  manufactured by Prime View, model no V16C6448AB
 *
 * 2001/07/23: <rmk@arm.linux.org.uk>
 *	- Hand merge version from handhelds.org CVS tree.  See patch
 *	  notes for 595/1 for more information.
 *	- Drop 12bpp (it's 16bpp with different colour register mappings).
 *	- This hardware can not do direct colour.  Therefore we don't
 *	  support it.
 *
 * 2001/07/27: <rmk@arm.linux.org.uk>
 *	- Halve YRES on dual scan LCDs.
 *
 * 2001/08/22: <rmk@arm.linux.org.uk>
 *	- Add b/w iPAQ pixclock value.
 *
 * 2001/10/12: <rmk@arm.linux.org.uk>
 *	- Add patch 681/1 and clean up stork definitions.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/cpufreq.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/arch/assabet.h>
#include <asm/arch/shannon.h>

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>

/*
 * debugging?
 */
#define DEBUG 0
/*
 * Complain if VAR is out of range.
 */
#define DEBUG_VAR 1

#undef ASSABET_PAL_VIDEO

#include "sa1100fb.h"

extern void (*sa1100fb_backlight_power)(int on);
extern void (*sa1100fb_lcd_power)(int on);

/*
 * IMHO this looks wrong.  In 8BPP, length should be 8.
 */
static struct sa1100fb_rgb rgb_8 = {
	.red	= { .offset = 0,  .length = 4, },
	.green	= { .offset = 0,  .length = 4, },
	.blue	= { .offset = 0,  .length = 4, },
	.transp	= { .offset = 0,  .length = 0, },
};

static struct sa1100fb_rgb def_rgb_16 = {
	.red	= { .offset = 11, .length = 5, },
	.green	= { .offset = 5,  .length = 6, },
	.blue	= { .offset = 0,  .length = 5, },
	.transp	= { .offset = 0,  .length = 0, },
};

#ifdef CONFIG_SA1100_ASSABET
#ifndef ASSABET_PAL_VIDEO
/*
 * The assabet uses a sharp LQ039Q2DS54 LCD module.  It is actually
 * takes an RGB666 signal, but we provide it with an RGB565 signal
 * instead (def_rgb_16).
 */
static struct sa1100fb_mach_info lq039q2ds54_info __initdata = {
	.pixclock	= 171521,	.bpp		= 16,
	.xres		= 320,		.yres		= 240,

	.hsync_len	= 5,		.vsync_len	= 1,
	.left_margin	= 61,		.upper_margin	= 3,
	.right_margin	= 9,		.lower_margin	= 0,

	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,

	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(2),
};
#else
static struct sa1100fb_mach_info pal_info __initdata = {
	.pixclock	= 67797,	.bpp		= 16,
	.xres		= 640,		.yres		= 512,

	.hsync_len	= 64,		.vsync_len	= 6,
	.left_margin	= 125,		.upper_margin	= 70,
	.right_margin	= 115,		.lower_margin	= 36,

	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(512),
};
#endif
#endif

#ifdef CONFIG_SA1100_H3800
static struct sa1100fb_mach_info h3800_info __initdata = {
	.pixclock	= 174757, 	.bpp		= 16,
	.xres		= 320,		.yres		= 240,

	.hsync_len	= 3,		.vsync_len	= 3,
	.left_margin	= 12,		.upper_margin	= 10,
	.right_margin	= 17,		.lower_margin	= 1,

	.cmap_static	= 1,

	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(2),
};
#endif

#ifdef CONFIG_SA1100_H3600
static struct sa1100fb_mach_info h3600_info __initdata = {
	.pixclock	= 174757, 	.bpp		= 16,
	.xres		= 320,		.yres		= 240,

	.hsync_len	= 3,		.vsync_len	= 3,
	.left_margin	= 12,		.upper_margin	= 10,
	.right_margin	= 17,		.lower_margin	= 1,

	.cmap_static	= 1,

	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(2),
};

static struct sa1100fb_rgb h3600_rgb_16 = {
	.red	= { .offset = 12, .length = 4, },
	.green	= { .offset = 7,  .length = 4, },
	.blue	= { .offset = 1,  .length = 4, },
	.transp	= { .offset = 0,  .length = 0, },
};
#endif

#ifdef CONFIG_SA1100_H3100
static struct sa1100fb_mach_info h3100_info __initdata = {
	.pixclock	= 406977, 	.bpp		= 4,
	.xres		= 320,		.yres		= 240,

	.hsync_len	= 26,		.vsync_len	= 41,
	.left_margin	= 4,		.upper_margin	= 0,
	.right_margin	= 4,		.lower_margin	= 0,

	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	.cmap_greyscale	= 1,
	.cmap_inverse	= 1,

	.lccr0		= LCCR0_Mono | LCCR0_4PixMono | LCCR0_Sngl | LCCR0_Pas,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(2),
};
#endif

#ifdef CONFIG_SA1100_BRUTUS
static struct sa1100fb_mach_info brutus_info __initdata = {
	.pixclock	= 0,		.bpp		= 8,
	.xres		= 320,		.yres		= 240,

	.hsync_len	= 3,		.vsync_len	= 1,
	.left_margin	= 41,		.upper_margin	= 0,
	.right_margin	= 101,		.lower_margin	= 0,

	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,

	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Pas,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(2) |
			  LCCR3_PixClkDiv(44),
};
#endif

#ifdef CONFIG_SA1100_CERF
static struct sa1100fb_mach_info cerf_info __initdata = {
#if defined(CONFIG_CERF_LCD_72_A)
	.pixclock	= 171521, 	.bpp		= 8,
	.xres		= 640,		.yres		= 480,
	.lccr0		= LCCR0_Color | LCCR0_Dual | LCCR0_Pas,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(2) |
			  LCCR3_PixClkDiv(38),
#elif defined(CONFIG_CERF_LCD_57_A)
	.pixclock	= 171521, 	.bpp		= 8,
	.xres		= 320,		.yres		= 240,
	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Pas,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(2) |
			  LCCR3_PixClkDiv(38),
#elif defined(CONFIG_CERF_LCD_38_A)
	.pixclock	= 171521, 	.bpp		= 8,
	.xres		= 240,		.yres		= 320,
	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Pas,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(56) |
			  LCCR3_PixClkDiv(38),
#elif defined(CONFIG_CERF_LCD_38_B)
	.pixclock	= 171521,	.bpp		= 4,
	.xres		= 320,		.yres		= 240,
	.lccr0		= LCCR0_Mono | LCCR0_4PixMono | LCCR0_Sngl | LCCR0_Pas,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(56) |
			  LCCR3_PixClkDiv(38),
#else
#error "Must have a CerfBoard LCD form factor selected"
#endif

	.hsync_len	= 5,		.vsync_len	= 1,
	.left_margin	= 61,		.upper_margin	= 3,
	.right_margin	= 9,		.lower_margin	= 0,

	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,

};

#if 0
static struct sa1100fb_rgb cerf_rgb_16 = {
	.red	= { .offset = 8,	.length = 4, },
	.green	= { .offset = 4,	.length = 4, },
	.blue	= { .offset = 0,	.length = 4, },
	.transp	= { .offset = 0,	.length = 0, },
};
#endif
#endif

#ifdef CONFIG_SA1100_FREEBIRD
#warning Please check this carefully
static struct sa1100fb_mach_info freebird_info __initdata = {
	.pixclock	= 171521,	.bpp		= 16,
	.xres		= 240,		.yres		= 320,

	.hsync_len	= 3,		.vsync_len	= 2,
	.left_margin	= 2,		.upper_margin	= 0,
	.right_margin	= 2,		.lower_margin	= 0,

	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,

	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Pas,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixFlEdg | LCCR3_ACBsDiv(2),
};

static struct sa1100fb_rgb freebird_rgb_16 = {
	.red	= { .offset = 8,  .length = 4, },
	.green	= { .offset = 4,  .length = 4, },
	.blue	= { .offset = 0,  .length = 4, },
	.transp	= { .offset = 12, .length = 4, },
};
#endif

#ifdef CONFIG_SA1100_GRAPHICSCLIENT
static struct sa1100fb_mach_info graphicsclient_info __initdata = {
	.pixclock	= 53500,	.bpp		= 8,
	.xres		= 640,		.yres		= 480,

	.hsync_len	= 9,		.vsync_len	= 9,
	.left_margin	= 54,		.upper_margin	= 24,
	.right_margin	= 54,		.lower_margin	= 32,

	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(2),
};
#endif

#ifdef CONFIG_SA1100_HUW_WEBPANEL
static struct sa1100fb_mach_info huw_webpanel_info __initdata = {
	.pixclock	= 0,		.bpp		= 8,
	.xres		= 640,		.yres		= 480,

	.hsync_len	= 3,		.vsync_len	= 1,
	.left_margin	= 41,		.upper_margin	= 0,
	.right_margin	= 101,		.lower_margin	= 0,

	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,

	.lccr0		= LCCR0_Color | LCCR0_Dual | LCCR0_Pas,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(2) | 8,
#error FIXME
	/*
	 * FIXME: please get rid of the '| 8' in preference to an
	 * LCCR3_PixClkDiv() version. --rmk
	 */
};
#endif

#ifdef LART_GREY_LCD
static struct sa1100fb_mach_info lart_grey_info __initdata = {
	.pixclock	= 150000,	.bpp		= 4,
	.xres		= 320,		.yres		= 240,

	.hsync_len	= 1,		.vsync_len	= 1,
	.left_margin	= 4,		.upper_margin	= 0,
	.right_margin	= 2,		.lower_margin	= 0,

	.cmap_greyscale	= 1,
	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,

	.lccr0		= LCCR0_Mono | LCCR0_Sngl | LCCR0_Pas | LCCR0_4PixMono,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(512),
};
#endif
#ifdef LART_COLOR_LCD
static struct sa1100fb_mach_info lart_color_info __initdata = {
	.pixclock	= 150000,	.bpp		= 16,
	.xres		= 320,		.yres		= 240,

	.hsync_len	= 2,		.vsync_len	= 3,
	.left_margin	= 69,		.upper_margin	= 14,
	.right_margin	= 8,		.lower_margin	= 4,

	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixFlEdg | LCCR3_ACBsDiv(512),
};
#endif
#ifdef LART_VIDEO_OUT
static struct sa1100fb_mach_info lart_video_info __initdata = {
	.pixclock	= 39721,	.bpp		= 16,
	.xres		= 640,		.yres		= 480,

	.hsync_len	= 95,		.vsync_len	= 2,
	.left_margin	= 40,		.upper_margin	= 32,
	.right_margin	= 24,		.lower_margin	= 11,

	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,

	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	.lccr3		= LCCR3_OutEnL | LCCR3_PixFlEdg | LCCR3_ACBsDiv(512),
};
#endif

#ifdef LART_KIT01_LCD
static struct sa1100fb_mach_info lart_kit01_info __initdata = {
	.pixclock	= 63291,	.bpp		= 16,
	.xres		= 640,		.yres		= 480,

	.hsync_len	= 64,		.vsync_len	= 3,
	.left_margin	= 122,		.upper_margin	= 45,
	.right_margin	= 10,		.lower_margin	= 10,

	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixFlEdg
};
#endif

#ifdef CONFIG_SA1100_SHANNON
static struct sa1100fb_mach_info shannon_info __initdata = {
	.pixclock	= 152500,	.bpp		= 8,
	.xres		= 640,		.yres		= 480,

	.hsync_len	= 4,		.vsync_len	= 3,
	.left_margin	= 2,		.upper_margin	= 0,
	.right_margin	= 1,		.lower_margin	= 0,

	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT, 

	.lccr0		= LCCR0_Color | LCCR0_Dual | LCCR0_Pas,
	.lccr3		= LCCR3_ACBsDiv(512),
};
#endif

#ifdef CONFIG_SA1100_OMNIMETER
static struct sa1100fb_mach_info omnimeter_info __initdata = {
	.pixclock	= 0,		.bpp		= 4,
	.xres		= 480,		.yres		= 320,

	.hsync_len	= 1,		.vsync_len	= 1,
	.left_margin	= 10,		.upper_margin	= 0,
	.right_margin	= 10,		.lower_margin	= 0,

	.cmap_greyscale	= 1,
	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,

	.lccr0		= LCCR0_Mono | LCCR0_Sngl | LCCR0_Pas | LCCR0_8PixMono,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(255) |
			  LCCR3_PixClkDiv(44),
#error FIXME: fix pixclock, ACBsDiv
	/*
	 * FIXME: I think ACBsDiv is wrong above - should it be 512 (disabled)?
	 *   - rmk
	 */
};
#endif

#ifdef CONFIG_SA1100_PANGOLIN
static struct sa1100fb_mach_info pangolin_info __initdata = {
	.pixclock	= 341521,	.bpp		= 16,
	.xres		= 800,		.yres		= 600,

	.hsync_len	= 64,		.vsync_len	= 7,
	.left_margin	= 160,		.upper_margin	= 7,
	.right_margin	= 24,		.lower_margin	= 1,

	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,

	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixFlEdg | LCCR3_ACBsCntOff,
};
#endif

#ifdef CONFIG_SA1100_STORK
#if STORK_TFT			/* ie the NEC TFT */
/*
 * pixclock is ps per clock. say 72Hz, 800x600 clocks => (1/72)/(800*600)
 * = 28935 and a bit
 * NB likely to be increased to ease bus timings wrt pcmcia interface
 */
static struct sa1100fb_mach_info stork_tft_info __initdata = {
	.pixclock	= 28935,	.bpp		= 16,
	.xres		= 640,		.yres		= 480,

	.hsync_len	= 64,		.vsync_len	= 2,
	.left_margin	= 48,		.upper_margin	= 12,
	.right_margin	= 48,		.lower_margin	= 31,

	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsCntOff,
};

static struct sa1100fb_rgb stork_tft_rgb_16 = {
	.red	= { .offset = 11, .length = 5, },
	.green	= { .offset = 5,  .length = 6, },
	.blue	= { .offset = 0,  .length = 5, },
	.transp	= { .offset = 0,  .length = 0, },
};

#else	/* Kyocera DSTN */

static struct sa1100fb_mach_info stork_dstn_info __initdata = {
	.pixclock	= 0,		.bpp		= 16,
	.xres		= 640,		.yres		= 480,

	.hsync_len	= 2,		.vsync_len	= 2,
	.left_margin	= 2,		.upper_margin	= 0,
	.right_margin	= 2,		.lower_margin	= 0,

	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT ,

	.lccr0		= LCCR0_Color | LCCR0_Dual | LCCR0_Pas,
#error Fixme
	.lccr3		= 0xff00 |
			0x18		/* ought to be 0x14 but DMA isn't up to that as yet */
};

static struct sa1100fb_rgb stork_dstn_rgb_16 = {
	.red	= { .offset = 8,  .length = 4, },
	.green	= { .offset = 4,  .length = 4, },
	.blue	= { .offset = 0,  .length = 4, },
	.transp	= { .offset = 0,  .length = 0, },
};
#endif
#endif

#ifdef CONFIG_SA1100_PT_SYSTEM3
/*
 * 648 x 480 x 8bpp x 75Hz Dual Panel Color STN Display
 *
 * pixclock = 1/( 640*3/8*240 ), [pixclock]=1e-12s=ps
 *	3 due to r,g,b lines
 *	8 due to 8 bit data bus
 *	640 due to 640 pixels per line
 *	240 = 480/2 due to dual panel display
 *	=>4.32Mhz => 231481E-12s
 */
static struct sa1100fb_mach_info system3_info __initdata = {
	.pixclock	= 231481,	.bpp		= 8,
	.xres		= 640,		.yres		= 480,

	.hsync_len	= 2,		.vsync_len	= 2,
	.left_margin	= 2,		.upper_margin	= 0,
	.right_margin	= 2,		.lower_margin	= 0,

	.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,

	.lccr0		= LCCR0_Color | LCCR0_Dual | LCCR0_Pas,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_ACBsDiv(512),
};
#endif

#ifdef CONFIG_SA1100_XP860
static struct sa1100fb_mach_info xp860_info __initdata = {
	.pixclock	= 0,		.bpp		= 8,
	.xres		= 1024,		.yres		= 768,

	.hsync_len	= 3,		.vsync_len	= 3,
	.left_margin	= 3,		.upper_margin	= 2,
	.right_margin	= 2,		.lower_margin	= 1,

	.lccr0		= LCCR0_Color | LCCR0_Sngl | LCCR0_Act,
	.lccr3		= LCCR3_OutEnH | LCCR3_PixRsEdg | LCCR3_PixClkDiv(6),
};
#endif



static struct sa1100fb_mach_info * __init
sa1100fb_get_machine_info(struct sa1100fb_info *fbi)
{
	struct sa1100fb_mach_info *inf = NULL;

	/*
	 *            R        G       B       T
	 * default  {11,5}, { 5,6}, { 0,5}, { 0,0}
	 * h3600    {12,4}, { 7,4}, { 1,4}, { 0,0}
	 * freebird { 8,4}, { 4,4}, { 0,4}, {12,4}
	 */
#ifdef CONFIG_SA1100_ASSABET
	if (machine_is_assabet()) {
#ifndef ASSABET_PAL_VIDEO
		inf = &lq039q2ds54_info;
#else
		inf = &pal_info;
#endif
	}
#endif
#ifdef CONFIG_SA1100_H3100
	if (machine_is_h3100()) {
		inf = &h3100_info;
	}
#endif
#ifdef CONFIG_SA1100_H3600
	if (machine_is_h3600()) {
		inf = &h3600_info;
		fbi->rgb[RGB_16] = &h3600_rgb_16;
	}
#endif
#ifdef CONFIG_SA1100_H3800
	if (machine_is_h3800()) {
		inf = &h3800_info;
	}
#endif
#ifdef CONFIG_SA1100_BRUTUS
	if (machine_is_brutus()) {
		inf = &brutus_info;
	}
#endif
#ifdef CONFIG_SA1100_CERF
	if (machine_is_cerf()) {
		inf = &cerf_info;
	}
#endif
#ifdef CONFIG_SA1100_FREEBIRD
	if (machine_is_freebird()) {
		inf = &freebird_info;
		fbi->rgb[RGB_16] = &freebird_rgb16;
	}
#endif
#ifdef CONFIG_SA1100_GRAPHICSCLIENT
	if (machine_is_graphicsclient()) {
		inf = &graphicsclient_info;
	}
#endif
#ifdef CONFIG_SA1100_HUW_WEBPANEL
	if (machine_is_huw_webpanel()) {
		inf = &huw_webpanel_info;
	}
#endif
#ifdef CONFIG_SA1100_LART
	if (machine_is_lart()) {
#ifdef LART_GREY_LCD
		inf = &lart_grey_info;
#endif
#ifdef LART_COLOR_LCD
		inf = &lart_color_info;
#endif
#ifdef LART_VIDEO_OUT
		inf = &lart_video_info;
#endif
#ifdef LART_KIT01_LCD
		inf = &lart_kit01_info;
#endif
	}
#endif
#ifdef CONFIG_SA1100_OMNIMETER
	if (machine_is_omnimeter()) {
		inf = &omnimeter_info;
	}
#endif
#ifdef CONFIG_SA1100_PANGOLIN
	if (machine_is_pangolin()) {
		inf = &pangolin_info;
	}
#endif
#ifdef CONFIG_SA1100_PT_SYSTEM3
	if (machine_is_pt_system3()) {
		inf = &system3_info;
	}
#endif
#ifdef CONFIG_SA1100_SHANNON
	if (machine_is_shannon()) {
		inf = &shannon_info;
	}
#endif
#ifdef CONFIG_SA1100_STORK
	if (machine_is_stork()) {
#if STORK_TFT
		inf = &stork_tft_info;
		fbi->rgb[RGB_16] = &stork_tft_rgb_16;
#else
		inf = &stork_dstn_info;
		fbi->rgb[RGB_16] = &stork_dstn_rgb_16;
#endif
	}
#endif
#ifdef CONFIG_SA1100_XP860
	if (machine_is_xp860()) {
		inf = &xp860_info;
	}
#endif
	return inf;
}

static int sa1100fb_activate_var(struct fb_var_screeninfo *var, struct sa1100fb_info *);
static void set_ctrlr_state(struct sa1100fb_info *fbi, u_int state);

static inline void sa1100fb_schedule_task(struct sa1100fb_info *fbi, u_int state)
{
	unsigned long flags;

	local_irq_save(flags);
	/*
	 * We need to handle two requests being made at the same time.
	 * There are two important cases:
	 *  1. When we are changing VT (C_REENABLE) while unblanking (C_ENABLE)
	 *     We must perform the unblanking, which will do our REENABLE for us.
	 *  2. When we are blanking, but immediately unblank before we have
	 *     blanked.  We do the "REENABLE" thing here as well, just to be sure.
	 */
	if (fbi->task_state == C_ENABLE && state == C_REENABLE)
		state = (u_int) -1;
	if (fbi->task_state == C_DISABLE && state == C_ENABLE)
		state = C_REENABLE;

	if (state != (u_int)-1) {
		fbi->task_state = state;
		schedule_task(&fbi->task);
	}
	local_irq_restore(flags);
}

static inline u_int chan_to_field(u_int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

/*
 * Convert bits-per-pixel to a hardware palette PBS value.
 */
static inline u_int palette_pbs(struct fb_var_screeninfo *var)
{
	int ret = 0;
	switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_CFB4
	case 4:  ret = 0 << 12;	break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:  ret = 1 << 12; break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16: ret = 2 << 12; break;
#endif
	}
	return ret;
}

static int
sa1100fb_setpalettereg(u_int regno, u_int red, u_int green, u_int blue,
		       u_int trans, struct fb_info *info)
{
	struct sa1100fb_info *fbi = (struct sa1100fb_info *)info;
	u_int val, ret = 1;

	if (regno < fbi->palette_size) {
		val = ((red >> 4) & 0xf00);
		val |= ((green >> 8) & 0x0f0);
		val |= ((blue >> 12) & 0x00f);

		if (regno == 0)
			val |= palette_pbs(&fbi->fb.var);

		fbi->palette_cpu[regno] = val;
		ret = 0;
	}
	return ret;
}

static int
sa1100fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
		   u_int trans, struct fb_info *info)
{
	struct sa1100fb_info *fbi = (struct sa1100fb_info *)info;
	unsigned int val;
	int ret = 1;

	/*
	 * If inverse mode was selected, invert all the colours
	 * rather than the register number.  The register number
	 * is what you poke into the framebuffer to produce the
	 * colour you requested.
	 */
	if (fbi->cmap_inverse) {
		red   = 0xffff - red;
		green = 0xffff - green;
		blue  = 0xffff - blue;
	}

	/*
	 * If greyscale is true, then we convert the RGB value
	 * to greyscale no mater what visual we are using.
	 */
	if (fbi->fb.var.grayscale)
		red = green = blue = (19595 * red + 38470 * green +
					7471 * blue) >> 16;

	switch (fbi->fb.fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/*
		 * 12 or 16-bit True Colour.  We encode the RGB value
		 * according to the RGB bitfield information.
		 */
		if (regno < 16) {
			u16 *pal = fbi->fb.pseudo_palette;

			val  = chan_to_field(red, &fbi->fb.var.red);
			val |= chan_to_field(green, &fbi->fb.var.green);
			val |= chan_to_field(blue, &fbi->fb.var.blue);

			pal[regno] = val;
			ret = 0;
		}
		break;

	case FB_VISUAL_STATIC_PSEUDOCOLOR:
	case FB_VISUAL_PSEUDOCOLOR:
		ret = sa1100fb_setpalettereg(regno, red, green, blue, trans, info);
		break;
	}

	return ret;
}

/*
 *  sa1100fb_display_dma_period()
 *    Calculate the minimum period (in picoseconds) between two DMA
 *    requests for the LCD controller.  If we hit this, it means we're
 *    doing nothing but LCD DMA.
 */
static unsigned int sa1100fb_display_dma_period(struct fb_var_screeninfo *var)
{
	/*
	 * Period = pixclock * bits_per_byte * bytes_per_transfer
	 *		/ memory_bits_per_pixel;
	 */
	return var->pixclock * 8 * 16 / var->bits_per_pixel;
}

/*
 *  sa1100fb_check_var():
 *    Round up in the following order: bits_per_pixel, xres,
 *    yres, xres_virtual, yres_virtual, xoffset, yoffset, grayscale,
 *    bitfields, horizontal timing, vertical timing.
 */
static int
sa1100fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct sa1100fb_info *fbi = (struct sa1100fb_info *)info;
	int rgbidx;

	if (var->xres < MIN_XRES)
		var->xres = MIN_XRES;
	if (var->yres < MIN_YRES)
		var->yres = MIN_YRES;
	if (var->xres > fbi->max_xres)
		var->xres = fbi->max_xres;
	if (var->yres > fbi->max_yres)
		var->yres = fbi->max_yres;
	var->xres_virtual = max(var->xres_virtual, var->xres);
	var->yres_virtual = max(var->yres_virtual, var->yres);

	DPRINTK("var->bits_per_pixel=%d\n", var->bits_per_pixel);
	switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_CFB4
	case 4:
		rgbidx = RGB_8;
		break;
#endif
#ifdef FBCON_HAS_CFB8
	case 8:
		rgbidx = RGB_8;
		break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		rgbidx = RGB_16;
		break;
#endif
	default:
		return -EINVAL;
	}

	/*
	 * Copy the RGB parameters for this display
	 * from the machine specific parameters.
	 */
	var->red    = fbi->rgb[rgbidx]->red;
	var->green  = fbi->rgb[rgbidx]->green;
	var->blue   = fbi->rgb[rgbidx]->blue;
	var->transp = fbi->rgb[rgbidx]->transp;

	DPRINTK("RGBT length = %d:%d:%d:%d\n",
		var->red.length, var->green.length, var->blue.length,
		var->transp.length);

	DPRINTK("RGBT offset = %d:%d:%d:%d\n",
		var->red.offset, var->green.offset, var->blue.offset,
		var->transp.offset);

#ifdef CONFIG_CPU_FREQ
	printk(KERN_DEBUG "dma period = %d ps, clock = %d kHz\n",
		sa1100fb_display_dma_period(var),
		cpufreq_get(smp_processor_id()));
#endif

	return 0;
}

static inline void sa1100fb_set_truecolor(u_int is_true_color)
{
	if (machine_is_assabet()) {
#if 1		// phase 4 or newer Assabet's
		if (is_true_color)
			ASSABET_BCR_set(ASSABET_BCR_LCD_12RGB);
		else
			ASSABET_BCR_clear(ASSABET_BCR_LCD_12RGB);
#else
		// older Assabet's
		if (is_true_color)
			ASSABET_BCR_clear(ASSABET_BCR_LCD_12RGB);
		else
			ASSABET_BCR_set(ASSABET_BCR_LCD_12RGB);
#endif
	}
}

/*
 * sa1100fb_set_par():
 *	Set the user defined part of the display for the specified console
 */
static int sa1100fb_set_par(struct fb_info *info)
{
	struct sa1100fb_info *fbi = (struct sa1100fb_info *)info;
	struct fb_var_screeninfo *var = &info->var;
	unsigned long palette_mem_size;

	DPRINTK("set_par\n");

	if (var->bits_per_pixel == 16)
		fbi->fb.fix.visual = FB_VISUAL_TRUECOLOR;
	else if (!fbi->cmap_static)
		fbi->fb.fix.visual = FB_VISUAL_PSEUDOCOLOR;
	else {
		/*
		 * Some people have weird ideas about wanting static
		 * pseudocolor maps.  I suspect their user space
		 * applications are broken.
		 */
		fbi->fb.fix.visual = FB_VISUAL_STATIC_PSEUDOCOLOR;
	}

	fbi->palette_size = var->bits_per_pixel == 8 ? 256 : 16;

	palette_mem_size = fbi->palette_size * sizeof(u16);

	DPRINTK("palette_mem_size = 0x%08lx\n", (u_long) palette_mem_size);

	fbi->palette_cpu = (u16 *)(fbi->map_cpu + PAGE_SIZE - palette_mem_size);
	fbi->palette_dma = fbi->map_dma + PAGE_SIZE - palette_mem_size;

	/*
	 * Set (any) board control register to handle new color depth
	 */
	sa1100fb_set_truecolor(fbi->fb.fix.visual == FB_VISUAL_TRUECOLOR);

#ifdef CONFIG_SA1100_OMNIMETER
#error Do we have to do this here?   We already do it at init time.
	if (machine_is_omnimeter())
		SetLCDContrast(DefaultLCDContrast);
#endif

	sa1100fb_activate_var(var, fbi);

	return 0;
}

/*
 * sa1100fb_set_var():
 *	Set the user defined part of the display for the specified console
 */
static int
sa1100fb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
	int ret, act;

	act = var->activate & FB_ACTIVATE_MASK;

	ret = gen_set_var(var, con, info);

	if (ret == 0 && act & FB_ACTIVATE_NOW) {
		struct display *display = (con < 0) ? info->disp : fb_display + con;

		/*
		 * fbcon assumes too much.
		 */
		display->can_soft_blank = 1;
	}

	return ret;
}

static int
sa1100fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
		  struct fb_info *info)
{
	struct sa1100fb_info *fbi = (struct sa1100fb_info *)info;
	struct display *disp = (con < 0) ? info->disp : (fb_display + con);

	/*
	 * Make sure the user isn't doing something stupid.
	 */
	if (!kspc && (disp->var.bits_per_pixel == 16 || fbi->cmap_static))
		return -EINVAL;

	return gen_set_cmap(cmap, kspc, con, info);
}

/*
 * Formal definition of the VESA spec:
 *  On
 *  	This refers to the state of the display when it is in full operation
 *  Stand-By
 *  	This defines an optional operating state of minimal power reduction with
 *  	the shortest recovery time
 *  Suspend
 *  	This refers to a level of power management in which substantial power
 *  	reduction is achieved by the display.  The display can have a longer 
 *  	recovery time from this state than from the Stand-by state
 *  Off
 *  	This indicates that the display is consuming the lowest level of power
 *  	and is non-operational. Recovery from this state may optionally require
 *  	the user to manually power on the monitor
 *
 *  Now, the fbdev driver adds an additional state, (blank), where they
 *  turn off the video (maybe by colormap tricks), but don't mess with the
 *  video itself: think of it semantically between on and Stand-By.
 *
 *  So here's what we should do in our fbdev blank routine:
 *
 *  	VESA_NO_BLANKING (mode 0)	Video on,  front/back light on
 *  	VESA_VSYNC_SUSPEND (mode 1)  	Video on,  front/back light off
 *  	VESA_HSYNC_SUSPEND (mode 2)  	Video on,  front/back light off
 *  	VESA_POWERDOWN (mode 3)		Video off, front/back light off
 *
 *  This will match the matrox implementation.
 */
/*
 * sa1100fb_blank():
 *	Blank the display by setting all palette values to zero.  Note, the 
 * 	12 and 16 bpp modes don't really use the palette, so this will not
 *      blank the display in all modes.  
 */
static int sa1100fb_blank(int blank, struct fb_info *info)
{
	struct sa1100fb_info *fbi = (struct sa1100fb_info *)info;
	int i;

	DPRINTK("sa1100fb_blank: blank=%d info->modename=%s\n", blank,
		fbi->fb.modename);

	switch (blank) {
	case VESA_POWERDOWN:
	case VESA_VSYNC_SUSPEND:
	case VESA_HSYNC_SUSPEND:
		if (fbi->fb.fix.visual == FB_VISUAL_PSEUDOCOLOR ||
		    fbi->fb.fix.visual == FB_VISUAL_STATIC_PSEUDOCOLOR)
			for (i = 0; i < fbi->palette_size; i++)
				sa1100fb_setpalettereg(i, 0, 0, 0, 0, info);
		sa1100fb_schedule_task(fbi, C_DISABLE);
		break;

	case VESA_NO_BLANKING:
		if (fbi->fb.fix.visual == FB_VISUAL_PSEUDOCOLOR ||
		    fbi->fb.fix.visual == FB_VISUAL_STATIC_PSEUDOCOLOR)
			fb_set_cmap(&fbi->fb.cmap, 1, info);
		sa1100fb_schedule_task(fbi, C_ENABLE);
	}
	return 0;
}

static struct fb_ops sa1100fb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= sa1100fb_check_var,
	.fb_set_par	= sa1100fb_set_par,
	.fb_set_var	= sa1100fb_set_var,
	.fb_get_cmap	= gen_get_cmap,
	.fb_set_cmap	= sa1100fb_set_cmap,
	.fb_setcolreg	= sa1100fb_setcolreg,
	.fb_blank	= sa1100fb_blank,
};

static int sa1100fb_updatevar(int con, struct fb_info *info)
{
	/* we don't support panning nor scrolling */
	return 0;
}

/*
 * Calculate the PCD value from the clock rate (in picoseconds).
 * We take account of the PPCR clock setting.
 */
static inline unsigned int get_pcd(unsigned int pixclock, unsigned int cpuclock)
{
	unsigned int pcd = cpuclock / 100;

	pcd *= pixclock;
	pcd /= 10000000;

	return pcd + 1;	/* make up for integer math truncations */
}

/*
 * sa1100fb_activate_var():
 *	Configures LCD Controller based on entries in var parameter.  Settings are      
 *	only written to the controller if changes were made.  
 */
static int sa1100fb_activate_var(struct fb_var_screeninfo *var, struct sa1100fb_info *fbi)
{
	struct sa1100fb_lcd_reg new_regs;
	u_int half_screen_size, yres, pcd;
	u_long flags;

	DPRINTK("Configuring SA1100 LCD\n");

	DPRINTK("var: xres=%d hslen=%d lm=%d rm=%d\n",
		var->xres, var->hsync_len,
		var->left_margin, var->right_margin);
	DPRINTK("var: yres=%d vslen=%d um=%d bm=%d\n",
		var->yres, var->vsync_len,
		var->upper_margin, var->lower_margin);

#if DEBUG_VAR
	if (var->xres < 16        || var->xres > 1024)
		printk(KERN_ERR "%s: invalid xres %d\n",
			fbi->fb.fix.id, var->xres);
	if (var->hsync_len < 1    || var->hsync_len > 64)
		printk(KERN_ERR "%s: invalid hsync_len %d\n",
			fbi->fb.fix.id, var->hsync_len);
	if (var->left_margin < 1  || var->left_margin > 255)
		printk(KERN_ERR "%s: invalid left_margin %d\n",
			fbi->fb.fix.id, var->left_margin);
	if (var->right_margin < 1 || var->right_margin > 255)
		printk(KERN_ERR "%s: invalid right_margin %d\n",
			fbi->fb.fix.id, var->right_margin);
	if (var->yres < 1         || var->yres > 1024)
		printk(KERN_ERR "%s: invalid yres %d\n",
			fbi->fb.fix.id, var->yres);
	if (var->vsync_len < 1    || var->vsync_len > 64)
		printk(KERN_ERR "%s: invalid vsync_len %d\n",
			fbi->fb.fix.id, var->vsync_len);
	if (var->upper_margin < 0 || var->upper_margin > 255)
		printk(KERN_ERR "%s: invalid upper_margin %d\n",
			fbi->fb.fix.id, var->upper_margin);
	if (var->lower_margin < 0 || var->lower_margin > 255)
		printk(KERN_ERR "%s: invalid lower_margin %d\n",
			fbi->fb.fix.id, var->lower_margin);
#endif

	new_regs.lccr0 = fbi->lccr0 |
		LCCR0_LEN | LCCR0_LDM | LCCR0_BAM |
		LCCR0_ERM | LCCR0_LtlEnd | LCCR0_DMADel(0);

	new_regs.lccr1 =
		LCCR1_DisWdth(var->xres) +
		LCCR1_HorSnchWdth(var->hsync_len) +
		LCCR1_BegLnDel(var->left_margin) +
		LCCR1_EndLnDel(var->right_margin);

	/*
	 * If we have a dual scan LCD, then we need to halve
	 * the YRES parameter.
	 */
	yres = var->yres;
	if (fbi->lccr0 & LCCR0_Dual)
		yres /= 2;

	new_regs.lccr2 =
		LCCR2_DisHght(yres) +
		LCCR2_VrtSnchWdth(var->vsync_len) +
		LCCR2_BegFrmDel(var->upper_margin) +
		LCCR2_EndFrmDel(var->lower_margin);

	pcd = get_pcd(var->pixclock, cpufreq_get(0));
	new_regs.lccr3 = LCCR3_PixClkDiv(pcd) | fbi->lccr3 |
		(var->sync & FB_SYNC_HOR_HIGH_ACT ? LCCR3_HorSnchH : LCCR3_HorSnchL) |
		(var->sync & FB_SYNC_VERT_HIGH_ACT ? LCCR3_VrtSnchH : LCCR3_VrtSnchL);

	DPRINTK("nlccr0 = 0x%08x\n", new_regs.lccr0);
	DPRINTK("nlccr1 = 0x%08x\n", new_regs.lccr1);
	DPRINTK("nlccr2 = 0x%08x\n", new_regs.lccr2);
	DPRINTK("nlccr3 = 0x%08x\n", new_regs.lccr3);

	half_screen_size = var->bits_per_pixel;
	half_screen_size = half_screen_size * var->xres * var->yres / 16;

	/* Update shadow copy atomically */
	local_irq_save(flags);
	fbi->dbar1 = fbi->palette_dma;
	fbi->dbar2 = fbi->screen_dma + half_screen_size;

	fbi->reg_lccr0 = new_regs.lccr0;
	fbi->reg_lccr1 = new_regs.lccr1;
	fbi->reg_lccr2 = new_regs.lccr2;
	fbi->reg_lccr3 = new_regs.lccr3;
	local_irq_restore(flags);

	/*
	 * Only update the registers if the controller is enabled
	 * and something has changed.
	 */
	if ((LCCR0 != fbi->reg_lccr0)       || (LCCR1 != fbi->reg_lccr1) ||
	    (LCCR2 != fbi->reg_lccr2)       || (LCCR3 != fbi->reg_lccr3) ||
	    (DBAR1 != fbi->dbar1) || (DBAR2 != fbi->dbar2))
		sa1100fb_schedule_task(fbi, C_REENABLE);

	return 0;
}

/*
 * NOTE!  The following functions are purely helpers for set_ctrlr_state.
 * Do not call them directly; set_ctrlr_state does the correct serialisation
 * to ensure that things happen in the right way 100% of time time.
 *	-- rmk
 */
static inline void __sa1100fb_backlight_power(struct sa1100fb_info *fbi, int on)
{
	DPRINTK("backlight o%s\n", on ? "n" : "ff");

	if (sa1100fb_backlight_power)
		sa1100fb_backlight_power(on);
}

static inline void __sa1100fb_lcd_power(struct sa1100fb_info *fbi, int on)
{
	DPRINTK("LCD power o%s\n", on ? "n" : "ff");

	if (sa1100fb_lcd_power)
		sa1100fb_lcd_power(on);
}

static void sa1100fb_setup_gpio(struct sa1100fb_info *fbi)
{
	u_int mask = 0;

	/*
	 * Enable GPIO<9:2> for LCD use if:
	 *  1. Active display, or
	 *  2. Color Dual Passive display
	 *
	 * see table 11.8 on page 11-27 in the SA1100 manual
	 *   -- Erik.
	 *
	 * SA1110 spec update nr. 25 says we can and should
	 * clear LDD15 to 12 for 4 or 8bpp modes with active
	 * panels.  
	 */
	if ((fbi->reg_lccr0 & LCCR0_CMS) == LCCR0_Color &&
	    (fbi->reg_lccr0 & (LCCR0_Dual|LCCR0_Act)) != 0) {
		mask = GPIO_LDD11 | GPIO_LDD10 | GPIO_LDD9  | GPIO_LDD8;

		if (fbi->fb.var.bits_per_pixel > 8 ||
		    (fbi->reg_lccr0 & (LCCR0_Dual|LCCR0_Act)) == LCCR0_Dual)
			mask |= GPIO_LDD15 | GPIO_LDD14 | GPIO_LDD13 | GPIO_LDD12;

	}

	/* GPIO15 is used as a bypass for 3.8" displays */
	if (machine_is_cerf())
		mask |= GPIO_GPIO15;

	if (mask) {
		GPDR |= mask;
		GAFR |= mask;
	}
}

static void sa1100fb_enable_controller(struct sa1100fb_info *fbi)
{
	DPRINTK("Enabling LCD controller\n");

	/*
	 * Make sure the mode bits are present in the first palette entry
	 */
	fbi->palette_cpu[0] &= 0xcfff;
	fbi->palette_cpu[0] |= palette_pbs(&fbi->fb.var);

	/* Sequence from 11.7.10 */
	LCCR3 = fbi->reg_lccr3;
	LCCR2 = fbi->reg_lccr2;
	LCCR1 = fbi->reg_lccr1;
	LCCR0 = fbi->reg_lccr0 & ~LCCR0_LEN;
	DBAR1 = fbi->dbar1;
	DBAR2 = fbi->dbar2;
	LCCR0 |= LCCR0_LEN;

#ifdef CONFIG_SA1100_GRAPHICSCLIENT
#error Where is GPIO24 set as an output?  Can we fit this in somewhere else?
	if (machine_is_graphicsclient()) {
		// From ADS doc again...same as disable
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(20 * HZ / 1000);
		GPSR |= GPIO_GPIO24;
	}
#endif

	if (machine_is_shannon()) {
		GPDR |= SHANNON_GPIO_DISP_EN;
		GPSR |= SHANNON_GPIO_DISP_EN;
	}	

	DPRINTK("DBAR1 = %p\n", DBAR1);
	DPRINTK("DBAR2 = %p\n", DBAR2);
	DPRINTK("LCCR0 = 0x%08x\n", LCCR0);
	DPRINTK("LCCR1 = 0x%08x\n", LCCR1);
	DPRINTK("LCCR2 = 0x%08x\n", LCCR2);
	DPRINTK("LCCR3 = 0x%08x\n", LCCR3);
}

static void sa1100fb_disable_controller(struct sa1100fb_info *fbi)
{
	DECLARE_WAITQUEUE(wait, current);

	DPRINTK("Disabling LCD controller\n");

#ifdef CONFIG_SA1100_GRAPHICSCLIENT
#error Where is GPIO24 set as an output?  Can we fit this in somewhere else?
	if (machine_is_graphicsclient()) {
		/*
		 * From ADS internal document:
		 *  GPIO24 should be LOW at least 10msec prior to disabling
		 *  the LCD interface.
		 *
		 * We'll wait 20msec.
		 */
		GPCR |= GPIO_GPIO24;
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(20 * HZ / 1000);
	}
#endif
#ifdef CONFIG_SA1100_HUW_WEBPANEL
#error Move me into __sa1100fb_lcd_power and/or __sa1100fb_backlight_power
	if (machine_is_huw_webpanel()) {
		// dont forget to set the control lines to zero (?)
		DPRINTK("ShutDown HuW LCD controller\n");
		BCR_clear(BCR_TFT_ENA + BCR_CCFL_POW + BCR_PWM_BACKLIGHT);
	}
#endif

	if (machine_is_shannon()) {
		GPCR |= SHANNON_GPIO_DISP_EN;
	}	

	add_wait_queue(&fbi->ctrlr_wait, &wait);
	set_current_state(TASK_UNINTERRUPTIBLE);

	LCSR = 0xffffffff;	/* Clear LCD Status Register */
	LCCR0 &= ~LCCR0_LDM;	/* Enable LCD Disable Done Interrupt */
	LCCR0 &= ~LCCR0_LEN;	/* Disable LCD Controller */

	schedule_timeout(20 * HZ / 1000);
	current->state = TASK_RUNNING;
	remove_wait_queue(&fbi->ctrlr_wait, &wait);
}

/*
 *  sa1100fb_handle_irq: Handle 'LCD DONE' interrupts.
 */
static void sa1100fb_handle_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	struct sa1100fb_info *fbi = dev_id;
	unsigned int lcsr = LCSR;

	if (lcsr & LCSR_LDD) {
		LCCR0 |= LCCR0_LDM;
		wake_up(&fbi->ctrlr_wait);
	}

	LCSR = lcsr;
}

/*
 * This function must be called from task context only, since it will
 * sleep when disabling the LCD controller, or if we get two contending
 * processes trying to alter state.
 */
static void set_ctrlr_state(struct sa1100fb_info *fbi, u_int state)
{
	u_int old_state;

	down(&fbi->ctrlr_sem);

	old_state = fbi->state;

	switch (state) {
	case C_DISABLE_CLKCHANGE:
		/*
		 * Disable controller for clock change.  If the
		 * controller is already disabled, then do nothing.
		 */
		if (old_state != C_DISABLE && old_state != C_DISABLE_PM) {
			fbi->state = state;
			sa1100fb_disable_controller(fbi);
		}
		break;

	case C_DISABLE_PM:
	case C_DISABLE:
		/*
		 * Disable controller
		 */
		if (old_state != C_DISABLE) {
			fbi->state = state;

			__sa1100fb_backlight_power(fbi, 0);
			if (old_state != C_DISABLE_CLKCHANGE)
				sa1100fb_disable_controller(fbi);
			__sa1100fb_lcd_power(fbi, 0);
		}
		break;

	case C_ENABLE_CLKCHANGE:
		/*
		 * Enable the controller after clock change.  Only
		 * do this if we were disabled for the clock change.
		 */
		if (old_state == C_DISABLE_CLKCHANGE) {
			fbi->state = C_ENABLE;
			sa1100fb_enable_controller(fbi);
		}
		break;

	case C_REENABLE:
		/*
		 * Re-enable the controller only if it was already
		 * enabled.  This is so we reprogram the control
		 * registers.
		 */
		if (old_state == C_ENABLE) {
			sa1100fb_disable_controller(fbi);
			sa1100fb_setup_gpio(fbi);
			sa1100fb_enable_controller(fbi);
		}
		break;

	case C_ENABLE_PM:
		/*
		 * Re-enable the controller after PM.  This is not
		 * perfect - think about the case where we were doing
		 * a clock change, and we suspended half-way through.
		 */
		if (old_state != C_DISABLE_PM)
			break;
		/* fall through */

	case C_ENABLE:
		/*
		 * Power up the LCD screen, enable controller, and
		 * turn on the backlight.
		 */
		if (old_state != C_ENABLE) {
			fbi->state = C_ENABLE;
			sa1100fb_setup_gpio(fbi);
			__sa1100fb_lcd_power(fbi, 1);
			sa1100fb_enable_controller(fbi);
			__sa1100fb_backlight_power(fbi, 1);
		}
		break;
	}
	up(&fbi->ctrlr_sem);
}

/*
 * Our LCD controller task (which is called when we blank or unblank)
 * via keventd.
 */
static void sa1100fb_task(void *dummy)
{
	struct sa1100fb_info *fbi = dummy;
	u_int state = xchg(&fbi->task_state, -1);

	set_ctrlr_state(fbi, state);
}

#ifdef CONFIG_CPU_FREQ
/*
 * Calculate the minimum DMA period over all displays that we own.
 * This, together with the SDRAM bandwidth defines the slowest CPU
 * frequency that can be selected.
 */
static unsigned int sa1100fb_min_dma_period(struct sa1100fb_info *fbi)
{
	unsigned int min_period = (unsigned int)-1;
	int i;

	for (i = 0; i < MAX_NR_CONSOLES; i++) {
		struct display *disp = &fb_display[i];
		unsigned int period;

		/*
		 * Do we own this display?
		 */
		if (disp->fb_info != &fbi->fb)
			continue;

		/*
		 * Ok, calculate its DMA period
		 */
		period = sa1100fb_display_dma_period(&disp->var);
		if (period < min_period)
			min_period = period;
	}

	return min_period;
}

/*
 * CPU clock speed change handler.  We need to adjust the LCD timing
 * parameters when the CPU clock is adjusted by the power management
 * subsystem.
 */
static int
sa1100fb_clkchg_notifier(struct notifier_block *nb, unsigned long val,
			 void *data)
{
	struct sa1100fb_info *fbi = TO_INF(nb, clockchg);
	struct cpufreq_minmax *mm = data;
	u_int pcd;

	switch (val) {
	case CPUFREQ_MINMAX:
		printk(KERN_DEBUG "min dma period: %d ps, old clock %d kHz, "
			"new clock %d kHz\n", sa1100fb_min_dma_period(fbi),
			mm->cur_freq, mm->new_freq);
		/* todo: fill in min/max values */
		break;

	case CPUFREQ_PRECHANGE:
		set_ctrlr_state(fbi, C_DISABLE_CLKCHANGE);
		break;

	case CPUFREQ_POSTCHANGE:
		pcd = get_pcd(fbi->fb.var.pixclock, cpufreq_get(0));
		fbi->reg_lccr3 = (fbi->reg_lccr3 & ~0xff) | LCCR3_PixClkDiv(pcd);
		set_ctrlr_state(fbi, C_ENABLE_CLKCHANGE);
		break;
	}
	return 0;
}
#endif

#ifdef CONFIG_PM
/*
 * Power management hook.  Note that we won't be called from IRQ context,
 * unlike the blank functions above, so we may sleep.
 */
static int
sa1100fb_pm_callback(struct pm_dev *pm_dev, pm_request_t req, void *data)
{
	struct sa1100fb_info *fbi = pm_dev->data;

	DPRINTK("pm_callback: %d\n", req);

	if (req == PM_SUSPEND || req == PM_RESUME) {
		int state = (int)data;

		if (state == 0) {
			/* Enter D0. */
			set_ctrlr_state(fbi, C_ENABLE_PM);
		} else {
			/* Enter D1-D3.  Disable the LCD controller.  */
			set_ctrlr_state(fbi, C_DISABLE_PM);
		}
	}
	DPRINTK("done\n");
	return 0;
}
#endif

/*
 * sa1100fb_map_video_memory():
 *      Allocates the DRAM memory for the frame buffer.  This buffer is  
 *	remapped into a non-cached, non-buffered, memory region to  
 *      allow palette and pixel writes to occur without flushing the 
 *      cache.  Once this area is remapped, all virtual memory
 *      access to the video memory should occur at the new region.
 */
static int __init sa1100fb_map_video_memory(struct sa1100fb_info *fbi)
{
	/*
	 * We reserve one page for the palette, plus the size
	 * of the framebuffer.
	 */
	fbi->map_size = PAGE_ALIGN(fbi->fb.fix.smem_len + PAGE_SIZE);
	fbi->map_cpu = consistent_alloc(GFP_KERNEL, fbi->map_size,
					&fbi->map_dma);

	if (fbi->map_cpu) {
		fbi->fb.screen_base = fbi->map_cpu + PAGE_SIZE;
		fbi->screen_dma = fbi->map_dma + PAGE_SIZE;
		fbi->fb.fix.smem_start = fbi->screen_dma;
	}

	return fbi->map_cpu ? 0 : -ENOMEM;
}

/* Fake monspecs to fill in fbinfo structure */
static struct fb_monspecs monspecs __initdata = {
	30000, 70000, 50, 65, 0	/* Generic */
};


static struct sa1100fb_info * __init sa1100fb_init_fbinfo(void)
{
	struct sa1100fb_mach_info *inf;
	struct sa1100fb_info *fbi;

	fbi = kmalloc(sizeof(struct sa1100fb_info) + sizeof(struct display) +
		      sizeof(u16) * 16, GFP_KERNEL);
	if (!fbi)
		return NULL;

	memset(fbi, 0, sizeof(struct sa1100fb_info) + sizeof(struct display));

	strcpy(fbi->fb.fix.id, SA1100_NAME);

	fbi->fb.fix.type	= FB_TYPE_PACKED_PIXELS;
	fbi->fb.fix.type_aux	= 0;
	fbi->fb.fix.xpanstep	= 0;
	fbi->fb.fix.ypanstep	= 0;
	fbi->fb.fix.ywrapstep	= 0;
	fbi->fb.fix.line_length	= 0;
	fbi->fb.fix.accel	= FB_ACCEL_NONE;

	fbi->fb.var.nonstd	= 0;
	fbi->fb.var.activate	= FB_ACTIVATE_NOW;
	fbi->fb.var.height	= -1;
	fbi->fb.var.width	= -1;
	fbi->fb.var.accel_flags	= 0;
	fbi->fb.var.vmode	= FB_VMODE_NONINTERLACED;

	strcpy(fbi->fb.modename, SA1100_NAME);
	strcpy(fbi->fb.fontname, "Acorn8x8");

	fbi->fb.fbops		= &sa1100fb_ops;
	fbi->fb.changevar	= NULL;
	fbi->fb.switch_con	= gen_switch;
	fbi->fb.updatevar	= sa1100fb_updatevar;
	fbi->fb.flags		= FBINFO_FLAG_DEFAULT;
	fbi->fb.node		= NODEV;
	fbi->fb.monspecs	= monspecs;
	fbi->fb.currcon		= -1;
	fbi->fb.disp		= (struct display *)(fbi + 1);
	fbi->fb.pseudo_palette	= (void *)(fbi->fb.disp + 1);

	fbi->rgb[RGB_8]		= &rgb_8;
	fbi->rgb[RGB_16]	= &def_rgb_16;

	inf = sa1100fb_get_machine_info(fbi);

	/*
	 * People just don't seem to get this.  We don't support
	 * anything but correct entries now, so panic if someone
	 * does something stupid.
	 */
	if (inf->lccr3 & (LCCR3_VrtSnchL|LCCR3_HorSnchL|0xff) ||
	    inf->pixclock == 0)
		panic("sa1100fb error: invalid LCCR3 fields set or zero "
			"pixclock.");

	fbi->max_xres			= inf->xres;
	fbi->fb.var.xres		= inf->xres;
	fbi->fb.var.xres_virtual	= inf->xres;
	fbi->max_yres			= inf->yres;
	fbi->fb.var.yres		= inf->yres;
	fbi->fb.var.yres_virtual	= inf->yres;
	fbi->max_bpp			= inf->bpp;
	fbi->fb.var.bits_per_pixel	= inf->bpp;
	fbi->fb.var.pixclock		= inf->pixclock;
	fbi->fb.var.hsync_len		= inf->hsync_len;
	fbi->fb.var.left_margin		= inf->left_margin;
	fbi->fb.var.right_margin	= inf->right_margin;
	fbi->fb.var.vsync_len		= inf->vsync_len;
	fbi->fb.var.upper_margin	= inf->upper_margin;
	fbi->fb.var.lower_margin	= inf->lower_margin;
	fbi->fb.var.sync		= inf->sync;
	fbi->fb.var.grayscale		= inf->cmap_greyscale;
	fbi->cmap_inverse		= inf->cmap_inverse;
	fbi->cmap_static		= inf->cmap_static;
	fbi->lccr0			= inf->lccr0;
	fbi->lccr3			= inf->lccr3;
	fbi->state			= C_DISABLE;
	fbi->task_state			= (u_char)-1;
	fbi->fb.fix.smem_len		= fbi->max_xres * fbi->max_yres *
					  fbi->max_bpp / 8;
	fbi->fb.disp->inverse		= inf->cmap_inverse;

	init_waitqueue_head(&fbi->ctrlr_wait);
	INIT_TQUEUE(&fbi->task, sa1100fb_task, fbi);
	init_MUTEX(&fbi->ctrlr_sem);

	return fbi;
}

int __init sa1100fb_init(void)
{
	struct sa1100fb_info *fbi;
	int ret;

	if (!request_mem_region(0xb0100000, 0x10000, "LCD"))
		return -EBUSY;

	fbi = sa1100fb_init_fbinfo();
	ret = -ENOMEM;
	if (!fbi)
		goto failed;

	/* Initialize video memory */
	ret = sa1100fb_map_video_memory(fbi);
	if (ret)
		goto failed;

	ret = request_irq(IRQ_LCD, sa1100fb_handle_irq, SA_INTERRUPT,
			  "LCD", fbi);
	if (ret) {
		printk(KERN_ERR "sa1100fb: request_irq failed: %d\n", ret);
		goto failed;
	}

#ifdef ASSABET_PAL_VIDEO
	if (machine_is_assabet())
		ASSABET_BCR_clear(ASSABET_BCR_LCD_ON);
#endif

#ifdef CONFIG_SA1100_FREEBIRD
#error Please move this into __sa1100fb_lcd_power
	if (machine_is_freebird()) {
		BCR_set(BCR_FREEBIRD_LCD_DISP);
		mdelay(20);
		BCR_set(BCR_FREEBIRD_LCD_PWR);
		mdelay(20);
	}
#endif

	sa1100fb_set_var(&fbi->fb.var, -1, &fbi->fb);

	ret = register_framebuffer(&fbi->fb);
	if (ret < 0)
		goto failed;

#ifdef CONFIG_PM
	/*
	 * Note that the console registers this as well, but we want to
	 * power down the display prior to sleeping.
	 */
	fbi->pm = pm_register(PM_SYS_DEV, PM_SYS_VGA, sa1100fb_pm_callback);
	if (fbi->pm)
		fbi->pm->data = fbi;
#endif
#ifdef CONFIG_CPU_FREQ
	fbi->clockchg.notifier_call = sa1100fb_clkchg_notifier;
	cpufreq_register_notifier(&fbi->clockchg);
#endif

	/*
	 * Ok, now enable the LCD controller
	 */
	set_ctrlr_state(fbi, C_ENABLE);

	/* This driver cannot be unloaded at the moment */
	MOD_INC_USE_COUNT;

	return 0;

failed:
	if (fbi)
		kfree(fbi);
	release_mem_region(0xb0100000, 0x10000);
	return ret;
}

int __init sa1100fb_setup(char *options)
{
#if 0
	char *this_opt;

	if (!options || !*options)
		return 0;

	while ((this_opt = strsep(&options, ",")) != NULL) {

		if (!strncmp(this_opt, "bpp:", 4))
			current_par.max_bpp =
			    simple_strtoul(this_opt + 4, NULL, 0);

		if (!strncmp(this_opt, "lccr0:", 6))
			lcd_shadow.lccr0 =
			    simple_strtoul(this_opt + 6, NULL, 0);
		if (!strncmp(this_opt, "lccr1:", 6)) {
			lcd_shadow.lccr1 =
			    simple_strtoul(this_opt + 6, NULL, 0);
			current_par.max_xres =
			    (lcd_shadow.lccr1 & 0x3ff) + 16;
		}
		if (!strncmp(this_opt, "lccr2:", 6)) {
			lcd_shadow.lccr2 =
			    simple_strtoul(this_opt + 6, NULL, 0);
			current_par.max_yres =
			    (lcd_shadow.
			     lccr0 & LCCR0_SDS) ? ((lcd_shadow.
						    lccr2 & 0x3ff) +
						   1) *
			    2 : ((lcd_shadow.lccr2 & 0x3ff) + 1);
		}
		if (!strncmp(this_opt, "lccr3:", 6))
			lcd_shadow.lccr3 =
			    simple_strtoul(this_opt + 6, NULL, 0);
	}
#endif
	return 0;
}

MODULE_DESCRIPTION("StrongARM-1100/1110 framebuffer driver");
MODULE_LICENSE("GPL");
