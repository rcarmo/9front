#include "u.h"
#include "tos.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "pool.h"
#include "io.h"
#include "../arm64/sysreg.h"
#include "ureg.h"

#include "rebootcode.i"

Conf conf;

static void
earlymark(int ch)
{
	volatile u32int *r;

	r = (u32int*)IOADDR(UART0);
	while((r[0x14/4] & (1<<5)) == 0)
		;
	r[0x00/4] = ch;
}

int
isaconfig(char *, int, ISAConf *)
{
	return 0;
}

void
init0(void)
{
	char buf[2*KNAMELEN], **sp, **ksp;
	uintptr usp;
	Page *p;
	KMap *k;

	earlymark('n');
	chandevinit();
	earlymark('o');

	if(!waserror()){
		snprint(buf, sizeof(buf), "%s %s", "ARM64", conffile);
		ksetenv("terminal", buf, 0);
		ksetenv("cputype", "arm64", 0);
		if(cpuserver)
			ksetenv("service", "cpu", 0);
		else
			ksetenv("service", "terminal", 0);
		setconfenv();
		poperror();
	}
	earlymark('A');
	/*
	 * Temporary bring-up simplification: skip the alarm kproc until first user
	 * entry works reliably. The current trace shows init0() reaching 'o' and then
	 * falling into heavy scheduler churn before 'p'; removing this extra kernel
	 * process lets us distinguish an alarm/clock interaction from a plain init0
	 * setup bug.
	 */
	/* kproc("alarm", alarmkproc, 0); */
	earlymark('B');

	/*
	 * Temporary bring-up workaround: writing directly to the first user stack
	 * page from EL1 is currently fault-looping before the first instruction in
	 * user mode. Seed that top stack page explicitly through its kernel alias so
	 * touser() can run even while the kernel-on-user-fault path is still under
	 * investigation.
	 */
	usp = USTKTOP-sizeof(Tos) - 8 - sizeof(sp[0])*4;
	sp = (char**)usp;
	earlymark('C');
	p = fillpage(newpage(USTKTOP-BY2PG, nil), 0);
	segpage(up->seg[SSEG], p);
	k = kmap(p);
	ksp = (char**)((uintptr)VA(k) + (usp & (BY2PG-1)));
	ksp[3] = ksp[2] = ksp[1] = nil;
	earlymark('D');
	strcpy(ksp[1] = (char*)&ksp[4], "boot");
	earlymark('E');
	ksp[0] = (void*)&sp[1];
	kunmap(k);
	earlymark('F');
	/*
	 * Bypass the currently-suspect user-fault return path for bring-up by
	 * faulting in both the first initcode instruction page and the top stack/TOS
	 * page before the first ERET to EL0. If pid1 then reaches syscalls, the
	 * remaining bug is specifically in resuming from the first user fault.
	 */
	if(fault(0x10028, 0x10028, 1) < 0)
		panic("init0: prefault text");
	earlymark('G');
	if(fault(usp, usp, 0) < 0)
		panic("init0: prefault stack top");
	earlymark('H');
	/*
	 * init9/startboot immediately builds a deeper stack frame below usp. Prefault
	 * the lower part of that same first stack page too so we can distinguish a
	 * genuine resume-path bug from a second early EL0 stack write fault.
	 */
	if(fault(usp-0x100, usp-0x100, 0) < 0)
		panic("init0: prefault stack mid");
	if(fault(usp-0x200, usp-0x200, 0) < 0)
		panic("init0: prefault stack low");
	earlymark('I');

	splhi();
	fpukexit(nil);
	earlymark('p');
	touser((uintptr)sp);
}

void
confinit(void)
{
	int userpcnt;
	ulong kpages;
	char *p;
	int i;

	conf.nmach = 1;
	if(p = getconf("*ncpu"))
		conf.nmach = strtol(p, 0, 0);
	if(conf.nmach > MAXMACH)
		conf.nmach = MAXMACH;

	if(p = getconf("service")){
		if(strcmp(p, "cpu") == 0)
			cpuserver = 1;
		else if(strcmp(p,"terminal") == 0)
			cpuserver = 0;
	}

	if(p = getconf("*kernelpercent"))
		userpcnt = 100 - strtol(p, 0, 0);
	else
		userpcnt = 0;

	if(userpcnt < 10)
		userpcnt = 60 + cpuserver*10;

	conf.npage = 0;
	for(i = 0; i < nelem(conf.mem); i++)
		conf.npage += conf.mem[i].npage;

	kpages = conf.npage - (conf.npage*userpcnt)/100;
	if(kpages > ((uintptr)-VDRAM)/BY2PG)
		kpages = ((uintptr)-VDRAM)/BY2PG;

	conf.upages = conf.npage - kpages;
	conf.ialloc = (kpages/2)*BY2PG;

	conf.nproc = 100 + ((conf.npage*BY2PG)/MB)*5;
	if(cpuserver)
		conf.nproc *= 3;
	if(conf.nproc > 4000)
		conf.nproc = 4000;
	conf.nswap = conf.npage*3;
	conf.nswppo = 4096;
	conf.nimage = 200;

	conf.copymode = conf.nmach > 1;

	kpages = conf.npage - conf.upages;
	kpages *= BY2PG;
	kpages -= conf.upages*sizeof(Page)
		+ conf.nproc*sizeof(Proc*)
		+ conf.nimage*sizeof(Image)
		+ conf.nswap
		+ conf.nswppo*sizeof(Page*);
	mainmem->maxsize = kpages;
	imagmem->maxsize = kpages;
}

void
machinit(void)
{
	m->ticks = 1;
	m->perf.period = 1;
	active.machs[m->machno] = 1;
}

void
mpinit(void)
{
	extern void _start(void);
	int i;

	for(i = 1; i < conf.nmach; i++){
		Ureg u = {0};

		MACHP(i)->machno = i;
		cachedwbinvse(MACHP(i), MACHSIZE);

		u.r0 = 0x84000003;	/* CPU_ON */
		u.r1 = (sysrd(MPIDR_EL1) & ~(0xFF0000FFULL)) | i;
		u.r2 = PADDR(_start);
		u.r3 = i;
		hvccall(&u);
	}
	synccycles();
}

void
cpuidprint(void)
{
	iprint("cpu%d: Allwinner A733\n", m->machno);
}

void
main(uintptr dtbpa)
{
	earlymark('H');
	setbootdtb(dtbpa);
	earlymark('I');
	machinit();
	earlymark('J');
	if(m->machno){
		trapinit();
		fpuinit();
		intrinit();
		clockinit();
		cpuidprint();
		synccycles();
		timersinit();
		mmu1init();
		m->ticks = MACHP(0)->ticks;
		schedinit();
	}
	earlymark('K');
	uartconsinit();
	earlymark('L');
	quotefmtinstall();
	earlymark('M');
	bootargsinit();
	earlymark('N');
	meminit();
	earlymark('O');
	confinit();
	earlymark('P');
	xinit();
	earlymark('Q');
	printinit();
	earlymark('R');
	print("\nPlan 9\n");
	earlymark('S');
	trapinit();
	earlymark('T');
	fpuinit();
	/*
	 * Follow the Linux early-init sequencing more closely: keep IRQ/time setup
	 * out of the deep bootstrap window and only start it after page/proc/chan
	 * bootstrap is done. Also re-mask async abort/debug after trapinit(), since
	 * trapinit() itself restores the normal arm64 runtime DAIF state.
	 */
	splx(0xF<<6);
	earlymark('Z');
	pageinitwrap();
	procinit0();
	earlymark('r');
	initseg();
	earlymark('s');
	links();
	earlymark('1');
	intrinit();
	earlymark('2');
	clockinit();
	earlymark('3');
	cpuidprint();
	earlymark('4');
	timersinit();
	earlymark('t');
	/*
	 * Temporary A733 bring-up bypass: chandevreset() currently trips a null
	 * function-pointer call inside device-reset plumbing before the first user
	 * process is created. Skip it for now so we can keep pushing bootstrap
	 * forward, then come back and isolate the offending device reset path.
	 */
	userinitwrap();
	earlymark('v');
	mpinit();
	earlymark('w');
	mmu1init();
	earlymark('x');
	/*
	 * Keep IRQ/FIQ masked until the first kproc reaches linkproc(), which
	 * drops to spllo() itself. Unmasking here can take a timer interrupt while
	 * up == nil, and that bootstrap IRQ path currently panics before the first
	 * scheduler handoff completes.
	 */
	splx(0x3<<6);
	earlymark('y');
	schedinit();
}

void
exit(int)
{
	Ureg u = { .r0 = 0x84000002 };	/* CPU_OFF */

	cpushutdown();
	splfhi();

	if(m->machno == 0){
		zeroprivatepages();
		poolreset(secrmem);

		u.r0 = 0x84000009;	/* SYSTEM RESET */
	}
	hvccall(&u);
}

static void
rebootjump(void *entry, void *code, ulong size)
{
	void (*f)(void*, void*, ulong);

	intrcpushutdown();
	setttbr(PADDR(L1BOT));
	f = (void*)REBOOTADDR;
	memmove(f, rebootcode, sizeof(rebootcode));

	cachedwbinvse(f, sizeof(rebootcode));
	cacheiinvse(f, sizeof(rebootcode));

	(*f)(entry, code, size);

	for(;;);
}

void
reboot(void*, void *code, ulong size)
{
	writeconf();
	while(m->machno != 0){
		procwired(up, 0);
		sched();
	}

	cpushutdown();
	delay(2000);

	splfhi();
	serialoq = nil;
	chandevshutdown();
	clockshutdown();
	intrsoff();
	zeroprivatepages();
	poolreset(secrmem);
	rebootjump((void*)(KTZERO-KZERO), code, size);
}

void
dmaflush(int clean, void *p, ulong len)
{
	uintptr s = (uintptr)p;
	uintptr e = (uintptr)p + len;

	if(clean){
		s &= ~(BLOCKALIGN-1);
		e += BLOCKALIGN-1;
		e &= ~(BLOCKALIGN-1);
		cachedwbse((void*)s, e - s);
		return;
	}
	if(s & BLOCKALIGN-1){
		s &= ~(BLOCKALIGN-1);
		cachedwbinvse((void*)s, BLOCKALIGN);
		s += BLOCKALIGN;
	}
	if(e & BLOCKALIGN-1){
		e &= ~(BLOCKALIGN-1);
		if(e < s)
			return;
		cachedwbinvse((void*)e, BLOCKALIGN);
	}
	if(s < e)
		cachedinvse((void*)s, e - s);
}
