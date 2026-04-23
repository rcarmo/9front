/*
 * Allwinner A733 UART driver
 *
 * The Allwinner UART is 8250/16550-register-compatible, MMIO-mapped.
 * Registers are 32-bit wide at 4-byte-aligned offsets.
 */

#include "u.h"
#include "../port/lib.h"
#include "../port/error.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"

enum {
	/* 8250/16550 register offsets (byte offsets, 4-byte stride) */
	Rbr	= 0x00,		/* Receive Buffer (read) */
	Thr	= 0x00,		/* Transmit Holding (write) */
	Ier	= 0x04,		/* Interrupt Enable */
	Iir	= 0x08,		/* Interrupt Identification (read) */
	Fcr	= 0x08,		/* FIFO Control (write) */
	Lcr	= 0x0C,		/* Line Control */
	Mcr	= 0x10,		/* Modem Control */
	Lsr	= 0x14,		/* Line Status */
	Msr	= 0x18,		/* Modem Status */
	Scr	= 0x1C,		/* Scratch */
	Dll	= 0x00,		/* Divisor Latch Low (DLAB=1) */
	Dlh	= 0x04,		/* Divisor Latch High (DLAB=1) */
	Usr	= 0x7C,		/* UART Status Register (Allwinner extension) */
	Halt	= 0xA4,		/* Halt TX (Allwinner extension) */

	/* Fcr bits */
	FIFOena		= 1<<0,
	FIFOrclr	= 1<<1,
	FIFOtclr	= 1<<2,
	FIFO64		= 1<<5,

	/* Lcr bits */
	Wls5		= 0<<0,
	Wls6		= 1<<0,
	Wls7		= 2<<0,
	Wls8		= 3<<0,
	Stb2		= 1<<2,
	Pen		= 1<<3,
	Eps		= 1<<4,
	Dlab		= 1<<7,

	/* Lsr bits */
	Dr		= 1<<0,		/* Data Ready */
	Thre		= 1<<5,		/* Transmit Holding Register Empty */
	Temt		= 1<<6,		/* Transmitter Empty */

	/* Ier bits */
	Ierbda		= 1<<0,		/* Received Data Available */
	Iethre		= 1<<1,		/* Transmit Holding Register Empty */
	Ierls		= 1<<2,		/* Receiver Line Status */
	Ierms		= 1<<3,		/* Modem Status */

	/* Iir bits */
	Iirip		= 1<<0,		/* Interrupt NOT Pending (active low) */
	Iirrls		= 3<<1,		/* Receiver Line Status */
	Iirbda		= 2<<1,		/* Received Data Available */
	Iirct		= 6<<1,		/* Character Timeout */
	Iirthre		= 1<<1,		/* Transmitter Holding Register Empty */
	Iirms		= 0<<1,		/* Modem Status */
	IirMASK		= 7<<1,

	/* Mcr bits */
	Dtr		= 1<<0,
	Rts		= 1<<1,
	Ie		= 1<<3,		/* Interrupt Enable (active-high on some variants) */
};

typedef struct Ctlr Ctlr;
struct Ctlr {
	uintptr	base;
	int	irq;
	int	iena;
};

static Ctlr a733uart[] = {
	{ .base = UART0, .irq = IRQuart0, },
};

#define	wr(c, r, v)	*IO(u32int, (c)->base + (r)) = (v)
#define	rd(c, r)	*IO(u32int, (c)->base + (r))

static void
awuartkick(Uart *u)
{
	Ctlr *c = u->regs;

	if(u->blocked)
		return;

	while(u->op < u->oe){
		if(!(rd(c, Lsr) & Thre))
			break;
		wr(c, Thr, *u->op++);
	}

	if(u->op >= u->oe){
		/* all data sent, disable TX interrupt */
		wr(c, Ier, rd(c, Ier) & ~Ierthre);
	} else {
		/* more data, enable TX interrupt */
		wr(c, Ier, rd(c, Ier) | Ierthre);
	}
}

static void
awuartintr(Ureg*, void *arg)
{
	Uart *u = arg;
	Ctlr *c = u->regs;
	u32int iir, lsr;

	for(;;){
		iir = rd(c, Iir);
		if(iir & Iirip)	/* no interrupt pending */
			break;

		switch(iir & IirMASK){
		case Iirbda:
		case Iirct:
			while(rd(c, Lsr) & Dr)
				uartrecv(u, rd(c, Rbr));
			break;

		case Iirthre:
			uartkick(u);
			break;

		case Iirrls:
			lsr = rd(c, Lsr);
			USED(lsr);
			break;

		case Iirms:
			rd(c, Msr);
			break;
		}
	}
}

static Uart*
awuartpnp(void)
{
	return &a733uart[0].uart;
}

static void
awuartenable(Uart *u, int ie)
{
	Ctlr *c = u->regs;

	/* 8N1, FIFO enabled */
	wr(c, Lcr, Wls8);
	wr(c, Fcr, FIFOena | FIFOrclr | FIFOtclr);
	wr(c, Mcr, Ie | Rts | Dtr);

	if(ie){
		if(!c->iena){
			intrenable(c->irq, awuartintr, u, BUSUNKNOWN, u->name);
			c->iena = 1;
		}
		wr(c, Ier, Ierbda | Ierls);
	}
}

static int
awuartgetc(Uart *u)
{
	Ctlr *c = u->regs;

	while(!(rd(c, Lsr) & Dr))
		;
	return rd(c, Rbr);
}

static void
awuartputc(Uart *u, int ch)
{
	Ctlr *c = u->regs;

	while(!(rd(c, Lsr) & Thre))
		;
	wr(c, Thr, ch);
}

void
uartconsinit(void)
{
	Uart *u;
	Ctlr *c = &a733uart[0];

	if((u = consuart) == nil){
		consuart = u = &(c->uart);
		u->regs = c;
		u->name = "uart0";
		u->freq = 0;
		u->phys = nil;
		u->console = 1;
	}
}

void
uartawlink(void)
{
	static PhysUart phys = {
		.name		= "awuart",
		.pnp		= awuartpnp,
		.enable		= awuartenable,
		.kick		= awuartkick,
		.getc		= awuartgetc,
		.putc		= awuartputc,
	};
	Ctlr *c;
	Uart *u;

	c = &a733uart[0];
	u = &(c->uart);
	u->regs = c;
	u->name = "uart0";
	u->phys = &phys;
	u->console = 1;
	u->special = 0;
	u->next = nil;
	consuart = u;

	addphysuart(&phys);
}
