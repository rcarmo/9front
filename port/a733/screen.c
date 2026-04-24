#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

#define	Image	IMAGE
#include <draw.h>
#include <memdraw.h>
#include <cursor.h>
#include "screen.h"

typedef struct Cursor Cursor;
extern void swcursorhide(int);
extern void swcursoravoid(Rectangle);
extern void swcursordraw(Point);
extern void swcursorload(Cursor*);
extern void swcursorinit(void);

Memimage *gscreen;

static Memsubfont *defont;
static Point	curpos;
static Rectangle window;
static Lock screenlock;
static uchar	*fbraw;

void
flushmemscreen(Rectangle r)
{
	uchar *s, *d;
	int n, pitch;

	if(fbraw == nil || gscreen == nil)
		return;
	if(!rectclip(&r, gscreen->r))
		return;

	s = (uchar*)wordaddr(gscreen, r.min);
	d = fbraw + (s - (uchar*)wordaddr(gscreen, gscreen->r.min));
	n = bytesperline(r, gscreen->depth);
	pitch = wordsperline(gscreen->r, gscreen->depth);
	while(r.min.y++ < r.max.y){
		memmove(d, s, n);
		d += pitch * sizeof(ulong);
		s += pitch * sizeof(ulong);
	}
}

static void
screenwin(void)
{
	Point p;

	window = insetrect(gscreen->r, 8);
	memimagedraw(gscreen, gscreen->r, memwhite, ZP, memopaque, ZP, S);
	memimagedraw(gscreen, window, memblack, ZP, memopaque, ZP, S);
	window = insetrect(window, 4);
	memimagedraw(gscreen, window, memwhite, ZP, memopaque, ZP, S);

	p.x = window.min.x + 8;
	p.y = window.min.y + 8;
	memimagestring(gscreen, p, memblack, ZP, defont, "Plan 9 (Orange Pi 4 Pro)");

	curpos.x = window.min.x + 8;
	curpos.y = window.min.y + 8 + defont->height * 2;
	flushmemscreen(gscreen->r);
}

static void
myscreenputs(char *s, int n)
{
	int locked;

	locked = canlock(&screenlock);
	while(n > 0){
		if(*s == '\n'){
			curpos.x = window.min.x;
			curpos.y += defont->height;
			if(curpos.y + defont->height > window.max.y){
				curpos.y -= defont->height;
				memimagedraw(gscreen,
					Rect(window.min.x, window.min.y,
						window.max.x, window.max.y - defont->height),
					gscreen,
					Pt(window.min.x, window.min.y + defont->height),
					nil, ZP, S);
				memimagedraw(gscreen,
					Rect(window.min.x, window.max.y - defont->height,
						window.max.x, window.max.y),
					memwhite, ZP, nil, ZP, S);
			}
			s++;
			n--;
			continue;
		}
		if(*s == '\t'){
			s++;
			n--;
			curpos.x += 4 * defont->info[' '].width;
			continue;
		}
		memimagestring(gscreen, curpos, memblack, ZP, defont, s);
		curpos.x += defont->info[(uchar)*s].width;
		s++;
		n--;
	}
	flushmemscreen(window);
	if(locked)
		unlock(&screenlock);
}

void*
screeninit(int width, int height, int depth)
{
	ulong chan;

	if(depth == 32)
		chan = XRGB32;
	else if(depth == 16)
		chan = RGB16;
	else
		return nil;

	memimageinit();
	gscreen = allocmemimage(Rect(0, 0, width, height), chan);
	if(gscreen == nil)
		return nil;

	conf.monitor = 1;
	fbraw = xspanalloc(PGROUND(gscreen->width*sizeof(ulong)*height), BY2PG, 0);

	defont = getmemdefont();
	screenwin();
	myscreenputs(kmesg.buf, kmesg.n);
	screenputs = myscreenputs;
	swcursorinit();

	return fbraw;
}

void
getcolor(ulong, ulong *r, ulong *g, ulong *b)
{
	*r = *g = *b = 0;
}

int
setcolor(ulong, ulong, ulong, ulong)
{
	return 0;
}

Memdata*
attachscreen(Rectangle *r, ulong *d, int *width, ulong *chan)
{
	if(gscreen == nil)
		return nil;
	*r = gscreen->r;
	*d = gscreen->depth;
	*chan = gscreen->chan;
	*width = gscreen->width;
	gscreen->data->ref++;
	return gscreen->data;
}

void
mousectl(Cmdbuf *cb)
{
	USED(cb);
}

void
cursoron(void)
{
	swcursorhide(0);
	swcursordraw(curpos);
}

void
cursoroff(void)
{
	swcursorhide(0);
}

void
setcursor(Cursor *curs)
{
	swcursorload(curs);
}

void
blankscreen(int blank)
{
	USED(blank);
}
