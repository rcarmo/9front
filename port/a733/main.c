/*
 * Allwinner A733 (Orange Pi 4 Pro) - board initialization
 *
 * Based on arm64/main.c and lx2k/main.c
 */
#include "u.h"
#include "tos.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "pool.h"
#include "io.h"

Conf conf;

/*
 * Option arguments from the command line.
 * oargv[0] is the boot file.
 * Arguments to the kernel are passed by U-Boot
 * after the kernel image is loaded.
 */
static int oargc;
static char* oargv[20];
static char oargb[128];
static int oargblen;

static uintptr sp;	/* user stack of init proc */

Memcache cachel[8];	/* filled in by l.s */
int ncaches;

/*
 * I/O mappings for the SoC peripherals.
 * These are set up early so that we can use
 * the UART for console output during boot.
 */
void
machinit(void)
{
	m->machno = 0;
	m->ticks = 0;
	m->perf.period = 1;

	active.machs[m->machno] = 1;
	active.exiting = 0;
}

/*
 * Map the SoC I/O space.
 * Called before first use of any device.
 */
void
archinit(void)
{
	/*
	 * Allwinner A733 I/O region: 0x01000000 - 0x07FFFFFF
	 * Map the entire region as device memory.
	 */
	vmap(0x01000000, 0x07000000);

	/*
	 * Map PCIe config/memory space.
	 * PCIe DBI: 0x06000000, size 0x480000
	 * PCIe memory: 0x20000000 - 0x27FFFFFF
	 */
	vmap(0x20000000, 0x08000000);
}

void
main(void)
{
	machinit();
	quotefmtinstall();
	confinit();
	uartconsinit();

	screeninit();

	print("\nPlan 9 from Bell Labs\n");
	print("Allwinner A733 (Orange Pi 4 Pro)\n");

	trapinit();
	archinit();

	timersinit();
	cpuidprint();

	procinit0();
	initseg();
	links();
	chandevreset();

	pageinit();
	userinit();
	schedinit();
}

void
init0(void)
{
	char buf[2*KNAMELEN];

	up->nerrlab = 0;
	spllo();

	/*
	 * These are o.k. because rootponents is shared.
	 */
	chandevinit();

	if(!waession()){
		snprint(buf, sizeof(buf), "%s %s", "sys", sysname);
		ksetenv("terminal", buf, 0);
		ksetenv("cputype", "arm64", 0);
		if(cpuserver)
			ksetenv("service", "cpu", 0);
		else
			ksetenv("service", "terminal", 0);
	}
	kproc("alarm", alarmkproc, 0);
	touser(sp);
}

static void
bootargs(void *base)
{
	USED(base);

	/* populated by bootargs.c from U-Boot */
}

void
confinit(void)
{
	ulong pa;

	conf.nmach = 1;	/* TODO: SMP bringup */

	/*
	 * A733 DRAM starts at 0x40000000.
	 * We don't know the exact size yet — conservatively
	 * assume 2 GB for initial bring-up.
	 */
	pa = PHYSDRAM;
	conf.mem[0].base = pa;
	conf.mem[0].limit = pa + 2*GiB;
	conf.mem[0].npage = (conf.mem[0].limit - conf.mem[0].base) / BY2PG;
	conf.npage = conf.mem[0].npage;
	conf.nproc = 100 + ((conf.npage*BY2PG)/MiB)*5;
	if(conf.nproc > 2000)
		conf.nproc = 2000;
	conf.ialloc = conf.nproc*3;
	conf.pipeqsize = 32*1024;
}

void
userinit(void)
{
	Proc *p;
	Segment *s;
	KMap *k;
	Page *pg;

	p = newproc();
	p->pgrp = newpgrp();
	p->egrp = smalloc(sizeof(Egrp));
	p->egrp->ref = 1;
	p->fgrp = dupfgrp(nil);
	p->rgrp = newrgrp();
	p->procmode = 0640;

	kstrdup(&eve, "eve");
	kstrdup(&p->text, "*init*");
	kstrdup(&p->user, eve);

	/*
	 * Kernel Stack
	 */
	p->sched.pc = (uintptr)init0;
	p->sched.sp = (uintptr)p->kstack+KSTACK-(sizeof(Sargs)+BY2WD);
	p->sched.sp = STACKALIGN(p->sched.sp);

	/*
	 * User Stack
	 */
	s = newseg(SG_STACK, USTKTOP-USTKSIZE, USTKSIZE/BY2PG);
	p->seg[SSEG] = s;

	/*
	 * Text
	 */
	s = newseg(SG_TEXT, UTZERO, 1);
	s->flushme++;
	p->seg[TSEG] = s;
	pg = newpage(1, 0, UTZERO);
	segpage(s, pg);
	k = kmap(pg);
	memset((void*)VA(k), 0, BY2PG);
	memmove((void*)VA(k), initcode, sizeof initcode);
	kunmap(k);

	ready(p);
}

void
cpuidprint(void)
{
	print("cpu%d: %dMHz ARM Cortex\n", m->machno, m->cpumhz);
}

void
mpinit(void)
{
	/* TODO: multi-processor bringup via PSCI */
}

void
reboot(void*, void*, ulong)
{
}

void
exit(int)
{
	splhi();
	for(;;) idlehands();
}
