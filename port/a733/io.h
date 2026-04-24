/*
 * Allwinner A733 I/O definitions
 */

#define BUSUNKNOWN (-1)

/* Interrupt numbers (GIC SPI + 32) */
enum {
	IRQuart0	= 32 + 2,
	IRQuart1	= 32 + 3,
	IRQgmac0	= 32 + 50,
	IRQpcie0	= 32 + 152,
	IRQpcimsi	= 32 + 153,
};
