# Allwinner A733 (sun60iw2) — Port Notes

## Hardware Addresses (from vendor DTS)

| Peripheral | Physical Address | Size | IRQ (SPI) |
|------------|-----------------|------|-----------|
| **UART0** | `0x02500000` | `0x400` | 2 |
| UART1 | `0x02501000` | `0x400` | 3 |
| UART2 | `0x02502000` | `0x400` | 4 |
| **GIC Distributor** | `0x03400000` | | |
| **GIC Redistributor** | `0x03460000` | | |
| GMAC0 (Ethernet) | `0x04500000` | | 50 (placeholder) |
| GMAC1 (Ethernet) | `0x04510000` | | |
| **PCIe DBI** | `0x06000000` | `0x480000` | 152/153 |
| PCIe I/O | `0x20000000` | `0x1000000` | |
| PCIe MMIO | `0x21000000` | `0x1000000` | |
| PCIe MEM | `0x22000000` | `0x6000000` | |
| **XHCI (USB3)** | `0x06A00000` | | |
| SD/MMC 0 | `0x04020000` | | |
| SD/MMC 1 | `0x04021000` | | |
| SD/MMC 2 (eMMC) | `0x04022000` | | |

## SoC Layout

- SoC register base: `0x03000000` (from DTS root `soc@3000000`)
- DRAM base: `0x40000000`
- CPU cores: 8 (cpu@0 through cpu@700) — big.LITTLE
- Pinctrl: `0x02000000`
- CCU (Clock Control): `0x02002000`
- RTC CCU: `0x07090000`

## UART

The Allwinner UART (`compatible = "allwinner,uart-v100"`) is 8250-register-compatible:
- 32-bit MMIO registers at 4-byte stride
- FIFO size: 64 bytes (`sunxi,uart-fifosize = 0x40`)
- Same register set as 16550: RBR/THR, IER, IIR/FCR, LCR, MCR, LSR, MSR, SCR
- Allwinner extensions: USR (0x7C), HALT (0xA4)

Console UART: UART0 at `0x02500000`

## GIC

ARM GICv3 (`compatible = "arm,gic-v3"`):
- Distributor: `0x03400000` (size `0x10000`)
- Redistributor: `0x03460000` (size `0xc0000`)
- `interrupt-cells = 3`

The existing `arm64/gic.c` should work directly — same register interface
as the QEMU virt GICv3.

## PCIe

Allwinner PCIe v300 RC (`compatible = "allwinner,sunxi-pcie-v300-rc"`):
- DBI base: `0x06000000` (size `0x480000`)
- 1 lane, max link speed Gen3
- Config space, I/O, and MMIO windows in ranges:
  - CFG: `0x20000000` (16MB)
  - I/O: `0x21000000` (16MB)
  - MEM: `0x22000000` (96MB)

This appears to be a DesignWare PCIe core — same IP as many other SoCs.

## Boot Chain

Allwinner custom: `boot0` (SPL) → U-Boot → kernel
- boot0 loads from SD/eMMC/SPI-NOR/UFS
- U-Boot uses dragon/sunxi tools for packaging
- Kernel loaded as uImage at configurable address

## Design Decisions

1. **UART driver**: New `uartaw.c` rather than adapting `uarti8250.c`,
   because the existing 8250 driver uses port I/O (inb/outb) while
   the Allwinner UART is purely MMIO. Simpler to write a clean MMIO
   driver from scratch than adapt the port I/O abstractions.

2. **Shared arm64 code**: Reuse `cache.v8.s`, `clock.c`, `fpu.c`,
   `gic.c`, `mmu.c`, `sysreg.c`, `trap.c` directly from `arm64/`.
   The A733 uses standard ARM IP for all of these.

3. **PCIe**: Will need a new `pciaw.c` for the DesignWare PCIe init.
   Once PCIe works, NVMe (`port/sdnvme.c`) and XHCI USB come for free.

4. **Initial target**: Serial console boot to `rc` shell via UART0.
   Storage via USB (XHCI) initially, PCIe/NVMe later.
