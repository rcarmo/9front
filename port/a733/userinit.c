#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"
#include	"../port/error.h"

static void
usermark(int ch)
{
	volatile u32int *r;

	r = (u32int*)IOADDR(UART0);
	while((r[0x14/4] & (1<<5)) == 0)
		;
	r[0x00/4] = ch;
}

/*
 * The initcode array contains the binary text of the first
 * user process. Its job is to invoke the exec system call
 * for /boot/boot.
 * Initcode does not link with standard plan9 libc _main()
 * trampoline due to size constrains. Instead it is linked
 * with a small machine specific trampoline init9.s that
 * only sets the base address register and passes arguments
 * to startboot() (see port/initcode.c).
 */
#include	"initcode.i"

/*
 * The first process kernel process starts here.
 */
static void
proc0(void*)
{
	KMap *k;
	Page *p;
	u32int *iw;

	usermark('l');
	spllo();
	if(waserror())
		panic("proc0: %s", up->errstr);

	up->pgrp = newpgrp();
	up->egrp = smalloc(sizeof(Egrp));
	up->egrp->ref = 1;
	up->fgrp = dupfgrp(nil);
	up->rgrp = newrgrp();

	/*
	 * These are o.k. because rootinit is null.
	 * Then early kproc's will have a root and dot.
	 */
	up->slash = namec("#/", Atodir, 0, 0);
	pathclose(up->slash->path);
	up->slash->path = newpath("/");
	up->dot = cclone(up->slash);

	/*
	 * Setup Text and Stack segments for initcode.
	 */
	up->seg[SSEG] = newseg(SG_STACK | SG_NOEXEC, USTKTOP-USTKSIZE, USTKSIZE / BY2PG);
	up->seg[TSEG] = newseg(SG_TEXT | SG_RONLY, UTZERO, 1);
	up->seg[TSEG]->flushme = 1;
	p = newpage(UTZERO, nil);
	k = kmap(p);
	memmove((uchar*)VA(k), initcode, sizeof(initcode));
	iw = (u32int*)((uchar*)VA(k) + 0x20);
	iprint("initcode size %ud words20 %.8ux %.8ux %.8ux %.8ux %.8ux %.8ux %.8ux %.8ux\n",
		sizeof(initcode), iw[0], iw[1], iw[2], iw[3], iw[4], iw[5], iw[6], iw[7]);
	memset((uchar*)VA(k)+sizeof(initcode), 0, BY2PG-sizeof(initcode));
	kunmap(k);
	segpage(up->seg[TSEG], p);

	/*
	 * Become a user process.
	 */
	up->kp = 0;
	up->noswap = 0;
	up->privatemem = 0;
	procpriority(up, PriNormal, 0);
	procsetup(up);

	flushmmu();
	usermark('m');

	poperror();

	/*
	 * init0():
	 *	call chandevinit()
	 *	setup environment variables
	 *	prepare the stack for initcode
	 *	switch to usermode to run initcode
	 */
	init0();

	/* init0 will never return */
	panic("init0");
}

void
userinit(void)
{
	usermark('i');
	up = nil;
	kstrdup(&eve, "");
	usermark('I');
	kproc("*init*", proc0, nil);
	usermark('J');
}
