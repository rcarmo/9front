/*
 * Memory and machine-specific definitions.  Used in C and assembler.
 *
 * Allwinner A733 (sun60iw2) - Orange Pi 4 Pro
 */
#define KiB		1024u			/* Kibi 0x0000000000000400 */
#define MiB		1048576u		/* Mebi 0x0000000000100000 */
#define GiB		1073741824u		/* Gibi 000000000040000000 */

/*
 * Sizes
 */
#define	BY2PG		(4*KiB)			/* bytes per page */
#define	BY2SE		(2*MiB)			/* bytes per section */
#define	PGSHIFT		12			/* log(BY2PG) */
#define	SESHIFT		21			/* log(BY2SE) */
#define	PGROUND(s)	ROUND(s, BY2PG)
#define	ROUND(s, sz)	(((s)+(sz-1))&~(sz-1))

#define	MAXMACH		8			/* max cpus */
#define	MACHSIZE	(8*KiB)

/*
 * Address space layout
 *
 * The Allwinner A733 boots with DRAM at 0x40000000.
 * Peripherals are mapped at 0x02000000-0x07ffffff.
 *
 * Virtual memory layout (from arm64/mem.h):
 *   KZERO is where the kernel virtual address space starts
 *   Physical memory is mapped at KZERO + physical address
 */
#define KZERO		0xFFFFFFFF80000000ULL	/* kernel address space */
#define KDZERO		0xFFFFFFFF80000000ULL	/* kernel data space */

#define UZERO		0x0ULL			/* user space starts */
#define UTZERO		(UZERO+0x10000)		/* user text start */
#define USTKTOP		0x0000007FC0000000ULL	/* user stack top */
#define USTKSIZE	(16*1024*1024)		/* user stack size */

#define KSEG0		0xFFFFFFFF00000000ULL	/* cached, unmapped */
#define VDMA		0xFFFFFFFE00000000ULL	/* uncached, unmapped (device I/O) */

#define	VIRTIO		0xFFFFFFFE00000000ULL	/* device mappings (virtual) */

/*
 * Physical memory
 */
#define	PHYSDRAM	0x40000000		/* DRAM base */

/*
 * Allwinner A733 peripheral base addresses
 */
#define	UART0		0x02500000		/* UART0 (debug console) */
#define	UART1		0x02501000
#define	UART2		0x02502000

#define	GICD		0x03400000		/* GIC Distributor */
#define	GICR		0x03460000		/* GIC Redistributor */

#define	PCIE		0x06000000		/* PCIe DBI base */

#define	GMAC0		0x04500000		/* Ethernet MAC 0 */
#define	GMAC1		0x04510000		/* Ethernet MAC 1 */

#define	XHCI		0x06A00000		/* USB3 XHCI */

/*
 * MMU
 */
#define	PTEPERTAB	(512)

#define	PTEMAPMEM	(PTEPERTAB*BY2PG)
#define	PTEPERTAB	(512)
#define	SEGMAPSIZE	8192
#define	SSEGMAPSIZE	16

#define	L1SIZE		(PTEPERTAB*8)	/* 4096 */
#define	L1TABLEX	(L1SIZE/BY2PG)
#define	L1TABLE		(4*BY2PG)
#define	L1TABLES	4

/*
 * Magic registers
 */
#define	CNTFRQ		0			/* timer frequency (set at runtime) */

/*
 * PSR bits
 */
#define PsrDirq		(1<<9)		/* Disable IRQ */
#define PsrDfiq		(1<<8)		/* Disable FIQ */
#define PsrA		(1<<7)		/* Disable imprecise abort */
#define PsrMel0t	0x0		/* EL0, SP_EL0, Aarch64 */
#define PsrMel1t	0x4		/* EL1, SP_EL0, Aarch64 */
#define PsrMel1h	0x5		/* EL1, SP_EL1, Aarch64 */
#define PsrMel2t	0x8		/* EL2, SP_EL2, Aarch64 */
#define PsrMel2h	0x9		/* EL2, SP_EL2, Aarch64 */
