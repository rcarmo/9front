/*
 * Allwinner A733 UART driver (8250-compatible, MMIO)
 *
 * Modeled on arm64/uartqemu.c (PL011 pattern).
 */
#include "u.h"
#include "../port/lib.h"
#include "../port/error.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"

enum {
	/* 8250 register indices (byte offset / 4) */
	Rbr	= 0x00/4,	/* RX buffer / TX holding */
	Ier	= 0x04/4,	/* Interrupt Enable */
	Iir	= 0x08/4,	/* Interrupt ID (read) / FIFO Control (write) */
	Lcr	= 0x0C/4,	/* Line Control */
	Mcr	= 0x10/4,	/* Modem Control */
	Lsr	= 0x14/4,	/* Line Status */
	Msr	= 0x18/4,	/* Modem Status */
	Scr	= 0x1C/4,	/* Scratch */

	/* Lcr */
	Wls8	= 3<<0,

	/* Lsr */
	Dr	= 1<<0,
	Thre	= 1<<5,

	/* Ier */
	Ierbda	= 1<<0,
	Ierthre	= 1<<1,
	Ierls	= 1<<2,

	/* Iir */
	Iirip	= 1<<0,

	/* Fcr (write to Iir offset) */
	FIFOena	= 1<<0,
	FIFOrclr= 1<<1,
	FIFOtclr= 1<<2,

	/* Mcr */
	Dtr	= 1<<0,
	Rts	= 1<<1,
};

extern PhysUart awphysuart;

static Uart awuart = {
	.regs	= (void*)IOADDR(UART0),
	.name	= "uart0",
	.freq	= 0,
	.phys	= &awphysuart,
	.console= 1,
	.special= 0,
};

static Uart*
pnp(void)
{
	return &awuart;
}

static void
kick(Uart *u)
{
	u32int *r = u->regs;

	if(u->blocked)
		return;
	while(u->op < u->oe){
		if(!(r[Lsr] & Thre))
			break;
		r[Rbr] = *u->op++;
	}
	if(u->op >= u->oe)
		r[Ier] &= ~Ierthre;
	else
		r[Ier] |= Ierthre;
}

static void
interrupt(Ureg*, void *arg)
{
	Uart *u = arg;
	u32int *r = u->regs;
	u32int iir;

	for(;;){
		iir = r[Iir];
		if(iir & Iirip)
			break;
		switch(iir & 0xE){
		case 0x4:	/* rx data */
		case 0xC:	/* char timeout */
			while(r[Lsr] & Dr)
				uartrecv(u, r[Rbr]);
			break;
		case 0x2:	/* tx empty */
			uartkick(u);
			break;
		case 0x6:	/* line status */
			USED(r[Lsr]);
			break;
		case 0x0:	/* modem status */
			USED(r[Msr]);
			break;
		}
	}
}

static void
enable(Uart *u, int ie)
{
	u32int *r = u->regs;

	r[Lcr] = Wls8;
	r[Iir] = FIFOena | FIFOrclr | FIFOtclr;
	r[Mcr] = Dtr | Rts;

	if(ie){
		intrenable(IRQuart0, interrupt, u, BUSUNKNOWN, u->name);
		r[Ier] = Ierbda | Ierls;
	}
}

static int
awgetc(Uart *u)
{
	u32int *r = u->regs;
	while(!(r[Lsr] & Dr))
		;
	return r[Rbr];
}

static void
awputc(Uart *u, int ch)
{
	u32int *r = u->regs;
	while(!(r[Lsr] & Thre))
		;
	r[Rbr] = ch;
}

void
uartconsinit(void)
{
	consuart = &awuart;
}

PhysUart awphysuart = {
	.name	= "awuart",
	.pnp	= pnp,
	.enable	= enable,
	.kick	= kick,
	.getc	= awgetc,
	.putc	= awputc,
};
