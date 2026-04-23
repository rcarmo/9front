/*
 * Allwinner A733 I/O definitions
 */

/* PCI configuration */
enum {
	Pcilog		= 8,
	Pciregshift	= 0,
};

/* Interrupt numbers (GIC SPI + 32) */
enum {
	IRQuart0	= 32 + 2,	/* UART0 */
	IRQuart1	= 32 + 3,	/* UART1 */
	IRQgmac0	= 32 + 50,	/* GMAC0 (placeholder, verify from DTS) */
	IRQpcie0	= 32 + 152,	/* PCIe SII */
	IRQpcimsi	= 32 + 153,	/* PCIe MSI */
};
