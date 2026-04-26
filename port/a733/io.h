/*
 * Allwinner A733 I/O definitions
 */

enum {
	IRQfiq		= -1,

	PPI		= 16,
	SPI		= 32,

	IRQcntvns	= PPI + 11,

	IRQuart0	= SPI + 2,
	IRQuart1	= SPI + 3,
	IRQgmac0	= SPI + 50,
	IRQpcie0	= SPI + 152,
	IRQpcimsi	= SPI + 153,
};

#define BUSUNKNOWN (-1)
