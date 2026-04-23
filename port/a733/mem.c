/*
 * Allwinner A733 memory setup
 */
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

void
meminit(void)
{
	/*
	 * A733 DRAM at physical 0x40000000.
	 * Exact size determined at runtime from U-Boot handoff
	 * or by probing. For now, use 2GB.
	 */
	conf.mem[0].base = PHYSDRAM;
	conf.mem[0].limit = PHYSDRAM + 2*GiB;
	conf.mem[0].npage = (conf.mem[0].limit - conf.mem[0].base) / BY2PG;
	conf.npage = conf.mem[0].npage;
}
