/*
 * Allwinner A733 display initialization
 * Inherits framebuffer from U-Boot.
 */
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"

extern void* screeninit(int, int, int);

static int fbwidth = 1920;
static int fbheight = 1080;
static int fbdepth = 32;
static uintptr fbaddr;

void
lcdinit(void)
{
	void *fb;

	if(fbaddr == 0){
		print("lcd: no framebuffer address, serial-only mode\n");
		return;
	}

	fb = vmap(fbaddr, PGROUND(fbwidth * fbheight * (fbdepth/8)));
	if(fb == nil){
		print("lcd: failed to map framebuffer at %#p\n", fbaddr);
		return;
	}

	print("lcd: %dx%dx%d at %#p\n", fbwidth, fbheight, fbdepth, fbaddr);

	if(screeninit(fbwidth, fbheight, fbdepth) == nil)
		print("lcd: screeninit failed\n");
}
