#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

/*
 * INITMAP is the amount of DRAM that must be reachable through the initial
 * high-half mappings before the regular meminit()/kmapram path runs.
 */
#define INITMAP	(ROUND((uintptr)end + BY2PG, PGLSZ(1))-KZERO)

/*
 * Create initial identity map in top-level page table
 * (L1BOT) for TTBR0. This page table is only used until
 * mmu1init() loads m->mmutop.
 *
 * Audit note: L1BOT itself is only one page, so a child table cannot live at
 * L1BOT+BY2PG without colliding with the shared L1 region. We therefore use a
 * dedicated low DRAM scratch page just above REBOOTADDR.
 *
 * Layout used here:
 *   - entry 0 -> child table for low MMIO as device memory
 *   - entry 1 -> 1GB block for low DRAM at 0x40000000
 */
void
mmuidmap(uintptr *l1bot)
{
	uintptr *l0;
	uintptr pa, pe, attr;

	/*
	 * Entry 0 uses a child table for low SoC MMIO so UART/GIC accesses keep
	 * device attributes during the fragile post-MMU transition.
	 *
	 * Do not alias this with the TTBR1 root page at L1TOP: Linux keeps separate
	 * idmap and swapper structures, and sharing one physical page for both table
	 * roles makes the audit harder. Use a dedicated scratch page instead.
	 */
	l0 = (uintptr*)IDMAPL0ADDR;
	memset(l0, 0, BY2PG);
	l1bot[PTLX(0, PTLEVELS-1)] = (uintptr)l0 | PTEVALID | PTETABLE;

	attr = PTEWRITE | PTEAF | PTEKERNEL | PTEUXN | PTEPXN | PTESH(SHARE_OUTER) | PTEDEVICE;
	for(pa = PHYSIO; pa < PHYSIOEND; pa += PGLSZ(1))
		l0[PTLX(pa, 1)] = pa | PTEVALID | PTEBLOCK | attr;

	/*
	 * High-half identity blocks for DRAM. With EVASHIFT=39 this now matches the
	 * saved Linux image's 4KB/3-level/39-bit-VA strategy much more closely than
	 * the earlier 36-bit layout.
	 */
	attr = PTEWRITE | PTEAF | PTEKERNEL | PTEUXN | PTESH(SHARE_INNER);
	pe = -KZERO;
	for(pa = VDRAM - KZERO; pa < pe; pa += PGLSZ(PTLEVELS-1))
		l1bot[PTLX(pa, PTLEVELS-1)] = pa | PTEVALID | PTEBLOCK | attr;
}

/*
 * Create initial shared kernel page table (L1) for TTBR1.
 * This page table covers the direct-map DRAM window and VIRTIO/SoC MMIO.
 */
void
mmu0init(uintptr *l1)
{
	uintptr va, pa, pe, attr;

	/*
	 * Direct-map the full KADDR()-reachable low DRAM window up front, not just
	 * INITMAP. pageinit() allocates and zeroes a large Page array through xalloc,
	 * and xallocz() dereferences KADDR(pa) for the entire kernel hole. With only
	 * INITMAP mapped, that memset walks straight off the mapped high-half window
	 * and faults during pageinit(). Linux's early linear map is broad enough that
	 * this class of allocation works before the generic page allocator is fully
	 * online, so match that model here.
	 */
	attr = PTEWRITE | PTEAF | PTEKERNEL | PTEUXN | PTESH(SHARE_INNER);
	pe = -KZERO;
	for(pa = VDRAM - KZERO, va = VDRAM; pa < pe; pa += PGLSZ(1), va += PGLSZ(1))
		l1[PTL1X(va, 1)] = pa | PTEVALID | PTEBLOCK | attr;

	/* SoC MMIO window */
	attr = PTEWRITE | PTEAF | PTEKERNEL | PTEUXN | PTEPXN | PTESH(SHARE_OUTER) | PTEDEVICE;
	pe = PHYSIOEND;
	for(pa = PHYSIO, va = VIRTIO; pa < pe; pa += PGLSZ(1), va += PGLSZ(1)){
		if(((pa|va) & PGLSZ(1)-1) != 0){
			l1[PTL1X(va, 1)] = (uintptr)l1 | PTEVALID | PTETABLE;
			for(; pa < pe && ((va|pa) & PGLSZ(1)-1) != 0; pa += PGLSZ(0), va += PGLSZ(0)){
				assert(l1[PTLX(va, 0)] == 0);
				l1[PTLX(va, 0)] = pa | PTEVALID | PTEPAGE | attr;
			}
			break;
		}
		l1[PTL1X(va, 1)] = pa | PTEVALID | PTEBLOCK | attr;
	}

	if(PTLEVELS > 2)
	for(va = KSEG0; va != 0; va += PGLSZ(2))
		l1[PTL1X(va, 2)] = (uintptr)&l1[L1TABLEX(va, 1)] | PTEVALID | PTETABLE;

	if(PTLEVELS > 3)
	for(va = KSEG0; va != 0; va += PGLSZ(3))
		l1[PTL1X(va, 3)] = (uintptr)&l1[L1TABLEX(va, 2)] | PTEVALID | PTETABLE;
}

void
meminit(void)
{
	char *p;
	uintptr l = GiB + 128 * MiB;

	if(p = getconf("*maxmem"))
		l = strtoull(p, 0, 0);
	conf.mem[0].base = PGROUND((uintptr)end - KZERO);
	conf.mem[0].limit = l;

	if(l > KLIMIT)
		l = KLIMIT;
	kmapram(conf.mem[0].base, l);

	conf.mem[0].npage = (conf.mem[0].limit - conf.mem[0].base)/BY2PG;
}
